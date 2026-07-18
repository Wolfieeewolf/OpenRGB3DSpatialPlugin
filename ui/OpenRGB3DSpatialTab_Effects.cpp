// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "EffectGlobalSettingsPanel.h"
#include "EffectStackBlendRow.h"
#include "EffectTransportRow.h"
#include "EffectListManager3D.h"
#include "ZoneGrid3D.h"
#include "Effects3D/ScreenMirror/ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "PluginLog.h"
#include "PluginSettingsPaths.h"
#include "PluginUiUtils.h"
#include "ui_OpenRGB3DSpatialTab.h"
#include <QAbstractItemView>
#include <QFont>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <unordered_set>

namespace
{
struct DeferEffectPanelMapping
{
    QPointer<QScrollArea> scroll;
    QPointer<QWidget> viewport;
    QPointer<QWidget> content;
    QPointer<QWidget> panel;
    bool applied_dont_show_on_screen = false;

    DeferEffectPanelMapping(QScrollArea* sa, QWidget* detail, QWidget* controls)
        : scroll(sa)
        , viewport(sa ? sa->viewport() : nullptr)
        , content(detail)
        , panel(controls)
    {
        if(panel && !panel->testAttribute(Qt::WA_DontShowOnScreen))
        {
            panel->setAttribute(Qt::WA_DontShowOnScreen, true);
            applied_dont_show_on_screen = true;
        }
        if(scroll) scroll->setUpdatesEnabled(false);
        if(viewport) viewport->setUpdatesEnabled(false);
        if(content) content->setUpdatesEnabled(false);
        if(panel) panel->setUpdatesEnabled(false);
    }

    ~DeferEffectPanelMapping()
    {
        if(panel) panel->setUpdatesEnabled(true);
        if(content) content->setUpdatesEnabled(true);
        if(viewport) viewport->setUpdatesEnabled(true);
        if(scroll) scroll->setUpdatesEnabled(true);
        if(applied_dont_show_on_screen && panel)
        {
            panel->setAttribute(Qt::WA_DontShowOnScreen, false);
        }
        if(panel) panel->update();
        if(scroll) scroll->update();
    }
};
}

void OpenRGB3DSpatialTab::removeEffectFromStackClicked()
{
    if(!effectStackList()) return;
    int current_row = effectStackList()->currentRow();

    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        return;
    }

    if(effectStackList()->currentRow() == current_row)
    {
        LoadStackEffectControls(nullptr);
    }

    effect_stack.erase(effect_stack.begin() + current_row);

    if(effect_stack.empty())
    {
        if(effect_running) stopEffectClicked();
        if(effectConfigGroup()) effectConfigGroup()->setVisible(false);
    }

    UpdateEffectStackList();

    if(effect_stack.empty())
    {
        effectStackList()->setCurrentRow(-1);
    }
    else
    {
        int new_row = std::min(current_row, (int)effect_stack.size() - 1);
        effectStackList()->setCurrentRow(new_row);
        effectStackSelectionChanged(new_row);
    }

    SetLayoutDirty();

    RenderEffectStack();
}

void OpenRGB3DSpatialTab::effectStackItemDoubleClicked(QListWidgetItem*)
{
    if(!effectStackList()) return;
    int current_row = effectStackList()->currentRow();

    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        return;
    }

    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->enabled = !instance->enabled;

    UpdateEffectStackList();
    effectStackList()->setCurrentRow(current_row);
    SetLayoutDirty();
}

