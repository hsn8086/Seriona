import QtQuick
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects
import Seriona

Row {
    id: root
    spacing: 8

    required property Window targetWindow

    Button {
        id: minimizeBtn
        width: 12
        height: 12
        hoverEnabled: true

        background: Rectangle {
            radius: 6
            color: minimizeBtn.hovered ? Theme.minimizeHoverColor : Theme.minimizeColor
            border.color: Qt.darker(color, 1.2)
            border.width: 1

            Image {
                id: minIcon
                anchors.centerIn: parent
                width: 6
                height: 6
                source: "qrc:/qt/qml/Seriona/qml/assets/minimize.svg"
                sourceSize.width: 6
                sourceSize.height: 6
                fillMode: Image.PreserveAspectFit
                visible: false
            }

            ColorOverlay {
                anchors.fill: minIcon
                source: minIcon
                color: "black"
                opacity: minimizeBtn.hovered ? 0.8 : 0.0
                Behavior on opacity { NumberAnimation { duration: 100 } }
            }
        }

        onClicked: targetWindow.showMinimized()
    }

    Button {
        id: maximizeBtn
        width: 12
        height: 12
        hoverEnabled: true

        background: Rectangle {
            radius: 6
            color: maximizeBtn.hovered ? Theme.maximizeHoverColor : Theme.maximizeColor
            border.color: Qt.darker(color, 1.2)
            border.width: 1

            Image {
                id: maxIcon
                anchors.centerIn: parent
                width: 6
                height: 6
                source: targetWindow.visibility === Window.Maximized ? "qrc:/qt/qml/Seriona/qml/assets/restore.svg" : "qrc:/qt/qml/Seriona/qml/assets/maximize.svg"
                sourceSize.width: 6
                sourceSize.height: 6
                fillMode: Image.PreserveAspectFit
                visible: false
            }

            ColorOverlay {
                anchors.fill: maxIcon
                source: maxIcon
                color: "black"
                opacity: maximizeBtn.hovered ? 0.8 : 0.0
                Behavior on opacity { NumberAnimation { duration: 100 } }
            }
        }

        onClicked: {
            if (targetWindow.visibility === Window.Maximized) {
                targetWindow.showNormal()
            } else {
                targetWindow.showMaximized()
            }
        }
    }

    Button {
        id: closeBtn
        width: 12
        height: 12
        hoverEnabled: true

        background: Rectangle {
            radius: 6
            color: closeBtn.hovered ? Theme.closeHoverColor : Theme.closeColor
            border.color: Qt.darker(color, 1.2)
            border.width: 1

            Image {
                id: closeIcon
                anchors.centerIn: parent
                width: 6
                height: 6
                source: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                sourceSize.width: 6
                sourceSize.height: 6
                fillMode: Image.PreserveAspectFit
                visible: false
            }

            ColorOverlay {
                anchors.fill: closeIcon
                source: closeIcon
                color: "black"
                opacity: closeBtn.hovered ? 0.8 : 0.0
                Behavior on opacity { NumberAnimation { duration: 100 } }
            }
        }

        onClicked: targetWindow.close()
    }
}
