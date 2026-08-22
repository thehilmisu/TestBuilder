# TestBuilder Documentation

| Document | Read it for |
|---|---|
| [`integration.md`](integration.md) | Running scenarios from another Qt application: linking the library, writing the backend, mapping blocks to your code |
| [`design.md`](design.md) | How the engine works and why: layering, the state machine, the await protocol, the invariants to preserve |
| [`scenario-export.md`](scenario-export.md) | The `.tbscn` file format and the reasoning behind it |

Working code lives in [`../examples/integration/`](../examples/integration) —
a copy-me backend and a `scenariorun` command whose exit code is the verdict.

> `scenario-runtime.md` was split into `integration.md` and `design.md`; it no
> longer exists.
