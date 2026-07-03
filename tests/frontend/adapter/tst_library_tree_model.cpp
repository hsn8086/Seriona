#include "library_model.h"

#include <QAbstractItemModel>
#include <QRegularExpression>
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

QVariant dataAt(const Seriona::App::LibraryModel &model, int row, Seriona::App::LibraryModel::Role role)
{
    return model.data(model.index(row, 0), role);
}
}

class LibraryTreeModelTest : public QObject
{
    Q_OBJECT

private slots:
    void rendersRootedTreeWithFolderAndTracks();
    void fallsBackToNodeOrderWithoutRoot();
    void fallsBackToNodeOrderWhenRootIdIsMissing();
    void mapsCueTrackIdToNodeId();
    void handlesEmptyTree();
    void resetsOnVersionUpdate();
    void keepsRoleNamesStable();
    void invalidChildNodeIdsAreSkipped();
};

void LibraryTreeModelTest::rendersRootedTreeWithFolderAndTracks()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 7;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"album"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("album", "Album A", {"track-1", "track-2"}, std::string{"root"}, PlaylistNodeKind::Album),
        makeTrack("track-1", "track-id-1", "Song One", "Artist A", "Album A", std::string{"album"}),
        makeTrack("track-2", "track-id-2", "Song Two", "Artist B", "Album A", std::string{"album"}),
    };

    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(snapshot);

    QCOMPARE(model.version(), 7ULL);
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::NodeIdRole).toString(), QStringLiteral("root"));
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::TypeRole).toString(), QStringLiteral("folder"));
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::NameRole).toString(), QStringLiteral("Library"));
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::SongCountRole).toInt(), 2);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::IsFolderRole).toBool(), true);

    QCOMPARE(dataAt(model, 1, Seriona::App::LibraryModel::NodeIdRole).toString(), QStringLiteral("album"));
    QCOMPARE(dataAt(model, 1, Seriona::App::LibraryModel::ParentNameRole).toString(), QStringLiteral("Library"));
    QCOMPARE(model.parentNodeId(QStringLiteral("album")), QStringLiteral("root"));
    QCOMPARE(model.childNodeIds(QStringLiteral("album")), QVector<QString>({QStringLiteral("track-1"), QStringLiteral("track-2")}));

    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::TypeRole).toString(), QStringLiteral("file"));
    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::TitleRole).toString(), QStringLiteral("Song One"));
    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::ArtistRole).toString(), QStringLiteral("Artist A"));
    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::AlbumRole).toString(), QStringLiteral("Album A"));
    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::TrackIdRole).toString(), QStringLiteral("track-id-1"));
    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::FormatRole).toString(), QStringLiteral("FLAC"));
    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::SampleRateRole).toInt(), 96000);
    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::BitDepthRole).toInt(), 24);
    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::IsPlayingRole).toBool(), false);
    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::IsFocusedRole).toBool(), false);
    QCOMPARE(dataAt(model, 2, Seriona::App::LibraryModel::IsExpandedRole).toBool(), false);
}

void LibraryTreeModelTest::fallsBackToNodeOrderWithoutRoot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 8;
    snapshot.nodes = {
        makeTrack("track-orphan", "track-orphan-id", "Loose Song", "Artist", "Loose Album"),
        makeFolder("folder", "Folder", {}, std::nullopt),
    };

    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(snapshot);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::NodeIdRole).toString(), QStringLiteral("track-orphan"));
    QCOMPARE(dataAt(model, 1, Seriona::App::LibraryModel::NodeIdRole).toString(), QStringLiteral("folder"));
}

void LibraryTreeModelTest::fallsBackToNodeOrderWhenRootIdIsMissing()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 9;
    snapshot.rootNodeId = std::string{"missing-root"};
    snapshot.nodes = {
        makeFolder("first", "First"),
        makeTrack("second", "second-id", "Second", "Artist", "Album"),
    };

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("LibraryModel missing root node missing-root.*")));

    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(snapshot);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::NodeIdRole).toString(), QStringLiteral("first"));
    QCOMPARE(dataAt(model, 1, Seriona::App::LibraryModel::NodeIdRole).toString(), QStringLiteral("second"));
}

void LibraryTreeModelTest::mapsCueTrackIdToNodeId()
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
    cueNode.kind = PlaylistNodeKind::Track;
    cueNode.displayName = "Cue Movement";
    cueNode.song = cueSong;

    PlaylistTreeSnapshot snapshot;
    snapshot.version = 10;
    snapshot.nodes = {cueNode};

    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(snapshot);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::TrackIdRole).toString(), QStringLiteral("cue-track-01"));
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::DurationRole).toString(), QStringLiteral("1:01"));
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::FormatRole).toString(), QStringLiteral("FLAC"));
    QCOMPARE(model.nodeIdForTrackId(QStringLiteral("cue-track-01")), QStringLiteral("cue-node"));
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
    first.nodes = {makeTrack("first", "first-track", "First", "Artist", "Album")};

    PlaylistTreeSnapshot second;
    second.version = 2;
    second.nodes = {makeTrack("second", "second-track", "Second", "Artist", "Album")};

    model.setPlaylistTreeSnapshot(first);
    model.setPlaylistTreeSnapshot(second);

    QCOMPARE(resetSpy.count(), 2);
    QCOMPARE(model.version(), 2ULL);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(dataAt(model, 0, Seriona::App::LibraryModel::NodeIdRole).toString(), QStringLiteral("second"));
    QCOMPARE(model.nodeIdForTrackId(QStringLiteral("first-track")), QString());
}

void LibraryTreeModelTest::keepsRoleNamesStable()
{
    const Seriona::App::LibraryModel model;
    const QHash<int, QByteArray> roles = model.roleNames();

    QCOMPARE(roles.value(Seriona::App::LibraryModel::TypeRole), QByteArray("type"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::NameRole), QByteArray("name"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::TitleRole), QByteArray("title"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::ArtistRole), QByteArray("artist"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::AlbumRole), QByteArray("album"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::ParentNameRole), QByteArray("parentName"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::SongCountRole), QByteArray("songCount"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::DurationRole), QByteArray("duration"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::FormatRole), QByteArray("format"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::SampleRateRole), QByteArray("sampleRate"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::BitDepthRole), QByteArray("bitDepth"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::NodeIdRole), QByteArray("nodeId"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::TrackIdRole), QByteArray("trackId"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::IsFolderRole), QByteArray("isFolder"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::IsPlayingRole), QByteArray("isPlaying"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::IsFocusedRole), QByteArray("isFocused"));
    QCOMPARE(roles.value(Seriona::App::LibraryModel::IsExpandedRole), QByteArray("isExpanded"));
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

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.childNodeIds(QStringLiteral("root")), QVector<QString>({QStringLiteral("valid-track")}));
    QCOMPARE(model.parentNodeId(QStringLiteral("valid-track")), QStringLiteral("root"));
    QCOMPARE(dataAt(model, 1, Seriona::App::LibraryModel::NodeIdRole).toString(), QStringLiteral("valid-track"));
}

QTEST_GUILESS_MAIN(LibraryTreeModelTest)

#include "tst_library_tree_model.moc"
