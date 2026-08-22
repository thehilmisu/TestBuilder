# Integration Guide

How to run TestBuilder scenarios from another Qt application.

You need two things: a **CMake line** and a **backend class**. Everything else in
this document is detail you can come back to.

```cmake
add_subdirectory(path/to/TestBuilder/runtime TestBuilderRuntime)
target_link_libraries(MyApp PRIVATE TestBuilder::Runtime)
```

```cpp
testbuilder::ScenarioModel model = testbuilder::ScenarioModel::fromFile("check.tbscn");
testbuilder::ScenarioRunner runner;
runner.setBackend(myBackend);   // <- the class you write
runner.load(model);
runner.start();                 // returns immediately; watch finished()
```

A complete, compiling version of exactly this lives in `examples/integration/`.
Copy that directory as your starting point.

---

## 1. Where the engine lives, and what you link

**The engine is not a separate project.** It is the `runtime/` folder inside the
TestBuilder repository you already have — seven files, about 1700 lines:

```
TestBuilder/                     <- the repo you already have
├── runtime/                     <- THE ENGINE. This is what your app links.
│   ├── CMakeLists.txt           <-   defines the TestBuilderRuntime library
│   ├── scenariobackend.{h,cpp}  <-   the interface you implement
│   ├── scenariomodel.{h,cpp}    <-   the graph
│   └── scenariorunner.{h,cpp}   <-   the state machine
├── nodeeditor/                  <- the canvas. Your app does NOT link this...
│   └── blocktypes.cpp           <-   ...except this one file: plain data
├── mainwindow.cpp               <- the editor application
└── examples/integration/        <- a working host app; copy this to start
```

"Separated" means a separate **CMake target**, not a separate codebase. The
`runtime/` folder builds as a static library called `TestBuilderRuntime` that
knows nothing about the canvas, and the editor links that same library rather
than containing it. There is one repository, one history, one place to fix a bug.

### Getting it next to your project

Your application needs the TestBuilder tree somewhere on disk. Pick whichever
suits how you manage third-party code:

```bash
# a) submodule -- tracks a specific commit, updates on your terms
git submodule add <testbuilder-url> external/TestBuilder

# b) vendored copy -- no link back upstream, simplest for a locked release
cp -r /path/to/TestBuilder external/TestBuilder

# c) sibling checkout -- fine while both are yours and move together
#    ../TestBuilder relative to your project
```

Then point `add_subdirectory()` at the `runtime/` folder inside it — see §2.

### What your binary actually gets

The example host application compiles **six** translation units:

```
main.cpp          appbackend.cpp        <- yours
scenariorunner.cpp  scenariomodel.cpp  scenariobackend.cpp  blocktypes.cpp
```

No `nodeitem.cpp`, no `nodescene.cpp`, no `nodeview.cpp`, no `blockpalette.cpp`,
no `propertypanel.cpp`, no `mainwindow.cpp`. The whole editor — roughly 2900
lines of QtWidgets code — stays out.

### Why it is split at all

Before the split, `runtime/scenariomodel.cpp` contained `fromScene()`, which
included `nodescene.h`, which pulls in `QGraphicsScene`. That single include
meant:

- linking the engine dragged in **QtWidgets**, so a headless CI runner or a
  daemon needed a GUI toolkit it never used;
- and it dragged in **the entire editor** — canvas, palette, property panel, node
  items, connections — because those are what `NodeScene` refers to.

Moving that one function to `nodeeditor/scenemodel.cpp` inverted the dependency.
Now the arrow points one way: the editor knows about the engine, the engine
knows nothing about the editor. Your application sits on the same side of that
line the editor does — a consumer of the library — which is why the diagram
above shows only your backend across the boundary.

The standalone build in `examples/integration/` is the canary: if someone ever
adds a canvas include back into `runtime/`, that build stops compiling.

## 2. Implement the backend

This is the whole interface. Four calls the engine makes into your code, four
signals you emit back.

```cpp
class ScenarioBackend : public QObject {
public:
    virtual bool open (QString *error);   // optional, defaults to "already open"
    virtual void close();                 // optional
    virtual bool isOpen() const;          // optional

    virtual bool writeValue  (const QString &name, const QVariant &value, QString *error) = 0;
    virtual bool readValue   (const QString &name, QVariant *value,       QString *error) = 0;
    virtual bool requestValue(const QString &name,                        QString *error) = 0;
    virtual bool sendMessage (int id, const QByteArray &data,             QString *error) = 0;

signals:
    void valueReceived (const QString &name, const QVariant &value);
    void requestFailed (const QString &name, const QString &reason);
    void messageReceived(int id, const QByteArray &data);
    void backendError  (const QString &reason);
};
```

