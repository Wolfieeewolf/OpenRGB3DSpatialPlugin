// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ControllerLayout3D.h"
#include "LogManager.h"
#include "CustomControllerDialog.h"
#include "DisplayPlaneManager.h"
#include "Effects3D/ScreenMirror/ScreenMirror.h"
#include "GridSpaceUtils.h"
#include <QStackedWidget>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>
#include <fstream>
#include <algorithm>
#include <functional>
#include <map>
#include <QList>
#include <QSignalBlocker>
#include <QColor>
#include <QFont>
#include <QSplitter>
#include <QFrame>
#include <QAbstractItemView>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include <QFileInfo>
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
#include "Effects3D/Games/Minecraft/MinecraftEffectLibrary.h"
#include "AudioContainer/AudioEffectLibrary.h"

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
    resource_manager(rm),
    first_load(true)
{

    stack_settings_updating = false;
    effect_config_group = nullptr;
    minecraft_library_panel = nullptr;
    effect_controls_widget = nullptr;
    effects_detail_scroll = nullptr;
    effect_controls_layout = nullptr;
    current_effect_ui = nullptr;
    start_effect_button = nullptr;
    stop_effect_button = nullptr;
    start_all_effects_btn = nullptr;
    stop_all_effects_btn = nullptr;
    stack_blend_container = nullptr;
    stack_effect_blend_combo = nullptr;
    effect_zone_label = nullptr;
    origin_label = nullptr;
    effect_origin_combo = nullptr;
    effect_zone_combo = nullptr;
    effect_category_combo = nullptr;
    effect_library_search = nullptr;
    effect_library_list = nullptr;
    effect_library_add_button = nullptr;
    minecraft_library_hub_active = false;
    minecraft_hub_preview_effect = nullptr;
    minecraft_library_layer_combo = nullptr;
    minecraft_hub_preview_holder = nullptr;
    audio_library_hub_active = false;
    audio_library_panel = nullptr;
    audio_hub_preview_effect = nullptr;
    audio_hub_layer_combo = nullptr;
    audio_hub_preview_holder = nullptr;
    audio_hub_low_slider = nullptr;
    audio_hub_high_slider = nullptr;
    audio_hub_low_label = nullptr;
    audio_hub_high_label = nullptr;
    audio_hub_zone_combo = nullptr;
    audio_hub_origin_combo = nullptr;
    effect_stack_row_label = nullptr;
    effect_combo = nullptr;
    effect_type_combo = nullptr;
    last_stack_selection_index = -1;
    add_controller_ref_point_check = nullptr;

    available_controllers_list = nullptr;
    custom_controllers_list = nullptr;
    object_creator_status_label = nullptr;
    controller_list = nullptr;
    reference_points_list = nullptr;
    ref_points_empty_label = nullptr;
    display_planes_list = nullptr;
    display_plane_name_edit = nullptr;
    display_plane_width_spin = nullptr;
    display_plane_height_spin = nullptr;
    display_plane_monitor_combo = nullptr;
    display_plane_monitor_brand_filter = nullptr;
    display_plane_monitor_sort_combo = nullptr;
    display_plane_capture_combo = nullptr;
    display_plane_refresh_capture_btn = nullptr;
    display_plane_visible_check = nullptr;
    add_display_plane_button = nullptr;
    remove_display_plane_button = nullptr;
    current_display_plane_index = -1;
    zones_list = nullptr;
    monitor_preset_completer = nullptr;

    viewport = nullptr;

    zone_manager = std::make_unique<ZoneManager3D>();

    grid_x_spin = nullptr;
    grid_y_spin = nullptr;
    grid_z_spin = nullptr;
    grid_snap_checkbox = nullptr;
    grid_scale_spin = nullptr;
    selection_info_label = nullptr;
    room_grid_overlay_checkbox = nullptr;
    custom_grid_x = 10;
    custom_grid_y = 10;
    custom_grid_z = 10;
    grid_scale_mm = 10.0f;

    room_width_spin = nullptr;
    room_depth_spin = nullptr;
    room_height_spin = nullptr;
    use_manual_room_size_checkbox = nullptr;
    manual_room_width = 1000.0f;
    manual_room_depth = 1000.0f;
    manual_room_height = 1000.0f;
    use_manual_room_size = false;

    led_spacing_x_spin = nullptr;
    led_spacing_y_spin = nullptr;
    led_spacing_z_spin = nullptr;

    edit_led_spacing_x_spin = nullptr;
    edit_led_spacing_y_spin = nullptr;
    edit_led_spacing_z_spin = nullptr;
    apply_spacing_button = nullptr;

    pos_x_spin = nullptr;
    pos_y_spin = nullptr;
    pos_z_spin = nullptr;
    pos_x_slider = nullptr;
    pos_y_slider = nullptr;
    pos_z_slider = nullptr;

    rot_x_spin = nullptr;
    rot_y_spin = nullptr;
    rot_z_spin = nullptr;
    rot_x_slider = nullptr;
    rot_y_slider = nullptr;
    rot_z_slider = nullptr;

    granularity_combo = nullptr;
    item_combo = nullptr;

    layout_profiles_combo = nullptr;
    auto_load_checkbox = nullptr;
    effect_profiles_combo = nullptr;
    effect_auto_load_checkbox = nullptr;
    auto_load_timer = nullptr;
    effect_timer = nullptr;

    ref_point_name_edit = nullptr;
    ref_point_type_combo = nullptr;
    ref_point_color_button = nullptr;
    add_ref_point_button = nullptr;
    remove_ref_point_button = nullptr;

    create_zone_button = nullptr;
    edit_zone_button = nullptr;
    delete_zone_button = nullptr;

    effect_stack_list = nullptr;
    stack_effect_type_combo = nullptr;
    stack_effect_zone_combo = nullptr;
    stack_presets_list = nullptr;
    next_effect_instance_id = 1;

    SetupUI();
    LoadDevices();
    LoadCustomControllers();
    UpdateDisplayPlanesList();
    RefreshDisplayPlaneDetails();

    UpdateEffectZoneCombo();
    UpdateEffectOriginCombo();
    UpdateFreqOriginCombo();

    const int kStartupDelayMs = 2000;
    auto_load_timer = new QTimer(this);
    auto_load_timer->setSingleShot(true);
    connect(auto_load_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::TryAutoLoadLayout);
    auto_load_timer->start(kStartupDelayMs);

    effect_timer = new QTimer(this);
    effect_timer->setTimerType(Qt::PreciseTimer);
    connect(effect_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::on_effect_timer_timeout);
}

OpenRGB3DSpatialTab::~OpenRGB3DSpatialTab()
{
    SaveEffectStack();
    try
    {
        nlohmann::json settings = GetPluginSettings();
        if(viewport)
        {
            float dist, yaw, pitch, tx, ty, tz;
            viewport->GetCamera(dist, yaw, pitch, tx, ty, tz);
            settings["Camera"]["Distance"] = dist;
            settings["Camera"]["Yaw"] = yaw;
            settings["Camera"]["Pitch"] = pitch;
            settings["Camera"]["TargetX"] = tx;
            settings["Camera"]["TargetY"] = ty;
            settings["Camera"]["TargetZ"] = tz;
            settings["RoomGrid"]["Show"] = viewport->GetShowRoomGridOverlay();
            settings["RoomGrid"]["Brightness"] = viewport->GetRoomGridBrightness();
            settings["RoomGrid"]["PointSize"] = viewport->GetRoomGridPointSize();
            settings["RoomGrid"]["Step"] = viewport->GetRoomGridStep();
            settings["Grid"]["SnapEnabled"] = viewport->IsGridSnapEnabled();
        }
        settings["Grid"]["X"] = custom_grid_x;
        settings["Grid"]["Y"] = custom_grid_y;
        settings["Grid"]["Z"] = custom_grid_z;
        settings["Grid"]["ScaleMM"] = grid_scale_mm;
        settings["Room"]["UseManual"] = use_manual_room_size;
        settings["Room"]["WidthMM"] = manual_room_width;
        settings["Room"]["HeightMM"] = manual_room_height;
        settings["Room"]["DepthMM"] = manual_room_depth;
        if(led_spacing_x_spin && led_spacing_y_spin && led_spacing_z_spin)
        {
            settings["LEDSpacing"]["X"] = led_spacing_x_spin->value();
            settings["LEDSpacing"]["Y"] = led_spacing_y_spin->value();
            settings["LEDSpacing"]["Z"] = led_spacing_z_spin->value();
        }
        if(layout_profiles_combo && layout_profiles_combo->currentIndex() >= 0)
            settings["SelectedProfile"] = layout_profiles_combo->currentText().toStdString();
        if(auto_load_checkbox)
            settings["AutoLoadEnabled"] = auto_load_checkbox->isChecked();
        SetPluginSettings(settings);
    }
    catch(const std::exception&){}

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
}

