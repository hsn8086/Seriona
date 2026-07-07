#include "library_model.h"

#include "backend_snapshot_mapper.h"

#include <QByteArray>
#include <QDebug>
#include <QFileInfo>
#include <QSet>
#include <QUrl>

#if SERIONA_HAS_BACKEND
#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>
#endif

namespace Seriona::App {

#if SERIONA_HAS_BACKEND
namespace {
QString toQString(const std::string &value)
{
    return QString::fromStdString(value);
}

std::string toStdString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

seriona::control::TrackIdentity trackIdentityForEntry(const LibraryModel::Entry &entry)
{
    seriona::control::TrackIdentity identity;
    identity.trackId = toStdString(entry.trackId);
    return identity;
}

QString typeForNode(const LibraryTreeStore::Node &node)
{
    return node.isFolder ? QStringLiteral("folder") : QStringLiteral("file");
}

QString formatDuration(std::chrono::milliseconds duration)
{
    const qint64 totalSeconds = duration.count() / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}

QString formatFromNode(const LibraryTreeStore::Node &node)
{
    const std::filesystem::path &path = !node.sourceFilePath.empty() ? node.sourceFilePath : node.filePath;
    QString extension = QString::fromStdString(path.extension().string());
    if (extension.startsWith(QLatin1Char('.'))) {
        extension.remove(0, 1);
    }
    return extension.toUpper();
}

LibraryModel::Entry entryFromNode(const LibraryTreeStore::Node &node, const QString &parentName)
{
    LibraryModel::Entry entry;
    entry.nodeId = node.nodeId;
    entry.type = typeForNode(node);
    entry.parentName = parentName;
    entry.songCount = node.isFolder ? node.descendantTrackCount : 0;
    entry.isFolder = node.isFolder;

    if (entry.isFolder) {
        entry.name = node.displayName;
        return entry;
    }

    entry.trackId = node.trackId;
    entry.title = node.title.isEmpty() ? node.displayName : node.title;
    entry.artist = node.artist;
    entry.album = node.album;
    entry.format = formatFromNode(node);
    entry.sampleRate = node.sampleRate;
    entry.bitDepth = node.bitDepth;
    entry.duration = node.duration.has_value() ? formatDuration(*node.duration) : QString();

    const std::filesystem::path &artworkPath = !node.thumbnailPath.empty() ? node.thumbnailPath : node.artworkPath;
    if (!artworkPath.empty()) {
        entry.artworkSource = QUrl::fromLocalFile(QString::fromStdString(artworkPath.string())).toString();
    }
    return entry;
}
}
#endif

namespace {

QString localDirectoryPath(const QUrl &rootUrl)
{
    QString rootPath;
    if (rootUrl.isLocalFile()) {
        rootPath = rootUrl.toLocalFile();
    } else if (rootUrl.scheme().isEmpty()) {
        rootPath = rootUrl.path();
    }

    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        return QString();
    }

    return rootInfo.absoluteFilePath();
}

}

LibraryModel::LibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    const Entry *entry = entryAt(index.row());
    if (entry == nullptr) {
        return {};
    }

    switch (role) {
    case TypeRole:
        return entry->type;
    case NameRole:
        return entry->name;
    case TitleRole:
        return entry->title;
    case ArtistRole:
        return entry->artist;
    case AlbumRole:
        return entry->album;
    case ParentNameRole:
        return entry->parentName;
    case SongCountRole:
        return entry->songCount;
    case DurationRole:
        return entry->duration;
    case FormatRole:
        return entry->format;
    case SampleRateRole:
        return entry->sampleRate;
    case BitDepthRole:
        return entry->bitDepth;
    case NodeIdRole:
        return entry->nodeId;
    case TrackIdRole:
        return entry->trackId;
    case IsFolderRole:
        return entry->isFolder;
    case IsPlayingRole:
        return entry->isPlaying;
    case IsFocusedRole:
        return entry->isFocused;
    case ParentNodeIdRole:
        return entry->parentNodeId;
    case ArtworkSourceRole:
        return entry->artworkSource;
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {{TypeRole, "type"},
            {NameRole, "name"},
            {TitleRole, "title"},
            {ArtistRole, "artist"},
            {AlbumRole, "album"},
            {ParentNameRole, "parentName"},
            {SongCountRole, "songCount"},
            {DurationRole, "duration"},
            {FormatRole, "format"},
            {SampleRateRole, "sampleRate"},
            {BitDepthRole, "bitDepth"},
            {NodeIdRole, "nodeId"},
            {TrackIdRole, "trackId"},
            {IsFolderRole, "isFolder"},
            {IsPlayingRole, "isPlaying"},
            {IsFocusedRole, "isFocused"},
            {ParentNodeIdRole, "parentNodeId"},
            {ArtworkSourceRole, "artworkSource"}};
}

