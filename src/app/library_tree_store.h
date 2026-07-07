#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#if SERIONA_HAS_BACKEND
#include "seriona/scanner/scanner_contracts.h"
#endif

namespace Seriona::App {

class LibraryTreeStore
{
public:
    struct Node {
        QString nodeId;
        QString parentNodeId;
        QString displayName;
        QVector<QString> childNodeIds;
        bool isFolder = false;
        QString trackId;
        QString logicalTrackId;
        QString title;
        QString artist;
        QString album;
        std::filesystem::path filePath;
        std::filesystem::path sourceFilePath;
        std::filesystem::path artworkPath;
        std::filesystem::path thumbnailPath;
        std::optional<std::chrono::milliseconds> duration;
        int sampleRate = 0;
        int bitDepth = 0;
        int descendantTrackCount = 0;
    };

    struct SkippedChild {
        QString parentNodeId;
        QString childNodeId;
    };

    [[nodiscard]] std::uint64_t version() const;
    [[nodiscard]] const QString &rootNodeId() const;
    [[nodiscard]] const QVector<QString> &rootChildNodeIds() const;
    [[nodiscard]] const QVector<QString> &nodeOrder() const;
    [[nodiscard]] const QVector<SkippedChild> &skippedChildren() const;
    [[nodiscard]] const QString &missingRootNodeId() const;

    [[nodiscard]] const Node *nodeById(const QString &nodeId) const;
    [[nodiscard]] bool containsNodeId(const QString &nodeId) const;
    [[nodiscard]] QVector<QString> childNodeIds(const QString &nodeId) const;
    [[nodiscard]] QString parentNodeId(const QString &nodeId) const;
    [[nodiscard]] QString nodeIdForTrackId(const QString &trackId) const;

    void clear();

#if SERIONA_HAS_BACKEND
    void setSnapshot(const seriona::scanner::PlaylistTreeSnapshot &snapshot);
#endif

private:
    [[nodiscard]] int descendantTrackCountFor(const QString &nodeId, QHash<QString, bool> &visiting) const;
    void rebuildRootChildren();
    void rebuildDescendantTrackCounts();

    QHash<QString, Node> m_nodesById;
    QHash<QString, QVector<QString>> m_childrenById;
    QHash<QString, QString> m_parentById;
    QHash<QString, QString> m_trackIdToNodeId;
    QVector<QString> m_nodeOrder;
    QVector<QString> m_rootChildNodeIds;
    QVector<SkippedChild> m_skippedChildren;
    QString m_rootNodeId;
    QString m_missingRootNodeId;
    std::uint64_t m_version = 0;
};

}
