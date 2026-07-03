cmake_minimum_required(VERSION 3.16)

project(Seriona VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.8)

include(CTest)

set(SERIONA_BACKEND_SOURCE_DIR "../../cppProject(app_and_lib)/Seriona_Backend" CACHE PATH "Seriona backend source tree")
set(SERIONA_BACKEND_CONTROL_TARGET "")

if(SERIONA_BACKEND_SOURCE_DIR STREQUAL "")
    message(STATUS "SERIONA_BACKEND_SOURCE_DIR is empty; building appSeriona in mock-only mode.")
else()
    get_filename_component(SERIONA_BACKEND_ABSOLUTE_SOURCE_DIR
        "${SERIONA_BACKEND_SOURCE_DIR}"
        ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    if(EXISTS "${SERIONA_BACKEND_ABSOLUTE_SOURCE_DIR}/CMakeLists.txt")
        message(STATUS "Seriona backend source found: ${SERIONA_BACKEND_ABSOLUTE_SOURCE_DIR}")
        set(SERIONA_BUILD_APP OFF CACHE BOOL "Build the standalone Seriona backend app" FORCE)
        add_subdirectory("${SERIONA_BACKEND_ABSOLUTE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/Seriona_Backend" EXCLUDE_FROM_ALL)

        if(TARGET SerionaBackend::control)
            set(SERIONA_BACKEND_CONTROL_TARGET SerionaBackend::control)
        elseif(TARGET seriona_control)
            set(SERIONA_BACKEND_CONTROL_TARGET seriona_control)
        else()
            message(FATAL_ERROR "Seriona backend was added, but neither SerionaBackend::control nor seriona_control exists.")
        endif()

        message(STATUS "Seriona frontend will link backend target: ${SERIONA_BACKEND_CONTROL_TARGET}")
    else()
        message(STATUS "Seriona backend source not found at ${SERIONA_BACKEND_ABSOLUTE_SOURCE_DIR}; building appSeriona in mock-only mode.")
    endif()
endif()

qt_add_executable(appSeriona
    src/main.cpp
)

# 注册 Theme.qml 为 Singleton
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appSeriona
    URI Seriona
    SOURCES
        src/app/app_facade.cpp
        src/app/app_facade.h
        src/app/backend_bridge.cpp
        src/app/backend_bridge.h
        src/app/lyrics_model.cpp
        src/app/lyrics_model.h
        src/app/library_model.cpp
        src/app/library_model.h
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/components/BubbleMenu.qml
        qml/components/BubbleMenuItem.qml
        qml/components/BubbleSubMenuItem.qml
        qml/components/StyleButton.qml
        qml/components/Sidebar.qml
        qml/components/WindowControls.qml
        qml/components/WaveformProgressBar.qml
        qml/components/MarqueeText.qml
        qml/views/MainContent.qml
        qml/views/StartupView.qml
    RESOURCES
        qml/assets/play.svg
        qml/assets/pause.svg
        qml/assets/prev.svg
        qml/assets/next.svg
        qml/assets/shuffle_on.svg
        qml/assets/shuffle_off.svg
        qml/assets/repeat_list.svg
        qml/assets/repeat_one.svg
        qml/assets/repeat_off.svg
        qml/assets/playlist.svg
        qml/assets/settings.svg
        qml/assets/minimize.svg
        qml/assets/maximize.svg
        qml/assets/restore.svg
        qml/assets/close.svg
        qml/assets/volume_down.svg
        qml/assets/volume_up.svg
        qml/assets/back.svg
        qml/assets/arrow_back.svg
        qml/assets/search.svg
        qml/assets/more_vert.svg
        qml/assets/folder.svg
        qml/assets/music_note.svg
        qml/assets/my_location.svg
        qml/assets/translate.svg
)

target_include_directories(appSeriona
    PRIVATE src/app
)

target_compile_definitions(appSeriona
    PRIVATE SERIONA_HAS_BACKEND=$<BOOL:${SERIONA_BACKEND_CONTROL_TARGET}>
)

# target_sources(appSeriona PRIVATE
# )

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appSeriona PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appSeriona
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appSeriona
    PRIVATE Qt6::Quick
)

if(SERIONA_BACKEND_CONTROL_TARGET)
    target_link_libraries(appSeriona
        PRIVATE ${SERIONA_BACKEND_CONTROL_TARGET}
    )
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Quick Test)

    add_executable(seriona_frontend_command_result_mapping
        tests/frontend/adapter/tst_command_result_mapping.cpp
    )
    target_link_libraries(seriona_frontend_command_result_mapping
        PRIVATE Qt6::Test
    )
    add_test(NAME seriona_frontend_command_result_mapping COMMAND seriona_frontend_command_result_mapping)

    add_executable(seriona_frontend_snapshot_mapping
        tests/frontend/adapter/tst_snapshot_mapping.cpp
    )
    target_link_libraries(seriona_frontend_snapshot_mapping
        PRIVATE Qt6::Test
    )
    add_test(NAME seriona_frontend_snapshot_mapping COMMAND seriona_frontend_snapshot_mapping)

    add_executable(seriona_frontend_library_tree_mapping
        tests/frontend/adapter/tst_library_tree_mapping.cpp
    )
    target_link_libraries(seriona_frontend_library_tree_mapping
        PRIVATE Qt6::Test
    )
    add_test(NAME seriona_frontend_library_tree_mapping COMMAND seriona_frontend_library_tree_mapping)

    add_executable(seriona_frontend_app_facade_smoke_mode
        tests/frontend/adapter/tst_app_facade_smoke_mode.cpp
        src/app/app_facade.cpp
        src/app/app_facade.h
        src/app/backend_bridge.cpp
        src/app/backend_bridge.h
    )
    target_include_directories(seriona_frontend_app_facade_smoke_mode
        PRIVATE src/app
    )
    target_compile_definitions(seriona_frontend_app_facade_smoke_mode
        PRIVATE SERIONA_HAS_BACKEND=$<BOOL:${SERIONA_BACKEND_CONTROL_TARGET}>
    )
    target_link_libraries(seriona_frontend_app_facade_smoke_mode
        PRIVATE Qt6::Quick Qt6::Test
    )
    if(SERIONA_BACKEND_CONTROL_TARGET)
        target_link_libraries(seriona_frontend_app_facade_smoke_mode
            PRIVATE ${SERIONA_BACKEND_CONTROL_TARGET}
        )
    endif()
    add_test(NAME seriona_frontend_app_facade_smoke_mode COMMAND seriona_frontend_app_facade_smoke_mode)

    if(SERIONA_BACKEND_CONTROL_TARGET)
        add_executable(seriona_frontend_backend_bridge_tests
            tests/frontend/adapter/tst_backend_bridge.cpp
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
        )
        target_include_directories(seriona_frontend_backend_bridge_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_backend_bridge_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_backend_bridge_tests
            PRIVATE Qt6::Test ${SERIONA_BACKEND_CONTROL_TARGET}
        )

        add_test(NAME seriona_frontend_backend_bridge_threading COMMAND seriona_frontend_backend_bridge_tests threading)
        add_test(NAME seriona_frontend_backend_bridge_shutdown COMMAND seriona_frontend_backend_bridge_tests shutdown)
        add_test(NAME seriona_frontend_backend_bridge_shutdown_stop_sent COMMAND seriona_frontend_backend_bridge_tests shutdownStopSent)
    endif()
endif()

install(TARGETS appSeriona
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
