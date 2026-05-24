## Pull request

<!-- Thank you for contributing! Link an issue when applicable: Fixes #123 -->

If you already know the fix (bug, feature, game bridge, or preset JSON), a PR is better than an issue-only report.

### Type

- [ ] Bug fix
- [ ] Feature / enhancement
- [ ] Game integration
- [ ] Documentation only
- [ ] Refactor / maintenance (no behavior change)

### Summary

<!-- What does this PR do and why? -->

### Issue

<!-- Link: Fixes #… or Relates to #… -->

### Testing

| | |
|---|---|
| **OpenRGB + Qt** | <!-- e.g. Windows 11, OpenRGB 0.9, Qt 5.15 MSVC --> |
| **What you tested** | <!-- steps or areas --> |

- [ ] Built locally (`qmake` + `nmake` / `make`) with OpenRGB submodule initialized
- [ ] Plugin loads in OpenRGB and **3D Spatial** tab opens
- [ ] Tested the changed behavior (describe above)
- [ ] No `QDebug` / `printf` — uses `LogManager` where logging was added
- [ ] No custom `setStyleSheet` / theme overrides on structural UI chrome (see CONTRIBUTING.md)

### Scope

- [ ] Changes are only in plugin-owned paths (not `OpenRGB/` subtree)
- [ ] Touched `RGBController` / zones / LED indices — guards and bounds checked (see CONTRIBUTING.md)
- [ ] Effect changes use `EvaluateColorGrid` from the stack where appropriate

### Screenshots / recordings (optional)

<!-- Viewport, effect stack, or settings UI -->

### Notes for reviewers

<!-- Breaking changes, follow-ups, preset repo updates needed, etc. -->
