// 队列视图组件（T15）测试：加载 qml/components/QueueView.qml，断言
// 空队列显示引导文案、条目渲染（标题/艺术家/播放中高亮）、移除按钮按
// 队列下标发 removeRequested 信号（index 即 RemoveFromQueue 的 queueIndex）。
//
// 依赖构建目录的 Seriona QML 模块产物（build/Seriona/qmldir），
// 因此 CMake 侧对测试目标 add_dependencies(appSeriona) 保证产物先就绪。
#include <QColor>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

namespace {

constexpr auto kHostQml = R"(
import QtQuick
import QtQuick.Window
import Seriona

Window {
    id: testWindow
    width: 400
    height: 600
    visible: true

    QueueView {
        id: queueView
        objectName: "queueView"
        anchors.fill: parent
        queueEntries: []
    }
}
)";

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

class QueueViewTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void emptyQueueShowsGuideText();
    void entriesRenderTitleArtistAndPlayingHighlight();
    void removeButtonEmitsIndexedRequest();

private:
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;
    QObject *queueView = nullptr;

    void setQueueEntries(const QVariantList &entries);
    QObject *findItem(const QString &objectName) const;
};

void QueueViewTest::initTestCase()
{
    engine.addImportPath(QCoreApplication::applicationDirPath());
    engine.loadData(QByteArray(kHostQml), QUrl(QStringLiteral("qrc:/seriona_queue_view_test.qml")));

    QVERIFY2(!engine.rootObjects().isEmpty(), "host Window failed to load");
    window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY2(window != nullptr, "host root is not a QQuickWindow");
    QVERIFY2(window->isVisible(), "host window is not visible");

    queueView = window->findChild<QObject *>(QStringLiteral("queueView"));
    QVERIFY2(queueView != nullptr, "QueueView instance not found");
}

void QueueViewTest::setQueueEntries(const QVariantList &entries)
{
    QVERIFY(queueView->setProperty("queueEntries", QVariant::fromValue(entries)));
}

QObject *QueueViewTest::findItem(const QString &objectName) const
{
    return window->findChild<QObject *>(objectName);
}

void QueueViewTest::emptyQueueShowsGuideText()
{
    setQueueEntries({});

    QCOMPARE(queueView->property("count").toInt(), 0);

    QObject *emptyState = findItem(QStringLiteral("queueEmptyState"));
    QVERIFY2(emptyState != nullptr, "queue empty state not found");
    QTRY_VERIFY(qobject_cast<QQuickItem *>(emptyState)->isVisible());

    QObject *queueList = findItem(QStringLiteral("queueList"));
    QVERIFY2(queueList != nullptr, "queue list not found");
    QTRY_VERIFY(!qobject_cast<QQuickItem *>(queueList)->isVisible());

    const QString title = queueView->property("emptyTitle").toString();
    const QString hint = queueView->property("emptyHint").toString();
    QVERIFY2(title.contains(QStringLiteral("队列为空")), qPrintable(title));
    QVERIFY2(hint.contains(QStringLiteral("添加到下一首播放")), qPrintable(hint));
}

void QueueViewTest::entriesRenderTitleArtistAndPlayingHighlight()
{
    QVariantList entries;
    entries.append(makeQueueEntry(QStringLiteral("t-1"), QStringLiteral("第一首"), QStringLiteral("歌手甲"), true));
    entries.append(makeQueueEntry(QStringLiteral("t-2"), QStringLiteral("Second"), QString(), false));
    setQueueEntries(entries);

    QCOMPARE(queueView->property("count").toInt(), 2);

    QObject *queueList = findItem(QStringLiteral("queueList"));
    QVERIFY(qobject_cast<QQuickItem *>(queueList)->isVisible());
    QObject *emptyState = findItem(QStringLiteral("queueEmptyState"));
    QVERIFY(!qobject_cast<QQuickItem *>(emptyState)->isVisible());

    QObject *title0 = findItem(QStringLiteral("queueTitle0"));
    QObject *artist0 = findItem(QStringLiteral("queueArtist0"));
    QVERIFY2(title0 != nullptr, "first delegate title missing");
    QVERIFY2(artist0 != nullptr, "first delegate artist missing");
    QCOMPARE(title0->property("text").toString(), QStringLiteral("第一首"));
    QCOMPARE(artist0->property("text").toString(), QStringLiteral("歌手甲"));

    // 当前播放曲目高亮（accent 色标题），非播放项为普通文本色
    const QColor accent = QColor(QStringLiteral("#5B9DFF"));
    const QColor titleColor0 = title0->property("color").value<QColor>();
    QCOMPARE(titleColor0, accent);

    QObject *title1 = findItem(QStringLiteral("queueTitle1"));
    QObject *artist1 = findItem(QStringLiteral("queueArtist1"));
    QVERIFY2(title1 != nullptr, "second delegate title missing");
    QVERIFY2(artist1 != nullptr, "second delegate artist missing");
    QCOMPARE(title1->property("text").toString(), QStringLiteral("Second"));
    QCOMPARE(artist1->property("text").toString(), QString());
    QVERIFY(title1->property("color").value<QColor>() != accent);
}

void QueueViewTest::removeButtonEmitsIndexedRequest()
{
    QVariantList entries;
    entries.append(makeQueueEntry(QStringLiteral("t-1"), QStringLiteral("第一首"), QString(), false));
    entries.append(makeQueueEntry(QStringLiteral("t-2"), QStringLiteral("第二首"), QString(), false));
    setQueueEntries(entries);

    QCOMPARE(queueView->property("count").toInt(), 2);

    QSignalSpy removeSpy(queueView, SIGNAL(removeRequested(int)));

    QQuickItem *removeButton = qobject_cast<QQuickItem *>(findItem(QStringLiteral("queueRemoveButton1")));
    QVERIFY2(removeButton != nullptr, "second delegate remove button missing");
    const QPointF center = removeButton->mapToScene(QPointF(removeButton->width() / 2, removeButton->height() / 2));
    QVERIFY(QMetaObject::invokeMethod(removeButton, "click"));

    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(removeSpy.at(0).at(0).toInt(), 1);
}

QTEST_MAIN(QueueViewTest)

#include "tst_queue_view.moc"
