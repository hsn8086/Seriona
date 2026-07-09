#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#if SERIONA_HAS_BACKEND
#include "seriona/control/media_controller.h"

#include <deque>
#include <functional>
#include <memory>
#endif

namespace Seriona::App {

class BackendBridge : public QObject
{
    Q_OBJECT

public:
#if SERIONA_HAS_BACKEND
    using ControllerFactory = std::function<std::unique_ptr<seriona::control::MediaController>()>;

    explicit BackendBridge(ControllerFactory controllerFactory, QObject *parent = nullptr);
#endif
    explicit BackendBridge(QObject *parent = nullptr);
    ~BackendBridge() override;

    void start();
    void shutdown();
    bool started() const;
    bool shuttingDown() const;
    void drainForTests();

#if SERIONA_HAS_BACKEND
    seriona::control::MediaControllerCommandResult submitCommand(const seriona::control::MediaControlCommand &command);
    seriona::control::MediaControllerCommandResult scanLibrary(const QString &rootPath);
    seriona::control::MediaControllerCommandResult applyFolderSortRules(
        const QString &rootPath,
        const QString &folderNodeId,
        const QVariantList &rules);
    const seriona::control::PlayerStateSnapshot &playerSnapshot() const;
    const seriona::control::LibraryStateSnapshot &librarySnapshot() const;
    const std::deque<seriona::control::ControlDomainNotification> &notifications() const;
#endif

signals:
    void startedChanged();
    void shutdownCompleted();
    void playerSnapshotChanged();
    void librarySnapshotChanged();
    void domainNotificationQueued();

private:
    Q_DISABLE_COPY_MOVE(BackendBridge)

#if SERIONA_HAS_BACKEND
    static ControllerFactory defaultControllerFactory();
    static seriona::control::MediaControllerCommandResult controllerStoppedResult();
    static void unsubscribe(seriona::control::SubscriptionHandle &handle);

    void registerSubscriptions();
    void unsubscribeAll();
    void submitShutdownStop();
    void applyPlayerSnapshot(seriona::control::PlayerStateSnapshot snapshot);
    void applyLibrarySnapshot(seriona::control::LibraryStateSnapshot snapshot);
    void enqueueCommandFailureNotification(const seriona::control::MediaControllerCommandResult &result);
    void enqueueNotification(seriona::control::ControlDomainNotification notification);

    ControllerFactory m_controllerFactory;
    std::unique_ptr<seriona::control::MediaController> m_controller;
    seriona::control::SubscriptionHandle m_playerSubscription;
    seriona::control::SubscriptionHandle m_librarySubscription;
    seriona::control::SubscriptionHandle m_notificationSubscription;
    seriona::control::PlayerStateSnapshot m_playerSnapshot;
    seriona::control::LibraryStateSnapshot m_librarySnapshot;
    std::deque<seriona::control::ControlDomainNotification> m_notifications;
#endif
    bool m_started = false;
    bool m_shuttingDown = false;
};

}
