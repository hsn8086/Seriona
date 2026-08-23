#include "app_facade.h"

#include "app_settings_storage.h"
#include "seriona/control/control_contracts.h"

#include <QFileInfo>
#include <QHash>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QUrl>
#include <QVariant>
#include <QtTest/QTest>

#include <memory>
#include <vector>

namespace {
constexpr auto kBackendBridgeAutostartProperty = "seriona.backendBridgeAutostartEnabled";
constexpr auto kLastLibraryRootGroup = "library";
constexpr auto kLastLibraryRootKey = "lastScanRoot";

using Seriona::App::AppFacade;
using seriona::control::MediaControllerCommandResult;
using seriona::control::MediaControllerErrorCode;

QString storageKey(const QString &group, const QString &key)
{
    return group + QLatin1Char('\x1f') + key;
}

MediaControllerCommandResult acceptedResult()
{
    MediaControllerCommandResult result;
    result.accepted = true;
    result.code = MediaControllerErrorCode::None;
    return result;
}

MediaControllerCommandResult rejectedResult(const std::string &message)
{
    MediaControllerCommandResult result;
    result.accepted = false;
    result.code = MediaControllerErrorCode::BackendRejected;
    result.message = message;
    return result;
}

struct ScanRecorder {
    std::vector<QString> roots;
    std::vector<seriona::scanner::ScanMode> modes;
    MediaControllerCommandResult result = acceptedResult();

    MediaControllerCommandResult record(const QString &rootPath, seriona::scanner::ScanMode mode)
    {
        roots.push_back(rootPath);
        modes.push_back(mode);
        return result;
    }
};
}

class StartupRestoreTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void scanLibraryPersistsRootForStartupRestore();
    void restoreSavedRootScansAndEntersMainShell();
    void missingSavedRoot_data();
    void missingSavedRoot();

private:
    void writeSavedRoot(const QString &rootPath);
    bool hasSavedRoot() const;
    QString savedRoot() const;
    Seriona::App::AppSettingsBackend testBackend();

    QHash<QString, QVariant> m_store;
};

void StartupRestoreTest::init()
{
    QCoreApplication::instance()->setProperty(kBackendBridgeAutostartProperty, false);
    m_store.clear();
}

void StartupRestoreTest::cleanup()
{
    QCoreApplication::instance()->setProperty(kBackendBridgeAutostartProperty, QVariant{});
    m_store.clear();
}

Seriona::App::AppSettingsBackend StartupRestoreTest::testBackend()
{
    return Seriona::App::AppSettingsBackend{
        .read = [this](const QString &group, const QString &key, const QVariant &defaultValue) -> std::optional<QVariant> {
            return m_store.value(storageKey(group, key), defaultValue);
        },
        .write = [this](const QString &group, const QString &key, const QVariant &value) {
            m_store.insert(storageKey(group, key), value);
        },
        .remove = [this](const QString &group, const QString &key) {
            m_store.remove(storageKey(group, key));
        },
    };
}

void StartupRestoreTest::writeSavedRoot(const QString &rootPath)
{
    m_store.insert(storageKey(kLastLibraryRootGroup, kLastLibraryRootKey), rootPath);
}

bool StartupRestoreTest::hasSavedRoot() const
{
    return m_store.contains(storageKey(kLastLibraryRootGroup, kLastLibraryRootKey));
}

QString StartupRestoreTest::savedRoot() const
{
    return m_store.value(storageKey(kLastLibraryRootGroup, kLastLibraryRootKey)).toString();
}

void StartupRestoreTest::scanLibraryPersistsRootForStartupRestore()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    const QString canonicalRoot = QFileInfo(musicDir.path()).absoluteFilePath();

    AppFacade facade;
    facade.navigation()->setSettingsStorageBackend(testBackend());
    ScanRecorder recorder;
    facade.library()->setScanExecutor([&recorder](const QString &rootPath, seriona::scanner::ScanMode mode) {
        return recorder.record(rootPath, mode);
    });

    QVERIFY(facade.scanLibrary(QUrl::fromLocalFile(musicDir.path())));

    QCOMPARE(recorder.roots, std::vector<QString>{canonicalRoot});
    QCOMPARE(recorder.modes, std::vector<seriona::scanner::ScanMode>{seriona::scanner::ScanMode::Full});
    QCOMPARE(facade.library()->savedRootPath(), canonicalRoot);
    QCOMPARE(savedRoot(), canonicalRoot);
    QVERIFY(facade.navigation()->startupScreenVisible());
}

