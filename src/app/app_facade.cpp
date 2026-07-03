#include "app_facade.h"

#include "backend_bridge.h"

#if SERIONA_HAS_BACKEND
#include "waveform_provider.h"

#include "seriona/control/control_contracts.h"
#endif

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QtMath>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>
#include <utility>

namespace Seriona::App {

namespace {

constexpr auto kBackendBridgeAutostartProperty = "seriona.backendBridgeAutostartEnabled";
constexpr auto kSettingsFileProperty = "seriona.settingsFileForTests";
constexpr auto kLastLibraryRootKey = "library/lastScanRoot";

bool backendBridgeAutostartEnabled()
{
    const QCoreApplication *application = QCoreApplication::instance();
    if (!application) {
        return true;
    }

    const QVariant configured = application->property(kBackendBridgeAutostartProperty);
    return configured.isValid() ? configured.toBool() : true;
}

QSettings applicationSettings()
{
    const QCoreApplication *application = QCoreApplication::instance();
    if (application) {
        const QString settingsFile = application->property(kSettingsFileProperty).toString();
        if (!settingsFile.isEmpty()) {
            return QSettings(settingsFile, QSettings::IniFormat);
        }
    }

    return QSettings(QStringLiteral("Seriona"), QStringLiteral("Seriona"));
}

QString savedLibraryRootPath()
{
    QSettings settings = applicationSettings();
    return settings.value(QString::fromUtf8(kLastLibraryRootKey)).toString();
}

void persistLibraryRootPath(const QString &rootPath)
{
    QSettings settings = applicationSettings();
    settings.setValue(QString::fromUtf8(kLastLibraryRootKey), rootPath);
    settings.sync();
}

void clearLibraryRootPath()
{
    QSettings settings = applicationSettings();
    settings.remove(QString::fromUtf8(kLastLibraryRootKey));
    settings.sync();
}

#if SERIONA_HAS_BACKEND
constexpr int kTimelineSmoothingIntervalMs = 100;

QString fromBackendString(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString fromBackendPath(const std::filesystem::path &path)
{
    return QString::fromStdString(path.string());
}

qreal secondsFromMilliseconds(std::chrono::milliseconds value);

struct SongLookupResult {
    const seriona::scanner::SongMetadata *song = nullptr;
    QString nodeId;
};

SongLookupResult findSongByTrackId(const seriona::control::LibraryStateSnapshot *library, const std::string &trackId)
{
    if (trackId.empty() || library == nullptr || !library->libraryTree) {
        return {};
    }

    for (const seriona::scanner::PlaylistNode &node : library->libraryTree->nodes) {
        if (node.song && node.song->trackId == trackId) {
            return SongLookupResult{&(*node.song), fromBackendString(node.nodeId)};
        }
    }
    return {};
}

#ifndef QT_NO_DEBUG
SongLookupResult findSongByDebugPath(
    const seriona::control::LibraryStateSnapshot *library,
    const std::filesystem::path &filePath)
{
    if (filePath.empty() || library == nullptr || !library->libraryTree) {
        return {};
    }

    for (const seriona::scanner::PlaylistNode &node : library->libraryTree->nodes) {
        if (!node.song) {
            continue;
        }
        if (node.song->filePath == filePath || node.song->sourceFilePath == filePath) {
            return SongLookupResult{&(*node.song), fromBackendString(node.nodeId)};
        }
    }
    return {};
}
#endif

SongLookupResult findCurrentSong(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::control::LibraryStateSnapshot *library)
{
    if (!snapshot.currentTrack) {
        return {};
    }

    const seriona::control::TrackIdentity &track = *snapshot.currentTrack;
    SongLookupResult result = findSongByTrackId(library, track.trackId);
    if (result.song != nullptr) {
        return result;
    }

#ifndef QT_NO_DEBUG
    if (track.trackId.empty()) {
        result = findSongByDebugPath(library, track.filePath);
        if (result.song != nullptr) {
            qWarning().noquote() << QStringLiteral("Current track metadata used debug filePath fallback because trackId is empty");
        }
    }
#endif
    return result;
}

QString titleFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (snapshot.display && !snapshot.display->title.empty()) {
        return fromBackendString(snapshot.display->title);
    }
    if (song != nullptr && !song->title.empty()) {
        return fromBackendString(song->title);
    }
    if (snapshot.currentTrack && !snapshot.currentTrack->trackId.empty()) {
        return fromBackendString(snapshot.currentTrack->trackId);
    }
    return QStringLiteral("No song selected");
}

QString artistFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (snapshot.display && !snapshot.display->artist.empty()) {
        return fromBackendString(snapshot.display->artist);
    }
    if (snapshot.display && !snapshot.display->albumArtist.empty()) {
        return fromBackendString(snapshot.display->albumArtist);
    }
    if (song != nullptr && !song->artist.empty()) {
        return fromBackendString(song->artist);
    }
    if (song != nullptr && !song->albumArtist.empty()) {
        return fromBackendString(song->albumArtist);
    }
    return QStringLiteral("Unknown Artist");
}

QString albumFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (snapshot.display && !snapshot.display->album.empty()) {
        return fromBackendString(snapshot.display->album);
    }
    if (song != nullptr && !song->album.empty()) {
        return fromBackendString(song->album);
    }
    return QStringLiteral("Unknown Album");
}

