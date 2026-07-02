import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects
import Seriona

Item {
    id: root
    width: Theme.sidebarWidth

    signal closeClicked

    // Properties to control shadow visibility
    property bool isDockCapable: false
    property bool isSidebarOpen: false
    property bool isSearching: false
    readonly property bool hasOpenMenu: sidebarMenu.visible

    LibraryController {
        id: libraryController
    }

    onIsSidebarOpenChanged: {
        if (!isSidebarOpen)
            closeMenus();
    }

    function closeMenus() {
        sidebarMenu.close();
    }

    function openFolder(index) {
        libraryController.enterFolder(index);
        pageStack.push(playlistPageComponent);
    }

    RectangularGlow {
        id: shadow
        anchors.fill: contentRect
        glowRadius: 10
        spread: 0.2
        color: "#80000000"
        visible: !root.isDockCapable && root.isSidebarOpen
        z: -1
    }

    Rectangle {
        id: contentRect
        anchors.fill: parent
        color: Theme.sidebarBackgroundColor

        // Right border line
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: "#15FFFFFF"
            z: 2
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Top Bar
            Rectangle {
                z: 2
                Layout.fillWidth: true
                Layout.preferredHeight: root.isSearching ? 100 : 50
                color: "transparent"

                Behavior on Layout.preferredHeight {
                    NumberAnimation {
                        duration: 250
                        easing.type: Easing.OutCubic
                    }
                }

                Column {
                    anchors.fill: parent

                    Item {
                        width: parent.width
                        height: 50

                        MouseArea {
                            anchors.fill: parent
                            visible: root.hasOpenMenu
                            enabled: visible
                            onClicked: root.closeMenus()
                        }

                        RowLayout {
                            z: 1
                            anchors.fill: parent
                            anchors.leftMargin: 15
                            anchors.rightMargin: 15
                            spacing: 14

                            StyleButton {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                iconSource: "qrc:/qt/qml/Seriona/qml/assets/arrow_back.svg"
                                buttonWidth: 20
                                buttonHeight: 20
                                iconSize: 14
                                enabled: libraryController.canGoBack
                                opacity: enabled ? 1.0 : 0.3
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: Theme.animationDuration
                                    }
                                }
                                onClicked: {
                                    libraryController.goBack();
                                    pageStack.pop();
                                }
                            }

                            StyleButton {
                                id: searchButton
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                iconSource: "qrc:/qt/qml/Seriona/qml/assets/search.svg"
                                buttonWidth: 20
                                buttonHeight: 20
                                iconSize: 14
                                checkable: true
                                checked: root.isSearching
                                textColor: root.isSearching ? Theme.accentColor : Theme.textColor
                                onClicked: {
                                    root.closeMenus();
                                    root.isSearching = !root.isSearching;
                                     if (root.isSearching) {
                                         searchInput.forceActiveFocus();
                                     } else {
                                         libraryController.clearSearch();
                                     }
                                 }
                             }

                            Text {
                                Layout.fillWidth: true
                                text: libraryController.currentFolderName
                                color: Theme.textColor
                                font.pixelSize: 15
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }

                            StyleButton {
                                id: sidebarMoreBtn
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                iconSource: "qrc:/qt/qml/Seriona/qml/assets/more_vert.svg"
                                buttonWidth: 20
                                buttonHeight: 20
                                iconSize: 14
                                onClicked: sidebarMenu.toggle()

                                BubbleMenu {
                                    id: sidebarMenu
                                    targetItem: sidebarMoreBtn

                                    BubbleMenuItem {
                                        text: qsTr("Sort by Name")
                                        onTriggered: sidebarMenu.close()
                                    }
                                    BubbleMenuItem {
                                        text: qsTr("Sort by Date")
                                        onTriggered: sidebarMenu.close()
                                    }
                                    BubbleMenuItem {
                                        text: qsTr("Refresh")
                                        onTriggered: {
                                            libraryController.refresh();
                                            sidebarMenu.close();
                                        }
                                    }
                                }
                            }

                            StyleButton {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                                buttonWidth: 20
                                buttonHeight: 20
                                iconSize: 14
                                onClicked: {
                                    root.closeMenus();
                                    root.closeClicked();
                                }
                            }
                        }
                    }

                    Item {
                        width: parent.width
                        height: 50
                        visible: opacity > 0.0
                        opacity: root.isSearching ? 1.0 : 0.0
                        z: 10

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 200
                            }
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width - 20
                            height: 36
                            color: Theme.baseColor
                            radius: 18

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                Item {
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18

                                    Image {
                                        id: searchFieldIcon
                                        anchors.fill: parent
                                        source: "qrc:/qt/qml/Seriona/qml/assets/search.svg"
                                        sourceSize.width: 18
                                        sourceSize.height: 18
                                        fillMode: Image.PreserveAspectFit
                                        visible: false
                                    }

                                    ColorOverlay {
                                        anchors.fill: searchFieldIcon
                                        source: searchFieldIcon
                                        color: Theme.secondaryTextColor
                                    }
                                }

                                TextField {
                                    id: searchInput
                                    Layout.fillWidth: true
                                    background: null
                                    color: Theme.textColor
                                    font.pixelSize: 14
                                    placeholderText: qsTr("搜索当前文件夹及子目录...")
                                     placeholderTextColor: "#60FFFFFF"
                                     selectByMouse: true
                                     text: libraryController.searchQuery
                                     verticalAlignment: Text.AlignVCenter
                                     onTextEdited: libraryController.searchQuery = text
                                     onAccepted: libraryController.submitSearch()
                                 }

                                 Item {
                                     Layout.preferredWidth: 16
                                     Layout.preferredHeight: 16
                                     visible: libraryController.searchQuery.length > 0

                                    Image {
                                        id: clearFieldIcon
                                        anchors.fill: parent
                                        source: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                                        sourceSize.width: 16
                                        sourceSize.height: 16
                                        fillMode: Image.PreserveAspectFit
                                        visible: false
                                    }

                                    ColorOverlay {
                                        anchors.fill: clearFieldIcon
                                        source: clearFieldIcon
                                        color: Theme.secondaryTextColor
                                    }

                                     MouseArea {
                                         anchors.fill: parent
                                         cursorShape: Qt.PointingHandCursor
                                         onClicked: libraryController.clearSearch()
                                     }
                                 }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: "#10FFFFFF"
                    }
                }
            }

            StackView {
                id: pageStack
                z: 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                initialItem: playlistPageComponent

                pushEnter: Transition {
                    PropertyAnimation {
                        property: "x"
                        from: pageStack.width
                        to: 0
                        duration: Theme.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
                pushExit: Transition {
                    PropertyAnimation {
                        property: "x"
                        from: 0
                        to: -pageStack.width
                        duration: Theme.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
                popEnter: Transition {
                    PropertyAnimation {
                        property: "x"
                        from: -pageStack.width
                        to: 0
                        duration: Theme.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
                popExit: Transition {
                    PropertyAnimation {
                        property: "x"
                        from: 0
                        to: pageStack.width
                        duration: Theme.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }

        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 50
            anchors.bottom: parent.bottom
            z: 9
            visible: root.hasOpenMenu
            enabled: visible
            onClicked: root.closeMenus()
        }

        // Floating Action Button (FAB)
        StyleButton {
            id: fab
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 15
            buttonWidth: 40
            buttonHeight: 40
            iconSize: 20
            iconSource: "qrc:/qt/qml/Seriona/qml/assets/my_location.svg"
            baseColor: Theme.accentColor
            textColor: "white"
            z: 10

            onClicked: libraryController.locateCurrentSong()

            // Shadow for FAB
            layer.enabled: true
            layer.effect: DropShadow {
                transparentBorder: true
                horizontalOffset: 0
                verticalOffset: 4
                radius: 8
                samples: 17
                color: "#80000000"
            }
        }
    }

    Component {
        id: playlistPageComponent
        ListView {
            id: playlistView
            model: libraryController.model
            spacing: 2
            topMargin: Theme.paddingMedium
            bottomMargin: 80 // Space for FAB
            clip: true

                delegate: ItemDelegate {
                    id: delegate
                    width: playlistView.width
                topPadding: 8
                bottomPadding: 8
                leftPadding: 15
                rightPadding: 15

                onClicked: {
                    if (model.type === "folder") {
                        root.openFolder(index);
                    } else {
                        libraryController.playItem(index);
                    }
                }

                background: Rectangle {
                    color: delegate.visualFocus || delegate.hovered ? Theme.hoverColor : "transparent"
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                }

                contentItem: RowLayout {
                    spacing: 12

                    // Left: Thumbnail/Cover
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        width: 44
                        height: 44
                        radius: 8
                        color: model.type === "folder" ? Theme.accentColor : Theme.mainColor
                        clip: true

                        Image {
                            id: folderThumbIcon
                            anchors.fill: parent
                            anchors.margins: model.type === "folder" ? 10 : 0
                            source: model.type === "folder" ? "qrc:/qt/qml/Seriona/qml/assets/folder.svg" : ""
                            fillMode: Image.PreserveAspectFit
                            visible: false
                        }

                        ColorOverlay {
                            anchors.fill: folderThumbIcon
                            source: folderThumbIcon
                            color: "white"
                            visible: model.type === "folder"
                        }

                        // Placeholder for cover art
                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: "#20FFFFFF"
                            visible: model.type === "file"
                            Text {
                                anchors.centerIn: parent
                                text: "♫"
                                color: "white"
                                font.pixelSize: 22
                            }
                        }
                    }

                    // Right: Info
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 1

                        // Top: Name/Title
                        Text {
                            Layout.fillWidth: true
                            text: model.type === "file" ? model.title : model.name
                            color: Theme.textColor
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        // Middle: Artist-Album / Parent Folder
                        Text {
                            Layout.fillWidth: true
                            text: model.type === "file" ? (model.artist + " - " + model.album) : model.parentName
                            color: Theme.secondaryTextColor
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        // Bottom: Stream Info / Folder Stats
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            // File specific info
                            RowLayout {
                                visible: model.type === "file"
                                spacing: 4
                                Text {
                                    text: model.duration || ""
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 10
                                }
                                Text {
                                    text: "|"
                                    color: "#30FFFFFF"
                                    font.pixelSize: 10
                                    visible: model.format !== undefined
                                }
                                Text {
                                    text: model.format || ""
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 10
                                }
                                Text {
                                    text: "|"
                                    color: "#30FFFFFF"
                                    font.pixelSize: 10
                                    visible: model.sampleRate > 44100
                                }
                                Text {
                                    text: (model.sampleRate / 1000) + "kHz"
                                    color: Theme.accentColor
                                    font.pixelSize: 10
                                    visible: model.sampleRate > 44100
                                }
                                Text {
                                    text: "|"
                                    color: "#30FFFFFF"
                                    font.pixelSize: 10
                                    visible: model.bitDepth > 16
                                }
                                Text {
                                    text: model.bitDepth + "bit"
                                    color: Theme.accentColor
                                    font.pixelSize: 10
                                    visible: model.bitDepth > 16
                                }
                            }

                            // Folder specific info
                            RowLayout {
                                visible: model.type === "folder"
                                spacing: 6
                                Item {
                                    Layout.preferredWidth: 10
                                    Layout.preferredHeight: 10
                                    Image {
                                        id: musicNoteIcon
                                        anchors.fill: parent
                                        source: "qrc:/qt/qml/Seriona/qml/assets/music_note.svg"
                                        sourceSize: Qt.size(10, 10)
                                        fillMode: Image.PreserveAspectFit
                                        visible: false
                                    }
                                    ColorOverlay {
                                        anchors.fill: musicNoteIcon
                                        source: musicNoteIcon
                                        color: Theme.secondaryTextColor
                                        opacity: 0.7
                                    }
                                }
                                Text {
                                    text: model.songCount + " Songs"
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 10
                                }
                                Text {
                                    text: "|"
                                    color: "#30FFFFFF"
                                    font.pixelSize: 10
                                }
                                Text {
                                    text: model.duration || ""
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }

                // Small folder icon in bottom-right for folders
                Image {
                    id: folderBadgeIcon
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 4
                    source: "qrc:/qt/qml/Seriona/qml/assets/folder.svg"
                    sourceSize: Qt.size(12, 12)
                    visible: false
                }
                ColorOverlay {
                    anchors.fill: folderBadgeIcon
                    source: folderBadgeIcon
                    color: "white"
                    opacity: 0.3
                    visible: model.type === "folder"
                }
            }
        }
    }

}
