#pragma once

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#include "backend_command_adapter.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVector>

#if SERIONA_HAS_BACKEND
#include "seriona/control/control_contracts.h"
#endif

namespace Seriona::App {

class NotificationController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged)
    Q_PROPERTY(int notificationCount READ notificationCount NOTIFY notificationsChanged)
    Q_PROPERTY(int capacity READ capacity CONSTANT)
    Q_PROPERTY(bool hasNotification READ hasNotification NOTIFY latestNotificationChanged)
    Q_PROPERTY(QString latestKind READ latestKind NOTIFY latestNotificationChanged)
    Q_PROPERTY(QString latestCode READ latestCode NOTIFY latestNotificationChanged)
    Q_PROPERTY(QString latestMessage READ latestMessage NOTIFY latestNotificationChanged)
    Q_PROPERTY(QString latestTitle READ latestTitle NOTIFY latestNotificationChanged)
    Q_PROPERTY(QString latestSeverity READ latestSeverity NOTIFY latestNotificationChanged)
    QML_ELEMENT
    QML_UNCREATABLE("NotificationController is owned by AppFacade")

public:
    explicit NotificationController(QObject *parent = nullptr);

    QVariantList notifications() const;
    int notificationCount() const;
    int capacity() const;
    bool hasNotification() const;
    QString latestKind() const;
    QString latestCode() const;
    QString latestMessage() const;
    QString latestTitle() const;
    QString latestSeverity() const;

    Q_INVOKABLE void showUnsupportedAction(const QString &actionName);
    // 通用信息 toast（T16：删除成功反馈等成功类提示）
    Q_INVOKABLE void showInfo(const QString &title, const QString &message);
    Q_INVOKABLE void clear();

#if SERIONA_HAS_BACKEND
    void enqueueDomainNotification(const seriona::control::ControlDomainNotification &notification);
    void enqueueCommandResult(const seriona::control::MediaControllerCommandResult &result);
#endif

signals:
    void notificationsChanged();
    void latestNotificationChanged();

private:
    static constexpr int kCapacity = 12;

    const NotificationViewState *latestEntry() const;
    void append(NotificationViewState entry);

    QVector<NotificationViewState> m_notifications;
};

}
