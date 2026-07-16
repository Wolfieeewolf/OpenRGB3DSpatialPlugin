// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "AudioInputPanel.h"
#include "ControllerDisplayUtils.h"
#include "EffectControlsHostPanel.h"
#include "EffectGlobalSettingsPanel.h"
#include "EffectLibraryPanel.h"
#include "EffectStackPanel.h"
#include "GridSettingsPanel.h"
#include "ObjectCreatorTabPanel.h"
#include "ProfilesTabPanel.h"
#include "SceneTransformPanel.h"
#include "ui/widgets/EffectRoomOutputPanel.h"
#include "ZonesPanel.h"
#include "PluginSettingsPaths.h"
#include "SpatialControllerCardList.h"
#include "ui_OpenRGB3DSpatialTab.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QHideEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QShowEvent>
#include <QScrollArea>
#include <QPointer>
#include <QVBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include "LEDViewport3D.h"
#include "PluginUiUtils.h"
#include "ControllerLayout3D.h"
#include "LogManager.h"
#include "CustomControllerDialog.h"
#include "DisplayPlaneManager.h"
#include "Effects3D/ScreenMirror/ScreenMirror.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "GridSpaceUtils.h"
#include <QStackedWidget>
#include <fstream>
#include <algorithm>
#include <map>
#include <QList>
#include <QSignalBlocker>
#include <QColor>
#include <QFont>
#include <QSplitter>
#include <QFrame>
#include <QAbstractItemView>
#include <QApplication>
#include <QTimer>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include <QCoreApplication>
#include <QVariant>
#include <QInputDialog>
#include <set>
#include <QFileDialog>
#include <cstring>
#include <filesystem>
#include <cmath>
#include "SettingsManager.h"
#include "Audio/AudioInputManager.h"
#include "Zone3D.h"
#include "TooltipProxy.h"

namespace
{
void ResolveEffectLibraryItem(QListWidgetItem* item, QString& out_class, QString& out_ui)
{
    if(!item)
    {
        out_class.clear();
        out_ui.clear();
        return;
    }
    out_class = item->data(Qt::UserRole).toString();
    out_ui = item->text();
}
}

OpenRGB3DSpatialTab::OpenRGB3DSpatialTab(ResourceManagerInterface* rm, QWidget *parent) :
    QWidget(parent),
    resource_manager(rm)
{
    qRegisterMetaType<SpatialControllerEntryKey>("SpatialControllerEntryKey");

    zone_manager = std::make_unique<ZoneManager3D>();

    ui = new Ui::OpenRGB3DSpatialTab();
    ui->setupUi(this);

    QStyle* const base_style = QApplication::style();
    setStyle(new SpatialTooltipProxy(base_style));

    bindUiPanels();
    PluginSettingsPaths::EnsurePluginDataLayout(resource_manager);
    LoadDefaultLedSpacingFromSettings();
    LoadDevices();

    QTimer::singleShot(0, this, [this]() { RunDeferredStartupTasks(); });

    const int kStartupDelayMs = 2000;
    auto_load_timer = new QTimer(this);
    auto_load_timer->setSingleShot(true);
    connect(auto_load_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::TryAutoLoadLayout);
    auto_load_timer->start(kStartupDelayMs);

    effect_timer = new QTimer(this);
    effect_timer->setTimerType(Qt::CoarseTimer);
    connect(effect_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::effectTimerTimeout);

    if(resource_manager)
    {
        resource_manager->RegisterDetectionEndCallback(&OpenRGB3DSpatialTab::OnOpenRgbDetectionEnded, this);
    }
}

void OpenRGB3DSpatialTab::RunDeferredStartupTasks()
{
    if(deferred_startup_done_)
    {
        return;
    }
    deferred_startup_done_ = true;

    LoadCustomControllers();
    UpdateDisplayPlanesList();
    RefreshDisplayPlaneDetails();
    UpdateEffectZoneCombo();
    UpdateEffectOriginCombo();
}

OpenRGB3DSpatialTab::~OpenRGB3DSpatialTab()
{
    if(resource_manager)
    {
        resource_manager->UnregisterDetectionEndCallback(&OpenRGB3DSpatialTab::OnOpenRgbDetectionEnded, this);
    }

    if(AudioInputManager* audio = AudioInputManager::instance())
    {
        audio->stop();
        disconnect(audio, nullptr, this, nullptr);
    }

    SaveEffectStack();
    SavePluginUiSettings();

    if(auto_load_timer)
    {
        auto_load_timer->stop();
        delete auto_load_timer;
    }

    if(effect_timer)
    {
        effect_timer->stop();
        delete effect_timer;
    }

    delete ui;
    ui = nullptr;
}

void OpenRGB3DSpatialTab::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if(viewport)
    {
        viewport->SetViewportPaintingEnabled(true);
    }
}

void OpenRGB3DSpatialTab::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    if(viewport)
    {
        viewport->SetViewportPaintingEnabled(false);
    }
}

void OpenRGB3DSpatialTab::InitLedViewport()
{
    if(!ui)
    {
        return;
    }

    viewport = ui->ledViewport;
    if(!viewport)
    {
        return;
    }

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->SetGridDimensions(custom_grid_x, custom_grid_y, custom_grid_z);
    viewport->SetGridSnapEnabled(false);
    viewport->SetReferencePoints(&reference_points);
    viewport->SetDisplayPlanes(&display_planes);
    viewport->SetGridScaleMM(grid_scale_mm);
    viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
    viewport->SetScreenPreviewTickCallback({});

    const nlohmann::json startup_settings = GetPluginSettings();

    try
    {
        if(startup_settings.contains("Camera"))
        {
            const nlohmann::json& cam = startup_settings["Camera"];
            const float dist  = cam.value("Distance", 20.0f);
            const float yaw   = cam.value("Yaw", 45.0f);
            const float pitch = cam.value("Pitch", 30.0f);
            const float tx    = cam.value("TargetX", 0.0f);
            const float ty    = cam.value("TargetY", 0.0f);
            const float tz    = cam.value("TargetZ", 0.0f);
            viewport->SetCamera(dist, yaw, pitch, tx, ty, tz);
        }
        if(startup_settings.contains("Grid") && startup_settings["Grid"].contains("SnapEnabled"))
        {
            viewport->SetGridSnapEnabled(startup_settings["Grid"]["SnapEnabled"].get<bool>());
        }
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to restore camera/grid startup settings: %s", e.what());
    }

    connect(viewport, &LEDViewport3D::ControllerSelected, this, &OpenRGB3DSpatialTab::viewportControllerSelected);
    connect(viewport, &LEDViewport3D::ControllerPositionChanged, this, &OpenRGB3DSpatialTab::controllerPositionChanged);
    connect(viewport, &LEDViewport3D::ControllerRotationChanged, this, &OpenRGB3DSpatialTab::controllerRotationChanged);
    connect(viewport, &LEDViewport3D::ControllerDeleteRequested, this, &OpenRGB3DSpatialTab::removeControllerFromViewport);
    connect(viewport, &LEDViewport3D::ReferencePointSelected, this, [this](int index) {
        refPointSelected(index, false);
    });
    connect(viewport, &LEDViewport3D::DisplayPlaneSelected, this, &OpenRGB3DSpatialTab::viewportDisplayPlaneSelected);
    connect(viewport, &LEDViewport3D::RoomViewportSelected, this, [this](bool) {
        UpdateSelectionInfo();
    });
    connect(viewport, &LEDViewport3D::ReferencePointPositionChanged, this, &OpenRGB3DSpatialTab::refPointPositionChanged);
    connect(viewport, &LEDViewport3D::DisplayPlanePositionChanged, this, &OpenRGB3DSpatialTab::displayPlanePositionSignal);
    connect(viewport, &LEDViewport3D::DisplayPlaneRotationChanged, this, &OpenRGB3DSpatialTab::displayPlaneRotationSignal);

    viewport->installViewportKeyboardShortcuts(this);
}

