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
    // 视图切换（T15）：false=文件夹视图，true=队列视图。仅切换列表内容，
    // 不持久化（每次启动默认文件夹视图）；文件夹视图状态无损保留。
    property bool queueViewActive: false
    property int folderTransitionDirection: 0
    readonly property bool hasOpenMenu: sidebarMenu.visible || trackContextMenu.isOpen
    readonly property ScrollBar verticalScrollBar: playlistView.ScrollBar.vertical
    readonly property bool scanRunning: libraryController.scanStatus === "running"
    readonly property bool scanError: libraryController.scanStatus === "error"
    readonly property string scanMessage: scanRunning
        ? (libraryController.totalSongCount > 0
            ? qsTr("正在扫描：%1 / %2").arg(libraryController.scannedSongCount).arg(libraryController.totalSongCount)
            : qsTr("正在扫描曲库…"))
        : scanError
            ? (libraryController.lastError.length > 0 ? libraryController.lastError : qsTr("扫描失败，请重新选择文件夹"))
            : ""
    readonly property string emptyStateText: scanRunning
        ? (libraryController.totalSongCount > 0
            ? qsTr("正在扫描：%1 / %2").arg(libraryController.scannedSongCount).arg(libraryController.totalSongCount)
            : qsTr("正在扫描曲库…"))
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
        trackContextMenu.close();
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
        color: Theme.shadowPopupColor
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
            color: Theme.borderSubtle
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
                        duration: Theme.animationStandard
                        easing.type: Theme.easingDecelerate
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

                        // 表头空白处按下拖拽窗口（Qt 6.8 Window.startSystemMove；
                        // offscreen/无窗口系统支持时无副作用）。按钮/搜索框等交互元素
                        // 位于上层（RowLayout z:1、搜索行 z:10），点击事件不受影响。
                        MouseArea {
                            id: headerDragArea
                            anchors.fill: parent
                            visible: !root.hasOpenMenu
                            enabled: visible
                            acceptedButtons: Qt.LeftButton
                            onPressed: root.Window.window.startSystemMove()
                        }

                        RowLayout {
                            z: 1
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacing16
                            anchors.rightMargin: Theme.spacing16
                            spacing: Theme.spacing12

                            StyleButton {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                iconSource: "qrc:/qt/qml/Seriona/qml/assets/arrow_back.svg"
                                buttonWidth: 20
                                buttonHeight: 20
                                iconSize: 14
                                enabled: libraryController.canGoBack
                                onClicked: {
                                    root.closeMenus();
                                    root.folderTransitionDirection = -1;
                                    libraryController.goBack();
                                    folderTransitionResetTimer.restart();
                                }
                                SharedToolTip {
                                    text: qsTr("返回")
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
                                textColor: root.isSearching ? Theme.accentColor : Theme.textPrimary
                                onClicked: {
                                    root.closeMenus();
                                    root.isSearching = !root.isSearching;
                                    if (root.isSearching) {
                                        searchInput.forceActiveFocus();
                                    } else {
                                        libraryController.clearSearch();
                                    }
                                }
                                SharedToolTip {
                                    text: qsTr("搜索")
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.queueViewActive ? qsTr("播放队列") : libraryController.currentFolderName
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontTitle
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

                                SharedToolTip {
                                    text: qsTr("更多")
                                }

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
                                SharedToolTip {
                                    text: qsTr("关闭")
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
                                duration: Theme.animationFast
                            }
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width - Theme.spacing24
                            height: 36
                            color: Theme.baseColor
                            radius: Theme.radiusLarge
                            border.color: searchInput.activeFocus ? Theme.borderAccent : Theme.borderSubtle
                            border.width: 1

                            Behavior on border.color {
                                ColorAnimation { duration: Theme.animationFast }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.spacing12
                                anchors.rightMargin: Theme.spacing12
                                spacing: Theme.spacing8

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
                                        color: searchInput.activeFocus ? Theme.accentColor : Theme.textSecondary
                                        Behavior on color {
                                            ColorAnimation { duration: Theme.animationFast }
                                        }
                                    }
                                }

                                TextField {
                                    id: searchInput
                                    Layout.fillWidth: true
                                    background: null
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontBody
                                    placeholderText: qsTr("搜索当前文件夹及子目录...")
                                    placeholderTextColor: Theme.textDisabled
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
                                        color: clearFieldMouseArea.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                    }

                                    MouseArea {
                                        id: clearFieldMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
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
                        color: Theme.borderSubtle
                    }
                }
            }

            // 视图切换条（T15）：文件夹视图 ↔ 队列视图。独立一行，不覆盖
            // 表头拖拽区/搜索行/条目列表的任何既有交互区域。
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: 176
                    height: 30
                    radius: Theme.radiusFull
                    color: Theme.borderSubtle
                    border.color: Theme.borderSubtle
                    border.width: 1

                    Row {
                        anchors.centerIn: parent
                        spacing: 4

                        Rectangle {
                            id: folderViewButton
                            objectName: "folderViewButton"
                            width: 82
                            height: 30
                            radius: 15
                            color: !root.queueViewActive ? Theme.hoverColor : "transparent"

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.animationFast
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.queueViewActive = false
                            }

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("文件夹")
                                color: !root.queueViewActive ? Theme.textPrimary : Theme.textSecondary
                                font.pixelSize: Theme.fontCaption + 1
                                font.weight: !root.queueViewActive ? Font.DemiBold : Font.Normal
                            }
                        }

                        Rectangle {
                            id: queueViewButton
                            objectName: "queueViewButton"
                            width: 82
                            height: 30
                            radius: 15
                            color: root.queueViewActive ? Theme.hoverColor : "transparent"

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.animationFast
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.queueViewActive = true
                            }

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("播放队列")
                                color: root.queueViewActive ? Theme.textPrimary : Theme.textSecondary
                                font.pixelSize: Theme.fontCaption + 1
                                font.weight: root.queueViewActive ? Font.DemiBold : Font.Normal
                            }
                        }
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
                    anchors.margins: Theme.spacing12
                    height: 34
                    radius: Theme.radiusLarge
                    color: root.scanError ? Theme.toastErrorBg : Theme.baseColor
                    border.color: root.scanError ? Theme.dangerColor : Theme.borderColor
                    border.width: 1
                    visible: (root.scanRunning || root.scanError) && !root.queueViewActive
                    z: 2

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacing12
                        anchors.rightMargin: Theme.spacing12
                        text: root.scanMessage
                        color: root.scanError ? Theme.dangerColor : Theme.textSecondary
                        font.pixelSize: Theme.fontCaption + 1
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                ListView {
                    id: playlistView
                    objectName: "playlistView"
                    anchors.fill: parent
                    model: libraryController.model
                    visible: !root.queueViewActive
                    spacing: 0
                    topMargin: Theme.paddingMedium + (scanBanner.visible ? scanBanner.height + 8 : 0)
                    bottomMargin: 80
                    clip: true
                    reuseItems: true

                    ScrollBar.vertical: ScrollBar {
                        id: playlistScrollBar
                        policy: ScrollBar.AsNeeded
                        width: Theme.scrollbarWidth

                        background: Rectangle {
                            color: "transparent"
                        }

                        contentItem: Rectangle {
                            implicitWidth: Theme.scrollbarWidth
                            radius: Theme.scrollbarWidth / 2
                            // 长列表（内容溢出）显示句柄，短列表/空列表隐藏
                            visible: playlistScrollBar.size < 1.0
                            color: playlistScrollBar.pressed ? Theme.pressedColor
                                 : playlistScrollBar.hovered ? Theme.scrollbarHoverColor
                                 : Theme.scrollbarColor

                            Behavior on color {
                                ColorAnimation { duration: Theme.animationFast }
                            }
                        }
                    }

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
                        required property int year

                        readonly property bool hasFolderArtwork: delegate.isFolder && delegate.artworkSource.length > 0

                        width: playlistView.width
                        height: 72
                        topPadding: Theme.spacing8
                        bottomPadding: Theme.spacing8
                        leftPadding: 15
                        rightPadding: 15
                        Accessible.role: Accessible.ListItem
                        Accessible.name: isFolder ? name : title

                        onClicked: root.activateNode(nodeId, isFolder)

                        // 右键菜单（T14）：仅接受右键，左键事件继续穿透给 ItemDelegate
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onPressed: (mouse) => {
                                if (trackContextMenu.isOpen) {
                                    trackContextMenu.close();
                                }
                            }
                            onClicked: (mouse) => {
                                root.closeMenus();
                                trackContextMenu.openForEntry({
                                    nodeId: delegate.nodeId,
                                    trackId: delegate.trackId,
                                    isFolder: delegate.isFolder,
                                    name: delegate.name,
                                    title: delegate.title,
                                    artist: delegate.artist,
                                    album: delegate.album,
                                    parentName: delegate.parentName,
                                    songCount: delegate.songCount,
                                    duration: delegate.duration,
                                    format: delegate.format,
                                    sampleRate: delegate.sampleRate,
                                    bitDepth: delegate.bitDepth,
                                    artworkSource: delegate.artworkSource,
                                    year: delegate.year,
                                    path: root.appFacade.filePathForNodeId(delegate.nodeId)
                                }, delegate, mouse.x, mouse.y);
                            }
                        }

                        background: Rectangle {
                            color: delegate.hovered ? Theme.hoverColor : (delegate.isPlaying ? Theme.baseColor : "transparent")

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.animationFast
                                }
                            }

                            // 播放中左侧高亮指示条
                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.topMargin: Theme.spacing8
                                anchors.bottomMargin: Theme.spacing8
                                width: 3
                                radius: 1.5
                                color: Theme.accentColor
                                visible: delegate.isPlaying
                            }
                        }

                        contentItem: RowLayout {
                            spacing: Theme.spacing12

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 44
                                Layout.preferredHeight: 44
                                radius: 12
                                color: Theme.mainColor
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
                                    color: Theme.accentColor
                                    visible: delegate.isFolder && !delegate.hasFolderArtwork
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 12
                                    color: Theme.baseColor
                                    visible: !delegate.isFolder && folderThumbIcon.status !== Image.Ready
                                    antialiasing: true

                                    Text {
                                        anchors.centerIn: parent
                                        text: "♫"
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontHeading
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 1

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacing4

                                    Text {
                                        Layout.fillWidth: true
                                        text: delegate.isFolder ? delegate.name : delegate.title
                                        color: delegate.isPlaying ? Theme.accentColor : Theme.textPrimary
                                        font.pixelSize: Theme.fontBody
                                        font.weight: delegate.isPlaying ? Font.Bold : Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: delegate.isFolder ? delegate.parentName : (delegate.artist.length > 0 && delegate.album.length > 0 ? delegate.artist + " - " + delegate.album : delegate.artist + delegate.album)
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacing4

                                    RowLayout {
                                        visible: !delegate.isFolder
                                        spacing: Theme.spacing4

                                        Text {
                                            text: delegate.duration || ""
                                            color: Theme.textSecondary
                                            font.pixelSize: 10
                                        }
                                        Text {
                                            text: "|"
                                            color: Theme.borderColor
                                            font.pixelSize: 10
                                            visible: delegate.duration.length > 0 && delegate.format.length > 0
                                        }
                                        Text {
                                            text: delegate.format || ""
                                            color: Theme.textSecondary
                                            font.pixelSize: 10
                                        }
                                        Text {
                                            text: "|"
                                            color: Theme.borderColor
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
                                            color: Theme.borderColor
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
                                        spacing: Theme.spacing4

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
                                                color: Theme.textSecondary
                                                opacity: 0.7
                                            }
                                        }

                                        Text {
                                            text: qsTr("%1 Songs").arg(delegate.songCount)
                                            color: Theme.textSecondary
                                            font.pixelSize: 10
                                        }
                                        Text {
                                            text: "|"
                                            color: Theme.borderColor
                                            font.pixelSize: 10
                                            visible: delegate.duration.length > 0
                                        }
                                        Text {
                                            text: delegate.duration || ""
                                            color: Theme.textSecondary
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
                                            color: Theme.textOnAccent
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
                    width: parent.width - Theme.spacing24 * 2
                    text: root.emptyStateText
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: libraryController.visibleNodeCount === 0 && !root.queueViewActive
                }

                // 队列视图（T15）：展示临时队列，空队列显示引导文案；
                // 移除/右键命令经信号上抛，由下方 onRemoveRequested/
                // onContextMenuRequested 接 AppFacade 与 TrackContextMenu。
                QueueView {
                    id: queueView
                    objectName: "queueListView"
                    anchors.fill: parent
                    visible: root.queueViewActive
                    queueEntries: root.appFacade.playback.queueEntries

                    onRemoveRequested: (index) => root.appFacade.removeFromQueue(index)

                    onContextMenuRequested: (index, targetDelegate, mouseX, mouseY) => {
                        const entry = root.appFacade.playback.queueEntries[index];
                        if (!entry)
                            return;
                        root.closeMenus();
                        trackContextMenu.openForEntry({
                            nodeId: entry.nodeId,
                            trackId: entry.trackId,
                            isFolder: false,
                            name: entry.title,
                            title: entry.title,
                            artist: entry.artist,
                            album: "",
                            parentName: "",
                            songCount: 0,
                            duration: "",
                            format: "",
                            sampleRate: 0,
                            bitDepth: 0,
                            artworkSource: "",
                            year: 0,
                            path: root.appFacade.filePathForNodeId(entry.nodeId),
                            queueIndex: index
                        }, targetDelegate, mouseX, mouseY);
                    }
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
            anchors.margins: Theme.spacing16
            buttonWidth: 40
            buttonHeight: 40
            iconSize: 20
            iconSource: "qrc:/qt/qml/Seriona/qml/assets/my_location.svg"
            baseColor: Theme.accentColor
            textColor: Theme.textOnAccent
            z: 10
            visible: !root.queueViewActive

            onClicked: libraryController.locateCurrentSong()

            // Shadow for FAB
            layer.enabled: true
            layer.effect: DropShadow {
                transparentBorder: true
                horizontalOffset: Theme.shadowCardOffsetX
                verticalOffset: Theme.shadowCardOffsetY
                radius: Theme.shadowCardBlur
                samples: 17
                color: Theme.shadowPopupColor
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

    // 播放列表条目右键菜单（T14）：详情 / 添加到下一首播放 / 删除（含确认弹窗）；
    // "从队列移除"（T15）在队列视图上下文可见（queueContext 绑定视图状态）。
    TrackContextMenu {
        id: trackContextMenu
        objectName: "trackContextMenu"
        appFacade: root.appFacade
        queueContext: root.queueViewActive
    }
}
