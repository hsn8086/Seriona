#include "backend_snapshot_mapper.h"

#if SERIONA_HAS_BACKEND
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <filesystem>
#endif

namespace Seriona::App {

#if SERIONA_HAS_BACKEND
namespace {

QString fromBackendPath(const std::filesystem::path &path)
{
    const auto utf8 = path.u8string();
    return QString::fromUtf8(
        reinterpret_cast<const char *>(utf8.data()), static_cast<qsizetype>(utf8.size()));
}

qreal secondsFromMilliseconds(std::chrono::milliseconds value)
{
    return static_cast<qreal>(value.count()) / 1000.0;
}

struct SongLookupResult {
    const seriona::scanner::SongMetadata *song = nullptr;
    QString nodeId;
};

SongLookupResult findSongByTrackId(const seriona::control::LibraryStateSnapshot *library, const std::string &trackId)
{
    if (trackId.empty() || library == nullptr || !library->libraryTree) {
        return {};
    }

    for (const seriona::scanner::PlaylistNode &node : library->libraryTree->nodes) {
        if (node.song && node.song->trackId == trackId) {
            return SongLookupResult{&(*node.song), backendStringToQString(node.nodeId)};
        }
    }
    return {};
}

#ifndef QT_NO_DEBUG
SongLookupResult findSongByDebugPath(
    const seriona::control::LibraryStateSnapshot *library,
    const std::filesystem::path &filePath)
{
    if (filePath.empty() || library == nullptr || !library->libraryTree) {
        return {};
    }

    for (const seriona::scanner::PlaylistNode &node : library->libraryTree->nodes) {
        if (!node.song) {
            continue;
        }
        if (node.song->filePath == filePath || node.song->sourceFilePath == filePath) {
            return SongLookupResult{&(*node.song), backendStringToQString(node.nodeId)};
        }
    }
    return {};
}
#endif

SongLookupResult findCurrentSong(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::control::LibraryStateSnapshot *library)
{
    if (!snapshot.currentTrack) {
        return {};
    }

    const seriona::control::TrackIdentity &track = *snapshot.currentTrack;
    SongLookupResult result = findSongByTrackId(library, track.trackId);
    if (result.song != nullptr) {
        return result;
    }

#ifndef QT_NO_DEBUG
    if (track.trackId.empty()) {
        result = findSongByDebugPath(library, track.filePath);
    }
#endif
    return result;
}

QString titleFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (snapshot.display && !snapshot.display->title.empty()) {
        return backendStringToQString(snapshot.display->title);
    }
    if (song != nullptr && !song->title.empty()) {
        return backendStringToQString(song->title);
    }
    if (snapshot.currentTrack && !snapshot.currentTrack->trackId.empty()) {
        return backendStringToQString(snapshot.currentTrack->trackId);
    }
    return QStringLiteral("No song selected");
}

QString artistFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (snapshot.display && !snapshot.display->artist.empty()) {
        return backendStringToQString(snapshot.display->artist);
    }
    if (snapshot.display && !snapshot.display->albumArtist.empty()) {
        return backendStringToQString(snapshot.display->albumArtist);
    }
    if (song != nullptr && !song->artist.empty()) {
        return backendStringToQString(song->artist);
    }
    if (song != nullptr && !song->albumArtist.empty()) {
        return backendStringToQString(song->albumArtist);
    }
    return QStringLiteral("Unknown Artist");
}

QString albumFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (snapshot.display && !snapshot.display->album.empty()) {
        return backendStringToQString(snapshot.display->album);
    }
    if (song != nullptr && !song->album.empty()) {
        return backendStringToQString(song->album);
    }
    return QStringLiteral("Unknown Album");
}

QString artworkPathFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (snapshot.artwork && snapshot.artwork->localPath && !snapshot.artwork->localPath->empty()) {
        return fromBackendPath(*snapshot.artwork->localPath);
    }
    if (song != nullptr && song->artworkPath && !song->artworkPath->empty()) {
        return fromBackendPath(*song->artworkPath);
    }
    return {};
}

QString preferredArtworkPathFromStateSource(const seriona::control::PlayerStateSnapshot &snapshot)
{
    if (snapshot.artwork && snapshot.artwork->localPath && !snapshot.artwork->localPath->empty()) {
        return fromBackendPath(*snapshot.artwork->localPath);
    }
    return {};
}

QString fallbackThumbnailPathFromStateSource(const seriona::control::PlayerStateSnapshot &snapshot)
{
    if (snapshot.artwork && snapshot.artwork->thumbnailPath && !snapshot.artwork->thumbnailPath->empty()) {
        return fromBackendPath(*snapshot.artwork->thumbnailPath);
    }
    return {};
}

std::optional<std::chrono::milliseconds> durationFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (song != nullptr && song->duration) {
        return song->duration;
    }
    return snapshot.timeline.duration;
}

