#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QList>
#include <QPair>

#include <functional>

namespace Seriona::App {

// 每台输出设备的能力（后端 AudioDeviceFormat 的 supportedSampleFormats/supportedSampleRates
// 映射，经 BackendBridge::enumeratePlaybackDeviceCapabilities 填充）。
// sampleFormats/sampleRates 为空 = 未枚举或全支持（T4 语义：miniaudio 的
// ma_format_unknown / sampleRate==0 不产生条目），设置窗口下拉显示全部标准选项。
struct PlaybackDeviceCapabilities {
    QString deviceId;
    QString deviceName;
    QList<int> sampleFormats{};
    QList<int> sampleRates{};
};

// 输出设置控制器：QSettings 持久化 + 后端 ConfigureOutput 透传。
// 本类为纯 QML 面，不依赖后端头文件/宏；与后端通信经 AppFacade 注入的 executor
// （BackendBridge::submitConfigureOutput / enumeratePlaybackDeviceCapabilities）。
//
// 推送策略：
//  - 离散控件（outputMode / sampleRate / sampleFormat / preferredDeviceId）变更立即推送；
//  - 连续控件（bufferDurationMs）变更先去抖（单发 QTimer，400ms）后推送。
//  - 后端拒绝 ConfigureOutput 时经 rollbackRejectedOutputConfig() 恢复上一次已提交的
//    合法值快照（lastValid*），快照在每次 apply() 提交前记录。
//
// 设备能力过滤（需求 3 前端）：
//  - sampleRateOptions()/sampleFormatOptions() 按当前选中设备（preferredDeviceId）的
//    能力与标准列表求交；空能力 = 全支持 = 显示全部。
//  - 已保存值不在过滤结果中时保留并标注（「设备不支持」后缀），已保存值本身不变。
//  - sampleParamsGreyed()：直接输出模式（outputMode==0）下采样率/位深行灰化（需求 1）。
class SettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int outputMode READ outputMode WRITE setOutputMode NOTIFY outputModeChanged)
    Q_PROPERTY(QStringList playbackDevices READ playbackDevices NOTIFY playbackDevicesChanged)
    Q_PROPERTY(QStringList playbackDeviceNames READ playbackDeviceNames NOTIFY playbackDeviceNamesChanged)
    Q_PROPERTY(QString preferredDeviceId READ preferredDeviceId WRITE setPreferredDeviceId NOTIFY preferredDeviceIdChanged)
    Q_PROPERTY(int sampleRate READ sampleRate WRITE setSampleRate NOTIFY sampleRateChanged)
    Q_PROPERTY(int sampleFormat READ sampleFormat WRITE setSampleFormat NOTIFY sampleFormatChanged)
    Q_PROPERTY(int bufferDurationMs READ bufferDurationMs WRITE setBufferDurationMs NOTIFY bufferDurationMsChanged)
    Q_PROPERTY(QStringList lyricDelimiters READ lyricDelimiters WRITE setLyricDelimiters NOTIFY lyricDelimitersChanged)
    Q_PROPERTY(int logLevel READ logLevel WRITE setLogLevel NOTIFY logLevelChanged)
    Q_PROPERTY(bool sampleParamsGreyed READ sampleParamsGreyed NOTIFY outputModeChanged)
    Q_PROPERTY(QVariantList sampleRateOptions READ sampleRateOptions NOTIFY sampleRateOptionsChanged)
    Q_PROPERTY(QVariantList sampleFormatOptions READ sampleFormatOptions NOTIFY sampleFormatOptionsChanged)
    Q_PROPERTY(QVariantList playbackDeviceCapabilities READ playbackDeviceCapabilities NOTIFY playbackDeviceCapabilitiesChanged)
    QML_ELEMENT
    QML_UNCREATABLE("SettingsController is owned by AppFacade")

