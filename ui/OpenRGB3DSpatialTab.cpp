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

    stack_settings_updating = false;
    spatial_debug_dump_pending = false;
    spatial_debug_enabled = false;
    effect_controls_widget = nullptr;
    effect_controls_layout = nullptr;
    current_effect_ui = nullptr;
    start_effect_button = nullptr;
    stop_effect_button = nullptr;
    spatial_debug_checkbox = nullptr;
    stack_blend_container = nullptr;
    stack_effect_blend_combo = nullptr;
    stack_effect_blend_combo = nullptr;
    stack_blend_container = nullptr;
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
    stack_blend_container = nullptr;
    stack_presets_list = nullptr;
    next_effect_instance_id = 1;

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

    // Connect GridLayoutChanged signal to invoke SDK callbacks
    connect(this, &OpenRGB3DSpatialTab::GridLayoutChanged, this, [this]() {
        for(size_t callback_index = 0; callback_index < grid_layout_callbacks.size(); callback_index++)
        {
            const std::pair<void (*)(void*), void*>& cb_pair = grid_layout_callbacks[callback_index];
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
    main_layout->setSpacing(6);
    main_layout->setContentsMargins(4, 4, 4, 4);

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
    left_panel->setSpacing(6);

    /*---------------------------------------------------------*\
    | Tab Widget for left panel                                |
    \*---------------------------------------------------------*/
    left_tabs = new QTabWidget();

    /*---------------------------------------------------------*\
    | Available Controllers Tab                                |
    \*---------------------------------------------------------*/
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
    connect(granularity_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_granularity_changed(int)));
    granularity_layout->addWidget(granularity_combo);
    available_layout->addLayout(granularity_layout);

    item_combo = new QComboBox();
    available_layout->addWidget(item_combo);

    // LED Spacing Controls
    QLabel* spacing_label = new QLabel("LED Spacing (mm):");
    QFont spacing_font = spacing_label->font();
    spacing_font.setBold(true);
    spacing_label->setFont(spacing_font);
    spacing_label->setContentsMargins(0, 3, 0, 0);
    available_layout->addWidget(spacing_label);

    QVBoxLayout* spacing_layout = new QVBoxLayout();
    spacing_layout->setSpacing(2);
    spacing_layout->setContentsMargins(0, 0, 0, 0);

    auto create_spacing_row = [](const QString& label_text, QDoubleSpinBox*& spin) -> QHBoxLayout*
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
        row->addWidget(spin, 1);

        return row;
    };

    QHBoxLayout* spacing_x_row = create_spacing_row("X:", led_spacing_x_spin);
    led_spacing_x_spin->setValue(10.0);
    led_spacing_x_spin->setToolTip("Horizontal spacing between LEDs (left/right)");
    spacing_layout->addLayout(spacing_x_row);

    QHBoxLayout* spacing_y_row = create_spacing_row("Y:", led_spacing_y_spin);
    led_spacing_y_spin->setValue(0.0);
    led_spacing_y_spin->setToolTip("Vertical spacing between LEDs (floor/ceiling)");
    spacing_layout->addLayout(spacing_y_row);

    QHBoxLayout* spacing_z_row = create_spacing_row("Z:", led_spacing_z_spin);
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

    /*---------------------------------------------------------*\
    | Controllers in 3D Scene (below tabs)                     |
    \*---------------------------------------------------------*/
    QGroupBox* controller_group = new QGroupBox("Controllers in 3D Scene");
    QVBoxLayout* controller_layout = new QVBoxLayout();
    controller_layout->setSpacing(3);

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
    QFont edit_spacing_font = edit_spacing_label->font();
    edit_spacing_font.setBold(true);
    edit_spacing_label->setFont(edit_spacing_font);
    edit_spacing_label->setContentsMargins(0, 3, 0, 0);
    controller_layout->addWidget(edit_spacing_label);

    QVBoxLayout* edit_spacing_layout = new QVBoxLayout();
    edit_spacing_layout->setSpacing(2);
    edit_spacing_layout->setContentsMargins(0, 0, 0, 0);

    auto create_edit_row = [](const QString& text, QDoubleSpinBox*& spin) -> QHBoxLayout*
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(3);
        row->setContentsMargins(0, 0, 0, 0);

        QLabel* lbl = new QLabel(text);
        lbl->setMinimumWidth(14);
        row->addWidget(lbl);

        spin = new QDoubleSpinBox();
        spin->setRange(0.0, 1000.0);
        spin->setSingleStep(1.0);
        spin->setSuffix(" mm");
        spin->setAlignment(Qt::AlignRight);
        spin->setEnabled(false);
        row->addWidget(spin, 1);

        return row;
    };

    QHBoxLayout* edit_x_row = create_edit_row("X:", edit_led_spacing_x_spin);
    edit_led_spacing_x_spin->setValue(10.0);
    edit_spacing_layout->addLayout(edit_x_row);

    QHBoxLayout* edit_y_row = create_edit_row("Y:", edit_led_spacing_y_spin);
    edit_led_spacing_y_spin->setValue(0.0);
    edit_spacing_layout->addLayout(edit_y_row);

    QHBoxLayout* edit_z_row = create_edit_row("Z:", edit_led_spacing_z_spin);
    edit_led_spacing_z_spin->setValue(0.0);
    edit_spacing_layout->addLayout(edit_z_row);

    apply_spacing_button = new QPushButton("Apply Spacing");
    apply_spacing_button->setEnabled(false);
    connect(apply_spacing_button, SIGNAL(clicked()), this, SLOT(on_apply_spacing_clicked()));
    edit_spacing_layout->addWidget(apply_spacing_button);

    controller_layout->addLayout(edit_spacing_layout);

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
    layout_layout->setSpacing(3);
    layout_layout->setContentsMargins(2, 2, 2, 2);

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
    layout_layout->addWidget(new QLabel("=== Room Dimensions (Origin: Front-Left-Floor Corner) ==="), 2, 0, 1, 6);

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
    room_height_spin = new QDoubleSpinBox();
    room_height_spin->setRange(100.0, 50000.0);
    room_height_spin->setSingleStep(10.0);
    room_height_spin->setValue(manual_room_height);
    room_height_spin->setSuffix(" mm");
    room_height_spin->setToolTip("Room height (floor to ceiling, Y-axis in standard OpenGL Y-up)");
    room_height_spin->setEnabled(use_manual_room_size);
    layout_layout->addWidget(room_height_spin, 4, 3);

    // Room Depth (Z-axis: Front to Back)
    layout_layout->addWidget(new QLabel("Depth (Z):"), 4, 4);
    room_depth_spin = new QDoubleSpinBox();
    room_depth_spin->setRange(100.0, 50000.0);
    room_depth_spin->setSingleStep(10.0);
    room_depth_spin->setValue(manual_room_depth);
    room_depth_spin->setSuffix(" mm");
    room_depth_spin->setToolTip("Room depth (front to back, Z-axis in standard OpenGL Y-up)");
    room_depth_spin->setEnabled(use_manual_room_size);
    layout_layout->addWidget(room_depth_spin, 4, 5);

    // Selection Info Label
    selection_info_label = new QLabel("No selection");
    selection_info_label->setAlignment(Qt::AlignRight);
    QFont selection_font = selection_info_label->font();
    selection_font.setBold(true);
    selection_info_label->setFont(selection_font);
    layout_layout->addWidget(selection_info_label, 1, 3, 1, 3);

    // Add helpful labels with text wrapping
    QLabel* grid_help1 = new QLabel(QString("Measure from front-left-floor corner. Positions in grid units (%1mm)").arg(grid_scale_mm));
    grid_help1->setForegroundRole(QPalette::PlaceholderText);
    grid_help1->setWordWrap(true);
    layout_layout->addWidget(grid_help1, 5, 0, 1, 6);

    QLabel* grid_help2 = new QLabel("Use Ctrl+Click for multi-select. Add User position in Object Creator tab");
    grid_help2->setForegroundRole(QPalette::PlaceholderText);
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
        if(room_height_spin) room_height_spin->setEnabled(checked);
        if(room_depth_spin) room_depth_spin->setEnabled(checked);

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

    connect(room_height_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        manual_room_height = value;

        // Update viewport with new depth
        if(viewport)
        {
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        emit GridLayoutChanged();
    });

    connect(room_depth_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        manual_room_depth = value;

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
    QHBoxLayout* transform_layout = new QHBoxLayout();
    transform_layout->setSpacing(6);
    transform_layout->setContentsMargins(2, 2, 2, 2);

    QGridLayout* position_layout = new QGridLayout();
    position_layout->setSpacing(3);
    position_layout->setContentsMargins(0, 0, 0, 0);

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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.position.x = pos_value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.position.x = value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.position.y = pos_value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.position.y = value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.position.z = pos_value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.position.z = value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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

    position_layout->setColumnStretch(1, 1);

    QGridLayout* rotation_layout = new QGridLayout();
    rotation_layout->setSpacing(3);
    rotation_layout->setContentsMargins(0, 0, 0, 0);

    rotation_layout->addWidget(new QLabel("Rotation X:"), 0, 0);

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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.rotation.x = rot_value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
    rotation_layout->addWidget(rot_x_slider, 0, 1);

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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.rotation.x = value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
    rotation_layout->addWidget(rot_x_spin, 0, 2);

    rotation_layout->addWidget(new QLabel("Rotation Y:"), 1, 0);

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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.rotation.y = rot_value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
    rotation_layout->addWidget(rot_y_slider, 1, 1);

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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.rotation.y = value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
    rotation_layout->addWidget(rot_y_spin, 1, 2);

    rotation_layout->addWidget(new QLabel("Rotation Z:"), 2, 0);

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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.rotation.z = rot_value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
    rotation_layout->addWidget(rot_z_slider, 2, 1);

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
            ControllerTransform* transform = controller_transforms[ctrl_row].get();
            if(transform)
            {
                transform->transform.rotation.z = value;
                ControllerLayout3D::MarkWorldPositionsDirty(transform);
            }
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
    rotation_layout->addWidget(rot_z_spin, 2, 2);

    rotation_layout->setColumnStretch(1, 1);

    transform_layout->addLayout(position_layout, 1);
    transform_layout->addLayout(rotation_layout, 1);

    transform_tab->setLayout(transform_layout);

    settings_tabs->addTab(transform_tab, "Position & Rotation");
    settings_tabs->addTab(grid_settings_tab, "Grid Settings");

    /*---------------------------------------------------------*\
    | Object Creator Tab (Custom Controllers, Ref Points, Displays) |
    \*---------------------------------------------------------*/
    QWidget* object_creator_tab = new QWidget();
    QVBoxLayout* creator_layout = new QVBoxLayout();
    creator_layout->setSpacing(6);

    // Type selector dropdown
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

    // Stacked widget for dynamic UI
    QStackedWidget* creator_stack = new QStackedWidget();

    // Page 0: Empty placeholder
    QWidget* empty_page = new QWidget();
    QVBoxLayout* empty_layout = new QVBoxLayout(empty_page);
    QLabel* empty_label = new QLabel("Select an object type from the dropdown above to begin creating custom objects.");
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

    /*---------------------------------------------------------*\
    | Custom Controllers Page                                  |
    \*---------------------------------------------------------*/
    QWidget* custom_controller_page = new QWidget();
    QVBoxLayout* custom_layout = new QVBoxLayout(custom_controller_page);
    custom_layout->setSpacing(4);

    QLabel* custom_list_label = new QLabel("Available Custom Controllers:");
    QFont custom_list_font = custom_list_label->font();
    custom_list_font.setBold(true);
    custom_list_label->setFont(custom_list_font);
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
    ref_points_layout->setSpacing(4);

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

    /*---------------------------------------------------------*\
    | Display Planes Page                                      |
    \*---------------------------------------------------------*/
    QWidget* display_plane_page = new QWidget();
    QVBoxLayout* display_layout = new QVBoxLayout(display_plane_page);
    display_layout->setSpacing(6);

    display_planes_list = new QListWidget();
    display_planes_list->setMinimumHeight(200);
    display_planes_list->setUniformItemSizes(true);
    display_planes_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
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
    RefreshDisplayPlaneCaptureSourceList();

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

    settings_tabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    settings_tabs->setMaximumHeight(260);
    middle_panel->addWidget(settings_tabs, 0, Qt::AlignTop);

    main_layout->addLayout(middle_panel, 3);  // Give middle panel more space

    /*---------------------------------------------------------*\
    | Effects Tab (Effect Controls and Presets)                |
    | Created first so it's the default tab on startup         |
    \*---------------------------------------------------------*/
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

    QGroupBox* effect_group = new QGroupBox("Effect Configuration");
    QVBoxLayout* effect_layout = new QVBoxLayout(effect_group);
    effect_layout->setSpacing(4);

    QLabel* effect_label = new QLabel("Effect:");
    effect_layout->addWidget(effect_label);

    effect_combo = new QComboBox();
    effect_combo->setToolTip("Select an effect layer from the stack to edit its controls.");
    connect(effect_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_effect_changed);
    UpdateEffectCombo();
    effect_layout->addWidget(effect_combo);

    QLabel* zone_label = new QLabel("Zone:");
    effect_layout->addWidget(zone_label);
    effect_zone_combo = new QComboBox();
    PopulateZoneTargetCombo(effect_zone_combo, -1);
    connect(effect_zone_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_effect_zone_changed(int)));
    effect_layout->addWidget(effect_zone_combo);

    QLabel* origin_label = new QLabel("Origin:");
    effect_layout->addWidget(origin_label);
    effect_origin_combo = new QComboBox();
    effect_origin_combo->addItem("Room Center", QVariant(-1));
    connect(effect_origin_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_effect_origin_changed);
    effect_layout->addWidget(effect_origin_combo);

    spatial_debug_checkbox = new QCheckBox("Spatial debug logging");
    spatial_debug_checkbox->setToolTip("When enabled, logging information about room/world coordinates will be printed the next time a controller is rotated.");
    spatial_debug_checkbox->setChecked(false);
    connect(spatial_debug_checkbox, &QCheckBox::toggled, this, &OpenRGB3DSpatialTab::on_spatial_debug_toggled);
    effect_layout->addWidget(spatial_debug_checkbox);

    stack_effect_type_combo = new QComboBox(effect_group);
    stack_effect_type_combo->addItem("None", "");
    std::vector<EffectRegistration3D> effect_list = EffectListManager3D::get()->GetAllEffects();
    for(unsigned int i = 0; i < effect_list.size(); i++)
    {
        stack_effect_type_combo->addItem(QString::fromStdString(effect_list[i].ui_name),
                                         QString::fromStdString(effect_list[i].class_name));
    }
    stack_effect_type_combo->setVisible(false);
    connect(stack_effect_type_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_stack_effect_type_changed(int)));

    stack_effect_zone_combo = new QComboBox(effect_group);
    stack_effect_zone_combo->addItem("All Controllers", -1);
    stack_effect_zone_combo->setVisible(false);
    connect(stack_effect_zone_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_stack_effect_zone_changed(int)));

    UpdateStackEffectZoneCombo();

    effect_controls_widget = new QWidget();
    effect_controls_layout = new QVBoxLayout();
    effect_controls_layout->setContentsMargins(0, 0, 0, 0);
    effect_controls_widget->setLayout(effect_controls_layout);
    effect_layout->addWidget(effect_controls_widget);

    detail_layout->addWidget(effect_group);

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

