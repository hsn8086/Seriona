#include "library_model.h"

#include "seriona/control/control_contracts.h"

#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest/QTest>

#include <chrono>
#include <cstdint>
#include <filesystem>
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
using seriona::control::FolderSortSetting;
using seriona::control::MediaControlCommand;
using seriona::control::MediaControlCommandKind;
using seriona::control::MediaControllerCommandResult;
using seriona::control::MediaControllerErrorCode;
using seriona::control::PlayerStateSnapshot;
using seriona::control::TrackIdentity;
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

    MediaControllerCommandResult record(const QString &rootPath)
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
                       const std::string &displayName,
                       const std::string &title,
                       const std::string &artist,
                       const std::string &album,
                       std::chrono::milliseconds duration,
                       std::optional<std::string> parentNodeId = std::nullopt,
                       std::optional<std::uint32_t> year = std::nullopt,
                       std::optional<std::uint32_t> discNumber = std::nullopt,
                       std::optional<std::uint32_t> trackNumber = std::nullopt,
                       std::optional<std::filesystem::file_time_type> fileMtime = std::nullopt)
{
    SongMetadata song;
    song.trackId = trackId;
    song.filePath = "/music/" + displayName;
    song.sourceFilePath = song.filePath;
    song.title = title;
    song.artist = artist;
    song.album = album;
    song.sampleRate = 48000;
    song.bitDepth = 24;
    song.duration = duration;
    song.year = year;
    song.discNumber = discNumber;
    song.trackNumber = trackNumber;
    song.fileMtime = fileMtime;

    PlaylistNode node;
    node.nodeId = nodeId;
    node.parentNodeId = std::move(parentNodeId);
    node.kind = PlaylistNodeKind::Track;
    node.displayName = displayName;
    node.song = std::move(song);
    return node;
}

PlaylistTreeSnapshot makeSortableSnapshot(std::uint64_t version = 21)
{
    const std::filesystem::file_time_type baseTime{};
    PlaylistTreeSnapshot snapshot;
    snapshot.version = version;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"folder-jazz", "track-root-z", "track-root-a"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("folder-jazz", "Jazz", {"track-folder-b", "track-folder-a", "track-folder-c"}, std::string{"root"}, PlaylistNodeKind::Album),
        makeTrack("track-folder-b", "track-folder-b-id", "02-beta.flac", "Beta Tune", "Charlie", "Album Z", std::chrono::milliseconds{180000}, std::string{"folder-jazz"}, std::uint32_t{2021}, std::uint32_t{2}, std::uint32_t{7}, baseTime + std::chrono::seconds{20}),
        makeTrack("track-folder-a", "track-folder-a-id", "01-alpha.flac", "Alpha Tune", "Delta", "Album A", std::chrono::milliseconds{60000}, std::string{"folder-jazz"}, std::uint32_t{2019}, std::uint32_t{1}, std::uint32_t{3}, baseTime + std::chrono::seconds{10}),
        makeTrack("track-folder-c", "track-folder-c-id", "03-gamma.flac", "Gamma Tune", "Bravo", "Album M", std::chrono::milliseconds{240000}, std::string{"folder-jazz"}, std::uint32_t{2020}, std::uint32_t{1}, std::uint32_t{9}, baseTime + std::chrono::seconds{30}),
        makeTrack("track-root-z", "track-root-z-id", "z-root.flac", "Zulu Root", "Root Artist B", "Root Album", std::chrono::milliseconds{300000}, std::string{"root"}, std::uint32_t{2022}, std::uint32_t{1}, std::uint32_t{2}, baseTime + std::chrono::seconds{50}),
        makeTrack("track-root-a", "track-root-a-id", "a-root.flac", "Alpha Root", "Root Artist A", "Root Album", std::chrono::milliseconds{90000}, std::string{"root"}, std::uint32_t{2018}, std::uint32_t{1}, std::uint32_t{1}, baseTime + std::chrono::seconds{40}),
    };
    return snapshot;
}