public:
    using ApplyOutputConfigExecutor =
        std::function<void(int outputMode, int sampleRate, int sampleFormat, int bufferDurationMs, const QString &preferredDeviceId)>;
    using EnumerateDevicesExecutor = std::function<QList<PlaybackDeviceCapabilities>()>;
    using LogLevelExecutor = std::function<void(int level)>;

    explicit SettingsController(QObject *parent = nullptr);

    // 日志等级：int 值与 spdlog::level::level_enum 一致（trace=0 .. critical=5），
    // 经 LogLevelExecutor 同步后端（seriona::app::setLogLevel）。纯 QML 面不依赖
    // 后端头文件，映射（字符串↔枚举）在前端完成。
    int logLevel() const;
    void setLogLevel(int level);
    static int logLevelFromString(const QString &name);
    static QString logLevelToString(int level);
    Q_INVOKABLE void applyLogLevel();

    int outputMode() const;
    void setOutputMode(int mode);

    QStringList playbackDevices() const;
    QStringList playbackDeviceNames() const;

    QString preferredDeviceId() const;
    void setPreferredDeviceId(const QString &deviceId);

    int sampleRate() const;
    void setSampleRate(int sampleRate);

    int sampleFormat() const;
    void setSampleFormat(int sampleFormat);

    int bufferDurationMs() const;
    void setBufferDurationMs(int bufferDurationMs);

    QStringList lyricDelimiters() const;
    void setLyricDelimiters(const QStringList &delimiters);

    bool sampleParamsGreyed() const;
    QVariantList sampleRateOptions() const;
    QVariantList sampleFormatOptions() const;
    QVariantList playbackDeviceCapabilities() const;

    // 后端协商结果落地：只更新属性（含 NOTIFY），不持久化、不推送。
    void setDefaults(int outputMode, int sampleRate, int bufferDurationMs, const QString &preferredDeviceId);

    // QSettings 读取 → 属性（不推送）。
    Q_INVOKABLE void reloadFromSettings();
    // 组装当前属性并提交后端 ConfigureOutput 命令。
    Q_INVOKABLE void apply();
    // 后端拒绝 ConfigureOutput 时恢复上一次已提交的合法值快照（emit NOTIFY，不持久化、不推送）。
    Q_INVOKABLE void rollbackRejectedOutputConfig();
    // 枚举后端输出设备 → playbackDevices。
    Q_INVOKABLE void enumerateDevices();

    void setApplyOutputConfigExecutor(ApplyOutputConfigExecutor executor);
    void setEnumerateDevicesExecutor(EnumerateDevicesExecutor executor);
    void setLogLevelExecutor(LogLevelExecutor executor);

signals:
    void outputModeChanged();
    void playbackDevicesChanged();
    void playbackDeviceNamesChanged();
    void preferredDeviceIdChanged();
    void sampleRateChanged();
    void sampleFormatChanged();
    void bufferDurationMsChanged();
    void lyricDelimitersChanged();
    void logLevelChanged();
    void sampleRateOptionsChanged();
    void sampleFormatOptionsChanged();
    void playbackDeviceCapabilitiesChanged();

private:
    void setOutputModeInternal(int mode);
    void setSampleRateInternal(int sampleRate);
    void setSampleFormatInternal(int sampleFormat);
    void setBufferDurationMsInternal(int bufferDurationMs);
    void setPreferredDeviceIdInternal(const QString &deviceId);
    void setLyricDelimitersInternal(const QStringList &delimiters);
    void setLogLevelInternal(int level);
    void persistValue(const char *group, const char *key, const QVariant &value);
    void persistOutputValue(const char *key, const QVariant &value);
    void scheduleDebouncedApply();
    void recordLastValidSnapshot();
    const PlaybackDeviceCapabilities *selectedDeviceCaps() const;
    QVariantList buildOptions(const QList<int> &standardValues,
                              const QList<int> &supportedValues,
                              int savedValue,
                              bool isSampleRate) const;
    QString sampleRateLabel(int value) const;
    QString sampleFormatLabel(int value) const;

    int m_outputMode = 0;
    QStringList m_playbackDevices;
    QStringList m_playbackDeviceNames;
    QString m_preferredDeviceId;
    int m_sampleRate = 48000;
    int m_sampleFormat = 0;
    int m_bufferDurationMs = 300;
    QStringList m_lyricDelimiters = {QStringLiteral(" / ")};
    int m_logLevel = 2;
    int m_lastValidOutputMode = 0;
    int m_lastValidSampleRate = 48000;
    int m_lastValidSampleFormat = 0;
    int m_lastValidBufferDurationMs = 300;
    QString m_lastValidPreferredDeviceId;
    QList<PlaybackDeviceCapabilities> m_deviceCapabilities;
    bool m_hasCommittedSnapshot = false;
    QTimer m_debounceTimer;
    ApplyOutputConfigExecutor m_applyOutputConfigExecutor;
    EnumerateDevicesExecutor m_enumerateDevicesExecutor;
    LogLevelExecutor m_logLevelExecutor;
};

}
