#include "library_tree_store.h"

#include <utility>

namespace Seriona::App {

namespace {
QString toQString(const std::string &value)
{
    return QString::fromStdString(value);
}

void appendUnique(QVector<QString> &values, const QString &value)
{
    if (!value.isEmpty() && !values.contains(value)) {
        values.append(value);
    }
}

bool isTrackNode(const LibraryTreeStore::Node &node)
{
    return !node.isFolder && !node.trackId.isEmpty();
}

#if SERIONA_HAS_BACKEND
bool isFolderKind(seriona::scanner::PlaylistNodeKind kind)
{
    return kind != seriona::scanner::PlaylistNodeKind::Track;
}

void copySongMetadata(const seriona::scanner::SongMetadata &song, LibraryTreeStore::Node &node)
{
    node.trackId = toQString(song.trackId);
    node.logicalTrackId = toQString(song.logicalTrackId);
    node.title = toQString(song.title);
    node.artist = toQString(song.artist);
    node.album = toQString(song.album);
    node.filePath = song.filePath;
    node.sourceFilePath = song.sourceFilePath;
    if (song.artworkPath.has_value()) {
        node.artworkPath = *song.artworkPath;
    }
    if (song.thumbnailPath.has_value()) {
        node.thumbnailPath = *song.thumbnailPath;
    }
    node.duration = song.duration;
    node.year = song.year;
    node.discNumber = song.discNumber;
    node.trackNumber = song.trackNumber;
    node.fileMtime = song.fileMtime;
    node.sampleRate = song.sampleRate.has_value() ? static_cast<int>(*song.sampleRate) : 0;
    node.bitDepth = song.bitDepth.has_value() ? static_cast<int>(*song.bitDepth) : 0;
}
#endif
}

std::uint64_t LibraryTreeStore::version() const
{
    return m_version;
}

const QString &LibraryTreeStore::rootNodeId() const
{
    return m_rootNodeId;
}

const QVector<QString> &LibraryTreeStore::rootChildNodeIds() const
{
    return m_rootChildNodeIds;
}

const QVector<QString> &LibraryTreeStore::nodeOrder() const
{
    return m_nodeOrder;
}

const QVector<LibraryTreeStore::SkippedChild> &LibraryTreeStore::skippedChildren() const
{
    return m_skippedChildren;
}

const QString &LibraryTreeStore::missingRootNodeId() const
{
    return m_missingRootNodeId;
}

const LibraryTreeStore::Node *LibraryTreeStore::nodeById(const QString &nodeId) const
{
    const auto nodeIt = m_nodesById.constFind(nodeId);
    if (nodeIt == m_nodesById.cend()) {
        return nullptr;
    }
    return &nodeIt.value();
}

bool LibraryTreeStore::containsNodeId(const QString &nodeId) const
{
    return !nodeId.isEmpty() && m_nodesById.contains(nodeId);
}

QVector<QString> LibraryTreeStore::childNodeIds(const QString &nodeId) const
{
    return m_childrenById.value(nodeId);
}

QString LibraryTreeStore::parentNodeId(const QString &nodeId) const
{
    return m_parentById.value(nodeId);
}

QString LibraryTreeStore::nodeIdForTrackId(const QString &trackId) const
{
    return m_trackIdToNodeId.value(trackId);
}

void LibraryTreeStore::clear()
{
    m_nodesById.clear();
    m_childrenById.clear();
    m_parentById.clear();
    m_trackIdToNodeId.clear();
    m_nodeOrder.clear();
    m_rootChildNodeIds.clear();
    m_skippedChildren.clear();
    m_rootNodeId.clear();
    m_missingRootNodeId.clear();
    m_version = 0;
}

#if SERIONA_HAS_BACKEND
void LibraryTreeStore::setSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot)
{
    clear();
    m_version = snapshot.version;

    QHash<QString, const seriona::scanner::PlaylistNode *> sourceNodeById;
    for (const seriona::scanner::PlaylistNode &sourceNode : snapshot.nodes) {
        const QString nodeId = toQString(sourceNode.nodeId);
        if (nodeId.isEmpty() || sourceNodeById.contains(nodeId)) {
            continue;
        }
        sourceNodeById.insert(nodeId, &sourceNode);
        m_nodeOrder.append(nodeId);
    }

    for (const QString &nodeId : std::as_const(m_nodeOrder)) {
        const seriona::scanner::PlaylistNode *sourceNode = sourceNodeById.value(nodeId, nullptr);
        if (sourceNode == nullptr) {
            continue;
        }

        Node node;
        node.nodeId = nodeId;
        node.displayName = toQString(sourceNode->displayName);
        node.isFolder = isFolderKind(sourceNode->kind);
        if (sourceNode->song.has_value()) {
            copySongMetadata(*sourceNode->song, node);
        }
        if (node.title.isEmpty()) {
            node.title = node.displayName;
        }

        m_nodesById.insert(nodeId, node);
        m_childrenById.insert(nodeId, {});

        if (!node.trackId.isEmpty() && !m_trackIdToNodeId.contains(node.trackId)) {
            m_trackIdToNodeId.insert(node.trackId, nodeId);
        }
        if (!node.logicalTrackId.isEmpty() && !m_trackIdToNodeId.contains(node.logicalTrackId)) {
            m_trackIdToNodeId.insert(node.logicalTrackId, nodeId);
        }
    }

    for (const QString &nodeId : std::as_const(m_nodeOrder)) {
        const seriona::scanner::PlaylistNode *sourceNode = sourceNodeById.value(nodeId, nullptr);
        if (sourceNode == nullptr) {
            continue;
        }

        QVector<QString> normalizedChildren;
        normalizedChildren.reserve(static_cast<qsizetype>(sourceNode->childNodeIds.size()));
        for (const std::string &rawChildNodeId : sourceNode->childNodeIds) {
            const QString childNodeId = toQString(rawChildNodeId);
            if (!sourceNodeById.contains(childNodeId)) {
                m_skippedChildren.append(SkippedChild{nodeId, childNodeId});
                continue;
            }
            appendUnique(normalizedChildren, childNodeId);
            if (!m_parentById.contains(childNodeId)) {
                m_parentById.insert(childNodeId, nodeId);
            }
        }
        m_childrenById.insert(nodeId, normalizedChildren);
    }

    for (const QString &nodeId : std::as_const(m_nodeOrder)) {
        const seriona::scanner::PlaylistNode *sourceNode = sourceNodeById.value(nodeId, nullptr);
        if (sourceNode == nullptr || !sourceNode->parentNodeId.has_value()) {
            continue;
        }

        const QString parentNodeId = toQString(*sourceNode->parentNodeId);
        if (!sourceNodeById.contains(parentNodeId) || m_parentById.contains(nodeId)) {
            continue;
        }

        m_parentById.insert(nodeId, parentNodeId);
        QVector<QString> children = m_childrenById.value(parentNodeId);
        appendUnique(children, nodeId);
        m_childrenById.insert(parentNodeId, children);
    }

    for (auto it = m_parentById.cbegin(); it != m_parentById.cend(); ++it) {
        Node &node = m_nodesById[it.key()];
        node.parentNodeId = it.value();
    }
    for (auto it = m_childrenById.cbegin(); it != m_childrenById.cend(); ++it) {
        Node &node = m_nodesById[it.key()];
        node.childNodeIds = it.value();
    }

    if (snapshot.rootNodeId.has_value()) {
        const QString requestedRootNodeId = toQString(*snapshot.rootNodeId);
        if (m_nodesById.contains(requestedRootNodeId)) {
            m_rootNodeId = requestedRootNodeId;
        } else {
            m_missingRootNodeId = requestedRootNodeId;
        }
    }

    rebuildRootChildren();
    rebuildDescendantTrackCounts();
}
#endif

