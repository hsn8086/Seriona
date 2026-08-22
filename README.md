# Seriona 前端

Seriona 是一个本地优先（local-first）的桌面音乐播放器。本仓库是它的 **Qt Quick 前端**：负责全部界面与交互，不直接实现文件系统扫描、音频解码、数据库等能力，而是通过命令 / 快照边界与独立仓库的 C++23 后端 [`Seriona_Backend`](https://github.com/kaizen857/Seriona_Backend) 协作——前端提交命令（播放、扫描、排序等），后端回推状态快照（播放状态、曲库树、扫描进度、通知）作为 UI 的唯一事实来源。

---

## 功能特性

以下功能均以当前源码为准（`CMakeLists.txt`、`src/`、`qml/`）：

| 功能 | 说明 |
|------|------|
| 播放控制 | 播放 / 暂停 / 切换播放状态、上一首 / 下一首、拖拽进度跳转（seek）、音量调节（钳制 0–1）、静音、随机播放（shuffle）开关、循环模式（关 / 列表循环 / 单曲循环） |
| 曲库扫描 | 通过 `scanLibrary()` 提交扫描命令；实时进度（`scannedSongCount / totalSongCount` 计数与 0–100 进度）、扫描状态机（pending / running / error / completed）与扫描进度提示 |
| 启动恢复 | 启动页可一键恢复上次曲库根（路径持久化在 `QSettings`，键 `library/lastScanRoot`），也可手动添加文件夹后扫描；根路径失效时给出错误提示 |
| 侧栏文件夹导航 | 侧栏以多级文件夹页栈浏览曲库（进入文件夹 / 返回，`folderStackDepth` 记录层级）；每一级文件夹拥有独立的投影模型与视图，浏览过程中滚动位置天然保留；进入 / 返回带滑动动画 |
| 搜索 | 侧栏搜索框，按当前文件夹子树范围搜索，对歌曲标题 / 歌手 / 专辑 / 文件名做加权评分排序；文件夹条目不参与搜索结果；清空搜索恢复原排序 |
| 播放队列 | 临时播放队列视图，每项显示封面缩略图（无封面时回退占位）；支持"添加到下一首播放"与"从队列移除" |
| 同步滚动歌词 | 歌词列表按时间戳与播放进度同步（`currentIndex` 驱动），支持显示行 / 翻译行分隔（分隔符可配置）、翻译显隐切换、切页滑动动画 |
| 每文件夹排序规则 | 排序对话框支持为每个文件夹配置最多 5 条排序规则（字段：歌曲名 / 歌手名 / 专辑名，升序 / 降序），按"曲库根 + 文件夹"保存并经后端持久化；搜索期间按相关性评分临时排序、不覆盖已存规则 |
| 动态背景 | 从当前封面缩略图提取 3 色（后台线程），驱动三对角渐变背景平滑过渡 |
| 无边框窗口 | 无边框单窗口（`Qt.FramelessWindowHint`），自定义标题栏，支持 `startSystemMove` 拖拽移动与八向 `startSystemResize` 缩放，最小化 / 最大化 / 关闭按钮 |
| 通知 | 有界通知队列（容量 12，自动丢弃最旧），界面以 toast 形式展示播放错误、扫描错误、命令被拒、不支持的操作等消息（自动隐藏） |
| 波形进度条 | 播放进度区以波形条呈现（波形由后端异步生成、前端线程池请求），支持拖拽跳转；歌词视图退化为线性滑杆 |
| 附加窗口 | 设置窗口（音频输出设备枚举、采样率 / 位深、日志等级等）、曲目详情窗口（年份、播放次数、星级）、均衡器窗口 |
| 歌曲统计 | 播放次数自增与星级评分的本地持久化（`QSettings`） |
| 关于 / 菜单 | 气泡式设置菜单（含真实可用的歌词分隔符设置、关于弹层、退出）、曲目右键菜单（详情 / 下一首播放 / 删除，删除需二次确认） |

> 说明：均衡器、淡入淡出、无缝播放、回放增益等设置项暂未接入后端能力，界面上统一走"暂未支持"的本地通知反馈，不会伪造后端命令。

---

## 架构概览

```
┌────────────────────────── QML 视图层（qml/）──────────────────────────┐
│  Main.qml（无边框窗口外壳，实例化唯一的 AppFacade）                     │
│  ├─ views/：StartupView（启动页）、MainContent（播放 / 歌词双态）       │
│  ├─ components/：Sidebar、QueueView、SortDialog、DynamicBackground、   │
│  │                WaveformProgressBar、BubbleMenu、TrackContextMenu…   │
│  ├─ windows/：SettingsWindow、TrackDetailWindow、EqualizerWindow       │
│  └─ theme/Theme.qml（singleton 设计 token）                             │
└──────────────┬──────────────────────────────────────────────────────────┘
               │ 读写 appFacade.playback / library / lyrics /
               │ notifications / navigation / settings / trackStats
┌──────────────▼────────────── C++ 应用层（src/app/）───────────────────┐
│  AppFacade（唯一组合根，持有全部控制器）                                │
│  ├─ PlaybackController       播放命令 + 曲目视图状态 + 时间轴平滑       │
│  ├─ LibraryController        曲库树、多级文件夹投影、双游标、排序、扫描  │
│  ├─ LyricsModel              歌词模型（分隔符切分、时间同步）           │
│  ├─ NotificationController   有界通知队列 + 不支持项本地反馈            │
│  ├─ NavigationController     视图 / 侧栏 / 启动屏状态 + 根路径持久化    │
│  ├─ SettingsController / TrackStatsController                          │
│  └─ WaveformProvider / ArtworkPaletteWorker（异步波形、封面取色）       │
└──────────────┬──────────────────────────────────────────────────────────┘
               │ BackendBridge（命令 / 快照边界）
┌──────────────▼────────────── 后端（Seriona_Backend，可选）────────────┐
│  MediaController：播放、扫描、元数据、通知                              │
│  推送 PlayerStateSnapshot / LibraryStateSnapshot / 域通知              │
└─────────────────────────────────────────────────────────────────────────┘
```

- **AppFacade**（`src/app/app_facade.{h,cpp}`，`QML_ELEMENT`）是唯一的组合根，按值持有 `PlaybackController`、`LibraryController`、`LyricsModel`、`NotificationController`、`NavigationController`、`SettingsController`、`TrackStatsController`，并持有 `BackendBridge`（命令 / 快照边界）；接入后端时还持有 `WaveformProvider`。`qml/Main.qml` 实例化唯一的 `AppFacade`，QML 只经它访问各控制器。
- **BackendBridge**（`src/app/backend_bridge.{h,cpp}`）是前后端唯一的边界：命令在 GUI 线程同步提交，后端回调线程的快照 / 通知经 `Qt::QueuedConnection` 投递回 GUI 线程应用；后端不可达或被拒的命令返回 `CommandRejected` 域通知，前端不会伪造成功。
- **中间层约束**：前端不直接实现文件系统 / 网络 / 数据库访问，一切后端能力都经 `BackendBridge` 的命令 / 快照边界（由 `scripts/verify-middle-layer.sh` 强制检查）。
- **mock-only 模式**：当后端不可用时（`SERIONA_BACKEND_SOURCE_DIR` 置空），前端以 mock-only 模式构建——UI 可完整构建运行（播放控制等 setter 为空操作，不伪造后端命令），用于界面开发与纯 QML 面测试。编译宏 `SERIONA_HAS_BACKEND` 由 CMake 自动推导，无需手动设置。
- **双游标**：播放身份（`playingTrackId`）与浏览焦点（`selectedBrowserNodeId`）相互独立；"定位当前歌曲"只移动浏览位置，不发播放命令。

### 状态流向（后端 → UI）

后端快照经 `BackendBridge` 的信号（`playerSnapshotChanged` / `librarySnapshotChanged` / 域通知）进入 `AppFacade` 后分发给各控制器：播放状态与时间轴进 `PlaybackController`（含 100ms 时间轴平滑插值、封面取色与波形请求）、曲库树进 `LibraryController`（含扫描状态 / 进度、排序规则回填）、当前曲目匹配进 `LyricsModel`；域通知进 `NotificationController` 入队展示。快照是 UI 状态的唯一事实来源，控制器 `apply*` 路径之外的直接写入只用于提交用户意图。

### 命令流（UI → 后端）

QML 控件调用控制器公开方法（如 `PlaybackController.play()/seek()/cycleRepeatMode()`、`LibraryController.playItem()/scanLibrary()/applySortRules()`），控制器构造类型化命令经 `AppFacade` 注入的执行器（即 `BackendBridge.submitCommand`）提交给后端的 `MediaController`；被拒的命令由 `BackendBridge` 转成 `CommandRejected` 域通知回流展示，前端不伪造成功。

---

## 构建要求

| 项 | 要求 |
|----|------|
| 语言 | C++（构建产物以 C++23 标准产出） |
| 框架 | Qt 6.8+（`qt_standard_project_setup(REQUIRES 6.8)`） |
| Qt 模块 | Quick、Concurrent、QuickDialogs2、Widgets（测试另需 Quick Test） |
| 构建系统 | CMake ≥ 3.16 |
| 后端 | 可选，见[后端集成](#后端集成)；缺省时前端自动尝试同级目录 |

标准构建顺序（在仓库根目录）：

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

运行应用：

```bash
./build/appSeriona
```

其他说明：

- `BUILD_TESTING` 默认为 `ON`，构建测试目标。
- `Release` 构建类型下按编译器启用全量优化（GCC/Clang 的 `-O3 -march=native` + LTO，MSVC 的 `/O2 /GL /LTCG`）。
- 接入后端时额外要求 `find_package(spdlog CONFIG REQUIRED)`，终端日志统一由 spdlog 输出（Qt 消息与 QML `console.log` 重定向到默认 logger）。

---

## 后端集成

前端本身不实现扫描、解码、数据库，需要与后端仓库 [`Seriona_Backend`](https://github.com/kaizen857/Seriona_Backend) 配合才有真实媒体能力。后端集成完全由 CMake 缓存变量 `SERIONA_BACKEND_SOURCE_DIR` 决定：

| `SERIONA_BACKEND_SOURCE_DIR` | 行为 |
|------------------------------|------|
| 默认值 `../Seriona_Backend` | 若该相对路径（相对仓库根目录）存在有效的后端源码，则作为 CMake 子树直接引入，并链接后端的 control / audio / app 三个目标 |
| 非空但路径无效 | 从 `https://github.com/kaizen857/Seriona_Backend.git` 的 `main` 分支 FetchContent 拉取（`GIT_SHALLOW`），抓取失败会导致配置失败，不会自动回退 |
| 空字符串 `""` | 强制 **mock-only 模式**：不引入后端，UI 以无真实媒体能力的方式构建运行 |

示例（显式指定本地后端路径）：

```bash
cmake -B build -DSERIONA_BACKEND_SOURCE_DIR=/path/to/Seriona_Backend
```

其他要点：

- 嵌入后端子树时，CMake 会自动关闭后端自身的独立 app 与测试目标，再恢复前端的 `BUILD_TESTING`。
- 接入后端时前端定义 `SERIONA_HAS_BACKEND=1` 并链接 `spdlog::spdlog`；mock-only 构建不要求 spdlog。
- 离线配置时可借助 `SERIONA_FETCHCONTENT_CATCH2_DIR` / `SERIONA_FETCHCONTENT_THREAD_POOL_DIR` 指向已有的后端依赖源码，避免 FetchContent 联网。

---

## 测试与验证

### CTest

```bash
ctest --test-dir build --output-on-failure
```

- 测试源位于 `tests/frontend/adapter/`（`tst_*.cpp`，基于 Qt Test），注册为以 `seriona_frontend_` 开头的 CTest 用例。
- 接入后端时注册完整测试集，覆盖：命令 / 快照映射、桥接线程与关闭时序、曲库树投影与多级文件夹投影、排序规则（含持久化与搜索隔离）、双游标与侧栏本地浏览、选曲上下文、扫描流程、启动恢复、播放快照与命令、通知、歌词、波形、封面渐变、曲目详情与删除链、UI-only 策略等。
- mock-only 构建只注册不依赖后端的纯 QML 面测试（命令 / 快照映射、库树映射、设置控制器、播放次数 / 星级、关于弹层、队列视图），其余测试目标不注册。
- 聚焦单个测试：`ctest --test-dir build -R '<regex>' --output-on-failure`；单个 QTest 用例可直接作为参数传入测试二进制，例如 `./build/seriona_frontend_library_sort_tests <caseName>`。

### 场景 Smoke（CLI 冒烟）

应用内置 smoke 计时器，可在 offscreen 环境自动运行指定场景并定时退出：

```bash
QT_QPA_PLATFORM=offscreen ./build/appSeriona \
  --smoke-scenario=<startup|main-playback|lyrics|sidebar-tree|settings-menu|empty-library> \
  --smoke-exit-ms=1000
```

- `--smoke-exit-ms` 为退出毫秒数（默认 1000）；退出码 0 表示场景运行成功，2 为参数 / 场景非法，3 为日志写入失败。
- 场景列表：`startup`、`main-playback`、`lyrics`、`sidebar-tree`、`settings-menu`、`empty-library`。
- smoke 模式下后端桥不自动启动，并隔离应用设置文件，避免污染用户真实配置。

### 中间层门禁脚本

```bash
./scripts/verify-middle-layer.sh
```

脚本（依赖 `rg`）执行：配置 → 构建 → 中间层不变量断言（前端源码不得出现直接文件系统 / 网络 / 数据库访问痕迹、目标与资源注册完备、契约文档存在）→ offscreen 启动冒烟（`QT_QPA_PLATFORM=offscreen timeout 5s ./build/appSeriona`，期望以超时码 124 退出，表示应用正常启动并持续运行）。可用 `SERIONA_BUILD_DIR` 覆盖构建目录，`SERIONA_BACKEND_SOURCE_DIR` 默认同样为 `../Seriona_Backend`。

---

## 目录结构

```
Seriona/
├── CMakeLists.txt              # 全部构建逻辑（单文件）：目标、QML 模块、测试注册
├── src/
│   ├── main.cpp                # 入口：QApplication、smoke CLI、加载 Seriona/Main
│   ├── app/                    # C++ 应用层（中间层）：AppFacade、控制器、模型、桥接、异步工具
│   │                           # （playback/library/lyrics/notification/navigation/settings/
│   │                           #  trackStats 控制器 + BackendBridge + WaveformProvider 等）
│   └── providers/              # 游离源码（未接入构建，修改不影响应用）
├── qml/
│   ├── Main.qml                # 无边框窗口外壳，实例化唯一 AppFacade
│   ├── views/                  # MainContent（播放 / 歌词）、StartupView（启动页）
│   ├── components/             # Sidebar、QueueView、SortDialog、DynamicBackground、
│   │                           # WaveformProgressBar、BubbleMenu、TrackContextMenu 等
│   ├── windows/                # SettingsWindow、TrackDetailWindow、EqualizerWindow
│   ├── theme/Theme.qml         # singleton 设计 token（颜色 / 尺寸 / 动画参数）
│   └── assets/                 # SVG 图标资源
├── tests/
│   └── frontend/adapter/       # Qt Test 测试源（tst_*.cpp）
├── scripts/
│   └── verify-middle-layer.sh  # 中间层门禁脚本（配置 + 构建 + 不变量断言 + offscreen 冒烟）
├── docs/                       # 架构契约与设计文档（backend-integration-contract.md 为现行契约）
├── DESIGN.md                   # 项目整体设计说明
└── LICENSE                     # GPL-3.0
```

---

## 相关项目

| 仓库 | 说明 |
|------|------|
| [Seriona_Backend](https://github.com/kaizen857/Seriona_Backend) | C++23 后端：播放、扫描、元数据、通知；前端经 `SERIONA_BACKEND_SOURCE_DIR` 嵌入（见[后端集成](#后端集成)） |

---

## 许可证

[GPL-3.0](./LICENSE)（GNU General Public License v3）。