void OpenRGB3DSpatialTab::effectStackSelectionChanged(int index)
{
    if(!effectStackList()) return;
    if(index < 0 || index >= (int)effect_stack.size())
    {
        last_stack_selection_index = -1;
        if(effectConfigGroup()) effectConfigGroup()->setVisible(false);
        if(stackEffectTypeCombo()) stackEffectTypeCombo()->setEnabled(false);
        if(stackEffectZoneCombo()) stackEffectZoneCombo()->setEnabled(false);
        if(stack_effect_blend_combo) stack_effect_blend_combo->setEnabled(false);
        if(effectZoneCombo()) effectZoneCombo()->setEnabled(false);
        LoadStackEffectControls(nullptr);

        if(effectCombo())
        {
            QSignalBlocker combo_blocker(effectCombo());
            if(effectCombo()->count() > 0)
            {
                effectCombo()->setCurrentIndex(-1);
            }
        }

        if(start_effect_button)
        {
            disconnect(start_effect_button, nullptr, this, nullptr);
            start_effect_button = nullptr;
        }
        if(stop_effect_button)
        {
            disconnect(stop_effect_button, nullptr, this, nullptr);
            stop_effect_button = nullptr;
        }
        current_effect_ui = nullptr;
        UpdateEffectCombo();
        if(effectZoneCombo())
        {
            QSignalBlocker blocker(effectZoneCombo());
            effectZoneCombo()->setCurrentIndex(0);
        }
        UpdateAudioPanelVisibility();
        UpdateEffectStackRowSelectorVisibility();
        return;
    }

    if(effectConfigGroup()) effectConfigGroup()->setVisible(true);
    if(stackEffectTypeCombo()) stackEffectTypeCombo()->setEnabled(true);
    if(stackEffectZoneCombo()) stackEffectZoneCombo()->setEnabled(true);
    if(stack_effect_blend_combo) stack_effect_blend_combo->setEnabled(true);
    if(effectZoneCombo()) effectZoneCombo()->setEnabled(true);

    EffectInstance3D* instance = effect_stack[index].get();
    if(!instance) return;
    last_stack_selection_index = index;

    QSignalBlocker type_blocker(stackEffectTypeCombo() ? stackEffectTypeCombo() : nullptr);
    QSignalBlocker zone_blocker(stackEffectZoneCombo() ? stackEffectZoneCombo() : nullptr);
    QSignalBlocker blend_blocker(stack_effect_blend_combo ? stack_effect_blend_combo : nullptr);

    if(stackEffectTypeCombo())
    {
        if(!instance->effect_class_name.empty())
        {
            QString class_name = QString::fromStdString(instance->effect_class_name);
            int type_index = stackEffectTypeCombo()->findData(class_name);
            if(type_index >= 0)
                stackEffectTypeCombo()->setCurrentIndex(type_index);
            else
                stackEffectTypeCombo()->setCurrentIndex(0);
        }
        else
        {
            stackEffectTypeCombo()->setCurrentIndex(0);
        }
    }

    UpdateStackEffectZoneCombo();
    if(stackEffectZoneCombo())
    {
        int zone_index = stackEffectZoneCombo()->findData(instance->zone_index);
        if(zone_index >= 0)
            stackEffectZoneCombo()->setCurrentIndex(zone_index);
        else
            stackEffectZoneCombo()->setCurrentIndex(0);
    }

    if(stack_effect_blend_combo)
    {
        int blend_index = stack_effect_blend_combo->findData((int)instance->blend_mode);
        if(blend_index >= 0)
        {
            stack_effect_blend_combo->setCurrentIndex(blend_index);
        }
        else
        {
            stack_effect_blend_combo->setCurrentIndex(0);
        }
    }

    LoadStackEffectControls(instance);

    if(effectZoneCombo())
    {
        QSignalBlocker blocker(effectZoneCombo());
        int zone_combo_index = effectZoneCombo()->findData(instance->zone_index);
        if(zone_combo_index >= 0)
        {
            effectZoneCombo()->setCurrentIndex(zone_combo_index);
        }
    }

    if(effectCombo())
    {
        QSignalBlocker combo_blocker(effectCombo());
        if(index >= 0 && index < effectCombo()->count())
        {
            effectCombo()->setCurrentIndex(index);
        }
    }

    UpdateEffectCombo();
    UpdateAudioPanelVisibility();
    UpdateEffectStackRowSelectorVisibility();
}

void OpenRGB3DSpatialTab::stackEffectTypeChanged(int)
{
    if(!effectStackList() || !stackEffectTypeCombo()) return;
    int current_row = effectStackList()->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
        return;

    EffectInstance3D* instance = effect_stack[current_row].get();
    QString class_name = stackEffectTypeCombo()->currentData().toString();
    QString ui_name = stackEffectTypeCombo()->currentText();

    if(class_name.isEmpty())
    {
        instance->effect.reset();
        instance->saved_settings.reset();
        instance->effect_class_name = "";
        instance->name = "None";

        UpdateEffectStackList();
        LoadStackEffectControls(instance);
        SetLayoutDirty();
        UpdateAudioPanelVisibility();
        return;
    }

    instance->effect.reset();
    instance->saved_settings.reset();
    instance->effect_class_name = class_name.toStdString();
    instance->name              = ui_name.toStdString();

    UpdateEffectStackList();
    LoadStackEffectControls(instance);
    SetLayoutDirty();
    UpdateAudioPanelVisibility();
}

