#pragma once

#include "app_settings_storage.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

#include <functional>

namespace Seriona::App {

class LibraryController;

class NavigationController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(QString capability READ capability CONSTANT)
    Q_PROPERTY(QString currentView READ currentView NOTIFY currentViewChanged)
    Q_PROPERTY(bool sidebarOpen READ sidebarOpen NOTIFY sidebarOpenChanged)
    Q_PROPERTY(bool startupScreenVisible READ startupScreenVisible NOTIFY startupScreenVisibleChanged)
    Q_PROPERTY(bool manualSidebarToggle READ manualSidebarToggle NOTIFY manualSidebarToggleChanged)
    QML_ELEMENT
    QML_UNCREATABLE("NavigationController is owned by AppFacade")

public:
    using StartupScanInvoker = std::function<bool(const QUrl &)>;

    explicit NavigationController(QObject *parent = nullptr);

    bool ready() const;
    QString capability() const;
    QString currentView() const;
    bool sidebarOpen() const;
    bool startupScreenVisible() const;
    bool manualSidebarToggle() const;

    bool scanLibrary(LibraryController &library, const QUrl &rootUrl);
    bool restorePlaylistFromStartup(LibraryController &library, const StartupScanInvoker &scanLibrary);

    // 设置存储后端注入（AppFacade 接入后端时注入 BackendBridge 实现；
    // 不注入时回退内存存储）。
    void setSettingsStorageBackend(AppSettingsBackend backend);

    Q_INVOKABLE void restorePlaylistFromStartup();
    Q_INVOKABLE void addFolderFromStartup();
    Q_INVOKABLE void showPlaybackView();
    Q_INVOKABLE void showLyricsView();
    Q_INVOKABLE void toggleSidebar();
    Q_INVOKABLE void closeSidebar();
    Q_INVOKABLE void clearManualSidebarToggle();
    Q_INVOKABLE void syncSidebarForDockCapability(bool dockCapable);

signals:
    void currentViewChanged();
    void sidebarOpenChanged();
    void startupScreenVisibleChanged();
    void manualSidebarToggleChanged();

private:
    void setCurrentView(const QString &view);
    void setSidebarOpen(bool open);
    void setStartupScreenVisible(bool visible);
    void setManualSidebarToggle(bool manual);
    void enterMainShell();

    QString m_currentView = QStringLiteral("playback");
    bool m_sidebarOpen = false;
    bool m_startupScreenVisible = true;
    bool m_manualSidebarToggle = false;
    AppSettingsStorage m_settingsStorage;
};

}