void OpenRGB3DSpatialTab::SetupEffectLibraryPanel(QVBoxLayout* parent_layout)
{
    QGroupBox* library_group = new QGroupBox("Effect Library");
    QVBoxLayout* library_layout = new QVBoxLayout(library_group);
    library_layout->setSpacing(4);

    QLabel* category_label = new QLabel("Category:");
    library_layout->addWidget(category_label);

    effect_category_combo = new QComboBox();
    effect_category_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(effect_category_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_effect_library_category_changed(int)));
    library_layout->addWidget(effect_category_combo);

    effect_library_list = new QListWidget();
    effect_library_list->setSelectionMode(QAbstractItemView::SingleSelection);
    effect_library_list->setMinimumHeight(160);
    connect(effect_library_list, SIGNAL(currentRowChanged(int)),
            this, SLOT(on_effect_library_selection_changed(int)));
    connect(effect_library_list, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            this, SLOT(on_effect_library_item_double_clicked(QListWidgetItem*)));
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
    /*---------------------------------------------------------*\
    | Clear current custom UI                                  |
    \*---------------------------------------------------------*/
    ClearCustomEffectUI();

    /*---------------------------------------------------------*\
    | Set up new custom UI based on effect type               |
    \*---------------------------------------------------------*/
    QString class_name;
    if(effect_type_combo && index >= 0)
    {
        class_name = effect_type_combo->itemData(index, kEffectRoleClassName).toString();
    }
    SetupCustomEffectUI(class_name);
}

