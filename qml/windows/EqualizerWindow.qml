import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Seriona

Window {
    id: root
    
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    
    width: 400
    height: 300
    
    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundColor
        radius: Theme.borderRadius
        border.color: "#30FFFFFF"
        border.width: 1
        
        // Title Bar
        Rectangle {
            id: titleBar
            width: parent.width
            height: 48
            color: "transparent"
            
            Text {
                text: qsTr("均衡器")
                color: Theme.textColor
                font.pixelSize: 16
                font.weight: Font.DemiBold
                anchors.centerIn: parent
            }
            
            StyleButton {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                buttonWidth: 24
                buttonHeight: 24
                iconSize: 14
                onClicked: {
                    root.close();
                }
            }
            
            MouseArea {
                anchors.fill: parent
                anchors.rightMargin: 40 // Don't overlap close button
                onPressed: {
                    root.startSystemMove();
                }
            }
        }
        
        Rectangle {
            id: divider
            width: parent.width - 24
            height: 1
            anchors.top: titleBar.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#20FFFFFF"
        }
        
        // Content Area
        Item {
            anchors.top: divider.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            
            Text {
                text: qsTr("均衡器（开发中）")
                color: Theme.secondaryTextColor
                font.pixelSize: 16
                anchors.centerIn: parent
            }
        }
    }
}
