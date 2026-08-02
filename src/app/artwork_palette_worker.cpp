#include "artwork_palette_worker.h"

#include <QColor>
#include <QImage>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Seriona::App {

namespace {

struct ScoredColor {
    QColor color;
    double weight = 0.0;
};

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

QColor toBackgroundTone(const QColor &input, qreal targetLightness)
{
    float h = 0.0F;
    float s = 0.0F;
    float l = 0.0F;
    input.getHslF(&h, &s, &l);

    s = std::min(s, 0.50F);
    l = static_cast<float>(qBound(0.12, targetLightness, 0.50));

    if (h < 0.0F)
        h = 0.0F;
    return QColor::fromHslF(h, s, l);
}

GradientPalette extractGradientPalette(const QString &imagePath)
{
    const GradientPalette fallback = defaultGradientPalette();

    if (imagePath.isEmpty())
        return fallback;

    QImage image(imagePath);
    if (image.isNull())
        return fallback;

    constexpr int kSampleSize = 32;
    constexpr int kMargin = 2;
    const QImage small = image.scaled(kSampleSize, kSampleSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    std::vector<ScoredColor> buckets;
    buckets.reserve(64);

    for (int y = kMargin; y < small.height() - kMargin; ++y) {
        for (int x = kMargin; x < small.width() - kMargin; ++x) {
            const QColor pixel = small.pixelColor(x, y);
            if (pixel.lightness() < 24 || pixel.lightness() > 236)
                continue;

            const double pixelWeight = (pixel.saturationF() * 2.0)
                + (1.0 - std::abs(pixel.lightnessF() - 0.5));

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

    std::sort(buckets.begin(), buckets.end(), [](const ScoredColor &a, const ScoredColor &b) {
        return a.weight > b.weight;
    });

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

    while (chosen.size() < 3) {
        if (!chosen.empty())
            chosen.push_back(chosen.back().darker(118));
        else
            return fallback;
    }

    const qreal targetLightness[3] = {0.46, 0.36, 0.26};
    GradientPalette palette;
    for (int i = 0; i < 3; ++i)
        palette[static_cast<std::size_t>(i)] = toBackgroundTone(chosen[static_cast<std::size_t>(i)], targetLightness[i]).name();

    return palette;
}

}

GradientPalette defaultGradientPalette()
{
    return {
        QStringLiteral("#4a2c2a"),
        QStringLiteral("#2b1a1a"),
        QStringLiteral("#1a1212"),
    };
}

GradientPalette decodeGradientPalette(const QString &thumbnailPath)
{
    return extractGradientPalette(thumbnailPath);
}

ArtworkPaletteWorker::ArtworkPaletteWorker(Decoder decoder, QObject *parent)
    : QObject(parent)
    , m_decoder(std::move(decoder))
{
}

ArtworkPaletteWorker::~ArtworkPaletteWorker()
{
    shutdown();
}

quint64 ArtworkPaletteWorker::requestPalette(const QString &thumbnailPath)
{
    if (thumbnailPath.isEmpty()) {
        return 0;
    }

    quint64 generation = 0;
    {
        std::lock_guard lock(m_mutex);
        if (m_stopped) {
            return 0;
        }
        ++m_latestGeneration;
        generation = m_latestGeneration;
        m_pending = PendingJob{generation, thumbnailPath};
        if (!m_threadStarted) {
            m_thread = std::thread(&ArtworkPaletteWorker::runLoop, this);
            m_threadStarted = true;
        }
    }
    m_condition.notify_one();
    return generation;
}

void ArtworkPaletteWorker::shutdown()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopped = true;
        m_pending.reset();
    }
    m_condition.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void ArtworkPaletteWorker::runLoop()
{
    for (;;) {
        std::optional<PendingJob> job;
        {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, [this] { return m_stopped || m_pending.has_value(); });
            if (m_stopped) {
                return;
            }
            job = std::move(m_pending);
            m_pending.reset();
        }

        const Palette palette = m_decoder(job->thumbnailPath);

        {
            std::lock_guard lock(m_mutex);
            if (m_stopped || job->generation != m_latestGeneration) {
                continue;
            }
            emit paletteReady(job->generation, palette[0], palette[1], palette[2]);
        }
    }
}

}
