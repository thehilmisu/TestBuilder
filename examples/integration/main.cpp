// ---------------------------------------------------------------------------
// Running a scenario from another application, with no editor and no GUI.
//
//     ./scenariorun path/to/scenario.tbscn
//
// Exit code is the verdict: 0 pass, 1 fail, 2 aborted, 3 error -- so this drops
// straight into a CI job as-is.
//
// The same four lines work inside a widget application; only the event loop and
// the reporting differ.
// ---------------------------------------------------------------------------
#include "appbackend.h"

#include "runtime/scenariomodel.h"
#include "runtime/scenariorunner.h"

#include <QCoreApplication>
#include <QTextStream>

using testbuilder::ScenarioRunner;

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

const char *verdictName(ScenarioRunner::Verdict verdict)
{
    switch (verdict) {
    case ScenarioRunner::Verdict::Pass:  return "PASS";
    case ScenarioRunner::Verdict::Fail:  return "FAIL";
    case ScenarioRunner::Verdict::Abort: return "ABORTED";
    case ScenarioRunner::Verdict::Error: return "ERROR";
    case ScenarioRunner::Verdict::None:  break;
    }
    return "NONE";
}

const char *levelName(ScenarioRunner::LogLevel level)
{
    switch (level) {
    case ScenarioRunner::LogLevel::Warning: return "WARN";
    case ScenarioRunner::LogLevel::Error:   return "FAIL";
    case ScenarioRunner::LogLevel::Info:    break;
    }
    return "INFO";
}

int exitCode(ScenarioRunner::Verdict verdict)
{
    switch (verdict) {
    case ScenarioRunner::Verdict::Pass:  return 0;
    case ScenarioRunner::Verdict::Fail:  return 1;
    case ScenarioRunner::Verdict::Abort: return 2;
    default: break;
    }
    return 3;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        out() << "usage: scenariorun <scenario.tbscn>\n";
        return 3;
    }

    // 1. Load the scenario the editor exported.
    QStringList problems;
    const testbuilder::ScenarioModel model =
        testbuilder::ScenarioModel::fromFile(QString::fromLocal8Bit(argv[1]), &problems);
    for (const QString &problem : problems)
        out() << "  load: " << problem << "\n";

    // 2. Point the engine at your application.
    AppBus bus;
    auto *backend = new AppBackend(&bus, &app);

    // --- or, without subclassing anything -----------------------------------
    //
    //   auto *backend = new testbuilder::CallbackBackend(&app);
    //   backend->onWriteValue = [&](const QString &n, const QVariant &v, QString *e) {
    //       return bus.setSignal(n, v);
    //   };
    //   backend->onReadValue = [&](const QString &n, QVariant *v, QString *e) {
    //       return bus.getSignal(n, v);
    //   };
    //   backend->onRequestValue = [&](const QString &n, QString *e) {
    //       return bus.requestParameter(n);
    //   };
    //   backend->onSendMessage = [&](int id, const QByteArray &d, QString *e) {
    //       return bus.transmit(id, d);
    //   };
    //   connect(&bus, &AppBus::frameReceived, backend, &CallbackBackend::deliverMessage);
    //   connect(&bus, &AppBus::parameterDecoded, backend, &CallbackBackend::deliverValue);

    ScenarioRunner runner;
    runner.setBackend(backend);
    runner.setStepDelayMs(0); // no pacing when nobody is watching

    // 3. Report as it goes.
    QObject::connect(&runner, &ScenarioRunner::logged,
                     [](const ScenarioRunner::LogEntry &entry) {
                         out() << QStringLiteral("%1 %2  %3\n")
                                      .arg(entry.elapsedMs, 6)
                                      .arg(QLatin1String(levelName(entry.level)), -4)
                                      .arg(entry.text);
                         out().flush();
                     });

    QObject::connect(&runner, &ScenarioRunner::finished,
                     [&app](ScenarioRunner::Verdict verdict, const QString &reason) {
                         out() << "\n" << verdictName(verdict) << " - " << reason << "\n";
                         out().flush();
                         app.exit(exitCode(verdict));
                     });

    // 4. Go. load() returns false only when the graph cannot run at all;
    //    anything else it reports is a warning worth printing.
    QStringList errors;
    if (!runner.load(model, &errors)) {
        for (const QString &error : errors)
            out() << "  " << error << "\n";
        return 3;
    }
    for (const QString &error : errors)
        out() << "  warning: " << error << "\n";

    runner.start();
    return app.exec();
}
