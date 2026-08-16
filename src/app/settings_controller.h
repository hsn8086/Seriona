#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include <functional>

namespace Seriona::App {

// 输出设置控制器：QSettings 持久化 + 后端 ConfigureOutput 透传。
// 本类为纯 QML 面，不依赖后端头文件/宏；与后端通信经 AppFacade 注入的 executor
// （BackendBridge::submitConfigureOutput / enumeratePlaybackDevices）。
//
// 推送策略：
//  - 离散控件（outputMode / sampleRate / preferredDeviceId）变更立即推送；
//  - 连续控件（bufferDurationMs）变更先去抖（单发 QTimer，400ms）后推送。
class SettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int outputMode READ outputMode WRITE setOutputMode NOTIFY outputModeChanged)
    Q_PROPERTY(QStringList playbackDevices READ playbackDevices NOTIFY playbackDevicesChanged)
    Q_PROPERTY(QString preferredDeviceId READ preferredDeviceId WRITE setPreferredDeviceId NOTIFY preferredDeviceIdChanged)
    Q_PROPERTY(int sampleRate READ sampleRate WRITE setSampleRate NOTIFY sampleRateChanged)
    Q_PROPERTY(int bufferDurationMs READ bufferDurationMs WRITE setBufferDurationMs NOTIFY bufferDurationMsChanged)
    QML_ELEMENT
    QML_UNCREATABLE("SettingsController is owned by AppFacade")

public:
    using ApplyOutputConfigExecutor =
        std::function<void(int outputMode, int sampleRate, int bufferDurationMs, const QString &preferredDeviceId)>;
    using EnumerateDevicesExecutor = std::function<QStringList()>;

    explicit SettingsController(QObject *parent = nullptr);

    int outputMode() const;
    void setOutputMode(int mode);

    QStringList playbackDevices() const;

    QString preferredDeviceId() const;
    void setPreferredDeviceId(const QString &deviceId);

    int sampleRate() const;
    void setSampleRate(int sampleRate);

    int bufferDurationMs() const;
    void setBufferDurationMs(int bufferDurationMs);

    // 后端协商结果落地：只更新属性（含 NOTIFY），不持久化、不推送。
    void setDefaults(int outputMode, int sampleRate, int bufferDurationMs, const QString &preferredDeviceId);

    // QSettings 读取 → 属性（不推送）。
    Q_INVOKABLE void reloadFromSettings();
    // 组装当前属性并提交后端 ConfigureOutput 命令。
    Q_INVOKABLE void apply();
    // 枚举后端输出设备 → playbackDevices。
    Q_INVOKABLE void enumerateDevices();

    void setApplyOutputConfigExecutor(ApplyOutputConfigExecutor executor);
    void setEnumerateDevicesExecutor(EnumerateDevicesExecutor executor);

signals:
    void outputModeChanged();
    void playbackDevicesChanged();
    void preferredDeviceIdChanged();
    void sampleRateChanged();
    void bufferDurationMsChanged();

private:
    void setOutputModeInternal(int mode);
    void setSampleRateInternal(int sampleRate);
    void setBufferDurationMsInternal(int bufferDurationMs);
    void setPreferredDeviceIdInternal(const QString &deviceId);
    void persistOutputValue(const char *key, const QVariant &value);
    void scheduleDebouncedApply();

    int m_outputMode = 0;
    QStringList m_playbackDevices;
    QString m_preferredDeviceId;
    int m_sampleRate = 48000;
    int m_bufferDurationMs = 300;
    QTimer m_debounceTimer;
    ApplyOutputConfigExecutor m_applyOutputConfigExecutor;
    EnumerateDevicesExecutor m_enumerateDevicesExecutor;
};

}
