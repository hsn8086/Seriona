#include "library_folder_projection_model.h"
#include "library_model.h"

#include <QFileInfo>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest/QTest>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace {
using Seriona::App::LibraryController;
using Seriona::App::LibraryFolderProjectionModel;
using Seriona::App::LibraryModel;
using seriona::control::MediaControlCommand;
using seriona::control::MediaControllerCommandResult;
using seriona::scanner::PlaylistNode;
using seriona::scanner::PlaylistNodeKind;
using seriona::scanner::PlaylistTreeSnapshot;
using seriona::scanner::SongMetadata;

MediaControllerCommandResult acceptedResult()
{
    MediaControllerCommandResult result;
    result.accepted = true;
    return result;
}

struct CommandRecorder {
    std::vector<MediaControlCommand> commands;

    MediaControllerCommandResult record(const MediaControlCommand &command)
    {
        commands.push_back(command);
        return acceptedResult();
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
                       const std::string &displayName,
                       const std::string &title,
                       const std::string &artist,
                       const std::string &album,
                       std::chrono::milliseconds duration,
                       std::optional<std::string> parentNodeId = std::nullopt)
{
    SongMetadata song;
    song.trackId = trackId;
    song.filePath = "/music/" + displayName;
    song.sourceFilePath = song.filePath;
    song.title = title;
    song.artist = artist;
    song.album = album;
    song.sampleRate = 96000;
    song.bitDepth = 24;
    song.duration = duration;

    PlaylistNode node;
    node.nodeId = nodeId;
    node.parentNodeId = std::move(parentNodeId);
    node.kind = PlaylistNodeKind::Track;
    node.displayName = displayName;
    node.song = std::move(song);
    return node;
}

PlaylistTreeSnapshot makeProjectedTreeSnapshot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 7;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"album-a", "track-c"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("album-a", "Album A", {"track-a", "track-b"}, std::string{"root"}, PlaylistNodeKind::Album),
        makeTrack("track-a", "track-a-id", "01-song-a.flac", "Song A", "Artist A", "Album A", std::chrono::milliseconds{185000}, std::string{"album-a"}),
        makeTrack("track-b", "track-b-id", "02-song-b.flac", "Song B", "Artist B", "Album A", std::chrono::milliseconds{200000}, std::string{"album-a"}),
        makeTrack("track-c", "track-c-id", "03-song-c.flac", "Song C", "Artist C", "Singles", std::chrono::milliseconds{150000}, std::string{"root"}),
    };
    return snapshot;
}

// album-a 的子女被替换为 track-x / track-y 的快照（树变化重建测试用）。
PlaylistTreeSnapshot makeProjectedTreeSnapshotV2()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 8;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"album-a", "track-c"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("album-a", "Album A", {"track-x", "track-y"}, std::string{"root"}, PlaylistNodeKind::Album),
        makeTrack("track-x", "track-x-id", "01-song-x.flac", "Song X", "Artist X", "Album A", std::chrono::milliseconds{185000}, std::string{"album-a"}),
        makeTrack("track-y", "track-y-id", "02-song-y.flac", "Song Y", "Artist Y", "Album A", std::chrono::milliseconds{200000}, std::string{"album-a"}),
        makeTrack("track-c", "track-c-id", "03-song-c.flac", "Song C", "Artist C", "Singles", std::chrono::milliseconds{150000}, std::string{"root"}),
    };
    return snapshot;
}

// 可排序快照：folder-jazz 内 track-folder-b / track-folder-a / track-folder-c。
PlaylistTreeSnapshot makeSortableSnapshot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 21;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"folder-jazz", "track-root"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("folder-jazz", "Jazz", {"track-folder-b", "track-folder-a", "track-folder-c"}, std::string{"root"}, PlaylistNodeKind::Album),
        makeTrack("track-folder-b", "track-folder-b-id", "02-beta.flac", "Beta Tune", "Charlie", "Album Z", std::chrono::milliseconds{180000}, std::string{"folder-jazz"}),
        makeTrack("track-folder-a", "track-folder-a-id", "01-alpha.flac", "Alpha Tune", "Delta", "Album A", std::chrono::milliseconds{60000}, std::string{"folder-jazz"}),
        makeTrack("track-folder-c", "track-folder-c-id", "03-gamma.flac", "Gamma Tune", "Bravo", "Album M", std::chrono::milliseconds{240000}, std::string{"folder-jazz"}),
        makeTrack("track-root", "track-root-id", "00-root.flac", "Root Tune", "Root Artist", "Root Album", std::chrono::milliseconds{120000}, std::string{"root"}),
    };
    return snapshot;
}

