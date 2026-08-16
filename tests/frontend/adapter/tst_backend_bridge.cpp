#include "backend_bridge.h"

#include "settings_controller.h"

#include "seriona/audio/audio_contracts.h"
#include "seriona/control/folder_sort_settings_store.h"
#include "seriona/metadata/metadata_contracts.h"
#include "seriona/scanner/scanner_contracts.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest/QTest>

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

class FakeAudioPlaybackService final : public seriona::audio::AudioPlaybackService
{
public:
    void setEventSink(seriona::audio::BackendEventSink sink) override
    {
        std::scoped_lock lock(m_mutex);
        if (!sink) {
            ++m_eventSinkClearCalls;
        }
        m_eventSink = std::move(sink);
    }

    void configureOutput(const seriona::audio::AudioOutputConfig &config) override
    {
        std::scoped_lock lock(m_mutex);
        ++m_configureOutputCalls;
        m_lastOutputConfig = config;
    }

    void loadTrack(const seriona::audio::TrackPlaybackRequest &) override { }
    void prepareNext(const seriona::audio::TrackPlaybackRequest &) override { }
    void play() override { }
    void pause() override { }
    void resume() override { }

    void stop() override
    {
        std::scoped_lock lock(m_mutex);
        ++m_stopCalls;
    }

    void seek(std::chrono::milliseconds) override { }
    void setVolume(float) override { }
    void setMuted(bool) override { }
    void selectOutputDevice(const std::string &) override { }

    seriona::audio::PlaybackClockSnapshot queryPlaybackClock() const override
    {
        return {};
    }

    seriona::audio::AudioOutputConfig lastOutputConfig() const
    {
        std::scoped_lock lock(m_mutex);
        return m_lastOutputConfig;
    }

    int configureOutputCalls() const
    {
        std::scoped_lock lock(m_mutex);
        return m_configureOutputCalls;
    }

    std::vector<seriona::audio::AudioDeviceFormat> enumeratePlaybackDevices() const override
    {
        return {
            {"dev-1", "Device One", "pulse", 48000, seriona::audio::AudioSampleFormat::Int16, 2, 0,
             seriona::audio::AudioOutputMode::Mixed, false},
            {"dev-2", "Device Two", "alsa", 96000, seriona::audio::AudioSampleFormat::Float32, 2, 0,
             seriona::audio::AudioOutputMode::Mixed, false},
            {"", "Nameless Device", "pulse", 44100, seriona::audio::AudioSampleFormat::Unknown, 0, 0,
             seriona::audio::AudioOutputMode::Mixed, false},
        };
    }

    void emitEvent(seriona::audio::BackendEvent event)
    {
        seriona::audio::BackendEventSink sink;
        {
            std::scoped_lock lock(m_mutex);
            sink = m_eventSink;
        }
        if (sink) {
            sink(std::move(event));
        }
    }

    int stopCalls() const
    {
        std::scoped_lock lock(m_mutex);
        return m_stopCalls;
    }

    int eventSinkClearCalls() const
    {
        std::scoped_lock lock(m_mutex);
        return m_eventSinkClearCalls;
    }

private:
    mutable std::mutex m_mutex;
    seriona::audio::BackendEventSink m_eventSink;
    seriona::audio::AudioOutputConfig m_lastOutputConfig;
    int m_stopCalls = 0;
    int m_eventSinkClearCalls = 0;
    int m_configureOutputCalls = 0;
};

class FakeFileScannerService final : public seriona::scanner::FileScannerService
{
public:
    void setEventSink(seriona::scanner::ScannerEventSink sink) override
    {
        if (!sink) {
            ++m_eventSinkClearCalls;
        }
        m_eventSink = std::move(sink);
    }
    void configure(const seriona::scanner::ScannerConfig &) override { }
    void scan(const std::vector<seriona::scanner::ScannerRoot> &, seriona::scanner::ScanMode mode) override
    {
        m_lastScanMode = mode;
        ++m_scanCalls;
    }
    void startWatching(const std::vector<seriona::scanner::ScannerRoot> &) override { }
    void stopWatching() override { }
    void stop() override { }