void OpenRGB3DSpatialTab::stackEffectZoneChanged(int)
{
    if(!effectStackList() || !stackEffectZoneCombo()) return;
    int current_row = effectStackList()->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
        return;

    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->zone_index = stackEffectZoneCombo()->currentData().toInt();

    UpdateEffectStackList();
    SetLayoutDirty();
}

void OpenRGB3DSpatialTab::stackEffectBlendChanged(int)
{
    if(!stack_effect_blend_combo || !effectStackList()) return;
    int current_row = effectStackList()->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        return;
    }

    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->blend_mode = (BlendMode)stack_effect_blend_combo->currentData().toInt();

    UpdateEffectStackList();
    SetLayoutDirty();
}

void OpenRGB3DSpatialTab::UpdateEffectStackList()
{
    if(!effectStackList()) return;
    int current_row = effectStackList()->currentRow();

    bool restore_signals = effectStackList()->blockSignals(true);
    effectStackList()->clear();

    for(unsigned int i = 0; i < effect_stack.size(); i++)
    {
        EffectInstance3D* instance = effect_stack[i].get();

        QString enabled_marker = instance->enabled ? "[ON] " : "[OFF] ";
        QString display_name   = QString::fromStdString(instance->GetDisplayName());

        QListWidgetItem* item = new QListWidgetItem(enabled_marker + display_name);
        effectStackList()->addItem(item);
    }

    if(current_row >= 0 && current_row < (int)effect_stack.size())
    {
        effectStackList()->setCurrentRow(current_row);
    }

    effectStackList()->blockSignals(restore_signals);

    UpdateEffectCombo();
}

void OpenRGB3DSpatialTab::UpdateStackEffectZoneCombo()
{
    PopulateZoneTargetCombo(stackEffectZoneCombo(), ResolveZoneTargetSelection(stackEffectZoneCombo()));
}

void OpenRGB3DSpatialTab::LoadStackEffectControls(EffectInstance3D* instance)
{
    UpdateAudioPanelVisibility();

    QScrollArea* detail_scroll = ui ? ui->effectsDetailScroll : nullptr;
    QWidget* scroll_content = detail_scroll ? detail_scroll->widget() : nullptr;
    DeferEffectPanelMapping mapping_guard(detail_scroll, scroll_content, effectControlsWidget());

    if(current_effect_ui)
    {
        disconnect(current_effect_ui, nullptr, this, nullptr);
        current_effect_ui = nullptr;
    }
    if(start_effect_button)
    {
        disconnect(start_effect_button, nullptr, this, nullptr);
        start_effect_button = nullptr;
    }
    if(stop_effect_button)
    {
        disconnect(stop_effect_button, nullptr, this, nullptr);
        stop_effect_button = nullptr;
    }
    QLayoutItem* layout_item;
    while(effectControlsLayout() && (layout_item = effectControlsLayout()->takeAt(0)) != nullptr)
    {
        if(QWidget* w = layout_item->widget())
        {
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete layout_item;
    }
    stack_effect_blend_combo = nullptr;
    stack_blend_container = nullptr;

    if(!instance)
    {
        if(effectControlsWidget())
        {
            effectControlsWidget()->setVisible(false);
        }
        SyncStackRoomOutputPanel();
        return;
    }

    if(instance->effect_class_name.empty())
    {
        if(effectControlsWidget())
        {
            effectControlsWidget()->setVisible(false);
        }
        return;
    }

    if(!instance->effect)
    {
        SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(instance->effect_class_name);
        if(!effect)
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create effect: %s", instance->effect_class_name.c_str());
            ClearCustomEffectUI();
            if(effectControlsWidget())
            {
                effectControlsWidget()->setVisible(false);
            }
            return;
        }
        instance->effect.reset(effect);

        if(instance->effect_class_name == "ScreenMirror")
        {
            ScreenMirror* screen_mirror = dynamic_cast<ScreenMirror*>(effect);
            if(screen_mirror && viewport)
            {
                connect(screen_mirror, &ScreenMirror::ScreenPreviewChanged,
                        viewport, &LEDViewport3D::SetShowScreenPreview, Qt::UniqueConnection);
                connect(screen_mirror, &ScreenMirror::CalibrationPatternChanged,
                        viewport, &LEDViewport3D::SetShowCalibrationPattern, Qt::UniqueConnection);
                screen_mirror->SetReferencePoints(&reference_points);

                QPointer<ScreenMirror> sm_ptr(screen_mirror);
                viewport->SetPerPlanePreviewQuery(
                    [sm_ptr](const std::string& name) -> bool {
                        return sm_ptr ? sm_ptr->ShouldShowScreenPreview(name) : false;
                    },
                    [sm_ptr](const std::string& name) -> bool {
                        return sm_ptr ? sm_ptr->ShouldShowCalibrationPattern(name) : false;
                    });
            }
        }

        if(instance->saved_settings && !instance->saved_settings->empty())
        {
            effect->LoadSettings(*instance->saved_settings);
        }
    }

    if(instance->effect)
    {
        if(!instance->saved_settings || instance->saved_settings->empty())
        {
            nlohmann::json current_settings = instance->effect->SaveSettings();
            instance->saved_settings = std::make_unique<nlohmann::json>(current_settings);
        }
    }

    DisplayEffectInstanceDetails(instance);

    if(effectControlsWidget())
    {
        effectControlsWidget()->setVisible(true);
    }
}