const LibraryModel::Entry *LibraryModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return nullptr;
    }

    return &m_entries.at(row);
}

const LibraryModel::Entry *LibraryModel::entryByNodeId(const QString &nodeId) const
{
    const auto entryIt = m_nodeById.constFind(nodeId);
    if (entryIt == m_nodeById.cend()) {
        return nullptr;
    }

    return &entryIt.value();
}

QVector<QString> LibraryModel::childNodeIds(const QString &nodeId) const
{
    return m_childrenById.value(nodeId);
}

QString LibraryModel::parentNodeId(const QString &nodeId) const
{
    return m_parentById.value(nodeId);
}

QString LibraryModel::nodeIdForTrackId(const QString &trackId) const
{
    return m_trackIdToNodeId.value(trackId);
}

bool LibraryModel::containsNodeId(const QString &nodeId) const
{
    return !nodeId.isEmpty() && m_nodeById.contains(nodeId);
}

int LibraryModel::rowForNodeId(const QString &nodeId) const
{
    return m_rowByNodeId.value(nodeId, -1);
}

QString LibraryModel::firstNodeId() const
{
    if (!m_rootNodeId.isEmpty() && containsNodeId(m_rootNodeId)) {
        return m_rootNodeId;
    }
    for (const QString &nodeId : m_nodeOrder) {
        if (!nodeId.isEmpty() && containsNodeId(nodeId)) {
            return nodeId;
        }
    }
    for (const Entry &entry : m_entries) {
        if (!entry.nodeId.isEmpty()) {
            return entry.nodeId;
        }
    }
    return {};
}

QVector<QString> LibraryModel::ancestorChainForNode(const QString &nodeId) const
{
    QVector<QString> chain;
    QSet<QString> visited;
    QString currentNodeId = nodeId;

    while (!currentNodeId.isEmpty() && !visited.contains(currentNodeId)) {
        chain.append(currentNodeId);
        visited.insert(currentNodeId);
        currentNodeId = m_parentById.value(currentNodeId);
    }

    const QString rootNodeId = firstNodeId();
    if (!rootNodeId.isEmpty() && !chain.contains(rootNodeId)) {
        chain.append(rootNodeId);
    }

    return chain;
}

std::uint64_t LibraryModel::version() const
{
    return m_version;
}

bool LibraryModel::setFocusedNodeId(const QString &nodeId)
{
    if (!nodeId.isEmpty() && !containsNodeId(nodeId)) {
        return false;
    }
    if (m_focusedNodeId == nodeId) {
        return false;
    }

    const QString previousFocusedNodeId = m_focusedNodeId;
    m_focusedNodeId = nodeId;

    bool changed = false;
    const int previousRow = rowForNodeId(previousFocusedNodeId);
    if (previousRow >= 0) {
        changed = setEntryRoleFlag(previousRow, IsFocusedRole, false, true) || changed;
    }
    const int nextRow = rowForNodeId(m_focusedNodeId);
    if (nextRow >= 0) {
        changed = setEntryRoleFlag(nextRow, IsFocusedRole, true, true) || changed;
    }
    return changed;
}

bool LibraryModel::setPlayingTrackId(const QString &trackId)
{
    if (m_playingTrackId == trackId) {
        return false;
    }

    const QString previousNodeId = nodeIdForTrackId(m_playingTrackId);
    m_playingTrackId = trackId;
    const QString nextNodeId = nodeIdForTrackId(m_playingTrackId);

    bool changed = false;
    const int previousRow = rowForNodeId(previousNodeId);
    if (previousRow >= 0) {
        changed = setEntryRoleFlag(previousRow, IsPlayingRole, false, true) || changed;
    }
    const int nextRow = rowForNodeId(nextNodeId);
    if (nextRow >= 0) {
        changed = setEntryRoleFlag(nextRow, IsPlayingRole, true, true) || changed;
    }
    return changed;
}

void LibraryModel::applyBrowsingState(const QString &focusedNodeId, const QString &playingTrackId, const QString &searchQuery, const QString &browserRootNodeId)
{
    m_focusedNodeId = containsNodeId(focusedNodeId) ? focusedNodeId : QString();
    m_playingTrackId = playingTrackId;
    const QString trimmedQuery = searchQuery.trimmed();

    QVector<QString> projectionNodeIds;
    if (!trimmedQuery.isEmpty()) {
        projectionNodeIds = searchProjectionNodeIds(trimmedQuery);
    } else if (!browserRootNodeId.isEmpty() && containsNodeId(browserRootNodeId)) {
        projectionNodeIds = childNodeIds(browserRootNodeId);
    } else {
        projectionNodeIds = m_rootProjectionNodeIds;
    }

    setProjectionNodeIds(projectionNodeIds);
}

