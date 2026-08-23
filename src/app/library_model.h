#pragma once

#include "library_tree_store.h"

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVector>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#if SERIONA_HAS_BACKEND
#include "seriona/control/control_contracts.h"
#include "seriona/scanner/scanner_contracts.h"

#include <functional>
#endif

namespace Seriona::App {

class LibraryFolderProjectionModel;

class LibraryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("LibraryModel is owned by LibraryController")
    Q_PROPERTY(int projectionRevision READ projectionRevision NOTIFY projectionRevisionChanged)
public:
    enum Role {
        TypeRole = Qt::UserRole + 1,
        NameRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        ParentNameRole,
        SongCountRole,
        DurationRole,
        FormatRole,
        SampleRateRole,
        BitDepthRole,
        NodeIdRole,
        TrackIdRole,
        IsFolderRole,
        IsPlayingRole,
        IsFocusedRole,
        ParentNodeIdRole,
        ArtworkSourceRole,
        // 只追加末尾：roleNames/既有 role 索引按数值兼容（T14 修复 B：年份 QML 可达）
        YearRole
    };
    Q_ENUM(Role)

    struct Entry {
        QString type;
        QString name;
        QString title;
        QString artist;
        QString album;
        QString fileName;
        QString parentName;
        int songCount = 0;
        QString duration;
        QString format;
        int sampleRate = 0;
        int bitDepth = 0;
        QString nodeId;
        QString trackId;
        bool isFolder = false;
        bool isPlaying = false;
        bool isFocused = false;
        QString parentNodeId;
        QString artworkSource;
        std::optional<std::chrono::milliseconds> durationValue;
        std::optional<std::uint32_t> year;
        std::optional<std::uint32_t> discNumber;
        std::optional<std::uint32_t> trackNumber;
        std::optional<std::filesystem::file_time_type> createdDate;
    };

    struct SortRule {
        QString field;
        QString order;
    };

    explicit LibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const Entry *entryAt(int row) const;
    const Entry *entryByNodeId(const QString &nodeId) const;
    QVector<QString> childNodeIds(const QString &nodeId) const;
    QString parentNodeId(const QString &nodeId) const;
    QString nodeIdForTrackId(const QString &trackId) const;
    // 绝对路径查询（T14 修复 A）：歌曲返回音频文件绝对路径（sourceFilePath 优先，
    // 与 fileNameFromNode 同一约定），文件夹沿显示名链从某个后代曲目重建完整目录路径；
    // 路径段与显示名不一致（cue 容器/虚拟目录等无文件系统实体）或未知节点返回空。
    QString absoluteFilePathForNode(const QString &nodeId) const;
    bool containsNodeId(const QString &nodeId) const;
    int rowForNodeId(const QString &nodeId) const;
    QString firstNodeId() const;
    QVector<QString> ancestorChainForNode(const QString &nodeId) const;
    std::uint64_t version() const;
    bool setFocusedNodeId(const QString &nodeId);
    bool setPlayingTrackId(const QString &trackId);
    void applyBrowsingState(const QString &focusedNodeId,
                            const QString &playingTrackId,
                            const QString &searchQuery,
                            const QString &browserRootNodeId = QString(),
                            const QVector<SortRule> &sortRules = {});
    int visibleNodeCount() const;
    QString firstVisibleNodeId() const;
    QString firstVisibleMatchingNodeId() const;
    bool hasLibraryContent() const;
    // 投影 revision：每次投影完整替换（setProjectionNodeIds）或快照更新
    // （setPlaylistTreeSnapshot）完成后递增并发出 projectionRevisionChanged；
    // QML 侧用它确认导航事务的投影已就绪。
    int projectionRevision() const;
    // 供每级文件夹投影模型（LibraryFolderProjectionModel）复用的只读访问器：
    // 根节点 id、根投影（虚拟根直接子级）、当前焦点节点与当前播放曲目 id。
    QString rootNodeId() const;
    QVector<QString> rootProjectionNodeIds() const;
    QString focusedNodeId() const;
    QString playingTrackId() const;
    // 投影排序（B'' 复用形式）：主模型投影与每级文件夹投影共用同一实现，
    // 行为语义完全一致（现有排序测试已覆盖，不许改变）。
    QVector<QString> sortedProjectionNodeIds(QVector<QString> nodeIds, const QVector<SortRule> &sortRules) const;
#if SERIONA_HAS_BACKEND
    void setPlaylistTreeSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot);
#endif

