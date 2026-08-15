#include "backend_command_adapter.h"

#if SERIONA_HAS_BACKEND
#include "seriona/control/control_contracts.h"
#endif

#include <QObject>
#include <QtTest/QTest>

#if SERIONA_HAS_BACKEND
#include <optional>
#include <utility>
#include <vector>
#endif

class CommandResultMappingTest : public QObject {
    Q_OBJECT

private slots:
#if SERIONA_HAS_BACKEND
    void mapsRejectedCommandResultToQtStateAndNotification();
    void acceptedCommandResultDoesNotQueueNotification();
    void mapsDomainNotificationDefaults();
    void mapsEveryDomainNotificationKindName();
#else
    void mapperUnavailableWithoutBackend();
#endif
};

#if SERIONA_HAS_BACKEND
namespace {

using Seriona::App::CommandResultViewState;
using Seriona::App::NotificationViewState;
using seriona::control::ControlDomainNotification;
using seriona::control::ControlDomainNotificationKind;
using seriona::control::MediaControllerCommandResult;
using seriona::control::MediaControllerErrorCode;

MediaControllerCommandResult commandResult(bool accepted, MediaControllerErrorCode code, std::string message)
{
    MediaControllerCommandResult result;
    result.accepted = accepted;
    result.code = code;
    result.message = std::move(message);
    return result;
}

ControlDomainNotification domainNotification(
    ControlDomainNotificationKind kind,
    MediaControllerErrorCode code,
    std::string message)
{
    ControlDomainNotification notification;
    notification.kind = kind;
    notification.errorCode = code;
    notification.message = std::move(message);
    return notification;
}

}

void CommandResultMappingTest::mapsRejectedCommandResultToQtStateAndNotification()
{
    const MediaControllerCommandResult result = commandResult(
        false,
        MediaControllerErrorCode::InvalidCommand,
        "unsupported command");

    const CommandResultViewState mapped = Seriona::App::mapCommandResult(result);
    const std::optional<ControlDomainNotification> notification = Seriona::App::notificationFromRejectedCommandResult(result);
    const std::optional<NotificationViewState> notificationView = Seriona::App::mapRejectedCommandResult(result);

    QCOMPARE(mapped.accepted, false);
    QCOMPARE(mapped.errorCode, QStringLiteral("InvalidCommand"));
    QCOMPARE(mapped.errorText, QStringLiteral("unsupported command"));
    QVERIFY(notification.has_value());
    QCOMPARE(notification->kind, ControlDomainNotificationKind::CommandRejected);
    QCOMPARE(notification->errorCode, MediaControllerErrorCode::InvalidCommand);
    QCOMPARE(QString::fromStdString(notification->message), QStringLiteral("unsupported command"));
    QVERIFY(notificationView.has_value());
    QCOMPARE(notificationView->kind, QStringLiteral("CommandRejected"));
    QCOMPARE(notificationView->code, QStringLiteral("InvalidCommand"));
    QCOMPARE(notificationView->message, QStringLiteral("unsupported command"));
    QCOMPARE(notificationView->title, QStringLiteral("命令被拒绝"));
    QCOMPARE(notificationView->severity, QStringLiteral("error"));
}

void CommandResultMappingTest::acceptedCommandResultDoesNotQueueNotification()
{
    const MediaControllerCommandResult result = commandResult(true, MediaControllerErrorCode::None, {});

    const CommandResultViewState mapped = Seriona::App::mapCommandResult(result);
    const std::optional<ControlDomainNotification> notification = Seriona::App::notificationFromRejectedCommandResult(result);
    const std::optional<NotificationViewState> notificationView = Seriona::App::mapRejectedCommandResult(result);

    QCOMPARE(mapped.accepted, true);
    QCOMPARE(mapped.errorCode, QStringLiteral("None"));
    QCOMPARE(mapped.errorText, QString());
    QVERIFY(!notification.has_value());
    QVERIFY(!notificationView.has_value());
}

