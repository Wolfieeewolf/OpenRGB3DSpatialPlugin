# 3D Grid SDK (v1)

This SDK adds spatial information on top of the OpenRGB SDK. It reuses OpenRGB controller and LED indices and exposes world positions in millimeters.

## Discovery

Same-process only. Retrieve a function table pointer from the Qt application property `OpenRGB3DSpatialGridAPI`.

```cpp
const ORGB3DGridAPI* api = nullptr;
QVariant v = qApp->property("OpenRGB3DSpatialGridAPI");
if (v.isValid()) api = reinterpret_cast<const ORGB3DGridAPI*>((uintptr_t)v.value<qulonglong>());
if (!api || api->api_version < 1) return;
```

## Struct

```c
struct ORGB3DGridAPI {
  int    api_version; // 1

  // Grid and room
  float  (*GetGridScaleMM)();
  void   (*GetRoomDimensions)(float* width_mm, float* depth_mm, float* height_mm, bool* use_manual);

  // Controllers
  size_t (*GetControllerCount)();
  bool   (*GetControllerName)(size_t ctrl_idx, char* name_utf8, size_t buf_size);
  bool   (*IsControllerVirtual)(size_t ctrl_idx);
  int    (*GetControllerGranularity)(size_t ctrl_idx); // -1 virtual, 0 device, 1 zone, 2 led
  int    (*GetControllerItemIndex)(size_t ctrl_idx);

  // LEDs (in current layout snapshot)
  size_t (*GetLEDCount)(size_t ctrl_idx);
  bool   (*GetLEDWorldPosition)(size_t ctrl_idx, size_t led_idx, float* x, float* y, float* z);
  bool   (*GetLEDWorldPositions)(size_t ctrl_idx, float* xyz_interleaved, size_t max_triplets, size_t* out_count);

  // Aggregate helpers
  size_t (*GetTotalLEDCount)();
  bool   (*GetAllLEDWorldPositions)(float* xyz_interleaved, size_t max_triplets, size_t* out_count);
  bool   (*GetAllLEDWorldPositionsWithOffsets)(float* xyz_interleaved, size_t max_triplets, size_t* out_triplets,
                                                size_t* ctrl_offsets, size_t offsets_capacity, size_t* out_controllers);

  // Change notification (optional): called when transforms/layout change
  bool   (*RegisterGridLayoutCallback)(void (*cb)(void*), void* user);
  bool   (*UnregisterGridLayoutCallback)(void (*cb)(void*), void* user);

  // Write paths
  bool   (*SetControllerColors)(size_t ctrl_idx, const unsigned int* bgr_colors, size_t count);
  bool   (*SetSingleLEDColor)(size_t ctrl_idx, size_t led_idx, unsigned int bgr_color);
  bool   (*SetGridOrderColors)(const unsigned int* bgr_colors_by_grid, size_t count);
  bool   (*SetGridOrderColorsWithOrder)(int order, const unsigned int* bgr_colors_by_grid, size_t count);
};
```

- Coordinates: X=left?right, Y=front?back, Z=floor?ceiling, millimeters.
- Colors: 0x00BBGGRR (OpenRGB standard).

## Minimal examples

Read positions for one controller:
```cpp
size_t n = api->GetLEDCount(c);
std::vector<float> xyz(n*3);
size_t got = 0;
api->GetLEDWorldPositions(c, xyz.data(), n, &got);
```

Read all positions in one call:
```cpp
size_t total = api->GetTotalLEDCount();
std::vector<float> xyz(total*3);
size_t out = 0;
api->GetAllLEDWorldPositions(xyz.data(), total, &out);
```

Listen for layout changes:
```cpp
api->RegisterGridLayoutCallback([](void* u){ /* refresh caches */ }, nullptr);
```

Set colors on a controller (OpenRGB LED order):
```cpp
std::vector<unsigned int> colors(api->GetLEDCount(c), 0x0000FF00); // green
api->SetControllerColors(c, colors.data(), colors.size());
```

Get controller metadata:
```cpp
char name[256];
api->GetControllerName(c, name, sizeof(name));
bool is_virtual = api->IsControllerVirtual(c);
int granularity = api->GetControllerGranularity(c); // -1=virtual, 0=device, 1=zone, 2=led
int item_index = api->GetControllerItemIndex(c);
```

Set all LEDs in grid order (concatenated by controller):
```cpp
size_t total = api->GetTotalLEDCount();
std::vector<unsigned int> colors(total, 0x00FF0000); // red
api->SetGridOrderColors(colors.data(), colors.size());
```

## Versioning

Append-only. New fields are added to the end and `api_version` is bumped.

## Relationship to OpenRGB SDK

Continue to use the OpenRGB SDK for device enumeration and sending color frames over network if needed. Use this 3D SDK for spatial queries and convenient in-process color writes.
