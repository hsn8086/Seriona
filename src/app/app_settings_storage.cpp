#include "app_settings_storage.h"

namespace Seriona::App {

QString AppSettingsMemoryStore::storageKey(const QString &group, const QString &key)
{
    return group + QLatin1Char('\x1f') + key;
}

QVariant AppSettingsMemoryStore::read(const QString &group, const QString &key, const QVariant &defaultValue) const
{
    return m_values.value(storageKey(group, key), defaultValue);
}

void AppSettingsMemoryStore::write(const QString &group, const QString &key, const QVariant &value)
{
    m_values.insert(storageKey(group, key), value);
}

void AppSettingsMemoryStore::remove(const QString &group, const QString &key)
{
    m_values.remove(storageKey(group, key));
}

void AppSettingsStorage::setBackend(AppSettingsBackend backend)
{
    m_backend = std::move(backend);
}

QVariant AppSettingsStorage::read(const QString &group, const QString &key, const QVariant &defaultValue) const
{
    if (m_backend.read) {
        const std::optional<QVariant> stored = m_backend.read(group, key, defaultValue);
        if (stored.has_value()) {
            return *stored;
        }
    }
    return m_memory.read(group, key, defaultValue);
}

void AppSettingsStorage::write(const QString &group, const QString &key, const QVariant &value)
{
    m_memory.write(group, key, value);
    if (m_backend.write) {
        m_backend.write(group, key, value);
    }
}

void AppSettingsStorage::remove(const QString &group, const QString &key)
{
    m_memory.remove(group, key);
    if (m_backend.remove) {
        m_backend.remove(group, key);
    }
}

}
