import QtQuick
import QtQuick.Controls.Basic
import Seriona

Rectangle {
    id: root

    height: 44
    radius: Theme.radiusSmall
    color: Theme.baseColor

    required property int ruleIndex
    required property string fieldValue
    required property string orderValue
    required property var fieldOptions
    required property var orderOptions

    signal fieldChanged(string newField)
    signal orderChanged(string newOrder)
    signal removeRequested()

    readonly property var prefixTexts: [
        qsTr("先按"), 
        qsTr("再按"), 
        qsTr("然后"), 
        qsTr("接着"), 
        qsTr("最后")
    ]

    Row {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing12
        anchors.rightMargin: Theme.spacing12
        spacing: Theme.spacing8

        Text {
            text: root.ruleIndex < root.prefixTexts.length ? root.prefixTexts[root.ruleIndex] : `${root.ruleIndex + 1}.`
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
            anchors.verticalCenter: parent.verticalCenter
            width: 40
        }

        ComboBox {
            id: fieldCombo
            width: 140
            height: 32
            anchors.verticalCenter: parent.verticalCenter

            model: root.fieldOptions
            textRole: "label"
            valueRole: "value"

            currentIndex: {
                for (var i = 0; i < root.fieldOptions.length; i++) {
                    if (root.fieldOptions[i].value === root.fieldValue) {
                        return i;
                    }
                }
                return 0;
            }

            onActivated: function(index) {
                root.fieldChanged(root.fieldOptions[index].value);
            }

            background: Rectangle {
                color: fieldCombo.hovered ? Theme.hoverColor : Theme.baseColor
                radius: Theme.radiusSmall
                border.color: fieldCombo.visualFocus ? Theme.borderAccent : Theme.borderColor
                border.width: 1

                Behavior on color { ColorAnimation { duration: Theme.animationFast } }
                Behavior on border.color { ColorAnimation { duration: Theme.animationFast } }
            }

            contentItem: Text {
                text: fieldCombo.displayText
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                verticalAlignment: Text.AlignVCenter
                leftPadding: Theme.spacing8
                rightPadding: fieldCombo.indicator.width + Theme.spacing8
                elide: Text.ElideRight
            }

            indicator: Text {
                text: "▼"
                color: Theme.textSecondary
                font.pixelSize: 10
                anchors.right: parent.right
                anchors.rightMargin: Theme.spacing8
                anchors.verticalCenter: parent.verticalCenter
            }

            delegate: ItemDelegate {
                width: fieldCombo.width
                height: 32

                contentItem: Text {
                    text: modelData.label
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontBody
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: Theme.spacing8
                }

                background: Rectangle {
                    color: parent.hovered ? Theme.hoverColor : Theme.raisedSurfaceColor
                    Behavior on color { ColorAnimation { duration: Theme.animationFast } }
                }
            }

            popup: Popup {
                width: fieldCombo.width
                height: fullListHeight
                margins: 8
                padding: 4
                readonly property real fullListHeight: contentItem.implicitHeight + topPadding + bottomPadding
                readonly property real comboTopInWindow: fieldCombo.mapToItem(null, 0, 0).y
                readonly property real preferredY: fieldCombo.height
                readonly property real windowTopLimit: margins
                readonly property real windowBottomLimit: fieldCombo.Window.window ? fieldCombo.Window.window.height - margins : comboTopInWindow + preferredY + fullListHeight
                readonly property real minY: windowTopLimit - comboTopInWindow
                readonly property real maxY: windowBottomLimit - comboTopInWindow - fullListHeight
                y: Math.max(minY, Math.min(preferredY, maxY))

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: fieldCombo.popup.visible ? fieldCombo.delegateModel : null
                    currentIndex: fieldCombo.highlightedIndex
                }

                background: Rectangle {
                    color: Theme.raisedSurfaceColor
                    radius: Theme.radiusMedium
                    border.color: Theme.borderColor
                    border.width: 1
                }
            }
        }

        ComboBox {
            id: orderCombo
            width: 90
            height: 32
            anchors.verticalCenter: parent.verticalCenter

            model: root.orderOptions
            textRole: "label"
            valueRole: "value"

            currentIndex: root.orderValue === "asc" ? 0 : 1

            onActivated: function(index) {
                root.orderChanged(root.orderOptions[index].value);
            }

            background: Rectangle {
                color: orderCombo.hovered ? Theme.hoverColor : Theme.baseColor
                radius: Theme.radiusSmall
                border.color: orderCombo.visualFocus ? Theme.borderAccent : Theme.borderColor
                border.width: 1

                Behavior on color { ColorAnimation { duration: Theme.animationFast } }
                Behavior on border.color { ColorAnimation { duration: Theme.animationFast } }
            }

            contentItem: Text {
                text: orderCombo.displayText
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                verticalAlignment: Text.AlignVCenter
                leftPadding: Theme.spacing8
                rightPadding: orderCombo.indicator.width + Theme.spacing8
                elide: Text.ElideRight
            }

            indicator: Text {
                text: "▼"
                color: Theme.textSecondary
                font.pixelSize: 10
                anchors.right: parent.right
                anchors.rightMargin: Theme.spacing8
                anchors.verticalCenter: parent.verticalCenter
            }

            delegate: ItemDelegate {
                width: orderCombo.width
                height: 32

                contentItem: Text {
                    text: modelData.label
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontBody
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: Theme.spacing8
                }

                background: Rectangle {
                    color: parent.hovered ? Theme.hoverColor : Theme.raisedSurfaceColor
                    Behavior on color { ColorAnimation { duration: Theme.animationFast } }
                }
            }

            popup: Popup {
                y: orderCombo.height
                width: orderCombo.width
                implicitHeight: contentItem.implicitHeight
                padding: Theme.spacing4

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: orderCombo.popup.visible ? orderCombo.delegateModel : null
                    currentIndex: orderCombo.highlightedIndex
                }

                background: Rectangle {
                    color: Theme.raisedSurfaceColor
                    radius: Theme.radiusMedium
                    border.color: Theme.borderColor
                    border.width: 1
                }
            }
        }

        Item {
            width: parent.width - 40 - 140 - 90 - 32 - 8 * 4
            height: 1
            anchors.verticalCenter: parent.verticalCenter
        }

        StyleButton {
            iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
            buttonWidth: 24
            buttonHeight: 24
            iconSize: 12
            baseColor: "transparent"
            hoverColor: Theme.dangerHoverColor
            pressedColor: Theme.dangerPressedColor
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.removeRequested()
        }
    }
}