PlaylistTreeSnapshot makeUpdatedSortableSnapshot()
{
    const std::filesystem::file_time_type baseTime{};
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 22;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"folder-jazz", "track-root-a", "track-root-z"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("folder-jazz", "Jazz", {"track-folder-c", "track-folder-a", "track-folder-b"}, std::string{"root"}, PlaylistNodeKind::Album),
        makeTrack("track-folder-c", "track-folder-c-id", "03-gamma.flac", "Gamma Tune", "Bravo", "Album M", std::chrono::milliseconds{240000}, std::string{"folder-jazz"}, std::uint32_t{2020}, std::uint32_t{1}, std::uint32_t{9}, baseTime + std::chrono::seconds{30}),
        makeTrack("track-folder-a", "track-folder-a-id", "01-alpha.flac", "Alpha Tune", "Delta", "Album A", std::chrono::milliseconds{60000}, std::string{"folder-jazz"}, std::uint32_t{2019}, std::uint32_t{1}, std::uint32_t{3}, baseTime + std::chrono::seconds{10}),
        makeTrack("track-folder-b", "track-folder-b-id", "02-beta.flac", "Beta Tune", "Charlie", "Album Z", std::chrono::milliseconds{180000}, std::string{"folder-jazz"}, std::uint32_t{2021}, std::uint32_t{2}, std::uint32_t{7}, baseTime + std::chrono::seconds{20}),
        makeTrack("track-root-a", "track-root-a-id", "a-root.flac", "Alpha Root", "Root Artist A", "Root Album", std::chrono::milliseconds{90000}, std::string{"root"}, std::uint32_t{2018}, std::uint32_t{1}, std::uint32_t{1}, baseTime + std::chrono::seconds{40}),
        makeTrack("track-root-z", "track-root-z-id", "z-root.flac", "Zulu Root", "Root Artist B", "Root Album", std::chrono::milliseconds{300000}, std::string{"root"}, std::uint32_t{2022}, std::uint32_t{1}, std::uint32_t{2}, baseTime + std::chrono::seconds{50}),
    };
    return snapshot;
}

PlaylistTreeSnapshot makeFolderRemovedSnapshot()
{
    const std::filesystem::file_time_type baseTime{};
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 23;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"track-root-a", "track-root-z"}, std::nullopt, PlaylistNodeKind::Root),
        makeTrack("track-root-a", "track-root-a-id", "a-root.flac", "Alpha Root", "Root Artist A", "Root Album", std::chrono::milliseconds{90000}, std::string{"root"}, std::uint32_t{2018}, std::uint32_t{1}, std::uint32_t{1}, baseTime + std::chrono::seconds{40}),
        makeTrack("track-root-z", "track-root-z-id", "z-root.flac", "Zulu Root", "Root Artist B", "Root Album", std::chrono::milliseconds{300000}, std::string{"root"}, std::uint32_t{2022}, std::uint32_t{1}, std::uint32_t{2}, baseTime + std::chrono::seconds{50}),
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

bool nodeIsPlaying(const LibraryModel *model, const QString &nodeId)
{
    const int row = model->rowForNodeId(nodeId);
    if (row < 0) {
        return false;
    }
    return model->data(model->index(row, 0), LibraryModel::IsPlayingRole).toBool();
}