void OpenRGB3DSpatialTab::DisplayEffectInstanceDetails(EffectInstance3D* instance)
{
    if(!instance || !effectControlsWidget() || !effectControlsLayout())
    {
        return;
    }

    if(instance->effect_class_name.empty())
    {
        return;
    }

    const SpatialEffectSettingsLayout settings_layout = settingsLayoutForClass(instance->effect_class_name);
    const bool use_direct_transport = (settings_layout != SpatialEffectSettingsLayout::FullWithTransport);

    QPushButton* direct_start = nullptr;
    QPushButton* direct_stop  = nullptr;
    if(use_direct_transport)
    {
        auto* transport_row = new EffectTransportRow(effectControlsWidget());
        direct_start = transport_row->startEffectButton();
        direct_stop = transport_row->stopEffectButton();
        effectControlsLayout()->addWidget(transport_row);
    }

    EffectSettingsUiMount mount = createEffectSettingsUi(effectControlsWidget(),
                                                       effectControlsLayout(),
                                                       instance->effect_class_name,
                                                       settings_layout);
    SpatialEffect3D* ui_effect = mount.effect;
    if(!ui_effect)
    {
        return;
    }

    current_effect_ui = ui_effect;

    if(settings_layout == SpatialEffectSettingsLayout::CustomOnly)
    {
        configureScreenMirrorEffectUi(ui_effect);
    }

    setStackLayerGlobalChromeVisible(settings_layout == SpatialEffectSettingsLayout::FullWithTransport);

    nlohmann::json settings;
    if(instance->saved_settings && !instance->saved_settings->empty())
    {
        settings = *instance->saved_settings;
    }
    else if(instance->effect)
    {
        settings = instance->effect->SaveSettings();
    }

    if(!settings.is_null())
    {
        ui_effect->LoadSettings(settings);
    }

    if(effectBoundsCombo())
    {
        QSignalBlocker b(effectBoundsCombo());
        int idx = effectBoundsCombo()->findData(QVariant(ui_effect->GetEffectBoundsMode()));
        if(idx >= 0) effectBoundsCombo()->setCurrentIndex(idx);
    }

    QPushButton* ui_start = use_direct_transport ? direct_start : ui_effect->GetStartButton();
    QPushButton* ui_stop  = use_direct_transport ? direct_stop  : ui_effect->GetStopButton();

    if(ui_start)
    {
        connect(ui_start, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::startEffectClicked);
    }
    if(ui_stop)
    {
        connect(ui_stop, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::stopEffectClicked);
    }

    start_effect_button = ui_start;
    stop_effect_button  = ui_stop;

    if(start_effect_button)
    {
        start_effect_button->setEnabled(!effect_running);
    }
    if(stop_effect_button)
    {
        stop_effect_button->setEnabled(effect_running);
    }

    QPointer<SpatialEffect3D> captured_ui(ui_effect);
    connect(ui_effect, &SpatialEffect3D::ParametersChanged, this,
            [this, instance, captured_ui]()
            {
                if(!instance || captured_ui.isNull())
                {
                    return;
                }
                bool still_in_stack = false;
                for(const std::unique_ptr<EffectInstance3D>& p : effect_stack)
                {
                    if(p.get() == instance)
                    {
                        still_in_stack = true;
                        break;
                    }
                }
                if(!still_in_stack)
                {
                    return;
                }

                if(stack_settings_updating)
                {
                    return;
                }

                stack_settings_updating = true;

                nlohmann::json updated = captured_ui->SaveSettings();
                instance->saved_settings = std::make_unique<nlohmann::json>(updated);
                if(instance->effect)
                {
                    instance->effect->LoadSettings(updated);
                }
                SetLayoutDirty();
                RefreshEffectDisplay();

                stack_settings_updating = false;
            });

    stack_blend_container = new EffectStackBlendRow(effectControlsWidget());
    stack_effect_blend_combo = static_cast<EffectStackBlendRow*>(stack_blend_container)->blendCombo();
    connect(stack_effect_blend_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OpenRGB3DSpatialTab::stackEffectBlendChanged);

    effectControlsLayout()->addWidget(stack_blend_container);

    effectControlsWidget()->updateGeometry();
    effectControlsWidget()->update();

    int current_blend_index = stack_effect_blend_combo->findData((int)instance->blend_mode);
    if(current_blend_index < 0)
    {
        current_blend_index = 0;
    }
    {
        QSignalBlocker block(*stack_effect_blend_combo);
        stack_effect_blend_combo->setCurrentIndex(current_blend_index);
    }

    UpdateEffectStackRowSelectorVisibility();
    SyncStackRoomOutputPanel();
    SyncSpatialLightingSceneForUi();
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
    if(effectZoneLabel())
    {
        effectZoneLabel()->setVisible(visible);
    }
    if(effectZoneCombo())
    {
        effectZoneCombo()->setVisible(visible);
    }
    if(originLabel())
    {
        originLabel()->setVisible(visible);
    }
    if(effectOriginCombo())
    {
        effectOriginCombo()->setVisible(visible);
    }
    if(effectBoundsLabel())
    {
        effectBoundsLabel()->setVisible(visible);
    }
    if(effectBoundsCombo())
    {
        effectBoundsCombo()->setVisible(visible);
    }
    if(EffectGlobalSettingsPanel* global_panel = effectGlobalSettingsPanel())
    {
        global_panel->setRoomOutputSectionVisible(
            visible && current_effect_ui && current_effect_ui->GetEffectInfo().show_room_output_control);
    }
}

