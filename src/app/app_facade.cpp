#include "app_facade.h"

#include "backend_bridge.h"

#if SERIONA_HAS_BACKEND
#include "seriona/control/control_contracts.h"
#endif

#include <QCoreApplication>
#include <QStringList>
#include <QtMath>

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

#if SERIONA_HAS_BACKEND
QString fromBackendString(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString titleFromSnapshot(const seriona::control::PlayerStateSnapshot &snapshot)
{
    if (snapshot.display && !snapshot.display->title.empty()) {
        return fromBackendString(snapshot.display->title);
    }
    if (snapshot.currentTrack && !snapshot.currentTrack->trackId.empty()) {
        return fromBackendString(snapshot.currentTrack->trackId);
    }
    return QStringLiteral("No song selected");
}

QString artistFromSnapshot(const seriona::control::PlayerStateSnapshot &snapshot)
{
    if (!snapshot.display) {
        return QStringLiteral("Unknown Artist");
    }
    if (!snapshot.display->artist.empty()) {
        return fromBackendString(snapshot.display->artist);
    }
    if (!snapshot.display->albumArtist.empty()) {
        return fromBackendString(snapshot.display->albumArtist);
    }
    return QStringLiteral("Unknown Artist");
}

QString albumFromSnapshot(const seriona::control::PlayerStateSnapshot &snapshot)
{
    if (snapshot.display && !snapshot.display->album.empty()) {
        return fromBackendString(snapshot.display->album);
    }
    return QStringLiteral("Unknown Album");
}

qreal secondsFromMilliseconds(std::chrono::milliseconds value)
{
    return static_cast<qreal>(value.count()) / 1000.0;
}

int repeatModeFromSnapshot(seriona::control::RepeatMode repeatMode)
{
    switch (repeatMode) {
    case seriona::control::RepeatMode::Off:
        return 0;
    case seriona::control::RepeatMode::All:
        return 1;
    case seriona::control::RepeatMode::One:
        return 2;
    }

    return 0;
}

QString capabilityFromSnapshot(const seriona::control::PlaybackCapabilities &capabilities)
{
    QStringList labels;
    if (capabilities.canPlay) {
        labels.append(QStringLiteral("play"));
    }
    if (capabilities.canPause) {
        labels.append(QStringLiteral("pause"));
    }
    if (capabilities.canStop) {
        labels.append(QStringLiteral("stop"));
    }
    if (capabilities.canSeek) {
        labels.append(QStringLiteral("seek"));
    }
    if (capabilities.canSkipNext) {
        labels.append(QStringLiteral("skip-next"));
    }
    if (capabilities.canSkipPrevious) {
        labels.append(QStringLiteral("skip-previous"));
    }
    if (capabilities.canSetRepeat) {
        labels.append(QStringLiteral("set-repeat"));
    }
    if (capabilities.canSetShuffle) {
        labels.append(QStringLiteral("set-shuffle"));
    }
    if (capabilities.canSetVolume) {
        labels.append(QStringLiteral("set-volume"));
    }
    if (capabilities.canSelectTrack) {
        labels.append(QStringLiteral("select-track"));
    }

    return labels.isEmpty() ? QStringLiteral("none") : labels.join(QLatin1Char(','));
}
#endif

}

PlaybackController::PlaybackController(QObject *parent)
    : QObject(parent)
{
}

bool PlaybackController::ready() const
{
    return true;
}

QString PlaybackController::capability() const
{
    return m_capability;
}

bool PlaybackController::isPlaying() const
{
    return m_isPlaying;
}

void PlaybackController::setPlaying(bool playing)
{
    if (m_isPlaying == playing) {
        return;
    }

    m_isPlaying = playing;
    emit isPlayingChanged();
}

qreal PlaybackController::currentPosition() const
{
    return m_currentPosition;
}

void PlaybackController::setCurrentPosition(qreal position)
{
    position = qMax(0.0, position);
    if (m_totalDuration > 0.0) {
        position = qMin(position, m_totalDuration);
    }
    if (qAbs(m_currentPosition - position) < 0.001) {
        return;
    }

    m_currentPosition = position;
    emit currentPositionChanged();
    emit durationDisplayChanged();
}

qreal PlaybackController::totalDuration() const
{
    return m_totalDuration;
}

