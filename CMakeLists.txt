cmake_minimum_required(VERSION 3.16)

project(Seriona VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Concurrent QuickDialogs2)

qt_standard_project_setup(REQUIRES 6.8)

include(CTest)

set(SERIONA_BACKEND_SOURCE_DIR "../../cppProject(app_and_lib)/Seriona_Backend" CACHE PATH "Seriona backend source tree")
set(SERIONA_BACKEND_CONTROL_TARGET "")
set(SERIONA_BACKEND_AUDIO_TARGET "")

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

        if(TARGET SerionaBackend::audio)
            set(SERIONA_BACKEND_AUDIO_TARGET SerionaBackend::audio)
        elseif(TARGET seriona_audio)
            set(SERIONA_BACKEND_AUDIO_TARGET seriona_audio)
        else()
            message(FATAL_ERROR "Seriona backend was added, but neither SerionaBackend::audio nor seriona_audio exists.")
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
        src/app/waveform_provider.cpp
        src/app/waveform_provider.h
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
    PRIVATE Qt6::Quick Qt6::Concurrent Qt6::QuickDialogs2
)

if(SERIONA_BACKEND_CONTROL_TARGET)
    target_link_libraries(appSeriona
        PRIVATE ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET}
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

    if(SERIONA_BACKEND_CONTROL_TARGET)
        add_executable(seriona_frontend_library_tree_model_tests
            tests/frontend/adapter/tst_library_tree_model.cpp
            src/app/library_model.cpp
            src/app/library_model.h
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
        add_test(NAME seriona_frontend_library_follow_playing COMMAND seriona_frontend_library_dual_cursor_tests followCurrentlyPlayingControlsBrowserCursor)
        add_test(NAME seriona_frontend_library_dual_cursor_refresh COMMAND seriona_frontend_library_dual_cursor_tests refreshKeepsAndFallsBackBrowserState)
        add_test(NAME seriona_frontend_library_dual_cursor_no_commands COMMAND seriona_frontend_library_dual_cursor_tests browsingActionsDoNotEmitBackendHookSignals)

        add_executable(seriona_frontend_sidebar_local_browsing_tests
            tests/frontend/adapter/tst_sidebar_local_browsing.cpp
            src/app/library_model.cpp
            src/app/library_model.h
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
        add_test(NAME seriona_frontend_sidebar_tree_local COMMAND seriona_frontend_sidebar_local_browsing_tests expansionAndBackStayLocal)

        add_executable(seriona_frontend_library_select_track_tests
            tests/frontend/adapter/tst_library_select_track.cpp
            src/app/library_model.cpp
            src/app/library_model.h
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
        add_test(NAME seriona_frontend_library_browse_no_command COMMAND seriona_frontend_library_select_track_tests browsingAndLocateCurrentSongDoNotSubmitCommands)

        add_executable(seriona_frontend_library_scan_flow_tests
            tests/frontend/adapter/tst_library_scan_flow.cpp
            src/app/library_model.cpp
            src/app/library_model.h
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
        add_test(NAME seriona_frontend_scan_library_request COMMAND seriona_frontend_library_scan_flow_tests scanLibraryRequestSubmitsBackendScanAndUpdatesTree)
        add_test(NAME seriona_frontend_scan_library_cancel_invalid COMMAND seriona_frontend_library_scan_flow_tests cancelAndInvalidSelectionDoNotScanOrSaveRoot)
        add_test(NAME seriona_frontend_scan_state_mapping COMMAND seriona_frontend_library_scan_flow_tests scanSnapshotsMapUiStates)
    endif()

        add_executable(seriona_frontend_app_facade_smoke_mode
            tests/frontend/adapter/tst_app_facade_smoke_mode.cpp
            src/app/app_facade.cpp
            src/app/app_facade.h
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
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
                PRIVATE ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET}
            )
        endif()
    add_test(NAME seriona_frontend_app_facade_smoke_mode COMMAND seriona_frontend_app_facade_smoke_mode)

    if(SERIONA_BACKEND_CONTROL_TARGET)
        add_executable(seriona_frontend_startup_restore_tests
            tests/frontend/adapter/tst_startup_restore.cpp
            src/app/app_facade.cpp
            src/app/app_facade.h
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
        )
        target_include_directories(seriona_frontend_startup_restore_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_startup_restore_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_startup_restore_tests
            PRIVATE Qt6::Quick Qt6::Test Qt6::Concurrent ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET}
        )

        add_test(NAME seriona_frontend_startup_persist_root COMMAND seriona_frontend_startup_restore_tests scanLibraryPersistsRootForStartupRestore)
        add_test(NAME seriona_frontend_startup_restore_root COMMAND seriona_frontend_startup_restore_tests restoreSavedRootScansAndEntersMainShell)
        add_test(NAME seriona_frontend_startup_missing_root COMMAND seriona_frontend_startup_restore_tests missingSavedRoot)

        add_executable(seriona_frontend_playback_snapshot_tests
            tests/frontend/adapter/tst_playback_snapshot_mapping.cpp
            src/app/app_facade.cpp
            src/app/app_facade.h
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
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
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
        )
        target_include_directories(seriona_frontend_playback_command_tests
            PRIVATE src/app
        )
        target_compile_definitions(seriona_frontend_playback_command_tests
            PRIVATE SERIONA_HAS_BACKEND=1
        )
        target_link_libraries(seriona_frontend_playback_command_tests
            PRIVATE Qt6::Quick Qt6::Test Qt6::Concurrent ${SERIONA_BACKEND_CONTROL_TARGET} ${SERIONA_BACKEND_AUDIO_TARGET}
        )

        add_test(NAME seriona_frontend_playback_command_mapping COMMAND seriona_frontend_playback_command_tests visiblePlaybackControlsSubmitCommands)
        add_test(NAME seriona_frontend_playback_rejected_command COMMAND seriona_frontend_playback_command_tests rejectedCommandKeepsReadModelAndQueuesNotification)

        add_executable(seriona_frontend_waveform_worker_tests
            tests/frontend/adapter/tst_waveform_worker.cpp
            src/app/app_facade.cpp
            src/app/app_facade.h
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
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
            src/app/backend_bridge.cpp
            src/app/backend_bridge.h
            src/app/waveform_provider.cpp
            src/app/waveform_provider.h
            src/app/lyrics_model.cpp
            src/app/lyrics_model.h
            src/app/library_model.cpp
            src/app/library_model.h
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
