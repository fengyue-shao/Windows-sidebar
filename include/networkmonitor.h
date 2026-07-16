#pragma once
#ifndef NETWORKMONITOR_H
#define NETWORKMONITOR_H
#include "monitorbase.h"
class NetworkMonitor : public MonitorBase
{
    Q_OBJECT
    Q_PROPERTY(QString downloadSpeedText
               READ downloadSpeedText
               NOTIFY downloadSpeedChanged)
    Q_PROPERTY(QString uploadSpeedText
               READ uploadSpeedText
               NOTIFY uploadSpeedChanged)

public:
    explicit NetworkMonitor(QObject* parent=nullptr);
    QString downloadSpeedText() const;
    QString uploadSpeedText() const;
signals:
    void downloadSpeedChanged();
    void uploadSpeedChanged();
protected:
    void updateData() override;
private:
    double m_downloadSpeed=0.0;
    double m_uploadSpeed=0.0;
    quint64 m_lastRecv=0;
    quint64 m_lastSend=0;
    QString formatSpeed(double kbps)const;
};

#endif // NETWORKMONITOR_H
