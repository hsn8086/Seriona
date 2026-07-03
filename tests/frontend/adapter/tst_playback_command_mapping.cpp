#include "app_facade.h"
#include "backend_bridge.h"

#include "seriona/control/control_contracts.h"

#include <QObject>
#include <QSignalSpy>
#include <QtTest/QTest>

#include <chrono>
#include <optional>

namespace {

seriona::control::MediaControllerCommandResult acceptedResult()
{
    seriona::control::MediaControllerCommandResult result;
    result.accepted = true;
    result.code = seriona::control::MediaControllerErrorCode::None;
    return result;
}

struct CommandRecorder {
    std::optional<seriona::control::MediaControlCommand> command;
    int count = 0;

    seriona::control::MediaControllerCommandResult record(const seriona::control::MediaControlCommand &nextCommand)
    {
        command = nextCommand;
        ++count;
        return acceptedResult();
    }

    void clear()
    {
        command.reset();
        count = 0;
    }
};

void expectCommandKind(const CommandRecorder &recorder, seriona::control::MediaControlCommandKind kind)
{
    QVERIFY(recorder.command.has_value());
    QCOMPARE(static_cast<int>(recorder.command->kind), static_cast<int>(kind));
}

seriona::control::PlayerStateSnapshot snapshotWithRepeatMode(seriona::control::RepeatMode repeatMode)
{
    seriona::control::PlayerStateSnapshot snapshot;
    snapshot.repeatMode = repeatMode;
    return snapshot;
}

}

class PlaybackCommandMappingTest : public QObject
{
    Q_OBJECT

private slots:
    void visiblePlaybackControlsSubmitCommands();
    void repeatCycleUsesSnapshotMode();
    void rejectedCommandKeepsReadModelAndQueuesNotification();
};

void PlaybackCommandMappingTest::visiblePlaybackControlsSubmitCommands()
{
    Seriona::App::PlaybackController controller;
    CommandRecorder recorder;
    controller.setCommandExecutor([&recorder](const seriona::control::MediaControlCommand &command) {
        return recorder.record(command);
    });

    controller.togglePlay();
    expectCommandKind(recorder, seriona::control::MediaControlCommandKind::TogglePlayPause);
    QCOMPARE(controller.isPlaying(), false);

    controller.skipPrevious();
    expectCommandKind(recorder, seriona::control::MediaControlCommandKind::SkipPrevious);

    controller.skipNext();
    expectCommandKind(recorder, seriona::control::MediaControlCommandKind::SkipNext);

    controller.seek(42.25);
    expectCommandKind(recorder, seriona::control::MediaControlCommandKind::SeekTo);
    QVERIFY(recorder.command->position.has_value());
    QCOMPARE(recorder.command->position->count(), 42250);
    QCOMPARE(controller.currentPosition(), 0.0);

    controller.setVolume(0.42);
    expectCommandKind(recorder, seriona::control::MediaControlCommandKind::SetVolume);
    QVERIFY(recorder.command->volume.has_value());
    QVERIFY(qAbs(static_cast<qreal>(*recorder.command->volume) - 0.42) < 0.001);
    QCOMPARE(controller.volume(), 1.0);

    controller.toggleShuffle();
    expectCommandKind(recorder, seriona::control::MediaControlCommandKind::SetShuffle);
    QVERIFY(recorder.command->shuffle.has_value());
    QCOMPARE(*recorder.command->shuffle, true);
    QCOMPARE(controller.isShuffle(), false);

    controller.cycleRepeatMode();
    expectCommandKind(recorder, seriona::control::MediaControlCommandKind::SetRepeatMode);
    QVERIFY(recorder.command->repeatMode.has_value());
    QCOMPARE(static_cast<int>(*recorder.command->repeatMode), static_cast<int>(seriona::control::RepeatMode::All));
    QCOMPARE(controller.repeatMode(), 0);

    controller.setMuted(true);
    expectCommandKind(recorder, seriona::control::MediaControlCommandKind::SetMuted);
    QVERIFY(recorder.command->muted.has_value());
    QCOMPARE(*recorder.command->muted, true);
}

void PlaybackCommandMappingTest::repeatCycleUsesSnapshotMode()
{
    Seriona::App::PlaybackController controller;
    CommandRecorder recorder;
    controller.setCommandExecutor([&recorder](const seriona::control::MediaControlCommand &command) {
        return recorder.record(command);
    });

    controller.applyPlayerStateSnapshot(snapshotWithRepeatMode(seriona::control::RepeatMode::All));
    recorder.clear();
    controller.cycleRepeatMode();
    expectCommandKind(recorder, seriona::control::MediaControlCommandKind::SetRepeatMode);
    QVERIFY(recorder.command->repeatMode.has_value());
    QCOMPARE(static_cast<int>(*recorder.command->repeatMode), static_cast<int>(seriona::control::RepeatMode::One));
    QCOMPARE(controller.repeatMode(), 1);

    controller.applyPlayerStateSnapshot(snapshotWithRepeatMode(seriona::control::RepeatMode::One));
    recorder.clear();
    controller.cycleRepeatMode();
    expectCommandKind(recorder, seriona::control::MediaControlCommandKind::SetRepeatMode);
    QVERIFY(recorder.command->repeatMode.has_value());
    QCOMPARE(static_cast<int>(*recorder.command->repeatMode), static_cast<int>(seriona::control::RepeatMode::Off));
    QCOMPARE(controller.repeatMode(), 2);
}

void PlaybackCommandMappingTest::rejectedCommandKeepsReadModelAndQueuesNotification()
{
    Seriona::App::BackendBridge bridge;
    Seriona::App::PlaybackController controller;
    controller.setCommandExecutor([&bridge](const seriona::control::MediaControlCommand &command) {
        return bridge.submitCommand(command);
    });
    QSignalSpy notificationSpy(&bridge, &Seriona::App::BackendBridge::domainNotificationQueued);

    controller.togglePlay();

    QCOMPARE(controller.isPlaying(), false);
    QCOMPARE(notificationSpy.count(), 1);
    QCOMPARE(bridge.notifications().size(), static_cast<std::size_t>(1));
    const seriona::control::ControlDomainNotification &notification = bridge.notifications().back();
    QCOMPARE(static_cast<int>(notification.kind), static_cast<int>(seriona::control::ControlDomainNotificationKind::CommandRejected));
    QCOMPARE(static_cast<int>(notification.errorCode), static_cast<int>(seriona::control::MediaControllerErrorCode::ControllerStopped));
    QCOMPARE(QString::fromStdString(notification.message), QStringLiteral("Media controller is stopped"));
}

QTEST_GUILESS_MAIN(PlaybackCommandMappingTest)

#include "tst_playback_command_mapping.moc"
