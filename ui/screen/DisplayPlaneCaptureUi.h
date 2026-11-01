// SPDX-License-Identifier: GPL-2.0-only
// Shared UI copy for display-plane capture (Object Creator, Scene object, Screen Mirror, dialogs).

#ifndef DISPLAYPLANE_CAPTURE_UI_H
#define DISPLAYPLANE_CAPTURE_UI_H

#include <QCoreApplication>
#include <QString>

namespace DisplayPlaneCaptureUi
{

inline QString Caption()
{
    return QCoreApplication::translate("DisplayPlaneCaptureUi", "Screen capture:");
}

inline QString ComboToolTip()
{
    return QCoreApplication::translate(
        "DisplayPlaneCaptureUi",
        "Which display this plane captures (Monitor 1, Monitor 2, \u2026). "
        "Same list in Object Creator, Scene object edit, and Screen Mirror.");
}

inline QString RefreshToolTip()
{
    return QCoreApplication::translate(
        "DisplayPlaneCaptureUi",
        "Refresh the list of displays the OS currently reports.");
}

inline QString ObjectCreatorSubtitle()
{
    return QCoreApplication::translate(
        "DisplayPlaneCaptureUi",
        "Add virtual screens for Screen Mirror. Pick a screen capture source on each plane "
        "(Monitor 1 / Monitor 2, \u2026), then add it to the 3D view from Available Controllers. "
        "Capture can also be set on the Scene object panel when the plane is selected.");
}

inline QString ScreenMirrorAssignHint()
{
    return QCoreApplication::translate(
        "DisplayPlaneCaptureUi",
        "Each display plane picks a screen capture source (Monitor 1, Monitor 2, \u2026). "
        "Assign it here or on the plane in Object Creator / Scene object edit — same setting.");
}

} // namespace DisplayPlaneCaptureUi

#endif
