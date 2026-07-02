#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>
#include <QVector>

namespace Seriona::App {

class LibraryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("LibraryModel is owned by LibraryController")

public:
    enum Role {
        TypeRole = Qt::UserRole + 1,
        NameRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        ParentNameRole,
        SongCountRole,
        DurationRole,
        FormatRole,
        SampleRateRole,
        BitDepthRole
    };
    Q_ENUM(Role)

    struct Entry {
        QString type;
        QString name;
        QString title;
        QString artist;
        QString album;
        QString parentName;
        int songCount = 0;
        QString duration;
        QString format;
        int sampleRate = 0;
        int bitDepth = 0;
    };

    explicit LibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const Entry *entryAt(int row) const;
    void setEntries(const QVector<Entry> &entries);

private:
    QVector<Entry> m_entries;
};

class LibraryController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(LibraryModel *model READ model CONSTANT)
    Q_PROPERTY(QString currentFolderName READ currentFolderName NOTIFY currentFolderNameChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY canGoBackChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    QML_ELEMENT

public:
    explicit LibraryController(QObject *parent = nullptr);

    LibraryModel *model();
    QString currentFolderName() const;
    bool canGoBack() const;
    QString searchQuery() const;
    void setSearchQuery(const QString &query);

    // future backend hook: enter a library folder and load its child entries.
    Q_INVOKABLE void enterFolder(int index);
    // future backend hook: navigate to the parent library folder.
    Q_INVOKABLE void goBack();
    // future backend hook: refresh the current library folder from the backend.
    Q_INVOKABLE void refresh();
    // future backend hook: request playback for a library item.
    Q_INVOKABLE void playItem(int index);
    // future backend hook: locate the current playing song in the library tree.
    Q_INVOKABLE void locateCurrentSong();
    // future backend hook: clear the current library search query and results.
    Q_INVOKABLE void clearSearch();
    // future backend hook: submit the current library search query to a backend/local index.
    Q_INVOKABLE void submitSearch();
    Q_INVOKABLE QString describeBackendHook() const;

signals:
    void currentFolderNameChanged();
    void canGoBackChanged();
    void searchQueryChanged();
    void searchSubmitted(QString query);
    void searchCleared();
    void playItemRequested(QString title);
    void currentSongLocationRequested();

private:
    enum class Folder {
        Root,
        Child
    };

    static QVector<LibraryModel::Entry> rootEntries();
    static QVector<LibraryModel::Entry> childEntries();
    QVector<LibraryModel::Entry> currentSourceEntries() const;
    QVector<LibraryModel::Entry> filteredEntries(const QVector<LibraryModel::Entry> &entries) const;
    void updateModelEntries();
    void setFolder(Folder folder, const QString &folderName);

    LibraryModel m_model;
    Folder m_folder = Folder::Root;
    QString m_currentFolderName = QStringLiteral("My Music");
    QString m_searchQuery;
};

}
