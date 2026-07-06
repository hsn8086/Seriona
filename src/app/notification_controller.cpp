#include "notification_controller.h"

#include <QDebug>
#include <QVariantMap>

#if SERIONA_HAS_BACKEND
#include <string>
#endif

#include <utility>

namespace Seriona::App {

namespace {

#if SERIONA_HAS_BACKEND
QString fromBackendString(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString notificationKindName(seriona::control::ControlDomainNotificationKind kind)
{
    switch (kind) {
    case seriona::control::ControlDomainNotificationKind::LibrarySnapshotUpdated:
        return QStringLiteral("LibrarySnapshotUpdated");
    case seriona::control::ControlDomainNotificationKind::LibraryScanStarted:
        return QStringLiteral("LibraryScanStarted");
    case seriona::control::ControlDomainNotificationKind::LibraryScanProgressUpdated:
        return QStringLiteral("LibraryScanProgressUpdated");
    case seriona::control::ControlDomainNotificationKind::LibraryScanCompleted:
        return QStringLiteral("LibraryScanCompleted");
    case seriona::control::ControlDomainNotificationKind::LibraryScanStopped:
        return QStringLiteral("LibraryScanStopped");
    case seriona::control::ControlDomainNotificationKind::LibraryScanError:
        return QStringLiteral("LibraryScanError");
    case seriona::control::ControlDomainNotificationKind::PlaybackEnded:
        return QStringLiteral("PlaybackEnded");
    case seriona::control::ControlDomainNotificationKind::PlaybackError:
        return QStringLiteral("PlaybackError");
    case seriona::control::ControlDomainNotificationKind::OutputModeFallback:
        return QStringLiteral("OutputModeFallback");
    case seriona::control::ControlDomainNotificationKind::CommandRejected:
        return QStringLiteral("CommandRejected");
    }

    return QStringLiteral("UnknownNotification");
}

QString errorCodeName(seriona::control::MediaControllerErrorCode code)
{
    switch (code) {
    case seriona::control::MediaControllerErrorCode::None:
        return QStringLiteral("None");
    case seriona::control::MediaControllerErrorCode::ControllerStopped:
        return QStringLiteral("ControllerStopped");
    case seriona::control::MediaControllerErrorCode::NoPlayableTrack:
        return QStringLiteral("NoPlayableTrack");
    case seriona::control::MediaControllerErrorCode::TrackNotInLibrary:
        return QStringLiteral("TrackNotInLibrary");
    case seriona::control::MediaControllerErrorCode::InvalidCommand:
        return QStringLiteral("InvalidCommand");
    case seriona::control::MediaControllerErrorCode::BackendRejected:
        return QStringLiteral("BackendRejected");
    }

    return QStringLiteral("UnknownError");
}
#endif

}

NotificationController::NotificationController(QObject *parent)
    : QObject(parent)
{
}

QVariantList NotificationController::notifications() const
{
    QVariantList items;
    items.reserve(m_notifications.size());
    for (const NotificationEntry &entry : m_notifications) {
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
    const NotificationEntry *entry = latestEntry();
    return entry != nullptr ? entry->kind : QString();
}

QString NotificationController::latestCode() const
{
    const NotificationEntry *entry = latestEntry();
    return entry != nullptr ? entry->code : QString();
}

QString NotificationController::latestMessage() const
{
    const NotificationEntry *entry = latestEntry();
    return entry != nullptr ? entry->message : QString();
}

QString NotificationController::latestTitle() const
{
    const NotificationEntry *entry = latestEntry();
    return entry != nullptr ? entry->title : QString();
}

QString NotificationController::latestSeverity() const
{
    const NotificationEntry *entry = latestEntry();
    return entry != nullptr ? entry->severity : QString();
}

void NotificationController::showUnsupportedAction(const QString &actionName)
{
    append(NotificationEntry{
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
    const QString kind = notificationKindName(notification.kind);
    const QString code = errorCodeName(notification.errorCode);
    QString message = fromBackendString(notification.message);
    QString title = tr("通知");
    QString severity = QStringLiteral("info");

    switch (notification.kind) {
    case seriona::control::ControlDomainNotificationKind::CommandRejected:
        title = tr("命令被拒绝");
        severity = QStringLiteral("error");
        if (message.isEmpty()) {
            message = tr("后端拒绝了这次操作");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::PlaybackError:
        title = tr("播放错误");
        severity = QStringLiteral("error");
        if (message.isEmpty()) {
            message = tr("播放过程中发生错误");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibraryScanError:
        title = tr("曲库扫描失败");
        severity = QStringLiteral("error");
        if (message.isEmpty()) {
            message = tr("曲库扫描失败，请重试");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibrarySnapshotUpdated:
        title = tr("曲库已更新");
        if (message.isEmpty()) {
            message = tr("曲库快照已更新");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibraryScanStarted:
        title = tr("开始扫描曲库");
        if (message.isEmpty()) {
            message = tr("正在扫描曲库");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibraryScanProgressUpdated:
        title = tr("曲库扫描中");
        if (message.isEmpty()) {
            message = tr("曲库扫描进度已更新");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibraryScanCompleted:
        title = tr("曲库扫描完成");
        if (message.isEmpty()) {
            message = tr("曲库扫描已完成");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibraryScanStopped:
        title = tr("曲库扫描已停止");
        severity = QStringLiteral("warning");
        if (message.isEmpty()) {
            message = tr("曲库扫描已停止");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::PlaybackEnded:
        title = tr("播放结束");
        if (message.isEmpty()) {
            message = tr("当前曲目播放结束");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::OutputModeFallback:
        title = tr("输出模式已回退");
        severity = QStringLiteral("warning");
        if (message.isEmpty()) {
            message = tr("音频输出模式已回退");
        }
        break;
    }

    append(NotificationEntry{kind, code, message, title, severity});
}

void NotificationController::enqueueCommandResult(const seriona::control::MediaControllerCommandResult &result)
{
    if (result.accepted) {
        return;
    }

    QString message = fromBackendString(result.message);
    if (message.isEmpty()) {
        message = tr("后端拒绝了这次操作");
    }
    append(NotificationEntry{
        QStringLiteral("CommandRejected"),
        errorCodeName(result.code),
        message,
        tr("命令被拒绝"),
        QStringLiteral("error"),
    });
}
#endif

const NotificationController::NotificationEntry *NotificationController::latestEntry() const
{
    return m_notifications.isEmpty() ? nullptr : &m_notifications.last();
}

void NotificationController::append(NotificationEntry entry)
{
    m_notifications.append(std::move(entry));
    while (m_notifications.size() > kCapacity) {
        m_notifications.removeFirst();
    }

    const NotificationEntry &latest = m_notifications.last();
    
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