int LibraryModel::visibleNodeCount() const
{
    return m_entries.size();
}

QString LibraryModel::firstVisibleNodeId() const
{
    for (const Entry &entry : m_entries) {
        if (!entry.nodeId.isEmpty()) {
            return entry.nodeId;
        }
    }
    return {};
}

QString LibraryModel::firstVisibleMatchingNodeId() const
{
    for (const Entry &entry : m_entries) {
        if (!entry.nodeId.isEmpty()) {
            return entry.nodeId;
        }
    }
    return {};
}

bool LibraryModel::hasLibraryContent() const
{
    for (const QString &nodeId : m_nodeOrder) {
        if (!nodeId.isEmpty() && nodeId != m_rootNodeId && m_nodeById.contains(nodeId)) {
            return true;
        }
    }
    return false;
}

bool LibraryModel::setEntryRoleFlag(int row, Role role, bool value, bool notify)
{
    if (row < 0 || row >= m_entries.size()) {
        return false;
    }

    Entry &entry = m_entries[row];
    bool *flag = nullptr;
    switch (role) {
    case IsPlayingRole:
        flag = &entry.isPlaying;
        break;
    case IsFocusedRole:
        flag = &entry.isFocused;
        break;
    default:
        return false;
    }

    if (*flag == value) {
        return false;
    }

    *flag = value;
    if (!entry.nodeId.isEmpty()) {
        m_nodeById.insert(entry.nodeId, entry);
    }
    if (notify) {
        const QModelIndex changedIndex = index(row, 0);
        emit dataChanged(changedIndex, changedIndex, QList<int>{role});
    }
    return true;
}

bool LibraryModel::entryMatchesSearch(const Entry &entry, const QString &trimmedQuery) const
{
    if (trimmedQuery.isEmpty()) {
        return true;
    }

    if (entry.isFolder) {
        return entry.name.contains(trimmedQuery, Qt::CaseInsensitive)
            || entry.parentName.contains(trimmedQuery, Qt::CaseInsensitive);
    }

    return entry.title.contains(trimmedQuery, Qt::CaseInsensitive)
        || entry.artist.contains(trimmedQuery, Qt::CaseInsensitive)
        || entry.album.contains(trimmedQuery, Qt::CaseInsensitive)
        || entry.format.contains(trimmedQuery, Qt::CaseInsensitive);
}

void LibraryModel::rebuildEntryIndexes()
{
    m_nodeById.clear();
    m_trackIdToNodeId.clear();
    m_rowByNodeId.clear();
    m_nodeOrder.clear();
    m_rootProjectionNodeIds.clear();

    for (int row = 0; row < m_entries.size(); ++row) {
        const Entry &entry = m_entries.at(row);
        if (!entry.nodeId.isEmpty()) {
            m_nodeById.insert(entry.nodeId, entry);
            m_rowByNodeId.insert(entry.nodeId, row);
            m_nodeOrder.append(entry.nodeId);
            m_rootProjectionNodeIds.append(entry.nodeId);
        }
        if (!entry.nodeId.isEmpty() && !entry.parentNodeId.isEmpty() && !m_parentById.contains(entry.nodeId)) {
            m_parentById.insert(entry.nodeId, entry.parentNodeId);
        }
        if (!entry.trackId.isEmpty()) {
            m_trackIdToNodeId.insert(entry.trackId, entry.nodeId);
        }
    }
}

void LibraryModel::rebuildProjectionIndexes()
{
    m_rowByNodeId.clear();
    for (int row = 0; row < m_entries.size(); ++row) {
        const Entry &entry = m_entries.at(row);
        if (!entry.nodeId.isEmpty()) {
            m_rowByNodeId.insert(entry.nodeId, row);
        }
    }
}

void LibraryModel::setProjectionNodeIds(const QVector<QString> &nodeIds)
{
    const QString playingNodeId = nodeIdForTrackId(m_playingTrackId);
    QVector<Entry> projectedEntries;
    projectedEntries.reserve(nodeIds.size());
    QSet<QString> projectedNodeIds;

    for (const QString &nodeId : nodeIds) {
        if (nodeId.isEmpty() || nodeId == m_rootNodeId || projectedNodeIds.contains(nodeId)) {
            continue;
        }
        const auto entryIt = m_nodeById.constFind(nodeId);
        if (entryIt == m_nodeById.cend()) {
            continue;
        }

        Entry entry = entryIt.value();
        entry.isFocused = entry.nodeId == m_focusedNodeId;
        entry.isPlaying = !playingNodeId.isEmpty() && entry.nodeId == playingNodeId;
        projectedEntries.append(entry);
        projectedNodeIds.insert(nodeId);
    }

    beginResetModel();
    m_entries = projectedEntries;
    rebuildProjectionIndexes();
    endResetModel();
}

