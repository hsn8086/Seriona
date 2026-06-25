# Seriona 代理说明

为后续 OpenCode 会话提供精简、已验证的项目上下文。所有面向用户的回答以及本仓库内的文档编写均使用中文。优先相信可执行来源（`CMakeLists.txt`、`.qmlls.ini`、`src/main.cpp`、`qml/`），不要依赖 `build/` 中可能过期的复制文件。

## 项目结构与入口
- **开发与构建环境**：这是一个基于 CMake 和 Qt 6.8+ (当前本地构建检测为 Qt 6.11) 的 Qt Quick 音乐播放器原型。
- **运行入口**：`src/main.cpp`。它设置了 `QT_IM_MODULE=qtvirtualkeyboard`，创建 `QQmlApplicationEngine` 并通过 `engine.loadFromModule("Seriona", "Main")` 加载主界面。
- **UI 源码入口**：`qml/Main.qml`。
- **状态与模拟数据**：播放/歌词状态及大部分模拟数据在 `qml/views/MainContent.qml`；侧边栏模拟曲库数据在 `qml/components/Sidebar.qml`。
- **实验与测试**：仓库无内置测试或 CI 校验。`test_popup.qml` 为独立的弹窗测试文件，未接入 CMake。

## 构建与运行命令
```bash
cmake -B build
cmake --build build
./build/appSeriona
```
- **LSP / QML 工具链**：在依赖 QML 语言工具或 LSP 诊断前，先执行 `cmake -B build`。本地 `.qmlls.ini` 指向了仓库的 `build` 目录、`/usr/share/doc/qt6` 和 `/usr/lib/qt6/qml`。
- **虚拟键盘插件错误**：如果在精简 Linux 环境中运行遇到虚拟键盘插件加载失败/警告，直接检查 `src/main.cpp` 里的 `QT_IM_MODULE=qtvirtualkeyboard` 设置，切勿误判为 QML 代码错误。

## QML 模块与资源规则
- **QML 模块名**：URI 为 `Seriona`（由 `qt_add_qml_module(appSeriona URI Seriona ...)` 声明）。导入本地组件必须使用 `import Seriona`，禁止使用相对路径导入。
- **CMake 文件同步**：新增 QML、C++ 源文件或资源资源（如 SVG）时，必须同步在 `CMakeLists.txt` 中注册。否则 `loadFromModule` 和 QRC 资源路径将无法识别。
- **全局单例**：`qml/theme/Theme.qml` 是通过 `set_source_files_properties` 注册的单例类型。共享的颜色、间距、动画时长等配置必须统一通过 `Theme.*` 引用。
- **SVG 资源引用**：必须使用绝对 Qt 资源路径引用，例如：`qrc:/qt/qml/Seriona/qml/assets/play.svg`。

## UI 开发与集成注意事项
- **源码修改限制**：只修改根目录 `qml/` 下的源文件。**绝不要**修改 `build/Seriona/qml/` 中生成的缓存副本，因为该目录在重新构建时会被覆盖。
- **窗口无边框与交互**：`qml/Main.qml` 声明了无边框窗口 `Qt.FramelessWindowHint`。窗口的拖拽移动通过 `window.startSystemMove()` 实现，边缘缩放通过本地的 `ResizeArea` 组件调用 `window.startSystemResize(edgeFlag)` 实现。
- **响应式侧边栏**：当 `width >= sidebarWidth + playerMinWidth` 时，侧边栏停靠显示；否则自动转为带半透明点击遮罩的浮层。
- **图形效果库**：当前项目同时使用了旧版的 `Qt5Compat.GraphicalEffects`（`ColorOverlay`、`OpacityMask` 等）和新版的 `QtQuick.Effects`（在 `MainContent.qml` 中）。请根据上下文谨慎选择，不要假设其中一方不可用。
- **C++ 与 QML 集成**：目前项目完全由 QML 模拟数据驱动。若未来需新增 C++ 后端类，优先使用 Modern Qt 6 方式（在类声明中添加 `QML_ELEMENT` / `QML_SINGLETON` 并编译 target，而非调用 `setContextProperty`）。