void OpenRGB3DSpatialTab::SetupUI()
{

    QVBoxLayout* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    QSplitter* main_splitter = new QSplitter(Qt::Horizontal);
    main_splitter->setChildrenCollapsible(false);
    main_splitter->setHandleWidth(6);

    const int kLeftPanelMinWidth = 280;
    QTabWidget* left_mode_tabs = new QTabWidget();
    run_setup_tab_widget = left_mode_tabs;
    left_mode_tabs->setTabPosition(QTabWidget::West);
    left_mode_tabs->setMinimumWidth(kLeftPanelMinWidth);
    left_mode_tabs->setMaximumWidth(420);

    QScrollArea* run_scroll = new QScrollArea();
    run_scroll->setWidgetResizable(true);
    run_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    run_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    run_scroll->setMinimumWidth(kLeftPanelMinWidth);

    QWidget* run_content = new QWidget();
    run_content->setMinimumWidth(kLeftPanelMinWidth);
    QVBoxLayout* run_layout = new QVBoxLayout(run_content);
    run_layout->setContentsMargins(4, 4, 12, 4);
    run_layout->setSpacing(6);

    SetupEffectLibraryPanel(run_layout);
    SetupEffectStackPanel(run_layout);
    SetupZonesPanel(run_layout);
    run_layout->addStretch();

    run_scroll->setWidget(run_content);
    left_mode_tabs->addTab(run_scroll, "Run");

    QScrollArea* left_scroll = new QScrollArea();
    left_scroll->setWidgetResizable(true);
    left_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    left_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    left_scroll->setMinimumWidth(260);
    left_scroll->setMaximumWidth(420);
    left_scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QWidget* left_content = new QWidget();
    QVBoxLayout* left_panel = new QVBoxLayout(left_content);
    left_panel->setSpacing(6);

    left_tabs = new QTabWidget();

    QWidget* available_tab = new QWidget();
    QVBoxLayout* available_layout = new QVBoxLayout();
    available_layout->setSpacing(3);

    available_controllers_list = new QListWidget();
    available_controllers_list->setMinimumHeight(200);
    connect(available_controllers_list, &QListWidget::currentRowChanged, [this](int) {
        on_granularity_changed(granularity_combo->currentIndex());
    });
    available_layout->addWidget(available_controllers_list);

    QHBoxLayout* granularity_layout = new QHBoxLayout();
    granularity_layout->setSpacing(3);
    granularity_layout->addWidget(new QLabel("Add:"));
    granularity_combo = new QComboBox();
    granularity_combo->addItem("Whole Device");
    granularity_combo->addItem("Zone");
    granularity_combo->addItem("LED");
    connect(granularity_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_granularity_changed);
    granularity_layout->addWidget(granularity_combo);
    available_layout->addLayout(granularity_layout);

    item_combo = new QComboBox();
    available_layout->addWidget(item_combo);

    QLabel* spacing_label = new QLabel("LED Spacing (mm):");
    QFont spacing_font = spacing_label->font();
    spacing_font.setBold(true);
    spacing_label->setFont(spacing_font);
    spacing_label->setContentsMargins(0, 3, 0, 0);
    available_layout->addWidget(spacing_label);

    QVBoxLayout* spacing_layout = new QVBoxLayout();
    spacing_layout->setSpacing(2);
    spacing_layout->setContentsMargins(0, 0, 0, 0);

    std::function<QHBoxLayout*(const QString&, QDoubleSpinBox*&, bool)> create_spacing_row =
        [](const QString& label_text, QDoubleSpinBox*& spin, bool enabled) -> QHBoxLayout*
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(3);
        row->setContentsMargins(0, 0, 0, 0);
        QLabel* lbl = new QLabel(label_text);
        lbl->setMinimumWidth(14);
        row->addWidget(lbl);

        spin = new QDoubleSpinBox();
        spin->setRange(0.0, 1000.0);
        spin->setSingleStep(1.0);
        spin->setSuffix(" mm");
        spin->setAlignment(Qt::AlignRight);
        spin->setEnabled(enabled);
        row->addWidget(spin, 1);

        return row;
    };

    QHBoxLayout* spacing_x_row = create_spacing_row("X:", led_spacing_x_spin, true);
    led_spacing_x_spin->setValue(10.0);
    led_spacing_x_spin->setToolTip("Horizontal spacing between LEDs (left/right)");
    spacing_layout->addLayout(spacing_x_row);

    QHBoxLayout* spacing_y_row = create_spacing_row("Y:", led_spacing_y_spin, true);
    led_spacing_y_spin->setValue(0.0);
    led_spacing_y_spin->setToolTip("Vertical spacing between LEDs (floor/ceiling)");
    spacing_layout->addLayout(spacing_y_row);

    QHBoxLayout* spacing_z_row = create_spacing_row("Z:", led_spacing_z_spin, true);
    led_spacing_z_spin->setValue(0.0);
    led_spacing_z_spin->setToolTip("Depth spacing between LEDs (front/back)");
    spacing_layout->addLayout(spacing_z_row);

    try
    {
        nlohmann::json settings = GetPluginSettings();
        if(settings.contains("LEDSpacing"))
        {
            const nlohmann::json& s = settings["LEDSpacing"];
            if(led_spacing_x_spin) led_spacing_x_spin->setValue(std::max(0.0, std::min(1000.0, (double)s.value("X", 10.0))));
            if(led_spacing_y_spin) led_spacing_y_spin->setValue(std::max(0.0, std::min(1000.0, (double)s.value("Y", 0.0))));
            if(led_spacing_z_spin) led_spacing_z_spin->setValue(std::max(0.0, std::min(1000.0, (double)s.value("Z", 0.0))));
        }
    }
    catch(const std::exception&) {}

    available_layout->addLayout(spacing_layout);

    QHBoxLayout* add_remove_layout = new QHBoxLayout();
    add_remove_layout->setSpacing(4);
    QPushButton* add_button = new QPushButton("Add to 3D View");
    connect(add_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_add_clicked);
    add_remove_layout->addWidget(add_button);

    QPushButton* remove_button = new QPushButton("Remove");
    connect(remove_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_remove_controller_clicked);
    add_remove_layout->addWidget(remove_button);

    QPushButton* clear_button = new QPushButton("Clear All");
    connect(clear_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_clear_all_clicked);
    add_remove_layout->addWidget(clear_button);
    available_layout->addLayout(add_remove_layout);

    add_controller_ref_point_check = new QCheckBox("Create linked controller reference point");
    add_controller_ref_point_check->setToolTip(
        "Optionally create a reference point at the controller center when adding to scene. "
        "The point stays synced to controller position.");
    add_controller_ref_point_check->setChecked(false);
    available_layout->addWidget(add_controller_ref_point_check);

    available_tab->setLayout(available_layout);
    left_tabs->addTab(available_tab, "Available Controllers");
    connect(left_tabs, &QTabWidget::currentChanged, this, [this, available_tab](int) {
        if(left_tabs && left_tabs->currentWidget() == available_tab)
        {
            UpdateDeviceList();
        }
    });

    left_panel->addWidget(left_tabs);

    QGroupBox* controller_group = new QGroupBox("Controllers in 3D Scene");
    QVBoxLayout* controller_layout = new QVBoxLayout();
    controller_layout->setSpacing(3);

    controller_list = new QListWidget();
    controller_list->setMaximumHeight(80);
    connect(controller_list, &QListWidget::currentRowChanged, [this](int row) {
        if(viewport)
        {
            viewport->SelectController(row >= 0 ? ControllerListRowToTransformIndex(row) : -1);
        }
        on_controller_selected(row);
    });
    controller_layout->addWidget(controller_list);

    QLabel* edit_spacing_label = new QLabel("Edit Selected LED Spacing:");
    QFont edit_spacing_font = edit_spacing_label->font();
    edit_spacing_font.setBold(true);
    edit_spacing_label->setFont(edit_spacing_font);
    edit_spacing_label->setContentsMargins(0, 3, 0, 0);
    controller_layout->addWidget(edit_spacing_label);

    QVBoxLayout* edit_spacing_layout = new QVBoxLayout();
    edit_spacing_layout->setSpacing(2);
    edit_spacing_layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* edit_x_row = create_spacing_row("X:", edit_led_spacing_x_spin, false);
    edit_led_spacing_x_spin->setValue(10.0);
    edit_spacing_layout->addLayout(edit_x_row);

    QHBoxLayout* edit_y_row = create_spacing_row("Y:", edit_led_spacing_y_spin, false);
    edit_led_spacing_y_spin->setValue(0.0);
    edit_spacing_layout->addLayout(edit_y_row);

    QHBoxLayout* edit_z_row = create_spacing_row("Z:", edit_led_spacing_z_spin, false);
    edit_led_spacing_z_spin->setValue(0.0);
    edit_spacing_layout->addLayout(edit_z_row);

    apply_spacing_button = new QPushButton("Apply Spacing");
    apply_spacing_button->setEnabled(false);
    connect(apply_spacing_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_apply_spacing_clicked);
    edit_spacing_layout->addWidget(apply_spacing_button);

    controller_layout->addLayout(edit_spacing_layout);

    controller_group->setLayout(controller_layout);
    left_panel->addWidget(controller_group);

    left_panel->addStretch();

    left_scroll->setWidget(left_content);
    left_mode_tabs->addTab(left_scroll, "Setup");
    main_splitter->addWidget(left_mode_tabs);

    QWidget* center_widget = new QWidget();
    QVBoxLayout* middle_panel = new QVBoxLayout(center_widget);
    middle_panel->setSpacing(3);
    middle_panel->setContentsMargins(0, 0, 0, 0);

    QLabel* controls_label = new QLabel("View (camera only – does not change effect or layout): Right mouse = Rotate view | Left drag = Pan | Scroll = Zoom | Left click = Select/Move objects");
    controls_label->setWordWrap(true);
    controls_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    middle_panel->addWidget(controls_label);

    try
    {
        nlohmann::json settings = GetPluginSettings();
        if(settings.contains("Grid"))
        {
            const nlohmann::json& g = settings["Grid"];
            custom_grid_x = std::max(1, std::min(100, (int)g.value("X", custom_grid_x)));
            custom_grid_y = std::max(1, std::min(100, (int)g.value("Y", custom_grid_y)));
            custom_grid_z = std::max(1, std::min(100, (int)g.value("Z", custom_grid_z)));
            grid_scale_mm = (float)std::max(0.1, std::min(1000.0, (double)g.value("ScaleMM", grid_scale_mm)));
        }
        if(settings.contains("Room"))
        {
            const nlohmann::json& r = settings["Room"];
            use_manual_room_size = r.value("UseManual", use_manual_room_size);
            manual_room_width  = (float)r.value("WidthMM", manual_room_width);
            manual_room_height = (float)r.value("HeightMM", manual_room_height);
            manual_room_depth  = (float)r.value("DepthMM", manual_room_depth);
        }
    }
    catch(const std::exception&) {}

    viewport = new LEDViewport3D();
    viewport->SetControllerTransforms(&controller_transforms);
    viewport->SetGridDimensions(custom_grid_x, custom_grid_y, custom_grid_z);
    viewport->SetGridSnapEnabled(false);
    viewport->SetReferencePoints(&reference_points);
    viewport->SetDisplayPlanes(&display_planes);
    viewport->SetGridScaleMM(grid_scale_mm);
    viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
    viewport->SetScreenPreviewTickCallback({});

    try
    {
        nlohmann::json settings = GetPluginSettings();
        if(settings.contains("Camera"))
        {
            const nlohmann::json& cam = settings["Camera"];
            float dist = cam.value("Distance", 20.0f);
            float yaw  = cam.value("Yaw", 45.0f);
            float pitch= cam.value("Pitch", 30.0f);
            float tx   = cam.value("TargetX", 0.0f);
            float ty   = cam.value("TargetY", 0.0f);
            float tz   = cam.value("TargetZ", 0.0f);
            viewport->SetCamera(dist, yaw, pitch, tx, ty, tz);
        }
        if(settings.contains("Grid") && settings["Grid"].contains("SnapEnabled"))
            viewport->SetGridSnapEnabled(settings["Grid"]["SnapEnabled"].get<bool>());
    }
    catch(const std::exception&) {}

    connect(viewport, &LEDViewport3D::ControllerSelected, this, &OpenRGB3DSpatialTab::on_viewport_controller_selected);
    connect(viewport, &LEDViewport3D::ControllerPositionChanged, this, &OpenRGB3DSpatialTab::on_controller_position_changed);
    connect(viewport, &LEDViewport3D::ControllerRotationChanged, this, &OpenRGB3DSpatialTab::on_controller_rotation_changed);
    connect(viewport, &LEDViewport3D::ControllerDeleteRequested, this, &OpenRGB3DSpatialTab::on_remove_controller_from_viewport);
    connect(viewport, &LEDViewport3D::ReferencePointSelected, this, &OpenRGB3DSpatialTab::on_ref_point_selected);
    connect(viewport, &LEDViewport3D::ReferencePointPositionChanged, this, &OpenRGB3DSpatialTab::on_ref_point_position_changed);
    connect(viewport, &LEDViewport3D::DisplayPlanePositionChanged, this, &OpenRGB3DSpatialTab::on_display_plane_position_signal);
    connect(viewport, &LEDViewport3D::DisplayPlaneRotationChanged, this, &OpenRGB3DSpatialTab::on_display_plane_rotation_signal);
    middle_panel->addWidget(viewport, 1);

    QTabWidget* settings_tabs = new QTabWidget();

    QWidget* grid_settings_tab = new QWidget();
    QVBoxLayout* grid_tab_main = new QVBoxLayout(grid_settings_tab);
    grid_tab_main->setSpacing(8);
    grid_tab_main->setContentsMargins(4, 4, 4, 4);

    QGroupBox* grid_scale_group = new QGroupBox("Grid & scale");
    QGridLayout* grid_gl = new QGridLayout(grid_scale_group);
    grid_gl->setSpacing(4);

    {
        QLabel* layout_lbl = new QLabel(QStringLiteral("Layout size (X × Y × Z units):"));
        layout_lbl->setToolTip(
            QStringLiteral("New controller LED grids: X = width count, Y = vertical layers, Z = depth count. "
                           "Matches scene axes (X left/right, Y up, Z front/back)."));
        grid_gl->addWidget(layout_lbl, 0, 0, 1, 2);
    }
    grid_gl->addWidget(new QLabel("X:"), 0, 2);
    grid_x_spin = new QSpinBox();
    grid_x_spin->setRange(1, 100);
    grid_x_spin->setValue(custom_grid_x);
    grid_x_spin->setToolTip("LED layout width (grid units) for new controllers");
    grid_gl->addWidget(grid_x_spin, 0, 3);
    grid_gl->addWidget(new QLabel("Y:"), 0, 4);
    grid_y_spin = new QSpinBox();
    grid_y_spin->setRange(1, 100);
    grid_y_spin->setValue(custom_grid_y);
    grid_y_spin->setToolTip("LED layout height (grid units) for new controllers");
    grid_gl->addWidget(grid_y_spin, 0, 5);
    grid_gl->addWidget(new QLabel("Z:"), 0, 6);
    grid_z_spin = new QSpinBox();
    grid_z_spin->setRange(1, 100);
    grid_z_spin->setValue(custom_grid_z);
    grid_z_spin->setToolTip("LED layout depth (grid units) for new controllers");
    grid_gl->addWidget(grid_z_spin, 0, 7);

    grid_gl->addWidget(new QLabel("Grid scale:"), 1, 0);
    grid_scale_spin = new QDoubleSpinBox();
    grid_scale_spin->setRange(0.1, 1000.0);
    grid_scale_spin->setSingleStep(1.0);
    grid_scale_spin->setValue(grid_scale_mm);
    grid_scale_spin->setSuffix(" mm/unit");
    grid_scale_spin->setToolTip("Size of one grid unit in mm. Position in grid units × scale = real size in mm (e.g. scale 10 → 10 mm per unit).");
    grid_gl->addWidget(grid_scale_spin, 1, 1, 1, 2);

    grid_snap_checkbox = new QCheckBox("Snap positions to grid");
    grid_snap_checkbox->setToolTip("When moving controllers, snap to grid intersections.");
    if(viewport) grid_snap_checkbox->setChecked(viewport->IsGridSnapEnabled());
    grid_gl->addWidget(grid_snap_checkbox, 1, 3, 1, 2);

    selection_info_label = new QLabel("No selection");
    selection_info_label->setAlignment(Qt::AlignRight);
    QFont selection_font = selection_info_label->font();
    selection_font.setBold(true);
    selection_info_label->setFont(selection_font);
    grid_gl->addWidget(new QLabel("Selection:"), 1, 5);
    grid_gl->addWidget(selection_info_label, 1, 6, 1, 2);

    QLabel* grid_scale_help = new QLabel("Default size for new LED layouts; scale is mm per grid unit.");
    PluginUiApplyMutedSecondaryLabel(grid_scale_help);
    grid_scale_help->setWordWrap(true);
    grid_gl->addWidget(grid_scale_help, 2, 0, 1, 8);

    connect(grid_scale_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value){
        grid_scale_mm = (float)value;
        if(viewport) {
            viewport->SetGridScaleMM(grid_scale_mm);
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        if(current_effect_ui)
        {
            ScreenMirror* sm = qobject_cast<ScreenMirror*>(current_effect_ui);
            if(sm)
                sm->SetGridScaleMM(grid_scale_mm);
        }
        for(unsigned int i = 0; i < controller_transforms.size(); i++)
        {
            RegenerateLEDPositions(controller_transforms[i].get());
            ControllerLayout3D::UpdateWorldPositions(controller_transforms[i].get());
        }
        if(viewport)
        {
            viewport->SetControllerTransforms(&controller_transforms);
            viewport->update();
        }
    });

    grid_tab_main->addWidget(grid_scale_group);

    QGroupBox* room_group = new QGroupBox("Room size");
    QGridLayout* room_gl = new QGridLayout(room_group);
    room_gl->setSpacing(4);

    use_manual_room_size_checkbox = new QCheckBox("Use manual room size");
    use_manual_room_size_checkbox->setChecked(use_manual_room_size);
    use_manual_room_size_checkbox->setToolTip("Off: room size is derived from LED positions. On: set width, height, depth below.");
    room_gl->addWidget(use_manual_room_size_checkbox, 0, 0, 1, 6);

    std::function<void(QGridLayout*, int, int, const QString&, QDoubleSpinBox*&, double, const QString&)> add_room_dim_spin =
        [this](QGridLayout* lo, int row, int col, const QString& label, QDoubleSpinBox*& spin, double value, const QString& tooltip)
    {
        lo->addWidget(new QLabel(label), row, col);
        spin = new QDoubleSpinBox();
        spin->setRange(100.0, 50000.0);
        spin->setSingleStep(10.0);
        spin->setValue(value);
        spin->setSuffix(" mm");
        spin->setToolTip(tooltip);
        spin->setEnabled(use_manual_room_size);
        lo->addWidget(spin, row, col + 1);
    };

    add_room_dim_spin(room_gl, 1, 0, "Width (X, mm):", room_width_spin, manual_room_width, "Left to right, in mm");
    add_room_dim_spin(room_gl, 1, 2, "Height (Y, mm):", room_height_spin, manual_room_height, "Floor to ceiling, in mm");
    add_room_dim_spin(room_gl, 1, 4, "Depth (Z, mm):", room_depth_spin, manual_room_depth, "Front to back, in mm");

    QLabel* room_help = new QLabel("Origin is front-left-floor. Room dimensions and grid scale are in mm. Positions in grid units × scale = mm.");
    PluginUiApplyMutedSecondaryLabel(room_help);
    room_help->setWordWrap(true);
    room_gl->addWidget(room_help, 2, 0, 1, 6);

    grid_tab_main->addWidget(room_group);

    QGroupBox* overlay_group = new QGroupBox("3D view overlay");
    QGridLayout* overlay_gl = new QGridLayout(overlay_group);
    overlay_gl->setSpacing(4);

    room_grid_overlay_checkbox = new QCheckBox("Show overlay in 3D view");
    room_grid_overlay_checkbox->setToolTip("Draw a dim grid of points in the room so you see the space. Real LEDs stand out.");
    overlay_gl->addWidget(room_grid_overlay_checkbox, 0, 0, 1, 3);

    int overlay_bright_pct = 35;
    int overlay_point_size = 3;
    int overlay_step = 4;

    overlay_gl->addWidget(new QLabel("Overlay brightness:"), 1, 0);
    QSlider* room_grid_overlay_bright_slider = new QSlider(Qt::Horizontal);
    room_grid_overlay_bright_slider->setRange(0, 100);
    room_grid_overlay_bright_slider->setValue(overlay_bright_pct);
    room_grid_overlay_bright_slider->setToolTip("How bright the preview grid points are (0–100%).");
    QLabel* room_grid_overlay_bright_label = new QLabel(QString::number(overlay_bright_pct) + "%");
    room_grid_overlay_bright_label->setMinimumWidth(44);
    overlay_gl->addWidget(room_grid_overlay_bright_slider, 1, 1);
    overlay_gl->addWidget(room_grid_overlay_bright_label, 1, 2);

    overlay_gl->addWidget(new QLabel("Point size:"), 2, 0);
    QSlider* room_grid_overlay_point_slider = new QSlider(Qt::Horizontal);
    room_grid_overlay_point_slider->setRange(1, 12);
    room_grid_overlay_point_slider->setValue(overlay_point_size);
    room_grid_overlay_point_slider->setToolTip("OpenGL point size for each overlay dot (1–12).");
    QLabel* room_grid_overlay_point_label = new QLabel(QString::number(overlay_point_size));
    room_grid_overlay_point_label->setMinimumWidth(44);
    overlay_gl->addWidget(room_grid_overlay_point_slider, 2, 1);
    overlay_gl->addWidget(room_grid_overlay_point_label, 2, 2);

    overlay_gl->addWidget(new QLabel("Grid step:"), 3, 0);
    QSlider* room_grid_overlay_step_slider = new QSlider(Qt::Horizontal);
    room_grid_overlay_step_slider->setRange(1, 24);
    room_grid_overlay_step_slider->setValue(overlay_step);
    room_grid_overlay_step_slider->setToolTip("Sample every Nth grid cell per axis (1 = densest, 24 = sparsest).");
    QLabel* room_grid_overlay_step_label = new QLabel(QString::number(overlay_step));
    room_grid_overlay_step_label->setMinimumWidth(44);
    overlay_gl->addWidget(room_grid_overlay_step_slider, 3, 1);
    overlay_gl->addWidget(room_grid_overlay_step_label, 3, 2);

    QLabel* overlay_hint = new QLabel(
        "These match the RoomGrid fields in your plugin settings and are saved when you change them.");
    PluginUiApplyMutedSecondaryLabel(overlay_hint);
    overlay_hint->setWordWrap(true);
    overlay_gl->addWidget(overlay_hint, 4, 0, 1, 3);

    grid_tab_main->addWidget(overlay_group);

    connect(room_grid_overlay_checkbox, &QCheckBox::toggled, [this](bool checked) {
        if(viewport) viewport->SetShowRoomGridOverlay(checked);
        PersistRoomGridOverlayToSettings();
    });

    try
    {
        nlohmann::json settings = GetPluginSettings();
        if(settings.contains("RoomGrid"))
        {
            const nlohmann::json& rg = settings["RoomGrid"];
            bool show = rg.value("Show", false);
            overlay_bright_pct = (int)(rg.value("Brightness", 0.35) * 100.0);
            overlay_point_size = (int)rg.value("PointSize", 3.0);
            overlay_step = (int)rg.value("Step", 4);
            overlay_bright_pct = std::max(0, std::min(100, overlay_bright_pct));
            overlay_point_size = std::max(1, std::min(12, overlay_point_size));
            overlay_step = std::max(1, std::min(24, overlay_step));
            if(room_grid_overlay_checkbox) room_grid_overlay_checkbox->setChecked(show);
            room_grid_overlay_bright_slider->setValue(overlay_bright_pct);
            room_grid_overlay_point_slider->setValue(overlay_point_size);
            room_grid_overlay_step_slider->setValue(overlay_step);
            room_grid_overlay_bright_label->setText(QString::number(overlay_bright_pct) + "%");
            room_grid_overlay_point_label->setText(QString::number(overlay_point_size));
            room_grid_overlay_step_label->setText(QString::number(overlay_step));
            if(viewport)
            {
                viewport->SetRoomGridBrightness((float)overlay_bright_pct / 100.0f);
                viewport->SetRoomGridPointSize((float)overlay_point_size);
                viewport->SetRoomGridStep(overlay_step);
            }
        }
    }
    catch(const std::exception&) {}

    connect(room_grid_overlay_bright_slider, &QSlider::valueChanged, this,
            [this, room_grid_overlay_bright_label](int v)
            {
                v = std::max(0, std::min(100, v));
                if(room_grid_overlay_bright_label) room_grid_overlay_bright_label->setText(QString::number(v) + "%");
                if(viewport) viewport->SetRoomGridBrightness((float)v / 100.0f);
                PersistRoomGridOverlayToSettings();
            });
    connect(room_grid_overlay_point_slider, &QSlider::valueChanged, this,
            [this, room_grid_overlay_point_label](int v)
            {
                v = std::max(1, std::min(12, v));
                if(room_grid_overlay_point_label) room_grid_overlay_point_label->setText(QString::number(v));
                if(viewport) viewport->SetRoomGridPointSize((float)v);
                PersistRoomGridOverlayToSettings();
            });
    connect(room_grid_overlay_step_slider, &QSlider::valueChanged, this,
            [this, room_grid_overlay_step_label](int v)
            {
                v = std::max(1, std::min(24, v));
                if(room_grid_overlay_step_label) room_grid_overlay_step_label->setText(QString::number(v));
                if(viewport) viewport->SetRoomGridStep(v);
                PersistRoomGridOverlayToSettings();
            });

    connect(grid_x_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_y_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_z_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_snap_checkbox, &QCheckBox::toggled, this, &OpenRGB3DSpatialTab::on_grid_snap_toggled);

    connect(use_manual_room_size_checkbox, &QCheckBox::toggled, [this](bool checked) {
        use_manual_room_size = checked;
        if(room_width_spin) room_width_spin->setEnabled(checked);
        if(room_height_spin) room_height_spin->setEnabled(checked);
        if(room_depth_spin) room_depth_spin->setEnabled(checked);

        if(viewport)
        {
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        emit GridLayoutChanged();
    });

    connect(room_width_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        manual_room_width = value;
        if(viewport) viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        emit GridLayoutChanged();
    });
    connect(room_height_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        manual_room_height = value;
        if(viewport) viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        emit GridLayoutChanged();
    });
    connect(room_depth_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        manual_room_depth = value;
        if(viewport) viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        emit GridLayoutChanged();
    });

    QWidget* transform_tab = new QWidget();
    QVBoxLayout* transform_tab_v = new QVBoxLayout(transform_tab);
    transform_tab_v->setSpacing(6);
    QLabel* transform_help = new QLabel(
        "Moves the selected controller, reference point, or display plane in the room (your layout). "
        "Does not change the camera view. Position in mm (same as Room size in Grid Settings). "
        "Axes: X = left/right (width), Y = floor→ceiling (height), Z = front→back (depth). "
        "Rotation in degrees.");
    transform_help->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(transform_help);
    transform_tab_v->addWidget(transform_help);

    QHBoxLayout* transform_layout = new QHBoxLayout();
    transform_layout->setSpacing(6);
    transform_layout->setContentsMargins(2, 2, 2, 2);

    QGroupBox* position_group = new QGroupBox("Scene object position (mm)");
    position_group->setToolTip("Where the selected controller, reference point, or display plane sits in the room. Not the camera.");
    QGridLayout* position_layout = new QGridLayout(position_group);
    position_layout->setSpacing(3);
    position_layout->setContentsMargins(0, 0, 0, 0);

    std::function<void(int, const QString&, QSlider*&, QDoubleSpinBox*&, int, bool, const QString&)> add_position_row =
        [this, position_layout](int row,
                                const QString& label,
                                QSlider*& sl,
                                QDoubleSpinBox*& sp,
                                int axis,
                                bool clamp_non_negative,
                                const QString& axis_tip_mm)
    {
        position_layout->addWidget(new QLabel(label), row, 0);
        sl = new QSlider(Qt::Horizontal);
        sl->setRange(-50000, 50000);
        sl->setValue(0);
        sl->setToolTip(axis_tip_mm);
        sp = new QDoubleSpinBox();
        sp->setRange(-50000.0, 50000.0);
        sp->setDecimals(0);
        sp->setSingleStep(10.0);
        sp->setMaximumWidth(80);
        sp->setSuffix(" mm");
        sp->setToolTip(axis_tip_mm);
        position_layout->addWidget(sl, row, 1);
        position_layout->addWidget(sp, row, 2);

        if(clamp_non_negative)
        {
            connect(sl, &QSlider::valueChanged, [this, sl, sp, axis](int value_mm) {
                if(value_mm < 0)
                {
                    if(sp) { QSignalBlocker b(sp); sp->setValue(0); }
                    if(sl) { QSignalBlocker b(sl); sl->setValue(0); }
                    ApplyPositionComponent(axis, 0.0);
                    return;
                }
                double scale = (grid_scale_spin != nullptr) ? grid_scale_spin->value() : (double)grid_scale_mm;
                if(scale < 0.001) scale = 10.0;
                double grid_units = (double)value_mm / scale;
                if(sp) { QSignalBlocker b(sp); sp->setValue(value_mm); }
                ApplyPositionComponent(axis, grid_units);
            });
            connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this, sl, sp, axis](double value_mm) {
                if(value_mm < 0.0)
                {
                    if(sp) { QSignalBlocker b(sp); sp->setValue(0); }
                    if(sl) { QSignalBlocker b(sl); sl->setValue(0); }
                    ApplyPositionComponent(axis, 0.0);
                    return;
                }
                double scale = (grid_scale_spin != nullptr) ? grid_scale_spin->value() : (double)grid_scale_mm;
                if(scale < 0.001) scale = 10.0;
                double grid_units = value_mm / scale;
                if(sl) { QSignalBlocker b(sl); sl->setValue((int)std::lround(value_mm)); }
                ApplyPositionComponent(axis, grid_units);
            });
        }
        else
        {
            connect(sl, &QSlider::valueChanged, [this, sl, sp, axis](int value_mm) {
                double scale = (grid_scale_spin != nullptr) ? grid_scale_spin->value() : (double)grid_scale_mm;
                if(scale < 0.001) scale = 10.0;
                double grid_units = (double)value_mm / scale;
                if(sp) { QSignalBlocker b(sp); sp->setValue(value_mm); }
                ApplyPositionComponent(axis, grid_units);
            });
            connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this, sl, sp, axis](double value_mm) {
                double scale = (grid_scale_spin != nullptr) ? grid_scale_spin->value() : (double)grid_scale_mm;
                if(scale < 0.001) scale = 10.0;
                double grid_units = value_mm / scale;
                if(sl) { QSignalBlocker b(sl); sl->setValue((int)std::lround(value_mm)); }
                ApplyPositionComponent(axis, grid_units);
            });
        }
    };

    add_position_row(0,
                     "X (width):",
                     pos_x_slider,
                     pos_x_spin,
                     0,
                     false,
                     QStringLiteral("Left–right in mm; same axis as room Width (X) and LED spacing X."));
    add_position_row(1,
                     "Y (height):",
                     pos_y_slider,
                     pos_y_spin,
                     1,
                     true,
                     QStringLiteral("Floor–ceiling in mm; same axis as room Height (Y). This row clamps to ≥0 from the UI."));
    add_position_row(2,
                     "Z (depth):",
                     pos_z_slider,
                     pos_z_spin,
                     2,
                     false,
                     QStringLiteral("Front–back in mm; same axis as room Depth (Z) and LED spacing Z."));

    position_layout->setColumnStretch(1, 1);

    QGroupBox* rotation_group = new QGroupBox("Scene object rotation (°)");
    rotation_group->setToolTip("Orientation of the selected controller, reference point, or display plane in the room. Not the camera view.");
    QGridLayout* rotation_layout = new QGridLayout(rotation_group);
    rotation_layout->setSpacing(3);
    rotation_layout->setContentsMargins(0, 0, 0, 0);

    std::function<void(int, const QString&, QSlider*&, QDoubleSpinBox*&, int)> add_rotation_row =
        [this, rotation_layout](int row, const QString& label, QSlider*& sl, QDoubleSpinBox*& sp, int axis)
    {
        rotation_layout->addWidget(new QLabel(label), row, 0);
        sl = new QSlider(Qt::Horizontal);
        sl->setRange(-180, 180);
        sl->setValue(0);
        sl->setToolTip("Rotation in degrees.");
        sp = new QDoubleSpinBox();
        sp->setRange(-180, 180);
        sp->setDecimals(1);
        sp->setMaximumWidth(80);
        sp->setSuffix(QString::fromUtf8(" °"));
        sp->setToolTip("Rotation in degrees.");
        rotation_layout->addWidget(sl, row, 1);
        rotation_layout->addWidget(sp, row, 2);
        connect(sl, &QSlider::valueChanged, [this, sl, sp, axis](int value) {
            double rot_value = (double)value;
            if(sp) { QSignalBlocker b(sp); sp->setValue(rot_value); }
            ApplyRotationComponent(axis, rot_value);
        });
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this, sl, sp, axis](double value) {
            if(sl) { QSignalBlocker b(sl); sl->setValue((int)std::lround(value)); }
            ApplyRotationComponent(axis, value);
        });
    };

    add_rotation_row(0, "X:", rot_x_slider, rot_x_spin, 0);
    add_rotation_row(1, "Y:", rot_y_slider, rot_y_spin, 1);
    add_rotation_row(2, "Z:", rot_z_slider, rot_z_spin, 2);

    rotation_layout->setColumnStretch(1, 1);

    transform_layout->addWidget(position_group, 1);
    transform_layout->addWidget(rotation_group, 1);

    transform_tab_v->addLayout(transform_layout);

    std::function<QScrollArea*(QWidget*)> wrap_tab_in_scroll = [](QWidget* content) {
        QScrollArea* sa = new QScrollArea();
        sa->setWidget(content);
        sa->setWidgetResizable(true);
        sa->setFrameShape(QFrame::NoFrame);
        sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        return sa;
    };

    settings_tabs->addTab(wrap_tab_in_scroll(transform_tab), "Scene object: position & rotation");
    settings_tabs->addTab(wrap_tab_in_scroll(grid_settings_tab), "Grid Settings");

    QWidget* object_creator_tab = new QWidget();
    QVBoxLayout* creator_layout = new QVBoxLayout();
    creator_layout->setSpacing(6);

    QLabel* type_label = new QLabel("Object Type:");
    QFont type_font = type_label->font();
    type_font.setBold(true);
    type_label->setFont(type_font);
    creator_layout->addWidget(type_label);

    QComboBox* object_type_combo = new QComboBox();
    object_type_combo->addItem("Select to Create...", -1);
    object_type_combo->addItem("Custom Controller", 0);
    object_type_combo->addItem("Reference Point", 1);
    object_type_combo->addItem("Display Plane", 2);
    object_type_combo->setMinimumWidth(240);
    creator_layout->addWidget(object_type_combo);

    object_creator_status_label = new QLabel();
    object_creator_status_label->setWordWrap(true);
    object_creator_status_label->setVisible(false);
    object_creator_status_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    creator_layout->addWidget(object_creator_status_label);

    QStackedWidget* creator_stack = new QStackedWidget();

    QWidget* empty_page = new QWidget();
    QVBoxLayout* empty_layout = new QVBoxLayout(empty_page);
    QLabel* empty_label = new QLabel("Choose Custom Controller, Reference Point, or Display Plane above to create objects and add them to the 3D view.");
    empty_label->setWordWrap(true);
    empty_label->setContentsMargins(0, 6, 0, 6);
    PluginUiApplyMutedSecondaryLabel(empty_label);
    QFont empty_font = empty_label->font();
    empty_font.setItalic(true);
    empty_label->setFont(empty_font);
    empty_label->setAlignment(Qt::AlignHCenter);
    empty_layout->addWidget(empty_label);
    empty_layout->addStretch();
    creator_stack->addWidget(empty_page);

    QWidget* custom_controller_page = new QWidget();
    QVBoxLayout* custom_layout = new QVBoxLayout(custom_controller_page);
    custom_layout->setSpacing(4);

    QLabel* custom_list_label = new QLabel("Available Custom Controllers:");
    QFont custom_list_font = custom_list_label->font();
    custom_list_font.setBold(true);
    custom_list_label->setFont(custom_list_font);
    custom_layout->addWidget(custom_list_label);

    QLabel* custom_subtitle = new QLabel("Create a grid of LEDs from your physical devices, then add it to the 3D view from the Available Controllers list.");
    custom_subtitle->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(custom_subtitle);
    custom_subtitle->setContentsMargins(0, 0, 0, 6);
    custom_layout->addWidget(custom_subtitle);

    custom_controllers_list = new QListWidget();
    custom_controllers_list->setMinimumHeight(150);
    custom_controllers_list->setToolTip("Select a custom controller to edit or export");
    custom_layout->addWidget(custom_controllers_list);
    connect(custom_controllers_list, &QListWidget::currentRowChanged, this, &OpenRGB3DSpatialTab::on_custom_controller_selection_changed);

    custom_controllers_empty_label = new QLabel("No custom controllers yet. Create one or import from file.");
    custom_controllers_empty_label->setWordWrap(true);
    PluginUiApplyItalicSecondaryLabel(custom_controllers_empty_label);
    custom_controllers_empty_label->setAlignment(Qt::AlignHCenter);
    custom_controllers_empty_label->setContentsMargins(0, 8, 0, 8);
    custom_layout->addWidget(custom_controllers_empty_label);

    QPushButton* custom_controller_button = new QPushButton("Create New Custom Controller");
    connect(custom_controller_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_create_custom_controller_clicked);
    custom_layout->addWidget(custom_controller_button);

    QHBoxLayout* custom_io_layout = new QHBoxLayout();
    QPushButton* add_from_preset_button = new QPushButton("Add from preset");
    add_from_preset_button->setToolTip("Add a pre-built custom controller from the preset library (uses official device names; multiple instances get numbered)");
    connect(add_from_preset_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_add_from_preset_clicked);
    custom_io_layout->addWidget(add_from_preset_button);
    QPushButton* import_button = new QPushButton("Import");
    import_button->setToolTip("Import a custom controller from file");
    connect(import_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_import_custom_controller_clicked);
    custom_io_layout->addWidget(import_button);

    export_custom_controller_btn = new QPushButton("Export");
    export_custom_controller_btn->setToolTip("Export selected custom controller to file");
    export_custom_controller_btn->setEnabled(false);
    connect(export_custom_controller_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_export_custom_controller_clicked);
    custom_io_layout->addWidget(export_custom_controller_btn);

    edit_custom_controller_btn = new QPushButton("Edit");
    edit_custom_controller_btn->setToolTip("Edit selected custom controller");
    edit_custom_controller_btn->setEnabled(false);
    connect(edit_custom_controller_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_edit_custom_controller_clicked);
    custom_io_layout->addWidget(edit_custom_controller_btn);

    delete_custom_controller_btn = new QPushButton("Delete");
    delete_custom_controller_btn->setToolTip("Remove selected custom controller from library (remove from 3D scene first if in use)");
    delete_custom_controller_btn->setEnabled(false);
    connect(delete_custom_controller_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_delete_custom_controller_clicked);
    custom_io_layout->addWidget(delete_custom_controller_btn);

    custom_layout->addLayout(custom_io_layout);
    custom_layout->addStretch();

    creator_stack->addWidget(custom_controller_page);

    QWidget* ref_point_page = new QWidget();
    QVBoxLayout* ref_points_layout = new QVBoxLayout(ref_point_page);
    ref_points_layout->setSpacing(4);

    QLabel* ref_list_label = new QLabel("Reference Points:");
    QFont ref_list_font = ref_list_label->font();
    ref_list_font.setBold(true);
    ref_list_label->setFont(ref_list_font);
    ref_points_layout->addWidget(ref_list_label);

    QLabel* ref_subtitle = new QLabel("Mark positions in the 3D room (e.g. monitor center). Add to the 3D view from Available Controllers.");
    ref_subtitle->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(ref_subtitle);
    ref_subtitle->setContentsMargins(0, 0, 0, 6);
    ref_points_layout->addWidget(ref_subtitle);

    reference_points_list = new QListWidget();
    reference_points_list->setMinimumHeight(150);
    connect(reference_points_list, &QListWidget::currentRowChanged, this, &OpenRGB3DSpatialTab::on_ref_point_selected);
    ref_points_layout->addWidget(reference_points_list);

    ref_points_empty_label = new QLabel("No reference points yet. Click Add Reference Point to create one.");
    ref_points_empty_label->setWordWrap(true);
    PluginUiApplyItalicSecondaryLabel(ref_points_empty_label);
    ref_points_empty_label->setAlignment(Qt::AlignHCenter);
    ref_points_empty_label->setContentsMargins(0, 8, 0, 8);
    ref_points_layout->addWidget(ref_points_empty_label);

    QHBoxLayout* name_layout = new QHBoxLayout();
    name_layout->addWidget(new QLabel("Name:"));
    ref_point_name_edit = new QLineEdit();
    ref_point_name_edit->setPlaceholderText("e.g., My Monitor");
    name_layout->addWidget(ref_point_name_edit);
    ref_points_layout->addLayout(name_layout);

    QHBoxLayout* type_layout = new QHBoxLayout();
    type_layout->addWidget(new QLabel("Type:"));
    ref_point_type_combo = new QComboBox();
    std::vector<std::string> type_names = VirtualReferencePoint3D::GetTypeNames();
    for(size_t i = 0; i < type_names.size(); i++)
    {
        ref_point_type_combo->addItem(QString::fromStdString(type_names[i]));
    }
    type_layout->addWidget(ref_point_type_combo);
    ref_points_layout->addLayout(type_layout);

    QHBoxLayout* color_layout = new QHBoxLayout();
    color_layout->addWidget(new QLabel("Color:"));
    ref_point_color_button = new QPushButton();
    ref_point_color_button->setFixedSize(30, 30);
    selected_ref_point_color = 0x00808080;
    unsigned int default_red = selected_ref_point_color & 0xFF;
    unsigned int default_green = (selected_ref_point_color >> 8) & 0xFF;
    unsigned int default_blue = (selected_ref_point_color >> 16) & 0xFF;
    PluginUiSetRgbSwatchButton(ref_point_color_button,
                               (int)default_red,
                               (int)default_green,
                               (int)default_blue);
    connect(ref_point_color_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_ref_point_color_clicked);
    color_layout->addWidget(ref_point_color_button);
    color_layout->addStretch();
    ref_points_layout->addLayout(color_layout);

    QLabel* help_label = new QLabel("Select a reference point to move it with the \"Scene object: position & rotation\" tab and 3D gizmo.");
    PluginUiApplyMutedSecondaryLabel(help_label);
    help_label->setWordWrap(true);
    ref_points_layout->addWidget(help_label);

    QHBoxLayout* ref_buttons_layout = new QHBoxLayout();
    add_ref_point_button = new QPushButton("Add Reference Point");
    connect(add_ref_point_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_add_ref_point_clicked);
    ref_buttons_layout->addWidget(add_ref_point_button);

    remove_ref_point_button = new QPushButton("Remove");
    remove_ref_point_button->setEnabled(false);
    connect(remove_ref_point_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_remove_ref_point_clicked);
    ref_buttons_layout->addWidget(remove_ref_point_button);

    ref_points_layout->addLayout(ref_buttons_layout);
    ref_points_layout->addStretch();

    creator_stack->addWidget(ref_point_page);

    QWidget* display_plane_page = new QWidget();
    QVBoxLayout* display_layout = new QVBoxLayout(display_plane_page);
    display_layout->setSpacing(6);

    QLabel* display_list_label = new QLabel("Display Planes:");
    QFont display_list_font = display_list_label->font();
    display_list_font.setBold(true);
    display_list_label->setFont(display_list_font);
    display_layout->addWidget(display_list_label);

    QLabel* display_subtitle = new QLabel("Add virtual screens for Screen Mirror and other effects. Add to the 3D view from Available Controllers.");
    display_subtitle->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(display_subtitle);
    display_subtitle->setContentsMargins(0, 0, 0, 6);
    display_layout->addWidget(display_subtitle);

    display_planes_list = new QListWidget();
    display_planes_list->setMinimumHeight(200);
    display_planes_list->setUniformItemSizes(true);
    display_planes_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(display_planes_list, &QListWidget::currentRowChanged, this, &OpenRGB3DSpatialTab::on_display_plane_selected);
    display_layout->addWidget(display_planes_list);

    display_planes_empty_label = new QLabel("No display planes yet. Click Add Display to create one.");
    display_planes_empty_label->setWordWrap(true);
    PluginUiApplyItalicSecondaryLabel(display_planes_empty_label);
    display_planes_empty_label->setAlignment(Qt::AlignHCenter);
    display_planes_empty_label->setContentsMargins(0, 8, 0, 8);
    display_layout->addWidget(display_planes_empty_label);

    QHBoxLayout* display_buttons = new QHBoxLayout();
    add_display_plane_button = new QPushButton("Add Display");
    connect(add_display_plane_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_add_display_plane_clicked);
    display_buttons->addWidget(add_display_plane_button);

    remove_display_plane_button = new QPushButton("Remove");
    remove_display_plane_button->setEnabled(false);
    connect(remove_display_plane_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_remove_display_plane_clicked);
    display_buttons->addWidget(remove_display_plane_button);

    display_layout->addLayout(display_buttons);

    QScrollArea* display_settings_scroll = new QScrollArea();
    display_settings_scroll->setWidgetResizable(true);
    display_settings_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    display_settings_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    display_settings_scroll->setFrameShape(QFrame::NoFrame);
    
    QWidget* settings_container = new QWidget();
    QVBoxLayout* settings_container_layout = new QVBoxLayout(settings_container);
    settings_container_layout->setContentsMargins(4, 4, 4, 4);
    settings_container_layout->setSpacing(6);

    QGridLayout* plane_form = new QGridLayout();
    plane_form->setHorizontalSpacing(8);
    plane_form->setVerticalSpacing(6);
    plane_form->setColumnStretch(1, 1);
    plane_form->setColumnStretch(3, 1);

    int plane_row = 0;
    plane_form->addWidget(new QLabel("Name:"), plane_row, 0);
    display_plane_name_edit = new QLineEdit();
    connect(display_plane_name_edit, &QLineEdit::textEdited, this, &OpenRGB3DSpatialTab::on_display_plane_name_edited);
    plane_form->addWidget(display_plane_name_edit, plane_row, 1, 1, 2);
    plane_row++;

    QLabel*     monitor_filter_label = new QLabel(tr("Filter:"));
    PluginUiApplyMutedSecondaryLabel(monitor_filter_label);
    plane_form->addWidget(monitor_filter_label, plane_row, 0);
    display_plane_monitor_brand_filter = new QComboBox();
    display_plane_monitor_brand_filter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    display_plane_monitor_brand_filter->addItem(tr("All brands"), QString());
    connect(display_plane_monitor_brand_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_monitor_filter_or_sort_changed);
    plane_form->addWidget(display_plane_monitor_brand_filter, plane_row, 1);

    QLabel* monitor_sort_label = new QLabel(tr("Sort:"));
    PluginUiApplyMutedSecondaryLabel(monitor_sort_label);
    plane_form->addWidget(monitor_sort_label, plane_row, 2);
    display_plane_monitor_sort_combo = new QComboBox();
    display_plane_monitor_sort_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    display_plane_monitor_sort_combo->addItem(tr("Brand"), QString("brand"));
    display_plane_monitor_sort_combo->addItem(tr("Model"), QString("model"));
    display_plane_monitor_sort_combo->addItem(tr("Size (width)"), QString("width"));
    connect(display_plane_monitor_sort_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_monitor_filter_or_sort_changed);
    plane_form->addWidget(display_plane_monitor_sort_combo, plane_row, 3);
    plane_row++;

    plane_form->addWidget(new QLabel("Monitor Preset:"), plane_row, 0);
    display_plane_monitor_combo = new QComboBox();
    display_plane_monitor_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    display_plane_monitor_combo->setEditable(true);
    display_plane_monitor_combo->setInsertPolicy(QComboBox::NoInsert);
    display_plane_monitor_combo->setPlaceholderText("Search brand or model...");
    display_plane_monitor_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    display_plane_monitor_combo->setMinimumContentsLength(20);
    if(QLineEdit* monitor_edit = display_plane_monitor_combo->lineEdit())
    {
        monitor_edit->setClearButtonEnabled(true);
        connect(monitor_edit, &QLineEdit::textEdited,
                this, &OpenRGB3DSpatialTab::on_monitor_preset_text_edited);
    }
    connect(display_plane_monitor_combo, QOverload<int>::of(&QComboBox::activated),
            this, &OpenRGB3DSpatialTab::on_display_plane_monitor_preset_selected);
    plane_form->addWidget(display_plane_monitor_combo, plane_row, 1, 1, 3);
    plane_row++;

    plane_form->addWidget(new QLabel("Width (mm):"), plane_row, 0);
    display_plane_width_spin = new QDoubleSpinBox();
    display_plane_width_spin->setRange(50.0, 5000.0);
    display_plane_width_spin->setDecimals(1);
    display_plane_width_spin->setSingleStep(10.0);
    connect(display_plane_width_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OpenRGB3DSpatialTab::on_display_plane_width_changed);
    plane_form->addWidget(display_plane_width_spin, plane_row, 1);

    plane_form->addWidget(new QLabel("Height (mm):"), plane_row, 2);
    display_plane_height_spin = new QDoubleSpinBox();
    display_plane_height_spin->setRange(50.0, 5000.0);
    display_plane_height_spin->setDecimals(1);
    display_plane_height_spin->setSingleStep(10.0);
    connect(display_plane_height_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OpenRGB3DSpatialTab::on_display_plane_height_changed);
    plane_form->addWidget(display_plane_height_spin, plane_row, 3);
    plane_row++;

    plane_form->addWidget(new QLabel("Capture Source:"), plane_row, 0);
    display_plane_capture_combo = new QComboBox();
    display_plane_capture_combo->setToolTip("Select which monitor/capture source to use");
    connect(display_plane_capture_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_display_plane_capture_changed);
    plane_form->addWidget(display_plane_capture_combo, plane_row, 1, 1, 2);

    display_plane_refresh_capture_btn = new QPushButton("Refresh");
    display_plane_refresh_capture_btn->setToolTip("Refresh list of available capture sources");
    connect(display_plane_refresh_capture_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_display_plane_refresh_capture_clicked);
    plane_form->addWidget(display_plane_refresh_capture_btn, plane_row, 3);
    plane_row++;

    settings_container_layout->addLayout(plane_form);

    display_plane_visible_check = new QCheckBox("Visible in viewport");
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(display_plane_visible_check, &QCheckBox::checkStateChanged,
            this, &OpenRGB3DSpatialTab::on_display_plane_visible_toggled);