void PlaybackController::setTotalDuration(qreal duration)
{
    duration = qMax(0.0, duration);
    if (qAbs(m_totalDuration - duration) < 0.001) {
        return;
    }

    m_totalDuration = duration;
    if (m_totalDuration > 0.0 && m_currentPosition > m_totalDuration) {
        m_currentPosition = m_totalDuration;
        emit currentPositionChanged();
    }
    emit totalDurationChanged();
    emit durationDisplayChanged();
}

qreal PlaybackController::volume() const
{
    return m_volume;
}

void PlaybackController::setVolume(qreal volume)
{
    volume = clamp(volume, 0.0, 1.0);
    if (qAbs(m_volume - volume) < 0.001) {
        return;
    }

    m_volume = volume;
    emit volumeChanged();
}

bool PlaybackController::isShuffle() const
{
    return m_isShuffle;
}

void PlaybackController::setShuffle(bool shuffle)
{
    if (m_isShuffle == shuffle) {
        return;
    }

    m_isShuffle = shuffle;
    emit isShuffleChanged();
}

int PlaybackController::repeatMode() const
{
    return m_repeatMode;
}

void PlaybackController::setRepeatMode(int repeatMode)
{
    repeatMode = qBound(0, repeatMode, 2);
    if (m_repeatMode == repeatMode) {
        return;
    }

    m_repeatMode = repeatMode;
    emit repeatModeChanged();
}

QString PlaybackController::songTitle() const
{
    return m_songTitle;
}

QString PlaybackController::artistName() const
{
    return m_artistName;
}

QString PlaybackController::albumName() const
{
    return m_albumName;
}

QString PlaybackController::coverPlaceholderText() const
{
    return m_coverPlaceholderText;
}

QString PlaybackController::currentPositionText() const
{
    return formatDuration(m_currentPosition);
}

QString PlaybackController::totalDurationText() const
{
    return formatDuration(m_totalDuration);
}

QString PlaybackController::remainingDurationText() const
{
    return QStringLiteral("-%1").arg(formatDuration(m_totalDuration - m_currentPosition));
}

QVariantList PlaybackController::waveformHeights() const
{
    return m_waveformHeights;
}

#if SERIONA_HAS_BACKEND
void PlaybackController::applyPlayerStateSnapshot(const seriona::control::PlayerStateSnapshot &snapshot)
{
    const qreal durationSeconds = snapshot.timeline.duration
        ? secondsFromMilliseconds(*snapshot.timeline.duration)
        : 0.0;
    const qreal positionSeconds = secondsFromMilliseconds(snapshot.timeline.position);
    const qreal effectiveVolume = snapshot.muted ? 0.0 : static_cast<qreal>(snapshot.volume);

    setTotalDuration(durationSeconds);
    setCurrentPosition(positionSeconds);
    setPlaying(snapshot.playback.state == seriona::control::PlaybackStatus::Playing);
    setVolume(effectiveVolume);
    setShuffle(snapshot.shuffle);
    setRepeatMode(repeatModeFromSnapshot(snapshot.repeatMode));
    setCurrentSong(titleFromSnapshot(snapshot), artistFromSnapshot(snapshot), albumFromSnapshot(snapshot));
    setCapability(capabilityFromSnapshot(snapshot.capabilities));
}
#endif

QString PlaybackController::describeBackendHook() const
{
    return QStringLiteral("Future backend hook: playback status, seek, volume, mode, and track commands.");
}

void PlaybackController::play()
{
    setPlaying(true);
}

void PlaybackController::pause()
{
    setPlaying(false);
}

void PlaybackController::togglePlay()
{
    setPlaying(!m_isPlaying);
}

void PlaybackController::seek(qreal position)
{
    setCurrentPosition(position);
}

void PlaybackController::toggleShuffle()
{
    setShuffle(!m_isShuffle);
}

void PlaybackController::cycleRepeatMode()
{
    setRepeatMode((m_repeatMode + 1) % 3);
}

qreal PlaybackController::clamp(qreal value, qreal minimum, qreal maximum)
{
    return qMin(qMax(value, minimum), maximum);
}