void OpenRGB3DSpatialTab::bindUiPanels()
{
    if(!ui)
    {
        return;
    }

    const int kLeftPanelMinWidth = 280;

    ui->effectLibraryPanel->bindTab(this);
    ui->effectStackPanel->bindTab(this);
    ui->zonesPanel->bindTab(this);
    connect(this, &OpenRGB3DSpatialTab::GridLayoutChanged, this, &OpenRGB3DSpatialTab::SyncSpatialLightingSceneForUi);

    ui->availableControllersPanel->bindTab(this, ControllerListPanel::Mode::Available);
    ui->controllersInScenePanel->bindTab(this, ControllerListPanel::Mode::InScene);
    ui->setupContentLayout->setStretch(0, 1);
    ui->setupContentLayout->setStretch(1, 2);
    ui->middlePanelLayout->setStretch(0, 0);
    ui->middlePanelLayout->setStretch(1, 1);
    connect(ui->setupLeftTabs, &QTabWidget::currentChanged, this, [this](int) {
        if(ui->setupLeftTabs && ui->setupLeftTabs->currentWidget() == ui->availableControllersPanel)
        {
            UpdateDeviceList();
        }
    });

    const nlohmann::json startup_settings = GetPluginSettings();

    try
    {
        if(startup_settings.contains("Grid"))
        {
            const nlohmann::json& g = startup_settings["Grid"];
            custom_grid_x = std::max(1, std::min(100, (int)g.value("X", custom_grid_x)));
            custom_grid_y = std::max(1, std::min(100, (int)g.value("Y", custom_grid_y)));
            custom_grid_z = std::max(1, std::min(100, (int)g.value("Z", custom_grid_z)));
            grid_scale_mm = (float)std::max(0.1, std::min(1000.0, (double)g.value("ScaleMM", grid_scale_mm)));
        }
        if(startup_settings.contains("Room"))
        {
            const nlohmann::json& r = startup_settings["Room"];
            use_manual_room_size = r.value("UseManual", use_manual_room_size);
            manual_room_width  = (float)r.value("WidthMM", manual_room_width);
            manual_room_height = (float)r.value("HeightMM", manual_room_height);
            manual_room_depth  = (float)r.value("DepthMM", manual_room_depth);
        }
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to restore grid/room startup settings: %s", e.what());
    }

    InitLedViewport();

    if(viewport)
    {
        viewport->SetViewportPaintingEnabled(isVisible());
    }

    BindSettingsPanels();

    ui->effectGlobalSettingsPanel->bindTab(this);
    SetupStackRoomOutputPanel();
    ui->effectControlsHostPanel->bindTab(this);
    ui->audioInputPanel->bindTab(this);

    LoadStackPresets();
    QTimer::singleShot(0, this, [this]() {
        LoadEffectStack();
        UpdateEffectStackList();
        if(!effect_stack.empty() && effectStackList())
        {
            const QSignalBlocker stack_sel_block(effectStackList());
            effectStackList()->setCurrentRow(0);
            effectStackSelectionChanged(0);
        }
        UpdateStartStopAllButtons();
    });

    ui->effectControlsHostPanel->updateGeometry();

    connect(ui->leftModeTabs, QOverload<int>::of(&QTabWidget::currentChanged), this,
            [this](int index) {
                if(ui->rightStacked)
                {
                    ui->rightStacked->setCurrentIndex(index);
                }
                SavePluginUiSettings();
            });
    try
    {
        if(startup_settings.contains("Ui") && startup_settings["Ui"].contains("LeftModeTab") && ui->leftModeTabs)
        {
            const int count = ui->leftModeTabs->count();
            if(count > 0)
            {
                int idx = startup_settings["Ui"]["LeftModeTab"].get<int>();
                idx       = std::max(0, std::min(count - 1, idx));
                const QSignalBlocker block(ui->leftModeTabs);
                ui->leftModeTabs->setCurrentIndex(idx);
                if(ui->rightStacked)
                {
                    ui->rightStacked->setCurrentIndex(idx);
                }
            }
        }
        else if(ui->rightStacked && ui->leftModeTabs)
        {
            ui->rightStacked->setCurrentIndex(ui->leftModeTabs->currentIndex());
        }
    }
    catch(const std::exception&)
    {
        if(ui->rightStacked && ui->leftModeTabs)
        {
            ui->rightStacked->setCurrentIndex(ui->leftModeTabs->currentIndex());
        }
    }

    QList<int> splitter_sizes;
    splitter_sizes << kLeftPanelMinWidth << 400 << 320;
    ui->mainSplitter->setSizes(splitter_sizes);
}

void OpenRGB3DSpatialTab::PopulateEffectLibraryCategories()
{
    if(!effectCategoryCombo())
    {
        return;
    }

    bool restore_signals = effectCategoryCombo()->blockSignals(true);
    effectCategoryCombo()->clear();
    effectCategoryCombo()->addItem(tr("Select Category"), QVariant());

    std::map<std::string, std::vector<EffectRegistration3D>> categorized = EffectListManager3D::get()->GetCategorizedEffects();
    for(std::map<std::string, std::vector<EffectRegistration3D>>::const_iterator cit = categorized.begin();
        cit != categorized.end();
        ++cit)
    {
        effectCategoryCombo()->addItem(QString::fromStdString(cit->first), QString::fromStdString(cit->first));
    }
    effectCategoryCombo()->blockSignals(restore_signals);
    effectCategoryCombo()->setCurrentIndex(0);
    UpdateEffectLibraryGameFilterVisibility();
    PopulateEffectLibrary();
}

void OpenRGB3DSpatialTab::UpdateEffectLibraryGameFilterVisibility()
{
    bool show_game_filter = false;
    if(effectCategoryCombo())
    {
        const QVariant category_data = effectCategoryCombo()->currentData();
        show_game_filter = category_data.isValid()
                           && category_data.toString().compare(QStringLiteral("Game"), Qt::CaseInsensitive) == 0;
    }

    if(ui && ui->effectLibraryPanel)
    {
        if(QLabel* label = ui->effectLibraryPanel->gameLabel())
        {
            label->setVisible(show_game_filter);
        }
        if(QComboBox* combo = ui->effectLibraryPanel->gameCombo())
        {
            combo->setVisible(show_game_filter);
        }
    }
}

void OpenRGB3DSpatialTab::PopulateEffectLibraryGames()
{
    QComboBox* game_combo = effectLibraryGameCombo();
    if(!game_combo)
    {
        return;
    }

    const QString preserve_game_id = game_combo->currentData().toString();
    const bool restore_signals = game_combo->blockSignals(true);
    game_combo->clear();
    game_combo->addItem(tr("Select game"), QVariant());

    const std::vector<std::pair<std::string, std::string>> games =
        EffectListManager3D::get()->GetRegisteredGames();
    for(const std::pair<std::string, std::string>& game : games)
    {
        game_combo->addItem(QString::fromStdString(game.second), QString::fromStdString(game.first));
    }

    int select_index = preserve_game_id.isEmpty() ? -1 : game_combo->findData(preserve_game_id);
    if(select_index < 0)
    {
        select_index = games.empty() ? 0 : 1;
    }
    game_combo->setCurrentIndex(select_index);
    game_combo->blockSignals(restore_signals);
}

bool OpenRGB3DSpatialTab::PrepareStackForPlayback()
{
    bool has_enabled_effect = false;

    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        EffectInstance3D* instance = effect_stack[i].get();
        if(!instance)
        {
            continue;
        }
        if(!instance->enabled)
        {
            continue;
        }
        if(instance->effect_class_name.empty())
        {
            continue;
        }

        if(!instance->effect)
        {
            SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(instance->effect_class_name);
            if(!effect)
            {
                LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create effect: %s", instance->effect_class_name.c_str());
                continue;
            }
            instance->effect.reset(effect);
            if(instance->saved_settings && !instance->saved_settings->empty())
            {
                effect->LoadSettings(*instance->saved_settings);
            }
        }

        if(instance->effect)
        {
            has_enabled_effect = true;
        }
    }

    return has_enabled_effect;
}

