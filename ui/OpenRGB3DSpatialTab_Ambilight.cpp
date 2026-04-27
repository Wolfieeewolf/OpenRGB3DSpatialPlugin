// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "Effects3D/ScreenMirror/ScreenMirror.h"
#include <QPointer>

bool OpenRGB3DSpatialTab::IsAmbilightEffectClass(const std::string& class_name) const
{
    return class_name == "ScreenMirror";
}

bool OpenRGB3DSpatialTab::IsAmbilightEffectClass(const QString& class_name) const
{
    return class_name == QLatin1String("ScreenMirror");
}

void OpenRGB3DSpatialTab::ApplyAmbilightOriginVisibility(const QString& class_name)
{
    bool is_ambilight = IsAmbilightEffectClass(class_name);
    if(origin_label)
    {
        origin_label->setVisible(!is_ambilight);
    }
    if(effect_origin_combo)
    {
        effect_origin_combo->setVisible(!is_ambilight);
    }
}

void OpenRGB3DSpatialTab::ConfigureAmbilightRuntimeEffect(SpatialEffect3D* effect)
{
    ScreenMirror* screen_mirror = dynamic_cast<ScreenMirror*>(effect);
    if(screen_mirror && viewport)
    {
        connect(screen_mirror, &ScreenMirror::ScreenPreviewChanged,
                viewport, &LEDViewport3D::SetShowScreenPreview, Qt::UniqueConnection);
        connect(screen_mirror, &ScreenMirror::TestPatternChanged,
                viewport, &LEDViewport3D::SetShowTestPattern, Qt::UniqueConnection);
        screen_mirror->SetReferencePoints(&reference_points);

        QPointer<ScreenMirror> sm_ptr(screen_mirror);
        viewport->SetPerPlanePreviewQuery(
            [sm_ptr](const std::string& name) -> bool {
                return sm_ptr ? sm_ptr->ShouldShowScreenPreview(name) : false;
            },
            [sm_ptr](const std::string& name) -> bool {
                return sm_ptr ? sm_ptr->ShouldShowTestPattern(name) : false;
            });
    }
}

void OpenRGB3DSpatialTab::ConfigureAmbilightUIEffect(SpatialEffect3D* effect)
{
    ScreenMirror* screen_mirror = dynamic_cast<ScreenMirror*>(effect);
    if(screen_mirror)
    {
        screen_mirror->SetReferencePoints(&reference_points);
        connect(this, &OpenRGB3DSpatialTab::GridLayoutChanged, screen_mirror, &ScreenMirror::RefreshMonitorStatus);
        QTimer::singleShot(200, screen_mirror, &ScreenMirror::RefreshMonitorStatus);
        QTimer::singleShot(300, screen_mirror, &ScreenMirror::RefreshReferencePointDropdowns);
    }
}

void OpenRGB3DSpatialTab::RefreshAmbilightReferencePointDropdowns()
{
    for(unsigned int i = 0; i < effect_stack.size(); i++)
    {
        std::unique_ptr<EffectInstance3D>& inst = effect_stack[i];
        if(inst && IsAmbilightEffectClass(inst->effect_class_name) && inst->effect)
        {
            ScreenMirror* screen_mirror = dynamic_cast<ScreenMirror*>(inst->effect.get());
            if(screen_mirror)
            {
                screen_mirror->RefreshReferencePointDropdowns();
            }
        }
    }

    if(current_effect_ui)
    {
        ScreenMirror* screen_mirror = dynamic_cast<ScreenMirror*>(current_effect_ui);
        if(screen_mirror)
        {
            screen_mirror->RefreshReferencePointDropdowns();
        }
    }
}

void OpenRGB3DSpatialTab::ApplyAmbilightGridScale(float grid_scale_mm)
{
    if(!current_effect_ui)
    {
        return;
    }

    ScreenMirror* screen_mirror = qobject_cast<ScreenMirror*>(current_effect_ui);
    if(screen_mirror)
    {
        screen_mirror->SetGridScaleMM(grid_scale_mm);
    }
}
