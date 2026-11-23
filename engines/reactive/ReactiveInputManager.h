// SPDX-License-Identifier: GPL-2.0-only

#ifndef REACTIVEINPUTMANAGER_H
#define REACTIVEINPUTMANAGER_H

#include "ReactiveInputTypes.h"

#include <QObject>

#include <atomic>

class ReactiveInputManager : public QObject
{
    Q_OBJECT

public:
    static ReactiveInputManager* instance();

    void start();
    void stop();
    bool isRunning() const;

    /* Plugin-wide switch, saved in the plugin settings. Off: never read keyboard, mouse or
       gamepad, even while a Reactive effect plays. */
    void setInputAllowed(bool allowed);
    bool isInputAllowed() const { return input_allowed_.load(); }

    /* Only these kinds are read at all (the ticks of every playing Reactive effect). Changing
       them while running restarts reading with the new set. */
    void setWantedKinds(bool keyboard, bool mouse, bool gamepad);
    bool wantsAnyKind() const { return wanted_kinds_.load() != 0; }

    void SetLayoutSnapshot(std::vector<ReactiveLedSample> samples);

    void DrainOriginEdges(std::vector<ReactiveOriginEvent>& out_edges);
    void CopyOriginEdgesForFrame(std::uint64_t frame_key, std::vector<ReactiveOriginEvent>& out_edges);
    void CopyHeldOrigins(std::vector<ReactiveHeldOrigin>& out_held);

signals:
    void inputAllowedChanged(bool allowed);

private:
    ReactiveInputManager();
    ~ReactiveInputManager() override;

    ReactiveInputManager(const ReactiveInputManager&) = delete;
    ReactiveInputManager& operator=(const ReactiveInputManager&) = delete;

    class Impl;
    Impl* impl_ = nullptr;
    std::atomic<bool> input_allowed_{true};
    std::atomic<unsigned> wanted_kinds_{7u};
};

#endif
