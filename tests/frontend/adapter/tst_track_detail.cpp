// T14【前端】播放列表右键菜单 + 详情窗口 —— 详情数据组装 / 删除确认链测试。
// 覆盖（CTest NAME: seriona_frontend_track_detail）：
//   1. trackDetailDataAssembledFromEntryAndStats：详情窗口数据收集（库条目字段 +
//      TrackStatsController 播放次数/星级）；星级点击 = setRating → 读回变化；无记录 = 未评级。
//   2. deleteChainConfirmedOnceSubmitsSingleCommand：每次确认后的 deleteTarget 调用
//      恰好派发一条删除命令（调用计数不合并、不吞掉）。
//   3. filePathForNodeIdReturnsAbsolutePaths（T14 修复 A）：删除链路径契约——歌曲返回
//      含目录的绝对路径、文件夹返回完整目录路径（非空）、未知节点返回空。
//   4. cueContainerReturnsNoDeletionPath（T14 R1 修复锁）：cue 容器（显示名 = .cue 文件
//      名，无文件系统实体目录）删除目标必须返回空，杜绝删除链误删 .cue 所在真实目录。
#include "app_facade.h"
#include "library_model.h"
#include "track_stats_controller.h"

#include <QCoreApplication>
#include <QObject>
#include <QVariant>
#include <QVariantMap>
#include <QtTest/QTest>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#if SERIONA_HAS_BACKEND
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

PlaylistNode makeTrack(const std::string &nodeId,
                       const std::string &trackId,
                       const std::string &parentNodeId,
                       const std::string &filePath = "/music/song-c.mp3")
{
    SongMetadata song;
    song.trackId = trackId;
    song.title = "Song C";
    song.artist = "Artist C";
    song.album = "Album C";
    song.filePath = std::filesystem::path(filePath);
    song.year = 2018;
    song.duration = std::chrono::milliseconds{120000};
    song.sampleRate = 96000;
    song.bitDepth = 24;

    PlaylistNode node;
    node.nodeId = nodeId;
    node.parentNodeId = parentNodeId;
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
        makeTrack("track-c", "track-c-id", "root"),
    };
    return snapshot;
}

// 三层目录链：root → folder-a(FolderA) → sub-b(SubB) → track-x（绝对路径与显示名一致），
// 用于验证 filePathForNodeId 对文件夹返回完整目录路径。
PlaylistTreeSnapshot makeFilePathSnapshot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 141;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"folder-a"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("folder-a", "FolderA", {"sub-b"}, "root"),
        makeFolder("sub-b", "SubB", {"track-x"}, "folder-a"),
        makeTrack("track-x", "track-x-id", "sub-b", "/music/FolderA/SubB/song-c.mp3"),
    };
    return snapshot;
}

// cue 曲目（R1 修复锁）：song.sourceFilePath 指向真实音频文件，filePath 是 .cue 文件
// 本身——effectivePath 优先 sourceFilePath，父目录显示名（"album"）与 cue 容器显示名
// （"album.cue"）不一致，是 absoluteFilePathForNode 必须拒绝的形态。
PlaylistNode makeCueTrack(const std::string &nodeId,
                          const std::string &trackId,
                          const std::string &parentNodeId)
{
    SongMetadata song;
    song.trackId = trackId;
    song.title = "Track 1";
    song.artist = "Artist C";
    song.album = "Album C";
    song.filePath = std::filesystem::path("/music/Album/album.cue");
    song.sourceFilePath = std::filesystem::path("/music/Album/album.flac");
    song.duration = std::chrono::milliseconds{180000};

    PlaylistNode node;
    node.nodeId = nodeId;
    node.parentNodeId = parentNodeId;
    node.kind = PlaylistNodeKind::Track;
    node.displayName = "Track 1";
    node.song = std::move(song);
    return node;
}

// cue 容器：folder 显示名 = .cue 文件名（无文件系统实体目录），cue 曲目是其直接子级，
// 真实目录 /music/Album 只存在于曲目 sourceFilePath 的父路径中。
PlaylistTreeSnapshot makeCueContainerSnapshot()
{
    PlaylistTreeSnapshot snapshot;
    snapshot.version = 142;
    snapshot.rootNodeId = std::string{"root"};
    snapshot.nodes = {
        makeFolder("root", "Library", {"cue-album"}, std::nullopt, PlaylistNodeKind::Root),
        makeFolder("cue-album", "album.cue", {"cue-track-1"}, "root", PlaylistNodeKind::Album),
        makeCueTrack("cue-track-1", "cue-track-1-id", "cue-album"),
    };
    return snapshot;
}

}
#endif

class TrackDetailTest : public QObject
{
    Q_OBJECT

private slots:
    void trackDetailDataAssembledFromEntryAndStats();
    void deleteChainConfirmedOnceSubmitsSingleCommand();
    void filePathForNodeIdReturnsAbsolutePaths();
    void cueContainerReturnsNoDeletionPath();
};

