#include "include/clipboardmanager.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QVariantMap>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt;
using namespace Windows::ApplicationModel::DataTransfer;

namespace {
fire_and_forget loadHistory(QPointer<ClipboardManager> manager)
{
    QVariantList items;
    QString statusText;
    try {
        const auto result = co_await Clipboard::GetHistoryItemsAsync();
        switch (result.Status()) {
        case ClipboardHistoryItemsResultStatus::Success:
            for (const auto &historyItem : result.Items()) {
                const auto content = historyItem.Content();
                if (!content.Contains(StandardDataFormats::Text()))
                    continue;
                QString text = QString::fromStdWString((co_await content.GetTextAsync()).c_str());
                text.replace(QLatin1Char('\r'), QLatin1Char(' '));
                text.replace(QLatin1Char('\n'), QLatin1Char(' '));
                text = text.simplified();
                if (text.isEmpty())
                    continue;
                QVariantMap entry;
                entry.insert(QStringLiteral("id"), QString::fromStdWString(historyItem.Id().c_str()));
                entry.insert(QStringLiteral("text"), text);
                entry.insert(QStringLiteral("preview"), text.left(240));
                items.append(entry);
                if (items.size() >= 24)
                    break;
            }
            statusText = items.isEmpty() ? QStringLiteral("历史中没有可显示的文本")
                                         : QStringLiteral("来自 Windows 剪贴板历史");
            break;
        case ClipboardHistoryItemsResultStatus::AccessDenied:
            statusText = QStringLiteral("Windows 拒绝访问剪贴板历史");
            break;
        case ClipboardHistoryItemsResultStatus::ClipboardHistoryDisabled:
            statusText = QStringLiteral("请先在 Windows 中开启剪贴板历史");
            break;
        default:
            statusText = QStringLiteral("无法读取剪贴板历史");
            break;
        }
    } catch (const hresult_error &error) {
        statusText = QStringLiteral("读取失败：") + QString::fromStdWString(error.message().c_str());
    }
    QMetaObject::invokeMethod(qGuiApp, [manager, items, statusText] {
        if (manager)
            manager->applyHistory(items, statusText);
    }, Qt::QueuedConnection);
}
}

ClipboardManager::ClipboardManager(QObject *parent)
    : QObject(parent)
{
}

QVariantList ClipboardManager::items() const { return m_items; }
QString ClipboardManager::statusText() const { return m_statusText; }
bool ClipboardManager::loading() const { return m_loading; }

void ClipboardManager::refresh()
{
    if (m_loading)
        return;
    m_loading = true;
    emit loadingChanged();
    loadHistory(this);
}

void ClipboardManager::copyText(const QString &text)
{
    qGuiApp->clipboard()->setText(text);
    m_statusText = QStringLiteral("已复制到剪贴板");
    emit statusChanged();
}

void ClipboardManager::clearHistory()
{
    try {
        if (Clipboard::ClearHistory()) {
            m_items.clear();
            m_statusText = QStringLiteral("历史已清空，固定项目会保留");
            emit itemsChanged();
            emit statusChanged();
        }
    } catch (const hresult_error &error) {
        m_statusText = QStringLiteral("清空失败：") + QString::fromStdWString(error.message().c_str());
        emit statusChanged();
    }
}

void ClipboardManager::applyHistory(const QVariantList &items, const QString &statusText)
{
    m_loading = false;
    emit loadingChanged();
    if (m_items != items) {
        m_items = items;
        emit itemsChanged();
    }
    if (m_statusText != statusText) {
        m_statusText = statusText;
        emit statusChanged();
    }
}