bool OpenRGB3DSpatialTab::IsAmbilightEffectClass(const std::string& class_name) const
{
    return class_name == "ScreenMirror";
}

bool OpenRGB3DSpatialTab::PlaybackUsesScreenMirror() const
{
    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        const std::unique_ptr<EffectInstance3D>& inst = effect_stack[i];
        if(inst && inst->enabled && inst->effect_class_name == "ScreenMirror")
        {
            return true;
        }
    }

    if(effect_stack.empty() && current_effect_ui)
    {
        return dynamic_cast<ScreenMirror*>(current_effect_ui) != nullptr;
    }

    return false;
}

void OpenRGB3DSpatialTab::SyncScreenCaptureSession()
{
    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
    const bool want_capture = effect_running && PlaybackUsesScreenMirror();
    capture_mgr.SetCaptureSessionActive(want_capture);
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

void OpenRGB3DSpatialTab::startEffectClicked()
{
    if(controller_transforms.empty())
    {
        QMessageBox::warning(this, "No Controllers", "Please add controllers to the 3D scene before starting effects.");
        return;
    }

    bool stack_has_entries = false;
    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        EffectInstance3D* instance = effect_stack[i].get();
        if(instance && !instance->effect_class_name.empty())
        {
            stack_has_entries = true;
            break;
        }
    }

    bool stack_ready = PrepareStackForPlayback();

    if(stack_has_entries)
    {
        if(!stack_ready)
        {
            QMessageBox::warning(this, "No Enabled Effects", "Enable at least one effect in the stack before starting.");
            return;
        }

        bool has_valid_controller = false;
        SetControllersToCustomMode(has_valid_controller);
        if(!has_valid_controller)
        {
            QMessageBox::warning(this, "No Valid Controllers", "No controllers are available for effects.");
            return;
        }

        effect_running = true;
        effect_time = 0.0f;
        effect_elapsed.restart();
        if(viewport)
        {
            viewport->SetEffectRenderOwnsScreenPreviewUploads(true);
        }

        if(effect_timer)
        {
            unsigned int target_fps = 30;
            for(size_t i = 0; i < effect_stack.size(); i++)
            {
                if(effect_stack[i])
                {
                    unsigned int fps = effect_stack[i]->GetEffectiveTargetFPS();
                    if(fps > target_fps)
                    {
                        target_fps = fps;
                    }
                }
            }
            if(target_fps < 1) target_fps = 30;
            bool stack_has_screen_mirror = false;
            for(size_t si = 0; si < effect_stack.size(); si++)
            {
                const std::unique_ptr<EffectInstance3D>& inst = effect_stack[si];
                if(inst && inst->enabled && inst->effect_class_name == "ScreenMirror")
                {
                    stack_has_screen_mirror = true;
                    break;
                }
            }
            if(stack_has_screen_mirror && target_fps < 120u)
            {
                target_fps = 120u;
            }
            int interval_ms = (int)(1000 / target_fps);
            if(interval_ms < 1) interval_ms = 1;
            effect_timer->start(interval_ms);
        }

        if(start_effect_button) start_effect_button->setEnabled(false);
        if(stop_effect_button) stop_effect_button->setEnabled(true);
        UpdateStartStopAllButtons();
        SyncScreenCaptureSession();
        return;
    }

    if(!current_effect_ui)
    {
        QMessageBox::warning(this, "No Effects", "Add an effect to the stack before starting.");
        return;
    }

    bool has_valid_controller = false;
    SetControllersToCustomMode(has_valid_controller);
    if(!has_valid_controller)
    {
        QMessageBox::warning(this, "No Valid Controllers", "No controllers are available for effects.");
        return;
    }

    effect_running = true;
    effect_time = 0.0f;
    effect_elapsed.restart();
    if(viewport)
    {
        viewport->SetEffectRenderOwnsScreenPreviewUploads(true);
    }

    if(effect_timer)
    {
        unsigned int target_fps = current_effect_ui->GetTargetFPS();
        if(target_fps < 1) target_fps = 30;
        if(dynamic_cast<ScreenMirror*>(current_effect_ui) && target_fps < 120u)
        {
            target_fps = 120u;
        }
        int interval_ms = (int)(1000 / target_fps);
        if(interval_ms < 1) interval_ms = 1;
        effect_timer->start(interval_ms);
    }

    if(start_effect_button) start_effect_button->setEnabled(false);
    if(stop_effect_button) stop_effect_button->setEnabled(true);
    SyncScreenCaptureSession();
}

