#include "app_facade.h"

#include "seriona/control/control_contracts.h"

#include <QObject>
#include <QString>
#include <QtTest/QTest>

#include <chrono>

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

QTEST_GUILESS_MAIN(PlaybackSnapshotMappingTest)

#include "tst_playback_snapshot_mapping.moc"