// 深层树：root → folder-a → folder-b → folder-c（栈生命周期测试用）。
PlaylistTreeSnapshot makeNestedTreeSnapshot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 31;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"folder-a", "track-root"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("folder-a", "Folder A", {"folder-b", "track-a1"}, std::string{"root"}, PlaylistNodeKind::Album),
        makeFolder("folder-b", "Folder B", {"folder-c", "track-b1"}, std::string{"folder-a"}, PlaylistNodeKind::Album),
        makeFolder("folder-c", "Folder C", {"track-c1"}, std::string{"folder-b"}, PlaylistNodeKind::Album),
        makeTrack("track-root", "track-root-id", "root.flac", "Root Tune", "Root Artist", "Root Album", std::chrono::milliseconds{120000}, std::string{"root"}),
        makeTrack("track-a1", "track-a1-id", "a1.flac", "A One", "A Artist", "A Album", std::chrono::milliseconds{130000}, std::string{"folder-a"}),
        makeTrack("track-b1", "track-b1-id", "b1.flac", "B One", "B Artist", "B Album", std::chrono::milliseconds{140000}, std::string{"folder-b"}),
        makeTrack("track-c1", "track-c1-id", "c1.flac", "C One", "C Artist", "C Album", std::chrono::milliseconds{150000}, std::string{"folder-c"}),
    };
    return snapshot;
}

QString nodeIdAt(const LibraryModel *model, int row)
{
    return model->data(model->index(row, 0), LibraryModel::NodeIdRole).toString();
}

QString nodeIdAt(const LibraryFolderProjectionModel *model, int row)
{
    return model->data(model->index(row, 0), LibraryModel::NodeIdRole).toString();
}

QVector<QString> projectionNodeIds(const LibraryFolderProjectionModel *model)
{
    QVector<QString> nodeIds;
    nodeIds.reserve(model->rowCount());
    for (int row = 0; row < model->rowCount(); ++row) {
        nodeIds.append(nodeIdAt(model, row));
    }
    return nodeIds;
}

void expectProjection(const LibraryFolderProjectionModel *model, const QVector<QString> &nodeIds)
{
    QCOMPARE(model->rowCount(), nodeIds.size());
    for (int row = 0; row < nodeIds.size(); ++row) {
        QCOMPARE(nodeIdAt(model, row), nodeIds.at(row));
    }
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
    controller.setScanExecutor([&recorder](const QString &rootPath, seriona::scanner::ScanMode mode) {
        return recorder.record(rootPath, mode);
    });

    const QString canonicalRoot = QFileInfo(musicDir.path()).absoluteFilePath();
    if (!controller.scanLibrary(QUrl::fromLocalFile(musicDir.path()))) {
        qFatal("scanTemporaryRoot expected scanLibrary to accept a temporary root");
    }
    return canonicalRoot;
}

void installCommandRecorder(LibraryController &controller, CommandRecorder &recorder)
{
    controller.setCommandExecutor([&recorder](const MediaControlCommand &command) {
        return recorder.record(command);
    });
}
}

class LibraryFolderProjectionModelTest : public QObject
{
    Q_OBJECT

private slots:
    void projectionContentsPerFolderLevel();
    void projectionSortsPerLevelRules();
    void revisionAdvancesOnRebuild();
    void treeChangeRebuildsProjection();
    void playingAndFocusSyncEmitDataChanged();
    void stackDepthAndProjectionLifecycle();
    void locateNodeInFolderStackNavigatesToTargetLevel();
    void roleNamesMatchLibraryModel();
};

