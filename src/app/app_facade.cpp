#include "app_facade.h"

#include <QtMath>

namespace Seriona::App {

PlaybackController::PlaybackController(QObject *parent)
    : QObject(parent)
{
    m_mockPositionTimer.setInterval(1000);
    connect(&m_mockPositionTimer, &QTimer::timeout, this, &PlaybackController::advanceMockPosition);
}

bool PlaybackController::ready() const
{
    return true;
}

QString PlaybackController::capability() const
{
    return QStringLiteral("playback-state-and-commands");
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
    updateMockTimer();
    emit isPlayingChanged();
}

qreal PlaybackController::currentPosition() const
{
    return m_currentPosition;
}

void PlaybackController::setCurrentPosition(qreal position)
{
    position = clamp(position, 0.0, m_totalDuration);
    if (qFuzzyCompare(m_currentPosition, position)) {
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
    if (qFuzzyCompare(m_totalDuration, duration)) {
        return;
    }

    m_totalDuration = duration;
    if (m_currentPosition > m_totalDuration) {
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
    if (qFuzzyCompare(m_volume, volume)) {
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

void PlaybackController::advanceMockPosition()
{
    if (m_currentPosition < m_totalDuration) {
        setCurrentPosition(m_currentPosition + 1.0);
    } else {
        setCurrentPosition(0.0);
    }
}

void PlaybackController::updateMockTimer()
{
    if (m_isPlaying) {
        m_mockPositionTimer.start();
    } else {
        m_mockPositionTimer.stop();
    }
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
{
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

NavigationController *AppFacade::navigation()
{
    return &m_navigation;
}

QString AppFacade::backendContractSummary() const
{
    return QStringLiteral("Foundation only: QML-visible facade owns compact controller anchors; no backend, storage, network, or scan implementation.");
}

}