PlayerStateSnapshot playerSnapshotForTrack(const std::string &trackId)
{
    PlayerStateSnapshot snapshot;
    snapshot.currentTrack = TrackIdentity{};
    snapshot.currentTrack->trackId = trackId;
    return snapshot;
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

QString scanTemporaryRoot(LibraryController &controller, QTemporaryDir &musicDir)
{
    Q_ASSERT(musicDir.isValid());
    ScanRecorder recorder;
    controller.setScanExecutor([&recorder](const QString &rootPath) {
        return recorder.record(rootPath);
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

void expectFolderSortCommand(const CommandRecorder &recorder,
                             const QString &rootPath,
                             const QString &folderNodeId,
                             FolderSortField field,
                             FolderSortDirection direction)
{
    QCOMPARE(recorder.commands.size(), std::size_t{1});
    const MediaControlCommand &command = recorder.commands.front();
    QCOMPARE(static_cast<int>(command.kind), static_cast<int>(MediaControlCommandKind::ApplyFolderSortRules));
    QVERIFY(command.folderSortSetting.has_value());
    QCOMPARE(QString::fromStdString(command.folderSortSetting->rootPath.generic_string()), rootPath);
    QCOMPARE(QString::fromStdString(command.folderSortSetting->folderNodeId), folderNodeId);
    QCOMPARE(command.folderSortSetting->rules.size(), std::size_t{1});
    const FolderSortRule &rule = command.folderSortSetting->rules.front();
    QCOMPARE(static_cast<int>(rule.field), static_cast<int>(field));
    QCOMPARE(static_cast<int>(rule.direction), static_cast<int>(direction));
    QCOMPARE(static_cast<int>(rule.missingValuePolicy), static_cast<int>(FolderSortMissingValuePolicy::Last));
}
}

class LibrarySortTest : public QObject
{
    Q_OBJECT

private slots:
    void noRulesPreserveScannerOrderForRootFolderAndSearch();
    void emptyRulesRestoreScannerOrderAfterSorting();
    void titleAscendingAndDescendingSortCurrentFolderProjection();
    void supportedSortDialogFieldsSortCurrentProjection();
    void currentRulesReapplyAcrossEnterFolderSearchSubmitAndClear();
    void snapshotUpdateReappliesRulesWithoutMutatingScannerOrder();
    void snapshotReconcileKeepsRootSearchSortAndPlaybackMarkers();
    void snapshotReconcileFallsBackToVisibleRowsWhenFolderDisappears();
    void invalidSortPayloadLeavesProjectionUnchanged();
    void folderSortSendsApplyCommandWithRootAndFolderKey();
    void sameFolderNodeIdUnderDifferentRootsDoesNotReuseSavedRules();
    void reenterFolderAndBackendStateReloadRestoreSavedRules();
    void backendFolderSortNotificationUpdatesActiveCurrentRules();
    void currentSortRulesExposeFolderRulesForQmlAndFallbacks();
    void searchProjectionSortDoesNotPersistOrOverwriteSavedFolderRules();
    void missingContextAndMalformedSortPayloadDoNotPersist();
};

void LibrarySortTest::noRulesPreserveScannerOrderForRootFolderAndSearch()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());

    expectProjection(controller.model(), {QStringLiteral("folder-jazz"), QStringLiteral("track-root-z"), QStringLiteral("track-root-a")});
    QCOMPARE(controller.model()->childNodeIds(QStringLiteral("root")), QVector<QString>({QStringLiteral("folder-jazz"), QStringLiteral("track-root-z"), QStringLiteral("track-root-a")}));

    controller.enterFolder(QStringLiteral("folder-jazz"));
    expectProjection(controller.model(), {QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")});
    QCOMPARE(controller.model()->childNodeIds(QStringLiteral("folder-jazz")), QVector<QString>({QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")}));

    controller.setSearchQuery(QStringLiteral("Tune"));
    expectProjection(controller.model(), {QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")});

    controller.clearSearch();
    expectProjection(controller.model(), {QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")});
}

void LibrarySortTest::emptyRulesRestoreScannerOrderAfterSorting()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));

    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("asc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    controller.applySortRules({});

    expectProjection(controller.model(), {QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")});
}

void LibrarySortTest::titleAscendingAndDescendingSortCurrentFolderProjection()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));

    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("asc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("desc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});
}

void LibrarySortTest::supportedSortDialogFieldsSortCurrentProjection()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));

    controller.applySortRules(sortRules({{QStringLiteral("artist"), QStringLiteral("asc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});

    controller.applySortRules(sortRules({{QStringLiteral("album"), QStringLiteral("desc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c"), QStringLiteral("track-folder-a")});

    controller.applySortRules(sortRules({{QStringLiteral("filename"), QStringLiteral("desc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});

    controller.applySortRules(sortRules({{QStringLiteral("year"), QStringLiteral("asc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b")});

    controller.applySortRules(sortRules({{QStringLiteral("duration"), QStringLiteral("asc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    controller.applySortRules(sortRules({{QStringLiteral("createdDate"), QStringLiteral("desc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});

    controller.applySortRules(sortRules({{QStringLiteral("discNumber"), QStringLiteral("desc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")});

    controller.applySortRules(sortRules({{QStringLiteral("trackNumber"), QStringLiteral("asc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});
}

void LibrarySortTest::currentRulesReapplyAcrossEnterFolderSearchSubmitAndClear()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());

    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("asc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-root-a"), QStringLiteral("folder-jazz"), QStringLiteral("track-root-z")});

    controller.enterFolder(QStringLiteral("folder-jazz"));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    controller.setSearchQuery(QStringLiteral("Tune"));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    controller.submitSearch();
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-folder-a"));
    QCOMPARE(controller.scrollRequest(), QStringLiteral("track-folder-a"));

    controller.clearSearch();
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});
}

void LibrarySortTest::snapshotUpdateReappliesRulesWithoutMutatingScannerOrder()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));
    controller.applySortRules(sortRules({{QStringLiteral("duration"), QStringLiteral("desc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});
    QCOMPARE(controller.model()->childNodeIds(QStringLiteral("folder-jazz")), QVector<QString>({QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")}));

    controller.setPlaylistTreeSnapshot(makeUpdatedSortableSnapshot());

    QCOMPARE(controller.model()->version(), 22ULL);
    expectProjection(controller.model(), {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});
    QCOMPARE(controller.model()->childNodeIds(QStringLiteral("folder-jazz")), QVector<QString>({QStringLiteral("track-folder-c"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b")}));
}

void LibrarySortTest::snapshotReconcileKeepsRootSearchSortAndPlaybackMarkers()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("desc")}}));
    controller.setSearchQuery(QStringLiteral("Root"));
    controller.setPlayingTrackId(QStringLiteral("track-root-z-id"));

    expectProjection(controller.model(), {QStringLiteral("track-root-z"), QStringLiteral("track-root-a")});
    QVERIFY(nodeIsPlaying(controller.model(), QStringLiteral("track-root-z")));

    controller.setPlaylistTreeSnapshot(makeUpdatedSortableSnapshot());

    QCOMPARE(controller.model()->version(), 22ULL);
    expectProjection(controller.model(), {QStringLiteral("track-root-z"), QStringLiteral("track-root-a")});
    QVERIFY(nodeIsPlaying(controller.model(), QStringLiteral("track-root-z")));

    controller.clearSearch();
    expectProjection(controller.model(), {QStringLiteral("track-root-z"), QStringLiteral("folder-jazz"), QStringLiteral("track-root-a")});

    controller.applyPlayerStateSnapshot(playerSnapshotForTrack("track-root-a-id"), true);
    QCOMPARE(controller.playingTrackId(), QStringLiteral("track-root-a-id"));
    QVERIFY(!nodeIsPlaying(controller.model(), QStringLiteral("track-root-z")));
    QVERIFY(nodeIsPlaying(controller.model(), QStringLiteral("track-root-a")));

    controller.applyPlayerStateSnapshot(PlayerStateSnapshot{}, true);
    QCOMPARE(controller.playingTrackId(), QString());
    QVERIFY(!nodeIsPlaying(controller.model(), QStringLiteral("track-root-a")));
}

void LibrarySortTest::snapshotReconcileFallsBackToVisibleRowsWhenFolderDisappears()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));
    controller.setSelectedBrowserNodeId(QStringLiteral("track-folder-b"));
    controller.setPlayingTrackId(QStringLiteral("track-folder-b-id"));

    controller.setPlaylistTreeSnapshot(makeFolderRemovedSnapshot());

    QCOMPARE(controller.model()->version(), 23ULL);
    QCOMPARE(controller.currentFolderName(), QStringLiteral("My Music"));
    QCOMPARE(controller.canGoBack(), false);
    expectProjection(controller.model(), {QStringLiteral("track-root-a"), QStringLiteral("track-root-z")});
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("track-root-a"));
    QCOMPARE(controller.selectedBrowserNodeId(), QStringLiteral("track-root-a"));
    QCOMPARE(controller.rowForNodeId(controller.focusedNodeId()), 0);
    QCOMPARE(controller.rowForNodeId(controller.selectedBrowserNodeId()), 0);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("folder-jazz")), -1);
    QCOMPARE(controller.rowForNodeId(QStringLiteral("track-folder-b")), -1);
    QVERIFY(!nodeIsPlaying(controller.model(), QStringLiteral("track-root-a")));

    controller.applyPlayerStateSnapshot(PlayerStateSnapshot{}, true);

    QCOMPARE(controller.playingTrackId(), QString());
}

void LibrarySortTest::invalidSortPayloadLeavesProjectionUnchanged()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));
    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("asc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    controller.applySortRules(sortRules({{QStringLiteral("unknownField"), QStringLiteral("desc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    controller.applySortRules(sortRules({{QStringLiteral("artist"), QStringLiteral("sideways")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});
}

void LibrarySortTest::folderSortSendsApplyCommandWithRootAndFolderKey()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    LibraryController controller;
    const QString rootPath = scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));

    CommandRecorder recorder;
    installCommandRecorder(controller, recorder);

    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("desc")}}));

    expectFolderSortCommand(recorder,
                            rootPath,
                            QStringLiteral("folder-jazz"),
                            FolderSortField::Title,
                            FolderSortDirection::Descending);
    expectProjection(controller.model(), {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});
    QCOMPARE(controller.lastError(), QString());
}

void LibrarySortTest::sameFolderNodeIdUnderDifferentRootsDoesNotReuseSavedRules()
{
    QTemporaryDir firstRoot;
    QTemporaryDir secondRoot;
    QVERIFY(firstRoot.isValid());
    QVERIFY(secondRoot.isValid());

    LibraryController controller;
    scanTemporaryRoot(controller, firstRoot);
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));

    CommandRecorder recorder;
    installCommandRecorder(controller, recorder);
    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("asc")}}));
    QCOMPARE(recorder.commands.size(), std::size_t{1});
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    scanTemporaryRoot(controller, secondRoot);
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot(31));
    controller.enterFolder(QStringLiteral("folder-jazz"));

    QCOMPARE(recorder.commands.size(), std::size_t{1});
    expectProjection(controller.model(), {QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")});
}

void LibrarySortTest::reenterFolderAndBackendStateReloadRestoreSavedRules()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    LibraryController controller;
    const QString rootPath = scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));

    CommandRecorder recorder;
    installCommandRecorder(controller, recorder);
    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("asc")}}));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    controller.goBack();
    controller.enterFolder(QStringLiteral("folder-jazz"));
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    LibraryController reloaded;
    scanTemporaryRoot(reloaded, musicDir);
    reloaded.setPlaylistTreeSnapshot(makeSortableSnapshot());

    FolderSortSetting saved;
    saved.rootPath = std::filesystem::path(rootPath.toStdString());
    saved.folderNodeId = "folder-jazz";
    saved.rules = {FolderSortRule{FolderSortField::Duration, FolderSortDirection::Descending, FolderSortMissingValuePolicy::Last}};
    reloaded.applyFolderSortSetting(saved);
    reloaded.enterFolder(QStringLiteral("folder-jazz"));

    expectProjection(reloaded.model(), {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});
}

