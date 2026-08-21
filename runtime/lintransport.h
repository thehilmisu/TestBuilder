#ifndef RUNTIME_LINTRANSPORT_H
#define RUNTIME_LINTRANSPORT_H

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>

namespace runtime {

// One frame on the wire. `id` is the 6-bit frame identifier; computing the
// protected id (parity bits) is the driver's job, not the scenario's.
struct LinFrame
{
    quint8 id = 0;
    QByteArray data;        // 1..8 bytes
    qint64 timestampUs = 0; // filled by the driver on receive; ignored on send
};

// A diagnostic master request (0x3C) payload.
struct DiagRequest
{
    quint8 nad = 0;    // node address of the addressed slave
    quint8 sid = 0;    // service id
    QByteArray data;   // service payload, without NAD/PCI/SID

    // Set when the request came from a "Request Diagnostic" block, so a
    // transport can map the human-readable choice onto whatever its database
    // calls it. Empty for hand-built requests.
    QString parameter;
};

// A slave response (0x3D).
struct DiagResponse
{
    quint8 nad = 0;
    QByteArray data;
    bool negative = false;   // true for a 0x7F negative response
    quint8 responseCode = 0; // NRC, valid when `negative`

    // Decoded value for the requested parameter, when the transport can decode
    // it. This is what `expect_signal` compares against after a request.
    QVariant value;
    QString parameter;
};

// ---------------------------------------------------------------------------
// The seam between the scenario engine and real hardware.
//
// Everything the runner needs from a LIN interface is behind this interface,
// so ScenarioRunner never talks to a driver directly and can be tested against
// SimulatedLinTransport below. Implement a subclass for your actual hardware.
//
// Send calls are synchronous: return true once the request is on the bus.
// Receiving is asynchronous: emit frameReceived()/slaveResponse() whenever the
// data arrives, and the runner matches it against whatever it is waiting for.
// ---------------------------------------------------------------------------
class LinTransport : public QObject
{
    Q_OBJECT

public:
    explicit LinTransport(QObject *parent = nullptr) : QObject(parent) {}
    ~LinTransport() override = default;

    // --- lifecycle ---------------------------------------------------------
    virtual bool open(QString *error = nullptr) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // --- raw frames --------------------------------------------------------
    // Publish an unconditional frame. Received frames arrive via frameReceived().
    virtual bool sendFrame(const LinFrame &frame, QString *error = nullptr) = 0;

    // --- signal level ------------------------------------------------------
    // Signals are named as in your LDF/database. writeSignal() updates the
    // value the master publishes; readSignal() returns the last decoded value.
    virtual bool writeSignal(const QString &signal, const QVariant &value,
                             QString *error = nullptr) = 0;
    virtual bool readSignal(const QString &signal, QVariant *value,
                            QString *error = nullptr) = 0;

    // --- diagnostics -------------------------------------------------------
    // Put the master request on the bus. The matching slave response is
    // delivered later through slaveResponse().
    virtual bool sendMasterRequest(const DiagRequest &request, QString *error = nullptr) = 0;

    // Maps a "Request Diagnostic" block's parameter choice onto a concrete
    // request. Override this instead of teaching the runner about NADs and SIDs.
    virtual DiagRequest buildParameterRequest(const QString &parameterName) const;

signals:
    void frameReceived(const runtime::LinFrame &frame);
    void slaveResponse(const runtime::DiagResponse &response);
    void transportError(const QString &message);
};

// ---------------------------------------------------------------------------
// A transport that talks to nothing. It keeps signal values in a map and
// answers diagnostic requests after a short delay, which is enough to exercise
// every path in the state machine without hardware attached.
// ---------------------------------------------------------------------------
class SimulatedLinTransport : public LinTransport
{
    Q_OBJECT

public:
    explicit SimulatedLinTransport(QObject *parent = nullptr);

    bool open(QString *error = nullptr) override;
    void close() override;
    bool isOpen() const override { return m_open; }

    bool sendFrame(const LinFrame &frame, QString *error = nullptr) override;
    bool writeSignal(const QString &signal, const QVariant &value, QString *error = nullptr) override;
    bool readSignal(const QString &signal, QVariant *value, QString *error = nullptr) override;
    bool sendMasterRequest(const DiagRequest &request, QString *error = nullptr) override;

    // Test hooks: preload what the "device" will report back.
    void setSignalValue(const QString &signal, const QVariant &value) { m_signals[signal] = value; }
    void setParameterValue(const QString &parameter, const QVariant &value) { m_parameters[parameter] = value; }
    void setResponseDelayMs(int ms) { m_responseDelayMs = ms; }

private:
    bool m_open = false;
    int m_responseDelayMs = 20;
    QHash<QString, QVariant> m_signals;
    QHash<QString, QVariant> m_parameters;
};

} // namespace runtime

Q_DECLARE_METATYPE(runtime::LinFrame)
Q_DECLARE_METATYPE(runtime::DiagRequest)
Q_DECLARE_METATYPE(runtime::DiagResponse)

#endif // RUNTIME_LINTRANSPORT_H
