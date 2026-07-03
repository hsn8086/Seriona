# 后端整合推荐方案

## 目标

Seriona 前端应继续作为 Qt Quick 应用工程维护，但最终产物应在同一个应用进程里链接 Seriona 后端 C++ 库。QML 不直接碰后端内部对象，而是通过前端仓库内的 Qt/C++ 适配层读写播放状态、音乐库模型和控制命令。

后端 `Seriona_Backend` 和外部库 `TagReader` 仍然是独立项目，不能把它们的源码复制进当前仓库。前端只应该以“外部源码依赖或已安装 CMake 包”的形式消费后端。

## 当前事实

- 当前前端目标是 `appSeriona`，`CMakeLists.txt` 只链接 `Qt6::Quick`，通过 `engine.loadFromModule("Seriona", "Main")` 加载 QML。
- 当前前端已有 QML 可见中间层：`AppFacade`、`PlaybackController`、`NavigationController`、`LibraryController`、`LibraryModel`。这些类目前仍是 mock 或 future hook，尚未持有真实后端。
- 后端已经拆成静态库目标：`seriona_audio`、`seriona_scanner`、`seriona_metadata`、`seriona_control`。前端最合适的链接边界是 `seriona_control`。
- 后端公开控制边界主要是 `seriona::control::MediaController` 和 `inc/seriona/control/control_contracts.h`。后端文档 `docs/frontend-media-control-integration.zh.md` 已明确前端不应绕过 `MediaController` 去碰 audio/scanner/metadata 内部服务。
- 少数后端工具函数可以作为独立前端能力直接调用，例如 `seriona::audio::buildAudioWaveform(...)`。这类函数必须是公开头暴露、无控制状态机副作用、语义独立于 `MediaController` 的纯计算或工具型 API。
- `TagReader` 已是独立静态库 `TagReaderCore`，并由后端 `seriona_scanner` 消费。前端不应直接依赖 `TagReader`。

## 推荐结论

采用“两层依赖策略”。

第一层是开发期外部 checkout。前端 CMake 通过一个 cache path 指向后端仓库，但项目文档和预设不能写死某台机器的绝对路径。推荐默认使用相对路径，例如前端、后端、TagReader 作为同级 checkout 时使用 `../Seriona_Backend`；确实需要本机自定义时，再通过 `-DSERIONA_BACKEND_SOURCE_DIR=...` 或本地 `CMakeUserPresets.json` 覆盖。这样后端可以继续在自己的仓库里开发、提交、测试，前端只消费当前 checkout 的 CMake targets。

第二层是可复现版本锁定。进入稳定对接后，用 Git submodule 或 FetchContent 的固定 commit/tag 锁定 `Seriona_Backend`；`Seriona_Backend` 再锁定或定位 `TagReader`。这样前端仓库记录“用哪个后端版本构建”，但不拥有后端源码。

不要使用源码复制、手动同步目录、或把后端文件加入前端 `src/` 的方式。这会让 bugfix、性能优化和契约变更在三个仓库之间分叉。

## CMake 接入策略

### 当前最优路径

前端仓库新增一个可选 CMake 路径变量来启用真实后端。默认值应是可跨机器的相对路径，而不是个人机器上的绝对路径：

```cmake
set(SERIONA_BACKEND_SOURCE_DIR "../Seriona_Backend" CACHE PATH "Path to Seriona_Backend checkout")
```

当该路径非空时，前端用 `add_subdirectory(${SERIONA_BACKEND_SOURCE_DIR} ${CMAKE_BINARY_DIR}/seriona_backend EXCLUDE_FROM_ALL)` 引入后端，并让 `appSeriona` 链接 `seriona_control`。

如果开发者本机仓库布局不同，不要修改已提交的 `CMakeLists.txt`；应使用命令行 cache 参数或不提交的 `CMakeUserPresets.json` 覆盖路径。

