#include "lintransport.h"

#include <QTimer>

namespace runtime {

namespace {

// Placeholder mapping from the "Request Diagnostic" block's parameter choices
// onto data identifiers. Replace the table with the real one from your ODX /
// supplier spec -- the runner only ever sees the DiagRequest this produces.
struct ParameterMapping
{
    const char *label;
    quint16 identifier;
};

const ParameterMapping kParameterMap[] = {
    {"Last Traveled Angle",       0xF190},
    {"Chip Temperature",          0xF191},
    {"Input Voltage",             0xF192},
    {"Over-Travel Counter",       0xF193},
    {"Blockage Counter",          0xF194},
    {"Electrical Error Counter",  0xF195},
};

constexpr quint8 kReadDataByIdentifier = 0x22;
constexpr quint8 kDefaultNad = 0x01;

} // namespace

DiagRequest LinTransport::buildParameterRequest(const QString &parameterName) const
{
    DiagRequest request;
    request.nad = kDefaultNad;
    request.sid = kReadDataByIdentifier;
    request.parameter = parameterName;

    for (const ParameterMapping &mapping : kParameterMap) {
        if (parameterName == QLatin1String(mapping.label)) {
            request.data.append(char((mapping.identifier >> 8) & 0xFF));
            request.data.append(char(mapping.identifier & 0xFF));
            break;
        }
    }
    return request;
}

// ---------------------------------------------------------------------------

SimulatedLinTransport::SimulatedLinTransport(QObject *parent)
    : LinTransport(parent)
{
    // Plausible starting values so a freshly built scenario has something to
    // compare against on the first run.
    m_signals[QStringLiteral("ACT_Position")] = 0;
    m_parameters[QStringLiteral("Last Traveled Angle")] = 90;
    m_parameters[QStringLiteral("Chip Temperature")] = 42;
    m_parameters[QStringLiteral("Input Voltage")] = 13500; // mV
    m_parameters[QStringLiteral("Over-Travel Counter")] = 0;
    m_parameters[QStringLiteral("Blockage Counter")] = 0;
    m_parameters[QStringLiteral("Electrical Error Counter")] = 0;
}

bool SimulatedLinTransport::open(QString *)
{
    m_open = true;
    return true;
}

void SimulatedLinTransport::close()
{
    m_open = false;
}

bool SimulatedLinTransport::sendFrame(const LinFrame &frame, QString *error)
{
    if (!m_open) {
        if (error)
            *error = QStringLiteral("transport is not open");
        return false;
    }

    // Echo the frame back as if a slave had answered the header, so scenarios
    // with an "Expect Frame" block make progress against the simulator.
    LinFrame echo = frame;
    QTimer::singleShot(m_responseDelayMs, this, [this, echo] { emit frameReceived(echo); });
    return true;
}

bool SimulatedLinTransport::writeSignal(const QString &signal, const QVariant &value, QString *error)
{
    if (!m_open) {
        if (error)
            *error = QStringLiteral("transport is not open");
        return false;
    }
    m_signals[signal] = value;
    return true;
}

bool SimulatedLinTransport::readSignal(const QString &signal, QVariant *value, QString *error)
{
    if (!m_open) {
        if (error)
            *error = QStringLiteral("transport is not open");
        return false;
    }
    if (!m_signals.contains(signal)) {
        if (error)
            *error = QStringLiteral("unknown signal '%1'").arg(signal);
        return false;
    }
    if (value)
        *value = m_signals.value(signal);
    return true;
}

bool SimulatedLinTransport::sendMasterRequest(const DiagRequest &request, QString *error)
{
    if (!m_open) {
        if (error)
            *error = QStringLiteral("transport is not open");
        return false;
    }

    DiagResponse response;
    response.nad = request.nad;
    response.parameter = request.parameter;
    response.value = m_parameters.value(request.parameter);
    response.negative = !m_parameters.contains(request.parameter);
    response.responseCode = response.negative ? 0x31 : 0x00; // requestOutOfRange

    QTimer::singleShot(m_responseDelayMs, this, [this, response] { emit slaveResponse(response); });
    return true;
}

} // namespace runtime
