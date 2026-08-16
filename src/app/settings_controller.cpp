#include "settings_controller.h"

#include <QCoreApplication>
#include <QSettings>
#include <QVariant>

namespace Seriona::App {

namespace {

constexpr auto kSettingsFileProperty = "seriona.settingsFileForTests";
constexpr auto kOutputGroup = "output";
constexpr auto kLyricsGroup = "lyrics";
constexpr auto kOutputModeKey = "outputMode";
constexpr auto kSampleRateKey = "sampleRate";
constexpr auto kSampleFormatKey = "sampleFormat";
constexpr auto kBufferDurationMsKey = "bufferDurationMs";
constexpr auto kPreferredDeviceIdKey = "preferredDeviceId";
constexpr auto kLyricsDelimitersKey = "delimiters";

constexpr int kDefaultOutputMode = 0; // Direct
constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultSampleFormat = 0; // 跟随设备
constexpr int kDefaultBufferDurationMs = 300;
constexpr int kMinSampleRate = 8000;
constexpr int kMaxSampleRate = 768000;
constexpr int kMinBufferDurationMs = 50;
constexpr int kMaxBufferDurationMs = 1000;
// 连续控件去抖窗口（300-500ms 要求区间内）
constexpr int kDebounceIntervalMs = 400;

const QStringList kDefaultLyricDelimiters = {QStringLiteral(" / ")};

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

bool isValidSampleFormat(int sampleFormat)
{
    // 0 = 设备默认；1/2/4 对应后端 AudioSampleFormat 的 Int16/Int24/Float32
    return sampleFormat == 0 || sampleFormat == 1 || sampleFormat == 2 || sampleFormat == 4;
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

QStringList SettingsController::playbackDeviceNames() const
{
    return m_playbackDeviceNames;
}

QString SettingsController::preferredDeviceId() const
{
    return m_preferredDeviceId;
}

int SettingsController::sampleRate() const
{
    return m_sampleRate;
}

int SettingsController::sampleFormat() const
{
    return m_sampleFormat;
}

int SettingsController::bufferDurationMs() const
{
    return m_bufferDurationMs;
}

QStringList SettingsController::lyricDelimiters() const
{
    return m_lyricDelimiters;
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

void SettingsController::setSampleFormat(int sampleFormat)
{
    if (!isValidSampleFormat(sampleFormat) || m_sampleFormat == sampleFormat) {
        return;
    }
    setSampleFormatInternal(sampleFormat);
    persistOutputValue(kSampleFormatKey, sampleFormat);
    apply();
}

void SettingsController::setLyricDelimiters(const QStringList &delimiters)
{
    if (m_lyricDelimiters == delimiters) {
        return;
    }
    setLyricDelimitersInternal(delimiters);
    persistValue(kLyricsGroup, kLyricsDelimitersKey, delimiters);
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
    const int sampleFormat = settings.value(QString::fromUtf8(kSampleFormatKey), kDefaultSampleFormat).toInt();
    const int bufferDurationMs = settings.value(QString::fromUtf8(kBufferDurationMsKey), kDefaultBufferDurationMs).toInt();
    const QString deviceId = settings.value(QString::fromUtf8(kPreferredDeviceIdKey)).toString();
    settings.endGroup();

    settings.beginGroup(QString::fromUtf8(kLyricsGroup));
    const QStringList delimiters = settings.value(QString::fromUtf8(kLyricsDelimitersKey), kDefaultLyricDelimiters).toStringList();
    settings.endGroup();

    setOutputModeInternal(mode);
    setSampleRateInternal(sampleRate);
    setSampleFormatInternal(sampleFormat);
    setBufferDurationMsInternal(bufferDurationMs);
    setPreferredDeviceIdInternal(deviceId);
    setLyricDelimitersInternal(delimiters);
}

void SettingsController::apply()
{
    if (!m_applyOutputConfigExecutor) {
        return;
    }
    recordLastValidSnapshot();
    m_applyOutputConfigExecutor(m_outputMode, m_sampleRate, m_sampleFormat, m_bufferDurationMs, m_preferredDeviceId);
}

void SettingsController::rollbackRejectedOutputConfig()
{
    if (!m_hasCommittedSnapshot) {
        return;
    }
    setOutputModeInternal(m_lastValidOutputMode);
    setSampleRateInternal(m_lastValidSampleRate);
    setSampleFormatInternal(m_lastValidSampleFormat);
    setBufferDurationMsInternal(m_lastValidBufferDurationMs);
    setPreferredDeviceIdInternal(m_lastValidPreferredDeviceId);
}

void SettingsController::enumerateDevices()
{
    if (!m_enumerateDevicesExecutor) {
        return;
    }
    const QList<QPair<QString, QString>> devices = m_enumerateDevicesExecutor();
    QStringList ids;
    QStringList names;
    ids.reserve(devices.size());
    names.reserve(devices.size());
    for (const auto &device : devices) {
        ids.append(device.first);
        names.append(device.second);
    }
    const bool idsChanged = ids != m_playbackDevices;
    const bool namesChanged = names != m_playbackDeviceNames;
    if (!idsChanged && !namesChanged) {
        return;
    }
    m_playbackDevices = ids;
    m_playbackDeviceNames = names;
    if (idsChanged) {
        emit playbackDevicesChanged();
    }
    if (namesChanged) {
        emit playbackDeviceNamesChanged();
    }
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

void SettingsController::setSampleFormatInternal(int sampleFormat)
{
    if (!isValidSampleFormat(sampleFormat) || m_sampleFormat == sampleFormat) {
        return;
    }
    m_sampleFormat = sampleFormat;
    emit sampleFormatChanged();
}

void SettingsController::setLyricDelimitersInternal(const QStringList &delimiters)
{
    if (m_lyricDelimiters == delimiters) {
        return;
    }
    m_lyricDelimiters = delimiters;
    emit lyricDelimitersChanged();
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

void SettingsController::recordLastValidSnapshot()
{
    m_lastValidOutputMode = m_outputMode;
    m_lastValidSampleRate = m_sampleRate;
    m_lastValidSampleFormat = m_sampleFormat;
    m_lastValidBufferDurationMs = m_bufferDurationMs;
    m_lastValidPreferredDeviceId = m_preferredDeviceId;
    m_hasCommittedSnapshot = true;
}

void SettingsController::persistValue(const char *group, const char *key, const QVariant &value)
{
    QSettings settings = applicationSettings();
    settings.beginGroup(QString::fromUtf8(group));
    settings.setValue(QString::fromUtf8(key), value);
    settings.endGroup();
    settings.sync();
}

void SettingsController::persistOutputValue(const char *key, const QVariant &value)
{
    persistValue(kOutputGroup, key, value);
}

}
