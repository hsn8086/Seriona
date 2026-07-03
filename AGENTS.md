# Seriona 代理说明

所有面向用户的回复，以及本仓库内新写的文档，都使用中文。优先相信 `CMakeLists.txt`、`.qmlls.ini`、`src/main.cpp`、根目录 `src/` 和 `qml/`；不要从 `build/Seriona/qml/` 的生成或残留副本推断源码状态。

## 项目入口
- 这是单可执行 Qt Quick/CMake 项目，不是多包仓库；入口是 `src/main.cpp`，通过 `engine.loadFromModule("Seriona", "Main")` 加载 `qml/Main.qml`。
- 本地 QML 模块 URI 是 `Seriona`；仓库内组件统一 `import Seriona`，不要改成相对路径导入。
- `src/app/*` 里的 `QML_ELEMENT` 类型提供中间层：`AppFacade` 拥有播放和导航控制器，`LibraryController`/`LyricsModel` 直接给 QML 使用。不要把播放、歌词、库导航状态重新塞回 QML mock 属性里。

## 构建与验证
- 常用命令：`cmake -B build`、`cmake --build build`、`./build/appSeriona`。
- 先跑 `cmake -B build` 再做 QML LSP/qmlls 诊断；`.qmlls.ini` 的 `buildDir` 固定指向本仓库 `build/`。
- 仓库当前没有源码级 CI、正式测试目录、lint/formatter 配置或 repo-local OpenCode 配置；`test_popup.qml` 未接入 CMake，只能当临时实验文件。
- `./scripts/verify-middle-layer.sh` 会配置、构建并做 offscreen smoke，但还会检查 `docs/architecture/backend-integration-contract.md`；该文档缺失时不要把脚本当作必然可通过的标准验证。

## QML 与资源
- 新增或重命名 QML、C++、SVG 资源时，同步更新 `CMakeLists.txt` 的 `qt_add_qml_module(...)`，否则模块加载或 QRC 路径会失效。
- `qml/theme/Theme.qml` 在 CMake 里注册为 singleton；共享颜色、尺寸、动画参数优先复用 `Theme.*`。
- SVG 资源使用绝对 QRC 路径，例如 `qrc:/qt/qml/Seriona/qml/assets/play.svg`；新增图标沿用该模式。

## 易踩坑
- `qml/Main.qml` 是无边框窗口；标题栏、遮罩、边框改动必须保住 `window.startSystemMove()` 拖拽和 `window.startSystemResize(...)` 八向缩放。
- 虚拟键盘来自 `qml/Main.qml` 的 `QtQuick.VirtualKeyboard` 和 `InputPanel`，不是 `src/main.cpp` 里的 `QT_IM_MODULE` 环境变量。
- 项目同时使用 `Qt5Compat.GraphicalEffects` 和 `QtQuick.Effects`；改图形效果前先看当前文件依赖哪套。
