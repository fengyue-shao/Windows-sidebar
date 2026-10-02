#include "../include/terminalcanvas.h"
#include <QPainter>
#include <QFontMetricsF>
#include <cmath>

TerminalCanvas::TerminalCanvas(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(false);
}
void TerminalCanvas::setScreen(QObject *object)
{
    auto *screen = qobject_cast<TerminalScreen *>(object);
    if (m_screen == screen) return;
    if (m_screen) disconnect(m_screen, nullptr, this, nullptr);
    m_screen = screen;
    if (screen) {
        connect(screen, &TerminalScreen::changed, this, [this] { update(); });
        connect(screen, &QObject::destroyed, this, [this] { update(); emit screenChanged(); });
    }
    update(); emit screenChanged();
}
void TerminalCanvas::setFont(const QFont &font) { if (m_font == font) return; m_font = font; update(); emit metricsChanged(); }
void TerminalCanvas::setCellWidth(qreal value) { value = qBound(1.0, value, 100.0); if (m_cellWidth == value) return; m_cellWidth = value; update(); emit metricsChanged(); }
void TerminalCanvas::setCellHeight(qreal value) { value = qBound(1.0, value, 100.0); if (m_cellHeight == value) return; m_cellHeight = value; update(); emit metricsChanged(); }
void TerminalCanvas::setScrollOffset(qreal value) { value = qMax(0.0, value); if (m_scrollOffset == value) return; m_scrollOffset = value; update(); emit metricsChanged(); }
void TerminalCanvas::paint(QPainter *painter)
{
    if (!m_screen) return;
    painter->setRenderHint(QPainter::TextAntialiasing);
    const int history = m_screen->scrollbackRows();
    const int count = history + m_screen->m_grid.size();
    const int first = qMax(0, int(std::floor(m_scrollOffset / m_cellHeight)));
    const int last = qMin(count - 1, int(std::ceil((m_scrollOffset + height()) / m_cellHeight)));
    for (int row = first; row <= last; ++row) {
        const auto &line = row < history ? m_screen->m_history[row] : m_screen->m_grid[row - history];
        const qreal top = row * m_cellHeight - m_scrollOffset;
        for (int column = 0; column < line.size(); ++column) {
            const auto &cell = line[column];
            if (cell.continuation) continue;
            const int span = column + 1 < line.size() && line[column + 1].continuation ? 2 : 1;
            const QRectF bounds(column * m_cellWidth, top, span * m_cellWidth, m_cellHeight);
            if (bounds.left() >= width()) break;
            QColor foreground(cell.style.foreground.isEmpty() ? "#D7E0EA" : cell.style.foreground);
            QColor background(cell.style.background.isEmpty() ? "#0E1117" : cell.style.background);
            if (cell.style.inverse) std::swap(foreground, background);
            if (!cell.style.background.isEmpty() || cell.style.inverse) painter->fillRect(bounds, background);
            if (cell.text == QStringLiteral(" ")) continue;
            QFont font = m_font; font.setBold(cell.style.bold); font.setItalic(cell.style.italic); font.setUnderline(cell.style.underline);
            painter->setFont(font); painter->setPen(foreground);
            const QFontMetricsF metrics(font);
            const qreal baseline = top + (m_cellHeight - metrics.height()) / 2 + metrics.ascent();
            painter->save(); painter->setClipRect(bounds, Qt::IntersectClip);
            painter->drawText(QPointF(bounds.left(), baseline), cell.text);
            painter->restore();
        }
    }
}
