pragma Singleton
import QtQuick

QtObject {
    // 颜色定义 (Colors)
    readonly property color backgroundColor: "#1e1e1e"
    readonly property color mainColor: "#2d2d2d"
    readonly property color sidebarBackgroundColor: "#151515"
    readonly property color accentColor: "#ff5c5c"
    
    // 按钮/控件状态颜色 (Control State Colors)
    readonly property color baseColor: "#20FFFFFF"
    readonly property color hoverColor: "#30FFFFFF"
    readonly property color pressedColor: "#40FFFFFF"
    readonly property color checkedColor: "#60FFFFFF"
    readonly property color textColor: "white"
    readonly property color secondaryTextColor: "#aaaaaa"
    readonly property color playButtonBg: "white"
    readonly property color playButtonText: "black"

    // 窗口控制按钮颜色 (Window Control Colors)
    readonly property color closeColor: "#ff5f56"
    readonly property color closeHoverColor: "#ff3b30"
    readonly property color minimizeColor: "#ffbd2e"
    readonly property color minimizeHoverColor: "#ffcc00"
    readonly property color maximizeColor: "#27c93f"
    readonly property color maximizeHoverColor: "#34c759"

    // 尺寸与间距 (Sizes and Spacing)
    readonly property int paddingSmall: 4
    readonly property int paddingMedium: 8
    readonly property int paddingLarge: 16
    readonly property int borderRadius: 8
    readonly property int sidebarWidth: 350

    // 动画时长 (Animation Durations)
    readonly property int animationDuration: 150
    readonly property int colorTransitionDuration: 500

    // 渐变背景色 (Gradient Background Colors)
    readonly property color gradientColor0: "#4a2c2a" // 示例红褐色/棕褐色渐变
    readonly property color gradientColor1: "#2b1a1a"
    readonly property color gradientColor2: "#1a1212"
}