QVector<QString> LibraryModel::searchProjectionNodeIds(const QString &searchQuery) const
{
    const QString trimmedQuery = searchQuery.trimmed();
    QVector<QString> nodeIds;
    if (trimmedQuery.isEmpty()) {
        return nodeIds;
    }

    for (const QString &nodeId : m_nodeOrder) {
        if (nodeId == m_rootNodeId) {
            continue;
        }
        const auto entryIt = m_nodeById.constFind(nodeId);
        if (entryIt != m_nodeById.cend() && entryMatchesSearch(entryIt.value(), trimmedQuery)) {
            nodeIds.append(nodeId);
        }
    }
    return nodeIds;
}

#if SERIONA_HAS_BACKEND
void LibraryModel::setPlaylistTreeSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot)
{
    m_treeStore.setSnapshot(snapshot);

    for (const LibraryTreeStore::SkippedChild &skippedChild : m_treeStore.skippedChildren()) {
        qWarning().noquote() << QStringLiteral("LibraryModel skipped missing child node %1 under %2").arg(skippedChild.childNodeId, skippedChild.parentNodeId);
    }
    if (!m_treeStore.missingRootNodeId().isEmpty()) {
        qWarning().noquote() << QStringLiteral("LibraryModel missing root node %1; falling back to snapshot node order").arg(m_treeStore.missingRootNodeId());
    }

    QHash<QString, Entry> nodeById;
    QHash<QString, QVector<QString>> childrenById;
    QHash<QString, QString> parentById;
    QHash<QString, QString> trackIdToNodeId;

    for (const QString &nodeId : m_treeStore.nodeOrder()) {
        const LibraryTreeStore::Node *node = m_treeStore.nodeById(nodeId);
        if (node == nullptr) {
            continue;
        }

        QString parentName;
        const QString parentNodeId = m_treeStore.parentNodeId(nodeId);
        if (!parentNodeId.isEmpty()) {
            const LibraryTreeStore::Node *parentNode = m_treeStore.nodeById(parentNodeId);
            if (parentNode != nullptr) {
                parentName = parentNode->displayName;
            }
        }

        Entry entry = entryFromNode(*node, parentName);
        entry.parentNodeId = parentNodeId;
        nodeById.insert(nodeId, entry);
        childrenById.insert(nodeId, m_treeStore.childNodeIds(nodeId));
        if (!parentNodeId.isEmpty()) {
            parentById.insert(nodeId, parentNodeId);
        }
        if (!node->trackId.isEmpty() && !trackIdToNodeId.contains(node->trackId)) {
            trackIdToNodeId.insert(node->trackId, nodeId);
        }
        if (!node->logicalTrackId.isEmpty() && !trackIdToNodeId.contains(node->logicalTrackId)) {
            trackIdToNodeId.insert(node->logicalTrackId, nodeId);
        }
    }

    beginResetModel();
    m_nodeById = nodeById;
    m_childrenById = childrenById;
    m_parentById = parentById;
    m_trackIdToNodeId = trackIdToNodeId;
    m_nodeOrder = m_treeStore.nodeOrder();
    m_rootNodeId = m_treeStore.rootNodeId();
    m_rootProjectionNodeIds = m_treeStore.rootChildNodeIds();
    m_focusedNodeId.clear();
    m_playingTrackId.clear();
    m_entries.clear();
    const QString playingNodeId = nodeIdForTrackId(m_playingTrackId);
    QSet<QString> projectedNodeIds;
    for (const QString &nodeId : m_rootProjectionNodeIds) {
        if (nodeId.isEmpty() || nodeId == m_rootNodeId || projectedNodeIds.contains(nodeId)) {
            continue;
        }
        const auto entryIt = m_nodeById.constFind(nodeId);
        if (entryIt == m_nodeById.cend()) {
            continue;
        }
        Entry entry = entryIt.value();
        entry.isFocused = false;
        entry.isPlaying = !playingNodeId.isEmpty() && entry.nodeId == playingNodeId;
        m_entries.append(entry);
        projectedNodeIds.insert(nodeId);
    }
    rebuildProjectionIndexes();
    m_version = snapshot.version;
    endResetModel();
}
#endif