void OpenRGB3DSpatialTab::stopEffectClicked()
{
    effect_running = false;
    if(viewport)
    {
        viewport->SetEffectRenderOwnsScreenPreviewUploads(false);
    }
    SyncScreenCaptureSession();
    if(effect_timer && effect_timer->isActive())
    {
        effect_timer->stop();
    }
    if(start_effect_button) start_effect_button->setEnabled(true);
    if(stop_effect_button) stop_effect_button->setEnabled(false);
    UpdateStartStopAllButtons();
    RenderEffectStack();
}

void OpenRGB3DSpatialTab::startAllEffectsClicked()
{
    startEffectClicked();
}

void OpenRGB3DSpatialTab::stopAllEffectsClicked()
{
    stopEffectClicked();
}

void OpenRGB3DSpatialTab::UpdateStartStopAllButtons()
{
    if(startAllEffectsButton())
        startAllEffectsButton()->setEnabled(!effect_running);
    if(stopAllEffectsButton())
        stopAllEffectsButton()->setEnabled(effect_running);
}

void OpenRGB3DSpatialTab::effectTimerTimeout()
{
    if(!effect_running)
    {
        return;
    }

    qint64 ms = effect_elapsed.isValid() ? effect_elapsed.restart() : 33;
    if(ms <= 0) { ms = 33; }
    float dt = static_cast<float>(ms) / 1000.0f;
    if(dt > 0.1f) dt = 0.1f;
    effect_time += dt;

    RenderEffectStack();
}

bool OpenRGB3DSpatialTab::RebuildEffectStackFromJson(const nlohmann::json& effects_array)
{
    LoadStackEffectControls(nullptr);
    effect_stack.clear();

    if(!effects_array.is_array())
    {
        return false;
    }

    bool loaded_any = false;
    for(unsigned int i = 0; i < effects_array.size(); i++)
    {
        std::unique_ptr<EffectInstance3D> instance = EffectInstance3D::FromJson(effects_array[i]);
        if(!instance)
        {
            continue;
        }
        if(!EffectListManager3D::get()->IsEffectRegistered(instance->effect_class_name))
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Skipping stack layer (effect no longer available): %s",
                        instance->effect_class_name.c_str());
            continue;
        }
        effect_stack.push_back(std::move(instance));
        loaded_any = true;
    }
    return loaded_any;
}

