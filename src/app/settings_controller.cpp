#include "settings_controller.h"

#include <QCoreApplication>
#include <QSettings>
#include <QVariant>

namespace Seriona::App {

namespace {

constexpr auto kSettingsFileProperty = "seriona.settingsFileForTests";
constexpr auto kOutputGroup = "output";
constexpr auto kOutputModeKey = "outputMode";
constexpr auto kSampleRateKey = "sampleRate";
constexpr auto kBufferDurationMsKey = "bufferDurationMs";
constexpr auto kPreferredDeviceIdKey = "preferredDeviceId";

constexpr int kDefaultOutputMode = 0; // Direct
constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultBufferDurationMs = 300;
constexpr int kMinSampleRate = 8000;
constexpr int kMaxSampleRate = 768000;
constexpr int kMinBufferDurationMs = 50;
constexpr int kMaxBufferDurationMs = 1000;
// 连续控件去抖窗口（300-500ms 要求区间内）
constexpr int kDebounceIntervalMs = 400;

QSettings applicationSettings()
{
    const QCoreApplication *application = QCoreApplication::instance();
    if (application) {
        const QString settingsFile = application->property(kSettingsFileProperty).toString();
        if (!settingsFile.isEmpty()) {
            return QSettings(settingsFile, QSettings::IniFormat);
        }
    }

    return QSettings(QStringLiteral("Seriona"), QStringLiteral("Seriona"));
}

bool isValidOutputMode(int mode)
{
    return mode == 0 || mode == 1;
}

bool isValidSampleRate(int sampleRate)
{
    // 0 = 跟随设备
    return sampleRate == 0 || (sampleRate >= kMinSampleRate && sampleRate <= kMaxSampleRate);
}

bool isValidBufferDurationMs(int bufferDurationMs)
{
    return bufferDurationMs >= kMinBufferDurationMs && bufferDurationMs <= kMaxBufferDurationMs;
}

} // namespace

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
{
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(kDebounceIntervalMs);
    connect(&m_debounceTimer, &QTimer::timeout, this, &SettingsController::apply);
}

int SettingsController::outputMode() const
{
    return m_outputMode;
}

QStringList SettingsController::playbackDevices() const
{
    return m_playbackDevices;
}

QString SettingsController::preferredDeviceId() const
{
    return m_preferredDeviceId;
}

int SettingsController::sampleRate() const
{
    return m_sampleRate;
}

int SettingsController::bufferDurationMs() const
{
    return m_bufferDurationMs;
}

void SettingsController::setOutputMode(int mode)
{
    if (!isValidOutputMode(mode) || m_outputMode == mode) {
        return;
    }
    setOutputModeInternal(mode);
    persistOutputValue(kOutputModeKey, mode);
    apply();
}

void SettingsController::setSampleRate(int sampleRate)
{
    if (!isValidSampleRate(sampleRate) || m_sampleRate == sampleRate) {
        return;
    }
    setSampleRateInternal(sampleRate);
    persistOutputValue(kSampleRateKey, sampleRate);
    apply();
}

void SettingsController::setBufferDurationMs(int bufferDurationMs)
{
    if (!isValidBufferDurationMs(bufferDurationMs) || m_bufferDurationMs == bufferDurationMs) {
        return;
    }
    setBufferDurationMsInternal(bufferDurationMs);
    persistOutputValue(kBufferDurationMsKey, bufferDurationMs);
    scheduleDebouncedApply();
}

void SettingsController::setPreferredDeviceId(const QString &deviceId)
{
    if (m_preferredDeviceId == deviceId) {
        return;
    }
    setPreferredDeviceIdInternal(deviceId);
    persistOutputValue(kPreferredDeviceIdKey, deviceId);
    apply();
}

void SettingsController::setDefaults(
    int outputMode,
    int sampleRate,
    int bufferDurationMs,
    const QString &preferredDeviceId)
{
    setOutputModeInternal(outputMode);
    setSampleRateInternal(sampleRate);
    setBufferDurationMsInternal(bufferDurationMs);
    setPreferredDeviceIdInternal(preferredDeviceId);
}

void SettingsController::reloadFromSettings()
{
    QSettings settings = applicationSettings();
    settings.beginGroup(QString::fromUtf8(kOutputGroup));
    const int mode = settings.value(QString::fromUtf8(kOutputModeKey), kDefaultOutputMode).toInt();
    const int sampleRate = settings.value(QString::fromUtf8(kSampleRateKey), kDefaultSampleRate).toInt();
    const int bufferDurationMs = settings.value(QString::fromUtf8(kBufferDurationMsKey), kDefaultBufferDurationMs).toInt();
    const QString deviceId = settings.value(QString::fromUtf8(kPreferredDeviceIdKey)).toString();
    settings.endGroup();

    setOutputModeInternal(mode);
    setSampleRateInternal(sampleRate);
    setBufferDurationMsInternal(bufferDurationMs);
    setPreferredDeviceIdInternal(deviceId);
}

void SettingsController::apply()
{
    if (!m_applyOutputConfigExecutor) {
        return;
    }
    m_applyOutputConfigExecutor(m_outputMode, m_sampleRate, m_bufferDurationMs, m_preferredDeviceId);
}

void SettingsController::enumerateDevices()
{
    if (!m_enumerateDevicesExecutor) {
        return;
    }
    const QStringList devices = m_enumerateDevicesExecutor();
    if (devices == m_playbackDevices) {
        return;
    }
    m_playbackDevices = devices;
    emit playbackDevicesChanged();
}

void SettingsController::setApplyOutputConfigExecutor(ApplyOutputConfigExecutor executor)
{
    m_applyOutputConfigExecutor = std::move(executor);
}

void SettingsController::setEnumerateDevicesExecutor(EnumerateDevicesExecutor executor)
{
    m_enumerateDevicesExecutor = std::move(executor);
}

void SettingsController::setOutputModeInternal(int mode)
{
    if (!isValidOutputMode(mode) || m_outputMode == mode) {
        return;
    }
    m_outputMode = mode;
    emit outputModeChanged();
}

void SettingsController::setSampleRateInternal(int sampleRate)
{
    if (!isValidSampleRate(sampleRate) || m_sampleRate == sampleRate) {
        return;
    }
    m_sampleRate = sampleRate;
    emit sampleRateChanged();
}

void SettingsController::setBufferDurationMsInternal(int bufferDurationMs)
{
    if (!isValidBufferDurationMs(bufferDurationMs) || m_bufferDurationMs == bufferDurationMs) {
        return;
    }
    m_bufferDurationMs = bufferDurationMs;
    emit bufferDurationMsChanged();
}

void SettingsController::setPreferredDeviceIdInternal(const QString &deviceId)
{
    if (m_preferredDeviceId == deviceId) {
        return;
    }
    m_preferredDeviceId = deviceId;
    emit preferredDeviceIdChanged();
}

void SettingsController::scheduleDebouncedApply()
{
    m_debounceTimer.start();
}

void SettingsController::persistOutputValue(const char *key, const QVariant &value)
{
    QSettings settings = applicationSettings();
    settings.beginGroup(QString::fromUtf8(kOutputGroup));
    settings.setValue(QString::fromUtf8(key), value);
    settings.endGroup();
    settings.sync();
}

}
