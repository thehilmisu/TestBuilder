# Design — How the Scenario Engine Works

Why the engine is built the way it is, and what you need to know before changing
it. For *using* it from another application, see `integration.md`.

---

## 1. The problem

A scenario is a **graph**, not a script. `Expect Signal` has `pass` and `fail`
outputs, `Repeat` has `body` and `done`, `Expect Frame` has `received` and
`timeout`. Flattening that into a step list loses the branches, so the engine
walks the graph directly.

Three things fall out of that and shape everything else:

1. **Steps take time.** A `Wait 5000 ms`, a diagnostic round trip, a frame that
   may never arrive. Blocking on any of them freezes the host UI.
2. **The graph is authored by hand** and is often wrong — a loose branch, a loop
   with no exit, two Start blocks.
3. **The engine must not know what it is testing.** It runs inside the editor
   against a simulation and inside a host application against real hardware,
   with the same code.

---

## 2. Layering

```
   ┌──────────────────────────────────────────┐
   │  nodeeditor/   canvas, palette, docks    │  QtWidgets
   │      └── scenemodel.cpp ─────────────┐   │
   └──────────────────────────────────────┼───┘
                                          │  the only file that knows both halves
   ┌──────────────────────────────────────▼───┐
   │  runtime/                                │  Qt Core + Qt Gui
   │    scenariomodel   the graph             │
   │    scenariorunner  the state machine     │
   │    scenariobackend the seam outward      │
   └──────────────────────────────────┬───────┘
                                      │
                            your application
```

**The dependency arrow points one way only.** `runtime/` never includes anything
from `nodeeditor/` except `blocktypes.h`, which is plain data — the shared
vocabulary describing what parameters a block carries and of what type.

That single rule is what makes the runtime linkable on its own. It was not free:
`ScenarioModel::fromScene()` used to live in `scenariomodel.cpp` and dragged
`QGraphicsScene` — and therefore all of QtWidgets, and therefore the whole
editor — into anything that linked the engine. Moving it to
`nodeeditor/scenemodel.cpp` inverted that. If you ever add an include from
`runtime/` to a canvas class, you have quietly re-broken it; the standalone
example build in `examples/integration/` is the canary.

`ScenarioBackend` is the seam on the other side, for the same reason in the
opposite direction: the engine must not know whether a value comes off a LIN
bus, a socket or a mock.

---

## 3. The model

```cpp
struct ScenarioNode { quint64 id; QString typeId; QString label; QVariantMap params; };
struct ScenarioLink { quint64 fromNode; QString fromPort; quint64 toNode; QString toPort; };
```

`ScenarioModel` holds nodes in a hash by id, links in a vector, plus one derived
index:

```cpp
QHash<QString, quint64> m_edges;   // "12/pass" -> 34
```

That index is the hot path. Every step asks the same question — *given that I am
leaving node 12 through port `pass`, where do I go?* — and `next()` answers it
with one lookup instead of scanning the link list.

**Ports are referenced by name, never by index.** Reordering `b.outputs` in
`blocktypes.cpp` would silently rewire every index-based link to the wrong
branch, and a test that quietly takes the `fail` path is far worse than one that
fails to load. Names survive reordering; a *rename* shows up as an explicit
"no such port" complaint at load time.

### Parameter coercion

JSON has one number type, so `tolerance: 5` comes back as a double and a block
would end up holding a double where its spec says Integer. `coerceParams()`
fixes both that and forward compatibility in one pass:

```
for each ParamSpec in the block type:
    value = file value, or the spec's default if absent
    coerce it to the spec's declared type
```

Defaults-first means a parameter added to a block type next month loads old
files with the new field at its default, rather than as an empty `QVariant`. A
`Choice` value that is no longer offered falls back to the default — running a
removed option is worse than running the documented one.

### Validation

`validate()` is the authoring pass, run before execution:

- exactly one `start` block
- no block with an unconnected input
- no output port left dangling
- no node unreachable from Start (BFS over the links)
- no unknown block types

It returns strings, not a bool, because the caller decides severity.
`ScenarioRunner::load()` returns `false` only when the graph cannot run at all
(no Start); everything else is advisory. That split is deliberate — a loose
`fail` branch is worth warning about but is perfectly runnable, and refusing to
run it would be more annoying than useful.

---

## 4. The state machine

```
                 load()                start()
        Idle ──────────────> Idle ───────────────> Running
                                                     │  ▲
                              pause()  ┌─────────────┤  │ event or timeout
                                       ▼             ▼  │
                                    Paused        Waiting
                                       │             │
                                       └──────┬──────┘
                                              ▼
                                          Finished
```

One step of `Running` is:

```cpp
executeStep():
    guard the step budget
    node = model.node(m_current)
    emit nodeEntered(node.id)
    handler = m_handlers[node.typeId]
    applyOutcome(node, handler(this, node))
```

The handler returns one of four outcomes, and that is the entire protocol:

| Outcome | Meaning | What the machine does |
|---|---|---|
| `follow(port)` | resolved; leave this way | look up `next()`, schedule the next step |
| `await()` | the handler armed an event | park in `Waiting` |
| `halt(verdict, reason)` | the scenario is over | `finished()` |
| `fault(message)` | this step could not be carried out | log, then finish as `Error` |

Steps are scheduled through a single-shot `QTimer`, never a loop. That is what
`setStepDelayMs()` adjusts: `0` for headless runs, ~120 ms in the editor so a
human can watch the highlight move. Even at `0` the timer yields to the event
loop between steps, so Stop stays responsive inside a tight `Repeat`.

**Falling off the end.** If `next()` returns 0 the port is unwired, and the run
ends with `Abort` and a message naming the block and the port. It is not an
`Error`: the graph was valid enough to run, the author just did not finish it,
and `validate()` already said so at load.

---

## 5. The await protocol

This is the subtle part of the engine. Read it before touching
`armAwait` / `queueResume` / `resumeFromAwait`.

A parked machine holds one `Await` record:

```cpp
struct Await {
    enum class What { Nothing, Timer, Value, Message };
    What what;
    QString name;             // Value: which one we asked for
    int messageId;            // Message: -1 matches any
    QByteArray expectedData;  // Message: empty matches any
    quint64 nodeId;           // who is waiting
    QString resumePort;       // leave here when it arrives
    QString timeoutPort;      // leave here on timeout; empty = fail the run
    bool resolved;            // a resume is already queued
    int generation;           // monotonic; identifies this await
};
```

Arming is always **before** calling the backend:

```cpp
const Outcome outcome = runner->awaitValue(name, timeoutMs, "out");  // arm
if (!backend->requestValue(name, &error))                            // then ask
    return Outcome::fault(...);
return outcome;
```

That order is load-bearing. Three failure modes are designed out here, and each
one is a bug that only shows up against certain backends:

### Race 1 — the synchronous answer

A backend with the value already cached may emit `valueReceived()` from *inside*
`requestValue()`. At that moment the handler has not returned, so the machine is
still `Running`, not `Waiting`. The naive implementation checks
`state == Waiting`, sees `Running`, and drops the answer — and the scenario then
times out against a backend that answered instantly.

Two things fix it together: arming before the call, so the `Await` record exists
when the signal fires; and `queueResume()`, which does not resume directly but
defers by one event-loop turn:

```cpp
m_await.resolved = true;
QTimer::singleShot(0, this, [this, generation, port] {
    if (m_await.generation == generation)
        resumeFromAwait(port);
});
```

By the time that fires, `applyOutcome()` has run and the state is `Waiting`. A
cached answer and a bus round trip take the same path.

### Race 2 — the stale timeout

Every await arms a timeout timer. If the await resolves first, that timer is
still queued. Without a guard it fires later and times out whatever the machine
is waiting on *by then* — a completely unrelated block, seconds later, with a
message naming the wrong thing.

`generation` is a monotonic counter that survives `clearAwait()`:

```cpp
void ScenarioRunner::clearAwait() {
    const int generation = m_await.generation + 1;
    m_await = Await();
    m_await.generation = generation;
}
```

Every timer captures the generation it was armed for and does nothing if it no
longer matches. Cheap, and it makes stale callbacks structurally impossible
rather than merely unlikely.

### Race 3 — the double resume

Two matching events can arrive before the queued resume runs — a duplicate
frame, a retried response. `resolved` latches on the first and makes the rest
no-ops.

### A dangling reference, for the record

`resumeFromAwait()` takes the port by `const QString &`, and every caller passes
`m_await.resumePort` — which `clearAwait()` then destroys. The reference dangled
and every async block resumed through an empty port name, aborting the run. The
fix is one line:

```cpp
const QString resumePort = port;   // clearAwait() owns the string `port` refers to
```

It is called out here because the shape invites it: any new code that passes a
piece of `m_await` into something that clears `m_await` has the same bug.

### Timeouts

When one fires and the block has a `timeoutPort` wired (only *Expect Frame*
does), the timeout becomes part of the test and the machine leaves that way.
With no timeout branch, nothing answered, and the run ends as **`Fail`** — not
`Error`. See below.

---

## 6. Verdicts

| Verdict | Cause |
|---|---|
| `Pass` / `Fail` | an End block with that verdict; a refused request; an await timeout |
| `Abort` | `stop()`, or an unwired output |
| `Error` | no Start block, unknown block type, no backend, a backend call returning `false`, `backendError()` |

The line is: **`Fail` is the thing under test misbehaving. `Error` is the tool
being unable to run the test at all.**

A device refusing a diagnostic request is a result — the test ran and the answer
was no. A dropped channel is not a result; it means you learned nothing. Reports
that collapse the two produce the worst possible outcome, a red build that
nobody can act on because it might be a broken rig.

This is why `requestFailed()` and `backendError()` are separate signals, and why
`readValue()` returning `false` routes to the block's `fail` branch instead of
faulting: a value that is not there is something the author can wire a response
to.

---

## 7. Blocks are handlers, not a switch

```cpp
QHash<QString, Handler> m_handlers;   // BlockType::id -> behaviour
```

