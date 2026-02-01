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

## Cleanup checklist

Use `CLEANUP_CHECKLIST.md` to audit files for DRY, KISS, SOLID, YAGNI, Tell Don't Ask, dead/debug code, and comment bloat. Mark items as done as you complete each pass.
