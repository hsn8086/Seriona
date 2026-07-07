#include "library_model.h"

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QtTest/QTest>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace {
using Seriona::App::LibraryController;
using Seriona::App::LibraryModel;
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
                       std::optional<std::string> parentNodeId = std::nullopt)
{
    SongMetadata song;
    song.trackId = trackId;
    song.filePath = "/music/" + trackId + ".flac";
    song.sourceFilePath = song.filePath;
    song.title = title;
    song.artist = "Artist";
    song.album = "Album";
    song.sampleRate = 48000;
    song.bitDepth = 24;
    song.duration = std::chrono::milliseconds{120000};

    PlaylistNode node;
    node.nodeId = nodeId;
    node.parentNodeId = std::move(parentNodeId);
    node.kind = PlaylistNodeKind::Track;
    node.displayName = title;
    node.song = std::move(song);
    return node;
}

PlaylistTreeSnapshot makeInitialSnapshot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 1;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"album-a", "track-c"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("album-a", "Album A", {"track-a", "track-b"}, std::string{"root"}, PlaylistNodeKind::Album),
        makeTrack("track-a", "track-a-id", "Song A", std::string{"album-a"}),
        makeTrack("track-b", "track-b-id", "Song B", std::string{"album-a"}),
        makeTrack("track-c", "track-c-id", "Song C", std::string{"root"}),
    };
    return snapshot;
}

QVariant dataForNode(const LibraryModel *model, const QString &nodeId, LibraryModel::Role role)
{
    const int row = model->rowForNodeId(nodeId);
    if (row < 0) {
        return {};
    }
    return model->data(model->index(row, 0), role);
}

}

class LibraryDualCursorTest : public QObject
{
    Q_OBJECT

private slots:
    void dualCursorKeepsFocusedAndPlayingSeparate();
    void followCurrentlyPlayingControlsBrowserCursor();
    void refreshKeepsAndFallsBackBrowserState();
    void browsingActionsStayLocalToProjection();
};

void LibraryDualCursorTest::dualCursorKeepsFocusedAndPlayingSeparate()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeInitialSnapshot());
    LibraryModel *model = controller.model();
    controller.enterFolder(QStringLiteral("album-a"));
    QSignalSpy dataChangedSpy(model, &QAbstractItemModel::dataChanged);

    controller.setFocusedNodeId(QStringLiteral("track-b"));
    controller.setPlayingTrackId(QStringLiteral("track-c-id"));

    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.playingTrackId(), QStringLiteral("track-c-id"));
    QCOMPARE(dataForNode(model, QStringLiteral("track-b"), LibraryModel::IsFocusedRole).toBool(), true);
    QCOMPARE(dataForNode(model, QStringLiteral("track-b"), LibraryModel::IsPlayingRole).toBool(), false);
    QCOMPARE(model->rowForNodeId(QStringLiteral("track-c")), -1);

    QCOMPARE(dataChangedSpy.count(), 1);
}

void LibraryDualCursorTest::followCurrentlyPlayingControlsBrowserCursor()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeInitialSnapshot());
    QSignalSpy scrollSpy(&controller, &LibraryController::scrollRequestChanged);

    controller.setFocusedNodeId(QStringLiteral("track-a"));
    controller.setPlayingTrackId(QStringLiteral("track-b-id"));

    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("root"));
    QCOMPARE(controller.scrollRequest(), QString());
    QCOMPARE(scrollSpy.count(), 0);

    controller.setFollowCurrentlyPlaying(true);

    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.scrollRequest(), QStringLiteral("track-b"));
    QCOMPARE(scrollSpy.count(), 1);

    controller.setPlayingTrackId(QStringLiteral("track-a-id"));

    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-a"));
    QCOMPARE(controller.scrollRequest(), QStringLiteral("track-a"));
    QCOMPARE(scrollSpy.count(), 2);
}

void LibraryDualCursorTest::refreshKeepsAndFallsBackBrowserState()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeInitialSnapshot());

    controller.setSelectedBrowserNodeId(QStringLiteral("track-b"));

    PlaylistTreeSnapshot trackRemoved;
    trackRemoved.version = 2;
    trackRemoved.rootNodeId = std::string{"root"};
    trackRemoved.nodes = {
        makeFolder("root", "Library", {"album-a", "track-c"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("album-a", "Album A", {"track-a"}, std::string{"root"}, PlaylistNodeKind::Album),
        makeTrack("track-a", "track-a-id", "Song A", std::string{"album-a"}),
        makeTrack("track-c", "track-c-id", "Song C", std::string{"root"}),
    };

    controller.setPlaylistTreeSnapshot(trackRemoved);

    QCOMPARE(controller.model()->version(), 2ULL);
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("album-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("album-a"));
    QCOMPARE(controller.model()->rowForNodeId(QStringLiteral("album-a")), 0);

    PlaylistTreeSnapshot albumRemoved;
    albumRemoved.version = 3;
    albumRemoved.rootNodeId = std::string{"root"};
    albumRemoved.nodes = {
        makeFolder("root", "Library", {"track-c"}, std::nullopt, PlaylistNodeKind::Root),
        makeTrack("track-c", "track-c-id", "Song C", std::string{"root"}),
    };

    controller.setPlaylistTreeSnapshot(albumRemoved);

    QCOMPARE(controller.model()->version(), 3ULL);
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("root"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("root"));
    QCOMPARE(controller.model()->rowForNodeId(QStringLiteral("track-c")), 0);
}

void LibraryDualCursorTest::browsingActionsStayLocalToProjection()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeInitialSnapshot());

    controller.setFocusedNodeId(QStringLiteral("track-a"));
    controller.selectBrowserNode(QStringLiteral("track-b"));
    controller.setSearchQuery(QStringLiteral("Song"));
    controller.submitSearch();
    controller.clearSearch();

    QCOMPARE(controller.searchQuery(), QString());
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-a"));
    QCOMPARE(controller.scrollRequest(), QStringLiteral("track-a"));
    QCOMPARE(controller.model()->rowCount(), 2);
    QCOMPARE(controller.model()->rowForNodeId(QStringLiteral("album-a")), 0);
    QCOMPARE(controller.model()->rowForNodeId(QStringLiteral("track-c")), 1);
}

QTEST_GUILESS_MAIN(LibraryDualCursorTest)

#include "tst_library_dual_cursor.moc"