void OpenRGB3DSpatialTab::ApplyLoadedStackSelection(int desired_index)
{
    if(desired_index < 0 || desired_index >= (int)effect_stack.size())
    {
        desired_index = effect_stack.empty() ? -1 : 0;
    }

    bool restore_stack_list_signals = false;
    if(effectStackList())
    {
        restore_stack_list_signals = effectStackList()->blockSignals(true);
    }
    UpdateEffectStackList();

    if(effectStackList())
    {
        effectStackList()->setCurrentRow(desired_index);
    }

    if(!effect_stack.empty() && desired_index >= 0 && desired_index < (int)effect_stack.size())
    {
        EffectInstance3D* instance = effect_stack[desired_index].get();
        LoadStackEffectControls(instance);
        if(effectZoneCombo())
        {
            QSignalBlocker zb(effectZoneCombo());
            int zi = effectZoneCombo()->findData(instance->zone_index);
            if(zi >= 0)
            {
                effectZoneCombo()->setCurrentIndex(zi);
            }
        }
        if(effectCombo() && desired_index < effectCombo()->count())
        {
            QSignalBlocker cb(effectCombo());
            effectCombo()->setCurrentIndex(desired_index);
        }
        UpdateEffectCombo();
        if(effectCombo() && desired_index < effectCombo()->count())
        {
            QSignalBlocker cb(effectCombo());
            effectCombo()->setCurrentIndex(desired_index);
        }
        UpdateAudioPanelVisibility();
    }
    else
    {
        ClearCustomEffectUI();
    }

    if(effectStackList())
    {
        effectStackList()->blockSignals(restore_stack_list_signals);
    }
}

std::string OpenRGB3DSpatialTab::GetStackPresetsPath()
{
    if(!resource_manager)
    {
        return std::string();
    }
    PluginSettingsPaths::EnsurePluginDataLayout(resource_manager);
    return PluginSettingsPaths::PluginRoot(resource_manager).string();
}

void OpenRGB3DSpatialTab::LoadStackPresets()
{
    stack_presets.clear();

    const std::string plugin_root = GetStackPresetsPath();
    if(plugin_root.empty())
    {
        return;
    }

    const filesystem::path presets_path(plugin_root);
    if(!filesystem::exists(presets_path))
    {
        return;
    }

    std::error_code iter_ec;
    for(const filesystem::directory_entry& entry :
        filesystem::directory_iterator(presets_path, iter_ec))
    {
        if(iter_ec || !entry.is_regular_file() || !PluginSettingsPaths::IsStackPresetFile(entry.path()))
        {
            continue;
        }

        std::ifstream file(entry.path().string());
        if(!file.is_open())
        {
            continue;
        }

        try
        {
            nlohmann::json j;
            file >> j;
            file.close();

            std::unique_ptr<StackPreset3D> preset = StackPreset3D::FromJson(j);
            if(preset)
            {
                stack_presets.push_back(std::move(preset));
            }
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to load stack preset: %s - %s",
                      entry.path().string().c_str(),
                      e.what());
        }
    }

    UpdateStackPresetsList();
    UpdateEffectCombo();
}

void OpenRGB3DSpatialTab::SaveStackPresets()
{
    if(!resource_manager)
    {
        return;
    }

    PluginSettingsPaths::EnsurePluginDataLayout(resource_manager);

    for(unsigned int i = 0; i < stack_presets.size(); i++)
    {
        const filesystem::path file_path =
            PluginSettingsPaths::StackPresetFile(resource_manager, stack_presets[i]->name);
        const std::string filename = file_path.string();

        std::ofstream file(filename);
        if(file.is_open())
        {
            try
            {
                nlohmann::json j = stack_presets[i]->ToJson();
                file << j.dump(4);
                if(file.fail() || file.bad())
                {
                    LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write stack preset: %s", filename.c_str());
                }
                file.close();
            }
            catch(const std::exception& e)
            {
                LOG_ERROR("[OpenRGB3DSpatialPlugin] Exception while saving stack preset: %s - %s",
                          filename.c_str(),
                          e.what());
                file.close();
            }
        }
        else
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open stack preset file for writing: %s",
                      filename.c_str());
        }
    }
}

