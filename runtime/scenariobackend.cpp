#include "scenariobackend.h"

#include <QTimer>

namespace testbuilder {

bool ScenarioBackend::open(QString *)
{
    return true;
}

void ScenarioBackend::close()
{
}

bool ScenarioBackend::isOpen() const
{
    return true;
}

// ---------------------------------------------------------------------------

namespace {

// A handler nobody assigned. Saying so is better than returning false with no
// reason, or true without doing anything: the run log then names the block that
// needs wiring up.
bool notImplemented(const char *what, QString *error)
{
    if (error)
        *error = QStringLiteral("The backend has no %1 handler.").arg(QLatin1String(what));
    return false;
}

} // namespace

bool CallbackBackend::open(QString *error)
{
    return onOpen ? onOpen(error) : true;
}

void CallbackBackend::close()
{
    if (onClose)
        onClose();
}

bool CallbackBackend::isOpen() const
{
    return onIsOpen ? onIsOpen() : true;
}

bool CallbackBackend::writeValue(const QString &name, const QVariant &value, QString *error)
{
    if (!onWriteValue)
        return notImplemented("onWriteValue", error);
    return onWriteValue(name, value, error);
}

bool CallbackBackend::readValue(const QString &name, QVariant *value, QString *error)
{
    if (!onReadValue)
        return notImplemented("onReadValue", error);
    return onReadValue(name, value, error);
}

bool CallbackBackend::requestValue(const QString &name, QString *error)
{
    if (!onRequestValue)
        return notImplemented("onRequestValue", error);
    return onRequestValue(name, error);
}

bool CallbackBackend::sendMessage(int id, const QByteArray &data, QString *error)
{
    if (!onSendMessage)
        return notImplemented("onSendMessage", error);
    return onSendMessage(id, data, error);
}

void CallbackBackend::deliverValue(const QString &name, const QVariant &value)
{
    emit valueReceived(name, value);
}

void CallbackBackend::failRequest(const QString &name, const QString &reason)
{
    emit requestFailed(name, reason);
}

void CallbackBackend::deliverMessage(int id, const QByteArray &data)
{
    emit messageReceived(id, data);
}

void CallbackBackend::reportError(const QString &reason)
{
    emit backendError(reason);
}

// ---------------------------------------------------------------------------

SimulatedBackend::SimulatedBackend(QObject *parent)
    : ScenarioBackend(parent)
{
    // Something plausible for the blocks' default field values, so a freshly
    // built scenario has something to compare against on its first run.
    m_values[QStringLiteral("ACT_Position")] = 0;
    m_values[QStringLiteral("Last Traveled Angle")] = 90;
    m_values[QStringLiteral("Chip Temperature")] = 42;
    m_values[QStringLiteral("Input Voltage")] = 13500;
    m_values[QStringLiteral("Over-Travel Counter")] = 0;
    m_values[QStringLiteral("Blockage Counter")] = 0;
    m_values[QStringLiteral("Electrical Error Counter")] = 0;
}

bool SimulatedBackend::open(QString *)
{
    m_open = true;
    return true;
}

void SimulatedBackend::close()
{
    m_open = false;
}

bool SimulatedBackend::writeValue(const QString &name, const QVariant &value, QString *error)
{
    if (!m_open) {
        if (error)
            *error = QStringLiteral("the simulated backend is closed");
        return false;
    }
    m_values[name] = value;
    return true;
}

bool SimulatedBackend::readValue(const QString &name, QVariant *value, QString *error)
{
    if (!m_open) {
        if (error)
            *error = QStringLiteral("the simulated backend is closed");
        return false;
    }
    if (!m_values.contains(name)) {
        if (error)
            *error = QStringLiteral("no simulated value named '%1'").arg(name);
        return false;
    }
    if (value)
        *value = m_values.value(name);
    return true;
}

bool SimulatedBackend::requestValue(const QString &name, QString *error)
{
    if (!m_open) {
        if (error)
            *error = QStringLiteral("the simulated backend is closed");
        return false;
    }

    // Answer on a timer rather than inline, so scenarios exercise the same
    // asynchronous path a real backend will take.
    const bool known = m_values.contains(name);
    const QVariant value = m_values.value(name);
    QTimer::singleShot(m_responseDelayMs, this, [this, name, value, known] {
        if (known)
            emit valueReceived(name, value);
        else
            emit requestFailed(name, QStringLiteral("no simulated value named '%1'").arg(name));
    });
    return true;
}

bool SimulatedBackend::sendMessage(int id, const QByteArray &data, QString *error)
{
    if (!m_open) {
        if (error)
            *error = QStringLiteral("the simulated backend is closed");
        return false;
    }

    // Echo it back, so a Send Frame -> Expect Frame pair makes progress.
    QTimer::singleShot(m_responseDelayMs, this, [this, id, data] {
        emit messageReceived(id, data);
    });
    return true;
}

} // namespace testbuilder
