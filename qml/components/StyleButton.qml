import QtQuick
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects
import Seriona

RoundButton {
    id: control

    // 使用 Theme 单例作为默认值
    property color baseColor: Theme.baseColor
    property color hoverColor: Theme.hoverColor
    property color pressedColor: Theme.pressedColor
    property color checkedColor: Theme.checkedColor

    // 接口属性（完整保留契约）
    property alias buttonWidth: control.width
    property alias buttonHeight: control.height
    property url iconSource: ""
    property int buttonRadius: Math.min(control.width, control.height) / 2
    property color textColor: Theme.textColor
    property real iconSize: Math.min(control.width, control.height) * 0.65

    checkable: false

    // 禁用所有内边距，防止 Control 基类通过 padding 影响 contentItem 位置
    padding: 0
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0

    background: Rectangle {
        id: bgRect
        radius: control.buttonRadius
        width: control.width
        height: control.height
        color: !control.enabled ? "transparent" : control.baseColor

        // 边框/聚焦环视觉增强
        border.width: (control.visualFocus && control.enabled) ? 1 : (control.checked ? 1 : 0)
        border.color: control.visualFocus ? Theme.borderAccent : (control.checked ? Theme.borderColor : "transparent")

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }

        Behavior on border.color {
            ColorAnimation { duration: Theme.animationFast }
        }

        states: [
            State {
                name: "disabledState"
                when: !control.enabled
                PropertyChanges {
                    target: bgRect
                    color: "transparent"
                }
            },
            State {
                name: "checkedState"
                when: control.enabled && control.checked
                PropertyChanges {
                    target: bgRect
                    color: control.checkedColor
                }
            },
            State {
                name: "pressed"
                when: control.enabled && control.pressed && !control.checked
                PropertyChanges { target: bgRect; color: control.pressedColor }
            },
            State {
                name: "hovered"
                when: control.enabled && control.hovered && !control.pressed && !control.checked
                PropertyChanges { target: bgRect; color: control.hoverColor }
            }
        ]
    }

    contentItem: Item {
        id: contentItem
        anchors.centerIn: parent
        width: control.iconSize
        height: control.iconSize
        opacity: control.enabled ? 1.0 : 0.45

        Behavior on opacity {
            NumberAnimation { duration: Theme.animationFast }
        }

        Image {
            id: iconImage
            anchors.centerIn: parent
            width: control.iconSize
            height: control.iconSize
            source: control.iconSource
            sourceSize.width: control.iconSize
            sourceSize.height: control.iconSize
            fillMode: Image.PreserveAspectFit
            visible: false
        }

        ColorOverlay {
            anchors.fill: iconImage
            source: iconImage
            color: !control.enabled ? Theme.textDisabled : control.textColor
            visible: control.iconSource.toString() !== ""

            Behavior on color {
                ColorAnimation { duration: Theme.animationFast }
            }
        }
    }
}