void StartupRestoreTest::restoreSavedRootScansAndEntersMainShell()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    const QString canonicalRoot = QFileInfo(musicDir.path()).absoluteFilePath();
    writeSavedRoot(canonicalRoot);

    AppFacade facade;
    facade.navigation()->setSettingsStorageBackend(testBackend());
    ScanRecorder recorder;
    facade.library()->setScanExecutor([&recorder](const QString &rootPath, seriona::scanner::ScanMode mode) {
        return recorder.record(rootPath, mode);
    });

    QVERIFY(facade.restorePlaylistFromStartup());

    QCOMPARE(recorder.roots, std::vector<QString>{canonicalRoot});
    QCOMPARE(recorder.modes, std::vector<seriona::scanner::ScanMode>{seriona::scanner::ScanMode::Incremental});
    QCOMPARE(facade.library()->savedRootPath(), canonicalRoot);
    QCOMPARE(savedRoot(), canonicalRoot);
    QCOMPARE(facade.navigation()->startupScreenVisible(), false);
    QCOMPARE(facade.navigation()->currentView(), QStringLiteral("playback"));

    QTemporaryDir rejectedDir;
    QVERIFY(rejectedDir.isValid());
    const QString rejectedRoot = QFileInfo(rejectedDir.path()).absoluteFilePath();
    writeSavedRoot(rejectedRoot);

    AppFacade rejectedFacade;
    rejectedFacade.navigation()->setSettingsStorageBackend(testBackend());
    ScanRecorder rejectedRecorder;
    rejectedRecorder.result = rejectedResult("startup restore scan rejected");
    rejectedFacade.library()->setScanExecutor([&rejectedRecorder](const QString &rootPath, seriona::scanner::ScanMode mode) {
        return rejectedRecorder.record(rootPath, mode);
    });
    rejectedFacade.navigation()->showLyricsView();
    rejectedFacade.navigation()->syncSidebarForDockCapability(true);

    QCOMPARE(rejectedFacade.restorePlaylistFromStartup(), false);

    QCOMPARE(rejectedRecorder.roots, std::vector<QString>{rejectedRoot});
    QCOMPARE(rejectedRecorder.modes, std::vector<seriona::scanner::ScanMode>{seriona::scanner::ScanMode::Incremental});
    QCOMPARE(rejectedFacade.navigation()->startupScreenVisible(), true);
    QCOMPARE(rejectedFacade.navigation()->currentView(), QStringLiteral("lyrics"));
    QCOMPARE(rejectedFacade.navigation()->sidebarOpen(), true);
    QCOMPARE(rejectedFacade.library()->scanStatus(), QStringLiteral("error"));
    QCOMPARE(rejectedFacade.library()->lastError(), QStringLiteral("startup restore scan rejected"));
}

void StartupRestoreTest::missingSavedRoot_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("messageFragment");

    QTest::newRow("absent") << QStringLiteral("absent") << QStringLiteral("添加音乐文件夹");
    QTest::newRow("deleted-directory") << QStringLiteral("deleted-directory") << QStringLiteral("重新选择文件夹");
    QTest::newRow("file-path") << QStringLiteral("file-path") << QStringLiteral("重新选择文件夹");
}

void StartupRestoreTest::missingSavedRoot()
{
    QFETCH(QString, scenario);
    QFETCH(QString, messageFragment);

    std::unique_ptr<QTemporaryFile> nonDirectory;
    if (scenario == QStringLiteral("deleted-directory")) {
        QTemporaryDir deletedDir;
        QVERIFY(deletedDir.isValid());
        const QString deletedPath = QFileInfo(deletedDir.path()).absoluteFilePath();
        QVERIFY(deletedDir.remove());
        writeSavedRoot(deletedPath);
    } else if (scenario == QStringLiteral("file-path")) {
        nonDirectory = std::make_unique<QTemporaryFile>();
        QVERIFY(nonDirectory->open());
        writeSavedRoot(QFileInfo(nonDirectory->fileName()).absoluteFilePath());
    }

    AppFacade facade;
    facade.navigation()->setSettingsStorageBackend(testBackend());
    ScanRecorder recorder;
    facade.library()->setScanExecutor([&recorder](const QString &rootPath, seriona::scanner::ScanMode mode) {
        return recorder.record(rootPath, mode);
    });
    facade.navigation()->showLyricsView();
    facade.navigation()->syncSidebarForDockCapability(true);

    QCOMPARE(facade.restorePlaylistFromStartup(), false);

    QCOMPARE(recorder.roots.size(), std::size_t{0});
    QCOMPARE(facade.navigation()->startupScreenVisible(), true);
    QCOMPARE(facade.navigation()->currentView(), QStringLiteral("lyrics"));
    QCOMPARE(facade.navigation()->sidebarOpen(), true);
    QCOMPARE(facade.library()->savedRootPath(), QString());
    QCOMPARE(facade.library()->scanStatus(), QStringLiteral("error"));
    QVERIFY(facade.library()->lastError().contains(messageFragment));
    QCOMPARE(hasSavedRoot(), false);
}

QTEST_GUILESS_MAIN(StartupRestoreTest)

#include "tst_startup_restore.moc"
