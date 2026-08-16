#include "settings_controller.h"

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest/QTest>

#include <memory>
#include <utility>

namespace {

constexpr auto kSettingsFileProperty = "seriona.settingsFileForTests";
constexpr auto kOutputGroup = "output";

} // namespace

class SettingsControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void defaults();
    void propertySettersPersistAndNotify();
    void invalidValuesRejected();
    void applyAssemblesPayload();
    void discreteControlsPushImmediately();
    void bufferDurationDebounces();
    void persistenceRoundTrip();
    void setDefaultsLandsPropertiesWithoutPersistenceOrPush();
    void enumerateDevicesUpdatesList();
    void startupPushSequence();
    void lyricDelimitersPersistRoundTrip();
    void sampleFormatPersistRoundTrip();
    void applyIncludesSampleFormat();
    void rollbackRejectedOutputConfigRestoresSnapshot();

private:
    std::unique_ptr<QTemporaryDir> m_settingsDir;
    QString m_settingsFile;
};

void SettingsControllerTest::init()
{
    m_settingsDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settingsDir->isValid());
    m_settingsFile = m_settingsDir->filePath(QStringLiteral("settings.ini"));
    QCoreApplication::instance()->setProperty(kSettingsFileProperty, m_settingsFile);

    QSettings settings(m_settingsFile, QSettings::IniFormat);
    settings.clear();
    settings.sync();
}

void SettingsControllerTest::cleanup()
{
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    settings.clear();
    settings.sync();

    QCoreApplication::instance()->setProperty(kSettingsFileProperty, QVariant{});
    m_settingsFile.clear();
    m_settingsDir.reset();
}

void SettingsControllerTest::defaults()
{
    Seriona::App::SettingsController settings;

    QCOMPARE(settings.outputMode(), 0);
    QVERIFY(settings.playbackDevices().isEmpty());
    QVERIFY(settings.preferredDeviceId().isEmpty());
    QCOMPARE(settings.sampleRate(), 48000);
    QCOMPARE(settings.sampleFormat(), 0);
    QCOMPARE(settings.bufferDurationMs(), 300);
    const QStringList defaultDelimiters{QStringLiteral(" / ")};
    QCOMPARE(settings.lyricDelimiters(), defaultDelimiters);
}

void SettingsControllerTest::propertySettersPersistAndNotify()
{
    Seriona::App::SettingsController settings;
    QSignalSpy modeSpy(&settings, &Seriona::App::SettingsController::outputModeChanged);
    QSignalSpy rateSpy(&settings, &Seriona::App::SettingsController::sampleRateChanged);
    QSignalSpy formatSpy(&settings, &Seriona::App::SettingsController::sampleFormatChanged);
    QSignalSpy durationSpy(&settings, &Seriona::App::SettingsController::bufferDurationMsChanged);
    QSignalSpy deviceSpy(&settings, &Seriona::App::SettingsController::preferredDeviceIdChanged);
    QSignalSpy delimiterSpy(&settings, &Seriona::App::SettingsController::lyricDelimitersChanged);

    settings.setOutputMode(1);
    settings.setSampleRate(96000);
    settings.setSampleFormat(2);
    settings.setBufferDurationMs(500);
    settings.setPreferredDeviceId(QStringLiteral("dev-1"));
    settings.setLyricDelimiters(QStringList{QStringLiteral(" / "), QStringLiteral(" | ")});

    QCOMPARE(settings.outputMode(), 1);
    QCOMPARE(settings.sampleRate(), 96000);
    QCOMPARE(settings.sampleFormat(), 2);
    QCOMPARE(settings.bufferDurationMs(), 500);
    QCOMPARE(settings.preferredDeviceId(), QStringLiteral("dev-1"));
    const QStringList expectedDelimiters{QStringLiteral(" / "), QStringLiteral(" | ")};
    QCOMPARE(settings.lyricDelimiters(), expectedDelimiters);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(rateSpy.count(), 1);
    QCOMPARE(formatSpy.count(), 1);
    QCOMPARE(durationSpy.count(), 1);
    QCOMPARE(deviceSpy.count(), 1);
    QCOMPARE(delimiterSpy.count(), 1);

    // 相同值不重复 NOTIFY
    settings.setOutputMode(1);
    settings.setSampleRate(96000);
    settings.setSampleFormat(2);
    settings.setLyricDelimiters(expectedDelimiters);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(rateSpy.count(), 1);
    QCOMPARE(formatSpy.count(), 1);
    QCOMPARE(delimiterSpy.count(), 1);

    QSettings stored(m_settingsFile, QSettings::IniFormat);
    stored.beginGroup(QString::fromUtf8(kOutputGroup));
    QCOMPARE(stored.value(QStringLiteral("outputMode")).toInt(), 1);
    QCOMPARE(stored.value(QStringLiteral("sampleRate")).toInt(), 96000);
    QCOMPARE(stored.value(QStringLiteral("sampleFormat")).toInt(), 2);
    QCOMPARE(stored.value(QStringLiteral("bufferDurationMs")).toInt(), 500);
    QCOMPARE(stored.value(QStringLiteral("preferredDeviceId")).toString(), QStringLiteral("dev-1"));
    stored.endGroup();
    stored.beginGroup(QStringLiteral("lyrics"));
    QCOMPARE(stored.value(QStringLiteral("delimiters")).toStringList(), expectedDelimiters);
    stored.endGroup();
}

