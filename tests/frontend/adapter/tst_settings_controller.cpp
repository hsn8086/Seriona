#include "settings_controller.h"

#include "app_settings_storage.h"

#include <QHash>
#include <QSignalSpy>
#include <QVariant>
#include <QtTest/QTest>

#include <memory>

namespace {

constexpr auto kOutputGroup = "output";

QString storageKey(const QString &group, const QString &key)
{
    return group + QLatin1Char('\x1f') + key;
}

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
    void directOutputGreyState();
    void deviceCapsEmptyShowsAllOptions();
    void deviceCapsFilterSampleRatesAndFormats();
    void deviceCapsFollowSelectedDevice();
    void enumerateDevicesExposesCapabilities();
    void unsupportedSavedValueKeptAndMarked();
    void logLevelPersistRoundTrip();
    void logLevelMapping();
    void logLevelPushAndDefense();

private:
    Seriona::App::AppSettingsBackend testBackend();
    QVariant storedValue(const QString &group, const QString &key, const QVariant &defaultValue = QVariant()) const;
    bool storedContains(const QString &group, const QString &key) const;
    void removeStored(const QString &group, const QString &key);

    QHash<QString, QVariant> m_store;
};

Seriona::App::AppSettingsBackend SettingsControllerTest::testBackend()
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

QVariant SettingsControllerTest::storedValue(const QString &group, const QString &key, const QVariant &defaultValue) const
{
    return m_store.value(storageKey(group, key), defaultValue);
}

bool SettingsControllerTest::storedContains(const QString &group, const QString &key) const
{
    return m_store.contains(storageKey(group, key));
}

void SettingsControllerTest::removeStored(const QString &group, const QString &key)
{
    m_store.remove(storageKey(group, key));
}

void SettingsControllerTest::init()
{
    m_store.clear();
}

void SettingsControllerTest::cleanup()
{
    m_store.clear();
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
    // 日志等级默认 info（与前端持久化默认一致，启动后经 applyLogLevel 同步后端）
    QCOMPARE(settings.logLevel(), 2);
}

void SettingsControllerTest::propertySettersPersistAndNotify()
{
    Seriona::App::SettingsController settings;
    settings.setSettingsStorageBackend(testBackend());
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

    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("outputMode")).toInt(), 1);
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("sampleRate")).toInt(), 96000);
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("sampleFormat")).toInt(), 2);
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("bufferDurationMs")).toInt(), 500);
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("preferredDeviceId")).toString(), QStringLiteral("dev-1"));
    QCOMPARE(storedValue(QStringLiteral("lyrics"), QStringLiteral("delimiters")).toStringList(), expectedDelimiters);
}

void SettingsControllerTest::invalidValuesRejected()
{
    Seriona::App::SettingsController settings;
    settings.setSettingsStorageBackend(testBackend());

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

    // 非法值不写入存储
    settings.setOutputMode(7);
    settings.setSampleRate(100);
    settings.setSampleFormat(3);
    settings.setBufferDurationMs(2000);
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("outputMode")).toInt(), 1);
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("sampleRate")).toInt(), 768000);
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("sampleFormat")).toInt(), 4);
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("bufferDurationMs")).toInt(), 1000);
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
        writer.setSettingsStorageBackend(testBackend());
        writer.setOutputMode(1);
        writer.setSampleRate(192000);
        writer.setBufferDurationMs(800);
        writer.setPreferredDeviceId(QStringLiteral("dev-3"));
    }

    Seriona::App::SettingsController reader;
    reader.setSettingsStorageBackend(testBackend());
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
    settings.setSettingsStorageBackend(testBackend());
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

    QVERIFY(!storedContains(QStringLiteral("output"), QStringLiteral("outputMode")));
    QVERIFY(!storedContains(QStringLiteral("output"), QStringLiteral("sampleRate")));
}

