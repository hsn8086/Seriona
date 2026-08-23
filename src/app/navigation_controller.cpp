#include "navigation_controller.h"

#include "library_model.h"

#include <QFileInfo>
#include <QVariant>

namespace Seriona::App {

namespace {

constexpr auto kLastLibraryRootGroup = "library";
constexpr auto kLastLibraryRootKey = "lastScanRoot";

}

NavigationController::NavigationController(QObject *parent)
    : QObject(parent)
{
}

bool NavigationController::ready() const
{
    return true;
}

QString NavigationController::capability() const
{
    return QStringLiteral("startup-navigation-and-shell-state");
}

QString NavigationController::currentView() const
{
    return m_currentView;
}

bool NavigationController::sidebarOpen() const
{
    return m_sidebarOpen;
}

bool NavigationController::startupScreenVisible() const
{
    return m_startupScreenVisible;
}

bool NavigationController::manualSidebarToggle() const
{
    return m_manualSidebarToggle;
}

bool NavigationController::scanLibrary(LibraryController &library, const QUrl &rootUrl)
{
    if (!library.scanLibrary(rootUrl)) {
        return false;
    }

    const QString rootPath = library.savedRootPath();
    if (!rootPath.isEmpty()) {
        m_settingsStorage.write(QString::fromUtf8(kLastLibraryRootGroup),
                                QString::fromUtf8(kLastLibraryRootKey),
                                rootPath);
    }
    return true;
}

bool NavigationController::restorePlaylistFromStartup(
    LibraryController &library,
    const StartupScanInvoker &scanLibrary)
{
    const QString rootPath = m_settingsStorage.read(QString::fromUtf8(kLastLibraryRootGroup),
                                                    QString::fromUtf8(kLastLibraryRootKey),
                                                    QString())
                                 .toString();
    if (rootPath.isEmpty()) {
        m_settingsStorage.remove(QString::fromUtf8(kLastLibraryRootGroup),
                                 QString::fromUtf8(kLastLibraryRootKey));
        library.clearSavedRootPath(tr("请先添加音乐文件夹"));
        return false;
    }

    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.isDir()) {
        m_settingsStorage.remove(QString::fromUtf8(kLastLibraryRootGroup),
                                 QString::fromUtf8(kLastLibraryRootKey));
        library.clearSavedRootPath(tr("上次曲库文件夹不可用，请重新选择文件夹"));
        return false;
    }

    if (!scanLibrary || !scanLibrary(QUrl::fromLocalFile(rootInfo.absoluteFilePath()))) {
        return false;
    }

    restorePlaylistFromStartup();
    return true;
}

void NavigationController::setSettingsStorageBackend(AppSettingsBackend backend)
{
    m_settingsStorage.setBackend(std::move(backend));
}

void NavigationController::restorePlaylistFromStartup()
{
    enterMainShell();
}

void NavigationController::addFolderFromStartup()
{
    enterMainShell();
}

void NavigationController::showPlaybackView()
{
    setCurrentView(QStringLiteral("playback"));
}

void NavigationController::showLyricsView()
{
    setCurrentView(QStringLiteral("lyrics"));
}

void NavigationController::toggleSidebar()
{
    setManualSidebarToggle(true);
    setSidebarOpen(!m_sidebarOpen);
}

void NavigationController::closeSidebar()
{
    setManualSidebarToggle(true);
    setSidebarOpen(false);
}

void NavigationController::clearManualSidebarToggle()
{
    setManualSidebarToggle(false);
}

void NavigationController::syncSidebarForDockCapability(bool dockCapable)
{
    setManualSidebarToggle(false);
    setSidebarOpen(dockCapable);
}

void NavigationController::setCurrentView(const QString &view)
{
    if (m_currentView == view) {
        return;
    }

    m_currentView = view;
    emit currentViewChanged();
}

void NavigationController::setSidebarOpen(bool open)
{
    if (m_sidebarOpen == open) {
        return;
    }

    m_sidebarOpen = open;
    emit sidebarOpenChanged();
}

void NavigationController::setStartupScreenVisible(bool visible)
{
    if (m_startupScreenVisible == visible) {
        return;
    }

    m_startupScreenVisible = visible;
    emit startupScreenVisibleChanged();
}

void NavigationController::setManualSidebarToggle(bool manual)
{
    if (m_manualSidebarToggle == manual) {
        return;
    }

    m_manualSidebarToggle = manual;
    emit manualSidebarToggleChanged();
}

void NavigationController::enterMainShell()
{
    setStartupScreenVisible(false);
    setCurrentView(QStringLiteral("playback"));
}

}
