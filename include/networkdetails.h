#pragma once
#include <QObject>
#include <QVariantList>
class QThread;

class NetworkDetails : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList adapters READ adapters NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString updatedAt READ updatedAt NOTIFY changed)
public:
    explicit NetworkDetails(QObject *parent = nullptr);
    ~NetworkDetails() override;
    QVariantList adapters() const { return m_adapters; }
    QString summary() const { return m_summary; }
    bool busy() const { return m_worker != nullptr; }
    QString updatedAt() const { return m_updatedAt; }
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    QVariantList m_adapters;
    QString m_summary;
    QThread *m_worker = nullptr;
    QString m_updatedAt;
};
