// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "gw_capture_log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace
{
std::mutex g_mutex;
FILE* g_file = nullptr;
bool g_enabled = true;
bool g_full = false; /* OPENRGB_GW_CAPTURE=full */
char g_path[MAX_PATH] = {};
char g_latest[MAX_PATH] = {};
char g_exe[MAX_PATH] = {};
DWORD g_pid = 0;
std::uint64_t g_lines = 0;

constexpr int kDeviceSlots = 8;
constexpr unsigned long long kGridLogMinIntervalMs = 100;

struct DeviceLogSlot
{
    char name[24]{};
    unsigned long long last_grid_ms = 0;
    unsigned long long grid_logged = 0;
    unsigned long long grid_suppressed = 0;
    unsigned long long creates = 0;
    unsigned long long sets_ok = 0;
    unsigned long long sets_miss = 0;
    unsigned long long unhandled = 0;
};

DeviceLogSlot g_dev[kDeviceSlots]{};
unsigned long long g_api_lines = 0;
unsigned long long g_unhandled_total = 0;

static unsigned long long NowMs()
{
#ifdef _WIN32
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u{};
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (u.QuadPart / 10000ull) - 11644473600000ull;
#else
    return 0;
#endif
}

static void ColorRefToRgb(std::uint32_t c, float& r, float& g, float& b)
{
    r = (float)(c & 0xFF) / 255.0f;
    g = (float)((c >> 8) & 0xFF) / 255.0f;
    b = (float)((c >> 16) & 0xFF) / 255.0f;
}

static bool IsLit(std::uint32_t c)
{
    return (c & 0x00FFFFFFu) != 0;
}

static DeviceLogSlot* SlotFor(const char* device)
{
    if(!device || !device[0])
    {
        device = "?";
    }
    for(int i = 0; i < kDeviceSlots; ++i)
    {
        if(g_dev[i].name[0] && std::strcmp(g_dev[i].name, device) == 0)
        {
            return &g_dev[i];
        }
    }
    for(int i = 0; i < kDeviceSlots; ++i)
    {
        if(!g_dev[i].name[0])
        {
            std::snprintf(g_dev[i].name, sizeof(g_dev[i].name), "%s", device);
            return &g_dev[i];
        }
    }
    return &g_dev[kDeviceSlots - 1];
}

static void WriteLine(const char* fmt, ...)
{
    if(!g_file)
    {
        return;
    }
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(g_file,
                 "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\t%llu\t",
                 (unsigned)st.wYear,
                 (unsigned)st.wMonth,
                 (unsigned)st.wDay,
                 (unsigned)st.wHour,
                 (unsigned)st.wMinute,
                 (unsigned)st.wSecond,
                 (unsigned)st.wMilliseconds,
                 (unsigned long long)NowMs());
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_file, fmt, ap);
    va_end(ap);
    std::fputc('\n', g_file);
    ++g_lines;
    if(g_full || (g_lines % 16ull) == 0ull)
    {
        std::fflush(g_file);
    }
}

static void WriteGridUnlocked(DeviceLogSlot* slot,
                              const char* device,
                              int effect,
                              const std::uint32_t* colors,
                              int rows,
                              int cols,
                              unsigned long long suppressed_batch)
{
    if(!colors || rows <= 0 || cols <= 0)
    {
        WriteLine("grid\t%s\t%d\t%d\t%d\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tempty;suppressed=%llu",
                  device ? device : "?",
                  effect,
                  rows,
                  cols,
                  suppressed_batch);
        return;
    }

    double ar = 0, ag = 0, ab = 0;
    double lr = 0, lg = 0, lb = 0;
    double rr = 0, rg = 0, rb = 0;
    int an = 0, ln = 0, rn = 0;
    const int left_end = cols / 3;
    const int right_start = cols - left_end;

    for(int row = 0; row < rows; ++row)
    {
        for(int col = 0; col < cols; ++col)
        {
            const std::uint32_t c = colors[row * cols + col];
            if(!IsLit(c))
            {
                continue;
            }
            float r, g, b;
            ColorRefToRgb(c, r, g, b);
            ar += r;
            ag += g;
            ab += b;
            ++an;
            if(col < left_end)
            {
                lr += r;
                lg += g;
                lb += b;
                ++ln;
            }
            else if(col >= right_start)
            {
                rr += r;
                rg += g;
                rb += b;
                ++rn;
            }
        }
    }

    auto avg = [](double s, int n) -> float { return n ? (float)(s / (double)n) : 0.0f; };
    const float r = avg(ar, an);
    const float g = avg(ag, an);
    const float b = avg(ab, an);
    WriteLine("grid\t%s\t%d\t%d\t%d\t%d\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%s;suppressed=%llu",
              device ? device : "?",
              effect,
              rows,
              cols,
              an,
              r,
              g,
              b,
              ln ? avg(lr, ln) : r,
              ln ? avg(lg, ln) : g,
              ln ? avg(lb, ln) : b,
              rn ? avg(rr, rn) : r,
              rn ? avg(rg, rn) : g,
              rn ? avg(rb, rn) : b,
              g_full ? "full" : "sampled",
              suppressed_batch);

    if(g_full)
    {
        const int total = rows * cols;
        constexpr int kChunk = 32;
        for(int i = 0; i < total; i += kChunk)
        {
            char hex[3 * kChunk * 9 + 8]{};
            char* out = hex;
            const int end = (i + kChunk < total) ? (i + kChunk) : total;
            for(int j = i; j < end; ++j)
            {
                out += std::snprintf(out, 16, "%08X", (unsigned)colors[j]);
                if(j + 1 < end)
                {
                    *out++ = ' ';
                }
            }
            WriteLine("grid_raw\t%s\t%d\t%d\t%d\toff=%d\tcount=%d\t%s",
                      device ? device : "?",
                      effect,
                      rows,
                      cols,
                      i,
                      end - i,
                      hex);
        }
    }
    (void)slot;
}

} // namespace

