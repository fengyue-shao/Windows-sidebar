#pragma once
#include <QObject>
#include <QVariantList>

class NetworkDetails : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList adapters READ adapters NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
public:
    explicit NetworkDetails(QObject *parent = nullptr);
    QVariantList adapters() const { return m_adapters; }
    QString summary() const { return m_summary; }
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    QVariantList m_adapters;
    QString m_summary;
};
