# Seriona 代理说明

所有面向用户的回复，以及本仓库内新写的文档，都使用中文。优先相信可执行来源：`CMakeLists.txt`、`.qmlls.ini`、`src/main.cpp`、`qml/`；不要从 `build/` 里的生成副本推断真实源码状态。

## 入口与结构
- 这是单可执行 Qt Quick 项目，不是多包仓库；应用入口是 `src/main.cpp`，通过 `engine.loadFromModule("Seriona", "Main")` 加载 `qml/Main.qml`。
- 主要 UI 状态集中在 `qml/Main.qml` 和 `qml/views/MainContent.qml`；侧边栏自己的模拟数据和导航在 `qml/components/Sidebar.qml`。
- `qml/theme/Theme.qml` 是在 `CMakeLists.txt` 里注册的 singleton；共享颜色、尺寸、动画参数优先复用 `Theme.*`，不要在组件里再发明一套常量。

## 构建与验证
- 基本命令：`cmake -B build`、`cmake --build build`、`./build/appSeriona`。
- 中间层验证脚本：运行 `./scripts/verify-middle-layer.sh` 可以执行验证（若需要验证 C++ 层数据和模型是否正常）。
- 先跑一次 `cmake -B build` 再做 QML LSP / qmlls 相关诊断；`.qmlls.ini` 的 `buildDir` 就指向本仓库的 `build/`。
- 仓库里没有现成的测试或 CI；`test_popup.qml` 也没有接入 CMake，不要把它当成可直接运行的正式测试入口。

## QML 模块规则
- 本地 QML 模块 URI 是 `Seriona`；仓库内组件统一 `import Seriona`，不要改成相对路径导入。
- 新增 QML、C++ 或资源文件时，必须同步更新 `CMakeLists.txt` 里的 `qt_add_qml_module(...)`，否则模块加载和资源解析都会失效。
- SVG 资源当前都走绝对 QRC 路径，例如 `qrc:/qt/qml/Seriona/qml/assets/play.svg`；新增资源时沿用这一模式。

## 易踩坑
- 只改根目录 `qml/` 下的源文件；`build/Seriona/qml/` 是构建产物，改了也会被覆盖。
- `src/main.cpp` 强制设置了 `QT_IM_MODULE=qtvirtualkeyboard`；如果运行时看到虚拟键盘插件相关警告，先排查这里，不要误判成 QML 逻辑问题。
- `qml/Main.qml` 是无边框窗口，拖拽和缩放分别依赖 `window.startSystemMove()` 与 `window.startSystemResize(...)`；改标题栏、边框或遮罩时要保住这些交互。
- 这个项目同时用了 `Qt5Compat.GraphicalEffects` 和 `QtQuick.Effects`；改图形效果前先看当前文件依赖哪套，不要假设可以随意互换。
