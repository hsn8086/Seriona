import QtQuick
import QtQuick.Controls.Basic
import Seriona

// 按钮悬停提示：hover 显示、按下抑制，500ms 延迟 + 200ms 透明度过渡
ToolTip {
    id: control

    property bool suppressed: false

    visible: parent.hovered && !suppressed
    delay: Theme.tooltipDelay

    Connections {
        target: parent

        function onPressedChanged() {
            if (parent.pressed)
                control.suppressed = true;
        }

        function onHoveredChanged() {
            if (!parent.hovered)
                control.suppressed = false;
        }
    }

    contentItem: Text {
        text: control.text
        color: Theme.tooltipTextColor
        font.pixelSize: Theme.tooltipFontSize
        leftPadding: Theme.paddingSmall
        rightPadding: Theme.paddingSmall
    }

    background: Rectangle {
        color: Theme.tooltipBackgroundColor
        radius: Theme.tooltipRadius
        opacity: Theme.tooltipBackgroundOpacity
        border.color: Theme.tooltipBorderColor
        border.width: 1
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: Theme.tooltipAnimationDuration
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: Theme.tooltipAnimationDuration
        }
    }
}