    seriona::scanner::PlaylistTreeSnapshot snapshot() const override
    {
        return {};
    }

    int eventSinkClearCalls() const
    {
        return m_eventSinkClearCalls;
    }

    int scanCalls() const
    {
        return m_scanCalls;
    }

    std::optional<seriona::scanner::ScanMode> lastScanMode() const
    {
        return m_lastScanMode;
    }

private:
    seriona::scanner::ScannerEventSink m_eventSink;
    int m_eventSinkClearCalls = 0;
    int m_scanCalls = 0;
    std::optional<seriona::scanner::ScanMode> m_lastScanMode;
};

class RecordingFolderSortSettingsStore final : public seriona::control::FolderSortSettingsStore
{
public:
    void upsert(seriona::control::FolderSortSetting setting) override
    {
        ++m_upsertCalls;
        m_lastSetting = std::move(setting);
    }

    std::optional<seriona::control::FolderSortSetting> load(const std::filesystem::path &, const std::string &) const override
    {
        return std::nullopt;
    }

    void remove(const std::filesystem::path &, const std::string &) override { }

    std::vector<seriona::control::FolderSortSetting> list(const std::filesystem::path &) const override
    {
        return {};
    }

    int upsertCalls() const
    {
        return m_upsertCalls;
    }

    const std::optional<seriona::control::FolderSortSetting> &lastSetting() const
    {
        return m_lastSetting;
    }

private:
    int m_upsertCalls = 0;
    std::optional<seriona::control::FolderSortSetting> m_lastSetting;
};

struct FakeMetadataState {
    bool throwOnStart = false;
    int startCalls = 0;
    int stopCalls = 0;
    int unsubscribeCalls = 0;
};

class FakeMetadataSharingService final : public seriona::metadata::MetadataSharingService
{
public:
    explicit FakeMetadataSharingService(std::shared_ptr<FakeMetadataState> state)
        : m_state(std::move(state))
    {
    }

    seriona::metadata::MetadataBackendKind backendKind() const override
    {
        return seriona::metadata::MetadataBackendKind::Noop;
    }

    seriona::metadata::MetadataBackendCapabilities capabilities() const override
    {
        return {};
    }

    seriona::control::SubscriptionHandle registerCommandCallback(seriona::control::MediaControlCommandSink callback) override
    {
        m_commandSink = std::move(callback);

        seriona::control::SubscriptionHandle handle;
        handle.subscriptionId = 1;
        handle.unsubscribe = [this, state = m_state] {
            ++state->unsubscribeCalls;
            m_commandSink = {};
        };
        return handle;
    }

    seriona::metadata::MetadataSyncResult start(const seriona::metadata::PlatformMediaState &) override
    {
        ++m_state->startCalls;
        if (m_state->throwOnStart) {
            throw std::runtime_error("metadata start failed");
        }
        return acceptedResult();
    }

    seriona::metadata::MetadataSyncResult update(const seriona::metadata::PlatformMediaState &) override
    {
        return acceptedResult();
    }

    seriona::metadata::MetadataSyncResult stop() override
    {
        ++m_state->stopCalls;
        return acceptedResult();
    }

private:
    static seriona::metadata::MetadataSyncResult acceptedResult()
    {
        seriona::metadata::MetadataSyncResult result;
        result.accepted = true;
        return result;
    }

    seriona::control::MediaControlCommandSink m_commandSink;
    std::shared_ptr<FakeMetadataState> m_state;
};

struct ControllerHarness {
    std::shared_ptr<FakeAudioPlaybackService> audio = std::make_shared<FakeAudioPlaybackService>();
    std::shared_ptr<FakeFileScannerService> scanner = std::make_shared<FakeFileScannerService>();
    std::shared_ptr<FakeMetadataState> metadata = std::make_shared<FakeMetadataState>();
    std::shared_ptr<RecordingFolderSortSettingsStore> folderSortStore = std::make_shared<RecordingFolderSortSettingsStore>();

