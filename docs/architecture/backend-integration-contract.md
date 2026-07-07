# 后端集成契约

Seriona 前端 QML 不直接持有后端状态；中间层 owners 负责把后端快照映射为 QML 可绑定状态，并把用户意图提交给后端。

- `PlaybackController`：播放 read model 与播放命令提交。
- `LibraryController`：曲库扫描、曲库树快照、浏览投影、空曲库和后端不可用状态。
- `LyricsModel`：歌词 read model 与本地歌词显示状态。
- `NavigationController`：启动/主界面、本地导航和 sidebar 状态。

后端 `PlayerStateSnapshot`、`LibraryStateSnapshot`、`PlaylistTreeSnapshot` 和命令结果仍是权威事实来源；前端不得用生产假数据替代缺失的后端曲库内容。