#else
    connect(display_plane_visible_check, &QCheckBox::stateChanged,
            this, [this](int state) {
                on_display_plane_visible_toggled(static_cast<Qt::CheckState>(state));
            });
#endif
    settings_container_layout->addWidget(display_plane_visible_check);

    settings_container_layout->addStretch();
    
    display_settings_scroll->setWidget(settings_container);
    
    display_layout->addWidget(display_settings_scroll, 1);

    creator_stack->addWidget(display_plane_page);
    RefreshDisplayPlaneCaptureSourceList();

    creator_layout->addWidget(creator_stack);
    creator_layout->addStretch();

    connect(object_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, creator_stack](int index) {
        if(index == 0) creator_stack->setCurrentIndex(0);
        else if(index == 1) creator_stack->setCurrentIndex(1);
        else if(index == 2) creator_stack->setCurrentIndex(2);
        else if(index == 3)
        {
            creator_stack->setCurrentIndex(3);
            UpdateDisplayPlanesList();
            RefreshDisplayPlaneDetails();
        }
    });
    object_type_combo->setCurrentIndex(0);

    object_creator_tab->setLayout(creator_layout);
    settings_tabs->addTab(wrap_tab_in_scroll(object_creator_tab), "Object Creator");

    LoadMonitorPresets();

    RefreshDisplayPlaneCaptureSourceList();

    SetupProfilesTab(settings_tabs);

    const int kSettingsTabsMinHeight = 320;
    settings_tabs->setMinimumHeight(kSettingsTabsMinHeight);
    settings_tabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    main_splitter->addWidget(center_widget);

    effects_detail_scroll = new QScrollArea();
    effects_detail_scroll->setWidgetResizable(true);
    effects_detail_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    effects_detail_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget* detail_content = new QWidget();
    QVBoxLayout* detail_layout = new QVBoxLayout(detail_content);
    detail_layout->setContentsMargins(4, 4, 4, 4);
    detail_layout->setSpacing(6);

    minecraft_library_panel = new QGroupBox(tr("Minecraft (Fabric) — add to stack"));
    minecraft_library_panel->setVisible(false);
    new QVBoxLayout(minecraft_library_panel);

    audio_library_panel = new QGroupBox(tr("Audio Effect — configure a range, then add to the stack"));
    audio_library_panel->setVisible(false);
    new QVBoxLayout(audio_library_panel);

    effect_config_group = new QGroupBox(tr("Effect global settings"));
    effect_config_group->setToolTip(tr(
        "Same controls as other stack effects: stack layer and zone apply to the selected layer. "
        "Effect center is applied globally to every running layer when the stack renders (see RenderEffectStack)."));
    effect_config_group->setVisible(false);
    QVBoxLayout* effect_layout = new QVBoxLayout(effect_config_group);
    effect_layout->setSpacing(6);

    effect_stack_row_label = new QLabel(tr("Effect:"));
    effect_stack_row_label->setToolTip(tr("Which stack layer is active for editing and preview (same list as Effect Layers on the left)."));

    effect_combo = new QComboBox();
    effect_combo->setToolTip(tr("Select an effect layer from the stack to edit its controls."));
    connect(effect_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_effect_changed);
    UpdateEffectCombo();
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(effect_stack_row_label);
        row->addWidget(effect_combo, 1);
        effect_layout->addLayout(row);
    }

    effect_zone_label = new QLabel(tr("Zone:"));
    effect_zone_label->setToolTip(tr(
        "Which LEDs this stack layer considers: all controllers, one zone, or one controller. "
        "Pair with Target bounds: zone bounds remap pattern coordinates into that box (good for strips or partial rooms); "
        "per-effect placement and coverage stay under Effect Controls (geometry and Scale)."));
    effect_zone_combo = new QComboBox();
    effect_zone_combo->setToolTip(tr(
        "Target selection for the selected layer. "
        "Target zone bounds (below) needs a real zone or a meaningful selection so the local grid can be built."));
    PopulateZoneTargetCombo(effect_zone_combo, -1);
    connect(effect_zone_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_effect_zone_changed(int)));
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(effect_zone_label);
        row->addWidget(effect_zone_combo, 1);
        effect_layout->addLayout(row);
    }

    origin_label = new QLabel(tr("Spatial anchor:"));
    origin_label->setToolTip(tr(
        "Where patterns attach in space for the whole stack. "
        "\"Mapped lights center\" (default) uses the average LED position so effects sit in your real layout, not an abstract room middle."));
    effect_origin_combo = new QComboBox();
    effect_origin_combo->setToolTip(tr(
        "Applied to every effect layer when rendering. Use \"Mapped lights center\" so origin-based math follows your strips/hardware; "
        "pick Room box center only when you want the manual room middle."));
    effect_origin_combo->addItem(tr("Mapped lights center (recommended)"), QVariant(-4));
    effect_origin_combo->addItem(tr("Room box center"), QVariant(-1));
    effect_origin_combo->addItem(tr("Target zone center"), QVariant(-2));
    effect_origin_combo->addItem(tr("No anchor (world 0,0,0)"), QVariant(-3));
    connect(effect_origin_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_effect_origin_changed);
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(origin_label);
        row->addWidget(effect_origin_combo, 1);
        effect_layout->addLayout(row);
    }

    effect_bounds_label = new QLabel(tr("Target bounds:"));
    effect_bounds_label->setToolTip(tr(
        "Which coordinate span the effect samples for this layer: full room/world grid, or the selected target's axis-aligned box. "
        "Does not replace per-effect geometry (center offset, axis scale, rotations, Scale slider)."));
    effect_bounds_combo = new QComboBox();
    effect_bounds_combo->setToolTip(tr(
        "Global: sample using the normal room (or world) grid. "
        "Target zone: sample positions mapped across the zone (or target) bounding box so motion and detail read on that volume."));
    effect_bounds_combo->addItem("Global bounds", QVariant((int)SpatialEffect3D::BOUNDS_MODE_GLOBAL));
    effect_bounds_combo->setItemData(0,
        tr("Use the full room or world grid bounds for this layer's pattern math (same space as the voxel preview)."),
        Qt::ToolTipRole);
    effect_bounds_combo->addItem("Target zone bounds", QVariant((int)SpatialEffect3D::BOUNDS_MODE_TARGET_ZONE));
    effect_bounds_combo->setItemData(1,
        tr("Build a local grid from the Zone target's bounding box and sample the effect in that space—useful when LEDs only cover part of the room."),
        Qt::ToolTipRole);
    connect(effect_bounds_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_effect_bounds_changed);
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(effect_bounds_label);
        row->addWidget(effect_bounds_combo, 1);
        effect_layout->addLayout(row);
    }

    stack_effect_type_combo = new QComboBox(effect_config_group);
    stack_effect_type_combo->addItem("None", "");
    std::vector<EffectRegistration3D> effect_list = EffectListManager3D::get()->GetAllEffects();
    for(unsigned int i = 0; i < effect_list.size(); i++)
    {
        stack_effect_type_combo->addItem(QString::fromStdString(effect_list[i].ui_name),
                                         QString::fromStdString(effect_list[i].class_name));
    }
    stack_effect_type_combo->setVisible(false);
    connect(stack_effect_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_stack_effect_type_changed);

    stack_effect_zone_combo = new QComboBox(effect_config_group);
    stack_effect_zone_combo->addItem("All Controllers", -1);
    stack_effect_zone_combo->setVisible(false);
    connect(stack_effect_zone_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_stack_effect_zone_changed);

    UpdateStackEffectZoneCombo();

    effect_controls_widget = new QWidget();
    effect_controls_layout = new QVBoxLayout();
    effect_controls_layout->setContentsMargins(0, 0, 0, 0);
    effect_controls_widget->setLayout(effect_controls_layout);

    LoadStackPresets();
    LoadEffectStack();
    UpdateEffectStackList();
    if(!effect_stack.empty() && effect_stack_list)
    {
        {
            const QSignalBlocker stack_sel_block(effect_stack_list);
            effect_stack_list->setCurrentRow(0);
        }
        on_effect_stack_selection_changed(0);
    }
    UpdateStartStopAllButtons();

    detail_layout->addWidget(effect_config_group);
    detail_layout->addWidget(effect_controls_widget);
    detail_layout->addWidget(minecraft_library_panel);
    detail_layout->addWidget(audio_library_panel);

    SetupAudioPanel(detail_layout);

    detail_layout->addStretch();
    effect_controls_widget->updateGeometry();

    effects_detail_scroll->setWidget(detail_content);

    QStackedWidget* right_stacked = new QStackedWidget();
    right_stacked->addWidget(effects_detail_scroll);
    right_stacked->addWidget(settings_tabs);
    connect(left_mode_tabs, QOverload<int>::of(&QTabWidget::currentChanged), right_stacked, &QStackedWidget::setCurrentIndex);
    right_stacked->setCurrentIndex(left_mode_tabs->currentIndex());

    main_splitter->addWidget(right_stacked);

    QList<int> splitter_sizes;
    splitter_sizes << kLeftPanelMinWidth << 400 << 320;
    main_splitter->setSizes(splitter_sizes);

    root_layout->addWidget(main_splitter);
    setLayout(root_layout);
}