There are deliberately **no domain types** here — no frames, no addresses, no
service ids, nothing of ours you have to convert to. A "value" is whatever the
block's *Signal* field names, in your vocabulary. Mapping that onto a database,
a register, a CAN/LIN signal or an HTTP field is your side of the line and the
engine never looks.

### The general contract

- **Calls out are synchronous.** Return `true` once the operation has been handed
  to your stack — not once it completed on the wire. Return `false` and fill
  `*error` if you could not even start.
- **`*error` is optional but always worth filling.** It is what turns a red line
  in the run log from *"Could not write 'ACT_Position': "* into something a test
  engineer can act on. It may be `nullptr`; check before writing.
- **Everything that arrives later comes back through the signals.** The engine
  matches it against whatever the running scenario is waiting for and ignores
  the rest, so it is safe to forward all of your traffic.

### `writeValue` — the *Set Signal* block

```cpp
bool AppBackend::writeValue(const QString &name, const QVariant &value, QString *error)
{
    if (!m_bus->setSignal(name, value)) {
        if (error) *error = QStringLiteral("the bus rejected '%1'").arg(name);
        return false;
    }
    return true;
}
```

`value` arrives already converted: a *Value* field of `1350` reaches you as an
integer `QVariant` (`qlonglong`), `12.5` as a double, `OPEN` as a string. Returning `false`
ends the run with an `Error` verdict — a value you cannot write is a broken
test, not a failing one.

### `readValue` — the *Expect Signal* block

```cpp
bool AppBackend::readValue(const QString &name, QVariant *value, QString *error)
{
    if (!m_bus->getSignal(name, value)) {
        if (error) *error = QStringLiteral("no signal named '%1'").arg(name);
        return false;
    }
    return true;
}
```

This one is synchronous because a check has to compare something *now*.

Returning `false` here does **not** end the run: the check takes its `fail`
branch and logs the reason. That is almost always what the test author meant —
"the value I wanted was not there" is a test result.

