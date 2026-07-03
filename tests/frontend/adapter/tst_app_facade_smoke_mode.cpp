#include "app_facade.h"

#include <QCoreApplication>
#include <QObject>
#include <QVariant>
#include <QtTest/QTest>

#if SERIONA_HAS_BACKEND
#include <chrono>
#include <optional>
#include <string>
#include <utility>
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

PlaylistNode makeTrack(const std::string &nodeId, const std::string &trackId)
{
    SongMetadata song;
    song.trackId = trackId;
    song.title = "Song C";
    song.duration = std::chrono::milliseconds{120000};

    PlaylistNode node;
    node.nodeId = nodeId;
    node.parentNodeId = std::string{"root"};
    node.kind = PlaylistNodeKind::Track;
    node.displayName = "Song C";
    node.song = std::move(song);
    return node;
}

PlaylistTreeSnapshot makeSnapshot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 140;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"track-c"}, std::nullopt, PlaylistNodeKind::Root),
        makeTrack("track-c", "track-c-id"),
    };
    return snapshot;
}

seriona::control::LibraryStateSnapshot makeLibrarySnapshot()
{
    seriona::control::LibraryStateSnapshot snapshot;
    snapshot.libraryTree = makeSnapshot();
    return snapshot;
}

seriona::control::PlayerStateSnapshot makePlayerSnapshot()
{
    seriona::control::PlayerStateSnapshot snapshot;
    snapshot.currentTrack = seriona::control::TrackIdentity{};
    snapshot.currentTrack->trackId = "track-c-id";
    return snapshot;
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
#endif

class AppFacadeSmokeModeTest : public QObject
{
    Q_OBJECT

private slots:
    void doesNotStartBackendBridgeWhenSmokeDisablesAutostart();
    void libraryControllerSubmitsTrackActivationThroughBridge();
    void lateLibrarySnapshotReappliesPlayingHighlight();
};

void AppFacadeSmokeModeTest::doesNotStartBackendBridgeWhenSmokeDisablesAutostart()
{
    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", false);

    Seriona::App::AppFacade facade;

    QCOMPARE(facade.backendBridgeStartedForTests(), false);

    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", QVariant{});
}

void AppFacadeSmokeModeTest::libraryControllerSubmitsTrackActivationThroughBridge()
{
#if SERIONA_HAS_BACKEND
    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", false);

    Seriona::App::AppFacade facade;
    facade.library()->setPlaylistTreeSnapshot(makeSnapshot());

    QCOMPARE(facade.backendNotificationCountForTests(), std::size_t{0});
    facade.library()->playItem(QStringLiteral("track-c"));

    QCOMPARE(facade.backendNotificationCountForTests(), std::size_t{1});

    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", QVariant{});
#else
    QSKIP("backend disabled");
#endif
}

void AppFacadeSmokeModeTest::lateLibrarySnapshotReappliesPlayingHighlight()
{
#if SERIONA_HAS_BACKEND
    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", false);

    Seriona::App::AppFacade facade;
    const seriona::control::PlayerStateSnapshot player = makePlayerSnapshot();
    facade.applyPlayerSnapshotForTests(player, seriona::control::LibraryStateSnapshot{});
    facade.library()->setPlayingTrackId(QString());
    facade.applyLibrarySnapshotForTests(player, makeLibrarySnapshot());

    QCOMPARE(facade.playback()->currentTrackId(), QStringLiteral("track-c-id"));
    QCOMPARE(facade.playback()->currentTrackNodeId(), QStringLiteral("track-c"));
    QCOMPARE(facade.library()->playingTrackId(), QStringLiteral("track-c-id"));
    QCOMPARE(modelValue(*facade.library()->model(), QStringLiteral("track-c"), Seriona::App::LibraryModel::IsPlayingRole).toBool(), true);

    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", QVariant{});
#else
    QSKIP("backend disabled");
#endif
}

QTEST_GUILESS_MAIN(AppFacadeSmokeModeTest)

#include "tst_app_facade_smoke_mode.moc"
