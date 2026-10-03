#include "../include/terminalscreen.h"
#include <QChar>
#include <algorithm>

namespace {
constexpr int HistoryLimit = 2000;
QString color(int n)
{
    static const char *palette[] = {"#161b22", "#ef6b73", "#85c88a", "#e5c07b", "#78a9ed", "#c792ea", "#71c9d3", "#d9e2ee",
                                    "#68778b", "#ff9299", "#a8e0ae", "#ffe0a6", "#a2c7ff", "#e0b2ff", "#9ee9f1", "#ffffff"};
    n = std::clamp(n, 0, 255);
    if (n < 16) return QString::fromLatin1(palette[n]);
    if (n >= 232) { int v = 8 + (n - 232) * 10; return QStringLiteral("#%1%1%1").arg(v, 2, 16, QLatin1Char('0')); }
    static const int levels[] = {0, 95, 135, 175, 215, 255};
    n -= 16;
    return QStringLiteral("#%1%2%3").arg(levels[n / 36], 2, 16, QLatin1Char('0'))
        .arg(levels[(n / 6) % 6], 2, 16, QLatin1Char('0')).arg(levels[n % 6], 2, 16, QLatin1Char('0'));
}
int cellWidth(char32_t c)
{
    if (QChar::combiningClass(c) != 0 || c == 0x200d || (c >= 0xfe00 && c <= 0xfe0f)) return 0;
    return (c >= 0x1100 && (c <= 0x115f || c == 0x2329 || c == 0x232a ||
            (c >= 0x2e80 && c <= 0xa4cf) || (c >= 0xac00 && c <= 0xd7a3) ||
            (c >= 0xf900 && c <= 0xfaff) || (c >= 0xfe10 && c <= 0xfe19) ||
            (c >= 0xfe30 && c <= 0xfe6f) || (c >= 0xff01 && c <= 0xff60) ||
            (c >= 0xffe0 && c <= 0xffe6) || (c >= 0x1f300 && c <= 0x1faff) || c >= 0x20000)) ? 2 : 1;
}
}

