#include "app_facade.h"

#include <QCoreApplication>
#include <QObject>
#include <QVariant>
#include <QtTest/QTest>

class AppFacadeSmokeModeTest : public QObject
{
    Q_OBJECT

private slots:
    void doesNotStartBackendBridgeWhenSmokeDisablesAutostart();
};

void AppFacadeSmokeModeTest::doesNotStartBackendBridgeWhenSmokeDisablesAutostart()
{
    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", false);

    Seriona::App::AppFacade facade;

    QCOMPARE(facade.backendBridgeStartedForTests(), false);

    QCoreApplication::instance()->setProperty("seriona.backendBridgeAutostartEnabled", QVariant{});
}

QTEST_GUILESS_MAIN(AppFacadeSmokeModeTest)

#include "tst_app_facade_smoke_mode.moc"