void SettingsControllerTest::invalidValuesRejected()
{
    Seriona::App::SettingsController settings;

    settings.setOutputMode(7);
    QCOMPARE(settings.outputMode(), 0);

    settings.setSampleRate(100);
    QCOMPARE(settings.sampleRate(), 48000);
    settings.setSampleRate(-1);
    QCOMPARE(settings.sampleRate(), 48000);
    settings.setSampleRate(768001);
    QCOMPARE(settings.sampleRate(), 48000);

    settings.setSampleFormat(3);
    QCOMPARE(settings.sampleFormat(), 0);
    settings.setSampleFormat(5);
    QCOMPARE(settings.sampleFormat(), 0);
    settings.setSampleFormat(99);
    QCOMPARE(settings.sampleFormat(), 0);
    settings.setSampleFormat(-1);
    QCOMPARE(settings.sampleFormat(), 0);

    settings.setBufferDurationMs(49);
    QCOMPARE(settings.bufferDurationMs(), 300);
    settings.setBufferDurationMs(2000);
    QCOMPARE(settings.bufferDurationMs(), 300);

    // 边界值合法
    settings.setOutputMode(1);
    settings.setSampleRate(8000);
    settings.setSampleRate(768000);
    settings.setSampleFormat(1);
    settings.setSampleFormat(2);
    settings.setSampleFormat(4);
    settings.setBufferDurationMs(50);
    settings.setBufferDurationMs(1000);
    QCOMPARE(settings.outputMode(), 1);
    QCOMPARE(settings.sampleRate(), 768000);
    QCOMPARE(settings.sampleFormat(), 4);
    QCOMPARE(settings.bufferDurationMs(), 1000);

    // 非法值不写入 QSettings
    settings.setOutputMode(7);
    settings.setSampleRate(100);
    settings.setSampleFormat(3);
    settings.setBufferDurationMs(2000);
    QSettings stored(m_settingsFile, QSettings::IniFormat);
    stored.beginGroup(QString::fromUtf8(kOutputGroup));
    QCOMPARE(stored.value(QStringLiteral("outputMode")).toInt(), 1);
    QCOMPARE(stored.value(QStringLiteral("sampleRate")).toInt(), 768000);
    QCOMPARE(stored.value(QStringLiteral("sampleFormat")).toInt(), 4);
    QCOMPARE(stored.value(QStringLiteral("bufferDurationMs")).toInt(), 1000);
    stored.endGroup();
}

void SettingsControllerTest::applyAssemblesPayload()
{
    Seriona::App::SettingsController settings;
    QStringList payloads;
    settings.setApplyOutputConfigExecutor(
        [&payloads](int outputMode, int sampleRate, int sampleFormat, int bufferDurationMs, const QString &preferredDeviceId) {
            payloads.append(QStringLiteral("%1|%2|%3|%4|%5")
                                .arg(outputMode)
                                .arg(sampleRate)
                                .arg(sampleFormat)
                                .arg(bufferDurationMs)
                                .arg(preferredDeviceId));
        });

    settings.setOutputMode(1);
    settings.setSampleRate(96000);
    settings.setPreferredDeviceId(QStringLiteral("dev-x"));
    settings.apply();

    QCOMPARE(payloads.size(), 4);
    QCOMPARE(payloads.at(0), QStringLiteral("1|48000|0|300|"));
    QCOMPARE(payloads.at(1), QStringLiteral("1|96000|0|300|"));
    QCOMPARE(payloads.at(2), QStringLiteral("1|96000|0|300|dev-x"));
    QCOMPARE(payloads.at(3), QStringLiteral("1|96000|0|300|dev-x"));
}

void SettingsControllerTest::discreteControlsPushImmediately()
{
    Seriona::App::SettingsController settings;
    int pushes = 0;
    settings.setApplyOutputConfigExecutor(
        [&pushes](int, int, int, int, const QString &) {
            ++pushes;
        });

    settings.setOutputMode(1);
    settings.setSampleRate(44100);
    settings.setPreferredDeviceId(QStringLiteral("dev-2"));
    QCOMPARE(pushes, 3);
}

