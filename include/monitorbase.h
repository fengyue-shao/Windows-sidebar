#pragma once
#ifndef MONITORBASE_H
#define MONITORBASE_H
#include <QTimer>
#include <QObject>

class MonitorBase : public QObject
{
    Q_OBJECT

public:
    explicit MonitorBase(int interval=1000,QObject *parent = nullptr);

    virtual ~MonitorBase()=default;
    void start();
    void stop();
protected:
    virtual void updateData()=0;
private:
    QTimer m_timer;
signals:
};

#endif // MONITORBASE_H