void OpenRGB3DSpatialTab::SetControllersToCustomMode(bool& has_valid_controller)
{
    has_valid_controller = false;

    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform)
        {
            continue;
        }

        if(transform->virtual_controller)
        {
            VirtualController3D* virtual_ctrl = transform->virtual_controller;
            const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();
            std::set<RGBController*> controllers_to_set;
            for(unsigned int i = 0; i < mappings.size(); i++)
            {
                if(mappings[i].controller)
                {
                    controllers_to_set.insert(mappings[i].controller);
                }
            }

            for(std::set<RGBController*>::iterator it = controllers_to_set.begin(); it != controllers_to_set.end(); ++it)
            {
                (*it)->SetCustomMode();
                has_valid_controller = true;
            }
            continue;
        }

        RGBController* controller = transform->controller;
        if(!controller)
        {
            continue;
        }

        controller->SetCustomMode();
        has_valid_controller = true;
    }
}

void OpenRGB3DSpatialTab::PopulateEffectLibrary()
{
    if(!effectLibraryList())
    {
        return;
    }

    UpdateEffectLibraryGameFilterVisibility();

    bool restore_signals = effectLibraryList()->blockSignals(true);
    effectLibraryList()->clear();

    QVariant category_data;
    if(effectCategoryCombo())
    {
        category_data = effectCategoryCombo()->currentData();
    }

    if(!category_data.isValid())
    {
        effectLibraryList()->blockSignals(restore_signals);
        effectLibrarySelectionChanged(-1);
        return;
    }

    QString selected_category = category_data.toString();
    QString search = effectLibrarySearch() ? effectLibrarySearch()->text().trimmed() : QString();

    std::map<std::string, std::vector<EffectRegistration3D>> categorized = EffectListManager3D::get()->GetCategorizedEffects();
    std::map<std::string, std::vector<EffectRegistration3D>>::iterator cat_it =
        categorized.find(selected_category.toStdString());
    if(cat_it == categorized.end())
    {
        effectLibraryList()->blockSignals(restore_signals);
        effectLibrarySelectionChanged(-1);
        return;
    }

    const std::vector<EffectRegistration3D>& effects = cat_it->second;

    struct LibRow
    {
        QString text;
        QString class_name;
        QString category;
    };
    std::vector<LibRow> rows;
    const bool game_category = (selected_category.compare(QStringLiteral("Game"), Qt::CaseInsensitive) == 0);
    const bool audio_category = (selected_category.compare(QStringLiteral("Audio"), Qt::CaseInsensitive) == 0);

    QString selected_game_id;
    if(game_category)
    {
        QComboBox* game_combo = effectLibraryGameCombo();
        if(game_combo)
        {
            if(!search.isEmpty())
            {
                for(int i = 1; i < game_combo->count(); ++i)
                {
                    if(game_combo->itemText(i).contains(search, Qt::CaseInsensitive))
                    {
                        game_combo->setCurrentIndex(i);
                        break;
                    }
                }
            }
            selected_game_id = game_combo->currentData().toString();
        }
    }

    for(const EffectRegistration3D& reg : effects)
    {
        if(game_category)
        {
            if(selected_game_id.isEmpty() || QString::fromStdString(reg.game_id) != selected_game_id)
            {
                continue;
            }
            if(!search.isEmpty())
            {
                const QString ui_name = QString::fromStdString(reg.ui_name);
                const QString game_label = QString::fromStdString(reg.game_label);
                if(!ui_name.contains(search, Qt::CaseInsensitive)
                   && !game_label.contains(search, Qt::CaseInsensitive))
                {
                    continue;
                }
            }
        }
        else if(!search.isEmpty() && !QString::fromStdString(reg.ui_name).contains(search, Qt::CaseInsensitive))
        {
            continue;
        }

        rows.push_back({QString::fromStdString(reg.ui_name), QString::fromStdString(reg.class_name), selected_category});
    }

    std::sort(rows.begin(), rows.end(), [](const LibRow& a, const LibRow& b) {
        return QString::localeAwareCompare(a.text, b.text) < 0;
    });

    for(const LibRow& r : rows)
    {
        QListWidgetItem* item = new QListWidgetItem(r.text);
        if(audio_category)
        {
            item->setToolTip(tr(
                "Adds this audio-reactive effect as its own stack layer. "
                "Use Audio Input below to start capture; set Low/High Hz in the effect settings."));
        }
        else if(game_category)
        {
            item->setToolTip(tr("Adds this game effect as its own stack layer. Configure it in the panel on the right."));
        }
        else
        {
            item->setToolTip(QStringLiteral("Category: %1").arg(r.category));
        }
        item->setData(Qt::UserRole, r.class_name);
        item->setData(Qt::UserRole + 1, r.category);
        effectLibraryList()->addItem(item);
    }

    effectLibraryList()->blockSignals(restore_signals);
    effectLibraryList()->setCurrentRow(-1);
    effectLibrarySelectionChanged(-1);
}

void OpenRGB3DSpatialTab::effectLibrarySearchChanged(const QString&)
{
    PopulateEffectLibrary();
}

void OpenRGB3DSpatialTab::AddEffectInstanceToStack(const QString& class_name,
                                                   const QString& ui_name,
                                                   int zone_index,
                                                   BlendMode blend_mode,
                                                   const nlohmann::json* preset_settings,
                                                   bool enabled)
{
    if(class_name.isEmpty())
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Cannot add effect - effect class name is empty");
        return;
    }

    std::unique_ptr<EffectInstance3D> instance = std::make_unique<EffectInstance3D>();
    instance->id            = next_effect_instance_id++;
    instance->name          = ui_name.toStdString();
    instance->zone_index    = zone_index;
    instance->blend_mode    = blend_mode;
    instance->enabled       = enabled;
    instance->effect_class_name = class_name.toStdString();

    if(preset_settings && !preset_settings->is_null())
    {
        instance->saved_settings = std::make_unique<nlohmann::json>(*preset_settings);
    }

    effect_stack.push_back(std::move(instance));
    const int new_index = (int)effect_stack.size() - 1;

    bool restore_stack_list_signals = false;
    if(effectStackList())
    {
        restore_stack_list_signals = effectStackList()->blockSignals(true);
    }
    UpdateEffectStackList();
    EffectInstance3D* new_instance = effect_stack[new_index].get();

    if(new_instance)
    {
        LoadStackEffectControls(new_instance);
        if(effectZoneCombo())
        {
            QSignalBlocker zb(effectZoneCombo());
            int zone_combo_index = effectZoneCombo()->findData(new_instance->zone_index);
            if(zone_combo_index >= 0)
            {
                effectZoneCombo()->setCurrentIndex(zone_combo_index);
            }
        }
        if(effectCombo() && new_index < effectCombo()->count())
        {
            QSignalBlocker cb(effectCombo());
            effectCombo()->setCurrentIndex(new_index);
        }
        UpdateEffectCombo();
        UpdateAudioPanelVisibility();
    }
    if(effectStackList())
    {
        effectStackList()->setCurrentRow(new_index);
        effectStackList()->blockSignals(restore_stack_list_signals);
    }
    effectStackSelectionChanged(new_index);
    SaveEffectStack();
}

void OpenRGB3DSpatialTab::effectLibraryCategoryChanged(int)
{
    UpdateEffectLibraryGameFilterVisibility();
    if(effectCategoryCombo())
    {
        const QVariant category_data = effectCategoryCombo()->currentData();
        if(category_data.isValid()
           && category_data.toString().compare(QStringLiteral("Game"), Qt::CaseInsensitive) == 0)
        {
            PopulateEffectLibraryGames();
        }
    }
    PopulateEffectLibrary();
}

void OpenRGB3DSpatialTab::effectLibraryGameChanged(int)
{
    PopulateEffectLibrary();
}

void OpenRGB3DSpatialTab::effectLibrarySelectionChanged(int row)
{
    const int si = effectStackList() ? effectStackList()->currentRow() : -1;
    if(si >= 0 && si < (int)effect_stack.size())
    {
        LoadStackEffectControls(effect_stack[si].get());
    }
    else
    {
        LoadStackEffectControls(nullptr);
        if(effectConfigGroup())
        {
            effectConfigGroup()->setVisible(false);
        }
    }

    if(effectLibraryAddButton())
    {
        effectLibraryAddButton()->setEnabled(row >= 0);
    }

    UpdateEffectStackRowSelectorVisibility();
}

