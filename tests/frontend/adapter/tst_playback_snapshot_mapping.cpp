#include "playback_controller.h"
#include "artwork_palette_worker.h"

#include "seriona/control/control_contracts.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSemaphore>
#include <QSignalSpy>
#include <QString>
#include <QtTest/QTest>
#include <QUrl>

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>

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
    void coverArtworkSourcesUrlEncodeRawPaths();
    void emptyArtExposesEmptySources();
    void thumbnailThenFullUpgradeSkipsSecondPaletteDecode();
    void slowPaletteDecoderDoesNotDelaySnapshotApplication();
    void rapidThumbnailUpdatesDeliverOnlyLatestPalette();
    void latePaletteResultDroppedAfterTrackSwitch();
    void stalePriorTrackFullArtworkDoesNotLeak();
    void shutdownWithBlockedDecoder();
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

void PlaybackSnapshotMappingTest::coverArtworkSourcesUrlEncodeRawPaths()
{
    Seriona::App::PlaybackController controller([](const QString &) {
        return Seriona::App::GradientPalette{
            QStringLiteral("#000000"), QStringLiteral("#111111"), QStringLiteral("#222222")};
    });

    seriona::control::PlayerStateSnapshot snapshot = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    const QString rawArtwork = QStringLiteral("/music/My Tracks/重低音 神曲.png");
    const QString rawThumbnail = QStringLiteral("/thumbs/缩 略 图/track cover.png");
    snapshot.artwork = seriona::control::ArtworkRef{
        .localPath = std::filesystem::path{rawArtwork.toStdString()},
        .thumbnailPath = std::filesystem::path{rawThumbnail.toStdString()},
    };

    controller.applyPlayerStateSnapshot(snapshot);

    QCOMPARE(controller.coverArtworkPath(), rawArtwork);
    QCOMPARE(controller.coverArtworkSource(), QUrl::fromLocalFile(rawArtwork).toString());
    QCOMPARE(controller.coverThumbnailSource(), QUrl::fromLocalFile(rawThumbnail).toString());
}

void PlaybackSnapshotMappingTest::emptyArtExposesEmptySources()
{
    Seriona::App::PlaybackController controller([](const QString &) {
        return Seriona::App::GradientPalette{
            QStringLiteral("#000000"), QStringLiteral("#111111"), QStringLiteral("#222222")};
    });

    seriona::control::PlayerStateSnapshot snapshot = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    controller.applyPlayerStateSnapshot(snapshot);

    QCOMPARE(controller.coverArtworkPath(), QString());
    QCOMPARE(controller.coverArtworkSource(), QString());
    QCOMPARE(controller.coverThumbnailSource(), QString());
    QCOMPARE(controller.songTitle(), QStringLiteral("Seriona Echo"));
}

void PlaybackSnapshotMappingTest::thumbnailThenFullUpgradeSkipsSecondPaletteDecode()
{
    int decodeCalls = 0;
    QString decodedPath;
    Seriona::App::PlaybackController controller(
        [&decodeCalls, &decodedPath](const QString &path) {
            ++decodeCalls;
            decodedPath = path;
            return Seriona::App::GradientPalette{
                QStringLiteral("#112233"), QStringLiteral("#223344"), QStringLiteral("#334455")};
        });

    seriona::control::PlayerStateSnapshot snapshot = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    const QString thumbnailPath = QStringLiteral("/thumbs/folder/track.png");
    snapshot.artwork = seriona::control::ArtworkRef{
        .localPath = std::filesystem::path{thumbnailPath.toStdString()},
        .thumbnailPath = std::filesystem::path{thumbnailPath.toStdString()},
    };

    controller.applyPlayerStateSnapshot(snapshot);

    QCOMPARE(controller.coverThumbnailSource(), QUrl::fromLocalFile(thumbnailPath).toString());
    QTRY_COMPARE_WITH_TIMEOUT(controller.gradientColor0(), QStringLiteral("#112233"), 2000);
    QCOMPARE(decodeCalls, 1);
    QCOMPARE(decodedPath, thumbnailPath);

    snapshot.artwork->localPath = std::filesystem::path{"/covers/full.png"};
    controller.applyPlayerStateSnapshot(snapshot);

    QCOMPARE(controller.coverArtworkSource(), QUrl::fromLocalFile(QStringLiteral("/covers/full.png")).toString());
    QCOMPARE(controller.coverThumbnailSource(), QUrl::fromLocalFile(thumbnailPath).toString());
    QCOMPARE(controller.coverArtworkPath(), QStringLiteral("/covers/full.png"));
    QCOMPARE(decodeCalls, 1);
    QCOMPARE(controller.songTitle(), QStringLiteral("Seriona Echo"));
    QCOMPARE(controller.artistName(), QStringLiteral("Adapter Fixture"));
    QCOMPARE(controller.albumName(), QStringLiteral("Contract Smoke"));
    QCOMPARE(controller.currentPosition(), 42.0);
    QCOMPARE(controller.totalDuration(), 185.0);
}

