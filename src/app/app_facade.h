#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>

#include <memory>

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#if SERIONA_HAS_BACKEND
namespace seriona::control {
struct PlayerStateSnapshot;
}
#endif

namespace Seriona::App {

class BackendBridge;

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
    Q_PROPERTY(QString coverPlaceholderText READ coverPlaceholderText NOTIFY currentSongChanged)
    Q_PROPERTY(QString currentPositionText READ currentPositionText NOTIFY durationDisplayChanged)
    Q_PROPERTY(QString totalDurationText READ totalDurationText NOTIFY durationDisplayChanged)
    Q_PROPERTY(QString remainingDurationText READ remainingDurationText NOTIFY durationDisplayChanged)
    Q_PROPERTY(QVariantList waveformHeights READ waveformHeights NOTIFY waveformHeightsChanged)
    QML_ELEMENT
    QML_UNCREATABLE("PlaybackController is owned by AppFacade")

public:
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
    QString coverPlaceholderText() const;
    QString currentPositionText() const;
    QString totalDurationText() const;
    QString remainingDurationText() const;
    QVariantList waveformHeights() const;

#if SERIONA_HAS_BACKEND
    void applyPlayerStateSnapshot(const seriona::control::PlayerStateSnapshot &snapshot);
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

private:
    static qreal clamp(qreal value, qreal minimum, qreal maximum);
    static QString formatDuration(qreal seconds);
    void setCurrentSong(const QString &title, const QString &artist, const QString &album);
    void setCapability(const QString &capability);

    bool m_isPlaying = false;
    qreal m_currentPosition = 0.0;
    qreal m_totalDuration = 0.0;
    qreal m_volume = 1.0;
    bool m_isShuffle = false;
    int m_repeatMode = 0;
    QString m_capability = QStringLiteral("none");
    QString m_songTitle = QStringLiteral("No song selected");
    QString m_artistName = QStringLiteral("Unknown Artist");
    QString m_albumName = QStringLiteral("Unknown Album");
    QString m_coverPlaceholderText = QStringLiteral("🎵");
    QVariantList m_waveformHeights = {
        20, 30, 40, 35, 25, 15, 10, 20, 30, 45, 50, 40, 30, 20, 15, 25, 35, 40, 30, 20,
        15, 10, 20, 35, 45, 40, 30, 25, 35, 45, 50, 40, 30, 20, 15, 25, 35, 40, 30, 20,
        15, 10, 20, 35, 45, 40, 30, 25, 35, 45, 50, 40, 30, 20, 15, 25, 35, 40, 30, 20
    };
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
    Q_PROPERTY(NavigationController *navigation READ navigation CONSTANT)
    QML_ELEMENT

public:
    explicit AppFacade(QObject *parent = nullptr);
    ~AppFacade() override;

    QString layerName() const;
    bool foundationReady() const;
    PlaybackController *playback();
    NavigationController *navigation();
    bool backendBridgeStartedForTests() const;

    Q_INVOKABLE QString backendContractSummary() const;

private:
    PlaybackController m_playback;
    NavigationController m_navigation;
    std::unique_ptr<BackendBridge> m_backendBridge;
};

}
