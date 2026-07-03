#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#if SERIONA_HAS_BACKEND
#include "seriona/scanner/scanner_contracts.h"
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
        IsExpandedRole,
        ParentNodeIdRole,
        DepthRole,
        IsVisibleRole,
        MatchesSearchRole
    };
    Q_ENUM(Role)

    struct Entry {
        QString type;
        QString name;
        QString title;
        QString artist;
        QString album;
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
        bool isExpanded = false;
        QString parentNodeId;
        int depth = 0;
        bool isVisible = true;
        bool matchesSearch = true;
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
    void setEntries(const QVector<Entry> &entries);
    bool setNodeExpanded(const QString &nodeId, bool expanded);
    bool setFocusedNodeId(const QString &nodeId);
    bool setPlayingTrackId(const QString &trackId);
    void applyBrowsingState(const QSet<QString> &expandedNodeIds, const QString &focusedNodeId, const QString &playingTrackId, const QString &searchQuery);
    int visibleNodeCount() const;
    QString firstVisibleNodeId() const;
    QString firstVisibleMatchingNodeId() const;
#if SERIONA_HAS_BACKEND
    void setPlaylistTreeSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot);
#endif

private:
    bool setEntryRoleFlag(int row, Role role, bool value, bool notify);
    bool entryMatchesSearch(const Entry &entry, const QString &trimmedQuery) const;
    bool entryVisibleByExpansion(const Entry &entry, const QSet<QString> &expandedNodeIds) const;
    void rebuildEntryIndexes();

    QVector<Entry> m_entries;
    QHash<QString, Entry> m_nodeById;
    QHash<QString, QVector<QString>> m_childrenById;
    QHash<QString, QString> m_parentById;
    QHash<QString, QString> m_trackIdToNodeId;
    QHash<QString, int> m_rowByNodeId;
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
    Q_PROPERTY(QStringList expandedNodeIds READ expandedNodeIds NOTIFY expandedNodeIdsChanged)
    Q_PROPERTY(QString focusedNodeId READ focusedNodeId WRITE setFocusedNodeId NOTIFY focusedNodeIdChanged)
    Q_PROPERTY(QString selectedBrowserNodeId READ selectedBrowserNodeId WRITE setSelectedBrowserNodeId NOTIFY selectedBrowserNodeIdChanged)
    Q_PROPERTY(QString scrollRequest READ scrollRequest NOTIFY scrollRequestChanged)
    Q_PROPERTY(QString playingTrackId READ playingTrackId WRITE setPlayingTrackId NOTIFY playingTrackIdChanged)
    Q_PROPERTY(bool followCurrentlyPlaying READ followCurrentlyPlaying WRITE setFollowCurrentlyPlaying NOTIFY followCurrentlyPlayingChanged)
    Q_PROPERTY(int visibleNodeCount READ visibleNodeCount NOTIFY visibleNodeCountChanged)
    QML_ELEMENT

public:
    explicit LibraryController(QObject *parent = nullptr);

    LibraryModel *model();
    QString currentFolderName() const;
    bool canGoBack() const;
    QString searchQuery() const;
    void setSearchQuery(const QString &query);
    QStringList expandedNodeIds() const;
    QString focusedNodeId() const;
    void setFocusedNodeId(const QString &nodeId);
    QString selectedBrowserNodeId() const;
    void setSelectedBrowserNodeId(const QString &nodeId);
    QString scrollRequest() const;
    int visibleNodeCount() const;
    QString playingTrackId() const;
    void setPlayingTrackId(const QString &trackId);
    bool followCurrentlyPlaying() const;
    void setFollowCurrentlyPlaying(bool follow);
#if SERIONA_HAS_BACKEND
    void setPlaylistTreeSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot);
#endif

    Q_INVOKABLE void enterFolder(int index);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void refresh();
    // future backend hook: request playback for a library item.
    Q_INVOKABLE void playItem(int index);
    // future backend hook: locate the current playing song in the library tree.
    Q_INVOKABLE void locateCurrentSong();
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void submitSearch();
    Q_INVOKABLE void expandNode(const QString &nodeId);
    Q_INVOKABLE void collapseNode(const QString &nodeId);
    Q_INVOKABLE void toggleExpanded(const QString &nodeId);
    Q_INVOKABLE void focusNode(const QString &nodeId);
    Q_INVOKABLE void selectBrowserNode(const QString &nodeId);
    Q_INVOKABLE void requestScrollToNode(const QString &nodeId);
    Q_INVOKABLE int rowForNodeId(const QString &nodeId) const;
    Q_INVOKABLE QString describeBackendHook() const;

signals:
    void currentFolderNameChanged();
    void canGoBackChanged();
    void searchQueryChanged();
    void searchSubmitted(QString query);
    void searchCleared();
    void playItemRequested(QString title);
    void currentSongLocationRequested();
    void expandedNodeIdsChanged();
    void focusedNodeIdChanged();
    void selectedBrowserNodeIdChanged();
    void scrollRequestChanged();
    void playingTrackIdChanged();
    void followCurrentlyPlayingChanged();
    void visibleNodeCountChanged();

private:
    enum class Folder {
        Root,
        Child
    };

    static QVector<LibraryModel::Entry> rootEntries();
    static QVector<LibraryModel::Entry> childEntries();
    QVector<LibraryModel::Entry> currentSourceEntries() const;
    QVector<LibraryModel::Entry> filteredEntries(const QVector<LibraryModel::Entry> &entries) const;
    void updateModelEntries();
    void setFolder(Folder folder, const QString &folderName);
    void setExpanded(const QString &nodeId, bool expanded);
    void applyBrowsingState();
    void updateVisibleNodeCount();
    void reconcileBrowsingState(const QVector<QString> &focusedFallbackChain, const QVector<QString> &selectedFallbackChain);
    QString firstExistingNode(const QVector<QString> &nodeIds) const;

    LibraryModel m_model;
    Folder m_folder = Folder::Root;
    QString m_currentFolderName = QStringLiteral("My Music");
    QString m_searchQuery;
    QSet<QString> m_expandedNodeIds;
    QString m_focusedNodeId;
    QString m_selectedBrowserNodeId;
    QString m_scrollRequest;
    QString m_playingTrackId;
    bool m_followCurrentlyPlaying = false;
    int m_visibleNodeCount = 0;
};

}
