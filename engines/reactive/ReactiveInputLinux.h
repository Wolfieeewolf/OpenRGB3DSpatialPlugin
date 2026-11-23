// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef REACTIVEINPUTLINUX_H
#define REACTIVEINPUTLINUX_H

#ifdef __linux__

#include "ReactiveInputTypes.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

/* Linux key, button and gamepad presses for Reactive, read from the kernel input devices
   (/dev/input/event*). Works the same under X11 and Wayland; needs read access to those
   devices, which normally means the user is in the "input" group.

   Presses come out in the Windows form the rest of Reactive already understands:
   keyboard = virtual key + set-1 scan code (+ E0 / Pause flags), mouse = button 1-5,
   gamepad = XInput button bit (0x0400 / 0x0800 for the triggers). */
namespace ReactiveLinuxInput
{

struct Edge
{
    ReactiveSourceKind kind = ReactiveSourceKind::Keyboard;
    std::uint32_t payload = 0;
    std::uint32_t pad = 0;
    bool down = false;
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
};

class Reader
{
public:
    using Sink = std::function<void(const Edge&)>;

    ~Reader();

    /* Starts the reader thread for the wanted kinds only; devices of other kinds are never
       opened. The sink is called from that thread. */
    void Start(Sink sink, bool keyboard, bool mouse, bool gamepad);
    void Stop();

private:
    void Run();

    Sink sink_;
    bool keyboard_ = true;
    bool mouse_ = true;
    bool gamepad_ = true;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

}

#endif

#endif
