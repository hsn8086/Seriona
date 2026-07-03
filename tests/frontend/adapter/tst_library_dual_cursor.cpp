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

void expectRoleChange(const QSignalSpy &spy, int signalIndex, int row, LibraryModel::Role role)
{
    const QList<QVariant> arguments = spy.at(signalIndex);
    const QModelIndex topLeft = arguments.at(0).value<QModelIndex>();
    const QModelIndex bottomRight = arguments.at(1).value<QModelIndex>();
    const QList<int> roles = arguments.at(2).value<QList<int>>();

    QCOMPARE(topLeft.row(), row);
    QCOMPARE(bottomRight.row(), row);
    QCOMPARE(roles, QList<int>{role});
}
}

class LibraryDualCursorTest : public QObject
{
    Q_OBJECT

private slots:
    void dualCursorKeepsFocusedAndPlayingSeparate();
    void followCurrentlyPlayingControlsBrowserCursor();
    void refreshKeepsAndFallsBackBrowserState();
    void browsingActionsDoNotEmitBackendHookSignals();
};

void LibraryDualCursorTest::dualCursorKeepsFocusedAndPlayingSeparate()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeInitialSnapshot());
    LibraryModel *model = controller.model();
    QSignalSpy dataChangedSpy(model, &QAbstractItemModel::dataChanged);

    controller.expandNode(QStringLiteral("album-a"));
    controller.setFocusedNodeId(QStringLiteral("track-b"));
    controller.setPlayingTrackId(QStringLiteral("track-c-id"));

    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.playingTrackId(), QStringLiteral("track-c-id"));
    QCOMPARE(dataForNode(model, QStringLiteral("album-a"), LibraryModel::IsExpandedRole).toBool(), true);
    QCOMPARE(dataForNode(model, QStringLiteral("track-b"), LibraryModel::IsFocusedRole).toBool(), true);
    QCOMPARE(dataForNode(model, QStringLiteral("track-b"), LibraryModel::IsPlayingRole).toBool(), false);
    QCOMPARE(dataForNode(model, QStringLiteral("track-c"), LibraryModel::IsFocusedRole).toBool(), false);
    QCOMPARE(dataForNode(model, QStringLiteral("track-c"), LibraryModel::IsPlayingRole).toBool(), true);

    QCOMPARE(dataChangedSpy.count(), 6);
    expectRoleChange(dataChangedSpy, 0, model->rowForNodeId(QStringLiteral("album-a")), LibraryModel::IsExpandedRole);
    expectRoleChange(dataChangedSpy, 1, model->rowForNodeId(QStringLiteral("track-a")), LibraryModel::IsVisibleRole);
    expectRoleChange(dataChangedSpy, 2, model->rowForNodeId(QStringLiteral("track-b")), LibraryModel::IsVisibleRole);
    expectRoleChange(dataChangedSpy, 3, model->rowForNodeId(QStringLiteral("root")), LibraryModel::IsFocusedRole);
    expectRoleChange(dataChangedSpy, 4, model->rowForNodeId(QStringLiteral("track-b")), LibraryModel::IsFocusedRole);
    expectRoleChange(dataChangedSpy, 5, model->rowForNodeId(QStringLiteral("track-c")), LibraryModel::IsPlayingRole);
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

    controller.expandNode(QStringLiteral("album-a"));
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
    QCOMPARE(controller.expandedNodeIds(), QStringList{QStringLiteral("album-a")});
    QCOMPARE(dataForNode(controller.model(), QStringLiteral("album-a"), LibraryModel::IsExpandedRole).toBool(), true);

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
    QCOMPARE(controller.expandedNodeIds(), QStringList{});
}

void LibraryDualCursorTest::browsingActionsDoNotEmitBackendHookSignals()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeInitialSnapshot());
    QSignalSpy searchSubmittedSpy(&controller, &LibraryController::searchSubmitted);
    QSignalSpy searchClearedSpy(&controller, &LibraryController::searchCleared);
    QSignalSpy playItemRequestedSpy(&controller, &LibraryController::playItemRequested);
    QSignalSpy currentSongLocationSpy(&controller, &LibraryController::currentSongLocationRequested);

    controller.expandNode(QStringLiteral("album-a"));
    controller.collapseNode(QStringLiteral("album-a"));
    controller.toggleExpanded(QStringLiteral("album-a"));
    controller.focusNode(QStringLiteral("track-a"));
    controller.selectBrowserNode(QStringLiteral("track-b"));
    controller.requestScrollToNode(QStringLiteral("track-b"));
    controller.setSearchQuery(QStringLiteral("Song"));
    controller.submitSearch();
    controller.clearSearch();

    QCOMPARE(searchSubmittedSpy.count(), 0);
    QCOMPARE(searchClearedSpy.count(), 0);
    QCOMPARE(playItemRequestedSpy.count(), 0);
    QCOMPARE(currentSongLocationSpy.count(), 0);
}

QTEST_GUILESS_MAIN(LibraryDualCursorTest)

#include "tst_library_dual_cursor.moc"
