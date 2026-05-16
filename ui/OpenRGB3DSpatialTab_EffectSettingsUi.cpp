// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"

#include "LogManager.h"
#include "EffectListManager3D.h"
#include "Effects3D/ScreenMirror/ScreenMirror.h"
#include <QTimer>
#include <QVBoxLayout>

SpatialEffectSettingsLayout OpenRGB3DSpatialTab::settingsLayoutForClass(const std::string& class_name) const
{
    if(class_name == "ScreenMirror")
    {
        return SpatialEffectSettingsLayout::CustomOnly;
    }

    const EffectRegistration3D reg = EffectListManager3D::get()->GetEffectInfo(class_name);
    if(reg.category == "Audio")
    {
        return SpatialEffectSettingsLayout::CommonNoTransport;
    }

    return SpatialEffectSettingsLayout::FullWithTransport;
}

OpenRGB3DSpatialTab::EffectSettingsUiMount OpenRGB3DSpatialTab::createEffectSettingsUi(QWidget* parent,
                                                                                      QBoxLayout* target_layout,
                                                                                      const std::string& class_name,
                                                                                      SpatialEffectSettingsLayout layout)
{
    EffectSettingsUiMount mount;
    if(!parent || class_name.empty())
    {
        return mount;
    }

    mount.effect = EffectListManager3D::get()->CreateEffect(class_name);
    if(!mount.effect)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create effect UI: %s", class_name.c_str());
        return mount;
    }

    mount.container = new QWidget(parent);
    auto* body_layout = new QVBoxLayout(mount.container);
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(4);

    mount.effect->setParent(mount.container);
    mount.effect->MountSettingsUi(mount.container, layout);

    if(target_layout)
    {
        target_layout->addWidget(mount.container);
    }

    return mount;
}

void OpenRGB3DSpatialTab::configureScreenMirrorEffectUi(SpatialEffect3D* effect)
{
    if(!effect)
    {
        return;
    }

    auto* screen_mirror = dynamic_cast<ScreenMirror*>(effect);
    if(!screen_mirror)
    {
        return;
    }

    screen_mirror->SetReferencePoints(&reference_points);
    connect(this, &OpenRGB3DSpatialTab::GridLayoutChanged, screen_mirror, &ScreenMirror::RefreshMonitorStatus);
    QTimer::singleShot(200, screen_mirror, &ScreenMirror::RefreshMonitorStatus);
    QTimer::singleShot(300, screen_mirror, &ScreenMirror::RefreshReferencePointDropdowns);
}

void OpenRGB3DSpatialTab::setStackLayerGlobalChromeVisible(bool visible)
{
    if(effect_zone_label)
    {
        effect_zone_label->setVisible(visible);
    }
    if(effect_zone_combo)
    {
        effect_zone_combo->setVisible(visible);
    }
    if(origin_label)
    {
        origin_label->setVisible(visible);
    }
    if(effect_origin_combo)
    {
        effect_origin_combo->setVisible(visible);
    }
    if(effect_bounds_label)
    {
        effect_bounds_label->setVisible(visible);
    }
    if(effect_bounds_combo)
    {
        effect_bounds_combo->setVisible(visible);
    }
}
