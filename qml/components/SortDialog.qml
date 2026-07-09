import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Seriona

Window {
    id: root
    
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.ApplicationModal
    color: "transparent"
    
    width: 480
    height: rulesAreaHeight + dialogChromeHeight
    
    property var sortRules: []  // [{field: "album", order: "asc"}, ...]
    
    signal accepted()
    signal rejected()

    readonly property int maxSortRules: 5
    readonly property int ruleRowHeight: 44
    readonly property int ruleSpacing: 12
    readonly property int dialogChromeHeight: 148
    readonly property int rulesAreaHeight: maxSortRules * ruleRowHeight + (maxSortRules - 1) * ruleSpacing
    readonly property int addRuleButtonY: (maxSortRules - 1) * (ruleRowHeight + ruleSpacing)
    
    readonly property var fieldOptions: [
        {value: "title", label: qsTr("歌曲名")},
        {value: "artist", label: qsTr("歌手名")},
        {value: "album", label: qsTr("专辑名")},
        {value: "filename", label: qsTr("文件名")},
        {value: "year", label: qsTr("歌曲年份")},
        {value: "duration", label: qsTr("歌曲长度")},
        {value: "createdDate", label: qsTr("文件创建时间")},
        {value: "discNumber", label: qsTr("碟片号")},
        {value: "trackNumber", label: qsTr("音轨号")}
    ]
    
    readonly property var orderOptions: [
        {value: "asc", label: qsTr("正序"), icon: "↑"},
        {value: "desc", label: qsTr("倒序"), icon: "↓"}
    ]
    
    Rectangle {
        anchors.fill: parent
        anchors.margins: 8
        color: Theme.mainColor
        radius: 12
        border.color: "#30FFFFFF"
        border.width: 1
        
        Rectangle {
            id: titleBar
            width: parent.width
            height: 48
            color: "transparent"
            
            Text {
                text: qsTr("排序规则")
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
                    root.rejected();
                    root.close();
                }
            }
        }
        
        Rectangle {
            width: parent.width - 24
            height: 1
            anchors.top: titleBar.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#20FFFFFF"
        }
        
        Item {
            id: rulesArea
            anchors.top: titleBar.bottom
            anchors.topMargin: 16
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            height: root.rulesAreaHeight
            
            Repeater {
                model: root.sortRules.length
                
                delegate: SortRuleRow {
                    required property int index
                    
                    y: index * (root.ruleRowHeight + root.ruleSpacing)
                    width: parent.width
                    height: root.ruleRowHeight
                    ruleIndex: index
                    fieldValue: root.sortRules[index].field
                    orderValue: root.sortRules[index].order
                    fieldOptions: root.fieldOptions
                    orderOptions: root.orderOptions
                    
                    onFieldChanged: function(newField) {
                        var rules = root.sortRules.slice();
                        rules[ruleIndex].field = newField;
                        root.sortRules = rules;
                    }
                    
                    onOrderChanged: function(newOrder) {
                        var rules = root.sortRules.slice();
                        rules[ruleIndex].order = newOrder;
                        root.sortRules = rules;
                    }
                    
                    onRemoveRequested: {
                        var rules = root.sortRules.slice();
                        rules.splice(ruleIndex, 1);
                        root.sortRules = rules;
                    }
                }
            }
            
            Rectangle {
                y: root.addRuleButtonY
                width: parent.width
                height: root.ruleRowHeight
                radius: 6
                color: addMouseArea.containsMouse ? Theme.hoverColor : "transparent"
                border.color: "#30FFFFFF"
                border.width: 1
                visible: root.sortRules.length < root.maxSortRules
                
                Text {
                    text: qsTr("+ 添加排序规则")
                    color: Theme.accentColor
                    font.pixelSize: 14
                    anchors.centerIn: parent
                }
                
                MouseArea {
                    id: addMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        var rules = root.sortRules.slice();
                        rules.push({field: "title", order: "asc"});
                        root.sortRules = rules;
                    }
                }
            }
        }
        
        Row {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 16
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            
            Rectangle {
                width: 100
                height: 36
                radius: 18
                color: cancelMouseArea.containsMouse ? Theme.hoverColor : "#20FFFFFF"
                
                Text {
                    text: qsTr("取消")
                    color: Theme.textColor
                    font.pixelSize: 14
                    anchors.centerIn: parent
                }
                
                MouseArea {
                    id: cancelMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.rejected();
                        root.close();
                    }
                }
            }
            
            Rectangle {
                width: 100
                height: 36
                radius: 18
                color: applyMouseArea.containsMouse ? Qt.lighter(Theme.accentColor, 1.2) : Theme.accentColor
                
                Text {
                    text: qsTr("应用")
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    anchors.centerIn: parent
                }
                
                MouseArea {
                    id: applyMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.accepted();
                        root.close();
                    }
                }
            }
        }
    }
}
