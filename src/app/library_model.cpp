#include "library_model.h"

#include <QDebug>

#if SERIONA_HAS_BACKEND
#include <QSet>

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
    return entry;
}
}
#endif

namespace {

LibraryModel::Entry mockFolderEntry(const QString &name, int songCount, const QString &duration)
{
    LibraryModel::Entry entry;
    entry.type = QStringLiteral("folder");
    entry.name = name;
    entry.parentName = QStringLiteral("Music");
    entry.songCount = songCount;
    entry.duration = duration;
    entry.isFolder = true;
    return entry;
}

LibraryModel::Entry mockFileEntry(const QString &title,
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
            {IsExpandedRole, "isExpanded"}};
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

void LibraryModel::applyBrowsingState(const QSet<QString> &expandedNodeIds, const QString &focusedNodeId, const QString &playingTrackId)
{
    m_focusedNodeId = containsNodeId(focusedNodeId) ? focusedNodeId : QString();
    m_playingTrackId = playingTrackId;
    const QString playingNodeId = nodeIdForTrackId(m_playingTrackId);

    for (int row = 0; row < m_entries.size(); ++row) {
        const Entry &entry = m_entries.at(row);
        setEntryRoleFlag(row, IsExpandedRole, expandedNodeIds.contains(entry.nodeId), true);
        setEntryRoleFlag(row, IsFocusedRole, entry.nodeId == m_focusedNodeId, true);
        setEntryRoleFlag(row, IsPlayingRole, !playingNodeId.isEmpty() && entry.nodeId == playingNodeId, true);
    }
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
        nodeById.insert(nodeId, entry);
    }

    QVector<Entry> entries;
    QSet<QString> renderedNodeIds;
    const std::function<void(const QString &)> appendSubtree = [&](const QString &nodeId) {
        if (renderedNodeIds.contains(nodeId)) {
            return;
        }
        const auto entryIt = nodeById.constFind(nodeId);
        if (entryIt == nodeById.cend()) {
            return;
        }

        renderedNodeIds.insert(nodeId);
        entries.append(entryIt.value());
        for (const QString &childNodeId : childrenById.value(nodeId)) {
            appendSubtree(childNodeId);
        }
    };

