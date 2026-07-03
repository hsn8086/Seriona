#include "lyrics_model.h"

#include <QtMath>

#include <cmath>
#include <utility>

namespace Seriona::App {

namespace {

#if SERIONA_HAS_BACKEND
QString fromBackendString(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}
#endif

}

LyricsModel::LyricsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LyricsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_lines.size();
}

QVariant LyricsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size()) {
        return {};
    }

    const QString &line = m_lines.at(index.row()).text;
    switch (role) {
    case Qt::DisplayRole:
    case DisplayLineRole:
        return displayLine(line);
    case RawLineRole:
        return line;
    case TranslationRole:
        return translationLine(line);
    case CurrentRole:
        return index.row() == m_currentIndex;
    default:
        return {};
    }
}

QHash<int, QByteArray> LyricsModel::roleNames() const
{
    return {{RawLineRole, "rawLine"},
            {DisplayLineRole, "displayLine"},
            {TranslationRole, "translation"},
            {CurrentRole, "isCurrent"}};
}

int LyricsModel::currentIndex() const
{
    return m_currentIndex;
}

void LyricsModel::setCurrentIndex(int index)
{
    if (m_lines.isEmpty()) {
        index = 0;
    } else if (index < 0) {
        index = 0;
    } else if (index >= m_lines.size()) {
        index = m_lines.size() - 1;
    }

    if (m_currentIndex == index) {
        return;
    }

    const int previousIndex = m_currentIndex;
    m_currentIndex = index;
    emitCurrentRoleChanged(previousIndex);
    emitCurrentRoleChanged(m_currentIndex);
    emit currentIndexChanged();
}

qreal LyricsModel::playbackPosition() const
{
    return m_playbackPosition;
}

void LyricsModel::setPlaybackPosition(qreal position)
{
    const qreal normalizedPosition = std::isfinite(position) ? qMax(0.0, position) : 0.0;
    if (qAbs(m_playbackPosition - normalizedPosition) < 0.001) {
        return;
    }

    m_playbackPosition = normalizedPosition;
    emit playbackPositionChanged();
    syncCurrentIndexToPlaybackPosition();
}

bool LyricsModel::showTranslation() const
{
    return m_showTranslation;
}

void LyricsModel::setShowTranslation(bool showTranslation)
{
    if (m_showTranslation == showTranslation) {
        return;
    }

    m_showTranslation = showTranslation;
    emit showTranslationChanged();
}

QString LyricsModel::lyricDelimiter() const
{
    return m_lyricDelimiter;
}

void LyricsModel::setLyricDelimiter(const QString &delimiter)
{
    if (m_lyricDelimiter == delimiter) {
        return;
    }

    m_lyricDelimiter = delimiter;
    emit lyricDelimiterChanged();
    emitAllLyricsChanged({DisplayLineRole, TranslationRole});
}

#if SERIONA_HAS_BACKEND
void LyricsModel::applyPlayerStateSnapshot(
    const seriona::control::PlayerStateSnapshot &snapshot,
    const seriona::control::LibraryStateSnapshot *library)
{
    if (!snapshot.currentTrack || snapshot.currentTrack->trackId.empty() || library == nullptr || !library->libraryTree) {
        replaceLyrics({}, false);
        return;
    }

    const std::string &trackId = snapshot.currentTrack->trackId;
    const seriona::scanner::SongMetadata *song = nullptr;
    for (const seriona::scanner::PlaylistNode &node : library->libraryTree->nodes) {
        if (node.song && node.song->trackId == trackId) {
            song = &(*node.song);
            break;
        }
    }

    if (song == nullptr || song->effectiveLyrics.empty()) {
        replaceLyrics({}, false);
        return;
    }

    QVector<Line> lines;
    lines.reserve(static_cast<qsizetype>(song->effectiveLyrics.size()));
    bool hasTimedLyrics = false;
    for (const seriona::scanner::LyricLine &line : song->effectiveLyrics) {
        lines.append(Line{line.timestamp, fromBackendString(line.text)});
        hasTimedLyrics = hasTimedLyrics || line.timestamp.count() > 0;
    }

    replaceLyrics(std::move(lines), hasTimedLyrics);
}
#endif

void LyricsModel::selectLyric(int index)
{
    setCurrentIndex(index);
}

void LyricsModel::toggleTranslation()
{
    setShowTranslation(!m_showTranslation);
}

QString LyricsModel::displayLine(const QString &line) const
{
    if (m_lyricDelimiter.isEmpty()) {
        return line;
    }

    const qsizetype delimiterIndex = line.indexOf(m_lyricDelimiter);
    if (delimiterIndex < 0) {
        return line;
    }

    return line.left(delimiterIndex).trimmed();
}

QString LyricsModel::translationLine(const QString &line) const
{
    if (m_lyricDelimiter.isEmpty()) {
        return {};
    }

    const qsizetype delimiterIndex = line.indexOf(m_lyricDelimiter);
    if (delimiterIndex < 0) {
        return {};
    }

    return line.mid(delimiterIndex + m_lyricDelimiter.size()).trimmed();
}

void LyricsModel::replaceLyrics(QVector<Line> lines, bool hasTimedLyrics)
{
    bool sameLyrics = m_hasTimedLyrics == hasTimedLyrics && m_lines.size() == lines.size();
    for (qsizetype i = 0; sameLyrics && i < m_lines.size(); ++i) {
        sameLyrics = m_lines.at(i).timestamp == lines.at(i).timestamp && m_lines.at(i).text == lines.at(i).text;
    }
    if (sameLyrics) {
        syncCurrentIndexToPlaybackPosition();
        return;
    }

    const int previousIndex = m_currentIndex;
    beginResetModel();
    m_lines = std::move(lines);
    m_hasTimedLyrics = hasTimedLyrics;
    m_currentIndex = currentIndexForPlaybackPosition();
    endResetModel();

    if (m_currentIndex != previousIndex) {
        emit currentIndexChanged();
    }
}

int LyricsModel::currentIndexForPlaybackPosition() const
{
    if (m_lines.isEmpty() || !m_hasTimedLyrics) {
        return 0;
    }

    const std::chrono::milliseconds playbackTimestamp{qRound64(m_playbackPosition * 1000.0)};
    int synchronizedIndex = 0;
    std::chrono::milliseconds synchronizedTimestamp{0};
    for (qsizetype i = 0; i < m_lines.size(); ++i) {
        const std::chrono::milliseconds lineTimestamp = m_lines.at(i).timestamp;
        if (lineTimestamp <= playbackTimestamp && lineTimestamp >= synchronizedTimestamp) {
            synchronizedIndex = static_cast<int>(i);
            synchronizedTimestamp = lineTimestamp;
        }
    }
    return synchronizedIndex;
}

void LyricsModel::syncCurrentIndexToPlaybackPosition()
{
    setCurrentIndex(currentIndexForPlaybackPosition());
}

void LyricsModel::emitAllLyricsChanged(const QList<int> &roles)
{
    if (m_lines.isEmpty()) {
        return;
    }

    emit dataChanged(index(0, 0), index(m_lines.size() - 1, 0), roles);
}

void LyricsModel::emitCurrentRoleChanged(int index)
{
    if (index < 0 || index >= m_lines.size()) {
        return;
    }

    const QModelIndex modelIndex = this->index(index, 0);
    emit dataChanged(modelIndex, modelIndex, {CurrentRole});
}

}