std::filesystem::path audioPathFromStateSource(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::scanner::SongMetadata *song)
{
    if (song != nullptr) {
        return song->sourceFilePath.empty() ? song->filePath : song->sourceFilePath;
    }
    if (snapshot.currentTrack) {
        return snapshot.currentTrack->filePath;
    }
    return {};
}

QString audioFormatFromPath(const std::filesystem::path &path)
{
    QString extension = fromBackendPath(path.extension());
    if (extension.startsWith(QLatin1Char('.'))) {
        extension.remove(0, 1);
    }
    return extension.toUpper();
}

CurrentTrackViewState mapCurrentTrackViewState(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::control::LibraryStateSnapshot *library)
{
    CurrentTrackViewState state;
    const SongLookupResult lookup = findCurrentSong(snapshot, library);
    const seriona::scanner::SongMetadata *song = lookup.song;

    if (snapshot.currentTrack) {
        state.trackId = backendStringToQString(snapshot.currentTrack->trackId);
    }
    state.nodeId = lookup.nodeId;
    state.title = titleFromStateSource(snapshot, song);
    state.artist = artistFromStateSource(snapshot, song);
    state.album = albumFromStateSource(snapshot, song);
    state.artworkPath = artworkPathFromStateSource(snapshot, song);
    state.preferredArtworkPath = preferredArtworkPathFromStateSource(snapshot);
    state.fallbackThumbnailPath = fallbackThumbnailPathFromStateSource(snapshot);

    const std::optional<std::chrono::milliseconds> duration = durationFromStateSource(snapshot, song);
    state.durationSeconds = duration ? secondsFromMilliseconds(*duration) : 0.0;

    state.audioFormat = audioFormatFromPath(audioPathFromStateSource(snapshot, song));
    if (song != nullptr) {
        state.audioSampleRate = song->sampleRate ? static_cast<int>(*song->sampleRate) : 0;
        state.audioBitDepth = song->bitDepth ? static_cast<int>(*song->bitDepth) : 0;
        state.audioChannels = song->channels ? static_cast<int>(*song->channels) : 0;
    }
    return state;
}

int repeatModeFromSnapshot(seriona::control::RepeatMode repeatMode)
{
    switch (repeatMode) {
    case seriona::control::RepeatMode::Off:
        return 0;
    case seriona::control::RepeatMode::All:
        return 1;
    case seriona::control::RepeatMode::One:
        return 2;
    }

    return 0;
}

QString capabilityFromSnapshot(const seriona::control::PlaybackCapabilities &capabilities)
{
    QStringList labels;
    if (capabilities.canPlay) {
        labels.append(QStringLiteral("play"));
    }
    if (capabilities.canPause) {
        labels.append(QStringLiteral("pause"));
    }
    if (capabilities.canStop) {
        labels.append(QStringLiteral("stop"));
    }
    if (capabilities.canSeek) {
        labels.append(QStringLiteral("seek"));
    }
    if (capabilities.canSkipNext) {
        labels.append(QStringLiteral("skip-next"));
    }
    if (capabilities.canSkipPrevious) {
        labels.append(QStringLiteral("skip-previous"));
    }
    if (capabilities.canSetRepeat) {
        labels.append(QStringLiteral("set-repeat"));
    }
    if (capabilities.canSetShuffle) {
        labels.append(QStringLiteral("set-shuffle"));
    }
    if (capabilities.canSetVolume) {
        labels.append(QStringLiteral("set-volume"));
    }
    if (capabilities.canSelectTrack) {
        labels.append(QStringLiteral("select-track"));
    }

    return labels.isEmpty() ? QStringLiteral("none") : labels.join(QLatin1Char(','));
}

bool shouldSmoothTimeline(seriona::control::PlaybackStatus status)
{
    return status == seriona::control::PlaybackStatus::Playing;
}

TimelineSnapshotViewState mapTimelineSnapshot(const seriona::control::PlayerStateSnapshot &snapshot)
{
    return TimelineSnapshotViewState{
        snapshot.timeline.position,
        snapshot.timeline.duration,
        snapshot.freshness.sampledAt,
        snapshot.freshness.version,
        snapshot.timeline.duration ? secondsFromMilliseconds(*snapshot.timeline.duration) : 0.0,
        secondsFromMilliseconds(snapshot.timeline.position),
        shouldSmoothTimeline(snapshot.playback.state),
    };
}

QString uiScanStatus(seriona::control::LibraryScanStatus status)
{
    switch (status) {
    case seriona::control::LibraryScanStatus::Idle:
    case seriona::control::LibraryScanStatus::Stopped:
        return QStringLiteral("pending");
    case seriona::control::LibraryScanStatus::Scanning:
        return QStringLiteral("running");
    case seriona::control::LibraryScanStatus::Completed:
        return QStringLiteral("completed");
    case seriona::control::LibraryScanStatus::Error:
        return QStringLiteral("error");
    }

    return QStringLiteral("pending");
}