QString PlaybackController::formatDuration(qreal seconds)
{
    const int clampedSeconds = qMax(0, qFloor(seconds));
    const int minutes = clampedSeconds / 60;
    const int remainder = clampedSeconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(remainder, 2, 10, QLatin1Char('0'));
}

void PlaybackController::setCurrentSong(const QString &title, const QString &artist, const QString &album)
{
    if (m_songTitle == title && m_artistName == artist && m_albumName == album) {
        return;
    }

    m_songTitle = title;
    m_artistName = artist;
    m_albumName = album;
    emit currentSongChanged();
}

void PlaybackController::setCapability(const QString &capability)
{
    if (m_capability == capability) {
        return;
    }

    m_capability = capability;
    emit capabilityChanged();
}

NavigationController::NavigationController(QObject *parent)
    : QObject(parent)
{
}

bool NavigationController::ready() const
{
    return true;
}

QString NavigationController::capability() const
{
    return QStringLiteral("startup-navigation-and-shell-state");
}

QString NavigationController::currentView() const
{
    return m_currentView;
}

bool NavigationController::sidebarOpen() const
{
    return m_sidebarOpen;
}

bool NavigationController::startupScreenVisible() const
{
    return m_startupScreenVisible;
}

bool NavigationController::manualSidebarToggle() const
{
    return m_manualSidebarToggle;
}

QString NavigationController::describeBackendHook() const
{
    return QStringLiteral("Future backend hook: startup restoration, library entry intents, and app navigation state.");
}

void NavigationController::restorePlaylistFromStartup()
{
    enterMainShell();
}

void NavigationController::addFolderFromStartup()
{
    enterMainShell();
}

void NavigationController::showPlaybackView()
{
    setCurrentView(QStringLiteral("playback"));
}

void NavigationController::showLyricsView()
{
    setCurrentView(QStringLiteral("lyrics"));
}

void NavigationController::toggleSidebar()
{
    setManualSidebarToggle(true);
    setSidebarOpen(!m_sidebarOpen);
}

void NavigationController::closeSidebar()
{
    setManualSidebarToggle(true);
    setSidebarOpen(false);
}

void NavigationController::clearManualSidebarToggle()
{
    setManualSidebarToggle(false);
}

void NavigationController::syncSidebarForDockCapability(bool dockCapable)
{
    setManualSidebarToggle(false);
    setSidebarOpen(dockCapable);
}

void NavigationController::setCurrentView(const QString &view)
{
    if (m_currentView == view) {
        return;
    }

    m_currentView = view;
    emit currentViewChanged();
}

void NavigationController::setSidebarOpen(bool open)
{
    if (m_sidebarOpen == open) {
        return;
    }

    m_sidebarOpen = open;
    emit sidebarOpenChanged();
}

void NavigationController::setStartupScreenVisible(bool visible)
{
    if (m_startupScreenVisible == visible) {
        return;
    }

    m_startupScreenVisible = visible;
    emit startupScreenVisibleChanged();
}

void NavigationController::setManualSidebarToggle(bool manual)
{
    if (m_manualSidebarToggle == manual) {
        return;
    }

    m_manualSidebarToggle = manual;
    emit manualSidebarToggleChanged();
}

void NavigationController::enterMainShell()
{
    setStartupScreenVisible(false);
    setCurrentView(QStringLiteral("playback"));
}

AppFacade::AppFacade(QObject *parent)
    : QObject(parent)
    , m_playback(this)
    , m_navigation(this)
    , m_backendBridge(std::make_unique<BackendBridge>(this))
{
#if SERIONA_HAS_BACKEND
    connect(m_backendBridge.get(), &BackendBridge::playerSnapshotChanged, this, [this] {
        m_playback.applyPlayerStateSnapshot(m_backendBridge->playerSnapshot());
    });
#endif

    if (backendBridgeAutostartEnabled()) {
        m_backendBridge->start();
    }
}

AppFacade::~AppFacade() = default;

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

NavigationController *AppFacade::navigation()
{
    return &m_navigation;
}

bool AppFacade::backendBridgeStartedForTests() const
{
    return m_backendBridge->started();
}

QString AppFacade::backendContractSummary() const
{
    return QStringLiteral("Foundation plus backend bridge: QML-visible facade owns compact controller anchors while BackendBridge owns MediaController lifecycle.");
}

}