LibraryController::LibraryController(QObject *parent)
    : QObject(parent)
    , m_model(this)
{
    reconcileBrowsingState({}, {});
}

LibraryModel *LibraryController::model()
{
    return &m_model;
}

QString LibraryController::currentFolderName() const
{
    return m_currentFolderName;
}

bool LibraryController::canGoBack() const
{
    return !m_currentFolderNodeId.isEmpty();
}

QString LibraryController::searchQuery() const
{
    return m_searchQuery;
}

QString LibraryController::focusedNodeId() const
{
    return m_focusedNodeId;
}

void LibraryController::setFocusedNodeId(const QString &nodeId)
{
    const bool previousCanGoBack = canGoBack();
    if (!nodeId.isEmpty() && !m_model.containsNodeId(nodeId)) {
        return;
    }
    if (m_focusedNodeId == nodeId) {
        return;
    }

    m_focusedNodeId = nodeId;
    m_model.setFocusedNodeId(m_focusedNodeId);
    emit focusedNodeIdChanged();
    if (previousCanGoBack != canGoBack()) {
        emit canGoBackChanged();
    }
}

QString LibraryController::selectedBrowserNodeId() const
{
    return m_selectedBrowserNodeId;
}

void LibraryController::setSelectedBrowserNodeId(const QString &nodeId)
{
    const bool previousCanGoBack = canGoBack();
    if (!nodeId.isEmpty() && !m_model.containsNodeId(nodeId)) {
        return;
    }
    if (m_selectedBrowserNodeId == nodeId) {
        setFocusedNodeId(nodeId);
        return;
    }

    m_selectedBrowserNodeId = nodeId;
    setFocusedNodeId(nodeId);
    emit selectedBrowserNodeIdChanged();
    if (previousCanGoBack != canGoBack()) {
        emit canGoBackChanged();
    }
}

QString LibraryController::scrollRequest() const
{
    return m_scrollRequest;
}

int LibraryController::visibleNodeCount() const
{
    return m_visibleNodeCount;
}

QString LibraryController::scanStatus() const
{
    return m_scanStatus;
}

int LibraryController::scanProgress() const
{
    return m_scanProgress;
}

QString LibraryController::lastError() const
{
    return m_lastError;
}

QString LibraryController::savedRootPath() const
{
    return m_savedRootPath;
}

QString LibraryController::playingTrackId() const
{
    return m_playingTrackId;
}

void LibraryController::setPlayingTrackId(const QString &trackId)
{
    if (m_playingTrackId == trackId) {
        return;
    }

    m_playingTrackId = trackId;
    m_model.setPlayingTrackId(m_playingTrackId);
    emit playingTrackIdChanged();

    if (!m_followCurrentlyPlaying) {
        return;
    }

    const QString playingNodeId = m_model.nodeIdForTrackId(m_playingTrackId);
    if (!playingNodeId.isEmpty()) {
        if (!showNodeInBrowserProjection(playingNodeId)) {
            return;
        }
        setSelectedBrowserNodeId(playingNodeId);
        requestScrollToNode(playingNodeId);
    }
}

bool LibraryController::followCurrentlyPlaying() const
{
    return m_followCurrentlyPlaying;
}

void LibraryController::setFollowCurrentlyPlaying(bool follow)
{
    if (m_followCurrentlyPlaying == follow) {
        return;
    }

    m_followCurrentlyPlaying = follow;
    emit followCurrentlyPlayingChanged();

    if (!m_followCurrentlyPlaying) {
        return;
    }

    const QString playingNodeId = m_model.nodeIdForTrackId(m_playingTrackId);
    if (!playingNodeId.isEmpty()) {
        if (!showNodeInBrowserProjection(playingNodeId)) {
            return;
        }
        setSelectedBrowserNodeId(playingNodeId);
        requestScrollToNode(playingNodeId);
    }
}

bool LibraryController::libraryEmpty() const
{
    return !m_model.hasLibraryContent();
}

bool LibraryController::backendAvailable() const
{
    return m_backendAvailable;
}

QString LibraryController::libraryState() const
{
    if (!m_backendAvailable) {
        return QStringLiteral("backendUnavailable");
    }
    if (libraryEmpty()) {
        return QStringLiteral("empty");
    }
    return QStringLiteral("ready");
}

#if SERIONA_HAS_BACKEND
void LibraryController::setCommandExecutor(CommandExecutor executor)
{
    m_commandExecutor = std::move(executor);
}