QString artworkPathFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (snapshot.artwork && snapshot.artwork->localPath && !snapshot.artwork->localPath->empty()) {
        return fromBackendPath(*snapshot.artwork->localPath);
    }
    if (song != nullptr && song->artworkPath && !song->artworkPath->empty()) {
        return fromBackendPath(*song->artworkPath);
    }
    return {};
}

std::optional<std::chrono::milliseconds> durationFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (song != nullptr && song->duration) {
        return song->duration;
    }
    return snapshot.timeline.duration;
}

std::filesystem::path audioPathFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (song != nullptr) {
        return song->sourceFilePath.empty() ? song->filePath : song->sourceFilePath;
    }
    if (snapshot.currentTrack) {
        return snapshot.currentTrack->filePath;
    }
    return {};
}

QString audioFormatFromPath(const std::filesystem::path &path)
{
    QString extension = fromBackendPath(path.extension());
    if (extension.startsWith(QLatin1Char('.'))) {
        extension.remove(0, 1);
    }
    return extension.toUpper();
}

bool sameCurrentTrackViewState(const CurrentTrackViewState &left, const CurrentTrackViewState &right)
{
    return left.trackId == right.trackId
        && left.nodeId == right.nodeId
        && left.title == right.title
        && left.artist == right.artist
        && left.album == right.album
        && left.artworkPath == right.artworkPath
        && qAbs(left.durationSeconds - right.durationSeconds) < 0.001
        && left.audioFormat == right.audioFormat
        && left.audioSampleRate == right.audioSampleRate
        && left.audioBitDepth == right.audioBitDepth
        && left.audioChannels == right.audioChannels;
}

CurrentTrackViewState currentTrackViewStateFromSnapshots(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::control::LibraryStateSnapshot *library)
{
    CurrentTrackViewState state;
    const SongLookupResult lookup = findCurrentSong(snapshot, library);
    const seriona::scanner::SongMetadata *song = lookup.song;

    if (snapshot.currentTrack) {
        state.trackId = fromBackendString(snapshot.currentTrack->trackId);
    }
    state.nodeId = lookup.nodeId;
    state.title = titleFromStateSource(snapshot, song);
    state.artist = artistFromStateSource(snapshot, song);
    state.album = albumFromStateSource(snapshot, song);
    state.artworkPath = artworkPathFromStateSource(snapshot, song);

    const std::optional<std::chrono::milliseconds> duration = durationFromStateSource(snapshot, song);
    state.durationSeconds = duration ? secondsFromMilliseconds(*duration) : 0.0;

    state.audioFormat = audioFormatFromPath(audioPathFromStateSource(snapshot, song));
    if (song != nullptr) {
        state.audioSampleRate = song->sampleRate ? static_cast<int>(*song->sampleRate) : 0;
        state.audioBitDepth = song->bitDepth ? static_cast<int>(*song->bitDepth) : 0;
        state.audioChannels = song->channels ? static_cast<int>(*song->channels) : 0;
    }
    return state;
}

qreal secondsFromMilliseconds(std::chrono::milliseconds value)
{
    return static_cast<qreal>(value.count()) / 1000.0;
}

std::optional<std::chrono::milliseconds> millisecondsFromSeconds(qreal seconds)
{
    if (!std::isfinite(seconds)) {
        return std::nullopt;
    }

    return std::chrono::milliseconds{qRound64(qMax(0.0, seconds) * 1000.0)};
}

