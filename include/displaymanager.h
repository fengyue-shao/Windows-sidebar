#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <QObject>
#include <QRect>
#include <QVariantList>

class DisplayManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList displays READ displays NOTIFY displaysChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QRect selectedGeometry READ selectedGeometry NOTIFY selectedGeometryChanged)
    Q_PROPERTY(int selectedX READ selectedX NOTIFY selectedGeometryChanged)
    Q_PROPERTY(int selectedY READ selectedY NOTIFY selectedGeometryChanged)
    Q_PROPERTY(int selectedWidth READ selectedWidth NOTIFY selectedGeometryChanged)
    Q_PROPERTY(int selectedHeight READ selectedHeight NOTIFY selectedGeometryChanged)
    Q_PROPERTY(QRect availableGeometry READ availableGeometry NOTIFY selectedGeometryChanged)
    Q_PROPERTY(int dockSide READ dockSide WRITE setDockSide NOTIFY dockSideChanged)

public:
    enum DockSide { Left = 0, Right = 1 };
    Q_ENUM(DockSide)

    explicit DisplayManager(QObject *parent = nullptr);

    QVariantList displays() const;
    int currentIndex() const;
    QRect selectedGeometry() const;
    QRect availableGeometry() const;
    int selectedX() const;
    int selectedY() const;
    int selectedWidth() const;
    int selectedHeight() const;
    int dockSide() const;

    Q_INVOKABLE void setCurrentIndex(int index);
    Q_INVOKABLE void setDockSide(int side);

signals:
    void displaysChanged();
    void currentIndexChanged();
    void selectedGeometryChanged();
    void dockSideChanged();

private slots:
    void rebuildScreens();
    void refreshSelectedGeometry();

private:
    QVariantList m_displays;
    QString m_selectedName;
    int m_currentIndex = 0;
    QRect m_selectedGeometry;
    QRect m_availableGeometry;
    DockSide m_dockSide = Right;
};

#endif // DISPLAYMANAGER_H