signals:
    void projectionRevisionChanged();
    // 树重建完成（setPlaylistTreeSnapshot 末尾，endResetModel 与 revision 递增之后）发出；
    // 每级文件夹投影模型监听它做全量重建。
    void treeChanged();
    // 播放/焦点身份变化（setPlayingTrackId / setFocusedNodeId 或浏览状态重投影时）
    // 发出；每级文件夹投影模型监听它同步投影行的 IsPlaying / IsFocused role。
    void playingTrackIdChanged();
    void focusedNodeIdChanged();

private:
    bool setEntryRoleFlag(int row, Role role, bool value, bool notify);
    int entrySearchScore(const Entry &entry, const QString &trimmedQuery) const;
    void rebuildEntryIndexes();
    void rebuildProjectionIndexes();
    void setProjectionNodeIds(const QVector<QString> &nodeIds);
    QVector<QString> searchProjectionNodeIds(const QString &searchQuery, const QString &subtreeRootNodeId);

    LibraryTreeStore m_treeStore;
    QVector<Entry> m_entries;
    QHash<QString, Entry> m_nodeById;
    QHash<QString, QVector<QString>> m_childrenById;
    QHash<QString, QString> m_parentById;
    QHash<QString, QString> m_trackIdToNodeId;
    QHash<QString, int> m_rowByNodeId;
    QHash<QString, int> m_searchScoreByNodeId;
    QVector<QString> m_nodeOrder;
    QVector<QString> m_rootProjectionNodeIds;
    QString m_rootNodeId;
    QString m_focusedNodeId;
    QString m_playingTrackId;
    std::uint64_t m_version = 0;
    int m_projectionRevision = 0;
};

class LibraryController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(LibraryModel *model READ model CONSTANT)
    Q_PROPERTY(QString currentFolderName READ currentFolderName NOTIFY currentFolderNameChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY canGoBackChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString focusedNodeId READ focusedNodeId WRITE setFocusedNodeId NOTIFY focusedNodeIdChanged)
    Q_PROPERTY(QString selectedBrowserNodeId READ selectedBrowserNodeId WRITE setSelectedBrowserNodeId NOTIFY selectedBrowserNodeIdChanged)
    Q_PROPERTY(QString scrollRequest READ scrollRequest NOTIFY scrollRequestChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)
    Q_PROPERTY(int scanProgress READ scanProgress NOTIFY scanProgressChanged)
    Q_PROPERTY(quint64 scannedSongCount READ scannedSongCount NOTIFY scanCountsChanged)
    Q_PROPERTY(quint64 totalSongCount READ totalSongCount NOTIFY scanCountsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString savedRootPath READ savedRootPath NOTIFY savedRootPathChanged)
    Q_PROPERTY(QString playingTrackId READ playingTrackId WRITE setPlayingTrackId NOTIFY playingTrackIdChanged)
    Q_PROPERTY(bool followCurrentlyPlaying READ followCurrentlyPlaying WRITE setFollowCurrentlyPlaying NOTIFY followCurrentlyPlayingChanged)
    Q_PROPERTY(int visibleNodeCount READ visibleNodeCount NOTIFY visibleNodeCountChanged)
    Q_PROPERTY(bool libraryEmpty READ libraryEmpty NOTIFY libraryEmptyChanged)
    Q_PROPERTY(bool backendAvailable READ backendAvailable NOTIFY backendAvailableChanged)
    Q_PROPERTY(QString libraryState READ libraryState NOTIFY libraryStateChanged)
    Q_PROPERTY(QVariantList currentSortRules READ currentSortRules NOTIFY currentSortRulesChanged)
    Q_PROPERTY(int folderStackDepth READ folderStackDepth NOTIFY folderStackDepthChanged)
    Q_PROPERTY(QString currentFolderNodeId READ currentFolderNodeId NOTIFY currentFolderNodeIdChanged)
    // 投影缓存代次：主模型树重建（treeChanged）时递增；缓存模型自重建，QML 无需消费。
    Q_PROPERTY(int projectionGeneration READ projectionGeneration NOTIFY projectionGenerationChanged)
    QML_ELEMENT

public:
#if SERIONA_HAS_BACKEND
    using CommandExecutor = std::function<seriona::control::MediaControllerCommandResult(const seriona::control::MediaControlCommand &)>;
    using FolderSortExecutor = std::function<seriona::control::MediaControllerCommandResult(const QString &, const QString &, const QVariantList &)>;
    using ScanExecutor = std::function<seriona::control::MediaControllerCommandResult(const QString &, seriona::scanner::ScanMode)>;
