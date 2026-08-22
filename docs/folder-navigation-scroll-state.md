# 文件夹导航滚动位置问题与解决方案

## 文档状态

本文档记录 Sidebar 文件夹导航滚动问题的调研结果和待实施方案。本文档本身不表示相关代码已经修复；实现完成后，需要按照文中的验证清单补充实际运行结果。

## 1. 需求

文件夹浏览需要同时满足以下行为：

- 进入子文件夹时，子文件夹列表从顶部开始显示。
- 进入子文件夹前，父文件夹列表的滚动位置被保存。
- 从子文件夹返回时，父文件夹恢复到进入前的视口位置，而不是回到顶部。
- 返回动画保留逐项滑入效果，并且从返回后视口中的第一项开始逐条错落，而不是固定从模型第 0 项开始。
- 搜索和定位当前播放曲目直接更新列表，不使用文件夹切换动画。

这里有两个独立的滚动状态：

1. 子文件夹的初始状态必须是顶部。
2. 父文件夹的恢复状态必须是进入前的视口锚点。

不能用一个无条件的 `contentY = 0` 或一个通用的 `contentY` 恢复逻辑同时处理这两种状态。

## 2. 已观察到的现象

当前实现曾经出现过以下现象：

- 返回上级目录后仍回到列表顶部。
- 返回动画可以出现，但滚动位置仍然错误。
- 进入子文件夹时，子列表偶尔继承父列表的滚动位置，没有从顶部开始。
- CTest 和 offscreen smoke 均通过，但手动操作仍然失败。

最后一个现象是预期的：现有测试主要覆盖 C++ 模型、控制器和启动 smoke，没有真正驱动 QML `ListView` 的模型变更、布局、delegate 创建和 `contentY` 时序。

## 3. 当前实现的关键时序

相关代码位于：

- `qml/components/Sidebar.qml`：`enterFolder()`、返回按钮、`ListView` 过渡和定位请求。
- `src/app/library_model.cpp`：`enterFolder()`、`goBack()`、`applyBrowsingState()` 和 `setProjectionNodeIds()`。
- `tests/frontend/adapter/tst_library_tree_model.cpp`：模型投影变更测试。
- `tests/frontend/adapter/tst_sidebar_local_browsing.cpp`：Sidebar 本地浏览行为测试。

进入子文件夹的实际顺序是：

1. QML 保存当前文件夹的 `playlistView.contentY`。
2. QML 设置 `folderTransitionDirection = 1`。
3. QML 同步调用 `libraryController.enterFolder(nodeId)`。
4. C++ 更新当前文件夹并同步调用 `applyBrowsingState()`。
5. 模型发出整批 remove/insert 或 reset 通知。
6. C++ 调用返回到 QML 后，QML 立即执行 `playlistView.contentY = 0`。
7. `ListView` 在当前 JavaScript 调用栈返回后，继续在 Qt 的 polish/layout 阶段处理待定 change set 和 delegate。

返回父文件夹的顺序类似：

1. QML 保存当前子文件夹的滚动位置。
2. QML 设置 `folderTransitionDirection = -1`。
3. QML 同步调用 `libraryController.goBack()`。
4. C++ 更新父文件夹并同步重建投影。
5. C++ 调用返回后，QML 立即读取保存值并写入 `playlistView.contentY`。
6. 后续 `ListView` 布局、`contentHeight` 修正或 delegate 处理可能再次修改最终位置。

`folderTransitionResetTimer` 的 300ms 延迟只适合清理视觉动画方向，不能证明模型布局已经完成，也不能作为滚动恢复的同步机制。

## 4. 根因

### 4.1 全量投影替换被当作增量变更

当前 `setProjectionNodeIds()` 通过删除所有旧行、再插入所有新行来替代 `beginResetModel/endResetModel`。

这种实现会让 `ListView` 按增量 change set 处理投影变化。进入子文件夹时，旧的父列表滚动位置可能被保留下来；此时 QML 立即写入 `contentY = 0`，但该值仍可能在下一轮布局中被重新计算或钳制。因此，进入子文件夹不一定从顶部开始。

父文件夹恢复也有同样的问题：同步写入保存值发生在 `ListView` 完成布局之前，后续 `contentHeight` 变化会触发 `fixupY()`，使最终位置与刚写入的值不一致。

文件夹投影是一次完整替换，不是同一列表中的局部新增。模型通知应表达真实语义；滚动恢复则应由 UI 在投影布局完成后显式处理。

### 4.2 裸 `contentY` 不是稳定的视口锚点

`contentY` 只表示当前内容坐标偏移。以下变化都会使裸像素值失去原本含义：

- delegate 高度变化。
- 顶部间距或扫描提示区域高度变化。
- 前置行被插入或删除。
- 模型投影排序发生变化。
- `contentHeight` 尚未稳定，后续触发位置钳制。