void OpenRGB3DSpatialTab::effectLibraryAddClicked()
{
    if(!effectLibraryList())
    {
        return;
    }

    int current_row = effectLibraryList()->currentRow();
    if(current_row < 0)
    {
        return;
    }
    QListWidgetItem* item = effectLibraryList()->item(current_row);
    if(!item)
    {
        return;
    }

    QString class_name;
    QString ui_name;
    ResolveEffectLibraryItem(item, class_name, ui_name);
    if(class_name.isEmpty())
    {
        return;
    }
    AddEffectInstanceToStack(class_name, ui_name);
}

void OpenRGB3DSpatialTab::effectLibraryItemDoubleClicked(QListWidgetItem* item)
{
    if(!item)
    {
        return;
    }
    QString class_name;
    QString ui_name;
    ResolveEffectLibraryItem(item, class_name, ui_name);
    if(class_name.isEmpty())
    {
        return;
    }
    AddEffectInstanceToStack(class_name, ui_name);
}

void OpenRGB3DSpatialTab::UpdateEffectStackRowSelectorVisibility()
{
    if(!effectCombo() || !effectConfigGroup())
    {
        return;
    }
    const bool show_effect_row = effectConfigGroup()->isVisible();
    if(effectStackRowLabel())
    {
        effectStackRowLabel()->setVisible(show_effect_row);
    }
    effectCombo()->setVisible(show_effect_row);
}

void OpenRGB3DSpatialTab::ClearCustomEffectUI()
{
    if(!effectControlsLayout())
    {
        return;
    }

    if(effect_timer && effect_timer->isActive())
    {
        effect_timer->stop();
    }
    effect_running = false;
    SyncScreenCaptureSession();

    if(current_effect_ui)
    {
        disconnect(current_effect_ui, nullptr, this, nullptr);
    }

    if(start_effect_button)
    {
        disconnect(start_effect_button, nullptr, this, nullptr);
    }
    if(stop_effect_button)
    {
        disconnect(stop_effect_button, nullptr, this, nullptr);
    }

    current_effect_ui = nullptr;
    start_effect_button = nullptr;
    stop_effect_button = nullptr;

    QLayoutItem* item;
    while((item = effectControlsLayout()->takeAt(0)) != nullptr)
    {
        if(QWidget* w = item->widget())
        {
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }

    if(effectControlsWidget())
    {
        effectControlsWidget()->setVisible(false);
    }
}

void OpenRGB3DSpatialTab::gridDimensionsChanged()
{
    if(gridXSpin()) custom_grid_x = gridXSpin()->value();
    if(gridYSpin()) custom_grid_y = gridYSpin()->value();
    if(gridZSpin()) custom_grid_z = gridZSpin()->value();

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        RegenerateLEDPositions(controller_transforms[i].get());
    }

    if(viewport)
    {
        viewport->SetGridDimensions(custom_grid_x, custom_grid_y, custom_grid_z);
        viewport->update();
    }
    SavePluginUiSettings();
}

void OpenRGB3DSpatialTab::gridSnapToggled(bool enabled)
{
    if(viewport)
    {
        viewport->SetGridSnapEnabled(enabled);
    }
    SavePluginUiSettings();
}

void OpenRGB3DSpatialTab::roomGuideLabelsToggled(bool enabled)
{
    if(viewport)
    {
        viewport->SetShowRoomGuideLabels(enabled);
    }
    SavePluginUiSettings();
}

void OpenRGB3DSpatialTab::frameSelectionInView()
{
    if(viewport)
    {
        viewport->FocusSelectionInView();
    }
}

void OpenRGB3DSpatialTab::resetViewportCamera()
{
    if(viewport)
    {
        viewport->ResetCameraToDefault();
    }
    SavePluginUiSettings();
}

void OpenRGB3DSpatialTab::UpdateSelectionInfo()
{
    if(!viewport || !selectionInfoLabel()) return;

    const std::vector<int>& selected = viewport->GetSelectedControllers();

    QFont info_font = selectionInfoLabel()->font();
    info_font.setBold(true);

    if(viewport->IsRoomViewportSelected())
    {
        selectionInfoLabel()->setText("Selected: Room (viewport spin)");
        info_font.setItalic(false);
    }
    else if(selected.empty())
    {
        selectionInfoLabel()->setText("No selection");
        info_font.setItalic(true);
    }
    else if(selected.size() == 1)
    {
        selectionInfoLabel()->setText(QString("Selected: 1 controller"));
        info_font.setItalic(false);
    }
    else
    {
        selectionInfoLabel()->setText(QString("Selected: %1 controllers").arg(selected.size()));
        info_font.setItalic(false);
    }

    selectionInfoLabel()->setFont(info_font);
}

void OpenRGB3DSpatialTab::effectChanged(int index)
{
    if(!effectCombo() || !effectStackList())
    {
        return;
    }

    if(effect_stack.empty())
    {
        ClearCustomEffectUI();
        return;
    }

    if(index < 0 || index >= (int)effect_stack.size())
    {
        return;
    }

    QString class_name = effectCombo()->itemData(index, kEffectRoleClassName).toString();
    
    if(class_name == QLatin1String("ScreenMirror"))
    {
        if(originLabel()) originLabel()->setVisible(false);
        if(effectOriginCombo()) effectOriginCombo()->setVisible(false);
        if(EffectGlobalSettingsPanel* global_panel = effectGlobalSettingsPanel())
        {
            if(QWidget* room_section = global_panel->roomOutputSection())
            {
                room_section->setVisible(false);
            }
        }
    }
    else
    {
        if(originLabel()) originLabel()->setVisible(true);
        if(effectOriginCombo()) effectOriginCombo()->setVisible(true);
        SyncStackRoomOutputPanel();
    }

    if(effectStackList()->currentRow() != index)
    {
        effectStackList()->setCurrentRow(index);
    }
}

void OpenRGB3DSpatialTab::UpdateEffectOriginCombo()
{
    if(!effectOriginCombo()) return;

    QVariant desired_selection = effectOriginCombo()->currentData();
    bool restore_signals = effectOriginCombo()->blockSignals(true);
    effectOriginCombo()->clear();

    effectOriginCombo()->addItem(tr("Mapped lights center (recommended)"), QVariant(-4));
    effectOriginCombo()->addItem(tr("Room box center"), QVariant(-1));
    effectOriginCombo()->addItem(tr("Target zone center"), QVariant(-2));
    effectOriginCombo()->addItem(tr("No anchor (world 0,0,0)"), QVariant(-3));

    for(size_t i = 0; i < reference_points.size(); i++)
    {
        VirtualReferencePoint3D* ref_point = reference_points[i].get();
        if(!ref_point) continue;

        QString name = QString::fromStdString(ref_point->GetName());
        QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
        QString display = QString("%1 (%2)").arg(name, type);
        effectOriginCombo()->addItem(display, QVariant((int)i));
    }

    int restore_index = effectOriginCombo()->findData(desired_selection);
    if(restore_index < 0)
    {
        restore_index = effectOriginCombo()->findData(QVariant(-4));
    }
    if(restore_index < 0)
    {
        restore_index = 0;
    }
    effectOriginCombo()->setCurrentIndex(restore_index);
    effectOriginCombo()->blockSignals(restore_signals);
}

void OpenRGB3DSpatialTab::UpdateEffectCombo()
{
    if(!effectCombo())
    {
        return;
    }

    QSignalBlocker blocker(effectCombo());
    effectCombo()->clear();

    if(effect_stack.empty())
    {
        effectCombo()->addItem("No Active Effects");
        effectCombo()->setEnabled(false);
        return;
    }

    effectCombo()->setEnabled(true);

    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        EffectInstance3D* instance = effect_stack[i].get();
        if(!instance)
        {
            continue;
        }

        QString label = QStringLiteral("#%1 \u2022 %2")
                            .arg(i + 1)
                            .arg(QString::fromStdString(instance->GetDisplayName()));
        effectCombo()->addItem(label);
        int row = effectCombo()->count() - 1;
        effectCombo()->setItemData(row, QString::fromStdString(instance->effect_class_name),
                                  kEffectRoleClassName);
        effectCombo()->setItemData(row, instance->id, kEffectRoleInstanceId);
    }

    int desired_index = effectStackList() ? effectStackList()->currentRow() : 0;
    if(desired_index < 0)
    {
        desired_index = 0;
    }
    if(desired_index >= effectCombo()->count())
    {
        desired_index = effectCombo()->count() - 1;
    }

    effectCombo()->setCurrentIndex(desired_index);
}

