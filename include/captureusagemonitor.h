#pragma once
#include <QObject>
#include <QVariantList>
#include <memory>
class CaptureUsageMonitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList applications READ applications NOTIFY usageChanged)
    Q_PROPERTY(bool available READ available NOTIFY usageChanged)
    Q_PROPERTY(int observedEndpointCount READ observedEndpointCount NOTIFY usageChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY usageChanged)
public:
    explicit CaptureUsageMonitor(QObject *parent=nullptr);
    ~CaptureUsageMonitor() override;
    QVariantList applications() const { return m_apps; }
    bool available() const { return m_available; }
    int observedEndpointCount() const { return m_observedEndpoints; }
    QString statusText() const { return m_status; }
signals:
    void usageChanged();
private:
    class Worker;
    std::unique_ptr<Worker> m_worker;
    QVariantList m_apps;
    bool m_available=false;
    int m_observedEndpoints=0;
    QString m_status;
};
