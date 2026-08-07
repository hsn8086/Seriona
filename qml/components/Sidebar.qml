import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects
import Qt.labs.platform as Platform
import Seriona

Item {
    id: root
    width: Theme.sidebarWidth

    signal closeClicked

    // Properties to control shadow visibility
    required property AppFacade appFacade
    required property LibraryController libraryController
    property bool isDockCapable: false
    property bool isSidebarOpen: false
    property bool isSearching: false
    property int folderTransitionDirection: 0
    readonly property bool hasOpenMenu: sidebarMenu.visible
    readonly property bool scanRunning: libraryController.scanStatus === "running"
    readonly property bool scanError: libraryController.scanStatus === "error"
    readonly property string scanMessage: scanRunning
        ? qsTr("正在扫描曲库… %1%").arg(libraryController.scanProgress)
        : scanError
            ? (libraryController.lastError.length > 0 ? libraryController.lastError : qsTr("扫描失败，请重新选择文件夹"))
            : ""
    readonly property string emptyStateText: scanRunning
        ? qsTr("正在扫描曲库… %1%").arg(libraryController.scanProgress)
        : scanError
            ? (libraryController.lastError.length > 0 ? libraryController.lastError : qsTr("扫描失败，请重新选择文件夹"))
            : libraryController.scanStatus === "completed"
                ? qsTr("扫描完成，但没有发现音频文件")
                : root.isSearching ? qsTr("没有匹配的本地结果") : qsTr("暂无曲库内容，请添加音乐文件夹")

    onIsSidebarOpenChanged: {
        if (!isSidebarOpen)
            closeMenus();
    }

    function closeMenus() {
        sidebarMenu.close();
    }

    function showUnsupportedFeedback(actionName) {
        root.appFacade.notifications.showUnsupportedAction(actionName);
        root.closeMenus();
    }

    function sortRulesForDialog() {
        var currentRules = libraryController.currentSortRules.slice();
        return currentRules.length > 0 ? currentRules : [{field: "filename", order: "asc"}];
    }

    function activateNode(nodeId, isFolder) {
        if (nodeId.length === 0)
            return;

        root.closeMenus();
        libraryController.selectBrowserNode(nodeId);
        if (isFolder) {
            root.folderTransitionDirection = 1;
            libraryController.enterFolder(nodeId);
            folderTransitionResetTimer.restart();
            return;
        }

        root.folderTransitionDirection = 0;
        libraryController.playItem(nodeId);
    }

    Timer {
        id: folderTransitionResetTimer
        interval: 300
        onTriggered: root.folderTransitionDirection = 0
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
        color: "transparent"

        DynamicBackground {
            anchors.fill: parent
            playbackController: root.appFacade.playback
        }

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
                                     root.closeMenus();
                                     root.folderTransitionDirection = -1;
                                     libraryController.goBack();
                                     folderTransitionResetTimer.restart();
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
                                     text: qsTr("排序")
                                     onTriggered: {
                                         sidebarMenu.close();
                                         sortDialog.sortRules = root.sortRulesForDialog();
                                         sortDialog.show();
                                     }
                                 }
                                    BubbleMenuItem {
                                        text: qsTr("刷新")
                                        onTriggered: {
                                            libraryController.refresh();
                                            sidebarMenu.close();
                                        }
                                    }
                                    BubbleMenuItem {
                                        text: qsTr("添加文件夹")
                                        onTriggered: {
                                            sidebarMenu.close();
                                            sidebarFolderDialog.open();
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

            Item {
                id: playlistPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Rectangle {
                    id: scanBanner
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    height: 34
                    radius: 12
                    color: root.scanError ? "#33FF5C5C" : Theme.baseColor
                    border.color: root.scanError ? Theme.accentColor : Theme.hoverColor
                    border.width: 1
                    visible: root.scanRunning || root.scanError
                    z: 2

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        text: root.scanMessage
                        color: root.scanError ? Theme.accentColor : Theme.secondaryTextColor
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                ListView {
                    id: playlistView
                    anchors.fill: parent
                    model: libraryController.model
                    spacing: 0
                    topMargin: Theme.paddingMedium + (scanBanner.visible ? scanBanner.height + 8 : 0)
                    bottomMargin: 80
                    clip: true
                    reuseItems: true

                    delegate: ItemDelegate {
                        id: delegate
                        required property int index
                        required property string type
                        required property string name
                        required property string title
                        required property string artist
                        required property string album
                        required property string parentName
                        required property int songCount
                        required property string duration
                        required property string format
                        required property int sampleRate
                        required property int bitDepth
                        required property string nodeId
                        required property string trackId
                        required property bool isFolder
                        required property bool isPlaying
                        required property bool isFocused
                        required property string artworkSource

                        readonly property bool hasFolderArtwork: delegate.isFolder && delegate.artworkSource.length > 0

                        width: playlistView.width
                        height: 72
                        topPadding: 8
                        bottomPadding: 8
                        leftPadding: 15
                        rightPadding: 15
                        Accessible.role: Accessible.ListItem
                        Accessible.name: isFolder ? name : title

                        onClicked: root.activateNode(nodeId, isFolder)

                        background: Rectangle {
                            color: delegate.hovered || delegate.isPlaying ? Theme.hoverColor : "transparent"

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }
                            }
                        }

                        contentItem: RowLayout {
                            spacing: 12

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 44
                                Layout.preferredHeight: 44
                                radius: 12
                                color: delegate.isFolder ? Theme.accentColor : Theme.mainColor
                                antialiasing: true
                                layer.enabled: true
                                layer.effect: OpacityMask {
                                    maskSource: Rectangle {
                                        width: 44
                                        height: 44
                                        radius: 12
                                    }
                                }

                                Image {
                                    id: folderThumbIcon
                                    anchors.fill: parent
                                    anchors.margins: delegate.isFolder ? 10 : 0
                                    source: delegate.isFolder ? "qrc:/qt/qml/Seriona/qml/assets/folder.svg" : delegate.artworkSource
                                    sourceSize.width: delegate.isFolder ? 24 : 44
                                    sourceSize.height: delegate.isFolder ? 24 : 44
                                    fillMode: delegate.isFolder ? Image.PreserveAspectFit : Image.PreserveAspectCrop
                                    asynchronous: !delegate.isFolder
                                    visible: !delegate.isFolder && status === Image.Ready
                                }

                                Image {
                                    id: folderArtworkIcon
                                    anchors.fill: parent
                                    source: delegate.artworkSource
                                    sourceSize: Qt.size(88, 88)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    visible: delegate.hasFolderArtwork && status === Image.Ready
                                }

                                ColorOverlay {
                                    anchors.fill: folderThumbIcon
                                    source: folderThumbIcon
                                    color: "white"
                                    visible: delegate.isFolder && !delegate.hasFolderArtwork
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 12
                                    color: "#20FFFFFF"
                                    visible: !delegate.isFolder && folderThumbIcon.status !== Image.Ready
                                    antialiasing: true

                                    Text {
                                        anchors.centerIn: parent
                                        text: "♫"
                                        color: "white"
                                        font.pixelSize: 22
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 1

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Text {
                                        Layout.fillWidth: true
                                        text: delegate.isFolder ? delegate.name : delegate.title
                                        color: delegate.isPlaying ? Theme.accentColor : Theme.textColor
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: delegate.isFolder ? delegate.parentName : (delegate.artist.length > 0 && delegate.album.length > 0 ? delegate.artist + " - " + delegate.album : delegate.artist + delegate.album)
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    RowLayout {
                                        visible: !delegate.isFolder
                                        spacing: 4

                                        Text {
                                            text: delegate.duration || ""
                                            color: Theme.secondaryTextColor
                                            font.pixelSize: 10
                                        }
                                        Text {
                                            text: "|"
                                            color: "#30FFFFFF"
                                            font.pixelSize: 10
                                            visible: delegate.duration.length > 0 && delegate.format.length > 0
                                        }
                                        Text {
                                            text: delegate.format || ""
                                            color: Theme.secondaryTextColor
                                            font.pixelSize: 10
                                        }
                                        Text {
                                            text: "|"
                                            color: "#30FFFFFF"
                                            font.pixelSize: 10
                                            visible: delegate.sampleRate > 44100
                                        }
                                        Text {
                                            text: (delegate.sampleRate / 1000) + "kHz"
                                            color: Theme.accentColor
                                            font.pixelSize: 10
                                            visible: delegate.sampleRate > 44100
                                        }
                                        Text {
                                            text: "|"
                                            color: "#30FFFFFF"
                                            font.pixelSize: 10
                                            visible: delegate.bitDepth > 16
                                        }
                                        Text {
                                            text: delegate.bitDepth + "bit"
                                            color: Theme.accentColor
                                            font.pixelSize: 10
                                            visible: delegate.bitDepth > 16
                                        }
                                    }

                                    RowLayout {
                                        visible: delegate.isFolder
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
                                            text: qsTr("%1 Songs").arg(delegate.songCount)
                                            color: Theme.secondaryTextColor
                                            font.pixelSize: 10
                                        }
                                        Text {
                                            text: "|"
                                            color: "#30FFFFFF"
                                            font.pixelSize: 10
                                            visible: delegate.duration.length > 0
                                        }
                                        Text {
                                            text: delegate.duration || ""
                                            color: Theme.secondaryTextColor
                                            font.pixelSize: 10
                                        }
                                    }

                                    Image {
                                        Layout.alignment: Qt.AlignVCenter
                                        source: "qrc:/qt/qml/Seriona/qml/assets/folder.svg"
                                        sourceSize: Qt.size(14, 14)
                                        fillMode: Image.PreserveAspectFit
                                        visible: delegate.isFolder

                                        ColorOverlay {
                                            anchors.fill: parent
                                            source: parent
                                            color: "white"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Connections {
                    target: libraryController

                    function onScrollRequestChanged() {
                        const row = libraryController.rowForNodeId(libraryController.scrollRequest);
                        if (row >= 0)
                            playlistView.positionViewAtIndex(row, ListView.Contain);
                    }
                }

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    text: root.emptyStateText
                    color: Theme.secondaryTextColor
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: libraryController.visibleNodeCount === 0
                }
            }
        }

        Platform.FolderDialog {
            id: sidebarFolderDialog
            title: qsTr("选择音乐文件夹")

            onAccepted: root.appFacade.scanLibrary(folder)
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

    SortDialog {
        id: sortDialog
        x: (root.width - width) / 2
        y: 100
        
        sortRules: []
        
        onAccepted: {
            libraryController.applySortRules(sortRules);
        }
    }

}
