cmake_minimum_required(VERSION 3.21)

project(qt_rainmeter VERSION 0.8.2.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Windows Sidebar requires Windows and the MSVC x64 toolchain.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Windows Sidebar requires a 64-bit build matching the x64 MSIX manifest.")
endif()
enable_language(RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Quick Svg Network)
qt_standard_project_setup(REQUIRES 6.8)

# The host has no build-time dependency on optional plugin implementations.
option(SIDEBAR_BUILD_TESTS "Build and register isolated host regression tests" OFF)
option(SIDEBAR_BUILD_INTEGRATION_TESTS "Build opt-in real Windows/device integration helpers (not automatic tests)" OFF)

set(SIDEBAR_MONITOR_SOURCES
    include/periodicmodule.h
    include/monitorbase.h src/monitorbase.cpp
    include/monitorscheduler.h src/monitorscheduler.cpp
    include/cpumonitor.h src/cpumonitor.cpp
    include/gpumonitor.h src/gpumonitor.cpp
    include/memorymonitor.h src/memorymonitor.cpp
    include/diskmonitor.h src/diskmonitor.cpp
    include/networkmonitor.h src/networkmonitor.cpp
    include/networkdetails.h src/networkdetails.cpp
)
set(SIDEBAR_CONTROL_SOURCES
    include/systemtraymanager.h src/systemtraymanager.cpp
    include/audiooutputswitch.h
    include/audioinputmanager.h src/audioinputmanager.cpp
    include/wificonnectionstate.h
    include/wifimanager.h src/wifimanager.cpp
    include/bluetoothmanager.h src/bluetoothmanager.cpp
    include/radiomanager.h src/radiomanager.cpp
    include/hotspotmanager.h src/hotspotmanager.cpp
    include/awakemanager.h src/awakemanager.cpp
    include/usbejectresult.h
    include/usbmanager.h src/usbmanager.cpp
)
set(SIDEBAR_MEDIA_SOURCES
    include/mediamonitor.h src/mediamonitor.cpp
    include/mediaworker.h src/mediaworker.cpp
    include/mediaspectrum.h src/mediaspectrum.cpp
    include/spectrumanalyzer.h
    include/coverimageprovider.h src/coverimageprovider.cpp
    include/captureusagemonitor.h src/captureusagemonitor.cpp
    include/islandbridge.h src/islandbridge.cpp
    include/focustimer.h src/focustimer.cpp
)
set(SIDEBAR_CONTENT_SOURCES
    include/Notification.h
    include/notificationmodel.h src/notificationmodel.cpp
    include/notificationlistener.h src/notificationlistener.cpp
    include/notificationviewmodel.h src/notificationviewmodel.cpp
    include/clipboardmanager.h src/clipboardmanager.cpp
    include/clipboardocr.h src/clipboardocr.cpp
    include/boundedasync.h
)
set(SIDEBAR_SHELL_SOURCES
    include/displaymanager.h src/displaymanager.cpp
    include/dropdowncontroller.h src/dropdowncontroller.cpp
    include/fullscreenpolicy.h
    include/hotkeypolicy.h
    include/thememanager.h src/thememanager.cpp
    include/uisettings.h src/uisettings.cpp
    include/uicard.h
    include/uicardmanager.h src/uicardmanager.cpp
    include/fluenticonprovider.h
)
set(SIDEBAR_PLUGIN_HOST_SOURCES
    include/pluginprotocol.h
    include/pluginregistry.h src/pluginregistry.cpp
    include/pluginprocess.h src/pluginprocess.cpp
)

set(SIDEBAR_QML_FILES
    # Shell, reusable card controls and selected-icon adapter.
    qml/Main.qml qml/Sidebar.qml qml/SystemControlBar.qml qml/Timebar.qml
    qml/CardSurface.qml qml/CardRegions.js qml/CardAction.qml qml/CardSlider.qml
    qml/FluentIcon.qml qml/SystemMetricGauge.qml
    # Default monitoring and the opt-in editable desktop both remain available.
    qml/DropdownMonitor.qml qml/QuickSettingsPanel.qml qml/DiskPanel.qml
    qml/NativeCardCanvas.qml qml/NativeMonitorCards.qml qml/DesktopCardCanvas.qml
    qml/NetworkDetailsCard.qml qml/HotspotCard.qml qml/AwakeCard.qml qml/UsbCard.qml
    # Media, microphone and Dynamic Island presentation.
    qml/AudioLevelBar.qml qml/MicrophoneCard.qml
    qml/MediaBar.qml qml/MediaMarquee.qml qml/MediaSpectrumBars.qml
    qml/IslandSources.qml qml/TopIslandHost.qml
    # Built-in pages and host-owned plugin management.
    qml/NotificationPage.qml qml/NotificationArea.qml qml/NotificationItem.qml
    qml/FocusPage.qml qml/FocusClockCard.qml qml/FocusPresetCard.qml
    qml/ClipboardPage.qml qml/ToolboxPage.qml
    # Existing explicit --ui-self-test hook; not a plugin component.
    qml/MediaBarSelfTest.qml
)
file(GLOB FLUENT_SELECTED_ICONS CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "assets/fluent/*.svg")

qt_add_executable(appqt_rainmeter src/main.cpp)
qt_add_qml_module(appqt_rainmeter
    URI qt_rainmeter
    VERSION 1.0
    QML_FILES ${SIDEBAR_QML_FILES}
    SOURCES
        ${SIDEBAR_MONITOR_SOURCES}
        ${SIDEBAR_CONTROL_SOURCES}
        ${SIDEBAR_MEDIA_SOURCES}
        ${SIDEBAR_CONTENT_SOURCES}
        ${SIDEBAR_SHELL_SOURCES}
        ${SIDEBAR_PLUGIN_HOST_SOURCES}
        include/featuretests.h src/featuretests.cpp
    RESOURCES ${FLUENT_SELECTED_ICONS} assets/fluent/LICENSE
)
target_include_directories(appqt_rainmeter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
configure_file(packaging/version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app-version.rc" @ONLY)
target_sources(appqt_rainmeter PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app-version.rc")
set_target_properties(appqt_rainmeter PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(appqt_rainmeter PRIVATE
    Qt6::Core Qt6::Quick Qt6::Svg Qt6::Network
    windowsapp iphlpapi ws2_32 setupapi cfgmgr32 pdh dxgi ole32 wininet wlanapi user32
)

get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
set(SIDEBAR_MSVC_RUNTIME_DIR "" CACHE PATH "MSVC x64 redistributable CRT folder for packaging")
add_custom_target(deploy_runtime
    COMMAND powershell.exe -NoProfile
        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/deploy-runtime.ps1"
        -Executable "$<TARGET_FILE:appqt_rainmeter>"
        -QtBin "${QT_BIN_DIR}"
        -SourceDir "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS appqt_rainmeter
    COMMENT "Deploying and verifying the host Qt runtime"
    VERBATIM
)
add_custom_target(package_msix
    COMMAND powershell.exe -NoProfile
        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-msix.ps1"
        -Executable "$<TARGET_FILE:appqt_rainmeter>"
        -QtBin "${QT_BIN_DIR}"
        -SourceDir "${CMAKE_CURRENT_SOURCE_DIR}"
        -OutputDir "${CMAKE_CURRENT_BINARY_DIR}/msix"
        -MsvcRuntimeDir "${SIDEBAR_MSVC_RUNTIME_DIR}"
    DEPENDS appqt_rainmeter
    COMMENT "Creating the host-only development MSIX (no installation or certificate trust)"
    VERBATIM
)
# Installation/certificate trust is an explicit user action via install-msix.ps1,
# never a side effect of a build target or of the regression suite.

if(SIDEBAR_BUILD_TESTS)
    enable_testing()
endif()
if(SIDEBAR_BUILD_TESTS OR SIDEBAR_BUILD_INTEGRATION_TESTS)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS appqt_rainmeter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
