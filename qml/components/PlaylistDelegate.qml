import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects
import Seriona

// 共享的列表项委托组件（含右键菜单触发、封面缩略图、内嵌导航滑入动画）
ItemDelegate {
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

    // 宿主注入三件套
    required property var activateNodeHandler
    required property var closeMenusHandler
    required property var contextMenuHost

    readonly property bool hasFolderArtwork: delegate.isFolder && delegate.artworkSource.length > 0

    width: ListView.view ? ListView.view.width : 0
    height: 72
    topPadding: Theme.spacing8
    bottomPadding: Theme.spacing8
    leftPadding: 15
    rightPadding: 15
    Accessible.role: Accessible.ListItem
    Accessible.name: isFolder ? name : title

    onClicked: activateNodeHandler(nodeId, isFolder)

    transform: Translate {
        id: navTranslate
        x: 0
    }

    SequentialAnimation {
        id: navSlideAnim

        PauseAnimation {
            id: navDelay
            duration: 0
        }

        ParallelAnimation {
            NumberAnimation {
                target: navTranslate
                property: "x"
                to: 0
                duration: 220
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: delegate
                property: "opacity"
                to: 1.0
                duration: 220
                easing.type: Easing.OutQuad
            }
        }
    }

    function startNavSlideIn(step, direction) {
        if (direction === 0) {
            navSlideAnim.stop();
            navTranslate.x = 0;
            delegate.opacity = 1.0;
            return;
        }
        navSlideAnim.stop();
        var startX = (direction > 0 ? 1 : -1) * delegate.width;
        navTranslate.x = startX;
        delegate.opacity = 0.0;
        navDelay.duration = Math.min(Math.max(0, step), 12) * 15;
        navSlideAnim.start();
    }

    // 右键菜单（T14）：仅接受右键，左键事件继续穿透给 ItemDelegate
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onPressed: (mouse) => {
            if (contextMenuHost && contextMenuHost.isOpen) {
                contextMenuHost.close();
            }
        }
        onClicked: (mouse) => {
            closeMenusHandler();
            contextMenuHost.openForEntry({
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
                year: delegate.year
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
