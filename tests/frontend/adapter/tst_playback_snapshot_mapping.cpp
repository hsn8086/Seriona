#include "app_facade.h"

#include "seriona/control/control_contracts.h"

#include <QObject>
#include <QString>
#include <QtTest/QTest>

#include <chrono>
#include <optional>

namespace {

seriona::control::PlayerStateSnapshot makeFilledSnapshot(seriona::control::PlaybackStatus status)
{
    seriona::control::PlayerStateSnapshot snapshot;
    snapshot.playback.state = status;

    snapshot.currentTrack = seriona::control::TrackIdentity{};
    snapshot.currentTrack->trackId = "track-echo";

    snapshot.display = seriona::control::DisplayMetadata{};
    snapshot.display->title = "Seriona Echo";
    snapshot.display->artist = "Adapter Fixture";
    snapshot.display->album = "Contract Smoke";

    snapshot.timeline.position = std::chrono::milliseconds{42000};
    snapshot.timeline.duration = std::chrono::milliseconds{185000};
    snapshot.volume = 0.35F;
    snapshot.shuffle = true;
    snapshot.repeatMode = seriona::control::RepeatMode::All;

    snapshot.capabilities.canPlay = true;
    snapshot.capabilities.canPause = true;
    snapshot.capabilities.canStop = true;
    snapshot.capabilities.canSeek = true;
    snapshot.capabilities.canSkipNext = true;
    snapshot.capabilities.canSkipPrevious = true;
    snapshot.capabilities.canSetRepeat = true;
    snapshot.capabilities.canSetShuffle = true;
    snapshot.capabilities.canSetVolume = true;
    snapshot.capabilities.canSelectTrack = true;

    return snapshot;
}

seriona::control::PlayerStateSnapshot makeTimelineSnapshot(
    seriona::control::PlaybackStatus status,
    std::chrono::milliseconds position,
    std::optional<std::chrono::milliseconds> duration)
{
    seriona::control::PlayerStateSnapshot snapshot;
    snapshot.playback.state = status;
    snapshot.freshness.version = 1;
    snapshot.freshness.sampledAt = std::chrono::steady_clock::now();
    snapshot.timeline.position = position;
    snapshot.timeline.duration = duration;
    return snapshot;
}

void expectPinnedSnapshotPosition(
    Seriona::App::PlaybackController &controller,
    seriona::control::PlaybackStatus status,
    qreal position)
{
    seriona::control::PlayerStateSnapshot snapshot = makeTimelineSnapshot(
        status,
        std::chrono::milliseconds{qRound64(position * 1000.0)},
        std::chrono::milliseconds{60000});
    snapshot.freshness.sampledAt -= std::chrono::seconds{3};

    controller.applyPlayerStateSnapshot(snapshot);

    QCOMPARE(controller.currentPosition(), position);
    QTest::qWait(180);
    QCOMPARE(controller.currentPosition(), position);
}

void applyAndCompareStoppedState(Seriona::App::PlaybackController &controller,
    seriona::control::PlayerStateSnapshot &snapshot,
    seriona::control::PlaybackStatus status)
{
    snapshot.playback.state = status;
    controller.applyPlayerStateSnapshot(snapshot);
    QCOMPARE(controller.isPlaying(), false);
}

}

class PlaybackSnapshotMappingTest : public QObject
{
    Q_OBJECT

private slots:
    void snapshotMapping();
    void emptySnapshot();
    void timelineSmoothing();
    void timelineStoppedAndErrorSnapToBackend();
};

void PlaybackSnapshotMappingTest::snapshotMapping()
{
    Seriona::App::PlaybackController controller;
    seriona::control::PlayerStateSnapshot snapshot = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);

    controller.applyPlayerStateSnapshot(snapshot);

    QCOMPARE(controller.isPlaying(), true);
    QCOMPARE(controller.songTitle(), QStringLiteral("Seriona Echo"));
    QCOMPARE(controller.artistName(), QStringLiteral("Adapter Fixture"));
    QCOMPARE(controller.albumName(), QStringLiteral("Contract Smoke"));
    QCOMPARE(controller.currentPosition(), 42.0);
    QCOMPARE(controller.totalDuration(), 185.0);
    QCOMPARE(controller.currentPositionText(), QStringLiteral("00:42"));
    QCOMPARE(controller.totalDurationText(), QStringLiteral("03:05"));
    QCOMPARE(controller.remainingDurationText(), QStringLiteral("-02:23"));
    QCOMPARE(controller.volume(), static_cast<qreal>(0.35F));
    QCOMPARE(controller.isShuffle(), true);
    QCOMPARE(controller.repeatMode(), 1);
    QCOMPARE(controller.capability(), QStringLiteral("play,pause,stop,seek,skip-next,skip-previous,set-repeat,set-shuffle,set-volume,select-track"));

    applyAndCompareStoppedState(controller, snapshot, seriona::control::PlaybackStatus::Paused);
    applyAndCompareStoppedState(controller, snapshot, seriona::control::PlaybackStatus::Stopped);
    applyAndCompareStoppedState(controller, snapshot, seriona::control::PlaybackStatus::Loading);
    applyAndCompareStoppedState(controller, snapshot, seriona::control::PlaybackStatus::Error);

    snapshot.playback.state = seriona::control::PlaybackStatus::Playing;
    snapshot.muted = true;
    snapshot.volume = 0.85F;
    snapshot.shuffle = false;
    snapshot.repeatMode = seriona::control::RepeatMode::One;
    controller.applyPlayerStateSnapshot(snapshot);

    QCOMPARE(controller.isPlaying(), true);
    QCOMPARE(controller.volume(), 0.0);
    QCOMPARE(controller.isShuffle(), false);
    QCOMPARE(controller.repeatMode(), 2);

    snapshot.muted = false;
    snapshot.volume = 2.0F;
    snapshot.repeatMode = seriona::control::RepeatMode::Off;
    controller.applyPlayerStateSnapshot(snapshot);

    QCOMPARE(controller.volume(), 1.0);
    QCOMPARE(controller.repeatMode(), 0);
}

