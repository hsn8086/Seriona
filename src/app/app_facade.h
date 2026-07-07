#pragma once

#include "library_model.h"
#include "lyrics_model.h"
#include "navigation_controller.h"
#include "notification_controller.h"
#include "playback_controller.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

#include <cstddef>
#include <memory>

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#if SERIONA_HAS_BACKEND
#include "seriona/control/control_contracts.h"
#endif

namespace Seriona::App {

class BackendBridge;
#if SERIONA_HAS_BACKEND
class WaveformProvider;
#endif

class AppFacade : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString layerName READ layerName CONSTANT)
    Q_PROPERTY(bool foundationReady READ foundationReady CONSTANT)
    Q_PROPERTY(PlaybackController *playback READ playback CONSTANT)
    Q_PROPERTY(LibraryController *library READ library CONSTANT)
    Q_PROPERTY(LyricsModel *lyrics READ lyrics CONSTANT)
    Q_PROPERTY(NotificationController *notifications READ notifications CONSTANT)
    Q_PROPERTY(NavigationController *navigation READ navigation CONSTANT)
    QML_ELEMENT

public:
    explicit AppFacade(QObject *parent = nullptr);
    ~AppFacade() override;

    QString layerName() const;
    bool foundationReady() const;
    PlaybackController *playback();
    LibraryController *library();
    LyricsModel *lyrics();
    NotificationController *notifications();
    NavigationController *navigation();
    bool backendBridgeStartedForTests() const;
    std::size_t backendNotificationCountForTests() const;
#if SERIONA_HAS_BACKEND
    void applyPlayerSnapshotForTests(
        const seriona::control::PlayerStateSnapshot &player,
        const seriona::control::LibraryStateSnapshot &library);
    void applyLibrarySnapshotForTests(
        const seriona::control::PlayerStateSnapshot &player,
        const seriona::control::LibraryStateSnapshot &library);
#endif

    Q_INVOKABLE void shutdown();
    Q_INVOKABLE bool scanLibrary(const QUrl &rootUrl);
    Q_INVOKABLE bool restorePlaylistFromStartup();

private:
#if SERIONA_HAS_BACKEND
    void handlePlayerSnapshotChanged(
        const seriona::control::PlayerStateSnapshot &player,
        const seriona::control::LibraryStateSnapshot &library);
    void handleLibrarySnapshotChanged(
        const seriona::control::PlayerStateSnapshot &player,
        const seriona::control::LibraryStateSnapshot &library);
#endif

    PlaybackController m_playback;
    LibraryController m_library;
    LyricsModel m_lyrics;
    NotificationController m_notifications;
    NavigationController m_navigation;
    std::unique_ptr<BackendBridge> m_backendBridge;
    bool m_shuttingDown = false;
#if SERIONA_HAS_BACKEND
    std::unique_ptr<WaveformProvider> m_waveformProvider;
#endif
};

}
