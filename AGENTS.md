# Seriona 代理说明

所有面向用户的回复及本仓库内新写的文档均使用中文。以根目录 `CMakeLists.txt`、`src/`、`qml/` 和 `scripts/` 为准；不要从 `build/Seriona/qml/` 的生成或残留副本推断源码状态。

## 入口与边界
- 这是单可执行 Qt Quick/CMake 项目（要求 Qt 6.8+，`qt_standard_project_setup(REQUIRES 6.8)`）；`src/main.cpp` 通过 `engine.loadFromModule("Seriona", "Main")` 加载 `qml/Main.qml`。
- `qt_add_qml_module(...)` 注册本地 URI `Seriona`。新增或重命名 `src/app` C++、模块内 QML、SVG 或 `tests/frontend/adapter/` 测试源时，同步更新 `CMakeLists.txt` 的 `SERIONA_APP_LAYER_SOURCES`、`SERIONA_QML_MODULE_FILES`、`SERIONA_QML_MODULE_RESOURCES` 及对应 `add_executable(...)`；不要把入口或测试 C++ 塞进 QML 模块。
- `qml/Main.qml` 实例化唯一的 `AppFacade`。它拥有播放、曲库、歌词、通知和导航对象，并持有 `BackendBridge` 与 `WaveformProvider`（两者始终实例化；`SERIONA_HAS_BACKEND=0` 时不编译后端调用）；不要把这些状态重新放回 QML mock 属性。
- `PlaybackController`、`NavigationController`、`NotificationController` 和 `LibraryModel` 是 `QML_UNCREATABLE`；`LibraryController` 定义在 `library_model.h/.cpp`（没有独立文件），`LyricsModel` 同为 `QML_ELEMENT`。正式 QML 统一使用 `appFacade.playback/library/lyrics/notifications/navigation`，不要另建可创建的控制器或模型。
- 孤儿文件（均未接入 CMake/engine，修改不影响正式应用或测试）：`src/providers/thumbnail_image_provider.{h,cpp}`、根目录 `test_popup.qml`、`qml/assets/MaterialIcons-Regular.ttf`（不在 QML 模块 RESOURCES，也无 FontLoader 引用）。根目录 `popup_log.txt` 是已跟踪的空日志残留（0 字节，`*.log` 在 .gitignore 中但该文件早于规则提交），属垃圾文件可删除。

## 构建与验证
- 顺序是 `cmake -B build`、`cmake --build build`、`ctest --test-dir build --output-on-failure`；应用为 `./build/appSeriona`。
- `.clangd` 读取 `build/`，`.qmlls.ini` 则硬编码当前工作区 `build/` 的绝对路径；新 worktree 或移动目录后先修正该路径并配置构建目录，再运行语言服务诊断。
- 聚焦一个 CTest：`ctest --test-dir build -R '^seriona_frontend_command_result_mapping$' --output-on-failure`。同一测试二进制中的单个 QTest case 直接作为参数传入，例如 `./build/seriona_frontend_library_sort_tests titleAscendingAndDescendingSortCurrentFolderProjection`（该目标依赖后端）。
- `./scripts/verify-middle-layer.sh`（依赖 `rg`）会配置、构建、检查中间层/CMake 不变量并运行 `QT_QPA_PLATFORM=offscreen timeout 5s ./build/appSeriona`，预期退出码为 `124`；它要求 `docs/architecture/backend-integration-contract.md` 存在，但不会运行 CTest。可用 `SERIONA_BUILD_DIR` 覆盖构建目录。它是必要子集门禁而非穷举：QML 只查 17 选 12（漏 DynamicBackground/SortDialog/SortRuleRow/SettingsWindow/EqualizerWindow），测试目标只查 21 选 20（漏 `seriona_frontend_library_sort_tests`），新增文件仍须手动同步 CMakeLists。
- Smoke CLI：`./build/appSeriona --smoke-scenario=<name> --smoke-exit-ms=<ms>`；场景为 `startup`/`main-playback`/`lyrics`/`sidebar-tree`/`settings-menu`/`empty-library`。默认 1000 ms 后退出并写入 `.omo/evidence/smoke/smoke-<scenario>.log`；`--smoke-output-dir=<dir>` 可改目录。`Main.qml` 的 `smokeVisualStateJson()` 已定义但当前无调用方（为扩展预留），不要删除或自行接线。
- 仓库没有独立的 lint、format、CI 或代码生成命令；QML/MOC/RCC 生成由 CMake/Qt 完成。CMake 只设 `CMAKE_CXX_STANDARD_REQUIRED ON`，未显式声明 `CMAKE_CXX_STANDARD`；标准由 Qt/后端传递（观测 `-std=c++23`），不要假设已声明某标准。
- 接入后端时终端日志统一走 spdlog：Qt 消息（qDebug/QML `console.log`）重定向到 spdlog 默认 logger，`find_package(spdlog CONFIG REQUIRED)` 在配置期强制（与后端同源，mock-only 不要求）；smoke 调试日志仅 Debug 构建输出（`main.cpp` 注入 `smokeLoggingEnabled`，`NDEBUG` 下恒为 false），mock-only 下通知类日志不输出到终端。

