// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "ControllerLayout3D.h"
#include "LogManager.h"
#include "CustomControllerDialog.h"
#include "VirtualController3D.h"
#include "DisplayPlaneManager.h"
#include "ScreenCaptureManager.h"
#include "Effects3D/ScreenMirror3D/ScreenMirror3D.h"
#include "GridSpaceUtils.h"
#include <QStackedWidget>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>
#include <fstream>
#include <algorithm>
#include <QList>
#include <QSignalBlocker>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QSplitter>
#include <QGroupBox>
#include <QScrollArea>
#include <QAbstractItemView>
#include <QTabWidget>
#include <functional>

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
#include <nlohmann/json.hpp>
#include "Audio/AudioInputManager.h"
#include "Zone3D.h"

OpenRGB3DSpatialTab::OpenRGB3DSpatialTab(ResourceManagerInterface* rm, QWidget *parent) :
    QWidget(parent),
    resource_manager(rm),
    first_load(true)
{

    stack_settings_updating = false;
    effect_config_group = nullptr;
    effect_controls_widget = nullptr;
    effect_controls_layout = nullptr;
    current_effect_ui = nullptr;
    start_effect_button = nullptr;
    stop_effect_button = nullptr;
    stack_blend_container = nullptr;
    stack_effect_blend_combo = nullptr;
    effect_zone_label = nullptr;
    origin_label = nullptr;
    effect_origin_combo = nullptr;
    effect_zone_combo = nullptr;
    effect_category_combo = nullptr;
    effect_library_list = nullptr;
    effect_library_add_button = nullptr;
    effect_combo = nullptr;
    effect_type_combo = nullptr;

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
    room_grid_brightness_slider = nullptr;
    room_grid_brightness_label = nullptr;
    room_grid_point_size_slider = nullptr;
    room_grid_point_size_label = nullptr;
    room_grid_step_slider = nullptr;
    room_grid_step_label = nullptr;
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

    auto_load_timer = new QTimer(this);
    auto_load_timer->setSingleShot(true);
    connect(auto_load_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::TryAutoLoadLayout);
    auto_load_timer->start(2000);

    effect_timer = new QTimer(this);
    effect_timer->setTimerType(Qt::PreciseTimer);
    connect(effect_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::on_effect_timer_timeout);
}

