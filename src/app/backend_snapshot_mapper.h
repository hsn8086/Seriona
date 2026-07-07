#pragma once

#include <QString>

#include <chrono>
#include <cstdint>
#include <optional>

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#if SERIONA_HAS_BACKEND
#include "seriona/control/control_contracts.h"
#endif

namespace Seriona::App {

struct CurrentTrackViewState {
    QString trackId;
    QString nodeId;
    QString title = QStringLiteral("No song selected");
    QString artist = QStringLiteral("Unknown Artist");
    QString album = QStringLiteral("Unknown Album");
    QString artworkPath;
    qreal durationSeconds = 0.0;
    QString audioFormat;
    int audioSampleRate = 0;
    int audioBitDepth = 0;
    int audioChannels = 0;
};

struct TimelineSnapshotViewState {
    std::chrono::milliseconds position{0};
    std::optional<std::chrono::milliseconds> duration;
    std::chrono::steady_clock::time_point sampledAt{};
    std::uint64_t version = 0;
    qreal durationSeconds = 0.0;
    qreal positionSeconds = 0.0;
    bool smooth = false;
};

struct PlayerSnapshotViewState {
    CurrentTrackViewState currentTrack;
    TimelineSnapshotViewState timeline;
    bool isPlaying = false;
    qreal volume = 1.0;
    bool shuffle = false;
    int repeatMode = 0;
    QString capability = QStringLiteral("none");
};

struct LibrarySnapshotViewState {
    QString scanStatus = QStringLiteral("pending");
    int scanProgress = 0;
    QString lastError;
};

#if SERIONA_HAS_BACKEND
[[nodiscard]] QString backendStringToQString(const std::string &value);
[[nodiscard]] QString playingTrackIdFromSnapshot(const seriona::control::PlayerStateSnapshot &snapshot);
[[nodiscard]] PlayerSnapshotViewState mapPlayerSnapshot(
    const seriona::control::PlayerStateSnapshot &player,
    const seriona::control::LibraryStateSnapshot *library = nullptr);
[[nodiscard]] LibrarySnapshotViewState mapLibrarySnapshot(
    const seriona::control::LibraryStateSnapshot &snapshot);
#endif

}
