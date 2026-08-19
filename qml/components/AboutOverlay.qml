import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Seriona

// 关于 Seriona 界面（T19）：主窗内 overlay（不弹独立窗口），参照 Amberol
// About 对话框样式——半透明遮罩 + 居中卡片，含应用名、占位 logo（程序化
// 绘制，不引外部图片）、版本、简介与许可证说明。
//
// 关闭方式：Esc / 点击遮罩（closePolicy），与 ConfirmDeleteDialog 先例一致。
Popup {
    id: root

    modal: true
    anchors.centerIn: Overlay.overlay
    width: 420
    padding: 0
    // Esc / 点击遮罩关闭（与 ConfirmDeleteDialog 先例一致）
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    // 半透明遮罩：仅作用于本 overlay（不影响其他 modal Popup）
    Overlay.modal: Rectangle {
        color: "#99000000"
    }

    // 占位 logo：圆角渐变方块 + 音符字符（程序化绘制，无外部图片资源）
    background: Rectangle {
        color: Theme.mainColor
        radius: 12
        border.color: "#30FFFFFF"
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        // 顶栏：标题 + 关闭按钮
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "transparent"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("关于 Seriona")
                color: Theme.textColor
                font.pixelSize: 17
                font.bold: true
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 36
                height: 36
                radius: 6
                color: closeMouse.containsMouse ? Theme.hoverColor : "transparent"

                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.close()
                }

                Text {
                    anchors.centerIn: parent
                    text: "\u2715"
                    color: Theme.secondaryTextColor
                    font.pixelSize: 15
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#30FFFFFF"
        }

        // 主体：logo + 应用名 + 版本 + 简介 + 许可证
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 28
            Layout.bottomMargin: 28
            Layout.leftMargin: 32
            Layout.rightMargin: 32
            spacing: 8

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 84
                Layout.preferredHeight: 84
                radius: 22
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.accentColor }
                    GradientStop { position: 1.0; color: "#8c2f2f" }
                }

                Text {
                    anchors.centerIn: parent
                    text: "\u266A"
                    color: "white"
                    font.pixelSize: 42
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 10
                text: "Seriona"
                color: Theme.textColor
                font.pixelSize: 24
                font.bold: true
            }

            // 版本号与 CMakeLists.txt project(VERSION 0.1) 同步
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("版本 0.1.0")
                color: Theme.secondaryTextColor
                font.pixelSize: 13
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 12
                text: qsTr("轻量级桌面音乐播放器")
                color: Theme.secondaryTextColor
                font.pixelSize: 14
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.bottomMargin: 16
                Layout.preferredHeight: 1
                color: "#30FFFFFF"
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("开源 · 免费使用")
                color: Theme.textColor
                font.pixelSize: 13
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                text: qsTr("© 2026 Seriona")
                color: Theme.secondaryTextColor
                font.pixelSize: 12
            }
        }
    }
}