OpenRGB3DSpatialTab::~OpenRGB3DSpatialTab()
{
    if(viewport)
    {
        float dist, yaw, pitch, tx, ty, tz;
        viewport->GetCamera(dist, yaw, pitch, tx, ty, tz);
        try
        {
            nlohmann::json settings = GetPluginSettings();
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
            SetPluginSettingsNoSave(settings);
        }
        catch(const std::exception&){}
    }

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

    QTabWidget* main_tabs = new QTabWidget();
    root_layout->addWidget(main_tabs);

    QWidget* setup_tab = new QWidget();
    QHBoxLayout* main_layout = new QHBoxLayout(setup_tab);
    main_layout->setSpacing(6);
    main_layout->setContentsMargins(4, 4, 4, 4);

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

    auto create_spacing_row = [](const QString& label_text, QDoubleSpinBox*& spin, bool enabled = true) -> QHBoxLayout*
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

    available_tab->setLayout(available_layout);
    left_tabs->addTab(available_tab, "Available Controllers");

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
    main_layout->addWidget(left_scroll, 1);

    QVBoxLayout* middle_panel = new QVBoxLayout();
    middle_panel->setSpacing(3);
    middle_panel->setContentsMargins(0, 0, 0, 0);

    QLabel* controls_label = new QLabel("Camera: Right mouse = Rotate | Left drag = Pan | Scroll = Zoom | Left click = Select/Move objects");
    controls_label->setWordWrap(true);
    controls_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    middle_panel->addWidget(controls_label);

    viewport = new LEDViewport3D();
    viewport->SetControllerTransforms(&controller_transforms);
    viewport->SetGridDimensions(custom_grid_x, custom_grid_y, custom_grid_z);
    viewport->SetGridSnapEnabled(false);
    viewport->SetReferencePoints(&reference_points);
    viewport->SetDisplayPlanes(&display_planes);
    viewport->SetGridScaleMM(grid_scale_mm);
    viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);

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

    grid_gl->addWidget(new QLabel("Layout size (X × Y × Z):"), 0, 0, 1, 2);
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
    grid_gl->addWidget(grid_snap_checkbox, 1, 3, 1, 2);

    selection_info_label = new QLabel("No selection");
    selection_info_label->setAlignment(Qt::AlignRight);
    QFont selection_font = selection_info_label->font();
    selection_font.setBold(true);
    selection_info_label->setFont(selection_font);
    grid_gl->addWidget(new QLabel("Selection:"), 1, 5);
    grid_gl->addWidget(selection_info_label, 1, 6, 1, 2);

    QLabel* grid_scale_help = new QLabel("Default size for new LED layouts; scale is mm per grid unit.");
    grid_scale_help->setForegroundRole(QPalette::PlaceholderText);
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
            ScreenMirror3D* sm = qobject_cast<ScreenMirror3D*>(current_effect_ui);
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

    auto add_room_dim_spin = [this](QGridLayout* lo, int row, int col, const QString& label, QDoubleSpinBox*& spin, double value, const QString& tooltip)
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
    room_help->setForegroundRole(QPalette::PlaceholderText);
    room_help->setWordWrap(true);
    room_gl->addWidget(room_help, 2, 0, 1, 6);

    grid_tab_main->addWidget(room_group);

    QGroupBox* overlay_group = new QGroupBox("3D view overlay");
    QGridLayout* overlay_gl = new QGridLayout(overlay_group);
    overlay_gl->setSpacing(4);

    room_grid_overlay_checkbox = new QCheckBox("Show overlay in 3D view");
    room_grid_overlay_checkbox->setToolTip("Draw a dim grid of points in the room so you see the space. Real LEDs stand out.");
    overlay_gl->addWidget(room_grid_overlay_checkbox, 0, 0, 1, 4);

    auto add_grid_slider_row = [this](QGridLayout* lo, int row, int label_col, int slider_col, int value_col,
        const QString& labelText, int minVal, int maxVal, int value, const QString& initialValueText, const QString& tooltip,
        QSlider*& slider, QLabel*& valueLabel, std::function<QString(int)> valueFormatter, std::function<void(int)> onValueChanged)
    {
        lo->addWidget(new QLabel(labelText), row, label_col);
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(minVal, maxVal);
        slider->setValue(value);
        slider->setToolTip(tooltip);
        valueLabel = new QLabel(initialValueText);
        lo->addWidget(slider, row, slider_col);
        lo->addWidget(valueLabel, row, value_col);
        connect(slider, &QSlider::valueChanged, [this, valueLabel, valueFormatter, onValueChanged](int v) {
            if(valueLabel) valueLabel->setText(valueFormatter(v));
            onValueChanged(v);
        });
    };

    add_grid_slider_row(overlay_gl, 1, 0, 1, 2, "Brightness:", 0, 100, 35, "35%", "Lower = real LEDs stand out more",
        room_grid_brightness_slider, room_grid_brightness_label, [](int v) { return QString("%1%").arg(v); },
        [this](int v) { if(viewport) viewport->SetRoomGridBrightness((float)v / 100.0f); });
    add_grid_slider_row(overlay_gl, 1, 3, 4, 5, "Point size:", 1, 12, 3, "3", "Larger = easier to see points",
        room_grid_point_size_slider, room_grid_point_size_label, [](int v) { return QString::number(v); },
        [this](int v) { if(viewport) viewport->SetRoomGridPointSize((float)v); });
    add_grid_slider_row(overlay_gl, 2, 0, 1, 2, "Step:", 1, 24, 4, "4", "Grid units between points (1=dense, 24=sparse). Step × grid scale (mm/unit) = spacing in mm.",
        room_grid_step_slider, room_grid_step_label, [](int v) { return QString::number(v); },
        [this](int v) { if(viewport) viewport->SetRoomGridStep(v); });

    grid_tab_main->addWidget(overlay_group);

    connect(room_grid_overlay_checkbox, &QCheckBox::toggled, [this](bool checked) {
        if(viewport) viewport->SetShowRoomGridOverlay(checked);
    });

    try
    {
        nlohmann::json settings = GetPluginSettings();
        if(settings.contains("RoomGrid"))
        {
            const nlohmann::json& rg = settings["RoomGrid"];
            bool show = rg.value("Show", false);
            int bright = (int)(rg.value("Brightness", 0.35) * 100.0);
            int size = (int)rg.value("PointSize", 3.0);
            int step_val = (int)rg.value("Step", 4);
            bright = std::max(0, std::min(100, bright));
            size = std::max(1, std::min(12, size));
            step_val = std::max(1, std::min(24, step_val));
            if(room_grid_overlay_checkbox) room_grid_overlay_checkbox->setChecked(show);
            if(room_grid_brightness_slider) room_grid_brightness_slider->setValue(bright);
            if(room_grid_point_size_slider) room_grid_point_size_slider->setValue(size);
            if(room_grid_step_slider) room_grid_step_slider->setValue(step_val);
        }
    }
    catch(const std::exception&) {}

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
    QLabel* transform_help = new QLabel("Select an item in the list. Position is in mm (same as Room size in Grid Settings). Rotation in degrees.");
    transform_help->setWordWrap(true);
    transform_help->setForegroundRole(QPalette::PlaceholderText);
    transform_tab_v->addWidget(transform_help);

    QHBoxLayout* transform_layout = new QHBoxLayout();
    transform_layout->setSpacing(6);
    transform_layout->setContentsMargins(2, 2, 2, 2);

    QGridLayout* position_layout = new QGridLayout();
    position_layout->setSpacing(3);
    position_layout->setContentsMargins(0, 0, 0, 0);

    auto add_position_row = [this, position_layout](int row, const QString& label, QSlider*& sl, QDoubleSpinBox*& sp, int axis, bool clamp_non_negative)
    {
        position_layout->addWidget(new QLabel(label), row, 0);
        sl = new QSlider(Qt::Horizontal);
        sl->setRange(-50000, 50000);
        sl->setValue(0);
        sl->setToolTip("Position in mm (same unit as Room size in Grid Settings).");
        sp = new QDoubleSpinBox();
        sp->setRange(-50000.0, 50000.0);
        sp->setDecimals(0);
        sp->setSingleStep(10.0);
        sp->setMaximumWidth(80);
        sp->setSuffix(" mm");
        sp->setToolTip("Position in mm (same unit as Room size in Grid Settings).");
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

    add_position_row(0, "Position X (mm):", pos_x_slider, pos_x_spin, 0, false);
    add_position_row(1, "Position Y (mm):", pos_y_slider, pos_y_spin, 1, true);
    add_position_row(2, "Position Z (mm):", pos_z_slider, pos_z_spin, 2, false);

    position_layout->setColumnStretch(1, 1);

    QGridLayout* rotation_layout = new QGridLayout();
    rotation_layout->setSpacing(3);
    rotation_layout->setContentsMargins(0, 0, 0, 0);

    auto add_rotation_row = [this, rotation_layout](int row, const QString& label, QSlider*& sl, QDoubleSpinBox*& sp, int axis)
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

    add_rotation_row(0, "Rotation X (°):", rot_x_slider, rot_x_spin, 0);
    add_rotation_row(1, "Rotation Y (°):", rot_y_slider, rot_y_spin, 1);
    add_rotation_row(2, "Rotation Z (°):", rot_z_slider, rot_z_spin, 2);

    rotation_layout->setColumnStretch(1, 1);

    transform_layout->addLayout(position_layout, 1);
    transform_layout->addLayout(rotation_layout, 1);

    transform_tab_v->addLayout(transform_layout);

    auto wrap_tab_in_scroll = [](QWidget* content) {
        QScrollArea* sa = new QScrollArea();
        sa->setWidget(content);
        sa->setWidgetResizable(true);
        sa->setFrameShape(QFrame::NoFrame);
        sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        return sa;
    };

    settings_tabs->addTab(wrap_tab_in_scroll(transform_tab), "Position & Rotation");
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
    empty_label->setForegroundRole(QPalette::PlaceholderText);
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
    custom_subtitle->setStyleSheet("color: gray; font-size: small;");
    custom_subtitle->setContentsMargins(0, 0, 0, 6);
    custom_layout->addWidget(custom_subtitle);

    custom_controllers_list = new QListWidget();
    custom_controllers_list->setMinimumHeight(150);
    custom_controllers_list->setToolTip("Select a custom controller to edit or export");
    custom_layout->addWidget(custom_controllers_list);

    custom_controllers_empty_label = new QLabel("No custom controllers yet. Create one or import from file.");
    custom_controllers_empty_label->setWordWrap(true);
    custom_controllers_empty_label->setStyleSheet("color: gray; font-style: italic;");
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

    QPushButton* export_button = new QPushButton("Export");
    export_button->setToolTip("Export selected custom controller to file");
    connect(export_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_export_custom_controller_clicked);
    custom_io_layout->addWidget(export_button);

    QPushButton* edit_button = new QPushButton("Edit");
    edit_button->setToolTip("Edit selected custom controller");
    connect(edit_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_edit_custom_controller_clicked);
    custom_io_layout->addWidget(edit_button);

    QPushButton* delete_custom_button = new QPushButton("Delete");
    delete_custom_button->setToolTip("Remove selected custom controller from library (remove from 3D scene first if in use)");
    connect(delete_custom_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_delete_custom_controller_clicked);
    custom_io_layout->addWidget(delete_custom_button);

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
    ref_subtitle->setStyleSheet("color: gray; font-size: small;");
    ref_subtitle->setContentsMargins(0, 0, 0, 6);
    ref_points_layout->addWidget(ref_subtitle);

    reference_points_list = new QListWidget();
    reference_points_list->setMinimumHeight(150);
    connect(reference_points_list, &QListWidget::currentRowChanged, this, &OpenRGB3DSpatialTab::on_ref_point_selected);
    ref_points_layout->addWidget(reference_points_list);

    ref_points_empty_label = new QLabel("No reference points yet. Click Add Reference Point to create one.");
    ref_points_empty_label->setWordWrap(true);
    ref_points_empty_label->setStyleSheet("color: gray; font-style: italic;");
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
    QString default_hex = QString("#%1%2%3")
        .arg(default_red, 2, 16, QChar('0'))
        .arg(default_green, 2, 16, QChar('0'))
        .arg(default_blue, 2, 16, QChar('0'))
        .toUpper();
    ref_point_color_button->setStyleSheet(QString("background-color: %1").arg(default_hex));
    connect(ref_point_color_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_ref_point_color_clicked);
    color_layout->addWidget(ref_point_color_button);
    color_layout->addStretch();
    ref_points_layout->addLayout(color_layout);

    QLabel* help_label = new QLabel("Select a reference point to move it with the Position & Rotation controls and 3D gizmo.");
    help_label->setForegroundRole(QPalette::PlaceholderText);
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
    display_subtitle->setStyleSheet("color: gray; font-size: small;");
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
    display_planes_empty_label->setStyleSheet("color: gray; font-style: italic;");
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

    QLabel* monitor_filter_label = new QLabel(tr("Filter:"));
    monitor_filter_label->setStyleSheet("color: gray; font-size: small;");
    plane_form->addWidget(monitor_filter_label, plane_row, 0);
    display_plane_monitor_brand_filter = new QComboBox();
    display_plane_monitor_brand_filter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    display_plane_monitor_brand_filter->addItem(tr("All brands"), QString());
    connect(display_plane_monitor_brand_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_monitor_filter_or_sort_changed);
    plane_form->addWidget(display_plane_monitor_brand_filter, plane_row, 1);

    QLabel* monitor_sort_label = new QLabel(tr("Sort:"));
    monitor_sort_label->setStyleSheet("color: gray; font-size: small;");
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
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
    middle_panel->addWidget(settings_tabs, 0, Qt::AlignTop);

    main_layout->addLayout(middle_panel, 3);

    QWidget* effects_tab = new QWidget();
    QVBoxLayout* effects_tab_layout = new QVBoxLayout(effects_tab);
    effects_tab_layout->setContentsMargins(4, 4, 4, 4);
    effects_tab_layout->setSpacing(6);

    const int kLeftPaneMinWidth = 320;
    QSplitter* effects_splitter = new QSplitter(Qt::Horizontal);
    effects_splitter->setChildrenCollapsible(false);
    effects_splitter->setHandleWidth(6);
    effects_tab_layout->addWidget(effects_splitter);

    QScrollArea* browser_scroll = new QScrollArea();
    browser_scroll->setWidgetResizable(true);
    browser_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    browser_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    browser_scroll->setMinimumWidth(kLeftPaneMinWidth);

    QWidget* browser_content = new QWidget();
    browser_content->setMinimumWidth(kLeftPaneMinWidth);
    QVBoxLayout* browser_layout = new QVBoxLayout(browser_content);
    browser_layout->setContentsMargins(4, 4, 12, 4);
    browser_layout->setSpacing(6);

    SetupEffectLibraryPanel(browser_layout);
    SetupEffectStackPanel(browser_layout);
    SetupZonesPanel(browser_layout);
    browser_layout->addStretch();

    browser_scroll->setWidget(browser_content);
    effects_splitter->addWidget(browser_scroll);

    QScrollArea* effects_detail_scroll = new QScrollArea();
    effects_detail_scroll->setWidgetResizable(true);
    effects_detail_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    effects_detail_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget* detail_content = new QWidget();
    QVBoxLayout* detail_layout = new QVBoxLayout(detail_content);
    detail_layout->setContentsMargins(4, 4, 4, 4);
    detail_layout->setSpacing(6);

    effect_config_group = new QGroupBox("Effect Configuration");
    effect_config_group->setVisible(false);
    QVBoxLayout* effect_layout = new QVBoxLayout(effect_config_group);
    effect_layout->setSpacing(4);

    QLabel* effect_label = new QLabel("Effect:");
    effect_layout->addWidget(effect_label);

    effect_combo = new QComboBox();
    effect_combo->setToolTip("Select an effect layer from the stack to edit its controls.");
    connect(effect_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_effect_changed);
    UpdateEffectCombo();
    effect_layout->addWidget(effect_combo);

    effect_zone_label = new QLabel("Zone:");
    effect_layout->addWidget(effect_zone_label);
    effect_zone_combo = new QComboBox();
    PopulateZoneTargetCombo(effect_zone_combo, -1);
    connect(effect_zone_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_effect_zone_changed(int)));
    effect_layout->addWidget(effect_zone_combo);

    origin_label = new QLabel("Origin:");
    effect_layout->addWidget(origin_label);
    effect_origin_combo = new QComboBox();
    effect_origin_combo->addItem("Room Center", QVariant(-1));
    connect(effect_origin_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_effect_origin_changed);
    effect_layout->addWidget(effect_origin_combo);

    stack_effect_type_combo = new QComboBox(effect_config_group);
    stack_effect_type_combo->addItem("None", "");
    std::vector<EffectRegistration3D> effect_list = EffectListManager3D::get()->GetAllEffects();
    for(unsigned int i = 0; i < effect_list.size(); i++)
    {
        if(effect_list[i].category == "Audio" && effect_list[i].class_name != "AudioContainer3D")
        {
            continue;
        }
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
    effect_layout->addWidget(effect_controls_widget);

    detail_layout->addWidget(effect_config_group);

    SetupAudioPanel(detail_layout);

    detail_layout->addStretch();
    effect_controls_widget->updateGeometry();
    effects_tab->updateGeometry();

    effects_detail_scroll->setWidget(detail_content);
    effects_splitter->addWidget(effects_detail_scroll);
    effects_splitter->setStretchFactor(0, 1);
    effects_splitter->setStretchFactor(1, 3);
    QList<int> initial_sizes;
    initial_sizes << kLeftPaneMinWidth << kLeftPaneMinWidth * 3;
    effects_splitter->setSizes(initial_sizes);

    connect(effects_splitter, &QSplitter::splitterMoved, this,
            [effects_splitter, kLeftPaneMinWidth](int, int)
            {
                QList<int> sizes = effects_splitter->sizes();
                if(sizes.size() < 2)
                {
                    return;
                }

                if(sizes[0] < kLeftPaneMinWidth)
                {
                    int total = sizes[0] + sizes[1];
                    sizes[0] = kLeftPaneMinWidth;
                    sizes[1] = std::max(1, total - kLeftPaneMinWidth);
                    effects_splitter->setSizes(sizes);
                }
            });

    main_tabs->addTab(effects_tab, "Effects / Presets");
    main_tabs->addTab(setup_tab, "Setup / Grid");

    setLayout(root_layout);
}

