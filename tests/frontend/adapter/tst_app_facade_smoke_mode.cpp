#include "app_facade.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QObject>
#include <QStringList>
#include <QTemporaryDir>
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
    void sourceKeepsFacadeThin();
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

void AppFacadeSmokeModeTest::sourceKeepsFacadeThin()
{
    const QDir sourceRoot(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
    QFile header(sourceRoot.filePath(QStringLiteral("src/app/app_facade.h")));
    QFile implementation(sourceRoot.filePath(QStringLiteral("src/app/app_facade.cpp")));

    QVERIFY2(header.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(header.fileName()));
    QVERIFY2(implementation.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(implementation.fileName()));

    const QString headerText = QString::fromUtf8(header.readAll());
    const QString implementationText = QString::fromUtf8(implementation.readAll());

    const QStringList forbiddenFacadeTokens = {
        QStringLiteral("requestWaveformForSnapshots"),
        QStringLiteral("syncLibraryPlayingTrackId"),
        QStringLiteral("m_currentWaveformCacheKey"),
        QStringLiteral("makeWaveformRequest"),
        QStringLiteral("playingTrackIdFromSnapshot"),
    };

    for (const QString &token : forbiddenFacadeTokens) {
        const QByteArray headerMessage = QStringLiteral("AppFacade header still owns business token: %1").arg(token).toUtf8();
        QVERIFY2(!headerText.contains(token), headerMessage.constData());
        const QByteArray implementationMessage = QStringLiteral("AppFacade implementation still owns business token: %1").arg(token).toUtf8();
        QVERIFY2(!implementationText.contains(token), implementationMessage.constData());
    }
}

void AppFacadeSmokeModeTest::libraryControllerSubmitsTrackActivationThroughBridge()
{
#if SERIONA_HAS_BACKEND
    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", false);

    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    const QString canonicalRoot = QFileInfo(musicDir.path()).absoluteFilePath();

    Seriona::App::AppFacade facade;
    std::vector<QString> scannedRoots;
    facade.library()->setScanExecutor([&scannedRoots](const QString &rootPath) {
        scannedRoots.push_back(rootPath);
        seriona::control::MediaControllerCommandResult result;
        result.accepted = true;
        result.code = seriona::control::MediaControllerErrorCode::None;
        return result;
    });
    QVERIFY(facade.scanLibrary(QUrl::fromLocalFile(musicDir.path())));
    QCOMPARE(scannedRoots, std::vector<QString>{canonicalRoot});
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
