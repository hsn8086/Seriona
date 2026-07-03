#pragma once

#include "library_model.h"
#include "lyrics_model.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

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

struct CurrentTrackViewState {
    QString trackId;
    QString nodeId;
    QString title = QStringLiteral("No song selected");
    QString artist = QStringLiteral("Unknown Artist");
    QString album = QStringLiteral("Unknown Album");
    QString artworkPath;
    qreal durationSeconds = 0.0;
    QString audioFormat;
    int audioSampleRate = 0;
    int audioBitDepth = 0;
    int audioChannels = 0;
};

class PlaybackController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(QString capability READ capability NOTIFY capabilityChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying WRITE setPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(qreal currentPosition READ currentPosition WRITE setCurrentPosition NOTIFY currentPositionChanged)
    Q_PROPERTY(qreal totalDuration READ totalDuration WRITE setTotalDuration NOTIFY totalDurationChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool isShuffle READ isShuffle WRITE setShuffle NOTIFY isShuffleChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(QString songTitle READ songTitle NOTIFY currentSongChanged)
    Q_PROPERTY(QString artistName READ artistName NOTIFY currentSongChanged)
    Q_PROPERTY(QString albumName READ albumName NOTIFY currentSongChanged)
    Q_PROPERTY(QString currentTrackId READ currentTrackId NOTIFY currentSongChanged)
    Q_PROPERTY(QString currentTrackNodeId READ currentTrackNodeId NOTIFY currentSongChanged)
    Q_PROPERTY(QString coverArtworkPath READ coverArtworkPath NOTIFY currentSongChanged)
    Q_PROPERTY(QString coverArtworkSource READ coverArtworkSource NOTIFY currentSongChanged)
    Q_PROPERTY(QString coverPlaceholderText READ coverPlaceholderText NOTIFY currentSongChanged)
    Q_PROPERTY(qreal currentTrackDuration READ currentTrackDuration NOTIFY currentSongChanged)
    Q_PROPERTY(QString audioFormat READ audioFormat NOTIFY currentSongChanged)
    Q_PROPERTY(int audioSampleRate READ audioSampleRate NOTIFY currentSongChanged)
    Q_PROPERTY(int audioBitDepth READ audioBitDepth NOTIFY currentSongChanged)
    Q_PROPERTY(int audioChannels READ audioChannels NOTIFY currentSongChanged)
    Q_PROPERTY(QString currentPositionText READ currentPositionText NOTIFY durationDisplayChanged)
    Q_PROPERTY(QString totalDurationText READ totalDurationText NOTIFY durationDisplayChanged)
    Q_PROPERTY(QString remainingDurationText READ remainingDurationText NOTIFY durationDisplayChanged)
    Q_PROPERTY(QVariantList waveformHeights READ waveformHeights NOTIFY waveformHeightsChanged)
    Q_PROPERTY(int waveformBarWidth READ waveformBarWidth NOTIFY waveformBarWidthChanged)
    QML_ELEMENT
    QML_UNCREATABLE("PlaybackController is owned by AppFacade")

public:
#if SERIONA_HAS_BACKEND
    using CommandExecutor = std::function<seriona::control::MediaControllerCommandResult(const seriona::control::MediaControlCommand &)>;
#endif

    explicit PlaybackController(QObject *parent = nullptr);

    bool ready() const;
    QString capability() const;
    bool isPlaying() const;
    void setPlaying(bool playing);
    qreal currentPosition() const;
    void setCurrentPosition(qreal position);
    qreal totalDuration() const;
    void setTotalDuration(qreal duration);
    qreal volume() const;
    void setVolume(qreal volume);
    bool isShuffle() const;
    void setShuffle(bool shuffle);
    int repeatMode() const;
    void setRepeatMode(int repeatMode);
    QString songTitle() const;
    QString artistName() const;
    QString albumName() const;
    QString currentTrackId() const;
    QString currentTrackNodeId() const;
    QString coverArtworkPath() const;
    QString coverArtworkSource() const;
    QString coverPlaceholderText() const;
    qreal currentTrackDuration() const;
    QString audioFormat() const;
    int audioSampleRate() const;
    int audioBitDepth() const;
    int audioChannels() const;
    QString currentPositionText() const;
    QString totalDurationText() const;
    QString remainingDurationText() const;
    QVariantList waveformHeights() const;
    int waveformBarWidth() const;
    void applyWaveform(const QVariantList &heights, int barWidth);

#if SERIONA_HAS_BACKEND
    void setCommandExecutor(CommandExecutor executor);
    void applyPlayerStateSnapshot(
        const seriona::control::PlayerStateSnapshot &snapshot,
        const seriona::control::LibraryStateSnapshot *library = nullptr);
#endif

    // future backend hook: expose playback status, transport commands, seek, volume, and mode control.
    Q_INVOKABLE QString describeBackendHook() const;
    // future backend hook: start playback through the audio engine.
    Q_INVOKABLE void play();
    // future backend hook: pause playback through the audio engine.
    Q_INVOKABLE void pause();
    // future backend hook: toggle transport state through the audio engine.
    Q_INVOKABLE void togglePlay();
    // future backend hook: request an absolute seek position from the audio engine.
    Q_INVOKABLE void seek(qreal position);
    // future backend hook: persist and apply shuffle mode through playback settings.
    Q_INVOKABLE void toggleShuffle();
    // future backend hook: persist and apply repeat mode through playback settings.
    Q_INVOKABLE void cycleRepeatMode();
    Q_INVOKABLE void skipPrevious();
    Q_INVOKABLE void skipNext();
    Q_INVOKABLE void setMuted(bool muted);

signals:
    void capabilityChanged();
    void isPlayingChanged();
    void currentPositionChanged();
    void totalDurationChanged();
    void volumeChanged();
    void isShuffleChanged();
    void repeatModeChanged();
    void currentSongChanged();
    void durationDisplayChanged();
    void waveformHeightsChanged();
    void waveformBarWidthChanged();

private:
    static qreal clamp(qreal value, qreal minimum, qreal maximum);
    static QString formatDuration(qreal seconds);
    void applyPlaying(bool playing);
    void applyCurrentPosition(qreal position);
    void applyTotalDuration(qreal duration);
    void applyVolume(qreal volume);
    void applyShuffle(bool shuffle);
    void applyRepeatMode(int repeatMode);
    void setCurrentTrackViewState(const CurrentTrackViewState &state);
    void setCapability(const QString &capability);

#if SERIONA_HAS_BACKEND
    static seriona::control::RepeatMode repeatModeForIndex(int repeatMode);
    void submitCommand(const seriona::control::MediaControlCommand &command);
    void applyTimelineSnapshot(const seriona::control::PlayerStateSnapshot &snapshot);
    bool updateSmoothedTimelinePosition();
    void stopTimelineSmoothing();

    CommandExecutor m_commandExecutor;
    QTimer m_timelineTimer;
    std::chrono::milliseconds m_timelineSnapshotPosition{0};
    std::optional<std::chrono::milliseconds> m_timelineSnapshotDuration;
    std::chrono::steady_clock::time_point m_timelineSnapshotSampledAt{};
    std::uint64_t m_timelineSnapshotVersion = 0;
#endif

    bool m_isPlaying = false;
    qreal m_currentPosition = 0.0;
    qreal m_totalDuration = 0.0;
    qreal m_volume = 1.0;
    bool m_isShuffle = false;
    int m_repeatMode = 0;
    QString m_capability = QStringLiteral("none");
    CurrentTrackViewState m_currentTrack;
    QString m_coverPlaceholderText = QStringLiteral("🎵");
    QVariantList m_waveformHeights = {
        20, 30, 40, 35, 25, 15, 10, 20, 30, 45, 50, 40, 30, 20, 15, 25, 35, 40, 30, 20,
        15, 10, 20, 35, 45, 40, 30, 25, 35, 45, 50, 40, 30, 20, 15, 25, 35, 40, 30, 20,
        15, 10, 20, 35, 45, 40, 30, 25, 35, 45, 50, 40, 30, 20, 15, 25, 35, 40, 30, 20
    };
    int m_waveformBarWidth = 3;
};

class NavigationController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(QString capability READ capability CONSTANT)
    Q_PROPERTY(QString currentView READ currentView NOTIFY currentViewChanged)
    Q_PROPERTY(bool sidebarOpen READ sidebarOpen NOTIFY sidebarOpenChanged)
    Q_PROPERTY(bool startupScreenVisible READ startupScreenVisible NOTIFY startupScreenVisibleChanged)
    Q_PROPERTY(bool manualSidebarToggle READ manualSidebarToggle NOTIFY manualSidebarToggleChanged)
    QML_ELEMENT
    QML_UNCREATABLE("NavigationController is owned by AppFacade")

public:
    explicit NavigationController(QObject *parent = nullptr);

    bool ready() const;
    QString capability() const;
    QString currentView() const;
    bool sidebarOpen() const;
    bool startupScreenVisible() const;
    bool manualSidebarToggle() const;

    // future backend hook: connect startup restoration, navigation state, and library entry intents.
    Q_INVOKABLE QString describeBackendHook() const;
    // future backend hook: restore persisted startup state and enter the main shell.
    Q_INVOKABLE void restorePlaylistFromStartup();
    // future backend hook: route add-folder startup intent to the future library importer.
    Q_INVOKABLE void addFolderFromStartup();
    // future backend hook: switch to the playback view from restored navigation state.
    Q_INVOKABLE void showPlaybackView();
    // future backend hook: switch to the lyrics view from restored navigation state.
    Q_INVOKABLE void showLyricsView();
    // future backend hook: apply user sidebar toggle intent and persist shell preference.
    Q_INVOKABLE void toggleSidebar();
    // future backend hook: close sidebar from shell or overlay interaction.
    Q_INVOKABLE void closeSidebar();
    Q_INVOKABLE void clearManualSidebarToggle();
    Q_INVOKABLE void syncSidebarForDockCapability(bool dockCapable);

signals:
    void currentViewChanged();
    void sidebarOpenChanged();
    void startupScreenVisibleChanged();
    void manualSidebarToggleChanged();

private:
    void setCurrentView(const QString &view);
    void setSidebarOpen(bool open);
    void setStartupScreenVisible(bool visible);
    void setManualSidebarToggle(bool manual);
    void enterMainShell();

    QString m_currentView = QStringLiteral("playback");
    bool m_sidebarOpen = false;
    bool m_startupScreenVisible = true;
    bool m_manualSidebarToggle = false;
};