void LibraryController::setScanExecutor(ScanExecutor executor)
{
    m_scanExecutor = std::move(executor);
    setBackendAvailable(static_cast<bool>(m_scanExecutor));
}

void LibraryController::setPlaylistTreeSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot)
{
    const bool previousLibraryEmpty = libraryEmpty();
    const QString previousLibraryState = libraryState();
    const bool previousBackendAvailable = m_backendAvailable;
    const QVector<QString> focusedFallbackChain = m_model.ancestorChainForNode(m_focusedNodeId);
    const QVector<QString> selectedFallbackChain = m_model.ancestorChainForNode(m_selectedBrowserNodeId);

    m_backendAvailable = true;
    m_model.setPlaylistTreeSnapshot(snapshot);
    reconcileBrowsingState(focusedFallbackChain, selectedFallbackChain);
    if (previousBackendAvailable != m_backendAvailable) {
        emit backendAvailableChanged();
    }
    emitLibraryStateChanges(previousLibraryEmpty, previousLibraryState);
}

void LibraryController::applyPlayerStateSnapshot(const seriona::control::PlayerStateSnapshot &snapshot, bool forceReapply)
{
    const QString trackId = playingTrackIdFromSnapshot(snapshot);
    if (forceReapply && !trackId.isEmpty() && m_playingTrackId == trackId) {
        setPlayingTrackId(QString());
    }
    setPlayingTrackId(trackId);
}

void LibraryController::applyLibraryStateSnapshot(const seriona::control::LibraryStateSnapshot &snapshot)
{
    setBackendAvailable(true);
    const LibrarySnapshotViewState mapped = mapLibrarySnapshot(snapshot);
    setScanStatus(mapped.scanStatus);
    setScanProgress(mapped.scanProgress);
    setLastError(mapped.lastError);
    if (snapshot.libraryTree.has_value()) {
        setPlaylistTreeSnapshot(*snapshot.libraryTree);
    }
}
#endif

void LibraryController::setSearchQuery(const QString &query)
{
    if (m_searchQuery == query) {
        return;
    }

    m_searchQuery = query;
    applyBrowsingState();
    emit searchQueryChanged();
}

void LibraryController::enterFolder(int index)
{
    const LibraryModel::Entry *entry = m_model.entryAt(index);
    if (entry == nullptr || entry->type != QStringLiteral("folder")) {
        return;
    }

    enterFolder(entry->nodeId);
}

void LibraryController::enterFolder(const QString &nodeId)
{
    const LibraryModel::Entry *entry = m_model.entryByNodeId(nodeId);
    if (entry == nullptr || !entry->isFolder) {
        return;
    }

    const bool previousCanGoBack = canGoBack();
    const bool folderChanged = m_currentFolderNodeId != nodeId;
    const bool nameChanged = m_currentFolderName != entry->name;

    m_currentFolderNodeId = nodeId;
    m_currentFolderName = entry->name;
    setSelectedBrowserNodeId(nodeId);
    applyBrowsingState();

    if (nameChanged) {
        emit currentFolderNameChanged();
    }
    if (previousCanGoBack != canGoBack() || folderChanged) {
        emit canGoBackChanged();
    }
}

void LibraryController::goBack()
{
    if (!canGoBack()) {
        return;
    }

    if (!m_currentFolderNodeId.isEmpty()) {
        const bool previousCanGoBack = canGoBack();
        const QString previousFolderNodeId = m_currentFolderNodeId;
        const QString parentNodeId = m_model.parentNodeId(m_currentFolderNodeId);
        const QString rootNodeId = m_model.firstNodeId();

        if (!parentNodeId.isEmpty() && parentNodeId != rootNodeId) {
            enterFolder(parentNodeId);
        } else {
            m_currentFolderNodeId.clear();
            m_currentFolderName = QStringLiteral("My Music");
            if (!previousFolderNodeId.isEmpty()) {
                setSelectedBrowserNodeId(previousFolderNodeId);
                requestScrollToNode(previousFolderNodeId);
            }
            applyBrowsingState();
            emit currentFolderNameChanged();
            if (previousCanGoBack != canGoBack()) {
                emit canGoBackChanged();
            }
        }
        return;
    }
}

bool LibraryController::refresh()
{
    if (m_savedRootPath.isEmpty()) {
        setScanStatus(QStringLiteral("error"));
        setScanProgress(0);
        setLastError(tr("尚未选择曲库文件夹"));
        return false;
    }

    return requestScanForRoot(m_savedRootPath);
}

void LibraryController::clearSavedRootPath(const QString &message)
{
    setSavedRootPath(QString());
    setScanStatus(QStringLiteral("error"));
    setScanProgress(0);
    setLastError(message);
}

