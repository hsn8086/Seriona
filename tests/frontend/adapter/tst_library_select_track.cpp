#include "library_model.h"

#include "seriona/control/control_contracts.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest/QTest>

#include <chrono>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
using Seriona::App::LibraryController;
using Seriona::App::LibraryModel;
using seriona::control::FolderSortDirection;
using seriona::control::FolderSortField;
using seriona::control::FolderSortMissingValuePolicy;
using seriona::control::FolderSortRule;
using seriona::control::MediaControlCommand;
using seriona::control::MediaControlCommandKind;
using seriona::control::MediaControllerCommandResult;
using seriona::control::MediaControllerErrorCode;
using seriona::control::PlaybackContextScope;
using seriona::scanner::PlaylistNode;
using seriona::scanner::PlaylistNodeKind;
using seriona::scanner::PlaylistTreeSnapshot;
using seriona::scanner::SongMetadata;

MediaControllerCommandResult acceptedResult()
{
    MediaControllerCommandResult result;
    result.accepted = true;
    result.code = MediaControllerErrorCode::None;
    return result;
}

struct CommandRecorder {
    std::vector<MediaControlCommand> commands;

    MediaControllerCommandResult record(const MediaControlCommand &command)
    {
        commands.push_back(command);
        return acceptedResult();
    }

    void clear()
    {
        commands.clear();
    }
};

struct ScanRecorder {
    std::vector<QString> roots;

    MediaControllerCommandResult record(const QString &rootPath, seriona::scanner::ScanMode)
    {
        roots.push_back(rootPath);
        return acceptedResult();
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
    snapshot.version = 14;
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

QString scanTemporaryRoot(LibraryController &controller, QTemporaryDir &musicDir)
{
    Q_ASSERT(musicDir.isValid());
    ScanRecorder recorder;
    controller.setScanExecutor([&recorder](const QString &rootPath, seriona::scanner::ScanMode mode) {
        return recorder.record(rootPath, mode);
    });

    const QString canonicalRoot = QFileInfo(musicDir.path()).absoluteFilePath();
    if (!controller.scanLibrary(QUrl::fromLocalFile(musicDir.path()))) {
        qFatal("scanTemporaryRoot expected scanLibrary to accept a temporary root");
    }
    if (recorder.roots != std::vector<QString>{canonicalRoot}) {
        qFatal("scanTemporaryRoot expected scan executor to receive the canonical root");
    }
    return canonicalRoot;
}

void installCommandRecorder(LibraryController &controller, CommandRecorder &recorder)
{
    controller.setCommandExecutor([&recorder](const MediaControlCommand &command) {
        return recorder.record(command);
    });
}

QVariantList sortRules(std::initializer_list<std::pair<QString, QString>> rules)
{
    QVariantList result;
    for (const auto &[field, order] : rules) {
        QVariantMap rule;
        rule.insert(QStringLiteral("field"), field);
        rule.insert(QStringLiteral("order"), order);
        result.append(rule);
    }
    return result;
}

void expectStartPlaybackFromContext(const CommandRecorder &recorder,
                                    const std::string &trackId,
                                    const QString &rootPath,
                                    PlaybackContextScope scope,
                                    const QString &folderNodeId,
                                    std::optional<FolderSortField> field = std::nullopt,
                                    std::optional<FolderSortDirection> direction = std::nullopt)
{
    QCOMPARE(recorder.commands.size(), std::size_t{1});
    const MediaControlCommand &command = recorder.commands.front();
    QCOMPARE(static_cast<int>(command.kind), static_cast<int>(MediaControlCommandKind::StartPlaybackFromContext));
    QVERIFY(command.track.has_value());
    QCOMPARE(command.track->trackId, trackId);
    QVERIFY(command.track->filePath.empty());
    QVERIFY(command.playbackContext.has_value());
    const auto &context = *command.playbackContext;
    QCOMPARE(static_cast<int>(context.scope), static_cast<int>(scope));
    QCOMPARE(QString::fromStdString(context.rootPath.generic_string()), rootPath);
    QCOMPARE(QString::fromStdString(context.folderNodeId), folderNodeId);
    QVERIFY(context.anchorTrack.has_value());
    QCOMPARE(context.anchorTrack->trackId, trackId);
    QVERIFY(context.anchorTrack->filePath.empty());
    if (field.has_value() || direction.has_value()) {
        QVERIFY(field.has_value());
        QVERIFY(direction.has_value());
        QCOMPARE(context.sortRules.size(), std::size_t{1});
        const FolderSortRule &rule = context.sortRules.front();
        QCOMPARE(static_cast<int>(rule.field), static_cast<int>(*field));
        QCOMPARE(static_cast<int>(rule.direction), static_cast<int>(*direction));
        QCOMPARE(static_cast<int>(rule.missingValuePolicy), static_cast<int>(FolderSortMissingValuePolicy::Last));
    } else {
        QCOMPARE(context.sortRules.size(), std::size_t{0});
    }
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

class LibrarySelectTrackTest : public QObject
{
    Q_OBJECT

private slots:
    void explicitTrackNodeActivationSubmitsStartPlaybackFromFolderContext();
    void indexedTrackActivationSubmitsStartPlaybackFromRootContext();
    void searchTrackActivationUsesContainingFolderContext();
    void searchRootTrackActivationFallsBackToRootContext();
    void hiddenTrackActivationDoesNotUseStaleFolderContext();
    void browsingAndLocateCurrentSongDoNotSubmitCommands();
    void locateCurrentSongMovesToContainingProjection();
    void locateMissingCurrentSongPreservesBrowserState();
};

void LibrarySelectTrackTest::explicitTrackNodeActivationSubmitsStartPlaybackFromFolderContext()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    LibraryController controller;
    const QString rootPath = scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setFolderSortExecutor([](const QString &, const QString &, const QVariantList &) {
        return acceptedResult();
    });

    controller.enterFolder(QStringLiteral("album-a"));
    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("desc")}}));

    CommandRecorder recorder;
    installCommandRecorder(controller, recorder);
    controller.playItem(QStringLiteral("track-b"));

    expectStartPlaybackFromContext(recorder,
                                   "track-b-id",
                                   rootPath,
                                   PlaybackContextScope::Folder,
                                   QStringLiteral("album-a"),
                                   FolderSortField::Title,
                                   FolderSortDirection::Descending);
    QCOMPARE(controller.currentFolderName(), QStringLiteral("Album A"));
}

