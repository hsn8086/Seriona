// 侧边栏文件夹/队列视图切换（T15）测试：加载 qml/components/Sidebar.qml，
// 断言默认文件夹视图、切换按钮状态、队列空状态说明、注入队列后条目渲染、
// 队列右键菜单"从队列移除"项在队列上下文可见（TrackContextMenu queueContext）。
//
// 测试用真实 AppFacade（关闭后端桥自启，避免启动线程）+ LibraryController
// 经 context property 注入；QSettings 经 seriona.settingsFileForTests 指向
// 临时文件，避免污染用户配置。
#include "app_facade.h"
#include "library_model.h"
#include "seriona/control/control_contracts.h"

#include <QColor>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QWindow>

#include <memory>

namespace {

constexpr auto kHostQml = R"(
import QtQuick
import QtQuick.Window
import Seriona

Window {
    id: testWindow
    width: 900
    height: 700
    visible: true

    Sidebar {
        id: sidebar
        objectName: "sidebar"
        appFacade: appFacadeContext
        libraryController: libraryContext
    }
}
)";

constexpr auto kSettingsFileProperty = "seriona.settingsFileForTests";

QVariantMap makeQueueEntry(const QString &trackId, const QString &title, const QString &artist, bool isPlaying)
{
    return QVariantMap{
        {QStringLiteral("trackId"), trackId},
        {QStringLiteral("nodeId"), trackId},
        {QStringLiteral("title"), title},
        {QStringLiteral("artist"), artist},
        {QStringLiteral("isPlaying"), isPlaying},
    };
}

} // namespace

class SidebarQueueSwitchTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultViewIsFolder();
    void switchToQueueShowsEmptyGuide();
    void switchBackToFolderRestoresFolderView();
    void queueEntriesRenderWithPlayingHighlight();
    void queueContextMenuItemVisibleOnlyInQueueContext();

private:
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;
    QQuickItem *sidebar = nullptr;
    QTemporaryDir tempDir;
    std::unique_ptr<Seriona::App::AppFacade> facade;
    Seriona::App::LibraryController libraryController;

    QObject *findItem(const QString &objectName) const;
    QQuickItem *switchButton(const QString &objectName) const;
    void clickSwitch(const QString &objectName);
    void applyQueueSnapshot(const QVariantList &entries, const QString &currentTrackId = QString());
};

void SidebarQueueSwitchTest::initTestCase()
{
    QCoreApplication::instance()->setProperty(kSettingsFileProperty,
        tempDir.filePath(QStringLiteral("queue-view.ini")));
    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", false);

    facade = std::make_unique<Seriona::App::AppFacade>();
    QCOMPARE(facade->backendBridgeStartedForTests(), false);

    engine.rootContext()->setContextProperty(QStringLiteral("appFacadeContext"), facade.get());
    engine.rootContext()->setContextProperty(QStringLiteral("libraryContext"), &libraryController);
    engine.addImportPath(QCoreApplication::applicationDirPath());
    engine.loadData(QByteArray(kHostQml), QUrl(QStringLiteral("qrc:/seriona_sidebar_queue_test.qml")));

    QVERIFY2(!engine.rootObjects().isEmpty(), "host Window failed to load");
    window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY2(window != nullptr, "host root is not a QQuickWindow");
    QVERIFY2(window->isVisible(), "host window is not visible");

    sidebar = qobject_cast<QQuickItem *>(window->findChild<QObject *>(QStringLiteral("sidebar")));
    QVERIFY2(sidebar != nullptr, "Sidebar instance not found");
}

QObject *SidebarQueueSwitchTest::findItem(const QString &objectName) const
{
    return window->findChild<QObject *>(objectName);
}

QQuickItem *SidebarQueueSwitchTest::switchButton(const QString &objectName) const
{
    return qobject_cast<QQuickItem *>(findItem(objectName));
}

void SidebarQueueSwitchTest::clickSwitch(const QString &objectName)
{
    QQuickItem *button = switchButton(objectName);
    QVERIFY2(button != nullptr, qPrintable(QStringLiteral("switch button %1 not found").arg(objectName)));
    const QPointF center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
}

