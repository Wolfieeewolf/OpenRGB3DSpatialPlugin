// SPDX-License-Identifier: GPL-2.0-only

#include "ReactiveInputManager.h"
#include "ReactiveKeyMap.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QWidget>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace
{
constexpr int kEdgeCap = 32;

std::string ToLowerCopy(const std::string& s)
{
    std::string out = s;
    for(char& c : out)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool TypeMatchesKind(device_type type, ReactiveSourceKind kind)
{
    switch(kind)
    {
    case ReactiveSourceKind::Keyboard:
        return type == DEVICE_TYPE_KEYBOARD || type == DEVICE_TYPE_KEYPAD || type == DEVICE_TYPE_LAPTOP;
    case ReactiveSourceKind::Mouse:
        return type == DEVICE_TYPE_MOUSE || type == DEVICE_TYPE_MOUSEMAT;
    case ReactiveSourceKind::Gamepad:
        return type == DEVICE_TYPE_GAMEPAD;
    default:
        return false;
    }
}

uint32_t PackId(ReactiveSourceKind kind, uint32_t payload, uint32_t pad = 0)
{
    return (static_cast<uint32_t>(kind) << 24) | ((pad & 0xFu) << 20) | (payload & 0xFFFFFu);
}

ReactiveSourceKind UnpackKind(uint32_t packed)
{
    return static_cast<ReactiveSourceKind>((packed >> 24) & 0x3u);
}

uint32_t UnpackPayload(uint32_t packed)
{
    return packed & 0xFFFFFu;
}

uint32_t UnpackVk(uint32_t packed)
{
    return UnpackPayload(packed) & 0xFFu;
}

uint32_t UnpackMake(uint32_t packed)
{
    return (UnpackPayload(packed) >> 8) & 0xFFu;
}

bool UnpackExtended(uint32_t packed)
{
    return (UnpackPayload(packed) & 0x10000u) != 0;
}

bool UnpackPausePrefix(uint32_t packed)
{
    return (UnpackPayload(packed) & 0x20000u) != 0;
}

float Distance3(const Vector3D& a, const Vector3D& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

struct OriginHit
{
    Vector3D position{};
    float    spread = 0.0f;
    float    spacing = 0.0f;
};

struct EdgeSlot
{
    uint32_t packed = 0;
    bool     down   = false;
    uint16_t vid    = 0;
    uint16_t pid    = 0;
};

struct DownKey
{
    uint32_t packed = 0;
    uint16_t vid    = 0;
    uint16_t pid    = 0;
};

uint64_t MakeSourceKey(uint32_t packed, uint16_t vid, uint16_t pid)
{
    return (static_cast<uint64_t>(vid) << 48) |
           (static_cast<uint64_t>(pid) << 32) |
           static_cast<uint64_t>(packed);
}

bool KeyNumericSuffix(const std::string& lower, int* out_number)
{
    if(!out_number)
    {
        return false;
    }
    std::size_t i = 0;
    if(lower.size() >= 4 && lower.compare(0, 4, "key:") == 0)
    {
        i = 4;
    }
    while(i < lower.size() && lower[i] == ' ')
    {
        ++i;
    }
    if(i >= lower.size() || !std::isdigit(static_cast<unsigned char>(lower[i])))
    {
        return false;
    }
    int n = 0;
    while(i < lower.size() && std::isdigit(static_cast<unsigned char>(lower[i])))
    {
        n = n * 10 + (lower[i] - '0');
        ++i;
    }
    while(i < lower.size() && lower[i] == ' ')
    {
        ++i;
    }
    if(i != lower.size())
    {
        return false;
    }
    *out_number = n;
    return true;
}

bool NameEqualsKey(const std::string& lower_led, const char* key_name)
{
    if(!key_name || key_name[0] == '\0')
    {
        return false;
    }
    const std::string want = ToLowerCopy(std::string(key_name));
    if(lower_led == want)
    {
        return true;
    }
    constexpr char kPrefix[] = "key: ";
    if(want.size() > 5 && want.compare(0, 5, kPrefix) == 0)
    {
        if(lower_led == want.substr(5))
        {
            return true;
        }
    }
    if(lower_led.size() > 5 && lower_led.compare(0, 5, kPrefix) == 0)
    {
        if(lower_led.substr(5) == want)
        {
            return true;
        }
    }
    int led_n = 0;
    int want_n = 0;
    if(KeyNumericSuffix(lower_led, &led_n) && KeyNumericSuffix(want, &want_n))
    {
        return led_n == want_n;
    }
    return false;
}

void AppendTokens(const std::string& lower, std::vector<std::string>* tokens)
{
    if(!tokens)
    {
        return;
    }
    std::string cur;
    for(char c : lower)
    {
        if(std::isalnum(static_cast<unsigned char>(c)))
        {
            cur.push_back(c);
        }
        else if(!cur.empty())
        {
            tokens->push_back(cur);
            cur.clear();
        }
    }
    if(!cur.empty())
    {
        tokens->push_back(cur);
    }
}

bool TokensMatchAlias(const std::string& lower_led, const char* alias)
{
    if(!alias || alias[0] == '\0')
    {
        return false;
    }
    std::vector<std::string> led_tokens;
    std::vector<std::string> want_tokens;
    AppendTokens(lower_led, &led_tokens);
    AppendTokens(ToLowerCopy(std::string(alias)), &want_tokens);
    if(want_tokens.empty() || led_tokens.empty())
    {
        return false;
    }
    /* Single-token aliases such as "Right" must not match "Right Bumper". */
    if(want_tokens.size() == 1 && led_tokens.size() != 1)
    {
        return false;
    }
    for(const std::string& want : want_tokens)
    {
        bool found = false;
        for(const std::string& tok : led_tokens)
        {
            if(tok == want)
            {
                found = true;
                break;
            }
        }
        if(!found)
        {
            return false;
        }
    }
    return true;
}
} // namespace

class ReactiveInputManager::Impl
{
public:
    QMutex mutex;
    std::atomic<bool> running{false};
    std::array<EdgeSlot, kEdgeCap> edges{};
    int edge_head = 0;
    int edge_count = 0;
    std::unordered_map<uint64_t, DownKey> down;
    std::vector<ReactiveLedSample> samples;
#ifdef _WIN32
    std::unordered_map<std::uintptr_t, uint32_t> hid_vidpid;
    std::atomic<unsigned long long> last_press_ms{0};
    std::atomic<uint32_t> last_press_vk{0};
    std::atomic<uint32_t> last_press_vidpid{0};
#endif

#ifdef _WIN32
    ~Impl()
    {
        delete watcher;
        watcher = nullptr;
        delete sink;
        sink = nullptr;
        if(xinput_dll)
        {
            xinput_get_state = nullptr;
            FreeLibrary(xinput_dll);
            xinput_dll = nullptr;
        }
    }
#else
    ~Impl() = default;
#endif

#ifdef _WIN32
    class SinkWidget : public QWidget
    {
    public:
        SinkWidget()
            : QWidget(nullptr)
        {
            setAttribute(Qt::WA_DontShowOnScreen, true);
            setAttribute(Qt::WA_NativeWindow, true);
            resize(1, 1);
            move(-10000, -10000);
        }
    };

    class Watcher : public QObject, public QAbstractNativeEventFilter
    {
    public:
        explicit Watcher(Impl* owner)
            : owner_(owner)
        {
            if(qApp)
            {
                qApp->installNativeEventFilter(this);
            }
        }

        ~Watcher() override
        {
            if(qApp)
            {
                qApp->removeNativeEventFilter(this);
            }
        }

        bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override
        {
            (void)result;
            if(!owner_ || !owner_->running.load())
            {
                return false;
            }
            if(eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
            {
                return false;
            }
            MSG* msg = static_cast<MSG*>(message);
            if(!msg || msg->message != WM_INPUT)
            {
                return false;
            }
            owner_->HandleRawInput(reinterpret_cast<HRAWINPUT>(msg->lParam));
            return false;
        }

        Impl* owner_ = nullptr;
    };

    SinkWidget* sink = nullptr;
    Watcher* watcher = nullptr;

    struct XInputGamepad
    {
        WORD  wButtons;
        BYTE  bLeftTrigger;
        BYTE  bRightTrigger;
        SHORT sThumbLX;
        SHORT sThumbLY;
        SHORT sThumbRX;
        SHORT sThumbRY;
    };

    struct XInputState
    {
        DWORD dwPacketNumber;
        XInputGamepad Gamepad;
    };

    using XInputGetStateFn = DWORD (WINAPI*)(DWORD, XInputState*);
    HMODULE xinput_dll = nullptr;
    XInputGetStateFn xinput_get_state = nullptr;
    std::array<uint16_t, 4> pad_buttons{};
    std::array<bool, 4> pad_lt{};
    std::array<bool, 4> pad_rt{};
    bool pads_inited = false;

    void LookupHidVidPid(HANDLE device, uint16_t* vid, uint16_t* pid)
    {
        if(!vid || !pid)
        {
            return;
        }
        *vid = 0;
        *pid = 0;
        if(!device)
        {
            return;
        }
        const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(device);
        const auto found = hid_vidpid.find(key);
        if(found != hid_vidpid.end())
        {
            *vid = static_cast<uint16_t>(found->second >> 16);
            *pid = static_cast<uint16_t>(found->second & 0xFFFFu);
            return;
        }
        UINT chars = 0;
        GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &chars);
        if(chars == 0 || chars > 1024)
        {
            hid_vidpid[key] = 0;
            return;
        }
        std::wstring wpath(chars, L'\0');
        if(GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, wpath.data(), &chars) == static_cast<UINT>(-1))
        {
            hid_vidpid[key] = 0;
            return;
        }
        std::string path;
        path.reserve(wpath.size());
        for(wchar_t c : wpath)
        {
            if(c == 0)
            {
                break;
            }
            path.push_back(static_cast<char>(c <= 127 ? c : '?'));
        }
        uint16_t parsed_vid = 0;
        uint16_t parsed_pid = 0;
        ParseHidVidPid(path, &parsed_vid, &parsed_pid);
        hid_vidpid[key] = (static_cast<uint32_t>(parsed_vid) << 16) | parsed_pid;
        *vid = parsed_vid;
        *pid = parsed_pid;
    }

    void HandleRawInput(HRAWINPUT handle)
    {
        UINT size = 0;
        GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
        if(size == 0 || size > 4096)
        {
            return;
        }
        alignas(RAWINPUT) unsigned char buf[4096];
        UINT got = size;
        if(GetRawInputData(handle, RID_INPUT, buf, &got, sizeof(RAWINPUTHEADER)) != size)
        {
            return;
        }
        const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(buf);
        uint16_t vid = 0;
        uint16_t pid = 0;
        LookupHidVidPid(raw->header.hDevice, &vid, &pid);
        if(raw->header.dwType == RIM_TYPEKEYBOARD)
        {
            const RAWKEYBOARD& kb = raw->data.keyboard;
            if(kb.VKey == VK_PACKET)
            {
                return;
            }
            if((kb.VKey == 0 || kb.VKey == 0xFF) && kb.MakeCode == 0)
            {
                return;
            }
            const bool up = (kb.Flags & RI_KEY_BREAK) != 0;
            const uint32_t payload = (static_cast<uint32_t>(kb.VKey) & 0xFFu) |
                                     ((static_cast<uint32_t>(kb.MakeCode) & 0xFFu) << 8) |
                                     ((kb.Flags & RI_KEY_E0) ? 0x10000u : 0u) |
                                     ((kb.Flags & RI_KEY_E1) ? 0x20000u : 0u);
            PushEdge(PackId(ReactiveSourceKind::Keyboard, payload), !up, vid, pid);
        }
        else if(raw->header.dwType == RIM_TYPEMOUSE)
        {
            const RAWMOUSE& ms = raw->data.mouse;
            const USHORT f = ms.usButtonFlags;
            if(f & RI_MOUSE_LEFT_BUTTON_DOWN)   PushEdge(PackId(ReactiveSourceKind::Mouse, 1), true, vid, pid);
            if(f & RI_MOUSE_LEFT_BUTTON_UP)     PushEdge(PackId(ReactiveSourceKind::Mouse, 1), false, vid, pid);
            if(f & RI_MOUSE_RIGHT_BUTTON_DOWN)  PushEdge(PackId(ReactiveSourceKind::Mouse, 2), true, vid, pid);
            if(f & RI_MOUSE_RIGHT_BUTTON_UP)    PushEdge(PackId(ReactiveSourceKind::Mouse, 2), false, vid, pid);
            if(f & RI_MOUSE_MIDDLE_BUTTON_DOWN) PushEdge(PackId(ReactiveSourceKind::Mouse, 3), true, vid, pid);
            if(f & RI_MOUSE_MIDDLE_BUTTON_UP)   PushEdge(PackId(ReactiveSourceKind::Mouse, 3), false, vid, pid);
            if(f & RI_MOUSE_BUTTON_4_DOWN)      PushEdge(PackId(ReactiveSourceKind::Mouse, 4), true, vid, pid);
            if(f & RI_MOUSE_BUTTON_4_UP)        PushEdge(PackId(ReactiveSourceKind::Mouse, 4), false, vid, pid);
            if(f & RI_MOUSE_BUTTON_5_DOWN)      PushEdge(PackId(ReactiveSourceKind::Mouse, 5), true, vid, pid);
            if(f & RI_MOUSE_BUTTON_5_UP)        PushEdge(PackId(ReactiveSourceKind::Mouse, 5), false, vid, pid);
        }
    }

    void EnsureXInput()
    {
        if(xinput_get_state)
        {
            return;
        }
        xinput_dll = LoadLibraryW(L"xinput1_4.dll");
        if(!xinput_dll)
        {
            xinput_dll = LoadLibraryW(L"xinput9_1_0.dll");
        }
        if(!xinput_dll)
        {
            return;
        }
        xinput_get_state = reinterpret_cast<XInputGetStateFn>(GetProcAddress(xinput_dll, "XInputGetState"));
    }

    void PollXInput()
    {
        EnsureXInput();
        if(!xinput_get_state)
        {
            return;
        }
        for(DWORD pad = 0; pad < 4; ++pad)
        {
            Impl::XInputState state{};
            if(xinput_get_state(pad, &state) != ERROR_SUCCESS)
            {
                if(pads_inited)
                {
                    const uint16_t prev = pad_buttons[pad];
                    for(int b = 0; b < 16; ++b)
                    {
                        const uint16_t bit = static_cast<uint16_t>(1u << b);
                        if(prev & bit)
                        {
                            PushEdge(PackId(ReactiveSourceKind::Gamepad, bit, pad), false);
                        }
                    }
                    if(pad_lt[pad])
                    {
                        PushEdge(PackId(ReactiveSourceKind::Gamepad, 0x0400, pad), false);
                    }
                    if(pad_rt[pad])
                    {
                        PushEdge(PackId(ReactiveSourceKind::Gamepad, 0x0800, pad), false);
                    }
                }
                pad_buttons[pad] = 0;
                pad_lt[pad] = false;
                pad_rt[pad] = false;
                continue;
            }
            const uint16_t now = state.Gamepad.wButtons;
            const uint16_t prev = pads_inited ? pad_buttons[pad] : now;
            for(int b = 0; b < 16; ++b)
            {
                const uint16_t bit = static_cast<uint16_t>(1u << b);
                const bool now_down = (now & bit) != 0;
                const bool was_down = (prev & bit) != 0;
                if(now_down != was_down)
                {
                    PushEdge(PackId(ReactiveSourceKind::Gamepad, bit, pad), now_down);
                }
            }
            const bool lt = state.Gamepad.bLeftTrigger > 30;
            const bool rt = state.Gamepad.bRightTrigger > 30;
            if(pads_inited)
            {
                if(lt != pad_lt[pad])
                {
                    PushEdge(PackId(ReactiveSourceKind::Gamepad, 0x0400, pad), lt);
                }
                if(rt != pad_rt[pad])
                {
                    PushEdge(PackId(ReactiveSourceKind::Gamepad, 0x0800, pad), rt);
                }
            }
            pad_buttons[pad] = now;
            pad_lt[pad] = lt;
            pad_rt[pad] = rt;
        }
        pads_inited = true;
    }
#endif

    void PushEdge(uint32_t packed, bool down_now, uint16_t vid = 0, uint16_t pid = 0)
    {
        QMutexLocker lock(&mutex);
        if(!running.load())
        {
            return;
        }
#ifdef _WIN32
        if(down_now && UnpackKind(packed) == ReactiveSourceKind::Keyboard)
        {
            const uint32_t vk = UnpackVk(packed);
            const unsigned long long now_ms = GetTickCount64();
            const unsigned long long prev_ms = last_press_ms.load();
            const uint32_t prev_vk = last_press_vk.load();
            const uint32_t prev_id = last_press_vidpid.load();
            const uint32_t id = (static_cast<uint32_t>(vid) << 16) | pid;
            if(prev_ms != 0 && now_ms >= prev_ms && (now_ms - prev_ms) < 80ull &&
               prev_vk == vk && prev_id != 0 && id != prev_id)
            {
                return;
            }
            last_press_ms.store(now_ms);
            last_press_vk.store(vk);
            last_press_vidpid.store(id);
        }
#endif
        const uint64_t source = MakeSourceKey(packed, vid, pid);
        const bool was_down = down.find(source) != down.end();
        if(down_now && was_down)
        {
            return;
        }
        if(!down_now && !was_down)
        {
            return;
        }
        if(down_now)
        {
            down[source] = DownKey{packed, vid, pid};
        }
        else
        {
            down.erase(source);
        }
        if(edge_count == kEdgeCap)
        {
            edge_head = (edge_head + 1) % kEdgeCap;
            edge_count -= 1;
        }
        const int idx = (edge_head + edge_count) % kEdgeCap;
        edges[static_cast<size_t>(idx)] = EdgeSlot{packed, down_now, vid, pid};
        edge_count += 1;
    }

    void WipeLocked()
    {
        edges.fill(EdgeSlot{});
        edge_head = 0;
        edge_count = 0;
        down.clear();
        samples.clear();
#ifdef _WIN32
        hid_vidpid.clear();
        last_press_ms.store(0);
        last_press_vk.store(0);
        last_press_vidpid.store(0);
        pad_buttons.fill(0);
        pad_lt.fill(false);
        pad_rt.fill(false);
        pads_inited = false;
#endif
    }

    void MeasureDeviceGeometry(const ReactiveLedSample& origin, float* spread, float* spacing) const
    {
        if(!spread || !spacing)
        {
            return;
        }
        float max_d = 0.0f;
        float nearest = 1.0e9f;
        int neighbor_count = 0;
        for(const ReactiveLedSample& s : samples)
        {
            if(origin.device_id != 0)
            {
                if(s.device_id != origin.device_id)
                {
                    continue;
                }
            }
            else if(s.type != origin.type)
            {
                continue;
            }
            const float d = Distance3(s.room_position, origin.room_position);
            if(d < 1.0e-5f)
            {
                continue;
            }
            max_d = std::max(max_d, d);
            nearest = std::min(nearest, d);
            neighbor_count += 1;
        }
        *spread = max_d;
        if(neighbor_count > 0 && nearest < 1.0e8f)
        {
            *spacing = nearest;
            if(max_d > 1.0e-5f)
            {
                *spacing = std::min(*spacing, max_d * 0.18f);
            }
        }
        else if(max_d > 1.0e-5f)
        {
            *spacing = max_d * 0.08f;
        }
        else
        {
            *spacing = 0.0f;
        }
    }

    bool FillHit(const ReactiveLedSample* led, OriginHit* out) const
    {
        if(!led || !out)
        {
            return false;
        }
        out->position = led->room_position;
        MeasureDeviceGeometry(*led, &out->spread, &out->spacing);
        return true;
    }

    bool ResolvePacked(uint32_t packed, uint16_t vid, uint16_t pid, OriginHit* out) const
    {
        if(!out)
        {
            return false;
        }
        const ReactiveSourceKind kind = UnpackKind(packed);
        const uint32_t payload = UnpackPayload(packed);
        const ReactiveLedSample* hit = nullptr;
        if(kind == ReactiveSourceKind::Keyboard)
        {
            const char* names[12] = {};
            const int n = ReactiveKeyMap::FillKeyboardLedNames(UnpackVk(packed),
                                                               UnpackMake(packed),
                                                               UnpackExtended(packed),
                                                               UnpackPausePrefix(packed),
                                                               names,
                                                               11);
            if(n <= 0)
            {
                return false;
            }
            const char* const* extra = (n > 2) ? (names + 2) : nullptr;
            hit = FindNamed(kind, names[0], n > 1 ? names[1] : nullptr, vid, pid, extra);
        }
        else if(kind == ReactiveSourceKind::Mouse)
        {
            const char* primary = ReactiveKeyMap::MousePrimaryName(payload);
            const char* const* aliases = ReactiveKeyMap::MouseAliasNames(payload);
            hit = FindNamed(kind, primary, nullptr, vid, pid, aliases);
            if(!hit)
            {
                hit = FindSpatialOnDevice(kind, payload, vid, pid);
            }
        }
        else if(kind == ReactiveSourceKind::Gamepad)
        {
            const char* const* aliases = ReactiveKeyMap::GamepadAliasNames(static_cast<uint16_t>(payload));
            hit = FindNamed(kind, aliases ? aliases[0] : nullptr, nullptr, vid, pid, aliases);
            if(!hit)
            {
                hit = FindSpatialOnDevice(kind, payload, vid, pid);
            }
        }
        return FillHit(hit, out);
    }

    static int NameMatchScore(const std::string& lower, const char* key)
    {
        if(!key || key[0] == '\0' || lower.empty())
        {
            return 0;
        }
        const bool key_named = (lower.size() >= 4 && lower.compare(0, 4, "key:") == 0);
        const bool want_key = (std::strncmp(key, "Key:", 4) == 0 || std::strncmp(key, "key:", 4) == 0);
        if(NameEqualsKey(lower, key))
        {
            return key_named ? 100 : 80;
        }
        /* "Key: Enter" must not match "Key: Number Pad Enter". */
        if(key_named && want_key)
        {
            return 0;
        }
        if(TokensMatchAlias(lower, key))
        {
            return key_named ? 70 : 40;
        }
        return 0;
    }

    const ReactiveLedSample* FindNamed(ReactiveSourceKind kind,
                                       const char* primary,
                                       const char* secondary,
                                       uint16_t vid,
                                       uint16_t pid,
                                       const char* const* aliases = nullptr) const
    {
        auto score_sample = [&](const ReactiveLedSample& s) -> int {
            if(!TypeMatchesKind(s.type, kind) || s.name.empty())
            {
                return 0;
            }
            const std::string lower = ToLowerCopy(s.name);
            int score = NameMatchScore(lower, primary);
            score = std::max(score, NameMatchScore(lower, secondary));
            if(aliases)
            {
                for(const char* const* a = aliases; *a; ++a)
                {
                    score = std::max(score, NameMatchScore(lower, *a));
                }
            }
            if(score <= 0)
            {
                return 0;
            }
            if(s.type == DEVICE_TYPE_KEYPAD)
            {
                score += 50;
            }
            if(vid != 0 && pid != 0 && s.vid == vid && s.pid == pid)
            {
                score += 2000;
            }
            return score;
        };

        const ReactiveLedSample* best = nullptr;
        int best_score = 0;
        if(vid != 0 && pid != 0)
        {
            for(const ReactiveLedSample& s : samples)
            {
                if(s.vid != vid || s.pid != pid)
                {
                    continue;
                }
                const int score = score_sample(s);
                if(score > best_score)
                {
                    best_score = score;
                    best = &s;
                }
            }
            if(best)
            {
                return best;
            }
            return nullptr;
        }

        best = nullptr;
        best_score = 0;
        for(const ReactiveLedSample& s : samples)
        {
            const int score = score_sample(s);
            if(score > best_score)
            {
                best_score = score;
                best = &s;
            }
        }
        return best;
    }

    std::uintptr_t LargestDevice(ReactiveSourceKind kind, uint16_t vid, uint16_t pid) const
    {
        std::map<std::uintptr_t, int> mouse_counts;
        std::map<std::uintptr_t, int> other_counts;
        for(const ReactiveLedSample& s : samples)
        {
            if(!TypeMatchesKind(s.type, kind) || s.device_id == 0)
            {
                continue;
            }
            if(vid != 0 && pid != 0 && (s.vid != vid || s.pid != pid))
            {
                continue;
            }
            if(kind == ReactiveSourceKind::Mouse && s.type == DEVICE_TYPE_MOUSE)
            {
                mouse_counts[s.device_id] += 1;
            }
            else
            {
                other_counts[s.device_id] += 1;
            }
        }
        const auto pick = [](const std::map<std::uintptr_t, int>& counts) -> std::uintptr_t {
            std::uintptr_t best = 0;
            int best_n = 0;
            for(const auto& item : counts)
            {
                if(item.second > best_n)
                {
                    best_n = item.second;
                    best = item.first;
                }
            }
            return best;
        };
        if(const std::uintptr_t mouse = pick(mouse_counts))
        {
            return mouse;
        }
        return pick(other_counts);
    }

    const ReactiveLedSample* PickLocalExtreme(ReactiveSourceKind kind,
                                              int axis,
                                              bool pick_max,
                                              uint16_t vid,
                                              uint16_t pid) const
    {
        const std::uintptr_t device = LargestDevice(kind, vid, pid);
        const ReactiveLedSample* picked = nullptr;
        float best = 0.0f;
        bool have = false;
        for(const ReactiveLedSample& s : samples)
        {
            if(!TypeMatchesKind(s.type, kind))
            {
                continue;
            }
            if(vid != 0 && pid != 0 && (s.vid != vid || s.pid != pid))
            {
                continue;
            }
            if(device != 0 && s.device_id != device)
            {
                continue;
            }
            const float v = (axis == 1) ? s.local_position.y
                          : (axis == 2) ? s.local_position.z
                                        : s.local_position.x;
            if(!have || (pick_max ? v > best : v < best))
            {
                have = true;
                best = v;
                picked = &s;
            }
        }
        return picked;
    }

    const ReactiveLedSample* FindSpatialOnDevice(ReactiveSourceKind kind,
                                                 uint32_t payload,
                                                 uint16_t vid,
                                                 uint16_t pid) const
    {
        if(kind == ReactiveSourceKind::Mouse)
        {
            switch(payload)
            {
            case 1:
                return PickLocalExtreme(kind, 0, false, vid, pid);
            case 2:
                return PickLocalExtreme(kind, 0, true, vid, pid);
            case 3:
            {
                const ReactiveLedSample* wheel = PickLocalExtreme(kind, 1, true, vid, pid);
                if(wheel)
                {
                    return wheel;
                }
                return PickLocalExtreme(kind, 2, true, vid, pid);
            }
            default:
                return PickLocalExtreme(kind, 2, false, vid, pid);
            }
        }
        if(kind == ReactiveSourceKind::Gamepad)
        {
            switch(payload)
            {
            case 0x1000:
                return PickLocalExtreme(kind, 1, false, vid, pid);
            case 0x8000:
                return PickLocalExtreme(kind, 1, true, vid, pid);
            case 0x4000:
                return PickLocalExtreme(kind, 0, false, vid, pid);
            case 0x2000:
                return PickLocalExtreme(kind, 0, true, vid, pid);
            case 0x0004:
            case 0x0100:
            case 0x0400:
                return PickLocalExtreme(kind, 0, false, vid, pid);
            case 0x0008:
            case 0x0200:
            case 0x0800:
                return PickLocalExtreme(kind, 0, true, vid, pid);
            case 0x0001:
                return PickLocalExtreme(kind, 2, true, vid, pid);
            case 0x0002:
                return PickLocalExtreme(kind, 2, false, vid, pid);
            default:
                return PickLocalExtreme(kind, 1, false, vid, pid);
            }
        }
        return nullptr;
    }
};

ReactiveInputManager* ReactiveInputManager::instance()
{
    static ReactiveInputManager mgr;
    return &mgr;
}

ReactiveInputManager::ReactiveInputManager()
    : impl_(new Impl)
{
}

ReactiveInputManager::~ReactiveInputManager()
{
    stop();
    delete impl_;
    impl_ = nullptr;
}

bool ReactiveInputManager::isRunning() const
{
    return impl_ && impl_->running.load();
}

void ReactiveInputManager::start()
{
    if(!impl_ || impl_->running.load())
    {
        return;
    }
#ifdef _WIN32
    if(!impl_->sink)
    {
        impl_->sink = new Impl::SinkWidget();
        impl_->sink->winId();
    }
    if(!impl_->watcher)
    {
        impl_->watcher = new Impl::Watcher(impl_);
    }
    RAWINPUTDEVICE rid[3]{};
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage     = 0x06;
    rid[0].dwFlags     = RIDEV_INPUTSINK;
    rid[0].hwndTarget  = reinterpret_cast<HWND>(impl_->sink->winId());
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage     = 0x02;
    rid[1].dwFlags     = RIDEV_INPUTSINK;
    rid[1].hwndTarget  = rid[0].hwndTarget;
    rid[2].usUsagePage = 0x01;
    rid[2].usUsage     = 0x07;
    rid[2].dwFlags     = RIDEV_INPUTSINK;
    rid[2].hwndTarget  = rid[0].hwndTarget;
    if(!RegisterRawInputDevices(rid, 3, sizeof(RAWINPUTDEVICE)))
    {
        delete impl_->watcher;
        impl_->watcher = nullptr;
        return;
    }
#endif
    {
        QMutexLocker lock(&impl_->mutex);
        impl_->WipeLocked();
        impl_->running.store(true);
    }
}

void ReactiveInputManager::stop()
{
    if(!impl_ || !impl_->running.load())
    {
        return;
    }
    impl_->running.store(false);
#ifdef _WIN32
    RAWINPUTDEVICE rid[3]{};
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage     = 0x06;
    rid[0].dwFlags     = RIDEV_REMOVE;
    rid[0].hwndTarget  = nullptr;
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage     = 0x02;
    rid[1].dwFlags     = RIDEV_REMOVE;
    rid[1].hwndTarget  = nullptr;
    rid[2].usUsagePage = 0x01;
    rid[2].usUsage     = 0x07;
    rid[2].dwFlags     = RIDEV_REMOVE;
    rid[2].hwndTarget  = nullptr;
    RegisterRawInputDevices(rid, 3, sizeof(RAWINPUTDEVICE));
    delete impl_->watcher;
    impl_->watcher = nullptr;
    if(impl_->xinput_dll)
    {
        impl_->xinput_get_state = nullptr;
        FreeLibrary(impl_->xinput_dll);
        impl_->xinput_dll = nullptr;
    }
#endif
    QMutexLocker lock(&impl_->mutex);
    impl_->WipeLocked();
}

void ReactiveInputManager::SetLayoutSnapshot(std::vector<ReactiveLedSample> samples)
{
    if(!impl_)
    {
        return;
    }
    QMutexLocker lock(&impl_->mutex);
    impl_->samples = std::move(samples);
}

void ReactiveInputManager::DrainOriginEdges(std::vector<ReactiveOriginEvent>& out_edges)
{
    out_edges.clear();
    if(!impl_)
    {
        return;
    }
#ifdef _WIN32
    if(impl_->running.load())
    {
        impl_->PollXInput();
    }
#endif
    QMutexLocker lock(&impl_->mutex);
    if(!impl_->running.load())
    {
        return;
    }
    out_edges.reserve(static_cast<size_t>(impl_->edge_count));
    for(int i = 0; i < impl_->edge_count; ++i)
    {
        const EdgeSlot& slot = impl_->edges[static_cast<size_t>((impl_->edge_head + i) % kEdgeCap)];
        OriginHit hit{};
        if(!impl_->ResolvePacked(slot.packed, slot.vid, slot.pid, &hit))
        {
            continue;
        }
        ReactiveOriginEvent ev{};
        ev.room_position = hit.position;
        ev.kind = UnpackKind(slot.packed);
        ev.down = slot.down;
        ev.device_spread = hit.spread;
        ev.led_spacing = hit.spacing;
        out_edges.push_back(ev);
    }
    impl_->edge_head = 0;
    impl_->edge_count = 0;
}

void ReactiveInputManager::CopyHeldOrigins(std::vector<ReactiveHeldOrigin>& out_held)
{
    out_held.clear();
    if(!impl_)
    {
        return;
    }
    QMutexLocker lock(&impl_->mutex);
    if(!impl_->running.load())
    {
        return;
    }
    out_held.reserve(impl_->down.size());
    for(const auto& item : impl_->down)
    {
        const DownKey& key = item.second;
        OriginHit hit{};
        if(!impl_->ResolvePacked(key.packed, key.vid, key.pid, &hit))
        {
            continue;
        }
        ReactiveHeldOrigin held{};
        held.room_position = hit.position;
        held.kind = UnpackKind(key.packed);
        held.device_spread = hit.spread;
        held.led_spacing = hit.spacing;
        out_held.push_back(held);
    }
}
