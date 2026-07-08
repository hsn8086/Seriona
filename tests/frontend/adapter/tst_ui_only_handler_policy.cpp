#include <QFile>
#include <QString>
#include <QtTest/QTest>

#include <initializer_list>

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

void expectContainsAll(const QString &source, const std::initializer_list<const char *> &needles)
{
    for (const char *needle : needles) {
        expectContains(source, QString::fromUtf8(needle));
    }
}

void expectInOrder(const QString &source, const std::initializer_list<const char *> &needles)
{
    qsizetype previous = -1;
    for (const char *needle : needles) {
        const qsizetype current = source.indexOf(QString::fromUtf8(needle), previous + 1);
        QVERIFY2(current >= 0, qPrintable(QStringLiteral("Missing expected layout marker in order: %1").arg(QString::fromUtf8(needle))));
        QVERIFY2(current >= previous, qPrintable(QStringLiteral("Unexpected layout marker order: %1").arg(QString::fromUtf8(needle))));
        previous = current;
    }
}

QString menuBlock(const QString &source, const QString &label, const QString &nextLabel)
{
    const QString startNeedle = QStringLiteral("text: qsTr(\"%1\")").arg(label);
    const QString endNeedle = QStringLiteral("text: qsTr(\"%1\")").arg(nextLabel);
    const qsizetype start = source.indexOf(startNeedle);
    if (start < 0) {
        QTest::qFail(qPrintable(QStringLiteral("Missing menu item: %1").arg(label)), __FILE__, __LINE__);
        return {};
    }

    const qsizetype end = source.indexOf(endNeedle, start + startNeedle.size());
    if (end <= start) {
        QTest::qFail(qPrintable(QStringLiteral("Missing menu item after %1: %2").arg(label, nextLabel)), __FILE__, __LINE__);
        return {};
    }
    return source.mid(start, end - start);
}

void expectUnsupportedOnlySortAction(const QString &source, const QString &label, const QString &nextLabel)
{
    const QString block = menuBlock(source, label, nextLabel);
    expectContains(block, QStringLiteral("onTriggered: root.showUnsupportedFeedback(qsTr(\"%1\"))").arg(label));
    expectAbsent(block, QStringLiteral("libraryController."));
    expectAbsent(block, QStringLiteral("appFacade.scanLibrary"));
    expectAbsent(block, QStringLiteral("submitCommand"));
    expectAbsent(block, QStringLiteral("sort"));
}

QString inlineMenuItemBlock(const QString &source, const QString &label)
{
    const QString startNeedle = QStringLiteral("BubbleMenuItem { text: qsTr(\"%1\")").arg(label);
    const qsizetype start = source.indexOf(startNeedle);
    if (start < 0) {
        QTest::qFail(qPrintable(QStringLiteral("Missing inline menu item: %1").arg(label)), __FILE__, __LINE__);
        return {};
    }

    const qsizetype end = source.indexOf(QLatin1Char('\n'), start);
    if (end <= start) {
        QTest::qFail(qPrintable(QStringLiteral("Missing inline menu item terminator: %1").arg(label)), __FILE__, __LINE__);
        return {};
    }
    return source.mid(start, end - start);
}

void expectUnsupportedOnlyMainAction(const QString &source, const QString &label)
{
    const QString block = inlineMenuItemBlock(source, label);
    expectContains(block, QStringLiteral("onTriggered: root.showUnsupportedFeedback(qsTr(\"%1\"))").arg(label));
    expectAbsent(block, QStringLiteral("playbackController."));
    expectAbsent(block, QStringLiteral("lyricsState."));
    expectAbsent(block, QStringLiteral("libraryController."));
    expectAbsent(block, QStringLiteral("appFacade."));
    expectAbsent(block, QStringLiteral("exitRequested"));
    expectAbsent(block, QStringLiteral("submitCommand"));
}

}

class UiOnlyHandlerPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void uiOnlyHandlersDoNotUseBackendCommands();
    void qmlLayoutSourceContractsStayStable();
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

    expectUnsupportedOnlyMainAction(mainContentQml, QStringLiteral("Crossfade"));
    expectUnsupportedOnlyMainAction(mainContentQml, QStringLiteral("Gapless Playback"));
    expectUnsupportedOnlyMainAction(mainContentQml, QStringLiteral("ReplayGain"));
    expectUnsupportedOnlyMainAction(mainContentQml, QStringLiteral("Equalizer"));
    expectUnsupportedOnlyMainAction(mainContentQml, QStringLiteral("About Seriona"));
    expectContains(mainContentQml, QStringLiteral("BubbleMenuItem { text: qsTr(\"Exit\"); onTriggered: { mainMenu.close(); root.exitRequested(); } }"));
    expectAbsent(mainContentQml, QStringLiteral("showUnsupportedFeedback(qsTr(\"Exit\"))"));
    expectAbsent(mainContentQml, QStringLiteral("submitCommand"));

    expectContains(sidebarQml, QStringLiteral("function showUnsupportedFeedback(actionName)"));
    expectContains(sidebarQml, QStringLiteral("root.appFacade.notifications.showUnsupportedAction(actionName);"));
    expectContains(sidebarQml, QStringLiteral("onTriggered: root.showUnsupportedFeedback(qsTr(\"Sort by Name\"))"));
    expectContains(sidebarQml, QStringLiteral("onTriggered: root.showUnsupportedFeedback(qsTr(\"Sort by Date\"))"));
    expectUnsupportedOnlySortAction(sidebarQml, QStringLiteral("Sort by Name"), QStringLiteral("Sort by Date"));
    expectUnsupportedOnlySortAction(sidebarQml, QStringLiteral("Sort by Date"), QStringLiteral("Refresh"));
    expectAbsent(sidebarQml, QStringLiteral("sortBy"));
    expectAbsent(sidebarQml, QStringLiteral("setSort"));
    expectAbsent(sidebarQml, QStringLiteral("sortOrder"));
    expectAbsent(sidebarQml, QStringLiteral("submitCommand"));
}

