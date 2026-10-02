#include "include/awakemanager.h"
#include <windows.h>

AwakeManager::AwakeManager(QObject *parent, std::function<bool(bool, bool)> request, std::function<quint64()> clock)
    : QObject(parent), m_request(std::move(request)), m_clock(std::move(clock))
{
    if (!m_request) m_request = [](bool active, bool display) {
        const auto flags = ES_CONTINUOUS | (active ? ES_SYSTEM_REQUIRED : 0) | (active && display ? ES_DISPLAY_REQUIRED : 0);
        return SetThreadExecutionState(flags) != 0;
    };
    if (!m_clock) m_clock = [] { return GetTickCount64(); };
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &AwakeManager::tick);
}

AwakeManager::~AwakeManager()
{
    if (m_active) m_request(false, false);
}

void AwakeManager::start(int minutes, bool keepDisplay)
{
    if (minutes < 1 || minutes > 240) {
        m_status = QStringLiteral("时长必须为 1–240 分钟"); emit changed(); return;
    }
    if (!m_request(true, keepDisplay)) {
        m_status = QStringLiteral("Windows 拒绝保持唤醒请求"); emit changed(); return;
    }
    m_active = true; m_keepDisplay = keepDisplay;
    m_stopRequested = false; m_expired = false; m_duration = minutes * 60;
    m_deadline = m_clock() + static_cast<quint64>(minutes) * 60000;
    m_remaining = minutes * 60;
    m_status = keepDisplay ? QStringLiteral("保持系统及屏幕唤醒") : QStringLiteral("保持系统唤醒 · 屏幕仍可自动关闭");
    m_timer.start(); emit changed();
}

void AwakeManager::stop()
{
    if (!m_active) return;
    m_expired = false; m_stopRequested = true; release();
}
void AwakeManager::release()
{
    if (!m_request(false, false)) {
        m_status = QStringLiteral("释放请求失败，正在重试");
        emit changed(); return;
    }
    m_timer.stop(); m_active = false; m_keepDisplay = false; m_remaining = 0;
    m_status = QStringLiteral("已关闭 · 系统电源策略恢复正常"); emit changed();
    m_stopRequested = false;
    if (m_expired) { m_expired = false; emit expired(); }
}

void AwakeManager::tick()
{
    if (!m_active) return;
    if (m_stopRequested) { release(); return; }
    const auto now = m_clock();
    if (now >= m_deadline) { m_remaining = 0; m_expired = true; m_stopRequested = true; release(); return; }
    m_remaining = static_cast<int>((m_deadline - now + 999) / 1000);
    emit changed();
}