#endif

    explicit LibraryController(QObject *parent = nullptr);

    LibraryModel *model();
    QString currentFolderName() const;
    bool canGoBack() const;
    QString searchQuery() const;
    void setSearchQuery(const QString &query);
    QString focusedNodeId() const;
    void setFocusedNodeId(const QString &nodeId);
    QString selectedBrowserNodeId() const;
    void setSelectedBrowserNodeId(const QString &nodeId);
    QString scrollRequest() const;
    int visibleNodeCount() const;
    QString scanStatus() const;
    int scanProgress() const;
    quint64 scannedSongCount() const;
    quint64 totalSongCount() const;
    QString lastError() const;
    QString savedRootPath() const;
    QString playingTrackId() const;
    void setPlayingTrackId(const QString &trackId);
    bool followCurrentlyPlaying() const;
    void setFollowCurrentlyPlaying(bool follow);
    bool libraryEmpty() const;
    bool backendAvailable() const;
    QString libraryState() const;
    QVariantList currentSortRules() const;
    void clearSavedRootPath(const QString &message);
#if SERIONA_HAS_BACKEND
    void setCommandExecutor(CommandExecutor executor);
    void setFolderSortExecutor(FolderSortExecutor executor);
    void setScanExecutor(ScanExecutor executor);
    void setPlaylistTreeSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot);
    void applyPlayerStateSnapshot(const seriona::control::PlayerStateSnapshot &snapshot, bool forceReapply);
    void applyLibraryStateSnapshot(const seriona::control::LibraryStateSnapshot &snapshot);
    void applyFolderSortSetting(const seriona::control::FolderSortSetting &setting);
#endif

    Q_INVOKABLE void enterFolder(int index);
    Q_INVOKABLE void enterFolder(const QString &nodeId);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool scanLibrary(const QUrl &rootUrl);
#if SERIONA_HAS_BACKEND
    bool scanLibrary(const QUrl &rootUrl, seriona::scanner::ScanMode mode);
#endif
    Q_INVOKABLE void playItem(int index);
    Q_INVOKABLE void playItem(const QString &nodeId);
    Q_INVOKABLE void locateCurrentSong();
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void submitSearch();
    Q_INVOKABLE void selectBrowserNode(const QString &nodeId);
    Q_INVOKABLE int rowForNodeId(const QString &nodeId) const;
    Q_INVOKABLE void applySortRules(const QVariantList &rules);
    // 每级文件夹一个独立投影模型实例（归 controller 所有，QML 只读绑定）：
    // 按 folderNodeId 缓存（QString() 根键保留），goBack/enterFolder 不再销毁实例；
    // 树重建时缓存模型经 setSource 连接的 treeChanged 自动原地重建（身份不变）。
    QString currentFolderNodeId() const;
    int projectionGeneration() const;
    int folderStackDepth() const;
    // 按 folderNodeId 取投影模型（缓存 get-or-create；排序规则经 sortRulesForProjectionLevel）。
    Q_INVOKABLE QObject *projectionModelForNodeId(const QString &folderNodeId);
    // 当前文件夹祖先链（从根向目标、排除根；现有 C++ 实现 :619-637 的"去根 + 倒序"等价变换）。
    Q_INVOKABLE QStringList ancestorChainForNode(const QString &nodeId) const;
    // 缓存条目计数（含根键），供测试/诊断观测缓存大小。
    Q_INVOKABLE int projectionCacheSize() const;
    Q_INVOKABLE void locateNodeInFolderStack(const QString &nodeId);
    // 设置已保存的曲库根路径（归一化后存值并发射 savedRootPathChanged）。
    // 真实应用经扫描流程（requestScanForRoot）设置；测试宿主未启动后端桥，
    // 需要直接配置该状态才能覆盖"重进目录恢复排序规则"路径。
    void setSavedRootPath(const QString &rootPath);

signals:
    void currentFolderNameChanged();
    void canGoBackChanged();
    void searchQueryChanged();
    void focusedNodeIdChanged();
    void selectedBrowserNodeIdChanged();
    void scrollRequestChanged();
    void scanStatusChanged();
    void scanProgressChanged();
    void scanCountsChanged();
    void lastErrorChanged();
    void savedRootPathChanged();
    void playingTrackIdChanged();
    void followCurrentlyPlayingChanged();
    void visibleNodeCountChanged();
    void libraryEmptyChanged();
    void backendAvailableChanged();
    void libraryStateChanged();
    void currentSortRulesChanged();
    void folderStackDepthChanged();
    void currentFolderNodeIdChanged();
    void projectionGenerationChanged();