void OpenRGB3DSpatialTab::SetupEffectLibraryPanel(QVBoxLayout* parent_layout)
{
    QGroupBox* library_group = new QGroupBox("Effect Library");
    QVBoxLayout* library_layout = new QVBoxLayout(library_group);
    library_layout->setSpacing(4);

    QLabel* category_label = new QLabel(tr("Filter:"));
    PluginUiApplyMutedSecondaryLabel(category_label);
    library_layout->addWidget(category_label);

    effect_category_combo = new QComboBox();
    effect_category_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(effect_category_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_effect_library_category_changed);
    library_layout->addWidget(effect_category_combo);

    effect_library_search = new QLineEdit();
    effect_library_search->setPlaceholderText(tr("Search effects..."));
    effect_library_search->setClearButtonEnabled(true);
    connect(effect_library_search, &QLineEdit::textChanged, this, &OpenRGB3DSpatialTab::on_effect_library_search_changed);
    library_layout->addWidget(effect_library_search);

    effect_library_list = new QListWidget();
    effect_library_list->setSelectionMode(QAbstractItemView::SingleSelection);
    effect_library_list->setMinimumHeight(160);
    connect(effect_library_list, &QListWidget::currentRowChanged, this, &OpenRGB3DSpatialTab::on_effect_library_selection_changed);
    connect(effect_library_list, &QListWidget::itemDoubleClicked, this, &OpenRGB3DSpatialTab::on_effect_library_item_double_clicked);
    library_layout->addWidget(effect_library_list);

    QHBoxLayout* library_button_layout = new QHBoxLayout();
    library_button_layout->addStretch();
    effect_library_add_button = new QPushButton("Add To Stack");
    effect_library_add_button->setEnabled(false);
    connect(effect_library_add_button, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_effect_library_add_clicked);
    library_button_layout->addWidget(effect_library_add_button);
    library_layout->addLayout(library_button_layout);

    parent_layout->addWidget(library_group);

    PopulateEffectLibraryCategories();
    PopulateEffectLibrary();
}