void UiOnlyHandlerPolicyTest::qmlLayoutSourceContractsStayStable()
{
    const QString themeQml = sourceFile(QStringLiteral("qml/theme/Theme.qml"));
    const QString mainQml = sourceFile(QStringLiteral("qml/Main.qml"));
    const QString mainContentQml = sourceFile(QStringLiteral("qml/views/MainContent.qml"));
    const QString sidebarQml = sourceFile(QStringLiteral("qml/components/Sidebar.qml"));
    const QString startupViewQml = sourceFile(QStringLiteral("qml/views/StartupView.qml"));

    expectContains(themeQml, QStringLiteral("readonly property int sidebarWidth: 350"));

    expectContainsAll(mainQml, {
        "width: 360",
        "height: 720",
        "minimumWidth: 360",
        "minimumHeight: 720",
        "readonly property int sidebarWidth: 350",
        "readonly property int playerMinWidth: 450",
        "Layout.preferredHeight: 40",
        "WindowControls {",
        "targetWindow: window",
        "onCloseRequested: window.requestApplicationClose()",
        "onPressed: window.startSystemMove()",
        "onPressed: window.startSystemResize(edgeFlag)",
        "Sidebar {",
        "MainContent {",
        "StartupView {"
    });
    expectInOrder(mainQml, {
        "Sidebar {",
        "MainContent {",
        "StartupView {"
    });

    expectContainsAll(mainContentQml, {
        "state: \"playback\"",
        "name: \"playback\"",
        "name: \"lyrics\"",
        "id: positionHelper",
        "width: 320",
        "id: coverContainer",
        "width: 240",
        "height: 240",
        "sourceSize.width: 240",
        "sourceSize.height: 240",
        "id: metadataContainer",
        "anchors.top: coverContainer.bottom",
        "anchors.horizontalCenter: positionHelper.horizontalCenter",
        "id: lyricsContainer",
        "anchors.top: coverContainer.bottom",
        "anchors.bottom: linearProgressContainer.top",
        "id: waveformProgressContainer",
        "id: linearProgressContainer",
        "id: controlsContainer",
        "id: volumeContainer",
        "id: bottomRowContainer",
        "id: toggleTranslationBtn",
        "PropertyChanges {",
        "target: coverContainer",
        "width: 44",
        "height: 44",
        "target: coverRect",
        "radius: 12",
        "target: coverIcon",
        "font.pixelSize: 20"
    });
    expectInOrder(mainContentQml, {
        "id: positionHelper",
        "id: coverContainer",
        "id: metadataContainer",
        "id: lyricsContainer",
        "id: waveformProgressContainer",
        "id: linearProgressContainer",
        "id: controlsContainer",
        "id: volumeContainer",
        "id: bottomRowContainer",
        "id: toggleTranslationBtn"
    });

    expectContainsAll(sidebarQml, {
        "width: Theme.sidebarWidth",
        "Accessible.role: Accessible.ListItem",
        "Accessible.name: isFolder ? name : title",
        "id: playlistView",
        "ItemDelegate {",
        "height: 72",
        "Layout.preferredWidth: 44",
        "Layout.preferredHeight: 44",
        "sourceSize.width: delegate.isFolder ? 24 : 44",
        "sourceSize.height: delegate.isFolder ? 24 : 44",
        "id: sidebarFolderDialog",
        "id: fab"
    });
    expectInOrder(sidebarQml, {
        "id: playlistView",
        "ItemDelegate {",
        "id: sidebarFolderDialog",
        "id: fab"
    });

    expectContainsAll(startupViewQml, {
        "width: Math.min(parent.width - Theme.paddingLarge * 2, 320)",
        "Layout.preferredWidth: 120",
        "Layout.preferredHeight: 120",
        "Layout.fillWidth: true",
        "id: restoreButton",
        "id: addFolderButton"
    });
}

QTEST_GUILESS_MAIN(UiOnlyHandlerPolicyTest)

#include "tst_ui_only_handler_policy.moc"
