---
name: openrgb3d-multipass-review
description: >-
  Catch-all multi-pass review AND fix for OpenRGB 3D Spatial plugin and presets
  (toolbox .fx/.fs, engines, packs, UI, docs). Lightens load — does not replace
  standalone VibeSec, Bugbot, thermo-nuclear, or deslop. Combines purpose/role fit,
  seams, sparse-LED looks, thermo-nuclear structure pressure (code judo,
  1k-line rule, no spaghetti), CONTRIBUTING hygiene (dead code, debug leftovers,
  minimal comments, no legacy shims), deslop, better-not-worse gate,
  architectural ceiling, UI wiring/integrity, plugin-tuned VibeSec security,
  end-of-run skill self-check and self-improvement (ratchet the brief each run),
  and actually applying needed fixes/updates across touched files. Use when the
  user asks for multi-pass, multipass, toolbox polish, Action Editor tool QA,
  "run the pass", deslop, fix what needs fixing, or a plugin-tuned deep quality pass.
disable-model-invocation: true
---

# OpenRGB3D multi-pass review

Persistent skill for this plugin (and its presets). **Do not lose the brief** — this file is the source of truth for later chats. Keep the plugin copy and the user Agent Store copy in sync when this skill changes.

General review **and fix** tool for **any** OpenRGB3D Spatial file (plugin or presets). Showcase *and* simple utilities. “Already good → no edit” is a valid outcome.

Sibling of thermo-nuclear code quality review **and** deslop, tuned to **this plugin’s purpose**: Action / Movie Sync / Triggers, GPU volume/particle/ribbon fields, sparse-LED rooms, once vs loop seams, and fit across engines ↔ packs ↔ UI ↔ docs — **plus** structural ambition on C++/GLSL/Qt when those files are in scope, **plus** **`CONTRIBUTING.md` hygiene** (dead/debug code out, comments minimal, no legacy/compat shims, match file + OpenRGB style), **plus** stripping AI-generated slop, **plus** **plugin-tuned VibeSec** (secure coding / bug-hunter lens — see Security pass), **plus** shipping the fixes, **plus** refusing net-worse edits, **plus** a mandatory end-of-run **self-check + self-improvement** so this skill ratchets sharper every time it runs.

### Related tools — still used side by side

Multipass is a **catch-all** that lightens the load on routine OpenRGB3D polish. It does **not** replace specialized skills/subagents. Keep using them alone when that is the right tool for the job.

