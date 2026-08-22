#include "library_folder_projection_model.h"

#include <QSet>

#include <utility>

namespace Seriona::App {

LibraryFolderProjectionModel::LibraryFolderProjectionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LibraryFolderProjectionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant LibraryFolderProjectionModel::data(const QModelIndex &index, int role) const
{
    const Entry *entry = entryAt(index.row());
    if (entry == nullptr) {
        return {};
    }

    // 角色取值与 LibraryModel 完全一致（QML 侧两种模型可互换绑定）。
    switch (role) {
    case LibraryModel::TypeRole:
        return entry->type;
    case LibraryModel::NameRole:
        return entry->name;
    case LibraryModel::TitleRole:
        return entry->title;
    case LibraryModel::ArtistRole:
        return entry->artist;
    case LibraryModel::AlbumRole:
        return entry->album;
    case LibraryModel::ParentNameRole:
        return entry->parentName;
    case LibraryModel::SongCountRole:
        return entry->songCount;
    case LibraryModel::DurationRole:
        return entry->duration;
    case LibraryModel::FormatRole:
        return entry->format;
    case LibraryModel::SampleRateRole:
        return entry->sampleRate;
    case LibraryModel::BitDepthRole:
        return entry->bitDepth;
    case LibraryModel::NodeIdRole:
        return entry->nodeId;
    case LibraryModel::TrackIdRole:
        return entry->trackId;
    case LibraryModel::IsFolderRole:
        return entry->isFolder;
    case LibraryModel::IsPlayingRole:
        return entry->isPlaying;
    case LibraryModel::IsFocusedRole:
        return entry->isFocused;
    case LibraryModel::ParentNodeIdRole:
        return entry->parentNodeId;
    case LibraryModel::ArtworkSourceRole:
        return entry->artworkSource;
    case LibraryModel::YearRole:
        return entry->year.has_value() ? QVariant{static_cast<qint64>(*entry->year)} : QVariant{};
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryFolderProjectionModel::roleNames() const
{
    // 与 LibraryModel::roleNames 完全一致（键为 LibraryModel::Role 数值）。
    return {{LibraryModel::TypeRole, "type"},
            {LibraryModel::NameRole, "name"},
            {LibraryModel::TitleRole, "title"},
            {LibraryModel::ArtistRole, "artist"},
            {LibraryModel::AlbumRole, "album"},
            {LibraryModel::ParentNameRole, "parentName"},
            {LibraryModel::SongCountRole, "songCount"},
            {LibraryModel::DurationRole, "duration"},
            {LibraryModel::FormatRole, "format"},
            {LibraryModel::SampleRateRole, "sampleRate"},
            {LibraryModel::BitDepthRole, "bitDepth"},
            {LibraryModel::NodeIdRole, "nodeId"},
            {LibraryModel::TrackIdRole, "trackId"},
            {LibraryModel::IsFolderRole, "isFolder"},
            {LibraryModel::IsPlayingRole, "isPlaying"},
            {LibraryModel::IsFocusedRole, "isFocused"},
            {LibraryModel::ParentNodeIdRole, "parentNodeId"},
            {LibraryModel::ArtworkSourceRole, "artworkSource"},
            {LibraryModel::YearRole, "year"}};
}

const LibraryFolderProjectionModel::Entry *LibraryFolderProjectionModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return nullptr;
    }

    return &m_entries.at(row);
}

int LibraryFolderProjectionModel::rowForNodeId(const QString &nodeId) const
{
    return m_rowByNodeId.value(nodeId, -1);
}

QString LibraryFolderProjectionModel::folderNodeId() const
{
    return m_folderNodeId;
}

QVector<LibraryFolderProjectionModel::SortRule> LibraryFolderProjectionModel::sortRules() const
{
    return m_sortRules;
}

int LibraryFolderProjectionModel::projectionRevision() const
{
    return m_projectionRevision;
}

void LibraryFolderProjectionModel::setSource(LibraryModel *source,
                                             const QString &folderNodeId,
                                             const QVector<SortRule> &sortRules)
{
    disconnectSource();
    m_source = source;
    m_folderNodeId = folderNodeId;
    m_sortRules = sortRules;
    if (m_source != nullptr) {
        connect(m_source, &LibraryModel::treeChanged, this, &LibraryFolderProjectionModel::rebuildFromSource);
        connect(m_source, &LibraryModel::playingTrackIdChanged, this, &LibraryFolderProjectionModel::onSourcePlayingTrackChanged);
        connect(m_source, &LibraryModel::focusedNodeIdChanged, this, &LibraryFolderProjectionModel::onSourceFocusedNodeChanged);
    }
    rebuildFromSource();
}

void LibraryFolderProjectionModel::disconnectSource()
{
    if (m_source == nullptr) {
        return;
    }

    disconnect(m_source, &LibraryModel::treeChanged, this, &LibraryFolderProjectionModel::rebuildFromSource);
    disconnect(m_source, &LibraryModel::playingTrackIdChanged, this, &LibraryFolderProjectionModel::onSourcePlayingTrackChanged);
    disconnect(m_source, &LibraryModel::focusedNodeIdChanged, this, &LibraryFolderProjectionModel::onSourceFocusedNodeChanged);
}

void LibraryFolderProjectionModel::rebuildFromSource()
{
    QVector<QString> nodeIds;
    if (m_source != nullptr) {
        nodeIds = m_folderNodeId.isEmpty() ? m_source->rootProjectionNodeIds() : m_source->childNodeIds(m_folderNodeId);
        nodeIds = m_source->sortedProjectionNodeIds(std::move(nodeIds), m_sortRules);
    }

    const QString rootNodeId = m_source != nullptr ? m_source->rootNodeId() : QString();
    const QString focusedNodeId = m_source != nullptr ? m_source->focusedNodeId() : QString();
    const QString playingNodeId = m_source != nullptr ? m_source->nodeIdForTrackId(m_source->playingTrackId()) : QString();

    QVector<Entry> projectedEntries;
    projectedEntries.reserve(nodeIds.size());
    QHash<QString, int> rowByNodeId;
    QSet<QString> projectedNodeIds;

    // 过滤规则与 LibraryModel::setProjectionNodeIds 完全一致：
    // 跳过空 nodeId、跳过 rootNodeId、跳过重复项、跳过未知节点。
    for (const QString &nodeId : nodeIds) {
        if (nodeId.isEmpty() || nodeId == rootNodeId || projectedNodeIds.contains(nodeId)) {
            continue;
        }
        const Entry *sourceEntry = m_source != nullptr ? m_source->entryByNodeId(nodeId) : nullptr;
        if (sourceEntry == nullptr) {
            continue;
        }

        Entry entry = *sourceEntry;
        entry.isFocused = entry.nodeId == focusedNodeId;
        entry.isPlaying = !playingNodeId.isEmpty() && entry.nodeId == playingNodeId;
        rowByNodeId.insert(nodeId, projectedEntries.size());
        projectedEntries.append(entry);
        projectedNodeIds.insert(nodeId);
    }

    // 完整投影替换使用 reset 语义（与主模型一致）；重建后视图位置归零属"内容变化"语义。
    beginResetModel();
    m_entries = std::move(projectedEntries);
    m_rowByNodeId = std::move(rowByNodeId);
    endResetModel();
    ++m_projectionRevision;
    emit projectionRevisionChanged();
}

void LibraryFolderProjectionModel::onSourcePlayingTrackChanged()
{
    if (m_source == nullptr || m_entries.isEmpty()) {
        return;
    }

    const QString playingNodeId = m_source->nodeIdForTrackId(m_source->playingTrackId());
    for (int row = 0; row < m_entries.size(); ++row) {
        Entry &entry = m_entries[row];
        const bool playing = !playingNodeId.isEmpty() && entry.nodeId == playingNodeId;
        if (entry.isPlaying != playing) {
            entry.isPlaying = playing;
            const QModelIndex changedIndex = index(row, 0);
            emit dataChanged(changedIndex, changedIndex, QList<int>{LibraryModel::IsPlayingRole});
        }
    }
}

void LibraryFolderProjectionModel::onSourceFocusedNodeChanged()
{
    if (m_source == nullptr || m_entries.isEmpty()) {
        return;
    }

    const QString focusedNodeId = m_source->focusedNodeId();
    for (int row = 0; row < m_entries.size(); ++row) {
        Entry &entry = m_entries[row];
        const bool focused = !focusedNodeId.isEmpty() && entry.nodeId == focusedNodeId;
        if (entry.isFocused != focused) {
            entry.isFocused = focused;
            const QModelIndex changedIndex = index(row, 0);
            emit dataChanged(changedIndex, changedIndex, QList<int>{LibraryModel::IsFocusedRole});
        }
    }
}

}