void LibrarySelectTrackTest::indexedTrackActivationSubmitsStartPlaybackFromRootContext()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    LibraryController controller;
    const QString rootPath = scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    CommandRecorder recorder;
    installCommandRecorder(controller, recorder);

    controller.playItem(controller.model()->rowForNodeId(QStringLiteral("track-c")));

    expectStartPlaybackFromContext(recorder,
                                   "track-c-id",
                                   rootPath,
                                   PlaybackContextScope::Root,
                                   QString());
}

void LibrarySelectTrackTest::searchTrackActivationUsesContainingFolderContext()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    LibraryController controller;
    const QString rootPath = scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setSearchQuery(QStringLiteral("Song B"));
    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("desc")}}));

    CommandRecorder recorder;
    installCommandRecorder(controller, recorder);

    controller.playItem(QStringLiteral("track-b"));

    expectStartPlaybackFromContext(recorder,
                                   "track-b-id",
                                   rootPath,
                                   PlaybackContextScope::Folder,
                                   QStringLiteral("album-a"));
    QCOMPARE(controller.currentFolderName(), QStringLiteral("My Music"));
    QCOMPARE(controller.searchQuery(), QStringLiteral("Song B"));
}

void LibrarySelectTrackTest::searchRootTrackActivationFallsBackToRootContext()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    LibraryController controller;
    const QString rootPath = scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setSearchQuery(QStringLiteral("Song C"));

    CommandRecorder recorder;
    installCommandRecorder(controller, recorder);

    controller.playItem(QStringLiteral("track-c"));

    expectStartPlaybackFromContext(recorder,
                                   "track-c-id",
                                   rootPath,
                                   PlaybackContextScope::Root,
                                   QString());
}

void LibrarySelectTrackTest::hiddenTrackActivationDoesNotUseStaleFolderContext()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    LibraryController controller;
    scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.enterFolder(QStringLiteral("album-a"));
    CommandRecorder recorder;
    installCommandRecorder(controller, recorder);

    controller.playItem(QStringLiteral("track-c"));

    QCOMPARE(recorder.commands.size(), std::size_t{0});
    QCOMPARE(controller.currentFolderName(), QStringLiteral("Album A"));
}

void LibrarySelectTrackTest::browsingAndLocateCurrentSongDoNotSubmitCommands()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setPlayingTrackId(QStringLiteral("track-b-id"));
    CommandRecorder recorder;
    controller.setCommandExecutor([&recorder](const MediaControlCommand &command) {
        return recorder.record(command);
    });

    controller.selectBrowserNode(QStringLiteral("album-a"));
    controller.playItem(QStringLiteral("album-a"));
    controller.playItem(controller.model()->rowForNodeId(QStringLiteral("album-a")));
    controller.locateCurrentSong();

    QCOMPARE(recorder.commands.size(), std::size_t{0});
    QCOMPARE(controller.currentFolderName(), QStringLiteral("Album A"));
    expectProjection(controller.model(), {QStringLiteral("track-a"), QStringLiteral("track-b")});
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-b")), 1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-c")), -1);
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.scrollRequest(), QStringLiteral("track-b"));
}

void LibrarySelectTrackTest::locateCurrentSongMovesToContainingProjection()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.setSelectedBrowserNodeId(QStringLiteral("album-a"));
    controller.setPlayingTrackId(QStringLiteral("track-b-id"));

    QCOMPARE(controller.currentFolderName(), QStringLiteral("My Music"));
    expectProjection(controller.model(), {QStringLiteral("album-a"), QStringLiteral("track-c")});
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-b")), -1);

    controller.locateCurrentSong();

    QCOMPARE(controller.currentFolderName(), QStringLiteral("Album A"));
    QCOMPARE(controller.canGoBack(), true);
    expectProjection(controller.model(), {QStringLiteral("track-a"), QStringLiteral("track-b")});
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-b")), 1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("album-a")), -1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-c")), -1);
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-b"));
    QCOMPARE(controller.scrollRequest(), QStringLiteral("track-b"));
}

void LibrarySelectTrackTest::locateMissingCurrentSongPreservesBrowserState()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSnapshot());
    controller.enterFolder(QStringLiteral("album-a"));
    controller.setSelectedBrowserNodeId(QStringLiteral("track-a"));
    controller.setPlayingTrackId(QStringLiteral("missing-track-id"));

    controller.locateCurrentSong();

    QCOMPARE(controller.currentFolderName(), QStringLiteral("Album A"));
    expectProjection(controller.model(), {QStringLiteral("track-a"), QStringLiteral("track-b")});
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-a"));
    QCOMPARE(controller.scrollRequest(), QString());
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-c")), -1);
}

QTEST_GUILESS_MAIN(LibrarySelectTrackTest)

#include "tst_library_select_track.moc"
