# Seriona

<div align="center">

**专为本地音乐爱好者打造的现代化桌面音乐播放器**

[![Qt](https://img.shields.io/badge/Qt-6.8%2B-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](./LICENSE)

[简介](#-简介) • [界面预览](#-界面预览) • [特性](#-特性一览) • [快速开始](#-快速开始) • [Windows 打包](#-windows-打包) • [测试](#-测试) • [架构简述](#-架构简述) • [许可证](#-许可证)

</div>

---

## 📖 简介

**Seriona** 是一款基于 **Qt Quick (QML) + C++23** 构建的现代化本地音乐播放器前端。

界面主打沉浸与流畅：无边框磨砂窗口、基于专辑封面实时取色的动态流光背景、可交互的音频波形进度条、精准对齐的双语滚动歌词，以及深入多层目录依然能完美保留滚动位置的页面栈式侧栏导航。

项目采用**前后端解耦**架构，前端专注交互与视觉呈现，底层扫描与播放由 [Seriona_Backend](https://github.com/kaizen857/Seriona_Backend) 驱动，同时原生支持无后端的独立 **Mock 模式**，便于快速进行 UI 开发。

---

## 🖼️ 界面预览

<div align="center">

| 🎵 主播放界面 | 📂 侧栏多级曲库 | 📜 同步双语歌词 |
| :---: | :---: | :---: |
| <img src="./img/mainWindow.png" alt="主播放界面" width="300px" /> | <img src="./img/musicLists.png" alt="曲库列表与侧栏浏览" width="300px" /> | <img src="./img/Lyrics.png" alt="同步滚动歌词" width="300px" /> |

</div>

---

## ✨ 特性一览

- 🎨 **沉浸视觉**：自适应专辑封面 3 色动态流动背景、无边框原生手感窗口拖拽与缩放。
- 📊 **波形进度条**：基于真实音频能量渲染的可视化进度条，支持毫秒级精准拖拽跳转。
- 📂 **平滑目录浏览**：逐级深入文件夹浏览，各层级独立记忆滚动位置，切页丝滑过渡。
- 🔍 **即时搜索与排序**：支持当前子树范围即时搜索，支持为不同文件夹定制专属排序偏好。
- 🎵 **双语滚动歌词**：时间戳精确同步滚动，支持中外文分行与翻译一键显隐。
- 🔀 **解耦双游标**：正在播放的曲目与正在翻找浏览的焦点互不干扰。
- 🛠️ **独立 Mock 模式**：无需配置后端依赖即可秒级启动，全功能体验 UI 交互。

---

## 🚀 快速开始

### 依赖要求
- 支持 **C++23** 的编译器（GCC 13+ / Clang 17+ / MSVC 2022+）
- **Qt 6.8+**（需包含 Quick、Concurrent、QuickDialogs2、Widgets 模块）
- **CMake ≥ 3.20**（Windows 打包使用 3.20+）

### 构建与运行

```bash
# 克隆代码
git clone https://github.com/kaizen857/Seriona.git
cd Seriona

# 编译并运行（默认自动关联同级目录的 ../Seriona_Backend）
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/appSeriona
```

> **💡 提示：纯前端 Mock 模式启动**  
> 如果暂时不想配置后端依赖，只需执行：  
> `cmake -B build -DSERIONA_BACKEND_SOURCE_DIR="" && cmake --build build`  
> 即可启动纯前端界面进行预览与调试。

### Windows 打包

Windows x64 的完整构建、测试、Qt 部署和 ZIP 打包由 `build.bat` 统一完成。它不会自动安装软件；首次运行前请准备：

- Visual Studio 2022，勾选“使用 C++ 的桌面开发”和 Windows 10/11 SDK。脚本只接受 VS 2022 的 MSVC x64 工具链。
- Qt 6.8 或更高版本的 `msvc2022_64` kit，并包含 Quick、Concurrent、QuickDialogs2、Widgets 模块。
- CMake 3.20+、Git、Python 3，以及已 bootstrap 的 vcpkg。Python 3 必须可通过 `python.exe` 或 `py.exe -3` 发现；可将 vcpkg 根目录加入 `VCPKG_ROOT`，或使用 `-VcpkgRoot` 指定。
- 可访问网络：首次运行 vcpkg manifest restore 和后端的 FetchContent 可能下载依赖。脚本不会安装 Visual Studio、Qt、CMake、Git 或 VC++ Redistributable。

在仓库根目录执行，或直接双击 `build.bat`：

```bat
build.bat
```

脚本会优先使用 `Qt6_ROOT`、`Qt6_DIR`、`QTDIR`、`VCPKG_ROOT` 等环境变量，也支持显式路径：

```powershell
.\scripts\build-package-windows.ps1 -QtRoot C:\Qt\6.10.1\msvc2022_64 -VcpkgRoot C:\vcpkg -Force
```

常用参数：`-Force` 替换已有 `dist` 包，`-KeepBuild` 保留构建和暂存目录，`-SkipTests` 跳过 CTest 但仍运行包 Smoke，`-Clean` 清理本工作流的构建目录，`-DryRun` 只检查工具并打印计划，`-BackendSourceDir` 和 `-TagReaderSourceDir` 指定本地源码。

默认会选用同级目录 `..\Seriona_Backend` 和 `..\TagReader`；找到时构建输出会记录“本地源码”，找不到时保留 CMake 既有的 FetchContent 行为。传入空的 `-BackendSourceDir ''` 可构建 mock-only 包，但该包没有真实扫描和播放能力。

成功产物位于 `dist\Seriona-windows-x64\` 和 `dist\Seriona-windows-x64.zip`，构建日志位于 `dist\logs\`。构建目录、manifest 安装和 vcpkg 中间状态分别位于 `dist\.build\`、`dist\.vcpkg_installed\` 和 `dist\.vcpkg\`；`-VcpkgInstalledDir` 可覆盖 manifest 安装位置。包内包含 `BUILD-INFO.txt`、`README.txt` 和 `LICENSE`。发布前会在只允许包目录与 `System32` 的 PATH 下运行 `startup`、`main-playback`、`lyrics`、`sidebar-tree`、`settings-menu`、`empty-library` 六个 Smoke 场景，并清理 Smoke 生成的 `SerionaData`。

**Windows 故障排查**

- 找不到工具链：确认 VS 2022 C++ 工作负载、`msvc2022_64` Qt kit、CMake 和 Python 3 均已安装；用 `-QtRoot`、`-VcpkgRoot` 显式指定对应路径。Python 3 需可通过 `python.exe` 或 `py.exe -3` 启动。
- manifest restore 失败：先确认网络和 vcpkg 已执行 `bootstrap-vcpkg.bat`，不要使用静态 triplet；删除失败的 `dist\.vcpkg_installed` 后重新运行即可。
- 配置阶段找不到 FFmpeg 或 `pkg-config` 模块：不要混用 Debug 的 `.pc` 文件，脚本会固定使用 `dist\.vcpkg_installed\x64-windows\lib\pkgconfig`；检查 `dist\logs` 中的 restore/configure 日志。
- Smoke 失败：保留 `-KeepBuild` 重新运行，查看 `dist\logs` 下对应场景和 `07-package-verify` 日志。包应整体移动，不能只复制 EXE。

---

## 🧪 测试

```bash
# 运行单元测试
ctest --test-dir build --output-on-failure

# 运行自动化无头场景冒烟测试
QT_QPA_PLATFORM=offscreen ./build/appSeriona --smoke-scenario=main-playback --smoke-exit-ms=1000
```

---

## 🏛️ 架构简述

前端严格遵循**命令-快照单向数据流**设计，由单一门面中介者 `AppFacade` 统一调度：

```mermaid
flowchart LR
    QML["🎨 QML 界面层"] -->|提交控制命令| Facade["🧩 AppFacade"]
    Facade -->|转发指令| Bridge["🌉 BackendBridge"]
    Bridge -->|执行| Backend["⚡ Seriona_Backend"]
    Backend -.->|广播权威状态快照| Bridge
    Bridge -.->|响应式刷新| Facade
    Facade -.->|驱动重绘| QML
```

- **单向不可变数据流**：UI 仅表达用户意图并派发强类型指令（Command），界面的改变完全由后端广播的权威快照（Snapshot）驱动，杜绝并发状态不一致。
- **单一中介者门面**：`AppFacade` 是 QML 层可见的唯一 C++ 组合根，统一收敛播放、曲库、歌词等子控制器。
- **双游标解耦**：播放游标（`playingTrackId`）与浏览焦点（`selectedBrowserNodeId`）独立，翻找曲库不影响正在播放的曲目。

---

## 🔗 相关项目

- **[Seriona_Backend](https://github.com/kaizen857/Seriona_Backend)**：专为 Seriona 打造的 C++23 音频扫描、元数据缓存与播放核心。
- **[TagReader](https://github.com/kaizen857/TagReader)**：高性能 C++23 音频标签解析与内嵌封面提取库。

---

## 📄 许可证

本项目基于 [GPL-3.0](./LICENSE) 许可证开源。
