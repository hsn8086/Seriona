import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects
import Seriona

Window {
    id: window
    width: 370
    height: 720
    minimumWidth: 370
    minimumHeight: 720
    visible: true
    title: qsTr("Seriona Music Player")
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    readonly property int sidebarWidth: 350
    readonly property int playerMinWidth: 450
    readonly property bool isDockCapable: width >= (sidebarWidth + playerMinWidth)
    property bool layoutAnimEnabled: true

    readonly property AppFacade appFacade: AppFacade {}
    readonly property var navigationController: appFacade.navigation

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

        // 背景色 (使用 Theme.backgroundColor)
        Rectangle {
            anchors.fill: parent
            color: Theme.backgroundColor
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

    // Virtual Keyboard Input Panel
    InputPanel {
        id: inputPanel
        z: 99
        y: window.height
        width: window.width

        states: State {
            name: "visible"
            when: inputPanel.active
            PropertyChanges {
                target: inputPanel
                y: window.height - inputPanel.height
            }
        }
        transitions: Transition {
            from: ""
            to: "visible"
            reversible: true
            NumberAnimation {
                properties: "y"
                easing.type: Easing.InOutQuad
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
}