void CommandResultMappingTest::mapsDomainNotificationDefaults()
{
    const NotificationViewState completed = Seriona::App::mapDomainNotification(domainNotification(
        ControlDomainNotificationKind::LibraryScanCompleted,
        MediaControllerErrorCode::None,
        {}));
    const NotificationViewState playbackError = Seriona::App::mapDomainNotification(domainNotification(
        ControlDomainNotificationKind::PlaybackError,
        MediaControllerErrorCode::BackendRejected,
        {}));
    const NotificationViewState playbackEnded = Seriona::App::mapDomainNotification(domainNotification(
        ControlDomainNotificationKind::PlaybackEnded,
        MediaControllerErrorCode::None,
        {}));

    QCOMPARE(completed.kind, QStringLiteral("LibraryScanCompleted"));
    QCOMPARE(completed.code, QStringLiteral("None"));
    QCOMPARE(completed.message, QStringLiteral("曲库扫描已完成"));
    QCOMPARE(completed.title, QStringLiteral("曲库扫描完成"));
    QCOMPARE(completed.severity, QStringLiteral("info"));
    QCOMPARE(playbackError.kind, QStringLiteral("PlaybackError"));
    QCOMPARE(playbackError.code, QStringLiteral("BackendRejected"));
    QCOMPARE(playbackError.message, QStringLiteral("播放过程中发生错误"));
    QCOMPARE(playbackError.title, QStringLiteral("播放错误"));
    QCOMPARE(playbackError.severity, QStringLiteral("error"));
    QCOMPARE(playbackEnded.kind, QStringLiteral("PlaybackEnded"));
    QCOMPARE(playbackEnded.code, QStringLiteral("None"));
    QCOMPARE(playbackEnded.message, QStringLiteral("播放列表已播放完毕"));
    QCOMPARE(playbackEnded.title, QStringLiteral("列表播放结束"));
    QCOMPARE(playbackEnded.severity, QStringLiteral("info"));
}

void CommandResultMappingTest::mapsEveryDomainNotificationKindName()
{
    const std::vector<std::pair<ControlDomainNotificationKind, QString>> cases = {
        {ControlDomainNotificationKind::LibrarySnapshotUpdated, QStringLiteral("LibrarySnapshotUpdated")},
        {ControlDomainNotificationKind::LibraryScanStarted, QStringLiteral("LibraryScanStarted")},
        {ControlDomainNotificationKind::LibraryScanProgressUpdated, QStringLiteral("LibraryScanProgressUpdated")},
        {ControlDomainNotificationKind::LibraryScanCompleted, QStringLiteral("LibraryScanCompleted")},
        {ControlDomainNotificationKind::LibraryScanStopped, QStringLiteral("LibraryScanStopped")},
        {ControlDomainNotificationKind::LibraryScanError, QStringLiteral("LibraryScanError")},
        {ControlDomainNotificationKind::PlaybackEnded, QStringLiteral("PlaybackEnded")},
        {ControlDomainNotificationKind::PlaybackError, QStringLiteral("PlaybackError")},
        {ControlDomainNotificationKind::OutputModeFallback, QStringLiteral("OutputModeFallback")},
        {ControlDomainNotificationKind::CommandRejected, QStringLiteral("CommandRejected")},
    };

    for (const auto &[kind, expectedName] : cases) {
        QCOMPARE(Seriona::App::notificationKindName(kind), expectedName);
        QCOMPARE(Seriona::App::mapDomainNotification(domainNotification(kind, MediaControllerErrorCode::None, "custom")).kind, expectedName);
    }
}
#else
void CommandResultMappingTest::mapperUnavailableWithoutBackend()
{
    QVERIFY(true);
}
#endif

QTEST_GUILESS_MAIN(CommandResultMappingTest)

#include "tst_command_result_mapping.moc"
