import QtQuick
import Seriona

Item {
    id: root

    signal triggered()

    property string text: ""

    function activate() {
        root.triggered();
    }

    width: parent ? parent.width : 160
    height: 40

    Rectangle {
        anchors.fill: parent
        anchors.margins: Theme.spacing4
        radius: Theme.radiusSmall
        color: mouseArea.containsMouse ? Theme.hoverColor : "transparent"

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }
    }

    Text {
        text: root.text
        color: Theme.textPrimary
        font.pixelSize: Theme.fontBody
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        anchors.fill: parent
        anchors.leftMargin: 15
        anchors.rightMargin: 15
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activate()
    }
}