void SettingsControllerTest::bufferDurationDebounces()
{
    Seriona::App::SettingsController settings;
    int pushes = 0;
    settings.setApplyOutputConfigExecutor(
        [&pushes](int, int, int, int, const QString &) {
            ++pushes;
        });

    // 离散控件立即推送
    settings.setOutputMode(1);
    QCOMPARE(pushes, 1);

    // 连续控件去抖：5 次变更只产生一次下发
    for (int i = 0; i < 5; ++i) {
        settings.setBufferDurationMs(100 + i * 10);
    }
    QCOMPARE(pushes, 1);
    QCOMPARE(settings.bufferDurationMs(), 140);

    QTRY_COMPARE_WITH_TIMEOUT(pushes, 2, 2000);

    QTest::qWait(800);
    QCOMPARE(pushes, 2);
}

void SettingsControllerTest::persistenceRoundTrip()
{
    {
        Seriona::App::SettingsController writer;
        writer.setOutputMode(1);
        writer.setSampleRate(192000);
        writer.setBufferDurationMs(800);
        writer.setPreferredDeviceId(QStringLiteral("dev-3"));
    }

    Seriona::App::SettingsController reader;
    int pushes = 0;
    reader.setApplyOutputConfigExecutor(
        [&pushes](int, int, int, int, const QString &) {
            ++pushes;
        });
    reader.reloadFromSettings();

    QCOMPARE(reader.outputMode(), 1);
    QCOMPARE(reader.sampleRate(), 192000);
    QCOMPARE(reader.bufferDurationMs(), 800);
    QCOMPARE(reader.preferredDeviceId(), QStringLiteral("dev-3"));
    // reload 只还原属性，不推送
    QCOMPARE(pushes, 0);
}

void SettingsControllerTest::setDefaultsLandsPropertiesWithoutPersistenceOrPush()
{
    Seriona::App::SettingsController settings;
    int pushes = 0;
    settings.setApplyOutputConfigExecutor(
        [&pushes](int, int, int, int, const QString &) {
            ++pushes;
        });

    settings.setDefaults(1, 96000, 500, QStringLiteral("dev-4"));

    QCOMPARE(settings.outputMode(), 1);
    QCOMPARE(settings.sampleRate(), 96000);
    QCOMPARE(settings.bufferDurationMs(), 500);
    QCOMPARE(settings.preferredDeviceId(), QStringLiteral("dev-4"));
    QCOMPARE(pushes, 0);

    QSettings stored(m_settingsFile, QSettings::IniFormat);
    stored.beginGroup(QString::fromUtf8(kOutputGroup));
    QVERIFY(!stored.contains(QStringLiteral("outputMode")));
    QVERIFY(!stored.contains(QStringLiteral("sampleRate")));
    stored.endGroup();
}

void SettingsControllerTest::enumerateDevicesUpdatesList()
{
    Seriona::App::SettingsController settings;
    settings.setEnumerateDevicesExecutor(
        [] {
            return QStringList{QStringLiteral("dev-1"), QStringLiteral("dev-2")};
        });
    QSignalSpy devicesSpy(&settings, &Seriona::App::SettingsController::playbackDevicesChanged);

    settings.enumerateDevices();
    const QStringList expectedDevices{QStringLiteral("dev-1"), QStringLiteral("dev-2")};
    QCOMPARE(settings.playbackDevices(), expectedDevices);
    QCOMPARE(devicesSpy.count(), 1);

    // 相同列表不重复 NOTIFY
    settings.enumerateDevices();
    QCOMPARE(devicesSpy.count(), 1);

    // 无 executor（mock 模式）时无副作用
    Seriona::App::SettingsController mockController;
    mockController.enumerateDevices();
    QVERIFY(mockController.playbackDevices().isEmpty());
}

void SettingsControllerTest::lyricDelimitersPersistRoundTrip()
{
    const QStringList delimiters{QStringLiteral(" / "), QStringLiteral(" | "), QStringLiteral(" - ")};
    {
        Seriona::App::SettingsController writer;
        writer.setLyricDelimiters(delimiters);
        QCOMPARE(writer.lyricDelimiters(), delimiters);
    }

    Seriona::App::SettingsController reader;
    reader.reloadFromSettings();
    QCOMPARE(reader.lyricDelimiters(), delimiters);

    // 空列表语义：合法（清空后歌词不切分），持久化并重读一致，不得崩溃
    {
        Seriona::App::SettingsController emptyWriter;
        emptyWriter.setLyricDelimiters({});
        QVERIFY(emptyWriter.lyricDelimiters().isEmpty());
    }
    Seriona::App::SettingsController emptyReader;
    emptyReader.reloadFromSettings();
    QVERIFY(emptyReader.lyricDelimiters().isEmpty());
}