void OpenRGB3DSpatialTab::PopulateEffectLibraryCategories()
{
    if(!effect_category_combo)
    {
        return;
    }

    bool restore_signals = effect_category_combo->blockSignals(true);
    effect_category_combo->clear();
    effect_category_combo->addItem(tr("Select Category"), QVariant());

    std::map<std::string, std::vector<EffectRegistration3D>> categorized = EffectListManager3D::get()->GetCategorizedEffects();
    for(std::map<std::string, std::vector<EffectRegistration3D>>::const_iterator cit = categorized.begin();
        cit != categorized.end();
        ++cit)
    {
        effect_category_combo->addItem(QString::fromStdString(cit->first), QString::fromStdString(cit->first));
    }
    effect_category_combo->blockSignals(restore_signals);
    effect_category_combo->setCurrentIndex(0);
    PopulateEffectLibrary();
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
    if(!effect_library_list)
    {
        return;
    }

    bool restore_signals = effect_library_list->blockSignals(true);
    effect_library_list->clear();

    QVariant category_data;
    if(effect_category_combo)
    {
        category_data = effect_category_combo->currentData();
    }

    if(!category_data.isValid())
    {
        effect_library_list->blockSignals(restore_signals);
        on_effect_library_selection_changed(-1);
        return;
    }

    QString selected_category = category_data.toString();
    QString search = effect_library_search ? effect_library_search->text().trimmed() : QString();

    std::map<std::string, std::vector<EffectRegistration3D>> categorized = EffectListManager3D::get()->GetCategorizedEffects();
    std::map<std::string, std::vector<EffectRegistration3D>>::iterator cat_it =
        categorized.find(selected_category.toStdString());
    if(cat_it == categorized.end())
    {
        effect_library_list->blockSignals(restore_signals);
        on_effect_library_selection_changed(-1);
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
    const bool show_minecraft_panel = game_category && MinecraftEffectLibrary::SearchMatchesFamily(search);

    for(const EffectRegistration3D& reg : effects)
    {
        if(audio_category && reg.class_name != AudioEffectLibrary::HubClassName())
        {
            continue;
        }
        if(MinecraftEffectLibrary::IsCollapsedClass(reg.class_name))
        {
            continue;
        }
        if(!search.isEmpty() && !QString::fromStdString(reg.ui_name).contains(search, Qt::CaseInsensitive))
        {
            continue;
        }
        rows.push_back({QString::fromStdString(reg.ui_name), QString::fromStdString(reg.class_name), selected_category});
    }
    if(show_minecraft_panel)
    {
        rows.push_back({QStringLiteral("Minecraft (Fabric)"),
                        QString::fromUtf8(MinecraftEffectLibrary::LibraryHubClassId()),
                        selected_category});
    }

    std::sort(rows.begin(), rows.end(), [](const LibRow& a, const LibRow& b) {
        return QString::localeAwareCompare(a.text, b.text) < 0;
    });

    for(const LibRow& r : rows)
    {
        QListWidgetItem* item = new QListWidgetItem(r.text);
        if(r.class_name == QString::fromUtf8(MinecraftEffectLibrary::LibraryHubClassId()))
        {
            item->setToolTip(tr(
                "Single-click opens the hub on the right; double-click opens it again if the panel switched away. "
                "Configure, then use Add Minecraft layer or Add To Stack to append a layer (the hub stays open)."));
        }
        else if(r.class_name == QString::fromUtf8(AudioEffectLibrary::HubClassName()))
        {
            item->setToolTip(tr(
                "Frequency ranges and per-range effects are configured inside this layer. "
                "Add it once; individual audio algorithms are not separate library entries."));
        }
        else
        {
            item->setToolTip(QStringLiteral("Category: %1").arg(r.category));
        }
        item->setData(Qt::UserRole, r.class_name);
        item->setData(Qt::UserRole + 1, r.category);
        effect_library_list->addItem(item);
    }

    effect_library_list->blockSignals(restore_signals);
    effect_library_list->setCurrentRow(-1);
    on_effect_library_selection_changed(-1);
}

void OpenRGB3DSpatialTab::on_effect_library_search_changed(const QString&)
{
    PopulateEffectLibrary();
}

void OpenRGB3DSpatialTab::AddEffectInstanceToStack(const QString& class_name,
                                                   const QString& ui_name,
                                                   int zone_index,
                                                   BlendMode blend_mode,
                                                   const nlohmann::json* preset_settings,
                                                   bool enabled,
                                                   bool keep_minecraft_hub_panel_after_add)
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
    if(effect_stack_list)
    {
        restore_stack_list_signals = effect_stack_list->blockSignals(true);
    }
    UpdateEffectStackList();
    EffectInstance3D* new_instance = effect_stack[new_index].get();

    if(keep_minecraft_hub_panel_after_add)
    {
        if(effect_stack_list)
        {
            effect_stack_list->setCurrentRow(new_index);
        }
        if(new_instance && effect_zone_combo)
        {
            QSignalBlocker zb(effect_zone_combo);
            int zone_combo_index = effect_zone_combo->findData(new_instance->zone_index);
            if(zone_combo_index >= 0)
            {
                effect_zone_combo->setCurrentIndex(zone_combo_index);
            }
        }
        UpdateEffectCombo();
        UpdateAudioPanelVisibility();
        if(effect_stack_list)
        {
            effect_stack_list->blockSignals(restore_stack_list_signals);
        }
        SaveEffectStack();
        if(effect_library_list && effectLibraryRowIsMinecraftHub(effect_library_list->currentRow()))
        {
            ShowMinecraftHubConfigurator();
        }
        return;
    }

    if(new_instance)
    {
        LoadStackEffectControls(new_instance);
        if(effect_zone_combo)
        {
            QSignalBlocker zb(effect_zone_combo);
            int zone_combo_index = effect_zone_combo->findData(new_instance->zone_index);
            if(zone_combo_index >= 0)
            {
                effect_zone_combo->setCurrentIndex(zone_combo_index);
            }
        }
        if(effect_combo && new_index < effect_combo->count())
        {
            QSignalBlocker cb(effect_combo);
            effect_combo->setCurrentIndex(new_index);
        }
        UpdateEffectCombo();
        UpdateAudioPanelVisibility();
    }
    if(effect_stack_list)
    {
        effect_stack_list->setCurrentRow(new_index);
        effect_stack_list->blockSignals(restore_stack_list_signals);
    }
    on_effect_stack_selection_changed(new_index);
    SaveEffectStack();
}