    Seriona::App::BackendBridge::ControllerFactory factory(bool runInlineForTests)
    {
        struct FactoryState {
            seriona::control::MediaControllerDependencies dependencies;
            seriona::control::MediaControllerOptions options;
        };

        auto state = std::make_shared<FactoryState>();
        state->dependencies.audio = audio;
        state->dependencies.scanner = scanner;
        state->dependencies.metadata = std::make_unique<FakeMetadataSharingService>(metadata);
        state->dependencies.folderSortSettingsStore = folderSortStore;
        state->options.runInlineForTests = runInlineForTests;

        return [state] {
            return seriona::control::makeMediaController(std::move(state->dependencies), state->options);
        };
    }
};

seriona::audio::BackendEvent makePlaybackStateEvent(std::uint64_t version, seriona::audio::PlaybackState state)
{
    seriona::audio::PlaybackStateChanged payload;
    payload.state = state;

    seriona::audio::BackendEvent event;
    event.type = seriona::audio::BackendEventType::PlaybackStateChanged;
    event.sourceModule = seriona::audio::BackendSourceModule::AudioPlaybackService;
    event.monotonicVersion = version;
    event.timestamp = std::chrono::steady_clock::now();
    event.payload = payload;
    return event;
}

void waitForInitialPlayerSnapshot(Seriona::App::BackendBridge &bridge)
{
    QSignalSpy playerSpy(&bridge, &Seriona::App::BackendBridge::playerSnapshotChanged);
    bridge.start();
    QTRY_VERIFY(playerSpy.count() > 0);
}

QVariantMap sortRule(const QString &field, const QString &order)
{
    QVariantMap rule;
    rule.insert(QStringLiteral("field"), field);
    rule.insert(QStringLiteral("order"), order);
    return rule;
}

QVariantList sortRules(std::initializer_list<QVariantMap> rules)
{
    QVariantList result;
    for (const QVariantMap &rule : rules) {
        result.append(rule);
    }
    return result;
}

}

class BackendBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    void threading();
    void shutdown();
    void shutdownStopSent();
    void shutdownSequence();
    void shutdownStartFailed();
    void scanLibraryDefaultsToFullMode();
    void scanLibraryForwardsIncrementalMode();
    void applyFolderSortRulesBuildsTypedBackendCommand();
    void applyFolderSortRulesAllowsEmptyRules();
    void applyFolderSortRulesRejectsInvalidPayloadWithoutDispatch();
    void applyFolderSortRulesRejectsMissingContextWithoutDispatch();
    void submitConfigureOutputBuildsTypedBackendCommand();
    void submitConfigureOutputRejectsInvalidPayloadWithoutDispatch();
    void enumeratePlaybackDevicesMapsDeviceIds();
    void settingsPushOnStart();
};

void BackendBridgeTest::threading()
{
    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(false));
    waitForInitialPlayerSnapshot(bridge);

    QThread *signalThread = nullptr;
    QSignalSpy playerSpy(&bridge, &Seriona::App::BackendBridge::playerSnapshotChanged);
    connect(&bridge, &Seriona::App::BackendBridge::playerSnapshotChanged, &bridge, [&signalThread] {
        signalThread = QThread::currentThread();
    });

    std::thread producer([&harness] {
        harness.audio->emitEvent(makePlaybackStateEvent(1, seriona::audio::PlaybackState::Playing));
    });
    producer.join();

    QTRY_VERIFY(playerSpy.count() > 0);
    QCOMPARE(signalThread, QCoreApplication::instance()->thread());
    QCOMPARE(bridge.playerSnapshot().playback.state, seriona::control::PlaybackStatus::Playing);

    bridge.shutdown();
}