void SidebarQueueSwitchTest::applyQueueSnapshot(const QVariantList &entries, const QString &currentTrackId)
{
    // 先注入曲库快照（同一批 trackId 可解析出标题/艺术家），队列映射依赖曲库
    seriona::control::LibraryStateSnapshot library;
    seriona::scanner::PlaylistTreeSnapshot tree;
    tree.rootNodeId = std::string{"root"};
    for (const QVariant &entry : entries) {
        const QVariantMap map = entry.toMap();
        seriona::scanner::SongMetadata song;
        song.trackId = map.value(QStringLiteral("trackId")).toString().toStdString();
        song.title = map.value(QStringLiteral("title")).toString().toStdString();
        song.artist = map.value(QStringLiteral("artist")).toString().toStdString();
        seriona::scanner::PlaylistNode node;
        node.nodeId = map.value(QStringLiteral("nodeId")).toString().toStdString();
        node.kind = seriona::scanner::PlaylistNodeKind::Track;
        node.displayName = song.title;
        node.song = std::move(song);
        tree.nodes.push_back(std::move(node));
    }
    library.libraryTree = std::move(tree);
    facade->library()->applyLibraryStateSnapshot(library);

    seriona::control::PlayerStateSnapshot snapshot;
    snapshot.playback.state = seriona::control::PlaybackStatus::Playing;
    if (!currentTrackId.isEmpty()) {
        snapshot.currentTrack = seriona::control::TrackIdentity{};
        snapshot.currentTrack->trackId = currentTrackId.toStdString();
    }
    for (const QVariant &entry : entries) {
        const QVariantMap map = entry.toMap();
        seriona::control::QueueEntry queueEntry;
        queueEntry.trackId = map.value(QStringLiteral("trackId")).toString().toStdString();
        queueEntry.nodeId = map.value(QStringLiteral("nodeId")).toString().toStdString();
        snapshot.queueEntries.push_back(std::move(queueEntry));
    }
    facade->playback()->applyPlayerStateSnapshot(snapshot, &library);
}

void SidebarQueueSwitchTest::defaultViewIsFolder()
{
    QCOMPARE(sidebar->property("queueViewActive").toBool(), false);

    QQuickItem *playlistView = qobject_cast<QQuickItem *>(findItem(QStringLiteral("playlistView")));
    QVERIFY2(playlistView != nullptr, "folder playlist view not found");
    QVERIFY(playlistView->isVisible());

    QQuickItem *queueListView = qobject_cast<QQuickItem *>(findItem(QStringLiteral("queueListView")));
    QVERIFY2(queueListView != nullptr, "queue view not found");
    QVERIFY(!queueListView->isVisible());
}

void SidebarQueueSwitchTest::switchToQueueShowsEmptyGuide()
{
    clickSwitch(QStringLiteral("queueViewButton"));
    QCOMPARE(sidebar->property("queueViewActive").toBool(), true);

    QQuickItem *queueListView = qobject_cast<QQuickItem *>(findItem(QStringLiteral("queueListView")));
    QVERIFY(queueListView->isVisible());
    QQuickItem *playlistView = qobject_cast<QQuickItem *>(findItem(QStringLiteral("playlistView")));
    QVERIFY(!playlistView->isVisible());

    QObject *emptyState = findItem(QStringLiteral("queueEmptyState"));
    QVERIFY2(emptyState != nullptr, "queue empty state not found");
    QTRY_VERIFY(qobject_cast<QQuickItem *>(emptyState)->isVisible());
    // emptyTitle/emptyHint 定义在 QueueView 根（queueListView），空状态 Column 只是展示容器
    const QString title = queueListView->property("emptyTitle").toString();
    const QString hint = queueListView->property("emptyHint").toString();
    QVERIFY2(title.contains(QStringLiteral("队列为空")), qPrintable(title));
    QVERIFY2(hint.contains(QStringLiteral("添加到下一首播放")), qPrintable(hint));
}

void SidebarQueueSwitchTest::switchBackToFolderRestoresFolderView()
{
    clickSwitch(QStringLiteral("folderViewButton"));
    QCOMPARE(sidebar->property("queueViewActive").toBool(), false);

    QQuickItem *playlistView = qobject_cast<QQuickItem *>(findItem(QStringLiteral("playlistView")));
    QVERIFY(playlistView->isVisible());
    QQuickItem *queueListView = qobject_cast<QQuickItem *>(findItem(QStringLiteral("queueListView")));
    QVERIFY(!queueListView->isVisible());
}