TerminalScreen::TerminalScreen(QObject *parent) : QObject(parent) { reset(); }
TerminalScreen::Line TerminalScreen::blankLine() const
{
    Line line(m_columns);
    for (Cell &cell : line) cell.style = m_style;
    return line;
}
void TerminalScreen::reset()
{
    m_style = {}; m_savedStyle = {}; m_primaryStyle = {};
    m_row = m_column = m_savedRow = m_savedColumn = 0;
    m_top = 0; m_bottom = m_rows - 1;
    m_wrapPending = m_alternate = m_bracketedPaste = m_applicationCursor = false;
    m_cursorVisible = m_autoWrap = true;
    m_history.clear(); m_primary.clear(); m_grid.clear();
    for (int r = 0; r < m_rows; ++r) m_grid.append(blankLine());
    m_title.clear();
}
void TerminalScreen::clear()
{
    m_history.clear(); m_grid.clear();
    for (int r = 0; r < m_rows; ++r) m_grid.append(blankLine());
    m_row = m_column = 0; m_wrapPending = false;
    emit changed();
}
void TerminalScreen::resetForSession()
{
    m_decoder = QStringDecoder(QStringConverter::Utf8);
    m_highSurrogate = 0; m_parser = Parser::Text; m_sequence.clear();
    reset(); emit changed();
}
void TerminalScreen::resize(int columns, int rows)
{
    columns = std::clamp(columns, 20, 240); rows = std::clamp(rows, 4, 100);
    if (columns == m_columns && rows == m_rows) return;
    m_columns = columns; m_rows = rows;
    auto reshape = [this](QVector<Line> &grid) {
        for (Line &line : grid) {
            line.resize(m_columns);
            if (!line.isEmpty() && line.last().continuation && (line.size() < 2 || line[line.size() - 2].continuation)) line.last() = Cell{};
        }
        while (grid.size() < m_rows) grid.append(blankLine());
        if (grid.size() > m_rows) grid.resize(m_rows);
    };
    reshape(m_grid);
    if (m_alternate) reshape(m_primary);
    m_row = std::clamp(m_row, 0, m_rows - 1); m_column = std::clamp(m_column, 0, m_columns - 1);
    m_top = 0; m_bottom = m_rows - 1; m_wrapPending = false;
    emit changed();
}
void TerminalScreen::feed(const QByteArray &bytes)
{
    const QString decoded = m_decoder(bytes);
    for (QChar ch : decoded) {
        if (m_highSurrogate) {
            const auto high = m_highSurrogate; m_highSurrogate = 0;
            if (ch.isLowSurrogate()) { consume(QChar::surrogateToUcs4(high, ch.unicode())); continue; }
            consume(QChar::ReplacementCharacter);
        }
        if (ch.isHighSurrogate()) m_highSurrogate = ch.unicode();
        else consume(ch.isLowSurrogate() ? char32_t(QChar::ReplacementCharacter) : char32_t(ch.unicode()));
    }
    if (!decoded.isEmpty()) emit changed();
}
void TerminalScreen::saveCursor() { m_savedRow = m_row; m_savedColumn = m_column; m_savedStyle = m_style; }
void TerminalScreen::restoreCursor()
{
    m_row = std::clamp(m_savedRow, 0, m_rows - 1); m_column = std::clamp(m_savedColumn, 0, m_columns - 1);
    m_style = m_savedStyle; m_wrapPending = false;
}
void TerminalScreen::consume(char32_t ch)
{
    if (m_parser == Parser::Osc || m_parser == Parser::OscEscape) {
        if (ch == 7 || (m_parser == Parser::OscEscape && ch == '\\')) {
            const int split = m_sequence.indexOf(';');
            const QString code = m_sequence.left(split);
            if (split >= 0 && (code == "0" || code == "2")) m_title = m_sequence.mid(split + 1).left(256);
            m_sequence.clear(); m_parser = Parser::Text;
        } else if (ch == 0x1b) m_parser = Parser::OscEscape;
        else { m_parser = Parser::Osc; if (m_sequence.size() < 4096) m_sequence += QChar::fromUcs4(ch); }
        return;
    }
    if (m_parser == Parser::Ignore || m_parser == Parser::IgnoreEscape) {
        if (m_parser == Parser::IgnoreEscape && ch == '\\') m_parser = Parser::Text;
        else m_parser = ch == 0x1b ? Parser::IgnoreEscape : Parser::Ignore;
        return;
    }
    if (ch == 0x1b) { m_parser = Parser::Escape; m_sequence.clear(); return; }
    if (ch == 0x18 || ch == 0x1a) { m_parser = Parser::Text; m_sequence.clear(); return; }
    if (m_parser == Parser::Charset) { m_parser = Parser::Text; return; }
    if (m_parser == Parser::Escape) {
        m_parser = Parser::Text;
        switch (ch) {
        case '[': m_parser = Parser::Csi; break;
        case ']': m_parser = Parser::Osc; break;
        case 'P': case '^': case '_': m_parser = Parser::Ignore; break;
        case '(': case ')': case '*': case '+': m_parser = Parser::Charset; break;
        case '7': saveCursor(); break;
        case '8': restoreCursor(); break;
        case 'D': lineFeed(); break;
        case 'E': m_column = 0; lineFeed(); break;
        case 'M': if (m_row == m_top) scroll(-1); else m_row = std::max(0, m_row - 1); break;
        case 'c': reset(); break;
        default: break;
        }
        return;
    }
    if (m_parser == Parser::Csi) {
        if (ch >= 0x40 && ch <= 0x7e) { csi(ch); m_sequence.clear(); m_parser = Parser::Text; }
        else if (ch >= 0x20 && ch <= 0x3f && m_sequence.size() < 256) m_sequence += QChar(ushort(ch));
        else if (ch >= 0x20) { m_sequence.clear(); m_parser = Parser::Text; }
        return;
    }
    switch (ch) {
    case '\r': m_column = 0; m_wrapPending = false; break;
    case '\n': case '\v': case '\f': lineFeed(); break;
    case '\b': m_column = std::max(0, m_column - 1); m_wrapPending = false; break;
    case '\t': m_column = std::min(m_columns - 1, ((m_column / 8) + 1) * 8); m_wrapPending = false; break;
    default: if (ch >= 0x20 && ch != 0x7f) put(ch); break;
    }
}
void TerminalScreen::eraseCells(Line &line, int first, int last)
{
    first = std::clamp(first, 0, int(line.size())); last = std::clamp(last, -1, int(line.size()) - 1);
    if (first <= last && line[first].continuation && first > 0) --first;
    if (first <= last && last + 1 < line.size() && line[last + 1].continuation) ++last;
    for (int c = first; c <= last; ++c) line[c] = Cell{QStringLiteral(" "), m_style, false};
}
void TerminalScreen::put(char32_t ch)
{
    int width = cellWidth(ch);
    if (width == 0) {
        int previous = m_wrapPending ? m_column : m_column - 1;
        if (previous >= 0) {
            while (previous > 0 && m_grid[m_row][previous].continuation) --previous;
            if (m_grid[m_row][previous].text.size() < 32) m_grid[m_row][previous].text += QChar::fromUcs4(ch);
        }
        return;
    }
    if (m_autoWrap && (m_wrapPending || (width == 2 && m_column == m_columns - 1))) { m_column = 0; lineFeed(); }
    m_wrapPending = false;
    if (width == 2 && m_column == m_columns - 1) width = 1;
    eraseCells(m_grid[m_row], m_column, m_column + width - 1);
    m_grid[m_row][m_column] = Cell{QString::fromUcs4(&ch, 1), m_style, false};
    if (width == 2) m_grid[m_row][m_column + 1] = Cell{QString(), m_style, true};
    m_column += width;
    if (m_column >= m_columns) { m_column = m_columns - 1; m_wrapPending = m_autoWrap; }
}
void TerminalScreen::lineFeed()
{
    m_wrapPending = false;
    if (m_row == m_bottom) scroll(1); else m_row = std::min(m_rows - 1, m_row + 1);
}
void TerminalScreen::scroll(int amount)
{
    amount = std::clamp(amount, -(m_bottom - m_top + 1), m_bottom - m_top + 1);
    while (amount > 0) {
        if (!m_alternate && m_top == 0 && m_bottom == m_rows - 1) {
            m_history.append(m_grid[m_top]);
            if (m_history.size() > HistoryLimit) m_history.removeFirst();
        }
        m_grid.removeAt(m_top); m_grid.insert(m_bottom, blankLine()); --amount;
    }
    while (amount < 0) { m_grid.removeAt(m_bottom); m_grid.insert(m_top, blankLine()); ++amount; }
}
void TerminalScreen::setMode(int mode, bool enabled)
{
    switch (mode) {
    case 1: m_applicationCursor = enabled; break;
    case 7: m_autoWrap = enabled; m_wrapPending = false; break;
    case 25: m_cursorVisible = enabled; break;
    case 2004: m_bracketedPaste = enabled; break;
    case 47: case 1047: case 1049:
        if (enabled == m_alternate) break;
        if (enabled) {
            m_primary = m_grid; m_primaryRow = m_row; m_primaryColumn = m_column; m_primaryStyle = m_style;
            m_grid.clear(); for (int r = 0; r < m_rows; ++r) m_grid.append(blankLine()); m_row = m_column = 0;
        } else {
            m_grid = m_primary; m_primary.clear(); m_style = m_primaryStyle;
            m_row = std::clamp(m_primaryRow, 0, m_rows - 1); m_column = std::clamp(m_primaryColumn, 0, m_columns - 1);
        }
        m_alternate = enabled; m_top = 0; m_bottom = m_rows - 1; m_wrapPending = false;
        break;
    default: break;
    }
}
void TerminalScreen::csi(char32_t final)
{
    const bool priv = m_sequence.startsWith('?');
    QString parameters = priv ? m_sequence.mid(1) : m_sequence;
    QList<int> p;
    for (const QString &part : parameters.split(';')) p.append(std::clamp(part.toInt(), 0, 65535));
    auto n = [&p](int index, int defaultValue = 1) { return index < p.size() && p[index] > 0 ? p[index] : defaultValue; };
    const int a = n(0);
    if (final != 'm') m_wrapPending = false;
    switch (final) {
    case 'A': m_row = std::max(0, m_row - a); break;
    case 'B': case 'e': m_row = std::min(m_rows - 1, m_row + a); break;
    case 'C': case 'a': m_column = std::min(m_columns - 1, m_column + a); break;
    case 'D': m_column = std::max(0, m_column - a); break;
    case 'E': m_row = std::min(m_rows - 1, m_row + a); m_column = 0; break;
    case 'F': m_row = std::max(0, m_row - a); m_column = 0; break;
    case 'G': case '`': m_column = std::clamp(a - 1, 0, m_columns - 1); break;
    case 'd': m_row = std::clamp(a - 1, 0, m_rows - 1); break;
    case 'H': case 'f': m_row = std::clamp(a - 1, 0, m_rows - 1); m_column = std::clamp(n(1) - 1, 0, m_columns - 1); break;
    case 'J':
        if (p[0] == 3) m_history.clear();
        else if (p[0] == 2) { for (Line &line : m_grid) eraseCells(line, 0, m_columns - 1); }
        else if (p[0] == 1) { for (int r = 0; r < m_row; ++r) eraseCells(m_grid[r], 0, m_columns - 1); eraseCells(m_grid[m_row], 0, m_column); }
        else { eraseCells(m_grid[m_row], m_column, m_columns - 1); for (int r = m_row + 1; r < m_rows; ++r) eraseCells(m_grid[r], 0, m_columns - 1); }
        break;
    case 'K': eraseCells(m_grid[m_row], p[0] == 0 ? m_column : 0, p[0] == 1 ? m_column : m_columns - 1); break;
    case 'X': eraseCells(m_grid[m_row], m_column, std::min(m_columns - 1, m_column + a - 1)); break;
    case 'P': { int count = std::min(a, m_columns - m_column); auto &line = m_grid[m_row]; eraseCells(line, m_column, m_column + count - 1); line.remove(m_column, count); while (line.size() < m_columns) line.append(Cell{QStringLiteral(" "), m_style, false}); break; }
    case '@': { auto &line = m_grid[m_row]; int count = std::min(a, m_columns - m_column); if (line[m_column].continuation) eraseCells(line, m_column, m_column); for (int i = 0; i < count; ++i) line.insert(m_column, Cell{QStringLiteral(" "), m_style, false}); line.resize(m_columns); break; }
    case 'L': case 'M':
        if (m_row >= m_top && m_row <= m_bottom) for (int i = 0; i < std::min(a, m_bottom - m_row + 1); ++i) {
            if (final == 'L') { m_grid.removeAt(m_bottom); m_grid.insert(m_row, blankLine()); }
            else { m_grid.removeAt(m_row); m_grid.insert(m_bottom, blankLine()); }
        }
        break;
    case 'S': scroll(a); break;
    case 'T': scroll(-a); break;
    case 'r': if (!priv && a < n(1, m_rows) && n(1, m_rows) <= m_rows) { m_top = a - 1; m_bottom = n(1, m_rows) - 1; m_row = m_column = 0; } break;
    case 's': saveCursor(); break;
    case 'u': restoreCursor(); break;
    case 'h': case 'l': if (priv) for (int mode : p) setMode(mode, final == 'h'); break;
    case 'm': sgr(p); break;
    case 'n':
        if (!priv && p[0] == 5) emit responseRequested(QStringLiteral("\x1b[0n"));
        if (!priv && p[0] == 6) emit responseRequested(QStringLiteral("\x1b[%1;%2R").arg(m_row + 1).arg(m_column + 1));
        break;
    case 'c': if (!priv && p[0] == 0) emit responseRequested(QStringLiteral("\x1b[?1;2c")); break;
    default: break;
    }
}
void TerminalScreen::sgr(const QList<int> &p)
{
    for (int i = 0; i < p.size(); ++i) {
        int code = p[i];
        if (code == 0) m_style = {};
        else if (code == 1) m_style.bold = true;
        else if (code == 3) m_style.italic = true;
        else if (code == 4) m_style.underline = true;
        else if (code == 7) m_style.inverse = true;
        else if (code == 22) m_style.bold = false;
        else if (code == 23) m_style.italic = false;
        else if (code == 24) m_style.underline = false;
        else if (code == 27) m_style.inverse = false;
        else if (code == 39) m_style.foreground.clear();
        else if (code == 49) m_style.background.clear();
        else if (code >= 30 && code <= 37) m_style.foreground = color(code - 30);
        else if (code >= 40 && code <= 47) m_style.background = color(code - 40);
        else if (code >= 90 && code <= 97) m_style.foreground = color(code - 90 + 8);
        else if (code >= 100 && code <= 107) m_style.background = color(code - 100 + 8);
        else if ((code == 38 || code == 48) && i + 1 < p.size()) {
            QString selected;
            if (p[i + 1] == 5 && i + 2 < p.size()) { selected = color(p[i + 2]); i += 2; }
            else if (p[i + 1] == 2 && i + 4 < p.size()) {
                selected = QStringLiteral("#%1%2%3").arg(std::min(255, p[i + 2]), 2, 16, QLatin1Char('0'))
                    .arg(std::min(255, p[i + 3]), 2, 16, QLatin1Char('0')).arg(std::min(255, p[i + 4]), 2, 16, QLatin1Char('0')); i += 4;
            }
            if (!selected.isEmpty()) { if (code == 38) m_style.foreground = selected; else m_style.background = selected; }
        }
    }
}
QString TerminalScreen::text(const Line &line) const
{
    QString result;
    for (const Cell &cell : line) if (!cell.continuation) result += cell.text;
    while (result.endsWith(' ')) result.chop(1);
    return result;
}
QString TerminalScreen::plainText() const
{
    QStringList lines;
    if (!m_alternate) for (const Line &line : m_history) lines.append(text(line));
    for (const Line &line : m_grid) lines.append(text(line));
    while (!lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();
    return lines.join('\n');
}
QString TerminalScreen::render(const Line &line) const
{
    QString result, run;
    Style current; bool haveRun = false;
    auto flush = [&]() {
        if (!haveRun) return;
        QString fg = current.foreground, bg = current.background;
        if (current.inverse) { if (fg.isEmpty()) fg = "#d9e2ee"; if (bg.isEmpty()) bg = "#091019"; std::swap(fg, bg); }
        QString css;
        if (!fg.isEmpty()) css += "color:" + fg + ";";
        if (!bg.isEmpty()) css += "background-color:" + bg + ";";
        if (current.bold) css += "font-weight:bold;";
        if (current.italic) css += "font-style:italic;";
        if (current.underline) css += "text-decoration:underline;";
        const QString escaped = run.toHtmlEscaped().replace(' ', QChar(0x00a0));
        result += css.isEmpty() ? escaped : "<span style=\"" + css + "\">" + escaped + "</span>";
        run.clear();
    };
    for (const Cell &cell : line) {
        if (cell.continuation) continue;
        if (haveRun && !(cell.style == current)) flush();
        current = cell.style; haveRun = true; run += cell.text;
    }
    flush(); return result;
}
QStringList TerminalScreen::renderedLines() const
{
    QStringList result;
    result.reserve(scrollbackRows() + m_rows);
    if (!m_alternate) for (const Line &line : m_history) result.append(render(line));
    for (const Line &line : m_grid) result.append(render(line));
    return result;
}