void BackendBridgeTest::shutdown()
{
    ControllerHarness harness;
    auto bridge = std::make_unique<Seriona::App::BackendBridge>(harness.factory(true));
    waitForInitialPlayerSnapshot(*bridge);

    QSignalSpy playerSpy(bridge.get(), &Seriona::App::BackendBridge::playerSnapshotChanged);
    harness.audio->emitEvent(makePlaybackStateEvent(1, seriona::audio::PlaybackState::Playing));
    bridge->drainForTests();

    bridge->shutdown();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCOMPARE(playerSpy.count(), 0);

    bridge.reset();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QVERIFY(true);
}

void BackendBridgeTest::shutdownStopSent()
{
    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    QCOMPARE(harness.audio->stopCalls(), 0);
    bridge.shutdown();
    QCOMPARE(harness.audio->stopCalls(), 1);
}

void BackendBridgeTest::shutdownSequence()
{
    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    QSignalSpy shutdownSpy(&bridge, &Seriona::App::BackendBridge::shutdownCompleted);
    QSignalSpy playerSpy(&bridge, &Seriona::App::BackendBridge::playerSnapshotChanged);

    bridge.shutdown();

    QCOMPARE(harness.audio->stopCalls(), 1);
    QVERIFY(harness.audio->eventSinkClearCalls() >= 1);
    QVERIFY(harness.scanner->eventSinkClearCalls() >= 1);
    QCOMPARE(harness.metadata->unsubscribeCalls, 1);
    QCOMPARE(harness.metadata->stopCalls, 1);
    QCOMPARE(bridge.started(), false);
    QCOMPARE(bridge.shuttingDown(), true);
    QCOMPARE(shutdownSpy.count(), 1);

    harness.audio->emitEvent(makePlaybackStateEvent(2, seriona::audio::PlaybackState::Playing));
    bridge.drainForTests();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCOMPARE(playerSpy.count(), 0);

    const int audioEventSinkClearCalls = harness.audio->eventSinkClearCalls();
    const int scannerEventSinkClearCalls = harness.scanner->eventSinkClearCalls();
    bridge.shutdown();
    QCOMPARE(harness.audio->stopCalls(), 1);
    QCOMPARE(harness.audio->eventSinkClearCalls(), audioEventSinkClearCalls);
    QCOMPARE(harness.scanner->eventSinkClearCalls(), scannerEventSinkClearCalls);
    QCOMPARE(harness.metadata->stopCalls, 1);
    QCOMPARE(shutdownSpy.count(), 1);
}

void BackendBridgeTest::shutdownStartFailed()
{
    ControllerHarness harness;
    harness.metadata->throwOnStart = true;
    Seriona::App::BackendBridge bridge(harness.factory(true));

    QSignalSpy playerSpy(&bridge, &Seriona::App::BackendBridge::playerSnapshotChanged);
    bridge.start();

    QCOMPARE(bridge.started(), false);
    QCOMPARE(bridge.shuttingDown(), true);
    QCOMPARE(harness.metadata->startCalls, 1);
    QCOMPARE(harness.metadata->unsubscribeCalls, 1);
    QCOMPARE(harness.metadata->stopCalls, 1);

    harness.audio->emitEvent(makePlaybackStateEvent(3, seriona::audio::PlaybackState::Playing));
    bridge.drainForTests();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCOMPARE(playerSpy.count(), 0);

    bridge.shutdown();
    bridge.shutdown();
    QCOMPARE(harness.audio->stopCalls(), 0);
    QCOMPARE(harness.metadata->stopCalls, 1);
}

void BackendBridgeTest::scanLibraryDefaultsToFullMode()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());

    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    const seriona::control::MediaControllerCommandResult result = bridge.scanLibrary(musicDir.path());

    QVERIFY(result.accepted);
    QCOMPARE(harness.scanner->scanCalls(), 1);
    QVERIFY(harness.scanner->lastScanMode().has_value());
    QCOMPARE(*harness.scanner->lastScanMode(), seriona::scanner::ScanMode::Full);

    bridge.shutdown();
}

