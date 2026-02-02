# Contributing to OpenRGB 3D Spatial Plugin

## Coding practices

- **DRY** (Don't Repeat Yourself): Extract repeated logic into shared helpers or types; avoid copy-paste.
- **KISS** (Keep It Simple): Prefer clear, straightforward code over clever or over-abstract solutions.
- **SOLID**: Single responsibility, open/closed, Liskov substitution, interface segregation, dependency inversion where they simplify the design.
- **YAGNI** (You Aren't Gonna Need It): Don't add code for hypothetical future needs; add it when required.
- **Tell, Don't Ask**: Prefer telling objects what to do (commands) over asking for data and deciding in the caller. Put behavior in the type that has the data; avoid "get X, get Y, then if X do A else B" in callers when that logic belongs on the object.

## Code quality

- **No debug code**: Remove `printf`, `qDebug`, temporary logging, or debug-only branches before committing.
- **No dead code**: Remove unused functions, variables, includes, and unreachable paths.
- **No legacy/deprecated**: Replace deprecated APIs and remove code that is no longer used or supported.
- **Comments**: Prefer minimal, meaningful comments. Avoid long decorative blocks; explain *why* when non-obvious.

## Style

- Follow existing style in the file you edit.
- 4 spaces per indent; braces on their own lines.
- `snake_case` for variables, `CamelCase` for types and functions.
- Do not modify files under `OpenRGB/` (upstream submodule); only plugin-owned sources.

## UI correctness and null safety

- **Connections**: Every signal that should drive UI or viewport behaviour is connected; no missing or duplicate connects.
- **Selection sync**: List selection (e.g. controller list, reference points, display planes) stays in sync with the viewport; selecting in list updates viewport, selecting in viewport updates list where applicable.
- **Clear selection**: When the user clears selection (e.g. list row -1), both viewport and UI (position/rotation controls, selection label, LED spacing) are updated so nothing appears “stuck” on the previous selection.
- **Layout**: UI layout makes sense; sections and tabs are grouped logically; no overlapping or squashed controls; tab content fits (scroll or min size where needed).
- **Clear on unselect**: When selection is cleared, all dependent UI clears or disables; nothing sticks to the previous selection.
- **No stuck state**: When an item is removed (e.g. effect removed from stack), dependent UI resets or closes (e.g. effect detail UI must not stay open for a removed effect); combo/list selection and detail panels stay in sync with actual data.
- **Null checks**: Any use of `viewport`, widget pointers, or other optional pointers is guarded (e.g. `if(viewport)`) before use so teardown or partial init cannot cause crashes.
- **Widget guards**: In slots that update many widgets (e.g. position/rotation spins and sliders), guard with null checks so reorder or late call cannot dereference null.

## Cleanup checklist

Use `CLEANUP_CHECKLIST.md` to audit files for DRY, KISS, SOLID, YAGNI, Tell Don't Ask, dead/debug code, comment bloat, **UI/links**, **null safety**, **layout**, and **clear/unstick** (clear on unselect, no stuck state when items removed). Mark items as done as you complete each pass. **Re-audit** already-checked files for the UI and null-safety criteria when doing Pass 2.
