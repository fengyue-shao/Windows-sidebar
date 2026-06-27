#include "networkmonitor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#pragma comment(lib,"Iphlpapi.lib")
NetworkMonitor::NetworkMonitor(QObject* parent)
    :MonitorBase(1000,parent) {
    NetworkMonitor::updateData();
    start();
}
QString NetworkMonitor::downloadSpeedText()const{
    return formatSpeed(m_downloadSpeed);
}
QString NetworkMonitor::uploadSpeedText()const{
    return formatSpeed(m_uploadSpeed);
}
void NetworkMonitor::updateData(){
    PMIB_IF_TABLE2 table=nullptr;
    if(GetIfTable2(&table)!=NO_ERROR){
        return;
    }
    quint64 totalRecv=0;
    quint64 totalSend=0;
    for(ULONG i=0;i<table->NumEntries;++i){
        const MIB_IF_ROW2& row =table->Table[i];
        if(row.OperStatus!=IfOperStatusUp){
            continue;
        }
        if(row.Type==IF_TYPE_SOFTWARE_LOOPBACK)
            continue;
        totalRecv+=row.InOctets;
        totalSend+=row.OutOctets;
    }
    FreeMibTable(table);
    if(m_lastRecv!=0){
        m_downloadSpeed=(totalRecv-m_lastRecv)/1024.0;
        m_uploadSpeed=(totalSend-m_lastSend)/1024.0;
        emit downloadSpeedChanged();
        emit uploadSpeedChanged();
    }
    m_lastRecv=totalRecv;
    m_lastSend=totalSend;
}
QString NetworkMonitor::formatSpeed(double kbps)const{
    if(kbps<1024){
        return QString("%1 Kb/s").arg(kbps,0,'f',1);
    }
    if(kbps<1024*1024){
        return QString("%1 Mb/s").arg(kbps/1024.0,0,'f',2);
    }
    return QString("%1 Gb/s").arg(kbps/1024.0/1024.0,0,'f',2);
}