class AppFacade : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString layerName READ layerName CONSTANT)
    Q_PROPERTY(bool foundationReady READ foundationReady CONSTANT)
    Q_PROPERTY(PlaybackController *playback READ playback CONSTANT)
    Q_PROPERTY(LibraryController *library READ library CONSTANT)
    Q_PROPERTY(LyricsModel *lyrics READ lyrics CONSTANT)
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

    Q_INVOKABLE QString backendContractSummary() const;
    Q_INVOKABLE bool scanLibrary(const QUrl &rootUrl);
    Q_INVOKABLE bool restorePlaylistFromStartup();

private:
#if SERIONA_HAS_BACKEND
    void requestWaveformForSnapshots(
        const seriona::control::PlayerStateSnapshot &player,
        const seriona::control::LibraryStateSnapshot &library);
    void handlePlayerSnapshotChanged(
        const seriona::control::PlayerStateSnapshot &player,
        const seriona::control::LibraryStateSnapshot &library);
    void handleLibrarySnapshotChanged(
        const seriona::control::PlayerStateSnapshot &player,
        const seriona::control::LibraryStateSnapshot &library);
    void syncLibraryPlayingTrackId(
        const seriona::control::PlayerStateSnapshot &player,
        bool forceReapply);
#endif

    PlaybackController m_playback;
    LibraryController m_library;
    LyricsModel m_lyrics;
    NavigationController m_navigation;
    std::unique_ptr<BackendBridge> m_backendBridge;
#if SERIONA_HAS_BACKEND
    std::unique_ptr<WaveformProvider> m_waveformProvider;
    QString m_currentWaveformCacheKey;
#endif
};

}
