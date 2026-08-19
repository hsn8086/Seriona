#include "notification_controller.h"

#include <QVariantMap>

#if SERIONA_HAS_BACKEND
#include <spdlog/spdlog.h>
#endif

#include <optional>
#include <utility>

namespace Seriona::App {

NotificationController::NotificationController(QObject *parent)
    : QObject(parent)
{
}

QVariantList NotificationController::notifications() const
{
    QVariantList items;
    items.reserve(m_notifications.size());
    for (const NotificationViewState &entry : m_notifications) {
        items.append(QVariantMap{
            {QStringLiteral("kind"), entry.kind},
            {QStringLiteral("code"), entry.code},
            {QStringLiteral("message"), entry.message},
            {QStringLiteral("title"), entry.title},
            {QStringLiteral("severity"), entry.severity},
        });
    }
    return items;
}

int NotificationController::notificationCount() const
{
    return m_notifications.size();
}

int NotificationController::capacity() const
{
    return kCapacity;
}

bool NotificationController::hasNotification() const
{
    return !m_notifications.isEmpty();
}

QString NotificationController::latestKind() const
{
    const NotificationViewState *entry = latestEntry();
    return entry != nullptr ? entry->kind : QString();
}

QString NotificationController::latestCode() const
{
    const NotificationViewState *entry = latestEntry();
    return entry != nullptr ? entry->code : QString();
}

QString NotificationController::latestMessage() const
{
    const NotificationViewState *entry = latestEntry();
    return entry != nullptr ? entry->message : QString();
}

QString NotificationController::latestTitle() const
{
    const NotificationViewState *entry = latestEntry();
    return entry != nullptr ? entry->title : QString();
}

QString NotificationController::latestSeverity() const
{
    const NotificationViewState *entry = latestEntry();
    return entry != nullptr ? entry->severity : QString();
}

void NotificationController::showUnsupportedAction(const QString &actionName)
{
    append(NotificationViewState{
        QStringLiteral("UnsupportedAction"),
        QStringLiteral("Unsupported"),
        tr("%1 暂未支持").arg(actionName),
        tr("暂未支持"),
        QStringLiteral("warning"),
    });
}

void NotificationController::showInfo(const QString &title, const QString &message)
{
    append(NotificationViewState{
        QStringLiteral("Info"),
        QStringLiteral("None"),
        message,
        title,
        QStringLiteral("info"),
    });
}

void NotificationController::clear()
{
    if (m_notifications.isEmpty()) {
        return;
    }

    m_notifications.clear();
    emit notificationsChanged();
    emit latestNotificationChanged();
}

#if SERIONA_HAS_BACKEND
void NotificationController::enqueueDomainNotification(const seriona::control::ControlDomainNotification &notification)
{
    append(mapDomainNotification(notification));
}

void NotificationController::enqueueCommandResult(const seriona::control::MediaControllerCommandResult &result)
{
    const std::optional<NotificationViewState> notification = mapRejectedCommandResult(result);
    if (!notification.has_value()) {
        return;
    }
    append(*notification);
}
#endif

const NotificationViewState *NotificationController::latestEntry() const
{
    return m_notifications.isEmpty() ? nullptr : &m_notifications.last();
}

void NotificationController::append(NotificationViewState entry)
{
    m_notifications.append(std::move(entry));
    while (m_notifications.size() > kCapacity) {
        m_notifications.removeFirst();
    }

    const NotificationViewState &latest = m_notifications.last();

#if SERIONA_HAS_BACKEND
    // 终端日志统一由 spdlog 负责（initializeApplicationLogging 已初始化默认 logger）
    if (latest.severity == QStringLiteral("error") || latest.severity == QStringLiteral("warning")) {
        const auto kind = latest.kind.toStdString();
        const auto code = latest.code.toStdString();
        const auto message = latest.message.toStdString();
        if (latest.severity == QStringLiteral("error")) {
            spdlog::error("Frontend notification kind={} code={} message={}", kind, code, message);
        } else {
            spdlog::warn("Frontend notification kind={} code={} message={}", kind, code, message);
        }
    }
#endif

    emit notificationsChanged();
    emit latestNotificationChanged();
}

}
