# Effect System Lifecycle & Cleanup

This document describes how the effect stack and frequency-range effects are added, removed, and cleared so the grid/overlay and LEDs stay in sync and don't show stale content.

## Stack effects (main layers)

- **Add**: Effect is appended to `effect_stack`. Each `EffectInstance3D` has its own `effect` (unique_ptr). No shared state between two instances of the same effect class.
- **Remove**: 
  - If the removed layer was selected, `LoadStackEffectControls(nullptr)` runs first so the config panel is cleared and all signals from that effect’s UI are disconnected (avoids use-after-free).
  - The instance is erased from `effect_stack`.
  - If the stack becomes empty, `on_stop_effect_clicked()` is called (stops timer, sets `effect_running = false`, and calls `RenderEffectStack()` to clear LEDs and overlay).
  - **Always** call `RenderEffectStack()` after a remove so the grid/overlay either re-renders with the remaining effects or runs the empty path and clears.
- **Stop**: `on_stop_effect_clicked()` sets `effect_running = false`, stops the effect timer, and calls `RenderEffectStack()`. The empty path in `RenderEffectStack()` clears all controller colors and the room grid overlay (black buffer + `ClearRoomGridOverlayBounds()`).

## Frequency range effects

- **Remove**: The range is erased from `frequency_ranges` (its `effect_instance` is destroyed). `RenderEffectStack()` is called so the overlay/LEDs update immediately and the removed range no longer contributes.

## Empty path in RenderEffectStack()

When `active_effects.empty() && !has_enabled_freq_ranges`:

1. All controller LED colors are set to black and `UpdateLEDs()` is called.
2. `viewport->UpdateColors()` and `viewport->ClearRoomGridOverlayBounds()` are called.
3. The room grid overlay is filled with black and set via `SetRoomGridColorBuffer(black_grid)`.

So after “Stop” or after removing the last layer (and having no freq ranges), the grid and overlay are cleared.

## Two of the same effect (no shared state)

- Each stack layer has its own `EffectInstance3D` and its own `instance->effect` (created with `EffectListManager3D::CreateEffect()`).
- The config panel uses a separate “UI” effect (`current_effect_ui`) created when you select a layer; parameter changes are synced to that layer’s `instance->effect` only.
- When switching selection, `LoadStackEffectControls(nullptr)` disconnects the previous UI effect from the tab, then the new layer’s UI is created. So there are no duplicate signal connections.
- Effects that use `AudioInputManager::instance()` all share that singleton; it is thread-safe and read-only from the effect side, so multiple effects of the same type can run without locking up.

If you see a lockup with two of the same effect, check for:

- Blocking or long-running work in that effect’s `CalculateColor` / `CalculateColorGrid`.
- Any effect-specific static or global state that could conflict when two instances exist.

## Checklist for future changes

- After removing a stack layer, always call `RenderEffectStack()` (do not guard with `effect_running` only).
- After removing a frequency range, call `RenderEffectStack()` so the grid updates immediately.
- When clearing the config panel (`LoadStackEffectControls(nullptr)`), disconnect the current effect UI from the tab before destroying it so no signals fire after the instance is removed from the stack.
- The empty path in `RenderEffectStack()` must clear both controller colors and the room grid overlay so “Stop” and “remove last layer” leave the grid black.
