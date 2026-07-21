#include "include/monitorbase.h"

MonitorBase::MonitorBase(QObject *parent)
    : QObject{parent}
{}

void MonitorBase::refresh()
{
    updateData();
}
