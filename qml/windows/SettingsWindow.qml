import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Seriona

Window {
    id: root
    objectName: "settingsWindow"
    
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    
    width: 400
    height: 540
    
    required property AppFacade appFacade
    
    readonly property var settings: appFacade.settings
    
    Rectangle {
        id: contentRect
        anchors.fill: parent
        color: Theme.backgroundColor
        radius: Theme.borderRadius
        border.color: "#30FFFFFF"
        border.width: 1
        focus: true
        
        Keys.onEscapePressed: {
            root.close();
        }
        
        // Title Bar
        Rectangle {
            id: titleBar
            width: parent.width
            height: 48
            color: "transparent"
            
            Text {
                text: qsTr("设置")
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
        Flickable {
            id: flickable
            anchors.top: divider.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            clip: true
            contentWidth: width
            contentHeight: contentLayout.implicitHeight + 40
            
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
            
            ColumnLayout {
                id: contentLayout
                width: parent.width - 40
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 20
                spacing: 16
            
            // Row 1: Output Mode
            RowLayout {
                id: outputModeRow
                objectName: "outputModeGroup"
                Layout.fillWidth: true
                
                Text {
                    text: qsTr("输出模式")
                    color: Theme.textColor
                    font.pixelSize: 14
                    Layout.preferredWidth: 100
                }
                
                ButtonGroup {
                    id: modeGroup
                }
                
                Rectangle {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 32
                    color: "#15FFFFFF"
                    radius: 6
                    border.color: "#20FFFFFF"
                    border.width: 1
                    
                    Row {
                        anchors.fill: parent
                        spacing: 0
                        
                        Button {
                            id: directOutputBtn
                            width: parent.width / 2
                            height: parent.height
                            checkable: true
                            checked: settings.outputMode === 0
                            ButtonGroup.group: modeGroup
                            
                            background: Rectangle {
                                radius: 6
                                color: directOutputBtn.checked ? Theme.accentColor : "transparent"
                            }
                            
                            contentItem: Text {
                                text: qsTr("直接输出")
                                color: directOutputBtn.checked ? "white" : Theme.secondaryTextColor
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            onClicked: {
                                settings.outputMode = 0;
                            }
                        }
                        
                        Button {
                            id: mixedOutputBtn
                            width: parent.width / 2
                            height: parent.height
                            checkable: true
                            checked: settings.outputMode === 1
                            ButtonGroup.group: modeGroup
                            
                            background: Rectangle {
                                radius: 6
                                color: mixedOutputBtn.checked ? Theme.accentColor : "transparent"
                            }
                            
                            contentItem: Text {
                                text: qsTr("混合输出")
                                color: mixedOutputBtn.checked ? "white" : Theme.secondaryTextColor
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            onClicked: {
                                settings.outputMode = 1;
                            }
                        }
                    }
                }
            }
            
            // Output Parameters Group (only enabled in Mixed mode)
            ColumnLayout {
                id: outputParamsGroup
                objectName: "outputParamsGroup"
                Layout.fillWidth: true
                spacing: 16
                enabled: settings.outputMode === 1
                
                // Row 2: Sample Rate
                RowLayout {
                Layout.fillWidth: true
                
                Text {
                    text: qsTr("采样率")
                    color: Theme.textColor
                    font.pixelSize: 14
                    Layout.preferredWidth: 100
                }
                
                ComboBox {
                    id: sampleRateCombo
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 32
                    
                    model: [
                        { value: 0, label: qsTr("跟随设备") },
                        { value: 44100, label: "44100 Hz" },
                        { value: 48000, label: "48000 Hz" },
                        { value: 96000, label: "96000 Hz" },
                        { value: 192000, label: "192000 Hz" }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    
                    Binding {
                        target: sampleRateCombo
                        property: "currentIndex"
                        value: {
                            for (var i = 0; i < sampleRateCombo.model.length; i++) {
                                if (sampleRateCombo.model[i].value === settings.sampleRate) {
                                    return i;
                                }
                            }
                            return 0;
                        }
                        restoreMode: Binding.RestoreBindingOrValue
                    }
                    
                    onActivated: function(index) {
                        settings.sampleRate = model[index].value;
                    }
                    
                    background: Rectangle {
                        color: sampleRateCombo.hovered ? Theme.hoverColor : Theme.baseColor
                        radius: 6
                        border.color: "#30FFFFFF"
                        border.width: 1
                    }
                    
                    contentItem: Text {
                        text: sampleRateCombo.displayText
                        color: Theme.textColor
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                        rightPadding: sampleRateCombo.indicator.width + 8
                        elide: Text.ElideRight
                    }
                    
                    indicator: Text {
                        text: "▼"
                        color: Theme.secondaryTextColor
                        font.pixelSize: 10
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    delegate: ItemDelegate {
                        width: sampleRateCombo.width
                        height: 32
                        required property var modelData
                        required property int index
                        
                        contentItem: Text {
                            text: modelData.label
                            color: Theme.textColor
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }
                        
                        background: Rectangle {
                            color: parent.hovered ? Theme.hoverColor : Theme.mainColor
                        }
                    }
                    
                    popup: Popup {
                        width: sampleRateCombo.width
                        height: fullListHeight
                        margins: 8
                        padding: 4
                        readonly property real fullListHeight: contentItem.implicitHeight + topPadding + bottomPadding
                        readonly property real comboTopInWindow: sampleRateCombo.mapToItem(null, 0, 0).y
                        readonly property real preferredY: sampleRateCombo.height
                        readonly property real windowTopLimit: margins
                        readonly property real windowBottomLimit: sampleRateCombo.Window.window ? sampleRateCombo.Window.window.height - margins : comboTopInWindow + preferredY + fullListHeight
                        readonly property real minY: windowTopLimit - comboTopInWindow
                        readonly property real maxY: windowBottomLimit - comboTopInWindow - fullListHeight
                        y: Math.max(minY, Math.min(preferredY, maxY))
                        
                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: sampleRateCombo.popup.visible ? sampleRateCombo.delegateModel : null
                            currentIndex: sampleRateCombo.highlightedIndex
                        }
                        
                        background: Rectangle {
                            color: Theme.mainColor
                            radius: 6
                            border.color: "#30FFFFFF"
                            border.width: 1
                        }
                    }
                }
            }
            
            // Row 2.5: Bit Depth (sampleFormat)
            RowLayout {
                Layout.fillWidth: true
                
                Text {
                    text: qsTr("位深")
                    color: Theme.textColor
                    font.pixelSize: 14
                    Layout.preferredWidth: 100
                }
                
                ComboBox {
                    id: sampleFormatCombo
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 32
                    
                    model: [
                        { value: 0, label: qsTr("跟随设备") },
                        { value: 1, label: "16-bit" },
                        { value: 2, label: "24-bit" },
                        { value: 4, label: "32-bit float" }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    
                    Binding {
                        target: sampleFormatCombo
                        property: "currentIndex"
                        value: {
                            for (var i = 0; i < sampleFormatCombo.model.length; i++) {
                                if (sampleFormatCombo.model[i].value === settings.sampleFormat) {
                                    return i;
                                }
                            }
                            return 0;
                        }
                        restoreMode: Binding.RestoreBindingOrValue
                    }
                    
                    onActivated: function(index) {
                        settings.sampleFormat = model[index].value;
                    }
                    
                    background: Rectangle {
                        color: sampleFormatCombo.hovered ? Theme.hoverColor : Theme.baseColor
                        radius: 6
                        border.color: "#30FFFFFF"
                        border.width: 1
                    }
                    
                    contentItem: Text {
                        text: sampleFormatCombo.displayText
                        color: Theme.textColor
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                        rightPadding: sampleFormatCombo.indicator.width + 8
                        elide: Text.ElideRight
                    }
                    
                    indicator: Text {
                        text: "▼"
                        color: Theme.secondaryTextColor
                        font.pixelSize: 10
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    delegate: ItemDelegate {
                        width: sampleFormatCombo.width
                        height: 32
                        required property var modelData
                        required property int index
                        
                        contentItem: Text {
                            text: modelData.label
                            color: Theme.textColor
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }
                        
                        background: Rectangle {
                            color: parent.hovered ? Theme.hoverColor : Theme.mainColor
                        }
                    }
                    
                    popup: Popup {
                        width: sampleFormatCombo.width
                        height: fullListHeight
                        margins: 8
                        padding: 4
                        readonly property real fullListHeight: contentItem.implicitHeight + topPadding + bottomPadding
                        readonly property real comboTopInWindow: sampleFormatCombo.mapToItem(null, 0, 0).y
                        readonly property real preferredY: sampleFormatCombo.height
                        readonly property real windowTopLimit: margins
                        readonly property real windowBottomLimit: sampleFormatCombo.Window.window ? sampleFormatCombo.Window.window.height - margins : comboTopInWindow + preferredY + fullListHeight
                        readonly property real minY: windowTopLimit - comboTopInWindow
                        readonly property real maxY: windowBottomLimit - comboTopInWindow - fullListHeight
                        y: Math.max(minY, Math.min(preferredY, maxY))
                        
                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: sampleFormatCombo.popup.visible ? sampleFormatCombo.delegateModel : null
                            currentIndex: sampleFormatCombo.highlightedIndex
                        }
                        
                        background: Rectangle {
                            color: Theme.mainColor
                            radius: 6
                            border.color: "#30FFFFFF"
                            border.width: 1
                        }
                    }
                }
            }
            
            // Row 3: Buffer Duration
            RowLayout {
                Layout.fillWidth: true
                
                Text {
                    text: qsTr("缓冲时长")
                    color: Theme.textColor
                    font.pixelSize: 14
                    Layout.preferredWidth: 100
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    
                    Slider {
                        id: bufferSlider
                        Layout.fillWidth: true
                        from: 50
                        to: 1000
                        stepSize: 50
                        value: settings.bufferDurationMs
                        onMoved: {
                            settings.bufferDurationMs = value;
                        }
                        
                        background: Rectangle {
                            x: bufferSlider.leftPadding
                            y: bufferSlider.topPadding + bufferSlider.availableHeight / 2 - height / 2
                            implicitWidth: 200
                            implicitHeight: 4
                            width: bufferSlider.availableWidth
                            height: implicitHeight
                            radius: 2
                            color: "#20FFFFFF"
                            
                            Rectangle {
                                width: bufferSlider.visualPosition * parent.width
                                height: parent.height
                                color: Theme.accentColor
                                radius: 2
                            }
                        }
                        
                        handle: Rectangle {
                            x: bufferSlider.leftPadding + bufferSlider.visualPosition * (bufferSlider.availableWidth - width)
                            y: bufferSlider.topPadding + bufferSlider.availableHeight / 2 - height / 2
                            implicitWidth: 16
                            implicitHeight: 16
                            radius: 8
                            color: bufferSlider.pressed ? Qt.darker(Theme.accentColor, 1.2) : (bufferSlider.hovered ? Qt.lighter(Theme.accentColor, 1.2) : Theme.accentColor)
                        }
                    }
                    
                    Text {
                        text: `${settings.bufferDurationMs} ms`
                        color: Theme.textColor
                        font.pixelSize: 13
                        Layout.preferredWidth: 50
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
            
            // Row 4: Output Device
            RowLayout {
                Layout.fillWidth: true
                opacity: deviceCombo.enabled ? 1.0 : 0.5
                
                Text {
                    text: qsTr("输出设备")
                    color: Theme.textColor
                    font.pixelSize: 14
                    Layout.preferredWidth: 100
                }
                
                ComboBox {
                    id: deviceCombo
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 32
                    
                    model: settings.playbackDevices
                    enabled: settings.outputMode === 1 && settings.playbackDevices.length > 0
                    
                    displayText: enabled ? currentText : (settings.outputMode === 0 ? qsTr("直接输出模式已禁用设备选择") : qsTr("默认设备"))
                    
                    Binding {
                        target: deviceCombo
                        property: "currentIndex"
                        value: {
                            if (!deviceCombo.enabled) return -1;
                            var idx = settings.playbackDevices.indexOf(settings.preferredDeviceId);
                            return idx >= 0 ? idx : 0;
                        }
                        restoreMode: Binding.RestoreBindingOrValue
                    }
                    
                    onActivated: function(index) {
                        if (index >= 0 && index < settings.playbackDevices.length) {
                            settings.preferredDeviceId = settings.playbackDevices[index];
                        }
                    }
                    
                    background: Rectangle {
                        color: !deviceCombo.enabled ? "#10FFFFFF" : (deviceCombo.hovered ? Theme.hoverColor : Theme.baseColor)
                        radius: 6
                        border.color: "#30FFFFFF"
                        border.width: 1
                    }
                    
                    contentItem: Text {
                        text: deviceCombo.displayText
                        color: deviceCombo.enabled ? Theme.textColor : Theme.secondaryTextColor
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                        rightPadding: deviceCombo.indicator.width + 8
                        elide: Text.ElideRight
                    }
                    
                    indicator: Text {
                        text: "▼"
                        color: Theme.secondaryTextColor
                        font.pixelSize: 10
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        visible: deviceCombo.enabled
                    }
                    
                    delegate: ItemDelegate {
                        width: deviceCombo.width
                        height: 32
                        required property var modelData
                        required property int index
                        
                        contentItem: Text {
                            text: modelData
                            color: Theme.textColor
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }
                        
                        background: Rectangle {
                            color: parent.hovered ? Theme.hoverColor : Theme.mainColor
                        }
                    }
                    
                    popup: Popup {
                        width: deviceCombo.width
                        height: fullListHeight
                        margins: 8
                        padding: 4
                        readonly property real fullListHeight: contentItem.implicitHeight + topPadding + bottomPadding
                        readonly property real comboTopInWindow: deviceCombo.mapToItem(null, 0, 0).y
                        readonly property real preferredY: deviceCombo.height
                        readonly property real windowTopLimit: margins
                        readonly property real windowBottomLimit: deviceCombo.Window.window ? deviceCombo.Window.window.height - margins : comboTopInWindow + preferredY + fullListHeight
                        readonly property real minY: windowTopLimit - comboTopInWindow
                        readonly property real maxY: windowBottomLimit - comboTopInWindow - fullListHeight
                        y: Math.max(minY, Math.min(preferredY, maxY))
                        
                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: deviceCombo.popup.visible ? deviceCombo.delegateModel : null
                            currentIndex: deviceCombo.highlightedIndex
                        }
                        
                        background: Rectangle {
                            color: Theme.mainColor
                            radius: 6
                            border.color: "#30FFFFFF"
                            border.width: 1
                        }
                    }
                }
            }
            
            }
            
            // Row 6: Lyric Delimiters Group
            ColumnLayout {
                id: delimiterListGroup
                objectName: "delimiterList"
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: qsTr("歌词分隔符")
                    color: Theme.textColor
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    Layout.bottomMargin: 4
                }
                
                Repeater {
                    id: delimiterRepeater
                    model: settings.lyricDelimiters
                    
                    delegate: RowLayout {
                        required property int index
                        required property string modelData
                        
                        Layout.fillWidth: true
                        spacing: 8
                        
                        TextField {
                            id: delimiterInput
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            text: modelData
                            color: Theme.textColor
                            font.pixelSize: 13
                            
                            background: Rectangle {
                                color: Theme.baseColor
                                radius: 6
                                border.color: delimiterInput.activeFocus ? Theme.accentColor : "#30FFFFFF"
                                border.width: 1
                            }
                            
                            onEditingFinished: {
                                if (text === "") {
                                    var list = [];
                                    for (var i = 0; i < settings.lyricDelimiters.length; i++) {
                                        if (i !== index) {
                                            list.push(settings.lyricDelimiters[i]);
                                        }
                                    }
                                    settings.lyricDelimiters = list;
                                } else {
                                    var list = [];
                                    for (var i = 0; i < settings.lyricDelimiters.length; i++) {
                                        list.push(settings.lyricDelimiters[i]);
                                    }
                                    list[index] = text;
                                    settings.lyricDelimiters = list;
                                }
                            }
                        }
                        
                        StyleButton {
                            iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            iconSize: 12
                            textColor: Theme.accentColor
                            onClicked: {
                                var list = [];
                                for (var i = 0; i < settings.lyricDelimiters.length; i++) {
                                    if (i !== index) {
                                        list.push(settings.lyricDelimiters[i]);
                                    }
                                }
                                settings.lyricDelimiters = list;
                            }
                        }
                    }
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    TextField {
                        id: newDelimiterInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        placeholderText: qsTr("输入新分隔符...")
                        color: Theme.textColor
                        font.pixelSize: 13
                        
                        background: Rectangle {
                            id: newDelimiterBg
                            color: Theme.baseColor
                            radius: 6
                            border.color: "#30FFFFFF"
                            border.width: 1
                            
                            states: [
                                State {
                                    name: "error"
                                    PropertyChanges {
                                        newDelimiterBg.border.color: Theme.accentColor
                                    }
                                }
                            ]
                            
                            transitions: [
                                Transition {
                                    from: ""
                                    to: "error"
                                    ColorAnimation { duration: 150 }
                                }
                            ]
                        }
                        
                        Timer {
                            id: errorTimer
                            interval: 1000
                            onTriggered: {
                                newDelimiterBg.state = "";
                            }
                        }
                    }
                    
                    Button {
                        id: addBtn
                        Layout.preferredHeight: 32
                        Layout.preferredWidth: 60
                        
                        background: Rectangle {
                            color: addBtn.pressed ? Theme.pressedColor : (addBtn.hovered ? Theme.hoverColor : Theme.baseColor)
                            radius: 6
                            border.color: "#30FFFFFF"
                            border.width: 1
                        }
                        
                        contentItem: Text {
                            text: qsTr("添加")
                            color: Theme.accentColor
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        
                        onClicked: {
                            if (newDelimiterInput.text === "") {
                                newDelimiterBg.state = "error";
                                errorTimer.restart();
                                return;
                            }
                            
                            var list = [];
                            for (var i = 0; i < settings.lyricDelimiters.length; i++) {
                                list.push(settings.lyricDelimiters[i]);
                            }
                            list.push(newDelimiterInput.text);
                            settings.lyricDelimiters = list;
                            newDelimiterInput.text = "";
                        }
                    }
                }
            }
            
            }
        }
    }
    
    onVisibleChanged: {
        if (visible) {
            settings.enumerateDevices();
            root.requestActivate();
            contentRect.forceActiveFocus();
        }
    }
}
