import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects
import Seriona

Item {
    id: root
    width: Theme.sidebarWidth

    // Properties to control shadow visibility
    property bool isDockCapable: false
    property bool isSidebarOpen: false

    RectangularGlow {
        id: shadow
        anchors.fill: contentRect
        glowRadius: 10
        spread: 0.2
        color: "#80000000"
        visible: !root.isDockCapable && root.isSidebarOpen
        z: -1
    }

    Rectangle {
        id: contentRect
        anchors.fill: parent
        color: Theme.sidebarBackgroundColor

        // Right border line
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: "#15FFFFFF"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingLarge
            spacing: 15

            Text {
                text: qsTr("Playlist")
                font.pixelSize: 20
                font.bold: true
                color: Theme.textColor
            }

            ListView {
                id: playlistView
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: 5
                delegate: ItemDelegate {
                    width: playlistView.width
                    text: qsTr("Track %1").arg(index + 1)
                    onClicked: console.log("Clicked track", index + 1)
                }
            }
        }
    }
}