void LibraryFolderProjectionModelTest::projectionContentsPerFolderLevel()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeProjectedTreeSnapshot());

    // 栈初始：只有根投影（level 0），投影 rootProjectionNodeIds。
    QCOMPARE(controller.folderStackDepth(), 0);
    auto *level0 = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(0));
    QVERIFY(level0 != nullptr);
    QVERIFY(controller.projectionModelForLevel(1) == nullptr);
    QVERIFY(controller.projectionModelForLevel(-1) == nullptr);
    QVERIFY(level0->folderNodeId().isEmpty());
    expectProjection(level0, {QStringLiteral("album-a"), QStringLiteral("track-c")});
    QCOMPARE(level0->data(level0->index(0, 0), LibraryModel::NameRole).toString(), QStringLiteral("Album A"));
    QCOMPARE(level0->data(level0->index(0, 0), LibraryModel::ParentNameRole).toString(), QStringLiteral("Library"));
    QCOMPARE(level0->data(level0->index(0, 0), LibraryModel::SongCountRole).toInt(), 2);
    QCOMPARE(level0->data(level0->index(0, 0), LibraryModel::IsFolderRole).toBool(), true);

    // 进入 album-a：level 1 出现其直接子级投影，level 0 不变。
    controller.enterFolder(QStringLiteral("album-a"));
    QCOMPARE(controller.folderStackDepth(), 1);
    auto *level0AfterEnter = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(0));
    auto *level1 = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(1));
    QVERIFY(level0AfterEnter == level0);
    QVERIFY(level1 != nullptr);
    QCOMPARE(level1->folderNodeId(), QStringLiteral("album-a"));
    expectProjection(level1, {QStringLiteral("track-a"), QStringLiteral("track-b")});
    expectProjection(level0, {QStringLiteral("album-a"), QStringLiteral("track-c")});
    QCOMPARE(level1->data(level1->index(1, 0), LibraryModel::TitleRole).toString(), QStringLiteral("Song B"));
    QCOMPARE(level1->data(level1->index(1, 0), LibraryModel::ArtistRole).toString(), QStringLiteral("Artist B"));
    QCOMPARE(level1->data(level1->index(1, 0), LibraryModel::IsFolderRole).toBool(), false);

    // 返回：level 1 释放（越界返回 nullptr），level 0 实例与内容保持。
    controller.goBack();
    QCOMPARE(controller.folderStackDepth(), 0);
    auto *level0AfterBack = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(0));
    QVERIFY(level0AfterBack == level0);
    QVERIFY(controller.projectionModelForLevel(1) == nullptr);
    expectProjection(level0, {QStringLiteral("album-a"), QStringLiteral("track-c")});
}

