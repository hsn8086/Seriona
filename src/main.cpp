#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QQmlApplicationEngine>
#include <QStringList>
#include <QTimer>

#if SERIONA_HAS_BACKEND
#include "seriona/app/application_logging.h"
#include "seriona/app/runtime_paths.h"

extern "C" {
#include <libavutil/log.h>
}
#endif

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
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

        app.setProperty("seriona.smokeScenario", smokeOptions.scenario);
        app.setProperty("seriona.backendBridgeAutostartEnabled", false);

        if (!writeSmokeLog(smokeOptions, &smokeError)) {
            std::cerr << smokeError.toStdString() << '\n';
            return 3;
        }

        QTimer::singleShot(smokeOptions.exitMs, &app, []() { QCoreApplication::quit(); });
    }

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []()
                     { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Seriona", "Main");

    return QCoreApplication::exec();
}