    if (snapshot.rootNodeId.has_value()) {
        const QString rootNodeId = toQString(*snapshot.rootNodeId);
        if (nodeById.contains(rootNodeId)) {
            appendSubtree(rootNodeId);
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
    return m_folder != Folder::Root;
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
    if (!nodeId.isEmpty() && !m_model.containsNodeId(nodeId)) {
        return;
    }
    if (m_focusedNodeId == nodeId) {
        return;
    }

    m_focusedNodeId = nodeId;
    m_model.setFocusedNodeId(m_focusedNodeId);
    emit focusedNodeIdChanged();
}

QString LibraryController::selectedBrowserNodeId() const
{
    return m_selectedBrowserNodeId;
}

void LibraryController::setSelectedBrowserNodeId(const QString &nodeId)
{
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
}

QString LibraryController::scrollRequest() const
{
    return m_scrollRequest;
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
void LibraryController::setPlaylistTreeSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot)
{
    const QVector<QString> focusedFallbackChain = m_model.ancestorChainForNode(m_focusedNodeId);
    const QVector<QString> selectedFallbackChain = m_model.ancestorChainForNode(m_selectedBrowserNodeId);

    m_model.setPlaylistTreeSnapshot(snapshot);
    reconcileBrowsingState(focusedFallbackChain, selectedFallbackChain);
}
#endif

void LibraryController::setSearchQuery(const QString &query)
{
    if (m_searchQuery == query) {
        return;
    }

    m_searchQuery = query;
    updateModelEntries();
    emit searchQueryChanged();
}

void LibraryController::enterFolder(int index)
{
    const LibraryModel::Entry *entry = m_model.entryAt(index);
    if (entry == nullptr || entry->type != QStringLiteral("folder")) {
        return;
    }

    if (!entry->nodeId.isEmpty()) {
        setSelectedBrowserNodeId(entry->nodeId);
        setExpanded(entry->nodeId, true);
    }

    setFolder(Folder::Child, entry->name);
}

void LibraryController::goBack()
{
    if (!canGoBack()) {
        return;
    }

    setFolder(Folder::Root, QStringLiteral("My Music"));
}

void LibraryController::refresh()
{
    setFolder(m_folder, m_currentFolderName);
}

void LibraryController::playItem(int index)
{
    const LibraryModel::Entry *entry = m_model.entryAt(index);
    if (entry == nullptr || entry->type != QStringLiteral("file")) {
        return;
    }

    emit playItemRequested(entry->title);
}

void LibraryController::locateCurrentSong()
{
    emit currentSongLocationRequested();
}

void LibraryController::clearSearch()
{
    if (m_searchQuery.isEmpty()) {
        return;
    }

    m_searchQuery.clear();
    updateModelEntries();
    emit searchQueryChanged();
}

void LibraryController::submitSearch()
{
    if (m_model.rowCount() <= 0) {
        return;
    }

    const LibraryModel::Entry *entry = m_model.entryAt(0);
    if (entry == nullptr || entry->nodeId.isEmpty()) {
        return;
    }

    setSelectedBrowserNodeId(entry->nodeId);
    requestScrollToNode(entry->nodeId);
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

QString LibraryController::describeBackendHook() const
{
    return QStringLiteral("Future backend hook: library folder browsing, refresh, item playback, current-song location, and library search intent.");
}

QVector<LibraryModel::Entry> LibraryController::rootEntries()
{
    return {mockFolderEntry(QStringLiteral("Hi-Res Collection"), 128, QStringLiteral("12:45:30")),
            mockFileEntry(QStringLiteral("Stairway to Heaven"), QStringLiteral("Led Zeppelin"), QStringLiteral("Led Zeppelin IV"), QStringLiteral("08:02"), QStringLiteral("FLAC"), 96000, 24),
            mockFileEntry(QStringLiteral("Bohemian Rhapsody"), QStringLiteral("Queen"), QStringLiteral("A Night at the Opera"), QStringLiteral("05:55"), QStringLiteral("WAV"), 192000, 24),
            mockFolderEntry(QStringLiteral("Rock Classics"), 45, QStringLiteral("03:12:00")),
            mockFileEntry(QStringLiteral("Imagine"), QStringLiteral("John Lennon"), QStringLiteral("Imagine"), QStringLiteral("03:03"), QStringLiteral("MP3"), 44100, 16),
            mockFileEntry(QStringLiteral("Hotel California"), QStringLiteral("Eagles"), QStringLiteral("Hotel California"), QStringLiteral("06:30"), QStringLiteral("FLAC"), 48000, 24),
            mockFolderEntry(QStringLiteral("Jazz Essentials"), 32, QStringLiteral("02:45:15"))};
}

QVector<LibraryModel::Entry> LibraryController::childEntries()
{
    return {mockFileEntry(QStringLiteral("Sub Song 1"), QStringLiteral("Artist A"), QStringLiteral("Album X"), QStringLiteral("03:45"), QStringLiteral("FLAC"), 44100, 16),
            mockFileEntry(QStringLiteral("Sub Song 2"), QStringLiteral("Artist B"), QStringLiteral("Album Y"), QStringLiteral("04:20"), QStringLiteral("MP3"), 44100, 16)};
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

    m_model.setEntries(filteredEntries(currentSourceEntries()));
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
    emit expandedNodeIdsChanged();
}

void LibraryController::reconcileBrowsingState(const QVector<QString> &focusedFallbackChain, const QVector<QString> &selectedFallbackChain)
{
    const QSet<QString> previousExpandedNodeIds = m_expandedNodeIds;
    const QString previousFocusedNodeId = m_focusedNodeId;
    const QString previousSelectedNodeId = m_selectedBrowserNodeId;

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
    if (m_selectedBrowserNodeId.isEmpty()) {
        m_selectedBrowserNodeId = m_focusedNodeId;
    }

    m_model.applyBrowsingState(m_expandedNodeIds, m_focusedNodeId, m_playingTrackId);

    if (previousExpandedNodeIds != m_expandedNodeIds) {
        emit expandedNodeIdsChanged();
    }
    if (previousFocusedNodeId != m_focusedNodeId) {
        emit focusedNodeIdChanged();
    }
    if (previousSelectedNodeId != m_selectedBrowserNodeId) {
        emit selectedBrowserNodeIdChanged();
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
