# Example portable presets

These files follow [docs/controller-preset-format.md](../../docs/controller-preset-format.md).

| File | OpenRGB device (`controller_name`) | Layout `name` |
| ---- | ------------------------------------ | --------------- |
| `amd_wraith_prism.json` | AMD Wraith Prism | AMD Wraith Prism |
| `intel_arc_a770_limited_edition.json` | Intel Arc A770 Limited Edition | Intel Arc A770 Limited Edition |

For multi-layout presets on one device (e.g. three case-fan positions on one Corsair hub), use a short layout label for `name` but keep the real `controller_name` in every mapping. See the format doc.
