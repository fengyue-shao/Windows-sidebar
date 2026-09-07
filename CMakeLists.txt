cmake_minimum_required(VERSION 3.16)

project(qt_rainmeter VERSION 0.8.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 REQUIRED COMPONENTS Quick)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Svg)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appqt_rainmeter
    src/main.cpp
)

qt_add_qml_module(appqt_rainmeter
    URI qt_rainmeter
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        SOURCES include/bluetoothmanager.h include/clipboardmanager.h include/coverimageprovider.h include/cpumonitor.h include/diskmonitor.h include/displaymanager.h include/focustimer.h include/gpumonitor.h include/mediamonitor.h include/mediaworker.h include/memorymonitor.h include/monitorbase.h include/monitorscheduler.h include/networkmonitor.h include/Notification.h include/notificationmodel.h include/periodicmodule.h include/systemtraymanager.h include/thememanager.h include/uisettings.h include/wifimanager.h src/bluetoothmanager.cpp src/clipboardmanager.cpp src/coverimageprovider.cpp src/cpumonitor.cpp src/diskmonitor.cpp src/displaymanager.cpp src/focustimer.cpp src/gpumonitor.cpp src/mediamonitor.cpp src/mediaworker.cpp src/memorymonitor.cpp src/monitorbase.cpp src/monitorscheduler.cpp src/networkmonitor.cpp src/notificationmodel.cpp src/systemtraymanager.cpp src/thememanager.cpp src/uisettings.cpp src/wifimanager.cpp
        QML_FILES qml/ClipboardPage.qml qml/DashboardPage.qml qml/DiskPanel.qml qml/FocusPage.qml qml/MediaBar.qml qml/MediaMarquee.qml qml/NotificationArea.qml qml/NotificationItem.qml qml/QuickSettingsPanel.qml qml/Sidebar.qml qml/SystemControlBar.qml qml/SystemMetricGauge.qml qml/SystemMonitorPanel.qml qml/Timebar.qml qml/WidgetCard.qml
        RESOURCES assets/icons/ic_fluent_next_48_regular.svg assets/icons/ic_fluent_pause_48_regular.svg assets/icons/ic_fluent_play_48_regular.svg assets/icons/ic_fluent_previous_48_regular.svg
        SOURCES include/notificationlistener.h src/notificationlistener.cpp
        SOURCES include/radiomanager.h src/radiomanager.cpp
        SOURCES include/pluginregistry.h src/pluginregistry.cpp
        SOURCES include/terminalmanager.h src/terminalmanager.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appqt_rainmeter PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appqt_rainmeter
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appqt_rainmeter
    PRIVATE Qt6::Quick
    windowsapp
)
target_link_libraries(appqt_rainmeter PRIVATE Qt6::Core)
target_link_libraries(appqt_rainmeter PRIVATE iphlpapi)
target_link_libraries(appqt_rainmeter PRIVATE pdh)
target_link_libraries(appqt_rainmeter PRIVATE dxgi)
target_link_libraries(appqt_rainmeter PRIVATE ole32)
target_link_libraries(appqt_rainmeter PRIVATE wininet)
target_link_libraries(appqt_rainmeter PRIVATE wlanapi)
target_link_libraries(appqt_rainmeter PRIVATE Qt6::Svg)

qt_add_executable(sidebar-terminal-runner
    runner/terminalrunner.cpp
)
target_link_libraries(sidebar-terminal-runner PRIVATE Qt6::Core)
set_target_properties(sidebar-terminal-runner PROPERTIES WIN32_EXECUTABLE FALSE)
add_dependencies(appqt_rainmeter sidebar-terminal-runner)

add_custom_command(TARGET appqt_rainmeter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins"
        "$<TARGET_FILE_DIR:appqt_rainmeter>/plugins"
    COMMENT "Copying trusted prototype plugins"
)

if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

    add_custom_target(package_msix
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-msix.ps1"
            -Executable "$<TARGET_FILE:appqt_rainmeter>"
            -QtBin "${QT_BIN_DIR}"
            -SourceDir "${CMAKE_CURRENT_SOURCE_DIR}"
            -OutputDir "${CMAKE_CURRENT_BINARY_DIR}/msix"
        DEPENDS appqt_rainmeter
        COMMENT "Deploying Qt and creating a signed MSIX package"
        VERBATIM
    )

    add_custom_target(install_msix
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install-msix.ps1"
            -PackagePath "${CMAKE_CURRENT_BINARY_DIR}/msix/WindowsSidebar.msix"
            -TrustDevelopmentCertificate
        DEPENDS package_msix
        COMMENT "Installing the Windows Sidebar MSIX package"
        VERBATIM
    )
endif()

include(GNUInstallDirs)
install(TARGETS appqt_rainmeter
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
