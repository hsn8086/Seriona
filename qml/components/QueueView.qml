import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Seriona

// 临时播放队列视图（T15）：展示 PlaybackController.queueEntries 映射条目
// （{trackId, nodeId, title, artist, isPlaying}）。只做展示与移除命令上抛，
// 不混入文件夹列表；空队列显示"添加到下一首播放"引导说明。命令经信号上抛，
// 由 Sidebar 接 appFacade（removeRequested → removeFromQueue(queueIndex)，
// queueIndex 即队列列表下标）。
//
// 队列是短时列表（插播几首），条目用命令式创建（createObject）而非
// ListView 虚拟化：条目在 queueEntries 变化时同步物化，无懒加载布局依赖；
// offscreen 测试环境与即时显示均可靠。队列变化（插入/移除/消费）都经后端
// 快照 → queueEntries 变化 → 整体重建，下标恒与当前 queueEntries 一致。
Item {
    id: root

    required property var queueEntries

    signal removeRequested(int index)
    signal contextMenuRequested(int index, real globalX, real globalY)

    readonly property bool isEmpty: !root.queueEntries || root.queueEntries.length === 0
    readonly property int count: root.isEmpty ? 0 : root.queueEntries.length
    readonly property string emptyTitle: qsTr("队列为空")
    readonly property string emptyHint: qsTr("右键任意歌曲选择「添加到下一首播放」；队列播完自动回到文件夹列表")

    Column {
        id: queueList
        objectName: "queueList"
        anchors.fill: parent
        clip: true
        visible: !root.isEmpty
    }

    function rebuild() {
        for (let i = queueList.children.length - 1; i >= 0; --i) {
            queueList.children[i].destroy();
        }
        const entries = root.queueEntries || [];
        for (let i = 0; i < entries.length; ++i) {
            const entry = entries[i];
            queueDelegateComponent.createObject(queueList, {
                trackId: entry.trackId,
                nodeId: entry.nodeId,
                title: entry.title,
                artist: entry.artist,
                isPlaying: entry.isPlaying,
                rowIndex: i
            });
        }
    }

    onQueueEntriesChanged: root.rebuild()

    Component.onCompleted: root.rebuild()

    Component {
        id: queueDelegateComponent

        ItemDelegate {
            id: delegate
            required property string trackId
            required property string nodeId
            required property string title
            required property string artist
            required property bool isPlaying
            required property int rowIndex

            objectName: "queueDelegate" + delegate.rowIndex

            width: queueList.width
            height: 56
            topPadding: Theme.spacing4
            bottomPadding: Theme.spacing4
            leftPadding: Theme.spacing16
            rightPadding: Theme.spacing16
            Accessible.role: Accessible.ListItem
            Accessible.name: title

            // 右键菜单（T15）：仅接受右键，左键事件继续穿透给 ItemDelegate
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: (mouse) => {
                    const global = delegate.mapToGlobal(mouse.x, mouse.y);
                    root.contextMenuRequested(delegate.rowIndex, global.x, global.y);
                }
            }

            background: Rectangle {
                color: delegate.hovered ? Theme.queueItemHoverBg : "transparent"
                radius: Theme.radiusSmall

                Behavior on color {
                    ColorAnimation {
                        duration: Theme.animationFast
                    }
                }
            }

            contentItem: RowLayout {
                spacing: Theme.spacing12

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    radius: Theme.radiusSmall
                    color: delegate.isPlaying ? Theme.accentColor : Theme.raisedSurfaceColor
                    antialiasing: true

                    Behavior on color {
                        ColorAnimation { duration: Theme.animationFast }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "♫"
                        color: delegate.isPlaying ? Theme.textOnAccent : Theme.textPrimary
                        font.pixelSize: Theme.fontSubtitle
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: Theme.spacing2

                    Text {
                        objectName: "queueTitle" + delegate.rowIndex
                        Layout.fillWidth: true
                        text: delegate.title
                        color: delegate.isPlaying ? Theme.queuePlayingHighlightColor : Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                        font.weight: delegate.isPlaying ? Font.Bold : Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        objectName: "queueArtist" + delegate.rowIndex
                        Layout.fillWidth: true
                        text: delegate.artist
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                        visible: delegate.artist.length > 0
                    }
                }

                StyleButton {
                    id: removeBtn
                    objectName: "queueRemoveButton" + delegate.rowIndex
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    buttonWidth: 28
                    buttonHeight: 28
                    iconSize: 12
                    iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                    textColor: removeBtn.hovered ? Theme.dangerColor : Theme.textSecondary
                    baseColor: "transparent"
                    hoverColor: Theme.baseColor
                    opacity: delegate.hovered ? 1.0 : 0.4

                    Behavior on opacity {
                        NumberAnimation { duration: Theme.animationFast }
                    }

                    onClicked: root.removeRequested(delegate.rowIndex)
                }
            }
        }
    }

    Column {
        id: emptyState
        objectName: "queueEmptyState"
        anchors.centerIn: parent
        width: parent.width - Theme.spacing24 * 2
        spacing: Theme.spacing8
        visible: root.isEmpty

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 56
            height: 56
            radius: Theme.radiusFull
            color: Theme.baseColor

            Text {
                anchors.centerIn: parent
                text: "♫"
                color: Theme.textSecondary
                font.pixelSize: 24
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.emptyTitle
            color: Theme.textPrimary
            font.pixelSize: Theme.fontTitle
            font.bold: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            text: root.emptyHint
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}