void OpenRGB3DSpatialTab::effectZoneChanged(int index)
{
    if(!effectZoneCombo())
    {
        return;
    }

    if(!effectStackList())
    {
        return;
    }

    int current_row = effectStackList()->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        return;
    }
    if(index < 0 || index >= effectZoneCombo()->count())
    {
        return;
    }

    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->zone_index = effectZoneCombo()->itemData(index).toInt();
    UpdateEffectStackList();
    if(effectStackList())
    {
        effectStackList()->setCurrentRow(current_row);
    }
    if(stackEffectZoneCombo())
    {
        int zone_combo_index = stackEffectZoneCombo()->findData(instance->zone_index);
        stackEffectZoneCombo()->blockSignals(true);
        if(zone_combo_index >= 0)
        {
            stackEffectZoneCombo()->setCurrentIndex(zone_combo_index);
        }
        stackEffectZoneCombo()->blockSignals(false);
    }
    SaveEffectStack();
}

void OpenRGB3DSpatialTab::effectOriginChanged(int index)
{
    if(!effectOriginCombo())
    {
        return;
    }
    if(index < 0 || index >= effectOriginCombo()->count())
    {
        return;
    }
    int ref_point_idx = effectOriginCombo()->itemData(index).toInt();

    Vector3D origin = {0.0f, 0.0f, 0.0f};

    if(ref_point_idx >= 0 && ref_point_idx < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_point = reference_points[ref_point_idx].get();
        if(ref_point)
        {
            origin = ref_point->GetPosition();
        }
    }

    if(current_effect_ui)
    {
        if(ref_point_idx == -2)
        {
            current_effect_ui->SetReferenceMode(REF_MODE_TARGET_ZONE_CENTER);
        }
        else if(ref_point_idx == -3)
        {
            current_effect_ui->SetReferenceMode(REF_MODE_WORLD_ORIGIN);
        }
        else if(ref_point_idx == -4)
        {
            current_effect_ui->SetReferenceMode(REF_MODE_LED_CENTROID);
        }
        else if(ref_point_idx >= 0)
        {
            current_effect_ui->SetReferenceMode(REF_MODE_CUSTOM_POINT);
            current_effect_ui->SetCustomReferencePoint(origin);
        }
        else
        {
            current_effect_ui->SetReferenceMode(REF_MODE_ROOM_CENTER);
        }
    }

    if(viewport) viewport->UpdateColors();
}

void OpenRGB3DSpatialTab::effectBoundsChanged(int index)
{
    if(!effectBoundsCombo())
    {
        return;
    }
    if(index < 0 || index >= effectBoundsCombo()->count())
    {
        return;
    }
    int mode = effectBoundsCombo()->itemData(index).toInt();

    if(current_effect_ui)
    {
        current_effect_ui->SetEffectBoundsMode(mode);
    }

    if(effectStackList())
    {
        int current_row = effectStackList()->currentRow();
        if(current_row >= 0 && current_row < (int)effect_stack.size())
        {
            EffectInstance3D* instance = effect_stack[current_row].get();
            if(instance)
            {
                SpatialEffect3D* settings_source = nullptr;
                if(instance->effect)
                {
                    instance->effect->SetEffectBoundsMode(mode);
                    settings_source = instance->effect.get();
                }
                else if(current_effect_ui)
                {
                    settings_source = current_effect_ui;
                }

                if(settings_source)
                {
                    nlohmann::json current_settings = settings_source->SaveSettings();
                    instance->saved_settings = std::make_unique<nlohmann::json>(current_settings);
                    SaveEffectStack();
                }
            }
        }
    }

    if(viewport) viewport->UpdateColors();
}

void OpenRGB3DSpatialTab::ComputeAutoRoomExtents(float& width_mm, float& depth_mm, float& height_mm) const
{
    ManualRoomSettings auto_room = MakeManualRoomSettings(false, 0.0f, 0.0f, 0.0f);
    GridBounds bounds = ComputeGridBounds(auto_room, grid_scale_mm, controller_transforms);
    GridExtents extents = BoundsToExtents(bounds);

    width_mm  = GridUnitsToMM(extents.width_units, grid_scale_mm);
    depth_mm  = GridUnitsToMM(extents.depth_units, grid_scale_mm);
    height_mm = GridUnitsToMM(extents.height_units, grid_scale_mm);
}

nlohmann::json OpenRGB3DSpatialTab::GetPluginSettings() const
{
    if(!resource_manager) return nlohmann::json::object();
    SettingsManager* mgr = resource_manager->GetSettingsManager();
    return mgr ? mgr->GetSettings("3DSpatialPlugin") : nlohmann::json::object();
}

void OpenRGB3DSpatialTab::SetPluginSettings(const nlohmann::json& settings)
{
    if(!resource_manager) return;
    SettingsManager* mgr = resource_manager->GetSettingsManager();
    if(mgr)
    {
        mgr->SetSettings("3DSpatialPlugin", settings);
        mgr->SaveSettings();
    }
}

void OpenRGB3DSpatialTab::SetPluginSettingsNoSave(const nlohmann::json& settings)
{
    if(!resource_manager) return;
    SettingsManager* mgr = resource_manager->GetSettingsManager();
    if(mgr) mgr->SetSettings("3DSpatialPlugin", settings);
}

void OpenRGB3DSpatialTab::PersistRoomGridOverlayToSettings()
{
    if(!viewport) return;
    try
    {
        nlohmann::json settings = GetPluginSettings();
        settings["RoomGrid"]["Show"] = viewport->GetShowRoomGridOverlay();
        settings["RoomGrid"]["Brightness"] = viewport->GetRoomGridBrightness();
        settings["RoomGrid"]["PointSize"] = viewport->GetRoomGridPointSize();
        settings["RoomGrid"]["Step"] = viewport->GetRoomGridStep();
        SetPluginSettings(settings);
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to persist room grid overlay settings: %s", e.what());
    }
}

void OpenRGB3DSpatialTab::RefreshEffectDisplay()
{
    if(effect_running)
    {
        RenderEffectStack();
    }
    else if(viewport)
    {
        viewport->UpdateColors();
    }
}

void OpenRGB3DSpatialTab::SetupStackRoomOutputPanel()
{
    EffectGlobalSettingsPanel* global_panel = effectGlobalSettingsPanel();
    if(!global_panel || stack_room_output_panel_)
    {
        return;
    }

    QWidget* host = global_panel->roomOutputHost();
    if(!host)
    {
        return;
    }

    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    stack_room_output_panel_ = new EffectRoomOutputPanel(host);
    layout->addWidget(stack_room_output_panel_);

    if(QWidget* section = global_panel->roomOutputSection())
    {
        section->setVisible(false);
    }
}

QString OpenRGB3DSpatialTab::sceneLabelForTransformIndex(int transform_index) const
{
    const int list_row = TransformIndexToControllerListRow(transform_index);
    if(list_row >= 0 && list_row < scene_controllers_.count())
    {
        return scene_controllers_.textAt(list_row);
    }
    if(transform_index >= 0 && transform_index < static_cast<int>(controller_transforms.size()))
    {
        return ControllerDisplay::FormatControllerTransformLabel(controller_transforms[transform_index].get(),
                                                                 transform_index);
    }
    return QStringLiteral("Controller %1").arg(transform_index);
}

void OpenRGB3DSpatialTab::SyncStackRoomOutputPanel()
{
    EffectGlobalSettingsPanel* global_panel = effectGlobalSettingsPanel();
    if(!global_panel || !stack_room_output_panel_)
    {
        return;
    }

    QWidget* section = global_panel->roomOutputSection();
    if(current_effect_ui)
    {
        current_effect_ui->DisconnectStackRoomOutputPanel();
    }

    if(!current_effect_ui || !current_effect_ui->GetEffectInfo().show_room_output_control)
    {
        if(section)
        {
            section->setVisible(false);
        }
        return;
    }

    if(section)
    {
        section->setVisible(true);
    }

    QPointer<SpatialEffect3D> captured_ui(current_effect_ui);
    current_effect_ui->ConnectStackRoomOutputPanel(
        stack_room_output_panel_,
        [captured_ui]()
        {
            if(!captured_ui.isNull())
            {
                emit captured_ui->ParametersChanged();
            }
        },
        [this](int transform_index) { return sceneLabelForTransformIndex(transform_index); });
}

