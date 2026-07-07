#include "app_facade.h"

#include "backend_bridge.h"

#if SERIONA_HAS_BACKEND
#include "waveform_provider.h"

#include "seriona/control/control_contracts.h"
#endif

#include <QCoreApplication>
#include <QUrl>
#include <QVariant>

namespace Seriona::App {

namespace {

constexpr auto kBackendBridgeAutostartProperty = "seriona.backendBridgeAutostartEnabled";

bool backendBridgeAutostartEnabled()
{
    const QCoreApplication *application = QCoreApplication::instance();
    if (!application) {
        return true;
    }

    const QVariant configured = application->property(kBackendBridgeAutostartProperty);
    return configured.isValid() ? configured.toBool() : true;
}

}

#if SERIONA_HAS_BACKEND
void AppFacade::handlePlayerSnapshotChanged(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library)
{
    m_playback.applyPlayerStateSnapshot(player, &library);
    m_lyrics.applyPlayerStateSnapshot(player, &library);
    m_library.applyPlayerStateSnapshot(player, false);
    m_waveformProvider->requestForSnapshots(player, library);
}

void AppFacade::handleLibrarySnapshotChanged(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library)
{
    m_library.applyLibraryStateSnapshot(library);
    m_playback.applyPlayerStateSnapshot(player, &library);
    m_lyrics.applyPlayerStateSnapshot(player, &library);
    m_library.applyPlayerStateSnapshot(player, true);
    m_waveformProvider->requestForSnapshots(player, library);
}
#endif

AppFacade::AppFacade(QObject *parent)
    : QObject(parent)
    , m_playback(this)
    , m_library(this)
    , m_lyrics(this)
    , m_notifications(this)
    , m_navigation(this)
    , m_backendBridge(std::make_unique<BackendBridge>(this))
#if SERIONA_HAS_BACKEND
    , m_waveformProvider(std::make_unique<WaveformProvider>(this))
#endif
{
#if SERIONA_HAS_BACKEND
    m_playback.setCommandExecutor([this](const seriona::control::MediaControlCommand &command) {
        return m_backendBridge->submitCommand(command);
    });
    m_library.setCommandExecutor([this](const seriona::control::MediaControlCommand &command) {
        return m_backendBridge->submitCommand(command);
    });
    m_library.setScanExecutor([this](const QString &rootPath) {
        return m_backendBridge->scanLibrary(rootPath);
    });
    connect(m_waveformProvider.get(), &WaveformProvider::waveformReady, this, [this](const WaveformResult &result) {
        if (m_shuttingDown) {
            return;
        }
        m_playback.applyWaveform(result.heights, result.barWidth);
    });
    connect(m_backendBridge.get(), &BackendBridge::playerSnapshotChanged, this, [this] {
        const seriona::control::PlayerStateSnapshot &player = m_backendBridge->playerSnapshot();
        const seriona::control::LibraryStateSnapshot &library = m_backendBridge->librarySnapshot();
        handlePlayerSnapshotChanged(player, library);
    });
    connect(m_backendBridge.get(), &BackendBridge::librarySnapshotChanged, this, [this] {
        const seriona::control::PlayerStateSnapshot &player = m_backendBridge->playerSnapshot();
        const seriona::control::LibraryStateSnapshot &library = m_backendBridge->librarySnapshot();
        handleLibrarySnapshotChanged(player, library);
    });
    connect(m_backendBridge.get(), &BackendBridge::domainNotificationQueued, this, [this] {
        const auto &notifications = m_backendBridge->notifications();
        if (notifications.empty()) {
            return;
        }
        m_notifications.enqueueDomainNotification(notifications.back());
    });
#endif

    if (backendBridgeAutostartEnabled()) {
        m_backendBridge->start();
    }

    if (QCoreApplication *application = QCoreApplication::instance()) {
        connect(application, &QCoreApplication::aboutToQuit, this, &AppFacade::shutdown, Qt::DirectConnection);
    }
}

AppFacade::~AppFacade()
{
    shutdown();
}

QString AppFacade::layerName() const
{
    return QStringLiteral("Seriona C++ Middle Layer");
}

bool AppFacade::foundationReady() const
{
    return true;
}

PlaybackController *AppFacade::playback()
{
    return &m_playback;
}

LibraryController *AppFacade::library()
{
    return &m_library;
}

LyricsModel *AppFacade::lyrics()
{
    return &m_lyrics;
}

NotificationController *AppFacade::notifications()
{
    return &m_notifications;
}

NavigationController *AppFacade::navigation()
{
    return &m_navigation;
}

bool AppFacade::backendBridgeStartedForTests() const
{
    return m_backendBridge->started();
}

std::size_t AppFacade::backendNotificationCountForTests() const
{
#if SERIONA_HAS_BACKEND
    return m_backendBridge->notifications().size();
#else
    return 0U;
#endif
}

bool AppFacade::scanLibrary(const QUrl &rootUrl)
{
    return m_navigation.scanLibrary(m_library, rootUrl);
}

bool AppFacade::restorePlaylistFromStartup()
{
    return m_navigation.restorePlaylistFromStartup(m_library, [this](const QUrl &rootUrl) {
        return scanLibrary(rootUrl);
    });
}

#if SERIONA_HAS_BACKEND
void AppFacade::applyPlayerSnapshotForTests(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library)
{
    handlePlayerSnapshotChanged(player, library);
}

void AppFacade::applyLibrarySnapshotForTests(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library)
{
    handleLibrarySnapshotChanged(player, library);
}
#endif

void AppFacade::shutdown()
{
    if (m_shuttingDown) {
        return;
    }

    m_shuttingDown = true;
#if SERIONA_HAS_BACKEND
    if (m_waveformProvider) {
        m_waveformProvider->cancelPending();
    }
#endif
    if (m_backendBridge) {
        m_backendBridge->shutdown();
    }
}

}
