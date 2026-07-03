#include <QFile>
#include <QString>
#include <QtTest/QTest>

#ifndef SERIONA_SOURCE_DIR
#error "SERIONA_SOURCE_DIR must point to the repository source root"
#endif

namespace {

QString sourceFile(const QString &relativePath)
{
    QFile file(QStringLiteral(SERIONA_SOURCE_DIR) + QLatin1Char('/') + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qFatal("Failed to open source fixture: %s", qPrintable(relativePath));
    }

    return QString::fromUtf8(file.readAll());
}

void expectContains(const QString &source, const QString &needle)
{
    QVERIFY2(source.contains(needle), qPrintable(QStringLiteral("Missing expected handler policy: %1").arg(needle)));
}

void expectAbsent(const QString &source, const QString &needle)
{
    QVERIFY2(!source.contains(needle), qPrintable(QStringLiteral("Unexpected backend/fake handler policy: %1").arg(needle)));
}

}

class UiOnlyHandlerPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void uiOnlyHandlersDoNotUseBackendCommands();
};

void UiOnlyHandlerPolicyTest::uiOnlyHandlersDoNotUseBackendCommands()
{
    const QString mainQml = sourceFile(QStringLiteral("qml/Main.qml"));
    const QString mainContentQml = sourceFile(QStringLiteral("qml/views/MainContent.qml"));
    const QString sidebarQml = sourceFile(QStringLiteral("qml/components/Sidebar.qml"));
    const QString windowControlsQml = sourceFile(QStringLiteral("qml/components/WindowControls.qml"));

    expectContains(mainQml, QStringLiteral("window.startSystemMove()"));
    expectContains(mainQml, QStringLiteral("edgeFlag: Qt.TopEdge"));
    expectContains(mainQml, QStringLiteral("onCloseRequested: window.requestApplicationClose()"));
    expectContains(mainQml, QStringLiteral("onExitRequested: window.requestApplicationClose()"));
    expectAbsent(mainQml, QStringLiteral("submitCommand"));
    expectAbsent(mainQml, QStringLiteral("showUnsupportedFeedback(qsTr(\"Exit\"))"));

    expectContains(windowControlsQml, QStringLiteral("targetWindow.showMinimized()"));
    expectContains(windowControlsQml, QStringLiteral("targetWindow.showMaximized()"));
    expectContains(windowControlsQml, QStringLiteral("targetWindow.showNormal()"));
    expectContains(windowControlsQml, QStringLiteral("root.closeRequested()"));
    expectAbsent(windowControlsQml, QStringLiteral("appFacade"));
    expectAbsent(windowControlsQml, QStringLiteral("playbackController"));
    expectAbsent(windowControlsQml, QStringLiteral("libraryController"));
    expectAbsent(windowControlsQml, QStringLiteral("notifications"));

    expectContains(mainContentQml, QStringLiteral("BubbleMenuItem { text: qsTr(\"Crossfade\"); onTriggered: root.showUnsupportedFeedback(qsTr(\"Crossfade\")) }"));
    expectContains(mainContentQml, QStringLiteral("BubbleMenuItem { text: qsTr(\"Gapless Playback\"); onTriggered: root.showUnsupportedFeedback(qsTr(\"Gapless Playback\")) }"));
    expectContains(mainContentQml, QStringLiteral("BubbleMenuItem { text: qsTr(\"ReplayGain\"); onTriggered: root.showUnsupportedFeedback(qsTr(\"ReplayGain\")) }"));
    expectContains(mainContentQml, QStringLiteral("BubbleMenuItem { text: qsTr(\"Equalizer\"); onTriggered: root.showUnsupportedFeedback(qsTr(\"Equalizer\")) }"));
    expectContains(mainContentQml, QStringLiteral("BubbleMenuItem { text: qsTr(\"About Seriona\"); onTriggered: root.showUnsupportedFeedback(qsTr(\"About Seriona\")) }"));
    expectContains(mainContentQml, QStringLiteral("BubbleMenuItem { text: qsTr(\"Exit\"); onTriggered: { mainMenu.close(); root.exitRequested(); } }"));
    expectAbsent(mainContentQml, QStringLiteral("showUnsupportedFeedback(qsTr(\"Exit\"))"));
    expectAbsent(mainContentQml, QStringLiteral("submitCommand"));

    expectContains(sidebarQml, QStringLiteral("function showUnsupportedFeedback(actionName)"));
    expectContains(sidebarQml, QStringLiteral("root.appFacade.notifications.showUnsupportedAction(actionName);"));
    expectContains(sidebarQml, QStringLiteral("onTriggered: root.showUnsupportedFeedback(qsTr(\"Sort by Name\"))"));
    expectContains(sidebarQml, QStringLiteral("onTriggered: root.showUnsupportedFeedback(qsTr(\"Sort by Date\"))"));
    expectAbsent(sidebarQml, QStringLiteral("submitCommand"));
}

QTEST_GUILESS_MAIN(UiOnlyHandlerPolicyTest)

#include "tst_ui_only_handler_policy.moc"
