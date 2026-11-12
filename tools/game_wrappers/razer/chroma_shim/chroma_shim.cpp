// SPDX-License-Identifier: GPL-2.0-only
// Universal Razer/Colore Chroma SDK proxy → GW01 DeviceGrid UDP + capture log.
//
// Chroma create/set semantics (see FinishCreate):
//   Create*(…, pEffectId=null)  → apply immediately
//   Create*(…, pEffectId!=null) → store + apply (titles that skip SetEffect still light)
//   SetEffect(id)               → re-apply stored frame (animation)

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#endif

#include "../../common/gw_udp_emit.h"
#include "../../common/gw_capture_log.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

using RZRESULT = int;
using RZDEVICEID = GUID;
using RZEFFECTID = GUID;
using PRZPARAM = void*;

constexpr RZRESULT RZRESULT_SUCCESS = 0;
constexpr RZRESULT RZRESULT_INVALID_PARAMETER = -1;

namespace ChromaSDK
{
struct APPINFOTYPE
{
    char Title[256];
    char Description[256];
    char Author[256];
    int SupportedDevice;
    int Category;
};

enum EFFECT_TYPE
{
    CHROMA_NONE = 0,
    CHROMA_STATIC = 4,
    CHROMA_CUSTOM = 7,
};

namespace Keyboard
{
enum EFFECT_TYPE
{
    CHROMA_NONE = 0,
    CHROMA_BREATHING = 1,
    CHROMA_CUSTOM = 2,
    CHROMA_REACTIVE = 3,
    CHROMA_STATIC = 4,
    CHROMA_SPECTRUMCYCLING = 5,
    CHROMA_WAVE = 6,
    CHROMA_RESERVED = 7,
    CHROMA_CUSTOM_KEY = 8,
    CHROMA_CUSTOM2 = 9,
    CHROMA_INVALID = 10,
};
constexpr int MAX_ROW = 6;
constexpr int MAX_COLUMN = 22;
/** Key colour valid bit in CUSTOM_KEY Key[] cells (RzChromaSDKTypes.h). */
constexpr std::uint32_t COLOR_VALID = 0x01000000u;
struct STATIC_EFFECT_TYPE
{
    std::uint32_t Color; /* official SDK: single COLORREF */
};
struct BREATHING_EFFECT_TYPE
{
    enum Type
    {
        TWO_COLORS = 1,
        RANDOM_COLORS = 2
    };
    Type Type;
    std::uint32_t Color1;
    std::uint32_t Color2;
};
struct REACTIVE_EFFECT_TYPE
{
    enum Duration
    {
        DURATION_NONE = 0,
        DURATION_SHORT = 1,
        DURATION_MEDIUM = 2,
        DURATION_LONG = 3
    };
    Duration Duration;
    std::uint32_t Color;
};
struct STARLIGHT_EFFECT_TYPE
{
    enum Type
    {
        TWO_COLORS = 1,
        RANDOM_COLORS = 2
    };
    Type Type;
    std::uint32_t Color1;
    std::uint32_t Color2;
    enum Duration
    {
        DURATION_SHORT = 1,
        DURATION_MEDIUM = 2,
        DURATION_LONG = 3
    };
    Duration Duration;
};
struct WAVE_EFFECT_TYPE
{
    enum Direction
    {
        DIRECTION_NONE = 0,
        DIRECTION_LEFT_TO_RIGHT = 1,
        DIRECTION_RIGHT_TO_LEFT = 2
    };
    Direction Direction;
};
struct CUSTOM_EFFECT_TYPE
{
    std::uint32_t Color[MAX_ROW][MAX_COLUMN];
};
struct CUSTOM_KEY_EFFECT_TYPE
{
    std::uint32_t Color[MAX_ROW][MAX_COLUMN];
    std::uint32_t Key[MAX_ROW][MAX_COLUMN];
};
} // namespace Keyboard

namespace Mouse
{
constexpr int MAX_LEDS = 30;
constexpr int MAX_ROW = 9;
constexpr int MAX_COLUMN = 7;
enum EFFECT_TYPE
{
    CHROMA_NONE = 0,
    CHROMA_BLINKING = 1,
    CHROMA_BREATHING = 2,
    CHROMA_CUSTOM = 3,
    CHROMA_REACTIVE = 4,
    CHROMA_SPECTRUMCYCLING = 5,
    CHROMA_STATIC = 6,
    CHROMA_WAVE = 7,
    CHROMA_CUSTOM2 = 8,
};
struct STATIC_EFFECT_TYPE
{
    std::uint32_t LEDId;
    std::uint32_t Color;
};
struct BREATHING_EFFECT_TYPE
{
    std::uint32_t LEDId;
    enum Type
    {
        ONE_COLOR = 1,
        TWO_COLORS = 2,
        RANDOM_COLORS = 3
    };
    Type Type;
    std::uint32_t Color1;
    std::uint32_t Color2;
};
struct CUSTOM_EFFECT_TYPE
{
    std::uint32_t Color[MAX_LEDS];
};
struct CUSTOM2_EFFECT_TYPE
{
    std::uint32_t Color[MAX_ROW][MAX_COLUMN];
};
} // namespace Mouse

namespace Mousepad
{
constexpr int MAX_LEDS = 15;
enum EFFECT_TYPE
{
    CHROMA_NONE = 0,
    CHROMA_BREATHING = 1,
    CHROMA_CUSTOM = 2,
    CHROMA_SPECTRUMCYCLING = 3,
    CHROMA_STATIC = 4,
    CHROMA_WAVE = 5,
};
struct STATIC_EFFECT_TYPE
{
    std::uint32_t Color;
};
struct BREATHING_EFFECT_TYPE
{
    enum Type
    {
        TWO_COLORS = 1,
        RANDOM_COLORS = 2
    };
    Type Type;
    std::uint32_t Color1;
    std::uint32_t Color2;
};
struct WAVE_EFFECT_TYPE
{
    enum Direction
    {
        DIRECTION_NONE = 0,
        DIRECTION_LEFT_TO_RIGHT = 1,
        DIRECTION_RIGHT_TO_LEFT = 2
    };
    Direction Direction;
};
struct CUSTOM_EFFECT_TYPE
{
    std::uint32_t Color[MAX_LEDS];
};
} // namespace Mousepad

namespace Headset
{
constexpr int MAX_LEDS = 5;
enum EFFECT_TYPE
{
    CHROMA_NONE = 0,
    CHROMA_STATIC = 1,
    CHROMA_BREATHING = 2,
    CHROMA_SPECTRUMCYCLING = 3,
    CHROMA_CUSTOM = 4,
};
struct STATIC_EFFECT_TYPE
{
    std::uint32_t Color;
};
struct BREATHING_EFFECT_TYPE
{
    enum Type
    {
        TWO_COLORS = 1,
        RANDOM_COLORS = 2
    };
    Type Type;
    std::uint32_t Color1;
    std::uint32_t Color2;
};
struct CUSTOM_EFFECT_TYPE
{
    std::uint32_t Color[MAX_LEDS];
};
} // namespace Headset

namespace Keypad
{
constexpr int MAX_ROW = 4;
constexpr int MAX_COLUMN = 5;
enum EFFECT_TYPE
{
    CHROMA_NONE = 0,
    CHROMA_BREATHING = 1,
    CHROMA_CUSTOM = 2,
    CHROMA_REACTIVE = 3,
    CHROMA_SPECTRUMCYCLING = 4,
    CHROMA_STATIC = 5,
    CHROMA_WAVE = 6,
};
struct STATIC_EFFECT_TYPE
{
    std::uint32_t Color;
};
struct BREATHING_EFFECT_TYPE
{
    enum Type
    {
        TWO_COLORS = 1,
        RANDOM_COLORS = 2
    };
    Type Type;
    std::uint32_t Color1;
    std::uint32_t Color2;
};
struct REACTIVE_EFFECT_TYPE
{
    enum Duration
    {
        DURATION_NONE = 0,
        DURATION_SHORT = 1,
        DURATION_MEDIUM = 2,
        DURATION_LONG = 3
    };
    Duration Duration;
    std::uint32_t Color;
};
struct WAVE_EFFECT_TYPE
{
    enum Direction
    {
        DIRECTION_NONE = 0,
        DIRECTION_LEFT_TO_RIGHT = 1,
        DIRECTION_RIGHT_TO_LEFT = 2
    };
    Direction Direction;
};
struct CUSTOM_EFFECT_TYPE
{
    std::uint32_t Color[MAX_ROW][MAX_COLUMN];
};
} // namespace Keypad

namespace ChromaLink
{
constexpr int MAX_LEDS = 5;
enum EFFECT_TYPE
{
    CHROMA_NONE = 0,
    CHROMA_CUSTOM = 1,
    CHROMA_STATIC = 2,
};
struct STATIC_EFFECT_TYPE
{
    std::uint32_t Color;
};
struct CUSTOM_EFFECT_TYPE
{
    std::uint32_t Color[MAX_LEDS];
};
} // namespace ChromaLink
} // namespace ChromaSDK

