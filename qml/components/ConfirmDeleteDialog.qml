import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Seriona

// 删除确认弹窗（T16）：用户点"删除"才发出 confirmed()，调用方此时才调后端
// DeleteTrack/DeleteFolder 命令；取消/点遮罩/Esc 只关闭，不发出任何信号。
Popup {
    id: root

    modal: true
    anchors.centerIn: Overlay.overlay
    width: 420
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // 待删除目标（显示用）
    property string targetName: ""
    // 文件夹条目显示递归删除警示与文件数；0/未设置时不显示
    property bool isFolder: false
    property int trackCount: 0

    // 仅当用户确认删除时发出；调用方收到后应调用 appFacade.deleteTarget(path, isFolder)
    signal confirmed()

    background: Rectangle {
        color: Theme.mainColor
        radius: 12
        border.color: "#30FFFFFF"
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "transparent"

            Text {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                verticalAlignment: Text.AlignVCenter
                text: root.isFolder ? qsTr("删除文件夹") : qsTr("删除歌曲")
                color: Theme.textColor
                font.pixelSize: 17
                font.bold: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#30FFFFFF"
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 18
            Layout.bottomMargin: 18
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: qsTr("将直接从磁盘删除原文件，不可恢复")
                color: Theme.textColor
                font.pixelSize: 15
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                text: root.isFolder
                    ? qsTr("警告：文件夹“%1”及其全部内容将被永久删除%2。")
                          .arg(root.targetName)
                          .arg(root.trackCount > 0 ? qsTr("（包含 %1 首歌曲）").arg(root.trackCount) : "")
                    : qsTr("警告：歌曲“%1”将从磁盘永久删除。").arg(root.targetName)
                color: Theme.secondaryTextColor
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#30FFFFFF"
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 8

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
            }

            Rectangle {
                Layout.preferredWidth: 96
                Layout.preferredHeight: 36
                radius: 6
                color: mouseAreaCancel.pressed ? Theme.pressedColor : (mouseAreaCancel.containsMouse ? Theme.hoverColor : "transparent")

                MouseArea {
                    id: mouseAreaCancel
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.close()
                }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("取消")
                    color: Theme.textColor
                    font.pixelSize: 14
                }
            }

            Rectangle {
                Layout.preferredWidth: 96
                Layout.preferredHeight: 36
                radius: 6
                color: mouseAreaDelete.pressed ? "#B33A3A" : (mouseAreaDelete.containsMouse ? "#E05252" : Theme.accentColor)

                MouseArea {
                    id: mouseAreaDelete
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.confirmed();
                        root.close();
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("删除")
                    color: "white"
                    font.pixelSize: 14
                    font.bold: true
                }
            }
        }
    }
}
