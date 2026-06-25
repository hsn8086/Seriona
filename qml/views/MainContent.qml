import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import QtQuick.Effects
import Seriona

Item {
    id: root

    signal coverClicked()
    signal coverDragRequested()
    signal backClicked()
    signal playlistToggled()
    property bool isSidebarOpen: false
    readonly property bool hasOpenMenu: mainMenu.visible

    // 播放状态属性 (Mock Data)
    property bool isPlaying: false
    property real currentPosition: 34 // 秒
    property real totalDuration: 225 // 秒
    property real volume: 0.7
    property bool isShuffle: false
    property int repeatMode: 0 // 0: 顺序, 1: 列表循环, 2: 单曲循环
    property int currentLyricIndex: 0
    property bool showTranslation: true
    property string lyricDelimiter: " / "
    property bool isTogglingTranslation: false
    readonly property real currentItemHeight: lyricsContainer.currentItem ? lyricsContainer.currentItem.height : 0
    readonly property real currentItemOriginalHeight: (lyricsContainer.currentItem && typeof lyricsContainer.currentItem.originalHeight !== "undefined") ? lyricsContainer.currentItem.originalHeight : 0
    readonly property real currentItemHeightUnscaled: (lyricsContainer.currentItem && typeof lyricsContainer.currentItem.fullHeightUnscaled !== "undefined") ? lyricsContainer.currentItem.fullHeightUnscaled : 0
    readonly property real currentItemOriginalHeightUnscaled: (lyricsContainer.currentItem && typeof lyricsContainer.currentItem.originalHeightUnscaled !== "undefined") ? lyricsContainer.currentItem.originalHeightUnscaled : 0

    onShowTranslationChanged: {
        isTogglingTranslation = true;
        toggleTranslationTimer.restart();
    }

    Timer {
        id: toggleTranslationTimer
        interval: 350
        onTriggered: root.isTogglingTranslation = false
    }

    property string songTitle: "Song Title"
    property string artistName: "Artist Name"
    property string albumName: "Album Name"

    // 模拟波形数据
    property var waveformHeights: [
        20, 30, 40, 35, 25, 15, 10, 20, 30, 45, 50, 40, 30, 20, 15, 25, 35, 40, 30, 20,
        15, 10, 20, 35, 45, 40, 30, 25, 35, 45, 50, 40, 30, 20, 15, 25, 35, 40, 30, 20,
        15, 10, 20, 35, 45, 40, 30, 25, 35, 45, 50, 40, 30, 20, 15, 25, 35, 40, 30, 20
    ]

    // 歌词数据
    readonly property var lyricsData: [
        qsTr("Music playing in the night... / 音乐在夜空中回荡..."),
        qsTr("Seriona shines so bright... / Seriona 闪耀着光芒..."),
        qsTr("A melody that guides the way... / 指引道路的旋律..."),
        qsTr("Through the dark and into day... / 穿过黑暗迎来黎明..."),
        qsTr("Feel the rhythm, feel the beat... / 感受旋律，感受节拍..."),
        qsTr("Walking down this lonely street... / 走在这条孤独的街上..."),
        qsTr("But with music in my soul... / 但只要我的灵魂有音乐..."),
        qsTr("I am happy, I am whole... / 我就快乐，我就完整...")
    ]

    // 时间格式化辅助函数
    function formatTime(seconds) {
        if (seconds < 0) seconds = 0;
        var m = Math.floor(seconds / 60);
        var s = Math.floor(seconds % 60);
        return (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s);
    }

    function closeMenus() {
        mainMenu.close();
    }

    // 播放进度模拟定时器
    Timer {
        interval: 1000
        running: root.isPlaying
        repeat: true
        onTriggered: {
            if (root.currentPosition < root.totalDuration) {
                root.currentPosition += 1;
            } else {
                root.currentPosition = 0;
            }
        }
    }

    // 歌词模拟定时器
    Timer {
        interval: 3000
        running: root.isPlaying && root.state === "lyrics"
        repeat: true
        onTriggered: {
            if (root.lyricsData.length > 0) {
                root.currentLyricIndex = (root.currentLyricIndex + 1) % root.lyricsData.length;
            }
        }
    }

    // 1. 播放布局定位辅助器 (仅在 playback 状态下用于定位)
    Item {
        id: positionHelper
        anchors.centerIn: parent
        width: 320
        height: 612
        visible: false
    }

    // 2. 封面组件 (共享元素)
    Item {
        id: coverContainer
        width: 240
        height: 240
        z: 10
        x: (parent.width - 240) / 2
        y: positionHelper.y

        RectangularGlow {
            id: coverGlow
            anchors.fill: coverRect
            glowRadius: 30
            spread: 0.0
            color: "#50000000"
            cornerRadius: coverRect.radius
            z: -1
            opacity: 1.0
        }

        Rectangle {
            id: coverRect
            anchors.fill: parent
            radius: 12
            color: Theme.mainColor
            clip: true

            Text {
                id: coverIcon
                anchors.centerIn: parent
                text: "🎵"
                font.pixelSize: 72
                color: Theme.textColor
                opacity: 0.6
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                property real pressX: 0
                property real pressY: 0
                property bool suppressClick: false

                onPressed: function (mouse) {
                    pressX = mouse.x;
                    pressY = mouse.y;
                    suppressClick = false;
                }

                onPositionChanged: function (mouse) {
                    if (pressed && root.state === "playback" && !root.hasOpenMenu && !suppressClick) {
                        var dx = mouse.x - pressX;
                        var dy = mouse.y - pressY;
                        if (Math.sqrt(dx * dx + dy * dy) >= Qt.styleHints.startDragDistance) {
                            suppressClick = true;
                            root.coverDragRequested();
                        }
                    }
                }

                onClicked: {
                    if (suppressClick) {
                        suppressClick = false;
                        return;
                    }

                    if (root.state === "playback") {
                        root.coverClicked();
                    } else {
                        root.backClicked();
                    }
                }
            }
        }
    }

    // 3. 歌曲元数据组件 (共享元素)
    Item {
        id: metadataContainer
        width: 320
        height: 80
        z: 9
        anchors.top: coverContainer.bottom
        anchors.topMargin: 20
        anchors.horizontalCenter: positionHelper.horizontalCenter

        Item {
            id: metadataLayout
            anchors.fill: parent

            MarqueeText {
                id: titleText
                width: Math.min(implicitWidth, parent.width)
                text: root.songTitle
                color: Theme.textColor
                font.pixelSize: 24
                font.bold: true
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
            }

            MarqueeText {
                id: artistText
                width: Math.min(implicitWidth, parent.width)
                text: root.artistName
                color: Theme.secondaryTextColor
                font.pixelSize: 16
                anchors.top: titleText.bottom
                anchors.topMargin: 5
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                id: dashText
                width: Math.min(implicitWidth, parent.width)
                text: " — "
                color: Theme.secondaryTextColor
                font.pixelSize: 12
                opacity: 0.0
                anchors.verticalCenter: artistText.verticalCenter
            }

            MarqueeText {
                id: albumText
                width: Math.min(implicitWidth, parent.width)
                text: root.albumName
                color: Theme.secondaryTextColor
                font.pixelSize: 12
                anchors.top: artistText.bottom
                anchors.topMargin: 5
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: root.state === "lyrics"
            cursorShape: Qt.PointingHandCursor
            onClicked: root.backClicked()
        }
    }

    // 4. 歌词滚动区域 (仅在 lyrics 状态下显示)
    ListView {
        id: lyricsContainer
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.lyricsData
        currentIndex: root.currentLyricIndex
        preferredHighlightBegin: height / 2 - root.currentItemHeightUnscaled / 2 - 30
        preferredHighlightEnd: preferredHighlightBegin
        
        Behavior on preferredHighlightBegin {
            id: highlightBehavior
            enabled: false
            NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
        }

        onCurrentIndexChanged: {
            if (!root.isTogglingTranslation) {
                highlightBehavior.enabled = true;
                disableBehaviorTimer.restart();
            }
        }

        Timer {
            id: disableBehaviorTimer
            interval: 450
            onTriggered: highlightBehavior.enabled = false
        }
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightMoveDuration: root.isTogglingTranslation ? 0 : 400
        opacity: 0.0
        visible: opacity > 0.0
        z: 5
        anchors.top: coverContainer.bottom
        anchors.bottom: linearProgressContainer.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Theme.paddingLarge
        anchors.bottomMargin: Theme.paddingLarge
        anchors.leftMargin: Theme.paddingLarge
        anchors.rightMargin: Theme.paddingLarge

        delegate: Item {
            id: delegateItem
            required property string modelData
            required property int index
            width: lyricsContainer.width
            height: lyricColumn.implicitHeight * lyricColumn.scale + Theme.paddingLarge

            readonly property bool isActive: index === root.currentLyricIndex
            readonly property real originalHeight: lyricText.implicitHeight * lyricColumn.scale + Theme.paddingLarge
            readonly property real originalHeightUnscaled: lyricText.implicitHeight + Theme.paddingLarge
            readonly property real fullHeightUnscaled: lyricColumn.implicitHeight + Theme.paddingLarge

            readonly property string originalText: {
                if (root.lyricDelimiter === "") return modelData;
                var idx = modelData.indexOf(root.lyricDelimiter);
                return idx !== -1 ? modelData.substring(0, idx).trim() : modelData;
            }

            readonly property string translationText: {
                if (root.lyricDelimiter === "") return "";
                var idx = modelData.indexOf(root.lyricDelimiter);
                return idx !== -1 ? modelData.substring(idx + root.lyricDelimiter.length).trim() : "";
            }

            Column {
                id: lyricColumn
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width - Theme.paddingLarge * 2, 600)
                spacing: 4

                transformOrigin: Item.TopLeft
                scale: delegateItem.isActive ? 1.0 : 0.75

                Behavior on scale {
                    NumberAnimation { duration: 350; easing.type: Easing.InOutQuad }
                }

                Text {
                    id: lyricText
                    width: parent.width
                    text: delegateItem.originalText
                    color: Theme.textColor
                    font.pixelSize: 32
                    font.weight: delegateItem.isActive ? Font.Bold : Font.Normal
                    opacity: delegateItem.isActive ? 1.0 : 0.4
                    horizontalAlignment: Text.AlignLeft
                    wrapMode: Text.WordWrap
                    Behavior on opacity {
                        NumberAnimation { duration: 350; easing.type: Easing.InOutQuad }
                    }
                }

                Text {
                    id: translationTextCtrl
                    width: parent.width
                    text: delegateItem.translationText
                    color: Theme.secondaryTextColor
                    font.pixelSize: 22
                    font.weight: delegateItem.isActive ? Font.Bold : Font.Normal
                    horizontalAlignment: Text.AlignLeft
                    wrapMode: Text.WordWrap
                    clip: true
                    
                    height: (root.showTranslation && delegateItem.translationText !== "") ? implicitHeight : 0
                    opacity: (root.showTranslation && delegateItem.translationText !== "") ? (delegateItem.isActive ? 0.8 : 0.3) : 0.0
                    visible: height > 0

                    Behavior on height {
                        NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
                    }
                    Behavior on opacity {
                        NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.currentLyricIndex = index
            }
        }
    }

    // 5. 波形进度条区域 (仅在 playback 状态下显示)
    Item {
        id: waveformProgressContainer
        width: 320
        height: 80
        opacity: 1.0
        visible: opacity > 0.0
        z: 8
        anchors.top: metadataContainer.bottom
        anchors.topMargin: 20
        anchors.horizontalCenter: positionHelper.horizontalCenter

        WaveformProgressBar {
            id: waveProgress
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: 320
            height: 60
            waveformHeights: root.waveformHeights
            progress: root.totalDuration > 0 ? (root.currentPosition / root.totalDuration) : 0
            onSeekRequested: function (pos) {
                root.currentPosition = pos * root.totalDuration;
            }
        }

        Item {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: 320
            height: 15

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: waveProgress.isHovering ? root.formatTime(root.totalDuration * waveProgress.hoverProgress) : root.formatTime(root.currentPosition)
                color: Theme.secondaryTextColor
                font.pixelSize: 12
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    if (waveProgress.isHovering) {
                        var remainingPreview = root.totalDuration * (1.0 - waveProgress.hoverProgress);
                        return "-" + root.formatTime(remainingPreview);
                    } else {
                        var remaining = root.totalDuration - root.currentPosition;
                        return "-" + root.formatTime(remaining);
                    }
                }
                color: Theme.secondaryTextColor
                font.pixelSize: 12
            }
        }
    }

    // 6. 线性进度条区域 (仅在 lyrics 状态下显示)
    Item {
        id: linearProgressContainer
        height: 40
        opacity: 0.0
        visible: opacity > 0.0
        z: 7
        anchors.top: metadataContainer.bottom
        anchors.left: positionHelper.left
        anchors.right: positionHelper.right

        Slider {
            id: progressSlider
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 20
            from: 0
            to: root.totalDuration
            value: root.currentPosition
            onMoved: {
                root.currentPosition = value;
            }

            background: Rectangle {
                x: progressSlider.leftPadding
                y: progressSlider.topPadding + progressSlider.availableHeight / 2 - height / 2
                width: progressSlider.availableWidth
                height: 4
                radius: 2
                color: Theme.baseColor

                Rectangle {
                    width: progressSlider.visualPosition * parent.width
                    height: parent.height
                    color: Theme.accentColor
                    radius: 2
                }
            }

            handle: Rectangle {
                x: progressSlider.leftPadding + progressSlider.visualPosition * (progressSlider.availableWidth - width)
                y: progressSlider.topPadding + progressSlider.availableHeight / 2 - height / 2
                width: 12
                height: 12
                radius: 6
                color: Theme.textColor
                visible: progressSlider.hovered || progressSlider.pressed
            }
        }

        Item {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 15

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: root.formatTime(root.currentPosition)
                color: Theme.secondaryTextColor
                font.pixelSize: 12
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: "-" + root.formatTime(root.totalDuration - root.currentPosition)
                color: Theme.secondaryTextColor
                font.pixelSize: 12
            }
        }
    }

    // 7. 播放控制按钮区域 (共享元素)
    Item {
        id: controlsContainer
        width: 320
        height: 60
        z: 6
        anchors.top: waveformProgressContainer.bottom
        anchors.topMargin: 20
        anchors.horizontalCenter: positionHelper.horizontalCenter

        StyleButton {
            id: prevButton
            width: 45
            height: 45
            anchors.verticalCenter: playButton.verticalCenter
            anchors.right: playButton.left
            anchors.rightMargin: 40
            iconSource: "qrc:/qt/qml/Seriona/qml/assets/prev.svg"
            textColor: Theme.textColor
            onClicked: console.log("Prev clicked")
        }

        StyleButton {
            id: playButton
            width: 60
            height: 60
            anchors.centerIn: parent
            iconSource: root.isPlaying ? "qrc:/qt/qml/Seriona/qml/assets/pause.svg" : "qrc:/qt/qml/Seriona/qml/assets/play.svg"
            baseColor: Theme.playButtonBg
            hoverColor: Qt.darker(Theme.playButtonBg, 1.1)
            pressedColor: Qt.darker(Theme.playButtonBg, 1.2)
            textColor: Theme.playButtonText
            onClicked: root.isPlaying = !root.isPlaying
        }

        StyleButton {
            id: nextButton
            width: 45
            height: 45
            anchors.verticalCenter: playButton.verticalCenter
            anchors.left: playButton.right
            anchors.leftMargin: 40
            iconSource: "qrc:/qt/qml/Seriona/qml/assets/next.svg"
            textColor: Theme.textColor
            onClicked: console.log("Next clicked")
        }
    }

    // 8. 音量控制区域 (仅在 playback 状态下显示)
    RowLayout {
        id: volumeContainer
        width: 320
        height: 40
        spacing: 15
        opacity: 1.0
        visible: opacity > 0.0
        z: 4
        anchors.top: controlsContainer.bottom
        anchors.topMargin: 15
        anchors.horizontalCenter: positionHelper.horizontalCenter

        Item {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            Image {
                id: volDownIcon
                anchors.fill: parent
                source: "qrc:/qt/qml/Seriona/qml/assets/volume_down.svg"
                sourceSize.width: 20
                sourceSize.height: 20
                visible: false
            }
            ColorOverlay {
                anchors.fill: volDownIcon
                source: volDownIcon
                color: Theme.secondaryTextColor
            }
        }

        Slider {
            id: volumeSlider
            Layout.preferredWidth: 240
            Layout.alignment: Qt.AlignVCenter
            from: 0.0
            to: 1.0
            value: root.volume
            onValueChanged: root.volume = value
            background: Rectangle {
                x: volumeSlider.leftPadding
                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                width: volumeSlider.availableWidth
                height: 12
                radius: 6
                color: Theme.baseColor
                Rectangle {
                    width: volumeSlider.visualPosition * parent.width
                    height: parent.height
                    color: Theme.checkedColor
                    radius: 6
                }
            }
            handle: Item {}
        }

        Item {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            Image {
                id: volUpIcon
                anchors.fill: parent
                source: "qrc:/qt/qml/Seriona/qml/assets/volume_up.svg"
                sourceSize.width: 20
                sourceSize.height: 20
                visible: false
            }
            ColorOverlay {
                anchors.fill: volUpIcon
                source: volUpIcon
                color: Theme.secondaryTextColor
            }
        }
    }

    // 9. 底部功能按钮区域 (仅在 playback 状态下显示)
    RowLayout {
        id: bottomRowContainer
        width: 320
        height: 40
        opacity: 1.0
        visible: opacity > 0.0
        z: 3
        anchors.top: volumeContainer.bottom
        anchors.topMargin: 15
        anchors.horizontalCenter: positionHelper.horizontalCenter

        Row {
            spacing: 20
            StyleButton {
                buttonWidth: 40
                buttonHeight: 40
                iconSource: "qrc:/qt/qml/Seriona/qml/assets/playlist.svg"
                textColor: Theme.textColor
                checkable: true
                checked: root.isSidebarOpen
                onClicked: root.playlistToggled()
            }
            StyleButton {
                buttonWidth: 40
                buttonHeight: 40
                iconSource: root.isShuffle ? "qrc:/qt/qml/Seriona/qml/assets/shuffle_on.svg" : "qrc:/qt/qml/Seriona/qml/assets/shuffle_off.svg"
                textColor: Theme.textColor
                checkable: true
                checked: root.isShuffle
                onClicked: root.isShuffle = !root.isShuffle
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Row {
            spacing: 20
            StyleButton {
                iconSource: root.repeatMode === 1 ? "qrc:/qt/qml/Seriona/qml/assets/repeat_list.svg" : root.repeatMode === 2 ? "qrc:/qt/qml/Seriona/qml/assets/repeat_one.svg" : "qrc:/qt/qml/Seriona/qml/assets/repeat_off.svg"
                buttonWidth: 40
                buttonHeight: 40
                textColor: Theme.textColor
                onClicked: root.repeatMode = (root.repeatMode + 1) % 3
            }
            StyleButton {
                id: settingsBtn
                buttonWidth: 40
                buttonHeight: 40
                iconSource: "qrc:/qt/qml/Seriona/qml/assets/settings.svg"
                textColor: Theme.textColor
                onClicked: mainMenu.toggle()

                BubbleMenu {
                    id: mainMenu
                    menuWidth: 180
                    arrowDirection: "down"
                    targetItem: settingsBtn

                    BubbleSubMenuItem {
                        text: qsTr("Settings")
                        title: qsTr("Settings")
                        page: Component {
                            Column {
                                width: parent ? parent.width : 180
                                spacing: 12

                                Item {
                                    width: parent.width
                                    height: 140

                                    Column {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        anchors.topMargin: 8
                                        spacing: 8

                                        Text {
                                            text: qsTr("Lyric Delimiter")
                                            color: Theme.textColor
                                            font.pixelSize: 13
                                            font.bold: true
                                        }

                                        TextField {
                                            id: customInput
                                            width: parent.width
                                            height: 28
                                            font.pixelSize: 12
                                            text: root.lyricDelimiter
                                            color: Theme.textColor
                                            placeholderText: "e.g. /"
                                            placeholderTextColor: "#80FFFFFF"
                                            background: Rectangle {
                                                color: "#15FFFFFF"
                                                border.color: customInput.activeFocus ? Theme.accentColor : "#30FFFFFF"
                                                border.width: 1
                                                radius: 4
                                            }
                                            onTextChanged: {
                                                root.lyricDelimiter = text;
                                            }
                                        }

                                        Text {
                                            text: qsTr("Presets:")
                                            color: Theme.secondaryTextColor
                                            font.pixelSize: 11
                                        }

                                        Row {
                                            spacing: 6
                                            width: parent.width

                                            Repeater {
                                                model: [" / ", " | ", " - ", " // "]
                                                
                                                Rectangle {
                                                    width: 32
                                                    height: 22
                                                    radius: 4
                                                    color: root.lyricDelimiter === modelData ? Theme.accentColor : "#15FFFFFF"
                                                    border.color: "#30FFFFFF"
                                                    border.width: 1
                                                    
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: modelData.trim() === "" ? modelData : modelData.trim()
                                                        color: Theme.textColor
                                                        font.pixelSize: 11
                                                    }

                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            root.lyricDelimiter = modelData;
                                                            customInput.text = modelData;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    BubbleSubMenuItem {
                        text: qsTr("Playback")
                        title: qsTr("Playback")
                        page: Component {
                            Column {
                                width: parent ? parent.width : 160
                                spacing: 0

                                BubbleMenuItem { text: qsTr("Crossfade"); onTriggered: mainMenu.close() }
                                BubbleMenuItem { text: qsTr("Gapless Playback"); onTriggered: mainMenu.close() }
                                BubbleMenuItem { text: qsTr("ReplayGain"); onTriggered: mainMenu.close() }
                            }
                        }
                    }
                    BubbleMenuItem { text: qsTr("Equalizer"); onTriggered: mainMenu.close() }
                    BubbleMenuItem { text: qsTr("About Seriona"); onTriggered: mainMenu.close() }
                    BubbleMenuItem { text: qsTr("Exit"); onTriggered: mainMenu.close() }
                }
            }
        }
    }

    // 10. 打开/关闭翻译按钮 (仅在 lyrics 状态下显示，位于右下角)
    StyleButton {
        id: toggleTranslationBtn
        buttonWidth: 36
        buttonHeight: 36
        iconSource: "qrc:/qt/qml/Seriona/qml/assets/translate.svg"
        textColor: Theme.textColor
        checkable: true
        checked: root.showTranslation
        z: 100
        opacity: 0.0
        visible: opacity > 0.0
        
        anchors.right: parent.right
        anchors.rightMargin: Theme.paddingLarge
        anchors.bottom: linearProgressContainer.top
        anchors.bottomMargin: Theme.paddingMedium

        onClicked: root.showTranslation = !root.showTranslation
    }

    // 状态定义
    state: "playback"

    states: [
        State {
            name: "playback"
            PropertyChanges {
                target: coverContainer
                x: (parent.width - 240) / 2
                y: positionHelper.y
                width: 240
                height: 240
            }
            AnchorChanges {
                target: titleText
                anchors.horizontalCenter: metadataLayout.horizontalCenter
                anchors.left: undefined
            }
            AnchorChanges {
                target: artistText
                anchors.horizontalCenter: metadataLayout.horizontalCenter
                anchors.left: undefined
            }
            AnchorChanges {
                target: albumText
                anchors.horizontalCenter: metadataLayout.horizontalCenter
                anchors.top: artistText.bottom
                anchors.left: undefined
                anchors.verticalCenter: undefined
            }
            AnchorChanges {
                target: dashText
                anchors.left: artistText.right
                anchors.verticalCenter: artistText.verticalCenter
            }
            PropertyChanges {
                target: dashText
                opacity: 0.0
            }
            PropertyChanges {
                target: prevButton
                anchors.rightMargin: 40
            }
            PropertyChanges {
                target: nextButton
                anchors.leftMargin: 40
            }
            PropertyChanges {
                target: waveformProgressContainer
                height: 80
            }
            PropertyChanges {
                target: waveProgress
                flatMode: false
                height: 60
            }
            PropertyChanges {
                target: toggleTranslationBtn
                opacity: 0.0
            }
        },
        State {
            name: "lyrics"
            AnchorChanges {
                target: metadataContainer
                anchors.top: parent.top
                anchors.left: coverContainer.right
                anchors.right: parent.right
                anchors.horizontalCenter: undefined
            }
            AnchorChanges {
                target: titleText
                anchors.left: metadataLayout.left
                anchors.horizontalCenter: undefined
            }
            AnchorChanges {
                target: artistText
                anchors.left: metadataLayout.left
                anchors.horizontalCenter: undefined
            }
            AnchorChanges {
                target: dashText
                anchors.left: artistText.right
                anchors.verticalCenter: artistText.verticalCenter
            }
            AnchorChanges {
                target: albumText
                anchors.left: dashText.right
                anchors.verticalCenter: artistText.verticalCenter
                anchors.top: undefined
                anchors.horizontalCenter: undefined
            }
            AnchorChanges {
                target: linearProgressContainer
                anchors.top: undefined
                anchors.bottom: controlsContainer.top
                anchors.left: parent.left
                anchors.right: parent.right
            }
            AnchorChanges {
                target: controlsContainer
                anchors.top: undefined
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
            }
            AnchorChanges {
                target: waveformProgressContainer
                anchors.top: undefined
                anchors.bottom: controlsContainer.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.horizontalCenter: undefined
            }
            PropertyChanges {
                target: coverContainer
                x: Theme.paddingLarge
                y: Theme.paddingLarge
                width: 40
                height: 40
            }
            PropertyChanges {
                target: coverRect
                radius: 4
            }
            PropertyChanges {
                target: coverIcon
                font.pixelSize: 18
            }
            PropertyChanges {
                target: coverGlow
                opacity: 0.0
            }
            PropertyChanges {
                target: metadataContainer
                anchors.topMargin: Theme.paddingLarge
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.paddingLarge
            }
            PropertyChanges {
                target: titleText
                font.pixelSize: 14
            }
            PropertyChanges {
                target: artistText
                width: implicitWidth
                font.pixelSize: 12
            }
            PropertyChanges {
                target: dashText
                opacity: 1.0
            }
            PropertyChanges {
                target: albumText
                width: Math.max(0, Math.min(implicitWidth, metadataLayout.width - artistText.implicitWidth - dashText.implicitWidth))
                anchors.leftMargin: 0
                opacity: 1.0
                visible: true
            }
            PropertyChanges {
                target: lyricsContainer
                opacity: 1.0
            }
            PropertyChanges {
                target: linearProgressContainer
                anchors.bottomMargin: Theme.paddingMedium
                anchors.leftMargin: Theme.paddingLarge
                anchors.rightMargin: Theme.paddingLarge
                opacity: 1.0
            }
            PropertyChanges {
                target: controlsContainer
                anchors.bottomMargin: Theme.paddingLarge
            }
            PropertyChanges {
                target: prevButton
                anchors.rightMargin: Theme.paddingLarge
            }
            PropertyChanges {
                target: nextButton
                anchors.leftMargin: Theme.paddingLarge
            }
            PropertyChanges {
                target: waveformProgressContainer
                anchors.leftMargin: Theme.paddingLarge
                anchors.rightMargin: Theme.paddingLarge
                anchors.bottomMargin: Theme.paddingMedium
                opacity: 0.0
                height: 60
            }
            PropertyChanges {
                target: waveProgress
                flatMode: true
                height: 40
            }
            PropertyChanges {
                target: volumeContainer
                opacity: 0.0
            }
            PropertyChanges {
                target: bottomRowContainer
                opacity: 0.0
            }
            PropertyChanges {
                target: toggleTranslationBtn
                opacity: 0.8
            }
        }
    ]

    transitions: [
        Transition {
            from: "playback"
            to: "lyrics"

            AnchorAnimation {
                targets: [metadataContainer, controlsContainer, waveformProgressContainer, linearProgressContainer, titleText, artistText, albumText, dashText]
                duration: 400
                easing.type: Easing.InOutCubic
            }

            NumberAnimation {
                target: coverContainer
                properties: "x,y,width,height"
                duration: 400
                easing.type: Easing.InOutCubic
            }

            NumberAnimation {
                targets: [coverRect, coverIcon, coverGlow, titleText, artistText, albumText, dashText, prevButton, nextButton]
                properties: "radius,font.pixelSize,opacity,spacing,anchors.topMargin,anchors.leftMargin,anchors.rightMargin,anchors.bottomMargin"
                duration: 400
                easing.type: Easing.InOutCubic
            }

            NumberAnimation {
                targets: [waveProgress, waveformProgressContainer]
                property: "height"
                duration: 400
                easing.type: Easing.InOutCubic
            }

            NumberAnimation {
                targets: [volumeContainer, bottomRowContainer]
                property: "opacity"
                duration: 180
                easing.type: Easing.OutQuad
            }

            NumberAnimation {
                target: waveformProgressContainer
                property: "opacity"
                duration: 300
                easing.type: Easing.OutQuad
            }

            SequentialAnimation {
                PauseAnimation { duration: 150 }
                NumberAnimation {
                    targets: [lyricsContainer, linearProgressContainer, toggleTranslationBtn]
                    property: "opacity"
                    duration: 250
                    easing.type: Easing.OutQuad
                }
            }
        },
        Transition {
            from: "lyrics"
            to: "playback"

            AnchorAnimation {
                targets: [metadataContainer, controlsContainer, waveformProgressContainer, linearProgressContainer, titleText, artistText, albumText, dashText]
                duration: 400
                easing.type: Easing.InOutCubic
            }

            NumberAnimation {
                target: coverContainer
                properties: "x,y,width,height"
                duration: 400
                easing.type: Easing.InOutCubic
            }

            NumberAnimation {
                targets: [coverRect, coverIcon, coverGlow, titleText, artistText, albumText, dashText, prevButton, nextButton]
                properties: "radius,font.pixelSize,opacity,spacing,anchors.topMargin,anchors.leftMargin,anchors.rightMargin,anchors.bottomMargin"
                duration: 400
                easing.type: Easing.InOutCubic
            }

            NumberAnimation {
                targets: [waveProgress, waveformProgressContainer]
                property: "height"
                duration: 400
                easing.type: Easing.InOutCubic
            }

            NumberAnimation {
                targets: [lyricsContainer, linearProgressContainer, toggleTranslationBtn]
                property: "opacity"
                duration: 180
                easing.type: Easing.OutQuad
            }

            SequentialAnimation {
                PauseAnimation { duration: 150 }
                NumberAnimation {
                    targets: [waveformProgressContainer, volumeContainer, bottomRowContainer]
                    property: "opacity"
                    duration: 250
                    easing.type: Easing.OutQuad
                }
            }
        }
    ]
}
