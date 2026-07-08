cmake_minimum_required(VERSION 3.16)

project(Seriona VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(QT_QML_GENERATE_QMLLS_INI ON)

include(FetchContent)

# Release 模式全量优化配置
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        include(ProcessorCount)
        ProcessorCount(N)
        if(N EQUAL 0)
            set(N 4)
        endif()
        
        # LTO 配置：GCC 支持 -flto=N，Clang 只支持 -flto
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            add_compile_options(-O3 -march=native -flto=${N} -DNDEBUG)
            add_link_options(-flto=${N} -fuse-linker-plugin)
        else()
            # Clang: 使用 -flto 不带数字，并行由链接器自动处理
            add_compile_options(-O3 -march=native -flto -DNDEBUG)
            add_link_options(-flto)
        endif()
        
        message(STATUS "Seriona frontend: Release mode with full optimization enabled (LTO jobs: ${N})")
    elseif(MSVC)
        add_compile_options(/O2 /GL /DNDEBUG)
        add_link_options(/LTCG)
        message(STATUS "Seriona frontend: Release mode with full optimization enabled")
    endif()
endif()

# 多线程编译配置（充分利用 32 核 CPU）
if(CMAKE_GENERATOR MATCHES "Ninja")
    # Ninja 自动检测 CPU 核心数，无需额外配置
    message(STATUS "Using Ninja generator - parallel compilation enabled automatically")
elseif(CMAKE_GENERATOR MATCHES "Unix Makefiles")
    include(ProcessorCount)
    ProcessorCount(N)
    if(NOT N EQUAL 0)
        set(CMAKE_BUILD_PARALLEL_LEVEL ${N} CACHE STRING "Number of parallel build jobs")
        message(STATUS "Parallel build enabled: ${N} jobs")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Quick Concurrent QuickDialogs2 Widgets)

qt_standard_project_setup(REQUIRES 6.8)

option(BUILD_TESTING "Build tests" ON)

set(SERIONA_BACKEND_SOURCE_DIR "../Seriona_Backend" CACHE PATH "Seriona backend source tree")
set(SERIONA_BACKEND_CONTROL_TARGET "")
set(SERIONA_BACKEND_AUDIO_TARGET "")
set(SERIONA_BACKEND_BINARY_DIR "")

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
        set(SERIONA_BACKEND_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/Seriona_Backend")
    else()
        message(STATUS "Seriona backend source not found at ${SERIONA_BACKEND_ABSOLUTE_SOURCE_DIR}; fetching from https://github.com/kaizen857/Seriona_Backend.git")
        FetchContent_Populate(seriona_backend
            GIT_REPOSITORY https://github.com/kaizen857/Seriona_Backend.git
            GIT_TAG main
            GIT_SHALLOW TRUE
        )
        set(SERIONA_BACKEND_ABSOLUTE_SOURCE_DIR "${seriona_backend_SOURCE_DIR}")
        set(SERIONA_BACKEND_BINARY_DIR "${seriona_backend_BINARY_DIR}")
    endif()

    if(EXISTS "${SERIONA_BACKEND_ABSOLUTE_SOURCE_DIR}/CMakeLists.txt")
        set(SERIONA_BUILD_APP OFF CACHE BOOL "Build the standalone Seriona backend app" FORCE)
        set(SERIONA_BUILD_TESTS OFF CACHE BOOL "Build Seriona tests" FORCE)
        set(SERIONA_FRONTEND_BUILD_TESTING "${BUILD_TESTING}")
        set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)
        add_subdirectory("${SERIONA_BACKEND_ABSOLUTE_SOURCE_DIR}" "${SERIONA_BACKEND_BINARY_DIR}" EXCLUDE_FROM_ALL)
        set(BUILD_TESTING "${SERIONA_FRONTEND_BUILD_TESTING}" CACHE BOOL "Build tests" FORCE)

        if(TARGET SerionaBackend::control)
            set(SERIONA_BACKEND_CONTROL_TARGET SerionaBackend::control)
        elseif(TARGET seriona_control)
            set(SERIONA_BACKEND_CONTROL_TARGET seriona_control)
        else()
            message(FATAL_ERROR "Seriona backend was added, but neither SerionaBackend::control nor seriona_control exists.")
        endif()

        if(TARGET SerionaBackend::audio)
            set(SERIONA_BACKEND_AUDIO_TARGET SerionaBackend::audio)
        elseif(TARGET seriona_audio)
            set(SERIONA_BACKEND_AUDIO_TARGET seriona_audio)
        else()
            message(FATAL_ERROR "Seriona backend was added, but neither SerionaBackend::audio nor seriona_audio exists.")
        endif()

        if(TARGET SerionaBackend::app)
            set(SERIONA_BACKEND_APP_TARGET SerionaBackend::app)
        elseif(TARGET seriona_app)
            set(SERIONA_BACKEND_APP_TARGET seriona_app)
        else()
            message(FATAL_ERROR "Seriona backend was added, but neither SerionaBackend::app nor seriona_app exists.")
        endif()

        message(STATUS "Seriona frontend will link backend target: ${SERIONA_BACKEND_CONTROL_TARGET}")
    else()
        message(FATAL_ERROR "Seriona backend source is required unless SERIONA_BACKEND_SOURCE_DIR is empty for mock-only mode.")
    endif()