int uiScanProgress(const seriona::control::LibraryStateSnapshot &snapshot)
{
    if (snapshot.scanStatus == seriona::control::LibraryScanStatus::Completed) {
        return 100;
    }
    if (!snapshot.scanProgress.has_value() || snapshot.scanProgress->filesDiscovered == 0U) {
        return 0;
    }

	const std::uint64_t discovered = snapshot.scanProgress->filesDiscovered;
	const std::uint64_t scanned = std::min(snapshot.scanProgress->filesScanned, discovered);
	const std::uint64_t skipped = std::min(snapshot.scanProgress->filesSkipped, discovered - scanned);
	const std::uint64_t processed = scanned + skipped;
	return static_cast<int>((processed * 100U) / discovered);
}

QString scannerErrorMessage(const seriona::scanner::ScannerError &error)
{
    if (!error.message.empty()) {
        return backendStringToQString(error.message);
    }
    if (!error.detail.empty()) {
        return backendStringToQString(error.detail);
    }

    return QStringLiteral("扫描失败");
}

}

QString backendStringToQString(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString playingTrackIdFromSnapshot(const seriona::control::PlayerStateSnapshot &snapshot)
{
    return snapshot.currentTrack.has_value()
        ? backendStringToQString(snapshot.currentTrack->trackId)
        : QString();
}

PlayerSnapshotViewState mapPlayerSnapshot(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot *library)
{
    return PlayerSnapshotViewState{
        mapCurrentTrackViewState(player, library),
        mapTimelineSnapshot(player),
        player.playback.state == seriona::control::PlaybackStatus::Playing,
        player.muted ? 0.0 : static_cast<qreal>(player.volume),
        player.shuffle,
        repeatModeFromSnapshot(player.repeatMode),
        capabilityFromSnapshot(player.capabilities),
    };
}

QVariantList mapQueueEntries(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot *library)
{
    QVariantList entries;
    entries.reserve(static_cast<qsizetype>(player.queueEntries.size()));
    const QString playingTrackId = playingTrackIdFromSnapshot(player);
    for (const seriona::control::QueueEntry &entry : player.queueEntries) {
        const SongLookupResult lookup = findSongByTrackId(library, entry.trackId);
        QVariantMap item;
        item.insert(QStringLiteral("trackId"), backendStringToQString(entry.trackId));
        item.insert(QStringLiteral("nodeId"), backendStringToQString(entry.nodeId));
        if (lookup.song != nullptr) {
            item.insert(QStringLiteral("title"),
                !lookup.song->title.empty() ? backendStringToQString(lookup.song->title)
                                            : backendStringToQString(entry.trackId));
            item.insert(QStringLiteral("artist"), backendStringToQString(lookup.song->artist));
            const std::filesystem::path &artPath = lookup.song->thumbnailPath.has_value() && !lookup.song->thumbnailPath->empty()
                ? *lookup.song->thumbnailPath
                : (lookup.song->artworkPath.has_value() ? *lookup.song->artworkPath : std::filesystem::path{});
            if (!artPath.empty()) {
                item.insert(QStringLiteral("artworkSource"),
                    QUrl::fromLocalFile(QString::fromStdString(artPath.string())).toString());
            } else {
                item.insert(QStringLiteral("artworkSource"), QString());
            }
        } else {
            item.insert(QStringLiteral("title"), backendStringToQString(entry.trackId));
            item.insert(QStringLiteral("artist"), QString());
            item.insert(QStringLiteral("artworkSource"), QString());
        }
        item.insert(QStringLiteral("isPlaying"),
            !playingTrackId.isEmpty() && playingTrackId == QString::fromStdString(entry.trackId));
        entries.append(item);
    }
    return entries;
}

LibrarySnapshotViewState mapLibrarySnapshot(const seriona::control::LibraryStateSnapshot &snapshot)
{
    QString lastError;
    if (snapshot.lastError.has_value()) {
        lastError = scannerErrorMessage(*snapshot.lastError);
    } else if (snapshot.scanStatus == seriona::control::LibraryScanStatus::Error) {
        lastError = QStringLiteral("扫描失败");
    }

    LibrarySnapshotViewState view{uiScanStatus(snapshot.scanStatus), uiScanProgress(snapshot), 0, 0, lastError};
    if (snapshot.scanProgress.has_value()) {
        const auto &progress = *snapshot.scanProgress;
        view.scannedSongCount = static_cast<quint64>(progress.filesScanned + progress.filesSkipped);
        view.totalSongCount = static_cast<quint64>(progress.filesDiscovered);
    }
    return view;
}
#endif

}