void OpenRGB3DSpatialTab::SyncSpatialLightingSceneForUi()
{
    if(!controller_transforms.empty())
    {
        SpatialLightingSceneProvider::instance()->SetControllers(&controller_transforms);
    }
    if(current_effect_ui)
    {
        current_effect_ui->RefreshRoomOutputControllerLists();
    }
}

double OpenRGB3DSpatialTab::EffectiveGridScaleMm() const
{
    const double scale = (gridScaleSpin() != nullptr) ? gridScaleSpin()->value() : (double)grid_scale_mm;
    return static_cast<double>(SafeGridScaleMm(static_cast<float>(scale)));
}

void OpenRGB3DSpatialTab::ScenePositionAxisLimitsMm(int axis, double& min_mm, double& max_mm) const
{
    const double scale = EffectiveGridScaleMm();

    if(use_manual_room_size)
    {
        double width_mm  = manual_room_width;
        double height_mm = manual_room_height;
        double depth_mm  = manual_room_depth;
        if(roomWidthSpin())
        {
            width_mm = roomWidthSpin()->value();
        }
        if(roomHeightSpin())
        {
            height_mm = roomHeightSpin()->value();
        }
        if(roomDepthSpin())
        {
            depth_mm = roomDepthSpin()->value();
        }
        if(width_mm < 100.0)
        {
            width_mm = 100.0;
        }
        if(height_mm < 100.0)
        {
            height_mm = 100.0;
        }
        if(depth_mm < 100.0)
        {
            depth_mm = 100.0;
        }

        if(axis == 0)
        {
            min_mm = 0.0;
            max_mm = width_mm;
        }
        else if(axis == 1)
        {
            min_mm = 0.0;
            max_mm = height_mm;
        }
        else
        {
            min_mm = 0.0;
            max_mm = depth_mm;
        }
        return;
    }

    ManualRoomSettings auto_room = MakeManualRoomSettings(false, 0.0f, 0.0f, 0.0f);
    GridBounds         bounds    = ComputeGridBounds(auto_room, (float)scale, controller_transforms);

    if(axis == 0)
    {
        min_mm = (double)GridUnitsToMM(bounds.min_x, (float)scale);
        max_mm = (double)GridUnitsToMM(bounds.max_x, (float)scale);
    }
    else if(axis == 1)
    {
        min_mm = (double)GridUnitsToMM(bounds.min_y, (float)scale);
        max_mm = (double)GridUnitsToMM(bounds.max_y, (float)scale);
    }
    else
    {
        min_mm = (double)GridUnitsToMM(bounds.min_z, (float)scale);
        max_mm = (double)GridUnitsToMM(bounds.max_z, (float)scale);
    }

    double span = max_mm - min_mm;
    if(span < 100.0)
    {
        float auto_w = 0.0f;
        float auto_d = 0.0f;
        float auto_h = 0.0f;
        ComputeAutoRoomExtents(auto_w, auto_d, auto_h);
        if(axis == 0)
        {
            min_mm = 0.0;
            max_mm = std::max(100.0, (double)auto_w);
        }
        else if(axis == 1)
        {
            min_mm = 0.0;
            max_mm = std::max(100.0, (double)auto_h);
        }
        else
        {
            min_mm = 0.0;
            max_mm = std::max(100.0, (double)auto_d);
        }
        span = max_mm - min_mm;
    }

    const double pad = std::max(50.0, span * 0.05);
    min_mm -= pad;
    max_mm += pad;
    if(axis == 1 && min_mm < 0.0)
    {
        min_mm = 0.0;
    }
}

void OpenRGB3DSpatialTab::SetScenePositionControlsMm(double x_mm, double y_mm, double z_mm)
{
    if(SceneTransformPanel* panel = sceneTransformPanelWidget())
    {
        panel->setPositionControlsMm(x_mm, y_mm, z_mm);
    }
}

void OpenRGB3DSpatialTab::ApplyScenePositionAbsoluteMm(int axis, double value_mm)
{
    if(axis == 1 && value_mm < 0.0)
    {
        value_mm = 0.0;
    }
    ApplyPositionComponent(axis,
                           MMToGridUnits(static_cast<float>(value_mm), static_cast<float>(EffectiveGridScaleMm())));
}

void OpenRGB3DSpatialTab::ApplyPositionComponent(int axis, double value)
{
    if(!viewport) return;

    const int scene_row = scene_controllers_.currentRow();
    if(scene_row >= 0 && scene_row < scene_controllers_.count() && scene_controllers_.hasUserRole(scene_row))
    {
        const SpatialControllerEntryKey metadata = scene_controllers_.userRoleAt(scene_row);
        if(metadata.first == -2)
        {
            const int ref_idx = metadata.second;
            if(ref_idx >= 0 && ref_idx < (int)reference_points.size() && !IsDeviceLinkedReferencePoint(ref_idx))
            {
                VirtualReferencePoint3D* ref_point = reference_points[ref_idx].get();
                Vector3D pos = ref_point->GetPosition();
                if(axis == 0) pos.x = (float)value;
                else if(axis == 1) pos.y = (float)value;
                else pos.z = (float)value;
                ref_point->SetPosition(pos);
                viewport->SelectReferencePoint(ref_idx);
                viewport->UpdateGizmoPosition();
                viewport->update();
                SetLayoutDirty();
                emit GridLayoutChanged();
            }
            return;
        }
        if(metadata.first == -3)
        {
            const int plane_index = FindDisplayPlaneIndexById(metadata.second);
            if(plane_index >= 0 && plane_index < (int)display_planes.size())
            {
                DisplayPlane3D* plane = display_planes[plane_index].get();
                if(plane)
                {
                    Transform3D& t = plane->GetTransform();
                    if(axis == 0) t.position.x = (float)value;
                    else if(axis == 1) t.position.y = (float)value;
                    else t.position.z = (float)value;
                    current_display_plane_index = plane_index;
                    SyncDisplayPlaneControls(plane);
                    viewport->SelectDisplayPlane(plane_index);
                    viewport->NotifyDisplayPlaneChanged();
                    SetLayoutDirty();
                    emit GridLayoutChanged();
                }
            }
            return;
        }
    }

    if(referencePointsList())
    {
        const int ref_idx = ReferencePointIndexFromListRow(referencePointsList()->currentRow());
        if(ref_idx >= 0 && ref_idx < (int)reference_points.size() && !IsDeviceLinkedReferencePoint(ref_idx))
        {
            VirtualReferencePoint3D* ref_point = reference_points[ref_idx].get();
            Vector3D pos = ref_point->GetPosition();
            if(axis == 0) pos.x = (float)value;
            else if(axis == 1) pos.y = (float)value;
            else pos.z = (float)value;
            ref_point->SetPosition(pos);
            viewport->SelectReferencePoint(ref_idx);
            viewport->UpdateGizmoPosition();
            viewport->update();
            SetLayoutDirty();
            emit GridLayoutChanged();
            return;
        }
    }

    int transform_index = ControllerListRowToTransformIndex(scene_controllers_.currentRow());
    if(transform_index >= 0 && transform_index < (int)controller_transforms.size())
    {
        ControllerTransform* transform = controller_transforms[transform_index].get();
        if(transform)
        {
            if(axis == 0) transform->transform.position.x = (float)value;
            else if(axis == 1) transform->transform.position.y = (float)value;
            else transform->transform.position.z = (float)value;
            ControllerLayout3D::MarkWorldPositionsDirty(transform);
            ControllerLayout3D::UpdateWorldPositions(transform);
            SyncControllerLinkedReferencePoint(transform_index);
        }
        viewport->NotifyControllerTransformChanged();
        if(effect_running) RenderEffectStack();
        SetLayoutDirty();
        emit GridLayoutChanged();
        return;
    }
    if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
    {
        DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
        if(plane)
        {
            Transform3D& t = plane->GetTransform();
            if(axis == 0) t.position.x = (float)value;
            else if(axis == 1) t.position.y = (float)value;
            else t.position.z = (float)value;
            SyncDisplayPlaneControls(plane);
            viewport->SelectDisplayPlane(current_display_plane_index);
            viewport->NotifyDisplayPlaneChanged();
            SetLayoutDirty();
            emit GridLayoutChanged();
        }
    }
}

