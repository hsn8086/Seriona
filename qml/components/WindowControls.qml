import QtQuick
import Seriona

Row {
    id: root
    spacing: 8

    required property Window targetWindow

    StyleButton {
        id: minimizeBtn
        buttonWidth: 26
        buttonHeight: 26
        iconSize: 11
        baseColor: "transparent"
        iconSource: "qrc:/qt/qml/Seriona/qml/assets/minimize.svg"
        textColor: Theme.textColor
        onClicked: targetWindow.showMinimized()
    }

    StyleButton {
        id: maximizeBtn
        buttonWidth: 26
        buttonHeight: 26
        iconSize: 11
        baseColor: "transparent"
        iconSource: targetWindow.visibility === Window.Maximized ? "qrc:/qt/qml/Seriona/qml/assets/restore.svg" : "qrc:/qt/qml/Seriona/qml/assets/maximize.svg"
        textColor: Theme.textColor
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
        textColor: Theme.textColor
        hoverColor: "#60ff3b30"  // 悬浮时带有一点红色倾向，提示关闭危险
        pressedColor: "#80ff3b30"
        onClicked: targetWindow.close()
    }
}
