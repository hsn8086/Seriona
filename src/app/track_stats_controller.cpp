#include "track_stats_controller.h"

#include <QCoreApplication>
#include <QSettings>

#include <algorithm>

namespace Seriona::App {

namespace {

constexpr auto kSettingsFileProperty = "seriona.settingsFileForTests";
constexpr auto kTrackStatsGroup = "trackStats";
constexpr auto kPlayCountKeyPrefix = "playCount/";
constexpr auto kRatingKeyPrefix = "rating/";

// 与 settings_controller 相同模式：测试可经
// QCoreApplication::property("seriona.settingsFileForTests") 注入临时文件；
// 未注入时落到用户级 QSettings（"Seriona/Seriona"）。
QSettings applicationSettings()
{
    const QCoreApplication *application = QCoreApplication::instance();
    if (application) {
        const QString settingsFile = application->property(kSettingsFileProperty).toString();
        if (!settingsFile.isEmpty()) {
            return QSettings(settingsFile, QSettings::IniFormat);
        }
    }

    return QSettings(QStringLiteral("Seriona"), QStringLiteral("Seriona"));
}

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

int TrackStatsController::playCountFor(const QString &trackId) const
{
    if (!validTrackId(trackId)) {
        return 0;
    }

    QSettings settings = applicationSettings();
    settings.beginGroup(QString::fromLatin1(kTrackStatsGroup));
    const int count = settings.value(playCountKey(trackId), 0).toInt();
    settings.endGroup();
    return std::max(count, 0);
}

void TrackStatsController::recordPlayback(const QString &trackId)
{
    if (!validTrackId(trackId)) {
        return;
    }

    QSettings settings = applicationSettings();
    settings.beginGroup(QString::fromLatin1(kTrackStatsGroup));
    const int count = playCountFor(trackId) + 1;
    settings.setValue(playCountKey(trackId), count);
    settings.endGroup();
    settings.sync();

    emit playCountChanged(trackId, count);
}

int TrackStatsController::ratingFor(const QString &trackId) const
{
    if (!validTrackId(trackId)) {
        return 0;
    }

    QSettings settings = applicationSettings();
    settings.beginGroup(QString::fromLatin1(kTrackStatsGroup));
    const int rating = settings.value(ratingKey(trackId), 0).toInt();
    settings.endGroup();
    return std::clamp(rating, 0, 5);
}

void TrackStatsController::setRating(const QString &trackId, int rating)
{
    if (!validTrackId(trackId)) {
        return;
    }

    const int clamped = std::clamp(rating, 0, 5);

    QSettings settings = applicationSettings();
    settings.beginGroup(QString::fromLatin1(kTrackStatsGroup));
    if (clamped == 0) {
        settings.remove(ratingKey(trackId));
    } else {
        settings.setValue(ratingKey(trackId), clamped);
    }
    settings.endGroup();
    settings.sync();

    emit ratingChanged(trackId, clamped);
}

bool TrackStatsController::validTrackId(const QString &trackId) const
{
    return !trackId.trimmed().isEmpty();
}

}
