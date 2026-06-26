import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Seriona

Item {
    id: root

    signal restorePlaylistRequested()
    signal addFolderRequested()

    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundColor
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.paddingLarge * 2, 320)
        spacing: Theme.paddingLarge

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 120
            Layout.preferredHeight: 120
            radius: 28
            color: Theme.mainColor
            border.color: Theme.hoverColor
            border.width: 1

            Rectangle {
                anchors.centerIn: parent
                width: 64
                height: 48
                radius: Theme.borderRadius
                color: "transparent"
                border.color: Theme.secondaryTextColor
                border.width: 2

                Rectangle {
                    x: 8
                    y: 9
                    width: 10
                    height: 10
                    radius: 5
                    color: Theme.secondaryTextColor
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 9
                    height: 18
                    color: Theme.secondaryTextColor
                    opacity: 0.7
                    rotation: -6
                    transformOrigin: Item.BottomLeft
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 24
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 9
                    height: 14
                    color: Theme.textColor
                    opacity: 0.45
                    rotation: 8
                    transformOrigin: Item.BottomLeft
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 14
                text: qsTr("图标占位")
                color: Theme.secondaryTextColor
                font.pixelSize: 12
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Seriona")
            color: Theme.textColor
            font.pixelSize: 30
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Button {
            id: restoreButton
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            text: qsTr("恢复播放列表")
            hoverEnabled: true
            onClicked: root.restorePlaylistRequested()

            contentItem: Text {
                text: restoreButton.text
                color: Theme.playButtonText
                font.pixelSize: 15
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: Theme.borderRadius
                color: restoreButton.pressed ? Qt.darker(Theme.playButtonBg, 1.2) : restoreButton.hovered ? Qt.darker(Theme.playButtonBg, 1.1) : Theme.playButtonBg
                Behavior on color { ColorAnimation { duration: Theme.animationDuration } }
            }
        }

        Button {
            id: addFolderButton
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            text: qsTr("添加文件夹")
            hoverEnabled: true
            onClicked: root.addFolderRequested()

            contentItem: Text {
                text: addFolderButton.text
                color: Theme.textColor
                font.pixelSize: 15
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: Theme.borderRadius
                color: addFolderButton.pressed ? Theme.pressedColor : addFolderButton.hovered ? Theme.hoverColor : Theme.baseColor
                border.color: Theme.hoverColor
                border.width: 1
                Behavior on color { ColorAnimation { duration: Theme.animationDuration } }
            }
        }
    }
}
