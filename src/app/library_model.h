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

class LibraryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("LibraryModel is owned by LibraryController")

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
        ArtworkSourceRole
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
#if SERIONA_HAS_BACKEND
    void setPlaylistTreeSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot);
#endif

private:
    bool setEntryRoleFlag(int row, Role role, bool value, bool notify);
    bool entryMatchesSearch(const Entry &entry, const QString &trimmedQuery) const;
    void rebuildEntryIndexes();
    void rebuildProjectionIndexes();
    void setProjectionNodeIds(const QVector<QString> &nodeIds);
    QVector<QString> sortedProjectionNodeIds(QVector<QString> nodeIds, const QVector<SortRule> &sortRules) const;
    QVector<QString> searchProjectionNodeIds(const QString &searchQuery) const;

    LibraryTreeStore m_treeStore;
    QVector<Entry> m_entries;
    QHash<QString, Entry> m_nodeById;
    QHash<QString, QVector<QString>> m_childrenById;
    QHash<QString, QString> m_parentById;
    QHash<QString, QString> m_trackIdToNodeId;
    QHash<QString, int> m_rowByNodeId;
    QVector<QString> m_nodeOrder;
    QVector<QString> m_rootProjectionNodeIds;
    QString m_rootNodeId;
    QString m_focusedNodeId;
    QString m_playingTrackId;
    std::uint64_t m_version = 0;
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
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString savedRootPath READ savedRootPath NOTIFY savedRootPathChanged)
    Q_PROPERTY(QString playingTrackId READ playingTrackId WRITE setPlayingTrackId NOTIFY playingTrackIdChanged)
    Q_PROPERTY(bool followCurrentlyPlaying READ followCurrentlyPlaying WRITE setFollowCurrentlyPlaying NOTIFY followCurrentlyPlayingChanged)
    Q_PROPERTY(int visibleNodeCount READ visibleNodeCount NOTIFY visibleNodeCountChanged)
    Q_PROPERTY(bool libraryEmpty READ libraryEmpty NOTIFY libraryEmptyChanged)
    Q_PROPERTY(bool backendAvailable READ backendAvailable NOTIFY backendAvailableChanged)
    Q_PROPERTY(QString libraryState READ libraryState NOTIFY libraryStateChanged)
    Q_PROPERTY(QVariantList currentSortRules READ currentSortRules NOTIFY currentSortRulesChanged)
    QML_ELEMENT

public:
#if SERIONA_HAS_BACKEND
    using CommandExecutor = std::function<seriona::control::MediaControllerCommandResult(const seriona::control::MediaControlCommand &)>;
    using FolderSortExecutor = std::function<seriona::control::MediaControllerCommandResult(const QString &, const QString &, const QVariantList &)>;
    using ScanExecutor = std::function<seriona::control::MediaControllerCommandResult(const QString &)>;
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
    Q_INVOKABLE void playItem(int index);
    Q_INVOKABLE void playItem(const QString &nodeId);
    Q_INVOKABLE void locateCurrentSong();
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void submitSearch();
    Q_INVOKABLE void selectBrowserNode(const QString &nodeId);
    Q_INVOKABLE int rowForNodeId(const QString &nodeId) const;
    Q_INVOKABLE void applySortRules(const QVariantList &rules);

signals:
    void currentFolderNameChanged();
    void canGoBackChanged();
    void searchQueryChanged();
    void focusedNodeIdChanged();
    void selectedBrowserNodeIdChanged();
    void scrollRequestChanged();
    void scanStatusChanged();
    void scanProgressChanged();
    void lastErrorChanged();
    void savedRootPathChanged();
    void playingTrackIdChanged();
    void followCurrentlyPlayingChanged();
    void visibleNodeCountChanged();
    void libraryEmptyChanged();
    void backendAvailableChanged();
    void libraryStateChanged();
    void currentSortRulesChanged();

private:
    bool requestScanForRoot(const QString &rootPath);
    void setScanStatus(const QString &status);
    void setScanProgress(int progress);
    void setLastError(const QString &error);
    void setSavedRootPath(const QString &rootPath);
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
#if SERIONA_HAS_BACKEND
    std::optional<std::vector<seriona::control::FolderSortRule>> backendSortRulesFromModelRules(const QVector<LibraryModel::SortRule> &rules) const;
    QVariantList sortRuleVariantsFromModelRules(const QVector<LibraryModel::SortRule> &rules) const;
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
    QString m_lastError;
    QString m_savedRootPath;
    QString m_playingTrackId;
    bool m_followCurrentlyPlaying = false;
    int m_visibleNodeCount = 0;
    bool m_backendAvailable = false;
    QVector<LibraryModel::SortRule> m_sortRules;
    QVector<LibraryModel::SortRule> m_searchSortRules;
    bool m_hasSearchSortRules = false;
    QHash<QString, QVector<LibraryModel::SortRule>> m_savedFolderSortRules;
    QString m_activeFolderSortKey;
#if SERIONA_HAS_BACKEND
    CommandExecutor m_commandExecutor;
    FolderSortExecutor m_folderSortExecutor;
    ScanExecutor m_scanExecutor;
#endif
};

}
