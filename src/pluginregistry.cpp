#include "include/pluginregistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>
#include <algorithm>

PluginRegistry::PluginRegistry(QObject *parent) : QObject(parent)
{
    reload();
}

void PluginRegistry::reload()
{
    QVariantList discovered;
    QStringList errors;
    const QDir root(QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"));
    const auto pluginDirectories = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &directoryInfo : pluginDirectories) {
        const QDir directory(directoryInfo.absoluteFilePath());
        QFile manifestFile(directory.filePath(QStringLiteral("plugin.json")));
        if (!manifestFile.open(QIODevice::ReadOnly)) {
            errors.append(directoryInfo.fileName() + QStringLiteral(": 缺少 plugin.json"));
            continue;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
        const QJsonObject manifest = document.object();
        const QString pluginId = manifest.value(QStringLiteral("id")).toString();
        const QString pluginName = manifest.value(QStringLiteral("name")).toString();
        if (parseError.error != QJsonParseError::NoError || pluginId.isEmpty() || pluginName.isEmpty()
            || manifest.value(QStringLiteral("schemaVersion")).toInt() != 1) {
            errors.append(directoryInfo.fileName() + QStringLiteral(": 清单无效"));
            continue;
        }
        if (!QSettings().value(QStringLiteral("plugins/%1/enabled").arg(pluginId), true).toBool())
            continue;

        const QJsonArray pages = manifest.value(QStringLiteral("contributes")).toObject()
                                     .value(QStringLiteral("pages")).toArray();
        for (const QJsonValue &pageValue : pages) {
            const QJsonObject page = pageValue.toObject();
            const QString pageId = page.value(QStringLiteral("id")).toString();
            const QString entry = page.value(QStringLiteral("entry")).toString();
            const QFileInfo entryInfo(directory.filePath(entry));
            const QString relativeEntry = QDir(directory.canonicalPath())
                                              .relativeFilePath(entryInfo.canonicalFilePath());
            if (pageId.isEmpty() || entry.isEmpty() || !entryInfo.exists()
                || QDir::isAbsolutePath(relativeEntry) || relativeEntry == QStringLiteral("..")
                || relativeEntry.startsWith(QStringLiteral("../"))) {
                errors.append(pluginId + QStringLiteral(": 页面路径无效"));
                continue;
            }
            QVariantMap descriptor;
            descriptor.insert(QStringLiteral("pluginId"), pluginId);
            descriptor.insert(QStringLiteral("pluginName"), pluginName);
            descriptor.insert(QStringLiteral("id"), pluginId + QLatin1Char('.') + pageId);
            descriptor.insert(QStringLiteral("title"), page.value(QStringLiteral("title")).toString(pluginName));
            descriptor.insert(QStringLiteral("symbol"), page.value(QStringLiteral("symbol")).toString(QStringLiteral("◇")));
            descriptor.insert(QStringLiteral("source"), QUrl::fromLocalFile(entryInfo.canonicalFilePath()));
            descriptor.insert(QStringLiteral("order"), page.value(QStringLiteral("order")).toInt(500));
            discovered.append(descriptor);
        }
    }

    std::sort(discovered.begin(), discovered.end(), [](const QVariant &left, const QVariant &right) {
        return left.toMap().value(QStringLiteral("order")).toInt()
             < right.toMap().value(QStringLiteral("order")).toInt();
    });
    m_pages = discovered;
    m_statusText = errors.isEmpty()
        ? QStringLiteral("已加载 %1 个插件页面").arg(m_pages.size())
        : QStringLiteral("已加载 %1 个页面，%2 个错误").arg(m_pages.size()).arg(errors.size());
    emit pagesChanged();
    emit statusChanged();
}
