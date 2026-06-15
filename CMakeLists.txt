cmake_minimum_required(VERSION 3.16)

project(Seriona VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appSeriona
    src/main.cpp
)

# 注册 Theme.qml 为 Singleton
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appSeriona
    URI Seriona
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/components/StyleButton.qml
        qml/components/Sidebar.qml
        qml/components/WindowControls.qml
        qml/components/WaveformProgressBar.qml
        qml/components/MarqueeText.qml
        qml/views/MainContent.qml
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

install(TARGETS appSeriona
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
