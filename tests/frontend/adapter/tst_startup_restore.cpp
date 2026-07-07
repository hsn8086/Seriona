#include "app_facade.h"

#include "seriona/control/control_contracts.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QUrl>
#include <QVariant>
#include <QtTest/QTest>

#include <memory>
#include <vector>

namespace {
constexpr auto kBackendBridgeAutostartProperty = "seriona.backendBridgeAutostartEnabled";
constexpr auto kSettingsFileProperty = "seriona.settingsFileForTests";
constexpr auto kLastLibraryRootKey = "library/lastScanRoot";

using Seriona::App::AppFacade;
using seriona::control::MediaControllerCommandResult;
using seriona::control::MediaControllerErrorCode;

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
    MediaControllerCommandResult result = acceptedResult();

    MediaControllerCommandResult record(const QString &rootPath)
    {
        roots.push_back(rootPath);
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

    std::unique_ptr<QTemporaryDir> m_settingsDir;
    QString m_settingsFile;
};

void StartupRestoreTest::init()
{
    QCoreApplication::instance()->setProperty(kBackendBridgeAutostartProperty, false);

    m_settingsDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settingsDir->isValid());
    m_settingsFile = m_settingsDir->filePath(QStringLiteral("settings.ini"));
    QCoreApplication::instance()->setProperty(kSettingsFileProperty, m_settingsFile);

    QSettings settings(m_settingsFile, QSettings::IniFormat);
    settings.clear();
    settings.sync();
}

void StartupRestoreTest::cleanup()
{
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    settings.clear();
    settings.sync();

    QCoreApplication::instance()->setProperty(kSettingsFileProperty, QVariant{});
    QCoreApplication::instance()->setProperty(kBackendBridgeAutostartProperty, QVariant{});
    m_settingsFile.clear();
    m_settingsDir.reset();
}

void StartupRestoreTest::scanLibraryPersistsRootForStartupRestore()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    const QString canonicalRoot = QFileInfo(musicDir.path()).absoluteFilePath();

    AppFacade facade;
    ScanRecorder recorder;
    facade.library()->setScanExecutor([&recorder](const QString &rootPath) {
        return recorder.record(rootPath);
    });

    QVERIFY(facade.scanLibrary(QUrl::fromLocalFile(musicDir.path())));

    QCOMPARE(recorder.roots, std::vector<QString>{canonicalRoot});
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
    ScanRecorder recorder;
    facade.library()->setScanExecutor([&recorder](const QString &rootPath) {
        return recorder.record(rootPath);
    });

    QVERIFY(facade.restorePlaylistFromStartup());

    QCOMPARE(recorder.roots, std::vector<QString>{canonicalRoot});
    QCOMPARE(facade.library()->savedRootPath(), canonicalRoot);
    QCOMPARE(savedRoot(), canonicalRoot);
    QCOMPARE(facade.navigation()->startupScreenVisible(), false);
    QCOMPARE(facade.navigation()->currentView(), QStringLiteral("playback"));

    QTemporaryDir rejectedDir;
    QVERIFY(rejectedDir.isValid());
    const QString rejectedRoot = QFileInfo(rejectedDir.path()).absoluteFilePath();
    writeSavedRoot(rejectedRoot);

    AppFacade rejectedFacade;
    ScanRecorder rejectedRecorder;
    rejectedRecorder.result = rejectedResult("startup restore scan rejected");
    rejectedFacade.library()->setScanExecutor([&rejectedRecorder](const QString &rootPath) {
        return rejectedRecorder.record(rootPath);
    });
    rejectedFacade.navigation()->showLyricsView();
    rejectedFacade.navigation()->syncSidebarForDockCapability(true);

    QCOMPARE(rejectedFacade.restorePlaylistFromStartup(), false);

    QCOMPARE(rejectedRecorder.roots, std::vector<QString>{rejectedRoot});
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
    ScanRecorder recorder;
    facade.library()->setScanExecutor([&recorder](const QString &rootPath) {
        return recorder.record(rootPath);
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

void StartupRestoreTest::writeSavedRoot(const QString &rootPath)
{
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    settings.setValue(kLastLibraryRootKey, rootPath);
    settings.sync();
}

bool StartupRestoreTest::hasSavedRoot() const
{
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    return settings.contains(kLastLibraryRootKey);
}

QString StartupRestoreTest::savedRoot() const
{
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    return settings.value(kLastLibraryRootKey).toString();
}

QTEST_GUILESS_MAIN(StartupRestoreTest)

#include "tst_startup_restore.moc"
