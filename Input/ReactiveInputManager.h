// SPDX-License-Identifier: GPL-2.0-only

#ifndef REACTIVEINPUTMANAGER_H
#define REACTIVEINPUTMANAGER_H

#include "ReactiveInputTypes.h"

#include <QObject>

class ReactiveInputManager : public QObject
{
    Q_OBJECT

public:
    static ReactiveInputManager* instance();

    void start();
    void stop();
    bool isRunning() const;

    void SetLayoutSnapshot(std::vector<ReactiveLedSample> samples);

    void DrainOriginEdges(std::vector<ReactiveOriginEvent>& out_edges);
    void CopyHeldOrigins(std::vector<ReactiveHeldOrigin>& out_held);

private:
    ReactiveInputManager();
    ~ReactiveInputManager() override;

    ReactiveInputManager(const ReactiveInputManager&) = delete;
    ReactiveInputManager& operator=(const ReactiveInputManager&) = delete;

    class Impl;
    Impl* impl_ = nullptr;
};

#endif
