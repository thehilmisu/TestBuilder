#include "scenariorunner.h"

#include "nodeeditor/blocktypes.h"

#include <QTimer>

#include <cmath>

namespace testbuilder {

namespace {

// Block params are stored as text so the property panel can stay a plain line
// edit. Anything that looks like a number is passed on as one.
QVariant numericOrText(const QVariant &value)
{
    if (value.typeId() == QMetaType::Int || value.typeId() == QMetaType::Double)
        return value;

    const QString text = value.toString().trimmed();
    bool ok = false;
    const double number = text.toDouble(&ok);
    if (!ok)
        return text;
    // Keep whole numbers as ints so they format as "1200", not "1200.0".
    if (std::floor(number) == number && std::abs(number) < 1e15)
        return QVariant(qint64(number));
    return QVariant(number);
}

// "01 02 FF", "0102ff" and "01:02:FF" all parse. Returns false on odd digits or
// non-hex characters, so a typo is reported instead of silently truncated.
bool parseHexPayload(const QString &text, QByteArray *out)
{
    QString cleaned = text;
    cleaned.remove(QLatin1Char(' '));
    cleaned.remove(QLatin1Char(':'));
    cleaned.remove(QLatin1Char('-'));
    cleaned.remove(QLatin1Char(','));

    if (cleaned.isEmpty()) {
        *out = QByteArray();
        return true;
    }
    if (cleaned.size() % 2 != 0)
        return false;
    for (const QChar c : cleaned) {
        if (!isxdigit(c.toLatin1()))
            return false;
    }
    *out = QByteArray::fromHex(cleaned.toLatin1());
    return true;
}

QString formatValue(const QVariant &value)
{
    return value.isValid() ? value.toString() : QStringLiteral("<none>");
}

QString formatId(int id)
{
    return QStringLiteral("0x%1").arg(id, 2, 16, QLatin1Char('0'));
}

// Applies `op` with a symmetric tolerance band around the expected value.
bool compareNumbers(double actual, const QString &op, double expected, double tolerance)
{
    if (op == QLatin1String("=="))
        return std::abs(actual - expected) <= tolerance;
    if (op == QLatin1String("!="))
        return std::abs(actual - expected) > tolerance;
    if (op == QLatin1String("<"))
        return actual < expected + tolerance;
    if (op == QLatin1String("<="))
        return actual <= expected + tolerance;
    if (op == QLatin1String(">"))
        return actual > expected - tolerance;
    if (op == QLatin1String(">="))
        return actual >= expected - tolerance;
    return false;
}

} // namespace

// ---------------------------------------------------------------------------

ScenarioRunner::Outcome ScenarioRunner::Outcome::follow(const QString &port)
{
    Outcome outcome;
    outcome.kind = Kind::Follow;
    outcome.port = port;
    return outcome;
}

ScenarioRunner::Outcome ScenarioRunner::Outcome::await()
{
    Outcome outcome;
    outcome.kind = Kind::Await;
    return outcome;
}

ScenarioRunner::Outcome ScenarioRunner::Outcome::halt(Verdict verdict, const QString &message)
{
    Outcome outcome;
    outcome.kind = Kind::Halt;
    outcome.verdict = verdict;
    outcome.message = message;
    return outcome;
}

ScenarioRunner::Outcome ScenarioRunner::Outcome::fault(const QString &message)
{
    Outcome outcome;
    outcome.kind = Kind::Fault;
    outcome.verdict = Verdict::Error;
    outcome.message = message;
    return outcome;
}

// ---------------------------------------------------------------------------

ScenarioRunner::ScenarioRunner(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<testbuilder::ScenarioRunner::LogEntry>();
    qRegisterMetaType<testbuilder::ScenarioRunner::State>();
    qRegisterMetaType<testbuilder::ScenarioRunner::Verdict>();

    m_stepTimer = new QTimer(this);
    m_stepTimer->setSingleShot(true);
    connect(m_stepTimer, &QTimer::timeout, this, &ScenarioRunner::executeStep);

    installDefaultHandlers();
}

ScenarioRunner::~ScenarioRunner() = default;

void ScenarioRunner::setBackend(ScenarioBackend *backend)
{
    if (m_backend == backend)
        return;

    if (m_backend)
        m_backend->disconnect(this);

    m_backend = backend;

    if (m_backend) {
        connect(m_backend, &ScenarioBackend::valueReceived, this, &ScenarioRunner::onValueReceived);
        connect(m_backend, &ScenarioBackend::requestFailed, this, &ScenarioRunner::onRequestFailed);
        connect(m_backend, &ScenarioBackend::messageReceived, this, &ScenarioRunner::onMessageReceived);
        connect(m_backend, &ScenarioBackend::backendError, this, &ScenarioRunner::onBackendError);
    }
}

bool ScenarioRunner::load(const ScenarioModel &model, QStringList *errors)
{
    if (isRunning())
        stop();

    m_model = model;
    m_current = 0;
    m_verdict = Verdict::None;
    m_loops.clear();
    m_context.clear();
    m_log.clear();
    setState(State::Idle);

    // validate() reports authoring problems; only the ones that make the graph
    // unrunnable block a run. A loose branch is worth showing but not fatal --
    // the scenario simply ends when it gets there.
    const QStringList problems = m_model.validate();
    if (errors)
        *errors = problems;

    return m_model.startNode() != 0;
}

qint64 ScenarioRunner::elapsedMs() const
{
    return m_clock.isValid() ? m_clock.elapsed() : 0;
}

void ScenarioRunner::registerHandler(const QString &typeId, Handler handler)
{
    if (handler)
        m_handlers.insert(typeId, std::move(handler));
    else
        m_handlers.remove(typeId);
}

void ScenarioRunner::logMessage(LogLevel level, const QString &text, quint64 nodeId)
{
    LogEntry entry;
    entry.elapsedMs = elapsedMs();
    entry.level = level;
    entry.nodeId = nodeId;
    entry.text = text;
    m_log.append(entry);
    emit logged(entry);
}

// --- parking the machine ---------------------------------------------------

void ScenarioRunner::armAwait(Await::What what, const QString &resumePort,
                              const QString &timeoutPort, int timeoutMs)
{
    clearAwait();
    m_await.what = what;
    m_await.nodeId = m_current;
    m_await.resumePort = resumePort;
    m_await.timeoutPort = timeoutPort;

    const int generation = m_await.generation;
    QTimer::singleShot(qMax(0, timeoutMs), this, [this, generation] {
        if (m_await.generation != generation || m_await.resolved)
            return;
        if (m_await.what == Await::What::Timer)
            queueResume(m_await.resumePort);
        else
            handleAwaitTimeout();
    });
}

ScenarioRunner::Outcome ScenarioRunner::awaitTimer(int milliseconds, const QString &resumePort)
{
    armAwait(Await::What::Timer, resumePort, QString(), milliseconds);
    return Outcome::await();
}

ScenarioRunner::Outcome ScenarioRunner::awaitValue(const QString &name, int timeoutMs,
                                                   const QString &resumePort,
                                                   const QString &timeoutPort)
{
    armAwait(Await::What::Value, resumePort, timeoutPort, qMax(1, timeoutMs));
    m_await.name = name;
    return Outcome::await();
}

ScenarioRunner::Outcome ScenarioRunner::awaitMessage(int messageId, const QByteArray &expectedData,
                                                     int timeoutMs, const QString &resumePort,
                                                     const QString &timeoutPort)
{
    armAwait(Await::What::Message, resumePort, timeoutPort, qMax(1, timeoutMs));
    m_await.messageId = messageId;
    m_await.expectedData = expectedData;
    return Outcome::await();
}

void ScenarioRunner::clearAwait()
{
    // The generation survives the reset: any timer still queued for the await
    // we are dropping compares its captured generation and does nothing.
    const int generation = m_await.generation + 1;
    m_await = Await();
    m_await.generation = generation;
}

// A backend is allowed to answer from inside the call that asked -- a cached
// value, a synchronous API. The handler has not returned yet at that point, so
// the machine is not in Waiting and cannot resume. Deferring by one event loop
// turn makes both timings take the same path.
void ScenarioRunner::queueResume(const QString &port)
{
    if (m_await.resolved)
        return;
    m_await.resolved = true;

    const int generation = m_await.generation;
    QTimer::singleShot(0, this, [this, generation, port] {
        if (m_await.generation == generation)
            resumeFromAwait(port);
    });
}

void ScenarioRunner::handleAwaitTimeout()
{
    if (m_await.what == Await::What::Nothing || m_await.resolved)
        return;

    const QString what = m_await.what == Await::What::Value
                             ? QStringLiteral("'%1'").arg(m_await.name)
                             : QStringLiteral("message %1").arg(formatId(m_await.messageId));

    // A branch wired for it turns the timeout into part of the test; without
    // one, nothing answered and the scenario failed.
    if (!m_await.timeoutPort.isEmpty()) {
        logMessage(LogLevel::Warning, QStringLiteral("Timed out waiting for %1.").arg(what),
                   m_await.nodeId);
        queueResume(m_await.timeoutPort);
        return;
    }

    const quint64 nodeId = m_await.nodeId;
    clearAwait();
    logMessage(LogLevel::Error, QStringLiteral("Timed out waiting for %1.").arg(what), nodeId);
    finish(Verdict::Fail, QStringLiteral("Nothing answered for %1.").arg(what));
}

void ScenarioRunner::resumeFromAwait(const QString &port)
{
    if (m_state != State::Waiting)
        return;

    // Callers pass m_await.resumePort straight in, and clearAwait() owns that
    // string -- take a copy before the await is torn down.
    const QString resumePort = port;
    const ScenarioNode *node = m_model.node(m_await.nodeId);
    clearAwait();
    setState(State::Running);

    if (!node) {
        finish(Verdict::Error, QStringLiteral("The awaiting block disappeared from the scenario."));
        return;
    }
    leaveVia(*node, resumePort);
}

// --- backend events --------------------------------------------------------

void ScenarioRunner::onValueReceived(const QString &name, const QVariant &value)
{
    // Values are remembered whenever they arrive, solicited or not, so an
    // Expect Signal later in the run can compare against the freshest one.
    if (value.isValid())
        setContextValue(name, value);

    if (m_await.what != Await::What::Value || m_await.resolved)
        return;
    if (!m_await.name.isEmpty() && name != m_await.name)
        return; // an answer to some earlier request

    logMessage(LogLevel::Info, QStringLiteral("Received %1 = %2").arg(name, formatValue(value)),
               m_await.nodeId);
    queueResume(m_await.resumePort);
}

void ScenarioRunner::onRequestFailed(const QString &name, const QString &reason)
{
    if (m_await.what != Await::What::Value || m_await.resolved)
        return;
    if (!m_await.name.isEmpty() && name != m_await.name)
        return;

    const quint64 nodeId = m_await.nodeId;
    clearAwait();
    logMessage(LogLevel::Error, QStringLiteral("Request for '%1' was refused: %2").arg(name, reason),
               nodeId);
    // The other side saying no is a test result, not a tool malfunction.
    finish(Verdict::Fail, QStringLiteral("Request for '%1' was refused.").arg(name));
}

void ScenarioRunner::onMessageReceived(int id, const QByteArray &data)
{
    if (m_await.what != Await::What::Message || m_await.resolved)
        return;
    if (m_await.messageId >= 0 && id != m_await.messageId)
        return;
    if (!m_await.expectedData.isEmpty() && data != m_await.expectedData)
        return;

    logMessage(LogLevel::Info,
               QStringLiteral("Received %1: %2")
                   .arg(formatId(id), QString::fromLatin1(data.toHex(' '))),
               m_await.nodeId);
    queueResume(m_await.resumePort);
}

void ScenarioRunner::onBackendError(const QString &reason)
{
    if (!isRunning())
        return;
    logMessage(LogLevel::Error, QStringLiteral("Backend error: %1").arg(reason), m_current);
    finish(Verdict::Error, reason);
}

// --- the machine -----------------------------------------------------------

void ScenarioRunner::start()
{
    if (isRunning())
        return;

    if (m_model.isEmpty()) {
        finish(Verdict::Error, QStringLiteral("No scenario loaded."));
        return;
    }

    const quint64 startNode = m_model.startNode();
    if (startNode == 0) {
        finish(Verdict::Error, QStringLiteral("The scenario has no Start block."));
        return;
    }

    if (m_backend && !m_backend->isOpen()) {
        QString error;
        if (!m_backend->open(&error)) {
            finish(Verdict::Error, QStringLiteral("Could not open the backend: %1").arg(error));
            return;
        }
    }

    m_log.clear();
    m_context.clear();
    m_loops.clear();
    m_stepCount = 0;
    m_verdict = Verdict::None;
    m_pauseRequested = false;
    clearAwait();
    m_clock.start();
    m_current = startNode;

    setState(State::Running);
    emit started();

    if (!m_backend) {
        logMessage(LogLevel::Warning,
                   QStringLiteral("No backend is set; every block that talks to one will fault."));
    }
    logMessage(LogLevel::Info, QStringLiteral("Running '%1'.")
                                   .arg(m_model.name().isEmpty() ? QStringLiteral("Untitled")
                                                                 : m_model.name()));
    scheduleStep();
}

void ScenarioRunner::stop()
{
    if (m_state == State::Idle || m_state == State::Finished)
        return;

    m_stepTimer->stop();
    clearAwait();
    logMessage(LogLevel::Warning, QStringLiteral("Stopped."), m_current);
    finish(Verdict::Abort, QStringLiteral("Stopped by the user."));
}

void ScenarioRunner::pause()
{
    if (!isRunning() || m_pauseRequested)
        return;

    m_pauseRequested = true;
    if (m_state == State::Running) {
        m_stepTimer->stop();
        setState(State::Paused);
    }
    // While Waiting we let the pending event resolve; scheduleStep() parks the
    // machine at the next step boundary instead of cutting an await short.
}

void ScenarioRunner::resume()
{
    if (!m_pauseRequested)
        return;

    m_pauseRequested = false;
    if (m_state == State::Paused) {
        setState(State::Running);
        scheduleStep();
    }
}

void ScenarioRunner::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

void ScenarioRunner::scheduleStep()
{
    if (m_pauseRequested) {
        setState(State::Paused);
        return;
    }
    if (m_state != State::Running)
        return;
    m_stepTimer->start(m_stepDelayMs);
}

void ScenarioRunner::executeStep()
{
    if (m_state != State::Running)
        return;

    if (++m_stepCount > m_maxSteps) {
        finish(Verdict::Error,
               QStringLiteral("Step limit of %1 reached -- the scenario is probably looping "
                              "without an exit.").arg(m_maxSteps));
        return;
    }

    const ScenarioNode *node = m_model.node(m_current);
    if (!node) {
        finish(Verdict::Error, QStringLiteral("Node %1 is not in the scenario.").arg(m_current));
        return;
    }

    emit nodeEntered(node->id);

    const Handler handler = m_handlers.value(node->typeId);
    if (!handler) {
        finish(Verdict::Error,
               QStringLiteral("No handler for block type '%1' -- register one with "
                              "ScenarioRunner::registerHandler().").arg(node->typeId));
        return;
    }

    // Copy the node: a handler is free to call back into the runner, and the
    // model's storage must not be referenced across that.
    const ScenarioNode current = *node;
    applyOutcome(current, handler(this, current));
}

void ScenarioRunner::applyOutcome(const ScenarioNode &node, const Outcome &outcome)
{
    switch (outcome.kind) {
    case Outcome::Kind::Follow:
        leaveVia(node, outcome.port);
        break;

    case Outcome::Kind::Await:
        // The handler armed the event before returning; just park.
        setState(State::Waiting);
        break;

    case Outcome::Kind::Halt:
        finish(outcome.verdict, outcome.message);
        break;

    case Outcome::Kind::Fault:
        logMessage(LogLevel::Error, outcome.message, node.id);
        finish(Verdict::Error, outcome.message);
        break;
    }
}

void ScenarioRunner::leaveVia(const ScenarioNode &node, const QString &port)
{
    emit nodeLeft(node.id, port);

    const quint64 nextId = m_model.next(node.id, port);
    if (nextId == 0) {
        finish(Verdict::Abort,
               QStringLiteral("'%1' has nothing wired to its '%2' output.")
                   .arg(node.label.isEmpty() ? node.typeId : node.label, port));
        return;
    }

    m_current = nextId;
    scheduleStep();
}

void ScenarioRunner::finish(Verdict verdict, const QString &reason)
{
    m_stepTimer->stop();
    clearAwait();
    m_verdict = verdict;
    setState(State::Finished);
    emit finished(verdict, reason);
}

// --- block behaviour -------------------------------------------------------

// A Repeat block is entered twice for every iteration: once from whatever
// precedes it, then again each time the body branch loops back into its input.
// The frame on top of the loop stack is what tells the two apart.
ScenarioRunner::Outcome ScenarioRunner::enterRepeat(const ScenarioNode &node)
{
    const int count = node.params.value(QStringLiteral("count")).toInt();

    if (!m_loops.isEmpty() && m_loops.last().nodeId == node.id) {
        LoopFrame &frame = m_loops.last();
        --frame.remaining;
        if (frame.remaining > 0) {
            logMessage(LogLevel::Info,
                       QStringLiteral("Iteration %1 of %2")
                           .arg(count - frame.remaining + 1).arg(count),
                       node.id);
            return Outcome::follow(QStringLiteral("body"));
        }
        m_loops.removeLast();
        logMessage(LogLevel::Info, QStringLiteral("Repeat finished (%1 iterations).").arg(count),
                   node.id);
        return Outcome::follow(QStringLiteral("done"));
    }

    // Re-entered from outside while an unfinished frame for this same block is
    // buried in the stack -- the graph jumped out of the loop and back in.
    // Drop the stale frames rather than counting against them.
    for (int i = m_loops.size() - 1; i >= 0; --i) {
        if (m_loops.at(i).nodeId == node.id) {
            m_loops.remove(i, m_loops.size() - i);
            break;
        }
    }

    if (count <= 0)
        return Outcome::follow(QStringLiteral("done"));

    m_loops.append({node.id, count});
    logMessage(LogLevel::Info, QStringLiteral("Iteration 1 of %1").arg(count), node.id);
    return Outcome::follow(QStringLiteral("body"));
}

void ScenarioRunner::installDefaultHandlers()
{
    m_handlers.insert(QStringLiteral("start"), [](ScenarioRunner *, const ScenarioNode &) {
        return Outcome::follow(QStringLiteral("out"));
    });

    m_handlers.insert(QStringLiteral("end"), [](ScenarioRunner *runner, const ScenarioNode &node) {
        const QString text = node.params.value(QStringLiteral("verdict")).toString();
        Verdict verdict = Verdict::Pass;
        if (text == QLatin1String("Fail"))
            verdict = Verdict::Fail;
        else if (text == QLatin1String("Abort"))
            verdict = Verdict::Abort;

        runner->logMessage(verdict == Verdict::Pass ? LogLevel::Info : LogLevel::Error,
                           QStringLiteral("End: %1").arg(text), node.id);
        return Outcome::halt(verdict, QStringLiteral("Reached '%1'.")
                                          .arg(node.label.isEmpty() ? text : node.label));
    });

    m_handlers.insert(QStringLiteral("set_signal"), [](ScenarioRunner *runner, const ScenarioNode &node) {
        ScenarioBackend *backend = runner->backend();
        if (!backend)
            return Outcome::fault(QStringLiteral("Set Signal needs a backend."));

        const QString name = node.params.value(QStringLiteral("signal")).toString();
        const QVariant value = numericOrText(node.params.value(QStringLiteral("value")));

        QString error;
        if (!backend->writeValue(name, value, &error))
            return Outcome::fault(QStringLiteral("Could not write '%1': %2").arg(name, error));

        runner->logMessage(LogLevel::Info,
                           QStringLiteral("Set %1 = %2").arg(name, formatValue(value)), node.id);
        return Outcome::follow(QStringLiteral("out"));
    });

    // The block id carries a typo ("reques_diagnostic"); it is what is written
    // into saved scenarios, so it stays until a format migration renames it.
    m_handlers.insert(QStringLiteral("request_diagnostic"), [](ScenarioRunner *runner, const ScenarioNode &node) {
        ScenarioBackend *backend = runner->backend();
        if (!backend)
            return Outcome::fault(QStringLiteral("Request Diagnostic needs a backend."));

        const QString name = node.params.value(QStringLiteral("param")).toString();
        const int timeoutMs = node.params.value(QStringLiteral("timeoutMs"), 200).toInt();

        // Arm the wait first: a backend that answers synchronously from inside
        // requestValue() is then still matched instead of arriving too early.
        const Outcome outcome = runner->awaitValue(name, timeoutMs, QStringLiteral("out"));
        runner->logMessage(LogLevel::Info, QStringLiteral("Requested '%1'.").arg(name), node.id);

        QString error;
        if (!backend->requestValue(name, &error))
            return Outcome::fault(QStringLiteral("Could not request '%1': %2").arg(name, error));

        return outcome;
    });

    m_handlers.insert(QStringLiteral("expect_signal"), [](ScenarioRunner *runner, const ScenarioNode &node) {
        const QString name = node.params.value(QStringLiteral("signal")).toString();
        const QString op = node.params.value(QStringLiteral("op")).toString();
        const QString expectedText = node.params.value(QStringLiteral("value")).toString();
        const double tolerance = node.params.value(QStringLiteral("tolerance")).toDouble();

        QVariant actual;
        if (runner->hasContextValue(name)) {
            // A value the backend delivered earlier in this run wins over a
            // fresh read: it is what the preceding block just asked for.
            actual = runner->contextValue(name);
        } else {
            ScenarioBackend *backend = runner->backend();
            if (!backend)
                return Outcome::fault(QStringLiteral("Expect Signal needs a backend."));

            QString error;
            if (!backend->readValue(name, &actual, &error)) {
                runner->logMessage(LogLevel::Warning,
                                   QStringLiteral("Could not read '%1': %2").arg(name, error),
                                   node.id);
                return Outcome::follow(QStringLiteral("fail"));
            }
        }

        bool actualIsNumber = false;
        bool expectedIsNumber = false;
        const double actualNumber = actual.toDouble(&actualIsNumber);
        const double expectedNumber = expectedText.toDouble(&expectedIsNumber);

        bool passed = false;
        if (actualIsNumber && expectedIsNumber) {
            passed = compareNumbers(actualNumber, op, expectedNumber, tolerance);
        } else if (op == QLatin1String("==")) {
            passed = actual.toString() == expectedText;
        } else if (op == QLatin1String("!=")) {
            passed = actual.toString() != expectedText;
        } else {
            return Outcome::fault(QStringLiteral("'%1' compares '%2' with '%3', but one of them "
                                                 "is not a number.")
                                      .arg(op, formatValue(actual), expectedText));
        }

        runner->logMessage(passed ? LogLevel::Info : LogLevel::Warning,
                           QStringLiteral("Expect %1 %2 %3%4 -> %5 (actual %6)")
                               .arg(name, op, expectedText,
                                    tolerance > 0 ? QStringLiteral(" +/-%1").arg(tolerance)
                                                  : QString(),
                                    passed ? QStringLiteral("pass") : QStringLiteral("fail"),
                                    formatValue(actual)),
                           node.id);

        return Outcome::follow(passed ? QStringLiteral("pass") : QStringLiteral("fail"));
    });

    m_handlers.insert(QStringLiteral("wait"), [](ScenarioRunner *runner, const ScenarioNode &node) {
        const int durationMs = node.params.value(QStringLiteral("durationMs")).toInt();
        runner->logMessage(LogLevel::Info, QStringLiteral("Wait %1 ms").arg(durationMs), node.id);
        return runner->awaitTimer(durationMs, QStringLiteral("out"));
    });

    m_handlers.insert(QStringLiteral("repeat"), [](ScenarioRunner *runner, const ScenarioNode &node) {
        return runner->enterRepeat(node);
    });

    m_handlers.insert(QStringLiteral("log"), [](ScenarioRunner *runner, const ScenarioNode &node) {
        const QString message = node.params.value(QStringLiteral("message")).toString();
        const QString level = node.params.value(QStringLiteral("level")).toString();

        LogLevel logLevel = LogLevel::Info;
        if (level == QLatin1String("Warning"))
            logLevel = LogLevel::Warning;
        else if (level == QLatin1String("Error"))
            logLevel = LogLevel::Error;

        runner->logMessage(logLevel, message, node.id);
        return Outcome::follow(QStringLiteral("out"));
    });

    m_handlers.insert(QStringLiteral("send_frame"), [](ScenarioRunner *runner, const ScenarioNode &node) {
        ScenarioBackend *backend = runner->backend();
        if (!backend)
            return Outcome::fault(QStringLiteral("Send Frame needs a backend."));

        const int id = node.params.value(QStringLiteral("frameId")).toInt();
        const QString payload = node.params.value(QStringLiteral("data")).toString();

        QByteArray data;
        if (!parseHexPayload(payload, &data)) {
            return Outcome::fault(QStringLiteral("'%1' is not a hex payload (expected pairs of "
                                                 "hex digits, e.g. \"01 02 FF\").").arg(payload));
        }

        QString error;
        if (!backend->sendMessage(id, data, &error))
            return Outcome::fault(QStringLiteral("Could not send %1: %2").arg(formatId(id), error));

        runner->logMessage(LogLevel::Info,
                           QStringLiteral("Sent %1: %2")
                               .arg(formatId(id), QString::fromLatin1(data.toHex(' '))),
                           node.id);
        return Outcome::follow(QStringLiteral("out"));
    });

    m_handlers.insert(QStringLiteral("expect_frame"), [](ScenarioRunner *runner, const ScenarioNode &node) {
        const int id = node.params.value(QStringLiteral("frameId")).toInt();
        const int timeoutMs = node.params.value(QStringLiteral("timeoutMs")).toInt();
        const QString payload = node.params.value(QStringLiteral("data")).toString();

        QByteArray expected;
        if (!parseHexPayload(payload, &expected)) {
            return Outcome::fault(QStringLiteral("'%1' is not a hex payload (expected pairs of "
                                                 "hex digits, e.g. \"01 02 FF\").").arg(payload));
        }

        runner->logMessage(LogLevel::Info,
                           QStringLiteral("Waiting up to %1 ms for %2")
                               .arg(timeoutMs).arg(formatId(id)),
                           node.id);

        return runner->awaitMessage(id, expected, timeoutMs, QStringLiteral("received"),
                                    QStringLiteral("timeout"));
    });
}

} // namespace testbuilder
