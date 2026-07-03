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
    required property PlaybackController playbackController
    readonly property bool hasOpenMenu: mainMenu.visible

    property bool isTogglingTranslation: false
    readonly property real currentItemHeight: lyricsContainer.currentItem ? lyricsContainer.currentItem.height : 0
    readonly property real currentItemOriginalHeight: (lyricsContainer.currentItem && typeof lyricsContainer.currentItem.originalHeight !== "undefined") ? lyricsContainer.currentItem.originalHeight : 0
    readonly property real currentItemHeightUnscaled: (lyricsContainer.currentItem && typeof lyricsContainer.currentItem.fullHeightUnscaled !== "undefined") ? lyricsContainer.currentItem.fullHeightUnscaled : 0
    readonly property real currentItemOriginalHeightUnscaled: (lyricsContainer.currentItem && typeof lyricsContainer.currentItem.originalHeightUnscaled !== "undefined") ? lyricsContainer.currentItem.originalHeightUnscaled : 0

    LyricsModel {
        id: lyricsState
        advancing: root.playbackController.isPlaying && root.state === "lyrics"
    }

    Connections {
        target: lyricsState

        function onShowTranslationChanged() {
            root.isTogglingTranslation = true;
            toggleTranslationTimer.restart();
        }
    }

    Timer {
        id: toggleTranslationTimer
        interval: 350
        onTriggered: root.isTogglingTranslation = false
    }

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

    // 1. 播放布局定位辅助器 (仅在 playback 状态下用于定位)
    Item {
        id: positionHelper
        anchors.centerIn: parent
        width: 320
        height: 604
        visible: false
    }

    // 2. 封面组件 (共享元素)
    Item {
        id: coverContainer
        width: 250
        height: 250
        z: 10
        x: (parent.width - 250) / 2
        y: positionHelper.y

        RectangularGlow {
            id: coverGlow
            anchors.fill: coverRect
            glowRadius: 40
            spread: 0.1
            color: "#40000000"
            cornerRadius: coverRect.radius + 15
            z: -1
            opacity: 1.0
        }

        Rectangle {
            id: coverRect
            anchors.fill: parent
            radius: 16
            color: Theme.mainColor
            clip: true

            Text {
                id: coverIcon
                anchors.centerIn: parent
                text: root.playbackController.coverPlaceholderText
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
        height: 64
        z: 9
        anchors.top: coverContainer.bottom
        anchors.topMargin: 30
        anchors.horizontalCenter: positionHelper.horizontalCenter

        Item {
            id: metadataLayout
            anchors.fill: parent

            MarqueeText {
                id: titleText
                width: Math.min(implicitWidth, parent.width)
                text: root.playbackController.songTitle
                color: Theme.textColor
                font.pixelSize: 22
                font.bold: true
                font.letterSpacing: 0.5
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
            }

            MarqueeText {
                id: artistText
                width: Math.min(implicitWidth, parent.width)
                text: root.playbackController.artistName
                color: Theme.secondaryTextColor
                font.pixelSize: 14
                font.weight: Font.Medium
                anchors.top: titleText.bottom
                anchors.topMargin: 6
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                id: dashText
                width: Math.min(implicitWidth, parent.width)
                text: " — "
                color: Theme.secondaryTextColor
                font.pixelSize: 14
                opacity: 0.0
                anchors.verticalCenter: artistText.verticalCenter
            }

            MarqueeText {
                id: albumText
                width: Math.min(implicitWidth, parent.width)
                text: root.playbackController.albumName
                color: Theme.secondaryTextColor
                font.pixelSize: 13
                opacity: 0.8
                anchors.top: artistText.bottom
                anchors.topMargin: 4
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
        model: lyricsState
        currentIndex: lyricsState.currentIndex
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
            required property int index
            required property string displayLine
            required property string translation
            required property bool isCurrent
            width: lyricsContainer.width
            height: lyricColumn.implicitHeight * lyricColumn.scale + Theme.paddingLarge

            readonly property bool isActive: isCurrent
            readonly property real originalHeight: lyricText.implicitHeight * lyricColumn.scale + Theme.paddingLarge
            readonly property real originalHeightUnscaled: lyricText.implicitHeight + Theme.paddingLarge
            readonly property real fullHeightUnscaled: lyricColumn.implicitHeight + Theme.paddingLarge

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
                    text: delegateItem.displayLine
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
                    text: delegateItem.translation
                    color: Theme.secondaryTextColor
                    font.pixelSize: 22
                    font.weight: delegateItem.isActive ? Font.Bold : Font.Normal
                    horizontalAlignment: Text.AlignLeft
                    wrapMode: Text.WordWrap
                    clip: true
                    
                    height: (lyricsState.showTranslation && delegateItem.translation !== "") ? implicitHeight : 0
                    opacity: (lyricsState.showTranslation && delegateItem.translation !== "") ? (delegateItem.isActive ? 0.8 : 0.3) : 0.0
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
                onClicked: lyricsState.selectLyric(index)
            }
        }
    }

    // 5. 波形进度条区域 (仅在 playback 状态下显示)
    Item {
        id: waveformProgressContainer
        width: 320
        height: 60
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
            height: 44
            waveformHeights: root.playbackController.waveformHeights
            progress: root.playbackController.totalDuration > 0 ? (root.playbackController.currentPosition / root.playbackController.totalDuration) : 0
            onSeekRequested: function (pos) {
                root.playbackController.seek(pos * root.playbackController.totalDuration);
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
                text: waveProgress.isHovering ? root.formatTime(root.playbackController.totalDuration * waveProgress.hoverProgress) : root.playbackController.currentPositionText
                color: Theme.secondaryTextColor
                font.pixelSize: 11
                font.weight: Font.Medium
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    if (waveProgress.isHovering) {
                        var remainingPreview = root.playbackController.totalDuration * (1.0 - waveProgress.hoverProgress);
                        return "-" + root.formatTime(remainingPreview);
                    } else {
                        return root.playbackController.remainingDurationText;
                    }
                }
                color: Theme.secondaryTextColor
                font.pixelSize: 11
                font.weight: Font.Medium
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
            to: root.playbackController.totalDuration
            value: root.playbackController.currentPosition
            onMoved: {
                root.playbackController.seek(value);
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
                text: root.playbackController.currentPositionText
                color: Theme.secondaryTextColor
                font.pixelSize: 11
                font.weight: Font.Medium
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.playbackController.remainingDurationText
                color: Theme.secondaryTextColor
                font.pixelSize: 11
                font.weight: Font.Medium
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
            anchors.rightMargin: 35
            iconSource: "qrc:/qt/qml/Seriona/qml/assets/prev.svg"
            textColor: Theme.textColor
            onClicked: root.playbackController.skipPrevious()
        }

        StyleButton {
            id: playButton
            width: 56
            height: 56
            anchors.centerIn: parent
            iconSource: root.playbackController.isPlaying ? "qrc:/qt/qml/Seriona/qml/assets/pause.svg" : "qrc:/qt/qml/Seriona/qml/assets/play.svg"
            baseColor: Theme.playButtonBg
            hoverColor: Qt.darker(Theme.playButtonBg, 1.1)
            pressedColor: Qt.darker(Theme.playButtonBg, 1.2)
            textColor: Theme.playButtonText
            onClicked: root.playbackController.togglePlay()
        }

        StyleButton {
            id: nextButton
            width: 45
            height: 45
            anchors.verticalCenter: playButton.verticalCenter
            anchors.left: playButton.right
            anchors.leftMargin: 35
            iconSource: "qrc:/qt/qml/Seriona/qml/assets/next.svg"
            textColor: Theme.textColor
            onClicked: root.playbackController.skipNext()
        }
    }

    // 8. 音量控制区域 (仅在 playback 状态下显示)
    RowLayout {
        id: volumeContainer
        width: 320
        height: 30
        spacing: 12
        opacity: 1.0
        visible: opacity > 0.0
        z: 4
        anchors.top: controlsContainer.bottom
        anchors.topMargin: 15
        anchors.horizontalCenter: positionHelper.horizontalCenter

        Item {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Image {
                id: volDownIcon
                anchors.fill: parent
                source: "qrc:/qt/qml/Seriona/qml/assets/volume_down.svg"
                sourceSize.width: 16
                sourceSize.height: 16
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
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            from: 0.0
            to: 1.0
            value: root.playbackController.volume
            onMoved: root.playbackController.setVolume(value)
            background: Rectangle {
                x: volumeSlider.leftPadding
                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                width: volumeSlider.availableWidth
                height: 4
                radius: 2
                color: Theme.baseColor
                Rectangle {
                    width: volumeSlider.visualPosition * parent.width
                    height: parent.height
                    color: Theme.textColor
                    radius: 2
                }
            }
            handle: Rectangle {
                x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                width: 10
                height: 10
                radius: 5
                color: Theme.textColor
                visible: volumeSlider.hovered || volumeSlider.pressed
            }
        }

        Item {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Image {
                id: volUpIcon
                anchors.fill: parent
                source: "qrc:/qt/qml/Seriona/qml/assets/volume_up.svg"
                sourceSize.width: 16
                sourceSize.height: 16
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
            spacing: 16
            StyleButton {
                buttonWidth: 36
                buttonHeight: 36
                iconSource: "qrc:/qt/qml/Seriona/qml/assets/playlist.svg"
                textColor: Theme.textColor
                checkable: true
                checked: root.isSidebarOpen
                onClicked: root.playlistToggled()
            }
            StyleButton {
                buttonWidth: 36
                buttonHeight: 36
                iconSource: root.playbackController.isShuffle ? "qrc:/qt/qml/Seriona/qml/assets/shuffle_on.svg" : "qrc:/qt/qml/Seriona/qml/assets/shuffle_off.svg"
                textColor: Theme.textColor
                checkable: true
                checked: root.playbackController.isShuffle
                onClicked: root.playbackController.toggleShuffle()
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Row {
            spacing: 16
            StyleButton {
                iconSource: root.playbackController.repeatMode === 1 ? "qrc:/qt/qml/Seriona/qml/assets/repeat_list.svg" : root.playbackController.repeatMode === 2 ? "qrc:/qt/qml/Seriona/qml/assets/repeat_one.svg" : "qrc:/qt/qml/Seriona/qml/assets/repeat_off.svg"
                buttonWidth: 36
                buttonHeight: 36
                textColor: Theme.textColor
                onClicked: root.playbackController.cycleRepeatMode()
            }
            StyleButton {
                id: settingsBtn
                buttonWidth: 36
                buttonHeight: 36
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
                                            text: lyricsState.lyricDelimiter
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
                                                lyricsState.lyricDelimiter = text;
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
                                                    color: lyricsState.lyricDelimiter === modelData ? Theme.accentColor : "#15FFFFFF"
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
                                                            lyricsState.lyricDelimiter = modelData;
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
        checked: lyricsState.showTranslation
        z: 100
        opacity: 0.0
        visible: opacity > 0.0
        
        anchors.right: parent.right
        anchors.rightMargin: Theme.paddingLarge
        anchors.bottom: linearProgressContainer.top
        anchors.bottomMargin: Theme.paddingMedium

        onClicked: lyricsState.toggleTranslation()
    }

    // 状态定义
    state: "playback"

    states: [
        State {
            name: "playback"
            PropertyChanges {
                target: coverContainer
                x: (parent.width - 250) / 2
                y: positionHelper.y
                width: 250
                height: 250
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
                anchors.rightMargin: 35
            }
            PropertyChanges {
                target: nextButton
                anchors.leftMargin: 35
            }
            PropertyChanges {
                target: waveformProgressContainer
                height: 60
            }
            PropertyChanges {
                target: waveProgress
                flatMode: false
                height: 44
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
                width: 44
                height: 44
            }
            PropertyChanges {
                target: coverRect
                radius: 8
            }
            PropertyChanges {
                target: coverIcon
                font.pixelSize: 20
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
                font.pixelSize: 16
            }
            PropertyChanges {
                target: artistText
                width: implicitWidth
                font.pixelSize: 13
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
