#include "library_model.h"

#include <QByteArray>
#include <QDebug>
#include <QFileInfo>
#include <QUrl>

#if SERIONA_HAS_BACKEND
#include <QSet>

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

QString uiScanStatus(seriona::control::LibraryScanStatus status)
{
    switch (status) {
    case seriona::control::LibraryScanStatus::Idle:
    case seriona::control::LibraryScanStatus::Stopped:
        return QStringLiteral("pending");
    case seriona::control::LibraryScanStatus::Scanning:
        return QStringLiteral("running");
    case seriona::control::LibraryScanStatus::Completed:
        return QStringLiteral("completed");
    case seriona::control::LibraryScanStatus::Error:
        return QStringLiteral("error");
    }

    return QStringLiteral("pending");
}

int uiScanProgress(const seriona::control::LibraryStateSnapshot &snapshot)
{
    if (snapshot.scanStatus == seriona::control::LibraryScanStatus::Completed) {
        return 100;
    }
    if (!snapshot.scanProgress.has_value() || snapshot.scanProgress->filesDiscovered == 0U) {
        return 0;
    }

    const std::uint64_t scanned = std::min(snapshot.scanProgress->filesScanned, snapshot.scanProgress->filesDiscovered);
    return static_cast<int>((scanned * 100U) / snapshot.scanProgress->filesDiscovered);
}

QString scannerErrorMessage(const seriona::scanner::ScannerError &error)
{
    if (!error.message.empty()) {
        return toQString(error.message);
    }
    if (!error.detail.empty()) {
        return toQString(error.detail);
    }

    return QStringLiteral("扫描失败");
}

bool isFolderKind(seriona::scanner::PlaylistNodeKind kind)
{
    return kind != seriona::scanner::PlaylistNodeKind::Track;
}

