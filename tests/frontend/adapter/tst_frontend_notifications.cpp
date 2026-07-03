#include "notification_controller.h"

#include "seriona/control/control_contracts.h"

#include <QSignalSpy>
#include <QVariantMap>
#include <QtTest/QTest>

#include <string>
#include <utility>

namespace {

using Seriona::App::NotificationController;
using seriona::control::ControlDomainNotification;
using seriona::control::ControlDomainNotificationKind;
using seriona::control::MediaControllerCommandResult;
using seriona::control::MediaControllerErrorCode;

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

QVariantMap latestItem(const NotificationController &controller)
{
    const QVariantList items = controller.notifications();
    return items.constLast().toMap();
}

}

class FrontendNotificationsTest : public QObject
{
    Q_OBJECT

private slots:
    void commandRejectedResultExposesCodeAndMessage();
    void playbackErrorExposesBackendMessage();
    void libraryScanErrorExposesBackendMessage();
    void librarySnapshotUpdatedUsesVisibleDefaultMessage();
    void boundedQueueDropsOldestNotifications();
    void unsupportedSettingActionShowsLocalFeedback();
};

void FrontendNotificationsTest::commandRejectedResultExposesCodeAndMessage()
{
    NotificationController controller;
    QSignalSpy notificationsSpy(&controller, &NotificationController::notificationsChanged);

    MediaControllerCommandResult result;
    result.accepted = false;
    result.code = MediaControllerErrorCode::InvalidCommand;
    result.message = "seek position missing";
    controller.enqueueCommandResult(result);

    QCOMPARE(notificationsSpy.count(), 1);
    QCOMPARE(controller.notificationCount(), 1);
    QCOMPARE(controller.latestKind(), QStringLiteral("CommandRejected"));
    QCOMPARE(controller.latestCode(), QStringLiteral("InvalidCommand"));
    QCOMPARE(controller.latestMessage(), QStringLiteral("seek position missing"));
    QCOMPARE(controller.latestTitle(), QStringLiteral("命令被拒绝"));
    QCOMPARE(controller.latestSeverity(), QStringLiteral("error"));

    const QVariantMap item = latestItem(controller);
    QCOMPARE(item.value(QStringLiteral("kind")).toString(), QStringLiteral("CommandRejected"));
    QCOMPARE(item.value(QStringLiteral("code")).toString(), QStringLiteral("InvalidCommand"));
    QCOMPARE(item.value(QStringLiteral("message")).toString(), QStringLiteral("seek position missing"));
}

void FrontendNotificationsTest::playbackErrorExposesBackendMessage()
{
    NotificationController controller;

    controller.enqueueDomainNotification(domainNotification(
        ControlDomainNotificationKind::PlaybackError,
        MediaControllerErrorCode::BackendRejected,
        "audio device disconnected"));

    QCOMPARE(controller.notificationCount(), 1);
    QCOMPARE(controller.latestKind(), QStringLiteral("PlaybackError"));
    QCOMPARE(controller.latestCode(), QStringLiteral("BackendRejected"));
    QCOMPARE(controller.latestMessage(), QStringLiteral("audio device disconnected"));
    QCOMPARE(controller.latestTitle(), QStringLiteral("播放错误"));
    QCOMPARE(controller.latestSeverity(), QStringLiteral("error"));
}

void FrontendNotificationsTest::libraryScanErrorExposesBackendMessage()
{
    NotificationController controller;

    controller.enqueueDomainNotification(domainNotification(
        ControlDomainNotificationKind::LibraryScanError,
        MediaControllerErrorCode::BackendRejected,
        "metadata parser failed"));

    QCOMPARE(controller.notificationCount(), 1);
    QCOMPARE(controller.latestKind(), QStringLiteral("LibraryScanError"));
    QCOMPARE(controller.latestCode(), QStringLiteral("BackendRejected"));
    QCOMPARE(controller.latestMessage(), QStringLiteral("metadata parser failed"));
    QCOMPARE(controller.latestTitle(), QStringLiteral("曲库扫描失败"));
    QCOMPARE(controller.latestSeverity(), QStringLiteral("error"));
}

void FrontendNotificationsTest::librarySnapshotUpdatedUsesVisibleDefaultMessage()
{
    NotificationController controller;

    controller.enqueueDomainNotification(domainNotification(
        ControlDomainNotificationKind::LibrarySnapshotUpdated,
        MediaControllerErrorCode::None,
        {}));

    QCOMPARE(controller.notificationCount(), 1);
    QCOMPARE(controller.latestKind(), QStringLiteral("LibrarySnapshotUpdated"));
    QCOMPARE(controller.latestCode(), QStringLiteral("None"));
    QCOMPARE(controller.latestMessage(), QStringLiteral("曲库快照已更新"));
    QCOMPARE(controller.latestTitle(), QStringLiteral("曲库已更新"));
    QCOMPARE(controller.latestSeverity(), QStringLiteral("info"));
}

void FrontendNotificationsTest::boundedQueueDropsOldestNotifications()
{
    NotificationController controller;

    const int overflowCount = controller.capacity() + 5;
    for (int index = 0; index < overflowCount; ++index) {
        controller.enqueueDomainNotification(domainNotification(
            ControlDomainNotificationKind::PlaybackError,
            MediaControllerErrorCode::BackendRejected,
            QStringLiteral("error %1").arg(index).toStdString()));
    }

    QCOMPARE(controller.notificationCount(), controller.capacity());
    const QVariantList items = controller.notifications();
    QCOMPARE(items.size(), controller.capacity());
    QCOMPARE(items.first().toMap().value(QStringLiteral("message")).toString(), QStringLiteral("error 5"));
    QCOMPARE(controller.latestMessage(), QStringLiteral("error 16"));
}

void FrontendNotificationsTest::unsupportedSettingActionShowsLocalFeedback()
{
    NotificationController controller;

    controller.showUnsupportedAction(QStringLiteral("Crossfade"));

    QCOMPARE(controller.notificationCount(), 1);
    QCOMPARE(controller.latestKind(), QStringLiteral("UnsupportedAction"));
    QCOMPARE(controller.latestCode(), QStringLiteral("Unsupported"));
    QCOMPARE(controller.latestMessage(), QStringLiteral("Crossfade 暂未支持"));
    QCOMPARE(controller.latestTitle(), QStringLiteral("暂未支持"));
    QCOMPARE(controller.latestSeverity(), QStringLiteral("warning"));
}

QTEST_GUILESS_MAIN(FrontendNotificationsTest)

#include "tst_frontend_notifications.moc"