void OpenRGB3DSpatialTab::on_effect_type_changed(int index)
{
    ClearCustomEffectUI();

    QString class_name;
    if(effect_type_combo && index >= 0)
    {
        class_name = effect_type_combo->itemData(index, kEffectRoleClassName).toString();
    }
    SetupCustomEffectUI(class_name);
}

void OpenRGB3DSpatialTab::SetupCustomEffectUI(const QString& class_name)
{
    if(!effect_controls_widget || !effect_controls_layout)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect controls widget or layout is null!");
        return;
    }

    if(!effect_zone_combo)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect zone combo is null!");
        return;
    }

    if(!effect_origin_combo)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect origin combo is null!");
        return;
    }

    if(!zone_manager)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Zone manager is null!");
        return;
    }

    if(!viewport)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Viewport is null!");
        return;
    }

    if(class_name.isEmpty())
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Attempted to setup effect with empty class name");
        return;
    }

    const std::string class_name_std = class_name.toStdString();
    EffectSettingsUiMount mount = createEffectSettingsUi(effect_controls_widget,
                                                       effect_controls_layout,
                                                       class_name_std,
                                                       settingsLayoutForClass(class_name_std));
    SpatialEffect3D* effect = mount.effect;
    if(!effect)
    {
        return;
    }

    current_effect_ui = effect;

    if(class_name == QLatin1String("ScreenMirror"))
    {
        configureScreenMirrorEffectUi(effect);
        if(origin_label)
        {
            origin_label->setVisible(false);
        }
        if(effect_origin_combo)
        {
            effect_origin_combo->setVisible(false);
        }
    }
    else
    {
        if(origin_label)
        {
            origin_label->setVisible(true);
        }
        if(effect_origin_combo)
        {
            effect_origin_combo->setVisible(true);
        }
    }

    start_effect_button = effect->GetStartButton();
    stop_effect_button = effect->GetStopButton();

    connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
    connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
    connect(effect, &SpatialEffect3D::ParametersChanged, this, [this]() {
        RefreshEffectDisplay();
    });

    effect_controls_widget->setVisible(true);
    effect_controls_widget->updateGeometry();
    effect_controls_widget->update();
}

void OpenRGB3DSpatialTab::SetupZonesPanel(QVBoxLayout* parent_layout)
{
    QGroupBox* zones_group = new QGroupBox("Zones");
    QVBoxLayout* zones_layout = new QVBoxLayout(zones_group);
    zones_layout->setSpacing(4);

    zones_list = new QListWidget();
    zones_list->setMinimumHeight(200);
    connect(zones_list, &QListWidget::currentRowChanged, this, &OpenRGB3DSpatialTab::on_zone_selected);
    zones_layout->addWidget(zones_list);

    QLabel* zones_help_label = new QLabel("Zones are groups of controllers for targeting effects.\n\nCreate zones like 'Desk', 'Front Wall', 'Ceiling', etc., then select them when configuring effects.");
    PluginUiApplyMutedSecondaryLabel(zones_help_label);
    zones_help_label->setWordWrap(true);
    zones_layout->addWidget(zones_help_label);

    QHBoxLayout* zone_buttons_layout = new QHBoxLayout();
    create_zone_button = new QPushButton("Create Zone");
    connect(create_zone_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_create_zone_clicked);
    zone_buttons_layout->addWidget(create_zone_button);

    edit_zone_button = new QPushButton("Edit");
    edit_zone_button->setEnabled(false);
    connect(edit_zone_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_edit_zone_clicked);
    zone_buttons_layout->addWidget(edit_zone_button);

    delete_zone_button = new QPushButton("Delete");
    delete_zone_button->setEnabled(false);
    connect(delete_zone_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_delete_zone_clicked);
    zone_buttons_layout->addWidget(delete_zone_button);

    zones_layout->addLayout(zone_buttons_layout);
    zones_layout->addStretch();

    parent_layout->addWidget(zones_group);
}

void OpenRGB3DSpatialTab::on_effect_library_category_changed(int)
{
    PopulateEffectLibrary();
}

void OpenRGB3DSpatialTab::on_effect_library_selection_changed(int row)
{
    if(effectLibraryRowIsMinecraftHub(row))
    {
        ShowMinecraftHubConfigurator();
    }
    else if(effectLibraryRowIsAudioHub(row))
    {
        ShowAudioHubConfigurator();
    }
    else
    {
        const int si = effect_stack_list ? effect_stack_list->currentRow() : -1;
        if(si >= 0 && si < (int)effect_stack.size())
        {
            LoadStackEffectControls(effect_stack[si].get());
        }
        else
        {
            LoadStackEffectControls(nullptr);
            if(effect_config_group)
            {
                effect_config_group->setVisible(false);
            }
        }
    }

    if(effect_library_add_button)
    {
        effect_library_add_button->setEnabled(row >= 0);
    }

    UpdateEffectStackRowSelectorVisibility();
}