void OpenRGB3DSpatialTab::ApplyRotationComponent(int axis, double value)
{
    if(!viewport) return;

    const int scene_row = scene_controllers_.currentRow();
    if(scene_row >= 0 && scene_row < scene_controllers_.count() && scene_controllers_.hasUserRole(scene_row))
    {
        const SpatialControllerEntryKey metadata = scene_controllers_.userRoleAt(scene_row);
        if(metadata.first == -2)
        {
            const int ref_idx = metadata.second;
            if(ref_idx >= 0 && ref_idx < (int)reference_points.size() && !IsDeviceLinkedReferencePoint(ref_idx))
            {
                VirtualReferencePoint3D* ref_point = reference_points[ref_idx].get();
                Rotation3D rot = ref_point->GetRotation();
                if(axis == 0) rot.x = (float)value;
                else if(axis == 1) rot.y = (float)value;
                else rot.z = (float)value;
                ref_point->SetRotation(rot);
                viewport->SelectReferencePoint(ref_idx);
                viewport->UpdateGizmoPosition();
                viewport->update();
                SetLayoutDirty();
                emit GridLayoutChanged();
            }
            return;
        }
        if(metadata.first == -3)
        {
            const int plane_index = FindDisplayPlaneIndexById(metadata.second);
            if(plane_index >= 0 && plane_index < (int)display_planes.size())
            {
                DisplayPlane3D* plane = display_planes[plane_index].get();
                if(plane)
                {
                    Transform3D& t = plane->GetTransform();
                    if(axis == 0) t.rotation.x = (float)value;
                    else if(axis == 1) t.rotation.y = (float)value;
                    else t.rotation.z = (float)value;
                    current_display_plane_index = plane_index;
                    SyncDisplayPlaneControls(plane);
                    viewport->SelectDisplayPlane(plane_index);
                    viewport->NotifyDisplayPlaneChanged();
                    SetLayoutDirty();
                    emit GridLayoutChanged();
                }
            }
            return;
        }
    }

    if(referencePointsList())
    {
        const int ref_idx = ReferencePointIndexFromListRow(referencePointsList()->currentRow());
        if(ref_idx >= 0 && ref_idx < (int)reference_points.size() && !IsDeviceLinkedReferencePoint(ref_idx))
        {
            VirtualReferencePoint3D* ref_point = reference_points[ref_idx].get();
            Rotation3D rot = ref_point->GetRotation();
            if(axis == 0) rot.x = (float)value;
            else if(axis == 1) rot.y = (float)value;
            else rot.z = (float)value;
            ref_point->SetRotation(rot);
            viewport->SelectReferencePoint(ref_idx);
            viewport->UpdateGizmoPosition();
            viewport->update();
            SetLayoutDirty();
            emit GridLayoutChanged();
            return;
        }
    }

    int transform_index = ControllerListRowToTransformIndex(scene_controllers_.currentRow());
    if(transform_index >= 0 && transform_index < (int)controller_transforms.size())
    {
        ControllerTransform* transform = controller_transforms[transform_index].get();
        if(transform)
        {
            if(axis == 0) transform->transform.rotation.x = (float)value;
            else if(axis == 1) transform->transform.rotation.y = (float)value;
            else transform->transform.rotation.z = (float)value;
            ControllerLayout3D::MarkWorldPositionsDirty(transform);
            ControllerLayout3D::UpdateWorldPositions(transform);
            SyncControllerLinkedReferencePoint(transform_index);
        }
        viewport->NotifyControllerTransformChanged();
        if(effect_running) RenderEffectStack();
        SetLayoutDirty();
        emit GridLayoutChanged();
        return;
    }
    if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
    {
        DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
        if(plane)
        {
            Transform3D& t = plane->GetTransform();
            if(axis == 0) t.rotation.x = (float)value;
            else if(axis == 1) t.rotation.y = (float)value;
            else t.rotation.z = (float)value;
            SyncDisplayPlaneControls(plane);
            viewport->SelectDisplayPlane(current_display_plane_index);
            viewport->NotifyDisplayPlaneChanged();
            SetLayoutDirty();
            emit GridLayoutChanged();
        }
    }
}

QListWidget* OpenRGB3DSpatialTab::zonesList() const
{
    return ui && ui->zonesPanel ? ui->zonesPanel->zonesList() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::editZoneButton() const
{
    return ui && ui->zonesPanel ? ui->zonesPanel->editZoneButton() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::deleteZoneButton() const
{
    return ui && ui->zonesPanel ? ui->zonesPanel->deleteZoneButton() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::effectCategoryCombo() const
{
    return ui && ui->effectLibraryPanel ? ui->effectLibraryPanel->categoryCombo() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::effectLibraryGameCombo() const
{
    return ui && ui->effectLibraryPanel ? ui->effectLibraryPanel->gameCombo() : nullptr;
}

QLineEdit* OpenRGB3DSpatialTab::effectLibrarySearch() const
{
    return ui && ui->effectLibraryPanel ? ui->effectLibraryPanel->searchEdit() : nullptr;
}

QListWidget* OpenRGB3DSpatialTab::effectLibraryList() const
{
    return ui && ui->effectLibraryPanel ? ui->effectLibraryPanel->libraryList() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::effectLibraryAddButton() const
{
    return ui && ui->effectLibraryPanel ? ui->effectLibraryPanel->addToStackButton() : nullptr;
}

QListWidget* OpenRGB3DSpatialTab::effectStackList() const
{
    return ui && ui->effectStackPanel ? ui->effectStackPanel->stackList() : nullptr;
}

QListWidget* OpenRGB3DSpatialTab::stackPresetsList() const
{
    return ui && ui->effectStackPanel ? ui->effectStackPanel->presetsList() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::startAllEffectsButton() const
{
    return ui && ui->effectStackPanel ? ui->effectStackPanel->startAllButton() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::stopAllEffectsButton() const
{
    return ui && ui->effectStackPanel ? ui->effectStackPanel->stopAllButton() : nullptr;
}

QGroupBox* OpenRGB3DSpatialTab::effectConfigGroup() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel : nullptr;
}

EffectGlobalSettingsPanel* OpenRGB3DSpatialTab::effectGlobalSettingsPanel() const
{
    return ui ? ui->effectGlobalSettingsPanel : nullptr;
}

QLabel* OpenRGB3DSpatialTab::effectStackRowLabel() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel->effectRowLabel() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::effectCombo() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel->effectCombo() : nullptr;
}

QLabel* OpenRGB3DSpatialTab::effectZoneLabel() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel->zoneLabel() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::effectZoneCombo() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel->zoneCombo() : nullptr;
}

QLabel* OpenRGB3DSpatialTab::originLabel() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel->originLabelWidget() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::effectOriginCombo() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel->originCombo() : nullptr;
}

QLabel* OpenRGB3DSpatialTab::effectBoundsLabel() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel->boundsLabel() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::effectBoundsCombo() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel->boundsCombo() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::stackEffectTypeCombo() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel->stackEffectTypeCombo() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::stackEffectZoneCombo() const
{
    return ui && ui->effectGlobalSettingsPanel ? ui->effectGlobalSettingsPanel->stackEffectZoneCombo() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::layoutProfilesCombo() const
{
    return ui && ui->profilesTabPanel ? ui->profilesTabPanel->layoutProfilesCombo() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::saveLayoutButton() const
{
    return ui && ui->profilesTabPanel ? ui->profilesTabPanel->saveLayoutButton() : nullptr;
}

QCheckBox* OpenRGB3DSpatialTab::autoLoadLayoutCheckbox() const
{
    return ui && ui->profilesTabPanel ? ui->profilesTabPanel->autoLoadLayoutCheckbox() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::effectProfilesCombo() const
{
    return ui && ui->profilesTabPanel ? ui->profilesTabPanel->effectProfilesCombo() : nullptr;
}

QCheckBox* OpenRGB3DSpatialTab::autoLoadEffectProfileCheckbox() const
{
    return ui && ui->profilesTabPanel ? ui->profilesTabPanel->autoLoadEffectProfileCheckbox() : nullptr;
}

QSpinBox* OpenRGB3DSpatialTab::gridXSpin() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->gridXSpin() : nullptr;
}

QSpinBox* OpenRGB3DSpatialTab::gridYSpin() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->gridYSpin() : nullptr;
}

QSpinBox* OpenRGB3DSpatialTab::gridZSpin() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->gridZSpin() : nullptr;
}

QCheckBox* OpenRGB3DSpatialTab::gridSnapCheckbox() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->gridSnapCheckbox() : nullptr;
}

QDoubleSpinBox* OpenRGB3DSpatialTab::gridScaleSpin() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->gridScaleSpin() : nullptr;
}

QLabel* OpenRGB3DSpatialTab::selectionInfoLabel() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->selectionInfoLabel() : nullptr;
}

