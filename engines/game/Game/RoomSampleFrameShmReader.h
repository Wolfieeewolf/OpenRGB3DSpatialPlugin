// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMSAMPLEFRAMESHMREADER_H
#define ROOMSAMPLEFRAMESHMREADER_H

#include <atomic>
#include <cstdint>
#include <thread>

class RoomSampleFrameShmReader
{
public:
    RoomSampleFrameShmReader();
    ~RoomSampleFrameShmReader();

    void Start();
    void Stop();

    static bool TryApplyLatest();

private:
    void PollLoop();

    std::atomic<bool> running_{false};
    std::thread thread_;
};

#endif
