#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#if SERIONA_HAS_BACKEND
#include "seriona/app/application_logging.h"
#include "seriona/app/runtime_paths.h"

#include <QtLogging>
#include <spdlog/spdlog.h>

extern "C" {
#include <libavutil/log.h>
}
#endif

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

#if SERIONA_HAS_BACKEND
void qtMessageToSpdlog(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    Q_UNUSED(context);
    // 终端日志统一由 spdlog 负责：Qt 消息（qDebug/qWarning/QML console）重定向到
    // 默认 logger。不传 source_loc：QML 场景 context.line 可为 -1、function 可为
    // nullptr（spdlog#3073），且 seriona 的 pattern 只用 %v。
    if (spdlog::default_logger() == nullptr) {
        return;
    }
    switch (type) {
    case QtDebugMsg:
        spdlog::debug("{}", message.toStdString());
        break;
    case QtInfoMsg:
        spdlog::info("{}", message.toStdString());
        break;
    case QtWarningMsg:
        spdlog::warn("{}", message.toStdString());
        break;
    case QtCriticalMsg:
        spdlog::error("{}", message.toStdString());
        break;
    case QtFatalMsg:
        spdlog::critical("{}", message.toStdString());
        spdlog::shutdown(); // Qt 在 handler 返回后立即 abort，先 flush 再退出
        break;
    }
}
#endif

struct SmokeOptions {
    bool enabled = false;
    int exitMs = 1000;
    QString scenario = QStringLiteral("startup");
    QString outputDir = QStringLiteral(".omo/evidence/smoke");
};

bool isKnownSmokeScenario(const QString &scenario)
{
    return scenario == QStringLiteral("main-playback") || scenario == QStringLiteral("lyrics")
           || scenario == QStringLiteral("sidebar-tree") || scenario == QStringLiteral("startup")
           || scenario == QStringLiteral("settings-menu") || scenario == QStringLiteral("empty-library");
}

bool parseSmokeOptions(const QStringList &arguments, SmokeOptions *options, QString *error)
{
    const QString exitPrefix = QStringLiteral("--smoke-exit-ms=");
    const QString scenarioPrefix = QStringLiteral("--smoke-scenario=");
    const QString outputDirPrefix = QStringLiteral("--smoke-output-dir=");

    for (const QString &argument : arguments.mid(1)) {
        if (argument.startsWith(exitPrefix)) {
            bool ok = false;
            const int exitMs = argument.mid(exitPrefix.size()).toInt(&ok);
            if (!ok || exitMs < 0) {
                *error = QStringLiteral("Invalid --smoke-exit-ms value: %1").arg(argument.mid(exitPrefix.size()));
                return false;
            }
            options->enabled = true;
            options->exitMs = exitMs;
        } else if (argument.startsWith(scenarioPrefix)) {
            options->enabled = true;
            options->scenario = argument.mid(scenarioPrefix.size());
        } else if (argument.startsWith(outputDirPrefix)) {
            options->enabled = true;
            options->outputDir = argument.mid(outputDirPrefix.size());
        } else if (argument.startsWith(QStringLiteral("--smoke-"))) {
            *error = QStringLiteral("Unknown smoke option: %1").arg(argument);
            return false;
        }
    }

    if (options->enabled && options->outputDir.isEmpty()) {
        *error = QStringLiteral("--smoke-output-dir cannot be empty");
        return false;
    }

    return true;
}

bool writeSmokeLog(const SmokeOptions &options, QString *error)
{
    std::error_code filesystemError;
    const std::filesystem::path outputDir(options.outputDir.toStdString());
    std::filesystem::create_directories(outputDir, filesystemError);
    if (filesystemError) {
        *error = QStringLiteral("Failed to create smoke output directory %1: %2")
                     .arg(options.outputDir, QString::fromStdString(filesystemError.message()));
        return false;
    }

    const std::filesystem::path logPath = outputDir / (QStringLiteral("smoke-%1.log").arg(options.scenario).toStdString());
    std::ofstream log(logPath, std::ios::trunc);
    if (!log.is_open()) {
        *error = QStringLiteral("Failed to open smoke log file: %1").arg(QString::fromStdString(logPath.string()));
        return false;
    }

    log << "scenario=" << options.scenario.toStdString() << '\n';
    log << "exit_ms=" << options.exitMs << '\n';
    log << "timestamp_utc=" << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString() << '\n';
    log << "artifact=log-only smoke harness; screenshot skipped because T5 only establishes CLI verification.\n";

    return true;
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#if SERIONA_HAS_BACKEND && !defined(NDEBUG)
    av_log_set_level(AV_LOG_WARNING);
#elif SERIONA_HAS_BACKEND
    av_log_set_level(AV_LOG_QUIET);
#endif

#if SERIONA_HAS_BACKEND
    try {
        const auto runtimePaths = seriona::app::resolveRuntimePaths(
            QCoreApplication::applicationFilePath().toStdString());
        seriona::app::initializeApplicationLogging(runtimePaths);
        // 错误处理器设为不抛：sink 失败（如磁盘满）时异常若从 Qt 消息 handler
        // 逃逸会直接 terminate 应用；静默丢弃单条日志优于进程崩溃。
        spdlog::set_error_handler([](const std::string &) {});
        qInstallMessageHandler(qtMessageToSpdlog);
    } catch (const std::exception &error) {
        std::cerr << "seriona: logging initialization failed: " << error.what() << '\n';
    }
#endif

    SmokeOptions smokeOptions;
    QString smokeError;
    if (!parseSmokeOptions(app.arguments(), &smokeOptions, &smokeError)) {
        std::cerr << smokeError.toStdString() << '\n';
        return 2;
    }

    if (smokeOptions.enabled) {
        if (!isKnownSmokeScenario(smokeOptions.scenario)) {
            std::cerr << "Unknown smoke scenario: " << smokeOptions.scenario.toStdString() << '\n';
            return 2;
        }

        app.setProperty("seriona.backendBridgeAutostartEnabled", false);

        // smoke 进程不接入后端（backendBridgeAutostartEnabled=false），三个控制器的
        // 应用设置自动回退内存存储，天然与用户真实设置隔离，不会持久化到磁盘。
        if (!writeSmokeLog(smokeOptions, &smokeError)) {
            std::cerr << smokeError.toStdString() << '\n';
            return 3;
        }

        QTimer::singleShot(smokeOptions.exitMs, &app, []() { QCoreApplication::quit(); });
    }

    QQmlApplicationEngine engine;
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("smokeScenario"), smokeOptions.enabled ? smokeOptions.scenario : QString{});
    // NDEBUG(Release)下恒为 false:smoke 调试日志不泄漏到正式构建。
#if defined(NDEBUG)
    const bool smokeLoggingEnabled = false;
#else
    const bool smokeLoggingEnabled = smokeOptions.enabled;
#endif
    initialProperties.insert(QStringLiteral("smokeLoggingEnabled"), smokeLoggingEnabled);
    engine.setInitialProperties(initialProperties);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []()
                     { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Seriona", "Main");

    return QCoreApplication::exec();
}
