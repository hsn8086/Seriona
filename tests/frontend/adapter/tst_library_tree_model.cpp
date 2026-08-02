#include "library_model.h"

#include <QAbstractItemModel>
#include <QByteArray>
#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QString>
#include <QtTest/QTest>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace {
using seriona::scanner::PlaylistNode;
using seriona::scanner::PlaylistNodeKind;
using seriona::scanner::PlaylistTreeSnapshot;
using seriona::scanner::SongMetadata;

PlaylistNode makeFolder(const std::string &nodeId,
                        const std::string &displayName,
                        std::vector<std::string> childNodeIds = {},
                        std::optional<std::string> parentNodeId = std::nullopt,
                        PlaylistNodeKind kind = PlaylistNodeKind::Directory)
{
    PlaylistNode node;
    node.nodeId = nodeId;
    node.displayName = displayName;
    node.kind = kind;
    node.parentNodeId = std::move(parentNodeId);
    node.childNodeIds = std::move(childNodeIds);
    return node;
}

PlaylistNode makeTrack(const std::string &nodeId,
                       const std::string &trackId,
                       const std::string &title,
                       const std::string &artist,
                       const std::string &album,
                       std::optional<std::string> parentNodeId = std::nullopt)
{
    SongMetadata song;
    song.trackId = trackId;
    song.filePath = "/music/" + trackId + ".flac";
    song.sourceFilePath = song.filePath;
    song.title = title;
    song.artist = artist;
    song.album = album;
    song.sampleRate = 96000;
    song.bitDepth = 24;
    song.duration = std::chrono::milliseconds{185000};

    PlaylistNode node;
    node.nodeId = nodeId;
    node.parentNodeId = std::move(parentNodeId);
    node.kind = PlaylistNodeKind::Track;
    node.displayName = title;
    node.song = std::move(song);
    return node;
}

PlaylistTreeSnapshot makeProjectedTreeSnapshot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 7;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"album-a", "track-c"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("album-a", "Album A", {"track-a", "track-b"}, std::string{"root"}, PlaylistNodeKind::Album),
        makeTrack("track-a", "track-a-id", "Song A", "Artist A", "Album A", std::string{"album-a"}),
        makeTrack("track-b", "track-b-id", "Song B", "Artist B", "Album A", std::string{"album-a"}),
        makeTrack("track-c", "track-c-id", "Song C", "Artist C", "Singles", std::string{"root"}),
    };
    return snapshot;
}

QVariant dataAt(const Seriona::App::LibraryModel &model, int row, Seriona::App::LibraryModel::Role role)
{
    return model.data(model.index(row, 0), role);
}

QString nodeIdAt(const Seriona::App::LibraryModel &model, int row)
{
    return dataAt(model, row, Seriona::App::LibraryModel::NodeIdRole).toString();
}

QSet<QByteArray> roleNameSet(const QHash<int, QByteArray> &roles)
{
    QSet<QByteArray> names;
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        names.insert(it.value());
    }
    return names;
}
}

class LibraryTreeModelTest : public QObject
{
    Q_OBJECT

private slots:
    void rootProjectionSkipsVirtualLibraryRow();
    void currentFolderProjectionUsesDirectChildrenOnly();
    void searchProjectionUsesResultRowsOnly();
    void mapsTrackAndLogicalTrackIdsToStableNodeIds();
    void exposesArtworkAndDurationRoles();
    void handlesEmptyTree();
    void resetsOnVersionUpdate();
    void resetsWhenProjectionChanges();
    void emitsDataChangedOnlyForProjectedRows();
    void keepsProjectionRoleNamesStable();
    void invalidChildNodeIdsAreSkipped();
};

void LibraryTreeModelTest::rootProjectionSkipsVirtualLibraryRow()
{
    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(makeProjectedTreeSnapshot());

    QCOMPARE(model.version(), 7ULL);
    QVERIFY(model.containsNodeId(QStringLiteral("root")));
    QCOMPARE(model.childNodeIds(QStringLiteral("root")), QVector<QString>({QStringLiteral("album-a"), QStringLiteral("track-c")}));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(nodeIdAt(model, 0), QStringLiteral("album-a"));
    QCOMPARE(nodeIdAt(model, 1), QStringLiteral("track-c"));
    QCOMPARE(model.rowForNodeId(QStringLiteral("root")), -1);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::NameRole).toString(), QStringLiteral("Album A"));
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::ParentNameRole).toString(), QStringLiteral("Library"));
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::SongCountRole).toInt(), 2);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::IsFolderRole).toBool(), true);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::ParentNodeIdRole).toString(), QStringLiteral("root"));
}

