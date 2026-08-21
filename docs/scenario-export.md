# Scenario Export — Design Notes

Plan for the `Export` action: how a scenario built on the canvas gets written to
disk and read back. Written before implementation; nothing here is coded yet.

## Decision: JSON, not XML

Use JSON unless a company toolchain forces XML.

- It lives in QtCore (`QJsonDocument` / `QJsonObject` / `QJsonArray`) — no extra
  `Qt6::Xml` dependency. XML DOM (`QDomDocument`) needs one; `QXmlStreamWriter` is
  in QtCore but then the reader is hand-rolled too.
- Node params are already a `QVariantMap`. `QJsonObject::fromVariantMap()` /
  `toVariantMap()` is close to a one-liner each way. XML would need an invented
  encoding for typed values (`<param name="tolerance" type="int">5</param>`) and a
  hand-written parser back.
- Diffs are readable in git, and anything downstream (Python CI, report
  generator, web viewer) parses it for free.

Pick XML only to validate against a company XSD, or to feed tooling that already
speaks XML (LDF/ODX-adjacent workflows, XSLT reports). If that shows up later,
keep the in-memory model and add a second serializer — the format must not be
baked into the item classes either way.

## Two different things are called "export"

1. **Document (save / open)** — the graph *plus* editor state: node positions,
   custom labels. Round-trips losslessly. This is what `Ctrl+S` should write.
2. **Runner export** — what the LIN execution engine consumes. No positions, no
   colors.

**Decision: one file**, with the editor-only bits in a per-node `ui` object that
the runner ignores. Two files drift out of sync, and a scenario without its
layout is unpleasant to reopen. Split only if the runner is a separate binary
with a locked-down input schema.

**Do not flatten the graph into a linear step list.** `expect_signal` (pass/fail)
and `repeat` (body/done) make this a real graph. Export nodes and links; the
runner walks it from the `start` node.

## File shape

```json
{
  "format": "testbuilder.scenario",
  "formatVersion": 1,
  "name": "Actuator position check",
  "nodes": [
    { "id": 1, "type": "start", "label": "Start",
      "params": {}, "ui": { "x": -380, "y": -60 } },
    { "id": 2, "type": "expect_signal", "label": "Expect Signal",
      "params": { "signal": "ACT_Position", "op": ">=", "value": "1200", "tolerance": 5 },
      "ui": { "x": 350, "y": -100 } }
  ],
  "links": [
    { "from": { "node": 1, "port": "out" }, "to": { "node": 2, "port": "in" } }
  ]
}
```

Why each piece:

- **`formatVersion`** from day one. When a field is added later, the loader
  branches on it instead of guessing.
- **`type`** is the `BlockType::id` — the point of the library being data-driven.
  Never serialize the title as the type; the user can rename a block.
- **`label`** stored separately, since `setTitle()` allows renames
  (e.g. `"End (failed)"`).
- **Links reference ports by name, not index.** Reordering `b.outputs` in
  `blocktypes.cpp` would silently rewire index-based links to the wrong branch.
  Names survive that.
- **`ui` nested**, so stripping editor state for a runner-only variant is
  deleting one key.

## Prerequisite: stable node IDs

`NodeItem` has no identity today, so links cannot be written. Add an `int m_id`
with getter/setter, assigned by `NodeScene::addNode()` from a counter member.

`int` over `QUuid` — readable in the file, easy to eyeball while debugging the
runner. Rules that keep it safe:

- Never reuse an ID within a session, even after deletion.
- On load, set the counter to `max(id) + 1`.
- IDs are meaningful only inside one file — never persist them into a database or
  report as if they were global.

Copy/paste between two open windows is where `int` bites (ID collision). If that
becomes a feature, remap IDs on paste rather than switching to UUIDs.

## Traps to handle in the loader

**JSON has one number type.** `tolerance: 5` comes back as a `double`, so
`toVariantMap()` yields `QVariant(double)`. A node then holding a double where
the spec says `Integer` makes `QSpinBox::setValue` and the node summary text
drift. Fix: don't trust the JSON type — walk `BlockType::params` and coerce each
value by its `ParamSpec::type` (`toInt()`, `toBool()`, `toString()`).

**Load defaults-first.** Start each node's params from the spec defaults, then
overlay what the file contains. Free forward migration: a param added to a block
type next month loads old files with the new field at its default instead of an
empty `QVariant`.

**Unknown block types.** A file referencing a `type` since removed (e.g. the
commented-out `actuator_move`) — decide up front: hard error, or skip the node
and report it. Skipping silently and dropping its links is the worst option; at
minimum collect the problems and show them.

**`nodes()` order is not stable.** It is built from `items()`, which returns
stacking order — this shifts as the user clicks around, so consecutive saves of
an unchanged graph produce different files and useless diffs. Sort by ID before
writing.

## Code structure

New files `nodeeditor/scenarioio.h` / `.cpp`, as free functions rather than
methods on `NodeScene`:

```cpp
QJsonObject toJson(const NodeScene *scene);
bool fromJson(NodeScene *scene, const QJsonObject &root, QStringList *errors);
```

This keeps serialization out of the item classes, makes it unit-testable without
a GUI, and it is the piece that gets lifted into the LIN tool.

`NodeScene` needs a `nodeById(int)` helper for the link pass. Load in two passes:
all nodes first, then links — a link can reference a node defined later in the
array.

## Validate at export time

Before writing, check and report:

- exactly one `start` node
- no node with an unconnected required input
- no unreachable nodes
- every `expect_signal` branch goes somewhere

Show the list in a `QMessageBox` and let the user export anyway or cancel. Cheap
to add, and it catches the "test silently did nothing" class of bug at authoring
time instead of on the bench.

## Mechanics

- `QFileDialog::getSaveFileName` with a filter like `Scenario (*.tbscn)` — a
  distinct extension beats `.json` for double-click association, and it is still
  JSON inside.
- Write with `QSaveFile` (atomic — a crash mid-write cannot corrupt the previous
  scenario) and `QJsonDocument::Indented`.
- Track a modified flag and prompt on close/new.
- Build the import path at the same time as the export path. A writer without a
  reader cannot be tested, and round-trip bugs only surface by doing both.

## Implementation checklist

- [ ] `NodeItem`: `int id()` / `setId(int)`
- [ ] `NodeScene`: ID counter, assign in `addNode()`, `nodeById(int)`
- [ ] `nodeeditor/scenarioio.{h,cpp}`: `toJson()` / `fromJson()`
- [ ] Param coercion from `ParamSpec::type` on load; defaults-first merge
- [ ] Sort nodes by ID when writing
- [ ] Validation pass + problem dialog
- [ ] Wire up the existing `Export` action: file dialog, `QSaveFile`
- [ ] Import / Open action and the modified flag
- [ ] Round-trip test: build graph → save → clear → load → compare