QCheckBox* OpenRGB3DSpatialTab::roomGridOverlayCheckbox() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->roomGridOverlayCheckbox() : nullptr;
}

QDoubleSpinBox* OpenRGB3DSpatialTab::roomWidthSpin() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->roomWidthSpin() : nullptr;
}

QDoubleSpinBox* OpenRGB3DSpatialTab::roomDepthSpin() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->roomDepthSpin() : nullptr;
}

QDoubleSpinBox* OpenRGB3DSpatialTab::roomHeightSpin() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->roomHeightSpin() : nullptr;
}

QCheckBox* OpenRGB3DSpatialTab::useManualRoomSizeCheckbox() const
{
    return ui && ui->gridSettingsPanel ? ui->gridSettingsPanel->useManualRoomSizeCheckbox() : nullptr;
}

QDoubleSpinBox* OpenRGB3DSpatialTab::posXSpin() const
{
    return sceneTransformPanelWidget() ? sceneTransformPanelWidget()->posXSpin() : nullptr;
}

QDoubleSpinBox* OpenRGB3DSpatialTab::posYSpin() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->posYSpin() : nullptr;
}

QDoubleSpinBox* OpenRGB3DSpatialTab::posZSpin() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->posZSpin() : nullptr;
}

QSlider* OpenRGB3DSpatialTab::posXSlider() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->posXSlider() : nullptr;
}

QSlider* OpenRGB3DSpatialTab::posYSlider() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->posYSlider() : nullptr;
}

QSlider* OpenRGB3DSpatialTab::posZSlider() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->posZSlider() : nullptr;
}

QDoubleSpinBox* OpenRGB3DSpatialTab::rotXSpin() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->rotXSpin() : nullptr;
}

QDoubleSpinBox* OpenRGB3DSpatialTab::rotYSpin() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->rotYSpin() : nullptr;
}

QDoubleSpinBox* OpenRGB3DSpatialTab::rotZSpin() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->rotZSpin() : nullptr;
}

QSlider* OpenRGB3DSpatialTab::rotXSlider() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->rotXSlider() : nullptr;
}

QSlider* OpenRGB3DSpatialTab::rotYSlider() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->rotYSlider() : nullptr;
}

QSlider* OpenRGB3DSpatialTab::rotZSlider() const
{
    return ui && sceneTransformPanelWidget() ? sceneTransformPanelWidget()->rotZSlider() : nullptr;
}

QWidget* OpenRGB3DSpatialTab::effectControlsWidget() const
{
    return ui && ui->effectControlsHostPanel ? ui->effectControlsHostPanel : nullptr;
}

QVBoxLayout* OpenRGB3DSpatialTab::effectControlsLayout() const
{
    return ui && ui->effectControlsHostPanel ? ui->effectControlsHostPanel->contentLayout() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::audioDeviceCombo() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->deviceCombo() : nullptr;
}

QSlider* OpenRGB3DSpatialTab::audioGainSlider() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->gainSlider() : nullptr;
}

QSlider* OpenRGB3DSpatialTab::audioClaritySlider() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->claritySlider() : nullptr;
}

QLabel* OpenRGB3DSpatialTab::audioClarityValueLabel() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->clarityValueLabel() : nullptr;
}

QSlider* OpenRGB3DSpatialTab::audioIsolationSlider() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->isolationSlider() : nullptr;
}

QLabel* OpenRGB3DSpatialTab::audioIsolationValueLabel() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->isolationValueLabel() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::audioMixPresetCombo() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->mixPresetCombo() : nullptr;
}

QProgressBar* OpenRGB3DSpatialTab::audioLevelBar() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->levelBar() : nullptr;
}

QProgressBar* OpenRGB3DSpatialTab::audioBassBar() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->bassBar() : nullptr;
}

QProgressBar* OpenRGB3DSpatialTab::audioMidBar() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->midBar() : nullptr;
}

QProgressBar* OpenRGB3DSpatialTab::audioHighBar() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->highBar() : nullptr;
}

QProgressBar* OpenRGB3DSpatialTab::audioKickStemBar() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->kickStemBar() : nullptr;
}

QProgressBar* OpenRGB3DSpatialTab::audioSnareStemBar() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->snareStemBar() : nullptr;
}

QProgressBar* OpenRGB3DSpatialTab::audioHihatStemBar() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->hihatStemBar() : nullptr;
}

QProgressBar* OpenRGB3DSpatialTab::audioBassStemBar() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->bassStemBar() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::audioStartButton() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->startButton() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::audioStopButton() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->stopButton() : nullptr;
}

QLabel* OpenRGB3DSpatialTab::audioGainValueLabel() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->gainValueLabel() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::audioBandsCombo() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->bandsCombo() : nullptr;
}

QComboBox* OpenRGB3DSpatialTab::audioFftCombo() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->fftCombo() : nullptr;
}

QLabel* OpenRGB3DSpatialTab::audioSpectrumLabel() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->spectrumLabel() : nullptr;
}

QLabel* OpenRGB3DSpatialTab::audioEqCaption() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->eqCaption() : nullptr;
}

QScrollArea* OpenRGB3DSpatialTab::audioEqScroll() const
{
    return ui && ui->audioInputPanel ? ui->audioInputPanel->eqScroll() : nullptr;
}

QLabel* OpenRGB3DSpatialTab::objectCreatorStatusLabel() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->statusLabel() : nullptr;
}

QListWidget* OpenRGB3DSpatialTab::customControllersList() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->customControllersList() : nullptr;
}

QWidget* OpenRGB3DSpatialTab::customControllersEmptyLabel() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->customControllersEmptyLabel() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::exportCustomControllerButton() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->exportCustomControllerButton() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::editCustomControllerButton() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->editCustomControllerButton() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::deleteCustomControllerButton() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->deleteCustomControllerButton() : nullptr;
}

QListWidget* OpenRGB3DSpatialTab::referencePointsList() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->referencePointsList() : nullptr;
}

QWidget* OpenRGB3DSpatialTab::refPointsEmptyLabel() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->refPointsEmptyLabel() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::editReferencePointButton() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->editReferencePointButton() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::removeRefPointButton() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->removeRefPointButton() : nullptr;
}

QListWidget* OpenRGB3DSpatialTab::displayPlanesList() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->displayPlanesList() : nullptr;
}

QWidget* OpenRGB3DSpatialTab::displayPlanesEmptyLabel() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->displayPlanesEmptyLabel() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::editDisplayPlaneButton() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->editDisplayPlaneButton() : nullptr;
}

QPushButton* OpenRGB3DSpatialTab::removeDisplayPlaneButton() const
{
    return ui && ui->objectCreatorTabPanel ? ui->objectCreatorTabPanel->removeDisplayPlaneButton() : nullptr;
}

SpatialControllerCardList* OpenRGB3DSpatialTab::availableControllerCards() const
{
    return ui && ui->availableControllersPanel ? ui->availableControllersPanel->cardList() : nullptr;
}

SpatialControllerCardList* OpenRGB3DSpatialTab::sceneControllerCards() const
{
    return ui && ui->controllersInScenePanel ? ui->controllersInScenePanel->cardList() : nullptr;
}
