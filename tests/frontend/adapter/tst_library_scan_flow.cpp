#include "library_model.h"

#include "seriona/control/control_contracts.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QUrl>
#include <QtTest/QTest>

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
using Seriona::App::LibraryController;
using seriona::control::LibraryScanStatus;
using seriona::control::LibraryStateSnapshot;
using seriona::control::MediaControllerCommandResult;
using seriona::control::MediaControllerErrorCode;
using seriona::scanner::PlaylistNode;
using seriona::scanner::PlaylistNodeKind;
using seriona::scanner::PlaylistTreeSnapshot;
using seriona::scanner::ScanProgress;
using seriona::scanner::ScannerError;
using seriona::scanner::SongMetadata;

MediaControllerCommandResult acceptedResult()
{
    MediaControllerCommandResult result;
    result.accepted = true;
    result.code = MediaControllerErrorCode::None;
    return result;
}

MediaControllerCommandResult rejectedResult(const std::string &message)
{
    MediaControllerCommandResult result;
    result.accepted = false;
    result.code = MediaControllerErrorCode::BackendRejected;
    result.message = message;
    return result;
}

struct ScanRecorder {
    std::vector<QString> roots;
    MediaControllerCommandResult result = acceptedResult();

    MediaControllerCommandResult record(const QString &rootPath)
    {
        roots.push_back(rootPath);
        return result;
    }

    void clear()
    {
        roots.clear();
    }
};

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
    snapshot.version = 15;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"track-a"}, std::nullopt, PlaylistNodeKind::Root),
        makeTrack("track-a", "track-a-id", "Song A", std::string{"root"}),
    };
    return snapshot;
}

PlaylistTreeSnapshot makeEmptySnapshot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 16;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {}, std::nullopt, PlaylistNodeKind::Root),
    };
    return snapshot;
}
}

class LibraryScanFlowTest : public QObject
{
    Q_OBJECT

private slots:
    void emptySnapshotExposesEmptyLibraryState();
    void backendUnavailableDoesNotFabricateRows();
    void scanLibraryRequestSubmitsBackendScanAndUpdatesTree();
    void cancelAndInvalidSelectionDoNotScanOrSaveRoot();
    void scanSnapshotsMapUiStates();
};

void LibraryScanFlowTest::emptySnapshotExposesEmptyLibraryState()
{
    LibraryController controller;
    controller.setScanExecutor([](const QString &) {
        return acceptedResult();
    });

    controller.setPlaylistTreeSnapshot(makeEmptySnapshot());

    QCOMPARE(controller.model()->rowCount(), 0);
    QCOMPARE(controller.visibleNodeCount(), 0);
    QCOMPARE(controller.libraryEmpty(), true);
    QCOMPARE(controller.backendAvailable(), true);
    QCOMPARE(controller.libraryState(), QStringLiteral("empty"));
    QCOMPARE(controller.rowForNodeId(QStringLiteral("root")), -1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("mock-track-stairway")), -1);
}

void LibraryScanFlowTest::backendUnavailableDoesNotFabricateRows()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());

    LibraryController controller;

    QCOMPARE(controller.backendAvailable(), false);
    QCOMPARE(controller.scanLibrary(QUrl::fromLocalFile(musicDir.path())), false);

    QCOMPARE(controller.model()->rowCount(), 0);
    QCOMPARE(controller.visibleNodeCount(), 0);
    QCOMPARE(controller.libraryEmpty(), true);
    QCOMPARE(controller.backendAvailable(), false);
    QCOMPARE(controller.libraryState(), QStringLiteral("backendUnavailable"));
    QCOMPARE(controller.savedRootPath(), QString());
    QCOMPARE(controller.scanStatus(), QStringLiteral("error"));
    QVERIFY(controller.lastError().contains(QStringLiteral("后端扫描服务不可用")));
    QCOMPARE(controller.rowForNodeId(QStringLiteral("mock-track-bohemian")), -1);
}

