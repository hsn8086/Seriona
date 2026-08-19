import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects
import QtCore
import Seriona

Window {
    id: window
    width: 360
    height: 720
    minimumWidth: 360
    minimumHeight: 720
    visible: true
    title: qsTr("Seriona Music Player")
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    readonly property int sidebarWidth: 350
    readonly property int playerMinWidth: 450
    readonly property bool isDockCapable: width >= (sidebarWidth + playerMinWidth)
    property string smokeScenario: ""
    property bool smokeLoggingEnabled: false
    property bool layoutAnimEnabled: true

    readonly property AppFacade appFacade: AppFacade {}
    readonly property var navigationController: appFacade.navigation
    property bool shutdownRequested: false

    // smoke 调试日志统一出口：仅在非 Release 构建且启用 smoke 模式时输出，
    // 由 C++ 侧 main.cpp 注入 smokeLoggingEnabled 控制（NDEBUG 下恒为 false）。
    function smokeLog(message) {
        if (window.smokeLoggingEnabled)
            console.log(message);
    }

    function requestApplicationClose() {
        if (shutdownRequested)
            return;

        shutdownRequested = true;
        close();
    }

    function applySmokeScenario(scenario) {
        smokeLog("[smoke] applySmokeScenario called with: " + scenario);
        if (scenario === "" || scenario === "startup")
            return;

        navigationController.restorePlaylistFromStartup();

        if (scenario === "lyrics") {
            navigationController.showLyricsView();
            return;
        }

        if (scenario === "sidebar-tree") {
            if (!navigationController.sidebarOpen)
                navigationController.toggleSidebar();
            appFacade.library.focusedNodeId = "favorites";
            appFacade.library.playingTrackId = "track-aurora";
            sidebarSmokeTimer.start();
            return;
        }

        if (scenario === "empty-library") {
            appFacade.library.searchQuery = "__seriona_empty_smoke__";
            return;
        }

        if (scenario === "settings-menu") {
            navigationController.showPlaybackView();
            mainContent.openMenuForSmoke();
            smokeLog("[smoke] mainMenu opened");
            smokeTimer.step = 0;
            smokeTimer.start();
            return;
        }

        navigationController.showPlaybackView();
    }

    function smokeTextItems(item) {
        var result = [];
        if (!item || !item.visible)
            return result;

        if (item.text !== undefined && item.text !== "") {
            result.push({
                "text": String(item.text),
                "x": Math.round(item.mapToItem(window.contentItem, 0, 0).x),
                "y": Math.round(item.mapToItem(window.contentItem, 0, 0).y),
                "width": Math.round(item.width || 0),
                "height": Math.round(item.height || 0),
                "paintedWidth": Math.round(item.paintedWidth || 0),
                "paintedHeight": Math.round(item.paintedHeight || 0),
                "truncatedRisk": item.paintedWidth !== undefined && item.width !== undefined && item.paintedWidth > item.width + 1
            });
        }

        var children = item.children || [];
        for (var i = 0; i < children.length; ++i)
            result = result.concat(smokeTextItems(children[i]));
        return result;
    }

    function smokeVisualStateJson() {
        var state = {
            "scenario": window.smokeScenario,
            "window": {
                "width": window.width,
                "height": window.height,
                "startupScreenVisible": navigationController.startupScreenVisible,
                "currentView": navigationController.currentView,
                "sidebarOpen": navigationController.sidebarOpen
            },
            "library": {
                "focusedNodeId": appFacade.library.focusedNodeId,
                "playingTrackId": appFacade.library.playingTrackId,
                "searchQuery": appFacade.library.searchQuery,
                "visibleNodeCount": appFacade.library.visibleNodeCount,
                "scanStatus": appFacade.library.scanStatus,
                "lastError": appFacade.library.lastError
            },
            "playback": {
                "songTitle": appFacade.playback.songTitle,
                "artistName": appFacade.playback.artistName,
                "albumName": appFacade.playback.albumName,
                "isPlaying": appFacade.playback.isPlaying
            },
            "texts": smokeTextItems(window.contentItem)
        };
        return JSON.stringify(state, null, 2);
    }

    Component.onCompleted: applySmokeScenario(window.smokeScenario)

    onClosing: function (closeEvent) {
        shutdownRequested = true;
        appFacade.shutdown();
    }

    Timer {
        id: animResetTimer
        interval: 50
        onTriggered: window.layoutAnimEnabled = true
    }

    Timer {
        id: manualAnimResetTimer
        interval: 310
        onTriggered: window.navigationController.clearManualSidebarToggle()
    }

    onIsDockCapableChanged: {
        layoutAnimEnabled = false;
        navigationController.syncSidebarForDockCapability(isDockCapable);
        animResetTimer.restart();
    }

    // 遮罩形状
    Rectangle {
        id: rectMask
        anchors.fill: parent
        radius: window.visibility === Window.Maximized ? 0 : 24
        visible: false
    }

    // 主内容容器
    Item {
        id: windowContent
        anchors.fill: parent
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: rectMask
        }

        // Global Drag Area
        MouseArea {
            anchors.fill: parent
            z: -1
            acceptedButtons: Qt.LeftButton
            onPressed: window.startSystemMove()
        }

        DynamicBackground {
            anchors.fill: parent
            playbackController: window.appFacade.playback
        }

        // Sidebar Container
        Sidebar {
            id: sidebarContainer
            height: parent.height
            z: 500
            visible: !window.navigationController.startupScreenVisible
            isDockCapable: window.isDockCapable
            isSidebarOpen: window.navigationController.sidebarOpen
            libraryController: window.appFacade.library
            appFacade: window.appFacade
            x: isSidebarOpen ? 0 : -width

            onCloseClicked: {
                sidebarContainer.closeMenus();
                window.navigationController.closeSidebar();
                manualAnimResetTimer.restart();
            }

            Behavior on x {
                enabled: window.layoutAnimEnabled
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }
        }

        // Click Mask
        MouseArea {
            id: clickMask
            anchors.fill: parent
            z: 499
            visible: (!window.isDockCapable && window.navigationController.sidebarOpen) || sidebarContainer.hasOpenMenu || mainContent.hasOpenMenu
            onClicked: {
                sidebarContainer.closeMenus();
                mainContent.closeMenus();
            }
        }

        // Player Container
        Item {
            id: playerContainer
            height: parent.height
            property bool isDocked: !window.navigationController.startupScreenVisible && window.isDockCapable && window.navigationController.sidebarOpen
            x: isDocked ? window.sidebarWidth : 0
            width: isDocked ? (window.width - window.sidebarWidth) : window.width

            Behavior on x {
                enabled: window.layoutAnimEnabled
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }

            Behavior on width {
                enabled: window.layoutAnimEnabled && window.navigationController.manualSidebarToggle
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Custom Title Bar
                Rectangle {
                    id: titleBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: "transparent"

                    MouseArea {
                        anchors.fill: parent
                        onPressed: window.startSystemMove()
                    }

                    Text {
                        text: qsTr("Seriona")
                        color: Theme.secondaryTextColor
                        font.pixelSize: 13
                        font.letterSpacing: 1.0
                        anchors.centerIn: parent
                    }

                    WindowControls {
                        targetWindow: window
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        onCloseRequested: window.requestApplicationClose()
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // Main Content (Playback & Lyrics with shared element transition)
                    MainContent {
                        id: mainContent
                        anchors.fill: parent
                        visible: !window.navigationController.startupScreenVisible
                        enabled: !window.navigationController.startupScreenVisible
                        state: window.navigationController.currentView
                        isSidebarOpen: window.navigationController.sidebarOpen
                        playbackController: window.appFacade.playback
                        notifications: window.appFacade.notifications
                        libraryController: window.appFacade.library
                        lyricsState: window.appFacade.lyrics

                        onCoverClicked: {
                            window.navigationController.showLyricsView();
                        }

                        onCoverDragRequested: {
                            window.startSystemMove();
                        }

                        onBackClicked: {
                            window.navigationController.showPlaybackView();
                        }

                        onPlaylistToggled: {
                            if (!window.navigationController.sidebarOpen)
                                mainContent.closeMenus();
                            if (window.navigationController.sidebarOpen)
                                sidebarContainer.closeMenus();
                            window.navigationController.toggleSidebar();
                            manualAnimResetTimer.restart();
                        }

                        onExitRequested: window.requestApplicationClose()

                        onOpenSettingsRequested: {
                            settingsWindow.show();
                        }

                        onOpenEqualizerRequested: {
                            equalizerWindow.show();
                        }
                    }

                    StartupView {
                        anchors.fill: parent
                        visible: window.navigationController.startupScreenVisible
                        enabled: window.navigationController.startupScreenVisible
                        navigationController: window.navigationController
                        appFacade: window.appFacade
                        libraryController: window.appFacade.library
                    }
                }
            }
        }
    }

    // 8-directional resizing using custom ResizeArea logic
    Item {
        anchors.fill: parent
        visible: window.visibility !== Window.Maximized
        z: 1000

        // Top
        ResizeArea {
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                leftMargin: 10
                rightMargin: 10
            }
            height: 5
            edgeFlag: Qt.TopEdge
        }
        // Bottom
        ResizeArea {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                leftMargin: 10
                rightMargin: 10
            }
            height: 5
            edgeFlag: Qt.BottomEdge
        }
        // Left
        ResizeArea {
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
                topMargin: 10
                bottomMargin: 10
            }
            width: 5
            edgeFlag: Qt.LeftEdge
        }
        // Right
        ResizeArea {
            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
                topMargin: 10
                bottomMargin: 10
            }
            width: 5
            edgeFlag: Qt.RightEdge
        }
        // TopLeft
        ResizeArea {
            anchors {
                left: parent.left
                top: parent.top
            }
            width: 10
            height: 10
            edgeFlag: Qt.TopEdge | Qt.LeftEdge
        }
        // TopRight
        ResizeArea {
            anchors {
                right: parent.right
                top: parent.top
            }
            width: 10
            height: 10
            edgeFlag: Qt.TopEdge | Qt.RightEdge
        }
        // BottomLeft
        ResizeArea {
            anchors {
                left: parent.left
                bottom: parent.bottom
            }
            width: 10
            height: 10
            edgeFlag: Qt.BottomEdge | Qt.LeftEdge
        }
        // BottomRight
        ResizeArea {
            anchors {
                right: parent.right
                bottom: parent.bottom
            }
            width: 10
            height: 10
            edgeFlag: Qt.BottomEdge | Qt.RightEdge
        }
    }

    component ResizeArea: MouseArea {
        property int edgeFlag
        cursorShape: {
            switch (edgeFlag) {
            case (Qt.TopEdge | Qt.LeftEdge):
                return Qt.SizeFDiagCursor;
            case (Qt.BottomEdge | Qt.RightEdge):
                return Qt.SizeFDiagCursor;
            case (Qt.TopEdge | Qt.RightEdge):
                return Qt.SizeBDiagCursor;
            case (Qt.BottomEdge | Qt.LeftEdge):
                return Qt.SizeBDiagCursor;
            case Qt.TopEdge:
                return Qt.SizeVerCursor;
            case Qt.BottomEdge:
                return Qt.SizeVerCursor;
            case Qt.LeftEdge:
                return Qt.SizeHorCursor;
            case Qt.RightEdge:
                return Qt.SizeHorCursor;
            default:
                return Qt.ArrowCursor;
            }
        }
        onPressed: window.startSystemResize(edgeFlag)
    }

    SettingsWindow {
        id: settingsWindow
        appFacade: window.appFacade
        x: window.x + (window.width - width) / 2
        y: window.y + (window.height - height) / 2
    }

    EqualizerWindow {
        id: equalizerWindow
        x: window.x + (window.width - width) / 2
        y: window.y + (window.height - height) / 2
    }

    Timer {
        id: sidebarSmokeTimer
        interval: 300
        onTriggered: {
            var sb = sidebarContainer.verticalScrollBar;
            if (sb) {
                smokeLog("[smoke] sidebar verticalScrollBar attached=true size=" + sb.size.toFixed(2) + " handleVisible=" + sb.contentItem.visible);
            } else {
                smokeLog("[smoke] sidebar verticalScrollBar attached=false");
            }
        }
    }

    Timer {
        id: smokeTimer
        interval: 100
        repeat: false
        property int step: 0
        onTriggered: {
            if (step === 0) {
                mainContent.openSettingsRequested();
                step = 1;
                smokeTimer.interval = 100;
                smokeTimer.start();
            } else if (step === 1) {
                if (settingsWindow.visible) {
                    console.log("[smoke] settingsWindow opened");
                    settingsWindow.close();
                }
                mainContent.openEqualizerRequested();
                step = 2;
                smokeTimer.interval = 100;
                smokeTimer.start();
            } else if (step === 2) {
                if (equalizerWindow.visible) {
                    console.log("[smoke] equalizerWindow opened");
                    equalizerWindow.close();
                }
            }
        }
    }
}