void OpenRGB3DSpatialTab::on_effect_library_add_clicked()
{
    if(!effect_library_list)
    {
        return;
    }

    int current_row = effect_library_list->currentRow();
    if(current_row < 0)
    {
        return;
    }
    if(effectLibraryRowIsMinecraftHub(current_row))
    {
        on_minecraft_library_add_clicked();
        return;
    }
    if(effectLibraryRowIsAudioHub(current_row))
    {
        on_audio_hub_add_range_clicked();
        return;
    }

    QListWidgetItem* item = effect_library_list->item(current_row);
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

void OpenRGB3DSpatialTab::on_effect_library_item_double_clicked(QListWidgetItem* item)
{
    if(!item)
    {
        return;
    }
    if(item->data(Qt::UserRole).toString() == QString::fromUtf8(MinecraftEffectLibrary::LibraryHubClassId()))
    {
        ShowMinecraftHubConfigurator();
        return;
    }
    if(item->data(Qt::UserRole).toString() == QString::fromUtf8(AudioEffectLibrary::HubClassName()))
    {
        ShowAudioHubConfigurator();
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

bool OpenRGB3DSpatialTab::effectLibraryRowIsMinecraftHub(int row) const
{
    if(row < 0 || !effect_library_list)
    {
        return false;
    }
    QListWidgetItem* it = effect_library_list->item(row);
    return it && it->data(Qt::UserRole).toString() == QString::fromUtf8(MinecraftEffectLibrary::LibraryHubClassId());
}

bool OpenRGB3DSpatialTab::effectLibraryRowIsAudioHub(int row) const
{
    if(row < 0 || !effect_library_list)
    {
        return false;
    }
    QListWidgetItem* it = effect_library_list->item(row);
    return it && it->data(Qt::UserRole).toString() == QString::fromUtf8(AudioEffectLibrary::HubClassName());
}

void OpenRGB3DSpatialTab::ClearAudioLibraryPanel()
{
    if(audio_hub_preview_effect)
    {
        disconnect(audio_hub_preview_effect, nullptr, this, nullptr);
    }
    audio_hub_preview_effect = nullptr;
    audio_hub_layer_combo = nullptr;
    audio_hub_preview_holder = nullptr;
    audio_hub_low_slider = nullptr;
    audio_hub_high_slider = nullptr;
    audio_hub_low_label = nullptr;
    audio_hub_high_label = nullptr;
    audio_hub_zone_combo = nullptr;
    audio_hub_origin_combo = nullptr;
    audio_library_hub_active = false;

    if(!audio_library_panel)
    {
        return;
    }

    if(freq_ranges_group && freq_ranges_group->parentWidget() == audio_library_panel && audio_library_panel->layout())
    {
        audio_library_panel->layout()->removeWidget(freq_ranges_group);
        freq_ranges_group->setParent(nullptr);
        freq_ranges_group->hide();
    }

    if(QLayout* lay = audio_library_panel->layout())
    {
        QLayoutItem* item = nullptr;
        while((item = lay->takeAt(0)) != nullptr)
        {
            if(QWidget* w = item->widget())
            {
                w->deleteLater();
            }
            delete item;
        }
    }
    audio_library_panel->setVisible(false);
    UpdateAudioPanelVisibility();
}

void OpenRGB3DSpatialTab::ClearMinecraftLibraryPanel()
{
    SpatialEffect3D* preview = minecraft_hub_preview_effect;
    if(preview)
    {
        disconnect(preview, nullptr, this, nullptr);
        if(start_effect_button)
        {
            disconnect(start_effect_button, nullptr, this, nullptr);
        }
        if(stop_effect_button)
        {
            disconnect(stop_effect_button, nullptr, this, nullptr);
        }
    }

    if(current_effect_ui == preview)
    {
        current_effect_ui = nullptr;
        start_effect_button = nullptr;
        stop_effect_button = nullptr;
    }

    minecraft_library_layer_combo = nullptr;
    minecraft_hub_preview_effect = nullptr;
    minecraft_hub_preview_holder = nullptr;
    minecraft_library_hub_active = false;

    if(!minecraft_library_panel)
    {
        UpdateAudioPanelVisibility();
        return;
    }

    if(QLayout* lay = minecraft_library_panel->layout())
    {
        QLayoutItem* item = nullptr;
        while((item = lay->takeAt(0)) != nullptr)
        {
            if(QWidget* w = item->widget())
            {
                w->deleteLater();
            }
            delete item;
        }
    }
    minecraft_library_panel->setVisible(false);
    UpdateAudioPanelVisibility();
}

void OpenRGB3DSpatialTab::ShowMinecraftHubConfigurator()
{
    if(!minecraft_library_panel)
    {
        return;
    }

    if(run_setup_tab_widget && run_setup_tab_widget->currentIndex() != 0)
    {
        run_setup_tab_widget->setCurrentIndex(0);
    }

    LoadStackEffectControls(nullptr);

    if(effect_config_group)
    {
        effect_config_group->setVisible(true);
    }
    if(effect_zone_combo)
    {
        effect_zone_combo->setEnabled(true);
    }
    if(effect_origin_combo)
    {
        effect_origin_combo->setEnabled(true);
    }
    if(origin_label)
    {
        origin_label->setVisible(true);
    }

    QVBoxLayout* ml = qobject_cast<QVBoxLayout*>(minecraft_library_panel->layout());
    if(!ml)
    {
        return;
    }

    minecraft_library_hub_active = true;

    QLabel* intro = new QLabel(tr(
        "Add one Minecraft layer at a time: choose the layer type, adjust settings in the scrollable area, then click Add Minecraft layer."));
    intro->setWordWrap(true);
    ml->addWidget(intro);

    QWidget* row = new QWidget(minecraft_library_panel);
    QHBoxLayout* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->addWidget(new QLabel(tr("Minecraft layer:"), row));
    minecraft_library_layer_combo = new QComboBox(row);
    minecraft_library_layer_combo->setMinimumWidth(220);
    minecraft_library_layer_combo->setToolTip(tr("Which Minecraft effect to add (UDP 127.0.0.1:9876). \"All layers (bundled)\" shows every channel in one place — use separate entries to build a stack."));
    for(const MinecraftEffectLibrary::Variant& var : MinecraftEffectLibrary::Variants())
    {
        minecraft_library_layer_combo->addItem(QString::fromUtf8(var.label), QString::fromUtf8(var.class_name));
    }
    {
        const int health_idx = minecraft_library_layer_combo->findData(QStringLiteral("MinecraftHealth"));
        int pick = health_idx >= 0 ? health_idx : (minecraft_library_layer_combo->count() > 1 ? 1 : 0);
        minecraft_library_layer_combo->setCurrentIndex(pick);
    }
    h->addWidget(minecraft_library_layer_combo, 1);
    ml->addWidget(row);

    QScrollArea* hub_settings_scroll = new QScrollArea(minecraft_library_panel);
    hub_settings_scroll->setWidgetResizable(true);
    hub_settings_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hub_settings_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    hub_settings_scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    hub_settings_scroll->setFrameShape(QFrame::StyledPanel);
    hub_settings_scroll->setMinimumHeight(260);
    hub_settings_scroll->setMaximumHeight(520);

    minecraft_hub_preview_holder = new QWidget();
    QVBoxLayout* holder_lay = new QVBoxLayout(minecraft_hub_preview_holder);
    holder_lay->setContentsMargins(4, 4, 4, 4);
    holder_lay->setSizeConstraint(QLayout::SetMinAndMaxSize);
    hub_settings_scroll->setWidget(minecraft_hub_preview_holder);
    ml->addWidget(hub_settings_scroll);

    rebuildMinecraftHubPreviewEffect();

    QPushButton* add_mc = new QPushButton(tr("Add Minecraft layer"), minecraft_library_panel);
    add_mc->setToolTip(tr("Add this configured layer to the effect stack."));
    connect(add_mc, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_minecraft_library_add_clicked);
    ml->addWidget(add_mc);

    connect(minecraft_library_layer_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_minecraft_library_layer_combo_changed);

    minecraft_library_panel->setVisible(true);
    minecraft_library_panel->updateGeometry();

    if(effect_controls_widget)
    {
        effect_controls_widget->setVisible(false);
    }

    QTimer::singleShot(0, this, [this]() {
        if(!minecraft_library_panel || !minecraft_library_panel->isVisible())
        {
            return;
        }
        QWidget* detail = minecraft_library_panel->parentWidget();
        if(!detail)
        {
            return;
        }
        QWidget* viewport = detail->parentWidget();
        if(!viewport)
        {
            return;
        }
        if(QScrollArea* sa = qobject_cast<QScrollArea*>(viewport->parentWidget()))
        {
            sa->ensureWidgetVisible(minecraft_library_panel, 0, 32);
        }
    });

    UpdateEffectStackRowSelectorVisibility();
    UpdateAudioPanelVisibility();
}

void OpenRGB3DSpatialTab::UpdateEffectStackRowSelectorVisibility()
{
    if(!effect_combo || !effect_config_group)
    {
        return;
    }
    const bool show_effect_row = effect_config_group->isVisible();
    if(effect_stack_row_label)
    {
        effect_stack_row_label->setVisible(show_effect_row);
    }
    effect_combo->setVisible(show_effect_row);
}

void OpenRGB3DSpatialTab::rebuildMinecraftHubPreviewEffect()
{
    if(!minecraft_library_hub_active || !minecraft_library_layer_combo || !minecraft_hub_preview_holder)
    {
        return;
    }

    QVBoxLayout* hl = qobject_cast<QVBoxLayout*>(minecraft_hub_preview_holder->layout());
    if(!hl)
    {
        return;
    }

    while(QLayoutItem* item = hl->takeAt(0))
    {
        if(QWidget* w = item->widget())
        {
            w->setVisible(false);
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }

    if(minecraft_hub_preview_effect)
    {
        disconnect(minecraft_hub_preview_effect, nullptr, this, nullptr);
        if(start_effect_button)
        {
            disconnect(start_effect_button, nullptr, this, nullptr);
        }
        if(stop_effect_button)
        {
            disconnect(stop_effect_button, nullptr, this, nullptr);
        }
        minecraft_hub_preview_effect = nullptr;
        current_effect_ui = nullptr;
        start_effect_button = nullptr;
        stop_effect_button = nullptr;
    }

    const QString class_name = minecraft_library_layer_combo->currentData().toString();
    if(class_name.isEmpty())
    {
        return;
    }

    SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(class_name.toStdString());
    if(!effect)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Minecraft hub: failed to create effect: %s", class_name.toStdString().c_str());
        return;
    }

    effect->setParent(minecraft_hub_preview_holder);
    effect->MountSettingsUi(minecraft_hub_preview_holder, SpatialEffectSettingsLayout::FullWithTransport);

    minecraft_hub_preview_effect = effect;
    current_effect_ui = effect;

    if(QPushButton* sb = effect->GetStartButton())
    {
        start_effect_button = sb;
        connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
    }
    if(QPushButton* stb = effect->GetStopButton())
    {
        stop_effect_button = stb;
        connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
    }

    connect(effect, &SpatialEffect3D::ParametersChanged, this, [this]() {
        RefreshEffectDisplay();
    });

    effect->adjustSize();
    minecraft_hub_preview_holder->adjustSize();

    if(minecraft_library_panel)
    {
        minecraft_library_panel->updateGeometry();
    }
}

void OpenRGB3DSpatialTab::on_minecraft_library_layer_combo_changed(int)
{
    rebuildMinecraftHubPreviewEffect();
}

void OpenRGB3DSpatialTab::on_minecraft_library_add_clicked()
{
    if(!minecraft_library_hub_active || !minecraft_library_layer_combo)
    {
        return;
    }
    const QString class_name = minecraft_library_layer_combo->currentData().toString();
    if(class_name.isEmpty())
    {
        return;
    }
    int zone_index = -1;
    if(effect_zone_combo && effect_zone_combo->currentIndex() >= 0)
    {
        zone_index = effect_zone_combo->currentData().toInt();
    }
    EffectRegistration3D info = EffectListManager3D::get()->GetEffectInfo(class_name.toStdString());
    const QString ui_name = info.ui_name.empty() ? minecraft_library_layer_combo->currentText()
                                                 : QString::fromStdString(info.ui_name);
    if(minecraft_hub_preview_effect)
    {
        nlohmann::json preset = minecraft_hub_preview_effect->SaveSettings();
        AddEffectInstanceToStack(class_name, ui_name, zone_index, BlendMode::NO_BLEND, &preset, true, true);
    }
    else
    {
        AddEffectInstanceToStack(class_name, ui_name, zone_index, BlendMode::NO_BLEND, nullptr, true, true);
    }
}

void OpenRGB3DSpatialTab::EnsureAudioContainerLayerOnStack()
{
    const std::string hub = AudioEffectLibrary::HubClassName();
    for(const std::unique_ptr<EffectInstance3D>& inst_ptr : effect_stack)
    {
        EffectInstance3D* inst = inst_ptr.get();
        if(inst && inst->effect_class_name == hub)
        {
            return;
        }
    }

    EffectRegistration3D info = EffectListManager3D::get()->GetEffectInfo(hub);
    const QString ui_name = info.ui_name.empty() ? QStringLiteral("Audio Effect")
                                                 : QString::fromStdString(info.ui_name);
    AddEffectInstanceToStack(QString::fromStdString(hub), ui_name);
}

void OpenRGB3DSpatialTab::ShowAudioHubConfigurator()
{
    if(!audio_library_panel)
    {
        return;
    }

    if(run_setup_tab_widget && run_setup_tab_widget->currentIndex() != 0)
    {
        run_setup_tab_widget->setCurrentIndex(0);
    }

    LoadStackEffectControls(nullptr);

    if(effect_config_group)
    {
        effect_config_group->setVisible(true);
    }
    if(effect_zone_combo)
    {
        effect_zone_combo->setEnabled(true);
    }
    if(effect_origin_combo)
    {
        effect_origin_combo->setEnabled(true);
    }
    if(origin_label)
    {
        origin_label->setVisible(true);
    }
    if(effect_bounds_label)
    {
        effect_bounds_label->setVisible(true);
    }
    if(effect_bounds_combo)
    {
        effect_bounds_combo->setVisible(true);
        effect_bounds_combo->setEnabled(true);
    }

    QVBoxLayout* ml = qobject_cast<QVBoxLayout*>(audio_library_panel->layout());
    if(!ml)
    {
        return;
    }

    audio_library_hub_active = true;

    QLabel* intro = new QLabel(tr(
        "Pick an audio-reactive effect, set its options below, choose the frequency band and output zone, then use "
        "\"Add To Stack\" (or the button here) to append a range. That also adds an \"Audio Effect\" layer if the stack "
        "does not have one yet. Start listening, then run the stack."));
    intro->setWordWrap(true);
    ml->addWidget(intro);

    QWidget* layer_row = new QWidget(audio_library_panel);
    QHBoxLayout* layer_l = new QHBoxLayout(layer_row);
    layer_l->setContentsMargins(0, 0, 0, 0);
    layer_l->addWidget(new QLabel(tr("Effect type:"), layer_row));
    audio_hub_layer_combo = new QComboBox(layer_row);
    audio_hub_layer_combo->setMinimumWidth(220);
    PopulateFreqEffectCombo(audio_hub_layer_combo);
    layer_l->addWidget(audio_hub_layer_combo, 1);
    ml->addWidget(layer_row);

    QGroupBox* band_group = new QGroupBox(tr("Frequency band (Hz)"), audio_library_panel);
    QVBoxLayout* band_l = new QVBoxLayout(band_group);
    QHBoxLayout* low_row = new QHBoxLayout();
    low_row->addWidget(new QLabel(tr("Low:")));
    audio_hub_low_slider = new QSlider(Qt::Horizontal);
    audio_hub_low_slider->setRange(20, 20000);
    audio_hub_low_slider->setValue(20);
    audio_hub_low_label = new QLabel(QStringLiteral("20 Hz"));
    audio_hub_low_label->setMinimumWidth(56);
    connect(audio_hub_low_slider, &QSlider::valueChanged, this, [this](int v) {
        if(audio_hub_high_slider && v > audio_hub_high_slider->value())
        {
            QSignalBlocker b(*audio_hub_high_slider);
            audio_hub_high_slider->setValue(v);
            if(audio_hub_high_label)
            {
                audio_hub_high_label->setText(QStringLiteral("%1 Hz").arg(v));
            }
        }
        if(audio_hub_low_label)
        {
            audio_hub_low_label->setText(QStringLiteral("%1 Hz").arg(v));
        }
    });
    low_row->addWidget(audio_hub_low_slider, 1);
    low_row->addWidget(audio_hub_low_label);
    band_l->addLayout(low_row);

    QHBoxLayout* high_row = new QHBoxLayout();
    high_row->addWidget(new QLabel(tr("High:")));
    audio_hub_high_slider = new QSlider(Qt::Horizontal);
    audio_hub_high_slider->setRange(20, 20000);
    audio_hub_high_slider->setValue(200);
    audio_hub_high_label = new QLabel(QStringLiteral("200 Hz"));
    audio_hub_high_label->setMinimumWidth(56);
    connect(audio_hub_high_slider, &QSlider::valueChanged, this, [this](int v) {
        if(audio_hub_low_slider && v < audio_hub_low_slider->value())
        {
            QSignalBlocker b(*audio_hub_low_slider);
            audio_hub_low_slider->setValue(v);
            if(audio_hub_low_label)
            {
                audio_hub_low_label->setText(QStringLiteral("%1 Hz").arg(v));
            }
        }
        if(audio_hub_high_label)
        {
            audio_hub_high_label->setText(QStringLiteral("%1 Hz").arg(v));
        }
    });
    high_row->addWidget(audio_hub_high_slider, 1);
    high_row->addWidget(audio_hub_high_label);
    band_l->addLayout(high_row);
    ml->addWidget(band_group);

    QWidget* zone_row = new QWidget(audio_library_panel);
    QHBoxLayout* zl = new QHBoxLayout(zone_row);
    zl->setContentsMargins(0, 0, 0, 0);
    zl->addWidget(new QLabel(tr("Zone / target:"), zone_row));
    audio_hub_zone_combo = new QComboBox(zone_row);
    FillFrequencyRangeZoneCombo(audio_hub_zone_combo, QVariant(-1));
    zl->addWidget(audio_hub_zone_combo, 1);
    ml->addWidget(zone_row);

    QWidget* origin_row = new QWidget(audio_library_panel);
    QHBoxLayout* ol = new QHBoxLayout(origin_row);
    ol->setContentsMargins(0, 0, 0, 0);
    ol->addWidget(new QLabel(tr("Pattern origin:"), origin_row));
    audio_hub_origin_combo = new QComboBox(origin_row);
    FillFrequencyRangeOriginCombo(audio_hub_origin_combo, QVariant(-1));
    ol->addWidget(audio_hub_origin_combo, 1);
    ml->addWidget(origin_row);

    QScrollArea* hub_scroll = new QScrollArea(audio_library_panel);
    hub_scroll->setWidgetResizable(true);
    hub_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hub_scroll->setMinimumHeight(200);
    audio_hub_preview_holder = new QWidget();
    new QVBoxLayout(audio_hub_preview_holder);
    hub_scroll->setWidget(audio_hub_preview_holder);
    ml->addWidget(hub_scroll);

    rebuildAudioHubPreviewEffect();

    AttachFrequencyRangeEditorToHub(ml);

    QPushButton* add_range = new QPushButton(tr("Add audio range to stack"), audio_library_panel);
    add_range->setToolTip(tr("Appends this band to the stack ranges list and ensures an Audio Effect layer exists."));
    connect(add_range, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_hub_add_range_clicked);
    ml->addWidget(add_range);

    connect(audio_hub_layer_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_audio_hub_layer_combo_changed);

    audio_library_panel->setVisible(true);
    audio_library_panel->updateGeometry();

    if(effect_controls_widget)
    {
        effect_controls_widget->setVisible(false);
    }

    QTimer::singleShot(0, this, [this]() {
        if(!audio_library_panel || !audio_library_panel->isVisible())
        {
            return;
        }
        QWidget* detail = audio_library_panel->parentWidget();
        if(!detail)
        {
            return;
        }
        QWidget* viewport_w = detail->parentWidget();
        if(!viewport_w)
        {
            return;
        }
        if(QScrollArea* sa = qobject_cast<QScrollArea*>(viewport_w->parentWidget()))
        {
            sa->ensureWidgetVisible(audio_library_panel, 0, 32);
        }
    });

    UpdateEffectStackRowSelectorVisibility();
    UpdateAudioPanelVisibility();
}

void OpenRGB3DSpatialTab::rebuildAudioHubPreviewEffect()
{
    if(!audio_library_hub_active || !audio_hub_layer_combo || !audio_hub_preview_holder)
    {
        return;
    }

    QVBoxLayout* hl = qobject_cast<QVBoxLayout*>(audio_hub_preview_holder->layout());
    if(!hl)
    {
        return;
    }

    while(QLayoutItem* item = hl->takeAt(0))
    {
        if(QWidget* w = item->widget())
        {
            w->setVisible(false);
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }

    if(audio_hub_preview_effect)
    {
        disconnect(audio_hub_preview_effect, nullptr, this, nullptr);
        audio_hub_preview_effect = nullptr;
    }

    const QString class_name = audio_hub_layer_combo->currentData(kEffectRoleClassName).toString();
    if(class_name.isEmpty())
    {
        return;
    }

    EffectSettingsUiMount mount = createEffectSettingsUi(audio_hub_preview_holder,
                                                       hl,
                                                       class_name.toStdString(),
                                                       SpatialEffectSettingsLayout::CommonNoTransport);
    SpatialEffect3D* effect = mount.effect;
    if(!effect)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Audio hub: failed to create effect: %s", class_name.toStdString().c_str());
        return;
    }

    audio_hub_preview_effect = effect;
    connect(effect, &SpatialEffect3D::ParametersChanged, this, [this]() {
        RefreshEffectDisplay();
    });

    effect->adjustSize();
    audio_hub_preview_holder->adjustSize();
    if(audio_library_panel)
    {
        audio_library_panel->updateGeometry();
    }
}

void OpenRGB3DSpatialTab::on_audio_hub_layer_combo_changed(int)
{
    rebuildAudioHubPreviewEffect();
}

void OpenRGB3DSpatialTab::on_audio_hub_add_range_clicked()
{
    if(!audio_library_hub_active || !audio_hub_layer_combo)
    {
        return;
    }
    const QString class_name = audio_hub_layer_combo->currentData(kEffectRoleClassName).toString();
    if(class_name.isEmpty())
    {
        QMessageBox::information(this, tr("Audio Effect"),
                                 tr("Choose an audio effect type (not \"None\") before adding a range."));
        return;
    }

    EnsureAudioContainerLayerOnStack();

    int zone_index = -1;
    if(audio_hub_zone_combo && audio_hub_zone_combo->currentIndex() >= 0)
    {
        zone_index = audio_hub_zone_combo->currentData().toInt();
    }

    int origin_ref_index = -1;
    if(audio_hub_origin_combo && audio_hub_origin_combo->currentIndex() >= 0)
    {
        origin_ref_index = audio_hub_origin_combo->currentData().toInt();
    }

    int low_hz = 20;
    int high_hz = 200;
    if(audio_hub_low_slider)
    {
        low_hz = audio_hub_low_slider->value();
    }
    if(audio_hub_high_slider)
    {
        high_hz = audio_hub_high_slider->value();
    }
    if(high_hz < low_hz)
    {
        std::swap(low_hz, high_hz);
    }

    std::unique_ptr<FrequencyRangeEffect3D> range = std::make_unique<FrequencyRangeEffect3D>();
    range->id = next_freq_range_id++;
    range->low_hz = (float)low_hz;
    range->high_hz = (float)high_hz;
    range->zone_index = zone_index;
    range->origin_ref_index = origin_ref_index;
    range->effect_class_name = class_name.toStdString();
    EffectRegistration3D info = EffectListManager3D::get()->GetEffectInfo(range->effect_class_name);
    QString base_name = info.ui_name.empty() ? class_name : QString::fromStdString(info.ui_name);
    range->name = QStringLiteral("%1 %2").arg(base_name).arg((int)frequency_ranges.size() + 1).toStdString();

    if(audio_hub_preview_effect)
    {
        range->effect_settings = audio_hub_preview_effect->SaveSettings();
    }
    else
    {
        range->effect_settings = nlohmann::json::object();
    }
    range->effect_instance.reset();

    frequency_ranges.push_back(std::move(range));
    UpdateFrequencyRangesList();
    SaveFrequencyRanges();

    if(freq_ranges_list)
    {
        const QSignalBlocker b(freq_ranges_list);
        freq_ranges_list->setCurrentRow((int)frequency_ranges.size() - 1);
    }
    if(!frequency_ranges.empty())
    {
        on_freq_range_selected((int)frequency_ranges.size() - 1);
    }

    for(int i = 0; i < (int)effect_stack.size(); i++)
    {
        if(effect_stack[i] && effect_stack[i]->effect_class_name == AudioEffectLibrary::HubClassName())
        {
            if(effect_stack_list)
            {
                const QSignalBlocker sb(effect_stack_list);
                effect_stack_list->setCurrentRow(i);
            }
            break;
        }
    }

    UpdateAudioPanelVisibility();
    UpdateEffectCombo();
    SaveEffectStack();
}

void OpenRGB3DSpatialTab::SetupStackPresetUI()
{
    if(!effect_controls_widget || !effect_controls_layout)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect controls widget or layout is null!");
        return;
    }

    QLabel* info_label = new QLabel(
        "This is a saved stack preset with pre-configured settings.\n\n"
        "Click Start to load and run all effects in this preset.\n\n"
        "To edit this preset, open the Effect Stack panel, load it,\n"
        "modify the effects, and save with the same name."
    );
    info_label->setWordWrap(true);
    info_label->setFrameShape(QFrame::StyledPanel);
    info_label->setFrameShadow(QFrame::Raised);
    info_label->setLineWidth(1);
    info_label->setContentsMargins(6, 6, 6, 6);
    effect_controls_layout->addWidget(info_label);

    QWidget* button_container = new QWidget();
    QHBoxLayout* button_layout = new QHBoxLayout(button_container);
    button_layout->setContentsMargins(0, 6, 0, 0);

    start_effect_button = new QPushButton("Start Effect");
    stop_effect_button = new QPushButton("Stop Effect");
    stop_effect_button->setEnabled(false);

    button_layout->addWidget(start_effect_button);
    button_layout->addWidget(stop_effect_button);
    button_layout->addStretch();

    effect_controls_layout->addWidget(button_container);

    connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
    connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);

    effect_controls_widget->setVisible(true);
    effect_controls_widget->updateGeometry();
    effect_controls_widget->update();
}

void OpenRGB3DSpatialTab::ClearCustomEffectUI()
{
    if(!effect_controls_layout)
    {
        return;
    }

    if(effect_timer && effect_timer->isActive())
    {
        effect_timer->stop();
    }
    effect_running = false;

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
    while((item = effect_controls_layout->takeAt(0)) != nullptr)
    {
        if(QWidget* w = item->widget())
        {
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }

    if(effect_controls_widget)
    {
        effect_controls_widget->setVisible(false);
    }
}

void OpenRGB3DSpatialTab::on_grid_dimensions_changed()
{
    if(grid_x_spin) custom_grid_x = grid_x_spin->value();
    if(grid_y_spin) custom_grid_y = grid_y_spin->value();
    if(grid_z_spin) custom_grid_z = grid_z_spin->value();

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        RegenerateLEDPositions(controller_transforms[i].get());
    }

    if(viewport)
    {
        viewport->SetGridDimensions(custom_grid_x, custom_grid_y, custom_grid_z);
        viewport->update();
    }
}

void OpenRGB3DSpatialTab::on_grid_snap_toggled(bool enabled)
{
    if(viewport)
    {
        viewport->SetGridSnapEnabled(enabled);
    }
}

void OpenRGB3DSpatialTab::UpdateSelectionInfo()
{
    if(!viewport || !selection_info_label) return;

    const std::vector<int>& selected = viewport->GetSelectedControllers();

    QFont info_font = selection_info_label->font();
    info_font.setBold(true);

    if(selected.empty())
    {
        selection_info_label->setText("No selection");
        info_font.setItalic(true);
    }
    else if(selected.size() == 1)
    {
        selection_info_label->setText(QString("Selected: 1 controller"));
        info_font.setItalic(false);
    }
    else
    {
        selection_info_label->setText(QString("Selected: %1 controllers").arg(selected.size()));
        info_font.setItalic(false);
    }

    selection_info_label->setFont(info_font);
}

void OpenRGB3DSpatialTab::on_effect_changed(int index)
{
    if(!effect_combo || !effect_stack_list)
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

    QString class_name = effect_combo->itemData(index, kEffectRoleClassName).toString();
    
    if(class_name == QLatin1String("ScreenMirror"))
    {
        if(origin_label) origin_label->setVisible(false);
        if(effect_origin_combo) effect_origin_combo->setVisible(false);
    }
    else
    {
        if(origin_label) origin_label->setVisible(true);
        if(effect_origin_combo) effect_origin_combo->setVisible(true);
    }

    if(effect_stack_list->currentRow() != index)
    {
        effect_stack_list->setCurrentRow(index);
    }
}

void OpenRGB3DSpatialTab::UpdateEffectOriginCombo()
{
    if(!effect_origin_combo) return;

    QVariant desired_selection = effect_origin_combo->currentData();
    bool restore_signals = effect_origin_combo->blockSignals(true);
    effect_origin_combo->clear();

    effect_origin_combo->addItem(tr("Mapped lights center (recommended)"), QVariant(-4));
    effect_origin_combo->addItem(tr("Room box center"), QVariant(-1));
    effect_origin_combo->addItem(tr("Target zone center"), QVariant(-2));
    effect_origin_combo->addItem(tr("No anchor (world 0,0,0)"), QVariant(-3));

    for(size_t i = 0; i < reference_points.size(); i++)
    {
        VirtualReferencePoint3D* ref_point = reference_points[i].get();
        if(!ref_point) continue;

        QString name = QString::fromStdString(ref_point->GetName());
        QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
        QString display = QString("%1 (%2)").arg(name, type);
        effect_origin_combo->addItem(display, QVariant((int)i));
    }

    int restore_index = effect_origin_combo->findData(desired_selection);
    if(restore_index < 0)
    {
        restore_index = effect_origin_combo->findData(QVariant(-4));
    }
    if(restore_index < 0)
    {
        restore_index = 0;
    }
    effect_origin_combo->setCurrentIndex(restore_index);
    effect_origin_combo->blockSignals(restore_signals);
}

void OpenRGB3DSpatialTab::UpdateEffectCombo()
{
    if(!effect_combo)
    {
        return;
    }

    QSignalBlocker blocker(effect_combo);
    effect_combo->clear();

    if(effect_stack.empty())
    {
        effect_combo->addItem("No Active Effects");
        effect_combo->setEnabled(false);
        return;
    }

    effect_combo->setEnabled(true);

    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        EffectInstance3D* instance = effect_stack[i].get();
        if(!instance)
        {
            continue;
        }

        QString label = QString("#%1 • %2")
                            .arg(i + 1)
                            .arg(QString::fromStdString(instance->GetDisplayName()));
        effect_combo->addItem(label);
        int row = effect_combo->count() - 1;
        effect_combo->setItemData(row, QString::fromStdString(instance->effect_class_name),
                                  kEffectRoleClassName);
        effect_combo->setItemData(row, instance->id, kEffectRoleInstanceId);
    }

    int desired_index = effect_stack_list ? effect_stack_list->currentRow() : 0;
    if(desired_index < 0)
    {
        desired_index = 0;
    }
    if(desired_index >= effect_combo->count())
    {
        desired_index = effect_combo->count() - 1;
    }

    effect_combo->setCurrentIndex(desired_index);
}

