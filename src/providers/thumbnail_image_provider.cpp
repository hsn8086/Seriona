#include "thumbnail_image_provider.h"

#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QUrl>
#include <QDebug>

namespace Seriona::App {

ThumbnailImageProvider::ThumbnailImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
    m_memoryCache.setMaxCost(50 * 1024);  // 50 MB default
    m_threadPool.setMaxThreadCount(4);
}

ThumbnailImageProvider::~ThumbnailImageProvider() = default;

QImage ThumbnailImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // Parse the ID - format: "file:///path/to/image.jpg"
    QString sourcePath = id;
    if (sourcePath.startsWith("file://")) {
        sourcePath = QUrl(sourcePath).toLocalFile();
    }

    if (sourcePath.isEmpty()) {
        return generatePlaceholder(requestedSize.isValid() ? requestedSize : QSize(240, 240));
    }

    // Check memory cache first
    QString cacheKey = computeCacheKey(sourcePath, requestedSize);
    {
        QMutexLocker locker(&m_cacheMutex);
        QImage *cached = m_memoryCache.object(cacheKey);
        if (cached) {
            if (size) {
                *size = cached->size();
            }
            return *cached;
        }
    }

    // Load thumbnail
    QImage thumbnail = loadThumbnail(sourcePath, requestedSize);
    
    if (thumbnail.isNull()) {
        thumbnail = generatePlaceholder(requestedSize.isValid() ? requestedSize : QSize(240, 240));
    }

    // Cache the result
    {
        QMutexLocker locker(&m_cacheMutex);
        int cost = thumbnail.sizeInBytes() / 1024;  // Cost in KB
        m_memoryCache.insert(cacheKey, new QImage(thumbnail), cost);
    }

    if (size) {
        *size = thumbnail.size();
    }

    return thumbnail;
}

QImage ThumbnailImageProvider::loadThumbnail(const QString &sourcePath, const QSize &requestedSize)
{
#if SERIONA_HAS_BACKEND
    if (m_thumbnailService) {
        seriona::thumbnail::ThumbnailRequest request;
        request.sourcePath = sourcePath.toStdString();
        
        // Determine appropriate size
        int maxDimension = qMax(requestedSize.width(), requestedSize.height());
        if (maxDimension <= 64) {
            request.size = seriona::thumbnail::ThumbnailSize::Small;
        } else if (maxDimension <= 240) {
            request.size = seriona::thumbnail::ThumbnailSize::Medium;
        } else {
            request.size = seriona::thumbnail::ThumbnailSize::Large;
        }
        
        request.format = seriona::thumbnail::ThumbnailFormat::JPEG;
        request.quality = 85;
        request.allowUpscaling = false;

        auto response = m_thumbnailService->generate(request);
        
        if (std::holds_alternative<seriona::thumbnail::ThumbnailResult>(response)) {
            const auto &result = std::get<seriona::thumbnail::ThumbnailResult>(response);
            QImage image(QString::fromStdString(result.thumbnailPath.string()));
            
            if (!image.isNull()) {
                return image;
            }
        }
    }
#endif

    // Fallback: load directly
    QImageReader reader(sourcePath);
    if (!reader.canRead()) {
        return QImage();
    }

    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(240, 240);
    QSize scaledSize = reader.size().scaled(targetSize, Qt::KeepAspectRatio);
    reader.setScaledSize(scaledSize);

    QImage image = reader.read();
    if (image.isNull()) {
        return QImage();
    }

    // Further scale if needed with high-quality transform
    if (image.size() != targetSize) {
        image = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return image;
}

QImage ThumbnailImageProvider::generatePlaceholder(const QSize &size)
{
    QImage placeholder(size, QImage::Format_ARGB32);
    placeholder.fill(QColor(40, 40, 40, 255));
    
    QPainter painter(&placeholder);
    painter.setPen(QColor(160, 160, 160, 128));
    
    // Draw a simple music note icon
    int centerX = size.width() / 2;
    int centerY = size.height() / 2;
    int iconSize = qMin(size.width(), size.height()) / 3;
    
    QRect iconRect(centerX - iconSize / 2, centerY - iconSize / 2, iconSize, iconSize);
    painter.drawEllipse(iconRect);
    
    return placeholder;
}

QString ThumbnailImageProvider::computeCacheKey(const QString &sourcePath, const QSize &size) const
{
    return QString("%1_%2x%3").arg(sourcePath).arg(size.width()).arg(size.height());
}

#if SERIONA_HAS_BACKEND
void ThumbnailImageProvider::setThumbnailService(
    std::shared_ptr<seriona::thumbnail::ThumbnailService> service)
{
    m_thumbnailService = std::move(service);
}
#endif

void ThumbnailImageProvider::setCacheSize(int maxCostKB)
{
    QMutexLocker locker(&m_cacheMutex);
    m_memoryCache.setMaxCost(maxCostKB);
}

void ThumbnailImageProvider::clearCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_memoryCache.clear();
}

}  // namespace Seriona::App
