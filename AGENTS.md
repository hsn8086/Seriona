# Seriona 代理说明

为后续 OpenCode 会话提供精简、已验证的项目上下文。所有面向用户的回答以及本仓库内的文档编写均使用中文。优先相信可执行来源（`CMakeLists.txt`、`.qmlls.ini`、`src/main.cpp`、`qml/`），不要依赖 `build/` 中可能过期的复制文件。

## 项目结构
- 这是一个基于 CMake 和 Qt 6.8+ 的 Qt Quick 音乐播放器原型（`qt_standard_project_setup(REQUIRES 6.8)`）。当前本地构建在 `build/CMakeFiles/appSeriona_autogen.dir/AutogenInfo.json` 中显示 Qt 6.11。
- 运行入口是 `src/main.cpp`；它设置 `QT_IM_MODULE=qtvirtualkeyboard`，创建 `QQmlApplicationEngine`，并通过 `engine.loadFromModule("Seriona", "Main")` 加载 `Seriona/Main`。
- UI 源码入口是 `qml/Main.qml`；播放/歌词状态和大部分模拟播放数据在 `qml/views/MainContent.qml`，侧边栏模拟曲库数据在 `qml/components/Sidebar.qml`。
- 仓库没有配置测试、lint、formatter 或 CI。`test_popup.qml` 是独立的弹窗实验文件，没有接入 CMake。

## 构建与运行
```bash
cmake -B build
cmake --build build
./build/appSeriona
```
- 依赖 QML 语言工具前先运行 `cmake -B build`；`.qmlls.ini` 指向仓库本地 `build` 目录，以及 `/usr/share/doc/qt6` 和 `/usr/lib/qt6/qml`。
- 如果在精简 Linux 环境中运行时出现虚拟键盘插件相关警告或失败，先检查 `src/main.cpp` 中的 `QT_IM_MODULE=qtvirtualkeyboard`，不要误查无关 QML 问题。

## QML 模块规则
- 应用的 QML 模块 URI 是 `Seriona`，由 `qt_add_qml_module(appSeriona URI Seriona ...)` 声明；项目 QML 文件通过 `import Seriona` 导入本地组件，不使用相对导入。
- 新增 QML 文件、C++ 源文件或资源时必须加入 `CMakeLists.txt`；否则 `loadFromModule` 和 `qrc:/qt/qml/Seriona/...` 资源路径无法看到它们。
- `qml/theme/Theme.qml` 是通过 `set_source_files_properties(... QT_QML_SINGLETON_TYPE TRUE)` 注册的单例。共享颜色、间距和动画时长应来自 `Theme.*`。
- SVG 资源使用绝对 Qt 资源路径，例如 `qrc:/qt/qml/Seriona/qml/assets/play.svg`。

## UI 与源码注意事项
- 只编辑根目录 `qml/` 下的源文件，不要编辑 `build/Seriona/qml/` 下生成或复制的 QML；构建树里有未列入 `CMakeLists.txt` 的旧视图/组件等过期文件。
- `qml/Main.qml` 是无边框窗口（`Qt.FramelessWindowHint`），通过 `window.startSystemMove()` 和本地 `ResizeArea` 组件调用 `window.startSystemResize(edgeFlag)` 实现移动/缩放。
- 侧边栏是响应式的：当 `width >= sidebarWidth + playerMinWidth` 时停靠，否则作为带点击遮罩的浮层显示。
- 图形效果当前同时使用 `Qt5Compat.GraphicalEffects`（`ColorOverlay`、`OpacityMask`、`DropShadow`、`RectangularGlow`）以及 `qml/views/MainContent.qml` 中的 `QtQuick.Effects`；不要假设 `QtQuick.Effects` 被禁止。

## C++/QML 集成
- 当前没有注册任何后端类；UI 由 QML 模拟数据驱动。
- 如果新增 Qt/C++ 后端类型，优先通过现有 `qt_add_qml_module` 目标使用 Qt 6 模块注册方式（`QML_ELEMENT`、`QML_SINGLETON` 等），而不是 `setContextProperty`。