void SettingsControllerTest::enumerateDevicesUpdatesList()
{
    Seriona::App::SettingsController settings;
    settings.setEnumerateDevicesExecutor(
        [] {
            return QList<Seriona::App::PlaybackDeviceCapabilities>{
                {QStringLiteral("dev-1"), QStringLiteral("Device One"), {}, {}},
                {QStringLiteral("dev-2"), QStringLiteral("Device Two"), {}, {}},
            };
        });
    QSignalSpy devicesSpy(&settings, &Seriona::App::SettingsController::playbackDevicesChanged);
    QSignalSpy namesSpy(&settings, &Seriona::App::SettingsController::playbackDeviceNamesChanged);

    settings.enumerateDevices();
    QCOMPARE(settings.playbackDevices(), QStringList({QStringLiteral("dev-1"), QStringLiteral("dev-2")}));
    QCOMPARE(settings.playbackDeviceNames(), QStringList({QStringLiteral("Device One"), QStringLiteral("Device Two")}));
    QCOMPARE(devicesSpy.count(), 1);
    QCOMPARE(namesSpy.count(), 1);

    // 相同列表不重复 NOTIFY（id 与名字都跳过）
    settings.enumerateDevices();
    QCOMPARE(devicesSpy.count(), 1);
    QCOMPARE(namesSpy.count(), 1);

    // 仅名字变化时只发 playbackDeviceNamesChanged，id 列表不变
    settings.setEnumerateDevicesExecutor(
        [] {
            return QList<Seriona::App::PlaybackDeviceCapabilities>{
                {QStringLiteral("dev-1"), QStringLiteral("Device One Renamed"), {}, {}},
                {QStringLiteral("dev-2"), QStringLiteral("Device Two"), {}, {}},
            };
        });
    settings.enumerateDevices();
    QCOMPARE(settings.playbackDevices(), QStringList({QStringLiteral("dev-1"), QStringLiteral("dev-2")}));
    QCOMPARE(settings.playbackDeviceNames(), QStringList({QStringLiteral("Device One Renamed"), QStringLiteral("Device Two")}));
    QCOMPARE(devicesSpy.count(), 1);
    QCOMPARE(namesSpy.count(), 2);

    // 无 executor（mock 模式）时无副作用
    Seriona::App::SettingsController mockController;
    mockController.enumerateDevices();
    QVERIFY(mockController.playbackDevices().isEmpty());
    QVERIFY(mockController.playbackDeviceNames().isEmpty());
    QVERIFY(mockController.playbackDeviceCapabilities().isEmpty());
}

void SettingsControllerTest::directOutputGreyState()
{
    Seriona::App::SettingsController settings;
    QSignalSpy modeSpy(&settings, &Seriona::App::SettingsController::outputModeChanged);

    // 默认直接输出（0）→ 采样率/位深行灰化
    QVERIFY(settings.sampleParamsGreyed());

    settings.setOutputMode(1);
    QVERIFY(!settings.sampleParamsGreyed());

    settings.setOutputMode(0);
    QVERIFY(settings.sampleParamsGreyed());
    QCOMPARE(modeSpy.count(), 2);
}

void SettingsControllerTest::deviceCapsEmptyShowsAllOptions()
{
    Seriona::App::SettingsController settings;
    settings.setEnumerateDevicesExecutor(
        [] {
            return QList<Seriona::App::PlaybackDeviceCapabilities>{
                {QStringLiteral("dev-1"), QStringLiteral("Device One"), {}, {}},
            };
        });
    settings.enumerateDevices();
    settings.setPreferredDeviceId(QStringLiteral("dev-1"));

    // 空能力 = 未枚举/全支持 → 显示全部标准选项（含 0=跟随设备）
    const QVariantList rates = settings.sampleRateOptions();
    QCOMPARE(rates.size(), 5);
    QCOMPARE(rates.at(0).toMap().value(QStringLiteral("value")).toInt(), 0);
    QCOMPARE(rates.at(1).toMap().value(QStringLiteral("value")).toInt(), 44100);
    QCOMPARE(rates.at(2).toMap().value(QStringLiteral("value")).toInt(), 48000);
    QCOMPARE(rates.at(3).toMap().value(QStringLiteral("value")).toInt(), 96000);
    QCOMPARE(rates.at(4).toMap().value(QStringLiteral("value")).toInt(), 192000);

    const QVariantList formats = settings.sampleFormatOptions();
    QCOMPARE(formats.size(), 4);
    QCOMPARE(formats.at(0).toMap().value(QStringLiteral("value")).toInt(), 0);
    QCOMPARE(formats.at(1).toMap().value(QStringLiteral("value")).toInt(), 1);
    QCOMPARE(formats.at(2).toMap().value(QStringLiteral("value")).toInt(), 2);
    QCOMPARE(formats.at(3).toMap().value(QStringLiteral("value")).toInt(), 4);
}

