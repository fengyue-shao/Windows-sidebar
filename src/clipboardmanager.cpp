#include "include/clipboardmanager.h"

#include <QBuffer>
#include <QGuiApplication>
#include <QImage>
#include <QMetaObject>
#include <QPointer>
#include <QVariantMap>
#include <QSettings>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt;
using namespace Windows::ApplicationModel::DataTransfer;

namespace {
fire_and_forget loadHistory(QPointer<ClipboardManager> manager, const QStringList pinnedIds)
{
    QVariantList items;
    QString statusText;
    try {
        const auto result = co_await Clipboard::GetHistoryItemsAsync();
        switch (result.Status()) {
        case ClipboardHistoryItemsResultStatus::Success: {
            QVariantList regularItems;
            QVariantList pinnedItems;
            for (const auto &historyItem : result.Items()) {
                const auto content = historyItem.Content();
                QVariantMap entry;
                const QString id = QString::fromStdWString(historyItem.Id().c_str());
                entry.insert(QStringLiteral("id"), id);
                entry.insert(QStringLiteral("pinned"), pinnedIds.contains(id));

                // Explorer file copies can also expose a text representation. Prefer
                // the richer formats so they are not flattened into a path string.
                if (content.Contains(StandardDataFormats::StorageItems())) {
                    const auto storageItems = co_await content.GetStorageItemsAsync();
                    QStringList names;
                    for (const auto &storageItem : storageItems)
                        names.append(QString::fromStdWString(storageItem.Name().c_str()));
                    if (names.isEmpty())
                        continue;
                    entry.insert(QStringLiteral("type"), QStringLiteral("files"));
                    entry.insert(QStringLiteral("preview"), names.mid(0, 4).join(QStringLiteral(" · ")));
                    entry.insert(QStringLiteral("detail"), QStringLiteral("%1 个文件或文件夹").arg(names.size()));
                } else if (content.Contains(StandardDataFormats::Bitmap())) {
                    const auto reference = co_await content.GetBitmapAsync();
                    const auto stream = co_await reference.OpenReadAsync();
                    if (stream.Size() > 12 * 1024 * 1024)
                        continue;
                    Windows::Storage::Streams::DataReader reader(stream);
                    const auto byteCount = co_await reader.LoadAsync(static_cast<uint32_t>(stream.Size()));
                    std::vector<uint8_t> bytes(byteCount);
                    reader.ReadBytes(bytes);
                    QImage image = QImage::fromData(bytes.data(), static_cast<int>(bytes.size()));
                    if (image.isNull())
                        continue;
                    image = image.scaled(360, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    QByteArray png;
                    QBuffer buffer(&png);
                    buffer.open(QIODevice::WriteOnly);
                    image.save(&buffer, "PNG");
                    entry.insert(QStringLiteral("type"), QStringLiteral("image"));
                    entry.insert(QStringLiteral("preview"), QStringLiteral("图片 %1 × %2").arg(image.width()).arg(image.height()));
                    entry.insert(QStringLiteral("detail"), QStringLiteral("图片"));
                    entry.insert(QStringLiteral("imageSource"), QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64()));
                } else if (content.Contains(StandardDataFormats::Text())) {
                    QString text = QString::fromStdWString((co_await content.GetTextAsync()).c_str());
                    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
                    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
                    text = text.simplified();
                    if (text.isEmpty())
                        continue;
                    entry.insert(QStringLiteral("type"), QStringLiteral("text"));
                    entry.insert(QStringLiteral("preview"), text.left(240));
                    entry.insert(QStringLiteral("detail"), QStringLiteral("文本"));
                } else {
                    continue;
                }

                if (entry.value(QStringLiteral("pinned")).toBool())
                    pinnedItems.append(entry);
                else
                    regularItems.append(entry);
                if (pinnedItems.size() + regularItems.size() >= 24)
                    break;
            }
            items = pinnedItems;
            items.append(regularItems);
            statusText = items.isEmpty() ? QStringLiteral("历史中没有可预览的内容")
                                         : QStringLiteral("来自 Windows 剪贴板历史");
            break;
        }
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

fire_and_forget activateHistoryItem(QPointer<ClipboardManager> manager, const QString itemId)
{
    QString statusText = QStringLiteral("历史项目已恢复到剪贴板");
    try {
        const auto result = co_await Clipboard::GetHistoryItemsAsync();
        bool found = false;
        if (result.Status() == ClipboardHistoryItemsResultStatus::Success) {
            for (const auto &historyItem : result.Items()) {
                if (QString::fromStdWString(historyItem.Id().c_str()) == itemId) {
                    Clipboard::SetHistoryItemAsContent(historyItem);
                    found = true;
                    break;
                }
            }
        }
        if (!found)
            statusText = QStringLiteral("该历史项目已不可用");
    } catch (const hresult_error &error) {
        statusText = QStringLiteral("恢复失败：") + QString::fromStdWString(error.message().c_str());
    }
    QMetaObject::invokeMethod(qGuiApp, [manager, statusText] {
        if (manager)
            manager->applyHistory(manager->items(), statusText);
    }, Qt::QueuedConnection);
}

fire_and_forget deleteHistoryItem(QPointer<ClipboardManager> manager, const QString itemId)
{
    bool deleted = false;
    QString statusText;
    try {
        const auto result = co_await Clipboard::GetHistoryItemsAsync();
        if (result.Status() == ClipboardHistoryItemsResultStatus::Success) {
            for (const auto &historyItem : result.Items()) {
                if (QString::fromStdWString(historyItem.Id().c_str()) == itemId) {
                    deleted = Clipboard::DeleteItemFromHistory(historyItem);
                    break;
                }
            }
        }
        statusText = deleted ? QStringLiteral("已删除这条剪贴板历史")
                             : QStringLiteral("该历史项目已不存在或无法删除");
    } catch (const hresult_error &error) {
        statusText = QStringLiteral("删除失败：") + QString::fromStdWString(error.message().c_str());
    }
    QMetaObject::invokeMethod(qGuiApp, [manager, itemId, deleted, statusText] {
        if (manager)
            manager->applyDeleteResult(itemId, deleted, statusText);
    }, Qt::QueuedConnection);
}

fire_and_forget clearUnpinnedHistory(QPointer<ClipboardManager> manager, const QStringList pinnedIds)
{
    QStringList deletedIds;
    int failedCount = 0;
    QString statusText;
    try {
        const auto result = co_await Clipboard::GetHistoryItemsAsync();
        if (result.Status() == ClipboardHistoryItemsResultStatus::Success) {
            for (const auto &historyItem : result.Items()) {
                const QString id = QString::fromStdWString(historyItem.Id().c_str());
                if (pinnedIds.contains(id)) continue;
                if (Clipboard::DeleteItemFromHistory(historyItem)) deletedIds.append(id);
                else ++failedCount;
            }
            statusText = failedCount > 0
                ? QStringLiteral("已清除 %1 条；%2 条删除失败，失败及固定项目保留").arg(deletedIds.size()).arg(failedCount)
                : !deletedIds.isEmpty() ? QStringLiteral("已清除 %1 条未固定历史，固定项目已保留").arg(deletedIds.size())
                : QStringLiteral("没有可清除的未固定历史");
        } else {
            statusText = QStringLiteral("无法读取剪贴板历史");
        }
    } catch (const hresult_error &error) {
        statusText = QStringLiteral("已清除 %1 条，其余清理失败：").arg(deletedIds.size()) + QString::fromStdWString(error.message().c_str());
    }
    QMetaObject::invokeMethod(qGuiApp, [manager, deletedIds, statusText] {
        if (manager)
            manager->applyClearResult(deletedIds, statusText);
    }, Qt::QueuedConnection);
}
}

ClipboardManager::ClipboardManager(QObject *parent, bool persist)
    : QObject(parent), m_persist(persist)
{
    if (m_persist) m_pinnedIds = QSettings().value(QStringLiteral("clipboard/pinnedIds")).toStringList();
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
    loadHistory(this, m_pinnedIds);
}

void ClipboardManager::activateItem(const QString &itemId)
{
    activateHistoryItem(this, itemId);
}

void ClipboardManager::togglePinned(const QString &itemId)
{
    if (m_pinnedIds.contains(itemId))
        m_pinnedIds.removeAll(itemId);
    else
        m_pinnedIds.prepend(itemId);
    if (m_persist) QSettings().setValue(QStringLiteral("clipboard/pinnedIds"), m_pinnedIds);
    m_statusText = m_pinnedIds.contains(itemId) ? QStringLiteral("已在应用内置顶")
                                                : QStringLiteral("已取消应用内置顶");
    emit statusChanged();
    refresh();
}

void ClipboardManager::deleteItem(const QString &itemId)
{
    deleteHistoryItem(this, itemId);
}

void ClipboardManager::clearHistory()
{
    clearUnpinnedHistory(this, m_pinnedIds);
}

QStringList ClipboardManager::pinnedIds() const
{
    return m_pinnedIds;
}

void ClipboardManager::applyDeleteResult(const QString &itemId, bool deleted, const QString &statusText)
{
    if (deleted) {
        m_pinnedIds.removeAll(itemId);
        if (m_persist) QSettings().setValue(QStringLiteral("clipboard/pinnedIds"), m_pinnedIds);
        for (qsizetype index = m_items.size() - 1; index >= 0; --index) {
            if (m_items.at(index).toMap().value(QStringLiteral("id")).toString() == itemId)
                m_items.removeAt(index);
        }
        emit itemsChanged();
    }
    if (m_statusText != statusText) {
        m_statusText = statusText;
        emit statusChanged();
    }
}

void ClipboardManager::applyClearResult(const QStringList &deletedIds, const QString &statusText)
{
    if (!deletedIds.isEmpty()) {
        for (qsizetype index = m_items.size() - 1; index >= 0; --index) {
            if (deletedIds.contains(m_items.at(index).toMap().value(QStringLiteral("id")).toString()))
                m_items.removeAt(index);
        }
        emit itemsChanged();
    }
    if (m_statusText != statusText) {
        m_statusText = statusText;
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
