pragma Singleton
import QtQuick

QtObject {
    // ==========================================
    // 既有基础颜色与兼容定义 (Existing Base Colors)
    // 注意：保留所有既有属性名与值，以保障测试与既有组件引用兼容
    // ==========================================
    readonly property color backgroundColor: "#1e1e1e"
    readonly property color mainColor: "#2d2d2d"
    readonly property color sidebarBackgroundColor: "#151515"
    readonly property color accentColor: "#5B9DFF" // 优雅现代蓝 (#5B9DFF)，和谐融入暗色界面

    // ==========================================
    // 控件状态颜色 (Control State Colors)
    // ==========================================
    readonly property color baseColor: "#20FFFFFF"
    readonly property color hoverColor: "#30FFFFFF"
    readonly property color pressedColor: "#40FFFFFF"
    readonly property color checkedColor: "#60FFFFFF"
    readonly property color textColor: "white"
    readonly property color secondaryTextColor: "#cccccc" // 增加对比度
    readonly property color playButtonBg: "white"
    readonly property color playButtonText: "black"

    // ==========================================
    // ToolTip 悬停提示 (ToolTip Colors)
    // ==========================================
    readonly property color tooltipBackgroundColor: "#1A1A1A"
    readonly property color tooltipBorderColor: "#333333"
    readonly property color tooltipTextColor: "#FFFFFF"
    readonly property real tooltipBackgroundOpacity: 0.95
    readonly property int tooltipFontSize: 12
    readonly property int tooltipRadius: 4
    readonly property int tooltipDelay: 500
    readonly property int tooltipAnimationDuration: 200

    // ==========================================
    // 窗口控制按钮颜色 (Window Control Colors)
    // ==========================================
    readonly property color closeColor: "#e05c5c" // 降低饱和度
    readonly property color closeHoverColor: "#ff4d4d"
    readonly property color minimizeColor: "#e0b04c"
    readonly property color minimizeHoverColor: "#ffc13b"
    readonly property color maximizeColor: "#4ca656"
    readonly property color maximizeHoverColor: "#32b842"

    // ==========================================
    // 既有尺寸与间距 (Existing Sizes and Spacing)
    // ==========================================
    readonly property int paddingSmall: 4
    readonly property int paddingMedium: 8
    readonly property int paddingLarge: 16
    readonly property int borderRadius: 8
    readonly property int sidebarWidth: 350

    // ==========================================
    // 既有动画时长 (Existing Animation Durations)
    // ==========================================
    readonly property int animationDuration: 150
    readonly property int colorTransitionDuration: 500

    // ==========================================
    // 既有渐变背景色 (Existing Gradient Background Colors)
    // ==========================================
    readonly property color gradientColor0: "#4a2c2a" // 示例红褐色/棕褐色渐变
    readonly property color gradientColor1: "#2b1a1a"
    readonly property color gradientColor2: "#1a1212"

    // ==========================================
    // 新增：色阶层级 (Surface & Elevation Hierarchy)
    // ==========================================
    readonly property color surfaceColor: backgroundColor          // 主背景层 (#1e1e1e)
    readonly property color raisedSurfaceColor: mainColor          // 悬浮/卡片层 (#2d2d2d)
    readonly property color overlayScrimColor: "#99000000"         // 模态弹窗/抽屉遮罩层半透明黑
    readonly property color sidebarSurfaceColor: sidebarBackgroundColor // 侧边栏背景层 (#151515)

    // ==========================================
    // 新增：文本层级 (Typography Hierarchy)
    // ==========================================
    readonly property color textPrimary: textColor                 // 主文本 (white)
    readonly property color textSecondary: secondaryTextColor      // 次要文本 (#cccccc)
    readonly property color textDisabled: "#60FFFFFF"              // 禁用/占位符文本
    readonly property color textOnAccent: "#FFFFFF"                // 主题强调色上的前景色

    // ==========================================
    // 新增：语义色系 (Semantic State Colors)
    // ==========================================
    readonly property color successColor: "#4ca656"                // 成功色 (绿)
    readonly property color warningColor: "#e0b04c"                // 警告色 (黄/橙)
    readonly property color dangerColor: "#e05c5c"                 // 危险/错误色 (红)
    readonly property color infoColor: "#4c8de0"                   // 提示/信息色 (蓝)

    // 危险操作交互态 (Danger Action States)
    readonly property color dangerHoverColor: "#ff4d4d"
    readonly property color dangerPressedColor: "#B33A3A"

    // Toast 背景及边框语义色 (Toast Semantic Styling)
    readonly property color toastErrorBg: "#E62D0D0D"
    readonly property color toastWarningBg: "#E62D2300"
    readonly property color toastInfoBg: "#E6202020"
    readonly property color toastWarningBorder: "#CCB35C00"

    // ==========================================
    // 新增：字号体系 (Typography Size Scale - px)
    // ==========================================
    readonly property int fontCaption: 11                          // 辅助标注/角标/次要元数据
    readonly property int fontBody: 13                             // 正文/列表主文本
    readonly property int fontTitle: 15                            // 标题/按钮/重要信息
    readonly property int fontSubtitle: 18                         // 副标题/弹窗标题
    readonly property int fontHeading: 22                          // 大标题/播放器曲名

    // ==========================================
    // 新增：间距体系 (Spacing Scale - px)
    // ==========================================
    readonly property int spacing2: 2                              // 极小间距 (分割线微调/图标微偏移)
    readonly property int spacing4: 4                              // 紧凑间距 (小组件内边距)
    readonly property int spacing8: 8                              // 标准小间距 (列表项间距/标签间距)
    readonly property int spacing12: 12                            // 中等间距 (表单行间距)
    readonly property int spacing16: 16                            // 标准间距 (卡片内边距/模块间距)
    readonly property int spacing24: 24                            // 宽松间距 (主要区块间隔)

    // ==========================================
    // 新增：圆角体系 (Corner Radius Scale - px)
    // ==========================================
    readonly property int radiusSmall: 4                           // 小控件/标签/输入框圆角
    readonly property int radiusMedium: borderRadius               // 标准圆角 (卡片/按钮, 8px)
    readonly property int radiusLarge: 12                          // 模态弹窗/大卡片/封面容器圆角
    readonly property int radiusFull: 9999                         // 全胶囊/圆形圆角 (FAB/徽标)

    // ==========================================
    // 新增：描边体系 (Border Tokens)
    // ==========================================
    readonly property color borderColor: "#30FFFFFF"               // 标准可见边框
    readonly property color borderSubtle: "#15FFFFFF"              // 微弱分隔/深色描边
    readonly property color borderAccent: accentColor              // 聚焦/高亮状态边框

    // ==========================================
    // 新增：阴影体系 (Elevation Shadows)
    // ==========================================
    // 卡片阴影 (Card Elevation)
    readonly property color shadowCardColor: "#40000000"
    readonly property real shadowCardBlur: 8.0
    readonly property real shadowCardOffsetX: 0.0
    readonly property real shadowCardOffsetY: 2.0

    // 弹窗/菜单阴影 (Popup/Menu Elevation)
    readonly property color shadowPopupColor: "#80000000"
    readonly property real shadowPopupBlur: 16.0
    readonly property real shadowPopupOffsetX: 0.0
    readonly property real shadowPopupOffsetY: 4.0

    // Toast/通知阴影 (Toast Elevation)
    readonly property color shadowToastColor: "#60000000"
    readonly property real shadowToastBlur: 12.0
    readonly property real shadowToastOffsetX: 0.0
    readonly property real shadowToastOffsetY: 3.0

    // ==========================================
    // 新增：动效时长与缓动 (Animation & Easing Curves)
    // ==========================================
    readonly property int animationFast: animationDuration         // 微交互 (按钮悬停/按压, 150ms)
    readonly property int animationStandard: 250                   // 标准组件过渡/展开收起 (250ms)
    readonly property int animationSlow: colorTransitionDuration   // 复杂场景/渐变色彩过渡 (500ms)

    readonly property int easingStandard: Easing.InOutQuad         // 标准平滑过渡
    readonly property int easingDecelerate: Easing.OutCubic        // 入场/出现缓动 (进场减速)
    readonly property int easingAccelerate: Easing.InCubic         // 退场/消失缓动 (出场加速)

    // ==========================================
    // 新增：滚动条组件专用令牌 (Scrollbar Tokens)
    // ==========================================
    readonly property color scrollbarColor: "#40FFFFFF"            // 滚动条滑块默认颜色
    readonly property color scrollbarHoverColor: "#70FFFFFF"       // 滚动条滑块悬停颜色
    readonly property int scrollbarWidth: 6                        // 滚动条厚度 (px)

    // ==========================================
    // 新增：播放控制与进度条专用令牌 (Playback & Controls)
    // ==========================================
    readonly property color progressBarColor: "#FFFFFF"            // 进度条已播高亮色 (纯白/中性高对比)
    readonly property color progressBarTrackColor: "#30FFFFFF"     // 进度条轨道底色
    readonly property color progressBarHoverColor: "#80FFFFFF"     // 进度条悬停/高光色
    readonly property color waveformColor: "#30FFFFFF"             // 波形未播放底色
    readonly property color waveformPlayedColor: "#FFFFFF"         // 波形已播放高亮色 (纯白/中性高对比)

    // ==========================================
    // 新增：星级评分专用令牌 (Rating Tokens)
    // ==========================================
    readonly property color ratingColor: "#FFC107"                 // 已选星级颜色 (温暖金黄琥珀色)
    readonly property color ratingUnselectedColor: "#40FFFFFF"     // 未选星级颜色

    // ==========================================
    // 新增：播放队列专用令牌 (Queue View Tokens)
    // ==========================================
    readonly property color queuePlayingHighlightColor: accentColor // 正在播放项强调高亮
    readonly property color queueItemHoverBg: hoverColor           // 队列项悬停背景

    // ==========================================
    // 新增：详情窗专用令牌 (Track Detail Tokens)
    // ==========================================
    readonly property color detailLabelColor: textSecondary        // 详情字段标签色
    readonly property color detailValueColor: textPrimary          // 详情字段数值色
}