void OpenRGB3DSpatialTab::SetupEffectLibraryPanel(QVBoxLayout* parent_layout)
{
    QGroupBox* library_group = new QGroupBox("Effect Library");
    QVBoxLayout* library_layout = new QVBoxLayout(library_group);
    library_layout->setSpacing(4);

    QLabel* category_label = new QLabel(tr("Filter:"));
    category_label->setStyleSheet("color: gray; font-size: small;");
    library_layout->addWidget(category_label);

    effect_category_combo = new QComboBox();
    effect_category_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(effect_category_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_effect_library_category_changed);
    library_layout->addWidget(effect_category_combo);

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

    effect_category_combo->blockSignals(true);
    effect_category_combo->clear();
    effect_category_combo->addItem("Select Category", QVariant());

    std::vector<EffectRegistration3D> effects = EffectListManager3D::get()->GetAllEffects();
    std::vector<std::string> categories;
    for(unsigned int i = 0; i < effects.size(); i++)
    {
        const std::string& category = effects[i].category;
        bool exists = false;
        for(unsigned int j = 0; j < categories.size(); j++)
        {
            if(categories[j] == category)
            {
                exists = true;
                break;
            }
        }
        if(!exists)
        {
            categories.push_back(category);
            effect_category_combo->addItem(QString::fromStdString(category),
                                           QString::fromStdString(category));
        }
    }
    effect_category_combo->blockSignals(false);
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

    effect_library_list->blockSignals(true);
    effect_library_list->clear();

    QVariant category_data;
    if(effect_category_combo)
    {
        category_data = effect_category_combo->currentData();
    }

    if(!category_data.isValid())
    {
        effect_library_list->blockSignals(false);
        on_effect_library_selection_changed(-1);
        return;
    }

    QString selected_category = category_data.toString();

    std::vector<EffectRegistration3D> effects = EffectListManager3D::get()->GetAllEffects();
    for(unsigned int i = 0; i < effects.size(); i++)
    {
        QString category = QString::fromStdString(effects[i].category);
        if(selected_category.isEmpty() ||
           category.compare(selected_category, Qt::CaseInsensitive) != 0)
        {
            continue;
        }
        if(category.compare("Audio", Qt::CaseInsensitive) == 0 &&
           effects[i].class_name != "AudioContainer3D")
        {
            continue;
        }
        QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(effects[i].ui_name));
        item->setToolTip(QString("Category: %1").arg(category));
        item->setData(Qt::UserRole, QString::fromStdString(effects[i].class_name));
        item->setData(Qt::UserRole + 1, category);
        effect_library_list->addItem(item);
    }

    effect_library_list->blockSignals(false);
    effect_library_list->setCurrentRow(-1);
    on_effect_library_selection_changed(-1);
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
    UpdateEffectStackList();
    if(effect_stack_list)
    {
        effect_stack_list->setCurrentRow((int)effect_stack.size() - 1);
    }
    if(effect_timer && !effect_timer->isActive())
    {
        effect_timer->start(33);
    }
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

    SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(class_name.toStdString());
    if(!effect)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create effect: %s", class_name.toStdString().c_str());
        return;
    }
    

    effect->setParent(effect_controls_widget);
    effect->CreateCommonEffectControls(effect_controls_widget);
    effect->SetupCustomUI(effect_controls_widget);
    current_effect_ui = effect;

    if(class_name == QLatin1String("ScreenMirror3D"))
    {
        ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(effect);
        if(screen_mirror)
        {
            screen_mirror->SetReferencePoints(&reference_points);
            connect(this, &OpenRGB3DSpatialTab::GridLayoutChanged, screen_mirror, &ScreenMirror3D::RefreshMonitorStatus);
            QTimer::singleShot(200, screen_mirror, &ScreenMirror3D::RefreshMonitorStatus);
            QTimer::singleShot(300, screen_mirror, &ScreenMirror3D::RefreshReferencePointDropdowns);
        }
        
        RemoveWidgetFromParentLayout(origin_label);
        RemoveWidgetFromParentLayout(effect_origin_combo);
    }
    else
    {
        if(origin_label) origin_label->setVisible(true);
        if(effect_origin_combo) effect_origin_combo->setVisible(true);
    }

    start_effect_button = effect->GetStartButton();
    stop_effect_button = effect->GetStopButton();
    
    connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
    connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
    connect(effect, &SpatialEffect3D::ParametersChanged, this, [this]()
    {
        RefreshEffectDisplay();
    });

    effect_controls_layout->addWidget(effect);

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
    zones_help_label->setForegroundRole(QPalette::PlaceholderText);
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
    if(effect_library_add_button)
    {
        effect_library_add_button->setEnabled(row >= 0);
    }
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

    QListWidgetItem* item = effect_library_list->item(current_row);
    if(!item)
    {
        return;
    }

    QString class_name = item->data(Qt::UserRole).toString();
    QString ui_name = item->text();
    AddEffectInstanceToStack(class_name, ui_name);
}