更可靠的状态是“首个可见的稳定条目 + 它相对视口顶部的像素偏移”。当前模型已有稳定的 node id，可以用它作为锚点。

### 4.3 `add` 过渡不匹配完整投影替换

Qt Quick `ListView` 的过渡语义是：

- `populate`：初次填充或模型 reset 后填充当前视图。
- `add`：增量插入新条目。
- `remove`：条目被删除。
- `move`：条目在同一模型中移动。
- `displaced`：其他条目因变更被推开。

把“删除全部旧行 + 插入全部新行”配合 `add` 使用，虽然可能产生滑入视觉效果，但不代表它是正确的 ListView 状态模型，也不能解决布局时序问题。

## 5. Qt 时序证据

Qt 6.8/6.9 的相关流程可以概括为：

```text
QQmlDelegateModel modelUpdated
    -> QQuickItemView modelUpdated
        -> 记录 change set
        -> polish
            -> QQuickItemView updatePolish
                -> layout
                    -> applyModelChanges / regenerate
```

因此，模型的 `rowsInserted`、`rowsRemoved` 或 `modelReset` 信号处理完成，不等于 `ListView` 已经完成布局。

重要行为：

- `beginResetModel/endResetModel` 会使 view 重新生成 delegate，并把位置重新放到内容起点。
- 增量 change set 会在布局阶段重新定位可见条目，以避免内容跳动。
- `forceLayout()` 用于立即推进待处理的 view 布局，但调用它之前仍需确保 view 已有有效尺寸和有效模型。
- `contentY` 写入之后，`contentHeight` 变化仍可能通过 Flickable 的位置修正逻辑改变最终值。
- `positionViewAtBeginning()` 和 `positionViewAtIndex()` 比直接计算裸 `contentY` 更适合定位模型条目。

参考资料：

