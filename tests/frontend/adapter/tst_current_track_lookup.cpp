#include "app_facade.h"
#include "library_model.h"

#include "seriona/control/control_contracts.h"

#include <QtTest/QTest>

#include <chrono>

namespace {

seriona::scanner::PlaylistNode makeRootNode()
{
    seriona::scanner::PlaylistNode node;
    node.nodeId = "root";
    node.kind = seriona::scanner::PlaylistNodeKind::Root;
    node.displayName = "Library";
    node.childNodeIds = {"node-shared-path", "node-cue-track"};
    return node;
}

seriona::scanner::PlaylistNode makeTrackNode(
    const std::string &nodeId,
    const std::string &trackId,
    const std::filesystem::path &filePath,
    const std::filesystem::path &sourceFilePath,
    const std::string &title)
{
    seriona::scanner::SongMetadata song;
    song.trackId = trackId;
    song.filePath = filePath;
    song.sourceFilePath = sourceFilePath;
    song.title = title;
    song.artist = "Cue Artist";
    song.album = "Cue Album";
    song.sampleRate = 96000;
    song.bitDepth = 24;
    song.channels = 2;
    song.duration = std::chrono::milliseconds{34000};

    seriona::scanner::PlaylistNode node;
    node.nodeId = nodeId;
    node.parentNodeId = "root";
    node.kind = seriona::scanner::PlaylistNodeKind::Track;
    node.displayName = title;
    node.song = song;
    return node;
}

seriona::control::LibraryStateSnapshot makeCueLibrary()
{
    seriona::control::LibraryStateSnapshot library;
    library.libraryTree = seriona::scanner::PlaylistTreeSnapshot{};
    library.libraryTree->version = 9;
    library.libraryTree->rootNodeId = "root";
    library.libraryTree->nodes = {
        makeRootNode(),
        makeTrackNode("node-shared-path", "shared-path-track", "/music/shared.flac", "/music/shared.flac", "Shared Path Decoy"),
        makeTrackNode("node-cue-track", "cue-track-02", "/music/album.cue", "/music/shared.flac", "Cue Movement")};
    library.libraryTree->nodes.back().song->artworkPath = "/art/cue-cover.png";
    return library;
}

seriona::control::PlayerStateSnapshot makeCuePlayer()
{
    seriona::control::PlayerStateSnapshot player;
    player.currentTrack = seriona::control::TrackIdentity{};
    player.currentTrack->trackId = "cue-track-02";
    player.currentTrack->filePath = "/runtime/playback/decoded-cue-track.wav";
    player.timeline.duration = std::chrono::milliseconds{120000};
    return player;
}

QVariant modelValue(const Seriona::App::LibraryModel &model, const QString &nodeId, int role)
{
    const int row = model.rowForNodeId(nodeId);
    if (row < 0) {
        return {};
    }
    return model.data(model.index(row, 0), role);
}

}

class CurrentTrackLookupTest : public QObject
{
    Q_OBJECT

private slots:
    void currentTrackLookup();
    void cuePathDoesNotDriveHighlight();
};

void CurrentTrackLookupTest::currentTrackLookup()
{
    const seriona::control::LibraryStateSnapshot library = makeCueLibrary();
    seriona::control::PlayerStateSnapshot player = makeCuePlayer();

    Seriona::App::PlaybackController playback;
    playback.applyPlayerStateSnapshot(player, &library);

    QCOMPARE(playback.currentTrackId(), QStringLiteral("cue-track-02"));
    QCOMPARE(playback.currentTrackNodeId(), QStringLiteral("node-cue-track"));
    QCOMPARE(playback.songTitle(), QStringLiteral("Cue Movement"));
    QCOMPARE(playback.artistName(), QStringLiteral("Cue Artist"));
    QCOMPARE(playback.albumName(), QStringLiteral("Cue Album"));
    QCOMPARE(playback.coverArtworkPath(), QStringLiteral("/art/cue-cover.png"));
    QCOMPARE(playback.currentTrackDuration(), 34.0);
    QCOMPARE(playback.totalDuration(), 120.0);
    QCOMPARE(playback.audioFormat(), QStringLiteral("FLAC"));
    QCOMPARE(playback.audioSampleRate(), 96000);
    QCOMPARE(playback.audioBitDepth(), 24);
    QCOMPARE(playback.audioChannels(), 2);
}

void CurrentTrackLookupTest::cuePathDoesNotDriveHighlight()
{
    const seriona::control::LibraryStateSnapshot library = makeCueLibrary();
    seriona::control::PlayerStateSnapshot player = makeCuePlayer();
    player.currentTrack->filePath = "/music/shared.flac";

    Seriona::App::PlaybackController playback;
    playback.applyPlayerStateSnapshot(player, &library);

    Seriona::App::LibraryController libraryController;
    libraryController.setPlaylistTreeSnapshot(*library.libraryTree);
    libraryController.setPlayingTrackId(playback.currentTrackId());

    QCOMPARE(playback.currentTrackNodeId(), QStringLiteral("node-cue-track"));
    QCOMPARE(libraryController.model()->nodeIdForTrackId(playback.currentTrackId()), QStringLiteral("node-cue-track"));
    QCOMPARE(modelValue(*libraryController.model(), QStringLiteral("node-cue-track"), Seriona::App::LibraryModel::IsPlayingRole).toBool(), true);
    QCOMPARE(modelValue(*libraryController.model(), QStringLiteral("node-shared-path"), Seriona::App::LibraryModel::IsPlayingRole).toBool(), false);
}

QTEST_GUILESS_MAIN(CurrentTrackLookupTest)

#include "tst_current_track_lookup.moc"