void LibrarySortTest::backendFolderSortNotificationUpdatesActiveCurrentRules()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    LibraryController controller;
    const QString rootPath = scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));
    QCOMPARE(controller.currentSortRules().size(), 0);

    QSignalSpy sortRulesChanged(&controller, &LibraryController::currentSortRulesChanged);
    FolderSortSetting saved;
    saved.rootPath = std::filesystem::path(rootPath.toStdString());
    saved.folderNodeId = "folder-jazz";
    saved.rules = {FolderSortRule{FolderSortField::Duration, FolderSortDirection::Descending, FolderSortMissingValuePolicy::Last}};

    controller.applyFolderSortSetting(saved);

    QCOMPARE(sortRulesChanged.count(), 1);
    const QVariantList currentRules = controller.currentSortRules();
    QCOMPARE(currentRules.size(), 1);
    QCOMPARE(currentRules.first().toMap().value(QStringLiteral("field")).toString(), QStringLiteral("duration"));
    QCOMPARE(currentRules.first().toMap().value(QStringLiteral("order")).toString(), QStringLiteral("desc"));
    expectProjection(controller.model(), {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});
}

void LibrarySortTest::currentSortRulesExposeFolderRulesForQmlAndFallbacks()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    LibraryController controller;
    scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));

    CommandRecorder recorder;
    installCommandRecorder(controller, recorder);
    controller.applySortRules(sortRules({{QStringLiteral("duration"), QStringLiteral("desc")}}));

    QVariantList currentRules = controller.currentSortRules();
    QCOMPARE(currentRules.size(), 1);
    const QVariantMap currentRule = currentRules.first().toMap();
    QCOMPARE(currentRule.value(QStringLiteral("field")).toString(), QStringLiteral("duration"));
    QCOMPARE(currentRule.value(QStringLiteral("order")).toString(), QStringLiteral("desc"));

    controller.applySortRules(sortRules({{QStringLiteral("notAField"), QStringLiteral("asc")}}));
    currentRules = controller.currentSortRules();
    QCOMPARE(currentRules.size(), 1);
    QCOMPARE(currentRules.first().toMap().value(QStringLiteral("field")).toString(), QStringLiteral("duration"));
    QCOMPARE(currentRules.first().toMap().value(QStringLiteral("order")).toString(), QStringLiteral("desc"));

    controller.setPlaylistTreeSnapshot(makeFolderRemovedSnapshot());

    QCOMPARE(controller.currentSortRules().size(), 0);
    expectProjection(controller.model(), {QStringLiteral("track-root-a"), QStringLiteral("track-root-z")});
}

