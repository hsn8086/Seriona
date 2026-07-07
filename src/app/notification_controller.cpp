#include "notification_controller.h"

#include <QDebug>
#include <QVariantMap>

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
    
    // 只输出错误和警告，跳过普通信息通知（避免进度更新刷屏）
    if (latest.severity == QStringLiteral("error") || latest.severity == QStringLiteral("warning")) {
        const QString line = QStringLiteral("Frontend notification kind=%1 code=%2 message=%3")
                                 .arg(latest.kind, latest.code, latest.message);
        if (latest.severity == QStringLiteral("error")) {
            qWarning().noquote() << line;
        } else {
            qInfo().noquote() << line;
        }
    }

    emit notificationsChanged();
    emit latestNotificationChanged();
}

}