## 中间层行为契约
- 前端不得直接实现文件系统/网络/数据库访问（`QDir::*`、`QFileSystem*`、`QNetwork*`、`QSql*` 等）；一切经 `BackendBridge` 的命令/快照边界，`verify-middle-layer.sh` 会对 `src/` 和 `qml/` 强制检查。
- 不支持的设置项（Crossfade、Equalizer 等）必须走 `NotificationController::showUnsupportedAction()` 本地反馈，禁止伪造后端命令或静默吞掉；`Exit` 是例外，必须走真实关闭链路。
- 播放游标与浏览游标分离：`playingTrackId` 与 `selectedBrowserNodeId` 各自独立，浏览/定位不得反向污染播放身份。
- 详细契约见 `docs/architecture/backend-integration-contract.md`（verify 脚本强制要求存在）与 `docs/backend-integration-strategy.md`。

## DESIGN.md 维护规范
- `DESIGN.md` 是描述项目长期稳定设计的架构文档，面向新开发者理解整体设计；它不是开发日志、变更记录或实现细节文档。它只是描述性文档，若与源码冲突，以源码为准，并按本节标准判断是否更新。
- 完成任何开发任务时，必须把"本次修改是否导致 `DESIGN.md` 已无法准确描述当前项目"列为检查项：若会，必须在同一次任务内同步更新；若不会，则不修改，也不为"保持同步"做无意义更新。确认无需更新时，不必在 `DESIGN.md` 中留下任何痕迹。
- 判断是否需要更新的唯一依据：新开发者只读旧版 `DESIGN.md` 是否会错误理解当前项目。与是否修改了代码、是否修改了架构名称无关。
- 通常不应更新（包括但不限于）：Bug 修复、代码重构、性能优化、实现细节调整、内部接口调整、参数修改、不影响整体设计的小功能开发、代码风格调整、测试补充。
- 通常应当更新（包括但不限于）：整体架构调整、模块新增或删除、模块职责变化、模块协作关系变化、启动流程变化、核心运行流程变化、配置体系变化、扩展机制变化、长期维护方式变化，以及其他会影响开发者理解项目整体设计的重要修改。
- `DESIGN.md` 应保持稳定：仅在长期设计变化时更新，避免随项目开发逐渐演变为实现文档或变更日志，不给开发增加不必要的维护负担。

## 后端集成（可选）
- `SERIONA_BACKEND_SOURCE_DIR` 默认是相对仓库根目录的 `../Seriona_Backend`；设为 `""` 才会强制 mock-only。非空路径不存在时 CMake 从 `https://github.com/kaizen857/Seriona_Backend.git` 的 `main` 分支 FetchContent，抓取失败会配置失败，不会自动回退。
- 接入后端时，前端链接后端的 control/audio/app 目标并定义 `SERIONA_HAS_BACKEND=1`；CMake 会关闭后端子树自身的 app/tests，再恢复前端的 `BUILD_TESTING`。
- `BUILD_TESTING` 默认为 `ON`；启用测试时，mock-only 只注册 `seriona_frontend_command_result_mapping`、`seriona_frontend_snapshot_mapping`、`seriona_frontend_library_tree_mapping`、`seriona_frontend_settings_controller_tests`、`seriona_frontend_app_facade_smoke_mode`（设置控制器是纯 QML 面，无后端依赖），其余前端测试都要求后端目标。
- 离线运行验证脚本时，可用 `SERIONA_FETCHCONTENT_CATCH2_DIR` 和 `SERIONA_FETCHCONTENT_THREAD_POOL_DIR` 指向已有依赖源码。

## QML 与资源
- `qml/theme/Theme.qml` 注册为 singleton；共享颜色、尺寸和动画参数复用 `Theme.*`。
- SVG 资源使用绝对 QRC 路径，例如 `qrc:/qt/qml/Seriona/qml/assets/play.svg`；新增图标沿用该模式。
- `qml/Main.qml` 是无边框窗口；标题栏、遮罩、边框改动必须保住 `window.startSystemMove()` 拖拽和 `window.startSystemResize(...)` 八向缩放。
- 项目同时使用 `Qt5Compat.GraphicalEffects` 和 `QtQuick.Effects`；改图形效果前先看当前文件依赖哪套。
