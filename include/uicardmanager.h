#pragma once
#include <QObject>
#include <QVariantList>
#include <QJsonObject>
#include <QHash>
#include <QRectF>
#include "uicard.h"

// Native application UI only. No plugin transport, QML evaluation or business services.
class UiCardManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY catalogChanged)
    Q_PROPERTY(int revision READ revision NOTIFY layoutChanged)
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    Q_PROPERTY(QString editingPage READ editingPage NOTIFY editingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)
    Q_PROPERTY(bool hasPinnedCards READ hasPinnedCards NOTIFY layoutChanged)
public:
    explicit UiCardManager(QObject *parent=nullptr,const QString &storagePath={});
    Q_INVOKABLE bool registerCard(const QVariantMap &definition);
    Q_INVOKABLE void setAvailable(const QString &id,bool available);
    QVariantList catalog() const { return m_catalog; }
    int revision() const { return m_revision; }
    bool editing() const { return !m_editPage.isEmpty(); }
    QString editingPage() const { return m_editPage; }
    QString lastError() const { return m_error; }
    Q_INVOKABLE QVariantMap geometry(const QString &id,qreal viewportWidth) const;
    Q_INVOKABLE qreal contentHeight(const QString &page,qreal viewportWidth) const;
    Q_INVOKABLE bool beginEdit(const QString &page,qreal viewportWidth);
    Q_INVOKABLE bool setGeometry(const QString &id,qreal x,qreal y,qreal width,qreal height,qreal viewportWidth);
    Q_INVOKABLE bool setVisible(const QString &id,bool visible);
    Q_INVOKABLE bool resetDraft();
    Q_INVOKABLE bool saveEdit();
    Q_INVOKABLE void cancelEdit();
    Q_INVOKABLE bool reportMinimumHeight(const QString &id,qreal height);
    Q_INVOKABLE void configureDesktop(qreal width,qreal height,const QVariantList &reserved);
    Q_INVOKABLE QVariantMap desktopGeometry(const QString &id) const;
    Q_INVOKABLE bool beginDesktopEdit(const QString &page);
    Q_INVOKABLE bool canPlaceDesktop(const QString &id,qreal x,qreal y,qreal width,qreal height) const;
    Q_INVOKABLE bool setDesktopGeometry(const QString &id,qreal x,qreal y,qreal width,qreal height);
    Q_INVOKABLE bool setEnabled(const QString &id,bool enabled);
    Q_INVOKABLE bool setPinned(const QString &id,bool pinned);
    bool hasPinnedCards() const;
signals:
    void catalogChanged();
    void layoutChanged();
    void editingChanged();
    void errorChanged();
private:
    QVariantMap definition(const QString &id) const;
    QString layoutKey(const QString &page,qreal width) const;
    QHash<QString,QRectF> resolved(const QString &page,qreal width,bool useSaved=true) const;
    QJsonObject records(const QString &page,qreal width) const;
    bool visible(const QString &id,const QJsonObject &records) const;
    bool reject(const QString &error);
    void changed();
    void load();
    QJsonObject desktopRecords(const QString &page) const;
    QHash<QString,UiCard> desktopResolved(const QString &page) const;
    QJsonObject desktopRecord(const UiCard &card) const;
    bool setDesktopFlag(const QString &id,const QString &flag,bool value);
    QVariantList m_catalog;
    struct LayoutCache { qreal width=0; int revision=-1; QHash<QString,QRectF> rectangles; };
    mutable QHash<QString,LayoutCache> m_layoutCache;
    QHash<QString,qreal> m_minimumHeights;
    QHash<QString,bool> m_available;
    QJsonObject m_saved,m_draft;
    QJsonObject m_desktopSaved;
    QSizeF m_desktopSize;
    QList<QRectF> m_reserved;
    bool m_desktopEditing=false;
    mutable QHash<QString,QHash<QString,UiCard>> m_desktopCache;
    QString m_path,m_legacyPath,m_editPage,m_editKey,m_error;
    qreal m_editWidth=0;
    int m_revision=0;
};
