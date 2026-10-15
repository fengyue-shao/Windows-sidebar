#include "include/uicardmanager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QRegularExpression>
#include <cmath>
#include <limits>
#include <algorithm>

namespace {
constexpr qreal Gap=16, Grid=8, MaxCanvas=32768;
bool number(const QJsonValue &v,qreal low,qreal high) { const auto n=v.toDouble(); return v.isDouble() && std::isfinite(n) && n>=low && n<=high; }
bool validId(const QString &s) { return QRegularExpression("^[a-z][a-z0-9_.-]{0,63}$").match(s).hasMatch(); }
bool keys(const QJsonObject &o,const QStringList &allowed) { for(auto it=o.begin();it!=o.end();++it) if(!allowed.contains(it.key())) return false; return true; }
QRectF avoid(QRectF r,const QList<QRectF> &occupied) {
    // Deterministic downward displacement; moving a card never rewrites its neighbours.
    for(int i=0;i<=occupied.size();++i) {
        qreal bottom=r.y();
        for(const auto &other:occupied) if(r.adjusted(-Gap/2,-Gap/2,Gap/2,Gap/2).intersects(other)) bottom=qMax(bottom,other.bottom()+Gap);
        if(bottom==r.y()) break;
        r.moveTop(std::ceil(bottom/Grid)*Grid);
    }
    return r;
}
QJsonObject record(const QRectF &r,qreal width,bool shown) { return {{"x",r.x()/width},{"y",r.y()},{"width",r.width()/width},{"height",r.height()},{"visible",shown}}; }
}
UiCardManager::UiCardManager(QObject *parent,const QString &path):QObject(parent),m_path(path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/cards/layouts-v2.json" : path) {
    if(path.isEmpty()) m_legacyPath=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/cards/layouts-v1.json";
    load();
}
bool UiCardManager::reject(const QString &error) { m_error=error; emit errorChanged(); return false; }
void UiCardManager::changed() { m_layoutCache.clear(); m_desktopCache.clear(); ++m_revision; emit layoutChanged(); }
QVariantMap UiCardManager::definition(const QString &id) const { for(const auto &v:m_catalog) if(v.toMap().value("id")==id) return v.toMap(); return {}; }
bool UiCardManager::registerCard(const QVariantMap &value) {
    const auto o=QJsonObject::fromVariantMap(value);
    if(editing() || m_catalog.size()>=64 || !keys(o,{"id","page","title","minimumWidth","minimumHeight","defaultHeight","maximumHeight"})) return reject("卡片定义字段或注册时机无效");
    const auto id=o.value("id").toString(), page=o.value("page").toString();
    if(!validId(id) || !QStringList{"monitor","notifications","focus","clipboard"}.contains(page) || !definition(id).isEmpty()
        || !o.value("title").isString() || o.value("title").toString().isEmpty() || o.value("title").toString().size()>64
        || !number(o.value("minimumWidth"),160,1024) || !number(o.value("minimumHeight"),48,4096)
        || !number(o.value("defaultHeight"),o.value("minimumHeight").toDouble(),4096)
        || !number(o.value("maximumHeight"),o.value("defaultHeight").toDouble(),8192)) return reject("卡片标识或尺寸无效");
    qreal packedHeight=o.value("defaultHeight").toDouble();
    for(const auto &existing:m_catalog) { const auto d=existing.toMap(); if(d.value("page")==page) packedHeight+=qMax(d.value("defaultHeight").toDouble(),m_minimumHeights.value(d.value("id").toString(),0))+Gap; }
    if(packedHeight>MaxCanvas) return reject("该页面卡片超过画布容量");
    m_catalog.append(value); emit catalogChanged(); changed(); return true;
}
QString UiCardManager::layoutKey(const QString &page,qreal width) const { return editing() && page==m_editPage ? m_editKey : page+(width>=620 ? ".wide" : ".narrow"); }
QJsonObject UiCardManager::records(const QString &page,qreal width) const { return editing() && page==m_editPage ? m_draft : m_saved.value(layoutKey(page,width)).toObject(); }
bool UiCardManager::visible(const QString &id,const QJsonObject &rows) const { return m_available.value(id,true) && rows.value(id).toObject().value("visible").toBool(true); }
QHash<QString,QRectF> UiCardManager::resolved(const QString &page,qreal width,bool useSaved) const {
    QHash<QString,QRectF> result; if(!std::isfinite(width) || width<160 || width>4096) return result;
    const auto cache=m_layoutCache.constFind(page);
    if(useSaved && cache!=m_layoutCache.cend() && cache->revision==m_revision && cache->width==width) return cache->rectangles;
    const auto rows=useSaved ? records(page,width) : QJsonObject{}; QList<QRectF> occupied; qreal columnY[2]{0,0}; int index=0;
    for(const auto &v:m_catalog) {
        const auto d=v.toMap(); const auto id=d.value("id").toString(); if(d.value("page")!=page) continue;
        const auto entry=rows.value(id).toObject(); const bool wide=width>=620; const int column=wide ? index%2 : 0; ++index;
        const qreal minimum=qMax(d.value("minimumHeight").toDouble(),m_minimumHeights.value(id,0));
        const qreal defaultWidth=wide ? (width-Gap)/2 : width;
        QRectF r(column*(defaultWidth+Gap),columnY[column],defaultWidth,qMax(minimum,d.value("defaultHeight").toDouble()));
        if(!entry.isEmpty()) r=QRectF(entry.value("x").toDouble()*width,entry.value("y").toDouble(),entry.value("width").toDouble()*width,entry.value("height").toDouble());
        r.setWidth(qBound(qMin(width,d.value("minimumWidth").toDouble()),r.width(),width));
        r.setHeight(qBound(minimum,r.height(),qMax(minimum,d.value("maximumHeight").toDouble())));
        r.moveLeft(qBound(0.0,r.x(),width-r.width())); r.moveTop(qBound(0.0,r.y(),MaxCanvas-r.height()));
        if(visible(id,rows)) { r=avoid(r,occupied); occupied.append(r); columnY[column]=r.bottom()+Gap; }
        if(r.bottom()>MaxCanvas) return useSaved ? resolved(page,width,false) : QHash<QString,QRectF>{};
        result.insert(id,r);
    }
    if(useSaved && !result.isEmpty()) m_layoutCache.insert(page,LayoutCache{width,m_revision,result});
    return result;
}
QVariantMap UiCardManager::geometry(const QString &id,qreal width) const {
    const auto d=definition(id); if(d.isEmpty()) return {};
    const auto all=resolved(d.value("page").toString(),width); if(!all.contains(id)) return {};
    const auto r=all.value(id);
    const auto rows=records(d.value("page").toString(),width);
    return {{"x",r.x()},{"y",r.y()},{"width",r.width()},{"height",r.height()},
            {"visible",visible(id,rows)}, {"available",m_available.value(id,true)},
            {"userVisible",rows.value(id).toObject().value("visible").toBool(true)}};
}
qreal UiCardManager::contentHeight(const QString &page,qreal width) const { const auto all=resolved(page,width); const auto rows=records(page,width); qreal h=0; for(auto it=all.begin();it!=all.end();++it) if(visible(it.key(),rows)) h=qMax(h,it.value().bottom()); return h; }
void UiCardManager::setAvailable(const QString &id,bool value) { if(!definition(id).isEmpty() && m_available.value(id,true)!=value) { m_available.insert(id,value); changed(); } }
bool UiCardManager::beginEdit(const QString &page,qreal width) {
    if(editing() || !std::isfinite(width) || width<160 || width>4096 || resolved(page,width).isEmpty()) return reject("页面无卡片、宽度无效或已有编辑事务");
    const auto all=resolved(page,width); const auto rows=records(page,width); const auto key=layoutKey(page,width);
    m_draft={}; for(auto it=all.begin();it!=all.end();++it) m_draft.insert(it.key(),record(it.value(),width,rows.value(it.key()).toObject().value("visible").toBool(true)));
    m_editPage=page; m_editKey=key; m_editWidth=width; m_error.clear(); emit errorChanged(); emit editingChanged(); changed(); return true;
}
bool UiCardManager::setGeometry(const QString &id,qreal x,qreal y,qreal w,qreal h,qreal width) {
    const auto d=definition(id);
    if(!editing() || m_desktopEditing || d.value("page")!=m_editPage || d.isEmpty() || !std::isfinite(width) || width<160 || width>4096
        || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h) || w<=0 || h<=0 || y>MaxCanvas) return reject("卡片几何或编辑上下文无效");
    const qreal minimum=qMax(d.value("minimumHeight").toDouble(),m_minimumHeights.value(id,0));
    w=qBound(qMin(width,d.value("minimumWidth").toDouble()),std::round(w/Grid)*Grid,width);
    h=qBound(minimum,std::round(h/Grid)*Grid,qMax(minimum,d.value("maximumHeight").toDouble()));
    QRectF r(qBound(0.0,std::round(x/Grid)*Grid,width-w),qBound(0.0,std::round(y/Grid)*Grid,MaxCanvas-h),w,h);
    QList<QRectF> occupied; const auto all=resolved(m_editPage,width);
    for(auto it=all.begin();it!=all.end();++it) if(it.key()!=id && visible(it.key(),m_draft)) occupied.append(it.value());
    r=avoid(r,occupied); if(r.bottom()>MaxCanvas) return reject("布局已超出画布上限");
    m_draft.insert(id,record(r,width,m_draft.value(id).toObject().value("visible").toBool(true))); changed(); return true;
}
bool UiCardManager::setVisible(const QString &id,bool shown) { if(!editing() || m_desktopEditing || definition(id).value("page")!=m_editPage || !m_draft.contains(id)) return reject("只能修改正在编辑页面的卡片"); auto row=m_draft.value(id).toObject(); row["visible"]=shown; m_draft[id]=row; changed(); return true; }
bool UiCardManager::resetDraft() {
    if(!editing()) return reject("尚未进入布局编辑");
    if(m_desktopEditing) {
        m_draft={}; changed();
        const auto defaults=desktopResolved(m_editPage);
        for(auto it=defaults.begin();it!=defaults.end();++it) {
            auto card=it.value();
            if(card.geometry.isEmpty()) card.geometry={0,0,qMin(320.0,m_desktopSize.width()),qMin(definition(it.key()).value("defaultHeight").toDouble(),m_desktopSize.height())};
            m_draft.insert(it.key(),desktopRecord(card));
        }
        changed(); return true;
    }
    m_draft={}; m_layoutCache.clear(); const auto defaults=resolved(m_editPage,m_editWidth);
    for(auto it=defaults.begin();it!=defaults.end();++it) m_draft.insert(it.key(),record(it.value(),m_editWidth,true));
    changed(); return true;
}
bool UiCardManager::saveEdit() {
    if(!editing()) return reject("尚未进入布局编辑");
    if(m_desktopEditing) {
        auto next=m_desktopSaved; next[m_editPage]=m_draft;
        if(!QDir().mkpath(QFileInfo(m_path).absolutePath())) return reject("无法创建布局目录，草稿未丢失");
        QSaveFile file(m_path);
        const auto bytes=QJsonDocument(QJsonObject{{"schemaVersion",2},{"layouts",m_saved},{"desktopLayouts",next}}).toJson();
        if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) return reject("布局保存失败，保留编辑草稿和原配置");
        m_desktopSaved=next; cancelEdit(); m_error.clear(); emit errorChanged(); return true;
    }
    auto next=m_saved; QJsonObject normalized; const auto all=resolved(m_editPage,m_editWidth);
    for(auto it=all.begin();it!=all.end();++it) normalized.insert(it.key(),record(it.value(),m_editWidth,m_draft.value(it.key()).toObject().value("visible").toBool(true)));
    next[m_editKey]=normalized;
    if(!QDir().mkpath(QFileInfo(m_path).absolutePath())) return reject("无法创建布局目录，草稿未丢失");
    QSaveFile file(m_path); const auto bytes=QJsonDocument(QJsonObject{{"schemaVersion",2},{"layouts",next},{"desktopLayouts",m_desktopSaved}}).toJson();
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) return reject("布局保存失败，保留编辑草稿和原配置");
    m_saved=next; cancelEdit(); m_error.clear(); emit errorChanged(); return true;
}
void UiCardManager::cancelEdit() { if(!editing()) return; m_editPage.clear(); m_editKey.clear(); m_draft={}; m_desktopEditing=false; emit editingChanged(); changed(); }
bool UiCardManager::reportMinimumHeight(const QString &id,qreal h) {
    const auto d=definition(id); if(d.isEmpty() || !std::isfinite(h) || h<0 || h>d.value("maximumHeight").toDouble()) return false;
    h=qMin(d.value("maximumHeight").toDouble(),std::ceil(h/Grid)*Grid);
    qreal packedHeight=0;
    for(const auto &v:m_catalog) { const auto other=v.toMap(); if(other.value("page")==d.value("page")) packedHeight+=qMax(other.value("defaultHeight").toDouble(),other.value("id")==id ? h : m_minimumHeights.value(other.value("id").toString(),0))+Gap; }
    if(packedHeight>MaxCanvas) return false;
    if(m_minimumHeights.value(id,0)!=h) { m_minimumHeights.insert(id,h); changed(); } return true;
}
void UiCardManager::load() {
    QFile file(m_path); if(!file.exists() && !m_legacyPath.isEmpty()) file.setFileName(m_legacyPath); if(!file.exists()) return;
    if(!file.open(QIODevice::ReadOnly) || file.size()>512*1024) { reject("布局文件不可读或过大，使用默认布局"); return; }
    QJsonParseError error; const auto document=QJsonDocument::fromJson(file.readAll(),&error); const auto root=document.object();
    const int version=root.value("schemaVersion").toInt();
    if(error.error!=QJsonParseError::NoError || !keys(root,version==2 ? QStringList{"schemaVersion","layouts","desktopLayouts"} : QStringList{"schemaVersion","layouts"}) || (version!=1 && version!=2) || !root.value("layouts").isObject()
        || (version==2 && !root.value("desktopLayouts").isObject())) { reject("布局文件版本或结构无效，使用默认布局"); return; }
    const auto layouts=root.value("layouts").toObject();
    for(auto it=layouts.begin();it!=layouts.end();++it) {
        if(!QRegularExpression("^(monitor|notifications|focus|clipboard)\\.(wide|narrow)$").match(it.key()).hasMatch() || !it.value().isObject() || it.value().toObject().size()>64) { reject("布局页面无效，使用默认布局"); return; }
        const auto rows=it.value().toObject();
        for(auto row=rows.begin();row!=rows.end();++row) { const auto r=row.value().toObject();
            if(!validId(row.key()) || !row.value().isObject() || !keys(r,{"x","y","width","height","visible"}) || !number(r.value("x"),0,1)
                || !number(r.value("y"),0,MaxCanvas) || !number(r.value("width"),0.01,1) || !number(r.value("height"),48,8192) || !r.value("visible").isBool()
                || r.value("x").toDouble()+r.value("width").toDouble()>1.000001 || r.value("y").toDouble()+r.value("height").toDouble()>MaxCanvas) { reject("布局坐标无效，使用默认布局"); return; }
        }
    }
    const auto desktops=root.value("desktopLayouts").toObject();
    for(auto page=desktops.begin();page!=desktops.end();++page) {
        if(!QStringList{"monitor","notifications","focus","clipboard"}.contains(page.key()) || !page.value().isObject() || page.value().toObject().size()>64) { reject("自由布局页面无效，使用默认布局"); return; }
        const auto rows=page.value().toObject();
        for(auto row=rows.begin();row!=rows.end();++row) {
            const auto r=row.value().toObject();
            if(!validId(row.key()) || !row.value().isObject() || !keys(r,{"x","y","width","height","enabled","pinned"})
                || !number(r.value("x"),0,1) || !number(r.value("y"),0,1) || !number(r.value("width"),0.001,1) || !number(r.value("height"),0.001,1)
                || !r.value("enabled").isBool() || !r.value("pinned").isBool()
                || r.value("x").toDouble()+r.value("width").toDouble()>1.000001 || r.value("y").toDouble()+r.value("height").toDouble()>1.000001) { reject("自由布局状态无效，使用默认布局"); return; }
        }
    }
    m_saved=layouts; m_desktopSaved=desktops;
}

