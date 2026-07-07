# Seriona 代理说明

所有面向用户的回复，以及本仓库内新写的文档，都使用中文。优先相信 `CMakeLists.txt`、`.qmlls.ini`、`src/main.cpp`、根目录 `src/` 和 `qml/`；不要从 `build/Seriona/qml/` 的生成或残留副本推断源码状态。

## 项目入口
- 这是单可执行 Qt Quick/CMake 项目，不是多包仓库；入口是 `src/main.cpp`，通过 `engine.loadFromModule("Seriona", "Main")` 加载 `qml/Main.qml`。
- 本地 QML 模块 URI 是 `Seriona`；仓库内组件统一 `import Seriona`，不要改成相对路径导入。
- `src/app/*` 里的 `QML_ELEMENT` 类型提供中间层：`AppFacade` 拥有播放和导航控制器，`LibraryModel`/`LyricsModel` 直接给 QML 使用。不要把播放、歌词、库导航状态重新塞回 QML mock 属性里。

## 构建与验证
- 常用命令：`cmake -B build`、`cmake --build build`、`./build/appSeriona`。
- 先跑 `cmake -B build` 再做 QML LSP/qmlls 诊断；`.qmlls.ini` 的 `buildDir` 固定指向本仓库 `build/`。
- 测试：`ctest --test-dir build` 或 `cmake --build build --target test`；单个测试用 `./build/seriona_frontend_<test_name>`。
- `./scripts/verify-middle-layer.sh` 会配置、构建、跑断言规则并做 offscreen smoke（期望超时退出码 124）；该脚本强制检查 `docs/architecture/backend-integration-contract.md` 存在。
- **Smoke 测试**：`./build/appSeriona --smoke-scenario=<name> --smoke-exit-ms=<ms>`；支持场景：`startup`/`main-playback`/`lyrics`/`sidebar-tree`/`settings-menu`/`empty-library`，默认 1000ms 后退出，日志输出到 `.omo/evidence/smoke/smoke-<scenario>.log`。验证脚本用 `QT_QPA_PLATFORM=offscreen timeout 5s` 模式。

## 后端集成（可选）
- CMake 变量 `SERIONA_BACKEND_SOURCE_DIR` 指向外部 C++ 后端源码树（**默认 `../Seriona_Backend`**，相对于本仓库根目录）；后端不存在时尝试从 `https://github.com/kaizen857/Seriona_Backend.git` FetchContent 拉取，仍失败则退回 mock-only 模式。
- 后端存在时，CMake 会 `add_subdirectory` 后端并链接 `SerionaBackend::control` 和 `SerionaBackend::audio`，同时定义 `SERIONA_HAS_BACKEND=1`。
- 多数 `tests/frontend/adapter/tst_*.cpp` 需要后端；mock-only 模式下这些测试不会被添加到 CTest。

## QML 与资源
- 新增或重命名 QML、C++、SVG 资源时，同步更新 `CMakeLists.txt` 的 `qt_add_qml_module(...)`，否则模块加载或 QRC 路径会失效。
- `qml/theme/Theme.qml` 在 CMake 里注册为 singleton；共享颜色、尺寸、动画参数优先复用 `Theme.*`。
- SVG 资源使用绝对 QRC 路径，例如 `qrc:/qt/qml/Seriona/qml/assets/play.svg`；新增图标沿用该模式。

## 易踩坑
- `qml/Main.qml` 是无边框窗口；标题栏、遮罩、边框改动必须保住 `window.startSystemMove()` 拖拽和 `window.startSystemResize(...)` 八向缩放。
- 虚拟键盘来自 `qml/Main.qml` 的 `QtQuick.VirtualKeyboard` 和 `InputPanel`，不是 `src/main.cpp` 里的 `QT_IM_MODULE` 环境变量。
- 项目同时使用 `Qt5Compat.GraphicalEffects` 和 `QtQuick.Effects`；改图形效果前先看当前文件依赖哪套。
- `test_popup.qml` 未接入 CMake，只能当临时实验文件；`tests/frontend/` 里的 `.cpp` 测试才是正式测试套件。