void SettingsControllerTest::deviceCapsFilterSampleRatesAndFormats()
{
    Seriona::App::SettingsController settings;
    settings.setEnumerateDevicesExecutor(
        [] {
            return QList<Seriona::App::PlaybackDeviceCapabilities>{
                {QStringLiteral("dev-1"), QStringLiteral("Device One"), {1, 4}, {44100, 48000}},
            };
        });
    settings.enumerateDevices();
    settings.setPreferredDeviceId(QStringLiteral("dev-1"));

    // 已枚举能力 → 与标准列表求交（0=跟随设备恒保留）
    const QVariantList rates = settings.sampleRateOptions();
    QCOMPARE(rates.size(), 3);
    QCOMPARE(rates.at(0).toMap().value(QStringLiteral("value")).toInt(), 0);
    QCOMPARE(rates.at(1).toMap().value(QStringLiteral("value")).toInt(), 44100);
    QCOMPARE(rates.at(2).toMap().value(QStringLiteral("value")).toInt(), 48000);

    const QVariantList formats = settings.sampleFormatOptions();
    QCOMPARE(formats.size(), 3);
    QCOMPARE(formats.at(0).toMap().value(QStringLiteral("value")).toInt(), 0);
    QCOMPARE(formats.at(1).toMap().value(QStringLiteral("value")).toInt(), 1);
    QCOMPARE(formats.at(2).toMap().value(QStringLiteral("value")).toInt(), 4);
}

void SettingsControllerTest::deviceCapsFollowSelectedDevice()
{
    Seriona::App::SettingsController settings;
    settings.setEnumerateDevicesExecutor(
        [] {
            return QList<Seriona::App::PlaybackDeviceCapabilities>{
                {QStringLiteral("dev-1"), QStringLiteral("Device One"), {1, 4}, {44100, 48000}},
                {QStringLiteral("dev-2"), QStringLiteral("Device Two"), {1, 2}, {48000, 96000}},
            };
        });
    settings.enumerateDevices();
    settings.setPreferredDeviceId(QStringLiteral("dev-1"));
    QSignalSpy rateOptionsSpy(&settings, &Seriona::App::SettingsController::sampleRateOptionsChanged);
    QSignalSpy formatOptionsSpy(&settings, &Seriona::App::SettingsController::sampleFormatOptionsChanged);

    settings.setPreferredDeviceId(QStringLiteral("dev-2"));
    const QVariantList rates = settings.sampleRateOptions();
    QCOMPARE(rates.size(), 3);
    QCOMPARE(rates.at(1).toMap().value(QStringLiteral("value")).toInt(), 48000);
    QCOMPARE(rates.at(2).toMap().value(QStringLiteral("value")).toInt(), 96000);
    const QVariantList formats = settings.sampleFormatOptions();
    QCOMPARE(formats.size(), 3);
    QCOMPARE(formats.at(1).toMap().value(QStringLiteral("value")).toInt(), 1);
    QCOMPARE(formats.at(2).toMap().value(QStringLiteral("value")).toInt(), 2);
    QCOMPARE(rateOptionsSpy.count(), 1);
    QCOMPARE(formatOptionsSpy.count(), 1);
}

void SettingsControllerTest::enumerateDevicesExposesCapabilities()
{
    Seriona::App::SettingsController settings;
    settings.setEnumerateDevicesExecutor(
        [] {
            return QList<Seriona::App::PlaybackDeviceCapabilities>{
                {QStringLiteral("dev-1"), QStringLiteral("Device One"), {1, 4}, {48000}},
            };
        });
    QSignalSpy capsSpy(&settings, &Seriona::App::SettingsController::playbackDeviceCapabilitiesChanged);

    settings.enumerateDevices();
    const QVariantList caps = settings.playbackDeviceCapabilities();
    QCOMPARE(caps.size(), 1);
    const QVariantMap device = caps.at(0).toMap();
    QCOMPARE(device.value(QStringLiteral("deviceId")).toString(), QStringLiteral("dev-1"));
    QCOMPARE(device.value(QStringLiteral("deviceName")).toString(), QStringLiteral("Device One"));
    QCOMPARE(device.value(QStringLiteral("sampleFormats")).toList(), QVariantList({1, 4}));
    QCOMPARE(device.value(QStringLiteral("sampleRates")).toList(), QVariantList({48000}));
    QCOMPARE(capsSpy.count(), 1);

    // 相同列表不重复 NOTIFY
    settings.enumerateDevices();
    QCOMPARE(capsSpy.count(), 1);
}

