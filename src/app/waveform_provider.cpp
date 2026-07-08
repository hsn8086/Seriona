#include "waveform_provider.h"

#if SERIONA_HAS_BACKEND
#include <QDebug>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <utility>
#include <vector>

namespace Seriona::App {

namespace {

QString pathToQString(const std::filesystem::path &path)
{
    return QString::fromStdString(path.string());
}

std::int64_t toMicroseconds(std::chrono::milliseconds value)
{
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(value).count();
    return std::max<std::int64_t>(0, microseconds);
}

const seriona::scanner::SongMetadata *findSongByTrackId(
    const seriona::control::LibraryStateSnapshot &library,
    const std::string &trackId)
{
    if (trackId.empty() || !library.libraryTree) {
        return nullptr;
    }

    for (const seriona::scanner::PlaylistNode &node : library.libraryTree->nodes) {
        if (node.song && node.song->trackId == trackId) {
            return &(*node.song);
        }
    }
    return nullptr;
}

QVariantList heightsToVariantList(const std::vector<int> &heights)
{
    QVariantList values;
    values.reserve(static_cast<qsizetype>(heights.size()));
    for (const int height : heights) {
        values.append(height);
    }
    return values;
}

WaveformResult runGenerator(
    const WaveformProvider::Generator &generator,
    const WaveformRequest &request,
    quint64 requestId,
    const QString &cacheKey)
{
    WaveformResult result;
    result.requestId = requestId;
    result.cacheKey = cacheKey;

    try {
        WaveformBuildInput input;
        input.trackId = request.trackId;
        input.waveformFilePath = request.waveformFilePath;
        input.barCount = request.parameters.barCount;
        input.totalWidth = request.parameters.totalWidth;
        input.maxHeight = request.parameters.maxHeight;
        input.startTimeUS = request.startTimeUS;
        input.endTimeUS = request.endTimeUS;
        input.config = request.parameters.config;

        const WaveformPayload payload = generator(input);
        result.heights = payload.heights;
        result.barWidth = payload.barWidth;
    } catch (const std::exception &error) {
        result.heights = QVariantList{};
        result.barWidth = 0;
        result.errorMessage = QString::fromUtf8(error.what());
    } catch (...) {
        result.heights = QVariantList{};
        result.barWidth = 0;
        result.errorMessage = QStringLiteral("Unknown waveform generation failure");
    }

    return result;
}

}

QString WaveformRequest::cacheKey() const
{
    const seriona::audio::WaveformConfig &config = parameters.config;
    return trackId
        + QStringLiteral("|file=") + waveformFilePath
        + QStringLiteral("|offsetUS=") + QString::number(offsetUS)
        + QStringLiteral("|durationUS=") + QString::number(durationUS)
        + QStringLiteral("|barCount=") + QString::number(parameters.barCount)
        + QStringLiteral("|totalWidth=") + QString::number(parameters.totalWidth)
        + QStringLiteral("|maxHeight=") + QString::number(parameters.maxHeight)
        + QStringLiteral("|dbFloor=") + QString::number(config.dbFloor, 'g', 9)
        + QStringLiteral("|dbCeiling=") + QString::number(config.dbCeiling, 'g', 9)
        + QStringLiteral("|enableSIMD=") + (config.enableSIMD ? QStringLiteral("1") : QStringLiteral("0"))
        + QStringLiteral("|threadCount=") + QString::number(config.threadCount);
}

WaveformProvider::WaveformProvider(QObject *parent)
    : QObject(parent)
    , m_generator(buildWithBackend)
{
    qRegisterMetaType<Seriona::App::WaveformResult>();
    const int idealThreadCount = QThread::idealThreadCount();
    m_threadPool.setMaxThreadCount(std::max(2, idealThreadCount));
}

WaveformProvider::~WaveformProvider()
{
    cancelPending();
    m_threadPool.clear();
    m_threadPool.waitForDone();
}

quint64 WaveformProvider::requestWaveform(WaveformRequest request)
{
    const QString cacheKey = request.cacheKey();
    const quint64 requestId = ++m_latestRequestId;

    const auto cached = m_cache.constFind(cacheKey);
    if (cached != m_cache.constEnd()) {
        WaveformResult result;
        result.requestId = requestId;
        result.cacheKey = cacheKey;
        result.heights = cached->heights;
        result.barWidth = cached->barWidth;
        result.cacheHit = true;

        QMetaObject::invokeMethod(this, [this, result] {
            if (result.requestId == m_latestRequestId) {
                emit waveformReady(result);
            }
        }, Qt::QueuedConnection);
        return requestId;
    }

    auto *watcher = new QFutureWatcher<WaveformResult>(this);
    connect(watcher, &QFutureWatcher<WaveformResult>::finished, this, [this, watcher] {
        const WaveformResult result = watcher->result();
        watcher->deleteLater();

        if (result.requestId != m_latestRequestId) {
            return;
        }

        if (!result.errorMessage.isEmpty()) {
            qWarning().noquote() << QStringLiteral("Waveform generation failed:") << result.errorMessage;
            emit waveformFailed(result);
            emit waveformReady(result);
            return;
        }

        m_cache.insert(result.cacheKey, CachedWaveform{result.heights, result.barWidth});
        emit waveformReady(result);
    });
    watcher->setFuture(QtConcurrent::run(&m_threadPool, [generator = m_generator, request, requestId, cacheKey] {
        return runGenerator(generator, request, requestId, cacheKey);
    }));

    return requestId;
}

void WaveformProvider::requestForSnapshots(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library)
{
    requestForSnapshots(player, library, defaultWaveformParameters());
}

void WaveformProvider::requestForSnapshots(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library,
    const WaveformParameters &parameters)
{
    std::optional<WaveformRequest> request = makeWaveformRequest(player, library, parameters);
    if (!request.has_value()) {
        cancelPending();

        WaveformResult result;
        result.requestId = m_latestRequestId;
        emit waveformReady(result);
        return;
    }

    const QString cacheKey = request->cacheKey();
    if (cacheKey == m_currentCacheKey) {
        return;
    }

    m_currentCacheKey = cacheKey;
    static_cast<void>(requestWaveform(std::move(*request)));
}

void WaveformProvider::cancelPending()
{
    m_currentCacheKey.clear();
    ++m_latestRequestId;
}

void WaveformProvider::setGeneratorForTests(Generator generator)
{
    m_generator = std::move(generator);
    m_cache.clear();
    cancelPending();
}

WaveformPayload WaveformProvider::buildWithBackend(const WaveformBuildInput &input)
{
    int barWidth = 0;
    const std::vector<int> heights = seriona::audio::buildAudioWaveform(
        input.waveformFilePath.toStdString(),
        input.barCount,
        input.totalWidth,
        barWidth,
        input.maxHeight,
        input.startTimeUS,
        input.endTimeUS,
        input.config);
    return WaveformPayload{heightsToVariantList(heights), barWidth};
}

WaveformParameters defaultWaveformParameters()
{
    WaveformParameters parameters;
    parameters.barCount = 60;
    parameters.totalWidth = 320;
    parameters.maxHeight = 68;
    parameters.config = seriona::audio::WaveformConfig{};
    return parameters;
}

std::optional<WaveformRequest> makeWaveformRequest(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library,
    const WaveformParameters &parameters)
{
    if (!player.currentTrack) {
        return std::nullopt;
    }

    const seriona::control::TrackIdentity &track = *player.currentTrack;
    const seriona::scanner::SongMetadata *song = findSongByTrackId(library, track.trackId);

    std::filesystem::path waveformPath;
    std::int64_t offsetUS = 0;
    std::int64_t durationUS = 0;
    if (song) {
        waveformPath = song->sourceFilePath.empty() ? song->filePath : song->sourceFilePath;
        if (song->offset) {
            offsetUS = toMicroseconds(*song->offset);
        }
        if (song->duration) {
            durationUS = toMicroseconds(*song->duration);
        }
    }
    if (waveformPath.empty()) {
        waveformPath = track.filePath;
    }
    if (waveformPath.empty()) {
        return std::nullopt;
    }

    WaveformRequest request;
    request.trackId = QString::fromStdString(track.trackId);
    request.waveformFilePath = pathToQString(waveformPath);
    request.offsetUS = offsetUS;
    request.durationUS = durationUS;
    request.startTimeUS = offsetUS;
    request.endTimeUS = durationUS > 0 ? offsetUS + durationUS : 0;
    request.parameters = parameters;
    return request;
}

}
#endif
