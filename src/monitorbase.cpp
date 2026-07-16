#include "include/monitorbase.h"

MonitorBase::MonitorBase(int interval,QObject *parent)
    : QObject{parent}
{
    m_timer.setInterval(interval);
    connect(&m_timer,&QTimer::timeout,this,&MonitorBase::updateData);
}
void MonitorBase::start(){
    m_timer.start();
}
void MonitorBase::stop(){
    m_timer.stop();
}
