#pragma once
#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QColor>
#include <QObject>

class ThemeManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY themeChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY themeChanged)
    Q_PROPERTY(QColor background READ background NOTIFY themeChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceStrong READ surfaceStrong NOTIFY themeChanged)
    Q_PROPERTY(QColor cardSurface READ cardSurface NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceHover READ surfaceHover NOTIFY themeChanged)
    Q_PROPERTY(QColor border READ border NOTIFY themeChanged)
    Q_PROPERTY(QColor borderStrong READ borderStrong NOTIFY themeChanged)
    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY themeChanged)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY themeChanged)
    Q_PROPERTY(QColor textMuted READ textMuted NOTIFY themeChanged)
    Q_PROPERTY(QColor textFaint READ textFaint NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QColor accentSecondary READ accentSecondary NOTIFY themeChanged)
    Q_PROPERTY(QColor accentHover READ accentHover NOTIFY themeChanged)
    Q_PROPERTY(QColor accentSoft READ accentSoft NOTIFY themeChanged)
    Q_PROPERTY(QColor success READ success NOTIFY themeChanged)
    Q_PROPERTY(QColor danger READ danger NOTIFY themeChanged)
    Q_PROPERTY(QColor dangerSoft READ dangerSoft NOTIFY themeChanged)
    Q_PROPERTY(QColor warning READ warning NOTIFY themeChanged)
    Q_PROPERTY(QColor track READ track NOTIFY themeChanged)
    Q_PROPERTY(QColor mediaStart READ mediaStart NOTIFY themeChanged)
    Q_PROPERTY(QColor mediaMiddle READ mediaMiddle NOTIFY themeChanged)
    Q_PROPERTY(QColor mediaEnd READ mediaEnd NOTIFY themeChanged)
    Q_PROPERTY(QColor mediaCover READ mediaCover NOTIFY themeChanged)
    Q_PROPERTY(QColor overlayStart READ overlayStart NOTIFY themeChanged)
    Q_PROPERTY(QColor overlayEnd READ overlayEnd NOTIFY themeChanged)
    Q_PROPERTY(QColor overlayText READ overlayText NOTIFY themeChanged)

public:
    enum Mode {
        FollowSystem,
        Light,
        Dark
    };
    Q_ENUM(Mode)

    explicit ThemeManager(QObject *parent = nullptr);

    Mode mode() const;
    bool dark() const;

    QColor background() const;
    QColor surface() const;
    QColor surfaceStrong() const;
    QColor cardSurface() const;
    QColor surfaceHover() const;
    QColor border() const;
    QColor borderStrong() const;
    QColor textPrimary() const;
    QColor textSecondary() const;
    QColor textMuted() const;
    QColor textFaint() const;
    QColor accent() const;
    QColor accentSecondary() const;
    QColor accentHover() const;
    QColor accentSoft() const;
    QColor success() const;
    QColor danger() const;
    QColor dangerSoft() const;
    QColor warning() const;
    QColor track() const;
    QColor mediaStart() const;
    QColor mediaMiddle() const;
    QColor mediaEnd() const;
    QColor mediaCover() const;
    QColor overlayStart() const;
    QColor overlayEnd() const;
    QColor overlayText() const;

public slots:
    void setMode(Mode mode);
    Q_INVOKABLE void setModeValue(int mode);

signals:
    void themeChanged();

private:
    void applyColorScheme();
    bool systemDark() const;
    QColor choose(const char *lightColor, const char *darkColor) const;

    Mode m_mode = FollowSystem;
};

#endif // THEMEMANAGER_H
