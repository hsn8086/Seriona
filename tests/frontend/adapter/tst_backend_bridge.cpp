#include "backend_bridge.h"

#include "seriona/audio/audio_contracts.h"
#include "seriona/metadata/metadata_contracts.h"
#include "seriona/scanner/scanner_contracts.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QSignalSpy>
#include <QThread>
#include <QtTest/QTest>

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace {

class FakeAudioPlaybackService final : public seriona::audio::AudioPlaybackService
{
public:
    void setEventSink(seriona::audio::BackendEventSink sink) override
    {
        std::scoped_lock lock(m_mutex);
        m_eventSink = std::move(sink);
    }

    void configureOutput(const seriona::audio::AudioOutputConfig &) override { }
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

private:
    mutable std::mutex m_mutex;
    seriona::audio::BackendEventSink m_eventSink;
    int m_stopCalls = 0;
};

class FakeFileScannerService final : public seriona::scanner::FileScannerService
{
public:
    void setEventSink(seriona::scanner::ScannerEventSink sink) override { m_eventSink = std::move(sink); }
    void configure(const seriona::scanner::ScannerConfig &) override { }
    void scan(const std::vector<seriona::scanner::ScannerRoot> &, seriona::scanner::ScanMode) override { }
    void startWatching(const std::vector<seriona::scanner::ScannerRoot> &) override { }
    void stopWatching() override { }
    void stop() override { }

    seriona::scanner::PlaylistTreeSnapshot snapshot() const override
    {
        return {};
    }

private:
    seriona::scanner::ScannerEventSink m_eventSink;
};

class FakeMetadataSharingService final : public seriona::metadata::MetadataSharingService
{
public:
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
        handle.unsubscribe = [this] {
            m_commandSink = {};
        };
        return handle;
    }

    seriona::metadata::MetadataSyncResult start(const seriona::metadata::PlatformMediaState &) override
    {
        return acceptedResult();
    }

    seriona::metadata::MetadataSyncResult update(const seriona::metadata::PlatformMediaState &) override
    {
        return acceptedResult();
    }

    seriona::metadata::MetadataSyncResult stop() override
    {
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
};

struct ControllerHarness {
    std::shared_ptr<FakeAudioPlaybackService> audio = std::make_shared<FakeAudioPlaybackService>();
    std::shared_ptr<FakeFileScannerService> scanner = std::make_shared<FakeFileScannerService>();

    Seriona::App::BackendBridge::ControllerFactory factory(bool runInlineForTests)
    {
        struct FactoryState {
            seriona::control::MediaControllerDependencies dependencies;
            seriona::control::MediaControllerOptions options;
        };

        auto state = std::make_shared<FactoryState>();
        state->dependencies.audio = audio;
        state->dependencies.scanner = scanner;
        state->dependencies.metadata = std::make_unique<FakeMetadataSharingService>();
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

}

class BackendBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    void threading();
    void shutdown();
    void shutdownStopSent();
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

QTEST_GUILESS_MAIN(BackendBridgeTest)

#include "tst_backend_bridge.moc"