private:
#if SERIONA_HAS_BACKEND
    bool requestScanForRoot(const QString &rootPath, seriona::scanner::ScanMode mode);
#else
    bool requestScanForRoot(const QString &rootPath);
#endif
    void setScanStatus(const QString &status);
    void setScanProgress(int progress);
    void setScanCounts(quint64 scanned, quint64 total);
    void setLastError(const QString &error);
    void setBackendAvailable(bool available);
    void emitLibraryStateChanges(bool previousLibraryEmpty, const QString &previousLibraryState);
    void applyBrowsingState();
    QVector<LibraryModel::SortRule> sortRulesForCurrentProjection() const;
    QString folderSortKey(const QString &rootPath, const QString &folderNodeId) const;
    QString currentFolderSortKey() const;
    void restoreSortRulesForCurrentFolder();
    void rememberFolderSortRules(const QString &rootPath, const QString &folderNodeId, const QVector<LibraryModel::SortRule> &rules);
    bool showNodeInBrowserProjection(const QString &nodeId);
    void updateVisibleNodeCount();
    void reconcileBrowsingState(const QVector<QString> &focusedFallbackChain, const QVector<QString> &selectedFallbackChain);
    QString firstExistingNode(const QVector<QString> &nodeIds) const;
    std::optional<LibraryModel::SortRule> sortRuleFromVariant(const QVariant &ruleVar) const;
    std::optional<QVector<LibraryModel::SortRule>> sortRulesFromVariants(const QVariantList &rules) const;
    void activateTrack(const LibraryModel::Entry *entry);
    void requestScrollToNode(const QString &nodeId);
    bool persistCurrentFolderSortRules(const QVector<LibraryModel::SortRule> &rules);
    QVariantList sortRuleVariantsFromModelRules(const QVector<LibraryModel::SortRule> &rules) const;
    // 当前文件夹投影缓存维护：确保祖先链各级（含根键）缓存条目存在；
    // 进入/返回不再销毁任何投影模型（实例跨导航复用，滚动位置保留的前提）。
    void syncFolderStackToCurrentFolder();
    // 统一修改 m_currentFolderNodeId/m_currentFolderName 并发射
    // currentFolderNodeIdChanged/currentFolderNameChanged/folderStackDepthChanged
    // （深度按祖先链计算，变化时发射）；canGoBackChanged 由调用点按既有语义发射。
    void setCurrentFolderNodeId(const QString &nodeId, const QString &folderName);
    LibraryFolderProjectionModel *createFolderProjection(const QString &folderNodeId);
    QVector<LibraryModel::SortRule> sortRulesForProjectionLevel(const QString &folderNodeId) const;
    void refreshCurrentFolderProjection();
#if SERIONA_HAS_BACKEND
    std::optional<std::vector<seriona::control::FolderSortRule>> backendSortRulesFromModelRules(const QVector<LibraryModel::SortRule> &rules) const;
    std::optional<QVector<LibraryModel::SortRule>> modelSortRulesFromBackendRules(const std::vector<seriona::control::FolderSortRule> &rules) const;
    seriona::control::MediaControllerCommandResult submitCommand(const seriona::control::MediaControlCommand &command);
#endif

    LibraryModel m_model;
    QString m_currentFolderName = QStringLiteral("My Music");
    QString m_searchQuery;
    QString m_currentFolderNodeId;
    QString m_focusedNodeId;
    QString m_selectedBrowserNodeId;
    QString m_scrollRequest;
    QString m_scanStatus = QStringLiteral("pending");
    int m_scanProgress = 0;
    quint64 m_scannedSongCount = 0;
    quint64 m_totalSongCount = 0;
    QString m_lastError;
    QString m_savedRootPath;
    QString m_playingTrackId;
    bool m_followCurrentlyPlaying = false;
    int m_visibleNodeCount = 0;
    bool m_backendAvailable = false;
    QVector<LibraryModel::SortRule> m_sortRules;
    QHash<QString, QVector<LibraryModel::SortRule>> m_savedFolderSortRules;
    QString m_activeFolderSortKey;
    QHash<QString, LibraryFolderProjectionModel *> m_folderProjectionCache;
    int m_projectionGeneration = 0;
#if SERIONA_HAS_BACKEND
    CommandExecutor m_commandExecutor;
    FolderSortExecutor m_folderSortExecutor;
    ScanExecutor m_scanExecutor;
#endif
};

}