**If your stack can only fetch asynchronously**, don't fake it. Put a *Request
Diagnostic* block in front of the check; its answer is remembered under the same
name and `Expect Signal` reads that instead of calling here. See
[The value channel](#the-value-channel).

### `requestValue` — the *Request Diagnostic* block

```cpp
bool AppBackend::requestValue(const QString &name, QString *error)
{
    if (!m_bus->requestParameter(name)) {
        if (error) *error = QStringLiteral("could not request '%1'").arg(name);
        return false;
    }
    return true;   // the answer arrives later, via valueReceived()
}
```

Return `true` once the request is out. Then, whenever the answer lands:

```cpp
emit valueReceived(name, decodedValue);              // it worked
emit requestFailed(name, "parameter not supported"); // it was refused
```

The block's *Response timeout* field is the backstop; if neither signal arrives
in time the run ends as `Fail`.

**Answering synchronously is fine.** If you already have the value cached, emit
`valueReceived()` from inside `requestValue()` before returning. The engine arms
its wait *before* calling you and defers the resume by one event-loop turn, so a
cached answer and a real round trip take exactly the same path. There is a test
for this; you do not need to work around it.

### `sendMessage` — the *Send Frame* block

```cpp
bool AppBackend::sendMessage(int id, const QByteArray &data, QString *error)
{
    if (!m_bus->transmit(id, data)) {
        if (error) *error = QStringLiteral("could not transmit 0x%1").arg(id, 2, 16, QLatin1Char('0'));
        return false;
    }
    return true;
}
```

`id` is the block's *Frame ID* field, `data` the *Data (hex)* field parsed into
bytes (`01 02 FF`, `0102ff` and `01:02:FF` all work; a malformed payload faults
the run before reaching you).

---

## 3. Feed your events back in

```cpp
AppBackend::AppBackend(AppBus *bus, QObject *parent)
    : testbuilder::ScenarioBackend(parent), m_bus(bus)
{
    connect(m_bus, &AppBus::frameReceived,    this, &AppBackend::messageReceived);
    connect(m_bus, &AppBus::parameterDecoded, this, &AppBackend::valueReceived);
    connect(m_bus, &AppBus::channelFailed,    this, &AppBackend::backendError);
}
```

Those signatures line up as-is when your signals carry `(int, QByteArray)` and
`(QString, QVariant)`. Otherwise connect to a lambda that adapts them.

| Signal | Emit it when | Effect on the run |
|---|---|---|
| `valueReceived(name, value)` | any value is decoded — solicited or not | remembered under `name`; resumes a waiting *Request Diagnostic* |
| `requestFailed(name, reason)` | a request is refused, out of range, unsupported | ends the run as **Fail** |
| `messageReceived(id, data)` | any message arrives | resumes a matching *Expect Frame*; ignored otherwise |
| `backendError(reason)` | the channel itself broke | ends the run as **Error** |

Forward **all** of your traffic through `messageReceived`. The engine filters by
id and payload and drops anything no block is waiting for, so there is no need
to gate it on your side.

---

## 4. If you would rather not subclass

`CallbackBackend` is the same interface with assignable handlers — useful when
the functions already exist and you just want to point at them:

```cpp
auto *backend = new testbuilder::CallbackBackend(this);

backend->onWriteValue   = [this](const QString &n, const QVariant &v, QString *e) { return m_bus->setSignal(n, v, e); };
backend->onReadValue    = [this](const QString &n, QVariant *v, QString *e)       { return m_bus->getSignal(n, v, e); };
backend->onRequestValue = [this](const QString &n, QString *e)                    { return m_bus->request(n, e); };
backend->onSendMessage  = [this](int id, const QByteArray &d, QString *e)         { return m_bus->transmit(id, d, e); };

connect(m_bus, &Bus::frameReceived,    backend, &testbuilder::CallbackBackend::deliverMessage);
connect(m_bus, &Bus::parameterDecoded, backend, &testbuilder::CallbackBackend::deliverValue);
connect(m_bus, &Bus::channelFailed,    backend, &testbuilder::CallbackBackend::reportError);
```

A handler you never assign reports *"the backend has no onWriteValue handler"*
and faults the run, rather than quietly succeeding. A half-wired backend fails
on the first block that needs it instead of producing a green run that tested
nothing.

There is also `SimulatedBackend` — values in a map, requests answered on a
timer, messages echoed — which is what the editor uses until you give it a real
one. Handy as a control when you are unsure whether a problem is in your backend
or in the scenario.

---

## 5. Run a scenario

```cpp
QStringList problems;
testbuilder::ScenarioModel model =
    testbuilder::ScenarioModel::fromFile("check.tbscn", &problems);
// `problems` here = file-level complaints: unknown block types, dropped links.

testbuilder::ScenarioRunner runner;
runner.setBackend(backend);
runner.setStepDelayMs(0);       // 0 = as fast as the event loop allows

QStringList errors;
if (!runner.load(model, &errors)) {
    // false means the graph cannot run at all (no Start block, empty file).
    return report(errors);
}
// A non-empty `errors` with load() == true is advisory: loose branches,
// unreachable blocks. Worth printing; not worth refusing to run.

connect(&runner, &ScenarioRunner::logged, this, &MyApp::appendLogLine);
connect(&runner, &ScenarioRunner::finished, this, &MyApp::reportVerdict);

runner.start();
```

### Signals to watch

| Signal | Use it for |
|---|---|
| `started()` | clear your log view |
| `logged(LogEntry)` | live log: `elapsedMs`, `level`, `nodeId`, `text` |
| `nodeEntered(id)` / `nodeLeft(id, port)` | highlighting a canvas, tracing coverage |
| `stateChanged(State)` | enabling/disabling Run and Stop |
| `finished(Verdict, reason)` | the result |

`runner.log()` returns the whole `QVector<LogEntry>` afterwards, which is what
you would render into a report.

### Controls

`start()`, `stop()`, `pause()`, `resume()`. `stop()` ends the run with `Abort`.
A `pause()` requested while the machine is waiting on an event is honoured at
the next step boundary rather than cutting the wait short.

### Verdicts

| Verdict | Means | Suggested exit code |
|---|---|---|
| `Pass` | reached an End block with verdict Pass | 0 |
| `Fail` | reached an End block with verdict Fail, **or** a request was refused, timed out, or a check failed its way to one | 1 |
| `Abort` | stopped by the user, or the machine ran off an unconnected output | 2 |
| `Error` | the tool could not run the test: no Start block, unknown block, no backend, `backendError()` | 3 |

The `Fail` / `Error` split is the important one for reporting: **`Fail` is the
thing under test, `Error` is your setup.** A device refusing a request is a
`Fail`. A dropped channel is an `Error`. Do not collapse them into one bucket.

---

## 6. Block reference

What each block on the canvas does to your backend:

| Block | Calls | Waits for | Leaves via |
|---|---|---|---|
| Start | — | — | `out` |
| End | — | — | *(ends the run)* |
| Set Signal | `writeValue(signal, value)` | — | `out` |
| Expect Signal | `readValue(signal)` *(or the value channel)* | — | `pass` / `fail` |
| Request Diagnostic | `requestValue(param)` | `valueReceived` / `requestFailed` | `out` |
| Send Frame | `sendMessage(frameId, data)` | — | `out` |
| Expect Frame | — | `messageReceived` matching id + data | `received` / `timeout` |
| Wait | — | its own timer | `out` |
| Repeat | — | — | `body` / `done` |
| Log | — | — | `out` |

### The value channel

`Request Diagnostic` stores its answer under the requested name, and
`Expect Signal` looks there **before** calling `readValue()`. So this pair works
with nothing extra wired up:

```
Request Diagnostic [param: Chip Temperature] ──> Expect Signal [signal: Chip Temperature, op: <, value: 85]
```

Every `valueReceived()` is remembered this way, including unsolicited ones. If
your stack pushes decoded values continuously, `Expect Signal` will read the
freshest one without a Request block in front of it at all.

The store is cleared at the start of each run, so one run never sees values from
the last.

### Naming

Block fields are free text. `Signal: ACT_Position` reaches you as the string
`"ACT_Position"` — whatever names your system already uses, type those into the
blocks. The *Request Diagnostic* block is a dropdown whose entries live in
`nodeeditor/blocktypes.cpp`; edit that list to match your parameters.

---

## 7. Threading

The engine is **single-threaded and lives on the thread that owns the runner.**

- The four `writeValue`/`readValue`/`requestValue`/`sendMessage` calls happen on
  the runner's thread, directly. If your bus lives on another thread, either
  make those four entry points thread-safe or post to your thread inside them.
- The four signals can be emitted from any thread. Qt's automatic connections
  queue them onto the runner's thread, which is the safe path — do not force
  `Qt::DirectConnection` on those connects.
- Do not call `start()`/`stop()` from another thread; use a queued invocation.

Running the engine on a worker thread is fine; give that thread an event loop
and create the runner there.

---

## 8. Ownership and lifetime

- `setBackend()` does **not** take ownership. The backend must outlive the
  runner. Parent it to the same object that owns the runner and you are safe.
- `load()` copies the model, so the `ScenarioModel` you pass can go out of scope.
- Changing the backend mid-run is not supported; `stop()` first.
- One runner runs one scenario at a time. For several in parallel, create
  several runners — they share nothing.

---

## 9. Embedding the editor as well

If you want the canvas inside your application too, link the editor sources
(`nodeeditor/`, `scenarioio.*`, `runpanel.*`) alongside the runtime and use
`nodeeditor::toScenarioModel(scene)` instead of `ScenarioModel::fromFile()`:

```cpp
#include "nodeeditor/scenemodel.h"

runner.load(nodeeditor::toScenarioModel(m_scene));
```

That is the only difference — the canvas is just another way to produce a model.
`nodeeditor/scenemodel.cpp` is deliberately the *only* file that knows about
both halves, which is why the runtime stays free of QtWidgets.

---

## 10. Overriding what a block does

Registering a handler for an id that already has one replaces it, so you can
change the meaning of a stock block from your application without touching the
engine:

```cpp
runner.registerHandler("set_signal", [](ScenarioRunner *r, const ScenarioNode &node) {
    // your own interpretation of the Set Signal block
    return ScenarioRunner::Outcome::follow("out");
});
```

Same call teaches it a block it has never seen — pair it with a new entry in
`blocktypes.cpp` so the block also appears in the palette. See `design.md` for
the four outcomes a handler can return.

---

## 11. Troubleshooting

| Symptom | Cause |
|---|---|
| Nothing happens after `start()` | no event loop running |
| `"The backend has no onWriteValue handler."` | `CallbackBackend` handler not assigned |
| `"Set Signal needs a backend."` | `setBackend()` never called |
| Run ends immediately, `Error`, *"no Start block"* | the graph has no Start, or the file failed to parse — check the `problems` list from `fromFile()` |
| `"... has nothing wired to its 'fail' output."` → `Abort` | a branch on the canvas leads nowhere; `validate()` warned about this at load |
| Request always times out | `valueReceived()` never emitted, or emitted with a *name* that differs from the block's field |
| `Expect Signal` compares against a stale value | a previous `valueReceived()` for that name is in the value channel — expected behaviour; rename or request again |
| Verdict `Error` where you expected `Fail` | your backend returned `false` from a call, or emitted `backendError()`, where `requestFailed()` was the right signal |
| `"Step limit of 100000 reached"` | a loop with no exit; check the Repeat wiring |
| Editor pulled into your binary | something links a canvas class. The runtime compiles exactly one file from `nodeeditor/` — `blocktypes.cpp`, which is plain data |

---

## 12. Checklist

- [ ] `add_subdirectory` + `target_link_libraries` with `TestBuilder::Runtime`
- [ ] Backend class with the four calls implemented
- [ ] All four return paths fill `*error` on failure
- [ ] Your receive signals forwarded to `valueReceived` / `messageReceived`
- [ ] Refusals go to `requestFailed`, channel failures to `backendError`
- [ ] Backend outlives the runner
- [ ] Event loop running
- [ ] `finished()` mapped to your report or exit code
- [ ] Verified against `SimulatedBackend` first, then your own