void LibraryFolderProjectionModelTest::projectionSortsPerLevelRules()
{
    QTemporaryDir musicDir;
    LibraryController controller;
    CommandRecorder commandRecorder;
    installCommandRecorder(controller, commandRecorder);
    scanTemporaryRoot(controller, musicDir);
    controller.setPlaylistTreeSnapshot(makeSortableSnapshot());

    controller.enterFolder(QStringLiteral("folder-jazz"));
    auto *level1 = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(1));
    QVERIFY(level1 != nullptr);
    expectProjection(level1, {QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a"), QStringLiteral("track-folder-c")});

    // 排序规则应用到当前文件夹投影（与主模型投影同语义）。
    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("asc")}}));
    expectProjection(level1, {QStringLiteral("track-folder-a"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-c")});

    controller.applySortRules(sortRules({{QStringLiteral("title"), QStringLiteral("desc")}}));
    expectProjection(level1, {QStringLiteral("track-folder-c"), QStringLiteral("track-folder-b"), QStringLiteral("track-folder-a")});

    // 父级投影不受子级排序影响。
    auto *level0 = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(0));
    expectProjection(level0, {QStringLiteral("folder-jazz"), QStringLiteral("track-root")});
}

void LibraryFolderProjectionModelTest::revisionAdvancesOnRebuild()
{
    LibraryModel source;
    source.setPlaylistTreeSnapshot(makeProjectedTreeSnapshot());

    LibraryFolderProjectionModel projection;
    projection.setSource(&source, QStringLiteral("album-a"), {});
    QCOMPARE(projection.projectionRevision(), 1);
    QSignalSpy revisionSpy(&projection, &LibraryFolderProjectionModel::projectionRevisionChanged);

    // 树变化 → 全量重建 → revision 递增。
    source.setPlaylistTreeSnapshot(makeProjectedTreeSnapshotV2());
    QCOMPARE(projection.projectionRevision(), 2);
    QCOMPARE(revisionSpy.count(), 1);
    expectProjection(&projection, {QStringLiteral("track-x"), QStringLiteral("track-y")});

    // 重复 setSource 也视为一次重建。
    projection.setSource(&source, QStringLiteral("album-a"), {});
    QCOMPARE(projection.projectionRevision(), 3);
    QCOMPARE(revisionSpy.count(), 2);
}

void LibraryFolderProjectionModelTest::treeChangeRebuildsProjection()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeProjectedTreeSnapshot());
    controller.enterFolder(QStringLiteral("album-a"));

    auto *level0 = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(0));
    auto *level1 = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(1));
    QVERIFY(level0 != nullptr);
    QVERIFY(level1 != nullptr);
    expectProjection(level1, {QStringLiteral("track-a"), QStringLiteral("track-b")});

    // 树重建：所有已建投影全量重建（实例保持，内容更新）。
    controller.setPlaylistTreeSnapshot(makeProjectedTreeSnapshotV2());
    expectProjection(level1, {QStringLiteral("track-x"), QStringLiteral("track-y")});
    expectProjection(level0, {QStringLiteral("album-a"), QStringLiteral("track-c")});
    QCOMPARE(controller.folderStackDepth(), 1);

    // 对账后当前文件夹仍为 album-a，返回行为一致。
    controller.goBack();
    QCOMPARE(controller.folderStackDepth(), 0);
    expectProjection(level0, {QStringLiteral("album-a"), QStringLiteral("track-c")});
}

void LibraryFolderProjectionModelTest::playingAndFocusSyncEmitDataChanged()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeProjectedTreeSnapshot());
    controller.enterFolder(QStringLiteral("album-a"));

    auto *level0 = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(0));
    auto *level1 = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(1));
    QVERIFY(level0 != nullptr);
    QVERIFY(level1 != nullptr);
    QSignalSpy level1Spy(level1, &QAbstractItemModel::dataChanged);
    QSignalSpy level0Spy(level0, &QAbstractItemModel::dataChanged);

    // 播放身份变化：仅投影内存在的行发 dataChanged（IsPlaying role）。
    controller.setPlayingTrackId(QStringLiteral("track-a-id"));
    QCOMPARE(level1Spy.count(), 1);
    const QList<QVariant> playingArguments = level1Spy.takeFirst();
    QCOMPARE(playingArguments.at(0).value<QModelIndex>().row(), 0);
    QCOMPARE(playingArguments.at(1).value<QModelIndex>().row(), 0);
    QCOMPARE(playingArguments.at(2).value<QList<int>>(), QList<int>{LibraryModel::IsPlayingRole});
    QCOMPARE(level1->data(level1->index(0, 0), LibraryModel::IsPlayingRole).toBool(), true);
    QCOMPARE(level1->data(level1->index(1, 0), LibraryModel::IsPlayingRole).toBool(), false);
    QCOMPARE(level0Spy.count(), 0);

    // 播放切到 level 1 外的曲目：level 1 不再发，level 0 对自身行发。
    controller.setPlayingTrackId(QStringLiteral("track-c-id"));
    QCOMPARE(level1Spy.count(), 1);
    const QList<QVariant> clearedArguments = level1Spy.takeFirst();
    QCOMPARE(clearedArguments.at(0).value<QModelIndex>().row(), 0);
    QCOMPARE(clearedArguments.at(2).value<QList<int>>(), QList<int>{LibraryModel::IsPlayingRole});
    QCOMPARE(level1->data(level1->index(0, 0), LibraryModel::IsPlayingRole).toBool(), false);
    QCOMPARE(level0Spy.count(), 1);
    const QList<QVariant> level0Arguments = level0Spy.takeFirst();
    QCOMPARE(level0Arguments.at(0).value<QModelIndex>().row(), 1);
    QCOMPARE(level0->data(level0->index(1, 0), LibraryModel::IsPlayingRole).toBool(), true);
    QCOMPARE(level0->data(level0->index(0, 0), LibraryModel::IsPlayingRole).toBool(), false);

    // 焦点身份变化：仅投影内存在的行发 dataChanged（IsFocused role）。
    controller.setFocusedNodeId(QStringLiteral("track-b"));
    QCOMPARE(level1Spy.count(), 1);
    const QList<QVariant> focusedArguments = level1Spy.takeFirst();
    QCOMPARE(focusedArguments.at(0).value<QModelIndex>().row(), 1);
    QCOMPARE(focusedArguments.at(2).value<QList<int>>(), QList<int>{LibraryModel::IsFocusedRole});
    QCOMPARE(level1->data(level1->index(1, 0), LibraryModel::IsFocusedRole).toBool(), true);
    // level 0 同步清除其投影内 album-a 的焦点标记（仅投影内存在的行）。
    QCOMPARE(level0Spy.count(), 1);
    const QList<QVariant> level0FocusCleared = level0Spy.takeFirst();
    QCOMPARE(level0FocusCleared.at(0).value<QModelIndex>().row(), 0);
    QCOMPARE(level0FocusCleared.at(2).value<QList<int>>(), QList<int>{LibraryModel::IsFocusedRole});
    QCOMPARE(level0->data(level0->index(0, 0), LibraryModel::IsFocusedRole).toBool(), false);

    // 焦点移到 level 1 外的节点：level 1 清标记，level 0 对自身行发。
    controller.setFocusedNodeId(QStringLiteral("album-a"));
    QCOMPARE(level1Spy.count(), 1);
    const QList<QVariant> focusClearedArguments = level1Spy.takeFirst();
    QCOMPARE(focusClearedArguments.at(0).value<QModelIndex>().row(), 1);
    QCOMPARE(level1->data(level1->index(1, 0), LibraryModel::IsFocusedRole).toBool(), false);
    QCOMPARE(level0Spy.count(), 1);
    QCOMPARE(level0->data(level0->index(0, 0), LibraryModel::IsFocusedRole).toBool(), true);
}