bool LibraryController::scanLibrary(const QUrl &rootUrl)
{
    const QString rootPath = localDirectoryPath(rootUrl);
    if (rootPath.isEmpty()) {
        setScanStatus(QStringLiteral("error"));
        setScanProgress(0);
        setLastError(tr("请选择有效的曲库文件夹"));
        return false;
    }

    return requestScanForRoot(rootPath);
}

bool LibraryController::requestScanForRoot(const QString &rootPath)
{
    setScanStatus(QStringLiteral("running"));
    setScanProgress(0);
    setLastError(QString());

#if SERIONA_HAS_BACKEND
    if (!m_scanExecutor) {
        setBackendAvailable(false);
        setScanStatus(QStringLiteral("error"));
        setLastError(tr("后端扫描服务不可用"));
        return false;
    }

    const seriona::control::MediaControllerCommandResult result = m_scanExecutor(rootPath);
    if (!result.accepted) {
        setScanStatus(QStringLiteral("error"));
        setLastError(result.message.empty() ? tr("后端拒绝扫描请求") : toQString(result.message));
        return false;
    }
#else
    setBackendAvailable(false);
    setScanStatus(QStringLiteral("error"));
    setLastError(tr("后端扫描服务不可用"));
    return false;
#endif

    setSavedRootPath(rootPath);
    applyBrowsingState();
    return true;
}

void LibraryController::setScanStatus(const QString &status)
{
    if (m_scanStatus == status) {
        return;
    }

    m_scanStatus = status;
    emit scanStatusChanged();
}

void LibraryController::setScanProgress(int progress)
{
    progress = qBound(0, progress, 100);
    if (m_scanProgress == progress) {
        return;
    }

    m_scanProgress = progress;
    emit scanProgressChanged();
}

void LibraryController::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }

    m_lastError = error;
    emit lastErrorChanged();
}

void LibraryController::setSavedRootPath(const QString &rootPath)
{
    if (m_savedRootPath == rootPath) {
        return;
    }

    m_savedRootPath = rootPath;
    emit savedRootPathChanged();
}

void LibraryController::setBackendAvailable(bool available)
{
    if (m_backendAvailable == available) {
        return;
    }

    const bool previousLibraryEmpty = libraryEmpty();
    const QString previousLibraryState = libraryState();
    m_backendAvailable = available;
    emit backendAvailableChanged();
    emitLibraryStateChanges(previousLibraryEmpty, previousLibraryState);
}

void LibraryController::emitLibraryStateChanges(bool previousLibraryEmpty, const QString &previousLibraryState)
{
    if (previousLibraryEmpty != libraryEmpty()) {
        emit libraryEmptyChanged();
    }
    if (previousLibraryState != libraryState()) {
        emit libraryStateChanged();
    }
}

void LibraryController::playItem(int index)
{
    activateTrack(m_model.entryAt(index));
}

void LibraryController::playItem(const QString &nodeId)
{
    activateTrack(m_model.entryByNodeId(nodeId));
}

void LibraryController::locateCurrentSong()
{
    const QString playingNodeId = m_model.nodeIdForTrackId(m_playingTrackId);
    if (playingNodeId.isEmpty()) {
        return;
    }

    if (!showNodeInBrowserProjection(playingNodeId)) {
        return;
    }
    setSelectedBrowserNodeId(playingNodeId);
    requestScrollToNode(playingNodeId);
}

void LibraryController::activateTrack(const LibraryModel::Entry *entry)
{
    if (entry == nullptr || entry->isFolder || entry->trackId.isEmpty()) {
        return;
    }

#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::SelectTrack;
    command.track = trackIdentityForEntry(*entry);
    submitCommand(command);
#endif
}

#if SERIONA_HAS_BACKEND
void LibraryController::submitCommand(const seriona::control::MediaControlCommand &command)
{
    if (!m_commandExecutor) {
        return;
    }

    static_cast<void>(m_commandExecutor(command));
}
#endif

void LibraryController::clearSearch()
{
    if (m_searchQuery.isEmpty()) {
        return;
    }

    m_searchQuery.clear();
    applyBrowsingState();
    emit searchQueryChanged();
}

void LibraryController::submitSearch()
{
    if (m_model.rowCount() <= 0 || m_visibleNodeCount <= 0) {
        return;
    }

    const QString nodeId = m_searchQuery.trimmed().isEmpty()
        ? m_model.firstVisibleNodeId()
        : m_model.firstVisibleMatchingNodeId();
    if (nodeId.isEmpty()) {
        return;
    }

    setSelectedBrowserNodeId(nodeId);
    requestScrollToNode(nodeId);
}