void SettingsControllerTest::unsupportedSavedValueKeptAndMarked()
{
    Seriona::App::SettingsController settings;
    settings.setSampleRate(96000);
    settings.setEnumerateDevicesExecutor(
        [] {
            return QList<Seriona::App::PlaybackDeviceCapabilities>{
                {QStringLiteral("dev-1"), QStringLiteral("Device One"), {1, 4}, {44100, 48000}},
            };
        });
    settings.enumerateDevices();
    settings.setPreferredDeviceId(QStringLiteral("dev-1"));

    // 已保存值不在设备支持列表 → 保留该选项并标注；已保存值本身不变
    bool foundSaved = false;
    const QVariantList rates = settings.sampleRateOptions();
    for (const auto &entry : rates) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("value")).toInt() == 96000) {
            foundSaved = true;
            QVERIFY(map.value(QStringLiteral("label")).toString().contains(QStringLiteral("设备不支持")));
        }
    }
    QVERIFY(foundSaved);
    QCOMPARE(settings.sampleRate(), 96000);

    // 支持列表内已保存值不标注
    settings.setSampleRate(44100);
    const QVariantList supportedRates = settings.sampleRateOptions();
    for (const auto &entry : supportedRates) {
        QVERIFY(!entry.toMap().value(QStringLiteral("label")).toString().contains(QStringLiteral("设备不支持")));
    }
}

void SettingsControllerTest::logLevelPersistRoundTrip()
{
    {
        Seriona::App::SettingsController writer;
        writer.setSettingsStorageBackend(testBackend());
        writer.setLogLevel(1); // debug
        QCOMPARE(writer.logLevel(), 1);
    }

    // 持久化键位于 logging 组，值为写入的枚举 int
    QCOMPARE(storedValue(QStringLiteral("logging"), QStringLiteral("logLevel")).toInt(), 1);

    Seriona::App::SettingsController reader;
    reader.setSettingsStorageBackend(testBackend());
    reader.reloadFromSettings();
    QCOMPARE(reader.logLevel(), 1);

    // 未写入时默认 info
    removeStored(QStringLiteral("logging"), QStringLiteral("logLevel"));
    Seriona::App::SettingsController defaultReader;
    defaultReader.setSettingsStorageBackend(testBackend());
    defaultReader.reloadFromSettings();
    QCOMPARE(defaultReader.logLevel(), 2);
}

void SettingsControllerTest::logLevelMapping()
{
    using Seriona::App::SettingsController;

    // 字符串 → 枚举（spdlog::level::level_enum 值：trace=0..critical=5）
    QCOMPARE(SettingsController::logLevelFromString(QStringLiteral("trace")), 0);
    QCOMPARE(SettingsController::logLevelFromString(QStringLiteral("debug")), 1);
    QCOMPARE(SettingsController::logLevelFromString(QStringLiteral("info")), 2);
    QCOMPARE(SettingsController::logLevelFromString(QStringLiteral("warn")), 3);
    QCOMPARE(SettingsController::logLevelFromString(QStringLiteral("error")), 4);
    QCOMPARE(SettingsController::logLevelFromString(QStringLiteral("critical")), 5);
    QCOMPARE(SettingsController::logLevelFromString(QStringLiteral("off")), -1);
    QCOMPARE(SettingsController::logLevelFromString(QStringLiteral("bogus")), -1);
    QCOMPARE(SettingsController::logLevelFromString(QString()), -1);

    // 枚举 → 字符串（往返一致）
    QCOMPARE(SettingsController::logLevelToString(0), QStringLiteral("trace"));
    QCOMPARE(SettingsController::logLevelToString(1), QStringLiteral("debug"));
    QCOMPARE(SettingsController::logLevelToString(2), QStringLiteral("info"));
    QCOMPARE(SettingsController::logLevelToString(3), QStringLiteral("warn"));
    QCOMPARE(SettingsController::logLevelToString(4), QStringLiteral("error"));
    QCOMPARE(SettingsController::logLevelToString(5), QStringLiteral("critical"));
    QCOMPARE(SettingsController::logLevelToString(6), QString());
    QCOMPARE(SettingsController::logLevelToString(-1), QString());
}

