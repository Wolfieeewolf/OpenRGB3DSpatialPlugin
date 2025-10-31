/*---------------------------------------------------------*\

    // Custom Audio Effects (save/load)
    SetupAudioCustomEffectsUI(layout);
/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab.cpp                                   |
|                                                           |
|   Main UI tab for 3D spatial control                     |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include "ControllerLayout3D.h"
#include "LogManager.h"
#include "CustomControllerDialog.h"
#include "VirtualController3D.h"
#include "DisplayPlaneManager.h"
#include "ScreenCaptureManager.h"
#include "Effects3D/ScreenMirror3D/ScreenMirror3D.h"
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
#include "sdk/OpenRGB3DSpatialSDK.h"

// SDK wrappers: expose data to other plugins without exposing internals
static OpenRGB3DSpatialTab* g_spatial_tab_sdk = nullptr;
static float SDK_Wrap_GetGridScaleMM() { return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_GetGridScaleMM() : 10.0f; }
static void  SDK_Wrap_GetRoomDimensions(float* w, float* d, float* h, bool* use_manual)
{
    if(!g_spatial_tab_sdk) { if(w) *w=0; if(d) *d=0; if(h) *h=0; if(use_manual) *use_manual=false; return; }
    float ww, dd, hh; bool um;
    g_spatial_tab_sdk->SDK_GetRoomDimensions(ww, dd, hh, um);
    if(w) *w=ww; if(d) *d=dd; if(h) *h=hh; if(use_manual) *use_manual=um;
}
static size_t SDK_Wrap_GetControllerCount() { return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_GetControllerCount() : 0; }
static bool   SDK_Wrap_GetControllerName(size_t idx, char* buf, size_t buf_size)
{
    if(!g_spatial_tab_sdk || !buf || buf_size==0) return false;
    std::string s; if(!g_spatial_tab_sdk->SDK_GetControllerName(idx, s)) return false;
    size_t n = std::min(buf_size-1, s.size()); memcpy(buf, s.data(), n); buf[n]='\0'; return true;
}
static bool   SDK_Wrap_IsControllerVirtual(size_t idx) { return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_IsControllerVirtual(idx) : false; }
static int    SDK_Wrap_GetControllerGranularity(size_t idx) { return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_GetControllerGranularity(idx) : 0; }
static int    SDK_Wrap_GetControllerItemIndex(size_t idx) { return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_GetControllerItemIndex(idx) : 0; }
static size_t SDK_Wrap_GetLEDCount(size_t c) { return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_GetLEDCount(c) : 0; }
static bool   SDK_Wrap_GetLEDWorldPosition(size_t c, size_t i, float* x, float* y, float* z)
{
    if(!g_spatial_tab_sdk) return false; float xx,yy,zz; bool ok = g_spatial_tab_sdk->SDK_GetLEDWorldPosition(c,i,xx,yy,zz);
    if(!ok) return false; if(x) *x=xx; if(y) *y=yy; if(z) *z=zz; return true;
}
static bool   SDK_Wrap_GetLEDWorldPositions(size_t c, float* xyz, size_t max_triplets, size_t* out_count)
{
    if(!g_spatial_tab_sdk || !xyz) { if(out_count) *out_count = 0; return false; }
    size_t out = 0; bool ok = g_spatial_tab_sdk->SDK_GetLEDWorldPositions(c, xyz, max_triplets, out);
    if(out_count) *out_count = out; return ok;
}



OpenRGB3DSpatialTab::OpenRGB3DSpatialTab(ResourceManagerInterface* rm, QWidget *parent) :
    QWidget(parent),
    resource_manager(rm),
    first_load(true)
{

    effect_controls_widget = nullptr;
    effect_controls_layout = nullptr;
    current_effect_ui = nullptr;
    start_effect_button = nullptr;
    stop_effect_button = nullptr;
    effect_origin_combo = nullptr;
    effect_zone_combo = nullptr;
    effect_combo = nullptr;
    effect_type_combo = nullptr;

    available_controllers_list = nullptr;
    custom_controllers_list = nullptr;
    object_creator_status_label = nullptr;
    controller_list = nullptr;
    reference_points_list = nullptr;
    display_planes_list = nullptr;
    display_plane_name_edit = nullptr;
    display_plane_width_spin = nullptr;
    display_plane_height_spin = nullptr;
    display_plane_monitor_combo = nullptr;
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
    custom_grid_x = 10;
    custom_grid_y = 10;
    custom_grid_z = 10;
    grid_scale_mm = 10.0f;  // Default: 10mm = 1 grid unit

    room_width_spin = nullptr;
    room_depth_spin = nullptr;
    room_height_spin = nullptr;
    use_manual_room_size_checkbox = nullptr;
    manual_room_width = 1000.0f;   // Default: 1000 mm
    manual_room_depth = 1000.0f;   // Default: 1000 mm
    manual_room_height = 1000.0f;  // Default: 1000 mm
    use_manual_room_size = false;  // Start with auto-detect

    led_spacing_x_spin = nullptr;
    led_spacing_y_spin = nullptr;
    led_spacing_z_spin = nullptr;
    led_spacing_preset_combo = nullptr;

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
    stack_effect_blend_combo = nullptr;
    stack_effect_controls_container = nullptr;
    stack_effect_controls_layout = nullptr;
    stack_presets_list = nullptr;
    next_effect_instance_id = 1;

    worker_thread = nullptr;

    SetupUI();
    LoadDevices();
    LoadCustomControllers();
    UpdateDisplayPlanesList();
    RefreshDisplayPlaneDetails();

    /*---------------------------------------------------------*\
    | Initialize zone and effect combos                        |
    \*---------------------------------------------------------*/
    UpdateEffectZoneCombo();        // Initialize Effects tab zone dropdown
    UpdateEffectOriginCombo();      // Initialize Effects tab origin dropdown
    UpdateAudioEffectZoneCombo();   // Initialize Audio tab zone dropdown
    UpdateAudioEffectOriginCombo(); // Initialize Audio tab origin dropdown

    auto_load_timer = new QTimer(this);
    auto_load_timer->setSingleShot(true);
    connect(auto_load_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::TryAutoLoadLayout);
    auto_load_timer->start(2000);

    effect_timer = new QTimer(this);
    effect_timer->setTimerType(Qt::PreciseTimer);
    connect(effect_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::on_effect_timer_timeout);

    worker_thread = new EffectWorkerThread3D(this);
    connect(worker_thread, &EffectWorkerThread3D::ColorsReady, this, &OpenRGB3DSpatialTab::ApplyColorsFromWorker);

    // Connect GridLayoutChanged signal to invoke SDK callbacks
    connect(this, &OpenRGB3DSpatialTab::GridLayoutChanged, this, [this]() {
        for(const std::pair<void (*)(void*), void*>& cb_pair : grid_layout_callbacks)
        {
            if(cb_pair.first) cb_pair.first(cb_pair.second);
        }
    });

    // Publish SDK surface for other plugins via Qt property
    {
        static ORGB3DGridAPI api; // static lifetime for stable address
        api.api_version = 1;
        g_spatial_tab_sdk = this;
        api.GetGridScaleMM = &SDK_Wrap_GetGridScaleMM;
        api.GetRoomDimensions = &SDK_Wrap_GetRoomDimensions;
        api.GetControllerCount = &SDK_Wrap_GetControllerCount;
        api.GetControllerName = &SDK_Wrap_GetControllerName;
        api.IsControllerVirtual = &SDK_Wrap_IsControllerVirtual;
        api.GetControllerGranularity = &SDK_Wrap_GetControllerGranularity;
        api.GetControllerItemIndex = &SDK_Wrap_GetControllerItemIndex;
        api.GetLEDCount = &SDK_Wrap_GetLEDCount;
        api.GetLEDWorldPosition = &SDK_Wrap_GetLEDWorldPosition;
        api.GetLEDWorldPositions = &SDK_Wrap_GetLEDWorldPositions;
        api.GetTotalLEDCount = []() -> size_t { return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_GetTotalLEDCount() : 0; };
        api.GetAllLEDWorldPositions = [](float* xyz, size_t max_triplets, size_t* out_count) -> bool {
            if(!g_spatial_tab_sdk) { if(out_count) *out_count = 0; return false; }
            size_t out = 0; bool ok = g_spatial_tab_sdk->SDK_GetAllLEDWorldPositions(xyz, max_triplets, out);
            if(out_count) *out_count = out; return ok;
        };
        api.GetAllLEDWorldPositionsWithOffsets = [](float* xyz, size_t max_triplets, size_t* out_triplets, size_t* offsets, size_t offsets_cap, size_t* out_ctrls) -> bool {
            if(!g_spatial_tab_sdk) { if(out_triplets) *out_triplets = 0; if(out_ctrls) *out_ctrls = 0; return false; }
            size_t trips = 0, ctrls = 0;
            bool ok = g_spatial_tab_sdk->SDK_GetAllLEDWorldPositionsWithOffsets(xyz, max_triplets, trips, offsets, offsets_cap, ctrls);
            if(out_triplets) *out_triplets = trips;
            if(out_ctrls) *out_ctrls = ctrls;
            return ok;
        };
        api.RegisterGridLayoutCallback = [](void (*cb)(void*), void* user) -> bool {
            return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_RegisterGridLayoutCallback(cb, user) : false;
        };
        api.UnregisterGridLayoutCallback = [](void (*cb)(void*), void* user) -> bool {
            return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_UnregisterGridLayoutCallback(cb, user) : false;
        };
        api.SetControllerColors = [](size_t ctrl_idx, const unsigned int* bgr, size_t count) -> bool {
            return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_SetControllerColors(ctrl_idx, bgr, count) : false;
        };
        api.SetSingleLEDColor = [](size_t ctrl_idx, size_t led_idx, unsigned int bgr) -> bool {
            return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_SetSingleLEDColor(ctrl_idx, led_idx, bgr) : false;
        };
        api.SetGridOrderColors = [](const unsigned int* bgr, size_t count) -> bool {
            return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_SetGridOrderColors(bgr, count) : false;
        };
        api.SetGridOrderColorsWithOrder = [](int order, const unsigned int* bgr, size_t count) -> bool {
            return g_spatial_tab_sdk ? g_spatial_tab_sdk->SDK_SetGridOrderColorsWithOrder(order, bgr, count) : false;
        };
        qApp->setProperty("OpenRGB3DSpatialGridAPI", QVariant::fromValue<qulonglong>((qulonglong)(uintptr_t)(&api)));
    }
}

