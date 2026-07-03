#include "backend_bridge.h"

#if SERIONA_HAS_BACKEND
#include <QMetaObject>
#include <QPointer>

#include <utility>
#endif

namespace Seriona::App {

namespace {

#if SERIONA_HAS_BACKEND
constexpr qsizetype kMaxQueuedNotifications = 64;
#endif

}

#if SERIONA_HAS_BACKEND
BackendBridge::BackendBridge(ControllerFactory controllerFactory, QObject *parent)
    : QObject(parent)
    , m_controllerFactory(std::move(controllerFactory))
{
}
#endif

BackendBridge::BackendBridge(QObject *parent)
    : QObject(parent)
#if SERIONA_HAS_BACKEND
    , m_controllerFactory(defaultControllerFactory())
#endif
{
}

BackendBridge::~BackendBridge()
{
    shutdown();
}

void BackendBridge::start()
{
    if (m_started) {
        return;
    }

    m_shuttingDown = false;
#if SERIONA_HAS_BACKEND
    if (!m_controller) {
        m_controller = m_controllerFactory();
    }
    if (!m_controller) {
        return;
    }

    registerSubscriptions();
    m_controller->start();
#endif
    m_started = true;
    emit startedChanged();
}

void BackendBridge::shutdown()
{
    if (!m_started) {
        return;
    }

#if SERIONA_HAS_BACKEND
    submitShutdownStop();
#endif
    m_shuttingDown = true;
#if SERIONA_HAS_BACKEND
    unsubscribeAll();
    if (m_controller) {
        m_controller->shutdown();
        m_controller.reset();
    }
#endif
    if (m_started) {
        m_started = false;
        emit startedChanged();
    }
    emit shutdownCompleted();
}

bool BackendBridge::started() const
{
    return m_started;
}

bool BackendBridge::shuttingDown() const
{
    return m_shuttingDown;
}

void BackendBridge::drainForTests()
{
#if SERIONA_HAS_BACKEND
    if (m_controller) {
        m_controller->drainForTests();
    }
#endif
}

#if SERIONA_HAS_BACKEND
seriona::control::MediaControllerCommandResult BackendBridge::submitCommand(const seriona::control::MediaControlCommand &command)
{
    if (m_shuttingDown || !m_controller) {
        return controllerStoppedResult();
    }

    return m_controller->submitCommand(command);
}

const seriona::control::PlayerStateSnapshot &BackendBridge::playerSnapshot() const
{
    return m_playerSnapshot;
}

const seriona::control::LibraryStateSnapshot &BackendBridge::librarySnapshot() const
{
    return m_librarySnapshot;
}

const std::deque<seriona::control::ControlDomainNotification> &BackendBridge::notifications() const
{
    return m_notifications;
}

BackendBridge::ControllerFactory BackendBridge::defaultControllerFactory()
{
    return [] {
        return seriona::control::makeProductionMediaController(seriona::control::MediaControllerOptions{});
    };
}

seriona::control::MediaControllerCommandResult BackendBridge::controllerStoppedResult()
{
    seriona::control::MediaControllerCommandResult result;
    result.accepted = false;
    result.code = seriona::control::MediaControllerErrorCode::ControllerStopped;
    result.message = "Media controller is stopped";
    return result;
}

void BackendBridge::unsubscribe(seriona::control::SubscriptionHandle &handle)
{
    if (handle.unsubscribe) {
        handle.unsubscribe();
        handle = {};
    }
}

void BackendBridge::registerSubscriptions()
{
    const QPointer<BackendBridge> receiver(this);
    m_playerSubscription = m_controller->subscribePlayerState([receiver](seriona::control::PlayerStateSnapshot snapshot) mutable {
        if (receiver.isNull()) {
            return;
        }

        QMetaObject::invokeMethod(receiver.data(), [receiver, snapshot = std::move(snapshot)]() mutable {
            if (receiver.isNull() || receiver->m_shuttingDown) {
                return;
            }
            receiver->applyPlayerSnapshot(std::move(snapshot));
        }, Qt::QueuedConnection);
    });
    m_librarySubscription = m_controller->subscribeLibraryState([receiver](seriona::control::LibraryStateSnapshot snapshot) mutable {
        if (receiver.isNull()) {
            return;
        }

        QMetaObject::invokeMethod(receiver.data(), [receiver, snapshot = std::move(snapshot)]() mutable {
            if (receiver.isNull() || receiver->m_shuttingDown) {
                return;
            }
            receiver->applyLibrarySnapshot(std::move(snapshot));
        }, Qt::QueuedConnection);
    });
    m_notificationSubscription = m_controller->subscribeDomainNotifications([receiver](seriona::control::ControlDomainNotification notification) mutable {
        if (receiver.isNull()) {
            return;
        }

        QMetaObject::invokeMethod(receiver.data(), [receiver, notification = std::move(notification)]() mutable {
            if (receiver.isNull() || receiver->m_shuttingDown) {
                return;
            }
            receiver->enqueueNotification(std::move(notification));
        }, Qt::QueuedConnection);
    });
}

void BackendBridge::unsubscribeAll()
{
    unsubscribe(m_playerSubscription);
    unsubscribe(m_librarySubscription);
    unsubscribe(m_notificationSubscription);
}

void BackendBridge::submitShutdownStop()
{
    if (!m_controller) {
        return;
    }

    seriona::control::MediaControlCommand stopCommand;
    stopCommand.kind = seriona::control::MediaControlCommandKind::Stop;
    static_cast<void>(m_controller->submitCommand(stopCommand));
}

void BackendBridge::applyPlayerSnapshot(seriona::control::PlayerStateSnapshot snapshot)
{
    m_playerSnapshot = std::move(snapshot);
    emit playerSnapshotChanged();
}

void BackendBridge::applyLibrarySnapshot(seriona::control::LibraryStateSnapshot snapshot)
{
    m_librarySnapshot = std::move(snapshot);
    emit librarySnapshotChanged();
}

void BackendBridge::enqueueNotification(seriona::control::ControlDomainNotification notification)
{
    m_notifications.push_back(std::move(notification));
    while (m_notifications.size() > kMaxQueuedNotifications) {
        m_notifications.pop_front();
    }
    emit domainNotificationQueued();
}
#endif

}
