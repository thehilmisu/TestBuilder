#include "blocktypes.h"

namespace nodeeditor {

namespace {

ParamSpec text(const QString &key, const QString &label, const QString &def = QString())
{
    ParamSpec p;
    p.key = key;
    p.label = label;
    p.type = ParamSpec::Text;
    p.defaultValue = def;
    return p;
}

ParamSpec integer(const QString &key, const QString &label, int def, int min, int max,
                  const QString &suffix = QString())
{
    ParamSpec p;
    p.key = key;
    p.label = label;
    p.type = ParamSpec::Integer;
    p.defaultValue = def;
    p.minimum = min;
    p.maximum = max;
    p.suffix = suffix;
    return p;
}

ParamSpec choice(const QString &key, const QString &label, const QStringList &choices)
{
    ParamSpec p;
    p.key = key;
    p.label = label;
    p.type = ParamSpec::Choice;
    p.choices = choices;
    p.defaultValue = choices.value(0);
    return p;
}

ParamSpec boolean(const QString &key, const QString &label, bool def)
{
    ParamSpec p;
    p.key = key;
    p.label = label;
    p.type = ParamSpec::Boolean;
    p.defaultValue = def;
    return p;
}

QVector<BlockType> makeBlocks()
{
    const QColor kFlow(0x6c, 0x7a, 0x89);
    const QColor kBus(0x2e, 0x86, 0xc1);
    const QColor kActuator(0x27, 0x94, 0x6b);
    const QColor kCheck(0xb9, 0x77, 0x0f);
    const QColor kControl(0x8e, 0x54, 0xb8);

    QVector<BlockType> list;

    {
        BlockType b;
        b.id = QStringLiteral("start");
        b.title = QStringLiteral("Start");
        b.category = QStringLiteral("Flow");
        b.description = QStringLiteral("Entry point of the test scenario.");
        b.accent = kFlow;
        b.outputs = {{QStringLiteral("out")}};
        list.append(b);
    }
    {
        BlockType b;
        b.id = QStringLiteral("end");
        b.title = QStringLiteral("End");
        b.category = QStringLiteral("Flow");
        b.description = QStringLiteral("Terminates the scenario with a verdict.");
        b.accent = kFlow;
        b.inputs = {{QStringLiteral("in")}};
        b.params = {choice(QStringLiteral("verdict"), QStringLiteral("Verdict"),
                           {QStringLiteral("Pass"), QStringLiteral("Fail"), QStringLiteral("Abort")})};
        list.append(b);
    }
    {
        BlockType b;
        b.id = QStringLiteral("lin_send");
        b.title = QStringLiteral("Send LIN Frame");
        b.category = QStringLiteral("LIN Bus");
        b.description = QStringLiteral("Publishes an unconditional frame on the bus.");
        b.accent = kBus;
        b.inputs = {{QStringLiteral("in")}};
        b.outputs = {{QStringLiteral("out")}};
        b.params = {text(QStringLiteral("frameId"), QStringLiteral("Frame ID"), QStringLiteral("0x21")),
                    text(QStringLiteral("data"), QStringLiteral("Data"), QStringLiteral("00 00 00 00")),
                    choice(QStringLiteral("checksum"), QStringLiteral("Checksum"),
                           {QStringLiteral("Enhanced"), QStringLiteral("Classic")})};
        list.append(b);
    }
    {
        BlockType b;
        b.id = QStringLiteral("lin_request");
        b.title = QStringLiteral("Request LIN Frame");
        b.category = QStringLiteral("LIN Bus");
        b.description = QStringLiteral("Sends a header and captures the slave response.");
        b.accent = kBus;
        b.inputs = {{QStringLiteral("in")}};
        b.outputs = {{QStringLiteral("received")}, {QStringLiteral("timeout")}};
        b.params = {text(QStringLiteral("frameId"), QStringLiteral("Frame ID"), QStringLiteral("0x22")),
                    integer(QStringLiteral("timeoutMs"), QStringLiteral("Timeout"), 100, 1, 60000,
                            QStringLiteral(" ms")),
                    text(QStringLiteral("store"), QStringLiteral("Store as"), QStringLiteral("response"))};
        list.append(b);
    }
    {
        BlockType b;
        b.id = QStringLiteral("actuator_move");
        b.title = QStringLiteral("Move Actuator");
        b.category = QStringLiteral("Actuator");
        b.description = QStringLiteral("Commands a target position and optionally waits.");
        b.accent = kActuator;
        b.inputs = {{QStringLiteral("in")}};
        b.outputs = {{QStringLiteral("out")}};
        b.params = {text(QStringLiteral("actuator"), QStringLiteral("Actuator"), QStringLiteral("ACT_1")),
                    integer(QStringLiteral("position"), QStringLiteral("Position"), 0, -32768, 32767),
                    integer(QStringLiteral("speed"), QStringLiteral("Speed"), 50, 0, 100, QStringLiteral(" %")),
                    boolean(QStringLiteral("waitReached"), QStringLiteral("Wait until reached"), true)};
        list.append(b);
    }
    {
        BlockType b;
        b.id = QStringLiteral("actuator_home");
        b.title = QStringLiteral("Home Actuator");
        b.category = QStringLiteral("Actuator");
        b.description = QStringLiteral("Runs the homing routine of the actuator.");
        b.accent = kActuator;
        b.inputs = {{QStringLiteral("in")}};
        b.outputs = {{QStringLiteral("done")}, {QStringLiteral("failed")}};
        b.params = {text(QStringLiteral("actuator"), QStringLiteral("Actuator"), QStringLiteral("ACT_1")),
                    integer(QStringLiteral("timeoutMs"), QStringLiteral("Timeout"), 5000, 1, 600000,
                            QStringLiteral(" ms"))};
        list.append(b);
    }
    {
        BlockType b;
        b.id = QStringLiteral("expect_signal");
        b.title = QStringLiteral("Expect Signal");
        b.category = QStringLiteral("Checks");
        b.description = QStringLiteral("Compares a decoded signal against an expected value.");
        b.accent = kCheck;
        b.inputs = {{QStringLiteral("in")}};
        b.outputs = {{QStringLiteral("pass")}, {QStringLiteral("fail")}};
        b.params = {text(QStringLiteral("signal"), QStringLiteral("Signal"), QStringLiteral("ACT_Position")),
                    choice(QStringLiteral("op"), QStringLiteral("Operator"),
                           {QStringLiteral("=="), QStringLiteral("!="), QStringLiteral("<"),
                            QStringLiteral("<="), QStringLiteral(">"), QStringLiteral(">=")}),
                    text(QStringLiteral("value"), QStringLiteral("Value"), QStringLiteral("0")),
                    integer(QStringLiteral("tolerance"), QStringLiteral("Tolerance"), 0, 0, 100000)};
        list.append(b);
    }
    {
        BlockType b;
        b.id = QStringLiteral("wait");
        b.title = QStringLiteral("Wait");
        b.category = QStringLiteral("Control");
        b.description = QStringLiteral("Delays the scenario for a fixed time.");
        b.accent = kControl;
        b.inputs = {{QStringLiteral("in")}};
        b.outputs = {{QStringLiteral("out")}};
        b.params = {integer(QStringLiteral("durationMs"), QStringLiteral("Duration"), 500, 1, 3600000,
                            QStringLiteral(" ms"))};
        list.append(b);
    }
    {
        BlockType b;
        b.id = QStringLiteral("repeat");
        b.title = QStringLiteral("Repeat");
        b.category = QStringLiteral("Control");
        b.description = QStringLiteral("Runs the body branch a number of times.");
        b.accent = kControl;
        b.inputs = {{QStringLiteral("in")}};
        b.outputs = {{QStringLiteral("body")}, {QStringLiteral("done")}};
        b.params = {integer(QStringLiteral("count"), QStringLiteral("Iterations"), 10, 1, 1000000)};
        list.append(b);
    }
    {
        BlockType b;
        b.id = QStringLiteral("log");
        b.title = QStringLiteral("Log");
        b.category = QStringLiteral("Control");
        b.description = QStringLiteral("Writes a line into the test report.");
        b.accent = kControl;
        b.inputs = {{QStringLiteral("in")}};
        b.outputs = {{QStringLiteral("out")}};
        b.params = {text(QStringLiteral("message"), QStringLiteral("Message"), QStringLiteral("step done")),
                    choice(QStringLiteral("level"), QStringLiteral("Level"),
                           {QStringLiteral("Info"), QStringLiteral("Warning"), QStringLiteral("Error")})};
        list.append(b);
    }

    return list;
}

} // namespace

const QVector<BlockType> &BlockLibrary::blocks()
{
    static const QVector<BlockType> all = makeBlocks();
    return all;
}

const BlockType *BlockLibrary::find(const QString &id)
{
    for (const BlockType &b : blocks()) {
        if (b.id == id)
            return &b;
    }
    return nullptr;
}

QStringList BlockLibrary::categories()
{
    QStringList result;
    for (const BlockType &b : blocks()) {
        if (!result.contains(b.category))
            result.append(b.category);
    }
    return result;
}

} // namespace nodeeditor