void OpenRGB3DSpatialTab::SetupCustomEffectUI(const QString& class_name)
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

    if(class_name.isEmpty())
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Attempted to setup effect with empty class name");
        return;
    }

    /*---------------------------------------------------------*\
    | Create effect using the registration system             |
    \*---------------------------------------------------------*/
    
    SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(class_name.toStdString());
    if(!effect)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create effect: %s", class_name.toStdString().c_str());
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
    if(class_name == QLatin1String("ScreenMirror3D"))
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
        "To edit this preset, open the Effect Stack panel, load it,\n"
        "modify the effects, and save with the same name."
    );
    info_label->setWordWrap(true);
    info_label->setFrameShape(QFrame::StyledPanel);
    info_label->setFrameShadow(QFrame::Raised);
    info_label->setLineWidth(1);
    info_label->setContentsMargins(6, 6, 6, 6);
    effect_controls_layout->addWidget(info_label);

    /*---------------------------------------------------------*\
    | Create Start/Stop button container                       |
    \*---------------------------------------------------------*/
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

/* Legacy user position control functions - REMOVED
 * User position is now handled through the reference points system
 * as a REF_POINT_USER type reference point.
 */

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

void OpenRGB3DSpatialTab::on_spatial_debug_toggled(bool enabled)
{
    spatial_debug_enabled = enabled;
    if(spatial_debug_enabled)
    {
        spatial_debug_dump_pending = true;
        LOG_INFO("[OpenRGB3DSpatialPlugin] Spatial debug logging enabled - rotate any controller to capture coordinates.");
    }
    else
    {
        LOG_INFO("[OpenRGB3DSpatialPlugin] Spatial debug logging disabled.");
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

    world.x = GridUnitsToMM(world.x, grid_scale_mm);
    world.y = GridUnitsToMM(world.y, grid_scale_mm);
    world.z = GridUnitsToMM(world.z, grid_scale_mm);
    return world;
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
    for(size_t transform_index = 0; transform_index < controller_transforms.size(); transform_index++)
    {
        const std::unique_ptr<ControllerTransform>& up = controller_transforms[transform_index];
        if(up) total += up->led_positions.size();
    }
    return total;
}

bool OpenRGB3DSpatialTab::SDK_GetAllLEDWorldPositions(float* xyz_interleaved, size_t max_triplets, size_t& out_count) const
{
    out_count = 0;
    if(!xyz_interleaved || max_triplets == 0) return false;
    size_t written = 0;
    for(size_t transform_index = 0; transform_index < controller_transforms.size(); transform_index++)
    {
        const std::unique_ptr<ControllerTransform>& up = controller_transforms[transform_index];
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

