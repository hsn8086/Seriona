# Seriona 项目指南 (AGENTS.md)

本文件旨在为后续 OpenCode 代理提供关于 Seriona 音乐播放器项目的快速上手与规范指南，避免重复探索和低级错误。

## 1. 项目概况与架构
- **技术栈**: Qt 6.8+, CMake, QML (Qt Quick)
- **核心文件**:
  - `src/main.cpp`: 入口文件。通过 `QQmlApplicationEngine::loadFromModule` 现代模块化方式加载。
  - `qml/Main.qml`: UI 主入口。
- **QML 模块与 URI**: 本项目的 QML 模块 URI 注册为 `Seriona`。
  - **重要**: 任何 QML 文件导入其他组件时，**必须且只能**使用 `import Seriona`。严禁使用相对路径（如 `import "../components"`）。
- **主题单例 (Theme Singleton)**: `qml/theme/Theme.qml` 声明了 `pragma Singleton`，并在 `CMakeLists.txt` 中通过 `set_source_files_properties` 显式注册为 QML 单例类型。
  - 访问配色、间距和动画时长必须通过全局的 `Theme.xxx`。

## 2. 关键设计模式与交互
- **无边框窗口与自定义窗口控制**:
  - `Main.qml` 设为了 `FramelessWindowHint`。
  - 窗口移动：调用 `window.startSystemMove()` 实现。
  - 窗口缩放：通过自定义的 `ResizeArea` 组件，调用 `window.startSystemResize(edgeFlag)` 支持 8 方向系统级拉伸。
- **响应式双栏布局**:
  - 当窗口宽度 $\ge 800\text{px}$ 时（`isDockCapable`），侧边栏 `Sidebar` 会自动停靠嵌入布局。
  - 宽度较小时，`Sidebar` 表现为带背景遮罩（点击可收起）的悬浮层。
- **视图与动画状态机**:
  - `MainContent.qml` 包含 `playback` 与 `lyrics` 状态。在两状态切换时，封面图、波形/线性进度条以及控制按钮会进行平滑的共享元素动画（Anchor & Number 动画）过渡。
- **高阶自定义 UI 组件**:
  - `MarqueeText.qml`: 跑马灯文本，支持无缝首尾相接滚动。
  - `WaveformProgressBar.qml`: 波形进度条，支持悬停预览及扁平化模式（`flatMode`）过渡动画。
  - `BubbleMenu.qml`: 带气泡指向箭头和弹性缩放动画的弹出菜单。

## 3. 开发与验证命令
### 构建与运行
```bash
# 1. 自动配置构建目录
cmake -B build

# 2. 编译项目
cmake --build build

# 3. 运行主程序
./build/appSeriona
```

### QML 语言服务 (LSP) 支持
- 项目根目录下包含 `.qmlls.ini` 配置文件，其中硬编码了 `buildDir`、`docDir` 及 `importPaths`。
- **重要**: `qmlls` (QML LSP) 的代码补全和类型诊断功能**必须在执行过一次成功的 CMake 构建后**才能正常工作。如果编辑器报错找不到 `Seriona` 模块，请先运行 `cmake -B build`。

## 4. 前后端交互规范 (C++ / QML)
- **当前状态**: 前端完全由 QML 模拟数据（Mock）驱动，C++ 后端类处于待开发阶段。
- **规范要求 (现代 Qt 6)**:
  - **切勿** 在 C++ 代码中调用废弃的 `setContextProperty` 方法注册类实例。
  - **必须** 在 C++ 头文件中使用 `QML_ELEMENT`（或 `QML_SINGLETON` 等）宏，在 `CMakeLists.txt` 的 `qt_add_qml_module` 下通过自动暴露机制将 C++ 后端类直接注册进 `Seriona` 命名空间。

## 5. 易犯错误与避坑指南
- **切勿在 `build/` 下编辑 QML**: 
  - CMake 在构建时会将 `qml/` 目录下的源文件同步复制至 `build/` 相关的临时目录（如 `build/Seriona/qml/`）。
  - **所有开发、修改、维护必须在根目录的 `qml/` 目录下进行**。在 `build/` 目录下做的任何修改都将在下一次构建时被无情覆盖。
- **图形效果依赖问题 (`Qt5Compat`)**:
  - 项目中的 SVG 图标着色和阴影效果等，已全面迁移或重构为使用 `import Qt5Compat.GraphicalEffects`（如 `ColorOverlay` 和 `OpacityMask`）。
  - **不要混入旧版或实验性的 `QtQuick.Effects`**，避免因缺少模块而引起编译或运行时错误。
- **图标资源路径引用**:
  - 所有 SVG 图标均被打包在 Qt 虚拟资源中。
  - 在 QML 中引用图标时，其绝对路径格式为：`"qrc:/qt/qml/Seriona/qml/assets/xxx.svg"`。
- **虚拟键盘 (`qtvirtualkeyboard`)**:
  - `main.cpp` 在初始化时强制设置了环境变量 `QT_IM_MODULE=qtvirtualkeyboard`。
  - 在部分 Linux 精简环境或未安装 `qtvirtualkeyboard` 插件的环境中运行 `./build/appSeriona` 可能会警告或崩溃。如果仅需在开发时本地调试非虚拟键盘逻辑，可临时注释此行。