// 详情窗口数据收集：库条目字段（LibraryModel Entry）+ 播放次数/星级（TrackStatsController）。
// 星级点击 = setRating；无记录 = 0 次播放 / 未评级（0）。
void TrackDetailTest::trackDetailDataAssembledFromEntryAndStats()
{
#if SERIONA_HAS_BACKEND
    Seriona::App::LibraryModel model;
    model.setPlaylistTreeSnapshot(makeSnapshot());

    const Seriona::App::LibraryModel::Entry *entry = model.entryByNodeId(QStringLiteral("track-c"));
    QVERIFY(entry != nullptr);
    QCOMPARE(entry->title, QStringLiteral("Song C"));
    QCOMPARE(entry->artist, QStringLiteral("Artist C"));
    QCOMPARE(entry->album, QStringLiteral("Album C"));
    QCOMPARE(entry->duration, QStringLiteral("2:00"));
    QCOMPARE(entry->format, QStringLiteral("MP3"));
    QCOMPARE(entry->sampleRate, 96000);
    QCOMPARE(entry->bitDepth, 24);
    QCOMPARE(entry->fileName, QStringLiteral("song-c.mp3"));
    QCOMPARE(entry->trackId, QStringLiteral("track-c-id"));
    QVERIFY(entry->year.has_value());
    QCOMPARE(*entry->year, std::uint32_t{2018});
    QCOMPARE(model.roleNames().value(Seriona::App::LibraryModel::YearRole), QByteArrayLiteral("year"));
    const int trackRow = model.rowForNodeId(QStringLiteral("track-c"));
    QVERIFY(trackRow >= 0);
    QCOMPARE(model.data(model.index(trackRow, 0), Seriona::App::LibraryModel::YearRole).toLongLong(),
             qint64{2018});

    Seriona::App::TrackStatsController stats;
    QCOMPARE(stats.playCountFor(entry->trackId), 0);
    QCOMPARE(stats.ratingFor(entry->trackId), 0);

    stats.recordPlayback(entry->trackId);
    stats.recordPlayback(entry->trackId);
    stats.setRating(entry->trackId, 4);

    QVariantMap detail;
    detail.insert(QStringLiteral("title"), entry->title);
    detail.insert(QStringLiteral("artist"), entry->artist);
    detail.insert(QStringLiteral("album"), entry->album);
    detail.insert(QStringLiteral("duration"), entry->duration);
    detail.insert(QStringLiteral("format"), entry->format);
    detail.insert(QStringLiteral("sampleRate"), entry->sampleRate);
    detail.insert(QStringLiteral("bitDepth"), entry->bitDepth);
    detail.insert(QStringLiteral("path"), entry->fileName);
    detail.insert(QStringLiteral("playCount"), stats.playCountFor(entry->trackId));
    detail.insert(QStringLiteral("rating"), stats.ratingFor(entry->trackId));

    QCOMPARE(detail.value(QStringLiteral("playCount")).toInt(), 2);
    QCOMPARE(detail.value(QStringLiteral("rating")).toInt(), 4);

    stats.setRating(entry->trackId, 0);
    detail.insert(QStringLiteral("rating"), stats.ratingFor(entry->trackId));
    QCOMPARE(detail.value(QStringLiteral("rating")).toInt(), 0);
#else
    QSKIP("backend disabled");
#endif
}

// 删除确认链：每次确认后的 deleteTarget 调用都恰好派发一条删除命令——
// 两次调用产生两条 CommandRejected 通知（controller 未启动的防御路径），
// 计数不合并、不吞掉。
void TrackDetailTest::deleteChainConfirmedOnceSubmitsSingleCommand()
{
#if SERIONA_HAS_BACKEND
    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", false);

    Seriona::App::AppFacade facade;
    const QString path = QStringLiteral("/music/song-c.mp3");

    QCOMPARE(facade.deleteTarget(path, false), false);
    QCOMPARE(facade.deleteTarget(path, false), false);
    QCOMPARE(facade.backendNotificationCountForTests(), std::size_t{2});

    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", QVariant{});
#else
    QSKIP("backend disabled");
#endif
}

// 删除链路径契约（T14 修复 A 回归锁）：filePathForNodeId 必须返回绝对路径——
// 歌曲 = 含目录的音频文件路径（不再是最底层 basename），文件夹 = 完整目录路径（非空），
// 未知节点 = 空。后端 DeleteTrack/DeleteFolder 按绝对路径删除原文件（app_facade.h 契约）。
void TrackDetailTest::filePathForNodeIdReturnsAbsolutePaths()
{
#if SERIONA_HAS_BACKEND
    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", false);

    Seriona::App::AppFacade facade;
    facade.library()->model()->setPlaylistTreeSnapshot(makeFilePathSnapshot());

    QCOMPARE(facade.filePathForNodeId(QStringLiteral("track-x")),
             QStringLiteral("/music/FolderA/SubB/song-c.mp3"));
    QCOMPARE(facade.filePathForNodeId(QStringLiteral("sub-b")),
             QStringLiteral("/music/FolderA/SubB"));
    QCOMPARE(facade.filePathForNodeId(QStringLiteral("folder-a")),
             QStringLiteral("/music/FolderA"));
    QCOMPARE(facade.filePathForNodeId(QStringLiteral("missing-node")), QString());

    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", QVariant{});
#else
    QSKIP("backend disabled");
#endif
}

// cue 容器删除目标（T14 R1 修复锁）：cue 容器显示名（.cue 文件名）与其曲目真实父目录名
// 不一致，absoluteFilePathForNode 必须返回空——删除链宁可拒绝，也不把 .cue 所在真实
// 目录送进递归删除（混有无关文件时误删）。cue 曲目本身仍是歌曲，返回音频绝对路径。
void TrackDetailTest::cueContainerReturnsNoDeletionPath()
{
#if SERIONA_HAS_BACKEND
    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", false);

    Seriona::App::AppFacade facade;
    facade.library()->model()->setPlaylistTreeSnapshot(makeCueContainerSnapshot());

    QCOMPARE(facade.filePathForNodeId(QStringLiteral("cue-album")), QString());
    QCOMPARE(facade.filePathForNodeId(QStringLiteral("cue-track-1")),
             QStringLiteral("/music/Album/album.flac"));

    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", QVariant{});
#else
    QSKIP("backend disabled");
#endif
}

QTEST_GUILESS_MAIN(TrackDetailTest)

#include "tst_track_detail.moc"