OpenRGB3DSpatialTab::~OpenRGB3DSpatialTab()
{
    // Clear published SDK pointer
    qApp->setProperty("OpenRGB3DSpatialGridAPI", QVariant());
    g_spatial_tab_sdk = nullptr;

    // Persist last camera to settings before teardown
    if(viewport)
    {
        float dist, yaw, pitch, tx, ty, tz;
        viewport->GetCamera(dist, yaw, pitch, tx, ty, tz);
        try
        {
            nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
            settings["Camera"]["Distance"] = dist;
            settings["Camera"]["Yaw"] = yaw;
            settings["Camera"]["Pitch"] = pitch;
            settings["Camera"]["TargetX"] = tx;
            settings["Camera"]["TargetY"] = ty;
            settings["Camera"]["TargetZ"] = tz;
            resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", settings);
        }
        catch(const std::exception&){ /* ignore settings errors */ }
    }

    if(worker_thread)
    {
        worker_thread->StopEffect();
        worker_thread->quit();
        worker_thread->wait();
        delete worker_thread;
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

    /*---------------------------------------------------------*\
    | Create main tab widget to separate Setup and Effects     |
    \*---------------------------------------------------------*/
    QVBoxLayout* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    QTabWidget* main_tabs = new QTabWidget();
    root_layout->addWidget(main_tabs);

    /*---------------------------------------------------------*\
    | Setup Tab (Grid/Layout Configuration)                    |
    \*---------------------------------------------------------*/
    QWidget* setup_tab = new QWidget();
    QHBoxLayout* main_layout = new QHBoxLayout(setup_tab);
    main_layout->setSpacing(8);
    main_layout->setContentsMargins(8, 8, 8, 8);

    /*---------------------------------------------------------*\
    | Left panel with scroll area                              |
    \*---------------------------------------------------------*/
    QScrollArea* left_scroll = new QScrollArea();
    left_scroll->setWidgetResizable(true);
    left_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    left_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Make side panel more flexible in narrow windows and cap width when maximized
    left_scroll->setMinimumWidth(260);
    left_scroll->setMaximumWidth(420);
    left_scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QWidget* left_content = new QWidget();
    QVBoxLayout* left_panel = new QVBoxLayout(left_content);
    left_panel->setSpacing(8);

    /*---------------------------------------------------------*\
    | Tab Widget for left panel                                |
    \*---------------------------------------------------------*/
    left_tabs = new QTabWidget();

    /*---------------------------------------------------------*\
    | Available Controllers Tab                                |
    \*---------------------------------------------------------*/
    QWidget* available_tab = new QWidget();
    QVBoxLayout* available_layout = new QVBoxLayout();
    available_layout->setSpacing(5);

    available_controllers_list = new QListWidget();
    available_controllers_list->setMinimumHeight(200);
    connect(available_controllers_list, &QListWidget::currentRowChanged, [this](int) {
        on_granularity_changed(granularity_combo->currentIndex());
    });
    available_layout->addWidget(available_controllers_list);

    QHBoxLayout* granularity_layout = new QHBoxLayout();
    granularity_layout->addWidget(new QLabel("Add:"));
    granularity_combo = new QComboBox();
    granularity_combo->addItem("Whole Device");
    granularity_combo->addItem("Zone");
    granularity_combo->addItem("LED");
    connect(granularity_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_granularity_changed(int)));
    granularity_layout->addWidget(granularity_combo);
    available_layout->addLayout(granularity_layout);

    item_combo = new QComboBox();
    available_layout->addWidget(item_combo);

    // LED Spacing Controls
    QLabel* spacing_label = new QLabel("LED Spacing (mm):");
    spacing_label->setStyleSheet("font-weight: bold; margin-top: 5px;");
    available_layout->addWidget(spacing_label);

    QGridLayout* spacing_grid = new QGridLayout();
    spacing_grid->setSpacing(3);

    spacing_grid->addWidget(new QLabel("X:"), 0, 0);
    led_spacing_x_spin = new QDoubleSpinBox();
    led_spacing_x_spin->setRange(0.0, 1000.0);
    led_spacing_x_spin->setSingleStep(1.0);
    led_spacing_x_spin->setValue(10.0);
    led_spacing_x_spin->setSuffix(" mm");
    led_spacing_x_spin->setToolTip("Horizontal spacing between LEDs (left/right)");
    spacing_grid->addWidget(led_spacing_x_spin, 0, 1);

    spacing_grid->addWidget(new QLabel("Y:"), 0, 2);
    led_spacing_y_spin = new QDoubleSpinBox();
    led_spacing_y_spin->setRange(0.0, 1000.0);
    led_spacing_y_spin->setSingleStep(1.0);
    led_spacing_y_spin->setValue(0.0);
    led_spacing_y_spin->setSuffix(" mm");
    led_spacing_y_spin->setToolTip("Vertical spacing between LEDs (floor/ceiling)");
    spacing_grid->addWidget(led_spacing_y_spin, 0, 3);

    spacing_grid->addWidget(new QLabel("Z:"), 1, 0);
    led_spacing_z_spin = new QDoubleSpinBox();
    led_spacing_z_spin->setRange(0.0, 1000.0);
    led_spacing_z_spin->setSingleStep(1.0);
    led_spacing_z_spin->setValue(0.0);
    led_spacing_z_spin->setSuffix(" mm");
    led_spacing_z_spin->setToolTip("Depth spacing between LEDs (front/back)");
    spacing_grid->addWidget(led_spacing_z_spin, 1, 1);

    led_spacing_preset_combo = new QComboBox();
    led_spacing_preset_combo->addItem("Custom");
    led_spacing_preset_combo->addItem("Dense Strip (10mm)");
    led_spacing_preset_combo->addItem("Keyboard (19mm)");
    led_spacing_preset_combo->addItem("Sparse Strip (33mm)");
    led_spacing_preset_combo->addItem("LED Cube (50mm)");
    led_spacing_preset_combo->setToolTip("Quick presets for common LED configurations");
    spacing_grid->addWidget(led_spacing_preset_combo, 1, 2, 1, 2);

    available_layout->addLayout(spacing_grid);

    // Connect LED spacing preset combo
    connect(led_spacing_preset_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_led_spacing_preset_changed(int)));

    QHBoxLayout* add_remove_layout = new QHBoxLayout();
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

    /*---------------------------------------------------------*\
    | Controllers in 3D Scene (below tabs)                     |
    \*---------------------------------------------------------*/
    QGroupBox* controller_group = new QGroupBox("Controllers in 3D Scene");
    QVBoxLayout* controller_layout = new QVBoxLayout();
    controller_layout->setSpacing(5);

    controller_list = new QListWidget();
    controller_list->setMaximumHeight(80);
    connect(controller_list, &QListWidget::currentRowChanged, [this](int row) {
        if(row >= 0)
        {
            viewport->SelectController(row);
            on_controller_selected(row);
        }
    });
    controller_layout->addWidget(controller_list);

    // LED Spacing edit for selected controller
    QLabel* edit_spacing_label = new QLabel("Edit Selected LED Spacing:");
    edit_spacing_label->setStyleSheet("font-weight: bold; margin-top: 5px;");
    controller_layout->addWidget(edit_spacing_label);

    QGridLayout* edit_spacing_grid = new QGridLayout();
    edit_spacing_grid->setSpacing(3);

    edit_spacing_grid->addWidget(new QLabel("X:"), 0, 0);
    edit_led_spacing_x_spin = new QDoubleSpinBox();
    edit_led_spacing_x_spin->setRange(0.0, 1000.0);
    edit_led_spacing_x_spin->setValue(10.0);
    edit_led_spacing_x_spin->setSuffix(" mm");
    edit_led_spacing_x_spin->setEnabled(false);
    edit_spacing_grid->addWidget(edit_led_spacing_x_spin, 0, 1);

    edit_spacing_grid->addWidget(new QLabel("Y:"), 0, 2);
    edit_led_spacing_y_spin = new QDoubleSpinBox();
    edit_led_spacing_y_spin->setRange(0.0, 1000.0);
    edit_led_spacing_y_spin->setValue(0.0);
    edit_led_spacing_y_spin->setSuffix(" mm");
    edit_led_spacing_y_spin->setEnabled(false);
    edit_spacing_grid->addWidget(edit_led_spacing_y_spin, 0, 3);

    edit_spacing_grid->addWidget(new QLabel("Z:"), 1, 0);
    edit_led_spacing_z_spin = new QDoubleSpinBox();
    edit_led_spacing_z_spin->setRange(0.0, 1000.0);
    edit_led_spacing_z_spin->setValue(0.0);
    edit_led_spacing_z_spin->setSuffix(" mm");
    edit_led_spacing_z_spin->setEnabled(false);
    edit_spacing_grid->addWidget(edit_led_spacing_z_spin, 1, 1);

    apply_spacing_button = new QPushButton("Apply Spacing");
    apply_spacing_button->setEnabled(false);
    connect(apply_spacing_button, SIGNAL(clicked()), this, SLOT(on_apply_spacing_clicked()));
    edit_spacing_grid->addWidget(apply_spacing_button, 1, 2, 1, 2);

    controller_layout->addLayout(edit_spacing_grid);

    controller_group->setLayout(controller_layout);
    left_panel->addWidget(controller_group);

    /*---------------------------------------------------------*\
    | Add stretch to push content to top of scroll area       |
    \*---------------------------------------------------------*/
    left_panel->addStretch();

    /*---------------------------------------------------------*\
    | Set up left scroll area and add to main layout          |
    \*---------------------------------------------------------*/
    left_scroll->setWidget(left_content);
    main_layout->addWidget(left_scroll, 1);

    QVBoxLayout* middle_panel = new QVBoxLayout();

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
    viewport->SetDisplayPlanes(&display_planes);
    // Ensure viewport uses the current grid scale for mm->grid conversion
    viewport->SetGridScaleMM(grid_scale_mm);
    viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);

    // Restore last camera from settings (if available)
    try
    {
        nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
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
    catch(const std::exception&){ /* ignore settings errors */ }

    connect(viewport, SIGNAL(ControllerSelected(int)), this, SLOT(on_controller_selected(int)));
    connect(viewport, SIGNAL(ControllerPositionChanged(int,float,float,float)),
            this, SLOT(on_controller_position_changed(int,float,float,float)));
    connect(viewport, SIGNAL(ControllerRotationChanged(int,float,float,float)),
            this, SLOT(on_controller_rotation_changed(int,float,float,float)));
    connect(viewport, SIGNAL(ControllerDeleteRequested(int)), this, SLOT(on_remove_controller_from_viewport(int)));
    connect(viewport, SIGNAL(ReferencePointSelected(int)), this, SLOT(on_ref_point_selected(int)));
    connect(viewport, SIGNAL(ReferencePointPositionChanged(int,float,float,float)),
            this, SLOT(on_ref_point_position_changed(int,float,float,float)));
    connect(viewport, SIGNAL(DisplayPlanePositionChanged(int,float,float,float)),
            this, SLOT(on_display_plane_position_signal(int,float,float,float)));
    connect(viewport, SIGNAL(DisplayPlaneRotationChanged(int,float,float,float)),
            this, SLOT(on_display_plane_rotation_signal(int,float,float,float)));
    middle_panel->addWidget(viewport, 1);

    /*---------------------------------------------------------*\
    | Tab Widget for Position/Rotation and Grid Settings      |
    \*---------------------------------------------------------*/
    QTabWidget* settings_tabs = new QTabWidget();

    /*---------------------------------------------------------*\
    | Grid Settings Tab                                        |
    \*---------------------------------------------------------*/
    QWidget* grid_settings_tab = new QWidget();
    QGridLayout* layout_layout = new QGridLayout();
    layout_layout->setSpacing(5);

    // Grid Dimensions
    layout_layout->addWidget(new QLabel("Grid X:"), 0, 0);
    grid_x_spin = new QSpinBox();
    grid_x_spin->setRange(1, 100);
    grid_x_spin->setValue(custom_grid_x);
    layout_layout->addWidget(grid_x_spin, 0, 1);

    layout_layout->addWidget(new QLabel("Grid Y:"), 0, 2);
    grid_y_spin = new QSpinBox();
    grid_y_spin->setRange(1, 100);
    grid_y_spin->setValue(custom_grid_y);
    layout_layout->addWidget(grid_y_spin, 0, 3);

    layout_layout->addWidget(new QLabel("Grid Z:"), 0, 4);
    grid_z_spin = new QSpinBox();
    grid_z_spin->setRange(1, 100);
    grid_z_spin->setValue(custom_grid_z);
    layout_layout->addWidget(grid_z_spin, 0, 5);

    // Grid Scale (mm per grid unit)
    layout_layout->addWidget(new QLabel("Grid Scale:"), 1, 0);
    grid_scale_spin = new QDoubleSpinBox();
    grid_scale_spin->setRange(0.1, 1000.0);
    grid_scale_spin->setSingleStep(1.0);
    grid_scale_spin->setValue(grid_scale_mm);
    grid_scale_spin->setSuffix(" mm/unit");
    grid_scale_spin->setToolTip("Physical size of one grid unit in millimeters (default: 10mm = 1cm)");
    layout_layout->addWidget(grid_scale_spin, 1, 1, 1, 2);

    // Update grid scale when changed: affects viewport conversion and LED spacing to grid units
    connect(grid_scale_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value){
        grid_scale_mm = (float)value;
        if(viewport) {
            viewport->SetGridScaleMM(grid_scale_mm);
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        // Regenerate LED positions for all controllers to reflect new grid scale
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

    // Grid Snap Checkbox
    grid_snap_checkbox = new QCheckBox("Grid Snapping");
    grid_snap_checkbox->setToolTip("Snap controller positions to grid intersections");
    layout_layout->addWidget(grid_snap_checkbox, 1, 3, 1, 3);

    /*---------------------------------------------------------*\
    | Room Dimensions Section                                  |
    \*---------------------------------------------------------*/
    layout_layout->addWidget(new QLabel("━━━ Room Dimensions (Origin: Front-Left-Floor Corner) ━━━"), 2, 0, 1, 6);

    // Manual room size checkbox
    use_manual_room_size_checkbox = new QCheckBox("Use Manual Room Size");
    use_manual_room_size_checkbox->setChecked(use_manual_room_size);
    use_manual_room_size_checkbox->setToolTip("Enable to set room dimensions manually. Disable to auto-detect from LED positions.");
    layout_layout->addWidget(use_manual_room_size_checkbox, 3, 0, 1, 2);

    // Room Width (X-axis: Left to Right)
    layout_layout->addWidget(new QLabel("Width (X):"), 4, 0);
    room_width_spin = new QDoubleSpinBox();
    room_width_spin->setRange(100.0, 50000.0);
    room_width_spin->setSingleStep(10.0);
    room_width_spin->setValue(manual_room_width);
    room_width_spin->setSuffix(" mm");
    room_width_spin->setToolTip("Room width (left wall to right wall)");
    room_width_spin->setEnabled(use_manual_room_size);
    layout_layout->addWidget(room_width_spin, 4, 1);

    // Room Height (Y-axis: Floor to Ceiling, Y-up)
    layout_layout->addWidget(new QLabel("Height (Y):"), 4, 2);
    room_depth_spin = new QDoubleSpinBox();  // NOTE: Variable name is legacy, actually controls HEIGHT
    room_depth_spin->setRange(100.0, 50000.0);
    room_depth_spin->setSingleStep(10.0);
    room_depth_spin->setValue(manual_room_depth);  // NOTE: Variable name is legacy, stores height
    room_depth_spin->setSuffix(" mm");
    room_depth_spin->setToolTip("Room height (floor to ceiling, Y-axis in standard OpenGL Y-up)");
    room_depth_spin->setEnabled(use_manual_room_size);
    layout_layout->addWidget(room_depth_spin, 4, 3);

    // Room Depth (Z-axis: Front to Back)
    layout_layout->addWidget(new QLabel("Depth (Z):"), 4, 4);
    room_height_spin = new QDoubleSpinBox();  // NOTE: Variable name is legacy, actually controls DEPTH
    room_height_spin->setRange(100.0, 50000.0);
    room_height_spin->setSingleStep(10.0);
    room_height_spin->setValue(manual_room_height);  // NOTE: Variable name is legacy, stores depth
    room_height_spin->setSuffix(" mm");
    room_height_spin->setToolTip("Room depth (front to back, Z-axis in standard OpenGL Y-up)");
    room_height_spin->setEnabled(use_manual_room_size);
    layout_layout->addWidget(room_height_spin, 4, 5);

    // Selection Info Label
    selection_info_label = new QLabel("No selection");
    selection_info_label->setStyleSheet("color: gray; font-size: 10px; font-weight: bold;");
    selection_info_label->setAlignment(Qt::AlignRight);
    layout_layout->addWidget(selection_info_label, 1, 3, 1, 3);

    // Add helpful labels with text wrapping
    QLabel* grid_help1 = new QLabel("Measure from front-left-floor corner • Positions in grid units (×" + QString::number(grid_scale_mm) + "mm)");
    grid_help1->setStyleSheet("color: gray; font-size: 10px;");
    grid_help1->setWordWrap(true);
    layout_layout->addWidget(grid_help1, 5, 0, 1, 6);

    QLabel* grid_help2 = new QLabel("Use Ctrl+Click for multi-select • Add User position in Object Creator tab");
    grid_help2->setStyleSheet("color: gray; font-size: 10px;");
    grid_help2->setWordWrap(true);
    layout_layout->addWidget(grid_help2, 6, 0, 1, 6);

    grid_settings_tab->setLayout(layout_layout);

    // Connect signals
    connect(grid_x_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_y_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_z_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_snap_checkbox, &QCheckBox::toggled, this, &OpenRGB3DSpatialTab::on_grid_snap_toggled);

    // Connect room dimension signals
    connect(use_manual_room_size_checkbox, &QCheckBox::toggled, [this](bool checked) {
        use_manual_room_size = checked;
        if(room_width_spin) room_width_spin->setEnabled(checked);
        if(room_depth_spin) room_depth_spin->setEnabled(checked);
        if(room_height_spin) room_height_spin->setEnabled(checked);

        // Update viewport with new settings
        if(viewport)
        {
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        emit GridLayoutChanged();
    });

    connect(room_width_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        manual_room_width = value;

        // Update viewport with new width
        if(viewport)
        {
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        emit GridLayoutChanged();
    });

    connect(room_depth_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        manual_room_depth = value;

        // Update viewport with new depth
        if(viewport)
        {
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        emit GridLayoutChanged();
    });

    connect(room_height_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        manual_room_height = value;

        // Update viewport with new height
        if(viewport)
        {
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        emit GridLayoutChanged();
    });

    /*---------------------------------------------------------*\
    | Position & Rotation Tab                                  |
    \*---------------------------------------------------------*/
    QWidget* transform_tab = new QWidget();
    QGridLayout* position_layout = new QGridLayout();
    position_layout->setSpacing(5);

    position_layout->addWidget(new QLabel("Position X:"), 0, 0);

    pos_x_slider = new QSlider(Qt::Horizontal);
    pos_x_slider->setRange(-5000, 5000);  // Corner-origin: 0 (left wall) to 500 grid units (5000mm = 5m)
    pos_x_slider->setValue(0);
    connect(pos_x_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        if(pos_x_spin)
        {
            QSignalBlocker block_spin(pos_x_spin);
            pos_x_spin->setValue(pos_value);
        }

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.position.x = pos_value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.position.x = (float)pos_value;
                SyncDisplayPlaneControls(plane);
                viewport->SelectDisplayPlane(current_display_plane_index);
                viewport->NotifyDisplayPlaneChanged();
                emit GridLayoutChanged();
            }
            return;
        }

        // Otherwise check if a reference point is selected
        int ref_idx = reference_points_list->currentRow();
        if(ref_idx >= 0 && ref_idx < (int)reference_points.size())
        {
            VirtualReferencePoint3D* ref_point = reference_points[ref_idx].get();
            Vector3D pos = ref_point->GetPosition();
            pos.x = pos_value;
            ref_point->SetPosition(pos);
            viewport->update();
        }
    });
    position_layout->addWidget(pos_x_slider, 0, 1);

    pos_x_spin = new QDoubleSpinBox();
    pos_x_spin->setRange(-500, 500);  // Allow negative for outside-room LEDs, up to 500 grid units
    pos_x_spin->setDecimals(1);
    pos_x_spin->setMaximumWidth(80);
    connect(pos_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        if(pos_x_slider)
        {
            QSignalBlocker block_slider(pos_x_slider);
            pos_x_slider->setValue((int)std::lround(value * 10.0));
        }

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.position.x = value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.position.x = (float)value;
                SyncDisplayPlaneControls(plane);
                viewport->SelectDisplayPlane(current_display_plane_index);
                viewport->NotifyDisplayPlaneChanged();
                emit GridLayoutChanged();
            }
            return;
        }

        // Otherwise check if a reference point is selected
        int ref_idx = reference_points_list->currentRow();
        if(ref_idx >= 0 && ref_idx < (int)reference_points.size())
        {
            VirtualReferencePoint3D* ref_point = reference_points[ref_idx].get();
            Vector3D pos = ref_point->GetPosition();
            pos.x = value;
            ref_point->SetPosition(pos);
            viewport->update();
        }
    });
    position_layout->addWidget(pos_x_spin, 0, 2);

    position_layout->addWidget(new QLabel("Position Y:"), 1, 0);

    pos_y_slider = new QSlider(Qt::Horizontal);
    pos_y_slider->setRange(-5000, 5000);
    pos_y_slider->setValue(0);
    connect(pos_y_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        if(pos_y_spin)
        {
            QSignalBlocker block_spin(pos_y_spin);
            pos_y_spin->setValue(pos_value);
        }

        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            if(pos_value < 0.0)
            {
                pos_value = 0.0;
                if(pos_y_spin)
                {
                    QSignalBlocker block_spin(pos_y_spin);
                    pos_y_spin->setValue(pos_value);
                }
                QSignalBlocker block_slider(pos_y_slider);
                pos_y_slider->setValue((int)std::lround(pos_value * 10.0));
            }
            controller_transforms[ctrl_row]->transform.position.y = pos_value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.position.y = (float)pos_value;
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
            pos.y = pos_value;
            ref_point->SetPosition(pos);
            viewport->update();
        }
    });
    position_layout->addWidget(pos_y_slider, 1, 1);

    pos_y_spin = new QDoubleSpinBox();
    pos_y_spin->setRange(-500, 500);  // Allow negative for outside-room LEDs, up to 500 grid units
    pos_y_spin->setDecimals(1);
    pos_y_spin->setMaximumWidth(80);
    connect(pos_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        if(pos_y_slider)
        {
            QSignalBlocker block_slider(pos_y_slider);
            pos_y_slider->setValue((int)std::lround(value * 10.0));
        }

        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            if(value < 0.0)
            {
                value = 0.0;
                if(pos_y_spin)
                {
                    QSignalBlocker block_spin(pos_y_spin);
                    pos_y_spin->setValue(value);
                }
                if(pos_y_slider)
                {
                    QSignalBlocker block_slider(pos_y_slider);
                    pos_y_slider->setValue((int)std::lround(value * 10.0));
                }
            }
            controller_transforms[ctrl_row]->transform.position.y = value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.position.y = (float)value;
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
            pos.y = value;
            ref_point->SetPosition(pos);
            viewport->update();
        }
    });
    position_layout->addWidget(pos_y_spin, 1, 2);

    position_layout->addWidget(new QLabel("Position Z:"), 2, 0);

    pos_z_slider = new QSlider(Qt::Horizontal);
    pos_z_slider->setRange(-5000, 5000);
    pos_z_slider->setValue(0);
    connect(pos_z_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        if(pos_z_spin)
        {
            QSignalBlocker block_spin(pos_z_spin);
            pos_z_spin->setValue(pos_value);
        }

        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.position.z = pos_value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.position.z = (float)pos_value;
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
            pos.z = pos_value;
            ref_point->SetPosition(pos);
            viewport->update();
        }
    });
    position_layout->addWidget(pos_z_slider, 2, 1);

    pos_z_spin = new QDoubleSpinBox();
    pos_z_spin->setRange(-500, 500);  // Allow negative for outside-room LEDs, up to 500 grid units
    pos_z_spin->setDecimals(1);
    pos_z_spin->setMaximumWidth(80);
    connect(pos_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        if(pos_z_slider)
        {
            QSignalBlocker block_slider(pos_z_slider);
            pos_z_slider->setValue((int)std::lround(value * 10.0));
        }

        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.position.z = value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.position.z = (float)value;
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
            pos.z = value;
            ref_point->SetPosition(pos);
            viewport->update();
        }
    });
    position_layout->addWidget(pos_z_spin, 2, 2);

    position_layout->addWidget(new QLabel("Rotation X:"), 3, 0);

    rot_x_slider = new QSlider(Qt::Horizontal);
    rot_x_slider->setRange(-180, 180);
    rot_x_slider->setValue(0);
    connect(rot_x_slider, &QSlider::valueChanged, [this](int value) {
        double rot_value = value;
        if(rot_x_spin)
        {
            QSignalBlocker block_spin(rot_x_spin);
            rot_x_spin->setValue(rot_value);
        }

        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.x = rot_value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.rotation.x = (float)rot_value;
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
            rot.x = rot_value;
            ref_point->SetRotation(rot);
            viewport->update();
        }
    });
    position_layout->addWidget(rot_x_slider, 3, 1);

    rot_x_spin = new QDoubleSpinBox();
    rot_x_spin->setRange(-180, 180);
    rot_x_spin->setDecimals(1);
    rot_x_spin->setMaximumWidth(80);
    connect(rot_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        if(rot_x_slider)
        {
            QSignalBlocker block_slider(rot_x_slider);
            rot_x_slider->setValue((int)std::lround(value));
        }

        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.x = value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.rotation.x = (float)value;
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
            rot.x = value;
            ref_point->SetRotation(rot);
            viewport->update();
        }
    });
    position_layout->addWidget(rot_x_spin, 3, 2);

    position_layout->addWidget(new QLabel("Rotation Y:"), 4, 0);

    rot_y_slider = new QSlider(Qt::Horizontal);
    rot_y_slider->setRange(-180, 180);
    rot_y_slider->setValue(0);
    connect(rot_y_slider, &QSlider::valueChanged, [this](int value) {
        double rot_value = value;
        if(rot_y_spin)
        {
            QSignalBlocker block_spin(rot_y_spin);
            rot_y_spin->setValue(rot_value);
        }

        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.y = rot_value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.rotation.y = (float)rot_value;
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
            rot.y = rot_value;
            ref_point->SetRotation(rot);
            viewport->update();
        }
    });
    position_layout->addWidget(rot_y_slider, 4, 1);

    rot_y_spin = new QDoubleSpinBox();
    rot_y_spin->setRange(-180, 180);
    rot_y_spin->setDecimals(1);
    rot_y_spin->setMaximumWidth(80);
    connect(rot_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        if(rot_y_slider)
        {
            QSignalBlocker block_slider(rot_y_slider);
            rot_y_slider->setValue((int)std::lround(value));
        }

        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.y = value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.rotation.y = (float)value;
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
            rot.y = value;
            ref_point->SetRotation(rot);
            viewport->update();
        }
    });
    position_layout->addWidget(rot_y_spin, 4, 2);

    position_layout->addWidget(new QLabel("Rotation Z:"), 5, 0);

    rot_z_slider = new QSlider(Qt::Horizontal);
    rot_z_slider->setRange(-180, 180);
    rot_z_slider->setValue(0);
    connect(rot_z_slider, &QSlider::valueChanged, [this](int value) {
        double rot_value = value;
        if(rot_z_spin)
        {
            QSignalBlocker block_spin(rot_z_spin);
            rot_z_spin->setValue(rot_value);
        }

        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.z = rot_value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.rotation.z = (float)rot_value;
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
            rot.z = rot_value;
            ref_point->SetRotation(rot);
            viewport->update();
        }
    });
    position_layout->addWidget(rot_z_slider, 5, 1);

    rot_z_spin = new QDoubleSpinBox();
    rot_z_spin->setRange(-180, 180);
    rot_z_spin->setDecimals(1);
    rot_z_spin->setMaximumWidth(80);
    connect(rot_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        if(rot_z_slider)
        {
            QSignalBlocker block_slider(rot_z_slider);
            rot_z_slider->setValue((int)std::lround(value));
        }

        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.z = value;
            viewport->NotifyControllerTransformChanged();
            emit GridLayoutChanged();
            return;
        }

        if(current_display_plane_index >= 0 && current_display_plane_index < (int)display_planes.size())
        {
            DisplayPlane3D* plane = display_planes[current_display_plane_index].get();
            if(plane)
            {
                Transform3D& transform = plane->GetTransform();
                transform.rotation.z = (float)value;
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
            rot.z = value;
            ref_point->SetRotation(rot);
            viewport->update();
        }
    });
    position_layout->addWidget(rot_z_spin, 5, 2);

    transform_tab->setLayout(position_layout);

    settings_tabs->addTab(transform_tab, "Position & Rotation");
    settings_tabs->addTab(grid_settings_tab, "Grid Settings");

    /*---------------------------------------------------------*\
    | Object Creator Tab (Custom Controllers, Ref Points, Displays) |
    \*---------------------------------------------------------*/
    QWidget* object_creator_tab = new QWidget();
    QVBoxLayout* creator_layout = new QVBoxLayout();
    creator_layout->setSpacing(10);

    // Type selector dropdown
    QLabel* type_label = new QLabel("Object Type:");
    type_label->setStyleSheet("font-weight: bold;");
    creator_layout->addWidget(type_label);

    QComboBox* object_type_combo = new QComboBox();
    object_type_combo->addItem("Select to Create...", -1);
    object_type_combo->addItem("Custom Controller", 0);
    object_type_combo->addItem("Reference Point", 1);
    object_type_combo->addItem("Display Plane", 2);
    creator_layout->addWidget(object_type_combo);

    object_creator_status_label = new QLabel();
    object_creator_status_label->setWordWrap(true);
    object_creator_status_label->setVisible(false);
    creator_layout->addWidget(object_creator_status_label);

    // Stacked widget for dynamic UI
    QStackedWidget* creator_stack = new QStackedWidget();

    // Page 0: Empty placeholder
    QWidget* empty_page = new QWidget();
    QVBoxLayout* empty_layout = new QVBoxLayout(empty_page);
    QLabel* empty_label = new QLabel("Select an object type from the dropdown above to begin creating custom objects.");
    empty_label->setWordWrap(true);
    empty_label->setStyleSheet("color: #888; font-style: italic; padding: 20px;");
    empty_layout->addWidget(empty_label);
    empty_layout->addStretch();
    creator_stack->addWidget(empty_page);

    /*---------------------------------------------------------*\
    | Custom Controllers Page                                  |
    \*---------------------------------------------------------*/
    QWidget* custom_controller_page = new QWidget();
    QVBoxLayout* custom_layout = new QVBoxLayout(custom_controller_page);
    custom_layout->setSpacing(5);

    QLabel* custom_list_label = new QLabel("Available Custom Controllers:");
    custom_list_label->setStyleSheet("font-weight: bold;");
    custom_layout->addWidget(custom_list_label);

    custom_controllers_list = new QListWidget();
    custom_controllers_list->setMinimumHeight(150);
    custom_controllers_list->setToolTip("Select a custom controller to edit or export");
    custom_layout->addWidget(custom_controllers_list);

    QPushButton* custom_controller_button = new QPushButton("Create New Custom Controller");
    connect(custom_controller_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_create_custom_controller_clicked);
    custom_layout->addWidget(custom_controller_button);

    QHBoxLayout* custom_io_layout = new QHBoxLayout();
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

    custom_layout->addLayout(custom_io_layout);
    custom_layout->addStretch();

    creator_stack->addWidget(custom_controller_page);

    /*---------------------------------------------------------*\
    | Reference Points Page                                    |
    \*---------------------------------------------------------*/
    QWidget* ref_point_page = new QWidget();
    QVBoxLayout* ref_points_layout = new QVBoxLayout(ref_point_page);
    ref_points_layout->setSpacing(5);

    reference_points_list = new QListWidget();
    reference_points_list->setMinimumHeight(150);
    connect(reference_points_list, &QListWidget::currentRowChanged, this, &OpenRGB3DSpatialTab::on_ref_point_selected);
    ref_points_layout->addWidget(reference_points_list);

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
    selected_ref_point_color = 0x00808080; // Default gray
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
    help_label->setStyleSheet("color: gray; font-size: 10px;");
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

    /*---------------------------------------------------------*\
    | Display Planes Page                                      |
    \*---------------------------------------------------------*/
    QWidget* display_plane_page = new QWidget();
    QVBoxLayout* display_layout = new QVBoxLayout(display_plane_page);
    display_layout->setSpacing(5);

    display_planes_list = new QListWidget();
    display_planes_list->setMinimumHeight(150);
    connect(display_planes_list, &QListWidget::currentRowChanged, this, &OpenRGB3DSpatialTab::on_display_plane_selected);
    display_layout->addWidget(display_planes_list);

    QHBoxLayout* display_buttons = new QHBoxLayout();
    add_display_plane_button = new QPushButton("Add Display");
    connect(add_display_plane_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_add_display_plane_clicked);
    display_buttons->addWidget(add_display_plane_button);

    remove_display_plane_button = new QPushButton("Remove");
    remove_display_plane_button->setEnabled(false);
    connect(remove_display_plane_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_remove_display_plane_clicked);
    display_buttons->addWidget(remove_display_plane_button);

    display_layout->addLayout(display_buttons);

    QGridLayout* plane_form = new QGridLayout();
    plane_form->setColumnStretch(1, 1);

    int plane_row = 0;
    plane_form->addWidget(new QLabel("Name:"), plane_row, 0);
    display_plane_name_edit = new QLineEdit();
    connect(display_plane_name_edit, &QLineEdit::textEdited, this, &OpenRGB3DSpatialTab::on_display_plane_name_edited);
    plane_form->addWidget(display_plane_name_edit, plane_row, 1, 1, 2);
    plane_row++;

    plane_form->addWidget(new QLabel("Monitor Preset:"), plane_row, 0);
    display_plane_monitor_combo = new QComboBox();
    display_plane_monitor_combo->setEditable(true);
    display_plane_monitor_combo->setInsertPolicy(QComboBox::NoInsert);
    display_plane_monitor_combo->setPlaceholderText("Search brand or model...");
    display_plane_monitor_combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
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
    connect(display_plane_capture_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_display_plane_capture_changed(int)));
    plane_form->addWidget(display_plane_capture_combo, plane_row, 1, 1, 2);

    display_plane_refresh_capture_btn = new QPushButton("Refresh");
    display_plane_refresh_capture_btn->setToolTip("Refresh list of available capture sources");
    connect(display_plane_refresh_capture_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_display_plane_refresh_capture_clicked);
    plane_form->addWidget(display_plane_refresh_capture_btn, plane_row, 3);
    plane_row++;

    display_layout->addLayout(plane_form);

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
    display_layout->addWidget(display_plane_visible_check);

    display_layout->addStretch();

    creator_stack->addWidget(display_plane_page);

    creator_layout->addWidget(creator_stack);
    creator_layout->addStretch();

    // Connect dropdown to switch pages
    connect(object_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [creator_stack](int index) {
        if(index == 0) creator_stack->setCurrentIndex(0); // Select to Create
        else if(index == 1) creator_stack->setCurrentIndex(1); // Custom Controller
        else if(index == 2) creator_stack->setCurrentIndex(2); // Reference Point
        else if(index == 3) creator_stack->setCurrentIndex(3); // Display Plane
    });
    object_type_combo->setCurrentIndex(0);

    object_creator_tab->setLayout(creator_layout);
    settings_tabs->addTab(object_creator_tab, "Object Creator");

    LoadMonitorPresets();

    // Initialize capture source list for display planes page
    RefreshDisplayPlaneCaptureSourceList();

    /*---------------------------------------------------------*\
    | Unified Profiles Tab (Layout + Effect profiles)         |
    \*---------------------------------------------------------*/
    SetupProfilesTab(settings_tabs);

    middle_panel->addWidget(settings_tabs);

    main_layout->addLayout(middle_panel, 3);  // Give middle panel more space

    /*---------------------------------------------------------*\
    | Effects Tab (Effect Controls and Presets)                |
    | Created first so it's the default tab on startup         |
    \*---------------------------------------------------------*/
    QWidget* effects_tab = new QWidget();
    QVBoxLayout* effects_tab_layout = new QVBoxLayout(effects_tab);
    effects_tab_layout->setContentsMargins(8, 8, 8, 8);
    effects_tab_layout->setSpacing(8);

    QScrollArea* effects_scroll = new QScrollArea();
    effects_scroll->setWidgetResizable(true);
    effects_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    effects_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget* effects_content = new QWidget();
    QVBoxLayout* right_panel = new QVBoxLayout(effects_content);

    /*---------------------------------------------------------*\
    | Right Tab Widget (Effects and Zones)                     |
    \*---------------------------------------------------------*/
    QTabWidget* right_tabs = new QTabWidget();

    /*---------------------------------------------------------*\
    | Effects Sub-Tab                                          |
    \*---------------------------------------------------------*/
    QWidget* effects_subtab = new QWidget();
    QVBoxLayout* effects_layout = new QVBoxLayout();

    // Effect selection
    effect_combo = new QComboBox();
    effect_combo->blockSignals(true);  // Block signals during initialization
    // Populate effect combo (will be updated after loading presets)
    UpdateEffectCombo();
    effect_combo->blockSignals(false); // Re-enable signals after initialization

    connect(effect_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_effect_changed);
    

    effects_layout->addWidget(new QLabel("Effect:"));
    effects_layout->addWidget(effect_combo);

    // Zone selector
    effects_layout->addWidget(new QLabel("Zone:"));
    effect_zone_combo = new QComboBox();
    effect_zone_combo->addItem("All Controllers");
    effects_layout->addWidget(effect_zone_combo);

    // Effect origin selector
    effects_layout->addWidget(new QLabel("Origin:"));
    effect_origin_combo = new QComboBox();
    effect_origin_combo->addItem("Room Center", QVariant(-1)); // -1 = no reference point (uses calculated room center)
    connect(effect_origin_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_effect_origin_changed);
    effects_layout->addWidget(effect_origin_combo);

    // Effect-specific controls container
    effect_controls_widget = new QWidget();
    effect_controls_layout = new QVBoxLayout();
    effect_controls_widget->setLayout(effect_controls_layout);
    effects_layout->addWidget(effect_controls_widget);

    effects_layout->addStretch();
    effects_subtab->setLayout(effects_layout);
    right_tabs->addTab(effects_subtab, "Effects");

    /*---------------------------------------------------------*\
    | Audio Tab                                                |
    \*---------------------------------------------------------*/
    SetupAudioTab(right_tabs);

    /*---------------------------------------------------------*\
    | Force layout update to prevent crash when selecting      |
    | effects before switching tabs                             |
    \*---------------------------------------------------------*/
    effect_controls_widget->updateGeometry();
    effects_tab->updateGeometry();

    /*---------------------------------------------------------*\
    | Effect Stack Tab (setup in separate function)            |
    \*---------------------------------------------------------*/
    SetupEffectStackTab(right_tabs);

    /*---------------------------------------------------------*\
    | Zones Tab                                                |
    \*---------------------------------------------------------*/
    QWidget* zones_tab = new QWidget();
    QVBoxLayout* zones_layout = new QVBoxLayout();
    zones_layout->setSpacing(5);

    // List of zones
    zones_list = new QListWidget();
    zones_list->setMinimumHeight(200);
    connect(zones_list, &QListWidget::currentRowChanged, this, &OpenRGB3DSpatialTab::on_zone_selected);
    zones_layout->addWidget(zones_list);

    // Help text
    QLabel* zones_help_label = new QLabel("Zones are groups of controllers for targeting effects.\n\nCreate zones like 'Desk', 'Front Wall', 'Ceiling', etc., then select them when configuring effects.");
    zones_help_label->setStyleSheet("color: gray; font-size: 10px;");
    zones_help_label->setWordWrap(true);
    zones_layout->addWidget(zones_help_label);

    // Create/Edit/Delete buttons
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

    zones_tab->setLayout(zones_layout);
    right_tabs->addTab(zones_tab, "Zones");

    // Add tabs to right panel
    right_panel->addWidget(right_tabs);

    /*---------------------------------------------------------*\
    | Add stretch to push content to top of right panel       |
    \*---------------------------------------------------------*/
    right_panel->addStretch();

    /*---------------------------------------------------------*\
    | Configure effects scroll area and add to tab             |
    \*---------------------------------------------------------*/
    effects_scroll->setMinimumWidth(400);
    effects_scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    effects_scroll->setWidget(effects_content);
    effects_tab_layout->addWidget(effects_scroll);

    /*---------------------------------------------------------*\
    | Add tabs to main tab widget (Effects first as default)  |
    \*---------------------------------------------------------*/
    main_tabs->addTab(effects_tab, "Effects / Presets");
    main_tabs->addTab(setup_tab, "Setup / Grid");

    /*---------------------------------------------------------*\
    | Set the root layout                                      |
    \*---------------------------------------------------------*/
    setLayout(root_layout);
}