void LibraryTreeModelTest::currentFolderProjectionUsesDirectChildrenOnly()
{
    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(makeProjectedTreeSnapshot());

    model.applyBrowsingState({}, {}, {}, QStringLiteral("album-a"));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(nodeIdAt(model, 0), QStringLiteral("track-a"));
    QCOMPARE(nodeIdAt(model, 1), QStringLiteral("track-b"));
    QCOMPARE(model.rowForNodeId(QStringLiteral("track-a")), 0);
    QCOMPARE(model.rowForNodeId(QStringLiteral("track-b")), 1);
    QCOMPARE(model.rowForNodeId(QStringLiteral("album-a")), -1);
    QCOMPARE(model.rowForNodeId(QStringLiteral("track-c")), -1);
}

void LibraryTreeModelTest::searchProjectionUsesResultRowsOnly()
{
    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(makeProjectedTreeSnapshot());

    model.applyBrowsingState({}, {}, QStringLiteral("Song B"), {});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(nodeIdAt(model, 0), QStringLiteral("track-b"));
    QCOMPARE(model.rowForNodeId(QStringLiteral("track-b")), 0);
    QCOMPARE(model.rowForNodeId(QStringLiteral("track-a")), -1);
    QCOMPARE(model.rowForNodeId(QStringLiteral("album-a")), -1);
}

void LibraryTreeModelTest::mapsTrackAndLogicalTrackIdsToStableNodeIds()
{
    SongMetadata cueSong;
    cueSong.trackId = "cue-track-01";
    cueSong.logicalTrackId = "album-cue#01";
    cueSong.filePath = "/music/album.cue";
    cueSong.sourceFilePath = "/music/album.flac";
    cueSong.title = "Cue Movement";
    cueSong.artist = "Cue Artist";
    cueSong.album = "Cue Album";
    cueSong.duration = std::chrono::milliseconds{61000};

    PlaylistNode cueNode;
    cueNode.nodeId = "cue-node";
    cueNode.parentNodeId = std::string{"root"};
    cueNode.kind = PlaylistNodeKind::Track;
    cueNode.displayName = "Cue Movement";
    cueNode.song = cueSong;

    PlaylistTreeSnapshot snapshot;
    snapshot.version = 10;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"cue-node"}, std::nullopt, PlaylistNodeKind::Root),
        cueNode,
    };

    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(snapshot);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.nodeIdForTrackId(QStringLiteral("cue-track-01")), QStringLiteral("cue-node"));
    QCOMPARE(model.nodeIdForTrackId(QStringLiteral("album-cue#01")), QStringLiteral("cue-node"));
    QVERIFY(model.entryByNodeId(QStringLiteral("cue-node")) != nullptr);
    QCOMPARE(model.rowForNodeId(QStringLiteral("cue-node")), 0);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::TrackIdRole).toString(), QStringLiteral("cue-track-01"));
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::DurationRole).toString(), QStringLiteral("1:01"));
}

void LibraryTreeModelTest::exposesArtworkAndDurationRoles()
{
    PlaylistNode trackA = makeTrack("track-cover-a", "track-cover-a-id", "Covered A", "Artist", "Album");
    trackA.song->duration = std::chrono::milliseconds{185000};
    trackA.song->artworkPath = "/music/cover-a.png";
    PlaylistNode trackB = makeTrack("track-cover-b", "track-cover-b-id", "Covered B", "Artist", "Album");
    trackB.song->artworkPath = "/music/cover-b.png";

    PlaylistTreeSnapshot snapshot;
    snapshot.version = 13;
    snapshot.nodes = {trackA, trackB};

    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(snapshot);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::DurationRole).toString(), QStringLiteral("3:05"));
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::ArtworkSourceRole).toString(), QStringLiteral("file:///music/cover-a.png"));
    QCOMPARE(dataAt(model, 1, Seriona::App::LibraryModel::ArtworkSourceRole).toString(), QStringLiteral("file:///music/cover-b.png"));

    // now-playing 封面双源（PlaybackController.coverArtworkSource / coverThumbnailSource）
    // 不回流到每行 delegate 的 artworkSource：播放状态变化不得改写各行的 per-entry 封面。
    QVERIFY(model.setPlayingTrackId(QStringLiteral("track-cover-b-id")));
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::ArtworkSourceRole).toString(), QStringLiteral("file:///music/cover-a.png"));
    QCOMPARE(dataAt(model, 1, Seriona::App::LibraryModel::ArtworkSourceRole).toString(), QStringLiteral("file:///music/cover-b.png"));
}

void LibraryTreeModelTest::handlesEmptyTree()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 11;

    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(snapshot);

    QCOMPARE(model.version(), 11ULL);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.entryByNodeId(QStringLiteral("missing")), nullptr);
    QCOMPARE(model.nodeIdForTrackId(QStringLiteral("missing-track")), QString());
}

