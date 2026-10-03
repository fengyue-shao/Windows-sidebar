#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
#include <QRegion>
#include <QTimer>
#include <QVariantList>
#include <QWindow>

// Owns native integration only; QML owns the Hidden/Bar/Cards state machine.
class DropdownController : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(bool pointerInside READ pointerInside NOTIFY pointerInsideChanged)
    Q_PROPERTY(bool hotkeyAvailable READ hotkeyAvailable NOTIFY hotkeyAvailableChanged)
    Q_PROPERTY(QString shortcutText READ shortcutText NOTIFY hotkeyAvailableChanged)
    Q_PROPERTY(QString hotkeyStatus READ hotkeyStatus NOTIFY hotkeyAvailableChanged)
public:
    explicit DropdownController(QObject *parent = nullptr);
    ~DropdownController() override;
    bool pointerInside() const { return m_inside; }
    bool hotkeyAvailable() const { return m_hotkey; }
    QString shortcutText() const { return m_shortcut; }
    QString hotkeyStatus() const { return m_hotkeyStatus; }
    Q_INVOKABLE bool applyShortcut(const QString &shortcut);
    Q_INVOKABLE void attach(QWindow *window);
    Q_INVOKABLE void configureEdge(int x, int y, int width);
    Q_INVOKABLE void blockEdgeUntilLeave();
    void setSuppressEdgeInFullscreen(bool enabled) { m_suppressFullscreen = enabled; m_dwell = 0; }
    Q_INVOKABLE void setRegions(const QVariantList &rectangles);
    Q_INVOKABLE bool regionContains(int x, int y) const { return m_region.contains(QPoint(x, y)); }
    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *result) override;
signals:
    void edgeRequested();
    void hotkeyRequested();
    void pointerInsideChanged();
    void hotkeyAvailableChanged();
private:
    void pollPointer();
    bool foregroundCoversTargetMonitor() const;
    QPointer<QWindow> m_window;
    QRegion m_region;
    QRect m_edge;
    QTimer m_timer;
    int m_dwell = 0;
    bool m_blocked = false;
    bool m_inside = false;
    bool m_hotkey = false;
    bool m_suppressFullscreen = false;
    int m_hotkeyId = 0x5342;
    unsigned int m_hotkeyModifiers = 0, m_hotkeyKey = 0;
    QString m_shortcut = QStringLiteral("Ctrl+Alt+Space"), m_hotkeyStatus;
};