void BackendBridgeTest::scanLibraryForwardsIncrementalMode()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());

    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    const seriona::control::MediaControllerCommandResult result = bridge.scanLibrary(
        musicDir.path(),
        seriona::scanner::ScanMode::Incremental);

    QVERIFY(result.accepted);
    QCOMPARE(harness.scanner->scanCalls(), 1);
    QVERIFY(harness.scanner->lastScanMode().has_value());
    QCOMPARE(*harness.scanner->lastScanMode(), seriona::scanner::ScanMode::Incremental);

    bridge.shutdown();
}

void BackendBridgeTest::applyFolderSortRulesBuildsTypedBackendCommand()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    const QString rootPath = QFileInfo(musicDir.path()).absoluteFilePath();

    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    const seriona::control::MediaControllerCommandResult result = bridge.applyFolderSortRules(
        rootPath + QStringLiteral("/../") + QFileInfo(musicDir.path()).fileName(),
        QStringLiteral("  folder-jazz  "),
        sortRules({sortRule(QStringLiteral("title"), QStringLiteral("desc")),
                   sortRule(QStringLiteral("createdDate"), QStringLiteral("asc"))}));

    QVERIFY(result.accepted);
    QCOMPARE(result.code, seriona::control::MediaControllerErrorCode::None);
    QCOMPARE(harness.folderSortStore->upsertCalls(), 1);
    QVERIFY(harness.folderSortStore->lastSetting().has_value());
    const seriona::control::FolderSortSetting &setting = *harness.folderSortStore->lastSetting();
    QCOMPARE(QString::fromStdString(setting.rootPath.generic_string()), rootPath);
    QCOMPARE(QString::fromStdString(setting.folderNodeId), QStringLiteral("folder-jazz"));
    QCOMPARE(setting.rules.size(), std::size_t{2});
    QCOMPARE(setting.rules.at(0).field, seriona::control::FolderSortField::Title);
    QCOMPARE(setting.rules.at(0).direction, seriona::control::FolderSortDirection::Descending);
    QCOMPARE(setting.rules.at(0).missingValuePolicy, seriona::control::FolderSortMissingValuePolicy::Last);
    QCOMPARE(setting.rules.at(1).field, seriona::control::FolderSortField::CreatedDate);
    QCOMPARE(setting.rules.at(1).direction, seriona::control::FolderSortDirection::Ascending);
    QCOMPARE(setting.rules.at(1).missingValuePolicy, seriona::control::FolderSortMissingValuePolicy::Last);

    bridge.shutdown();
}

void BackendBridgeTest::applyFolderSortRulesAllowsEmptyRules()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    const QString rootPath = QFileInfo(musicDir.path()).absoluteFilePath();

    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    const seriona::control::MediaControllerCommandResult result = bridge.applyFolderSortRules(
        rootPath,
        QStringLiteral("folder-jazz"),
        QVariantList{});

    QCOMPARE(result.accepted, false);
    QCOMPARE(result.code, seriona::control::MediaControllerErrorCode::InvalidCommand);
    QVERIFY(QString::fromStdString(result.message).contains(QStringLiteral("sort rule"), Qt::CaseInsensitive));
    QCOMPARE(harness.folderSortStore->upsertCalls(), 0);
    QVERIFY(!harness.folderSortStore->lastSetting().has_value());

    bridge.shutdown();
}

