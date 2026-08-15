#include "backend_command_adapter.h"

#if SERIONA_HAS_BACKEND
#include <QCoreApplication>

#include <string>
#include <utility>
#endif

namespace Seriona::App {

#if SERIONA_HAS_BACKEND
namespace {

QString fromBackendString(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString notificationText(const char *sourceText)
{
    return QCoreApplication::translate("NotificationController", sourceText);
}

QString commandRejectedFallbackMessage()
{
    return notificationText("后端拒绝了这次操作");
}

NotificationViewState commandRejectedNotification(const QString &code, QString message)
{
    if (message.isEmpty()) {
        message = commandRejectedFallbackMessage();
    }
    return NotificationViewState{
        QStringLiteral("CommandRejected"),
        code,
        message,
        notificationText("命令被拒绝"),
        QStringLiteral("error"),
    };
}

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
    case seriona::control::ControlDomainNotificationKind::FolderSortRulesApplied:
        return QStringLiteral("FolderSortRulesApplied");
    }

    return QStringLiteral("UnknownNotification");
}

CommandResultViewState mapCommandResult(const seriona::control::MediaControllerCommandResult &result)
{
    QString message = fromBackendString(result.message);
    if (!result.accepted && message.isEmpty()) {
        message = commandRejectedFallbackMessage();
    }
    return CommandResultViewState{result.accepted, errorCodeName(result.code), message};
}

std::optional<seriona::control::ControlDomainNotification> notificationFromRejectedCommandResult(
    const seriona::control::MediaControllerCommandResult &result)
{
    if (result.accepted) {
        return std::nullopt;
    }

    seriona::control::ControlDomainNotification notification;
    notification.kind = seriona::control::ControlDomainNotificationKind::CommandRejected;
    notification.errorCode = result.code;
    notification.message = result.message;
    return notification;
}

NotificationViewState mapDomainNotification(const seriona::control::ControlDomainNotification &notification)
{
    const QString kind = notificationKindName(notification.kind);
    const QString code = errorCodeName(notification.errorCode);
    QString message = fromBackendString(notification.message);
    QString title = notificationText("通知");
    QString severity = QStringLiteral("info");

    switch (notification.kind) {
    case seriona::control::ControlDomainNotificationKind::CommandRejected:
        return commandRejectedNotification(code, std::move(message));
    case seriona::control::ControlDomainNotificationKind::PlaybackError:
        title = notificationText("播放错误");
        severity = QStringLiteral("error");
        if (message.isEmpty()) {
            message = notificationText("播放过程中发生错误");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibraryScanError:
        title = notificationText("曲库扫描失败");
        severity = QStringLiteral("error");
        if (message.isEmpty()) {
            message = notificationText("曲库扫描失败，请重试");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibrarySnapshotUpdated:
        title = notificationText("曲库已更新");
        if (message.isEmpty()) {
            message = notificationText("曲库快照已更新");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibraryScanStarted:
        title = notificationText("开始扫描曲库");
        if (message.isEmpty()) {
            message = notificationText("正在扫描曲库");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibraryScanProgressUpdated:
        title = notificationText("曲库扫描中");
        if (message.isEmpty()) {
            message = notificationText("曲库扫描进度已更新");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibraryScanCompleted:
        title = notificationText("曲库扫描完成");
        if (message.isEmpty()) {
            message = notificationText("曲库扫描已完成");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::LibraryScanStopped:
        title = notificationText("曲库扫描已停止");
        severity = QStringLiteral("warning");
        if (message.isEmpty()) {
            message = notificationText("曲库扫描已停止");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::PlaybackEnded:
        title = notificationText("列表播放结束");
        if (message.isEmpty()) {
            message = notificationText("播放列表已播放完毕");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::OutputModeFallback:
        title = notificationText("输出模式已回退");
        severity = QStringLiteral("warning");
        if (message.isEmpty()) {
            message = notificationText("音频输出模式已回退");
        }
        break;
    case seriona::control::ControlDomainNotificationKind::FolderSortRulesApplied:
        title = notificationText("排序规则已保存");
        if (message.isEmpty()) {
            message = notificationText("文件夹排序规则已保存");
        }
        break;
    }

    return NotificationViewState{kind, code, message, title, severity};
}

std::optional<NotificationViewState> mapRejectedCommandResult(
    const seriona::control::MediaControllerCommandResult &result)
{
    const std::optional<seriona::control::ControlDomainNotification> notification = notificationFromRejectedCommandResult(result);
    if (!notification.has_value()) {
        return std::nullopt;
    }
    return mapDomainNotification(*notification);
}
#endif

}