endif()

include(CTest)

if(SERIONA_BACKEND_CONTROL_TARGET)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SerionaFrontendCTestIsolation.cmake" [=[
function(subdirs)
    foreach(directory IN LISTS ARGN)
        if(NOT directory STREQUAL "Seriona_Backend")
            message(FATAL_ERROR "Unexpected CTest subdir '${directory}' in Seriona frontend test tree.")
        endif()
    endforeach()
endfunction()
]=])
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${CMAKE_CURRENT_BINARY_DIR}/SerionaFrontendCTestIsolation.cmake")
endif()

qt_add_executable(appSeriona
    src/main.cpp
)

# 注册 Theme.qml 为 Singleton
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(SERIONA_APP_LAYER_SOURCES
    src/app/app_facade.cpp
    src/app/app_facade.h
    src/app/playback_controller.cpp
    src/app/playback_controller.h
    src/app/navigation_controller.cpp
    src/app/navigation_controller.h
    src/app/backend_snapshot_mapper.cpp
    src/app/backend_snapshot_mapper.h
    src/app/backend_command_adapter.cpp
    src/app/backend_command_adapter.h
    src/app/backend_bridge.cpp
    src/app/backend_bridge.h
    src/app/notification_controller.cpp
    src/app/notification_controller.h
    src/app/waveform_provider.cpp
    src/app/waveform_provider.h
    src/app/lyrics_model.cpp
    src/app/lyrics_model.h
    src/app/library_model.cpp
    src/app/library_model.h
    src/app/library_tree_store.cpp
    src/app/library_tree_store.h
)

set(SERIONA_QML_MODULE_FILES
    qml/Main.qml
    qml/theme/Theme.qml
    qml/components/BubbleMenu.qml
    qml/components/BubbleMenuItem.qml
    qml/components/BubbleSubMenuItem.qml
    qml/components/DynamicBackground.qml
    qml/components/StyleButton.qml
    qml/components/Sidebar.qml
    qml/components/WindowControls.qml
    qml/components/WaveformProgressBar.qml
    qml/components/MarqueeText.qml
    qml/views/MainContent.qml
    qml/views/StartupView.qml
)

set(SERIONA_QML_MODULE_RESOURCES
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

qt_add_qml_module(appSeriona
    URI Seriona
    SOURCES
        ${SERIONA_APP_LAYER_SOURCES}
    QML_FILES
        ${SERIONA_QML_MODULE_FILES}
    RESOURCES
        ${SERIONA_QML_MODULE_RESOURCES}
)

target_include_directories(appSeriona
    PRIVATE src/app
)

target_compile_definitions(appSeriona
    PRIVATE SERIONA_HAS_BACKEND=$<BOOL:${SERIONA_BACKEND_CONTROL_TARGET}>
)

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
    PRIVATE Qt6::Quick Qt6::Concurrent Qt6::QuickDialogs2 Qt6::Widgets
)