namespace GwCapture
{

void BeginSession()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(g_file)
    {
        return;
    }

    char flag[16]{};
    const DWORD n = GetEnvironmentVariableA("OPENRGB_GW_CAPTURE", flag, sizeof(flag));
    if(n > 0 && (flag[0] == '0' || _stricmp(flag, "off") == 0))
    {
        g_enabled = false;
        return;
    }
    g_full = !(n > 0 && (_stricmp(flag, "sampled") == 0 || _stricmp(flag, "sample") == 0));
    g_enabled = true;
    for(int i = 0; i < kDeviceSlots; ++i)
    {
        g_dev[i] = DeviceLogSlot{};
    }
    g_api_lines = 0;
    g_unhandled_total = 0;
    g_lines = 0;

    g_pid = GetCurrentProcessId();
    g_exe[0] = '\0';
    char module[MAX_PATH]{};
    if(GetModuleFileNameA(nullptr, module, MAX_PATH) > 0)
    {
        const char* base = module;
        for(const char* p = module; *p; ++p)
        {
            if(*p == '\\' || *p == '/')
            {
                base = p + 1;
            }
        }
        std::snprintf(g_exe, sizeof(g_exe), "%s", base);
        for(char* p = g_exe; *p; ++p)
        {
            if(*p == ' ')
            {
                *p = '_';
            }
        }
    }
    else
    {
        std::snprintf(g_exe, sizeof(g_exe), "unknown");
    }

    char temp[MAX_PATH]{};
    if(GetTempPathA(MAX_PATH, temp) == 0)
    {
        std::snprintf(temp, MAX_PATH, ".\\");
    }
    std::snprintf(g_path, MAX_PATH, "%sopenrgb_gw_capture_%s_%lu.log", temp, g_exe, (unsigned long)g_pid);
    std::snprintf(g_latest, MAX_PATH, "%sopenrgb_gw_capture_latest.log", temp);

    if(fopen_s(&g_file, g_path, "w") != 0 || !g_file)
    {
        g_file = nullptr;
        return;
    }

    std::fprintf(g_file,
                 "# OpenRGB game-wrapper Chroma capture\n"
                 "# exe=%s pid=%lu path=%s mode=%s\n"
                 "# columns: local_time, unix_ms, kind, device, effect, rows, cols, lit, "
                 "avg_r,avg_g,avg_b, left_r,left_g,left_b, right_r,right_g,right_b, detail\n"
                 "# OPENRGB_GW_CAPTURE=0 off | full (default) | sampled (~10Hz/device)\n",
                 g_exe,
                 (unsigned long)g_pid,
                 g_path,
                 g_full ? "full" : "sampled");
    std::fflush(g_file);

    FILE* latest = nullptr;
    if(fopen_s(&latest, g_latest, "w") == 0 && latest)
    {
        std::fprintf(latest, "%s\n", g_path);
        std::fclose(latest);
    }
}

void EndSession()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(!g_file)
    {
        return;
    }
    WriteLine("session\tsummary\tapi=%llu\tunhandled=%llu\tmode=%s",
              (unsigned long long)g_api_lines,
              (unsigned long long)g_unhandled_total,
              g_full ? "full" : "sampled");
    for(int i = 0; i < kDeviceSlots; ++i)
    {
        if(!g_dev[i].name[0])
        {
            continue;
        }
        WriteLine("session\tdevice\t%s\tcreates=%llu\tsets_ok=%llu\tsets_miss=%llu\t"
                  "grids_logged=%llu\tgrids_suppressed=%llu\tunhandled=%llu",
                  g_dev[i].name,
                  (unsigned long long)g_dev[i].creates,
                  (unsigned long long)g_dev[i].sets_ok,
                  (unsigned long long)g_dev[i].sets_miss,
                  (unsigned long long)g_dev[i].grid_logged,
                  (unsigned long long)g_dev[i].grid_suppressed,
                  (unsigned long long)g_dev[i].unhandled);
    }
    WriteLine("session\tend\tlines=%llu", (unsigned long long)g_lines);
    std::fflush(g_file);
    std::fclose(g_file);
    g_file = nullptr;
}

