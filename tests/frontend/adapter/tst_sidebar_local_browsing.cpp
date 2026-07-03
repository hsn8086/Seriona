#include "library_model.h"

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

PlaylistTreeSnapshot makeSnapshot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 13;
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

class SidebarLocalBrowsingTest : public QObject
{
    Q_OBJECT

private slots:
    void localSearchFiltersAndFocusesFirstMatch();
    void emptySearchDoesNotMoveBrowserOrPlayback();
    void expansionAndBackStayLocal();
};

void SidebarLocalBrowsingTest::localSearchFiltersAndFocusesFirstMatch()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setPlayingTrackId(QStringLiteral("track-c-id"));
    QSignalSpy playSpy(&controller, &LibraryController::playItemRequested);
    QSignalSpy currentSongSpy(&controller, &LibraryController::currentSongLocationRequested);
    QSignalSpy scrollSpy(&controller, &LibraryController::scrollRequestChanged);

    controller.setSearchQuery(QStringLiteral("Song B"));

    QCOMPARE(controller.visibleNodeCount(), 1);
    QCOMPARE(dataForNode(controller.model(), QStringLiteral("track-b"), LibraryModel::MatchesSearchRole).toBool(), true);
    QCOMPARE(dataForNode(controller.model(), QStringLiteral("track-b"), LibraryModel::IsVisibleRole).toBool(), true);
    QCOMPARE(dataForNode(controller.model(), QStringLiteral("track-a"), LibraryModel::IsVisibleRole).toBool(), false);

    controller.submitSearch();

    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.scrollRequest(), QStringLiteral("track-b"));
    QCOMPARE(controller.playingTrackId(), QStringLiteral("track-c-id"));
    QCOMPARE(playSpy.count(), 0);
    QCOMPARE(currentSongSpy.count(), 0);
    QCOMPARE(scrollSpy.count(), 1);
}

void SidebarLocalBrowsingTest::emptySearchDoesNotMoveBrowserOrPlayback()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setSelectedBrowserNodeId(QStringLiteral("track-a"));
    controller.setPlayingTrackId(QStringLiteral("track-c-id"));
    QSignalSpy playSpy(&controller, &LibraryController::playItemRequested);
    QSignalSpy currentSongSpy(&controller, &LibraryController::currentSongLocationRequested);
    QSignalSpy scrollSpy(&controller, &LibraryController::scrollRequestChanged);

    controller.setSearchQuery(QStringLiteral("missing local song"));
    controller.submitSearch();

    QCOMPARE(controller.visibleNodeCount(), 0);
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-a"));
    QCOMPARE(controller.scrollRequest(), QString());
    QCOMPARE(controller.playingTrackId(), QStringLiteral("track-c-id"));
    QCOMPARE(playSpy.count(), 0);
    QCOMPARE(currentSongSpy.count(), 0);
    QCOMPARE(scrollSpy.count(), 0);

    controller.clearSearch();

    QCOMPARE(controller.visibleNodeCount(), 3);
    QCOMPARE(dataForNode(controller.model(), QStringLiteral("album-a"), LibraryModel::IsVisibleRole).toBool(), true);
}

void SidebarLocalBrowsingTest::expansionAndBackStayLocal()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setPlayingTrackId(QStringLiteral("track-c-id"));
    QSignalSpy playSpy(&controller, &LibraryController::playItemRequested);
    QSignalSpy currentSongSpy(&controller, &LibraryController::currentSongLocationRequested);

    QCOMPARE(controller.visibleNodeCount(), 3);
    QCOMPARE(dataForNode(controller.model(), QStringLiteral("track-a"), LibraryModel::IsVisibleRole).toBool(), false);

    controller.expandNode(QStringLiteral("album-a"));

    QCOMPARE(controller.visibleNodeCount(), 5);
    QCOMPARE(dataForNode(controller.model(), QStringLiteral("album-a"), LibraryModel::IsExpandedRole).toBool(), true);
    QCOMPARE(dataForNode(controller.model(), QStringLiteral("track-a"), LibraryModel::IsVisibleRole).toBool(), true);

    controller.selectBrowserNode(QStringLiteral("track-a"));
    controller.goBack();

    QCOMPARE(controller.focusedNodeId(), QStringLiteral("album-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("album-a"));
    QCOMPARE(controller.scrollRequest(), QStringLiteral("album-a"));
    QCOMPARE(controller.playingTrackId(), QStringLiteral("track-c-id"));

    controller.collapseNode(QStringLiteral("album-a"));

    QCOMPARE(controller.visibleNodeCount(), 3);
    QCOMPARE(dataForNode(controller.model(), QStringLiteral("track-a"), LibraryModel::IsVisibleRole).toBool(), false);
    QCOMPARE(playSpy.count(), 0);
    QCOMPARE(currentSongSpy.count(), 0);
}

QTEST_GUILESS_MAIN(SidebarLocalBrowsingTest)

#include "tst_sidebar_local_browsing.moc"
