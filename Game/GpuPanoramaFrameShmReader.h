// SPDX-License-Identifier: GPL-2.0-only

#ifndef GPUPANORAMAFRAMESHMREADER_H
#define GPUPANORAMAFRAMESHMREADER_H

#include <atomic>
#include <cstdint>
#include <thread>

class GpuPanoramaFrameShmReader
{
public:
    GpuPanoramaFrameShmReader();
    ~GpuPanoramaFrameShmReader();

    void Start();
    void Stop();

    static bool TryApplyLatest();

private:
    void PollLoop();

    std::atomic<bool> running_{false};
    std::thread thread_;
};

#endif
