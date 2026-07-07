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

QString nodeIdAt(const LibraryModel *model, int row)
{
    return model->data(model->index(row, 0), LibraryModel::NodeIdRole).toString();
}

void expectProjection(const LibraryModel *model, const QVector<QString> &nodeIds)
{
    QCOMPARE(model->rowCount(), nodeIds.size());
    for (int row = 0; row < nodeIds.size(); ++row) {
        QCOMPARE(nodeIdAt(model, row), nodeIds.at(row));
        QCOMPARE(model->rowForNodeId(nodeIds.at(row)), row);
    }
}
}

class SidebarLocalBrowsingTest : public QObject
{
    Q_OBJECT

private slots:
    void localSearchFiltersAndFocusesFirstMatch();
    void emptySearchDoesNotMoveBrowserOrPlayback();
    void enterFolderAndBackStayLocal();
    void enterFolderShowsFlatDirectChildrenOnly();
    void selectingRootProjectionNodeDoesNotEnableBack();
};

void SidebarLocalBrowsingTest::localSearchFiltersAndFocusesFirstMatch()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setPlayingTrackId(QStringLiteral("track-c-id"));
    QSignalSpy scrollSpy(&controller, &LibraryController::scrollRequestChanged);

    controller.setSearchQuery(QStringLiteral("Song B"));

    QCOMPARE(controller.visibleNodeCount(), 1);
    expectProjection(controller.model(), {QStringLiteral("track-b")});
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-a")), -1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("album-a")), -1);

    controller.submitSearch();

    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.scrollRequest(), QStringLiteral("track-b"));
    QCOMPARE(controller.playingTrackId(), QStringLiteral("track-c-id"));
    QCOMPARE(scrollSpy.count(), 1);
}

void SidebarLocalBrowsingTest::emptySearchDoesNotMoveBrowserOrPlayback()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setSelectedBrowserNodeId(QStringLiteral("album-a"));
    controller.setPlayingTrackId(QStringLiteral("track-c-id"));
    QSignalSpy scrollSpy(&controller, &LibraryController::scrollRequestChanged);

    controller.setSearchQuery(QStringLiteral("missing local song"));
    controller.submitSearch();

    QCOMPARE(controller.visibleNodeCount(), 0);
    expectProjection(controller.model(), {});
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("album-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("album-a"));
    QCOMPARE(controller.scrollRequest(), QString());
    QCOMPARE(controller.playingTrackId(), QStringLiteral("track-c-id"));
    QCOMPARE(scrollSpy.count(), 0);

    controller.clearSearch();

    QCOMPARE(controller.visibleNodeCount(), 2);
    expectProjection(controller.model(), {QStringLiteral("album-a"), QStringLiteral("track-c")});
    QCOMPARE(controller.rowForNodeId(QStringLiteral("root")), -1);
}

void SidebarLocalBrowsingTest::enterFolderAndBackStayLocal()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setPlayingTrackId(QStringLiteral("track-c-id"));

    QCOMPARE(controller.currentFolderName(), QStringLiteral("My Music"));
    QCOMPARE(controller.visibleNodeCount(), 2);
    expectProjection(controller.model(), {QStringLiteral("album-a"), QStringLiteral("track-c")});
    QCOMPARE(controller.rowForNodeId(QStringLiteral("root")), -1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-a")), -1);

    controller.enterFolder(QStringLiteral("album-a"));

    QCOMPARE(controller.currentFolderName(), QStringLiteral("Album A"));
    QCOMPARE(controller.canGoBack(), true);
    QCOMPARE(controller.visibleNodeCount(), 2);
    expectProjection(controller.model(), {QStringLiteral("track-a"), QStringLiteral("track-b")});
    QCOMPARE(controller.rowForNodeId(QStringLiteral("album-a")), -1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-c")), -1);

    controller.goBack();

    QCOMPARE(controller.currentFolderName(), QStringLiteral("My Music"));
    QCOMPARE(controller.visibleNodeCount(), 2);
    expectProjection(controller.model(), {QStringLiteral("album-a"), QStringLiteral("track-c")});
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("album-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("album-a"));
    QCOMPARE(controller.scrollRequest(), QStringLiteral("album-a"));
    QCOMPARE(controller.playingTrackId(), QStringLiteral("track-c-id"));
}

void SidebarLocalBrowsingTest::enterFolderShowsFlatDirectChildrenOnly()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setPlayingTrackId(QStringLiteral("track-c-id"));

    controller.enterFolder(QStringLiteral("album-a"));

    QCOMPARE(controller.currentFolderName(), QStringLiteral("Album A"));
    QCOMPARE(controller.canGoBack(), true);
    QCOMPARE(controller.visibleNodeCount(), 2);
    expectProjection(controller.model(), {QStringLiteral("track-a"), QStringLiteral("track-b")});
    QCOMPARE(controller.rowForNodeId(QStringLiteral("root")), -1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("album-a")), -1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-c")), -1);

    controller.goBack();

    QCOMPARE(controller.currentFolderName(), QStringLiteral("My Music"));
    QCOMPARE(controller.visibleNodeCount(), 2);
    expectProjection(controller.model(), {QStringLiteral("album-a"), QStringLiteral("track-c")});
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-a")), -1);
}

void SidebarLocalBrowsingTest::selectingRootProjectionNodeDoesNotEnableBack()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());

    controller.setSelectedBrowserNodeId(QStringLiteral("album-a"));

    QCOMPARE(controller.currentFolderName(), QStringLiteral("My Music"));
    QCOMPARE(controller.canGoBack(), false);
    expectProjection(controller.model(), {QStringLiteral("album-a"), QStringLiteral("track-c")});

    controller.goBack();

    QCOMPARE(controller.currentFolderName(), QStringLiteral("My Music"));
    QCOMPARE(controller.canGoBack(), false);
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("album-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("album-a"));
    QCOMPARE(controller.scrollRequest(), QString());
    expectProjection(controller.model(), {QStringLiteral("album-a"), QStringLiteral("track-c")});
}

QTEST_GUILESS_MAIN(SidebarLocalBrowsingTest)

#include "tst_sidebar_local_browsing.moc"
