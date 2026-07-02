#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

log() {
    printf '\n==> %s\n' "$1"
}

assert_no_match() {
    local description="$1"
    local pattern="$2"
    shift 2

    log "$description"
    if rg -n "$pattern" "$@"; then
        printf 'Invariant failed: %s\n' "$description" >&2
        return 1
    fi
}

assert_match() {
    local description="$1"
    local pattern="$2"
    shift 2

    log "$description"
    rg -n "$pattern" "$@" >/dev/null
}

log "Configure"
cmake -B build

log "Build"
cmake --build build

assert_no_match \
    "No root QML navigation business state" \
    'property (string|bool) (currentView|isSidebarOpen|showStartupScreen)' \
    qml/Main.qml

assert_no_match \
    "No MainContent QML playback/song/lyrics mock ownership" \
    'property (bool|real|int|string|var) (isPlaying|currentPosition|totalDuration|volume|isShuffle|repeatMode|songTitle|artistName|albumName|waveformHeights)|lyricsData|Song Title|Artist Name|Album Name|Seriona shines|Music playing in the night' \
    qml/views/MainContent.qml

assert_no_match \
    "No Sidebar QML library/search mock ownership" \
    'ListModel|mockModel|mockSubModel|property string searchQuery|property var pageModel|Stairway to Heaven|Bohemian Rhapsody|Hi-Res Collection|Jazz Essentials' \
    qml/components/Sidebar.qml

assert_no_match \
    "No backend/network/database/filesystem/persistence implementation" \
    'QDir|QFileSystem|QNetwork|QSql|QTcp|QUdp|HTTP|database|SQLite|filesystem scan|persistence|persistent' \
    src qml

assert_match \
    "Backend handoff still references middle-layer owners" \
    'PlaybackController|LyricsModel|LibraryController|NavigationController' \
    docs/architecture/backend-integration-contract.md

if [[ -n "${VERIFY_MIDDLE_LAYER_EXTRA_FORBIDDEN_PATTERN:-}" ]]; then
    assert_no_match \
        "Extra injected forbidden pattern" \
        "$VERIFY_MIDDLE_LAYER_EXTRA_FORBIDDEN_PATTERN" \
        src qml docs/architecture
fi

log "Offscreen startup smoke"
set +e
QT_QPA_PLATFORM=offscreen timeout 5s ./build/appSeriona
smoke_status=$?
set -e

if [[ "$smoke_status" -ne 124 ]]; then
    printf 'Expected offscreen smoke to exit with timeout code 124, got %s\n' "$smoke_status" >&2
    exit 1
fi

log "Middle-layer verification passed"
