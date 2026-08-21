# Scenario Runtime — Executing a Graph

How a scenario built on the canvas actually runs, and where to plug in real
hardware. Companion to `scenario-export.md`, which covers the file format.

## Layers

```
NodeScene (canvas)  ──fromScene()──┐
                                   ├──> ScenarioModel ──> ScenarioRunner ──> LinTransport
scenario.tbscn file ──fromFile()───┘      (the graph)      (state machine)     (the bus)
```

- **`runtime/scenariomodel.{h,cpp}`** — the graph without the editor: ids, block
  type, params, links. No positions, no `QGraphicsItem`. Built either from the
  live canvas or from a `.tbscn` file, so a scenario runs identically in the
  editor and from a future headless runner.
- **`runtime/scenariorunner.{h,cpp}`** — walks the model as a state machine.
- **`runtime/lintransport.{h,cpp}`** — the interface to the bus, plus a
  simulator so everything above can be exercised with no hardware attached.

Nothing in `runtime/` needs the editor to run a scenario; `fromScene()` is the
only place the two meet.

## The state machine

The runner never blocks. Each step either

- **resolves now** and names the output port to leave by (`Expect Signal`
  returns `pass` or `fail`), or
- **parks** on an event — a timer, a slave response, an incoming frame — and the
  machine resumes when that event, or its timeout, arrives.

That is what keeps the UI responsive through a `Wait 5000 ms`, what makes Stop
work mid-scenario, and what lets the canvas highlight the block being executed.

States are `Idle → Running ⇄ Waiting → Finished`, with `Paused` reachable from
`Running`. A pause requested while `Waiting` is honoured at the next step
boundary rather than cutting an await short.

A run ends with a verdict:

| Verdict | Means |
|---|---|
| `Pass` / `Fail` | the scenario reached an End block with that verdict, or a device failed to answer |
| `Abort` | stopped by the user, or the machine ran off an unconnected output |
| `Error` | the tool could not run the test: no Start block, unknown block type, no transport, bus error |

The split matters for reports: `Fail` is the device misbehaving, `Error` is the
setup being wrong.

Two guards keep a bad graph from hanging the app: a step budget
(`setMaxSteps()`, default 100000) catches a loop with no exit, and every await
carries a timeout.

## Adding a block

Block behaviour lives in handlers keyed by `BlockType::id`, so there is no
switch statement to extend:

```cpp
runner->registerHandler("actuator_move", [](ScenarioRunner *r, const ScenarioNode &node) {
    const int angle = node.params.value("angle").toInt();
    QString error;
    if (!r->transport()->writeSignal("ACT_Target", angle, &error))
        return ScenarioRunner::Outcome::fault("Could not command the actuator: " + error);
    r->logMessage(ScenarioRunner::LogLevel::Info, QStringLiteral("Move to %1").arg(angle), node.id);
    return ScenarioRunner::Outcome::follow("out");
});
```

A handler returns one of four outcomes:

- `follow(port)` — leave through that output now.
- `await()` — returned by `awaitTimer()`, `awaitSlaveResponse()` and
  `awaitFrame()`, which arm the event before handing it back.
- `halt(verdict, reason)` — the scenario is over.
- `fault(message)` — this step could not be carried out; ends the run as `Error`.

Adding the block to the palette is still the one entry in `blocktypes.cpp`.

## Implementing a real transport

`LinTransport` is pure virtual except for `buildParameterRequest()`. Subclass it
against your driver and hand it to the runner:

```cpp
m_transport = new MyVectorLinTransport(this);   // instead of SimulatedLinTransport
m_runner->setTransport(m_transport);
```

Sends are synchronous — return `true` once the request is on the bus. Receives
are asynchronous — emit the signal whenever the data lands, and the runner
matches it against whatever it is waiting for:

| Implement | Called by | Contract |
|---|---|---|
| `open()` / `close()` / `isOpen()` | the runner, at the start of a run | open the channel; fill `*error` on failure |
| `sendFrame()` | `Send Frame` | publish an unconditional frame; `id` is the 6-bit frame id, parity is yours |
| `writeSignal()` | `Set Signal` | update what the master publishes for that signal |
| `readSignal()` | `Expect Signal` | last decoded value; `false` + `*error` if the name is unknown |
| `sendMasterRequest()` | `Request Diagnostic` | put the 0x3C request on the bus, then emit `slaveResponse()` when 0x3D comes back |
| `buildParameterRequest()` | `Request Diagnostic` | map the block's parameter choice onto NAD/SID — override this instead of teaching the runner about your database |
| emit `frameReceived()` | — | every received frame; `Expect Frame` filters by id and payload |
| emit `slaveResponse()` | — | fill `value` with the decoded parameter when you can |
| emit `transportError()` | — | a bus-level failure; ends the run as `Error` |

The default `buildParameterRequest()` maps the six parameter choices onto
placeholder identifiers (`0xF190`…) with `ReadDataByIdentifier`. Replace the
table in `lintransport.cpp` with the real one from your ODX or supplier spec.

### The value channel

`Request Diagnostic` stores the decoded response under the parameter name, and
`Expect Signal` looks there before falling back to `readSignal()`. So this pair
works with no extra wiring:

```
Request Diagnostic [param: Chip Temperature] ──> Expect Signal [signal: Chip Temperature, op: <, value: 85]
```

If a response carries no decoded `value`, only its raw bytes are logged and the
following `Expect Signal` reads the bus instead.

## Drawing a loop

An input port accepts several incoming links; an output port drives exactly one.
That asymmetry is what makes a loop drawable — a `Repeat` body has to come back
into the same input the scenario entered through — while keeping every step's
successor unambiguous.

```
Start ──> Repeat ──body──> Set Signal ──> Wait ──┐
            ^                                    │
            └────────────────────────────────────┘
          └──done──> End
```

`Repeat` is entered once per iteration and the loop stack tells the two cases
apart, so `count: 4` runs the body exactly four times.

## Validation

`ScenarioModel::validate()` runs the authoring checks before a run: exactly one
Start block, no unconnected inputs or outputs, no unreachable nodes, no unknown
block types. The Run action blocks only on problems that make the graph
unrunnable and offers to continue past the rest — a loose branch just ends the
run when it is reached.
