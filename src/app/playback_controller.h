#pragma once

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#include "backend_snapshot_mapper.h"
#include "artwork_palette_worker.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>

#if SERIONA_HAS_BACKEND
#include "seriona/control/control_contracts.h"
#endif

namespace Seriona::App {

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
    Q_PROPERTY(QString coverThumbnailSource READ coverThumbnailSource NOTIFY currentSongChanged)
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
    Q_PROPERTY(QString gradientColor0 READ gradientColor0 NOTIFY gradientColorsChanged)
    Q_PROPERTY(QString gradientColor1 READ gradientColor1 NOTIFY gradientColorsChanged)
    Q_PROPERTY(QString gradientColor2 READ gradientColor2 NOTIFY gradientColorsChanged)
    Q_PROPERTY(QVariantList queueEntries READ queueEntries NOTIFY queueEntriesChanged)
    QML_ELEMENT
    QML_UNCREATABLE("PlaybackController is owned by AppFacade")

public:
    using TrackStartedHandler = std::function<void(const QString &trackId)>;

#if SERIONA_HAS_BACKEND
    using CommandExecutor = std::function<seriona::control::MediaControllerCommandResult(const seriona::control::MediaControlCommand &)>;
#endif

    explicit PlaybackController(QObject *parent = nullptr);
    explicit PlaybackController(ArtworkPaletteWorker::Decoder decoder, QObject *parent = nullptr);
    ~PlaybackController() override;

    // 轨道切换（新曲目开始播放）回调：AppFacade 接 TrackStatsController::recordPlayback，
    // 作为播放次数自增的计数点（T16）。空实现默认无副作用。
    void setTrackStartedHandler(TrackStartedHandler handler);

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
    QString coverThumbnailSource() const;
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
    QString gradientColor0() const;
    QString gradientColor1() const;
    QString gradientColor2() const;
    QVariantList queueEntries() const;

#if SERIONA_HAS_BACKEND
    void setCommandExecutor(CommandExecutor executor);
    void applyPlayerStateSnapshot(
        const seriona::control::PlayerStateSnapshot &snapshot,
        const seriona::control::LibraryStateSnapshot *library = nullptr);
#endif

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void seek(qreal position);
    Q_INVOKABLE void toggleShuffle();
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
    void gradientColorsChanged();
    void queueEntriesChanged();

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
    void applyGradientPalette(const GradientPalette &palette);
    void applyPaletteResult(quint64 generation, const QString &color0, const QString &color1, const QString &color2);
    void applyQueueEntries(const QVariantList &entries);

#if SERIONA_HAS_BACKEND
    static seriona::control::RepeatMode repeatModeForIndex(int repeatMode);
    void submitCommand(const seriona::control::MediaControlCommand &command);
    void applyTimelineSnapshot(const TimelineSnapshotViewState &snapshot);
    bool updateSmoothedTimelinePosition();
    void stopTimelineSmoothing();

    CommandExecutor m_commandExecutor;
    QTimer m_timelineTimer;
    std::chrono::milliseconds m_timelineSnapshotPosition{0};
    std::optional<std::chrono::milliseconds> m_timelineSnapshotDuration;
    std::chrono::steady_clock::time_point m_timelineSnapshotSampledAt{};
    std::uint64_t m_timelineSnapshotVersion = 0;
#endif

    TrackStartedHandler m_trackStartedHandler;
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
    ArtworkPaletteWorker m_paletteWorker{decodeGradientPalette};
    // Generation of the most recent palette request; paletteReady deliveries
    // for any older generation are stale (a track switch happened while the
    // result was in flight) and must be dropped.
    quint64 m_lastPaletteRequestGeneration = 0;
    QString m_gradientColor0 = QStringLiteral("#2d2d2d");
    QString m_gradientColor1 = QStringLiteral("#1a1a1a");
    QString m_gradientColor2 = QStringLiteral("#121212");
    QVariantList m_queueEntries;
};

}
