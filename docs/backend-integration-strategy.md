# 后端整合说明

## 现状

Seriona 现在是单可执行 Qt Quick 应用，入口是 `src/main.cpp`，通过 `engine.loadFromModule("Seriona", "Main")` 加载 QML。前端不是直接摸后端内部对象，而是通过 `AppFacade`、`PlaybackController`、`LibraryController`、`LibraryModel`、`NotificationController` 这层 Qt/C++ seam 读写状态。

## 真实 CMake 选项

前端根 `CMakeLists.txt` 里真实存在的后端开关只有一个：

```cmake
set(SERIONA_BACKEND_SOURCE_DIR "../../cppProject(app_and_lib)/Seriona_Backend" CACHE PATH "Seriona backend source tree")
```

它是相对 `Seriona` 仓库根目录解析的，不要在已提交文档里写个人绝对路径。当前逻辑是：

- 为空时，`appSeriona` 走 mock-only mode
- 路径存在且有 `CMakeLists.txt` 时，前端 `add_subdirectory(...)` 引入后端
- `SERIONA_BUILD_APP` 会被强制设为 `OFF`，避免把后端独立 app 一起拉进来
- 前端最终只链接后端控制面和音频工具面，分别落在 `SERIONA_BACKEND_CONTROL_TARGET` 和 `SERIONA_BACKEND_AUDIO_TARGET`

## 相对路径布局

当前可复现的开发布局是同级 checkout，前端从自己的仓库根目录去找后端 checkout。文档里只写相对形式，不写机器路径。

如果本机布局不同，用命令行覆盖 `SERIONA_BACKEND_SOURCE_DIR`，不要改成写死路径的仓库配置。

## 后端 API 白名单和使用 seam

前端现在只消费这些真实 seam：

- `BackendBridge::start()`, `shutdown()`, `submitCommand()`, `scanLibrary()`
- `BackendBridge::playerSnapshot()`, `librarySnapshot()`, `notifications()`
- `AppFacade` 把 `BackendBridge::playerSnapshotChanged` 和 `librarySnapshotChanged` 投影到 `PlaybackController` / `LibraryController`
- `PlaybackController::setCommandExecutor()` 和 `LibraryController::setCommandExecutor()` 都只接 `BackendBridge::submitCommand()`
- `LibraryController::setScanExecutor()` 只接 `BackendBridge::scanLibrary()`

白名单之外的后端内部服务不应被前端直接碰。已确认的工具型例外只有波形生成这类独立能力，其他控制面仍以 `MediaController` 快照和命令为边界。

## 双游标播放和浏览规则

播放游标和浏览游标是分开的，不能再合成一个状态。

- `playingTrackId` 只表示当前播放曲目
- `selectedBrowserNodeId` 只表示当前浏览焦点
- `setSelectedBrowserNodeId()` 会同步焦点，但不会自动替换播放身份
- `locateCurrentSong()` 只根据 `playingTrackId` 找节点，然后本地改浏览选择和滚动请求，不发播放命令
- `followCurrentlyPlaying` 只影响浏览游标是否跟随播放，不改变后端播放身份
- `setPlaylistTreeSnapshot()` 会保留 `rootNodeId` 优先级，找不到 root 时回退到 snapshot 原始顺序，再找不到就保持空树策略

这条规则的核心是，浏览可以追随播放，但播放不能被浏览反向污染。

## 不支持和 UI only 政策

当前不支持的设置项必须走本地反馈，不得伪造后端命令，也不得静默吞掉。

已确认的本地反馈项包括：`Crossfade`、`Gapless Playback`、`ReplayGain`、`Equalizer`、`About Seriona`，以及 `Sidebar` 里的 `Sort by Name`、`Sort by Date`。

`Exit` 是例外，必须走真实关闭链路，继续触发 `requestApplicationClose()` 和 `AppFacade::shutdown()`。

政策很简单，UI only handler 不准调用 `submitCommand()`，unsupported 项必须走 `NotificationController::showUnsupportedAction()` 或等价本地通知，退出项不能被当成 unsupported。

## 已验证命令

- `cmake -B build-doc-missing -DSERIONA_BACKEND_SOURCE_DIR=../missing`：已成功；配置阶段明确退化为 mock-only mode，并打印找不到后端源码的状态说明。
- `cmake --build build-doc-missing --target appSeriona`：已成功；mock-only 前端目标完成构建。
- `export https_proxy=http://127.0.0.1:7897 http_proxy=http://127.0.0.1:7897 all_proxy=socks5://127.0.0.1:7897 && cmake -B build-doc-check -DSERIONA_BACKEND_SOURCE_DIR="../../cppProject(app_and_lib)/Seriona_Backend"`：已成功；在代理环境下完成后端路径配置和依赖获取。
- `cmake --build build-doc-check --target appSeriona`：已成功；真实后端路径下前端目标完成构建。
- `git diff --check`：已通过，没有空白或补丁格式错误。

当前 T21 记录的验证状态均为成功；其中真实后端路径配置依赖代理启用后的网络环境，mock-only 路径仍按预期退化并可构建。
