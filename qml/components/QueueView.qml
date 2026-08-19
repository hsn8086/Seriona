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
            topPadding: 4
            bottomPadding: 4
            leftPadding: 15
            rightPadding: 15
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
                color: delegate.hovered ? Theme.hoverColor : "transparent"

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
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: 10
                    color: Theme.mainColor
                    antialiasing: true

                    Text {
                        anchors.centerIn: parent
                        text: "♫"
                        color: "white"
                        font.pixelSize: 18
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 2

                    Text {
                        objectName: "queueTitle" + delegate.rowIndex
                        Layout.fillWidth: true
                        text: delegate.title
                        color: delegate.isPlaying ? Theme.accentColor : Theme.textColor
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        objectName: "queueArtist" + delegate.rowIndex
                        Layout.fillWidth: true
                        text: delegate.artist
                        color: Theme.secondaryTextColor
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        visible: delegate.artist.length > 0
                    }
                }

                StyleButton {
                    objectName: "queueRemoveButton" + delegate.rowIndex
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    buttonWidth: 24
                    buttonHeight: 24
                    iconSize: 12
                    iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                    textColor: Theme.secondaryTextColor
                    baseColor: "transparent"
                    onClicked: root.removeRequested(delegate.rowIndex)
                }
            }
        }
    }

    Column {
        id: emptyState
        objectName: "queueEmptyState"
        anchors.centerIn: parent
        width: parent.width - 40
        spacing: 6
        visible: root.isEmpty

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.emptyTitle
            color: Theme.textColor
            font.pixelSize: 14
            font.bold: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            text: root.emptyHint
            color: Theme.secondaryTextColor
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