std::optional<float> volumeFromQml(qreal volume)
{
    if (!std::isfinite(volume)) {
        return std::nullopt;
    }

    return static_cast<float>(qBound(0.0, volume, 1.0));
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

seriona::control::RepeatMode repeatModeFromIndex(int repeatMode)
{
    switch (qBound(0, repeatMode, 2)) {
    case 1:
        return seriona::control::RepeatMode::All;
    case 2:
        return seriona::control::RepeatMode::One;
    case 0:
    default:
        return seriona::control::RepeatMode::Off;
    }
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

QString playingTrackIdFromSnapshot(const seriona::control::PlayerStateSnapshot &player)
{
    return player.currentTrack.has_value()
        ? fromBackendString(player.currentTrack->trackId)
        : QString();
}

qreal durationSecondsFromTimeline(const seriona::control::PlaybackTimeline &timeline)
{
    return timeline.duration ? secondsFromMilliseconds(*timeline.duration) : 0.0;
}

bool shouldSmoothTimeline(seriona::control::PlaybackStatus status)
{
    return status == seriona::control::PlaybackStatus::Playing;
}

std::chrono::milliseconds elapsedSince(std::chrono::steady_clock::time_point sampledAt)
{
    if (sampledAt == std::chrono::steady_clock::time_point{}) {
        return std::chrono::milliseconds{0};
    }

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (now <= sampledAt) {
        return std::chrono::milliseconds{0};
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(now - sampledAt);
}
#endif

}

PlaybackController::PlaybackController(QObject *parent)
    : QObject(parent)
{
#if SERIONA_HAS_BACKEND
    m_timelineTimer.setInterval(kTimelineSmoothingIntervalMs);
    m_timelineTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_timelineTimer, &QTimer::timeout, this, [this] {
        static_cast<void>(updateSmoothedTimelinePosition());
    });
#endif
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
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = playing ? seriona::control::MediaControlCommandKind::Play : seriona::control::MediaControlCommandKind::Pause;
    submitCommand(command);
#else
    Q_UNUSED(playing)
#endif
}

void PlaybackController::applyPlaying(bool playing)
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
    seek(position);
}

void PlaybackController::applyCurrentPosition(qreal position)
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
    applyTotalDuration(duration);
}

void PlaybackController::applyTotalDuration(qreal duration)
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
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::SetVolume;
    command.volume = volumeFromQml(volume);
    submitCommand(command);
#else
    Q_UNUSED(volume)
#endif
}

void PlaybackController::applyVolume(qreal volume)
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
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::SetShuffle;
    command.shuffle = shuffle;
    submitCommand(command);
#else
    Q_UNUSED(shuffle)
#endif
}

void PlaybackController::applyShuffle(bool shuffle)
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
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::SetRepeatMode;
    command.repeatMode = repeatModeForIndex(repeatMode);
    submitCommand(command);
#else
    Q_UNUSED(repeatMode)
#endif
}

void PlaybackController::applyRepeatMode(int repeatMode)
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
    return m_currentTrack.title;
}

QString PlaybackController::artistName() const
{
    return m_currentTrack.artist;
}

QString PlaybackController::albumName() const
{
    return m_currentTrack.album;
}

QString PlaybackController::currentTrackId() const
{
    return m_currentTrack.trackId;
}

QString PlaybackController::currentTrackNodeId() const
{
    return m_currentTrack.nodeId;
}

QString PlaybackController::coverArtworkPath() const
{
    return m_currentTrack.artworkPath;
}

QString PlaybackController::coverArtworkSource() const
{
    return m_currentTrack.artworkPath.isEmpty()
        ? QString()
        : QUrl::fromLocalFile(m_currentTrack.artworkPath).toString();
}

QString PlaybackController::coverPlaceholderText() const
{
    return m_coverPlaceholderText;
}

qreal PlaybackController::currentTrackDuration() const
{
    return m_currentTrack.durationSeconds;
}

QString PlaybackController::audioFormat() const
{
    return m_currentTrack.audioFormat;
}

int PlaybackController::audioSampleRate() const
{
    return m_currentTrack.audioSampleRate;
}

int PlaybackController::audioBitDepth() const
{
    return m_currentTrack.audioBitDepth;
}

