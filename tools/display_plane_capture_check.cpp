// SPDX-License-Identifier: GPL-2.0-only
// g++ -std=c++17 -O2 -o /tmp/display_plane_capture_check tools/display_plane_capture_check.cpp && /tmp/display_plane_capture_check

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../DisplayPlaneCaptureLabels.h"

int main()
{
    int fails = 0;

    if(MonitorNumberFromSourceId("screen_0") != 1 ||
       MonitorNumberFromSourceId("screen_1") != 2 ||
       MonitorNumberFromSourceId("screen_9") != 10)
    {
        std::fprintf(stderr, "MonitorNumberFromSourceId failed\n");
        fails++;
    }
    if(MonitorNumberFromSourceId("") != 0 || MonitorNumberFromSourceId("display") != 0)
    {
        std::fprintf(stderr, "MonitorNumberFromSourceId should reject bad ids\n");
        fails++;
    }

    CaptureSourceInfo primary;
    primary.id = "screen_0";
    primary.name = "\\\\.\\DISPLAY1";
    primary.width = 1920;
    primary.height = 1080;
    primary.display_number = 1;
    primary.is_primary = true;
    primary.is_available = true;
    const std::string primary_label = FormatCaptureSourceLabel(primary);
    if(primary_label.find("Monitor 1") == std::string::npos ||
       primary_label.find("DISPLAY1") == std::string::npos ||
       primary_label.find("[Primary]") == std::string::npos ||
       primary_label.find("1920x1080") == std::string::npos)
    {
        std::fprintf(stderr, "primary label '%s'\n", primary_label.c_str());
        fails++;
    }

    CaptureSourceInfo second;
    second.id = "screen_1";
    second.name = "\\\\.\\DISPLAY2";
    second.width = 2560;
    second.height = 1440;
    second.display_number = 2;
    second.is_primary = false;
    second.is_available = true;
    const std::string second_label = FormatCaptureSourceLabel(second);
    if(second_label.find("Monitor 2") == std::string::npos ||
       second_label.find("[Primary]") != std::string::npos)
    {
        std::fprintf(stderr, "second label '%s'\n", second_label.c_str());
        fails++;
    }

    std::vector<CaptureSourceInfo> sources = {second, primary};
    SortCaptureSourcesByDisplayNumber(sources);
    if(sources.size() != 2 || sources[0].id != "screen_0" || sources[1].id != "screen_1")
    {
        std::fprintf(stderr, "sort order failed\n");
        fails++;
    }

    if(SuggestCaptureSourceId(sources, {}) != "screen_0")
    {
        std::fprintf(stderr, "empty used should pick primary\n");
        fails++;
    }
    if(SuggestCaptureSourceId(sources, {"screen_0"}) != "screen_1")
    {
        std::fprintf(stderr, "used primary should pick unused monitor 2, got '%s'\n",
                     SuggestCaptureSourceId(sources, {"screen_0"}).c_str());
        fails++;
    }
    if(SuggestCaptureSourceId({}, {}).empty() == false)
    {
        std::fprintf(stderr, "no sources should suggest empty\n");
        fails++;
    }

    if(fails != 0)
    {
        std::fprintf(stderr, "FAILED %d checks\n", fails);
        return 1;
    }
    std::printf("display_plane_capture_check: monitor labels, sort, suggest OK\n");
    return 0;
}