- [ListView.forceLayout()](https://doc.qt.io/qt-6.8/qml-qtquick-listview.html#forceLayout-method)
- [ListView.positionViewAtIndex()](https://doc.qt.io/qt-6.8/qml-qtquick-listview.html#positionViewAtIndex-method)
- [ListView.add](https://doc.qt.io/qt-6.8/qml-qtquick-listview.html#add-prop)
- [ListView.populate](https://doc.qt.io/qt-6.8/qml-qtquick-listview.html#populate-prop)
- [ListView Reusing Items](https://doc.qt.io/qt-6.8/qml-qtquick-listview.html#reusing-items)
- [QQuickItemView modelUpdated()](https://github.com/qt/qtdeclarative/blob/35ba89a42a74ff6668a64a1361fe7208fee554af/src/quick/items/qquickitemview.cpp#L1262-L1297)
- [QQuickItemView updatePolish()](https://github.com/qt/qtdeclarative/blob/855f02c96d3c089ea7c0010ebbe4b29fab9cc1ba/src/quick/items/qquickitemview.cpp#L1501-L1506)
- [QQmlDelegateModel reset 与 change set](https://github.com/qt/qtdeclarative/blob/855f02c96d3c089ea7c0010ebbe4b29fab9cc1ba/src/qmlmodels/qqmldelegatemodel.cpp#L1918-L2037)

## 6. 推荐解决方案

推荐继续使用当前单个 `ListView`，但把导航改为有明确阶段和 revision 的事务。独立 folder page 只有在每个页面持有独立模型快照时才成立；如果页面仍共享同一个可变投影模型，只是把同一个问题复制到多个 view。

### 6.1 模型层

1. `setProjectionNodeIds()` 对完整文件夹投影替换使用 `beginResetModel/endResetModel`，不要用全量 remove/insert 模拟增量变化。
2. 在最终投影更新完成后发出明确的 `projectionChanged` 或 `projectionRevisionChanged` 信号。
3. 信号携带至少：revision、当前 projection/folder id、model count。
4. `setPlaylistTreeSnapshot()` 等异步快照更新也必须增加 revision，避免 QML 把旧导航事务的恢复请求应用到新投影上。

### 6.2 QML 保存状态

进入子文件夹之前保存一个状态对象：

```text
folderId
projectionRevision
firstVisibleNodeId
firstVisibleIndex
visibleOffset
```

其中：

- `firstVisibleNodeId` 是首个可见 delegate 对应的稳定 node id。
- `firstVisibleIndex` 只是 node id 找不到时的回退值。
- `visibleOffset = delegate.y - playlistView.contentY`，表示该条目距离视口顶部的像素偏移。

保存动作必须发生在调用 `enterFolder()` 或 `goBack()` 之前。

### 6.3 进入子文件夹

进入时创建一个新的 navigation transaction：

```text
pending.mode = Enter
pending.revision = nextRevision
pending.targetFolderId = childId
controller.enterFolder(childId)
```

收到目标 projection 完成信号后：

1. 确认 revision 仍是当前事务。
2. 确认 `playlistView.width > 0`、`playlistView.height > 0`、model count 已更新。
3. 调用 `playlistView.forceLayout()`。
4. 调用 `playlistView.positionViewAtBeginning()`。
5. 清除 pending transaction。

不要在 `enterFolder()` 调用前写 `contentY = 0`，也不要把子列表顶部逻辑和父列表恢复逻辑合并。

### 6.4 返回父文件夹

返回时创建另一个 transaction：

```text
pending.mode = Restore
pending.anchor = savedFolderState
controller.goBack()
```

收到父 projection 完成信号后：

1. 调用 `forceLayout()`，确保当前 projection 和 delegate 已可查询。
2. 使用稳定 node id 找到新的 index；找不到时使用保存的 index。
3. 调用 `positionViewAtIndex(index, ListView.Beginning)`。
4. 根据保存的 `visibleOffset` 调整 contentY，使该条目回到原来的视口偏移。
5. 对结果执行边界限制，避免短列表或条目已经被删除时产生无效位置。
6. 清除 pending transaction。

概念伪代码：

```qml
function restoreAnchor(anchor) {
    playlistView.forceLayout()

    var index = libraryController.indexOfNodeId(anchor.nodeId)
    if (index < 0)
        index = anchor.fallbackIndex

    if (index < 0 || index >= playlistView.count) {
        playlistView.positionViewAtBeginning()
        return
    }

    playlistView.positionViewAtIndex(index, ListView.Beginning)
    playlistView.contentY = Math.max(0, playlistView.contentY - anchor.visibleOffset)
}
```

这段逻辑只能在 view 已经处理当前 projection 后执行。`Qt.callLater()` 只有在等待明确的 layout invariant 时才可使用；不能用固定时长的 timer 作为正确性依据。

### 6.5 定位请求隔离

`scrollRequest` 不能只依赖 `folderTransitionDirection` 和 300ms 定时器进行门控。导航事务期间应使用明确的 revision 或 transaction id：

- 文件夹进入/返回期间，忽略旧的播放定位请求。
- 搜索和定位请求拥有独立的 mode，直接调用定位，不触发文件夹动画。
- 导航事务结束后，才允许新的播放定位请求生效。

这样可以避免根目录返回时的 `requestScrollToNode()` 或并发播放状态覆盖父文件夹恢复位置。

## 7. 动画方案

动画和滚动恢复必须解耦：动画不能改变 delegate 高度或 `contentY`，也不能决定滚动恢复是否完成。

推荐规则：

- 完整投影替换使用 `populate` 语义。
- 真正的增量插入使用 `add`。
- 进入子文件夹时，动画基准 index 为 0。
- 返回父文件夹时，动画基准 index 使用保存状态中的首个可见 index。
- 延迟使用 `max(0, index - firstVisibleIndex) * step`。
- 不要在 transition 的延迟表达式中依赖尚未稳定的 `indexAt()`。
- 开启 `reuseItems` 时，在 `ListView.onReused` 中重置动画临时状态。

如果 `populate` 的启动时机无法与恢复事务可靠地分离，应改为在锚点恢复完成后由 delegate 显式启动导航动画；不要再用 `add` 伪装完整投影替换。

## 8. 运行时验证方案

在实现前后记录以下事件，每条日志带有同一个 navigation revision：

```text
enterFolder / goBack
projectionChanged
rowsAboutToBeRemoved / rowsRemoved
rowsAboutToBeInserted / rowsInserted
modelReset
countChanged
contentHeightChanged
contentYChanged
firstVisibleIndex
firstVisibleNodeId
currentIndex
movementStarted / movementEnded
```

每条记录至少包含：

```text
revision, folderId, mode, count, width, height,
contentY, contentHeight, firstVisibleIndex, firstVisibleNodeId
```

必须验证以下序列：

1. 根目录滚动到非零位置，进入子目录，确认子目录最终首项在顶部。
2. 根目录滚动到非零位置，进入子目录，再返回，确认首个可见 node id 和像素偏移恢复。
3. 根目录进入子目录，再进入孙目录，逐级返回，确认每一级状态独立。
4. 子目录为空或内容少于一屏时返回，确认位置正确钳制。
5. 返回期间触发播放定位，确认定位请求不会覆盖恢复事务。
6. 搜索、清除搜索、定位当前播放曲目，确认直接更新且无文件夹切换动画。

## 9. 测试缺口

当前 CTest 通过不能证明上述 UI 行为，因为现有测试没有覆盖真实 `ListView` 布局和 contentY 时序。

实施方案时需要：

- 新增或扩展 QML 集成测试，至少断言子列表顶部和父列表锚点恢复。
- 检查 `tst_library_tree_model.cpp` 中对 `modelReset` 的断言，使测试契约与最终模型通知语义一致。
- 保留 C++ 模型投影测试，覆盖稳定 node id、排序和空列表。
- 继续运行前端完整构建、CTest、`sidebar-tree` 和 `main-playback` smoke。
- 最后执行真实手动导航验证；编译和 offscreen smoke 不能替代它。

## 10. 实施检查清单

- [ ] 恢复完整投影使用正确的 reset 语义。
- [ ] 增加 projection revision/完成信号。
- [ ] 保存稳定 node id、回退 index 和像素偏移。
- [ ] 进入与返回使用不同的 pending mode。
- [ ] 所有恢复动作在 `forceLayout()` 后执行。
- [ ] 移除依赖 300ms timer 的滚动正确性逻辑。
- [ ] 隔离导航、搜索和播放定位请求。
- [ ] 按完整替换/增量插入分别选择 `populate`/`add`。
- [ ] 增加覆盖真实 ListView 的集成测试。
- [ ] 完成手动根目录、嵌套目录和并发定位验证。

## 11. 设计文档影响

本记录描述的是 Sidebar 内部实现和 bug 修复方案，不改变 Seriona 的整体模块边界、后端集成方式或长期架构。因此不更新 `DESIGN.md`；待方案实际落地后，只在实现改变整体导航架构时重新评估。

## 12. B'' 重构落地记录（2026-08-22）

### 结论：放弃"reset 后恢复滚动位置"路线

早期方案（裸 contentY 恢复 → 全量 remove+insert → reset+锚点事务）经实测均无法恢复返回位置。综合调研（qtdeclarative 源码 + 多个 QTBUG + Plasma FolderView/Elisa/Vvave/Dolphin/PCManFM/Strawberry 源码）确认根因是机制性的：

- `QQuickItemView` 对 reset 无任何位置保留契约：`regenerate() → setPosition(contentStartOffset())` 无条件归顶（所有 Qt 版本）。
- `positionViewAtIndex` 对未实例化目标按平均高度估算（QTBUG-114346/132454）；`layout()` 的 `fixupPosition()` 会覆盖早期写入的 contentY；populate 过渡在 reset 后自动武装并与定位竞争。
- Qt 唯一保留 contentY 的路径是**视图不换模型**（视图实例存活）。

### B'' 架构：页面栈 + 每级独立投影模型

- **模型层**（`src/app/library_folder_projection_model.{h,cpp}`，LibraryController 拥有）：
  - `LibraryFolderProjectionModel`：每级文件夹一个独立投影（直接子级、过滤/排序与主模型一致，复用 `sortedProjectionNodeIds`）；监听主模型 `treeChanged` 全量重建（revision 递增）、`playingTrackIdChanged`/`focusedNodeIdChanged` 仅对投影内行发 dataChanged；`projectionModelForLevel(level)` 越界返回 nullptr，实例 CppOwnership 归 controller。
  - 路径栈：`enterFolder` 压栈、`goBack` 弹栈并释放该级实例，前缀级实例跨导航复用；`folderStackDepth` = 已进入文件夹层数（根浏览=0）；`locateNodeInFolderStack(nodeId)` 跨级定位。
  - `LibraryModel` 新增 `treeChanged`/`playingTrackIdChanged`/`focusedNodeIdChanged` 信号与 `rootNodeId`/`rootProjectionNodeIds` 等 const 访问器；主模型投影能力保留给搜索/曲库页（reset+revision 语义保留）。
- **QML 层**（`qml/components/Sidebar.qml`）：
  - 页面栈：根页/一级页常驻 + 二级动态页 + 独立搜索页，`visible` 切换不卸载；每级视图绑定各自投影模型实例 → 返回时 contentY 天然保留，**零恢复逻辑**。
  - 显式导航动画：delegate 内嵌 `startNavSlideIn(step, direction)`（15ms×step 错落 + 220ms OutCubic 滑入），页面激活后对可见行触发，错落基准 = `indexAt(0, contentY+1)`（当前可见第一项，进入=0）——满足"从当前可见第一项开始逐条滑入"。
  - 删除：`savedAnchors`/`captureAnchor`/`navPending`/`navStaggerBase`/`folderTransitionResetTimer`/populate·add stagger 全部移除；`scrollRequest` 当前页直接定位，跨级走 `locateNodeInFolderStack`。

### 验证结果（B''）

- 构建通过；ctest **137/137 通过**（新增 `tst_library_folder_projection_model`：每级投影内容/排序、revision 递增、树变化重建、播放/焦点同步、栈 API 与跨级定位；顺带修复基线失败的 `sidebar_queue_switch`）。
- smoke：`startup` / `sidebar-tree` / `main-playback` 均退出码 0；`verify-middle-layer.sh` 通过。
- **待执行**：第 8 节手动序列 1-6（真实窗口验证动画错落基准与返回位置保留）。
