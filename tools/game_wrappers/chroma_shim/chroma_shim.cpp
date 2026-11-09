// SPDX-License-Identifier: GPL-2.0-only
// Razer Chroma SDK proxy — forwards Init to success and mirrors STATIC/CUSTOM colors to GW01 UDP.

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "../common/gw_udp_emit.h"

#include <cstdint>
#include <cstring>

using RZRESULT = int;
using RZDEVICEID = int;
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
    CHROMA_WAVE = 2,
    CHROMA_SPECTRUMCYCLING = 3,
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
    CHROMA_WAVE = 5,
};

struct STATIC_EFFECT_TYPE
{
    enum Color
    {
        Single = 0,
        Dual = 1
    };
    Color Color;
    std::uint32_t Color1;
    std::uint32_t Color2;
};
} // namespace Keyboard
} // namespace ChromaSDK

static bool g_initialized = false;

extern "C" __declspec(dllexport) RZRESULT Init()
{
    g_initialized = true;
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT InitSDK(ChromaSDK::APPINFOTYPE* /*pAppInfo*/)
{
    g_initialized = true;
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT UnInit()
{
    g_initialized = false;
    return RZRESULT_SUCCESS;
}

static void EmitStaticColors(std::uint32_t c1, std::uint32_t c2)
{
    GwUdp::EmitFromColorRef(c1, c2, "chroma_shim");
}

extern "C" __declspec(dllexport) RZRESULT CreateKeyboardEffect(ChromaSDK::Keyboard::EFFECT_TYPE Effect,
                                                               PRZPARAM pParam,
                                                               RZEFFECTID* /*pEffectId*/)
{
    if(!g_initialized || !pParam)
    {
        return RZRESULT_INVALID_PARAMETER;
    }
    if(Effect == ChromaSDK::Keyboard::CHROMA_STATIC)
    {
        const auto* eff = static_cast<const ChromaSDK::Keyboard::STATIC_EFFECT_TYPE*>(pParam);
        EmitStaticColors(eff->Color1, eff->Color2);
        return RZRESULT_SUCCESS;
    }
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT CreateEffect(RZDEVICEID /*DeviceId*/,
                                                       ChromaSDK::EFFECT_TYPE Effect,
                                                       PRZPARAM pParam,
                                                       RZEFFECTID* /*pEffectId*/)
{
    if(!g_initialized)
    {
        return RZRESULT_INVALID_PARAMETER;
    }
    if(Effect == ChromaSDK::CHROMA_STATIC && pParam)
    {
        const auto* eff = static_cast<const ChromaSDK::Keyboard::STATIC_EFFECT_TYPE*>(pParam);
        EmitStaticColors(eff->Color1, eff->Color2);
    }
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT CreateMouseEffect(int /*Effect*/, PRZPARAM /*pParam*/, RZEFFECTID* /*pEffectId*/)
{
    return g_initialized ? RZRESULT_SUCCESS : RZRESULT_INVALID_PARAMETER;
}

extern "C" __declspec(dllexport) RZRESULT CreateHeadsetEffect(int /*Effect*/, PRZPARAM /*pParam*/, RZEFFECTID* /*pEffectId*/)
{
    return g_initialized ? RZRESULT_SUCCESS : RZRESULT_INVALID_PARAMETER;
}

extern "C" __declspec(dllexport) RZRESULT CreateMousepadEffect(int /*Effect*/, PRZPARAM /*pParam*/, RZEFFECTID* /*pEffectId*/)
{
    return g_initialized ? RZRESULT_SUCCESS : RZRESULT_INVALID_PARAMETER;
}

extern "C" __declspec(dllexport) RZRESULT CreateKeypadEffect(int /*Effect*/, PRZPARAM /*pParam*/, RZEFFECTID* /*pEffectId*/)
{
    return g_initialized ? RZRESULT_SUCCESS : RZRESULT_INVALID_PARAMETER;
}

extern "C" __declspec(dllexport) RZRESULT CreateChromaLinkEffect(int /*Effect*/, PRZPARAM /*pParam*/, RZEFFECTID* /*pEffectId*/)
{
    return g_initialized ? RZRESULT_SUCCESS : RZRESULT_INVALID_PARAMETER;
}

extern "C" __declspec(dllexport) RZRESULT DeleteEffect(RZEFFECTID /*EffectId*/)
{
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT SetEffect(RZEFFECTID /*EffectId*/)
{
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT RegisterEventNotification(HWND /*hWnd*/)
{
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT UnregisterEventNotification()
{
    return RZRESULT_SUCCESS;
}

extern "C" __declspec(dllexport) RZRESULT QueryDevice(RZDEVICEID /*DeviceId*/, int& /*DeviceInfo*/)
{
    return RZRESULT_SUCCESS;
}
