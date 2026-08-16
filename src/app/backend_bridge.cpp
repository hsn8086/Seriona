#include "backend_bridge.h"

#if SERIONA_HAS_BACKEND
#include "backend_command_adapter.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPair>
#include <QList>
#include <QPointer>
#include <QVariantMap>

#include "seriona/app/runtime_paths.h"

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#endif

namespace Seriona::App {

namespace {

#if SERIONA_HAS_BACKEND
constexpr qsizetype kMaxQueuedNotifications = 64;

std::string toBackendString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

std::filesystem::path toBackendPath(const QString &path)
{
    return std::filesystem::path(toBackendString(path));
}

QString normalizedRootPath(const QString &rootPath)
{
    const QString trimmed = rootPath.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());
}

seriona::control::MediaControllerCommandResult invalidCommandResult(std::string message)
{
    seriona::control::MediaControllerCommandResult result;
    result.accepted = false;
    result.code = seriona::control::MediaControllerErrorCode::InvalidCommand;
    result.message = std::move(message);
    return result;
}

std::optional<seriona::control::FolderSortField> sortFieldFromPayload(const QString &field)
{
    const QString trimmed = field.trimmed();
    if (trimmed.compare(QStringLiteral("title"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortField::Title;
    }
    if (trimmed.compare(QStringLiteral("artist"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortField::Artist;
    }
    if (trimmed.compare(QStringLiteral("album"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortField::Album;
    }
    if (trimmed.compare(QStringLiteral("filename"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortField::Filename;
    }
    if (trimmed.compare(QStringLiteral("year"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortField::Year;
    }
    if (trimmed.compare(QStringLiteral("duration"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortField::Duration;
    }
    if (trimmed.compare(QStringLiteral("createdDate"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortField::CreatedDate;
    }
    if (trimmed.compare(QStringLiteral("discNumber"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortField::DiscNumber;
    }
    if (trimmed.compare(QStringLiteral("trackNumber"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortField::TrackNumber;
    }
    return std::nullopt;
}

std::optional<seriona::control::FolderSortDirection> sortDirectionFromPayload(const QString &order)
{
    const QString trimmed = order.trimmed();
    if (trimmed.compare(QStringLiteral("asc"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortDirection::Ascending;
    }
    if (trimmed.compare(QStringLiteral("desc"), Qt::CaseInsensitive) == 0) {
        return seriona::control::FolderSortDirection::Descending;
    }
    return std::nullopt;
}

std::optional<seriona::control::FolderSortRule> sortRuleFromPayload(const QVariant &ruleVar, std::string &error)
{
    if (!ruleVar.canConvert<QVariantMap>()) {
        error = "Malformed sort rule payload";
        return std::nullopt;
    }

    const QVariantMap ruleMap = ruleVar.toMap();
    const std::optional<seriona::control::FolderSortField> field = sortFieldFromPayload(
        ruleMap.value(QStringLiteral("field")).toString());
    if (!field.has_value()) {
        error = "Invalid sort field";
        return std::nullopt;
    }

    const std::optional<seriona::control::FolderSortDirection> direction = sortDirectionFromPayload(
        ruleMap.value(QStringLiteral("order")).toString());
    if (!direction.has_value()) {
        error = "Invalid sort direction";
        return std::nullopt;
    }

    seriona::control::FolderSortRule rule;
    rule.field = *field;
    rule.direction = *direction;
    rule.missingValuePolicy = seriona::control::FolderSortMissingValuePolicy::Last;
    return rule;
}

std::optional<std::vector<seriona::control::FolderSortRule>> sortRulesFromPayload(const QVariantList &rules, std::string &error)
{
    std::vector<seriona::control::FolderSortRule> parsedRules;
    parsedRules.reserve(static_cast<std::size_t>(rules.size()));
    for (const QVariant &ruleVar : rules) {
        std::optional<seriona::control::FolderSortRule> parsedRule = sortRuleFromPayload(ruleVar, error);
        if (!parsedRule.has_value()) {
            return std::nullopt;
        }
        parsedRules.push_back(*parsedRule);
    }
    return parsedRules;
}
#endif

}

#if SERIONA_HAS_BACKEND
BackendBridge::BackendBridge(ControllerFactory controllerFactory, QObject *parent)
    : QObject(parent)
    , m_controllerFactory(std::move(controllerFactory))
{
}
#endif

BackendBridge::BackendBridge(QObject *parent)
    : QObject(parent)
#if SERIONA_HAS_BACKEND
    , m_controllerFactory(defaultControllerFactory())
#endif
{
}

BackendBridge::~BackendBridge()
{
    shutdown();
}

void BackendBridge::start()
{
    if (m_started) {
        return;
    }

    m_shuttingDown = false;
#if SERIONA_HAS_BACKEND
    try {
        if (!m_controller) {
            m_controller = m_controllerFactory();
        }
        if (!m_controller) {
            return;
        }

        registerSubscriptions();
        m_controller->start();
    } catch (...) {
        m_shuttingDown = true;
        unsubscribeAll();
        if (m_controller) {
            m_controller->shutdown();
            m_controller.reset();
        }
        return;
    }
#endif
    m_started = true;
    emit startedChanged();
}

void BackendBridge::shutdown()
{
#if SERIONA_HAS_BACKEND
    const bool hasController = static_cast<bool>(m_controller);
#else
    const bool hasController = false;
#endif
    if (!m_started && !hasController) {
        return;
    }

#if SERIONA_HAS_BACKEND
    if (m_started) {
        submitShutdownStop();
    }
#endif
    m_shuttingDown = true;
#if SERIONA_HAS_BACKEND
    unsubscribeAll();
    if (m_controller) {
        m_controller->shutdown();
        m_controller.reset();
    }
#endif
    if (m_started) {
        m_started = false;
        emit startedChanged();
    }
    emit shutdownCompleted();
}

bool BackendBridge::started() const
{
    return m_started;
}

bool BackendBridge::shuttingDown() const
{
    return m_shuttingDown;
}

void BackendBridge::drainForTests()
{
#if SERIONA_HAS_BACKEND
    if (m_controller) {
        m_controller->drainForTests();
    }
#endif
}

#if SERIONA_HAS_BACKEND
seriona::control::MediaControllerCommandResult BackendBridge::submitCommand(const seriona::control::MediaControlCommand &command)
{
    if (m_shuttingDown || !m_controller) {
        seriona::control::MediaControllerCommandResult result = controllerStoppedResult();
        enqueueCommandFailureNotification(result);
        return result;
    }

    seriona::control::MediaControllerCommandResult result = m_controller->submitCommand(command);
    enqueueCommandFailureNotification(result);
    return result;
}

seriona::control::MediaControllerCommandResult BackendBridge::scanLibrary(const QString &rootPath, seriona::scanner::ScanMode mode)
{
    if (m_shuttingDown || !m_controller) {
        seriona::control::MediaControllerCommandResult result = controllerStoppedResult();
        enqueueCommandFailureNotification(result);
        return result;
    }

    const QByteArray utf8Path = rootPath.toUtf8();
    seriona::scanner::ScannerRoot root;
    root.path = std::filesystem::path(std::string(utf8Path.constData(), static_cast<std::size_t>(utf8Path.size())));
    root.recursive = true;
    seriona::control::MediaControllerCommandResult result = m_controller->scanLibrary({std::move(root)}, mode);
    enqueueCommandFailureNotification(result);
    return result;
}

seriona::control::MediaControllerCommandResult BackendBridge::applyFolderSortRules(
    const QString &rootPath,
    const QString &folderNodeId,
    const QVariantList &rules)
{
    const QString normalizedRoot = normalizedRootPath(rootPath);
    if (normalizedRoot.isEmpty()) {
        seriona::control::MediaControllerCommandResult result = invalidCommandResult("Folder sort command requires a root path");
        enqueueCommandFailureNotification(result);
        return result;
    }

    const QString normalizedFolderNodeId = folderNodeId.trimmed();
    if (normalizedFolderNodeId.isEmpty()) {
        seriona::control::MediaControllerCommandResult result = invalidCommandResult("Folder sort command requires a folder node id");
        enqueueCommandFailureNotification(result);
        return result;
    }

    std::string error;
    std::optional<std::vector<seriona::control::FolderSortRule>> parsedRules = sortRulesFromPayload(rules, error);
    if (!parsedRules.has_value()) {
        seriona::control::MediaControllerCommandResult result = invalidCommandResult(std::move(error));
        enqueueCommandFailureNotification(result);
        return result;
    }

    seriona::control::FolderSortSetting setting;
    setting.rootPath = toBackendPath(normalizedRoot);
    setting.folderNodeId = toBackendString(normalizedFolderNodeId);
    setting.rules = std::move(*parsedRules);

    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::ApplyFolderSortRules;
    command.folderSortSetting = std::move(setting);
    return submitCommand(command);
}

seriona::control::MediaControllerCommandResult BackendBridge::submitConfigureOutput(
    int outputMode,
    int sampleRate,
    int sampleFormat,
    int bufferDurationMs,
    const QString &preferredDeviceId)
{
    if (outputMode != 0 && outputMode != 1) {
        seriona::control::MediaControllerCommandResult result = invalidCommandResult(
            "ConfigureOutput requires outputMode 0 (Direct) or 1 (Mixed)");
        enqueueCommandFailureNotification(result);
        return result;
    }
    if (sampleRate != 0 && (sampleRate < 8000 || sampleRate > 768000)) {
        seriona::control::MediaControllerCommandResult result = invalidCommandResult(
            "ConfigureOutput sample rate is out of range (8000-768000)");
        enqueueCommandFailureNotification(result);
        return result;
    }
    if (sampleFormat != 0 && sampleFormat != 1 && sampleFormat != 2 && sampleFormat != 4) {
        seriona::control::MediaControllerCommandResult result = invalidCommandResult(
            "ConfigureOutput sample format must be 0 (device default), 1 (Int16), 2 (Int24), or 4 (Float32)");
        enqueueCommandFailureNotification(result);
        return result;
    }
    if (bufferDurationMs < 50 || bufferDurationMs > 1000) {
        seriona::control::MediaControllerCommandResult result = invalidCommandResult(
            "ConfigureOutput buffer duration is out of range (50-1000 ms)");
        enqueueCommandFailureNotification(result);
        return result;
    }

    seriona::audio::AudioOutputConfig config;
    config.outputMode = (outputMode == 0) ? seriona::audio::AudioOutputMode::Direct
                                          : seriona::audio::AudioOutputMode::Mixed;
    if (sampleRate > 0) {
        config.targetSampleRate = static_cast<std::uint32_t>(sampleRate);
    }
    if (sampleFormat > 0) {
        config.targetSampleFormat = static_cast<seriona::audio::AudioSampleFormat>(sampleFormat);
    }
    config.bufferDuration = std::chrono::milliseconds(bufferDurationMs);
    config.preferredDeviceId = toBackendString(preferredDeviceId);

    seriona::control::MediaControlCommand command;
    command.kind = seriona::control::MediaControlCommandKind::ConfigureOutput;
    command.outputConfig = std::move(config);
    return submitCommand(command);
}

QList<QPair<QString, QString>> BackendBridge::enumeratePlaybackDevices()
{
    if (m_shuttingDown || !m_controller) {
        return {};
    }

    const std::vector<seriona::audio::AudioDeviceFormat> devices = m_controller->enumeratePlaybackDevices();
    QList<QPair<QString, QString>> devicePairs;
    devicePairs.reserve(static_cast<qsizetype>(devices.size()));
    for (const seriona::audio::AudioDeviceFormat &device : devices) {
        const QString deviceId = QString::fromStdString(device.deviceId);
        if (deviceId.isEmpty()) {
            continue;
        }
        QString deviceName = QString::fromStdString(device.deviceName);
        if (deviceName.isEmpty()) {
            deviceName = deviceId;
        }
        devicePairs.append({deviceId, deviceName});
    }
    return devicePairs;
}

const seriona::control::PlayerStateSnapshot &BackendBridge::playerSnapshot() const
{
    return m_playerSnapshot;
}

const seriona::control::LibraryStateSnapshot &BackendBridge::librarySnapshot() const
{
    return m_librarySnapshot;
}

const std::deque<seriona::control::ControlDomainNotification> &BackendBridge::notifications() const
{
    return m_notifications;
}

BackendBridge::ControllerFactory BackendBridge::defaultControllerFactory()
{
    return [] {
#if SERIONA_HAS_BACKEND
        const auto exePath = QCoreApplication::applicationFilePath().toStdString();
        const auto runtimePaths = seriona::app::resolveRuntimePaths(exePath);
        runtimePaths.ensureDirectoriesExist();
        const auto &[dataRoot, logFile, mediaStorePath, artworkDir] = runtimePaths;
        static_cast<void>(dataRoot);
        static_cast<void>(logFile);
        return seriona::control::makeProductionMediaController(
            seriona::control::MediaControllerOptions{},
            mediaStorePath,
            artworkDir);
#else
        return nullptr;
#endif
    };
}

seriona::control::MediaControllerCommandResult BackendBridge::controllerStoppedResult()
{
    seriona::control::MediaControllerCommandResult result;
    result.accepted = false;
    result.code = seriona::control::MediaControllerErrorCode::ControllerStopped;
    result.message = "Media controller is stopped";
    return result;
}

void BackendBridge::unsubscribe(seriona::control::SubscriptionHandle &handle)
{
    if (handle.unsubscribe) {
        handle.unsubscribe();
        handle = {};
    }
}

void BackendBridge::registerSubscriptions()
{
    const QPointer<BackendBridge> receiver(this);
    m_playerSubscription = m_controller->subscribePlayerState([receiver](seriona::control::PlayerStateSnapshot snapshot) mutable {
        if (receiver.isNull()) {
            return;
        }

        QMetaObject::invokeMethod(receiver.data(), [receiver, snapshot = std::move(snapshot)]() mutable {
            if (receiver.isNull() || receiver->m_shuttingDown) {
                return;
            }
            receiver->applyPlayerSnapshot(std::move(snapshot));
        }, Qt::QueuedConnection);
    });
    m_librarySubscription = m_controller->subscribeLibraryState([receiver](seriona::control::LibraryStateSnapshot snapshot) mutable {
        if (receiver.isNull()) {
            return;
        }

        QMetaObject::invokeMethod(receiver.data(), [receiver, snapshot = std::move(snapshot)]() mutable {
            if (receiver.isNull() || receiver->m_shuttingDown) {
                return;
            }
            receiver->applyLibrarySnapshot(std::move(snapshot));
        }, Qt::QueuedConnection);
    });
    m_notificationSubscription = m_controller->subscribeDomainNotifications([receiver](seriona::control::ControlDomainNotification notification) mutable {
        if (receiver.isNull()) {
            return;
        }

        QMetaObject::invokeMethod(receiver.data(), [receiver, notification = std::move(notification)]() mutable {
            if (receiver.isNull() || receiver->m_shuttingDown) {
                return;
            }
            receiver->enqueueNotification(std::move(notification));
        }, Qt::QueuedConnection);
    });
}

void BackendBridge::unsubscribeAll()
{
    unsubscribe(m_playerSubscription);
    unsubscribe(m_librarySubscription);
    unsubscribe(m_notificationSubscription);
}

void BackendBridge::submitShutdownStop()
{
    if (!m_controller) {
        return;
    }

    seriona::control::MediaControlCommand stopCommand;
    stopCommand.kind = seriona::control::MediaControlCommandKind::Stop;
    static_cast<void>(m_controller->submitCommand(stopCommand));
}

void BackendBridge::applyPlayerSnapshot(seriona::control::PlayerStateSnapshot snapshot)
{
    m_playerSnapshot = std::move(snapshot);
    emit playerSnapshotChanged();
}

void BackendBridge::applyLibrarySnapshot(seriona::control::LibraryStateSnapshot snapshot)
{
    m_librarySnapshot = std::move(snapshot);
    emit librarySnapshotChanged();
}

void BackendBridge::enqueueCommandFailureNotification(const seriona::control::MediaControllerCommandResult &result)
{
    std::optional<seriona::control::ControlDomainNotification> notification = notificationFromRejectedCommandResult(result);
    if (!notification.has_value()) {
        return;
    }
    enqueueNotification(std::move(*notification));
}

void BackendBridge::enqueueNotification(seriona::control::ControlDomainNotification notification)
{
    m_notifications.push_back(std::move(notification));
    while (m_notifications.size() > kMaxQueuedNotifications) {
        m_notifications.pop_front();
    }
    emit domainNotificationQueued();
}
#endif

}
