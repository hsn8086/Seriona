import QtQuick
import Seriona

BubbleMenuItem {
    id: root

    property string title: text
    property Component page: null

    function activate() {
        if (page && parent && parent.menu)
            parent.menu.pushPage(title, page);
    }

    Text {
        text: qsTr("›")
        color: Theme.secondaryTextColor
        font.pixelSize: 18
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
    }
}