void OpenRGB3DSpatialTab::UpdateStackPresetsList()
{
    if(!stackPresetsList())
    {
        return;
    }

    int selected_row = stackPresetsList()->currentRow();
    stackPresetsList()->clear();

    for(unsigned int i = 0; i < stack_presets.size(); i++)
    {
        stackPresetsList()->addItem(QString::fromStdString(stack_presets[i]->name));
    }

    if(selected_row >= 0 && selected_row < stackPresetsList()->count())
    {
        stackPresetsList()->setCurrentRow(selected_row);
    }
}

void OpenRGB3DSpatialTab::saveStackPresetClicked()
{
    if(effect_stack.empty())
    {
        QMessageBox::information(this, "No Effects",
                                "Please add some effects to the stack before saving.");
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, "Save Stack Preset",
                                        "Enter preset name:", QLineEdit::Normal,
                                        "", &ok);

    if(!ok || name.isEmpty())
    {
        return;
    }

    std::string preset_name = name.toStdString();

    for(unsigned int i = 0; i < stack_presets.size(); i++)
    {
        if(stack_presets[i]->name == preset_name)
        {
            QMessageBox::StandardButton reply = QMessageBox::question(this, "Overwrite Preset",
                "A preset with this name already exists. Overwrite?",
                QMessageBox::Yes | QMessageBox::No);

            if(reply == QMessageBox::Yes)
            {
                stack_presets.erase(stack_presets.begin() + i);
            }
            else
            {
                return;
            }
            break;
        }
    }

    std::unique_ptr<StackPreset3D> preset = StackPreset3D::CreateFromStack(preset_name, effect_stack);
    stack_presets.push_back(std::move(preset));

    SaveStackPresets();

    UpdateStackPresetsList();
    UpdateEffectCombo();

    QMessageBox::information(this, "Success",
                            QString("Stack preset \"%1\" saved successfully!").arg(name));
}

void OpenRGB3DSpatialTab::loadStackPresetClicked()
{
    if(!stackPresetsList()) return;
    int current_row = stackPresetsList()->currentRow();

    if(current_row < 0 || current_row >= (int)stack_presets.size())
    {
        QMessageBox::information(this, "No Preset Selected",
                                "Please select a preset to load.");
        return;
    }

    StackPreset3D* preset = stack_presets[current_row].get();
    if(!preset) return;

    nlohmann::json effects_array = nlohmann::json::array();
    for(unsigned int i = 0; i < preset->effect_instances.size(); i++)
    {
        if(!preset->effect_instances[i]) continue;
        effects_array.push_back(preset->effect_instances[i]->ToJson());
    }

    RebuildEffectStackFromJson(effects_array);
    ApplyLoadedStackSelection(0);

    SetLayoutDirty();

    QMessageBox::information(this, "Success",
                            QString("Stack preset \"%1\" loaded successfully!")
                            .arg(QString::fromStdString(preset->name)));
}

void OpenRGB3DSpatialTab::deleteStackPresetClicked()
{
    if(!stackPresetsList()) return;
    int current_row = stackPresetsList()->currentRow();

    if(current_row < 0 || current_row >= (int)stack_presets.size())
    {
        QMessageBox::information(this, "No Preset Selected",
                                "Please select a preset to delete.");
        return;
    }

    StackPreset3D* preset = stack_presets[current_row].get();
    QString preset_name = QString::fromStdString(preset->name);

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Preset",
        QString("Are you sure you want to delete the preset \"%1\"?").arg(preset_name),
        QMessageBox::Yes | QMessageBox::No);

    if(reply != QMessageBox::Yes)
    {
        return;
    }

    if(resource_manager)
    {
        const filesystem::path file_path =
            PluginSettingsPaths::StackPresetFile(resource_manager, preset->name);
        if(filesystem::exists(file_path))
        {
            std::error_code remove_ec;
            filesystem::remove(file_path, remove_ec);
        }
    }

    stack_presets.erase(stack_presets.begin() + current_row);

    UpdateStackPresetsList();
    UpdateEffectCombo();

    QMessageBox::information(this, "Success",
                            QString("Stack preset \"%1\" deleted successfully!").arg(preset_name));
}