void UiCardManager::configureDesktop(qreal width,qreal height,const QVariantList &reserved) {
    if(!std::isfinite(width) || !std::isfinite(height) || width<160 || height<120 || width>16384 || height>16384 || reserved.size()>16) return;
    QList<QRectF> areas;
    for(const auto &v:reserved) {
        const auto r=v.toMap(); const QRectF rect(r.value("x").toDouble(),r.value("y").toDouble(),r.value("width").toDouble(),r.value("height").toDouble());
        if(!std::isfinite(rect.x()) || !std::isfinite(rect.y()) || !std::isfinite(rect.width()) || !std::isfinite(rect.height())) return;
        if(rect.width()>0 && rect.height()>0) areas.append(rect.intersected(QRectF(0,0,width,height)));
    }
    if(m_desktopSize==QSizeF(width,height) && areas==m_reserved) return;
    m_desktopSize={width,height}; m_reserved=areas; changed();
}
QJsonObject UiCardManager::desktopRecords(const QString &page) const { return m_desktopEditing && m_editPage==page ? m_draft : m_desktopSaved.value(page).toObject(); }
QJsonObject UiCardManager::desktopRecord(const UiCard &card) const {
    return {{"x",card.geometry.x()/m_desktopSize.width()},{"y",card.geometry.y()/m_desktopSize.height()},
            {"width",card.geometry.width()/m_desktopSize.width()},{"height",card.geometry.height()/m_desktopSize.height()},
            {"enabled",card.enabled},{"pinned",card.pinned}};
}
QHash<QString,UiCard> UiCardManager::desktopResolved(const QString &page) const {
    if(m_desktopSize.isEmpty()) return {};
    if(m_desktopCache.contains(page)) return m_desktopCache.value(page);
    QHash<QString,UiCard> result; QList<QRectF> occupied=m_reserved; const auto rows=desktopRecords(page);
    auto ordered=m_catalog;
    // Give existing pinned positions first refusal when a card is restored or the screen changes.
    std::stable_sort(ordered.begin(),ordered.end(),[&](const QVariant &a,const QVariant &b) {
        return rows.value(a.toMap().value("id").toString()).toObject().value("pinned").toBool(false)
            > rows.value(b.toMap().value("id").toString()).toObject().value("pinned").toBool(false);
    });
    for(const auto &v:ordered) {
        const auto d=v.toMap(); if(d.value("page")!=page) continue;
        const auto id=d.value("id").toString(); const auto entry=rows.value(id).toObject(); UiCard card;
        card.enabled=entry.value("enabled").toBool(true); card.pinned=entry.value("pinned").toBool(false);
        const qreal w=qMin(m_desktopSize.width(),qMax(d.value("minimumWidth").toDouble(),entry.isEmpty() ? 320.0 : entry.value("width").toDouble()*m_desktopSize.width()));
        const qreal h=qMin(m_desktopSize.height(),qBound(d.value("minimumHeight").toDouble(),entry.isEmpty() ? d.value("defaultHeight").toDouble() : entry.value("height").toDouble()*m_desktopSize.height(),d.value("maximumHeight").toDouble()));
        QRectF desired(qBound(0.0,entry.value("x").toDouble()*m_desktopSize.width(),m_desktopSize.width()-w),qBound(0.0,entry.value("y").toDouble()*m_desktopSize.height(),m_desktopSize.height()-h),w,h);
        auto fits=[&](const QRectF &r) { if(!QRectF(QPointF(0,0),m_desktopSize).contains(r)) return false; for(const auto &other:occupied) if(r.intersects(other)) return false; return true; };
        card.geometry=desired;
        if(card.enabled && m_available.value(id,true)) {
            if(!fits(desired)) {
                QList<qreal> xs{0,desired.x()},ys{0,desired.y()};
                for(const auto &other:occupied) { xs.append(other.right()+8); ys.append(other.bottom()+8); }
                qreal distance=std::numeric_limits<qreal>::max(); QRectF best;
                for(qreal y:ys) for(qreal x:xs) { const QRectF candidate(x,y,w,h); if(!fits(candidate)) continue; const qreal score=std::abs(x-desired.x())+std::abs(y-desired.y()); if(score<distance) { best=candidate; distance=score; } }
                card.geometry=best; // Empty means no safe slot, never stack or leave screen bounds.
            }
            if(!card.geometry.isEmpty()) occupied.append(card.geometry);
        }
        result.insert(id,card);
    }
    m_desktopCache.insert(page,result); return result;
}
QVariantMap UiCardManager::desktopGeometry(const QString &id) const {
    const auto d=definition(id); if(d.isEmpty()) return {};
    const auto cards=desktopResolved(d.value("page").toString()); if(!cards.contains(id)) return {};
    const auto card=cards.value(id); const auto r=card.geometry;
    return {{"x",r.x()},{"y",r.y()},{"width",r.width()},{"height",r.height()},
            {"enabled",card.enabled},{"pinned",card.pinned},{"available",m_available.value(id,true)},
            {"placed",!r.isEmpty()},{"visible",card.enabled && m_available.value(id,true) && !r.isEmpty()}};
}
bool UiCardManager::hasPinnedCards() const {
    for(const auto &v:m_catalog) { const auto g=desktopGeometry(v.toMap().value("id").toString()); if(g.value("visible").toBool() && g.value("pinned").toBool()) return true; }
    return false;
}
bool UiCardManager::beginDesktopEdit(const QString &page) {
    if(editing() || m_desktopSize.isEmpty()) return reject("屏幕范围未就绪或已有编辑事务");
    const auto cards=desktopResolved(page); if(cards.isEmpty()) return reject("页面没有可编辑卡片");
    m_draft={};
    for(auto it=cards.begin();it!=cards.end();++it) {
        auto card=it.value(); if(card.geometry.isEmpty()) card.geometry={0,0,qMin(320.0,m_desktopSize.width()),qMin(definition(it.key()).value("defaultHeight").toDouble(),m_desktopSize.height())};
        m_draft[it.key()]=desktopRecord(card);
    }
    m_desktopEditing=true; m_editPage=page; m_error.clear(); emit errorChanged(); emit editingChanged(); changed(); return true;
}
bool UiCardManager::canPlaceDesktop(const QString &id,qreal x,qreal y,qreal width,qreal height) const {
    const auto d=definition(id);
    if(!m_desktopEditing || d.isEmpty() || d.value("page")!=m_editPage || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height)
        || width<qMin(m_desktopSize.width(),d.value("minimumWidth").toDouble()) || height<qMin(m_desktopSize.height(),d.value("minimumHeight").toDouble()) || height>d.value("maximumHeight").toDouble()) return false;
    const QRectF r(x,y,width,height); if(!QRectF(QPointF(0,0),m_desktopSize).contains(r)) return false;
    for(const auto &other:m_reserved) if(r.intersects(other)) return false;
    const auto all=desktopResolved(m_editPage);
    for(auto it=all.begin();it!=all.end();++it) if(it.key()!=id && it->enabled && m_available.value(it.key(),true) && r.intersects(it->geometry)) return false;
    return true;
}
bool UiCardManager::setDesktopGeometry(const QString &id,qreal x,qreal y,qreal width,qreal height) {
    if(!canPlaceDesktop(id,x,y,width,height)) return reject("位置或尺寸超出屏幕、遮挡编辑工具或与其他卡片重叠");
    const auto old=m_draft.value(id).toObject();
    m_draft[id]=desktopRecord(UiCard{QRectF(x,y,width,height),old.value("enabled").toBool(true),old.value("pinned").toBool(false)});
    m_error.clear(); emit errorChanged(); changed(); return true;
}
bool UiCardManager::setDesktopFlag(const QString &id,const QString &flag,bool value) {
    if(!m_desktopEditing || definition(id).value("page")!=m_editPage || !m_draft.contains(id)) return reject("只能修改当前编辑页面的卡片");
    auto record=m_draft.value(id).toObject(); record[flag]=value; m_draft[id]=record; changed(); return true;
}
bool UiCardManager::setEnabled(const QString &id,bool value) { return setDesktopFlag(id,"enabled",value); }
bool UiCardManager::setPinned(const QString &id,bool value) { return setDesktopFlag(id,"pinned",value); }
