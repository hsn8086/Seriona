#include "track_stats_controller.h"

#include <QCoreApplication>
#include <QObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest/QTest>

#include <utility>

namespace {

constexpr auto kSettingsFileProperty = "seriona.settingsFileForTests";

// 把 QSettings 指向临时文件，避免测试污染用户真实配置（与
// tst_settings_controller.cpp 同模式）。
class ScopedSettingsFile
{
public:
    explicit ScopedSettingsFile(QTemporaryDir &dir)
        : m_file(dir.filePath(QStringLiteral("track-stats.ini")))
    {
        QCoreApplication::instance()->setProperty(kSettingsFileProperty, m_file);
    }

    ~ScopedSettingsFile()
    {
        QCoreApplication::instance()->setProperty(kSettingsFileProperty, QVariant{});
    }

    const QString &file() const
    {
        return m_file;
    }

private:
    QString m_file;
};

}

class TrackStatsControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreZeroAndUnrated();
    void recordPlaybackIncrementsPerTrack();
    void recordPlaybackEmitsSignalWithTrackAndCount();
    void ratingRoundTripAndClamping();
    void ratingEmitsSignal();
    void persistenceRoundTripAcrossControllerInstances();
    void emptyTrackIdIgnored();
};

void TrackStatsControllerTest::defaultsAreZeroAndUnrated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ScopedSettingsFile settings(dir);

    Seriona::App::TrackStatsController controller;
    QCOMPARE(controller.playCountFor(QStringLiteral("track-a")), 0);
    QCOMPARE(controller.ratingFor(QStringLiteral("track-a")), 0);
}

void TrackStatsControllerTest::recordPlaybackIncrementsPerTrack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ScopedSettingsFile settings(dir);

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
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ScopedSettingsFile settings(dir);

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
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ScopedSettingsFile settings(dir);

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
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ScopedSettingsFile settings(dir);

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
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    {
        ScopedSettingsFile settings(dir);
        Seriona::App::TrackStatsController writer;
        writer.recordPlayback(QStringLiteral("track-a"));
        writer.recordPlayback(QStringLiteral("track-a"));
        writer.setRating(QStringLiteral("track-b"), 5);
    }

    // 新实例（同一 QSettings 文件）应读到持久化的值
    ScopedSettingsFile settings(dir);
    Seriona::App::TrackStatsController reader;
    QCOMPARE(reader.playCountFor(QStringLiteral("track-a")), 2);
    QCOMPARE(reader.ratingFor(QStringLiteral("track-b")), 5);
    QCOMPARE(reader.playCountFor(QStringLiteral("track-b")), 0);
    QCOMPARE(reader.ratingFor(QStringLiteral("track-a")), 0);
}

void TrackStatsControllerTest::emptyTrackIdIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ScopedSettingsFile settings(dir);

    Seriona::App::TrackStatsController controller;
    QSignalSpy playSpy(&controller, &Seriona::App::TrackStatsController::playCountChanged);
    QSignalSpy ratingSpy(&controller, &Seriona::App::TrackStatsController::ratingChanged);

    controller.recordPlayback(QString());
    controller.setRating(QString(), 3);

    QCOMPARE(playSpy.count(), 0);
    QCOMPARE(ratingSpy.count(), 0);

    // 空 trackId 不产生任何持久化键
    QSettings stored(settings.file(), QSettings::IniFormat);
    QCOMPARE(stored.childGroups().size(), 0);
}

QTEST_GUILESS_MAIN(TrackStatsControllerTest)

#include "tst_track_stats_controller.moc"
