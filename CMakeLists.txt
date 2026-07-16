cmake_minimum_required(VERSION 3.16)

project(qt_rainmeter VERSION 0.1 LANGUAGES CXX)

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
        SOURCES include/coverimageprovider.h include/cpumonitor.h include/mediamonitor.h include/mediaworker.h include/memorymonitor.h include/monitorbase.h include/networkmonitor.h include/Notification.h include/notificationmodel.h src/coverimageprovider.cpp src/cpumonitor.cpp src/mediamonitor.cpp src/mediaworker.cpp src/memorymonitor.cpp src/monitorbase.cpp src/networkmonitor.cpp src/notificationmodel.cpp
        QML_FILES qml/MediaBar.qml qml/NotificationArea.qml qml/NotificationItem.qml qml/Sidebar.qml qml/Timebar.qml qml/WidgetCard.qml
        RESOURCES assets/icons/ic_fluent_next_48_regular.svg assets/icons/ic_fluent_pause_48_regular.svg assets/icons/ic_fluent_play_48_regular.svg assets/icons/ic_fluent_previous_48_regular.svg
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
target_link_libraries(appqt_rainmeter PRIVATE Qt6::Svg)

include(GNUInstallDirs)
install(TARGETS appqt_rainmeter
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
