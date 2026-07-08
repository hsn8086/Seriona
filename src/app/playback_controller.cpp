#include "playback_controller.h"

#if SERIONA_HAS_BACKEND
#include "seriona/control/control_contracts.h"
#endif

#include <QColor>
#include <QImage>
#include <QUrl>
#include <QtMath>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace Seriona::App {

namespace {

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

// ---------------------------------------------------------------------------
// 封面主色调提取（在旧项目 UIController::updateGradientColors 基础上优化）
//
// 旧实现：缩放到 20x20、按 (饱和度*2 + 居中亮度) 打分、用 CIE 近似色距
// 去重取 3 色、最后统一压到 s<=0.4 / l<=0.5。
//
// 本版改进：
//   1. 采样密度提升到 32x32，边缘裁掉 2px 以弱化专辑封面常见的黑边/白框；
//   2. 打分引入「面积权重」——相近颜色累加权重，避免一小撮高饱和噪点
//      压过整体主色；
//   3. 去重阈值随已选数量递减，保证第 2、3 色和主色拉开层次；
//   4. 收敛时按亮度递减排布 color0→color2，渲染出更自然的上亮下暗光晕；
//   5. 空封面回退到 Theme 默认深色三连，杜绝突兀跳色。
// ---------------------------------------------------------------------------
struct ScoredColor {
    QColor color;
    double weight = 0.0;
};

// CIE 近似加权色距（低成本感知距离，来自 old UIController::colorDistance）
double perceptualColorDistance(const QColor &a, const QColor &b)
{
    const long rmean = (static_cast<long>(a.red()) + static_cast<long>(b.red())) / 2;
    const long r = static_cast<long>(a.red()) - static_cast<long>(b.red());
    const long g = static_cast<long>(a.green()) - static_cast<long>(b.green());
    const long bl = static_cast<long>(a.blue()) - static_cast<long>(b.blue());
    return std::sqrt(
        static_cast<double>(((512 + rmean) * r * r) >> 8)
        + 4.0 * static_cast<double>(g * g)
        + static_cast<double>(((767 - rmean) * bl * bl) >> 8));
}

// 把颜色收敛到适合作为深色背景的亮度/饱和度区间，保证前景文字对比度
QColor toBackgroundTone(const QColor &input, qreal targetLightness)
{
    float h = 0.0F;
    float s = 0.0F;
    float l = 0.0F;
    input.getHslF(&h, &s, &l);

    // 饱和度上限 0.5：保留丰富色相，增强视觉冲击
    s = std::min(s, 0.50F);
    // 亮度提升：从 0.08~0.42 提升到 0.12~0.50，使背景更明亮
    l = static_cast<float>(qBound(0.12, targetLightness, 0.50));

    if (h < 0.0F)
        h = 0.0F;
    return QColor::fromHslF(h, s, l);
}

// 从封面图提取三个用于渐变背景的主色调（含默认回退）
std::array<QString, 3> extractGradientPalette(const QString &imagePath)
{
    // 默认深色三连（与 Theme.gradientColor* 保持一致的暗色基调）
    std::array<QString, 3> fallback = {
        QStringLiteral("#4a2c2a"),
        QStringLiteral("#2b1a1a"),
        QStringLiteral("#1a1212"),
    };

    if (imagePath.isEmpty())
        return fallback;

    QImage image(imagePath);
    if (image.isNull())
        return fallback;

    // 缩放到 32x32 平滑采样：兼顾细节与性能；裁边 2px 去掉封面黑边/白框
    constexpr int kSampleSize = 32;
    constexpr int kMargin = 2;
    const QImage small = image.scaled(kSampleSize, kSampleSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    std::vector<ScoredColor> buckets;
    buckets.reserve(64);

    for (int y = kMargin; y < small.height() - kMargin; ++y) {
        for (int x = kMargin; x < small.width() - kMargin; ++x) {
            const QColor pixel = small.pixelColor(x, y);
            // 丢弃接近纯黑/纯白的像素，它们无法提供有效色相
            if (pixel.lightness() < 24 || pixel.lightness() > 236)
                continue;

            // 单像素权重：高饱和 + 居中亮度更可能是封面主色
            const double pixelWeight = (pixel.saturationF() * 2.0)
                + (1.0 - std::abs(pixel.lightnessF() - 0.5));

            // 面积聚合：相近颜色累加权重，抑制孤立噪点
            bool merged = false;
            for (auto &bucket : buckets) {
                if (perceptualColorDistance(pixel, bucket.color) < 40.0) {
                    bucket.weight += pixelWeight;
                    merged = true;
                    break;
                }
            }
            if (!merged)
                buckets.push_back({pixel, pixelWeight});
        }
    }

    if (buckets.empty())
        return fallback;

    // 按累计权重降序：权重最高者为整幅封面的视觉主色
    std::sort(buckets.begin(), buckets.end(), [](const ScoredColor &a, const ScoredColor &b) {
        return a.weight > b.weight;
    });

    // 去重挑选最多 3 个彼此区分度足够的主色；阈值随进度递减，
    // 让后续颜色更宽松，避免单色封面凑不满三色而重复
    std::vector<QColor> chosen;
    const double distanceThresholds[3] = {0.0, 90.0, 60.0};
    for (const auto &bucket : buckets) {
        bool distinct = true;
        for (std::size_t i = 0; i < chosen.size(); ++i) {
            if (perceptualColorDistance(bucket.color, chosen[i]) < distanceThresholds[chosen.size()]) {
                distinct = false;
                break;
            }
        }
        if (distinct) {
            chosen.push_back(bucket.color);
            if (chosen.size() >= 3)
                break;
        }
    }

    // 不足 3 色时用主色的渐暗变体补齐，保持同色系过渡
    while (chosen.size() < 3) {
        if (!chosen.empty())
            chosen.push_back(chosen.back().darker(118));
        else
            return fallback;
    }

    // color0 最亮、color2 最暗：配合三层渐变自上而下的光晕层次
    const qreal targetLightness[3] = {0.46, 0.36, 0.26};
    std::array<QString, 3> palette;
    for (int i = 0; i < 3; ++i)
        palette[i] = toBackgroundTone(chosen[static_cast<std::size_t>(i)], targetLightness[i]).name();

    return palette;
}

#if SERIONA_HAS_BACKEND
constexpr int kTimelineSmoothingIntervalMs = 100;

qreal secondsFromMilliseconds(std::chrono::milliseconds value);

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
    const PlayerSnapshotViewState mapped = mapPlayerSnapshot(snapshot, library);

    applyPlaying(mapped.isPlaying);
    applyTimelineSnapshot(mapped.timeline);
    applyVolume(mapped.volume);
    applyShuffle(mapped.shuffle);
    applyRepeatMode(mapped.repeatMode);
    setCurrentTrackViewState(mapped.currentTrack);
    setCapability(mapped.capability);
}

void PlaybackController::applyTimelineSnapshot(const TimelineSnapshotViewState &snapshot)
{
    m_timelineSnapshotPosition = snapshot.position;
    m_timelineSnapshotDuration = snapshot.duration;
    m_timelineSnapshotSampledAt = snapshot.sampledAt;
    m_timelineSnapshotVersion = snapshot.version;

    applyTotalDuration(snapshot.durationSeconds);
    if (snapshot.smooth) {
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
    updateGradientColors(m_currentTrack.artworkPath);
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

void PlaybackController::updateGradientColors(const QString &imagePath)
{
    const std::array<QString, 3> palette = extractGradientPalette(imagePath);

    if (m_gradientColor0 == palette[0] && m_gradientColor1 == palette[1] && m_gradientColor2 == palette[2]) {
        return;
    }

    m_gradientColor0 = palette[0];
    m_gradientColor1 = palette[1];
    m_gradientColor2 = palette[2];
    emit gradientColorsChanged();
}

QString PlaybackController::gradientColor0() const
{
    return m_gradientColor0;
}

QString PlaybackController::gradientColor1() const
{
    return m_gradientColor1;
}

QString PlaybackController::gradientColor2() const
{
    return m_gradientColor2;
}

}
