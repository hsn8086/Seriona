import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Seriona

// 关于 Seriona 界面（T19）：主窗内 overlay（不弹独立窗口），参照 Amberol
// About 对话框样式——半透明遮罩 + 底部滑入卡片，含应用名、占位 logo（程序化
// 绘制，不引外部图片）、版本、简介与许可证说明，支持主页与法律信息页切换。
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
        color: Theme.overlayScrimColor
    }

    // 页面状态：0 = 主页, 1 = 法律与许可证信息
    property int currentPage: 0

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: Theme.animationStandard
                easing.type: Theme.easingDecelerate
            }
            NumberAnimation {
                property: "scale"
                from: 0.92
                to: 1.0
                duration: Theme.animationStandard
                easing.type: Theme.easingDecelerate
            }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: Theme.animationFast
                easing.type: Theme.easingAccelerate
            }
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.95
                duration: Theme.animationFast
                easing.type: Theme.easingAccelerate
            }
        }
    }

    background: Rectangle {
        color: Theme.raisedSurfaceColor
        radius: Theme.radiusLarge
        border.color: Theme.borderColor
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        // 顶栏：标题 + 页面切换/返回 + 关闭按钮
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing16
                anchors.rightMargin: Theme.spacing8
                spacing: Theme.spacing8

                // 返回按钮（仅在法律信息页显示）
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: Theme.radiusSmall
                    color: backMouse.containsMouse ? Theme.hoverColor : "transparent"
                    visible: root.currentPage !== 0

                    MouseArea {
                        id: backMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentPage = 0
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "←"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontTitle
                    }
                }

                Text {
                    Layout.fillWidth: true
                    verticalAlignment: Text.AlignVCenter
                    text: root.currentPage === 0 ? qsTr("关于 Seriona") : qsTr("法律与许可证")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                }

                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: Theme.radiusSmall
                    color: closeMouse.containsMouse ? Theme.hoverColor : "transparent"

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "\u2715"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontTitle
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderColor
        }

        // 主体内容区：StackLayout 切换主页与法律信息页
        StackLayout {
            Layout.fillWidth: true
            currentIndex: root.currentPage

            // Page 0: 主页
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacing24
                Layout.bottomMargin: Theme.spacing24
                Layout.leftMargin: Theme.spacing24
                Layout.rightMargin: Theme.spacing24
                spacing: Theme.spacing8

                // 程序化 Logo：渐变圆角方块 + 阴影 + 音符
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 88
                    radius: 24
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Theme.accentColor }
                        GradientStop { position: 1.0; color: Theme.gradientColor0 }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 76
                        height: 76
                        radius: 20
                        color: "transparent"
                        border.color: Theme.borderSubtle
                        border.width: 1
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "\u266A"
                        color: Theme.textOnAccent
                        font.pixelSize: 44
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacing8
                    text: "Seriona"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontHeading
                    font.bold: true
                }

                // 版本号与 CMakeLists.txt project(VERSION 0.1) 同步
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("版本 0.1.0")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacing8
                    text: qsTr("轻量级桌面音乐播放器")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontTitle
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.spacing12
                    Layout.bottomMargin: Theme.spacing12
                    Layout.preferredHeight: 1
                    color: Theme.borderColor
                }

                // 法律信息入口按钮
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 140
                    Layout.preferredHeight: 32
                    radius: Theme.radiusSmall
                    color: legalMouse.pressed ? Theme.pressedColor : (legalMouse.containsMouse ? Theme.hoverColor : Theme.baseColor)
                    border.color: Theme.borderSubtle
                    border.width: 1

                    Behavior on color {
                        ColorAnimation { duration: Theme.animationFast }
                    }

                    MouseArea {
                        id: legalMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentPage = 1
                    }

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("法律与许可证")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacing4
                    text: qsTr("© 2026 Seriona")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
            }

            // Page 1: 法律与许可证信息
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacing16
                Layout.bottomMargin: Theme.spacing24
                Layout.leftMargin: Theme.spacing24
                Layout.rightMargin: Theme.spacing24
                spacing: Theme.spacing12

                Text {
                    Layout.fillWidth: true
                    text: qsTr("开源许可证")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    radius: Theme.radiusSmall
                    color: Theme.surfaceColor
                    border.color: Theme.borderSubtle
                    border.width: 1

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: Theme.spacing8
                        contentWidth: width
                        contentHeight: licenseText.implicitHeight
                        clip: true

                        Text {
                            id: licenseText
                            width: parent.width
                            text: qsTr("Seriona 是一款开源音乐播放器。\n\n本软件基于 MIT 许可证分发。\n您可以自由使用、修改和分发本软件。\n\n感谢所有开源社区贡献者与依赖库支持。")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.Wrap
                        }
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("开源 · 免费使用")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
            }
        }
    }
}

