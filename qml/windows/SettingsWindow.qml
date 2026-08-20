import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Seriona

Window {
    id: root
    objectName: "settingsWindow"
    
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    
    width: 440
    height: 580
    
    required property AppFacade appFacade
    
    readonly property var settings: appFacade.settings
    
    Rectangle {
        id: contentRect
        anchors.fill: parent
        color: Theme.surfaceColor
        radius: Theme.radiusLarge
        border.color: Theme.borderColor
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
                color: Theme.textPrimary
                font.pixelSize: Theme.fontTitle
                font.weight: Font.DemiBold
                anchors.centerIn: parent
            }
            
            StyleButton {
                anchors.right: parent.right
                anchors.rightMargin: Theme.spacing12
                anchors.verticalCenter: parent.verticalCenter
                iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                buttonWidth: 28
                buttonHeight: 28
                iconSize: 12
                textColor: Theme.textSecondary
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
            width: parent.width
            height: 1
            anchors.top: titleBar.bottom
            color: Theme.borderColor
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
            contentHeight: contentLayout.implicitHeight + Theme.spacing24 * 2
            
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                width: Theme.scrollbarWidth
                background: Rectangle {
                    color: "transparent"
                }
                contentItem: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.hovered ? Theme.scrollbarHoverColor : Theme.scrollbarColor
                }
            }
            
            ColumnLayout {
                id: contentLayout
                width: parent.width - Theme.spacing24 * 2
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: Theme.spacing16
                spacing: Theme.spacing16

                // ==========================================
                // Card 1: 音频输出配置 (Audio Output Card)
                // ==========================================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: audioCardLayout.implicitHeight + Theme.spacing16 * 2
                    color: Theme.raisedSurfaceColor
                    radius: Theme.radiusMedium
                    border.color: Theme.borderSubtle
                    border.width: 1

                    ColumnLayout {
                        id: audioCardLayout
                        anchors.fill: parent
                        anchors.margins: Theme.spacing16
                        spacing: Theme.spacing16

                        Text {
                            text: qsTr("音频输出")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontTitle
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }

                        // Row 1: Output Mode
                        RowLayout {
                            id: outputModeRow
                            objectName: "outputModeGroup"
                            Layout.fillWidth: true
                            
                            Text {
                                text: qsTr("输出模式")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                                Layout.preferredWidth: 100
                            }
                            
                            ButtonGroup {
                                id: modeGroup
                            }
                            
                            Rectangle {
                                Layout.preferredWidth: 200
                                Layout.preferredHeight: 32
                                color: Theme.surfaceColor
                                radius: Theme.radiusSmall
                                border.color: Theme.borderSubtle
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
                                            radius: Theme.radiusSmall
                                            color: directOutputBtn.checked ? Theme.accentColor : "transparent"
                                            
                                            Behavior on color {
                                                ColorAnimation { duration: Theme.animationFast }
                                            }
                                        }
                                        
                                        contentItem: Text {
                                            text: qsTr("直接输出")
                                            color: directOutputBtn.checked ? Theme.textOnAccent : Theme.textSecondary
                                            font.pixelSize: Theme.fontBody
                                            font.weight: directOutputBtn.checked ? Font.DemiBold : Font.Normal
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
                                            radius: Theme.radiusSmall
                                            color: mixedOutputBtn.checked ? Theme.accentColor : "transparent"
                                            
                                            Behavior on color {
                                                ColorAnimation { duration: Theme.animationFast }
                                            }
                                        }
                                        
                                        contentItem: Text {
                                            text: qsTr("混合输出")
                                            color: mixedOutputBtn.checked ? Theme.textOnAccent : Theme.textSecondary
                                            font.pixelSize: Theme.fontBody
                                            font.weight: mixedOutputBtn.checked ? Font.DemiBold : Font.Normal
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
                        
                        // Output Parameters Group（灰化按行控制：仅采样率/位深两行在直接输出模式下禁用）
                        ColumnLayout {
                            id: outputParamsGroup
                            objectName: "outputParamsGroup"
                            Layout.fillWidth: true
                            spacing: Theme.spacing16
                            
                            // Row 2: Sample Rate（直接输出模式下灰化）
                            RowLayout {
                                Layout.fillWidth: true
                                enabled: !settings.sampleParamsGreyed
                                opacity: settings.sampleParamsGreyed ? 0.45 : 1.0
                                
                                Behavior on opacity {
                                    NumberAnimation { duration: Theme.animationFast }
                                }
                                
                                Text {
                                    text: qsTr("采样率")
                                    color: settings.sampleParamsGreyed ? Theme.textDisabled : Theme.textPrimary
                                    font.pixelSize: Theme.fontBody
                                    Layout.preferredWidth: 100
                                }
                                
                                ComboBox {
                                    id: sampleRateCombo
                                    Layout.preferredWidth: 200
                                    Layout.preferredHeight: 32
                                    
                                    model: settings.sampleRateOptions
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
                                        radius: Theme.radiusSmall
                                        border.color: Theme.borderColor
                                        border.width: 1
                                    }
                                    
                                    contentItem: Text {
                                        text: sampleRateCombo.displayText
                                        color: settings.sampleParamsGreyed ? Theme.textDisabled : Theme.textPrimary
                                        font.pixelSize: Theme.fontBody
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: Theme.spacing8
                                        rightPadding: sampleRateCombo.indicator.width + Theme.spacing8
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
                                        width: sampleRateCombo.width
                                        height: 32
                                        required property var modelData
                                        required property int index
                                        
                                        contentItem: Text {
                                            text: modelData.label
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontBody
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: Theme.spacing8
                                        }
                                        
                                        background: Rectangle {
                                            color: parent.hovered ? Theme.hoverColor : Theme.raisedSurfaceColor
                                        }
                                    }
                                    
                                    popup: Popup {
                                        width: sampleRateCombo.width
                                        height: fullListHeight
                                        margins: Theme.spacing8
                                        padding: Theme.spacing4
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
                                            color: Theme.raisedSurfaceColor
                                            radius: Theme.radiusSmall
                                            border.color: Theme.borderColor
                                            border.width: 1
                                        }
                                    }
                                }
                            }
                            
                            // Row 2.5: Bit Depth (sampleFormat)（直接输出模式下灰化）
                            RowLayout {
                                Layout.fillWidth: true
                                enabled: !settings.sampleParamsGreyed
                                opacity: settings.sampleParamsGreyed ? 0.45 : 1.0
                                
                                Behavior on opacity {
                                    NumberAnimation { duration: Theme.animationFast }
                                }
                                
                                Text {
                                    text: qsTr("位深")
                                    color: settings.sampleParamsGreyed ? Theme.textDisabled : Theme.textPrimary
                                    font.pixelSize: Theme.fontBody
                                    Layout.preferredWidth: 100
                                }
                                
                                ComboBox {
                                    id: sampleFormatCombo
                                    Layout.preferredWidth: 200
                                    Layout.preferredHeight: 32
                                    
                                    model: settings.sampleFormatOptions
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
                                        radius: Theme.radiusSmall
                                        border.color: Theme.borderColor
                                        border.width: 1
                                    }
                                    
                                    contentItem: Text {
                                        text: sampleFormatCombo.displayText
                                        color: settings.sampleParamsGreyed ? Theme.textDisabled : Theme.textPrimary
                                        font.pixelSize: Theme.fontBody
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: Theme.spacing8
                                        rightPadding: sampleFormatCombo.indicator.width + Theme.spacing8
                                        elide: Text.ElideRight
                                    }
                                    
                                    indicator: Text {
                                        text: "▼"
                                        color: Theme.secondaryTextColor
                                        font.pixelSize: 10
                                        anchors.right: parent.right
                                        anchors.rightMargin: Theme.spacing8
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    
                                    delegate: ItemDelegate {
                                        width: sampleFormatCombo.width
                                        height: 32
                                        required property var modelData
                                        required property int index
                                        
                                        contentItem: Text {
                                            text: modelData.label
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontBody
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: Theme.spacing8
                                        }
                                        
                                        background: Rectangle {
                                            color: parent.hovered ? Theme.hoverColor : Theme.raisedSurfaceColor
                                        }
                                    }
                                    
                                    popup: Popup {
                                        width: sampleFormatCombo.width
                                        height: fullListHeight
                                        margins: Theme.spacing8
                                        padding: Theme.spacing4
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
                                            color: Theme.raisedSurfaceColor
                                            radius: Theme.radiusSmall
                                            border.color: Theme.borderColor
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
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontBody
                                    Layout.preferredWidth: 100
                                }
                                
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacing12
                                    
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
                                            color: Theme.progressBarTrackColor
                                            
                                            Rectangle {
                                                width: bufferSlider.visualPosition * parent.width
                                                height: parent.height
                                                color: Theme.progressBarColor
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
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontBody
                                        Layout.preferredWidth: 50
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }
                    }
                }

                // ==========================================
                // Card 2: 设备与调试配置 (Device & Logging Card)
                // ==========================================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: deviceCardLayout.implicitHeight + Theme.spacing16 * 2
                    color: Theme.raisedSurfaceColor
                    radius: Theme.radiusMedium
                    border.color: Theme.borderSubtle
                    border.width: 1

                    ColumnLayout {
                        id: deviceCardLayout
                        anchors.fill: parent
                        anchors.margins: Theme.spacing16
                        spacing: Theme.spacing16

                        Text {
                            text: qsTr("设备与系统")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontTitle
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }

                        // Row 4: Output Device
                        RowLayout {
                            Layout.fillWidth: true
                            opacity: deviceCombo.enabled ? 1.0 : 0.5
                            
                            Text {
                                text: qsTr("输出设备")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                                Layout.preferredWidth: 100
                            }
                            
                            ComboBox {
                                id: deviceCombo
                                Layout.fillWidth: true
                                Layout.minimumWidth: 120
                                Layout.preferredHeight: 32
                                
                                model: settings.playbackDeviceNames
                                enabled: settings.playbackDevices.length > 0
                                
                                displayText: enabled ? currentText : qsTr("无可用输出设备")
                                
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
                                    color: !deviceCombo.enabled ? Theme.borderSubtle : (deviceCombo.hovered ? Theme.hoverColor : Theme.baseColor)
                                    radius: Theme.radiusSmall
                                    border.color: Theme.borderColor
                                    border.width: 1
                                }
                                
                                contentItem: Text {
                                    text: deviceCombo.displayText
                                    color: deviceCombo.enabled ? Theme.textPrimary : Theme.textDisabled
                                    font.pixelSize: Theme.fontBody
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: Theme.spacing8
                                    rightPadding: deviceCombo.indicator.width + Theme.spacing8
                                    elide: Text.ElideRight
                                }
                                
                                indicator: Text {
                                    text: "▼"
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 10
                                    anchors.right: parent.right
                                    anchors.rightMargin: Theme.spacing8
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
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontBody
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: Theme.spacing8
                                        rightPadding: Theme.spacing8
                                        elide: Text.ElideRight
                                    }
                                    
                                    background: Rectangle {
                                        color: parent.hovered ? Theme.hoverColor : Theme.raisedSurfaceColor
                                    }
                                }
                                
                                popup: Popup {
                                    width: deviceCombo.width
                                    height: fullListHeight
                                    margins: Theme.spacing8
                                    padding: Theme.spacing4
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
                                        color: Theme.raisedSurfaceColor
                                        radius: Theme.radiusSmall
                                        border.color: Theme.borderColor
                                        border.width: 1
                                    }
                                }
                            }
                        }
                        
                        // Row 5: Log Level
                        RowLayout {
                            Layout.fillWidth: true
                            
                            Text {
                                text: qsTr("日志等级")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                                Layout.preferredWidth: 100
                            }
                            
                            ComboBox {
                                id: logLevelCombo
                                Layout.preferredWidth: 200
                                Layout.preferredHeight: 32
                                
                                model: [
                                    { value: 0, label: qsTr("Trace") },
                                    { value: 1, label: qsTr("Debug") },
                                    { value: 2, label: qsTr("Info") },
                                    { value: 3, label: qsTr("Warn") },
                                    { value: 4, label: qsTr("Error") },
                                    { value: 5, label: qsTr("Critical") }
                                ]
                                textRole: "label"
                                valueRole: "value"
                                
                                Binding {
                                    target: logLevelCombo
                                    property: "currentIndex"
                                    value: {
                                        for (var i = 0; i < logLevelCombo.model.length; i++) {
                                            if (logLevelCombo.model[i].value === settings.logLevel) {
                                                return i;
                                            }
                                        }
                                        return 0;
                                    }
                                    restoreMode: Binding.RestoreBindingOrValue
                                }
                                
                                onActivated: function(index) {
                                    settings.logLevel = model[index].value;
                                }
                                
                                background: Rectangle {
                                    color: logLevelCombo.hovered ? Theme.hoverColor : Theme.baseColor
                                    radius: Theme.radiusSmall
                                    border.color: Theme.borderColor
                                    border.width: 1
                                }
                                
                                contentItem: Text {
                                    text: logLevelCombo.displayText
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontBody
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: Theme.spacing8
                                    rightPadding: logLevelCombo.indicator.width + Theme.spacing8
                                    elide: Text.ElideRight
                                }
                                
                                indicator: Text {
                                    text: "▼"
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 10
                                    anchors.right: parent.right
                                    anchors.rightMargin: Theme.spacing8
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                
                                delegate: ItemDelegate {
                                    width: logLevelCombo.width
                                    height: 32
                                    required property var modelData
                                    required property int index
                                    
                                    contentItem: Text {
                                        text: modelData.label
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontBody
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: Theme.spacing8
                                    }
                                    
                                    background: Rectangle {
                                        color: parent.hovered ? Theme.hoverColor : Theme.raisedSurfaceColor
                                    }
                                }
                                
                                popup: Popup {
                                    width: logLevelCombo.width
                                    height: fullListHeight
                                    margins: Theme.spacing8
                                    padding: Theme.spacing4
                                    readonly property real fullListHeight: contentItem.implicitHeight + topPadding + bottomPadding
                                    readonly property real comboTopInWindow: logLevelCombo.mapToItem(null, 0, 0).y
                                    readonly property real preferredY: logLevelCombo.height
                                    readonly property real windowTopLimit: margins
                                    readonly property real windowBottomLimit: logLevelCombo.Window.window ? logLevelCombo.Window.window.height - margins : comboTopInWindow + preferredY + fullListHeight
                                    readonly property real minY: windowTopLimit - comboTopInWindow
                                    readonly property real maxY: windowBottomLimit - comboTopInWindow - fullListHeight
                                    y: Math.max(minY, Math.min(preferredY, maxY))
                                    
                                    contentItem: ListView {
                                        clip: true
                                        implicitHeight: contentHeight
                                        model: logLevelCombo.popup.visible ? logLevelCombo.delegateModel : null
                                        currentIndex: logLevelCombo.highlightedIndex
                                    }
                                    
                                    background: Rectangle {
                                        color: Theme.raisedSurfaceColor
                                        radius: Theme.radiusSmall
                                        border.color: Theme.borderColor
                                        border.width: 1
                                    }
                                }
                            }
                        }
                    }
                }

                // ==========================================
                // Card 3: 歌词分隔符配置 (Lyric Delimiters Card)
                // ==========================================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: delimiterCardLayout.implicitHeight + Theme.spacing16 * 2
                    color: Theme.raisedSurfaceColor
                    radius: Theme.radiusMedium
                    border.color: Theme.borderSubtle
                    border.width: 1

                    ColumnLayout {
                        id: delimiterCardLayout
                        anchors.fill: parent
                        anchors.margins: Theme.spacing16
                        spacing: Theme.spacing12

                        // Row 6: Lyric Delimiters Group
                        ColumnLayout {
                            id: delimiterListGroup
                            objectName: "delimiterList"
                            Layout.fillWidth: true
                            spacing: Theme.spacing8
                            
                            Text {
                                text: qsTr("歌词分隔符")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontTitle
                                font.weight: Font.DemiBold
                                Layout.bottomMargin: Theme.spacing4
                            }
                            
                            Repeater {
                                id: delimiterRepeater
                                model: settings.lyricDelimiters
                                
                                delegate: RowLayout {
                                    required property int index
                                    required property string modelData
                                    
                                    Layout.fillWidth: true
                                    spacing: Theme.spacing8
                                    
                                    TextField {
                                        id: delimiterInput
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 32
                                        text: modelData
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontBody
                                        
                                        background: Rectangle {
                                            color: Theme.baseColor
                                            radius: Theme.radiusSmall
                                            border.color: delimiterInput.activeFocus ? Theme.borderAccent : Theme.borderColor
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
                                                    if (i !== index) {
                                                        list.push(settings.lyricDelimiters[i]);
                                                    }
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
                                        textColor: Theme.textSecondary
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
                                spacing: Theme.spacing8
                                
                                TextField {
                                    id: newDelimiterInput
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    placeholderText: qsTr("输入新分隔符...")
                                    placeholderTextColor: Theme.textDisabled
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontBody
                                    
                                    background: Rectangle {
                                        id: newDelimiterBg
                                        color: Theme.baseColor
                                        radius: Theme.radiusSmall
                                        border.color: Theme.borderColor
                                        border.width: 1
                                        
                                        states: [
                                            State {
                                                name: "error"
                                                PropertyChanges {
                                                    newDelimiterBg.border.color: Theme.dangerColor
                                                }
                                            }
                                        ]
                                        
                                        transitions: [
                                            Transition {
                                                from: ""
                                                to: "error"
                                                ColorAnimation { duration: Theme.animationFast }
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
                                        radius: Theme.radiusSmall
                                        border.color: Theme.borderColor
                                        border.width: 1
                                    }
                                    
                                    contentItem: Text {
                                        text: qsTr("添加")
                                        color: Theme.accentColor
                                        font.pixelSize: Theme.fontBody
                                        font.weight: Font.DemiBold
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
