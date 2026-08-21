.pragma library
// BubbleMenu 实例注册表。.pragma library 使模块在 QML 引擎内单例共享：
// 无该指令时每个导入方得到独立模块实例（实测 B_SEES_COUNT=0），注册表会失效。
// Wayland 下 xdg_popup 有 grab 链约束：同一时刻只允许一个 grabbing popup。
// 任何 BubbleMenu 打开前先关闭其他实例，避免 "Creating a popup with a parent
// which does not match the current topmost grabbing popup" 警告与双菜单并存。

var menus = [];

function register(menu) {
    if (menus.indexOf(menu) < 0)
        menus.push(menu);
}

function unregister(menu) {
    var idx = menus.indexOf(menu);
    if (idx >= 0)
        menus.splice(idx, 1);
}

// 关闭除 self 外的所有已注册 BubbleMenu 实例。
function closeOthers(self) {
    var i, menu;
    for (i = 0; i < menus.length; ++i) {
        menu = menus[i];
        if (menu !== self && menu.isBubbleMenu)
            menu.close();
    }
}
