#pragma once
#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <cmath>

// Data-only protocol. This class never launches processes or executes messages.
namespace PluginProtocol {
constexpr int Version = 1;
constexpr qsizetype MaxFrameBytes = 32 * 1024;
inline QString namespaceFor(const QString &id) {
    return "plugin." + QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}
inline bool localId(const QString &id) {
    static const QRegularExpression pattern("^[A-Za-z0-9_-]{1,24}$");
    return pattern.match(id).hasMatch();
}
inline bool exactKeys(const QJsonObject &object, const QStringList &allowed) {
    for (auto it=object.begin(); it!=object.end(); ++it) if (!allowed.contains(it.key())) return false;
    return true;
}
class Decoder {
public:
    QList<QJsonObject> feed(const QByteArray &chunk) {
        if (failed) return {};
        if (chunk.size()>2*MaxFrameBytes) return reject();
        buffer += chunk;
        QList<QJsonObject> frames;
        while (true) {
            const auto end = buffer.indexOf('\n');
            if (end<0) break;
            if (end>MaxFrameBytes || frames.size()>=64) return reject();
            const auto line=buffer.left(end); buffer.remove(0,end+1);
            QJsonParseError error;
            const auto document=QJsonDocument::fromJson(line,&error);
            if (error.error!=QJsonParseError::NoError || !document.isObject()) return reject();
            frames.append(document.object());
        }
        if (buffer.size()>MaxFrameBytes) return reject();
        return frames;
    }
    bool failed = false;
private:
    QByteArray buffer;
    QList<QJsonObject> reject() { failed=true; buffer.clear(); return {}; }
};
struct Message {
    QString type, sourceId, key;
    QVariantMap state;
    int priority=0, durationMs=4000, validForMs=15000;
};
class Session {
public:
    Session(QString pluginId, QByteArray token, QSet<QString> pages)
        : id(std::move(pluginId)), secret(std::move(token)), pageIds(std::move(pages)), prefix(namespaceFor(id)) {}
    bool authenticated() const { return ready; }
    QStringList sourceIds() const { return owned.values(); }
    bool accept(const QJsonObject &object, qint64 nowMs, Message &result) {
        result={};
        if (nowMs<0) return false;
        if (nowMs-windowStart>=1000) { windowStart=nowMs; count=0; }
        if (++count>30) return false;
        const auto type=object.value("type").toString();
        if (!ready) {
            if (type!="hello" || !exactKeys(object,{"type","protocolVersion","pluginId","token"})
                || object.value("protocolVersion")!=Version || object.value("pluginId").toString()!=id
                || secret.size()<32 || object.value("token").toString().toUtf8()!=secret) return false;
            ready=true; secret.clear(); result.type="hello"; return true;
        }
        if (type=="closing") { if (!exactKeys(object,{"type"})) return false; result.type=type; return true; }
        if (type!="state" && type!="event" && type!="withdraw") return false;
        if (!exactKeys(object,{"type","source","state","priority","key","durationMs","validForMs"})) return false;
        const auto source=object.value("source").toString();
        if (!localId(source)) return false;
        result.type=type; result.sourceId=prefix+'.'+source;
        if (type=="withdraw") {
            if (!exactKeys(object,{"type","source"})) return false;
            owned.remove(result.sourceId); return true;
        }
        if (!object.value("state").isObject() || !object.value("priority").isDouble()) return false;
        const double priority=object.value("priority").toDouble();
        if (!std::isfinite(priority) || std::floor(priority)!=priority || priority<0 || priority>(type=="event" ? 95 : 60)) return false;
        const auto state=object.value("state").toObject();
        if (!exactKeys(state,{"title","subtitle","detailText","iconName","tone","progress","pageId"})) return false;
        if (!state.value("title").isString() || state.value("title").toString().isEmpty()) return false;
        for (const auto &field:QStringList{"title","subtitle","detailText","iconName","tone","pageId"}) {
            if (!state.contains(field)) continue;
            if (!state.value(field).isString()) return false;
            const auto text=state.value(field).toString();
            const int limit=field=="subtitle" ? 256 : field=="title" ? 128 : 24;
            if (text.size()>limit || text.contains(QChar::Null)) return false;
            if (field!="pageId") result.state.insert(field,text);
        }
        if (state.contains("iconName") && !QRegularExpression("^[a-z0-9_]{1,24}$").match(state.value("iconName").toString()).hasMatch()) return false;
        if (state.contains("tone") && !QStringList{"neutral","success","danger"}.contains(state.value("tone").toString())) return false;
        const auto page=state.value("pageId").toString();
        if (!pageIds.contains(page) || !localId(page)) return false;
        result.state.insert("route",prefix+'.'+page);
        result.state.insert("kind","task");
        if (state.contains("progress")) {
            if (!state.value("progress").isDouble()) return false;
            const auto value=state.value("progress").toDouble();
            if (!std::isfinite(value) || value<0 || value>1) return false;
            result.state.insert("progress",value);
        }
        if (type=="event") {
            result.key=object.value("key").toString();
            if (!QRegularExpression("^[A-Za-z0-9_-]{1,64}$").match(result.key).hasMatch()) return false;
            for (const auto &field:QStringList{"durationMs","validForMs"}) {
                if (!object.contains(field)) continue;
                if (!object.value(field).isDouble()) return false;
                const double value=object.value(field).toDouble();
                if (!std::isfinite(value) || std::floor(value)!=value || value<1000 || value>(field=="durationMs" ? 10000 : 60000)) return false;
                if (field=="durationMs") result.durationMs=int(value); else result.validForMs=int(value);
            }
        } else if (!exactKeys(object,{"type","source","state","priority"})) return false;
        if (owned.size()>=8 && !owned.contains(result.sourceId)) return false;
        owned.insert(result.sourceId); result.priority=int(priority); return true;
    }
private:
    QString id;
    QByteArray secret;
    QSet<QString> pageIds, owned;
    QString prefix;
    bool ready=false;
    qint64 windowStart=0;
    int count=0;
};
}
