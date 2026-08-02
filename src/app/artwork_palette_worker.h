#pragma once

#include <QObject>
#include <QString>

#include <array>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace Seriona::App {

using GradientPalette = std::array<QString, 3>;

[[nodiscard]] GradientPalette defaultGradientPalette();

[[nodiscard]] GradientPalette decodeGradientPalette(const QString &thumbnailPath);

class ArtworkPaletteWorker : public QObject
{
    Q_OBJECT

public:
    using Palette = GradientPalette;
    using Decoder = std::function<Palette(const QString &thumbnailPath)>;

    explicit ArtworkPaletteWorker(Decoder decoder, QObject *parent = nullptr);
    ~ArtworkPaletteWorker() override;

    // Requests palette extraction for thumbnailPath. Returns the generation
    // assigned to this request (0 when nothing was recorded, i.e. an empty
    // path or a request made after shutdown). Callers should compare the
    // returned generation against the one carried by paletteReady and drop
    // results whose generation does not match the latest request.
    [[nodiscard]] quint64 requestPalette(const QString &thumbnailPath);
    void shutdown();

signals:
    void paletteReady(quint64 generation, const QString &color0, const QString &color1, const QString &color2);

private:
    struct PendingJob {
        quint64 generation = 0;
        QString thumbnailPath;
    };

    void runLoop();

    Decoder m_decoder;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::thread m_thread;
    bool m_threadStarted = false;
    bool m_stopped = false;
    quint64 m_latestGeneration = 0;
    std::optional<PendingJob> m_pending;
};

}