这只是方向，不应直接照抄成最终代码。原因是后端当前 `CMakeLists.txt` 仍会无条件 `add_subdirectory(app)`，且 `TagReader` 的路径候选也写在后端 CMake 内部。真正落地前，应先在后端仓库补齐“被前端消费”的 CMake 入口。

### 后端应先补的消费面

后端仓库建议先做一个小改造，避免前端为了消费库目标被迫构建后端 CLI：

1. 增加 `SERIONA_BUILD_APP` 选项，默认 `ON`，被前端作为子项目引入时可设为 `OFF`。
2. 增加可覆盖的 `SERIONA_TAGREADER_SOURCE_DIR` cache path，避免只能靠固定 sibling 路径查找 `TagReader`。
3. 保留现有 `seriona_audio`、`seriona_scanner`、`seriona_metadata`、`seriona_control` 目标名，或增加稳定 alias，例如 `SerionaBackend::control`。
4. 后续再考虑 `install(EXPORT ...)` 和 CMake package config；当前阶段不必一开始就做安装包。

有了这些后，前端接入可以保持很小：只新增后端路径选项、引入后端子目录、链接控制库、编译前端自己的 Qt adapter。

## 前端适配层边界

前端应保留现有 QML-facing 类名和导入方式，让 QML 继续 `import Seriona`，不要让 QML 直接看到后端 C++ 类型。

建议新增或重构为以下职责：

- `AppFacade` 拥有真实后端运行时对象，负责创建、启动、关闭 `MediaController`。
- `PlaybackController` 把 `PlayerStateSnapshot` 映射成 QML 属性，把 play/pause/seek/volume/repeat/shuffle 等 UI 操作转换成 `MediaControlCommand`。
- `LibraryController` 和 `LibraryModel` 把 `LibraryStateSnapshot.libraryTree` 映射成 QML 可读模型，选曲时使用后端返回的 `TrackIdentity`，不要自行拼 ID。
- 通知、错误条、toast 来源于 `MediaControllerCommandResult` 和 `ControlDomainNotification`。
- 后端订阅回调可能来自非 Qt 主线程，进入 Qt 对象前必须通过 queued signal、`QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 或等价机制切回 Qt 主线程。

前端不应直接持有或调用 `AudioPlaybackService`、`FileScannerService`、`MetadataSharingService`、`audioEventSink()`、`scannerEventSink()`。这些属于后端装配细节。

前端可以直接调用后端明确公开的工具型 API。当前已知例外是 `seriona::audio::buildAudioWaveform(...)`：它服务于波形条绘制，输入是文件路径、条数、宽高和时间窗，输出是高度数组，不参与播放控制状态机。

## 生命周期

应用启动顺序建议如下：

1. `AppFacade` 解析运行时路径，包括数据库路径、封面目录和用户音乐库根目录。
2. 创建 `MediaController`，优先使用后端 `makeProductionMediaController(...)`。
3. 注册播放器状态、音乐库状态和领域通知订阅。
4. 调用 `start()`。
5. 根据用户动作或启动配置调用 `scanLibrary(...)`。
6. QML 全程只读 Qt adapter 暴露的属性和模型。

应用退出顺序建议如下：

1. 提交一次 `Stop` 命令。
2. 调用 `shutdown()`。
3. 调用并释放所有 `SubscriptionHandle::unsubscribe`。
4. 再销毁 Qt adapter 和 `MediaController`。

## TagReader 所属边界

`TagReader` 应继续只属于后端依赖图。前端不应该链接 `TagReaderCore`，也不应该从 QML 或前端 adapter 直接读取标签。

原因：

- 后端 scanner 已经把 `TagReader::Read(...)` 和 `ReadCueSheet(...)` 映射到 `SongMetadata`、`PlaylistTreeSnapshot` 和控制层快照。
- `TagReader` 有封面导出和缓存副作用，应该由后端统一选择私有 `coverExportDir`。
- `ReadCueSheet()` 的空结果可能代表失败或无可用曲目，应该由后端适配层统一映射为 scanner/control 错误语义，而不是让 UI 判断。

## 版本与分支工作流

开发期推荐使用 sibling checkout：

```text
workspace/
  Seriona/
  Seriona_Backend/
  TagReader/