void LibraryController::selectBrowserNode(const QString &nodeId)
{
    setSelectedBrowserNodeId(nodeId);
}

void LibraryController::requestScrollToNode(const QString &nodeId)
{
    if (!m_model.containsNodeId(nodeId)) {
        return;
    }

    m_scrollRequest = nodeId;
    emit scrollRequestChanged();
}

int LibraryController::rowForNodeId(const QString &nodeId) const
{
    return m_model.rowForNodeId(nodeId);
}

void LibraryController::applyBrowsingState()
{
    m_model.applyBrowsingState(m_focusedNodeId, m_playingTrackId, m_searchQuery, m_currentFolderNodeId);
    updateVisibleNodeCount();
}

bool LibraryController::showNodeInBrowserProjection(const QString &nodeId)
{
    const LibraryModel::Entry *entry = m_model.entryByNodeId(nodeId);
    if (entry == nullptr) {
        return false;
    }

    const bool previousCanGoBack = canGoBack();
    const QString rootNodeId = m_model.firstNodeId();
    const QString parentNodeId = m_model.parentNodeId(nodeId);
    QString folderNodeId;

    if (!parentNodeId.isEmpty() && parentNodeId != rootNodeId) {
        folderNodeId = parentNodeId;
    }

    QString folderName = QStringLiteral("My Music");
    if (!folderNodeId.isEmpty()) {
        const LibraryModel::Entry *folderEntry = m_model.entryByNodeId(folderNodeId);
        if (folderEntry == nullptr || !folderEntry->isFolder) {
            return false;
        }
        folderName = folderEntry->name;
    }

    const bool folderChanged = m_currentFolderNodeId != folderNodeId;
    const bool folderNameChanged = m_currentFolderName != folderName;
    const bool searchChanged = !m_searchQuery.isEmpty();

    m_currentFolderNodeId = folderNodeId;
    m_currentFolderName = folderName;
    if (searchChanged) {
        m_searchQuery.clear();
    }

    applyBrowsingState();

    if (searchChanged) {
        emit searchQueryChanged();
    }
    if (folderNameChanged) {
        emit currentFolderNameChanged();
    }
    if (previousCanGoBack != canGoBack() || folderChanged) {
        emit canGoBackChanged();
    }

    return m_model.rowForNodeId(nodeId) >= 0;
}

void LibraryController::updateVisibleNodeCount()
{
    const int visibleNodeCount = m_model.visibleNodeCount();
    if (m_visibleNodeCount == visibleNodeCount) {
        return;
    }

    m_visibleNodeCount = visibleNodeCount;
    emit visibleNodeCountChanged();
}

void LibraryController::reconcileBrowsingState(const QVector<QString> &focusedFallbackChain, const QVector<QString> &selectedFallbackChain)
{
    const QString previousFocusedNodeId = m_focusedNodeId;
    const QString previousSelectedNodeId = m_selectedBrowserNodeId;
    const bool previousCanGoBack = canGoBack();

    if (!m_focusedNodeId.isEmpty() && !m_model.containsNodeId(m_focusedNodeId)) {
        m_focusedNodeId = firstExistingNode(focusedFallbackChain);
    }
    if (m_focusedNodeId.isEmpty()) {
        m_focusedNodeId = m_model.firstNodeId();
    }

    if (!m_selectedBrowserNodeId.isEmpty() && !m_model.containsNodeId(m_selectedBrowserNodeId)) {
        m_selectedBrowserNodeId = firstExistingNode(selectedFallbackChain);
    }
    if (!m_currentFolderNodeId.isEmpty() && !m_model.containsNodeId(m_currentFolderNodeId)) {
        m_currentFolderNodeId.clear();
        m_currentFolderName = QStringLiteral("My Music");
        emit currentFolderNameChanged();
    }
    if (m_selectedBrowserNodeId.isEmpty()) {
        m_selectedBrowserNodeId = m_focusedNodeId;
    }

    applyBrowsingState();

    if (previousFocusedNodeId != m_focusedNodeId) {
        emit focusedNodeIdChanged();
    }
    if (previousSelectedNodeId != m_selectedBrowserNodeId) {
        emit selectedBrowserNodeIdChanged();
    }
    if (previousCanGoBack != canGoBack()) {
        emit canGoBackChanged();
    }
}

QString LibraryController::firstExistingNode(const QVector<QString> &nodeIds) const
{
    for (const QString &nodeId : nodeIds) {
        if (m_model.containsNodeId(nodeId)) {
            return nodeId;
        }
    }
    return m_model.firstNodeId();
}

}
