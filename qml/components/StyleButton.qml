import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import Seriona

RoundButton {
    id: control

    // 使用 Theme 单例作为默认值
    property color baseColor: Theme.baseColor
    property color hoverColor: Theme.hoverColor
    property color pressedColor: Theme.pressedColor
    property color checkedColor: Theme.checkedColor

    // 接口属性
    property alias buttonWidth: control.width
    property alias buttonHeight: control.height
    property url iconSource: ""
    property int buttonRadius: Math.min(control.width, control.height) / 2
    property color textColor: Theme.textColor
    property real iconSize: Math.min(control.width, control.height) * 0.65

    checkable: false

    background: Rectangle {
        id: bgRect
        radius: control.buttonRadius
        width: control.width
        height: control.height
        color: control.baseColor

        Behavior on color {
            ColorAnimation { duration: Theme.animationDuration }
        }

        states: [
            State {
                name: "checkedState"
                when: control.checked
                PropertyChanges {
                    target: bgRect
                    color: control.checkedColor
                }
            },
            State {
                name: "hovered"
                when: control.hovered && !control.pressed && !control.checked
                PropertyChanges { target: bgRect; color: control.hoverColor }
            },
            State {
                name: "pressed"
                when: control.pressed && !control.checked
                PropertyChanges { target: bgRect; color: control.pressedColor }
            }
        ]
    }

    contentItem: Item {
        id: contentItem
        anchors.fill: parent

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
            color: control.textColor
            visible: control.iconSource.toString() !== ""
        }
    }
}