void OpenRGB3DSpatialTab::on_effect_type_changed(int index)
{
    /*---------------------------------------------------------*\
    | Clear current custom UI                                  |
    \*---------------------------------------------------------*/
    ClearCustomEffectUI();

    /*---------------------------------------------------------*\
    | Set up new custom UI based on effect type               |
    \*---------------------------------------------------------*/
    SetupCustomEffectUI(index);
}

void OpenRGB3DSpatialTab::SetupCustomEffectUI(int effect_type)
{
    /*---------------------------------------------------------*\
    | Validate all required UI components                      |
    \*---------------------------------------------------------*/
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

    

    /*---------------------------------------------------------*\
    | Map effect type index to effect class name              |
    \*---------------------------------------------------------*/
    const char* effect_names[] = {
        "Wave3D",           // 0
        "Wipe3D",           // 1
        "Plasma3D",         // 2
        "Spiral3D",         // 3
        "Spin3D",           // 4
        "DNAHelix3D",       // 5
        "BreathingSphere3D",// 6
        "Explosion3D",      // 7
        "Rain3D",           // 8
        "Tornado3D",        // 9
        "Lightning3D",      // 10
        "Matrix3D",         // 11
        "BouncingBall3D",   // 12
        "AudioLevel3D",     // 13
        "SpectrumBars3D",   // 14
        "BeatPulse3D",      // 15
        "BandScan3D",       // 16
        "ScreenMirror3D"    // 17
    };
    const int num_effects = sizeof(effect_names) / sizeof(effect_names[0]);

    if(effect_type < 0 || effect_type >= num_effects)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Invalid effect type: %d", effect_type);
        return;
    }

    /*---------------------------------------------------------*\
    | Create effect using the registration system             |
    \*---------------------------------------------------------*/
    
    SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(effect_names[effect_type]);
    if(!effect)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create effect: %s", effect_names[effect_type]);
        return;
    }
    

    /*---------------------------------------------------------*\
    | Setup effect UI (same for ALL effects)                  |
    \*---------------------------------------------------------*/
    effect->setParent(effect_controls_widget);
    effect->CreateCommonEffectControls(effect_controls_widget);
    effect->SetupCustomUI(effect_controls_widget);
    current_effect_ui = effect;

    // Set reference points for ScreenMirror3D UI effect
    if (std::string(effect_names[effect_type]) == "ScreenMirror3D")
    {
        ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(effect);
        if (screen_mirror)
        {
            screen_mirror->SetReferencePoints(&reference_points);
        }
    }

    /*---------------------------------------------------------*\
    | Get and connect buttons                                  |
    \*---------------------------------------------------------*/
    start_effect_button = effect->GetStartButton();
    stop_effect_button = effect->GetStopButton();
    connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
    connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
    connect(effect, &SpatialEffect3D::ParametersChanged, [this]() {});

    /*---------------------------------------------------------*\
    | Add effect to layout                                     |
    \*---------------------------------------------------------*/
    effect_controls_layout->addWidget(effect);

    /*---------------------------------------------------------*\
    | Force geometry update to ensure layout is ready         |
    \*---------------------------------------------------------*/
    effect_controls_widget->updateGeometry();
    effect_controls_widget->update();
}

