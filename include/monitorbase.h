#pragma once
#ifndef MONITORBASE_H
#define MONITORBASE_H
#include <QObject>
#include "periodicmodule.h"

class MonitorBase : public QObject, public PeriodicModule
{
    Q_OBJECT

public:
    explicit MonitorBase(QObject *parent = nullptr);

    virtual ~MonitorBase()=default;
    void refresh() final;
protected:
    virtual void updateData()=0;
signals:
};

#endif // MONITORBASE_H
