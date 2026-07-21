#include <QtQml/qqmlprivate.h>
#include <QtCore/qdir.h>
#include <QtCore/qurl.h>
#include <QtCore/qhash.h>
#include <QtCore/qstring.h>

namespace QmlCacheGeneratedCode {
namespace _qt_qml_qt_rainmeter_qml_Main_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_qt_rainmeter_qml_MediaBar_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_qt_rainmeter_qml_NotificationArea_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_qt_rainmeter_qml_NotificationItem_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_qt_rainmeter_qml_Sidebar_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_qt_rainmeter_qml_Timebar_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_qt_rainmeter_qml_WidgetCard_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}

}
namespace {
struct Registry {
    Registry();
    ~Registry();
    QHash<QString, const QQmlPrivate::CachedQmlUnit*> resourcePathToCachedUnit;
    static const QQmlPrivate::CachedQmlUnit *lookupCachedUnit(const QUrl &url);
};

Q_GLOBAL_STATIC(Registry, unitRegistry)


Registry::Registry() {
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/qt_rainmeter/qml/Main.qml"), &QmlCacheGeneratedCode::_qt_qml_qt_rainmeter_qml_Main_qml::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/qt_rainmeter/qml/MediaBar.qml"), &QmlCacheGeneratedCode::_qt_qml_qt_rainmeter_qml_MediaBar_qml::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/qt_rainmeter/qml/NotificationArea.qml"), &QmlCacheGeneratedCode::_qt_qml_qt_rainmeter_qml_NotificationArea_qml::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/qt_rainmeter/qml/NotificationItem.qml"), &QmlCacheGeneratedCode::_qt_qml_qt_rainmeter_qml_NotificationItem_qml::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/qt_rainmeter/qml/Sidebar.qml"), &QmlCacheGeneratedCode::_qt_qml_qt_rainmeter_qml_Sidebar_qml::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/qt_rainmeter/qml/Timebar.qml"), &QmlCacheGeneratedCode::_qt_qml_qt_rainmeter_qml_Timebar_qml::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/qt_rainmeter/qml/WidgetCard.qml"), &QmlCacheGeneratedCode::_qt_qml_qt_rainmeter_qml_WidgetCard_qml::unit);
    QQmlPrivate::RegisterQmlUnitCacheHook registration;
    registration.structVersion = 0;
    registration.lookupCachedQmlUnit = &lookupCachedUnit;
    QQmlPrivate::qmlregister(QQmlPrivate::QmlUnitCacheHookRegistration, &registration);
}

Registry::~Registry() {
    QQmlPrivate::qmlunregister(QQmlPrivate::QmlUnitCacheHookRegistration, quintptr(&lookupCachedUnit));
}

const QQmlPrivate::CachedQmlUnit *Registry::lookupCachedUnit(const QUrl &url) {
    if (url.scheme() != QLatin1String("qrc"))
        return nullptr;
    QString resourcePath = QDir::cleanPath(url.path());
    if (resourcePath.isEmpty())
        return nullptr;
    if (!resourcePath.startsWith(QLatin1Char('/')))
        resourcePath.prepend(QLatin1Char('/'));
    return unitRegistry()->resourcePathToCachedUnit.value(resourcePath, nullptr);
}
}
int QT_MANGLE_NAMESPACE(qInitResources_qmlcache_appqt_rainmeter)() {
    ::unitRegistry();
    return 1;
}
Q_CONSTRUCTOR_FUNCTION(QT_MANGLE_NAMESPACE(qInitResources_qmlcache_appqt_rainmeter))
int QT_MANGLE_NAMESPACE(qCleanupResources_qmlcache_appqt_rainmeter)() {
    return 1;
}