| Tool | Job | Alone vs multipass |
|------|-----|--------------------|
| **This multipass skill** | Catch-all review+fix on OpenRGB3D files (looks, engines, UI, docs); pulls in a **light** security lens when in scope | Default for “run the pass” / toolbox QA |
| **VibeSec** (`.cursor/skills/vibesec/`) | Full upstream secure-coding checklist ([BehiSecc/VibeSec-Skill](https://github.com/BehiSecc/VibeSec-Skill)) | Still run **alone** for deep security / web-app depth; multipass only embeds the **tuned subset** (pass 2b), not the whole encyclopedia on every `.fx` |
| **Bugbot** (`review-bugbot` / `bugbot` subagent) | Diff/PR bug finding | Still run **alone** when you want `/review-bugbot` or a focused change review; **not** auto-spawned by multipass |
| **security-review** subagent | Explicit security review of local diffs | Still run **alone** when you ask for it |
| **thermo-nuclear / deslop** | Structure pressure / AI-slop cleanup | Multipass folds their *spirit* into passes 7–8; the standalone skills remain available |

## Execute — do not only report

Multi-pass is an **apply** loop, not a review-only memo.

When a pass finds something wrong, incomplete, drifted, or outdated: **fix it, update it, or tidy it** in the same run. That includes:

- Tool / shader / pack / engine / UI / docs content that fails a pass
- Call sites, headers, aliases, catalog entries, and AppData copies that must stay in sync
- Related files the target touches (applier, player clocks, Movie Sync/Triggers wiring, recipe map)
- Wrong swatches, broken knobs, docs that no longer match behaviour
- **Hygiene:** narrating / stale comments, dead or unused helpers, debug leftovers, legacy/compat shims (see CONTRIBUTING)

**Keep** only when the pass genuinely concludes the file already fulfills its role. Do not leave known fail findings as “noted for later” unless the user asked for audit-only, or a fix is blocked (missing secret, needs human product call) — then say what blocked it and the smallest next step.

Prefer focused fixes that preserve intended behaviour; deepen polish only when it still fits the role.

## CONTRIBUTING hygiene (binding on plugin code)

Multi-pass **follows** [`CONTRIBUTING.md`](../../../CONTRIBUTING.md) (and `OpenRGB/CONTRIBUTING.md` for C++/Qt/logging where it does not conflict). Presets / toolbox content follow the same spirit (no dead aliases, no leftover commented-out bodies, minimal `#` / GLSL noise).

On every touched plugin file (and when scanning related call sites), **apply**:

| Rule (from CONTRIBUTING) | Multipass action |
|--------------------------|------------------|
| No debug or dead code | Delete unused functions/paths; grep call sites before keeping a non-virtual helper |
| No legacy / backward-compat shims | Remove dual loaders, old-key fallbacks, migration branches; do not add new ones |
| Comments minimal — non-obvious intent only | Strip narrating “what” comments; keep seam/contract “why” (once→black, GpuAnimMs, etc.) |
| DRY / KISS / YAGNI / single-responsibility | Prefer delete or reuse over a new parallel path |
| Style: file first, then OpenRGB CONTRIBUTING | 4 spaces, brace style, `LogManager` not `printf`/`QDebug`/`std::cout`, SPDX on new/substantial files |
| RGBController safety | Guard pointers / indices before use (when touching device paths) |
| Engines vs content | No stock looks in the plugin tree; no new `REGISTER_EFFECT_3D` except Game packs |

Pass **2** (correctness) and pass **8** (deslop) both own this cleanup — finding dead code or comment noise is a **fix in the same run**, not a note. When you delete dead paths, mention them in the run report (and MR description if committing) so they are not reintroduced.

## Better, not worse (hard gate)

Every edit must leave the target **net better** than before. “Touched” is not success.

Before landing (or keeping) a change, ask:

1. **Role** — still the same honest job (or a clearer one)? Did we break the tool’s purpose?
2. **Seams** — once→black and forever/`while_active` continuous time still hold? Did we introduce wrap flash, linger, or a jump?
3. **Fit** — callers, packs, docs, AppData still agree? Orphans or half-wired aliases?
4. **Clarity** — easier to read and reason about, or noisier / more special cases?
5. **Structure** — fewer concepts / less spaghetti, or a new bolted-on branch?
6. **Diff cost** — smallest change that fixes the fail? Or churn that mostly rearranges?
7. **Feel / snappy** — does the product still feel usable? Multi-second UI freezes, obvious hot loops on the UI thread, or “every click stalls” count as regressions alongside wrong looks

**Fail the edit** (revert or do not apply) when any of these are true:

- Behaviour regresses (wrong look, broken loop, linger after once, silent dark where lit was correct)
- The file is harder to maintain for the same behaviour
- We traded one bug for another without a clear net win
- Polish / rewrite that does not improve role, seams, fit, or structure
- “Improvements” that only match AI taste (extra comments, renames, drive-by refactors)
- Interaction feel got worse (freezes, stalls, work that belongs off the hot path now runs on every paint/click)

**Keep / no-edit** beats a doubtful change. If unsure, keep the file and say why.

When reporting, prefer: **better** (what improved) / **keep** / **reverted** (tried X, made worse, rolled back).

## Architectural ceiling (strive for best-in-limits)

Aim for the **best path the plugin’s architecture already allows** — canonical helpers, shared pools/contracts, real engines — not a “this will do” shortcut when a stronger option fits inside current limits.

- Prefer the ceiling solution (reuse the shared path, batch the hot work, cache, defer off the critical path) over a weaker parallel design.
- When you hit a **hard ceiling** (documented constraint, driver/API reality, single shared resource), **name it and stop** inventing workarounds that fight the design.
- Do not add a second stack “just to go faster” if the first stack was made singular for stability or correctness.
- Fix one-off bugs in code; do not turn ceilings or incidents into ever-growing skill bans.

## Multi-pass attack surfaces

| Pass | Attack surface |
|------|----------------|
| 1 | **Purpose / role** — fulfills the intended job; name/description/section match behaviour; if a pack **id/name embeds a tool token** (e.g. `strata`), that tool (or rename) must appear in a clip — mismatch fails |
| 2 | **Correctness + dead code** — loads/compiles; headers match body (`shader:` / `particles:` / `ribbons:`); swatch/knobs honest; same-engine sibling tools share one header skeleton (icons/knobs/paint — header-only orphans fail); if `.fx` has `shader:` + `knobs:… pulse`, paired `.fs` must use `param: size` / `u_params[2]`; **remove** unused helpers/paths/debug leftovers (grep call sites) |
| 2b | **Security (plugin-tuned VibeSec)** — when touching C++/Qt, HTTP/listeners, path/file loaders, settings JSON, Lua/VLC helpers, SHM/game bridge, or anything that handles untrusted/external input: apply the checklist below; fix fails in the same run. Skip deep web-only XSS/GraphQL theater on pure `.fx`/`.fs` look files unless they embed executable/script surfaces |
| 3 | **Fit, integration & wiring** — callers, applier, player loop modes, Movie Sync / Triggers packs, Action Editor toolbar, docs, AppData paths; **trace signals/slots and knob↔field links** so they connect and do the right job; if a live-preview dirty flag exists, every authoring mutator that bypasses the form-write path must set it; **every path that empties the action player** (Stop, uncheck, level-off, natural end) must `ClearActionPreview`, not only timer-finish; no orphans, half-wired actions, or “works alone / breaks in stack” |
| 4 | **Seams & bugs** — once→black; forever/`while_active` continuous time (not progress wrap flash); no linger/jump/recycle; clear-on-idle when playback ends by any path; when one field assist shares one ApplyPackFrame sequence across tracks, prepare must still push per-clip params (never treat “same seq” as “same clip”) |
| 5 | **Readability / UX + UI integrity** — sparse-LED survival for GPU looks; clear behaviour for utilities; **labels/tooltips/status/errors read as human UI** (no placeholder junk, raw ids, truncated nonsense); layout usable; broken/empty controls fail. **Plugin UI standards (fail):** (1) ranged effect/settings numbers use **`EffectSliderRow`** (spin over thin slider) or Action Editor–equivalent spin+slider — bare `QSlider`+value `QLabel` pairs fail (EQ vertical bands exempt); (2) every discrete destructive/structural action has **three ways** — mouse control, keyboard path, and `QMenu` / overflow menu; (3) right-click that instantly deletes fails — must open a real menu. Canonical helpers: `PromoteSliderLabelToEffectRow` / `PromoteSliderLabelInGrid` / `SyncEffectSliderValue` (`EffectSliderPromote.h`), `PluginUiWireListDestructiveDelete`. See `.cursor/rules/ui-accessibility-three-ways.mdc` + `ui-slider-spinbox.mdc`. Coverage: `Documentation/multipass-coverage.md` § UI accessibility. |
| 6 | **Character polish** — deepen only if earlier passes left it meh **and** polish still fits the role (do not turn `solid` into aurora) |
| 7 | **Structure (thermo-nuclear)** — when touching engines/UI/C++/large GLSL: code judo, file size, spaghetti, wrong layer |
| 8 | **Deslop + CONTRIBUTING cleanup** — the clean/tidy pass (do **not** invent a separate “tidy” pass); strip AI slop, narrating comments, unused includes, dead/debug/legacy paths; match CONTRIBUTING / file style |
| 9 | **Close the loop** — apply fixes from 1–8 **and 2b**; sync dependents; **tidy sweep on the diff** (comments, dead helpers, unused includes leftovers); **re-check better-not-worse** (incl. snappy/feel) |
| 10 | **Skill self-check + self-improvement** — audit *and* upgrade this brief before ending the run (see below) |

## Security — plugin-tuned VibeSec (pass 2b)

Upstream spirit: [BehiSecc/VibeSec-Skill](https://github.com/BehiSecc/VibeSec-Skill) (vendored at `.cursor/skills/vibesec/`). **Defense in depth, fail closed, least privilege, never trust external input.** Do not paste the full web encyclopedia into every toolbox pass — use this OpenRGB3D lens:

| Threat (this codebase) | Multipass action |
|------------------------|------------------|
| **Secrets in tree** | No API keys, tokens, passwords in sources, presets JSON, Lua, logs, or UI copy; strip if found |
| **Path traversal / unsafe file IO** | Catalog loaders, FolderVolume/ShaderField disk scan, mesh import, PluginSettingsPaths — reject `..`, absolute escapes outside plugin data roots; fail closed on bad paths |
| **Movie Sync HTTP** | Stay loopback-bound; no bind-all interfaces; cap request size; do not fetch arbitrary user URLs (SSRF); validate paths/ids from requests before play |
| **Injection** | No shelling user-controlled paths; no concatenating untrusted strings into commands/SQL; Lua/VLC helpers treat filenames as data |
| **Settings / JSON mass-assignment** | `LoadSettings` / pack JSON — only known keys; ignore/drop unknown privilege-like fields; clamp ranges |
| **JSON / text size ceilings** | Every loader of untrusted or user/AppData JSON or text (action packs, trigger bindings, movie-sync catalog, FolderVolume/ShaderField specs, `.fs` bodies, settings blobs) must enforce a hard byte ceiling and **fail closed** when over — reuse shared constants when they exist (e.g. `Action::kMaxPackJsonBytes`, shader/spec 512 KiB). Unbounded `readAll` / `ifstream` on those paths is a fail |
| **RGBController / device safety** | Guard pointers/indices (CONTRIBUTING); never write past zone/LED bounds from pack or network input |
| **SHM / game bridge** | Treat SHM frames as untrusted layout; validate sizes/magic/seq before use; no trusting mod-supplied paths blindly |
| **Authn/session (where present)** | Triggers/Manual fire stay local UX; do not invent remote admin without auth; token/session revoke if any remote control is added later |
| **Error leakage** | User-visible errors stay human; no stack traces / internal paths in UI labels |

When a change is security-sensitive **and** the user wants a second opinion on the diff: offer **Bugbot** (`/review-bugbot`) or **security-review** — do not auto-spawn them on every multipass.

## Positive / negative prompts

Use these as the quality brief for **create and fix** — same weight on a new Lua helper or `.fx` as on a bugfix.

**Keep:** honest role, wired knobs, matches callers/docs, continuous `u_time` / `time_ms` for ambient motion, once dies clean, fat kernels / hot heads where the look needs them, direct boring maintainable code, reuse of canonical helpers (`ClipSpanProgress`, `ClipSweepProgress`, `GpuAnimMsOr`, field Prepare paths), net-better diffs only, best-in-limits solutions that respect shared architecture, human-readable UI copy, live connects that actually drive behaviour, fail-closed I/O and network edges.

**Kill:** wrong job for the name, misleading aliases, thin LED misses, orange wash, progress-wrap flash, linger, docs drift, unused knobs, silent fail, over-polishing a utility into a showcase, thin wrappers, bolted-on special cases, unjustified 1k-line growth, feature logic leaking into shared paths, narrating comments, dead/debug code, legacy/compat shims, abnormal defensive try/catch on trusted paths, `any` casts to silence types, deep nesting that wants early returns, `printf`/`QDebug`/`std::cout` in plugin paths, “good enough” when a stronger in-limits path exists, parallel stacks that fight a documented singular design, “findings only” when the fix was obvious and unblocked, net-worse or taste-only rewrites, **dead/`connect` with no slot, missing clears, orphan actions, garbage UI strings, layout that fights the control**, secrets in tree, path escape outside plugin data roots, bind-all HTTP, SSRF via user URLs, unbounded SHM trust, unbounded JSON/text `readAll` without a size ceiling.

## Draft → refine (seed loop)

First landing of new or fixed work is a **draft seed**, not “done.”

1. **Draft** — smallest honest implementation that fits the role and architectural ceiling.
2. **Multipass the seed** — run keep/kill + applicable passes on those same files in the same session.
3. **Refine** — apply fails; optional second pass on the same seed when seams/wiring/UI still wobble.
4. **Commit** only after better-not-worse holds on the refined seed.

Same loop for bugs: fix → multipass the fix → refine → commit. Do not treat “it compiles / it plays once” as the finish line when keep/kill would still fail.

## Thermo-nuclear pressure (when code is in scope)

Borrow these standards from thermo-nuclear review; apply them to plugin engines, applier, Movie Sync/Triggers runtimes, and Action UI — not as an excuse to rewrite a fine `.fx` utility.

1. **Be ambitious about structural simplification (code judo).** Prefer reframes that delete branches, dual paths, or whole helper layers while preserving behaviour. If CPU/GPU dual or “silent fallback” still exists for a true-GPU tool, that is a design smell.
2. **File size without explosion.** Do not push a file from under ~1k lines to over ~1k without a strong reason — extract helpers / split mode bodies when needed. Also: **clean, browsable file/folder/code — no explosion.** Same-class multi-`.cpp` is fine (Tab_* style); cap about **+1 file per concern** (UI vs tick, platform capture vs analysis). Slightly over ~1k beats a folder of tiny shards. **Apply splits only where the pass confirms a natural seam** (a file you would open by name). Never Gpu/Sample/Spectrum micro-files “because 1k”. Prefer merging weak fragments back when a pass finds them. (Do not confuse with **pass 2b** security.)
3. **No spaghetti growth.** Suspicious of new ad-hoc conditionals in applier/player/tick paths. Prefer a dedicated mode/helper over one more `if` in a busy function.
4. **Bias toward cleaning the design, not just accepting working code.** Behaviour-correct but messier is a fail on pass 7.
5. **Prefer direct, boring, maintainable code over hacky or magical code.** Fail-closed GPU > fake CPU success; explicit contracts > hidden globals (except documented clocks like `SetGpuAnimMs`).
6. **Keep logic in the canonical layer.** Field time/progress belongs in Prepare/Clip helpers; pack loop policy in `MapPlaybackTime` / Player; do not re-implement wrap math in every `.fx`.
7. **Reuse existing helpers.** Prefer `ClipSpanProgress`, `ClipSweepProgress`, `GpuAnimMsOr`, `volume_field` / `particle_field` / `ribbon_field` Prepare, catalog loaders — do not fork near-duplicates.

### Flag aggressively

- Complicated dual path where one true path would delete a category of bugs
- Refactors that move complexity without reducing concepts
- File crossing ~1000 lines when modes could split **into a named seam** (not into crumbs)
- Aggressive same-class shards that force hopping three files for one edit
- Special-case branches bolted onto applier / tick / preview
- Feature-specific Movie Sync logic leaking into shared Action apply
- Thin wrappers / identity abstractions
- Copy-pasted GLSL or script snippets instead of shared helpers
- “Temporary” branch that will become permanent debt

### Preferred remedies

- Delete a layer of indirection rather than polish it
- One continuous anim clock / one span/sweep progress helper used everywhere
- Extract particle/ribbon mode bodies or Prepare helpers
- Move tool-specific look into toolbox `.fs` / `.fx`; keep engines generic
- Collapse duplicate seam fixes into one place (`GpuAnimMs`, `ClearActionPreview`)
- Merge over-split same-class `.cpp` fragments when the seam is not load-bearing

### Structure review questions

- Is there a code-judo move that makes this dramatically simpler?
- Is this logic in the right file/layer (engine vs toolbox vs pack vs UI)?
- Did we enlarge a busy file past a healthy size?
- Did the pass confirm a **natural seam**, or are we splitting for the line count?
- After the change, is the tree still **clean and browsable** (no file/folder explosion)?
- Would a split create a file you open by name, or just chase the line budget?
- Are we duplicating a canonical helper?
- Does this make once/loop seams harder or easier to reason about?

## Deslop + CONTRIBUTING cleanup (pass 8)

From the deslop skill **and** this repo’s `CONTRIBUTING.md` Code quality / Style sections. Run on the diff / touched files after purpose and structure passes (or whenever the user says deslop). **This is the clean-and-tidy pass** — do not add a separate numbered “tidy” attack surface; sharpen this one instead. Comments, dead code, and leftover noise are in scope every run that touches those files.

### Focus

- Extra comments that are unnecessary or inconsistent with local style (narrating “what” instead of non-obvious “why”) — CONTRIBUTING: *comments should be minimal and explain non-obvious intent only*
- Dead / unused code: helpers with no call sites, commented-out blocks, `#if 0`, leftover debug prints, unused batch paths
- Unused includes / forward decls left behind after a delete or move
- Legacy / backward-compat shims (`legacy`, `migration`, `compat`, old-key fallbacks) — remove; do not add
- Defensive checks or try/catch that are abnormal for trusted plugin paths
- Casts / type bypasses used only to silence the compiler
- Deeply nested code that should be early returns / flatter control flow
- Logging style: `LogManager`, not `printf` / `std::cout` / `QDebug`
- Other patterns inconsistent with the file, `CONTRIBUTING.md`, and surrounding codebase (GLSL, `.fx`, Qt/C++)

### Guardrails

- Keep behaviour unchanged unless fixing a clear bug
- Prefer minimal, focused edits over broad rewrites
- Do not strip useful seam/role comments that document once→black, GpuAnimMs, or other non-obvious contracts
- Grep before deleting a helper that might be used via macros / Qt slots / function pointers
- Keep the final deslop summary concise (1–3 sentences) when deslop is the only ask

## Close the loop (pass 9)

After applying fixes from passes 1–8:

1. Sync dependents (catalogs, AppData, docs, call sites) so nothing is half-wired.
2. **Tidy sweep on the diff** — re-read what you just changed for narrating comments, dead helpers, unused includes, and other pass-8 leftovers that slipped in while fixing seams/bugs. Same muscles as pass 8; this is the “don’t leave the kitchen messy” step.
3. **Better-not-worse** again, including snappy/feel — revert net-worse edits.

## Skill self-check + self-improvement (pass 10 — end of every run)

Mandatory close-out. Ask: **how can this tool do what it already does — better?** Not “what new special case should we memorize?”

**Self-check** = did the existing passes (purpose, correctness, fit, seams, polish, structure, deslop/CONTRIBUTING, close-the-loop, better-not-worse) run cleanly?  
**Self-improvement** = propose sharpening those same muscles (clearer deslop, tighter tidy, stronger bug/seam weight) when the *process* felt weak.

### Default: propose, do not edit

- Report **skill: keep** or **skill: proposed** (one short idea).
- **Do not edit this `SKILL.md` unless the user explicitly says to add/apply it.** Avoid silent bloat.
- Never append file-specific or incident-specific bans. Fix those in code.

### What a good proposal looks like

- Sharpen an **existing** pass (deslop, better-not-worse, close-the-loop, architectural ceiling) so every file benefits
- Shorter, clearer how-to-run if the agent wasted motion
- Delete weak/duplicate guidance when adding clarity

### What a bad proposal looks like

- A new numbered “clean & tidy” pass that duplicates pass 8/9
- Naming one API, one file, or one pack from this chat
- Longer skill text without deleting something weaker

### Steps

1. Briefly revisit whether existing gates were enough for this run.
2. If a **cross-cutting process** tweak would help every future file, put it in the report as **skill: proposed** — wait for user OK before editing.
3. If nothing process-level stood out: **skill: keep**.

### Ratchet rule

Codebase gets better every run. The skill grows only when **you** approve a general “do the same job better” change.

### Ratchet re-check (after a skill gate lands)

When this brief gains a real gate (ceiling, snappy, wiring, UI integrity, …), **do not** full-redo every historical “keep.” Preferentially re-apply **that new gate** on:

- Recently touched / high-traffic surfaces (Action Editor, timeline, preview, applier)
- Anything previously accepted as “good enough” that the new gate would reject
- Files already in the current batch

Pure toolbox looks that already passed purpose/seams/fit stay keep unless the batch is in that folder anyway.

## How to run

1. List target files (user batch, new draft seed, or next unfinished toolbox / engine section). Prefer ratchet re-check targets when a gate just landed.
2. Walk passes 1–6 on content; add **pass 2b (security)** when C++/network/paths/JSON/Lua/SHM are in scope; add pass 7 when C++/engine/UI is involved; add pass 8 (deslop) on touched code. On UI/C++: **trace wiring (pass 3)** and **UI integrity (pass 5)**. Hold the keep/kill brief in mind the whole time.
3. Note **keep** / **fix** / **deepen** / **judo** / **deslop** per file.
4. **Apply** fixes for every fail finding; leave keep findings alone.
5. **Pass 9:** sync dependents; tidy sweep on the diff; better-not-worse (incl. snappy/feel). If this was a new draft, treat the result as a seed and **re-walk** any weak passes once more before commit.
6. **Pass 10:** skill self-check; **propose** process upgrades only (edit skill only if user approved).
7. Report a short table: file → pass results → **what changed** (or keep / reverted); plus **skill: keep / proposed**.
8. Sync AppData when presets change; rebuild DLL when C++/embedded GLSL changes.

## Output

Prioritize:

1. Purpose/role failures and integration breaks → **fixed** (only if net better)
2. Security fails (pass 2b) and seam/bug failures (jump, linger, wrap flash) → **fixed**
3. Structural / code-judo opportunities (when code is in scope) → **applied when clear**
4. Deslop on the same touched set
5. Readability / sparse-LED
6. Character polish that still fits the role
7. Skill self-check (pass 10) — propose process upgrades; edit only if user OK’d

Do not flood with cosmetic nits when larger issues exist. Prefer fewer high-conviction actions that actually land in the tree. Prefer **keep** or **revert** over a doubtful “improvement.”

## Reference

- Plugin brief: `Documentation/action-engine-recipe-map.md` (Catch-all multi-pass)
- **Contributing (binding):** `CONTRIBUTING.md` — Code quality, Style, no legacy paths; plus `OpenRGB/CONTRIBUTING.md` for C++/Qt/logging
- Upstream spirit: cursor-team-kit `thermo-nuclear-code-quality-review` and `deslop` skills
- Security / review siblings (still standalone): `.cursor/skills/vibesec/` ← [BehiSecc/VibeSec-Skill](https://github.com/BehiSecc/VibeSec-Skill); Bugbot via `review-bugbot` — multipass lightens load, does not replace them
- Canonical clocks/progress: `GpuAnimMsOr`, `ClipSpanProgress`, `ClipSweepProgress`, `ClipPeriodProgress`
