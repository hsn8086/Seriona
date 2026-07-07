#pragma once

#include <QObject>

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#if SERIONA_HAS_BACKEND
#include "seriona/audio/waveform_generator.h"
#include "seriona/control/control_contracts.h"

#include <QHash>
#include <QString>
#include <QThreadPool>
#include <QVariantList>

#include <cstdint>
#include <functional>
#include <optional>

namespace Seriona::App {

struct WaveformParameters {
    int barCount = 0;
    int totalWidth = 0;
    int maxHeight = 0;
    seriona::audio::WaveformConfig config{};
};

struct WaveformBuildInput {
    QString trackId;
    QString waveformFilePath;
    int barCount = 0;
    int totalWidth = 0;
    int maxHeight = 0;
    std::int64_t startTimeUS = 0;
    std::int64_t endTimeUS = 0;
    seriona::audio::WaveformConfig config{};
};

struct WaveformPayload {
    QVariantList heights;
    int barWidth = 0;
};

struct WaveformRequest {
    QString trackId;
    QString waveformFilePath;
    std::int64_t offsetUS = 0;
    std::int64_t durationUS = 0;
    std::int64_t startTimeUS = 0;
    std::int64_t endTimeUS = 0;
    WaveformParameters parameters;

    [[nodiscard]] QString cacheKey() const;
};

struct WaveformResult {
    quint64 requestId = 0;
    QString cacheKey;
    QVariantList heights;
    int barWidth = 0;
    bool cacheHit = false;
    QString errorMessage;
};

class WaveformProvider : public QObject
{
    Q_OBJECT

public:
    using Generator = std::function<WaveformPayload(const WaveformBuildInput &)>;

    explicit WaveformProvider(QObject *parent = nullptr);
    ~WaveformProvider() override;

    [[nodiscard]] quint64 requestWaveform(WaveformRequest request);
    void requestForSnapshots(
        const seriona::control::PlayerStateSnapshot &player,
        const seriona::control::LibraryStateSnapshot &library);
    void requestForSnapshots(
        const seriona::control::PlayerStateSnapshot &player,
        const seriona::control::LibraryStateSnapshot &library,
        const WaveformParameters &parameters);
    void cancelPending();
    void setGeneratorForTests(Generator generator);

signals:
    void waveformReady(const Seriona::App::WaveformResult &result);
    void waveformFailed(const Seriona::App::WaveformResult &result);

private:
    struct CachedWaveform {
        QVariantList heights;
        int barWidth = 0;
    };

    static WaveformPayload buildWithBackend(const WaveformBuildInput &input);

    Generator m_generator;
    QHash<QString, CachedWaveform> m_cache;
    QThreadPool m_threadPool;
    quint64 m_latestRequestId = 0;
    QString m_currentCacheKey;
};

[[nodiscard]] WaveformParameters defaultWaveformParameters();
[[nodiscard]] std::optional<WaveformRequest> makeWaveformRequest(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot &library,
    const WaveformParameters &parameters);

}

Q_DECLARE_METATYPE(Seriona::App::WaveformResult)
#endif
