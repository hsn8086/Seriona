#pragma once

#include <QHash>
#include <QString>
#include <QVariant>

#include <functional>
#include <optional>

namespace Seriona::App {

// 应用设置存储后端：read/write/remove 由 AppFacade 注入。
// 接入后端时经 BackendBridge → MediaController 键值存储；
// 不注入时控制器回退到内存存储（mock-only / smoke / 测试，进程内有效，
// 不持久化到磁盘）。
// read 返回 nullopt 表示后端不可用（未启动/已关闭/存储错误），由
// AppSettingsStorage 回退内存缓存；返回 QVariant 表示后端给出的值。
using AppSettingsRead =
    std::function<std::optional<QVariant>(const QString &group, const QString &key, const QVariant &defaultValue)>;
using AppSettingsWrite = std::function<void(const QString &group, const QString &key, const QVariant &value)>;
using AppSettingsRemove = std::function<void(const QString &group, const QString &key)>;

struct AppSettingsBackend {
    AppSettingsRead read;
    AppSettingsWrite write;
    AppSettingsRemove remove;
};

// 内存键值存储（进程内）；复合键 = group + '\x1f' + key。
class AppSettingsMemoryStore {
public:
    [[nodiscard]] QVariant read(const QString &group, const QString &key, const QVariant &defaultValue) const;
    void write(const QString &group, const QString &key, const QVariant &value);
    void remove(const QString &group, const QString &key);

private:
    [[nodiscard]] static QString storageKey(const QString &group, const QString &key);

    mutable QHash<QString, QVariant> m_values;
};

// 控制器持有的设置存储：write/remove 总是先更新内存缓存，再尝试后端持久化；
// read 优先后端，后端不可用（nullopt）时回退内存缓存。
class AppSettingsStorage {
public:
    void setBackend(AppSettingsBackend backend);

    [[nodiscard]] QVariant read(const QString &group, const QString &key, const QVariant &defaultValue) const;
    void write(const QString &group, const QString &key, const QVariant &value);
    void remove(const QString &group, const QString &key);

private:
    AppSettingsBackend m_backend;
    AppSettingsMemoryStore m_memory;
};

}
