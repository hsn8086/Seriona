#include "lyrics_model.h"

namespace Seriona::App {

LyricsModel::LyricsModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_lines({QStringLiteral("Music playing in the night... / 音乐在夜空中回荡..."),
               QStringLiteral("Seriona shines so bright... / Seriona 闪耀着光芒..."),
               QStringLiteral("A melody that guides the way... / 指引道路的旋律..."),
               QStringLiteral("Through the dark and into day... / 穿过黑暗迎来黎明..."),
               QStringLiteral("Feel the rhythm, feel the beat... / 感受旋律，感受节拍..."),
               QStringLiteral("Walking down this lonely street... / 走在这条孤独的街上..."),
               QStringLiteral("But with music in my soul... / 但只要我的灵魂有音乐..."),
               QStringLiteral("I am happy, I am whole... / 我就快乐，我就完整...")})
{
    m_advanceTimer.setInterval(3000);
    connect(&m_advanceTimer, &QTimer::timeout, this, &LyricsModel::advanceLyric);
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

    const QString &line = m_lines.at(index.row());
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

bool LyricsModel::advancing() const
{
    return m_advancing;
}

void LyricsModel::setAdvancing(bool advancing)
{
    if (m_advancing == advancing) {
        return;
    }

    m_advancing = advancing;
    if (m_advancing) {
        m_advanceTimer.start();
    } else {
        m_advanceTimer.stop();
    }
    emit advancingChanged();
}

void LyricsModel::selectLyric(int index)
{
    setCurrentIndex(index);
}

void LyricsModel::toggleTranslation()
{
    setShowTranslation(!m_showTranslation);
}

void LyricsModel::advanceLyric()
{
    if (m_lines.isEmpty()) {
        return;
    }

    setCurrentIndex((m_currentIndex + 1) % m_lines.size());
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