void BackendBridgeTest::applyFolderSortRulesRejectsInvalidPayloadWithoutDispatch()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    const QString rootPath = QFileInfo(musicDir.path()).absoluteFilePath();

    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    const seriona::control::MediaControllerCommandResult invalidField = bridge.applyFolderSortRules(
        rootPath,
        QStringLiteral("folder-jazz"),
        sortRules({sortRule(QStringLiteral("unknownField"), QStringLiteral("asc"))}));
    QCOMPARE(invalidField.accepted, false);
    QCOMPARE(invalidField.code, seriona::control::MediaControllerErrorCode::InvalidCommand);
    QVERIFY(QString::fromStdString(invalidField.message).contains(QStringLiteral("field"), Qt::CaseInsensitive));

    const seriona::control::MediaControllerCommandResult invalidDirection = bridge.applyFolderSortRules(
        rootPath,
        QStringLiteral("folder-jazz"),
        sortRules({sortRule(QStringLiteral("title"), QStringLiteral("sideways"))}));
    QCOMPARE(invalidDirection.accepted, false);
    QCOMPARE(invalidDirection.code, seriona::control::MediaControllerErrorCode::InvalidCommand);
    QVERIFY(QString::fromStdString(invalidDirection.message).contains(QStringLiteral("direction"), Qt::CaseInsensitive));

    QVariantList malformed;
    malformed.append(QStringLiteral("not-a-map"));
    const seriona::control::MediaControllerCommandResult malformedPayload = bridge.applyFolderSortRules(
        rootPath,
        QStringLiteral("folder-jazz"),
        malformed);
    QCOMPARE(malformedPayload.accepted, false);
    QCOMPARE(malformedPayload.code, seriona::control::MediaControllerErrorCode::InvalidCommand);
    QVERIFY(QString::fromStdString(malformedPayload.message).contains(QStringLiteral("payload"), Qt::CaseInsensitive));
    QCOMPARE(harness.folderSortStore->upsertCalls(), 0);

    bridge.shutdown();
}

void BackendBridgeTest::applyFolderSortRulesRejectsMissingContextWithoutDispatch()
{
    QTemporaryDir musicDir;
    QVERIFY(musicDir.isValid());
    const QString rootPath = QFileInfo(musicDir.path()).absoluteFilePath();

    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    const seriona::control::MediaControllerCommandResult missingRoot = bridge.applyFolderSortRules(
        QString(),
        QStringLiteral("folder-jazz"),
        sortRules({sortRule(QStringLiteral("title"), QStringLiteral("asc"))}));
    QCOMPARE(missingRoot.accepted, false);
    QCOMPARE(missingRoot.code, seriona::control::MediaControllerErrorCode::InvalidCommand);
    QVERIFY(QString::fromStdString(missingRoot.message).contains(QStringLiteral("root"), Qt::CaseInsensitive));

    const seriona::control::MediaControllerCommandResult missingFolder = bridge.applyFolderSortRules(
        rootPath,
        QStringLiteral("   "),
        sortRules({sortRule(QStringLiteral("title"), QStringLiteral("asc"))}));
    QCOMPARE(missingFolder.accepted, false);
    QCOMPARE(missingFolder.code, seriona::control::MediaControllerErrorCode::InvalidCommand);
    QVERIFY(QString::fromStdString(missingFolder.message).contains(QStringLiteral("folder"), Qt::CaseInsensitive));
    QCOMPARE(harness.folderSortStore->upsertCalls(), 0);

    bridge.shutdown();
}

