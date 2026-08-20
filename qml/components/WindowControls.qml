import QtQuick
import Seriona

Row {
    id: root
    spacing: Theme.spacing8

    required property Window targetWindow
    signal closeRequested()

    StyleButton {
        id: minimizeBtn
        buttonWidth: 26
        buttonHeight: 26
        iconSize: 11
        baseColor: "transparent"
        hoverColor: Theme.minimizeHoverColor
        pressedColor: Theme.warningColor
        iconSource: "qrc:/qt/qml/Seriona/qml/assets/minimize.svg"
        textColor: Theme.textPrimary
        onClicked: targetWindow.showMinimized()
    }

    StyleButton {
        id: maximizeBtn
        buttonWidth: 26
        buttonHeight: 26
        iconSize: 11
        baseColor: "transparent"
        hoverColor: Theme.maximizeHoverColor
        pressedColor: Theme.successColor
        iconSource: targetWindow.visibility === Window.Maximized ? "qrc:/qt/qml/Seriona/qml/assets/restore.svg" : "qrc:/qt/qml/Seriona/qml/assets/maximize.svg"
        textColor: Theme.textPrimary
        onClicked: {
            if (targetWindow.visibility === Window.Maximized) {
                targetWindow.showNormal()
            } else {
                targetWindow.showMaximized()
            }
        }
    }

    StyleButton {
        id: closeBtn
        buttonWidth: 26
        buttonHeight: 26
        iconSize: 11
        baseColor: "transparent"
        iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
        textColor: Theme.textPrimary
        hoverColor: Theme.closeHoverColor
        pressedColor: Theme.dangerPressedColor
        onClicked: root.closeRequested()
    }
}