void OpenRGB3DSpatialTab::SetupStackPresetUI()
{
    /*---------------------------------------------------------*\
    | Validate required UI components                          |
    \*---------------------------------------------------------*/
    if(!effect_controls_widget || !effect_controls_layout)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect controls widget or layout is null!");
        return;
    }

    

    /*---------------------------------------------------------*\
    | Create informational label                               |
    \*---------------------------------------------------------*/
    QLabel* info_label = new QLabel(
        "This is a saved stack preset with pre-configured settings.\n\n"
        "Click Start to load and run all effects in this preset.\n\n"
        "To edit this preset, go to the Effect Stack tab, load it,\n"
        "modify the effects, and save with the same name."
    );
    info_label->setWordWrap(true);
    info_label->setStyleSheet(
        "QLabel {"
        "    padding: 10px;"
        "    background-color: #2a2a2a;"
        "    border: 1px solid #444;"
        "    border-radius: 4px;"
        "    color: #ccc;"
        "}"
    );
    effect_controls_layout->addWidget(info_label);

    /*---------------------------------------------------------*\
    | Create Start/Stop button container                       |
    \*---------------------------------------------------------*/
    QWidget* button_container = new QWidget();
    QHBoxLayout* button_layout = new QHBoxLayout(button_container);
    button_layout->setContentsMargins(0, 10, 0, 0);

    start_effect_button = new QPushButton("Start Effect");
    stop_effect_button = new QPushButton("Stop Effect");
    stop_effect_button->setEnabled(false);

    button_layout->addWidget(start_effect_button);
    button_layout->addWidget(stop_effect_button);
    button_layout->addStretch();

    effect_controls_layout->addWidget(button_container);

    /*---------------------------------------------------------*\
    | Connect buttons to handlers                              |
    \*---------------------------------------------------------*/
    connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
    connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);

    /*---------------------------------------------------------*\
    | Force geometry update                                    |
    \*---------------------------------------------------------*/
    effect_controls_widget->updateGeometry();
    effect_controls_widget->update();
}

