#ifndef APPBACKEND_H
#define APPBACKEND_H

#include "runtime/scenariobackend.h"

// ---------------------------------------------------------------------------
// COPY THIS FILE INTO YOUR PROJECT AND FILL IN THE FOUR FUNCTIONS.
//
// This is the whole integration surface. The scenario engine calls the four
// overrides below when a block needs something, and you emit the inherited
// signals when something arrives. Nothing else about your application has to
// change, and the engine never learns what is behind them.
//
// Replace `AppBus` with whatever class already owns your signals, frames and
// diagnostics. If you would rather not subclass at all, see the CallbackBackend
// route in main.cpp -- same interface, wired with lambdas instead.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// DELETE THIS. It stands in for the class in your project that already sends
// signals, reads them back, requests diagnostics and transmits frames. Replace
// it with an include of your own header.
// ---------------------------------------------------------------------------
class AppBus
{
public:
    bool setSignal(const QString &, const QVariant &) { return true; }
    bool getSignal(const QString &name, QVariant *out)
    {
        *out = name == QLatin1String("ACT_Position") ? QVariant(1350) : QVariant(0);
        return true;
    }
    bool requestParameter(const QString &) { return true; }
    bool transmit(int, const QByteArray &) { return true; }
    bool connected() const { return true; }
};

class AppBackend : public testbuilder::ScenarioBackend
{
    Q_OBJECT

public:
    explicit AppBackend(AppBus *bus, QObject *parent = nullptr);

    // --- lifecycle ---------------------------------------------------------
    // Only worth overriding if the engine should open the channel. If your
    // application already manages that, delete these three and the base class
    // reports "always open".
    bool open(QString *error) override;
    void close() override;
    bool isOpen() const override;

    // --- the four the engine actually needs --------------------------------

    // "Set Signal" block -> push a value out.
    bool writeValue(const QString &name, const QVariant &value, QString *error) override;

    // "Expect Signal" block -> read the current value, now.
    bool readValue(const QString &name, QVariant *value, QString *error) override;

    // "Request Diagnostic" block -> ask for a value that arrives later. Return
    // true once the request is out; emit valueReceived(name, value) when the
    // answer lands, or requestFailed(name, reason) if it is refused.
    bool requestValue(const QString &name, QString *error) override;

    // "Send Frame" block -> put a message on the wire.
    bool sendMessage(int id, const QByteArray &data, QString *error) override;

private:
    AppBus *m_bus = nullptr;
};

#endif // APPBACKEND_H