void SettingsControllerTest::sampleFormatPersistRoundTrip()
{
    {
        Seriona::App::SettingsController writer;
        writer.setSampleFormat(4);
        QCOMPARE(writer.sampleFormat(), 4);
    }

    Seriona::App::SettingsController reader;
    reader.reloadFromSettings();
    QCOMPARE(reader.sampleFormat(), 4);

    // 未写入时默认 0
    QSettings stored(m_settingsFile, QSettings::IniFormat);
    stored.remove(QStringLiteral("output/sampleFormat"));
    stored.sync();
    Seriona::App::SettingsController defaultReader;
    defaultReader.reloadFromSettings();
    QCOMPARE(defaultReader.sampleFormat(), 0);
}

void SettingsControllerTest::applyIncludesSampleFormat()
{
    Seriona::App::SettingsController settings;
    QStringList payloads;
    settings.setApplyOutputConfigExecutor(
        [&payloads](int outputMode, int sampleRate, int sampleFormat, int bufferDurationMs, const QString &preferredDeviceId) {
            Q_UNUSED(outputMode);
            Q_UNUSED(sampleRate);
            Q_UNUSED(bufferDurationMs);
            Q_UNUSED(preferredDeviceId);
            payloads.append(QString::number(sampleFormat));
        });

    settings.setOutputMode(1);
    QCOMPARE(payloads.size(), 1);
    QCOMPARE(payloads.at(0), QStringLiteral("0"));

    settings.setSampleFormat(2);
    QCOMPARE(payloads.size(), 2);
    QCOMPARE(payloads.at(1), QStringLiteral("2"));

    settings.apply();
    QCOMPARE(payloads.size(), 3);
    QCOMPARE(payloads.at(2), QStringLiteral("2"));
}

void SettingsControllerTest::rollbackRejectedOutputConfigRestoresSnapshot()
{
    Seriona::App::SettingsController settings;
    int pushes = 0;
    settings.setApplyOutputConfigExecutor(
        [&pushes](int, int, int, int, const QString &) {
            ++pushes;
        });

    // 无快照（从未提交）时回退忽略
    settings.rollbackRejectedOutputConfig();
    QCOMPARE(settings.outputMode(), 0);
    QCOMPARE(settings.sampleFormat(), 0);
    QCOMPARE(pushes, 0);

    // 离散 setter 每次提交并记录快照；bufferDurationMs 去抖未提交
    settings.setOutputMode(1);
    settings.setSampleFormat(2);
    settings.setBufferDurationMs(500);
    QCOMPARE(pushes, 2);

    // 拒绝回退 → 恢复最近一次已提交的值，未提交的 bufferDurationMs 变更被回滚
    settings.rollbackRejectedOutputConfig();
    QCOMPARE(settings.outputMode(), 1);
    QCOMPARE(settings.sampleRate(), 48000);
    QCOMPARE(settings.sampleFormat(), 2);
    QCOMPARE(settings.bufferDurationMs(), 300);
    QCOMPARE(settings.preferredDeviceId(), QString());
    // 回退本身不推送
    QCOMPARE(pushes, 2);

    // 回退不持久化：QSettings 保持 setter 写入的值
    QSettings stored(m_settingsFile, QSettings::IniFormat);
    stored.beginGroup(QString::fromUtf8(kOutputGroup));
    QCOMPARE(stored.value(QStringLiteral("outputMode")).toInt(), 1);
    QCOMPARE(stored.value(QStringLiteral("sampleFormat")).toInt(), 2);
    QCOMPARE(stored.value(QStringLiteral("bufferDurationMs")).toInt(), 500);
    stored.endGroup();
}

void SettingsControllerTest::startupPushSequence()
{
    Seriona::App::SettingsController settings;
    QStringList payloads;
    settings.setApplyOutputConfigExecutor(
        [&payloads](int outputMode, int sampleRate, int sampleFormat, int bufferDurationMs, const QString &preferredDeviceId) {
            payloads.append(QStringLiteral("%1|%2|%3|%4")
                                .arg(outputMode)
                                .arg(sampleRate)
                                .arg(bufferDurationMs)
                                .arg(preferredDeviceId));
        });

    // 启动路径：reloadFromSettings → apply 恰好推送一次（空设置 → 默认值）
    settings.reloadFromSettings();
    settings.apply();

    QCOMPARE(payloads.size(), 1);
    QCOMPARE(payloads.at(0), QStringLiteral("0|48000|300|"));
}

QTEST_GUILESS_MAIN(SettingsControllerTest)

#include "tst_settings_controller.moc"
