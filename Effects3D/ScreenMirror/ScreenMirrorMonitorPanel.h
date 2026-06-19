// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCREENMIRRORMONITORPANEL_H
#define SCREENMIRRORMONITORPANEL_H

#include "ScreenMirror.h"
#include <QWidget>

class DisplayPlane3D;

namespace Ui {
class ScreenMirrorMonitorSettings;
}

class ScreenMirrorMonitorPanel : public QWidget
{
public:
    explicit ScreenMirrorMonitorPanel(QWidget* parent = nullptr);
    ~ScreenMirrorMonitorPanel() override;

    void initialize(ScreenMirror* effect,
                    ScreenMirror::MonitorSettings& settings,
                    DisplayPlane3D* plane,
                    bool has_capture_source);

    static void PopulateLedTrimHost(ScreenMirror* effect,
                                    ScreenMirror::MonitorSettings& settings,
                                    QWidget* host,
                                    bool has_capture);

    void ensureWhiteRolloffAndVibranceWired(ScreenMirror* effect,
                                            ScreenMirror::MonitorSettings& settings,
                                            bool has_capture_source);

private:
    Ui::ScreenMirrorMonitorSettings* ui = nullptr;
};

#endif
