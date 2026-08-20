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

    Overlay.modal: Rectangle {
        color: Theme.overlayScrimColor
    }

    // 待删除目标（显示用）
    property string targetName: ""
    // 文件夹条目显示递归删除警示与文件数；0/未设置时不显示
    property bool isFolder: false
    property int trackCount: 0

    // 仅当用户确认删除时发出；调用方收到后应调用 appFacade.deleteTarget(path, isFolder)
    signal confirmed()

    background: Rectangle {
        color: Theme.raisedSurfaceColor
        radius: Theme.radiusLarge
        border.color: Theme.borderColor
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        // 顶部警示标题区
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing16
                anchors.rightMargin: Theme.spacing16
                spacing: Theme.spacing8

                Rectangle {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    radius: Theme.radiusFull
                    color: Theme.dangerColor
                    opacity: 0.15

                    Text {
                        anchors.centerIn: parent
                        text: "!"
                        color: Theme.dangerColor
                        font.pixelSize: Theme.fontTitle
                        font.bold: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    verticalAlignment: Text.AlignVCenter
                    text: root.isFolder ? qsTr("删除文件夹") : qsTr("删除歌曲")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderColor
        }

        // 警示内容区
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacing16
            Layout.rightMargin: Theme.spacing16
            Layout.topMargin: Theme.spacing16
            Layout.bottomMargin: Theme.spacing16
            spacing: Theme.spacing12

            Text {
                Layout.fillWidth: true
                text: qsTr("将直接从磁盘删除原文件，不可恢复")
                color: Theme.dangerColor
                font.pixelSize: Theme.fontTitle
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                text: root.isFolder
                    ? qsTr("警告：文件夹“%1”及其全部内容将被永久删除%2。")
                          .arg(root.targetName)
                          .arg(root.trackCount > 0 ? qsTr("（包含 %1 首歌曲）").arg(root.trackCount) : "")
                    : qsTr("警告：歌曲“%1”将从磁盘永久删除。").arg(root.targetName)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderColor
        }

        // 底部操作按钮区
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.leftMargin: Theme.spacing12
            Layout.rightMargin: Theme.spacing12
            spacing: Theme.spacing8

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
            }

            Rectangle {
                Layout.preferredWidth: 96
                Layout.preferredHeight: 36
                radius: Theme.radiusSmall
                color: mouseAreaCancel.pressed ? Theme.pressedColor : (mouseAreaCancel.containsMouse ? Theme.hoverColor : "transparent")
                border.color: Theme.borderSubtle
                border.width: 1

                Behavior on color {
                    ColorAnimation { duration: Theme.animationFast }
                }

                MouseArea {
                    id: mouseAreaCancel
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("取消")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontBody
                }
            }

            Rectangle {
                Layout.preferredWidth: 96
                Layout.preferredHeight: 36
                radius: Theme.radiusSmall
                color: mouseAreaDelete.pressed ? Theme.dangerPressedColor : (mouseAreaDelete.containsMouse ? Theme.dangerHoverColor : Theme.dangerColor)

                Behavior on color {
                    ColorAnimation { duration: Theme.animationFast }
                }

                MouseArea {
                    id: mouseAreaDelete
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.confirmed();
                        root.close();
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("删除")
                    color: Theme.textOnAccent
                    font.pixelSize: Theme.fontBody
                    font.bold: true
                }
            }
        }
    }
}