int PlaybackController::audioChannels() const
{
    return m_currentTrack.audioChannels;
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

int PlaybackController::waveformBarWidth() const
{
    return m_waveformBarWidth;
}

void PlaybackController::applyWaveform(const QVariantList &heights, int barWidth)
{
    barWidth = qMax(0, barWidth);
    const bool heightsChanged = m_waveformHeights != heights;
    const bool barWidthChanged = m_waveformBarWidth != barWidth;

    if (!heightsChanged && !barWidthChanged) {
        return;
    }

    m_waveformHeights = heights;
    m_waveformBarWidth = barWidth;

    if (heightsChanged) {
        emit waveformHeightsChanged();
    }
    if (barWidthChanged) {
        emit waveformBarWidthChanged();
    }
}

#if SERIONA_HAS_BACKEND
void PlaybackController::setCommandExecutor(CommandExecutor executor)
{
    m_commandExecutor = std::move(executor);
}

void PlaybackController::applyPlayerStateSnapshot(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::control::LibraryStateSnapshot *library)
{
    const CurrentTrackViewState currentTrack = currentTrackViewStateFromSnapshots(snapshot, library);
    const qreal effectiveVolume = snapshot.muted ? 0.0 : static_cast<qreal>(snapshot.volume);

    applyPlaying(snapshot.playback.state == seriona::control::PlaybackStatus::Playing);
    applyTimelineSnapshot(snapshot);
    applyVolume(effectiveVolume);
    applyShuffle(snapshot.shuffle);
    applyRepeatMode(repeatModeFromSnapshot(snapshot.repeatMode));
    setCurrentTrackViewState(currentTrack);
    setCapability(capabilityFromSnapshot(snapshot.capabilities));
}

void PlaybackController::applyTimelineSnapshot(const seriona::control::PlayerStateSnapshot &snapshot)
{
    m_timelineSnapshotPosition = snapshot.timeline.position;
    m_timelineSnapshotDuration = snapshot.timeline.duration;
    m_timelineSnapshotSampledAt = snapshot.freshness.sampledAt;
    m_timelineSnapshotVersion = snapshot.freshness.version;

    applyTotalDuration(durationSecondsFromTimeline(snapshot.timeline));
    if (shouldSmoothTimeline(snapshot.playback.state)) {
        const bool reachedEnd = updateSmoothedTimelinePosition();
        if (!reachedEnd && !m_timelineTimer.isActive()) {
            m_timelineTimer.start();
        }
        return;
    }

    stopTimelineSmoothing();
    applyCurrentPosition(secondsFromMilliseconds(m_timelineSnapshotPosition));
}

bool PlaybackController::updateSmoothedTimelinePosition()
{
    std::chrono::milliseconds position = m_timelineSnapshotPosition + elapsedSince(m_timelineSnapshotSampledAt);
    bool reachedEnd = false;
    if (m_timelineSnapshotDuration && position >= *m_timelineSnapshotDuration) {
        position = *m_timelineSnapshotDuration;
        reachedEnd = true;
    }

    applyCurrentPosition(secondsFromMilliseconds(position));
    if (reachedEnd) {
        stopTimelineSmoothing();
    }
    return reachedEnd;
}

void PlaybackController::stopTimelineSmoothing()
{
    if (m_timelineTimer.isActive()) {
        m_timelineTimer.stop();
    }
}
#endif

QString PlaybackController::describeBackendHook() const
{
    return QStringLiteral("Future backend hook: playback status, seek, volume, mode, and track commands.");
}

void PlaybackController::play()
{
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::Play;
    submitCommand(command);
#endif
}

void PlaybackController::pause()
{
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::Pause;
    submitCommand(command);
#endif
}

void PlaybackController::togglePlay()
{
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::TogglePlayPause;
    submitCommand(command);
#endif
}

void PlaybackController::seek(qreal position)
{
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::SeekTo;
    command.position = millisecondsFromSeconds(position);
    submitCommand(command);
#else
    Q_UNUSED(position)
#endif
}

void PlaybackController::toggleShuffle()
{
    setShuffle(!m_isShuffle);
}

void PlaybackController::cycleRepeatMode()
{
    setRepeatMode((m_repeatMode + 1) % 3);
}

void PlaybackController::skipPrevious()
{
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::SkipPrevious;
    submitCommand(command);
#endif
}

void PlaybackController::skipNext()
{
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::SkipNext;
    submitCommand(command);
#endif
}

void PlaybackController::setMuted(bool muted)
{
#if SERIONA_HAS_BACKEND
    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::SetMuted;
    command.muted = muted;
    submitCommand(command);
#else
    Q_UNUSED(muted)
#endif
}

#if SERIONA_HAS_BACKEND
seriona::control::RepeatMode PlaybackController::repeatModeForIndex(int repeatMode)
{
    return repeatModeFromIndex(repeatMode);
}

void PlaybackController::submitCommand(const seriona::control::MediaControlCommand &command)
{
    if (!m_commandExecutor) {
        return;
    }

    static_cast<void>(m_commandExecutor(command));
}
#endif

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

void PlaybackController::setCurrentTrackViewState(const CurrentTrackViewState &state)
{
    if (sameCurrentTrackViewState(m_currentTrack, state)) {
        return;
    }

    m_currentTrack = state;
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

#if SERIONA_HAS_BACKEND
void AppFacade::requestWaveformForSnapshots(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library)
{
    std::optional<WaveformRequest> request = makeWaveformRequest(player, library, defaultWaveformParameters());
    if (!request) {
        m_currentWaveformCacheKey.clear();
        m_waveformProvider->cancelPending();
        m_playback.applyWaveform(QVariantList{}, 0);
        return;
    }

    const QString cacheKey = request->cacheKey();
    if (cacheKey == m_currentWaveformCacheKey) {
        return;
    }

    m_currentWaveformCacheKey = cacheKey;
    static_cast<void>(m_waveformProvider->requestWaveform(std::move(*request)));
}

void AppFacade::handlePlayerSnapshotChanged(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library)
{
    m_playback.applyPlayerStateSnapshot(player, &library);
    m_lyrics.applyPlayerStateSnapshot(player, &library);
    syncLibraryPlayingTrackId(player, false);
    requestWaveformForSnapshots(player, library);
}

void AppFacade::handleLibrarySnapshotChanged(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library)
{
    m_library.applyLibraryStateSnapshot(library);
    if (library.libraryTree.has_value()) {
        m_library.setPlaylistTreeSnapshot(*library.libraryTree);
    }
    m_playback.applyPlayerStateSnapshot(player, &library);
    m_lyrics.applyPlayerStateSnapshot(player, &library);
    syncLibraryPlayingTrackId(player, true);
    requestWaveformForSnapshots(player, library);
}

void AppFacade::syncLibraryPlayingTrackId(
    const seriona::control::PlayerStateSnapshot &player,
    bool forceReapply)
{
    const QString trackId = playingTrackIdFromSnapshot(player);
    if (forceReapply && !trackId.isEmpty() && m_library.playingTrackId() == trackId) {
        m_library.setPlayingTrackId(QString());
    }
    m_library.setPlayingTrackId(trackId);
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
        if (result.cacheKey != m_currentWaveformCacheKey) {
            return;
        }
        m_playback.applyWaveform(result.heights, result.barWidth);
    });
    connect(m_waveformProvider.get(), &WaveformProvider::waveformFailed, this, [this](const WaveformResult &result) {
        if (result.cacheKey != m_currentWaveformCacheKey) {
            return;
        }
        qWarning().noquote() << QStringLiteral("Waveform generation failed:") << result.errorMessage;
        m_playback.applyWaveform(QVariantList{}, 0);
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
    if (!m_library.scanLibrary(rootUrl)) {
        return false;
    }

    const QString rootPath = m_library.savedRootPath();
    if (!rootPath.isEmpty()) {
        persistLibraryRootPath(rootPath);
    }
    return true;
}

bool AppFacade::restorePlaylistFromStartup()
{
    const QString rootPath = savedLibraryRootPath();
    if (rootPath.isEmpty()) {
        clearLibraryRootPath();
        m_library.clearSavedRootPath(tr("请先添加音乐文件夹"));
        return false;
    }

    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.isDir()) {
        clearLibraryRootPath();
        m_library.clearSavedRootPath(tr("上次曲库文件夹不可用，请重新选择文件夹"));
        return false;
    }

    if (!scanLibrary(QUrl::fromLocalFile(rootInfo.absoluteFilePath()))) {
        return false;
    }

    m_navigation.restorePlaylistFromStartup();
    return true;
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

QString AppFacade::backendContractSummary() const
{
    return QStringLiteral("Foundation plus backend bridge: QML-visible facade owns compact controller anchors while BackendBridge owns MediaController lifecycle.");
}

}