namespace
{
bool g_initialized = false;
char g_source[24] = "chroma_shim";

struct StoredEffect
{
    GwUdp::DeviceId device = GwUdp::DeviceId::Keyboard;
    int effect = 0;
    int rows = 0;
    int cols = 0;
    std::vector<std::uint32_t> colors;
};

struct GuidLess
{
    bool operator()(const GUID& a, const GUID& b) const
    {
        return std::memcmp(&a, &b, sizeof(GUID)) < 0;
    }
};

std::mutex g_fx_mutex;
std::map<GUID, StoredEffect, GuidLess> g_effects;

static void InitSourceName()
{
    wchar_t path_w[MAX_PATH]{};
    if(GetModuleFileNameW(nullptr, path_w, MAX_PATH) == 0)
    {
        return;
    }
    const wchar_t* base = path_w;
    for(const wchar_t* p = path_w; *p; ++p)
    {
        if(*p == L'\\' || *p == L'/')
        {
            base = p + 1;
        }
    }
    char utf8[MAX_PATH]{};
    if(WideCharToMultiByte(CP_UTF8, 0, base, -1, utf8, (int)sizeof(utf8), nullptr, nullptr) <= 0)
    {
        return;
    }
    std::memset(g_source, 0, sizeof(g_source));
    std::strncpy(g_source, utf8, sizeof(g_source) - 1);
}

static const char* Source()
{
    return g_source;
}

static const char* DeviceName(GwUdp::DeviceId id)
{
    switch(id)
    {
        case GwUdp::DeviceId::Keyboard:
            return "Keyboard";
        case GwUdp::DeviceId::Mouse:
            return "Mouse";
        case GwUdp::DeviceId::Mousepad:
            return "Mousepad";
        case GwUdp::DeviceId::Headset:
            return "Headset";
        case GwUdp::DeviceId::Keypad:
            return "Keypad";
        case GwUdp::DeviceId::ChromaLink:
            return "ChromaLink";
        default:
            return "Unknown";
    }
}

static StoredEffect MakeEffect(GwUdp::DeviceId device,
                               int effect,
                               const std::uint32_t* colors,
                               int rows,
                               int cols)
{
    StoredEffect out;
    out.device = device;
    out.effect = effect;
    out.rows = rows;
    out.cols = cols;
    const int n = rows * cols;
    out.colors.resize(n > 0 ? (size_t)n : 0);
    if(n > 0 && colors)
    {
        std::memcpy(out.colors.data(), colors, (size_t)n * sizeof(std::uint32_t));
    }
    return out;
}

static StoredEffect MakeSolid(GwUdp::DeviceId device, int effect, std::uint32_t color)
{
    return MakeEffect(device, effect, &color, 1, 1);
}

static void ApplyEffect(const StoredEffect& fx)
{
    if(fx.rows <= 0 || fx.cols <= 0 || fx.colors.empty())
    {
        return;
    }
    GwCapture::LogGrid(DeviceName(fx.device), fx.effect, fx.colors.data(), fx.rows, fx.cols);
    /* Auto-flush ~15 ms — force-flush every SetEffect drops under animation storms. */
    GwUdp::SetDeviceGridColorRef(fx.device, fx.colors.data(), fx.rows, fx.cols, Source());
}

static StoredEffect MakeKeyboardCustomKey(const ChromaSDK::Keyboard::CUSTOM_KEY_EFFECT_TYPE* eff, int effect)
{
    /* Color[] is the base matrix; Key[] overlays cells marked COLOR_VALID. */
    std::uint32_t merged[ChromaSDK::Keyboard::MAX_ROW * ChromaSDK::Keyboard::MAX_COLUMN];
    for(int row = 0; row < ChromaSDK::Keyboard::MAX_ROW; ++row)
    {
        for(int col = 0; col < ChromaSDK::Keyboard::MAX_COLUMN; ++col)
        {
            const std::uint32_t key = eff->Key[row][col];
            const std::uint32_t base = eff->Color[row][col];
            merged[row * ChromaSDK::Keyboard::MAX_COLUMN + col] =
                (key & ChromaSDK::Keyboard::COLOR_VALID) ? (key & 0x00FFFFFFu) : base;
        }
    }
    return MakeEffect(GwUdp::DeviceId::Keyboard,
                      effect,
                      merged,
                      ChromaSDK::Keyboard::MAX_ROW,
                      ChromaSDK::Keyboard::MAX_COLUMN);
}

/* null id → apply now; non-null → store (+ apply). SetEffect re-applies. */
static RZRESULT FinishCreate(RZEFFECTID* pEffectId, StoredEffect&& fx)
{
    GwCapture::NoteCreate(DeviceName(fx.device), fx.effect, pEffectId != nullptr);
    if(!pEffectId)
    {
        ApplyEffect(fx);
        return RZRESULT_SUCCESS;
    }

    GUID id = *pEffectId;
    if(IsEqualGUID(id, GUID_NULL))
    {
        if(FAILED(CoCreateGuid(&id)))
        {
            ApplyEffect(fx);
            return RZRESULT_SUCCESS;
        }
        *pEffectId = id;
    }

    {
        std::lock_guard<std::mutex> lock(g_fx_mutex);
        g_effects[id] = fx;
    }
    ApplyEffect(fx);
    return RZRESULT_SUCCESS;
}

/** Best-effort class for CreateEffect(RZDEVICEID) — product GUIDs from RzChromaSDKDefines.h. */
static GwUdp::DeviceId ClassifyDeviceGuid(const GUID& device)
{
    if(IsEqualGUID(device, GUID_NULL))
    {
        return GwUdp::DeviceId::Keyboard;
    }

    auto eq = [&](unsigned long d1, unsigned short d2, unsigned short d3,
                  unsigned char b0, unsigned char b1, unsigned char b2, unsigned char b3,
                  unsigned char b4, unsigned char b5, unsigned char b6, unsigned char b7) {
        const GUID g = {d1, d2, d3, {b0, b1, b2, b3, b4, b5, b6, b7}};
        return IsEqualGUID(device, g);
    };

    /* Mice */
    if(eq(0xaec50d91, 0xb1f1, 0x452f, 0x8e, 0x16, 0x7b, 0x73, 0xf3, 0x76, 0xfd, 0xf3) ||
       eq(0x7ec00450, 0xe0ee, 0x4289, 0x89, 0xd5, 0x0d, 0x87, 0x9c, 0x19, 0x06, 0x1a) ||
       eq(0xff8a5929, 0x4512, 0x4257, 0x8d, 0x59, 0xc6, 0x47, 0xbf, 0x99, 0x35, 0xd0) ||
       eq(0xd527cbdc, 0xeb0a, 0x483a, 0x9e, 0x89, 0x66, 0xd5, 0x04, 0x63, 0xec, 0x6c) ||
       eq(0xd714c50b, 0x7158, 0x4368, 0xb9, 0x9c, 0x60, 0x1a, 0xcb, 0x98, 0x5e, 0x98) ||
       eq(0xf1876328, 0x6ca4, 0x46ae, 0xbe, 0x04, 0xbe, 0x81, 0x2b, 0x41, 0x44, 0x33) ||
       eq(0x52c15681, 0x4ece, 0x4dd9, 0x8a, 0x52, 0xa1, 0x41, 0x84, 0x59, 0xeb, 0x34) ||
       eq(0x195d70f5, 0xf285, 0x4cff, 0x99, 0xf2, 0xb8, 0xc0, 0xe9, 0x65, 0x8d, 0xb4) ||
       eq(0x77834867, 0x3237, 0x4a9f, 0xad, 0x77, 0x4a, 0x46, 0xc4, 0x18, 0x30, 0x03))
    {
        return GwUdp::DeviceId::Mouse;
    }
    /* Mousepads */
    if(eq(0x80f95a94, 0x73d2, 0x48ca, 0xae, 0x9a, 0x09, 0x86, 0x78, 0x9a, 0x9a, 0xf2))
    {
        return GwUdp::DeviceId::Mousepad;
    }
    /* Headsets / speakers → headset slot */
    if(eq(0xcd1e09a5, 0xd5e6, 0x4a6c, 0xa9, 0x3b, 0xe6, 0xd9, 0xbf, 0x1d, 0x20, 0x92) ||
       eq(0xdf3164d7, 0x5408, 0x4a0e, 0x8a, 0x7f, 0xa7, 0x41, 0x2f, 0x26, 0xbe, 0xbf) ||
       eq(0x7fb8a36e, 0x9e74, 0x4bb3, 0x8c, 0x86, 0xca, 0xc7, 0xf7, 0x89, 0x1e, 0xbd) ||
       eq(0xfb357780, 0x4617, 0x43a7, 0x96, 0x0f, 0xd1, 0x19, 0x0e, 0xd5, 0x48, 0x06) ||
       eq(0x45b308f2, 0xcd44, 0x4594, 0x83, 0x75, 0x4d, 0x59, 0x45, 0xad, 0x88, 0x0e) ||
       eq(0x3017280b, 0xd7f9, 0x4d7b, 0x93, 0x0e, 0x7b, 0x47, 0x18, 0x1b, 0x46, 0xb5))
    {
        return GwUdp::DeviceId::Headset;
    }
    /* Keypads */
    if(eq(0x00f0545c, 0xe180, 0x4ad1, 0x8e, 0x8a, 0x41, 0x90, 0x61, 0xce, 0x50, 0x5e) ||
       eq(0x9d24b0ab, 0x0162, 0x466c, 0x96, 0x40, 0x7a, 0x92, 0x4a, 0xa4, 0xd9, 0xfd))
    {
        return GwUdp::DeviceId::Keypad;
    }
    /* ChromaLink / chassis */
    if(eq(0x35f6f18d, 0x1ae5, 0x436c, 0xa5, 0x75, 0xab, 0x44, 0xa1, 0x27, 0x90, 0x3a) ||
       eq(0x47db1fa7, 0x6b9b, 0x4ee6, 0xb6, 0xf4, 0x40, 0x71, 0xa3, 0xb2, 0x05, 0x3b) ||
       eq(0x0201203b, 0x62f3, 0x4c50, 0x83, 0xdd, 0x59, 0x8b, 0xab, 0xd2, 0x08, 0xe0) ||
       eq(0xbb2e9c9b, 0xb0d2, 0x461a, 0xba, 0x52, 0x23, 0x0b, 0x5d, 0x6c, 0x36, 0x09))
    {
        return GwUdp::DeviceId::ChromaLink;
    }
    /* Keyboards + unknown products → keyboard grid (most common CreateEffect payload). */
    return GwUdp::DeviceId::Keyboard;
}

static std::uint32_t DeviceTypeForSlot(GwUdp::DeviceId id)
{
    switch(id)
    {
        case GwUdp::DeviceId::Keyboard:
            return 1; /* DEVICE_KEYBOARD */
        case GwUdp::DeviceId::Mouse:
            return 2;
        case GwUdp::DeviceId::Headset:
            return 3;
        case GwUdp::DeviceId::Mousepad:
            return 4;
        case GwUdp::DeviceId::Keypad:
            return 5;
        case GwUdp::DeviceId::ChromaLink:
            return 6; /* DEVICE_SYSTEM / link */
        default:
            return 1;
    }
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if(reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        InitSourceName();
        GwCapture::BeginSession();
        GwCapture::LogApi("DllMain", "ATTACH");
    }
    else if(reason == DLL_PROCESS_DETACH)
    {
        GwUdp::FlushDevices(Source());
        {
            std::lock_guard<std::mutex> lock(g_fx_mutex);
            g_effects.clear();
        }
        GwCapture::LogApi("DllMain", "DETACH");
        GwCapture::EndSession();
    }
    return TRUE;
}

extern "C" __declspec(dllexport) RZRESULT Init()
{
    g_initialized = true;
    GwCapture::BeginSession();
    GwCapture::LogApi("Init", GwCapture::SessionPath());
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT InitSDK(ChromaSDK::APPINFOTYPE* info)
{
    g_initialized = true;
    GwCapture::BeginSession();
    GwCapture::LogApi("InitSDK", info ? info->Title : "(null)");
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT UnInit()
{
    GwUdp::FlushDevices(Source());
    GwUdp::ClearDevices();
    {
        std::lock_guard<std::mutex> lock(g_fx_mutex);
        g_effects.clear();
    }
    g_initialized = false;
    GwCapture::LogApi("UnInit", "");
    GwCapture::EndSession(); /* some titles UnInit without process exit */
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT CreateKeyboardEffect(ChromaSDK::Keyboard::EFFECT_TYPE Effect,
                                                               PRZPARAM pParam,
                                                               RZEFFECTID* pEffectId)
{
    if(!g_initialized)
    {
        return RZRESULT_INVALID_PARAMETER;
    }
    if(Effect == ChromaSDK::Keyboard::CHROMA_NONE)
    {
        GwCapture::LogApi("CreateKeyboardEffect", "NONE");
        return RZRESULT_SUCCESS;
    }
    if(!pParam)
    {
        GwCapture::LogUnhandled("Keyboard", (int)Effect, "null_param");
        return RZRESULT_SUCCESS;
    }
    if(Effect == ChromaSDK::Keyboard::CHROMA_STATIC)
    {
        const auto* eff = static_cast<const ChromaSDK::Keyboard::STATIC_EFFECT_TYPE*>(pParam);
        GwCapture::LogStatic("Keyboard", (int)Effect, eff->Color);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Keyboard, (int)Effect, eff->Color));
    }
    if(Effect == ChromaSDK::Keyboard::CHROMA_BREATHING)
    {
        const auto* eff = static_cast<const ChromaSDK::Keyboard::BREATHING_EFFECT_TYPE*>(pParam);
        GwCapture::LogStaticDual("Keyboard", (int)Effect, eff->Color1, eff->Color2);
        const std::uint32_t pair[2] = {eff->Color1, eff->Color2};
        return FinishCreate(pEffectId, MakeEffect(GwUdp::DeviceId::Keyboard, (int)Effect, pair, 1, 2));
    }
    if(Effect == ChromaSDK::Keyboard::CHROMA_REACTIVE)
    {
        const auto* eff = static_cast<const ChromaSDK::Keyboard::REACTIVE_EFFECT_TYPE*>(pParam);
        GwCapture::LogStatic("Keyboard", (int)Effect, eff->Color);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Keyboard, (int)Effect, eff->Color));
    }
    if(Effect == ChromaSDK::Keyboard::CHROMA_RESERVED)
    {
        const auto* eff = static_cast<const ChromaSDK::Keyboard::STARLIGHT_EFFECT_TYPE*>(pParam);
        GwCapture::LogStaticDual("Keyboard", (int)Effect, eff->Color1, eff->Color2);
        GwCapture::LogParamDump("Keyboard", (int)Effect, pParam, sizeof(*eff));
        const std::uint32_t pair[2] = {eff->Color1, eff->Color2};
        return FinishCreate(pEffectId, MakeEffect(GwUdp::DeviceId::Keyboard, (int)Effect, pair, 1, 2));
    }
    if(Effect == ChromaSDK::Keyboard::CHROMA_SPECTRUMCYCLING)
    {
        GwCapture::LogApi("CreateKeyboardEffect", "SPECTRUMCYCLING");
        GwCapture::LogParamDump("Keyboard", (int)Effect, pParam, 16);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Keyboard, (int)Effect, 0x00AAAAAAu));
    }
    if(Effect == ChromaSDK::Keyboard::CHROMA_WAVE)
    {
        const auto* eff = static_cast<const ChromaSDK::Keyboard::WAVE_EFFECT_TYPE*>(pParam);
        char detail[48];
        std::snprintf(detail, sizeof(detail), "WAVE dir=%d", (int)eff->Direction);
        GwCapture::LogApi("CreateKeyboardEffect", detail);
        GwCapture::LogParamDump("Keyboard", (int)Effect, pParam, sizeof(*eff));
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Keyboard, (int)Effect, 0x0000AAAAu));
    }
    if(Effect == ChromaSDK::Keyboard::CHROMA_CUSTOM || Effect == ChromaSDK::Keyboard::CHROMA_CUSTOM2)
    {
        const auto* eff = static_cast<const ChromaSDK::Keyboard::CUSTOM_EFFECT_TYPE*>(pParam);
        return FinishCreate(pEffectId,
                            MakeEffect(GwUdp::DeviceId::Keyboard,
                                       (int)Effect,
                                       &eff->Color[0][0],
                                       ChromaSDK::Keyboard::MAX_ROW,
                                       ChromaSDK::Keyboard::MAX_COLUMN));
    }
    if(Effect == ChromaSDK::Keyboard::CHROMA_CUSTOM_KEY)
    {
        const auto* eff = static_cast<const ChromaSDK::Keyboard::CUSTOM_KEY_EFFECT_TYPE*>(pParam);
        return FinishCreate(pEffectId, MakeKeyboardCustomKey(eff, (int)Effect));
    }
    GwCapture::LogParamDump("Keyboard", (int)Effect, pParam, 64);
    GwCapture::LogUnhandled("Keyboard", (int)Effect, "unknown_effect");
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT CreateEffect(RZDEVICEID DeviceId,
                                                       ChromaSDK::EFFECT_TYPE Effect,
                                                       PRZPARAM pParam,
                                                       RZEFFECTID* pEffectId)
{
    if(!g_initialized)
    {
        return RZRESULT_INVALID_PARAMETER;
    }
    if(!pParam)
    {
        return RZRESULT_SUCCESS;
    }
    const GwUdp::DeviceId slot = ClassifyDeviceGuid(DeviceId);
    {
        char guid_detail[80];
        std::snprintf(guid_detail,
                      sizeof(guid_detail),
                      "guid=%08lX-%04X-%04X effect=%d",
                      (unsigned long)DeviceId.Data1,
                      (unsigned)DeviceId.Data2,
                      (unsigned)DeviceId.Data3,
                      (int)Effect);
        GwCapture::LogApi("CreateEffect", guid_detail);
    }
    if(Effect == ChromaSDK::CHROMA_STATIC)
    {
        const auto* color = static_cast<const std::uint32_t*>(pParam);
        GwCapture::LogStatic(DeviceName(slot), (int)Effect, *color);
        return FinishCreate(pEffectId, MakeSolid(slot, (int)Effect, *color));
    }
    if(Effect == ChromaSDK::CHROMA_CUSTOM)
    {
        const auto* eff = static_cast<const ChromaSDK::Keyboard::CUSTOM_EFFECT_TYPE*>(pParam);
        return FinishCreate(pEffectId,
                            MakeEffect(slot,
                                       (int)Effect,
                                       &eff->Color[0][0],
                                       ChromaSDK::Keyboard::MAX_ROW,
                                       ChromaSDK::Keyboard::MAX_COLUMN));
    }
    GwCapture::LogParamDump(DeviceName(slot), (int)Effect, pParam, 64);
    GwCapture::LogUnhandled(DeviceName(slot), (int)Effect, "CreateEffect_unknown");
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT CreateMouseEffect(ChromaSDK::Mouse::EFFECT_TYPE Effect,
                                                            PRZPARAM pParam,
                                                            RZEFFECTID* pEffectId)
{
    if(!g_initialized)
    {
        return RZRESULT_INVALID_PARAMETER;
    }
    if(!pParam)
    {
        return RZRESULT_SUCCESS;
    }
    if(Effect == ChromaSDK::Mouse::CHROMA_STATIC || Effect == ChromaSDK::Mouse::CHROMA_BLINKING)
    {
        const auto* eff = static_cast<const ChromaSDK::Mouse::STATIC_EFFECT_TYPE*>(pParam);
        GwCapture::LogStatic("Mouse", (int)Effect, eff->Color);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Mouse, (int)Effect, eff->Color));
    }
    if(Effect == ChromaSDK::Mouse::CHROMA_BREATHING)
    {
        const auto* eff = static_cast<const ChromaSDK::Mouse::BREATHING_EFFECT_TYPE*>(pParam);
        GwCapture::LogStaticDual("Mouse", (int)Effect, eff->Color1, eff->Color2);
        const std::uint32_t pair[2] = {eff->Color1, eff->Color2};
        return FinishCreate(pEffectId, MakeEffect(GwUdp::DeviceId::Mouse, (int)Effect, pair, 1, 2));
    }
    if(Effect == ChromaSDK::Mouse::CHROMA_CUSTOM)
    {
        const auto* eff = static_cast<const ChromaSDK::Mouse::CUSTOM_EFFECT_TYPE*>(pParam);
        return FinishCreate(pEffectId,
                            MakeEffect(GwUdp::DeviceId::Mouse,
                                       (int)Effect,
                                       eff->Color,
                                       1,
                                       ChromaSDK::Mouse::MAX_LEDS));
    }
    if(Effect == ChromaSDK::Mouse::CHROMA_CUSTOM2)
    {
        const auto* eff = static_cast<const ChromaSDK::Mouse::CUSTOM2_EFFECT_TYPE*>(pParam);
        return FinishCreate(pEffectId,
                            MakeEffect(GwUdp::DeviceId::Mouse,
                                       (int)Effect,
                                       &eff->Color[0][0],
                                       ChromaSDK::Mouse::MAX_ROW,
                                       ChromaSDK::Mouse::MAX_COLUMN));
    }
    if(Effect == ChromaSDK::Mouse::CHROMA_SPECTRUMCYCLING || Effect == ChromaSDK::Mouse::CHROMA_WAVE ||
       Effect == ChromaSDK::Mouse::CHROMA_REACTIVE)
    {
        GwCapture::LogParamDump("Mouse", (int)Effect, pParam, 32);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Mouse, (int)Effect, 0x00AAAAAAu));
    }
    GwCapture::LogParamDump("Mouse", (int)Effect, pParam, 64);
    GwCapture::LogUnhandled("Mouse", (int)Effect, "unknown_effect");
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT CreateHeadsetEffect(ChromaSDK::Headset::EFFECT_TYPE Effect,
                                                              PRZPARAM pParam,
                                                              RZEFFECTID* pEffectId)
{
    if(!g_initialized)
    {
        return RZRESULT_INVALID_PARAMETER;
    }
    if(!pParam || Effect == ChromaSDK::Headset::CHROMA_NONE)
    {
        if(Effect == ChromaSDK::Headset::CHROMA_NONE)
        {
            GwCapture::LogApi("CreateHeadsetEffect", "NONE");
        }
        return RZRESULT_SUCCESS;
    }
    if(Effect == ChromaSDK::Headset::CHROMA_STATIC)
    {
        const auto* eff = static_cast<const ChromaSDK::Headset::STATIC_EFFECT_TYPE*>(pParam);
        GwCapture::LogStatic("Headset", (int)Effect, eff->Color);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Headset, (int)Effect, eff->Color));
    }
    if(Effect == ChromaSDK::Headset::CHROMA_BREATHING)
    {
        const auto* eff = static_cast<const ChromaSDK::Headset::BREATHING_EFFECT_TYPE*>(pParam);
        GwCapture::LogStaticDual("Headset", (int)Effect, eff->Color1, eff->Color2);
        const std::uint32_t pair[2] = {eff->Color1, eff->Color2};
        return FinishCreate(pEffectId, MakeEffect(GwUdp::DeviceId::Headset, (int)Effect, pair, 1, 2));
    }
    if(Effect == ChromaSDK::Headset::CHROMA_CUSTOM)
    {
        const auto* eff = static_cast<const ChromaSDK::Headset::CUSTOM_EFFECT_TYPE*>(pParam);
        return FinishCreate(pEffectId,
                            MakeEffect(GwUdp::DeviceId::Headset,
                                       (int)Effect,
                                       eff->Color,
                                       1,
                                       ChromaSDK::Headset::MAX_LEDS));
    }
    if(Effect == ChromaSDK::Headset::CHROMA_SPECTRUMCYCLING)
    {
        GwCapture::LogParamDump("Headset", (int)Effect, pParam, 16);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Headset, (int)Effect, 0x00AAAAAAu));
    }
    GwCapture::LogParamDump("Headset", (int)Effect, pParam, 64);
    GwCapture::LogUnhandled("Headset", (int)Effect, "unknown_effect");
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT CreateMousepadEffect(ChromaSDK::Mousepad::EFFECT_TYPE Effect,
                                                               PRZPARAM pParam,
                                                               RZEFFECTID* pEffectId)
{
    if(!g_initialized)
    {
        return RZRESULT_INVALID_PARAMETER;
    }
    if(!pParam)
    {
        return RZRESULT_SUCCESS;
    }
    if(Effect == ChromaSDK::Mousepad::CHROMA_STATIC)
    {
        const auto* eff = static_cast<const ChromaSDK::Mousepad::STATIC_EFFECT_TYPE*>(pParam);
        GwCapture::LogStatic("Mousepad", (int)Effect, eff->Color);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Mousepad, (int)Effect, eff->Color));
    }
    if(Effect == ChromaSDK::Mousepad::CHROMA_BREATHING)
    {
        const auto* eff = static_cast<const ChromaSDK::Mousepad::BREATHING_EFFECT_TYPE*>(pParam);
        GwCapture::LogStaticDual("Mousepad", (int)Effect, eff->Color1, eff->Color2);
        const std::uint32_t pair[2] = {eff->Color1, eff->Color2};
        return FinishCreate(pEffectId, MakeEffect(GwUdp::DeviceId::Mousepad, (int)Effect, pair, 1, 2));
    }
    if(Effect == ChromaSDK::Mousepad::CHROMA_CUSTOM)
    {
        const auto* eff = static_cast<const ChromaSDK::Mousepad::CUSTOM_EFFECT_TYPE*>(pParam);
        return FinishCreate(pEffectId,
                            MakeEffect(GwUdp::DeviceId::Mousepad,
                                       (int)Effect,
                                       eff->Color,
                                       1,
                                       ChromaSDK::Mousepad::MAX_LEDS));
    }
    if(Effect == ChromaSDK::Mousepad::CHROMA_WAVE || Effect == ChromaSDK::Mousepad::CHROMA_SPECTRUMCYCLING)
    {
        GwCapture::LogParamDump("Mousepad", (int)Effect, pParam, 16);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Mousepad, (int)Effect, 0x00AAAAAAu));
    }
    GwCapture::LogParamDump("Mousepad", (int)Effect, pParam, 64);
    GwCapture::LogUnhandled("Mousepad", (int)Effect, "unknown_effect");
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT CreateKeypadEffect(ChromaSDK::Keypad::EFFECT_TYPE Effect,
                                                             PRZPARAM pParam,
                                                             RZEFFECTID* pEffectId)
{
    if(!g_initialized)
    {
        return RZRESULT_INVALID_PARAMETER;
    }
    if(!pParam || Effect == ChromaSDK::Keypad::CHROMA_NONE)
    {
        if(Effect == ChromaSDK::Keypad::CHROMA_NONE)
        {
            GwCapture::LogApi("CreateKeypadEffect", "NONE");
        }
        return RZRESULT_SUCCESS;
    }
    if(Effect == ChromaSDK::Keypad::CHROMA_STATIC)
    {
        const auto* eff = static_cast<const ChromaSDK::Keypad::STATIC_EFFECT_TYPE*>(pParam);
        GwCapture::LogStatic("Keypad", (int)Effect, eff->Color);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Keypad, (int)Effect, eff->Color));
    }
    if(Effect == ChromaSDK::Keypad::CHROMA_BREATHING)
    {
        const auto* eff = static_cast<const ChromaSDK::Keypad::BREATHING_EFFECT_TYPE*>(pParam);
        GwCapture::LogStaticDual("Keypad", (int)Effect, eff->Color1, eff->Color2);
        const std::uint32_t pair[2] = {eff->Color1, eff->Color2};
        return FinishCreate(pEffectId, MakeEffect(GwUdp::DeviceId::Keypad, (int)Effect, pair, 1, 2));
    }
    if(Effect == ChromaSDK::Keypad::CHROMA_REACTIVE)
    {
        const auto* eff = static_cast<const ChromaSDK::Keypad::REACTIVE_EFFECT_TYPE*>(pParam);
        GwCapture::LogStatic("Keypad", (int)Effect, eff->Color);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Keypad, (int)Effect, eff->Color));
    }
    if(Effect == ChromaSDK::Keypad::CHROMA_CUSTOM)
    {
        const auto* eff = static_cast<const ChromaSDK::Keypad::CUSTOM_EFFECT_TYPE*>(pParam);
        return FinishCreate(pEffectId,
                            MakeEffect(GwUdp::DeviceId::Keypad,
                                       (int)Effect,
                                       &eff->Color[0][0],
                                       ChromaSDK::Keypad::MAX_ROW,
                                       ChromaSDK::Keypad::MAX_COLUMN));
    }
    if(Effect == ChromaSDK::Keypad::CHROMA_WAVE || Effect == ChromaSDK::Keypad::CHROMA_SPECTRUMCYCLING)
    {
        GwCapture::LogParamDump("Keypad", (int)Effect, pParam, 16);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::Keypad, (int)Effect, 0x00AAAAAAu));
    }
    GwCapture::LogParamDump("Keypad", (int)Effect, pParam, 64);
    GwCapture::LogUnhandled("Keypad", (int)Effect, "unknown_effect");
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT CreateChromaLinkEffect(ChromaSDK::ChromaLink::EFFECT_TYPE Effect,
                                                                 PRZPARAM pParam,
                                                                 RZEFFECTID* pEffectId)
{
    if(!g_initialized)
    {
        return RZRESULT_INVALID_PARAMETER;
    }
    if(!pParam || Effect == ChromaSDK::ChromaLink::CHROMA_NONE)
    {
        if(Effect == ChromaSDK::ChromaLink::CHROMA_NONE)
        {
            GwCapture::LogApi("CreateChromaLinkEffect", "NONE");
        }
        return RZRESULT_SUCCESS;
    }
    if(Effect == ChromaSDK::ChromaLink::CHROMA_STATIC)
    {
        const auto* eff = static_cast<const ChromaSDK::ChromaLink::STATIC_EFFECT_TYPE*>(pParam);
        GwCapture::LogStatic("ChromaLink", (int)Effect, eff->Color);
        return FinishCreate(pEffectId, MakeSolid(GwUdp::DeviceId::ChromaLink, (int)Effect, eff->Color));
    }
    if(Effect == ChromaSDK::ChromaLink::CHROMA_CUSTOM)
    {
        const auto* eff = static_cast<const ChromaSDK::ChromaLink::CUSTOM_EFFECT_TYPE*>(pParam);
        return FinishCreate(pEffectId,
                            MakeEffect(GwUdp::DeviceId::ChromaLink,
                                       (int)Effect,
                                       eff->Color,
                                       1,
                                       ChromaSDK::ChromaLink::MAX_LEDS));
    }
    GwCapture::LogParamDump("ChromaLink", (int)Effect, pParam, 64);
    GwCapture::LogUnhandled("ChromaLink", (int)Effect, "unknown_effect");
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT DeleteEffect(RZEFFECTID EffectId)
{
    {
        std::lock_guard<std::mutex> lock(g_fx_mutex);
        g_effects.erase(EffectId);
    }
    GwCapture::NoteDeleteEffect();
    char detail[48];
    std::snprintf(detail,
                  sizeof(detail),
                  "id=%08lX",
                  (unsigned long)EffectId.Data1);
    GwCapture::LogApi("DeleteEffect", detail);
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT SetEffect(RZEFFECTID EffectId)
{
    StoredEffect fx;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_fx_mutex);
        const auto it = g_effects.find(EffectId);
        if(it != g_effects.end())
        {
            fx = it->second;
            found = true;
        }
    }
    if(!found)
    {
        GwCapture::NoteSetEffect("?", false);
        char detail[48];
        std::snprintf(detail, sizeof(detail), "unknown_id=%08lX", (unsigned long)EffectId.Data1);
        GwCapture::LogApi("SetEffect", detail);
        return RZRESULT_SUCCESS;
    }
    GwCapture::NoteSetEffect(DeviceName(fx.device), true);
    ApplyEffect(fx);
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT RegisterEventNotification(HWND /*hWnd*/)
{
    GwCapture::LogApi("RegisterEventNotification", "");
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT UnregisterEventNotification()
{
    GwCapture::LogApi("UnregisterEventNotification", "");
    return RZRESULT_SUCCESS;
}

struct DEVICE_INFO_TYPE
{
    std::uint32_t DeviceType;
    std::uint32_t Connected;
};

extern "C" __declspec(dllexport) RZRESULT QueryDevice(RZDEVICEID DeviceId, DEVICE_INFO_TYPE& DeviceInfo)
{
    const GwUdp::DeviceId slot = ClassifyDeviceGuid(DeviceId);
    DeviceInfo.DeviceType = DeviceTypeForSlot(slot);
    DeviceInfo.Connected = 1;
    char detail[96];
    std::snprintf(detail,
                  sizeof(detail),
                  "guid=%08lX type=%u connected=1 slot=%s",
                  (unsigned long)DeviceId.Data1,
                  (unsigned)DeviceInfo.DeviceType,
                  DeviceName(slot));
    GwCapture::LogApi("QueryDevice", detail);
    return RZRESULT_SUCCESS;
}
