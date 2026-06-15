# Seriona 项目指南 (AGENTS.md)

本文件旨在为 OpenCode 代理提供关于 Seriona 音乐播放器项目的快速上手指南，避免重复探索并减少错误。

## 1. 项目概况
- **类型**: 音乐播放器应用 (QML 前端 + C++ 后端)
- **目标平台**: 优先 Linux (Arch Linux 开发环境), 计划兼容 Windows
- **技术栈**: Qt 6.8+, CMake, QML (Qt Quick)
- **主要执行文件**: `appSeriona`

## 2. 核心架构
- **初始化流**: 
  - 入口: `src/main.cpp`
  - 引擎: `QQmlApplicationEngine`
  - 加载方式: `engine.loadFromModule("Seriona", "Main")` (现代 Qt 6 模块化加载)
  - 环境配置: 自动设置 `QT_IM_MODULE=qtvirtualkeyboard`
- **前后端交互**:
  - 当前状态: 纯 QML 骨架，尚未实现 C++ 后端类。
  - 推荐模式: 使用 `QML_ELEMENT` 宏在 C++ 类中注册，通过 `qt_add_qml_module` 自动暴露给 QML。
- **目录结构**:
  - `src/`: C++ 源代码 (如 `main.cpp`)
  - `qml/`: QML 界面代码
    - `qml/Main.qml`: 主程序入口
    - `qml/views/`: 业务逻辑视图 (如播放界面、搜索界面)
    - `qml/components/`: 通用 UI 组件 (如自定义按钮、进度条)
    - `qml/dialogs/`: 弹出窗口与对话框
    - `qml/theme/`: 样式与主题定义 (建议使用 Singleton)
  - `build/`: 构建产物 (注意：由于历史清理，build 目录可能存有已删除的旧 QML 文件镜像，开发时请以 `qml/` 目录为准)

## 3. 开发命令
### 构建
```bash
# 配置
cmake -B build
# 编译
cmake --build build
```

### 运行
```bash
./build/appSeriona
```

## 4. 关键规范与注意事项
- **语言约束**: **本项目所有文档以及 OpenCode 代理的回答均必须使用中文**。
- **QML 模块**: 模块 URI 为 `Seriona`。导入时使用 `import Seriona`。
- **环境要求**: 需要 Qt 6.8 或更高版本。
- **虚拟键盘**: 项目集成了 `QtQuick.VirtualKeyboard`，默认开启。
- **平台差异**: 目前主要在 Arch Linux 下开发，注意路径分隔符和库依赖。

## 5. 常见错误规避
- **不要直接在 build 目录编辑 QML**: 虽发现 build 目录下有 `LyricsView.qml` 等残留，但源码目录 `qml/` 才是唯一真实源。
- **类型安全**: 避免在 C++ 中使用 `setContextProperty`，优先使用 `QML_ELEMENT`。
- **LSP 支持**: `CMakeLists.txt` 已配置 `QT_QML_GENERATE_QMLLS_INI` 以支持 QML LSP (`qmlls`)。
