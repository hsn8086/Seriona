#include "library_model.h"

namespace Seriona::App {

LibraryModel::LibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    const Entry *entry = entryAt(index.row());
    if (entry == nullptr) {
        return {};
    }

    switch (role) {
    case TypeRole:
        return entry->type;
    case NameRole:
        return entry->name;
    case TitleRole:
        return entry->title;
    case ArtistRole:
        return entry->artist;
    case AlbumRole:
        return entry->album;
    case ParentNameRole:
        return entry->parentName;
    case SongCountRole:
        return entry->songCount;
    case DurationRole:
        return entry->duration;
    case FormatRole:
        return entry->format;
    case SampleRateRole:
        return entry->sampleRate;
    case BitDepthRole:
        return entry->bitDepth;
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {{TypeRole, "type"},
            {NameRole, "name"},
            {TitleRole, "title"},
            {ArtistRole, "artist"},
            {AlbumRole, "album"},
            {ParentNameRole, "parentName"},
            {SongCountRole, "songCount"},
            {DurationRole, "duration"},
            {FormatRole, "format"},
            {SampleRateRole, "sampleRate"},
            {BitDepthRole, "bitDepth"}};
}

const LibraryModel::Entry *LibraryModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return nullptr;
    }

    return &m_entries.at(row);
}

void LibraryModel::setEntries(const QVector<Entry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

LibraryController::LibraryController(QObject *parent)
    : QObject(parent)
    , m_model(this)
{
    m_model.setEntries(rootEntries());
}

LibraryModel *LibraryController::model()
{
    return &m_model;
}

QString LibraryController::currentFolderName() const
{
    return m_currentFolderName;
}

bool LibraryController::canGoBack() const
{
    return m_folder != Folder::Root;
}

QString LibraryController::searchQuery() const
{
    return m_searchQuery;
}

void LibraryController::setSearchQuery(const QString &query)
{
    if (m_searchQuery == query) {
        return;
    }

    m_searchQuery = query;
    updateModelEntries();
    emit searchQueryChanged();
}

void LibraryController::enterFolder(int index)
{
    const LibraryModel::Entry *entry = m_model.entryAt(index);
    if (entry == nullptr || entry->type != QStringLiteral("folder")) {
        return;
    }

    setFolder(Folder::Child, entry->name);
}

void LibraryController::goBack()
{
    if (!canGoBack()) {
        return;
    }

    setFolder(Folder::Root, QStringLiteral("My Music"));
}

void LibraryController::refresh()
{
    setFolder(m_folder, m_currentFolderName);
}

void LibraryController::playItem(int index)
{
    const LibraryModel::Entry *entry = m_model.entryAt(index);
    if (entry == nullptr || entry->type != QStringLiteral("file")) {
        return;
    }

    emit playItemRequested(entry->title);
}

void LibraryController::locateCurrentSong()
{
    emit currentSongLocationRequested();
}

void LibraryController::clearSearch()
{
    if (m_searchQuery.isEmpty()) {
        emit searchCleared();
        return;
    }

    m_searchQuery.clear();
    updateModelEntries();
    emit searchQueryChanged();
    emit searchCleared();
}

void LibraryController::submitSearch()
{
    emit searchSubmitted(m_searchQuery);
}

QString LibraryController::describeBackendHook() const
{
    return QStringLiteral("Future backend hook: library folder browsing, refresh, item playback, current-song location, and library search intent.");
}

QVector<LibraryModel::Entry> LibraryController::rootEntries()
{
    return {{QStringLiteral("folder"), QStringLiteral("Hi-Res Collection"), {}, {}, {}, QStringLiteral("Music"), 128, QStringLiteral("12:45:30")},
            {QStringLiteral("file"), {}, QStringLiteral("Stairway to Heaven"), QStringLiteral("Led Zeppelin"), QStringLiteral("Led Zeppelin IV"), {}, 0, QStringLiteral("08:02"), QStringLiteral("FLAC"), 96000, 24},
            {QStringLiteral("file"), {}, QStringLiteral("Bohemian Rhapsody"), QStringLiteral("Queen"), QStringLiteral("A Night at the Opera"), {}, 0, QStringLiteral("05:55"), QStringLiteral("WAV"), 192000, 24},
            {QStringLiteral("folder"), QStringLiteral("Rock Classics"), {}, {}, {}, QStringLiteral("Music"), 45, QStringLiteral("03:12:00")},
            {QStringLiteral("file"), {}, QStringLiteral("Imagine"), QStringLiteral("John Lennon"), QStringLiteral("Imagine"), {}, 0, QStringLiteral("03:03"), QStringLiteral("MP3"), 44100, 16},
            {QStringLiteral("file"), {}, QStringLiteral("Hotel California"), QStringLiteral("Eagles"), QStringLiteral("Hotel California"), {}, 0, QStringLiteral("06:30"), QStringLiteral("FLAC"), 48000, 24},
            {QStringLiteral("folder"), QStringLiteral("Jazz Essentials"), {}, {}, {}, QStringLiteral("Music"), 32, QStringLiteral("02:45:15")}};
}

QVector<LibraryModel::Entry> LibraryController::childEntries()
{
    return {{QStringLiteral("file"), {}, QStringLiteral("Sub Song 1"), QStringLiteral("Artist A"), QStringLiteral("Album X"), {}, 0, QStringLiteral("03:45"), QStringLiteral("FLAC"), 44100, 16},
            {QStringLiteral("file"), {}, QStringLiteral("Sub Song 2"), QStringLiteral("Artist B"), QStringLiteral("Album Y"), {}, 0, QStringLiteral("04:20"), QStringLiteral("MP3"), 44100, 16}};
}

QVector<LibraryModel::Entry> LibraryController::currentSourceEntries() const
{
    return m_folder == Folder::Root ? rootEntries() : childEntries();
}

QVector<LibraryModel::Entry> LibraryController::filteredEntries(const QVector<LibraryModel::Entry> &entries) const
{
    const QString trimmedQuery = m_searchQuery.trimmed();
    if (trimmedQuery.isEmpty()) {
        return entries;
    }

    QVector<LibraryModel::Entry> filtered;
    filtered.reserve(entries.size());
    for (const LibraryModel::Entry &entry : entries) {
        const bool matchesFolder = entry.type == QStringLiteral("folder")
            && (entry.name.contains(trimmedQuery, Qt::CaseInsensitive)
                || entry.parentName.contains(trimmedQuery, Qt::CaseInsensitive));
        const bool matchesFile = entry.type == QStringLiteral("file")
            && (entry.title.contains(trimmedQuery, Qt::CaseInsensitive)
                || entry.artist.contains(trimmedQuery, Qt::CaseInsensitive)
                || entry.album.contains(trimmedQuery, Qt::CaseInsensitive)
                || entry.format.contains(trimmedQuery, Qt::CaseInsensitive));

        if (matchesFolder || matchesFile) {
            filtered.append(entry);
        }
    }

    return filtered;
}

void LibraryController::updateModelEntries()
{
    m_model.setEntries(filteredEntries(currentSourceEntries()));
}

void LibraryController::setFolder(Folder folder, const QString &folderName)
{
    const bool previousCanGoBack = canGoBack();
    const bool folderChanged = m_folder != folder;
    const bool nameChanged = m_currentFolderName != folderName;

    m_folder = folder;
    m_currentFolderName = folderName;
    updateModelEntries();

    if (nameChanged) {
        emit currentFolderNameChanged();
    }
    if (previousCanGoBack != canGoBack() || folderChanged) {
        emit canGoBackChanged();
    }
}

}