void SettingsControllerTest::logLevelPushAndDefense()
{
    Seriona::App::SettingsController settings;
    settings.setSettingsStorageBackend(testBackend());
    QList<int> pushed;
    settings.setLogLevelExecutor([&pushed](int level) {
        pushed.append(level);
    });

    // 离散变更立即推送，值正确
    settings.setLogLevel(1);
    QCOMPARE(pushed, QList<int>({1}));
    settings.setLogLevel(4);
    QCOMPARE(pushed, QList<int>({1, 4}));

    // 相同值不重复推送
    settings.setLogLevel(4);
    QCOMPARE(pushed.size(), 2);

    // 越界值防御：spdlog::level::level_enum 是 int 底层枚举，
    // 越界值会破坏 should_log 比较，前端直接拒绝（不推送、不持久化）
    settings.setLogLevel(-1);
    settings.setLogLevel(6);
    settings.setLogLevel(99);
    QCOMPARE(settings.logLevel(), 4);
    QCOMPARE(pushed.size(), 2);

    // 拒绝不写入存储
    QCOMPARE(storedValue(QStringLiteral("logging"), QStringLiteral("logLevel")).toInt(), 4);

    // applyLogLevel：推送当前值，不持久化（启动路径：reload 后同步后端）
    removeStored(QStringLiteral("logging"), QStringLiteral("logLevel"));
    settings.applyLogLevel();
    QCOMPARE(pushed.size(), 3);
    QCOMPARE(pushed.at(2), 4);
    QVERIFY(!storedContains(QStringLiteral("logging"), QStringLiteral("logLevel")));

    // 无 executor（mock-only）时 setter/applyLogLevel 无副作用
    Seriona::App::SettingsController mockController;
    mockController.setLogLevel(3);
    mockController.applyLogLevel();
    QCOMPARE(mockController.logLevel(), 3);

    // reload 防御：存储中的非法值回退默认
    m_store.insert(storageKey(QStringLiteral("logging"), QStringLiteral("logLevel")), 42);
    Seriona::App::SettingsController corruptReader;
    corruptReader.setSettingsStorageBackend(testBackend());
    corruptReader.reloadFromSettings();
    QCOMPARE(corruptReader.logLevel(), 2);
}

void SettingsControllerTest::lyricDelimitersPersistRoundTrip()
{
    const QStringList delimiters{QStringLiteral(" / "), QStringLiteral(" | "), QStringLiteral(" - ")};
    {
        Seriona::App::SettingsController writer;
        writer.setSettingsStorageBackend(testBackend());
        writer.setLyricDelimiters(delimiters);
        QCOMPARE(writer.lyricDelimiters(), delimiters);
    }

    Seriona::App::SettingsController reader;
    reader.setSettingsStorageBackend(testBackend());
    reader.reloadFromSettings();
    QCOMPARE(reader.lyricDelimiters(), delimiters);

    // 空列表语义：合法（清空后歌词不切分），持久化并重读一致，不得崩溃
    {
        Seriona::App::SettingsController emptyWriter;
        emptyWriter.setSettingsStorageBackend(testBackend());
        emptyWriter.setLyricDelimiters({});
        QVERIFY(emptyWriter.lyricDelimiters().isEmpty());
    }
    Seriona::App::SettingsController emptyReader;
    emptyReader.setSettingsStorageBackend(testBackend());
    emptyReader.reloadFromSettings();
    QVERIFY(emptyReader.lyricDelimiters().isEmpty());
}

void SettingsControllerTest::sampleFormatPersistRoundTrip()
{
    {
        Seriona::App::SettingsController writer;
        writer.setSettingsStorageBackend(testBackend());
        writer.setSampleFormat(4);
        QCOMPARE(writer.sampleFormat(), 4);
    }

    Seriona::App::SettingsController reader;
    reader.setSettingsStorageBackend(testBackend());
    reader.reloadFromSettings();
    QCOMPARE(reader.sampleFormat(), 4);

    // 未写入时默认 0
    removeStored(QStringLiteral("output"), QStringLiteral("sampleFormat"));
    Seriona::App::SettingsController defaultReader;
    defaultReader.setSettingsStorageBackend(testBackend());
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
    settings.setSettingsStorageBackend(testBackend());
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

    // 回退不持久化：存储保持 setter 写入的值
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("outputMode")).toInt(), 1);
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("sampleFormat")).toInt(), 2);
    QCOMPARE(storedValue(QStringLiteral("output"), QStringLiteral("bufferDurationMs")).toInt(), 500);
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
