#pragma once
#include <QObject>
#include <QStringList>
#include <QStringConverter>
#include <QVector>

// A bounded, local VT display model. Escape sequences never open URLs or write
// the clipboard; the only outgoing data are terminal capability/status replies.
class TerminalScreen : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList renderedLines READ renderedLines NOTIFY changed)
    Q_PROPERTY(QString plainOutput READ plainText NOTIFY changed)
    Q_PROPERTY(int cursorRow READ cursorRow NOTIFY changed)
    Q_PROPERTY(int cursorColumn READ cursorColumn NOTIFY changed)
    Q_PROPERTY(int columns READ columns NOTIFY changed)
    Q_PROPERTY(int rows READ rows NOTIFY changed)
    Q_PROPERTY(int scrollbackRows READ scrollbackRows NOTIFY changed)
    Q_PROPERTY(bool cursorVisible READ cursorVisible NOTIFY changed)
    Q_PROPERTY(bool alternateScreen READ alternateScreen NOTIFY changed)
    Q_PROPERTY(bool bracketedPaste READ bracketedPaste NOTIFY changed)
    Q_PROPERTY(bool applicationCursorKeys READ applicationCursorKeys NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
public:
    explicit TerminalScreen(QObject *parent = nullptr);
    void feed(const QByteArray &data);
    void resize(int columns, int rows);
    Q_INVOKABLE void clear();
    void resetForSession();
    Q_INVOKABLE QString plainText() const;
    QStringList renderedLines() const;
    int cursorRow() const { return scrollbackRows() + m_row; }
    int cursorColumn() const { return m_column; }
    int columns() const { return m_columns; }
    int rows() const { return m_rows; }
    int scrollbackRows() const { return m_alternate ? 0 : int(m_history.size()); }
    bool cursorVisible() const { return m_cursorVisible; }
    bool alternateScreen() const { return m_alternate; }
    bool bracketedPaste() const { return m_bracketedPaste; }
    bool applicationCursorKeys() const { return m_applicationCursor; }
    QString title() const { return m_title; }
signals:
    void changed();
    void responseRequested(const QString &data);
private:
    friend class TerminalCanvas;
    struct Style {
        QString foreground, background;
        bool bold = false, italic = false, underline = false, inverse = false;
        bool operator==(const Style &) const = default;
    };
    struct Cell { QString text = QStringLiteral(" "); Style style; bool continuation = false; };
    using Line = QVector<Cell>;
    enum class Parser { Text, Escape, Csi, Osc, OscEscape, Ignore, IgnoreEscape, Charset };
    Line blankLine() const;
    void consume(char32_t ch);
    void put(char32_t ch);
    void lineFeed();
    void scroll(int amount);
    void csi(char32_t final);
    void sgr(const QList<int> &parameters);
    void setMode(int mode, bool enabled);
    void reset();
    void eraseCells(Line &line, int first, int last);
    void saveCursor();
    void restoreCursor();
    QString render(const Line &line) const;
    QString text(const Line &line) const;
    int m_columns = 80, m_rows = 24;
    int m_row = 0, m_column = 0, m_top = 0, m_bottom = 23;
    int m_savedRow = 0, m_savedColumn = 0;
    int m_primaryRow = 0, m_primaryColumn = 0;
    bool m_wrapPending = false, m_autoWrap = true;
    bool m_cursorVisible = true, m_alternate = false, m_bracketedPaste = false;
    bool m_applicationCursor = false;
    Style m_style, m_savedStyle, m_primaryStyle;
    QVector<Line> m_grid, m_primary, m_history;
    Parser m_parser = Parser::Text;
    QString m_sequence, m_title;
    QStringDecoder m_decoder{QStringConverter::Utf8};
    char16_t m_highSurrogate = 0;
};