void PlaybackSnapshotMappingTest::emptySnapshot()
{
    Seriona::App::PlaybackController controller;
    seriona::control::PlayerStateSnapshot snapshot;
    snapshot.timeline.position = std::chrono::milliseconds{12000};

    controller.applyPlayerStateSnapshot(snapshot);

    QCOMPARE(controller.isPlaying(), false);
    QCOMPARE(controller.songTitle(), QStringLiteral("No song selected"));
    QCOMPARE(controller.artistName(), QStringLiteral("Unknown Artist"));
    QCOMPARE(controller.albumName(), QStringLiteral("Unknown Album"));
    QCOMPARE(controller.currentPosition(), 12.0);
    QCOMPARE(controller.totalDuration(), 0.0);
    QCOMPARE(controller.currentPositionText(), QStringLiteral("00:12"));
    QCOMPARE(controller.totalDurationText(), QStringLiteral("00:00"));
    QCOMPARE(controller.remainingDurationText(), QStringLiteral("-00:00"));
    QCOMPARE(controller.volume(), 1.0);
    QCOMPARE(controller.isShuffle(), false);
    QCOMPARE(controller.repeatMode(), 0);
    QCOMPARE(controller.capability(), QStringLiteral("none"));
}

void PlaybackSnapshotMappingTest::timelineSmoothing()
{
    Seriona::App::PlaybackController controller;
    seriona::control::PlayerStateSnapshot playing = makeTimelineSnapshot(
        seriona::control::PlaybackStatus::Playing,
        std::chrono::milliseconds{5800},
        std::chrono::milliseconds{60000});

    controller.applyPlayerStateSnapshot(playing);

    QCOMPARE(controller.totalDuration(), 60.0);
    QCOMPARE(controller.currentPositionText(), QStringLiteral("00:05"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.currentPosition() >= 6.0, 800);
    QCOMPARE(controller.currentPositionText(), QStringLiteral("00:06"));

    seriona::control::PlayerStateSnapshot paused = makeTimelineSnapshot(
        seriona::control::PlaybackStatus::Paused,
        std::chrono::milliseconds{12000},
        std::chrono::milliseconds{60000});
    controller.applyPlayerStateSnapshot(paused);
    QCOMPARE(controller.currentPosition(), 12.0);
    QTest::qWait(180);
    QCOMPARE(controller.currentPosition(), 12.0);

    seriona::control::PlayerStateSnapshot seeked = makeTimelineSnapshot(
        seriona::control::PlaybackStatus::Playing,
        std::chrono::milliseconds{33000},
        std::chrono::milliseconds{60000});
    controller.applyPlayerStateSnapshot(seeked);
    QVERIFY(controller.currentPosition() >= 33.0);
    QVERIFY(controller.currentPosition() < 33.1);
    QCOMPARE(controller.currentPositionText(), QStringLiteral("00:33"));

    seriona::control::PlayerStateSnapshot missingDuration = makeTimelineSnapshot(
        seriona::control::PlaybackStatus::Paused,
        std::chrono::milliseconds{15000},
        std::nullopt);
    controller.applyPlayerStateSnapshot(missingDuration);
    QCOMPARE(controller.currentPosition(), 15.0);
    QCOMPARE(controller.totalDuration(), 0.0);
    QCOMPARE(controller.currentPositionText(), QStringLiteral("00:15"));
    QCOMPARE(controller.remainingDurationText(), QStringLiteral("-00:00"));
}

void PlaybackSnapshotMappingTest::timelineStoppedAndErrorSnapToBackend()
{
    Seriona::App::PlaybackController controller;

    expectPinnedSnapshotPosition(controller, seriona::control::PlaybackStatus::Stopped, 9.0);
    expectPinnedSnapshotPosition(controller, seriona::control::PlaybackStatus::Loading, 11.0);
    expectPinnedSnapshotPosition(controller, seriona::control::PlaybackStatus::Error, 13.0);
}

QTEST_GUILESS_MAIN(PlaybackSnapshotMappingTest)

#include "tst_playback_snapshot_mapping.moc"
