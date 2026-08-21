#ifndef RUNTIME_SCENARIORUNNER_H
#define RUNTIME_SCENARIORUNNER_H

#include "lintransport.h"
#include "scenariomodel.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVector>

#include <functional>

class QTimer;

namespace runtime {

// ---------------------------------------------------------------------------
// Executes a ScenarioModel as a state machine.
//
// The runner never blocks. Every step either resolves immediately and hands
// back the output port to leave by, or parks the machine on an event -- a
// timer, a slave response, an incoming frame -- and the machine resumes when
// that event (or its timeout) arrives. That is what keeps the UI responsive
// during a `Wait 5000 ms` and what makes pause/stop possible mid-scenario.
//
// Block behaviour lives in handlers keyed by BlockType::id, so teaching the
// runner a new block is registerHandler() plus the entry in blocktypes.cpp --
// there is no switch to extend.
// ---------------------------------------------------------------------------
class ScenarioRunner : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Running, Waiting, Paused, Finished };
    enum class Verdict { None, Pass, Fail, Abort, Error };
    enum class LogLevel { Info, Warning, Error };
    Q_ENUM(State)
    Q_ENUM(Verdict)
    Q_ENUM(LogLevel)

    struct LogEntry
    {
        qint64 elapsedMs = 0;
        LogLevel level = LogLevel::Info;
        quint64 nodeId = 0;
        QString text;
    };

    // What a block handler tells the machine to do next.
    struct Outcome
    {
        enum class Kind {
            Follow, // leave through `port` now
            Await,  // the handler armed an event; the machine parks until it fires
            Halt,   // the scenario is over, with `verdict`
            Fault   // the step could not be carried out (bad params, bus error)
        };

        Kind kind = Kind::Follow;
        QString port;
        Verdict verdict = Verdict::None;
        QString message;

        static Outcome follow(const QString &port = QStringLiteral("out"));
        static Outcome await();
        static Outcome halt(Verdict verdict, const QString &message = QString());
        static Outcome fault(const QString &message);
    };

    using Handler = std::function<Outcome(ScenarioRunner *, const ScenarioNode &)>;

    explicit ScenarioRunner(QObject *parent = nullptr);
    ~ScenarioRunner() override;

    // The runner does not take ownership; the transport must outlive it.
    void setTransport(LinTransport *transport);
    LinTransport *transport() const { return m_transport; }

    // Replaces the loaded scenario. Returns false (and fills `errors`) when the
    // graph cannot run at all -- a missing Start block, say.
    bool load(const ScenarioModel &model, QStringList *errors = nullptr);
    const ScenarioModel &model() const { return m_model; }

    State state() const { return m_state; }
    bool isRunning() const { return m_state == State::Running || m_state == State::Waiting; }
    Verdict verdict() const { return m_verdict; }
    qint64 elapsedMs() const;
    const QVector<LogEntry> &log() const { return m_log; }

    // Pause between steps, purely so a run is watchable on the canvas. 0 runs
    // the scenario as fast as the event loop allows.
    void setStepDelayMs(int ms) { m_stepDelayMs = qMax(0, ms); }
    int stepDelayMs() const { return m_stepDelayMs; }

    // How long an await waits before it gives up. Blocks with their own timeout
    // parameter use that instead.
    void setDefaultTimeoutMs(int ms) { m_defaultTimeoutMs = qMax(1, ms); }

    // Runaway guard: a scenario whose Repeat loops back forever would otherwise
    // spin until the process is killed.
    void setMaxSteps(int steps) { m_maxSteps = qMax(1, steps); }

    // Overrides the built-in behaviour for a block type, or teaches the runner
    // one it does not know.
    void registerHandler(const QString &typeId, Handler handler);

    // --- helpers for handlers ----------------------------------------------
    void logMessage(LogLevel level, const QString &text, quint64 nodeId = 0);

    // Values picked up during the run -- decoded diagnostic responses, mostly --
    // which `expect_signal` consults before falling back to the bus.
    void setContextValue(const QString &key, const QVariant &value) { m_context[key] = value; }
    QVariant contextValue(const QString &key) const { return m_context.value(key); }
    bool hasContextValue(const QString &key) const { return m_context.contains(key); }

    // Park the machine. Each returns an Outcome::Await for the handler to return.
    Outcome awaitTimer(int milliseconds, const QString &resumePort = QStringLiteral("out"));
    Outcome awaitSlaveResponse(const QString &parameter, int timeoutMs,
                               const QString &resumePort = QStringLiteral("out"),
                               const QString &timeoutPort = QString());
    Outcome awaitFrame(int frameId, const QByteArray &expectedData, int timeoutMs,
                       const QString &resumePort = QStringLiteral("out"),
                       const QString &timeoutPort = QString());

public slots:
    void start();
    void stop(); // ends the run with an Abort verdict
    void pause();
    void resume();

signals:
    void started();
    void stateChanged(runtime::ScenarioRunner::State state);
    void nodeEntered(quint64 nodeId);
    void nodeLeft(quint64 nodeId, const QString &port);
    void logged(const runtime::ScenarioRunner::LogEntry &entry);
    void finished(runtime::ScenarioRunner::Verdict verdict, const QString &reason);

private slots:
    void onSlaveResponse(const runtime::DiagResponse &response);
    void onFrameReceived(const runtime::LinFrame &frame);
    void onTransportError(const QString &message);

private:
    // What the machine is currently parked on.
    struct Await
    {
        enum class What { Nothing, Timer, SlaveResponse, Frame };

        What what = What::Nothing;
        QString parameter;
        int frameId = -1;         // -1 matches any frame
        QByteArray expectedData;  // empty matches any payload
        quint64 nodeId = 0;
        QString resumePort;
        QString timeoutPort; // empty: a timeout fails the scenario
        int generation = 0;
    };

    void installDefaultHandlers();
    Outcome enterRepeat(const ScenarioNode &node);
    void handleAwaitTimeout();
    void setState(State state);
    void scheduleStep();
    void executeStep();
    void applyOutcome(const ScenarioNode &node, const Outcome &outcome);
    void leaveVia(const ScenarioNode &node, const QString &port);
    void resumeFromAwait(const QString &port);
    void clearAwait();
    void finish(Verdict verdict, const QString &reason);

    ScenarioModel m_model;
    LinTransport *m_transport = nullptr;
    QHash<QString, Handler> m_handlers;

    State m_state = State::Idle;
    Verdict m_verdict = Verdict::None;
    quint64 m_current = 0;
    int m_stepCount = 0;
    int m_maxSteps = 100000;
    int m_stepDelayMs = 120;
    int m_defaultTimeoutMs = 1000;

    Await m_await;
    bool m_pauseRequested = false;
    QTimer *m_stepTimer = nullptr;
    QElapsedTimer m_clock;

    // Repeat blocks, innermost last. A Repeat is re-entered by its own body
    // branch, so the frame on top tells it whether it is starting or looping.
    struct LoopFrame
    {
        quint64 nodeId = 0;
        int remaining = 0;
    };
    QVector<LoopFrame> m_loops;

    QHash<QString, QVariant> m_context;
    QVector<LogEntry> m_log;
};

} // namespace runtime

Q_DECLARE_METATYPE(runtime::ScenarioRunner::LogEntry)

#endif // RUNTIME_SCENARIORUNNER_H