void LibrarySortTest::searchProjectionSortDoesNotPersistOrOverwriteSavedFolderRules()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    LibraryController controller;
    scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());
    controller.enterFolder(QStringLiteral("folder-jazz"));

    CommandRecorder recorder;
    installCommandRecorder(controller, recorder);
    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("asc")}}));
    QCOMPARE(recorder.commands.size(), std::size_t{1});

    controller.setSearchQuery(QStringLiteral("Tune"));
    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("desc")}}));

    QCOMPARE(recorder.commands.size(), std::size_t{1});
    expectProjection(controller.model(), {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});

    controller.clearSearch();
    controller.goBack();
    controller.enterFolder(QStringLiteral("folder-jazz"));

    QCOMPARE(recorder.commands.size(), std::size_t{1});
    expectProjection(controller.model(), {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});
}

void LibrarySortTest::missingContextAndMalformedSortPayloadDoNotPersist()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());

    LibraryController rootOnly;
    scanTemporaryRoot(rootOnly, musicDir);
    rootOnly.setPlaylistTreeSnapshot(makeSortableSnapshot());
    CommandRecorder rootRecorder;
    installCommandRecorder(rootOnly, rootRecorder);

    rootOnly.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("asc")}}));

    QCOMPARE(rootRecorder.commands.size(), std::size_t{0});
    QVERIFY(rootOnly.lastError().contains(QStringLiteral("文件夹")));

    LibraryController folderWithoutRoot;
    folderWithoutRoot.setPlaylistTreeSnapshot(makeSortableSnapshot());
    folderWithoutRoot.enterFolder(QStringLiteral("folder-jazz"));
    CommandRecorder noRootRecorder;
    installCommandRecorder(folderWithoutRoot, noRootRecorder);

    folderWithoutRoot.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("asc")}}));

    QCOMPARE(noRootRecorder.commands.size(), std::size_t{0});
    QVERIFY(folderWithoutRoot.lastError().contains(QStringLiteral("曲库")));

    LibraryController malformed;
    scanTemporaryRoot(malformed, musicDir);
    malformed.setPlaylistTreeSnapshot(makeSortableSnapshot());
    malformed.enterFolder(QStringLiteral("folder-jazz"));
    CommandRecorder malformedRecorder;
    installCommandRecorder(malformed, malformedRecorder);
    expectProjection(malformed.model(), {QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")});

    malformed.applySortRules(sortRules({{QStringLiteral("unknownField"), QStringLiteral("desc")}}));
    malformed.applySortRules(sortRules({{QStringLiteral("artist"), QStringLiteral("sideways")}}));

    QCOMPARE(malformedRecorder.commands.size(), std::size_t{0});
    expectProjection(malformed.model(), {QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")});
}

QTEST_GUILESS_MAIN(LibrarySortTest)

#include "tst_library_sort.moc"