void OpenRGB3DSpatialTab::on_effect_library_item_double_clicked(QListWidgetItem* item)
{
    if(!item)
    {
        return;
    }

    QString class_name = item->data(Qt::UserRole).toString();
    QString ui_name = item->text();
    AddEffectInstanceToStack(class_name, ui_name);
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

    if(!effect_controls_layout)
    {
        return;
    }
    QLayoutItem* item;
    while((item = effect_controls_layout->takeAt(0)) != nullptr)
    {
        if(QWidget* w = item->widget())
        {
            w->hide();
            w->setParent(nullptr);
            delete w;
        }
        delete item;
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
    
    if(class_name == QLatin1String("ScreenMirror3D"))
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

    effect_origin_combo->blockSignals(true);
    effect_origin_combo->clear();

    effect_origin_combo->addItem("Room Center", QVariant(-1));

    for(size_t i = 0; i < reference_points.size(); i++)
    {
        VirtualReferencePoint3D* ref_point = reference_points[i].get();
        if(!ref_point) continue;

        QString name = QString::fromStdString(ref_point->GetName());
        QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
        QString display = QString("%1 (%2)").arg(name).arg(type);
        effect_origin_combo->addItem(display, QVariant((int)i));
    }

    effect_origin_combo->blockSignals(false);
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
    int ref_point_idx = effect_origin_combo->itemData(index).toInt();

    Vector3D origin = {0.0f, 0.0f, 0.0f};

    if(ref_point_idx >= 0 && ref_point_idx < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_point = reference_points[ref_point_idx].get();
        origin = ref_point->GetPosition();
    }

    if(current_effect_ui)
    {
        current_effect_ui->SetCustomReferencePoint(origin);
    }

    if(viewport) viewport->UpdateColors();
}

void OpenRGB3DSpatialTab::ComputeAutoRoomExtents(float& width_mm, float& depth_mm, float& height_mm) const
{
    ManualRoomSettings auto_room = MakeManualRoomSettings(false, 0.0f, 0.0f, 0.0f);
    GridBounds bounds = ComputeGridBounds(auto_room, grid_scale_mm, controller_transforms);
    GridExtents extents = BoundsToExtents(bounds);

    width_mm  = GridUnitsToMM(extents.width_units, grid_scale_mm);
    depth_mm  = GridUnitsToMM(extents.height_units, grid_scale_mm);
    height_mm = GridUnitsToMM(extents.depth_units, grid_scale_mm);
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