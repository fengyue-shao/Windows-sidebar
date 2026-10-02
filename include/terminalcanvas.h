#pragma once
#include <QQuickPaintedItem>
#include <QFont>
#include <QPointer>
#include "terminalscreen.h"

// Paint only the visible viewport, positioning each glyph by terminal cells.
// Font fallback and bold glyph advances must not move subsequent columns.
class TerminalCanvas : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QObject* screen READ screen WRITE setScreen NOTIFY screenChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY metricsChanged)
    Q_PROPERTY(qreal cellWidth READ cellWidth WRITE setCellWidth NOTIFY metricsChanged)
    Q_PROPERTY(qreal cellHeight READ cellHeight WRITE setCellHeight NOTIFY metricsChanged)
    Q_PROPERTY(qreal scrollOffset READ scrollOffset WRITE setScrollOffset NOTIFY metricsChanged)
public:
    explicit TerminalCanvas(QQuickItem *parent = nullptr);
    QObject *screen() const { return m_screen; }
    void setScreen(QObject *screen);
    QFont font() const { return m_font; }
    void setFont(const QFont &font);
    qreal cellWidth() const { return m_cellWidth; }
    void setCellWidth(qreal value);
    qreal cellHeight() const { return m_cellHeight; }
    void setCellHeight(qreal value);
    qreal scrollOffset() const { return m_scrollOffset; }
    void setScrollOffset(qreal value);
    void paint(QPainter *painter) override;
signals:
    void screenChanged();
    void metricsChanged();
private:
    QPointer<TerminalScreen> m_screen;
    QFont m_font;
    qreal m_cellWidth = 7, m_cellHeight = 16, m_scrollOffset = 0;
};
