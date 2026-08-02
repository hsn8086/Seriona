#pragma once

#include <QQuickImageProvider>
#include <QThreadPool>
#include <QImage>
#include <QCache>
#include <QMutex>
#include <QString>
#include <QRunnable>

#include <memory>
#include <filesystem>

#if SERIONA_HAS_BACKEND
#include "seriona/thumbnail/thumbnail_service.h"
#endif

namespace Seriona::App {

class ThumbnailImageProvider : public QQuickImageProvider
{
public:
    ThumbnailImageProvider();
    ~ThumbnailImageProvider() override;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

#if SERIONA_HAS_BACKEND
    void setThumbnailService(std::shared_ptr<seriona::thumbnail::ThumbnailService> service);
#endif

    void setCacheSize(int maxCostKB);
    void clearCache();

private:
    QImage loadThumbnail(const QString &sourcePath, const QSize &requestedSize);
    QImage generatePlaceholder(const QSize &size);
    QString computeCacheKey(const QString &sourcePath, const QSize &size) const;

#if SERIONA_HAS_BACKEND
    std::shared_ptr<seriona::thumbnail::ThumbnailService> m_thumbnailService;
#endif
    
    QCache<QString, QImage> m_memoryCache;
    QMutex m_cacheMutex;
    QThreadPool m_threadPool;
};

}  // namespace Seriona::App