int LibraryTreeStore::descendantTrackCountFor(const QString &nodeId, QHash<QString, bool> &visiting) const
{
    if (visiting.value(nodeId, false)) {
        return 0;
    }

    const Node *node = nodeById(nodeId);
    if (node == nullptr) {
        return 0;
    }
    if (isTrackNode(*node)) {
        return 1;
    }

    visiting.insert(nodeId, true);
    int total = 0;
    for (const QString &childNodeId : m_childrenById.value(nodeId)) {
        total += descendantTrackCountFor(childNodeId, visiting);
    }
    visiting.insert(nodeId, false);
    return total;
}

void LibraryTreeStore::rebuildRootChildren()
{
    m_rootChildNodeIds.clear();
    if (!m_rootNodeId.isEmpty()) {
        m_rootChildNodeIds = m_childrenById.value(m_rootNodeId);
        return;
    }

    for (const QString &nodeId : std::as_const(m_nodeOrder)) {
        if (!m_parentById.contains(nodeId)) {
            appendUnique(m_rootChildNodeIds, nodeId);
        }
    }
    if (m_rootChildNodeIds.isEmpty()) {
        m_rootChildNodeIds = m_nodeOrder;
    }
}

void LibraryTreeStore::rebuildDescendantTrackCounts()
{
    for (const QString &nodeId : std::as_const(m_nodeOrder)) {
        Node &node = m_nodesById[nodeId];
        if (!node.isFolder) {
            node.descendantTrackCount = 0;
            continue;
        }

        QHash<QString, bool> visiting;
        node.descendantTrackCount = descendantTrackCountFor(nodeId, visiting);
    }
}

}