void BackendBridgeTest::submitConfigureOutputBuildsTypedBackendCommand()
{
    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    const seriona::control::MediaControllerCommandResult mixed = bridge.submitConfigureOutput(
        1, 96000, 500, QStringLiteral("dev-1"));
    QVERIFY(mixed.accepted);
    QCOMPARE(mixed.code, seriona::control::MediaControllerErrorCode::None);
    QCOMPARE(harness.audio->configureOutputCalls(), 1);
    const seriona::audio::AudioOutputConfig mixedConfig = harness.audio->lastOutputConfig();
    QCOMPARE(mixedConfig.outputMode, seriona::audio::AudioOutputMode::Mixed);
    QVERIFY(mixedConfig.targetSampleRate.has_value());
    QCOMPARE(*mixedConfig.targetSampleRate, std::uint32_t{96000});
    QCOMPARE(mixedConfig.bufferDuration, std::chrono::milliseconds(500));
    QCOMPARE(QString::fromStdString(mixedConfig.preferredDeviceId), QStringLiteral("dev-1"));

    // 0 采样率 = 跟随设备：不携带 targetSampleRate；空设备 id 表示默认设备
    const seriona::control::MediaControllerCommandResult direct = bridge.submitConfigureOutput(
        0, 0, 300, QString());
    QVERIFY(direct.accepted);
    QCOMPARE(direct.code, seriona::control::MediaControllerErrorCode::None);
    QCOMPARE(harness.audio->configureOutputCalls(), 2);
    const seriona::audio::AudioOutputConfig directConfig = harness.audio->lastOutputConfig();
    QCOMPARE(directConfig.outputMode, seriona::audio::AudioOutputMode::Direct);
    QVERIFY(!directConfig.targetSampleRate.has_value());
    QCOMPARE(directConfig.bufferDuration, std::chrono::milliseconds(300));
    QVERIFY(directConfig.preferredDeviceId.empty());

    bridge.shutdown();
}

void BackendBridgeTest::submitConfigureOutputRejectsInvalidPayloadWithoutDispatch()
{
    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    const seriona::control::MediaControllerCommandResult invalidMode = bridge.submitConfigureOutput(
        7, 48000, 300, QString());
    QCOMPARE(invalidMode.accepted, false);
    QCOMPARE(invalidMode.code, seriona::control::MediaControllerErrorCode::InvalidCommand);

    const seriona::control::MediaControllerCommandResult invalidRate = bridge.submitConfigureOutput(
        0, 100, 300, QString());
    QCOMPARE(invalidRate.accepted, false);
    QCOMPARE(invalidRate.code, seriona::control::MediaControllerErrorCode::InvalidCommand);

    const seriona::control::MediaControllerCommandResult invalidDuration = bridge.submitConfigureOutput(
        0, 48000, 2000, QString());
    QCOMPARE(invalidDuration.accepted, false);
    QCOMPARE(invalidDuration.code, seriona::control::MediaControllerErrorCode::InvalidCommand);

    QCOMPARE(harness.audio->configureOutputCalls(), 0);

    bridge.shutdown();
}

void BackendBridgeTest::enumeratePlaybackDevicesMapsDeviceIds()
{
    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    waitForInitialPlayerSnapshot(bridge);

    // 空 deviceId 的设备不参与映射（无法被选择）
    const QStringList devices = bridge.enumeratePlaybackDevices();
    QCOMPARE(devices, QStringList({QStringLiteral("dev-1"), QStringLiteral("dev-2")}));

    bridge.shutdown();
}

void BackendBridgeTest::settingsPushOnStart()
{
    QTemporaryFile settingsFile;
    QVERIFY(settingsFile.open());
    settingsFile.close();
    QCoreApplication::instance()->setProperty("seriona.settingsFileForTests", settingsFile.fileName());

    ControllerHarness harness;
    Seriona::App::BackendBridge bridge(harness.factory(true));
    Seriona::App::SettingsController settings;
    int pushCount = 0;
    settings.setApplyOutputConfigExecutor(
        [&pushCount](int, int, int, const QString &) {
            ++pushCount;
        });

    // AppFacade 启动挂钩契约：startedChanged 且 started() 为真时推送一次持久化配置
    connect(&bridge, &Seriona::App::BackendBridge::startedChanged, &bridge, [&] {
        if (!bridge.started()) {
            return;
        }
        settings.reloadFromSettings();
        settings.apply();
    });

    bridge.start();
    QCOMPARE(bridge.started(), true);
    QCOMPARE(pushCount, 1);

    // shutdown 的 startedChanged（started()==false）不得再次推送
    bridge.shutdown();
    QCOMPARE(pushCount, 1);

    QCoreApplication::instance()->setProperty("seriona.settingsFileForTests", QVariant{});
}

QTEST_GUILESS_MAIN(BackendBridgeTest)

#include "tst_backend_bridge.moc"
