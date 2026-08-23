#include "track_stats_controller.h"

#include "app_settings_storage.h"

#include <QHash>
#include <QObject>
#include <QSignalSpy>
#include <QVariant>
#include <QtTest/QTest>

namespace {

QString storageKey(const QString &group, const QString &key)
{
    return group + QLatin1Char('\x1f') + key;
}

} // namespace

class TrackStatsControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void defaultsAreZeroAndUnrated();
    void recordPlaybackIncrementsPerTrack();
    void recordPlaybackEmitsSignalWithTrackAndCount();
    void ratingRoundTripAndClamping();
    void ratingEmitsSignal();
    void persistenceRoundTripAcrossControllerInstances();
    void emptyTrackIdIgnored();

private:
    Seriona::App::AppSettingsBackend testBackend();
    QVariant storedValue(const QString &group, const QString &key, const QVariant &defaultValue = QVariant()) const;

    QHash<QString, QVariant> m_store;
};

void TrackStatsControllerTest::init()
{
    m_store.clear();
}

void TrackStatsControllerTest::cleanup()
{
    m_store.clear();
}

Seriona::App::AppSettingsBackend TrackStatsControllerTest::testBackend()
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

QVariant TrackStatsControllerTest::storedValue(const QString &group, const QString &key, const QVariant &defaultValue) const
{
    return m_store.value(storageKey(group, key), defaultValue);
}

void TrackStatsControllerTest::defaultsAreZeroAndUnrated()
{
    Seriona::App::TrackStatsController controller;
    QCOMPARE(controller.playCountFor(QStringLiteral("track-a")), 0);
    QCOMPARE(controller.ratingFor(QStringLiteral("track-a")), 0);
}

void TrackStatsControllerTest::recordPlaybackIncrementsPerTrack()
{
    Seriona::App::TrackStatsController controller;
    controller.recordPlayback(QStringLiteral("track-a"));
    controller.recordPlayback(QStringLiteral("track-a"));
    controller.recordPlayback(QStringLiteral("track-b"));

    QCOMPARE(controller.playCountFor(QStringLiteral("track-a")), 2);
    QCOMPARE(controller.playCountFor(QStringLiteral("track-b")), 1);
    // 不同曲目互不影响
    QCOMPARE(controller.playCountFor(QStringLiteral("track-c")), 0);
}

void TrackStatsControllerTest::recordPlaybackEmitsSignalWithTrackAndCount()
{
    Seriona::App::TrackStatsController controller;
    QSignalSpy spy(&controller, &Seriona::App::TrackStatsController::playCountChanged);

    controller.recordPlayback(QStringLiteral("track-a"));
    controller.recordPlayback(QStringLiteral("track-a"));
    controller.recordPlayback(QStringLiteral("track-b"));

    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("track-a"));
    QCOMPARE(spy.at(0).at(1).toInt(), 1);
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("track-a"));
    QCOMPARE(spy.at(1).at(1).toInt(), 2);
    QCOMPARE(spy.at(2).at(0).toString(), QStringLiteral("track-b"));
    QCOMPARE(spy.at(2).at(1).toInt(), 1);
}

void TrackStatsControllerTest::ratingRoundTripAndClamping()
{
    Seriona::App::TrackStatsController controller;
    controller.setRating(QStringLiteral("track-a"), 4);
    QCOMPARE(controller.ratingFor(QStringLiteral("track-a")), 4);

    // 越界钳制：>5 → 5，<0 → 0
    controller.setRating(QStringLiteral("track-a"), 7);
    QCOMPARE(controller.ratingFor(QStringLiteral("track-a")), 5);
    controller.setRating(QStringLiteral("track-a"), -3);
    QCOMPARE(controller.ratingFor(QStringLiteral("track-a")), 0);

    // 0 = 清除评级（回到未评级）
    controller.setRating(QStringLiteral("track-a"), 3);
    controller.setRating(QStringLiteral("track-a"), 0);
    QCOMPARE(controller.ratingFor(QStringLiteral("track-a")), 0);
}

void TrackStatsControllerTest::ratingEmitsSignal()
{
    Seriona::App::TrackStatsController controller;
    QSignalSpy spy(&controller, &Seriona::App::TrackStatsController::ratingChanged);

    controller.setRating(QStringLiteral("track-a"), 2);
    controller.setRating(QStringLiteral("track-a"), 3);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("track-a"));
    QCOMPARE(spy.at(0).at(1).toInt(), 2);
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("track-a"));
    QCOMPARE(spy.at(1).at(1).toInt(), 3);
}

void TrackStatsControllerTest::persistenceRoundTripAcrossControllerInstances()
{
    {
        Seriona::App::TrackStatsController writer;
        writer.setSettingsStorageBackend(testBackend());
        writer.recordPlayback(QStringLiteral("track-a"));
        writer.recordPlayback(QStringLiteral("track-a"));
        writer.setRating(QStringLiteral("track-b"), 5);
    }

    // 新实例（同一 backend）应读到持久化的值
    Seriona::App::TrackStatsController reader;
    reader.setSettingsStorageBackend(testBackend());
    QCOMPARE(reader.playCountFor(QStringLiteral("track-a")), 2);
    QCOMPARE(reader.ratingFor(QStringLiteral("track-b")), 5);
    QCOMPARE(reader.playCountFor(QStringLiteral("track-b")), 0);
    QCOMPARE(reader.ratingFor(QStringLiteral("track-a")), 0);
}

void TrackStatsControllerTest::emptyTrackIdIgnored()
{
    Seriona::App::TrackStatsController controller;
    controller.setSettingsStorageBackend(testBackend());
    QSignalSpy playSpy(&controller, &Seriona::App::TrackStatsController::playCountChanged);
    QSignalSpy ratingSpy(&controller, &Seriona::App::TrackStatsController::ratingChanged);

    controller.recordPlayback(QString());
    controller.setRating(QString(), 3);

    QCOMPARE(playSpy.count(), 0);
    QCOMPARE(ratingSpy.count(), 0);

    // 空 trackId 不产生任何持久化键
    QVERIFY(m_store.isEmpty());
}

QTEST_GUILESS_MAIN(TrackStatsControllerTest)

#include "tst_track_stats_controller.moc"
