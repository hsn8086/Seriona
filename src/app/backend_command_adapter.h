#pragma once

#include <QString>

#include <optional>

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#if SERIONA_HAS_BACKEND
#include "seriona/control/control_contracts.h"
#endif

namespace Seriona::App {

struct CommandResultViewState {
    bool accepted = false;
    QString errorCode;
    QString errorText;
};

struct NotificationViewState {
    QString kind;
    QString code;
    QString message;
    QString title;
    QString severity;
};

#if SERIONA_HAS_BACKEND
[[nodiscard]] QString errorCodeName(seriona::control::MediaControllerErrorCode code);
[[nodiscard]] QString notificationKindName(seriona::control::ControlDomainNotificationKind kind);
[[nodiscard]] CommandResultViewState mapCommandResult(
    const seriona::control::MediaControllerCommandResult &result);
[[nodiscard]] std::optional<seriona::control::ControlDomainNotification> notificationFromRejectedCommandResult(
    const seriona::control::MediaControllerCommandResult &result);
[[nodiscard]] NotificationViewState mapDomainNotification(
    const seriona::control::ControlDomainNotification &notification);
[[nodiscard]] std::optional<NotificationViewState> mapRejectedCommandResult(
    const seriona::control::MediaControllerCommandResult &result);
#endif

}