void LibraryScanFlowTest::scanLibraryRequestSubmitsBackendScanAndUpdatesTree()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    const QString canonicalRoot = QFileInfo(musicDir.path()).absoluteFilePath();

    LibraryController controller;
    ScanRecorder recorder;
    controller.setScanExecutor([&recorder](const QString &rootPath) {
        return recorder.record(rootPath);
    });

    QVERIFY(controller.scanLibrary(QUrl::fromLocalFile(musicDir.path())));

    QCOMPARE(recorder.roots, std::vector<QString>{canonicalRoot});
    QCOMPARE(controller.savedRootPath(), canonicalRoot);
    QCOMPARE(controller.scanStatus(), QStringLiteral("running"));
    QCOMPARE(controller.scanProgress(), 0);
    QCOMPARE(controller.lastError(), QString());

    recorder.clear();
    QVERIFY(controller.refresh());
    QCOMPARE(recorder.roots, std::vector<QString>{canonicalRoot});

    LibraryStateSnapshot completed;
    completed.scanStatus = LibraryScanStatus::Completed;
    completed.libraryTree = makeSnapshot();
    controller.applyLibraryStateSnapshot(completed);
    controller.setPlaylistTreeSnapshot(*completed.libraryTree);

    QCOMPARE(controller.scanStatus(), QStringLiteral("completed"));
    QCOMPARE(controller.scanProgress(), 100);
    QCOMPARE(controller.model()->version(), 15ULL);
    QCOMPARE(controller.visibleNodeCount(), 1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("root")), -1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-a")), 0);

    QTemporaryDir rejectedDir;
    QVERIFY(rejectedDir.isValid());
    const QString rejectedRoot = QFileInfo(rejectedDir.path()).absoluteFilePath();
    const QString previousRoot = controller.savedRootPath();
    const std::uint64_t previousVersion = controller.model()->version();
    const int previousVisibleNodeCount = controller.visibleNodeCount();

    recorder.clear();
    recorder.result = rejectedResult("backend scan rejected");

    QCOMPARE(controller.scanLibrary(QUrl::fromLocalFile(rejectedDir.path())), false);

    QCOMPARE(recorder.roots, std::vector<QString>{rejectedRoot});
    QCOMPARE(controller.savedRootPath(), previousRoot);
    QCOMPARE(controller.model()->version(), previousVersion);
    QCOMPARE(controller.visibleNodeCount(), previousVisibleNodeCount);
    QCOMPARE(controller.scanStatus(), QStringLiteral("error"));
    QCOMPARE(controller.lastError(), QStringLiteral("backend scan rejected"));
}

void LibraryScanFlowTest::cancelAndInvalidSelectionDoNotScanOrSaveRoot()
{
    QTemporaryFile nonDirectory;
    QVERIFY(nonDirectory.open());

    LibraryController controller;
    ScanRecorder recorder;
    controller.setScanExecutor([&recorder](const QString &rootPath) {
        return recorder.record(rootPath);
    });

    QCOMPARE(controller.scanLibrary(QUrl()), false);
    QCOMPARE(controller.scanLibrary(QUrl::fromLocalFile(nonDirectory.fileName())), false);
    QCOMPARE(controller.refresh(), false);

    QCOMPARE(recorder.roots.size(), std::size_t{0});
    QCOMPARE(controller.savedRootPath(), QString());
    QCOMPARE(controller.scanStatus(), QStringLiteral("error"));
    QVERIFY(!controller.lastError().isEmpty());
}

void LibraryScanFlowTest::scanSnapshotsMapUiStates()
{
    LibraryController controller;

    LibraryStateSnapshot idle;
    idle.scanStatus = LibraryScanStatus::Idle;
    controller.applyLibraryStateSnapshot(idle);
    QCOMPARE(controller.scanStatus(), QStringLiteral("pending"));
    QCOMPARE(controller.scanProgress(), 0);

    LibraryStateSnapshot running;
    running.scanStatus = LibraryScanStatus::Scanning;
    ScanProgress progress;
    progress.filesDiscovered = 10;
    progress.filesScanned = 4;
    running.scanProgress = progress;
    controller.applyLibraryStateSnapshot(running);
    QCOMPARE(controller.scanStatus(), QStringLiteral("running"));
    QCOMPARE(controller.scanProgress(), 40);

    LibraryStateSnapshot completed;
    completed.scanStatus = LibraryScanStatus::Completed;
    controller.applyLibraryStateSnapshot(completed);
    QCOMPARE(controller.scanStatus(), QStringLiteral("completed"));
    QCOMPARE(controller.scanProgress(), 100);
    QCOMPARE(controller.lastError(), QString());

    LibraryStateSnapshot failed;
    failed.scanStatus = LibraryScanStatus::Error;
    ScannerError error;
    error.message = "metadata read failed";
    failed.lastError = error;
    controller.applyLibraryStateSnapshot(failed);
    QCOMPARE(controller.scanStatus(), QStringLiteral("error"));
    QCOMPARE(controller.lastError(), QStringLiteral("metadata read failed"));
}

QTEST_GUILESS_MAIN(LibraryScanFlowTest)

#include "tst_library_scan_flow.moc"
