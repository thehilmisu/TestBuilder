#ifndef TESTBUILDER_SCENARIOBACKEND_H
#define TESTBUILDER_SCENARIOBACKEND_H

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>

#include <functional>

namespace testbuilder {

// ---------------------------------------------------------------------------
// Everything the scenario engine needs from the outside world.
//
// Four calls out, four signals back. There are no domain types in this
// interface on purpose: whether a "value" is a decoded bus signal, a register,
// a REST field or a number your simulation made up is entirely your business.
// The engine only ever asks to write one, read one, request one, or send a
// message -- and waits for what comes back.
//
// Calls out are synchronous: return true once the operation has been handed to
// your stack, or fill *error and return false. Everything that arrives later
// comes back through the signals, and the engine matches it against whatever
// the running scenario is waiting for.
//
// Implement this directly, or use CallbackBackend below if you would rather
// wire up existing functions than write a subclass.
// ---------------------------------------------------------------------------
class ScenarioBackend : public QObject
{
    Q_OBJECT

public:
    explicit ScenarioBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~ScenarioBackend() override = default;

    // --- lifecycle ---------------------------------------------------------
    // Optional. The defaults say "already connected", which is what you want
    // when the channel is owned and opened elsewhere in your application.
    virtual bool open(QString *error = nullptr);
    virtual void close();
    virtual bool isOpen() const;

    // --- required ----------------------------------------------------------

    // "Set Signal": push a value out. \a name is whatever the block's Signal
    // field says, so it is your naming scheme, not ours.
    virtual bool writeValue(const QString &name, const QVariant &value, QString *error) = 0;

    // "Expect Signal": read the current value. Return false with *error set if
    // the name is unknown -- the check then takes its `fail` branch rather than
    // pretending the comparison succeeded.
    //
    // This one is synchronous because a check has to compare something now. If
    // your stack can only fetch asynchronously, put a "Request Diagnostic"
    // block in front of the check: its answer is remembered under the same name
    // and the check reads that instead of calling here.
    virtual bool readValue(const QString &name, QVariant *value, QString *error) = 0;

    // "Request Diagnostic": ask for a value that arrives later. Return true once
    // the request is out; then emit valueReceived() with the same \a name when
    // the answer lands, or requestFailed() if it is refused.
    //
    // Answering synchronously from inside this call is fine -- the engine arms
    // its wait before calling you.
    virtual bool requestValue(const QString &name, QString *error) = 0;

    // "Send Frame": emit a message. \a id is the block's Frame ID field.
    virtual bool sendMessage(int id, const QByteArray &data, QString *error) = 0;

signals:
    // An answer to requestValue(), or an unsolicited update -- both are fine.
    // The value is remembered under \a name for the rest of the run, so a later
    // "Expect Signal" on that name compares against it.
    void valueReceived(const QString &name, const QVariant &value);

    // The request could not be answered: refused, out of range, no such value.
    // Fails the running scenario with a Fail verdict, not an Error -- a device
    // saying no is a test result.
    void requestFailed(const QString &name, const QString &reason);

    // Every message you receive. "Expect Frame" filters by id and payload; the
    // rest are ignored, so it is safe to forward all of your traffic here.
    void messageReceived(int id, const QByteArray &data);

    // The channel itself broke. Ends the run with an Error verdict.
    void backendError(const QString &reason);
};

// ---------------------------------------------------------------------------
// A backend you configure instead of subclass.
//
// Assign the four handlers to functions you already have, and call the
// deliver*() slots from your own signals. Any handler left unset reports
// "not implemented" rather than silently succeeding, so a half-wired backend
// fails loudly on the first block that needs it.
//
//   auto *backend = new CallbackBackend(this);
//   backend->onWriteValue = [this](const QString &n, const QVariant &v, QString *e) {
//       return m_bus->setSignal(n, v, e);
//   };
//   connect(m_bus, &Bus::frameReceived, backend, &CallbackBackend::deliverMessage);
// ---------------------------------------------------------------------------
class CallbackBackend : public ScenarioBackend
{
    Q_OBJECT

public:
    explicit CallbackBackend(QObject *parent = nullptr) : ScenarioBackend(parent) {}

    // Set these to your own functions. The QString* is an out-parameter for an
    // error message; filling it is optional but makes the run log far better.
    std::function<bool(const QString &name, const QVariant &value, QString *error)> onWriteValue;
    std::function<bool(const QString &name, QVariant *value, QString *error)> onReadValue;
    std::function<bool(const QString &name, QString *error)> onRequestValue;
    std::function<bool(int id, const QByteArray &data, QString *error)> onSendMessage;

    // Optional; when unset the backend reports itself as permanently open.
    std::function<bool(QString *error)> onOpen;
    std::function<void()> onClose;
    std::function<bool()> onIsOpen;

public slots:
    // Call these from your application's own signals and slots.
    void deliverValue(const QString &name, const QVariant &value);
    void failRequest(const QString &name, const QString &reason);
    void deliverMessage(int id, const QByteArray &data);
    void reportError(const QString &reason);

public:
    bool open(QString *error = nullptr) override;
    void close() override;
    bool isOpen() const override;

    bool writeValue(const QString &name, const QVariant &value, QString *error) override;
    bool readValue(const QString &name, QVariant *value, QString *error) override;
    bool requestValue(const QString &name, QString *error) override;
    bool sendMessage(int id, const QByteArray &data, QString *error) override;
};

// ---------------------------------------------------------------------------
// A backend wired to nothing: values live in a map, requests are answered after
// a short delay, messages are echoed back. Enough to exercise every path in the
// state machine with no hardware and no application attached, which is what the
// editor uses until you give it a real one.
// ---------------------------------------------------------------------------
class SimulatedBackend : public ScenarioBackend
{
    Q_OBJECT

public:
    explicit SimulatedBackend(QObject *parent = nullptr);

    bool open(QString *error = nullptr) override;
    void close() override;
    bool isOpen() const override { return m_open; }

    bool writeValue(const QString &name, const QVariant &value, QString *error) override;
    bool readValue(const QString &name, QVariant *value, QString *error) override;
    bool requestValue(const QString &name, QString *error) override;
    bool sendMessage(int id, const QByteArray &data, QString *error) override;

    // Preload what the imaginary device will report.
    void setValue(const QString &name, const QVariant &value) { m_values[name] = value; }
    void setResponseDelayMs(int ms) { m_responseDelayMs = ms; }

private:
    bool m_open = false;
    int m_responseDelayMs = 20;
    QHash<QString, QVariant> m_values;
};

} // namespace testbuilder

#endif // TESTBUILDER_SCENARIOBACKEND_H
