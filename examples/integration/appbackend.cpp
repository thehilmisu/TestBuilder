#include "appbackend.h"

#include <QDebug>
#include <QTimer>

AppBackend::AppBackend(AppBus *bus, QObject *parent)
    : testbuilder::ScenarioBackend(parent)
    , m_bus(bus)
{
    // Forward what your application already emits. These three connections are
    // the entire "receive" side of the integration:
    //
    //   connect(m_bus, &AppBus::frameReceived,     this, &AppBackend::messageReceived);
    //   connect(m_bus, &AppBus::parameterDecoded,  this, &AppBackend::valueReceived);
    //   connect(m_bus, &AppBus::channelFailed,     this, &AppBackend::backendError);
    //
    // Signatures line up as-is when your signals carry (int, QByteArray) and
    // (QString, QVariant); otherwise connect to a lambda that adapts them.
}

bool AppBackend::open(QString *error)
{
    // TODO: open your channel. Fill *error and return false if it fails.
    Q_UNUSED(error)
    return true;
}

void AppBackend::close()
{
    // TODO: close your channel, if the engine owns its lifetime.
}

bool AppBackend::isOpen() const
{
    return m_bus && m_bus->connected();
}

bool AppBackend::writeValue(const QString &name, const QVariant &value, QString *error)
{
    // TODO: call your own "send signal" here.
    if (!m_bus->setSignal(name, value)) {
        if (error)
            *error = QStringLiteral("the bus rejected '%1'").arg(name);
        return false;
    }
    qInfo() << "[backend] write" << name << "=" << value;
    return true;
}

bool AppBackend::readValue(const QString &name, QVariant *value, QString *error)
{
    // TODO: return the value your stack last decoded for this name.
    //
    // A name you do not know is not a crash: return false with a reason and the
    // Expect Signal block takes its `fail` branch, which is almost always what
    // the test author meant.
    if (!m_bus->getSignal(name, value)) {
        if (error)
            *error = QStringLiteral("no signal named '%1'").arg(name);
        return false;
    }
    qInfo() << "[backend] read" << name << "->" << *value;
    return true;
}

bool AppBackend::requestValue(const QString &name, QString *error)
{
    // TODO: send your master request / diagnostic read here, then emit
    // valueReceived() from whatever slot receives the answer.
    if (!m_bus->requestParameter(name)) {
        if (error)
            *error = QStringLiteral("could not request '%1'").arg(name);
        return false;
    }
    qInfo() << "[backend] request" << name;

    // Stand-in for your real response arriving later. Delete this: the point is
    // that answering asynchronously is normal and the engine waits for you,
    // with the block's own timeout as the backstop.
    QTimer::singleShot(30, this, [this, name] {
        if (name == QLatin1String("Chip Temperature"))
            emit valueReceived(name, 42);
        else
            emit requestFailed(name, QStringLiteral("parameter not supported"));
    });
    return true;
}

bool AppBackend::sendMessage(int id, const QByteArray &data, QString *error)
{
    // TODO: transmit the message. Incoming ones go back through
    // messageReceived(id, data) -- forward all of your traffic; the engine
    // ignores anything no block is waiting for.
    if (!m_bus->transmit(id, data)) {
        if (error)
            *error = QStringLiteral("could not transmit 0x%1").arg(id, 2, 16, QLatin1Char('0'));
        return false;
    }
    qInfo() << "[backend] send" << Qt::hex << id << data.toHex(' ');
    return true;
}