void SidebarQueueSwitchTest::queueEntriesRenderWithPlayingHighlight()
{
    QVariantList entries;
    entries.append(makeQueueEntry(QStringLiteral("t-1"), QStringLiteral("队列第一首"), QStringLiteral("歌手甲"), true));
    entries.append(makeQueueEntry(QStringLiteral("t-2"), QStringLiteral("Queue Two"), QString(), false));
    applyQueueSnapshot(entries, QStringLiteral("t-1"));

    clickSwitch(QStringLiteral("queueViewButton"));
    QObject *queueListView = findItem(QStringLiteral("queueListView"));
    QTRY_COMPARE(queueListView->property("count").toInt(), 2);

    QObject *emptyState = findItem(QStringLiteral("queueEmptyState"));
    QVERIFY(!qobject_cast<QQuickItem *>(emptyState)->isVisible());

    QObject *title0 = findItem(QStringLiteral("queueTitle0"));
    QVERIFY2(title0 != nullptr, "queue delegate title 0 missing");
    QCOMPARE(title0->property("text").toString(), QStringLiteral("队列第一首"));
    QObject *artist0 = findItem(QStringLiteral("queueArtist0"));
    QCOMPARE(artist0->property("text").toString(), QStringLiteral("歌手甲"));
    QCOMPARE(title0->property("color").value<QColor>(), QColor(QStringLiteral("#5B9DFF")));

    QObject *title1 = findItem(QStringLiteral("queueTitle1"));
    QVERIFY2(title1 != nullptr, "queue delegate title 1 missing");
    QCOMPARE(title1->property("text").toString(), QStringLiteral("Queue Two"));
    QVERIFY(title1->property("color").value<QColor>() != QColor(QStringLiteral("#5B9DFF")));
}

void SidebarQueueSwitchTest::queueContextMenuItemVisibleOnlyInQueueContext()
{
    QVariantList entries;
    entries.append(makeQueueEntry(QStringLiteral("t-1"), QStringLiteral("队列第一首"), QString(), false));
    applyQueueSnapshot(entries);

    clickSwitch(QStringLiteral("queueViewButton"));
    QObject *queueListView = findItem(QStringLiteral("queueListView"));
    QTRY_COMPARE(queueListView->property("count").toInt(), 1);

    // 队列条目右键 → 菜单打开，"从队列移除"项可见（queueContext 生效）
    QQuickItem *delegate = qobject_cast<QQuickItem *>(findItem(QStringLiteral("queueDelegate0")));
    QVERIFY2(delegate != nullptr, "queue delegate 0 not found");
    const QPointF delegateCenter = delegate->mapToScene(QPointF(delegate->width() / 2, delegate->height() / 2));
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, delegateCenter.toPoint());

    QObject *removeFromQueueItem = findItem(QStringLiteral("removeFromQueueItem"));
    QVERIFY2(removeFromQueueItem != nullptr, "remove from queue menu item not found");
    QTRY_VERIFY(qobject_cast<QQuickItem *>(removeFromQueueItem)->isVisible());

    QObject *contextMenu = findItem(QStringLiteral("trackContextMenu"));
    QVERIFY2(contextMenu != nullptr, "track context menu not found");
    QObject *contextMenuPopup = findItem(QStringLiteral("trackContextMenuPopup"));
    QVERIFY2(contextMenuPopup != nullptr, "track context menu popup not found");
    QVERIFY(QMetaObject::invokeMethod(contextMenuPopup, "close"));

    // 切回文件夹视图，文件夹条目右键 → 菜单中"从队列移除"隐藏
    clickSwitch(QStringLiteral("folderViewButton"));
    QCOMPARE(sidebar->property("queueViewActive").toBool(), false);
    QTRY_VERIFY(!qobject_cast<QQuickItem *>(removeFromQueueItem)->isVisible());
}

QTEST_MAIN(SidebarQueueSwitchTest)

#include "tst_sidebar_queue_switch.moc"