void PlaybackSnapshotMappingTest::slowPaletteDecoderDoesNotDelaySnapshotApplication()
{
    std::atomic<bool> releaseDecoder{false};
    Seriona::App::PlaybackController controller([&releaseDecoder](const QString &) {
        while (!releaseDecoder.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return Seriona::App::GradientPalette{
            QStringLiteral("#aabbcc"), QStringLiteral("#bbccdd"), QStringLiteral("#ccddee")};
    });

    seriona::control::PlayerStateSnapshot snapshot = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    snapshot.artwork = seriona::control::ArtworkRef{
        .localPath = std::filesystem::path{"/thumbs/slow.png"},
        .thumbnailPath = std::filesystem::path{"/thumbs/slow.png"},
    };

    QSignalSpy songSpy(&controller, &Seriona::App::PlaybackController::currentSongChanged);
    QElapsedTimer timer;
    timer.start();
    controller.applyPlayerStateSnapshot(snapshot);
    const qint64 applyElapsedMs = timer.nsecsElapsed() / 1000000;

    QVERIFY2(applyElapsedMs <= 50,
        qPrintable(QStringLiteral("snapshot application blocked %1 ms behind a 500 ms-class decoder").arg(applyElapsedMs)));
    QCOMPARE(songSpy.count(), 1);
    QCOMPARE(controller.coverArtworkSource(), QUrl::fromLocalFile(QStringLiteral("/thumbs/slow.png")).toString());

    releaseDecoder.store(true);
    QTRY_COMPARE_WITH_TIMEOUT(controller.gradientColor0(), QStringLiteral("#aabbcc"), 2000);
}

void PlaybackSnapshotMappingTest::rapidThumbnailUpdatesDeliverOnlyLatestPalette()
{
    QSemaphore firstDecodeEntered;
    QSemaphore releaseFirstDecode;
    Seriona::App::PlaybackController controller(
        [&firstDecodeEntered, &releaseFirstDecode](const QString &path) {
            if (path == QStringLiteral("/thumbs/a.png")) {
                firstDecodeEntered.release();
                releaseFirstDecode.acquire();
                return Seriona::App::GradientPalette{
                    QStringLiteral("#aa0000"), QStringLiteral("#bb0000"), QStringLiteral("#cc0000")};
            }
            return Seriona::App::GradientPalette{
                QStringLiteral("#00bb00"), QStringLiteral("#00cc00"), QStringLiteral("#00dd00")};
        });

    QSignalSpy gradientSpy(&controller, &Seriona::App::PlaybackController::gradientColorsChanged);

    seriona::control::PlayerStateSnapshot trackA = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    trackA.artwork = seriona::control::ArtworkRef{
        .localPath = std::filesystem::path{"/thumbs/a.png"},
        .thumbnailPath = std::filesystem::path{"/thumbs/a.png"},
    };
    controller.applyPlayerStateSnapshot(trackA);
    QVERIFY(firstDecodeEntered.tryAcquire(1, 2000));

    seriona::control::PlayerStateSnapshot trackB = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    trackB.currentTrack->trackId = "track-echo-2";
    trackB.artwork = seriona::control::ArtworkRef{
        .localPath = std::filesystem::path{"/thumbs/b.png"},
        .thumbnailPath = std::filesystem::path{"/thumbs/b.png"},
    };
    controller.applyPlayerStateSnapshot(trackB);

    releaseFirstDecode.release();
    QTRY_COMPARE_WITH_TIMEOUT(controller.gradientColor0(), QStringLiteral("#00bb00"), 2000);
    QCOMPARE(gradientSpy.count(), 1);
}

void PlaybackSnapshotMappingTest::latePaletteResultDroppedAfterTrackSwitch()
{
    QSemaphore firstDecodeEntered;
    QSemaphore releaseFirstDecode;
    Seriona::App::PlaybackController controller(
        [&firstDecodeEntered, &releaseFirstDecode](const QString &path) {
            if (path == QStringLiteral("/thumbs/a.png")) {
                firstDecodeEntered.release();
                releaseFirstDecode.acquire();
                return Seriona::App::GradientPalette{
                    QStringLiteral("#aa0000"), QStringLiteral("#bb0000"), QStringLiteral("#cc0000")};
            }
            return Seriona::App::GradientPalette{
                QStringLiteral("#00bb00"), QStringLiteral("#00cc00"), QStringLiteral("#00dd00")};
        });

    QSignalSpy gradientSpy(&controller, &Seriona::App::PlaybackController::gradientColorsChanged);

    seriona::control::PlayerStateSnapshot trackA = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    trackA.artwork = seriona::control::ArtworkRef{
        .localPath = std::filesystem::path{"/thumbs/a.png"},
        .thumbnailPath = std::filesystem::path{"/thumbs/a.png"},
    };
    controller.applyPlayerStateSnapshot(trackA);
    QVERIFY(firstDecodeEntered.tryAcquire(1, 2000));

    // Switch to a track without artwork: no worker request is made, but the
    // expected generation advances so A's in-flight result becomes stale.
    seriona::control::PlayerStateSnapshot trackB = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    trackB.currentTrack->trackId = "track-echo-2";
    controller.applyPlayerStateSnapshot(trackB);
    QCOMPARE(controller.coverThumbnailSource(), QString());

    // A's decode completes while the worker still considers it the latest
    // generation, so paletteReady(1) is emitted; it must be dropped by the
    // controller because the expected generation already moved past it.
    releaseFirstDecode.release();
    QTRY_COMPARE_WITH_TIMEOUT(controller.gradientColor0(), QStringLiteral("#4a2c2a"), 2000);
    QCOMPARE(gradientSpy.count(), 1);

    seriona::control::PlayerStateSnapshot trackC = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    trackC.currentTrack->trackId = "track-echo-3";
    trackC.artwork = seriona::control::ArtworkRef{
        .localPath = std::filesystem::path{"/thumbs/c.png"},
        .thumbnailPath = std::filesystem::path{"/thumbs/c.png"},
    };
    controller.applyPlayerStateSnapshot(trackC);
    QTRY_COMPARE_WITH_TIMEOUT(controller.gradientColor0(), QStringLiteral("#00bb00"), 2000);
    QCOMPARE(gradientSpy.count(), 2);
}

void PlaybackSnapshotMappingTest::stalePriorTrackFullArtworkDoesNotLeak()
{
    Seriona::App::PlaybackController controller([](const QString &) {
        return Seriona::App::GradientPalette{
            QStringLiteral("#010101"), QStringLiteral("#020202"), QStringLiteral("#030303")};
    });

    seriona::control::PlayerStateSnapshot trackA = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    trackA.artwork = seriona::control::ArtworkRef{
        .localPath = std::filesystem::path{"/covers/a-full.png"},
        .thumbnailPath = std::filesystem::path{"/thumbs/a.png"},
    };
    controller.applyPlayerStateSnapshot(trackA);
    QCOMPARE(controller.coverArtworkSource(), QUrl::fromLocalFile(QStringLiteral("/covers/a-full.png")).toString());

    seriona::control::PlayerStateSnapshot trackB = makeFilledSnapshot(seriona::control::PlaybackStatus::Playing);
    trackB.currentTrack->trackId = "track-echo-2";
    controller.applyPlayerStateSnapshot(trackB);

    QCOMPARE(controller.currentTrackId(), QStringLiteral("track-echo-2"));
    QCOMPARE(controller.coverArtworkPath(), QString());
    QCOMPARE(controller.coverArtworkSource(), QString());
    QCOMPARE(controller.coverThumbnailSource(), QString());
}

void PlaybackSnapshotMappingTest::shutdownWithBlockedDecoder()
{
    QSemaphore decoderEntered;
    QSemaphore releaseDecode;
    Seriona::App::ArtworkPaletteWorker worker([&decoderEntered, &releaseDecode](const QString &) {
        decoderEntered.release();
        releaseDecode.acquire();
        return Seriona::App::GradientPalette{
            QStringLiteral("#000000"), QStringLiteral("#111111"), QStringLiteral("#222222")};
    });
    worker.requestPalette(QStringLiteral("/thumbs/blocked.png"));
    QVERIFY(decoderEntered.tryAcquire(1, 2000));

    std::atomic<bool> shutdownReturned{false};
    std::thread shutdownThread([&worker, &shutdownReturned] {
        worker.shutdown();
        shutdownReturned.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    QCOMPARE(shutdownReturned.load(), false);

    releaseDecode.release();
    shutdownThread.join();
    QCOMPARE(shutdownReturned.load(), true);

    worker.shutdown();
}

QTEST_GUILESS_MAIN(PlaybackSnapshotMappingTest)

#include "tst_playback_snapshot_mapping.moc"