QString typeForKind(seriona::scanner::PlaylistNodeKind kind)
{
    return isFolderKind(kind) ? QStringLiteral("folder") : QStringLiteral("file");
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

QString formatFromSong(const seriona::scanner::SongMetadata &song)
{
    const std::filesystem::path &path = !song.sourceFilePath.empty() ? song.sourceFilePath : song.filePath;
    QString extension = QString::fromStdString(path.extension().string());
    if (extension.startsWith(QLatin1Char('.'))) {
        extension.remove(0, 1);
    }
    return extension.toUpper();
}

bool isTrackNode(const seriona::scanner::PlaylistNode &node)
{
    return node.kind == seriona::scanner::PlaylistNodeKind::Track && node.song.has_value() && !node.song->trackId.empty();
}

int descendantTrackCount(const QString &nodeId,
                         const QHash<QString, const seriona::scanner::PlaylistNode *> &sourceNodeById,
                         const QHash<QString, QVector<QString>> &childrenById,
                         QSet<QString> &visited)
{
    if (visited.contains(nodeId)) {
        return 0;
    }
    visited.insert(nodeId);

    const seriona::scanner::PlaylistNode *node = sourceNodeById.value(nodeId, nullptr);
    if (node == nullptr) {
        return 0;
    }
    if (isTrackNode(*node)) {
        return 1;
    }

    int total = 0;
    for (const QString &childNodeId : childrenById.value(nodeId)) {
        total += descendantTrackCount(childNodeId, sourceNodeById, childrenById, visited);
    }
    return total;
}

LibraryModel::Entry entryFromNode(const seriona::scanner::PlaylistNode &node, const QString &parentName, int songCount)
{
    LibraryModel::Entry entry;
    entry.nodeId = toQString(node.nodeId);
    entry.type = typeForKind(node.kind);
    entry.parentName = parentName;
    entry.songCount = songCount;
    entry.isFolder = isFolderKind(node.kind);

    if (entry.isFolder) {
        entry.name = toQString(node.displayName);
        return entry;
    }

    if (!node.song.has_value()) {
        entry.title = toQString(node.displayName);
        return entry;
    }

    const seriona::scanner::SongMetadata &song = *node.song;
    entry.trackId = toQString(song.trackId);
    entry.title = song.title.empty() ? toQString(node.displayName) : toQString(song.title);
    entry.artist = toQString(song.artist);
    entry.album = toQString(song.album);
    entry.format = formatFromSong(song);
    entry.sampleRate = song.sampleRate.has_value() ? static_cast<int>(*song.sampleRate) : 0;
    entry.bitDepth = song.bitDepth.has_value() ? static_cast<int>(*song.bitDepth) : 0;
    entry.duration = song.duration.has_value() ? formatDuration(*song.duration) : QString();
    if (song.artworkPath && !song.artworkPath->empty()) {
        entry.artworkSource = QUrl::fromLocalFile(QString::fromStdString(song.artworkPath->string())).toString();
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

LibraryModel::Entry mockFolderEntry(const QString &nodeId, const QString &name, int songCount, const QString &duration)
{
    LibraryModel::Entry entry;
    entry.type = QStringLiteral("folder");
    entry.name = name;
    entry.parentName = QStringLiteral("Music");
    entry.songCount = songCount;
    entry.duration = duration;
    entry.isFolder = true;
    entry.nodeId = nodeId;
    return entry;
}

LibraryModel::Entry mockFileEntry(const QString &nodeId,
                                  const QString &trackId,
                                  const QString &title,
                                  const QString &artist,
                                  const QString &album,
                                  const QString &duration,
                                  const QString &format,
                                  int sampleRate,
                                  int bitDepth)
{
    LibraryModel::Entry entry;
    entry.type = QStringLiteral("file");
    entry.title = title;
    entry.artist = artist;
    entry.album = album;
    entry.duration = duration;
    entry.format = format;
    entry.sampleRate = sampleRate;
    entry.bitDepth = bitDepth;
    entry.nodeId = nodeId;
    entry.trackId = trackId;
    return entry;
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
    case IsExpandedRole:
        return entry->isExpanded;
    case ParentNodeIdRole:
        return entry->parentNodeId;
    case DepthRole:
        return entry->depth;
    case IsVisibleRole:
        return entry->isVisible;
    case MatchesSearchRole:
        return entry->matchesSearch;
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
            {IsExpandedRole, "isExpanded"},
            {ParentNodeIdRole, "parentNodeId"},
            {DepthRole, "depth"},
            {IsVisibleRole, "isVisible"},
            {MatchesSearchRole, "matchesSearch"},
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
    return !nodeId.isEmpty() && m_rowByNodeId.contains(nodeId);
}

int LibraryModel::rowForNodeId(const QString &nodeId) const
{
    return m_rowByNodeId.value(nodeId, -1);
}

QString LibraryModel::firstNodeId() const
{
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

bool LibraryModel::setNodeExpanded(const QString &nodeId, bool expanded)
{
    const int row = rowForNodeId(nodeId);
    if (row < 0) {
        return false;
    }

    return setEntryRoleFlag(row, IsExpandedRole, expanded, true);
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

void LibraryModel::applyBrowsingState(const QSet<QString> &expandedNodeIds, const QString &focusedNodeId, const QString &playingTrackId, const QString &searchQuery, const QString &browserRootNodeId)
{
    m_focusedNodeId = containsNodeId(focusedNodeId) ? focusedNodeId : QString();
    m_playingTrackId = playingTrackId;
    const QString playingNodeId = nodeIdForTrackId(m_playingTrackId);
    const QString trimmedQuery = searchQuery.trimmed();

    // 优化：提前计算可见节点集合，避免重复计算
    QSet<QString> visibleNodeIds;
    if (!browserRootNodeId.isEmpty()) {
        // 进入文件夹模式：只有直接子节点可见
        for (const Entry &entry : m_entries) {
            if (!entry.nodeId.isEmpty() && m_parentById.value(entry.nodeId) == browserRootNodeId) {
                visibleNodeIds.insert(entry.nodeId);
            }
        }
    } else if (trimmedQuery.isEmpty()) {
        // 展开/折叠模式：计算所有可见节点
        const QString rootNodeId = firstNodeId();
        for (const Entry &entry : m_entries) {
            if (entry.nodeId.isEmpty() || entry.depth == 0) {
                visibleNodeIds.insert(entry.nodeId);
                continue;
            }
            
            QString parentNodeId = m_parentById.value(entry.nodeId);
            if (parentNodeId.isEmpty() || parentNodeId == rootNodeId) {
                visibleNodeIds.insert(entry.nodeId);
                continue;
            }
            
            // 检查所有父节点是否展开
            bool allParentsExpanded = true;
            QSet<QString> visited;
            while (!parentNodeId.isEmpty() && !visited.contains(parentNodeId)) {
                visited.insert(parentNodeId);
                if (parentNodeId != rootNodeId && !expandedNodeIds.contains(parentNodeId)) {
                    allParentsExpanded = false;
                    break;
                }
                parentNodeId = m_parentById.value(parentNodeId);
            }
            
            if (allParentsExpanded) {
                visibleNodeIds.insert(entry.nodeId);
            }
        }
    }

    // 批量更新：收集所有变化的行，最后一次性发送信号
    int firstChangedRow = -1;
    int lastChangedRow = -1;
    QSet<int> changedRoles;

    for (int row = 0; row < m_entries.size(); ++row) {
        const Entry &entry = m_entries.at(row);
        
        // 优化后的可见性计算：使用预计算的集合
        bool isVisible;
        if (!trimmedQuery.isEmpty()) {
            // 搜索模式：检查匹配
            const bool matchesSearch = entry.isFolder
                ? (entry.name.contains(trimmedQuery, Qt::CaseInsensitive) || entry.parentName.contains(trimmedQuery, Qt::CaseInsensitive))
                : (entry.title.contains(trimmedQuery, Qt::CaseInsensitive) || entry.artist.contains(trimmedQuery, Qt::CaseInsensitive) 
                   || entry.album.contains(trimmedQuery, Qt::CaseInsensitive) || entry.format.contains(trimmedQuery, Qt::CaseInsensitive));
            isVisible = matchesSearch;
            
            if (setEntryRoleFlag(row, MatchesSearchRole, matchesSearch, false)) {
                changedRoles.insert(MatchesSearchRole);
                if (firstChangedRow < 0) firstChangedRow = row;
                lastChangedRow = row;
            }
        } else {
            // 非搜索模式：使用预计算的可见集合
            isVisible = visibleNodeIds.contains(entry.nodeId);
        }
        
        // 批量更新，不立即通知
        if (setEntryRoleFlag(row, IsExpandedRole, expandedNodeIds.contains(entry.nodeId), false)) {
            changedRoles.insert(IsExpandedRole);
            if (firstChangedRow < 0) firstChangedRow = row;
            lastChangedRow = row;
        }
        if (setEntryRoleFlag(row, IsFocusedRole, entry.nodeId == m_focusedNodeId, false)) {
            changedRoles.insert(IsFocusedRole);
            if (firstChangedRow < 0) firstChangedRow = row;
            lastChangedRow = row;
        }
        if (setEntryRoleFlag(row, IsPlayingRole, !playingNodeId.isEmpty() && entry.nodeId == playingNodeId, false)) {
            changedRoles.insert(IsPlayingRole);
            if (firstChangedRow < 0) firstChangedRow = row;
            lastChangedRow = row;
        }
        if (setEntryRoleFlag(row, IsVisibleRole, isVisible, false)) {
            changedRoles.insert(IsVisibleRole);
            if (firstChangedRow < 0) firstChangedRow = row;
            lastChangedRow = row;
        }
    }

    // 一次性发送 dataChanged 信号，覆盖所有变化的行
    if (firstChangedRow >= 0 && lastChangedRow >= 0) {
        const QModelIndex firstIndex = index(firstChangedRow, 0);
        const QModelIndex lastIndex = index(lastChangedRow, 0);
        emit dataChanged(firstIndex, lastIndex, changedRoles.values());
    }
}

int LibraryModel::visibleNodeCount() const
{
    int count = 0;
    for (const Entry &entry : m_entries) {
        if (entry.isVisible) {
            ++count;
        }
    }
    return count;
}

QString LibraryModel::firstVisibleNodeId() const
{
    for (const Entry &entry : m_entries) {
        if (entry.isVisible && !entry.nodeId.isEmpty()) {
            return entry.nodeId;
        }
    }
    return {};
}

QString LibraryModel::firstVisibleMatchingNodeId() const
{
    for (const Entry &entry : m_entries) {
        if (entry.isVisible && entry.matchesSearch && !entry.nodeId.isEmpty()) {
            return entry.nodeId;
        }
    }
    return {};
}

void LibraryModel::setEntries(const QVector<Entry> &entries)
{
    beginResetModel();
    m_entries = entries;
    m_childrenById.clear();
    m_parentById.clear();
    m_focusedNodeId.clear();
    m_playingTrackId.clear();
    rebuildEntryIndexes();
    m_version = 0;
    endResetModel();
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
    case IsExpandedRole:
        flag = &entry.isExpanded;
        break;
    case IsVisibleRole:
        flag = &entry.isVisible;
        break;
    case MatchesSearchRole:
        flag = &entry.matchesSearch;
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

bool LibraryModel::entryVisibleByExpansion(const Entry &entry, const QSet<QString> &expandedNodeIds) const
{
    if (entry.nodeId.isEmpty()) {
        return true;
    }

    const QString rootNodeId = firstNodeId();
    QString parentNodeId = m_parentById.value(entry.nodeId);
    if (parentNodeId.isEmpty() || parentNodeId == rootNodeId) {
        return true;
    }

    QSet<QString> visited;
    while (!parentNodeId.isEmpty() && !visited.contains(parentNodeId)) {
        visited.insert(parentNodeId);
        if (parentNodeId != rootNodeId && !expandedNodeIds.contains(parentNodeId)) {
            return false;
        }
        parentNodeId = m_parentById.value(parentNodeId);
    }

    return true;
}

bool LibraryModel::entryVisibleInBrowserRoot(const Entry &entry, const QString &browserRootNodeId) const
{
    if (entry.nodeId.isEmpty()) {
        return true;
    }
    return m_parentById.value(entry.nodeId) == browserRootNodeId;
}

void LibraryModel::rebuildEntryIndexes()
{
    m_nodeById.clear();
    m_trackIdToNodeId.clear();
    m_rowByNodeId.clear();

    for (int row = 0; row < m_entries.size(); ++row) {
        const Entry &entry = m_entries.at(row);
        if (!entry.nodeId.isEmpty()) {
            m_nodeById.insert(entry.nodeId, entry);
            m_rowByNodeId.insert(entry.nodeId, row);
        }
        if (!entry.nodeId.isEmpty() && !entry.parentNodeId.isEmpty() && !m_parentById.contains(entry.nodeId)) {
            m_parentById.insert(entry.nodeId, entry.parentNodeId);
        }
        if (!entry.trackId.isEmpty()) {
            m_trackIdToNodeId.insert(entry.trackId, entry.nodeId);
        }
    }
}

#if SERIONA_HAS_BACKEND
void LibraryModel::setPlaylistTreeSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot)
{
    QHash<QString, const seriona::scanner::PlaylistNode *> sourceNodeById;
    QVector<QString> sourceOrderNodeIds;
    sourceOrderNodeIds.reserve(static_cast<qsizetype>(snapshot.nodes.size()));

    for (const seriona::scanner::PlaylistNode &node : snapshot.nodes) {
        const QString nodeId = toQString(node.nodeId);
        if (nodeId.isEmpty()) {
            qWarning().noquote() << QStringLiteral("LibraryModel skipped node with empty nodeId");
            continue;
        }
        sourceNodeById.insert(nodeId, &node);
        sourceOrderNodeIds.append(nodeId);
    }

    QHash<QString, QVector<QString>> childrenById;
    QHash<QString, QString> parentById;
    for (const QString &nodeId : sourceOrderNodeIds) {
        childrenById.insert(nodeId, {});
        const seriona::scanner::PlaylistNode *node = sourceNodeById.value(nodeId, nullptr);
        if (node != nullptr && node->parentNodeId.has_value()) {
            const QString parentNodeId = toQString(*node->parentNodeId);
            if (sourceNodeById.contains(parentNodeId)) {
                parentById.insert(nodeId, parentNodeId);
            }
        }
    }

    for (const QString &nodeId : sourceOrderNodeIds) {
        const seriona::scanner::PlaylistNode *node = sourceNodeById.value(nodeId, nullptr);
        if (node == nullptr) {
            continue;
        }

        QVector<QString> validChildren;
        validChildren.reserve(static_cast<qsizetype>(node->childNodeIds.size()));
        for (const std::string &rawChildNodeId : node->childNodeIds) {
            const QString childNodeId = toQString(rawChildNodeId);
            if (!sourceNodeById.contains(childNodeId)) {
                qWarning().noquote() << QStringLiteral("LibraryModel skipped missing child node %1 under %2").arg(childNodeId, nodeId);
                continue;
            }
            validChildren.append(childNodeId);
            parentById.insert(childNodeId, nodeId);
        }
        childrenById.insert(nodeId, validChildren);
    }

    QHash<QString, Entry> nodeById;
    for (const QString &nodeId : sourceOrderNodeIds) {
        const seriona::scanner::PlaylistNode *node = sourceNodeById.value(nodeId, nullptr);
        if (node == nullptr) {
            continue;
        }

        QString parentName;
        const QString parentNodeId = parentById.value(nodeId);
        if (!parentNodeId.isEmpty()) {
            const seriona::scanner::PlaylistNode *parentNode = sourceNodeById.value(parentNodeId, nullptr);
            if (parentNode != nullptr) {
                parentName = toQString(parentNode->displayName);
            }
        }

        QSet<QString> visitedForCount;
        const int songCount = isFolderKind(node->kind)
            ? descendantTrackCount(nodeId, sourceNodeById, childrenById, visitedForCount)
            : 0;
        Entry entry = entryFromNode(*node, parentName, songCount);
        entry.parentNodeId = parentNodeId;
        nodeById.insert(nodeId, entry);
    }

    QVector<Entry> entries;
    QSet<QString> renderedNodeIds;
    const std::function<void(const QString &, int)> appendSubtree = [&](const QString &nodeId, int depth) {
        if (renderedNodeIds.contains(nodeId)) {
            return;
        }
        const auto entryIt = nodeById.constFind(nodeId);
        if (entryIt == nodeById.cend()) {
            return;
        }

        renderedNodeIds.insert(nodeId);
        Entry entry = entryIt.value();
        entry.depth = depth;
        entries.append(entry);
        for (const QString &childNodeId : childrenById.value(nodeId)) {
            appendSubtree(childNodeId, depth + 1);
        }
    };

    if (snapshot.rootNodeId.has_value()) {
        const QString rootNodeId = toQString(*snapshot.rootNodeId);
        if (nodeById.contains(rootNodeId)) {
            appendSubtree(rootNodeId, 0);
        } else {
            qWarning().noquote() << QStringLiteral("LibraryModel missing root node %1; falling back to snapshot node order").arg(rootNodeId);
        }
    }

    if (entries.isEmpty() && !sourceOrderNodeIds.isEmpty()) {
        for (const QString &nodeId : sourceOrderNodeIds) {
            const auto entryIt = nodeById.constFind(nodeId);
            if (entryIt != nodeById.cend()) {
                entries.append(entryIt.value());
            }
        }
    }

    beginResetModel();
    m_entries = entries;
    m_childrenById = childrenById;
    m_parentById = parentById;
    m_focusedNodeId.clear();
    m_playingTrackId.clear();
    rebuildEntryIndexes();
    m_version = snapshot.version;
    endResetModel();
}
#endif

LibraryController::LibraryController(QObject *parent)
    : QObject(parent)
    , m_model(this)
{
    m_model.setEntries(rootEntries());
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
    if (!m_currentFolderNodeId.isEmpty()) {
        return true;
    }
    if (m_folder != Folder::Root) {
        return true;
    }

    const QString nodeId = !m_selectedBrowserNodeId.isEmpty() ? m_selectedBrowserNodeId : m_focusedNodeId;
    return !m_model.parentNodeId(nodeId).isEmpty();
}

QString LibraryController::searchQuery() const
{
    return m_searchQuery;
}

QStringList LibraryController::expandedNodeIds() const
{
    QStringList nodeIds;
    nodeIds.reserve(m_expandedNodeIds.size());
    for (int row = 0; row < m_model.rowCount(); ++row) {
        const LibraryModel::Entry *entry = m_model.entryAt(row);
        if (entry != nullptr && m_expandedNodeIds.contains(entry->nodeId)) {
            nodeIds.append(entry->nodeId);
        }
    }
    return nodeIds;
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
        setSelectedBrowserNodeId(playingNodeId);
        requestScrollToNode(playingNodeId);
    }
}

#if SERIONA_HAS_BACKEND
void LibraryController::setCommandExecutor(CommandExecutor executor)
{
    m_commandExecutor = std::move(executor);
}

void LibraryController::setScanExecutor(ScanExecutor executor)
{
    m_scanExecutor = std::move(executor);
}

void LibraryController::setPlaylistTreeSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot)
{
    const QVector<QString> focusedFallbackChain = m_model.ancestorChainForNode(m_focusedNodeId);
    const QVector<QString> selectedFallbackChain = m_model.ancestorChainForNode(m_selectedBrowserNodeId);

    m_model.setPlaylistTreeSnapshot(snapshot);
    reconcileBrowsingState(focusedFallbackChain, selectedFallbackChain);
}

void LibraryController::applyLibraryStateSnapshot(const seriona::control::LibraryStateSnapshot &snapshot)
{
    setScanStatus(uiScanStatus(snapshot.scanStatus));
    setScanProgress(uiScanProgress(snapshot));
    if (snapshot.lastError.has_value()) {
        setLastError(scannerErrorMessage(*snapshot.lastError));
    } else if (snapshot.scanStatus == seriona::control::LibraryScanStatus::Error) {
        setLastError(QStringLiteral("扫描失败"));
    } else {
        setLastError(QString());
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

    m_folder = Folder::Root;
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

    const QString nodeId = !m_selectedBrowserNodeId.isEmpty() ? m_selectedBrowserNodeId : m_focusedNodeId;
    const QString parentNodeId = m_model.parentNodeId(nodeId);
    if (!parentNodeId.isEmpty()) {
        setSelectedBrowserNodeId(parentNodeId);
        requestScrollToNode(parentNodeId);
        return;
    }

    setFolder(Folder::Root, QStringLiteral("My Music"));
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
    setSavedRootPath(rootPath);
    setScanStatus(QStringLiteral("running"));
    setScanProgress(0);
    setLastError(QString());

#if SERIONA_HAS_BACKEND
    if (!m_scanExecutor) {
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
    setScanStatus(QStringLiteral("error"));
    setLastError(tr("后端扫描服务不可用"));
    return false;
#endif

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

void LibraryController::expandNode(const QString &nodeId)
{
    setExpanded(nodeId, true);
}

void LibraryController::collapseNode(const QString &nodeId)
{
    setExpanded(nodeId, false);
}

void LibraryController::toggleExpanded(const QString &nodeId)
{
    if (!m_model.containsNodeId(nodeId)) {
        return;
    }

    setExpanded(nodeId, !m_expandedNodeIds.contains(nodeId));
}

void LibraryController::focusNode(const QString &nodeId)
{
    setFocusedNodeId(nodeId);
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

QString LibraryController::describeBackendHook() const
{
    return QStringLiteral("Future backend hook: library folder browsing, refresh, item playback, current-song location, and library search intent.");
}

QVector<LibraryModel::Entry> LibraryController::rootEntries()
{
    return {mockFolderEntry(QStringLiteral("mock-folder-hires"), QStringLiteral("Hi-Res Collection"), 128, QStringLiteral("12:45:30")),
            mockFileEntry(QStringLiteral("mock-track-stairway"), QStringLiteral("mock-track-stairway-id"), QStringLiteral("Stairway to Heaven"), QStringLiteral("Led Zeppelin"), QStringLiteral("Led Zeppelin IV"), QStringLiteral("08:02"), QStringLiteral("FLAC"), 96000, 24),
            mockFileEntry(QStringLiteral("mock-track-bohemian"), QStringLiteral("mock-track-bohemian-id"), QStringLiteral("Bohemian Rhapsody"), QStringLiteral("Queen"), QStringLiteral("A Night at the Opera"), QStringLiteral("05:55"), QStringLiteral("WAV"), 192000, 24),
            mockFolderEntry(QStringLiteral("mock-folder-rock"), QStringLiteral("Rock Classics"), 45, QStringLiteral("03:12:00")),
            mockFileEntry(QStringLiteral("mock-track-imagine"), QStringLiteral("mock-track-imagine-id"), QStringLiteral("Imagine"), QStringLiteral("John Lennon"), QStringLiteral("Imagine"), QStringLiteral("03:03"), QStringLiteral("MP3"), 44100, 16),
            mockFileEntry(QStringLiteral("mock-track-hotel"), QStringLiteral("mock-track-hotel-id"), QStringLiteral("Hotel California"), QStringLiteral("Eagles"), QStringLiteral("Hotel California"), QStringLiteral("06:30"), QStringLiteral("FLAC"), 48000, 24),
            mockFolderEntry(QStringLiteral("mock-folder-jazz"), QStringLiteral("Jazz Essentials"), 32, QStringLiteral("02:45:15"))};
}

QVector<LibraryModel::Entry> LibraryController::childEntries()
{
    return {mockFileEntry(QStringLiteral("mock-child-track-1"), QStringLiteral("mock-child-track-1-id"), QStringLiteral("Sub Song 1"), QStringLiteral("Artist A"), QStringLiteral("Album X"), QStringLiteral("03:45"), QStringLiteral("FLAC"), 44100, 16),
            mockFileEntry(QStringLiteral("mock-child-track-2"), QStringLiteral("mock-child-track-2-id"), QStringLiteral("Sub Song 2"), QStringLiteral("Artist B"), QStringLiteral("Album Y"), QStringLiteral("04:20"), QStringLiteral("MP3"), 44100, 16)};
}

QVector<LibraryModel::Entry> LibraryController::currentSourceEntries() const
{
    return m_folder == Folder::Root ? rootEntries() : childEntries();
}

QVector<LibraryModel::Entry> LibraryController::filteredEntries(const QVector<LibraryModel::Entry> &entries) const
{
    const QString trimmedQuery = m_searchQuery.trimmed();
    if (trimmedQuery.isEmpty()) {
        return entries;
    }

    QVector<LibraryModel::Entry> filtered;
    filtered.reserve(entries.size());
    for (const LibraryModel::Entry &entry : entries) {
        const bool matchesFolder = entry.type == QStringLiteral("folder")
            && (entry.name.contains(trimmedQuery, Qt::CaseInsensitive)
                || entry.parentName.contains(trimmedQuery, Qt::CaseInsensitive));
        const bool matchesFile = entry.type == QStringLiteral("file")
            && (entry.title.contains(trimmedQuery, Qt::CaseInsensitive)
                || entry.artist.contains(trimmedQuery, Qt::CaseInsensitive)
                || entry.album.contains(trimmedQuery, Qt::CaseInsensitive)
                || entry.format.contains(trimmedQuery, Qt::CaseInsensitive));

        if (matchesFolder || matchesFile) {
            filtered.append(entry);
        }
    }

    return filtered;
}

void LibraryController::updateModelEntries()
{
    const QVector<QString> focusedFallbackChain = m_model.ancestorChainForNode(m_focusedNodeId);
    const QVector<QString> selectedFallbackChain = m_model.ancestorChainForNode(m_selectedBrowserNodeId);

    m_model.setEntries(currentSourceEntries());
    reconcileBrowsingState(focusedFallbackChain, selectedFallbackChain);
}

void LibraryController::setFolder(Folder folder, const QString &folderName)
{
    const bool previousCanGoBack = canGoBack();
    const bool folderChanged = m_folder != folder;
    const bool nameChanged = m_currentFolderName != folderName;

    m_folder = folder;
    m_currentFolderName = folderName;
    updateModelEntries();

    if (nameChanged) {
        emit currentFolderNameChanged();
    }
    if (previousCanGoBack != canGoBack() || folderChanged) {
        emit canGoBackChanged();
    }
}

void LibraryController::setExpanded(const QString &nodeId, bool expanded)
{
    const LibraryModel::Entry *entry = m_model.entryByNodeId(nodeId);
    if (entry == nullptr || !entry->isFolder) {
        return;
    }

    if (expanded) {
        if (m_expandedNodeIds.contains(nodeId)) {
            return;
        }
        m_expandedNodeIds.insert(nodeId);
    } else {
        if (!m_expandedNodeIds.contains(nodeId)) {
            return;
        }
        m_expandedNodeIds.remove(nodeId);
    }

    m_model.setNodeExpanded(nodeId, expanded);
    applyBrowsingState();
    emit expandedNodeIdsChanged();
}

void LibraryController::applyBrowsingState()
{
    m_model.applyBrowsingState(m_expandedNodeIds, m_focusedNodeId, m_playingTrackId, m_searchQuery, m_currentFolderNodeId);
    updateVisibleNodeCount();
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
    const QSet<QString> previousExpandedNodeIds = m_expandedNodeIds;
    const QString previousFocusedNodeId = m_focusedNodeId;
    const QString previousSelectedNodeId = m_selectedBrowserNodeId;
    const bool previousCanGoBack = canGoBack();

    for (auto it = m_expandedNodeIds.begin(); it != m_expandedNodeIds.end();) {
        const LibraryModel::Entry *entry = m_model.entryByNodeId(*it);
        if (entry == nullptr || !entry->isFolder) {
            it = m_expandedNodeIds.erase(it);
        } else {
            ++it;
        }
    }

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

    if (previousExpandedNodeIds != m_expandedNodeIds) {
        emit expandedNodeIdsChanged();
    }
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