if(SERIONA_BACKEND_CONTROL_TARGET)
    target_link_libraries(appSeriona
        PRIVATE ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET} ${SERIONA_BACKEND_APP_TARGET}
    )
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Quick Test)

    add_executable(seriona_frontend_command_result_mapping
        tests/frontend/adapter/tst_command_result_mapping.cpp
        src/app/backend_command_adapter.cpp
        src/app/backend_command_adapter.h
    )
    target_include_directories(seriona_frontend_command_result_mapping
        PRIVATE src/app
    )
    target_compile_definitions(seriona_frontend_command_result_mapping
        PRIVATE SERIONA_HAS_BACKEND=$<BOOL:${SERIONA_BACKEND_CONTROL_TARGET}>
    )
    target_link_libraries(seriona_frontend_command_result_mapping
        PRIVATE Qt6::Test
    )
    if(SERIONA_BACKEND_CONTROL_TARGET)
        target_link_libraries(seriona_frontend_command_result_mapping
            PRIVATE ${SERIONA_BACKEND_CONTROL_TARGET}
        )
    endif()
    add_test(NAME seriona_frontend_command_result_mapping COMMAND seriona_frontend_command_result_mapping)

    add_executable(seriona_frontend_snapshot_mapping
        tests/frontend/adapter/tst_snapshot_mapping.cpp
        src/app/backend_snapshot_mapper.cpp
        src/app/backend_snapshot_mapper.h
    )
    target_include_directories(seriona_frontend_snapshot_mapping
        PRIVATE src/app
    )
    target_compile_definitions(seriona_frontend_snapshot_mapping
        PRIVATE SERIONA_HAS_BACKEND=$<BOOL:${SERIONA_BACKEND_CONTROL_TARGET}>
    )
    target_link_libraries(seriona_frontend_snapshot_mapping
        PRIVATE Qt6::Test
    )
    if(SERIONA_BACKEND_CONTROL_TARGET)
        target_link_libraries(seriona_frontend_snapshot_mapping
            PRIVATE ${SERIONA_BACKEND_CONTROL_TARGET}
        )
    endif()
    add_test(NAME seriona_frontend_snapshot_mapping COMMAND seriona_frontend_snapshot_mapping)

    add_executable(seriona_frontend_library_tree_mapping
        tests/frontend/adapter/tst_library_tree_mapping.cpp
    )
    target_link_libraries(seriona_frontend_library_tree_mapping
        PRIVATE Qt6::Test
    )
    add_test(NAME seriona_frontend_library_tree_mapping COMMAND seriona_frontend_library_tree_mapping)

    if(SERIONA_BACKEND_CONTROL_TARGET)
        add_executable(seriona_frontend_library_tree_model_tests
            tests/frontend/adapter/tst_library_tree_model.cpp
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
        )
        target_include_directories(seriona_frontend_library_tree_model_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_library_tree_model_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_library_tree_model_tests
            PRIVATE Qt6::Quick Qt6::Test ${SERIONA_BACKEND_CONTROL_TARGET}
        )
        add_test(NAME seriona_frontend_library_tree_model COMMAND seriona_frontend_library_tree_model_tests)
        add_test(NAME seriona_frontend_library_tree_invalid_child COMMAND seriona_frontend_library_tree_model_tests invalidChildNodeIdsAreSkipped)

        add_executable(seriona_frontend_library_dual_cursor_tests
            tests/frontend/adapter/tst_library_dual_cursor.cpp
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
        )
        target_include_directories(seriona_frontend_library_dual_cursor_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_library_dual_cursor_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_library_dual_cursor_tests
            PRIVATE Qt6::Quick Qt6::Test ${SERIONA_BACKEND_CONTROL_TARGET}
        )
        add_test(NAME seriona_frontend_library_dual_cursor COMMAND seriona_frontend_library_dual_cursor_tests dualCursorKeepsFocusedAndPlayingSeparate)
        add_test(NAME seriona_frontend_library_dual_follow_playing COMMAND seriona_frontend_library_dual_cursor_tests followCurrentlyPlayingControlsBrowserCursor)
        add_test(NAME seriona_frontend_library_follow_playing COMMAND seriona_frontend_library_dual_cursor_tests followCurrentlyPlayingControlsBrowserCursor)
        add_test(NAME seriona_frontend_library_dual_cursor_refresh COMMAND seriona_frontend_library_dual_cursor_tests refreshKeepsAndFallsBackBrowserState)
        add_test(NAME seriona_frontend_library_dual_cursor_no_commands COMMAND seriona_frontend_library_dual_cursor_tests browsingActionsStayLocalToProjection)

        add_executable(seriona_frontend_sidebar_local_browsing_tests
            tests/frontend/adapter/tst_sidebar_local_browsing.cpp
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
        )
        target_include_directories(seriona_frontend_sidebar_local_browsing_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_sidebar_local_browsing_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_sidebar_local_browsing_tests
            PRIVATE Qt6::Quick Qt6::Test ${SERIONA_BACKEND_CONTROL_TARGET}
        )
        add_test(NAME seriona_frontend_sidebar_search_local COMMAND seriona_frontend_sidebar_local_browsing_tests localSearchFiltersAndFocusesFirstMatch)
        add_test(NAME seriona_frontend_sidebar_search_empty COMMAND seriona_frontend_sidebar_local_browsing_tests emptySearchDoesNotMoveBrowserOrPlayback)
        add_test(NAME seriona_frontend_sidebar_tree_local COMMAND seriona_frontend_sidebar_local_browsing_tests enterFolderAndBackStayLocal)
        add_test(NAME seriona_frontend_sidebar_root_select_no_back COMMAND seriona_frontend_sidebar_local_browsing_tests selectingRootProjectionNodeDoesNotEnableBack)

        add_executable(seriona_frontend_ui_only_handler_policy_tests
            tests/frontend/adapter/tst_ui_only_handler_policy.cpp
        )
        target_compile_definitions(seriona_frontend_ui_only_handler_policy_tests
            PRIVATE SERIONA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        target_link_libraries(seriona_frontend_ui_only_handler_policy_tests
            PRIVATE Qt6::Test
        )
        add_test(NAME seriona_frontend_ui_only_no_backend_command COMMAND seriona_frontend_ui_only_handler_policy_tests uiOnlyHandlersDoNotUseBackendCommands)
        add_test(NAME seriona_frontend_qml_layout COMMAND seriona_frontend_ui_only_handler_policy_tests qmlLayoutSourceContractsStayStable)

        add_executable(seriona_frontend_library_select_track_tests
            tests/frontend/adapter/tst_library_select_track.cpp
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
        )
        target_include_directories(seriona_frontend_library_select_track_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_library_select_track_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_library_select_track_tests
            PRIVATE Qt6::Quick Qt6::Test ${SERIONA_BACKEND_CONTROL_TARGET}
        )
        add_test(NAME seriona_frontend_library_select_track COMMAND seriona_frontend_library_select_track_tests explicitTrackNodeActivationSubmitsSelectTrack)
        add_test(NAME seriona_frontend_library_select_track_by_index COMMAND seriona_frontend_library_select_track_tests indexedTrackActivationSubmitsSelectTrack)
        add_test(NAME seriona_frontend_library_select_locate_projection COMMAND seriona_frontend_library_select_track_tests locateCurrentSongMovesToContainingProjection)
        add_test(NAME seriona_frontend_library_select_locate_missing COMMAND seriona_frontend_library_select_track_tests locateMissingCurrentSongPreservesBrowserState)
        add_test(NAME seriona_frontend_library_select_browse_no_command COMMAND seriona_frontend_library_select_track_tests browsingAndLocateCurrentSongDoNotSubmitCommands)
        add_test(NAME seriona_frontend_library_browse_no_command COMMAND seriona_frontend_library_select_track_tests browsingAndLocateCurrentSongDoNotSubmitCommands)

        add_executable(seriona_frontend_library_scan_flow_tests
            tests/frontend/adapter/tst_library_scan_flow.cpp
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
        )
        target_include_directories(seriona_frontend_library_scan_flow_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_library_scan_flow_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_library_scan_flow_tests
            PRIVATE Qt6::Quick Qt6::Test ${SERIONA_BACKEND_CONTROL_TARGET}
        )
add_test(NAME seriona_frontend_library_scan_request COMMAND seriona_frontend_library_scan_flow_tests scanLibraryRequestSubmitsBackendScanAndUpdatesTree)
add_test(NAME seriona_frontend_library_scan_cancel_invalid COMMAND seriona_frontend_library_scan_flow_tests cancelAndInvalidSelectionDoNotScanOrSaveRoot)
add_test(NAME seriona_frontend_library_scan_state_mapping COMMAND seriona_frontend_library_scan_flow_tests scanSnapshotsMapUiStates)
add_test(NAME seriona_frontend_library_scan_empty_state COMMAND seriona_frontend_library_scan_flow_tests emptySnapshotExposesEmptyLibraryState)
add_test(NAME seriona_frontend_library_scan_backend_unavailable COMMAND seriona_frontend_library_scan_flow_tests backendUnavailableDoesNotFabricateRows)
    endif()

        add_executable(seriona_frontend_app_facade_smoke_mode
            tests/frontend/adapter/tst_app_facade_smoke_mode.cpp
            src/app/app_facade.cpp
            src/app/app_facade.h
            src/app/playback_controller.cpp
            src/app/playback_controller.h
            src/app/navigation_controller.cpp
            src/app/navigation_controller.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
            src/app/backend_command_adapter.cpp
            src/app/backend_command_adapter.h
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/notification_controller.cpp
            src/app/notification_controller.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
        )
    target_include_directories(seriona_frontend_app_facade_smoke_mode
        PRIVATE src/app
    )
    target_compile_definitions(seriona_frontend_app_facade_smoke_mode
        PRIVATE SERIONA_HAS_BACKEND=$<BOOL:${SERIONA_BACKEND_CONTROL_TARGET}>
    )
        target_link_libraries(seriona_frontend_app_facade_smoke_mode
            PRIVATE Qt6::Quick Qt6::Test Qt6::Concurrent
        )
        if(SERIONA_BACKEND_CONTROL_TARGET)
            target_link_libraries(seriona_frontend_app_facade_smoke_mode
                PRIVATE ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET} ${SERIONA_BACKEND_APP_TARGET}
            )
        endif()
    add_test(NAME seriona_frontend_app_facade_smoke_mode COMMAND seriona_frontend_app_facade_smoke_mode)

    if(SERIONA_BACKEND_CONTROL_TARGET)
        add_executable(seriona_frontend_startup_restore_tests
            tests/frontend/adapter/tst_startup_restore.cpp
            src/app/app_facade.cpp
            src/app/app_facade.h
            src/app/playback_controller.cpp
            src/app/playback_controller.h
            src/app/navigation_controller.cpp
            src/app/navigation_controller.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
            src/app/backend_command_adapter.cpp
            src/app/backend_command_adapter.h
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/notification_controller.cpp
            src/app/notification_controller.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
        )
        target_include_directories(seriona_frontend_startup_restore_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_startup_restore_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_startup_restore_tests
            PRIVATE Qt6::Quick Qt6::Test Qt6::Concurrent ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET} ${SERIONA_BACKEND_APP_TARGET}
        )

        add_test(NAME seriona_frontend_startup_persist_root COMMAND seriona_frontend_startup_restore_tests scanLibraryPersistsRootForStartupRestore)
        add_test(NAME seriona_frontend_startup_restore_root COMMAND seriona_frontend_startup_restore_tests restoreSavedRootScansAndEntersMainShell)
        add_test(NAME seriona_frontend_startup_missing_root COMMAND seriona_frontend_startup_restore_tests missingSavedRoot)

        add_executable(seriona_frontend_playback_snapshot_tests
            tests/frontend/adapter/tst_playback_snapshot_mapping.cpp
            src/app/app_facade.cpp
            src/app/app_facade.h
            src/app/playback_controller.cpp
            src/app/playback_controller.h
            src/app/navigation_controller.cpp
            src/app/navigation_controller.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
            src/app/backend_command_adapter.cpp
            src/app/backend_command_adapter.h
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/notification_controller.cpp
            src/app/notification_controller.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
        )
        target_include_directories(seriona_frontend_playback_snapshot_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_playback_snapshot_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_playback_snapshot_tests
            PRIVATE Qt6::Quick Qt6::Test Qt6::Concurrent ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET}
        )

        add_test(NAME seriona_frontend_playback_snapshot_mapping COMMAND seriona_frontend_playback_snapshot_tests snapshotMapping)
        add_test(NAME seriona_frontend_playback_empty_snapshot COMMAND seriona_frontend_playback_snapshot_tests emptySnapshot)
        add_test(NAME seriona_frontend_playback_timeline_smoothing COMMAND seriona_frontend_playback_snapshot_tests timelineSmoothing)
        add_test(NAME seriona_frontend_playback_timeline_stopped_error COMMAND seriona_frontend_playback_snapshot_tests timelineStoppedAndErrorSnapToBackend)

        add_executable(seriona_frontend_playback_command_tests
            tests/frontend/adapter/tst_playback_command_mapping.cpp
            src/app/app_facade.cpp
            src/app/app_facade.h
            src/app/playback_controller.cpp
            src/app/playback_controller.h
            src/app/navigation_controller.cpp
            src/app/navigation_controller.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
            src/app/backend_command_adapter.cpp
            src/app/backend_command_adapter.h
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/notification_controller.cpp
            src/app/notification_controller.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
        )
        target_include_directories(seriona_frontend_playback_command_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_playback_command_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_playback_command_tests
            PRIVATE Qt6::Quick Qt6::Test Qt6::Concurrent ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET} ${SERIONA_BACKEND_APP_TARGET}
        )

        add_test(NAME seriona_frontend_playback_command_mapping COMMAND seriona_frontend_playback_command_tests visiblePlaybackControlsSubmitCommands)
        add_test(NAME seriona_frontend_playback_rejected_command COMMAND seriona_frontend_playback_command_tests rejectedCommandKeepsReadModelAndQueuesNotification)

        add_executable(seriona_frontend_notifications_tests
            tests/frontend/adapter/tst_frontend_notifications.cpp
            src/app/notification_controller.cpp
            src/app/notification_controller.h
            src/app/backend_command_adapter.cpp
            src/app/backend_command_adapter.h
        )
        target_include_directories(seriona_frontend_notifications_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_notifications_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_notifications_tests
            PRIVATE Qt6::Quick Qt6::Test ${SERIONA_BACKEND_CONTROL_TARGET}
        )

        add_test(NAME seriona_frontend_notifications_command_rejected COMMAND seriona_frontend_notifications_tests commandRejectedResultExposesCodeAndMessage)
        add_test(NAME seriona_frontend_notifications_playback_error COMMAND seriona_frontend_notifications_tests playbackErrorExposesBackendMessage)
        add_test(NAME seriona_frontend_notifications_library_scan_error COMMAND seriona_frontend_notifications_tests libraryScanErrorExposesBackendMessage)
        add_test(NAME seriona_frontend_notifications_library_snapshot_updated COMMAND seriona_frontend_notifications_tests librarySnapshotUpdatedUsesVisibleDefaultMessage)
        add_test(NAME seriona_frontend_notifications_bounded_queue COMMAND seriona_frontend_notifications_tests boundedQueueDropsOldestNotifications)
        add_test(NAME seriona_frontend_notifications_unsupported_feedback COMMAND seriona_frontend_notifications_tests unsupportedSettingsActionsShowLocalFeedback)
        add_test(NAME seriona_frontend_unsupported_settings_feedback COMMAND seriona_frontend_notifications_tests unsupportedSettingsActionsShowLocalFeedback)

        add_executable(seriona_frontend_waveform_worker_tests
            tests/frontend/adapter/tst_waveform_worker.cpp
            src/app/app_facade.cpp
            src/app/app_facade.h
            src/app/playback_controller.cpp
            src/app/playback_controller.h
            src/app/navigation_controller.cpp
            src/app/navigation_controller.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
            src/app/backend_command_adapter.cpp
            src/app/backend_command_adapter.h
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/notification_controller.cpp
            src/app/notification_controller.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
        )
        target_include_directories(seriona_frontend_waveform_worker_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_waveform_worker_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_waveform_worker_tests
            PRIVATE Qt6::Quick Qt6::Test Qt6::Concurrent ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET}
        )

        add_test(NAME seriona_frontend_waveform_worker COMMAND seriona_frontend_waveform_worker_tests)
        add_test(NAME seriona_frontend_waveform_failure COMMAND seriona_frontend_waveform_worker_tests missingFileFailureReturnsEmptyWaveform)

        add_executable(seriona_frontend_current_track_lookup_tests
            tests/frontend/adapter/tst_current_track_lookup.cpp
            src/app/app_facade.cpp
            src/app/app_facade.h
            src/app/playback_controller.cpp
            src/app/playback_controller.h
            src/app/navigation_controller.cpp
            src/app/navigation_controller.h
            src/app/backend_snapshot_mapper.cpp
            src/app/backend_snapshot_mapper.h
            src/app/backend_command_adapter.cpp
            src/app/backend_command_adapter.h
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/notification_controller.cpp
            src/app/notification_controller.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
            src/app/library_tree_store.cpp
            src/app/library_tree_store.h
        )
        target_include_directories(seriona_frontend_current_track_lookup_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_current_track_lookup_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_current_track_lookup_tests
            PRIVATE Qt6::Quick Qt6::Test Qt6::Concurrent ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET}
        )

        add_test(NAME seriona_frontend_current_track_lookup COMMAND seriona_frontend_current_track_lookup_tests currentTrackLookup)
        add_test(NAME seriona_frontend_current_track_cue_path COMMAND seriona_frontend_current_track_lookup_tests cuePathDoesNotDriveHighlight)

        add_executable(seriona_frontend_lyrics_model_tests
            tests/frontend/adapter/tst_lyrics_model.cpp
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
        )
        target_include_directories(seriona_frontend_lyrics_model_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_lyrics_model_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_lyrics_model_tests
            PRIVATE Qt6::Quick Qt6::Test ${SERIONA_BACKEND_CONTROL_TARGET}
        )

        add_test(NAME seriona_frontend_lyrics_trackid_lookup COMMAND seriona_frontend_lyrics_model_tests trackidLookup)
        add_test(NAME seriona_frontend_lyrics_missing_empty COMMAND seriona_frontend_lyrics_model_tests missingEmpty)

        add_executable(seriona_frontend_backend_bridge_tests
            tests/frontend/adapter/tst_backend_bridge.cpp
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/backend_command_adapter.cpp
            src/app/backend_command_adapter.h
        )
        target_include_directories(seriona_frontend_backend_bridge_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_backend_bridge_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_backend_bridge_tests
            PRIVATE Qt6::Test ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_APP_TARGET}
        )

        add_test(NAME seriona_frontend_backend_bridge_threading COMMAND seriona_frontend_backend_bridge_tests threading)
        add_test(NAME seriona_frontend_backend_bridge_shutdown COMMAND seriona_frontend_backend_bridge_tests shutdown)
        add_test(NAME seriona_frontend_backend_bridge_shutdown_stop_sent COMMAND seriona_frontend_backend_bridge_tests shutdownStopSent)
        add_test(NAME seriona_frontend_shutdown_sequence COMMAND seriona_frontend_backend_bridge_tests shutdownSequence)
        add_test(NAME seriona_frontend_shutdown_start_failed COMMAND seriona_frontend_backend_bridge_tests shutdownStartFailed)
    endif()
endif()

install(TARGETS appSeriona
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
