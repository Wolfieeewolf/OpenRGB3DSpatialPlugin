// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif

#include "WinsockLifetime.h"

#include <mutex>

namespace
{
std::mutex g_mutex;
int g_refs = 0;
} // namespace

namespace WinsockLifetime
{

bool Acquire()
{
    std::lock_guard<std::mutex> lock(g_mutex);
#ifdef _WIN32
    if(g_refs == 0)
    {
        WSADATA wsa{};
        if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            return false;
        }
    }
#endif
    g_refs++;
    return true;
}

void Release()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(g_refs <= 0)
    {
        return;
    }
    g_refs--;
#ifdef _WIN32
    if(g_refs == 0)
    {
        WSACleanup();
    }
#endif
}

} // namespace WinsockLifetime