void LogApi(const char* api, const char* detail)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(!g_enabled || !g_file)
    {
        return;
    }
    ++g_api_lines;
    WriteLine("api\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t%s\t%s",
              api ? api : "",
              detail ? detail : "");
}

void LogUnhandled(const char* api, int effect, const char* detail)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(!g_enabled || !g_file)
    {
        return;
    }
    ++g_unhandled_total;
    DeviceLogSlot* slot = SlotFor(api ? api : "?");
    ++slot->unhandled;
    WriteLine("unhandled\t%s\t%d\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t%s",
              api ? api : "?",
              effect,
              detail ? detail : "");
}

void LogStatic(const char* device, int effect, std::uint32_t color)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(!g_enabled || !g_file)
    {
        return;
    }
    float r, g, b;
    ColorRefToRgb(color, r, g, b);
    WriteLine("static\t%s\t%d\t1\t1\t%d\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t0x%08X",
              device ? device : "?",
              effect,
              IsLit(color) ? 1 : 0,
              r,
              g,
              b,
              r,
              g,
              b,
              r,
              g,
              b,
              (unsigned)color);
}

void LogStaticDual(const char* device, int effect, std::uint32_t c1, std::uint32_t c2)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(!g_enabled || !g_file)
    {
        return;
    }
    float r1, g1, b1, r2, g2, b2;
    ColorRefToRgb(c1, r1, g1, b1);
    ColorRefToRgb(c2, r2, g2, b2);
    WriteLine("static_dual\t%s\t%d\t1\t1\t%d\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\tc1=0x%08X;c2=0x%08X",
              device ? device : "?",
              effect,
              (IsLit(c1) || IsLit(c2)) ? 1 : 0,
              (r1 + r2) * 0.5f,
              (g1 + g2) * 0.5f,
              (b1 + b2) * 0.5f,
              r1,
              g1,
              b1,
              r2,
              g2,
              b2,
              (unsigned)c1,
              (unsigned)c2);
}

void LogGrid(const char* device, int effect, const std::uint32_t* colors, int rows, int cols)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(!g_enabled || !g_file)
    {
        return;
    }

    DeviceLogSlot* slot = SlotFor(device);
    const unsigned long long now = NowMs();
    if(!g_full && slot->last_grid_ms != 0 && (now - slot->last_grid_ms) < kGridLogMinIntervalMs)
    {
        ++slot->grid_suppressed;
        return;
    }
    const unsigned long long suppressed = slot->grid_suppressed;
    slot->grid_suppressed = 0;
    slot->last_grid_ms = now;
    ++slot->grid_logged;
    WriteGridUnlocked(slot, device, effect, colors, rows, cols, suppressed);
}

void NoteCreate(const char* device, int effect, bool stored)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    DeviceLogSlot* slot = SlotFor(device);
    ++slot->creates;
    (void)effect;
    (void)stored;
}

void NoteSetEffect(const char* device, bool found)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    DeviceLogSlot* slot = SlotFor(device);
    if(found)
    {
        ++slot->sets_ok;
    }
    else
    {
        ++slot->sets_miss;
    }
}

void NoteDeleteEffect()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_api_lines;
}

void LogParamDump(const char* api, int effect, const void* data, std::size_t bytes)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(!g_enabled || !g_file)
    {
        return;
    }
    /* Dump up to 256 bytes of effect params in 64-byte chunks. */
    const std::size_t n = bytes < 256 ? bytes : 256;
    const auto* p = static_cast<const unsigned char*>(data);
    for(std::size_t off = 0; off < n; off += 64)
    {
        char hex[3 * 64 + 8]{};
        char* out = hex;
        const std::size_t end = (off + 64 < n) ? (off + 64) : n;
        for(std::size_t i = off; i < end; ++i)
        {
            out += std::snprintf(out, 4, "%02X", p ? p[i] : 0);
            if(i + 1 < end)
            {
                *out++ = ' ';
            }
        }
        WriteLine("param\t%s\t%d\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\tbytes=%zu\toffset=%zu\t%s%s",
                  api ? api : "?",
                  effect,
                  bytes,
                  off,
                  hex,
                  bytes > 256 ? "…" : "");
    }
}

const char* SessionPath()
{
    return g_path;
}

} // namespace GwCapture