```

前端通过 CMake cache path 指向后端。后端通过自己的 CMake 继续定位或接收 `TagReader` 路径。这样三个仓库都能独立提交和回滚。

提交到仓库的配置只应包含相对路径或可覆盖变量。个人机器上的绝对路径只允许存在于本地命令行、shell alias、IDE 配置或未提交的 `CMakeUserPresets.json` 中。

稳定对接期推荐锁定版本：

- 前端记录后端 commit，例如 Git submodule 或 FetchContent `GIT_TAG`。
- 后端记录 TagReader commit，避免前端间接消费到未验证的 TagReader 修改。
- 每次提升后端版本时，在前端开一个单独 PR/commit，只做“更新后端版本 + 适配必要契约变化”。

不推荐前端直接跟踪后端默认分支自动构建。后端仍在修改时，自动追随默认分支会让前端构建变成不可复现状态。

## 实施步骤

建议按这个顺序推进：

1. 在后端仓库先补 CMake 消费面：`SERIONA_BUILD_APP`、可覆盖的 `SERIONA_TAGREADER_SOURCE_DIR`、稳定库 target 或 alias。
2. 在前端仓库新增 `SERIONA_BACKEND_SOURCE_DIR`，只在该路径存在时启用真实后端。
3. 新建前端 Qt adapter，先只接 `PlayerStateSnapshot`、`MediaControlCommand` 和基础生命周期。
4. 把 `PlaybackController` 的 mock 状态改成订阅驱动。
5. 把 `LibraryModel` 的 mock 条目改成 `LibraryStateSnapshot.libraryTree` 映射。
6. 接入通知和错误展示。
7. 最后再接 waveform、歌词、搜索、定位当前歌曲等增强能力。

每一步都应该保持应用可构建、可运行，不要一次性替换所有 mock。

## 验证计划

后端独立验证：

```bash
cmake -B build
cmake --build build
```

TagReader 独立验证：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

前端整合验证：

```bash
cmake -B build -DSERIONA_BACKEND_SOURCE_DIR=../Seriona_Backend
cmake --build build
./build/appSeriona
```

后端目前测试目标较多，但需要确认哪些已被 CTest 注册。对接阶段不要把“能构建测试目标”等同于“CTest 已覆盖全部目标”。

## 风险与处理

- 后端 API 仍在变化：把 `MediaController` 和契约头当作媒体控制主边界；工具型公开 API 只允许逐个白名单接入，例如波形生成。
- CMake 子项目污染：后端应提供关闭 CLI 和测试的选项，前端引入时只构建需要的库。
- 私有仓库访问：开发期用本地路径，稳定期用 submodule 或固定 commit，避免构建时隐式拉取不可访问仓库。
- 跨机器路径差异：已提交配置不能包含个人绝对路径；用相对 checkout 布局和本地 cache 覆盖解决。
- 线程边界：后端订阅回调进入 Qt 前必须切主线程。
- TagReader 失败语义偏宽松：前端只读后端已经整理过的 scanner/control 状态，不直接解释 TagReader 异常或空结果。
- 构建不可复现：不要让前端直接消费后端默认分支；每次后端版本提升都要显式记录 commit 并跑整合验证。

## 最终推荐

最优方案是：前端仓库保留 Qt/QML app 形态，后端和 TagReader 保持独立仓库；开发期用可覆盖的 CMake cache path 指向相对位置的后端 checkout，稳定期用 pinned submodule 或 fixed Git revision 锁定后端版本。前端新增 Qt adapter 层消费 `MediaController`，并对白名单内的后端工具函数直接调用；不要复制后端源码，也不要让 QML 或前端 adapter 直接依赖 TagReader。
