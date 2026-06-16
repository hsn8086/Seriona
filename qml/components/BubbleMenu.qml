import QtQuick
import QtQuick.Controls.Basic
import Seriona

Menu {
    id: control

    property string arrowDirection: "up"
    // The target button that opens the menu, used for aligning the arrow
    property Item targetItem: null

    // Compute the center of the target item mapped to the menu's coordinate space
    // We map the center point (width/2, 0) of the targetItem into the menu's coordinate system
    property point mappedCenter: targetItem ? mapFromItem(targetItem, targetItem.width / 2, 0) : Qt.point((width - 12) / 2, 0)
    
    // The exact arrow position: we want the center of the arrow to be exactly mappedCenter.x
    // We do NOT clamp it anymore. We want it perfectly aligned.
    property real arrowX: mappedCenter.x - 6

    // transformOrigin is available on Popup in Qt6
    transformOrigin: control.arrowDirection === "up" ? Popup.Top : Popup.Bottom

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.8; to: 1.0; duration: 200; easing.type: Easing.OutBack; easing.overshoot: 1.2 }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 150; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; from: 1.0; to: 0.8; duration: 150; easing.type: Easing.InCubic }
        }
    }

    background: Item {
        implicitWidth: 160
        implicitHeight: control.contentHeight

        // Arrow Border (Behind)
        Rectangle {
            width: 14
            height: 14
            color: "#15FFFFFF"
            rotation: 45
            x: control.arrowX - 1
            y: control.arrowDirection === "up" ? -7 : parent.height - 7
        }

        // Main Background
        Rectangle {
            anchors.fill: parent
            color: Theme.mainColor
            radius: 8
            border.color: "#15FFFFFF"
            border.width: 1
        }

        // Arrow Fill (In front of Main Background's border)
        Rectangle {
            width: 12
            height: 12
            color: Theme.mainColor
            rotation: 45
            x: control.arrowX
            y: control.arrowDirection === "up" ? -6 : parent.height - 6
        }
    }

    delegate: MenuItem {
        id: menuItem
        implicitWidth: 160
        implicitHeight: 40

        contentItem: Text {
            text: menuItem.text
            color: Theme.textColor
            font.pixelSize: 13
            verticalAlignment: Text.AlignVCenter
            leftPadding: 15
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: menuItem.highlighted ? Theme.hoverColor : "transparent"
            radius: 6
            anchors.fill: parent
            anchors.margins: 4
        }
    }
}