void OpenRGB3DSpatialTab::on_effect_zone_changed(int index)
{
    if(!effect_zone_combo)
    {
        return;
    }

    if(!effect_stack_list)
    {
        return;
    }

    int current_row = effect_stack_list->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        return;
    }
    if(index < 0 || index >= effect_zone_combo->count())
    {
        return;
    }

    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->zone_index = effect_zone_combo->itemData(index).toInt();
    UpdateEffectStackList();
    if(effect_stack_list)
    {
        effect_stack_list->setCurrentRow(current_row);
    }
    if(stack_effect_zone_combo)
    {
        int zone_combo_index = stack_effect_zone_combo->findData(instance->zone_index);
        stack_effect_zone_combo->blockSignals(true);
        if(zone_combo_index >= 0)
        {
            stack_effect_zone_combo->setCurrentIndex(zone_combo_index);
        }
        stack_effect_zone_combo->blockSignals(false);
    }
    SaveEffectStack();
}

void OpenRGB3DSpatialTab::on_effect_origin_changed(int index)
{
    if(!effect_origin_combo)
    {
        return;
    }
    if(index < 0 || index >= effect_origin_combo->count())
    {
        return;
    }
    int ref_point_idx = effect_origin_combo->itemData(index).toInt();

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

void OpenRGB3DSpatialTab::on_effect_bounds_changed(int index)
{
    if(!effect_bounds_combo)
    {
        return;
    }
    if(index < 0 || index >= effect_bounds_combo->count())
    {
        return;
    }
    int mode = effect_bounds_combo->itemData(index).toInt();

    if(current_effect_ui)
    {
        current_effect_ui->SetEffectBoundsMode(mode);
    }

    if(effect_stack_list)
    {
        int current_row = effect_stack_list->currentRow();
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
    catch(const std::exception&) {}
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

void OpenRGB3DSpatialTab::ApplyPositionComponent(int axis, double value)
{
    if(!controller_list || !reference_points_list || !viewport) return;
    int transform_index = ControllerListRowToTransformIndex(controller_list->currentRow());
    if(transform_index >= 0 && transform_index < (int)controller_transforms.size())
    {
        ControllerTransform* transform = controller_transforms[transform_index].get();
        if(transform)
        {
            if(axis == 0) transform->transform.position.x = value;
            else if(axis == 1) transform->transform.position.y = value;
            else transform->transform.position.z = value;
            SyncControllerLinkedReferencePoint(transform_index);
            ControllerLayout3D::MarkWorldPositionsDirty(transform);
        }
        viewport->NotifyControllerTransformChanged();
        if(effect_running) RenderEffectStack();
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
            emit GridLayoutChanged();
        }
        return;
    }
    int ref_idx = reference_points_list->currentRow();
    if(ref_idx >= 0 && ref_idx < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_point = reference_points[ref_idx].get();
        Vector3D pos = ref_point->GetPosition();
        if(axis == 0) pos.x = value;
        else if(axis == 1) pos.y = value;
        else pos.z = value;
        ref_point->SetPosition(pos);
        viewport->update();
    }
}

void OpenRGB3DSpatialTab::ApplyRotationComponent(int axis, double value)
{
    if(!controller_list || !reference_points_list || !viewport) return;
    int transform_index = ControllerListRowToTransformIndex(controller_list->currentRow());
    if(transform_index >= 0 && transform_index < (int)controller_transforms.size())
    {
        ControllerTransform* transform = controller_transforms[transform_index].get();
        if(transform)
        {
            if(axis == 0) transform->transform.rotation.x = value;
            else if(axis == 1) transform->transform.rotation.y = value;
            else transform->transform.rotation.z = value;
            ControllerLayout3D::MarkWorldPositionsDirty(transform);
        }
        viewport->NotifyControllerTransformChanged();
        if(effect_running) RenderEffectStack();
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
            emit GridLayoutChanged();
        }
        return;
    }
    int ref_idx = reference_points_list->currentRow();
    if(ref_idx >= 0 && ref_idx < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_point = reference_points[ref_idx].get();
        Rotation3D rot = ref_point->GetRotation();
        if(axis == 0) rot.x = value;
        else if(axis == 1) rot.y = value;
        else rot.z = value;
        ref_point->SetRotation(rot);
        viewport->update();
    }
}

void OpenRGB3DSpatialTab::RemoveWidgetFromParentLayout(QWidget* w)
{
    if(!w || !w->parentWidget()) return;
    QLayout* layout = w->parentWidget()->layout();
    if(layout) layout->removeWidget(w);
    w->hide();
}