void LibraryFolderProjectionModelTest::stackDepthAndProjectionLifecycle()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeNestedTreeSnapshot());

    QObject *rootProjection = controller.projectionModelForLevel(0);
    QVERIFY(rootProjection != nullptr);
    QCOMPARE(controller.folderStackDepth(), 0);

    controller.enterFolder(QStringLiteral("folder-a"));
    QCOMPARE(controller.folderStackDepth(), 1);
    QVERIFY(controller.projectionModelForLevel(0) == rootProjection);
    auto *level1First = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(1));
    QVERIFY(level1First != nullptr);
    expectProjection(level1First, {QStringLiteral("folder-b"), QStringLiteral("track-a1")});

    controller.enterFolder(QStringLiteral("folder-b"));
    QCOMPARE(controller.folderStackDepth(), 2);
    QVERIFY(controller.projectionModelForLevel(0) == rootProjection);
    QVERIFY(controller.projectionModelForLevel(1) == level1First);
    auto *level2 = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(2));
    QVERIFY(level2 != nullptr);
    expectProjection(level2, {QStringLiteral("folder-c"), QStringLiteral("track-b1")});

    // 返回：栈顶级释放（越界返回 nullptr），前缀级实例保留（视图滚动位置保留的前提）。
    controller.goBack();
    QCOMPARE(controller.folderStackDepth(), 1);
    QVERIFY(controller.projectionModelForLevel(0) == rootProjection);
    QVERIFY(controller.projectionModelForLevel(1) == level1First);
    QVERIFY(controller.projectionModelForLevel(2) == nullptr);

    // 再次进入同级目录：新实例（旧实例已释放）。
    controller.enterFolder(QStringLiteral("folder-b"));
    QCOMPARE(controller.folderStackDepth(), 2);
    QVERIFY(controller.projectionModelForLevel(0) == rootProjection);
    QVERIFY(controller.projectionModelForLevel(1) == level1First);
    auto *level2Reentered = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(2));
    QVERIFY(level2Reentered != nullptr);
    QVERIFY(level2Reentered != level2);
    expectProjection(level2Reentered, {QStringLiteral("folder-c"), QStringLiteral("track-b1")});

    controller.goBack();
    controller.goBack();
    QCOMPARE(controller.folderStackDepth(), 0);
    QVERIFY(controller.projectionModelForLevel(0) == rootProjection);
    expectProjection(qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(0)),
                     {QStringLiteral("folder-a"), QStringLiteral("track-root")});
}

