#include "track_stats_controller.h"

#include <algorithm>

namespace Seriona::App {

namespace {

constexpr auto kTrackStatsGroup = "trackStats";
constexpr auto kPlayCountKeyPrefix = "playCount/";
constexpr auto kRatingKeyPrefix = "rating/";

QString playCountKey(const QString &trackId)
{
    return QString::fromLatin1(kPlayCountKeyPrefix) + trackId;
}

QString ratingKey(const QString &trackId)
{
    return QString::fromLatin1(kRatingKeyPrefix) + trackId;
}

}

TrackStatsController::TrackStatsController(QObject *parent)
    : QObject(parent)
{
}

void TrackStatsController::setSettingsStorageBackend(AppSettingsBackend backend)
{
    m_settingsStorage.setBackend(std::move(backend));
}

int TrackStatsController::playCountFor(const QString &trackId) const
{
    if (!validTrackId(trackId)) {
        return 0;
    }

    const int count = m_settingsStorage.read(QString::fromLatin1(kTrackStatsGroup),
                                             playCountKey(trackId),
                                             0)
                          .toInt();
    return std::max(count, 0);
}

void TrackStatsController::recordPlayback(const QString &trackId)
{
    if (!validTrackId(trackId)) {
        return;
    }

    const int count = playCountFor(trackId) + 1;
    m_settingsStorage.write(QString::fromLatin1(kTrackStatsGroup), playCountKey(trackId), count);
    emit playCountChanged(trackId, count);
}

int TrackStatsController::ratingFor(const QString &trackId) const
{
    if (!validTrackId(trackId)) {
        return 0;
    }

    const int rating = m_settingsStorage.read(QString::fromLatin1(kTrackStatsGroup),
                                              ratingKey(trackId),
                                              0)
                           .toInt();
    return std::clamp(rating, 0, 5);
}

void TrackStatsController::setRating(const QString &trackId, int rating)
{
    if (!validTrackId(trackId)) {
        return;
    }

    const int clamped = std::clamp(rating, 0, 5);

    if (clamped == 0) {
        m_settingsStorage.remove(QString::fromLatin1(kTrackStatsGroup), ratingKey(trackId));
    } else {
        m_settingsStorage.write(QString::fromLatin1(kTrackStatsGroup), ratingKey(trackId), clamped);
    }

    emit ratingChanged(trackId, clamped);
}

bool TrackStatsController::validTrackId(const QString &trackId) const
{
    return !trackId.trimmed().isEmpty();
}

}