`installDefaultHandlers()` fills it in the constructor;
`registerHandler(typeId, handler)` adds or **replaces** an entry.

Adding a block is one entry in `blocktypes.cpp` (palette, ports, parameters) and
one handler. Neither touches the state machine. A host application can also
override a stock block's meaning without forking the engine.

The lambdas are defined inside a member function, so they can reach private
members through the `runner` pointer — which is how `enterRepeat()` gets at the
loop stack while staying private to everyone else.

### `Repeat` and the loop stack

A `Repeat` is entered once per iteration: first from whatever precedes it, then
again each time `body` loops back into its input. The two are told apart by the
top of a stack:

```cpp
QVector<LoopFrame> m_loops;   // { nodeId, remaining }
```

- top of stack is this node → decrement; `> 0` leave via `body`, else pop and
  leave via `done`
- otherwise → push `{id, count}` and leave via `body`

Nesting works because inner loops push and pop above outer ones. If the graph
jumps out of a loop and back in from outside, any stale frame for that node is
dropped rather than counted against — re-entering a loop starts it over, which
is the only reading that is not surprising.

---

## 8. Fan-in, fan-out

**An input port accepts many incoming links. An output port drives exactly one.**

The asymmetry is the whole loop story. A `Repeat` body has to come back into the
same input the scenario first entered through:

```
Start ──> Repeat ──body──> Set Signal ──> Wait ──┐
            ^                                    │
            └────────────────────────────────────┘
          └──done──> End
```

The editor originally replaced any existing link into an input, which made this
graph impossible to draw: wiring the loop deleted the entry edge and the Repeat
became unreachable. The block was effectively dead in the UI.

Keeping outputs single is what preserves determinism — every step has exactly
one successor per port, so there is never a question of which branch runs. The
runner does not care how many edges *arrive* at a node; it only ever follows
edges out.

---

## 9. Guards

| Guard | Catches |
|---|---|
| step budget (`setMaxSteps`, default 100000) | a loop with no exit, before it spins the host |
| per-await timeout | a device that never answers |
| `generation` counter | stale timer callbacks |
| `resolved` latch | duplicate events |
| unwired output → `Abort` | an unfinished graph, with the block and port named |
| unknown block type → `Error` | a file from a newer version, or a removed block |
| unassigned `CallbackBackend` handler → fault | a half-wired backend producing a green run that tested nothing |

The last one is worth dwelling on. A missing handler that silently returned
`true` would give a passing run that touched no hardware — the single most
dangerous failure mode a test tool can have. It reports itself instead.

---

## 10. Invariants

Things that will break if you change them without care:

1. **`runtime/` must not include a canvas class.** `blocktypes.h` is the only
   permitted crossing. Verify with the standalone example build.
2. **Arm the await before calling the backend.** Otherwise synchronous backends
   break (§5, Race 1).
3. **Never resume directly from a backend signal.** Go through `queueResume()`.
4. **Bump `generation` on every clear.** It is what makes stale callbacks inert.
5. **Do not hold a reference into `m_await` across `clearAwait()`.** Copy it.
6. **Ports are matched by name.** Do not introduce index-based link storage.
7. **Block ids are file format.** `reques_diagnostic` carries a typo and stays
   until there is a format migration; renaming it orphans saved scenarios.
8. **`Fail` and `Error` are different.** Do not merge them for convenience.

---

## 11. What the tests cover

The engine is exercised headlessly with no GUI and no hardware, one scenario per
check: both `Expect Signal` branches, tolerance bands, `Repeat` running its body
exactly `count` times, an asynchronous request, **a synchronous request**, a
refused request, a request timeout, a non-blocking `Wait` (measured), a
send/expect message round trip, an `Expect Frame` timeout branch, unrelated
traffic being ignored while awaiting a specific message, an unwired handler
faulting, an unsolicited value reaching a later check, a backend error, the
runaway-loop guard, a loose output, running with no backend, and `validate()`.

Alongside those: a save → load → save round trip that must be byte-identical,
and a smoke test that drives the real `MainWindow` through its Run action.

The synchronous-request check is the one to keep. It is the only thing standing
between the current design and a bug that appears exclusively against fast
backends — which is to say, in someone else's application and not in ours.

---

## 12. Known limits

- **One scenario per runner.** Parallel runs need several runners; they share
  nothing, so this works, but nothing coordinates them.
- **No sub-scenarios.** A block that runs another `.tbscn` would need a runner
  stack; the handler registry is the right place to add it.
- **No variables or expressions.** `Expect Signal` compares a value against a
  literal. Anything richer means an expression parser and a variable scope.
- **The value channel is flat and untyped** — one `QHash<QString, QVariant>` for
  the whole run, last write wins. Fine for the current blocks; it would need
  scoping before sub-scenarios.
- **`Frame ID` is capped at 0–63** by the block definition, inherited from where
  this started. It is one edit in `blocktypes.cpp` if your ids are wider.
- **Pause during an await** is honoured at the next step boundary, not
  immediately. Cutting a wait short would mean deciding what happens to the
  event that arrives during the pause.