void LibraryFolderProjectionModelTest::locateNodeInFolderStackNavigatesToTargetLevel()
{
    LibraryController controller;
    controller.setPlaylistTreeSnapshot(makeNestedTreeSnapshot());

    // 深层曲目：从根逐级进入直到其直接父级，目标进入顶层投影（中间级逐级入栈）。
    controller.locateNodeInFolderStack(QStringLiteral("track-b1"));
    QCOMPARE(controller.folderStackDepth(), 2);
    auto *midLevel = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(1));
    QVERIFY(midLevel != nullptr);
    QCOMPARE(midLevel->folderNodeId(), QStringLiteral("folder-a"));
    expectProjection(midLevel, {QStringLiteral("folder-b"), QStringLiteral("track-a1")});
    auto *top = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(2));
    QVERIFY(top != nullptr);
    QCOMPARE(top->folderNodeId(), QStringLiteral("folder-b"));
    expectProjection(top, {QStringLiteral("folder-c"), QStringLiteral("track-b1")});

    // 目标已在当前投影：不导航。
    const int depthBefore = controller.folderStackDepth();
    controller.locateNodeInFolderStack(QStringLiteral("folder-c"));
    QCOMPARE(controller.folderStackDepth(), depthBefore);
    QCOMPARE(controller.projectionModelForLevel(2), top);

    // 根直属目标（父为根节点）：回到根浏览，目标在根投影。
    controller.locateNodeInFolderStack(QStringLiteral("track-root"));
    QCOMPARE(controller.folderStackDepth(), 0);
    auto *rootProjection = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(0));
    QVERIFY(rootProjection != nullptr);
    expectProjection(rootProjection, {QStringLiteral("folder-a"), QStringLiteral("track-root")});

    // 文件夹目标：进入其直接父级（目标显示在父级投影中）。
    controller.locateNodeInFolderStack(QStringLiteral("folder-b"));
    QCOMPARE(controller.folderStackDepth(), 1);
    auto *folderTop = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(1));
    QVERIFY(folderTop != nullptr);
    QCOMPARE(folderTop->folderNodeId(), QStringLiteral("folder-a"));
    expectProjection(folderTop, {QStringLiteral("folder-b"), QStringLiteral("track-a1")});

    // 从深层回跳到浅层目标：栈收缩到目标级。
    controller.locateNodeInFolderStack(QStringLiteral("track-c1"));
    QCOMPARE(controller.folderStackDepth(), 3);
    controller.locateNodeInFolderStack(QStringLiteral("track-a1"));
    QCOMPARE(controller.folderStackDepth(), 1);
    auto *shallowTop = qobject_cast<LibraryFolderProjectionModel *>(controller.projectionModelForLevel(1));
    QVERIFY(shallowTop != nullptr);
    QCOMPARE(shallowTop->folderNodeId(), QStringLiteral("folder-a"));

    // 未知节点：不导航。
    const int depthBeforeUnknown = controller.folderStackDepth();
    controller.locateNodeInFolderStack(QStringLiteral("missing-node"));
    QCOMPARE(controller.folderStackDepth(), depthBeforeUnknown);
}

void LibraryFolderProjectionModelTest::roleNamesMatchLibraryModel()
{
    const LibraryModel libraryModel;
    const LibraryFolderProjectionModel projectionModel;

    const QHash<int, QByteArray> expected = libraryModel.roleNames();
    const QHash<int, QByteArray> actual = projectionModel.roleNames();
    QCOMPARE(actual, expected);
    QCOMPARE(actual.size(), expected.size());
}

QTEST_GUILESS_MAIN(LibraryFolderProjectionModelTest)

#include "tst_library_folder_projection_model.moc"
