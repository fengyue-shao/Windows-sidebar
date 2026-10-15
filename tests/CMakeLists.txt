# Host-only tests: no optional plugin implementation sources are required.
set(SIDEBAR_HOST_ROOT "${PROJECT_SOURCE_DIR}")
set(SIDEBAR_HOST_SRC "${SIDEBAR_HOST_ROOT}/src")
set(SIDEBAR_HOST_INCLUDE "${SIDEBAR_HOST_ROOT}/include")
set(SIDEBAR_HOST_QML "${SIDEBAR_HOST_ROOT}/qml")
set(SIDEBAR_TEST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
file(MAKE_DIRECTORY "${SIDEBAR_TEST_OUTPUT}")
get_filename_component(SIDEBAR_QT_ROOT "${QT_BIN_DIR}" DIRECTORY)

function(sidebar_host_test_target target)
    cmake_parse_arguments(TEST "" "" "SOURCES;LIBRARIES" ${ARGN})
    qt_add_executable(${target} ${TEST_SOURCES})
    target_include_directories(${target} PRIVATE "${SIDEBAR_HOST_ROOT}")
    target_link_libraries(${target} PRIVATE Qt6::Core ${TEST_LIBRARIES})
    # Preserve build-root/Release locations used by explicit diagnostic scripts.
    set_target_properties(${target} PROPERTIES
        WIN32_EXECUTABLE FALSE RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()

function(sidebar_register_host_test target)
    # One quoted PATH argument goes to cmake -E env, not to an environment-
    # property list. Test executables and their private child fixtures use this Qt.
    add_test(NAME ${target}
        COMMAND "${CMAKE_COMMAND}" -E env
            --unset=SIDEBAR_PLUGIN_ENDPOINT
            --unset=SIDEBAR_PLUGIN_TOKEN
            --unset=SIDEBAR_PLUGIN_ID
            --unset=SIDEBAR_TEST_BAD_TOKEN
            --unset=SIDEBAR_TEST_EXIT
            "PATH=${QT_BIN_DIR};$ENV{PATH}"
            "QT_PLUGIN_PATH=${SIDEBAR_QT_ROOT}/plugins"
            "QT_QPA_PLATFORM_PLUGIN_PATH=${SIDEBAR_QT_ROOT}/plugins/platforms"
            "QML_IMPORT_PATH=${SIDEBAR_QT_ROOT}/qml"
            "QML2_IMPORT_PATH=${SIDEBAR_QT_ROOT}/qml"
            "QT_QPA_PLATFORM=offscreen"
            "QT_QUICK_BACKEND=software"
            "QT_QUICK_CONTROLS_STYLE=Basic"
            "QT_QPA_FONTDIR=$ENV{SystemRoot}/Fonts"
            "QT_FORCE_STDERR_LOGGING=1"
            "SIDEBAR_QML_PROFILE=0"
            "$<TARGET_FILE:${target}>" ${ARGN})
    set_tests_properties(${target} PROPERTIES
        WORKING_DIRECTORY "${SIDEBAR_HOST_ROOT}"
        LABELS "host;isolated" TIMEOUT 60)
endfunction()

if(SIDEBAR_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

    set(card_model_sources
        "${SIDEBAR_HOST_SRC}/uicardmanager.cpp"
        "${SIDEBAR_HOST_INCLUDE}/uicardmanager.h" "${SIDEBAR_HOST_INCLUDE}/uicard.h")
    foreach(target IN ITEMS sidebar-card-manager-test sidebar-desktop-card-manager-test)
        string(REPLACE "sidebar-" "" source "${target}")
        sidebar_host_test_target(${target} SOURCES "${source}.cpp" ${card_model_sources})
        sidebar_register_host_test(${target})
    endforeach()
    sidebar_host_test_target(sidebar-card-canvas-test
        SOURCES card-canvas-test.cpp ${card_model_sources} LIBRARIES Qt6::Quick Qt6::Test)
    sidebar_register_host_test(sidebar-card-canvas-test
        "${CMAKE_CURRENT_SOURCE_DIR}/NativeCardCanvasFixture.qml" "${SIDEBAR_TEST_OUTPUT}/card-canvas.png")
    sidebar_host_test_target(sidebar-desktop-card-canvas-test
        SOURCES desktop-card-canvas-test.cpp ${card_model_sources} LIBRARIES Qt6::Quick Qt6::Test)
    sidebar_register_host_test(sidebar-desktop-card-canvas-test
        "${CMAKE_CURRENT_SOURCE_DIR}/DesktopCardCanvasFixture.qml" "${SIDEBAR_TEST_OUTPUT}/desktop-canvas")
    # Manual benchmark: no machine-dependent timing assertion in default CTest.
    sidebar_host_test_target(sidebar-card-layout-benchmark
        SOURCES card-layout-benchmark.cpp ${card_model_sources} LIBRARIES Qt6::Test)

    set(plugin_host_sources
        "${SIDEBAR_HOST_SRC}/pluginregistry.cpp" "${SIDEBAR_HOST_INCLUDE}/pluginregistry.h"
        "${SIDEBAR_HOST_SRC}/pluginprocess.cpp" "${SIDEBAR_HOST_INCLUDE}/pluginprocess.h"
        "${SIDEBAR_HOST_SRC}/islandbridge.cpp" "${SIDEBAR_HOST_INCLUDE}/islandbridge.h")
    foreach(target IN ITEMS sidebar-plugin-package-test sidebar-plugin-version-test)
        string(REPLACE "sidebar-" "" source "${target}")
        sidebar_host_test_target(${target} SOURCES "${source}.cpp" ${plugin_host_sources}
            LIBRARIES Qt6::Gui Qt6::Network)
        # Private temporary settings and fake/self-built plugin fixtures only.
        sidebar_register_host_test(${target})
    endforeach()
    sidebar_host_test_target(sidebar-plugin-process-test SOURCES plugin-process-test.cpp
        "${SIDEBAR_HOST_SRC}/pluginprocess.cpp" "${SIDEBAR_HOST_INCLUDE}/pluginprocess.h"
        LIBRARIES Qt6::Network)
    sidebar_register_host_test(sidebar-plugin-process-test)
    sidebar_host_test_target(sidebar-plugin-protocol-test SOURCES plugin-protocol-test.cpp)
    sidebar_register_host_test(sidebar-plugin-protocol-test)
    sidebar_host_test_target(sidebar-toolbox-ui-test SOURCES toolbox-ui-test.cpp LIBRARIES Qt6::Quick Qt6::Test)
    sidebar_register_host_test(sidebar-toolbox-ui-test
        "${SIDEBAR_HOST_QML}/ToolboxPage.qml" "${SIDEBAR_TEST_OUTPUT}/toolbox.png")

    sidebar_host_test_target(sidebar-clipboard-result-test SOURCES clipboard-result-test.cpp
        "${SIDEBAR_HOST_SRC}/clipboardmanager.cpp" "${SIDEBAR_HOST_INCLUDE}/clipboardmanager.h"
        LIBRARIES Qt6::Gui windowsapp)
    sidebar_register_host_test(sidebar-clipboard-result-test)
    set(notification_model_sources
        "${SIDEBAR_HOST_SRC}/notificationmodel.cpp" "${SIDEBAR_HOST_INCLUDE}/notificationmodel.h")
    set(notification_view_sources
        "${SIDEBAR_HOST_SRC}/notificationviewmodel.cpp" "${SIDEBAR_HOST_INCLUDE}/notificationviewmodel.h")
    sidebar_host_test_target(sidebar-notification-model-test SOURCES notification-model-test.cpp
        ${notification_model_sources} LIBRARIES Qt6::Test)
    sidebar_register_host_test(sidebar-notification-model-test)
    sidebar_host_test_target(sidebar-notification-listener-test SOURCES notification-listener-test.cpp
        "${SIDEBAR_HOST_SRC}/notificationlistener.cpp" "${SIDEBAR_HOST_INCLUDE}/notificationlistener.h"
        LIBRARIES Qt6::Test windowsapp)
    sidebar_register_host_test(sidebar-notification-listener-test)
    sidebar_host_test_target(sidebar-notification-view-test SOURCES notification-view-test.cpp
        ${notification_model_sources} ${notification_view_sources} LIBRARIES Qt6::Test)
    sidebar_register_host_test(sidebar-notification-view-test)
    sidebar_host_test_target(sidebar-notification-ui-test SOURCES notification-ui-test.cpp
        ${notification_model_sources} ${notification_view_sources} LIBRARIES Qt6::Quick Qt6::Test)
    sidebar_register_host_test(sidebar-notification-ui-test
        "${SIDEBAR_HOST_QML}/NotificationArea.qml" "${SIDEBAR_TEST_OUTPUT}/notification.png")

    sidebar_host_test_target(sidebar-wifi-state-test SOURCES wifi-state-test.cpp)
    sidebar_register_host_test(sidebar-wifi-state-test)
    sidebar_host_test_target(sidebar-fullscreen-policy-test SOURCES fullscreen-policy-test.cpp)
    sidebar_register_host_test(sidebar-fullscreen-policy-test)
    sidebar_host_test_target(sidebar-hotkey-policy-test SOURCES hotkey-policy-test.cpp LIBRARIES Qt6::Gui)
    sidebar_register_host_test(sidebar-hotkey-policy-test)
    sidebar_host_test_target(sidebar-bluetooth-lifecycle-test SOURCES bluetooth-lifecycle-test.cpp
        "${SIDEBAR_HOST_SRC}/bluetoothmanager.cpp" "${SIDEBAR_HOST_INCLUDE}/bluetoothmanager.h"
        LIBRARIES Qt6::Gui windowsapp)
    sidebar_register_host_test(sidebar-bluetooth-lifecycle-test)
    sidebar_host_test_target(sidebar-native-async-test SOURCES native-async-test.cpp
        "${SIDEBAR_HOST_SRC}/clipboardocr.cpp" "${SIDEBAR_HOST_INCLUDE}/clipboardocr.h"
        "${SIDEBAR_HOST_SRC}/hotspotmanager.cpp" "${SIDEBAR_HOST_INCLUDE}/hotspotmanager.h"
        LIBRARIES Qt6::Gui windowsapp)
    sidebar_register_host_test(sidebar-native-async-test)
    sidebar_host_test_target(sidebar-native-operation-ui-test SOURCES native-operation-ui-test.cpp
        LIBRARIES Qt6::Quick Qt6::Test)
    sidebar_register_host_test(sidebar-native-operation-ui-test
        "${SIDEBAR_HOST_QML}/ClipboardPage.qml" "${SIDEBAR_HOST_QML}/HotspotCard.qml"
        "${SIDEBAR_TEST_OUTPUT}/native-operation.png")
    sidebar_host_test_target(sidebar-usb-result-test SOURCES usb-result-test.cpp
        "${SIDEBAR_HOST_SRC}/usbmanager.cpp" "${SIDEBAR_HOST_INCLUDE}/usbmanager.h"
        LIBRARIES Qt6::Gui windowsapp setupapi cfgmgr32)
    sidebar_register_host_test(sidebar-usb-result-test)
    sidebar_host_test_target(sidebar-audio-output-test SOURCES audio-output-test.cpp)
    sidebar_register_host_test(sidebar-audio-output-test)
    sidebar_host_test_target(sidebar-audio-output-ui-test SOURCES audio-output-ui-test.cpp
        LIBRARIES Qt6::Quick Qt6::Test)
    sidebar_register_host_test(sidebar-audio-output-ui-test
        "${SIDEBAR_HOST_QML}/QuickSettingsPanel.qml" "${SIDEBAR_TEST_OUTPUT}/audio-output.png")
    sidebar_host_test_target(sidebar-island-ui-test SOURCES island-ui-test.cpp
        "${SIDEBAR_HOST_SRC}/islandbridge.cpp" "${SIDEBAR_HOST_INCLUDE}/islandbridge.h"
        LIBRARIES Qt6::Quick Qt6::Svg Qt6::Test)
    sidebar_register_host_test(sidebar-island-ui-test "${SIDEBAR_HOST_QML}" "${SIDEBAR_TEST_OUTPUT}/island.png")
    sidebar_host_test_target(sidebar-island-core-test SOURCES island-core-test.cpp
        "${SIDEBAR_HOST_SRC}/islandbridge.cpp" "${SIDEBAR_HOST_INCLUDE}/islandbridge.h"
        "${SIDEBAR_HOST_SRC}/awakemanager.cpp" "${SIDEBAR_HOST_INCLUDE}/awakemanager.h"
        "${SIDEBAR_HOST_SRC}/focustimer.cpp" "${SIDEBAR_HOST_INCLUDE}/focustimer.h")
    sidebar_register_host_test(sidebar-island-core-test)
    sidebar_host_test_target(sidebar-stability-test SOURCES stability-test.cpp LIBRARIES windowsapp)
    sidebar_register_host_test(sidebar-stability-test)
endif()

if(SIDEBAR_BUILD_INTEGRATION_TESTS)
    # Building these does not run them. No CTest registrations: real device/session
    # observation, loopback capture and media publication require explicit consent.
    sidebar_host_test_target(sidebar-island-test SOURCES island-test.cpp
        "${SIDEBAR_HOST_SRC}/islandbridge.cpp" "${SIDEBAR_HOST_INCLUDE}/islandbridge.h"
        "${SIDEBAR_HOST_SRC}/captureusagemonitor.cpp" "${SIDEBAR_HOST_INCLUDE}/captureusagemonitor.h"
        LIBRARIES ole32 uuid)
    sidebar_host_test_target(sidebar-spectrum-test SOURCES spectrum-test.cpp
        "${SIDEBAR_HOST_SRC}/mediaspectrum.cpp" "${SIDEBAR_HOST_INCLUDE}/mediaspectrum.h"
        LIBRARIES ole32 uuid)
    sidebar_host_test_target(sidebar-media-fixture SOURCES mediafixture.cpp LIBRARIES windowsapp shell32 user32)
    sidebar_host_test_target(sidebar-media-integration SOURCES media-integration.cpp
        "${SIDEBAR_HOST_SRC}/mediaworker.cpp" "${SIDEBAR_HOST_INCLUDE}/mediaworker.h"
        LIBRARIES Qt6::Gui windowsapp)
endif()