void LibraryTreeModelTest::resetsOnVersionUpdate()
{
    Seriona::App::LibraryModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    PlaylistTreeSnapshot first;
    first.version = 1;
    first.rootNodeId = std::string{"root"};
    first.nodes = {
        makeFolder("root", "Library", {"first"}, std::nullopt, PlaylistNodeKind::Root),
        makeTrack("first", "first-track", "First", "Artist", "Album", std::string{"root"}),
    };

    PlaylistTreeSnapshot second;
    second.version = 2;
    second.rootNodeId = std::string{"root"};
    second.nodes = {
        makeFolder("root", "Library", {"second"}, std::nullopt, PlaylistNodeKind::Root),
        makeTrack("second", "second-track", "Second", "Artist", "Album", std::string{"root"}),
    };

    model.setPlaylistTreeSnapshot(first);
    model.setPlaylistTreeSnapshot(second);

    QCOMPARE(resetSpy.count(), 2);
    QCOMPARE(model.version(), 2ULL);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(nodeIdAt(model, 0), QStringLiteral("second"));
    QCOMPARE(model.nodeIdForTrackId(QStringLiteral("first-track")), QString());
}

void LibraryTreeModelTest::resetsWhenProjectionChanges()
{
    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(makeProjectedTreeSnapshot());
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

    model.applyBrowsingState({}, {}, {}, QStringLiteral("album-a"));

    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(dataChangedSpy.count(), 0);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(nodeIdAt(model, 0), QStringLiteral("track-a"));
    QCOMPARE(nodeIdAt(model, 1), QStringLiteral("track-b"));
}

void LibraryTreeModelTest::emitsDataChangedOnlyForProjectedRows()
{
    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(makeProjectedTreeSnapshot());
    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

    QVERIFY(!model.setPlayingTrackId(QStringLiteral("track-a-id")));
    QCOMPARE(dataChangedSpy.count(), 0);

    QVERIFY(model.setPlayingTrackId(QStringLiteral("track-c-id")));
    QCOMPARE(dataChangedSpy.count(), 1);
    const QList<QVariant> changedArguments = dataChangedSpy.takeFirst();
    QCOMPARE(changedArguments.at(0).value<QModelIndex>().row(), 1);
    QCOMPARE(changedArguments.at(1).value<QModelIndex>().row(), 1);
    QCOMPARE(changedArguments.at(2).value<QList<int>>(), QList<int>{Seriona::App::LibraryModel::IsPlayingRole});

    QVERIFY(model.setFocusedNodeId(QStringLiteral("album-a")));
    QCOMPARE(dataChangedSpy.count(), 1);
    const QList<QVariant> focusedArguments = dataChangedSpy.takeFirst();
    QCOMPARE(focusedArguments.at(0).value<QModelIndex>().row(), 0);
    QCOMPARE(focusedArguments.at(1).value<QModelIndex>().row(), 0);
    QCOMPARE(focusedArguments.at(2).value<QList<int>>(), QList<int>{Seriona::App::LibraryModel::IsFocusedRole});
}

void LibraryTreeModelTest::keepsProjectionRoleNamesStable()
{
    const Seriona::App::LibraryModel model;
    const QSet<QByteArray> roles = roleNameSet(model.roleNames());

    const QList<QByteArray> expectedRoles = {
        "type",
        "name",
        "title",
        "artist",
        "album",
        "parentName",
        "songCount",
        "duration",
        "format",
        "sampleRate",
        "bitDepth",
        "nodeId",
        "trackId",
        "isFolder",
        "isPlaying",
        "isFocused",
        "parentNodeId",
        "artworkSource",
    };

    for (const QByteArray &roleName : expectedRoles) {
        QVERIFY2(roles.contains(roleName), qPrintable(QStringLiteral("missing projection role %1").arg(QString::fromUtf8(roleName))));
    }
    QVERIFY2(!roles.contains("isVisible"), "projection model must not expose hidden full-tree visibility state");
    QVERIFY2(!roles.contains("matchesSearch"), "projection model must not expose full-tree search match state");
    QVERIFY2(!roles.contains("depth"), "projection rows are flat current-view rows, not tree-depth rows");
    QVERIFY2(!roles.contains("isExpanded"), "projection rows must not depend on virtual tree expansion state");
}

void LibraryTreeModelTest::invalidChildNodeIdsAreSkipped()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 12;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"missing-child", "valid-track"}, std::nullopt, PlaylistNodeKind::Root),
        makeTrack("valid-track", "valid-track-id", "Valid", "Artist", "Album", std::string{"root"}),
    };

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("LibraryModel skipped missing child node missing-child under root.*")));

    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(snapshot);

    QCOMPARE(model.childNodeIds(QStringLiteral("root")), QVector<QString>({QStringLiteral("valid-track")}));
    QCOMPARE(model.parentNodeId(QStringLiteral("valid-track")), QStringLiteral("root"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(nodeIdAt(model, 0), QStringLiteral("valid-track"));
    QCOMPARE(model.rowForNodeId(QStringLiteral("root")), -1);
    QCOMPARE(model.rowForNodeId(QStringLiteral("valid-track")), 0);
}

QTEST_GUILESS_MAIN(LibraryTreeModelTest)

#include "tst_library_tree_model.moc"