void OpenRGB3DSpatialTab::ClearCustomEffectUI()
{
    if(!effect_controls_layout)
    {
        return;
    }

    /*---------------------------------------------------------*\
    | Stop timer to prevent callbacks during cleanup           |
    \*---------------------------------------------------------*/
    if(effect_timer && effect_timer->isActive())
    {
        effect_timer->stop();
    }
    effect_running = false;

    /*---------------------------------------------------------*\
    | Disconnect current effect to prevent crashes             |
    \*---------------------------------------------------------*/
    if(current_effect_ui)
    {
        disconnect(current_effect_ui, nullptr, this, nullptr);
    }

    /*---------------------------------------------------------*\
    | Disconnect buttons before deletion                       |
    \*---------------------------------------------------------*/
    if(start_effect_button)
    {
        disconnect(start_effect_button, nullptr, this, nullptr);
    }
    if(stop_effect_button)
    {
        disconnect(stop_effect_button, nullptr, this, nullptr);
    }

    /*---------------------------------------------------------*\
    | Reset effect UI pointers BEFORE deletion                 |
    \*---------------------------------------------------------*/
    current_effect_ui = nullptr;
    start_effect_button = nullptr;
    stop_effect_button = nullptr;

    /*---------------------------------------------------------*\
    | Remove all widgets from the container                    |
    \*---------------------------------------------------------*/
    QLayoutItem* item;
    while((item = effect_controls_layout->takeAt(0)) != nullptr)
    {
        if(item->widget())
        {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void OpenRGB3DSpatialTab::on_grid_dimensions_changed()
{
    /*---------------------------------------------------------*\
    | Update grid dimensions from UI                           |
    \*---------------------------------------------------------*/
    if(grid_x_spin) custom_grid_x = grid_x_spin->value();
    if(grid_y_spin) custom_grid_y = grid_y_spin->value();
    if(grid_z_spin) custom_grid_z = grid_z_spin->value();

    /*---------------------------------------------------------*\
    | Regenerate LED positions for all controllers            |
    | Must respect granularity and LED spacing!               |
    \*---------------------------------------------------------*/
    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        RegenerateLEDPositions(controller_transforms[i].get());
    }

    /*---------------------------------------------------------*\
    | Update viewport                                          |
    \*---------------------------------------------------------*/
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

    if(selected.empty())
    {
        selection_info_label->setText("No selection");
        selection_info_label->setStyleSheet("color: gray; font-size: 10px; font-weight: bold;");
    }
    else if(selected.size() == 1)
    {
        selection_info_label->setText(QString("Selected: 1 controller"));
        selection_info_label->setStyleSheet("color: #ffaa00; font-size: 10px; font-weight: bold;");
    }
    else
    {
        selection_info_label->setText(QString("Selected: %1 controllers").arg(selected.size()));
        selection_info_label->setStyleSheet("color: #ffaa00; font-size: 10px; font-weight: bold;");
    }
}

/*---------------------------------------------------------*\
| User Position Control Functions                          |
\*---------------------------------------------------------*/
/* Legacy user position control functions - REMOVED
 * User position is now handled through the reference points system
 * as a REF_POINT_USER type reference point.
 */

void OpenRGB3DSpatialTab::on_effect_changed(int index)
{
    

    /*---------------------------------------------------------*\
    | Validate index range                                     |
    \*---------------------------------------------------------*/
    if(index < 0)
    {
        
        return;
    }

    /*---------------------------------------------------------*\
    | Remember if effect was running so we can restart it      |
    \*---------------------------------------------------------*/
    bool was_running = effect_running;
    

    /*---------------------------------------------------------*\
    | Stop effect timer and set flag BEFORE clearing UI        |
    \*---------------------------------------------------------*/
    if(effect_running && effect_timer)
    {
        effect_running = false;
        effect_timer->stop();
    }

    /*---------------------------------------------------------*\
    | Update button states if they exist                       |
    \*---------------------------------------------------------*/
    if(start_effect_button)
    {
        start_effect_button->setEnabled(true);
    }
    if(stop_effect_button)
    {
        stop_effect_button->setEnabled(false);
    }

    /*---------------------------------------------------------*\
    | Clear current effect UI (disconnects and deletes)        |
    \*---------------------------------------------------------*/
    ClearCustomEffectUI();

    /*---------------------------------------------------------*\
    | Set up new effect UI based on selection                  |
    \*---------------------------------------------------------*/
    if(index > 0)  // Skip "None" option
    {
        // Check if this is a stack preset (has user data)
        QVariant data = effect_combo->itemData(index);

        if(data.isValid() && data.toInt() < 0)
        {
            
            // This is a stack preset - show simplified UI
            SetupStackPresetUI();

            /*---------------------------------------------------------*\
            | Disable zone and origin combos - stack has own settings |
            \*---------------------------------------------------------*/
            if(effect_zone_combo)
            {
                effect_zone_combo->setEnabled(false);
            }
            if(effect_origin_combo)
            {
                effect_origin_combo->setEnabled(false);
            }
        }
        else
        {
            
            // This is a regular effect
            SetupCustomEffectUI(index - 1);  // Adjust for "None" offset

            /*---------------------------------------------------------*\
            | Enable zone and origin combos for regular effects       |
            \*---------------------------------------------------------*/
            if(effect_zone_combo)
            {
                effect_zone_combo->setEnabled(true);
            }
            if(effect_origin_combo)
            {
                effect_origin_combo->setEnabled(true);
            }
        }

        /*---------------------------------------------------------*\
        | If effect was running, automatically start the new one   |
        \*---------------------------------------------------------*/
        if(was_running)
        {
            
            on_start_effect_clicked();
        }
    }
    else
    {
        

        /*---------------------------------------------------------*\
        | Enable zone and origin combos when no effect selected   |
        \*---------------------------------------------------------*/
        if(effect_zone_combo)
        {
            effect_zone_combo->setEnabled(true);
        }
        if(effect_origin_combo)
        {
            effect_origin_combo->setEnabled(true);
        }
    }
}



void OpenRGB3DSpatialTab::UpdateEffectOriginCombo()
{
    if(!effect_origin_combo) return;

    effect_origin_combo->blockSignals(true);
    effect_origin_combo->clear();

    // Always add "Room Center" as first option (calculated center, not 0,0,0)
    effect_origin_combo->addItem("Room Center", QVariant(-1));

    // Add all reference points
    for(size_t i = 0; i < reference_points.size(); i++)
    {
        VirtualReferencePoint3D* ref_point = reference_points[i].get();
        if(!ref_point) continue; // Skip null pointers

        QString name = QString::fromStdString(ref_point->GetName());
        QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
        QString display = QString("%1 (%2)").arg(name).arg(type);
        effect_origin_combo->addItem(display, QVariant((int)i));
    }

    effect_origin_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::UpdateEffectCombo()
{
    if(!effect_combo) return;

    effect_combo->blockSignals(true);
    effect_combo->clear();

    // Add "None" option first
    effect_combo->addItem("None");  // index 0

    // Add all individual effects
    effect_combo->addItem("Wave 3D");          // index 1 (effect_type 0)
    effect_combo->addItem("Wipe 3D");          // index 2 (effect_type 1)
    effect_combo->addItem("Plasma 3D");        // index 3 (effect_type 2)
    effect_combo->addItem("Spiral 3D");        // index 4 (effect_type 3)
    effect_combo->addItem("Spin 3D");          // index 5 (effect_type 4)
    effect_combo->addItem("DNA Helix 3D");     // index 6 (effect_type 5)
    effect_combo->addItem("Breathing Sphere 3D"); // index 7 (effect_type 6)
    effect_combo->addItem("Explosion 3D");     // index 8 (effect_type 7)
    effect_combo->addItem("Rain 3D");          // index 9 (effect_type 8)
    effect_combo->addItem("Tornado 3D");       // index 10 (effect_type 9)
    effect_combo->addItem("Lightning 3D");     // index 11 (effect_type 10)
    effect_combo->addItem("Matrix 3D");        // index 12 (effect_type 11)
    effect_combo->addItem("Bouncing Ball 3D"); // index 13 (effect_type 12)
    effect_combo->addItem("Audio Level 3D");   // index 14 (effect_type 13)
    effect_combo->addItem("Spectrum Bars 3D"); // index 15 (effect_type 14)
    effect_combo->addItem("Beat Pulse 3D");    // index 16 (effect_type 15)
    effect_combo->addItem("Band Scan 3D");     // index 17 (effect_type 16)
    effect_combo->addItem("Screen Mirror 3D"); // index 18 (effect_type 17)

    // Add stack presets with [Stack] suffix
    // Store negative indices to distinguish presets from effects
    for(size_t i = 0; i < stack_presets.size(); i++)
    {
        QString preset_name = QString::fromStdString(stack_presets[i]->name) + " [Stack]";
        effect_combo->addItem(preset_name);
        // Store preset index as user data (negative to distinguish from effect type)
        effect_combo->setItemData(effect_combo->count() - 1, QVariant(-(int)i - 1));
    }

    effect_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::on_effect_origin_changed(int index)
{
    // Get the selected reference point index (-1 means room center)
    int ref_point_idx = effect_origin_combo->itemData(index).toInt();

    // Update the current effect with the new origin
    // All effects will use this selected origin point
    Vector3D origin = {0.0f, 0.0f, 0.0f};

    if(ref_point_idx >= 0 && ref_point_idx < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref_point = reference_points[ref_point_idx].get();
        origin = ref_point->GetPosition();
    }

    // Update current effect instance
    if(current_effect_ui)
    {
        current_effect_ui->SetCustomReferencePoint(origin);
    }

    // Trigger viewport update
    if(viewport) viewport->UpdateColors();
}

/*---------------------------------------------------------*\
| Background Effect Worker Thread Implementation           |
\*---------------------------------------------------------*/

EffectWorkerThread3D::EffectWorkerThread3D(QObject* parent)
    : QThread(parent), effect(nullptr), active_zone(-1)
{
}

EffectWorkerThread3D::~EffectWorkerThread3D()
{
    StopEffect();
    quit();
    wait();
}

void EffectWorkerThread3D::StartEffect(
    SpatialEffect3D* eff,
    const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
    const std::vector<std::unique_ptr<VirtualReferencePoint3D>>& ref_points,
    ZoneManager3D* zone_mgr,
    int active_zone_idx)
{
    QMutexLocker locker(&state_mutex);

    effect = eff;
    active_zone = active_zone_idx;

    // Create snapshots of transforms
    transform_snapshots.clear();
    for(size_t i = 0; i < transforms.size(); i++)
    {
        std::unique_ptr<ControllerTransform> snapshot = std::make_unique<ControllerTransform>();
        snapshot->controller = transforms[i]->controller;
        snapshot->virtual_controller = transforms[i]->virtual_controller;
        snapshot->transform = transforms[i]->transform;
        snapshot->led_positions = transforms[i]->led_positions;
        snapshot->world_positions_dirty = false;
        transform_snapshots.push_back(std::move(snapshot));
    }

    // Create snapshots of reference points
    ref_point_snapshots.clear();
    for(size_t i = 0; i < ref_points.size(); i++)
    {
        const std::unique_ptr<VirtualReferencePoint3D>& ref_point = ref_points[i];
        std::unique_ptr<VirtualReferencePoint3D> snapshot = std::make_unique<VirtualReferencePoint3D>(
            ref_point->GetName(),
            ref_point->GetType(),
            ref_point->GetPosition().x,
            ref_point->GetPosition().y,
            ref_point->GetPosition().z
        );
        snapshot->SetDisplayColor(ref_point->GetDisplayColor());
        ref_point_snapshots.push_back(std::move(snapshot));
    }

    // Create zone manager snapshot
    if(zone_mgr)
    {
        zone_snapshot = std::make_unique<ZoneManager3D>();
        // Copy zones from zone_mgr to zone_snapshot
        for(int i = 0; i < zone_mgr->GetZoneCount(); i++)
        {
            Zone3D* zone = zone_mgr->GetZone(i);
            if(zone)
            {
                Zone3D* new_zone = zone_snapshot->CreateZone(zone->GetName());
                if(new_zone)
                {
                    // Copy all controllers from the original zone
                    const std::vector<int>& controllers = zone->GetControllers();
                    for(size_t j = 0; j < controllers.size(); j++)
                    {
                        new_zone->AddController(controllers[j]);
                    }
                }
            }
        }
    }

    should_stop = false;
    running = true;

    if(!isRunning())
    {
        start();
    }

    start_condition.wakeOne();
}

void EffectWorkerThread3D::StopEffect()
{
    should_stop = true;
    running = false;
    start_condition.wakeOne();
}

void EffectWorkerThread3D::UpdateTime(float time)
{
    current_time = time;
}

bool EffectWorkerThread3D::GetColors(
    std::vector<RGBColor>& out_colors,
    std::vector<LEDPosition3D*>& out_leds)
{
    QMutexLocker locker(&buffer_mutex);

    if(front_buffer.colors.empty())
    {
        return false;
    }

    out_colors = front_buffer.colors;
    out_leds = front_buffer.leds;

    return true;
}

void EffectWorkerThread3D::run()
{
    while(!should_stop)
    {
        QMutexLocker locker(&state_mutex);

        if(!running)
        {
            start_condition.wait(&state_mutex);
            continue;
        }

        if(!effect || transform_snapshots.empty())
        {
            msleep(16); // ~60 FPS
            continue;
        }

        locker.unlock();

        // Calculate colors on background thread
        std::vector<RGBColor> colors;
        std::vector<LEDPosition3D*> leds;

        float time = current_time.load();

        // Calculate colors for all LEDs
        for(size_t i = 0; i < transform_snapshots.size(); i++)
        {
            ControllerTransform* transform = transform_snapshots[i].get();
            for(size_t j = 0; j < transform->led_positions.size(); j++)
            {
                LEDPosition3D* led_pos = &transform->led_positions[j];
                RGBColor color = effect->CalculateColor(
                    led_pos->world_position.x,
                    led_pos->world_position.y,
                    led_pos->world_position.z,
                    time
                );

                colors.push_back(color);
                leds.push_back(led_pos);
            }
        }

        // Swap buffers
        {
            QMutexLocker buffer_locker(&buffer_mutex);
            back_buffer.colors = std::move(colors);
            back_buffer.leds = std::move(leds);
            std::swap(front_buffer, back_buffer);
        }

        emit ColorsReady();

        msleep(33); // ~30 FPS
    }
}

void OpenRGB3DSpatialTab::ApplyColorsFromWorker()
{
    if(!worker_thread) return;

    std::vector<RGBColor> colors;
    std::vector<LEDPosition3D*> leds;

    if(!worker_thread->GetColors(colors, leds))
    {
        return;
    }

    // Apply colors to controllers
    for(size_t i = 0; i < leds.size() && i < colors.size(); i++)
    {
        LEDPosition3D* led = leds[i];
        if(!led || !led->controller) continue;

        if(led->zone_idx >= led->controller->zones.size()) continue;

        unsigned int led_global_idx = led->controller->zones[led->zone_idx].start_idx + led->led_idx;

        if(led_global_idx < led->controller->colors.size())
        {
            led->controller->colors[led_global_idx] = colors[i];
        }
    }

    // Update all controllers
    std::set<RGBController*> updated_controllers;
    for(size_t i = 0; i < leds.size(); i++)
    {
        if(leds[i] && leds[i]->controller)
        {
            if(updated_controllers.find(leds[i]->controller) == updated_controllers.end())
            {
                leds[i]->controller->UpdateLEDs();
                updated_controllers.insert(leds[i]->controller);
            }
        }
    }

    if(viewport)
    {
        viewport->UpdateColors();
    }
}

Vector3D OpenRGB3DSpatialTab::ComputeWorldPositionForSDK(const ControllerTransform* transform, size_t led_idx) const
{
    Vector3D zero{0.0f, 0.0f, 0.0f};
    if(!transform || led_idx >= transform->led_positions.size())
    {
        return zero;
    }

    const LEDPosition3D& led = transform->led_positions[led_idx];
    Vector3D world = transform->world_positions_dirty ?
        ControllerLayout3D::CalculateWorldPosition(led.local_position, transform->transform) :
        led.world_position;

    world.x *= grid_scale_mm;
    world.y *= grid_scale_mm;
    world.z *= grid_scale_mm;
    return world;
}

void OpenRGB3DSpatialTab::ComputeAutoRoomExtents(float& width_mm, float& depth_mm, float& height_mm) const
{
    bool has_leds = false;
    float min_x = 0.0f, max_x = 0.0f;
    float min_y = 0.0f, max_y = 0.0f;
    float min_z = 0.0f, max_z = 0.0f;

    for(const std::unique_ptr<ControllerTransform>& transform_ptr : controller_transforms)
    {
        const ControllerTransform* transform = transform_ptr.get();
        if(!transform) continue;

        for(size_t i = 0; i < transform->led_positions.size(); ++i)
        {
            Vector3D world = ComputeWorldPositionForSDK(transform, i);
            if(!has_leds)
            {
                min_x = max_x = world.x;
                min_y = max_y = world.y;
                min_z = max_z = world.z;
                has_leds = true;
            }
            else
            {
                if(world.x < min_x) min_x = world.x;
                if(world.x > max_x) max_x = world.x;
                if(world.y < min_y) min_y = world.y;
                if(world.y > max_y) max_y = world.y;
                if(world.z < min_z) min_z = world.z;
                if(world.z > max_z) max_z = world.z;
            }
        }
    }

    if(!has_leds)
    {
        width_mm = manual_room_width;
        depth_mm = manual_room_depth;
        height_mm = manual_room_height;
        return;
    }

    width_mm  = std::max(0.0f, max_x - min_x);
    depth_mm  = std::max(0.0f, max_y - min_y);
    height_mm = std::max(0.0f, max_z - min_z);
}

/*---------------------------------------------------------*
| Custom Audio Effects (save/load)                         |
*---------------------------------------------------------*/













 


static inline float mapFalloff(int slider) { return std::max(0.2f, std::min(5.0f, slider / 100.0f)); }







void OpenRGB3DSpatialTab::SDK_GetRoomDimensions(float& w, float& d, float& h, bool& use_manual)
{
    use_manual = use_manual_room_size;
    if(use_manual_room_size)
    {
        w = manual_room_width;
        d = manual_room_depth;
        h = manual_room_height;
        return;
    }

    ComputeAutoRoomExtents(w, d, h);
}

// --- SDK getters ---
bool OpenRGB3DSpatialTab::SDK_GetControllerName(size_t idx, std::string& out) const
{
    if(idx >= controller_transforms.size()) return false;
    ControllerTransform* t = controller_transforms[idx].get();
    if(t && t->controller) out = t->controller->name;
    else if(t && t->virtual_controller) out = std::string("[Virtual] ") + t->virtual_controller->GetName();
    else out = std::string("Controller ") + std::to_string(idx);
    return true;
}

bool OpenRGB3DSpatialTab::SDK_IsControllerVirtual(size_t idx) const
{
    if(idx >= controller_transforms.size()) return false;
    ControllerTransform* t = controller_transforms[idx].get();
    return (t && !t->controller && t->virtual_controller);
}

int OpenRGB3DSpatialTab::SDK_GetControllerGranularity(size_t idx) const
{
    if(idx >= controller_transforms.size()) return 0;
    ControllerTransform* t = controller_transforms[idx].get();
    return t ? t->granularity : 0;
}

int OpenRGB3DSpatialTab::SDK_GetControllerItemIndex(size_t idx) const
{
    if(idx >= controller_transforms.size()) return 0;
    ControllerTransform* t = controller_transforms[idx].get();
    return t ? t->item_idx : 0;
}

size_t OpenRGB3DSpatialTab::SDK_GetLEDCount(size_t ctrl_idx) const
{
    if(ctrl_idx >= controller_transforms.size()) return 0;
    ControllerTransform* t = controller_transforms[ctrl_idx].get();
    return t ? t->led_positions.size() : 0;
}

bool OpenRGB3DSpatialTab::SDK_GetLEDWorldPosition(size_t ctrl_idx, size_t led_idx, float& x, float& y, float& z) const
{
    if(ctrl_idx >= controller_transforms.size()) return false;
    ControllerTransform* t = controller_transforms[ctrl_idx].get();
    if(!t || led_idx >= t->led_positions.size()) return false;
    Vector3D world = ComputeWorldPositionForSDK(t, led_idx);
    x = world.x;
    y = world.y;
    z = world.z;
    return true;
}




/*---------------------------------------------------------*\
| Display Plane Management                                 |
\*---------------------------------------------------------*/


















bool OpenRGB3DSpatialTab::SDK_GetLEDWorldPositions(size_t ctrl_idx, float* xyz_interleaved, size_t max_triplets, size_t& out_count) const
{
    out_count = 0;
    if(ctrl_idx >= controller_transforms.size() || !xyz_interleaved || max_triplets == 0) return false;
    ControllerTransform* t = controller_transforms[ctrl_idx].get();
    if(!t) return false;
    size_t n = std::min(max_triplets, t->led_positions.size());
    for(size_t i = 0; i < n; ++i)
    {
        Vector3D world = ComputeWorldPositionForSDK(t, i);
        xyz_interleaved[i*3+0] = world.x;
        xyz_interleaved[i*3+1] = world.y;
        xyz_interleaved[i*3+2] = world.z;
    }
    out_count = n;
    return true;
}






size_t OpenRGB3DSpatialTab::SDK_GetTotalLEDCount() const
{
    size_t total = 0;
    for(const std::unique_ptr<ControllerTransform>& up : controller_transforms)
    {
        if(up) total += up->led_positions.size();
    }
    return total;
}

bool OpenRGB3DSpatialTab::SDK_GetAllLEDWorldPositions(float* xyz_interleaved, size_t max_triplets, size_t& out_count) const
{
    out_count = 0;
    if(!xyz_interleaved || max_triplets == 0) return false;
    size_t written = 0;
    for(const std::unique_ptr<ControllerTransform>& up : controller_transforms)
    {
        if(!up) continue;
        for(size_t i = 0; i < up->led_positions.size(); ++i)
        {
            if(written >= max_triplets) { out_count = written; return true; }
            Vector3D world = ComputeWorldPositionForSDK(up.get(), i);
            xyz_interleaved[written*3+0] = world.x;
            xyz_interleaved[written*3+1] = world.y;
            xyz_interleaved[written*3+2] = world.z;
            ++written;
        }
    }
    out_count = written;
    return true;
}

bool OpenRGB3DSpatialTab::SDK_RegisterGridLayoutCallback(void (*cb)(void*), void* user)
{
    if(!cb) return false;
    grid_layout_callbacks.emplace_back(cb, user);
    return true;
}

bool OpenRGB3DSpatialTab::SDK_UnregisterGridLayoutCallback(void (*cb)(void*), void* user)
{
    for(std::vector<std::pair<void (*)(void*), void*>>::iterator it = grid_layout_callbacks.begin(); it != grid_layout_callbacks.end(); ++it)
    {
        if(it->first == cb && it->second == user) { grid_layout_callbacks.erase(it); return true; }
    }
    return false;
}

bool OpenRGB3DSpatialTab::SDK_SetControllerColors(size_t ctrl_idx, const unsigned int* bgr_colors, size_t count)
{
    if(ctrl_idx >= controller_transforms.size() || !bgr_colors || count == 0) return false;
    ControllerTransform* t = controller_transforms[ctrl_idx].get();
    if(!t || !t->controller) return false;
    size_t n = std::min<size_t>(count, t->controller->colors.size());
    for(size_t i=0;i<n;++i) t->controller->colors[i] = (RGBColor)bgr_colors[i];
    t->controller->UpdateLEDs();
    return true;
}

bool OpenRGB3DSpatialTab::SDK_SetSingleLEDColor(size_t ctrl_idx, size_t led_idx, unsigned int bgr_color)
{
    if(ctrl_idx >= controller_transforms.size()) return false;
    ControllerTransform* t = controller_transforms[ctrl_idx].get();
    if(!t || !t->controller) return false;
    if(led_idx >= t->controller->colors.size()) return false;
    t->controller->colors[led_idx] = (RGBColor)bgr_color;
    t->controller->UpdateSingleLED((int)led_idx);
    return true;
}


// Order enum
static const int GRID_ORDER_CONTROLLER = 0;
static const int GRID_ORDER_RASTER_XYZ = 1;

static inline bool PosLessXYZ(const LEDPosition3D* a, const LEDPosition3D* b)
{
    if(a->world_position.z != b->world_position.z) return a->world_position.z < b->world_position.z;
    if(a->world_position.y != b->world_position.y) return a->world_position.y < b->world_position.y;
    if(a->world_position.x != b->world_position.x) return a->world_position.x < b->world_position.x;
    if(a->controller != b->controller) return a->controller < b->controller;
    return a->led_idx < b->led_idx;
}

bool OpenRGB3DSpatialTab::SDK_GetAllLEDWorldPositionsWithOffsets(float* xyz_interleaved, size_t max_triplets, size_t& out_triplets, size_t* ctrl_offsets, size_t offsets_capacity, size_t& out_controllers) const
{
    out_triplets = 0; out_controllers = 0;
    if(!xyz_interleaved || max_triplets == 0 || !ctrl_offsets || offsets_capacity == 0) return false;
    size_t written = 0;
    size_t ctrl_count = controller_transforms.size();
    if(offsets_capacity < ctrl_count + 1) return false;
    ctrl_offsets[0] = 0; size_t oi = 1;
    for(size_t c=0; c<ctrl_count; ++c)
    {
        ControllerTransform* t = controller_transforms[c].get(); if(!t) { ctrl_offsets[oi++] = written; continue; }
        size_t n = std::min(max_triplets - written, t->led_positions.size());
        for(size_t i=0; i<n; ++i)
        {
            Vector3D world = ComputeWorldPositionForSDK(t, i);
            xyz_interleaved[written*3+0] = world.x;
            xyz_interleaved[written*3+1] = world.y;
            xyz_interleaved[written*3+2] = world.z;
            ++written;
            if(written >= max_triplets) { ++out_controllers; break; }
        }
        ctrl_offsets[oi++] = written;
        ++out_controllers;
        if(written >= max_triplets) break;
    }
    out_triplets = written;
    return true;
}

bool OpenRGB3DSpatialTab::SDK_SetGridOrderColors(const unsigned int* bgr_colors_by_grid, size_t count)
{
    return SDK_SetGridOrderColorsWithOrder(GRID_ORDER_CONTROLLER, bgr_colors_by_grid, count);
}

bool OpenRGB3DSpatialTab::SDK_SetGridOrderColorsWithOrder(int order, const unsigned int* bgr, size_t count)
{
    if(!bgr || count == 0) return false;
    // Build mapping
    std::vector<std::pair<size_t,size_t>> map; // (ctrl_idx, led_idx)
    if(order == GRID_ORDER_CONTROLLER)
    {
        for(size_t c=0;c<controller_transforms.size();++c)
        {
            ControllerTransform* t = controller_transforms[c].get(); if(!t || !t->controller) continue;
            for(size_t i=0;i<t->controller->colors.size();++i) map.emplace_back(c,i);
        }
    }
    else if(order == GRID_ORDER_RASTER_XYZ)
    {
        std::vector<const LEDPosition3D*> all;
        for(size_t c=0;c<controller_transforms.size();++c)
        {
            ControllerTransform* t = controller_transforms[c].get(); if(!t || !t->controller) continue;
            for(size_t i=0;i<t->led_positions.size();++i) all.push_back(&t->led_positions[i]);
        }
        std::stable_sort(all.begin(), all.end(), PosLessXYZ);
        map.reserve(all.size());
        for(size_t idx = 0; idx < all.size(); ++idx)
        {
            const LEDPosition3D* p = all[idx];
            // Find controller index by pointer match (linear scan acceptable for moderate sizes)
            size_t cidx = 0;
            for(; cidx<controller_transforms.size(); ++cidx)
            {
                if(controller_transforms[cidx].get() && controller_transforms[cidx]->controller == p->controller) break;
            }
            map.emplace_back(cidx, (size_t)p->led_idx);
        }
    }
    if(map.empty()) return false;
    size_t n = std::min(count, map.size());
    // Apply colors
    for(size_t k=0;k<n;++k)
    {
        size_t c = map[k].first; size_t i = map[k].second;
        ControllerTransform* t = controller_transforms[c].get(); if(!t || !t->controller) continue;
        if(i < t->controller->colors.size()) t->controller->colors[i] = (RGBColor)bgr[k];
    }
    // Update devices
    for(size_t c=0;c<controller_transforms.size();++c)
    {
        ControllerTransform* t = controller_transforms[c].get(); if(t && t->controller) t->controller->UpdateLEDs();
    }
    return true;
}

/*---------------------------------------------------------*\
| Refresh Display Plane Capture Source List                |
\*---------------------------------------------------------*/
