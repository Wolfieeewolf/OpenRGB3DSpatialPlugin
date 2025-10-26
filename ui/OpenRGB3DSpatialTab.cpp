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

static int MapHzToBandIndex(float hz, int bands, float f_min, float f_max)
{
    float clamped = hz;
    if(clamped < f_min) clamped = f_min;
    if(clamped > f_max) clamped = f_max;
    float t = std::log(clamped / f_min) / std::log(f_max / f_min);
    int idx = (int)std::floor(t * bands);
    if(idx < 0) idx = 0;
    if(idx > bands - 1) idx = bands - 1;
    return idx;
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
    controller_list = nullptr;
    reference_points_list = nullptr;
    display_planes_list = nullptr;
    display_plane_name_edit = nullptr;
    display_plane_width_spin = nullptr;
    display_plane_height_spin = nullptr;
    display_plane_bezel_spin = nullptr;
    display_plane_capture_combo = nullptr;
    display_plane_refresh_capture_btn = nullptr;
    display_plane_visible_check = nullptr;
    add_display_plane_button = nullptr;
    remove_display_plane_button = nullptr;
    current_display_plane_index = -1;
    zones_list = nullptr;

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

    /*---------------------------------------------------------*\
    | Custom 3D Controllers Tab                                |
    \*---------------------------------------------------------*/
    QWidget* custom_tab = new QWidget();
    QVBoxLayout* custom_layout = new QVBoxLayout();
    custom_layout->setSpacing(5);

    // Custom controllers list
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

    // Import/Export/Edit buttons
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

    custom_tab->setLayout(custom_layout);
    left_tabs->addTab(custom_tab, "Custom Controllers");

    /*---------------------------------------------------------*\
    | Reference Points Tab                                     |
    \*---------------------------------------------------------*/
    QWidget* ref_points_tab = new QWidget();
    QVBoxLayout* ref_points_layout = new QVBoxLayout();
    ref_points_layout->setSpacing(5);

    // List of reference points
    reference_points_list = new QListWidget();
    reference_points_list->setMinimumHeight(150);
    connect(reference_points_list, &QListWidget::currentRowChanged, this, &OpenRGB3DSpatialTab::on_ref_point_selected);
    ref_points_layout->addWidget(reference_points_list);

    // Name input
    QHBoxLayout* name_layout = new QHBoxLayout();
    name_layout->addWidget(new QLabel("Name:"));
    ref_point_name_edit = new QLineEdit();
    ref_point_name_edit->setPlaceholderText("e.g., My Monitor");
    name_layout->addWidget(ref_point_name_edit);
    ref_points_layout->addLayout(name_layout);

    // Type combo
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

    // Color picker
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

    // Help text
    QLabel* help_label = new QLabel("Select a reference point to move it with the Position & Rotation controls and 3D gizmo.");
    help_label->setStyleSheet("color: gray; font-size: 10px;");
    help_label->setWordWrap(true);
    ref_points_layout->addWidget(help_label);

    // Add/Remove buttons
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

    ref_points_tab->setLayout(ref_points_layout);
    left_tabs->addTab(ref_points_tab, "Reference Points");

    /*---------------------------------------------------------*\
    | Display Planes Tab                                       |
    \*---------------------------------------------------------*/
    QWidget* display_tab = new QWidget();
    QVBoxLayout* display_layout = new QVBoxLayout();
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

    plane_form->addWidget(new QLabel("Bezel (mm):"), plane_row, 0);
    display_plane_bezel_spin = new QDoubleSpinBox();
    display_plane_bezel_spin->setRange(0.0, 200.0);
    display_plane_bezel_spin->setDecimals(1);
    display_plane_bezel_spin->setSingleStep(1.0);
    connect(display_plane_bezel_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OpenRGB3DSpatialTab::on_display_plane_bezel_changed);
    plane_form->addWidget(display_plane_bezel_spin, plane_row, 1);
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
    connect(display_plane_visible_check, &QCheckBox::stateChanged,
            this, &OpenRGB3DSpatialTab::on_display_plane_visible_toggled);
    display_layout->addWidget(display_plane_visible_check);

    display_layout->addStretch();

    display_tab->setLayout(display_layout);
    left_tabs->addTab(display_tab, "Display Planes");

    // Initialize capture source list
    RefreshDisplayPlaneCaptureSourceList();

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

    QLabel* grid_help2 = new QLabel("Use Ctrl+Click for multi-select • Add User position in Reference Points tab");
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
    | Unified Profiles Tab (Layout + Effect profiles)         |
    \*---------------------------------------------------------*/
    SetupProfilesTab(settings_tabs);

    middle_panel->addWidget(settings_tabs);

    main_layout->addLayout(middle_panel, 3);  // Give middle panel more space

    /*---------------------------------------------------------*\
    | Add Setup tab to main tabs                               |
    \*---------------------------------------------------------*/
    main_tabs->addTab(setup_tab, "Setup / Grid");

    /*---------------------------------------------------------*\
    | Effects Tab (Effect Controls and Presets)                |
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
    | Effects Tab                                              |
    \*---------------------------------------------------------*/
    QWidget* effects_tab = new QWidget();
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
    effects_tab->setLayout(effects_layout);
    right_tabs->addTab(effects_tab, "Effects");

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
    | Add Effects tab to main tabs                             |
    \*---------------------------------------------------------*/
    main_tabs->addTab(effects_tab, "Effects / Presets");

    /*---------------------------------------------------------*\
    | Set the root layout                                      |
    \*---------------------------------------------------------*/
    setLayout(root_layout);
}

void OpenRGB3DSpatialTab::SetupAudioTab(QTabWidget* tab_widget)
{
    audio_tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(audio_tab);

    QLabel* hdr = new QLabel("Audio Input (used by Audio effects)");
    hdr->setStyleSheet("font-weight: bold;");
    layout->addWidget(hdr);

    // Top controls: Start/Stop and Level meter
    QHBoxLayout* top_controls = new QHBoxLayout();
    audio_start_button = new QPushButton("Start Listening");
    audio_stop_button = new QPushButton("Stop");
    audio_stop_button->setEnabled(false);
    top_controls->addWidget(audio_start_button);
    top_controls->addWidget(audio_stop_button);
    top_controls->addStretch();
    layout->addLayout(top_controls);

    layout->addWidget(new QLabel("Level:"));
    audio_level_bar = new QProgressBar();
    audio_level_bar->setRange(0, 1000);
    audio_level_bar->setValue(0);
    audio_level_bar->setTextVisible(false);
    audio_level_bar->setFixedHeight(14);
    layout->addWidget(audio_level_bar);

    connect(audio_start_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_start_clicked);
    connect(audio_stop_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_stop_clicked);
    connect(AudioInputManager::instance(), &AudioInputManager::LevelUpdated, this, &OpenRGB3DSpatialTab::on_audio_level_updated);

    // Capture source (Windows only)
#ifdef _WIN32
    // Render device combo removed; unified device list used
#endif

    // Device selection
    layout->addWidget(new QLabel("Input Device:"));
    audio_device_combo = new QComboBox();
    audio_device_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    audio_device_combo->setMinimumWidth(200);
    QStringList devs = AudioInputManager::instance()->listInputDevices();
    if(devs.isEmpty())
    {
        audio_device_combo->addItem("No input devices detected");
        audio_device_combo->setEnabled(false);
    }
    else
    {
        audio_device_combo->addItems(devs);
        connect(audio_device_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_audio_device_changed);
        // Initialize selection to first device
        audio_device_combo->setCurrentIndex(0);
        on_audio_device_changed(0);
    }
    layout->addWidget(audio_device_combo);

    // Gain
    QHBoxLayout* gain_layout = new QHBoxLayout();
    gain_layout->addWidget(new QLabel("Gain:"));
    audio_gain_slider = new QSlider(Qt::Horizontal);
    audio_gain_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    audio_gain_slider->setRange(1, 100); // maps to 0.1..10.0
    audio_gain_slider->setValue(10);     // 1.0x
    connect(audio_gain_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_audio_gain_changed);
    gain_layout->addWidget(audio_gain_slider);
    // Numeric readout (e.g., 1.0x)
    audio_gain_value_label = new QLabel("1.0x");
    audio_gain_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    audio_gain_value_label->setMinimumWidth(48);
    gain_layout->addWidget(audio_gain_value_label);
    layout->addLayout(gain_layout);

    // Input smoothing removed; per-effect smoothing is configured below

    // Bands & crossovers
    QHBoxLayout* bands_layout = new QHBoxLayout();
    bands_layout->addWidget(new QLabel("Bands:"));
    audio_bands_combo = new QComboBox();
    audio_bands_combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    audio_bands_combo->addItems({"8", "16", "32"});
    audio_bands_combo->setCurrentText("16");
    connect(audio_bands_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_audio_bands_changed);
    bands_layout->addWidget(audio_bands_combo);
    bands_layout->addStretch();
    layout->addLayout(bands_layout);

    // Crossovers removed from UI; per-effect Hz mapping used instead
    // Audio Effects section
    QGroupBox* audio_fx_group = new QGroupBox("Audio Effects");
    QVBoxLayout* audio_fx_layout = new QVBoxLayout(audio_fx_group);
    QHBoxLayout* fx_row1 = new QHBoxLayout();
    fx_row1->addWidget(new QLabel("Effect:"));
    audio_effect_combo = new QComboBox();
    audio_effect_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    audio_effect_combo->addItem("None");              // index 0
    audio_effect_combo->addItem("Audio Level 3D");    // index 1
    audio_effect_combo->addItem("Spectrum Bars 3D");  // index 2
    audio_effect_combo->addItem("Beat Pulse 3D");     // index 3
    audio_effect_combo->addItem("Band Scan 3D");      // index 4
    fx_row1->addWidget(audio_effect_combo);
    audio_fx_layout->addLayout(fx_row1);

    // Zone selector (on its own row, matching Effects tab layout)
    QHBoxLayout* fx_row2 = new QHBoxLayout();
    fx_row2->addWidget(new QLabel("Zone:"));
    audio_effect_zone_combo = new QComboBox();
    audio_effect_zone_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fx_row2->addWidget(audio_effect_zone_combo);
    connect(audio_effect_zone_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_audio_effect_zone_changed);
    audio_fx_layout->addLayout(fx_row2);

    // Origin selector (on its own row, matching Effects tab layout)
    QHBoxLayout* fx_row3 = new QHBoxLayout();
    fx_row3->addWidget(new QLabel("Origin:"));
    audio_effect_origin_combo = new QComboBox();
    audio_effect_origin_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    audio_effect_origin_combo->addItem("Room Center", QVariant(-1));
    connect(audio_effect_origin_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_audio_effect_origin_changed);
    fx_row3->addWidget(audio_effect_origin_combo);
    audio_fx_layout->addLayout(fx_row3);

    // Per-effect Hz mapping
    // Dynamic effect controls (consistent with main Effects tab)
    audio_effect_controls_widget = new QWidget();
    audio_effect_controls_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    audio_effect_controls_layout = new QVBoxLayout(audio_effect_controls_widget);
    audio_effect_controls_layout->setContentsMargins(0,0,0,0);
    audio_effect_controls_widget->setLayout(audio_effect_controls_layout);
    audio_fx_layout->addWidget(audio_effect_controls_widget);

    // Standard Audio Controls panel (Hz, smoothing, falloff)
    SetupStandardAudioControls(audio_fx_layout);

    // Dynamic effect UI provides Start/Stop (consistent with main Effects tab)
    layout->addWidget(audio_fx_group);

    connect(audio_effect_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::SetupAudioEffectUI);
    audio_effect_combo->setCurrentIndex(0);  // Default to "None"
    SetupAudioEffectUI(0);  // Initialize with "None" selected (hides controls)

    // Help text
    QLabel* help = new QLabel("Use Effects > select 'Audio Level 3D' to react to audio.\nThis tab manages input device and sensitivity shared by audio effects.");
    help->setStyleSheet("color: gray; font-size: 10px;");
    help->setWordWrap(true);
    layout->addWidget(help);

    // Load persisted audio settings (device, gain, bands, audio controls)
    {
        nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
        // Device index
        if(audio_device_combo && audio_device_combo->isEnabled() && settings.contains("AudioDeviceIndex"))
        {
            int di = settings["AudioDeviceIndex"].get<int>();
            if(di >= 0 && di < audio_device_combo->count())
            {
                audio_device_combo->blockSignals(true);
                audio_device_combo->setCurrentIndex(di);
                audio_device_combo->blockSignals(false);
                on_audio_device_changed(di);
            }
        }
        // Gain (slider 1..100)
        if(audio_gain_slider && settings.contains("AudioGain"))
        {
            int gv = settings["AudioGain"].get<int>();
            gv = std::max(1, std::min(100, gv));
            audio_gain_slider->blockSignals(true);
            audio_gain_slider->setValue(gv);
            audio_gain_slider->blockSignals(false);
            on_audio_gain_changed(gv);
        }
        // Input smoothing removed (now per-effect)
        // Bands (8/16/32)
        if(audio_bands_combo && settings.contains("AudioBands"))
        {
            int bc = settings["AudioBands"].get<int>();
            int idx = audio_bands_combo->findText(QString::number(bc));
            if(idx >= 0)
            {
                audio_bands_combo->blockSignals(true);
                audio_bands_combo->setCurrentIndex(idx);
                audio_bands_combo->blockSignals(false);
                on_audio_bands_changed(idx);
            }
        }
        // Standard Audio Controls
        if(audio_low_spin && settings.contains("AudioLowHz"))
        {
            audio_low_spin->blockSignals(true);
            audio_low_spin->setValue(settings["AudioLowHz"].get<int>());
            audio_low_spin->blockSignals(false);
        }
        if(audio_high_spin && settings.contains("AudioHighHz"))
        {
            audio_high_spin->blockSignals(true);
            audio_high_spin->setValue(settings["AudioHighHz"].get<int>());
            audio_high_spin->blockSignals(false);
        }
        if(audio_smooth_slider && settings.contains("AudioSmoothing"))
        {
            int sv = settings["AudioSmoothing"].get<int>();
            sv = std::max(0, std::min(99, sv));
            audio_smooth_slider->blockSignals(true);
            audio_smooth_slider->setValue(sv);
            audio_smooth_slider->blockSignals(false);
        }
        if(audio_falloff_slider && settings.contains("AudioFalloff"))
        {
            int fv = settings["AudioFalloff"].get<int>();
            fv = std::max(20, std::min(500, fv));
            audio_falloff_slider->blockSignals(true);
            audio_falloff_slider->setValue(fv);
            audio_falloff_slider->blockSignals(false);
        }
        if(audio_fft_combo && settings.contains("AudioFFTSize"))
        {
            int n = settings["AudioFFTSize"].get<int>();
            int idx = audio_fft_combo->findText(QString::number(n));
            if(idx >= 0)
            {
                audio_fft_combo->blockSignals(true);
                audio_fft_combo->setCurrentIndex(idx);
                audio_fft_combo->blockSignals(false);
                on_audio_fft_changed(idx);
            }
        }
        // Apply audio controls to effect UI if present
        on_audio_std_low_changed(audio_low_spin ? audio_low_spin->value() : 0.0);
        on_audio_std_smooth_changed(audio_smooth_slider ? audio_smooth_slider->value() : 60);
        on_audio_std_falloff_changed(audio_falloff_slider ? audio_falloff_slider->value() : 100);
    }
    // Populate origin combo after zones
    UpdateAudioEffectOriginCombo();

    layout->addStretch();

    tab_widget->addTab(audio_tab, "Audio");
}

void OpenRGB3DSpatialTab::on_audio_effect_start_clicked()
{
    if(!audio_effect_combo) return;
    int eff_idx = audio_effect_combo->currentIndex();

    // Index 0 is "None", actual effects start at index 1
    if(eff_idx <= 0 || eff_idx > 4) return;

    const char* class_names[] = { "AudioLevel3D", "SpectrumBars3D", "BeatPulse3D", "BandScan3D" };
    int actual_idx = eff_idx - 1;  // Adjust for "None" at index 0
    std::string class_name = class_names[actual_idx];

    // Build a single-effect stack from current audio effect UI settings
    effect_stack.clear();
    if(!current_audio_effect_ui) { SetupAudioEffectUI(eff_idx); }
    nlohmann::json settings = current_audio_effect_ui ? current_audio_effect_ui->SaveSettings() : nlohmann::json();
    SpatialEffect3D* eff = EffectListManager3D::get()->CreateEffect(class_name);
    if(!eff) return;
    std::unique_ptr<EffectInstance3D> inst = std::make_unique<EffectInstance3D>();
    inst->name = class_name;
    inst->effect_class_name = class_name;
    inst->effect.reset(eff);

    int target = -1;
    if(audio_effect_zone_combo)
    {
        QVariant data = audio_effect_zone_combo->itemData(audio_effect_zone_combo->currentIndex());
        if(data.isValid()) target = data.toInt();
    }
    inst->zone_index = target; // -1 all, >=0 zone, <=-1000 controller
    inst->blend_mode = BlendMode::ADD;
    inst->enabled = true;
    inst->id = next_effect_instance_id++;

    // Apply per-effect settings captured from UI
    eff->LoadSettings(settings);
    inst->saved_settings = std::make_unique<nlohmann::json>(settings);

    // Connect ScreenMirror3D screen preview signal to viewport
    if (class_name == "ScreenMirror3D")
    {
        ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(eff);
        if (screen_mirror && viewport)
        {
            connect(screen_mirror, &ScreenMirror3D::ScreenPreviewChanged,
                    viewport, &LEDViewport3D::SetShowScreenPreview);
            screen_mirror->SetReferencePoints(&reference_points);
        }
    }

    effect_stack.push_back(std::move(inst));
    UpdateEffectStackList();

    // Start rendering (ensure controllers in custom mode, start timer)
    bool has_valid_controller = false;
    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform) continue;
        if(transform->virtual_controller)
        {
            VirtualController3D* virtual_ctrl = transform->virtual_controller;
            const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();
            std::set<RGBController*> controllers_to_set;
            for(unsigned int i = 0; i < mappings.size(); i++)
            {
                if(mappings[i].controller) controllers_to_set.insert(mappings[i].controller);
            }
            for(std::set<RGBController*>::iterator it = controllers_to_set.begin(); it != controllers_to_set.end(); ++it)
            {
                (*it)->SetCustomMode();
                has_valid_controller = true;
            }
            continue;
        }
        RGBController* controller = transform->controller;
        if(!controller) continue;
        controller->SetCustomMode();
        has_valid_controller = true;
    }
    if(has_valid_controller && effect_timer && !effect_timer->isActive())
    {
        effect_time = 0.0f;
        effect_elapsed.restart();
        unsigned int target_fps = 30;
        for(size_t i = 0; i < effect_stack.size(); i++)
        {
            if(effect_stack[i] && effect_stack[i]->effect && effect_stack[i]->enabled)
            {
                unsigned int f = effect_stack[i]->effect->GetTargetFPSSetting();
                if(f > target_fps) target_fps = f;
            }
        }
        if(target_fps < 1) target_fps = 30;
        int interval_ms = (int)(1000 / target_fps);
        if(interval_ms < 1) interval_ms = 1;
        effect_timer->start(interval_ms);
    }
    running_audio_effect = eff;
    if(audio_effect_start_button) audio_effect_start_button->setEnabled(false);
    if(audio_effect_stop_button) audio_effect_stop_button->setEnabled(true);
}

void OpenRGB3DSpatialTab::on_audio_effect_stop_clicked()
{
    if(effect_timer && effect_timer->isActive()) effect_timer->stop();
    effect_stack.clear();
    running_audio_effect = nullptr;
    UpdateEffectStackList();
    if(audio_effect_start_button) audio_effect_start_button->setEnabled(true);
    if(audio_effect_stop_button) audio_effect_stop_button->setEnabled(false);
}

 

void OpenRGB3DSpatialTab::on_audio_device_changed(int index)
{
    AudioInputManager::instance()->setDeviceByIndex(index);
    // Persist setting
    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    settings["AudioDeviceIndex"] = index;
    resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", settings);
    resource_manager->GetSettingsManager()->SaveSettings();
}

void OpenRGB3DSpatialTab::on_audio_gain_changed(int value)
{
    float g = std::max(0.1f, std::min(10.0f, value / 10.0f));
    AudioInputManager::instance()->setGain(g);
    if(audio_gain_value_label)
    {
        audio_gain_value_label->setText(QString::number(g, 'f', (g < 10.0f ? 1 : 0)) + "x");
    }
    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    settings["AudioGain"] = value;
    resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", settings);
    resource_manager->GetSettingsManager()->SaveSettings();
}

// input smoothing removed

void OpenRGB3DSpatialTab::SetupAudioEffectUI(int eff_index)
{
    if(!audio_effect_controls_widget || !audio_effect_controls_layout) return;
    // Clear previous controls
    while(QLayoutItem* it = audio_effect_controls_layout->takeAt(0))
    {
        if(QWidget* w = it->widget()) { w->deleteLater(); }
        delete it;
    }
    current_audio_effect_ui = nullptr;

    // Handle "None" option (index 0)
    if(eff_index == 0)
    {
        // Hide effect controls and audio controls panel when "None" is selected
        if(audio_effect_controls_widget) audio_effect_controls_widget->hide();
        if(audio_std_group) audio_std_group->hide();
        return;
    }

    // Show controls for actual effects
    if(audio_effect_controls_widget) audio_effect_controls_widget->show();
    if(audio_std_group) audio_std_group->show();

    // Adjust for "None" being index 0 (subtract 1 from effect index)
    const char* class_names[] = { "AudioLevel3D", "SpectrumBars3D", "BeatPulse3D", "BandScan3D" };
    int actual_index = eff_index - 1;  // Offset for "None" at index 0
    if(actual_index < 0 || actual_index > 3) return;
    SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(class_names[actual_index]);
    if(!effect) return;
    effect->setParent(audio_effect_controls_widget);
    effect->CreateCommonEffectControls(audio_effect_controls_widget);
    effect->SetupCustomUI(audio_effect_controls_widget);
    current_audio_effect_ui = effect;
    // Hook Start/Stop from effect's own buttons to our audio handlers
    audio_effect_start_button = effect->GetStartButton();
    audio_effect_stop_button  = effect->GetStopButton();
    if(audio_effect_start_button)
    {
        // Avoid duplicate connections by disconnecting previous
        QObject::disconnect(audio_effect_start_button, nullptr, this, nullptr);
        connect(audio_effect_start_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_effect_start_clicked);
    }
    if(audio_effect_stop_button)
    {
        QObject::disconnect(audio_effect_stop_button, nullptr, this, nullptr);
        connect(audio_effect_stop_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_effect_stop_clicked);
        audio_effect_stop_button->setEnabled(false);
    }
    connect(effect, &SpatialEffect3D::ParametersChanged, this, &OpenRGB3DSpatialTab::OnAudioEffectParamsChanged);
    // Sync standard audio controls from effect settings
    if(audio_std_group && current_audio_effect_ui)
    {
        nlohmann::json s = current_audio_effect_ui->SaveSettings();
        if(audio_low_spin && s.contains("low_hz")) audio_low_spin->setValue(s["low_hz"].get<int>());
        if(audio_high_spin && s.contains("high_hz")) audio_high_spin->setValue(s["high_hz"].get<int>());
        if(audio_smooth_slider && s.contains("smoothing"))
        {
            int sv = (int)std::round(std::max(0.0f, std::min(0.99f, s["smoothing"].get<float>())) * 100.0f);
            audio_smooth_slider->setValue(sv);
        }
        if(audio_falloff_slider && s.contains("falloff"))
        {
            int fv = (int)std::round(std::max(0.2f, std::min(5.0f, s["falloff"].get<float>())) * 100.0f);
            audio_falloff_slider->setValue(fv);
        }
    }
    // Apply current origin selection to the new UI
    if(audio_effect_origin_combo)
    {
        int idx = audio_effect_origin_combo->currentIndex();
        on_audio_effect_origin_changed(idx);
    }
    audio_effect_controls_widget->updateGeometry();
    audio_effect_controls_widget->update();
}

void OpenRGB3DSpatialTab::UpdateAudioEffectOriginCombo()
{
    if(!audio_effect_origin_combo) return;
    audio_effect_origin_combo->blockSignals(true);
    audio_effect_origin_combo->clear();
    audio_effect_origin_combo->addItem("Room Center", QVariant(-1));
    for(size_t i = 0; i < reference_points.size(); ++i)
    {
        VirtualReferencePoint3D* ref = reference_points[i].get();
        if(!ref) continue;
        QString name = QString::fromStdString(ref->GetName());
        QString type = QString(VirtualReferencePoint3D::GetTypeName(ref->GetType()));
        audio_effect_origin_combo->addItem(QString("%1 (%2)").arg(name).arg(type), QVariant((int)i));
    }
    audio_effect_origin_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::UpdateAudioEffectZoneCombo()
{
    if(!audio_effect_zone_combo) return;

    /*---------------------------------------------------------*\
    | Save current selection to restore after rebuild         |
    \*---------------------------------------------------------*/
    int saved_index = audio_effect_zone_combo->currentIndex();
    if(saved_index < 0)
    {
        saved_index = 0;  // Default to "All Controllers"
    }

    audio_effect_zone_combo->blockSignals(true);
    audio_effect_zone_combo->clear();

    /*---------------------------------------------------------*\
    | Add "All Controllers" option with data -1               |
    \*---------------------------------------------------------*/
    audio_effect_zone_combo->addItem("All Controllers", QVariant(-1));

    /*---------------------------------------------------------*\
    | Add all zones with their index as data                  |
    \*---------------------------------------------------------*/
    if(zone_manager)
    {
        for(int i = 0; i < zone_manager->GetZoneCount(); i++)
        {
            Zone3D* zone = zone_manager->GetZone(i);
            if(zone)
            {
                QString zone_name = QString::fromStdString(zone->GetName());
                audio_effect_zone_combo->addItem(zone_name, QVariant(i));
            }
        }
    }

    /*---------------------------------------------------------*\
    | Add individual controllers with encoded index           |
    | Data is encoded as -(controller_index) - 1000           |
    \*---------------------------------------------------------*/
    for(unsigned int ci = 0; ci < controller_transforms.size(); ci++)
    {
        ControllerTransform* t = controller_transforms[ci].get();
        QString name;
        if(t && t->controller)
        {
            name = QString::fromStdString(t->controller->name);
        }
        else if(t && t->virtual_controller)
        {
            name = QString("[Virtual] ") + QString::fromStdString(t->virtual_controller->GetName());
        }
        else
        {
            name = QString("Controller %1").arg((int)ci);
        }
        audio_effect_zone_combo->addItem(QString("(Controller) %1").arg(name), QVariant(-(int)ci - 1000));
    }

    /*---------------------------------------------------------*\
    | Restore previous selection (or default to 0 if invalid) |
    \*---------------------------------------------------------*/
    if(saved_index < audio_effect_zone_combo->count())
    {
        audio_effect_zone_combo->setCurrentIndex(saved_index);
    }
    else
    {
        audio_effect_zone_combo->setCurrentIndex(0);  // Default to "All Controllers"
    }

    audio_effect_zone_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::on_audio_effect_origin_changed(int index)
{
    if(!audio_effect_origin_combo) return;
    int ref_idx = audio_effect_origin_combo->itemData(index).toInt();

    ReferenceMode mode = REF_MODE_ROOM_CENTER;
    Vector3D origin = {0.0f, 0.0f, 0.0f};
    if(ref_idx >= 0 && ref_idx < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref = reference_points[ref_idx].get();
        if(ref)
        {
            origin = ref->GetPosition();
            mode = REF_MODE_CUSTOM_POINT;
        }
    }

    if(current_audio_effect_ui)
    {
        current_audio_effect_ui->SetCustomReferencePoint(origin);
        current_audio_effect_ui->SetReferenceMode(mode);
    }
    if(running_audio_effect)
    {
        running_audio_effect->SetCustomReferencePoint(origin);
        running_audio_effect->SetReferenceMode(mode);
    }
    if(viewport) viewport->UpdateColors();
}

void OpenRGB3DSpatialTab::on_audio_start_clicked()
{
    AudioInputManager::instance()->start();
    audio_start_button->setEnabled(false);
    audio_stop_button->setEnabled(true);
}

void OpenRGB3DSpatialTab::on_audio_stop_clicked()
{
    AudioInputManager::instance()->stop();
    audio_start_button->setEnabled(true);
    audio_stop_button->setEnabled(false);
    if(audio_level_bar) audio_level_bar->setValue(0);
}

void OpenRGB3DSpatialTab::on_audio_level_updated(float level)
{
    if(!audio_level_bar) return;
    int v = (int)std::round(level * 1000.0f);
    audio_level_bar->setValue(v);
}

void OpenRGB3DSpatialTab::on_audio_bands_changed(int index)
{
    int bands = audio_bands_combo->itemText(index).toInt();
    AudioInputManager::instance()->setBandsCount(bands);
    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    settings["AudioBands"] = bands;
    resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", settings);
    resource_manager->GetSettingsManager()->SaveSettings();
}

//

void OpenRGB3DSpatialTab::LoadDevices()
{
    if(!resource_manager)
    {
        return;
    }

    UpdateAvailableControllersList();

    viewport->SetControllerTransforms(&controller_transforms);
}

void OpenRGB3DSpatialTab::UpdateAvailableControllersList()
{
    available_controllers_list->clear();

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        int unassigned_zones = GetUnassignedZoneCount(controllers[i]);
        int unassigned_leds = GetUnassignedLEDCount(controllers[i]);

        if(unassigned_leds > 0)
        {
            QString display_text = QString::fromStdString(controllers[i]->name) +
                                   QString(" [%1 zones, %2 LEDs available]").arg(unassigned_zones).arg(unassigned_leds);
            available_controllers_list->addItem(display_text);
        }
    }

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(virtual_controllers[i]->GetName()));
    }

    // Also update the custom controllers list
    UpdateCustomControllersList();
}

void OpenRGB3DSpatialTab::UpdateCustomControllersList()
{
    custom_controllers_list->clear();

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        custom_controllers_list->addItem(QString::fromStdString(virtual_controllers[i]->GetName()));
    }
}

void OpenRGB3DSpatialTab::UpdateDeviceList()
{
    LoadDevices();
}

void OpenRGB3DSpatialTab::on_controller_selected(int index)
{
    if(display_planes_list)
    {
        QSignalBlocker block(display_planes_list);
        display_planes_list->clearSelection();
    }
    current_display_plane_index = -1;
    if(viewport) viewport->SelectDisplayPlane(-1);

    if(index >= 0 && index < (int)controller_transforms.size())
    {
        controller_list->setCurrentRow(index);

        ControllerTransform* ctrl = controller_transforms[index].get();

        // Block signals to prevent feedback loops
        pos_x_spin->blockSignals(true);
        pos_y_spin->blockSignals(true);
        pos_z_spin->blockSignals(true);
        rot_x_spin->blockSignals(true);
        rot_y_spin->blockSignals(true);
        rot_z_spin->blockSignals(true);
        pos_x_slider->blockSignals(true);
        pos_y_slider->blockSignals(true);
        pos_z_slider->blockSignals(true);
        rot_x_slider->blockSignals(true);
        rot_y_slider->blockSignals(true);
        rot_z_slider->blockSignals(true);

        pos_x_spin->setValue(ctrl->transform.position.x);
        pos_y_spin->setValue(ctrl->transform.position.y);
        pos_z_spin->setValue(ctrl->transform.position.z);
        rot_x_spin->setValue(ctrl->transform.rotation.x);
        rot_y_spin->setValue(ctrl->transform.rotation.y);
        rot_z_spin->setValue(ctrl->transform.rotation.z);

        pos_x_slider->setValue((int)(ctrl->transform.position.x * 10));
        float constrained_y = std::max(ctrl->transform.position.y, (float)0.0f);
        pos_y_slider->setValue((int)(constrained_y * 10));
        pos_z_slider->setValue((int)(ctrl->transform.position.z * 10));
        rot_x_slider->setValue((int)(ctrl->transform.rotation.x));
        rot_y_slider->setValue((int)(ctrl->transform.rotation.y));
        rot_z_slider->setValue((int)(ctrl->transform.rotation.z));

        // Unblock signals
        pos_x_spin->blockSignals(false);
        pos_y_spin->blockSignals(false);
        pos_z_spin->blockSignals(false);
        rot_x_spin->blockSignals(false);
        rot_y_spin->blockSignals(false);
        rot_z_spin->blockSignals(false);
        pos_x_slider->blockSignals(false);
        pos_y_slider->blockSignals(false);
        pos_z_slider->blockSignals(false);
        rot_x_slider->blockSignals(false);
        rot_y_slider->blockSignals(false);
        rot_z_slider->blockSignals(false);

        // Clear reference point selection when controller is selected
        reference_points_list->blockSignals(true);
        reference_points_list->clearSelection();
        reference_points_list->blockSignals(false);

        // Enable rotation controls - controllers have rotation
        rot_x_slider->setEnabled(true);
        rot_y_slider->setEnabled(true);
        rot_z_slider->setEnabled(true);
        rot_x_spin->setEnabled(true);
        rot_y_spin->setEnabled(true);
        rot_z_spin->setEnabled(true);

        // Update LED spacing controls
        if(edit_led_spacing_x_spin)
        {
            edit_led_spacing_x_spin->setEnabled(true);
            edit_led_spacing_x_spin->blockSignals(true);
            edit_led_spacing_x_spin->setValue(ctrl->led_spacing_mm_x);
            edit_led_spacing_x_spin->blockSignals(false);
        }
        if(edit_led_spacing_y_spin)
        {
            edit_led_spacing_y_spin->setEnabled(true);
            edit_led_spacing_y_spin->blockSignals(true);
            edit_led_spacing_y_spin->setValue(ctrl->led_spacing_mm_y);
            edit_led_spacing_y_spin->blockSignals(false);
        }
        if(edit_led_spacing_z_spin)
        {
            edit_led_spacing_z_spin->setEnabled(true);
            edit_led_spacing_z_spin->blockSignals(true);
            edit_led_spacing_z_spin->setValue(ctrl->led_spacing_mm_z);
            edit_led_spacing_z_spin->blockSignals(false);
        }
        if(apply_spacing_button)
        {
            apply_spacing_button->setEnabled(true);
        }
    }
    else if(index == -1)
    {
        controller_list->setCurrentRow(-1);

        // Disable LED spacing controls
        if(edit_led_spacing_x_spin) edit_led_spacing_x_spin->setEnabled(false);
        if(edit_led_spacing_y_spin) edit_led_spacing_y_spin->setEnabled(false);
        if(edit_led_spacing_z_spin) edit_led_spacing_z_spin->setEnabled(false);
        if(apply_spacing_button) apply_spacing_button->setEnabled(false);
    }

    UpdateSelectionInfo();
    RefreshDisplayPlaneDetails();
}

void OpenRGB3DSpatialTab::on_controller_position_changed(int index, float x, float y, float z)
{
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        ControllerTransform* ctrl = controller_transforms[index].get();
        ctrl->transform.position.x = x;
        ctrl->transform.position.y = y;
        ctrl->transform.position.z = z;
        ctrl->world_positions_dirty = true;

        // Block signals to prevent feedback loops
        pos_x_spin->blockSignals(true);
        pos_y_spin->blockSignals(true);
        pos_z_spin->blockSignals(true);
        pos_x_slider->blockSignals(true);
        pos_y_slider->blockSignals(true);
        pos_z_slider->blockSignals(true);

        pos_x_spin->setValue(x);
        pos_y_spin->setValue(y);
        pos_z_spin->setValue(z);

        pos_x_slider->setValue((int)(x * 10));
        float constrained_y = std::max(y, (float)0.0f);
        pos_y_slider->setValue((int)(constrained_y * 10));
        pos_z_slider->setValue((int)(z * 10));

        // Unblock signals
        pos_x_spin->blockSignals(false);
        pos_y_spin->blockSignals(false);
        pos_z_spin->blockSignals(false);
        pos_x_slider->blockSignals(false);
        pos_y_slider->blockSignals(false);
        pos_z_slider->blockSignals(false);
    }
}

void OpenRGB3DSpatialTab::on_controller_rotation_changed(int index, float x, float y, float z)
{
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        ControllerTransform* ctrl = controller_transforms[index].get();
        ctrl->transform.rotation.x = x;
        ctrl->transform.rotation.y = y;
        ctrl->transform.rotation.z = z;
        ctrl->world_positions_dirty = true;

        // Block signals to prevent feedback loops
        rot_x_spin->blockSignals(true);
        rot_y_spin->blockSignals(true);
        rot_z_spin->blockSignals(true);
        rot_x_slider->blockSignals(true);
        rot_y_slider->blockSignals(true);
        rot_z_slider->blockSignals(true);

        rot_x_spin->setValue(x);
        rot_y_spin->setValue(y);
        rot_z_spin->setValue(z);

        rot_x_slider->setValue((int)x);
        rot_y_slider->setValue((int)y);
        rot_z_slider->setValue((int)z);

        // Unblock signals
        rot_x_spin->blockSignals(false);
        rot_y_spin->blockSignals(false);
        rot_z_spin->blockSignals(false);
        rot_x_slider->blockSignals(false);
        rot_y_slider->blockSignals(false);
        rot_z_slider->blockSignals(false);
    }
}


void OpenRGB3DSpatialTab::on_start_effect_clicked()
{
    /*---------------------------------------------------------*\
    | Check if a stack preset is selected                      |
    \*---------------------------------------------------------*/
    if(effect_combo && effect_combo->currentIndex() > 0)
    {
        QVariant data = effect_combo->itemData(effect_combo->currentIndex());
        if(data.isValid() && data.toInt() < 0)
        {
            /*---------------------------------------------------------*\
            | This is a stack preset - load it and start rendering     |
            \*---------------------------------------------------------*/
            int preset_index = -(data.toInt() + 1);
            if(preset_index >= 0 && preset_index < (int)stack_presets.size())
            {
                StackPreset3D* preset = stack_presets[preset_index].get();

                /*---------------------------------------------------------*\
                | Clear current stack                                      |
                \*---------------------------------------------------------*/
                effect_stack.clear();

                /*---------------------------------------------------------*\
                | Load preset effects (deep copy)                          |
                \*---------------------------------------------------------*/
                

                for(unsigned int i = 0; i < preset->effect_instances.size(); i++)
                {
                    nlohmann::json instance_json = preset->effect_instances[i]->ToJson();
                    std::unique_ptr<EffectInstance3D> copied_instance = EffectInstance3D::FromJson(instance_json);
                    if(copied_instance)
                    {
                        // Connect ScreenMirror3D screen preview signal to viewport
                        if (copied_instance->effect_class_name == "ScreenMirror3D" && copied_instance->effect)
                        {
                            ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(copied_instance->effect.get());
                            if (screen_mirror && viewport)
                            {
                                connect(screen_mirror, &ScreenMirror3D::ScreenPreviewChanged,
                                        viewport, &LEDViewport3D::SetShowScreenPreview);
                                screen_mirror->SetReferencePoints(&reference_points);
                            }
                        }

                        effect_stack.push_back(std::move(copied_instance));
                    }
                    else
                    {
                    }
                }

                /*---------------------------------------------------------*\
                | Update Effect Stack tab UI (if visible)                  |
                \*---------------------------------------------------------*/
                UpdateEffectStackList();
                if(!effect_stack.empty())
                {
                    effect_stack_list->setCurrentRow(0);
                }

                /*---------------------------------------------------------*\
                | Put all controllers in direct control mode               |
                \*---------------------------------------------------------*/
                bool has_valid_controller = false;
                for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
                {
                    ControllerTransform* transform = controller_transforms[ctrl_idx].get();
                    if(!transform)
                    {
                        continue;
                    }

                    // Handle virtual controllers - they map to physical controllers
                    if(transform->virtual_controller)
                    {
                        VirtualController3D* virtual_ctrl = transform->virtual_controller;
                        const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();

                        // Set all physical controllers mapped to this virtual controller to direct mode
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

                    // Handle regular controllers
                    RGBController* controller = transform->controller;
                    if(!controller)
                    {
                        continue;
                    }

                    controller->SetCustomMode();
                    has_valid_controller = true;
                }

                

                /*---------------------------------------------------------*\
                | Start effect timer                                       |
                \*---------------------------------------------------------*/
                if(effect_timer && !effect_timer->isActive())
                {
                    effect_time = 0.0f;
                    effect_elapsed.restart();
                    // Compute timer interval from stack effects (use highest requested FPS)
                    unsigned int target_fps = 30;
                    for(size_t i = 0; i < effect_stack.size(); i++)
                    {
                        if(effect_stack[i] && effect_stack[i]->effect && effect_stack[i]->enabled)
                        {
                            unsigned int f = effect_stack[i]->effect->GetTargetFPSSetting();
                            if(f > target_fps) target_fps = f;
                        }
                    }
                    if(target_fps < 1) target_fps = 30;
                    int interval_ms = (int)(1000 / target_fps);
                    if(interval_ms < 1) interval_ms = 1;
                    effect_timer->start(interval_ms);
                    
                }
                else
                {
                    
                }

                /*---------------------------------------------------------*\
                | Update button states                                     |
                \*---------------------------------------------------------*/
                start_effect_button->setEnabled(false);
                stop_effect_button->setEnabled(true);

                
                return;
            }
        }
    }

    /*---------------------------------------------------------*\
    | Regular effect handling                                  |
    \*---------------------------------------------------------*/
    if(!current_effect_ui)
    {
        QMessageBox::warning(this, "No Effect Selected", "Please select an effect before starting.");
        return;
    }

    if(controller_transforms.empty())
    {
        QMessageBox::warning(this, "No Controllers", "Please add controllers to the 3D scene before starting effects.");
        return;
    }

    /*---------------------------------------------------------*\
    | Put all controllers in direct control mode               |
    \*---------------------------------------------------------*/
    bool has_valid_controller = false;
    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform)
        {
            continue;
        }

        // Handle virtual controllers - they map to physical controllers
        if(transform->virtual_controller)
        {
            VirtualController3D* virtual_ctrl = transform->virtual_controller;
            const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();

            // Set all physical controllers mapped to this virtual controller to direct mode
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

        // Handle regular controllers
        RGBController* controller = transform->controller;
        if(!controller)
        {
            continue;
        }

        controller->SetCustomMode();
        has_valid_controller = true;
    }

    if(!has_valid_controller)
    {
        QMessageBox::warning(this, "No Valid Controllers", "No controllers are available for effects.");
        return;
    }

    /*---------------------------------------------------------*\
    | Start the effect                                         |
    \*---------------------------------------------------------*/
    effect_running = true;
    effect_time = 0.0f;
    effect_elapsed.restart();

    /*---------------------------------------------------------*\
    | Set timer interval from effect FPS (default 30 FPS)      |
    \*---------------------------------------------------------*/
    {
        unsigned int target_fps = current_effect_ui ? current_effect_ui->GetTargetFPSSetting() : 30;
        if(target_fps < 1) target_fps = 30;
        int interval_ms = (int)(1000 / target_fps);
        if(interval_ms < 1) interval_ms = 1;
        effect_timer->start(interval_ms);
    }

    /*---------------------------------------------------------*\
    | Update UI                                                |
    \*---------------------------------------------------------*/
    start_effect_button->setEnabled(false);
    stop_effect_button->setEnabled(true);

}

void OpenRGB3DSpatialTab::on_stop_effect_clicked()
{
    /*---------------------------------------------------------*\
    | Check if a stack preset is currently running             |
    \*---------------------------------------------------------*/
    if(effect_combo && effect_combo->currentIndex() > 0)
    {
        QVariant data = effect_combo->itemData(effect_combo->currentIndex());
        if(data.isValid() && data.toInt() < 0)
        {
            /*---------------------------------------------------------*\
            | This is a stack preset - stop and clear the stack        |
            \*---------------------------------------------------------*/
            effect_timer->stop();

            /*---------------------------------------------------------*\
            | Clear effect stack                                       |
            \*---------------------------------------------------------*/
            effect_stack.clear();
            UpdateEffectStackList();

            /*---------------------------------------------------------*\
            | Update button states                                     |
            \*---------------------------------------------------------*/
            start_effect_button->setEnabled(true);
            stop_effect_button->setEnabled(false);

            return;
        }
    }

    /*---------------------------------------------------------*\
    | Regular effect stop handling                             |
    \*---------------------------------------------------------*/
    effect_running = false;
    effect_timer->stop();

    /*---------------------------------------------------------*\
    | Update UI                                                |
    \*---------------------------------------------------------*/
    start_effect_button->setEnabled(true);
    stop_effect_button->setEnabled(false);
}

void OpenRGB3DSpatialTab::on_effect_updated()
{
    viewport->UpdateColors();
}

void OpenRGB3DSpatialTab::on_effect_timer_timeout()
{
    // Advance time based on real elapsed time for smooth animation
    qint64 ms = effect_elapsed.isValid() ? effect_elapsed.restart() : 33;
    if(ms <= 0) { ms = 33; }
    float dt = static_cast<float>(ms) / 1000.0f;
    if(dt > 0.1f) dt = 0.1f; // clamp spikes
    effect_time += dt;
    /*---------------------------------------------------------*\
    | Check if we should render effect stack instead of       |
    | single effect                                            |
    \*---------------------------------------------------------*/
    bool has_stack_effects = false;
    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        if(effect_stack[i]->enabled && effect_stack[i]->effect)
        {
            has_stack_effects = true;
            break;
        }
    }

    if(has_stack_effects)
    {
        /*---------------------------------------------------------*\
        | Render effect stack (multi-effect mode)                 |
        \*---------------------------------------------------------*/
        
        RenderEffectStack();
        return;
    }
    else
    {
        
    }

    /*---------------------------------------------------------*\
    | Fall back to single effect rendering (Effects tab)       |
    \*---------------------------------------------------------*/
    

    if(!effect_running || !current_effect_ui)
    {
        return;
    }

    

    /*---------------------------------------------------------*\
    | Safety: Check if we have any controllers                |
    \*---------------------------------------------------------*/
    if(controller_transforms.empty())
    {
        return; // No controllers to update
    }

    /*---------------------------------------------------------*\
    | Safety: Verify effect timer and viewport are valid      |
    \*---------------------------------------------------------*/
    if(!effect_timer || !viewport)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect timer or viewport is null, stopping effect");
        on_stop_effect_clicked();
        return;
    }

    // effect_time already advanced at timer start

    /*---------------------------------------------------------*\
    | Calculate room bounds for effects                        |
    | Uses same corner-origin system as Effect Stack          |
    \*---------------------------------------------------------*/
    float grid_min_x = 0.0f, grid_max_x = 0.0f;
    float grid_min_y = 0.0f, grid_max_y = 0.0f;
    float grid_min_z = 0.0f, grid_max_z = 0.0f;

    if(use_manual_room_size)
    {
        /*---------------------------------------------------------*\
        | Use manually configured room dimensions                  |
        | Origin at front-left-floor corner (0,0,0)               |
        | IMPORTANT: Convert millimeters to grid units (/ 10.0f)  |
        | LED world_position uses grid units, not millimeters!    |
        \*---------------------------------------------------------*/
        grid_min_x = 0.0f;
        grid_max_x = manual_room_width / grid_scale_mm;  // Convert mm to grid units
        grid_min_y = 0.0f;
        grid_max_y = manual_room_depth / grid_scale_mm;  // Convert mm to grid units
        grid_min_z = 0.0f;
        grid_max_z = manual_room_height / grid_scale_mm; // Convert mm to grid units

        
    }
    else
    {
        /*---------------------------------------------------------*\
        | Auto-detect from LED positions                           |
        \*---------------------------------------------------------*/
        bool has_leds = false;

        // Update world positions first
        for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
        {
            ControllerTransform* transform = controller_transforms[ctrl_idx].get();
            if(transform && transform->world_positions_dirty)
            {
                ControllerLayout3D::UpdateWorldPositions(transform);
            }
        }

        // Find min/max positions from ALL LEDs
        for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
        {
            ControllerTransform* transform = controller_transforms[ctrl_idx].get();
            if(!transform) continue;

            for(unsigned int led_idx = 0; led_idx < transform->led_positions.size(); led_idx++)
            {
                float x = transform->led_positions[led_idx].world_position.x;
                float y = transform->led_positions[led_idx].world_position.y;
                float z = transform->led_positions[led_idx].world_position.z;

                if(!has_leds)
                {
                    grid_min_x = grid_max_x = x;
                    grid_min_y = grid_max_y = y;
                    grid_min_z = grid_max_z = z;
                    has_leds = true;
                }
                else
                {
                    if(x < grid_min_x) grid_min_x = x;
                    if(x > grid_max_x) grid_max_x = x;
                    if(y < grid_min_y) grid_min_y = y;
                    if(y > grid_max_y) grid_max_y = y;
                    if(z < grid_min_z) grid_min_z = z;
                    if(z > grid_max_z) grid_max_z = z;
                }
            }
        }

        if(!has_leds)
        {
            // Fallback if no LEDs found (convert default mm to grid units)
            grid_min_x = 0.0f;
            grid_max_x = 1000.0f / grid_scale_mm;
            grid_min_y = 0.0f;
            grid_max_y = 1000.0f / grid_scale_mm;
            grid_min_z = 0.0f;
            grid_max_z = 1000.0f / grid_scale_mm;
        }

        
    }

    // Create grid context for effects
    GridContext3D grid_context(grid_min_x, grid_max_x, grid_min_y, grid_max_y, grid_min_z, grid_max_z);

    

    /*---------------------------------------------------------*\
    | Configure effect origin mode                             |
    | Pass absolute world coords to CalculateColorGrid         |
    \*---------------------------------------------------------*/
    if(current_effect_ui)
    {
        ReferenceMode mode = REF_MODE_ROOM_CENTER;
        Vector3D ref_origin = {0.0f, 0.0f, 0.0f};

        if(effect_origin_combo)
        {
            int index = effect_origin_combo->currentIndex();
            int ref_point_idx = effect_origin_combo->itemData(index).toInt();
            if(ref_point_idx >= 0 && ref_point_idx < (int)reference_points.size())
            {
                VirtualReferencePoint3D* ref_point = reference_points[ref_point_idx].get();
                ref_origin = ref_point->GetPosition();
                mode = REF_MODE_USER_POSITION;
                
            }
            else
            {
                mode = REF_MODE_ROOM_CENTER;
                
            }
        }

        current_effect_ui->SetGlobalReferencePoint(ref_origin);
        current_effect_ui->SetReferenceMode(mode);
    }

    /*---------------------------------------------------------*\
    | Determine which controllers to apply effects to based   |
    | on the selected zone                                     |
    \*---------------------------------------------------------*/
    std::vector<int> allowed_controllers;

    if(!effect_zone_combo || !zone_manager)
    {
        // Safety: If UI not ready, allow all controllers
        for(unsigned int i = 0; i < controller_transforms.size(); i++)
        {
            allowed_controllers.push_back(i);
        }
    }
    else
    {
        int combo_idx = effect_zone_combo->currentIndex();
        int zone_count = zone_manager ? zone_manager->GetZoneCount() : 0;

        /*---------------------------------------------------------*\
        | Safety: If index is invalid, default to all controllers |
        \*---------------------------------------------------------*/
        if(combo_idx < 0 || combo_idx >= effect_zone_combo->count())
        {
            for(unsigned int i = 0; i < controller_transforms.size(); i++)
            {
                allowed_controllers.push_back(i);
            }
        }
        else if(combo_idx == 0)
        {
            /*---------------------------------------------------------*\
            | "All Controllers" selected - allow all                   |
            \*---------------------------------------------------------*/
            for(unsigned int i = 0; i < controller_transforms.size(); i++)
            {
                allowed_controllers.push_back(i);
            }
        }
        else if(zone_count > 0 && combo_idx >= 1 && combo_idx <= zone_count)
        {
            /*---------------------------------------------------------*\
            | Zone selected - get controllers from zone manager       |
            | Zone indices: combo index 1 = zone 0, etc.              |
            \*---------------------------------------------------------*/
            Zone3D* zone = zone_manager->GetZone(combo_idx - 1);
            if(zone)
            {
                allowed_controllers = zone->GetControllers();
            }
            else
            {
                /*---------------------------------------------------------*\
                | Zone not found - allow all as fallback                  |
                \*---------------------------------------------------------*/
                for(unsigned int i = 0; i < controller_transforms.size(); i++)
                {
                    allowed_controllers.push_back(i);
                }
            }
        }
        else
        {
            /*---------------------------------------------------------*\
            | Individual controller selected                           |
            | Combo index = zone_count + 1 + controller_index          |
            \*---------------------------------------------------------*/
            int ctrl_idx = combo_idx - zone_count - 1;
            if(ctrl_idx >= 0 && ctrl_idx < (int)controller_transforms.size())
            {
                allowed_controllers.push_back(ctrl_idx);
            }
            else
            {
                /*---------------------------------------------------------*\
                | Invalid controller index - allow all as fallback        |
                \*---------------------------------------------------------*/
                for(unsigned int i = 0; i < controller_transforms.size(); i++)
                {
                    allowed_controllers.push_back(i);
                }
            }
        }
    }

    // Now map each controller's LEDs to the unified grid and apply effects
    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        // Skip controllers not in the selected zone
        if(std::find(allowed_controllers.begin(), allowed_controllers.end(), (int)ctrl_idx) == allowed_controllers.end())
        {
            continue; // Controller not in selected zone
        }

        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform)
        {
            continue;
        }

        // Handle virtual controllers
        if(transform->virtual_controller && !transform->controller)
        {
            VirtualController3D* virtual_ctrl = transform->virtual_controller;
            const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();

            // Update cached world positions if dirty
            if(transform->world_positions_dirty)
            {
                ControllerLayout3D::UpdateWorldPositions(transform);
            }

            // Apply effects to each virtual LED
            for(unsigned int mapping_idx = 0; mapping_idx < mappings.size(); mapping_idx++)
            {
                const GridLEDMapping& mapping = mappings[mapping_idx];
                if(!mapping.controller) continue;

                // Use pre-computed world position from cached LED positions
                if(mapping_idx < transform->led_positions.size())
                {
                    float x = transform->led_positions[mapping_idx].world_position.x;
                    float y = transform->led_positions[mapping_idx].world_position.y;
                    float z = transform->led_positions[mapping_idx].world_position.z;

                    // Only apply effects to LEDs within the room-centered grid bounds
                    if(x >= grid_min_x && x <= grid_max_x &&
                       y >= grid_min_y && y <= grid_max_y &&
                       z >= grid_min_z && z <= grid_max_z)
                    {
                        // Safety: Ensure controller is still valid
                        if(!mapping.controller || mapping.controller->zones.empty() || mapping.controller->colors.empty())
                        {
                            continue;
                        }

                        // Calculate effect color using grid-aware method (world coords)
                        RGBColor color = current_effect_ui->CalculateColorGrid(x, y, z, effect_time, grid_context);;
color = current_effect_ui->PostProcessColorGrid(x, y, z, color, grid_context);

                        // Apply color to the mapped physical LED (with bounds checking)
                        if(mapping.zone_idx < mapping.controller->zones.size())
                        {
                            unsigned int led_global_idx = mapping.controller->zones[mapping.zone_idx].start_idx + mapping.led_idx;
                            if(led_global_idx < mapping.controller->colors.size())
                            {
                                mapping.controller->colors[led_global_idx] = color;
                            }
                        }
                    }
                }
            }

            // Update the physical controllers that this virtual controller maps to
            std::set<RGBController*> updated_controllers;
            for(unsigned int i = 0; i < mappings.size(); i++)
            {
                if(mappings[i].controller && updated_controllers.find(mappings[i].controller) == updated_controllers.end())
                {
                    mappings[i].controller->UpdateLEDs();
                    updated_controllers.insert(mappings[i].controller);
                }
            }

            continue;
        }

        // Handle regular controllers
        RGBController* controller = transform->controller;
        if(!controller || controller->zones.empty() || controller->colors.empty())
        {
            continue;
        }

        /*---------------------------------------------------------*\
        | Update cached world positions if dirty                  |
        \*---------------------------------------------------------*/
        if(transform->world_positions_dirty)
        {
            ControllerLayout3D::UpdateWorldPositions(transform);
        }

        /*---------------------------------------------------------*\
        | Calculate colors for each LED using cached positions    |
        \*---------------------------------------------------------*/
        for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); led_pos_idx++)
        {
            LEDPosition3D& led_position = transform->led_positions[led_pos_idx];

            /*---------------------------------------------------------*\
            | Use pre-computed world position (no calculation!)      |
            \*---------------------------------------------------------*/
            float x = led_position.world_position.x;
            float y = led_position.world_position.y;
            float z = led_position.world_position.z;

            // Validate zone index before accessing
            if(led_position.zone_idx >= controller->zones.size())
            {
                continue; // Skip invalid zone
            }

            // Get the actual LED index for color updates
            unsigned int led_global_idx = controller->zones[led_position.zone_idx].start_idx + led_position.led_idx;

            // Only apply effects to LEDs within the room-centered grid bounds
            if(x >= grid_min_x && x <= grid_max_x &&
               y >= grid_min_y && y <= grid_max_y &&
               z >= grid_min_z && z <= grid_max_z)
            {
                /*---------------------------------------------------------*\
                | Calculate effect color using grid-aware method          |
                \*---------------------------------------------------------*/
                RGBColor color = current_effect_ui->CalculateColorGrid(x, y, z, effect_time, grid_context);
                color = current_effect_ui->PostProcessColorGrid(x, y, z, color, grid_context);

                // Apply color to the correct LED using the global LED index
                if(led_global_idx < controller->colors.size())
                {
                    controller->colors[led_global_idx] = color;
                }
            }
            // LEDs outside the grid remain unlit (keep their current color)
        }

        /*---------------------------------------------------------*\
        | Update the controller                                    |
        \*---------------------------------------------------------*/
        controller->UpdateLEDs();
    }

    /*---------------------------------------------------------*\
    | Update the 3D viewport                                   |
    \*---------------------------------------------------------*/
    viewport->UpdateColors();
}

void OpenRGB3DSpatialTab::on_granularity_changed(int)
{
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::on_led_spacing_preset_changed(int index)
{
    if(!led_spacing_x_spin || !led_spacing_y_spin || !led_spacing_z_spin)
    {
        return;
    }

    // Block signals to prevent triggering changes while updating
    led_spacing_x_spin->blockSignals(true);
    led_spacing_y_spin->blockSignals(true);
    led_spacing_z_spin->blockSignals(true);

    switch(index)
    {
        case 1: // Dense Strip (10mm)
            led_spacing_x_spin->setValue(10.0);
            led_spacing_y_spin->setValue(0.0);
            led_spacing_z_spin->setValue(0.0);
            break;
        case 2: // Keyboard (19mm)
            led_spacing_x_spin->setValue(19.0);
            led_spacing_y_spin->setValue(0.0);
            led_spacing_z_spin->setValue(19.0);
            break;
        case 3: // Sparse Strip (33mm)
            led_spacing_x_spin->setValue(33.0);
            led_spacing_y_spin->setValue(0.0);
            led_spacing_z_spin->setValue(0.0);
            break;
        case 4: // LED Cube (50mm)
            led_spacing_x_spin->setValue(50.0);
            led_spacing_y_spin->setValue(50.0);
            led_spacing_z_spin->setValue(50.0);
            break;
        case 0: // Custom
        default:
            // Do nothing - user controls manually
            break;
    }

    led_spacing_x_spin->blockSignals(false);
    led_spacing_y_spin->blockSignals(false);
    led_spacing_z_spin->blockSignals(false);
}

void OpenRGB3DSpatialTab::UpdateAvailableItemCombo()
{
    item_combo->clear();

    int list_row = available_controllers_list->currentRow();
    if(list_row < 0)
    {
        return;
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    int actual_ctrl_idx = -1;
    int visible_idx = 0;

    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        if(GetUnassignedLEDCount(controllers[i]) > 0)
        {
            if(visible_idx == list_row)
            {
                actual_ctrl_idx = i;
                break;
            }
            visible_idx++;
        }
    }

    if(actual_ctrl_idx >= 0)
    {
        RGBController* controller = controllers[actual_ctrl_idx];
        int granularity = granularity_combo->currentIndex();

        if(granularity == 0)
        {
            if(!IsItemInScene(controller, granularity, 0))
            {
                item_combo->addItem(QString::fromStdString(controller->name), QVariant::fromValue(qMakePair(actual_ctrl_idx, 0)));
            }
        }
        else if(granularity == 1)
        {
            for(unsigned int i = 0; i < controller->zones.size(); i++)
            {
                if(!IsItemInScene(controller, granularity, i))
                {
                    item_combo->addItem(QString::fromStdString(controller->zones[i].name), QVariant::fromValue(qMakePair(actual_ctrl_idx, (int)i)));
                }
            }
        }
        else if(granularity == 2)
        {
            for(unsigned int i = 0; i < controller->leds.size(); i++)
            {
                if(!IsItemInScene(controller, granularity, i))
                {
                    item_combo->addItem(QString::fromStdString(controller->leds[i].name), QVariant::fromValue(qMakePair(actual_ctrl_idx, (int)i)));
                }
            }
        }
        return;
    }

    int virtual_offset = visible_idx;
    if(list_row >= virtual_offset && list_row < virtual_offset + (int)virtual_controllers.size())
    {
        item_combo->addItem("Whole Device", QVariant::fromValue(qMakePair(-1, list_row - virtual_offset)));
    }
}

void OpenRGB3DSpatialTab::on_add_clicked()
{
    int granularity = granularity_combo->currentIndex();
    int combo_idx = item_combo->currentIndex();

    if(combo_idx < 0)
    {
        QMessageBox::information(this, "No Item Selected",
                                "Please select a controller, zone, or LED to add to the scene.");
        return;
    }

    QPair<int, int> data = item_combo->currentData().value<QPair<int, int>>();
    int ctrl_idx = data.first;
    int item_row = data.second;

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    if(ctrl_idx < 0)
    {
        if(item_row >= (int)virtual_controllers.size())
        {
            return;
        }

        VirtualController3D* virtual_ctrl = virtual_controllers[item_row].get();

        std::unique_ptr<ControllerTransform> ctrl_transform = std::make_unique<ControllerTransform>();
        ctrl_transform->controller = nullptr;
        ctrl_transform->virtual_controller = virtual_ctrl;
        ctrl_transform->transform.position = {-5.0f, 0.0f, -5.0f}; // Snapped to 0.5 grid
        ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
        ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};

        // Set LED spacing from UI
        ctrl_transform->led_spacing_mm_x = led_spacing_x_spin ? (float)led_spacing_x_spin->value() : 10.0f;
        ctrl_transform->led_spacing_mm_y = led_spacing_y_spin ? (float)led_spacing_y_spin->value() : 0.0f;
        ctrl_transform->led_spacing_mm_z = led_spacing_z_spin ? (float)led_spacing_z_spin->value() : 0.0f;

        // Virtual controllers always use whole device granularity
        ctrl_transform->granularity = -1;  // -1 = virtual controller
        ctrl_transform->item_idx = -1;

        ctrl_transform->led_positions = virtual_ctrl->GenerateLEDPositions(grid_scale_mm);
        ctrl_transform->world_positions_dirty = true;

        int hue = (controller_transforms.size() * 137) % 360;
        QColor color = QColor::fromHsv(hue, 200, 255);
        ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();

        // Pre-compute world positions before adding to vector
        ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());

        controller_transforms.push_back(std::move(ctrl_transform));

        QString name = QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName());
        QListWidgetItem* list_item = new QListWidgetItem(name);
        controller_list->addItem(list_item);

            viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();
        UpdateAvailableControllersList();
        UpdateAvailableItemCombo();
        return;
    }

    if(ctrl_idx >= (int)controllers.size())
    {
        return;
    }

    RGBController* controller = controllers[ctrl_idx];

    std::unique_ptr<ControllerTransform> ctrl_transform = std::make_unique<ControllerTransform>();
    ctrl_transform->controller = controller;
    ctrl_transform->virtual_controller = nullptr;
    ctrl_transform->transform.position = {-5.0f, 0.0f, -5.0f}; // Snapped to 0.5 grid
    ctrl_transform->transform.rotation = {0.0f, 0.0f, 0.0f};
    ctrl_transform->transform.scale = {1.0f, 1.0f, 1.0f};

    // Set LED spacing from UI
    ctrl_transform->led_spacing_mm_x = led_spacing_x_spin ? (float)led_spacing_x_spin->value() : 10.0f;
    ctrl_transform->led_spacing_mm_y = led_spacing_y_spin ? (float)led_spacing_y_spin->value() : 0.0f;
    ctrl_transform->led_spacing_mm_z = led_spacing_z_spin ? (float)led_spacing_z_spin->value() : 0.0f;

    // Set granularity
    ctrl_transform->granularity = granularity;
    ctrl_transform->item_idx = item_row;

    QString name;

    if(granularity == 0)
    {
        ctrl_transform->led_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y, custom_grid_z,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);
        name = QString("[Device] ") + QString::fromStdString(controller->name);
    }
    else if(granularity == 1)
    {
        if(item_row >= (int)controller->zones.size())
        {
            return;  // ctrl_transform auto-deleted
        }

        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y, custom_grid_z,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);
        zone* z = &controller->zones[item_row];

        for(unsigned int i = 0; i < all_positions.size(); i++)
        {
            if(all_positions[i].zone_idx == (unsigned int)item_row)
            {
                ctrl_transform->led_positions.push_back(all_positions[i]);
            }
        }
        

        name = QString("[Zone] ") + QString::fromStdString(controller->name) + " - " + QString::fromStdString(z->name);
    }
    else if(granularity == 2)
    {
        if(item_row >= (int)controller->leds.size())
        {
            return;  // ctrl_transform auto-deleted
        }

        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            controller, custom_grid_x, custom_grid_y, custom_grid_z,
            ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
            grid_scale_mm);

        for(unsigned int i = 0; i < all_positions.size(); i++)
        {
            unsigned int global_led_idx = controller->zones[all_positions[i].zone_idx].start_idx + all_positions[i].led_idx;
            if(global_led_idx == (unsigned int)item_row)
            {
                ctrl_transform->led_positions.push_back(all_positions[i]);
                break;
            }
        }

        name = QString("[LED] ") + QString::fromStdString(controller->name) + " - " + QString::fromStdString(controller->leds[item_row].name);
    }

    int hue = (controller_transforms.size() * 137) % 360;
    QColor color = QColor::fromHsv(hue, 200, 255);
    ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();

    ctrl_transform->world_positions_dirty = true;
    ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());

    controller_transforms.push_back(std::move(ctrl_transform));

    QListWidgetItem* item = new QListWidgetItem(name);
    controller_list->addItem(item);

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();

}

void OpenRGB3DSpatialTab::on_remove_controller_clicked()
{
    int selected_row = controller_list->currentRow();
    if(selected_row < 0 || selected_row >= (int)controller_transforms.size())
    {
        return;
    }

    controller_transforms.erase(controller_transforms.begin() + selected_row);  // Auto-deleted

    controller_list->takeItem(selected_row);

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::on_remove_controller_from_viewport(int index)
{
    if(index < 0 || index >= (int)controller_transforms.size())
    {
        return;
    }

    controller_transforms.erase(controller_transforms.begin() + index);

    controller_list->takeItem(index);

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::on_clear_all_clicked()
{
    controller_transforms.clear();

    controller_list->clear();

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::on_apply_spacing_clicked()
{
    int selected_row = controller_list->currentRow();
    if(selected_row < 0 || selected_row >= (int)controller_transforms.size())
    {
        return;
    }

    ControllerTransform* ctrl = controller_transforms[selected_row].get();

    // Update LED spacing values
    ctrl->led_spacing_mm_x = edit_led_spacing_x_spin ? (float)edit_led_spacing_x_spin->value() : 10.0f;
    ctrl->led_spacing_mm_y = edit_led_spacing_y_spin ? (float)edit_led_spacing_y_spin->value() : 0.0f;
    ctrl->led_spacing_mm_z = edit_led_spacing_z_spin ? (float)edit_led_spacing_z_spin->value() : 0.0f;

    // Regenerate LED positions with new spacing
    RegenerateLEDPositions(ctrl);

    // Mark world positions dirty so effects and viewport can recompute
    ctrl->world_positions_dirty = true;

    // Update viewport
    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
}

void OpenRGB3DSpatialTab::on_save_layout_clicked()
{
    // Update all settings from UI before saving
    if(grid_x_spin) custom_grid_x = grid_x_spin->value();
    if(grid_y_spin) custom_grid_y = grid_y_spin->value();
    if(grid_z_spin) custom_grid_z = grid_z_spin->value();

    // User position is now handled through reference points system

    bool ok;
    QString profile_name = QInputDialog::getText(this, "Save Layout Profile",
                                                 "Profile name:", QLineEdit::Normal,
                                                 layout_profiles_combo->currentText(), &ok);

    if(!ok || profile_name.isEmpty())
    {
        return;
    }

    std::string layout_path = GetLayoutPath(profile_name.toStdString());

    /*---------------------------------------------------------*\
    | Check if profile already exists                          |
    \*---------------------------------------------------------*/
    if(filesystem::exists(layout_path))
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Overwrite Profile",
            QString("Layout profile \"%1\" already exists. Overwrite?").arg(profile_name),
            QMessageBox::Yes | QMessageBox::No);

        if(reply != QMessageBox::Yes)
        {
            return;
        }
    }

    SaveLayout(layout_path);

    PopulateLayoutDropdown();

    int index = layout_profiles_combo->findText(profile_name);
    if(index >= 0)
    {
        layout_profiles_combo->setCurrentIndex(index);
    }

    // Save the selected profile name to settings
    SaveCurrentLayoutName();

    QMessageBox::information(this, "Layout Saved",
                            QString("Profile '%1' saved to plugins directory").arg(profile_name));
}

void OpenRGB3DSpatialTab::on_load_layout_clicked()
{
    QString profile_name = layout_profiles_combo->currentText();

    if(profile_name.isEmpty())
    {
        QMessageBox::warning(this, "No Profile Selected", "Please select a profile to load");
        return;
    }

    std::string layout_path = GetLayoutPath(profile_name.toStdString());
    QFileInfo check_file(QString::fromStdString(layout_path));

    if(!check_file.exists())
    {
        QMessageBox::warning(this, "Profile Not Found", "Selected profile file not found");
        return;
    }

    LoadLayout(layout_path);
    QMessageBox::information(this, "Layout Loaded",
                            QString("Profile '%1' loaded successfully").arg(profile_name));
}

void OpenRGB3DSpatialTab::on_delete_layout_clicked()
{
    QString profile_name = layout_profiles_combo->currentText();

    if(profile_name.isEmpty())
    {
        QMessageBox::warning(this, "No Profile Selected", "Please select a profile to delete");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Profile",
                                        QString("Are you sure you want to delete profile '%1'?").arg(profile_name),
                                        QMessageBox::Yes | QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        std::string layout_path = GetLayoutPath(profile_name.toStdString());
        QFile file(QString::fromStdString(layout_path));

        if(file.remove())
        {
            PopulateLayoutDropdown();
            QMessageBox::information(this, "Profile Deleted",
                                    QString("Profile '%1' deleted successfully").arg(profile_name));
        }
        else
        {
            QMessageBox::warning(this, "Delete Failed", "Failed to delete profile file");
        }
    }
}

void OpenRGB3DSpatialTab::on_layout_profile_changed(int)
{
    SaveCurrentLayoutName();
}

void OpenRGB3DSpatialTab::on_create_custom_controller_clicked()
{
    CustomControllerDialog dialog(resource_manager, this);

    if(dialog.exec() == QDialog::Accepted)
    {
        std::unique_ptr<VirtualController3D> virtual_ctrl = std::make_unique<VirtualController3D>(
            dialog.GetControllerName().toStdString(),
            dialog.GetGridWidth(),
            dialog.GetGridHeight(),
            dialog.GetGridDepth(),
            dialog.GetLEDMappings(),
            dialog.GetSpacingX(),
            dialog.GetSpacingY(),
            dialog.GetSpacingZ()
        );

        available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName()));

        virtual_controllers.push_back(std::move(virtual_ctrl));

        SaveCustomControllers();

        QMessageBox::information(this, "Custom Controller Created",
                                QString("Custom controller '%1' created successfully!\n\nYou can now add it to the 3D view.")
                                .arg(QString::fromStdString(virtual_ctrl->GetName())));
    }
}

void OpenRGB3DSpatialTab::on_export_custom_controller_clicked()
{
    if(virtual_controllers.empty())
    {
        QMessageBox::warning(this, "No Custom Controllers", "No custom controllers available to export");
        return;
    }

    int list_row = custom_controllers_list->currentRow();
    if(list_row < 0)
    {
        QMessageBox::warning(this, "No Selection", "Please select a custom controller from the list to export");
        return;
    }

    if(list_row >= (int)virtual_controllers.size())
    {
        QMessageBox::warning(this, "Invalid Selection", "Selected custom controller does not exist");
        return;
    }

    VirtualController3D* ctrl = virtual_controllers[list_row].get();

    QString filename = QFileDialog::getSaveFileName(this, "Export Custom Controller",
                                                    QString::fromStdString(ctrl->GetName()) + ".3dctrl",
                                                    "3D Controller Files (*.3dctrl)");
    if(filename.isEmpty())
    {
        return;
    }

    nlohmann::json export_data = ctrl->ToJson();

    std::ofstream file(filename.toStdString());
    if(file.is_open())
    {
        file << export_data.dump(4);
        file.close();
        QMessageBox::information(this, "Export Successful",
                                QString("Custom controller '%1' exported successfully to:\n%2")
                                .arg(QString::fromStdString(ctrl->GetName())).arg(filename));
    }
    else
    {
        QMessageBox::critical(this, "Export Failed",
                            QString("Failed to export custom controller to:\n%1").arg(filename));
    }
}

void OpenRGB3DSpatialTab::on_import_custom_controller_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this, "Import Custom Controller",
                                                    "",
                                                    "3D Controller Files (*.3dctrl);;All Files (*)");
    if(filename.isEmpty())
    {
        return;
    }

    std::ifstream file(filename.toStdString());
    if(!file.is_open())
    {
        QMessageBox::critical(this, "Import Failed",
                            QString("Failed to open file:\n%1").arg(filename));
        return;
    }

    try
    {
        nlohmann::json import_data;
        file >> import_data;
        file.close();

        std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
        std::unique_ptr<VirtualController3D> virtual_ctrl = VirtualController3D::FromJson(import_data, controllers);

        if(virtual_ctrl)
        {
            std::string ctrl_name = virtual_ctrl->GetName();

            for(unsigned int i = 0; i < virtual_controllers.size(); i++)
            {
                if(virtual_controllers[i]->GetName() == ctrl_name)
                {
                    QMessageBox::StandardButton reply = QMessageBox::question(this, "Duplicate Name",
                        QString("A custom controller named '%1' already exists.\n\nDo you want to replace it?")
                        .arg(QString::fromStdString(ctrl_name)),
                        QMessageBox::Yes | QMessageBox::No);

                    if(reply == QMessageBox::No)
                    {
                        return;  // unique_ptr automatically cleans up
                    }
                    else
                    {
                        for(unsigned int j = 0; j < virtual_controllers.size(); j++)
                        {
                            if(virtual_controllers[j]->GetName() == ctrl_name)
                            {
                                virtual_controllers.erase(virtual_controllers.begin() + j);
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            virtual_controllers.push_back(std::move(virtual_ctrl));
            SaveCustomControllers();
            UpdateAvailableControllersList();


            QMessageBox::information(this, "Import Successful",
                                    QString("Custom controller '%1' imported successfully!\n\n"
                                           "Grid: %2x%3x%4\n"
                                           "LEDs: %5\n\n"
                                           "You can now add it to the 3D view.")
                                    .arg(QString::fromStdString(virtual_ctrl->GetName()))
                                    .arg(virtual_ctrl->GetWidth())
                                    .arg(virtual_ctrl->GetHeight())
                                    .arg(virtual_ctrl->GetDepth())
                                    .arg(virtual_ctrl->GetMappings().size()));
        }
        else
        {
            QMessageBox::warning(this, "Import Warning",
                                "Failed to import custom controller.\n\n"
                                "The required physical controllers may not be connected.");
        }
    }
    catch(const std::exception& e)
    {
        QMessageBox::critical(this, "Import Failed",
                            QString("Failed to parse custom controller file:\n\n%1").arg(e.what()));
    }
}

void OpenRGB3DSpatialTab::on_edit_custom_controller_clicked()
{
    int list_row = custom_controllers_list->currentRow();
    if(list_row < 0)
    {
        QMessageBox::warning(this, "No Selection", "Please select a custom controller from the list to edit");
        return;
    }

    if(list_row >= (int)virtual_controllers.size())
    {
        QMessageBox::warning(this, "Invalid Selection", "Selected custom controller does not exist");
        return;
    }

    VirtualController3D* virtual_ctrl = virtual_controllers[list_row].get();

    CustomControllerDialog dialog(resource_manager, this);
    dialog.setWindowTitle("Edit Custom 3D Controller");
    dialog.LoadExistingController(virtual_ctrl->GetName(),
                                  virtual_ctrl->GetWidth(),
                                  virtual_ctrl->GetHeight(),
                                  virtual_ctrl->GetDepth(),
                                  virtual_ctrl->GetMappings());

    if(dialog.exec() == QDialog::Accepted)
    {
        std::string old_name = virtual_ctrl->GetName();
        std::string new_name = dialog.GetControllerName().toStdString();

        if(old_name != new_name)
        {
            std::string config_dir = resource_manager->GetConfigurationDirectory().string();
            std::string custom_dir = config_dir + "/plugins/settings/OpenRGB3DSpatialPlugin/custom_controllers";

            std::string safe_old_name = old_name;
            for(unsigned int i = 0; i < safe_old_name.length(); i++)
            {
                if(safe_old_name[i] == '/' || safe_old_name[i] == '\\' || safe_old_name[i] == ':' || safe_old_name[i] == '*' || safe_old_name[i] == '?' || safe_old_name[i] == '"' || safe_old_name[i] == '<' || safe_old_name[i] == '>' || safe_old_name[i] == '|')
                {
                    safe_old_name[i] = '_';
                }
            }

            std::string old_filepath = custom_dir + "/" + safe_old_name + ".json";
            if(filesystem::exists(old_filepath))
            {
                filesystem::remove(old_filepath);
            }
        }

        // Keep pointer to old instance so we can retarget any viewport transforms
        VirtualController3D* old_ptr = virtual_controllers[list_row].get();

        virtual_controllers[list_row] = std::make_unique<VirtualController3D>(
            new_name,
            dialog.GetGridWidth(),
            dialog.GetGridHeight(),
            dialog.GetGridDepth(),
            dialog.GetLEDMappings(),
            dialog.GetSpacingX(),
            dialog.GetSpacingY(),
            dialog.GetSpacingZ()
        );

        // Update any transforms in the viewport that referenced the old custom controller
        VirtualController3D* new_ptr = virtual_controllers[list_row].get();
        for(size_t i = 0; i < controller_transforms.size(); i++)
        {
            ControllerTransform* t = controller_transforms[i].get();
            if(t && t->virtual_controller == old_ptr)
            {
                t->virtual_controller = new_ptr;
                // Regenerate LED positions from the updated mapping and spacing
                t->led_positions = new_ptr->GenerateLEDPositions(grid_scale_mm);
                t->world_positions_dirty = true;

                // Update controller list item text to reflect the new name
                if(i < (size_t)controller_list->count())
                {
                    controller_list->item((int)i)->setText(QString("[Custom] ") + QString::fromStdString(new_ptr->GetName()));
                }
            }
        }

        SaveCustomControllers();
        UpdateAvailableControllersList();

        // Refresh viewport so changes take effect immediately
        viewport->SetControllerTransforms(&controller_transforms);
        viewport->update();

        QMessageBox::information(this, "Custom Controller Updated",
                                QString("Custom controller '%1' updated successfully!")
                                .arg(QString::fromStdString(virtual_controllers[list_row]->GetName())));
    }
}

void OpenRGB3DSpatialTab::SaveLayout(const std::string& filename)
{

    nlohmann::json layout_json;

    /*---------------------------------------------------------*\
    | Header Information                                       |
    \*---------------------------------------------------------*/
    layout_json["format"] = "OpenRGB3DSpatialLayout";
    layout_json["version"] = 6;

    /*---------------------------------------------------------*\
    | Grid Settings                                            |
    \*---------------------------------------------------------*/
    layout_json["grid"]["dimensions"]["x"] = custom_grid_x;
    layout_json["grid"]["dimensions"]["y"] = custom_grid_y;
    layout_json["grid"]["dimensions"]["z"] = custom_grid_z;
    layout_json["grid"]["snap_enabled"] = (viewport && viewport->IsGridSnapEnabled());
    layout_json["grid"]["scale_mm"] = grid_scale_mm;

    /*---------------------------------------------------------*\
    | Room Dimensions (Manual room size settings)             |
    \*---------------------------------------------------------*/
    layout_json["room"]["use_manual_size"] = use_manual_room_size;
    layout_json["room"]["width"] = manual_room_width;
    layout_json["room"]["depth"] = manual_room_depth;
    layout_json["room"]["height"] = manual_room_height;

    /*---------------------------------------------------------*\
    | User Position                                            |
    \*---------------------------------------------------------*/
    layout_json["user_position"]["x"] = user_position.x;
    layout_json["user_position"]["y"] = user_position.y;
    layout_json["user_position"]["z"] = user_position.z;
    layout_json["user_position"]["visible"] = user_position.visible;

    /*---------------------------------------------------------*\
    | Camera                                                   |
    \*---------------------------------------------------------*/
    if(viewport)
    {
        float dist, yaw, pitch, tx, ty, tz;
        viewport->GetCamera(dist, yaw, pitch, tx, ty, tz);
        layout_json["camera"]["distance"] = dist;
        layout_json["camera"]["yaw"] = yaw;
        layout_json["camera"]["pitch"] = pitch;
        layout_json["camera"]["target"]["x"] = tx;
        layout_json["camera"]["target"]["y"] = ty;
        layout_json["camera"]["target"]["z"] = tz;
    }

    /*---------------------------------------------------------*\
    | Controllers                                              |
    \*---------------------------------------------------------*/
    layout_json["controllers"] = nlohmann::json::array();

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        nlohmann::json controller_json;

        if(ct->controller == nullptr)
        {
            QListWidgetItem* item = controller_list->item(i);
            QString display_name = item ? item->text() : "Unknown Custom Controller";

            controller_json["name"] = display_name.toStdString();
            controller_json["type"] = "virtual";
            controller_json["location"] = "VIRTUAL_CONTROLLER";
        }
        else
        {
            controller_json["name"] = ct->controller->name;
            controller_json["type"] = "physical";
            controller_json["location"] = ct->controller->location;
        }

        /*---------------------------------------------------------*\
        | LED Mappings                                             |
        \*---------------------------------------------------------*/
        controller_json["led_mappings"] = nlohmann::json::array();
        for(unsigned int j = 0; j < ct->led_positions.size(); j++)
        {
            nlohmann::json led_mapping;
            led_mapping["zone_index"] = ct->led_positions[j].zone_idx;
            led_mapping["led_index"] = ct->led_positions[j].led_idx;
            controller_json["led_mappings"].push_back(led_mapping);
        }

        /*---------------------------------------------------------*\
        | Transform                                                |
        \*---------------------------------------------------------*/
        controller_json["transform"]["position"]["x"] = ct->transform.position.x;
        controller_json["transform"]["position"]["y"] = ct->transform.position.y;
        controller_json["transform"]["position"]["z"] = ct->transform.position.z;

        controller_json["transform"]["rotation"]["x"] = ct->transform.rotation.x;
        controller_json["transform"]["rotation"]["y"] = ct->transform.rotation.y;
        controller_json["transform"]["rotation"]["z"] = ct->transform.rotation.z;

        controller_json["transform"]["scale"]["x"] = ct->transform.scale.x;
        controller_json["transform"]["scale"]["y"] = ct->transform.scale.y;
        controller_json["transform"]["scale"]["z"] = ct->transform.scale.z;

        /*---------------------------------------------------------*\
        | LED Spacing                                              |
        \*---------------------------------------------------------*/
        controller_json["led_spacing_mm"]["x"] = ct->led_spacing_mm_x;
        controller_json["led_spacing_mm"]["y"] = ct->led_spacing_mm_y;
        controller_json["led_spacing_mm"]["z"] = ct->led_spacing_mm_z;

        /*---------------------------------------------------------*\
        | Granularity (-1=virtual, 0=device, 1=zone, 2=LED)       |
        \*---------------------------------------------------------*/
        controller_json["granularity"] = ct->granularity;
        controller_json["item_idx"] = ct->item_idx;

        controller_json["display_color"] = ct->display_color;

        layout_json["controllers"].push_back(controller_json);
    }

    /*---------------------------------------------------------*\
    | Reference Points                                         |
    \*---------------------------------------------------------*/
    layout_json["reference_points"] = nlohmann::json::array();
    for(size_t i = 0; i < reference_points.size(); i++)
    {
        if(!reference_points[i]) continue; // Skip null pointers

        layout_json["reference_points"].push_back(reference_points[i]->ToJson());
    }

    /*---------------------------------------------------------*\
    | Display Planes                                           |
    \*---------------------------------------------------------*/
    layout_json["display_planes"] = nlohmann::json::array();
    for(size_t i = 0; i < display_planes.size(); i++)
    {
        if(!display_planes[i]) continue;
        layout_json["display_planes"].push_back(display_planes[i]->ToJson());
    }

    /*---------------------------------------------------------*\
    | Zones                                                    |
    \*---------------------------------------------------------*/
    if(zone_manager)
    {
        layout_json["zones"] = zone_manager->ToJSON();
    }

    /*---------------------------------------------------------*\
    | Write JSON to file                                       |
    \*---------------------------------------------------------*/
    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QString error_msg = QString("Failed to save layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename))
            .arg(file.errorString());
        QMessageBox::critical(this, "Save Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open file for writing: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    QTextStream out(&file);
    out << QString::fromStdString(layout_json.dump(4));
    file.close();

    if(file.error() != QFile::NoError)
    {
        QString error_msg = QString("Failed to write layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename))
            .arg(file.errorString());
        QMessageBox::critical(this, "Write Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write file: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    
}

void OpenRGB3DSpatialTab::LoadLayoutFromJSON(const nlohmann::json& layout_json)
{

    /*---------------------------------------------------------*\
    | Load Grid Settings                                       |
    \*---------------------------------------------------------*/
    if(layout_json.contains("grid"))
    {
        custom_grid_x = layout_json["grid"]["dimensions"]["x"].get<int>();
        custom_grid_y = layout_json["grid"]["dimensions"]["y"].get<int>();
        custom_grid_z = layout_json["grid"]["dimensions"]["z"].get<int>();

        if(grid_x_spin)
        {
            grid_x_spin->blockSignals(true);
            grid_x_spin->setValue(custom_grid_x);
            grid_x_spin->blockSignals(false);
        }
        if(grid_y_spin)
        {
            grid_y_spin->blockSignals(true);
            grid_y_spin->setValue(custom_grid_y);
            grid_y_spin->blockSignals(false);
        }
        if(grid_z_spin)
        {
            grid_z_spin->blockSignals(true);
            grid_z_spin->setValue(custom_grid_z);
            grid_z_spin->blockSignals(false);
        }

        if(viewport)
        {
            viewport->SetGridDimensions(custom_grid_x, custom_grid_y, custom_grid_z);
        }

        bool grid_snap_enabled = layout_json["grid"]["snap_enabled"].get<bool>();
        if(grid_snap_checkbox) grid_snap_checkbox->setChecked(grid_snap_enabled);
        if(viewport) viewport->SetGridSnapEnabled(grid_snap_enabled);

        // Load grid scale if available (default to 10mm for older layouts)
        if(layout_json["grid"].contains("scale_mm"))
        {
            grid_scale_mm = layout_json["grid"]["scale_mm"].get<float>();
            if(grid_scale_spin)
            {
                grid_scale_spin->blockSignals(true);
                grid_scale_spin->setValue(grid_scale_mm);
                grid_scale_spin->blockSignals(false);
            }
        }
    }

    /*---------------------------------------------------------*\
    | Load Room Dimensions                                     |
    \*---------------------------------------------------------*/
    if(layout_json.contains("room"))
    {
        if(layout_json["room"].contains("use_manual_size"))
        {
            use_manual_room_size = layout_json["room"]["use_manual_size"].get<bool>();
            if(use_manual_room_size_checkbox)
            {
                use_manual_room_size_checkbox->blockSignals(true);
                use_manual_room_size_checkbox->setChecked(use_manual_room_size);
                use_manual_room_size_checkbox->blockSignals(false);
            }
        }

        if(layout_json["room"].contains("width"))
        {
            manual_room_width = layout_json["room"]["width"].get<float>();
            if(room_width_spin)
            {
                room_width_spin->blockSignals(true);
                room_width_spin->setValue(manual_room_width);
                room_width_spin->setEnabled(use_manual_room_size);
                room_width_spin->blockSignals(false);
            }
        }

        if(layout_json["room"].contains("depth"))
        {
            manual_room_depth = layout_json["room"]["depth"].get<float>();
            if(room_depth_spin)
            {
                room_depth_spin->blockSignals(true);
                room_depth_spin->setValue(manual_room_depth);
                room_depth_spin->setEnabled(use_manual_room_size);
                room_depth_spin->blockSignals(false);
            }
        }

        if(layout_json["room"].contains("height"))
        {
            manual_room_height = layout_json["room"]["height"].get<float>();
            if(room_height_spin)
            {
                room_height_spin->blockSignals(true);
                room_height_spin->setValue(manual_room_height);
                room_height_spin->setEnabled(use_manual_room_size);
                room_height_spin->blockSignals(false);
            }
        }

        // Update viewport with loaded manual room dimensions
        if(viewport)
        {
            viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
        }
        emit GridLayoutChanged();
    }

    /*---------------------------------------------------------*\
    | Load User Position                                       |
    \*---------------------------------------------------------*/
    if(layout_json.contains("user_position"))
    {
        user_position.x = layout_json["user_position"]["x"].get<float>();
        user_position.y = layout_json["user_position"]["y"].get<float>();
        user_position.z = layout_json["user_position"]["z"].get<float>();
        user_position.visible = layout_json["user_position"]["visible"].get<bool>();

        // User position UI controls have been removed - values stored for legacy compatibility
        if(viewport) viewport->SetUserPosition(user_position);
    }

    /*---------------------------------------------------------*\
    | Load Camera                                              |
    \*---------------------------------------------------------*/
    if(layout_json.contains("camera") && viewport)
    {
        const nlohmann::json& cam = layout_json["camera"];
        float dist = cam.contains("distance") ? cam["distance"].get<float>() : 20.0f;
        float yaw  = cam.contains("yaw") ? cam["yaw"].get<float>() : 45.0f;
        float pitch= cam.contains("pitch") ? cam["pitch"].get<float>() : 30.0f;
        float tx = 0.0f, ty = 0.0f, tz = 0.0f;
        if(cam.contains("target"))
        {
            const nlohmann::json& tgt = cam["target"];
            if(tgt.contains("x")) tx = tgt["x"].get<float>();
            if(tgt.contains("y")) ty = tgt["y"].get<float>();
            if(tgt.contains("z")) tz = tgt["z"].get<float>();
        }
        viewport->SetCamera(dist, yaw, pitch, tx, ty, tz);
    }

    /*---------------------------------------------------------*\
    | Clear existing controllers                               |
    \*---------------------------------------------------------*/
    on_clear_all_clicked();

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();

    /*---------------------------------------------------------*\
    | Load Controllers                                         |
    \*---------------------------------------------------------*/
    if(layout_json.contains("controllers"))
    {
        const nlohmann::json& controllers_array = layout_json["controllers"];
        for(size_t i = 0; i < controllers_array.size(); i++)
        {
            const nlohmann::json& controller_json = controllers_array[i];
            std::string ctrl_name = controller_json["name"].get<std::string>();
            std::string ctrl_location = controller_json["location"].get<std::string>();
            std::string ctrl_type = controller_json["type"].get<std::string>();

            RGBController* controller = nullptr;
            bool is_virtual = (ctrl_type == "virtual");

            if(!is_virtual)
            {
                for(unsigned int j = 0; j < controllers.size(); j++)
                {
                    if(controllers[j]->name == ctrl_name && controllers[j]->location == ctrl_location)
                    {
                        controller = controllers[j];
                        break;
                    }
                }

                if(!controller)
                {
                    continue;
                }
            }

            std::unique_ptr<ControllerTransform> ctrl_transform = std::make_unique<ControllerTransform>();
            ctrl_transform->controller = controller;
            ctrl_transform->virtual_controller = nullptr;

            // Load LED spacing first (needed for position generation)
            if(controller_json.contains("led_spacing_mm"))
            {
                ctrl_transform->led_spacing_mm_x = controller_json["led_spacing_mm"]["x"].get<float>();
                ctrl_transform->led_spacing_mm_y = controller_json["led_spacing_mm"]["y"].get<float>();
                ctrl_transform->led_spacing_mm_z = controller_json["led_spacing_mm"]["z"].get<float>();
            }
            else
            {
                ctrl_transform->led_spacing_mm_x = 10.0f;
                ctrl_transform->led_spacing_mm_y = 0.0f;
                ctrl_transform->led_spacing_mm_z = 0.0f;
            }

            // Load granularity
            if(controller_json.contains("granularity"))
            {
                ctrl_transform->granularity = controller_json["granularity"].get<int>();
                ctrl_transform->item_idx = controller_json["item_idx"].get<int>();
            }
            else
            {
                // Default for older files: -1 for virtual, 0 for physical
                ctrl_transform->granularity = is_virtual ? -1 : 0;
                ctrl_transform->item_idx = 0;
            }

            if(is_virtual)
            {
                QString virtual_name = QString::fromStdString(ctrl_name);
                if(virtual_name.startsWith("[Custom] "))
                {
                    virtual_name = virtual_name.mid(9);
                }

                VirtualController3D* virtual_ctrl = nullptr;
                for(unsigned int i = 0; i < virtual_controllers.size(); i++)
                {
                    if(QString::fromStdString(virtual_controllers[i]->GetName()) == virtual_name)
                    {
                        virtual_ctrl = virtual_controllers[i].get();
                        break;
                    }
                }

                if(virtual_ctrl)
                {
                    ctrl_transform->controller = nullptr;
                    ctrl_transform->virtual_controller = virtual_ctrl;
                    ctrl_transform->led_positions = virtual_ctrl->GenerateLEDPositions(grid_scale_mm);
                }
                else
                {
                    continue;  // ctrl_transform auto-deleted
                }
            }
            else
            {
                // Load LED mappings for physical controllers
                const nlohmann::json& led_mappings_array = controller_json["led_mappings"];
                for(size_t j = 0; j < led_mappings_array.size(); j++)
                {
                    const nlohmann::json& led_mapping = led_mappings_array[j];
                    unsigned int zone_idx = led_mapping["zone_index"].get<unsigned int>();
                    unsigned int led_idx = led_mapping["led_index"].get<unsigned int>();

                    std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
                        controller, custom_grid_x, custom_grid_y, custom_grid_z,
                        ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
                        grid_scale_mm);

                    for(unsigned int k = 0; k < all_positions.size(); k++)
                    {
                        if(all_positions[k].zone_idx == zone_idx && all_positions[k].led_idx == led_idx)
                        {
                            ctrl_transform->led_positions.push_back(all_positions[k]);
                            break;
                        }
                    }
                }

                // Validate/infer granularity from loaded LED positions (FAILSAFE)
                // This corrects any corrupted or mismatched granularity data
                if(ctrl_transform->led_positions.size() > 0)
                {
                    int original_granularity = ctrl_transform->granularity;
                    int original_item_idx = ctrl_transform->item_idx;

                    // Count total LEDs in controller
                    std::vector<LEDPosition3D> all_leds = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
                        controller, custom_grid_x, custom_grid_y, custom_grid_z,
                        ctrl_transform->led_spacing_mm_x, ctrl_transform->led_spacing_mm_y, ctrl_transform->led_spacing_mm_z,
                        grid_scale_mm);

                    if(ctrl_transform->led_positions.size() == all_leds.size())
                    {
                        // All LEDs loaded - this is whole device
                        if(ctrl_transform->granularity != 0)
                        {
                            ctrl_transform->granularity = 0;
                            ctrl_transform->item_idx = 0;
                        }
                    }
                    else if(ctrl_transform->led_positions.size() == 1)
                    {
                        // Single LED - granularity should be 2
                        if(ctrl_transform->granularity != 2)
                        {
                            ctrl_transform->granularity = 2;
                            // Calculate global LED index from zone/led indices
                            unsigned int zone_idx = ctrl_transform->led_positions[0].zone_idx;
                            unsigned int led_idx = ctrl_transform->led_positions[0].led_idx;
                            if(zone_idx < controller->zones.size())
                            {
                                ctrl_transform->item_idx = controller->zones[zone_idx].start_idx + led_idx;
                            }
                        }
                    }
                    else
                    {
                        // Multiple LEDs but not all - check if they're all from same zone
                        unsigned int first_zone = ctrl_transform->led_positions[0].zone_idx;
                        bool same_zone = true;
                        for(unsigned int i = 1; i < ctrl_transform->led_positions.size(); i++)
                        {
                            if(ctrl_transform->led_positions[i].zone_idx != first_zone)
                            {
                                same_zone = false;
                                break;
                            }
                        }

                        if(same_zone)
                        {
                            // All from same zone
                            if(ctrl_transform->granularity != 1)
                            {
                                ctrl_transform->granularity = 1;
                                ctrl_transform->item_idx = first_zone;
                            }
                        }
                        else
                        {
                            // LEDs from multiple zones - this is corrupted data!
                            // Best we can do is treat as whole device and regenerate
                            LOG_WARNING("[OpenRGB3DSpatialPlugin] CORRUPTED DATA for '%s': has %d LEDs from multiple zones with granularity=%d. Treating as Whole Device and will regenerate on next change.",
                                        controller->name.c_str(),
                                        (int)ctrl_transform->led_positions.size(),
                                        original_granularity);
                            ctrl_transform->granularity = 0;
                            ctrl_transform->item_idx = 0;
                            // Keep the loaded LED positions for now, but they'll be regenerated on next change
                        }
                    }

                    // Validation success - no logging needed
                    if(ctrl_transform->granularity == original_granularity && ctrl_transform->item_idx == original_item_idx)
                    {
                    }
                }
            }

            // Load transform
            ctrl_transform->transform.position.x = controller_json["transform"]["position"]["x"].get<float>();
            ctrl_transform->transform.position.y = controller_json["transform"]["position"]["y"].get<float>();
            ctrl_transform->transform.position.z = controller_json["transform"]["position"]["z"].get<float>();

            ctrl_transform->transform.rotation.x = controller_json["transform"]["rotation"]["x"].get<float>();
            ctrl_transform->transform.rotation.y = controller_json["transform"]["rotation"]["y"].get<float>();
            ctrl_transform->transform.rotation.z = controller_json["transform"]["rotation"]["z"].get<float>();

            ctrl_transform->transform.scale.x = controller_json["transform"]["scale"]["x"].get<float>();
            ctrl_transform->transform.scale.y = controller_json["transform"]["scale"]["y"].get<float>();
            ctrl_transform->transform.scale.z = controller_json["transform"]["scale"]["z"].get<float>();

            ctrl_transform->display_color = controller_json["display_color"].get<unsigned int>();

            // Save values before moving ctrl_transform
            unsigned int display_color = ctrl_transform->display_color;
            int granularity = ctrl_transform->granularity;
            int item_idx = ctrl_transform->item_idx;
            size_t led_positions_size = ctrl_transform->led_positions.size();
            unsigned int first_zone_idx = (led_positions_size > 0) ? ctrl_transform->led_positions[0].zone_idx : 0;
            unsigned int first_led_idx = (led_positions_size > 0) ? ctrl_transform->led_positions[0].led_idx : 0;

            // Pre-compute world positions
            ctrl_transform->world_positions_dirty = true;
            ControllerLayout3D::UpdateWorldPositions(ctrl_transform.get());

            controller_transforms.push_back(std::move(ctrl_transform));

            QColor color;
            color.setRgb(display_color & 0xFF,
                         (display_color >> 8) & 0xFF,
                         (display_color >> 16) & 0xFF);

            QString name;
            if(is_virtual)
            {
                name = QString::fromStdString(ctrl_name);
            }
            else
            {
                // Use granularity info to create proper name with prefix
                if(granularity == 0)
                {
                    name = QString("[Device] ") + QString::fromStdString(controller->name);
                }
                else if(granularity == 1)
                {
                    name = QString("[Zone] ") + QString::fromStdString(controller->name);
                    if(item_idx >= 0 && item_idx < (int)controller->zones.size())
                    {
                        name += " - " + QString::fromStdString(controller->zones[item_idx].name);
                    }
                }
                else if(granularity == 2)
                {
                    name = QString("[LED] ") + QString::fromStdString(controller->name);
                    if(item_idx >= 0 && item_idx < (int)controller->leds.size())
                    {
                        name += " - " + QString::fromStdString(controller->leds[item_idx].name);
                    }
                }
                else
                {
                    // Fallback for old files without granularity
                    name = QString::fromStdString(controller->name);
                    if(led_positions_size < controller->leds.size())
                    {
                        if(led_positions_size == 1)
                        {
                            unsigned int led_global_idx = controller->zones[first_zone_idx].start_idx + first_led_idx;
                            name = QString("[LED] ") + name + " - " + QString::fromStdString(controller->leds[led_global_idx].name);
                        }
                        else
                        {
                            name = QString("[Zone] ") + name + " - " + QString::fromStdString(controller->zones[first_zone_idx].name);
                        }
                    }
                    else
                    {
                        name = QString("[Device] ") + name;
                    }
                }
            }

            QListWidgetItem* item = new QListWidgetItem(name);
            controller_list->addItem(item);
        }
    }

    /*---------------------------------------------------------*\
    | Load Reference Points                                    |
    \*---------------------------------------------------------*/
    // Clear existing reference points
    reference_points.clear();

    if(layout_json.contains("reference_points"))
    {
        const nlohmann::json& ref_points_array = layout_json["reference_points"];
        for(size_t i = 0; i < ref_points_array.size(); i++)
        {
            std::unique_ptr<VirtualReferencePoint3D> ref_point = VirtualReferencePoint3D::FromJson(ref_points_array[i]);
            if(ref_point)
            {
                reference_points.push_back(std::move(ref_point));
            }
        }
    }

    UpdateReferencePointsList();

    /*---------------------------------------------------------*\
    | Load Display Planes                                      |
    \*---------------------------------------------------------*/
    display_planes.clear();
    current_display_plane_index = -1;
    if(layout_json.contains("display_planes"))
    {
        const nlohmann::json& planes_array = layout_json["display_planes"];
        for(size_t i = 0; i < planes_array.size(); i++)
        {
            std::unique_ptr<DisplayPlane3D> plane = DisplayPlane3D::FromJson(planes_array[i]);
            if(plane)
            {
                display_planes.push_back(std::move(plane));
            }
        }
    }
    UpdateDisplayPlanesList();
    RefreshDisplayPlaneDetails();

    // Sync display planes to global manager
    std::vector<DisplayPlane3D*> plane_ptrs;
    for(auto& plane : display_planes)
    {
        if(plane)
        {
            plane_ptrs.push_back(plane.get());
        }
    }
    DisplayPlaneManager::instance()->SetDisplayPlanes(plane_ptrs);

    emit GridLayoutChanged();

    /*---------------------------------------------------------*\
    | Load Zones                                               |
    \*---------------------------------------------------------*/
    if(zone_manager)
    {
        if(layout_json.contains("zones"))
        {
            try
            {
                zone_manager->FromJSON(layout_json["zones"]);
                UpdateZonesList();
                
            }
            catch(const std::exception& e)
            {
                LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to load zones from layout: %s", e.what());
                zone_manager->ClearAllZones();
                UpdateZonesList();
            }
        }
        else
        {
            // Old layout file without zones - just initialize empty
            
            zone_manager->ClearAllZones();
            UpdateZonesList();
        }
    }

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->SetReferencePoints(&reference_points);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::LoadLayout(const std::string& filename)
{

    QFile file(QString::fromStdString(filename));

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString error_msg = QString("Failed to open layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename))
            .arg(file.errorString());
        QMessageBox::critical(this, "Load Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open file for reading: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    if(file.error() != QFile::NoError)
    {
        QString error_msg = QString("Failed to read layout file:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename))
            .arg(file.errorString());
        QMessageBox::critical(this, "Read Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to read file: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    try
    {
        nlohmann::json layout_json = nlohmann::json::parse(content.toStdString());
        LoadLayoutFromJSON(layout_json);
        
        return;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to parse JSON: %s", e.what());
        QMessageBox::critical(this, "Invalid Layout File",
                            QString("Failed to parse layout file:\n%1\n\nThe file may be corrupted or in an invalid format.\n\nError: %2")
                            .arg(QString::fromStdString(filename))
                            .arg(e.what()));
        return;
    }
}

std::string OpenRGB3DSpatialTab::GetLayoutPath(const std::string& layout_name)
{
    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path plugins_dir = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin" / "layouts";

    QDir dir;
    dir.mkpath(QString::fromStdString(plugins_dir.string()));

    std::string filename = layout_name + ".json";
    filesystem::path layout_file = plugins_dir / filename;

    return layout_file.string();
}

void OpenRGB3DSpatialTab::PopulateLayoutDropdown()
{
    QString current_text = layout_profiles_combo->currentText();

    layout_profiles_combo->blockSignals(true);
    layout_profiles_combo->clear();

    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path layouts_dir = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin" / "layouts";


    QDir dir(QString::fromStdString(layouts_dir.string()));
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);


    for(int i = 0; i < files.size(); i++)
    {
        QString base_name = files[i].baseName();
        layout_profiles_combo->addItem(base_name);
    }

    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    QString saved_profile = "";
    if(settings.contains("SelectedProfile"))
    {
        saved_profile = QString::fromStdString(settings["SelectedProfile"].get<std::string>());
    }
    else
    {
    }

    if(!saved_profile.isEmpty())
    {
        int index = layout_profiles_combo->findText(saved_profile);
        if(index >= 0)
        {
            layout_profiles_combo->setCurrentIndex(index);
        }
        else
        {
        }
    }
    else if(!current_text.isEmpty())
    {
        int index = layout_profiles_combo->findText(current_text);
        if(index >= 0)
        {
            layout_profiles_combo->setCurrentIndex(index);
        }
    }

    layout_profiles_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::SaveCurrentLayoutName()
{
    if(!layout_profiles_combo || !auto_load_checkbox)
    {
        return;
    }

    std::string profile_name = layout_profiles_combo->currentText().toStdString();
    bool auto_load_enabled = auto_load_checkbox->isChecked();

    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    settings["SelectedProfile"] = profile_name;
    settings["AutoLoadEnabled"] = auto_load_enabled;
    resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", settings);
    resource_manager->GetSettingsManager()->SaveSettings();
}

void OpenRGB3DSpatialTab::TryAutoLoadLayout()
{
    if(!first_load)
    {
        return;
    }

    first_load = false;

    if(!auto_load_checkbox || !layout_profiles_combo)
    {
        return;
    }

    /*---------------------------------------------------------*\
    | Load saved settings                                      |
    \*---------------------------------------------------------*/
    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");

    bool auto_load_enabled = false;
    std::string saved_profile;

    if(settings.contains("AutoLoadEnabled"))
    {
        auto_load_enabled = settings["AutoLoadEnabled"].get<bool>();
    }

    if(settings.contains("SelectedProfile"))
    {
        saved_profile = settings["SelectedProfile"].get<std::string>();
    }


    /*---------------------------------------------------------*\
    | Restore checkbox state                                   |
    \*---------------------------------------------------------*/
    auto_load_checkbox->blockSignals(true);
    auto_load_checkbox->setChecked(auto_load_enabled);
    auto_load_checkbox->blockSignals(false);

    /*---------------------------------------------------------*\
    | Restore profile selection                                |
    \*---------------------------------------------------------*/
    if(!saved_profile.empty())
    {
        int index = layout_profiles_combo->findText(QString::fromStdString(saved_profile));
        if(index >= 0)
        {
            layout_profiles_combo->blockSignals(true);
            layout_profiles_combo->setCurrentIndex(index);
            layout_profiles_combo->blockSignals(false);
        }
    }

    /*---------------------------------------------------------*\
    | Auto-load if enabled                                     |
    \*---------------------------------------------------------*/
    if(auto_load_enabled && !saved_profile.empty())
    {
        std::string layout_path = GetLayoutPath(saved_profile);
        QFileInfo check_file(QString::fromStdString(layout_path));

        if(check_file.exists())
        {
            LoadLayout(layout_path);
        }
    }

    /*---------------------------------------------------------*\
    | Try to auto-load effect profile after layout loads      |
    \*---------------------------------------------------------*/
    TryAutoLoadEffectProfile();
}

void OpenRGB3DSpatialTab::SaveCustomControllers()
{
    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string custom_dir = config_dir + "/plugins/settings/OpenRGB3DSpatialPlugin/custom_controllers";

#ifdef _WIN32
    CreateDirectoryA(custom_dir.c_str(), NULL);
#else
    mkdir(custom_dir.c_str(), 0755);
#endif

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        std::string safe_name = virtual_controllers[i]->GetName();
        for(unsigned int j = 0; j < safe_name.length(); j++)
        {
            if(safe_name[j] == '/' || safe_name[j] == '\\' || safe_name[j] == ':' || safe_name[j] == '*' || safe_name[j] == '?' || safe_name[j] == '"' || safe_name[j] == '<' || safe_name[j] == '>' || safe_name[j] == '|')
            {
                safe_name[j] = '_';
            }
        }

        std::string filepath = custom_dir + "/" + safe_name + ".json";
        std::ofstream file(filepath);
        if(file.is_open())
        {
            nlohmann::json ctrl_json = virtual_controllers[i]->ToJson();
            file << ctrl_json.dump(4);
            file.close();

            if(file.fail())
            {
                LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write custom controller: %s", filepath.c_str());
                // Don't show error dialog here - too noisy during auto-save
            }
            else
            {
                
            }
        }
        else
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open custom controller file: %s", filepath.c_str());
            // Don't show error dialog here - too noisy during auto-save
        }
    }
}

void OpenRGB3DSpatialTab::LoadCustomControllers()
{
    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string custom_dir = config_dir + "/plugins/settings/OpenRGB3DSpatialPlugin/custom_controllers";

    filesystem::path dir_path(custom_dir);
    if(!filesystem::exists(dir_path))
    {
        return;
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    int loaded_count = 0;

    try
    {
        std::filesystem::directory_iterator dir_iter(dir_path);
        std::filesystem::directory_iterator end_iter;

        for(std::filesystem::directory_iterator entry = dir_iter; entry != end_iter; ++entry)
        {
            if(entry->path().extension() == ".json")
            {
                std::ifstream file(entry->path().string());
                if(file.is_open())
                {
                    try
                    {
                        nlohmann::json ctrl_json;
                        file >> ctrl_json;
                        file.close();

                        std::unique_ptr<VirtualController3D> virtual_ctrl = VirtualController3D::FromJson(ctrl_json, controllers);
                        if(virtual_ctrl)
                        {
                            std::string ctrl_name = virtual_ctrl->GetName();
                            available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(ctrl_name));
                            virtual_controllers.push_back(std::move(virtual_ctrl));
                            loaded_count++;
                            
                        }
                        else
                        {
                            LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to create custom controller from: %s",
                                      entry->path().filename().string().c_str());
                        }
                    }
                    catch(const std::exception& e)
                    {
                        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to load custom controller %s: %s",
                                entry->path().filename().string().c_str(), e.what());
                    }
                }
                else
                {
                    LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to open custom controller file: %s",
                              entry->path().string().c_str());
                }
            }
        }

    }
    catch(const std::exception&)
    {
    }
}

bool OpenRGB3DSpatialTab::IsItemInScene(RGBController* controller, int granularity, int item_idx)
{
    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i].get();
        if(ct->controller == nullptr) continue;
        if(ct->controller != controller) continue;

        // Use granularity field if available
        if(ct->granularity == granularity && ct->item_idx == item_idx)
        {
            return true;
        }

        // Fallback: check by LED positions (for older data or edge cases)
        if(granularity == 0)
        {
            // Check if this is whole device by comparing LED count
            if(ct->granularity == 0)
            {
                return true;
            }
            // Legacy check for controllers without granularity field
            if(ct->granularity < 0 || ct->granularity > 2)
            {
                std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayout(controller, custom_grid_x, custom_grid_y, custom_grid_z);
                if(ct->led_positions.size() == all_positions.size())
                {
                    return true;
                }
            }
        }
        else if(granularity == 1)
        {
            // Check if any LED from this zone is in the controller
            for(unsigned int j = 0; j < ct->led_positions.size(); j++)
            {
                if(ct->led_positions[j].zone_idx == (unsigned int)item_idx)
                {
                    return true;
                }
            }
        }
        else if(granularity == 2)
        {
            // Check if this specific LED is in the controller
            for(unsigned int j = 0; j < ct->led_positions.size(); j++)
            {
                unsigned int global_led_idx = controller->zones[ct->led_positions[j].zone_idx].start_idx + ct->led_positions[j].led_idx;
                if(global_led_idx == (unsigned int)item_idx)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

int OpenRGB3DSpatialTab::GetUnassignedZoneCount(RGBController* controller)
{
    int unassigned_count = 0;
    for(unsigned int i = 0; i < controller->zones.size(); i++)
    {
        if(!IsItemInScene(controller, 1, i))
        {
            unassigned_count++;
        }
    }
    return unassigned_count;
}

int OpenRGB3DSpatialTab::GetUnassignedLEDCount(RGBController* controller)
{
    int total_leds = (int)controller->leds.size();
    int assigned_leds = 0;

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        if(controller_transforms[i]->controller == controller)
        {
            assigned_leds += (int)controller_transforms[i]->led_positions.size();
        }
    }

    return total_leds - assigned_leds;
}

void OpenRGB3DSpatialTab::RegenerateLEDPositions(ControllerTransform* transform)
{
    if(!transform) return;

    if(transform->virtual_controller)
    {
        // Virtual controller
        transform->led_positions = transform->virtual_controller->GenerateLEDPositions(grid_scale_mm);
    }
    else if(transform->controller)
    {
        // Physical controller - regenerate with spacing and respect granularity
        std::vector<LEDPosition3D> all_positions = ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(
            transform->controller,
            custom_grid_x, custom_grid_y, custom_grid_z,
            transform->led_spacing_mm_x, transform->led_spacing_mm_y, transform->led_spacing_mm_z,
            grid_scale_mm);

        transform->led_positions.clear();

        if(transform->granularity == 0)
        {
            // Whole device - use all positions
            transform->led_positions = all_positions;
        }
        else if(transform->granularity == 1)
        {
            // Zone - filter to specific zone
            for(unsigned int i = 0; i < all_positions.size(); i++)
            {
                if(all_positions[i].zone_idx == (unsigned int)transform->item_idx)
                {
                    transform->led_positions.push_back(all_positions[i]);
                }
            }
        }
        else if(transform->granularity == 2)
        {
            // LED - filter to specific LED
            for(unsigned int i = 0; i < all_positions.size(); i++)
            {
                unsigned int global_led_idx = transform->controller->zones[all_positions[i].zone_idx].start_idx + all_positions[i].led_idx;
                if(global_led_idx == (unsigned int)transform->item_idx)
                {
                    transform->led_positions.push_back(all_positions[i]);
                    break;
                }
            }
        }
    }
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
    if (class_name == "ScreenMirror3D")
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
void OpenRGB3DSpatialTab::SetupAudioCustomEffectsUI(QVBoxLayout* parent_layout)
{
    if(audio_custom_group) return;
    audio_custom_group = new QGroupBox("Custom Audio Effects");
    QVBoxLayout* v = new QVBoxLayout(audio_custom_group);

    audio_custom_list = new QListWidget();
    audio_custom_list->setMinimumHeight(140);
    v->addWidget(audio_custom_list);

    QHBoxLayout* name_row = new QHBoxLayout();
    name_row->addWidget(new QLabel("Name:"));
    audio_custom_name_edit = new QLineEdit();
    name_row->addWidget(audio_custom_name_edit);
    v->addLayout(name_row);

    QHBoxLayout* btns = new QHBoxLayout();
    audio_custom_save_btn = new QPushButton("Save");
    audio_custom_load_btn = new QPushButton("Load");
    audio_custom_delete_btn = new QPushButton("Delete");
    audio_custom_add_to_stack_btn = new QPushButton("Add Selected to Stack");
    btns->addWidget(audio_custom_save_btn);
    btns->addWidget(audio_custom_load_btn);
    btns->addWidget(audio_custom_delete_btn);
    btns->addStretch();
    btns->addWidget(audio_custom_add_to_stack_btn);
    v->addLayout(btns);

    parent_layout->addWidget(audio_custom_group);

    connect(audio_custom_save_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_custom_save_clicked);
    connect(audio_custom_load_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_custom_load_clicked);
    connect(audio_custom_delete_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_custom_delete_clicked);
    connect(audio_custom_add_to_stack_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_custom_add_to_stack_clicked);

    UpdateAudioCustomEffectsList();
}

std::string OpenRGB3DSpatialTab::GetAudioCustomEffectsDir()
{
    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path dir = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin" / "AudioCustomEffects";
    filesystem::create_directories(dir);
    return dir.string();
}

std::string OpenRGB3DSpatialTab::GetAudioCustomEffectPath(const std::string& name)
{
    filesystem::path base_dir(GetAudioCustomEffectsDir());
    filesystem::path file_path = base_dir / (name + ".audiocust.json");
    return file_path.string();
}

void OpenRGB3DSpatialTab::UpdateAudioCustomEffectsList()
{
    if(!audio_custom_list) return;
    audio_custom_list->clear();
    std::string dir = GetAudioCustomEffectsDir();
    for(filesystem::directory_iterator entry(dir); entry != filesystem::directory_iterator(); ++entry)
    {
        if(entry->path().extension() == ".json")
        {
            std::string stem = entry->path().stem().string();
            if(stem.size() > 10 && stem.substr(stem.size()-10) == ".audiocust")
            {
                std::string name = stem.substr(0, stem.size()-10);
                audio_custom_list->addItem(QString::fromStdString(name));
            }
        }
    }
}

void OpenRGB3DSpatialTab::on_audio_custom_save_clicked()
{
    if(!audio_effect_combo) return;
    QString name = audio_custom_name_edit ? audio_custom_name_edit->text() : QString();
    if(name.trimmed().isEmpty())
    {
        name = QInputDialog::getText(this, "Save Custom Audio Effect", "Enter name:");
        if(name.trimmed().isEmpty()) return;
    }

    int eff_idx = audio_effect_combo->currentIndex();

    // Index 0 is "None", actual effects start at index 1
    if(eff_idx <= 0 || eff_idx > 4) return;

    const char* class_names[] = { "AudioLevel3D", "SpectrumBars3D", "BeatPulse3D", "BandScan3D" };
    int actual_idx = eff_idx - 1;  // Adjust for "None" at index 0
    std::string class_name = class_names[actual_idx];

    // Capture settings from the currently mounted audio effect UI
    if(!current_audio_effect_ui) { SetupAudioEffectUI(eff_idx); }
    if(!current_audio_effect_ui) return;
    nlohmann::json settings = current_audio_effect_ui->SaveSettings();

    int target = -1;
    if(audio_effect_zone_combo)
    {
        QVariant data = audio_effect_zone_combo->itemData(audio_effect_zone_combo->currentIndex());
        if(data.isValid()) target = data.toInt();
    }

    nlohmann::json j;
    j["name"] = name.toStdString();
    j["effect_class"] = class_name;
    j["target"] = target;
    j["settings"] = settings;

    std::string path = GetAudioCustomEffectPath(name.toStdString());
    QFile file(QString::fromStdString(path));
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out << QString::fromStdString(j.dump(4));
        file.close();
    }
    UpdateAudioCustomEffectsList();
}

void OpenRGB3DSpatialTab::on_audio_custom_load_clicked()
{
    if(!audio_custom_list || audio_custom_list->currentRow() < 0) return;
    QString name = audio_custom_list->currentItem()->text();
    std::string path = GetAudioCustomEffectPath(name.toStdString());
    if(!filesystem::exists(path)) return;
    QFile file(QString::fromStdString(path));
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString json = file.readAll(); file.close();
    try
    {
        nlohmann::json j = nlohmann::json::parse(json.toStdString());
        std::string cls = j.value("effect_class", "");
        // Map class name to combo index (0="None", 1="AudioLevel3D", etc.)
        int idx = 0;
        if(cls == "AudioLevel3D") idx = 1;
        else if(cls == "SpectrumBars3D") idx = 2;
        else if(cls == "BeatPulse3D") idx = 3;
        else if(cls == "BandScan3D") idx = 4;

        if(audio_effect_combo) audio_effect_combo->setCurrentIndex(idx);
        int target = j.value("target", -1);
        if(audio_effect_zone_combo)
        {
            int ti = audio_effect_zone_combo->findData(QVariant(target));
            if(ti >= 0) audio_effect_zone_combo->setCurrentIndex(ti);
        }
        if(j.contains("settings"))
        {
            const nlohmann::json& s = j["settings"];
            // Ensure effect UI exists, then load settings into effect UI
            SetupAudioEffectUI(idx);
            if(current_audio_effect_ui) current_audio_effect_ui->LoadSettings(s);
        }
    }
    catch(...){ }
}

void OpenRGB3DSpatialTab::on_audio_custom_delete_clicked()
{
    if(!audio_custom_list || audio_custom_list->currentRow() < 0) return;
    QString name = audio_custom_list->currentItem()->text();
    std::string path = GetAudioCustomEffectPath(name.toStdString());
    if(filesystem::exists(path)) filesystem::remove(path);
    UpdateAudioCustomEffectsList();
}

void OpenRGB3DSpatialTab::on_audio_custom_add_to_stack_clicked()
{
    if(!audio_custom_list || audio_custom_list->currentRow() < 0) return;
    QString name = audio_custom_list->currentItem()->text();
    std::string path = GetAudioCustomEffectPath(name.toStdString());
    if(!filesystem::exists(path)) return;
    QFile file(QString::fromStdString(path));
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString json = file.readAll(); file.close();
    try
    {
        nlohmann::json j = nlohmann::json::parse(json.toStdString());
        std::string cls = j.value("effect_class", "");
        SpatialEffect3D* eff = EffectListManager3D::get()->CreateEffect(cls);
        if(!eff) return;
        std::unique_ptr<EffectInstance3D> inst = std::make_unique<EffectInstance3D>();
        inst->name = j.value("name", name.toStdString());
        inst->effect_class_name = cls;
        inst->zone_index = j.value("target", -1);
        inst->blend_mode = BlendMode::ADD;
        inst->enabled = true;
        inst->id = next_effect_instance_id++;
        const nlohmann::json& s = j["settings"];
        eff->LoadSettings(s);
        inst->effect.reset(eff);
        inst->saved_settings = std::make_unique<nlohmann::json>(s);

        // Connect ScreenMirror3D screen preview signal to viewport
        if (cls == "ScreenMirror3D")
        {
            ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(eff);
            if (screen_mirror && viewport)
            {
                connect(screen_mirror, &ScreenMirror3D::ScreenPreviewChanged,
                        viewport, &LEDViewport3D::SetShowScreenPreview);
                screen_mirror->SetReferencePoints(&reference_points);
            }
        }

        effect_stack.push_back(std::move(inst));
        UpdateEffectStackList();
        if(effect_stack_list) effect_stack_list->setCurrentRow((int)effect_stack.size()-1);
    }
    catch(...){ }
}





void OpenRGB3DSpatialTab::OnAudioEffectParamsChanged()
{
    if(!current_audio_effect_ui || !running_audio_effect) return;
    nlohmann::json s = current_audio_effect_ui->SaveSettings();
    running_audio_effect->LoadSettings(s);
    if(viewport) viewport->UpdateColors();
}

 

void OpenRGB3DSpatialTab::SetupStandardAudioControls(QVBoxLayout* parent_layout)
{
    if(audio_std_group) return;
    audio_std_group = new QGroupBox("Audio Controls");
    QGridLayout* g = new QGridLayout(audio_std_group);
    int sr = AudioInputManager::instance()->getSampleRate();
    int nyq = std::max(2000, sr > 0 ? sr/2 : 24000);

    // Low/High Hz
    g->addWidget(new QLabel("Low Hz:"), 0, 0);
    audio_low_spin = new QDoubleSpinBox(audio_std_group);
    audio_low_spin->setRange(0, nyq); audio_low_spin->setDecimals(0); audio_low_spin->setValue(60);
    g->addWidget(audio_low_spin, 0, 1);

    g->addWidget(new QLabel("High Hz:"), 0, 2);
    audio_high_spin = new QDoubleSpinBox(audio_std_group);
    audio_high_spin->setRange(0, nyq); audio_high_spin->setDecimals(0); audio_high_spin->setValue(200);
    g->addWidget(audio_high_spin, 0, 3);

    // Smoothing
    g->addWidget(new QLabel("Smoothing:"), 1, 0);
    audio_smooth_slider = new QSlider(Qt::Horizontal, audio_std_group);
    audio_smooth_slider->setRange(0, 99); audio_smooth_slider->setValue(60);
    g->addWidget(audio_smooth_slider, 1, 1, 1, 3);
    audio_smooth_value_label = new QLabel("60%");
    audio_smooth_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    g->addWidget(audio_smooth_value_label, 1, 4);

    // Falloff (gamma shaping)
    g->addWidget(new QLabel("Falloff:"), 2, 0);
    audio_falloff_slider = new QSlider(Qt::Horizontal, audio_std_group);
    audio_falloff_slider->setRange(20, 500); // maps to 0.2 .. 5.0
    audio_falloff_slider->setValue(100);     // 1.0
    g->addWidget(audio_falloff_slider, 2, 1, 1, 3);
    audio_falloff_value_label = new QLabel("1.00x");
    audio_falloff_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    g->addWidget(audio_falloff_value_label, 2, 4);

    // FFT Size (advanced)
    g->addWidget(new QLabel("FFT Size:"), 3, 0);
    audio_fft_combo = new QComboBox(audio_std_group);
    audio_fft_combo->addItems({"512","1024","2048","4096","8192"});
    // Initialize to current analyzer size
    {
        int cur = AudioInputManager::instance()->getFFTSize();
        int idx = audio_fft_combo->findText(QString::number(cur));
        if(idx >= 0) audio_fft_combo->setCurrentIndex(idx);
    }
    g->addWidget(audio_fft_combo, 3, 1);
    g->setColumnStretch(1, 1);
    g->setColumnStretch(3, 1);

    parent_layout->addWidget(audio_std_group);

    connect(audio_low_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_audio_std_low_changed);
    connect(audio_high_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_audio_std_high_changed);
    connect(audio_smooth_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_audio_std_smooth_changed);
    connect(audio_falloff_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_audio_std_falloff_changed);
    connect(audio_fft_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_audio_fft_changed);
}

static inline float mapFalloff(int slider) { return std::max(0.2f, std::min(5.0f, slider / 100.0f)); }

void OpenRGB3DSpatialTab::on_audio_std_low_changed(double)
{
    if(!current_audio_effect_ui) return;
    nlohmann::json s = current_audio_effect_ui->SaveSettings();
    int lowhz = audio_low_spin ? (int)audio_low_spin->value() : 0;
    int highhz = audio_high_spin ? (int)audio_high_spin->value() : lowhz+1;
    // Write per-effect fields
    s["low_hz"] = lowhz;
    s["high_hz"] = highhz;
    // For band-based effects, also map Hz to band indices if keys exist
    if(s.contains("band_start") || s.contains("band_end"))
    {
        int bands = AudioInputManager::instance()->getBandsCount();
        float fs = (float)AudioInputManager::instance()->getSampleRate();
        if(bands <= 0) bands = 1;
        float f_min = std::max(1.0f, fs / (float)AudioInputManager::instance()->getFFTSize());
        float f_max = fs * 0.5f;
        if(f_max <= f_min) f_max = f_min + 1.0f;
        int bs = MapHzToBandIndex((float)lowhz, bands, f_min, f_max);
        int be = MapHzToBandIndex((float)highhz, bands, f_min, f_max);
        if(be <= bs) be = std::min(bs + 1, bands - 1);
        s["band_start"] = bs;
        s["band_end"] = be;
    }
    current_audio_effect_ui->LoadSettings(s);
    if(running_audio_effect) running_audio_effect->LoadSettings(s);
    if(viewport) viewport->UpdateColors();
    // Persist
    if(resource_manager)
    {
        nlohmann::json st = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
        st["AudioLowHz"] = lowhz;
        st["AudioHighHz"] = highhz;
        resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", st);
        resource_manager->GetSettingsManager()->SaveSettings();
    }
}

void OpenRGB3DSpatialTab::on_audio_std_high_changed(double v)
{
    on_audio_std_low_changed(v);
}

void OpenRGB3DSpatialTab::on_audio_std_smooth_changed(int)
{
    if(!current_audio_effect_ui) return;
    nlohmann::json s = current_audio_effect_ui->SaveSettings();
    float smooth = audio_smooth_slider ? (audio_smooth_slider->value() / 100.0f) : 0.6f;
    if(smooth < 0.0f) smooth = 0.0f; if(smooth > 0.99f) smooth = 0.99f;
    s["smoothing"] = smooth;
    if(audio_smooth_value_label && audio_smooth_slider)
    {
        audio_smooth_value_label->setText(QString::number(audio_smooth_slider->value()) + "%");
    }
    current_audio_effect_ui->LoadSettings(s);
    if(running_audio_effect) running_audio_effect->LoadSettings(s);
    if(viewport) viewport->UpdateColors();
    if(resource_manager)
    {
        nlohmann::json st = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
        st["AudioSmoothing"] = audio_smooth_slider ? audio_smooth_slider->value() : 60;
        resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", st);
        resource_manager->GetSettingsManager()->SaveSettings();
    }
}

void OpenRGB3DSpatialTab::on_audio_std_falloff_changed(int)
{
    if(!current_audio_effect_ui) return;
    nlohmann::json s = current_audio_effect_ui->SaveSettings();
    float fo = mapFalloff(audio_falloff_slider ? audio_falloff_slider->value() : 100);
    s["falloff"] = fo;
    if(audio_falloff_value_label)
    {
        audio_falloff_value_label->setText(QString::number(fo, 'f', 2) + "x");
    }
    current_audio_effect_ui->LoadSettings(s);
    if(running_audio_effect) running_audio_effect->LoadSettings(s);
    if(viewport) viewport->UpdateColors();
    if(resource_manager)
    {
        nlohmann::json st = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
        st["AudioFalloff"] = audio_falloff_slider ? audio_falloff_slider->value() : 100;
        resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", st);
        resource_manager->GetSettingsManager()->SaveSettings();
    }
}

void OpenRGB3DSpatialTab::on_audio_effect_zone_changed(int index)
{
    Q_UNUSED(index);
    if(!audio_effect_zone_combo) return;
    QVariant data = audio_effect_zone_combo->itemData(audio_effect_zone_combo->currentIndex());
    if(!data.isValid()) return;
    int target = data.toInt();

    // Apply to running effect if one exists
    if(!effect_stack.empty() && effect_stack[0])
    {
        effect_stack[0]->zone_index = target;
        if(viewport) viewport->UpdateColors();
    }
    // Selection is already stored in combo and will be read when effect starts
}

void OpenRGB3DSpatialTab::on_audio_fft_changed(int)
{
    if(!audio_fft_combo) return;
    int n = audio_fft_combo->currentText().toInt();
    AudioInputManager::instance()->setFFTSize(n);
    // Re-apply Hz mapping for band-based effects since resolution changed
    on_audio_std_low_changed(audio_low_spin ? audio_low_spin->value() : 0.0);
    // Persist setting
    if(resource_manager)
    {
        nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
        settings["AudioFFTSize"] = n;
        resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", settings);
        resource_manager->GetSettingsManager()->SaveSettings();
    }
}

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

DisplayPlane3D* OpenRGB3DSpatialTab::GetSelectedDisplayPlane()
{
    if(current_display_plane_index >= 0 &&
       current_display_plane_index < (int)display_planes.size())
    {
        return display_planes[current_display_plane_index].get();
    }
    return nullptr;
}

void OpenRGB3DSpatialTab::SyncDisplayPlaneControls(DisplayPlane3D* plane)
{
    if(!plane)
    {
        return;
    }

    const Transform3D& transform = plane->GetTransform();

    if(pos_x_spin) { QSignalBlocker block(pos_x_spin); pos_x_spin->setValue(transform.position.x); }
    if(pos_x_slider) { QSignalBlocker block(pos_x_slider); pos_x_slider->setValue((int)std::lround(transform.position.x * 10.0f)); }

    if(pos_y_spin) { QSignalBlocker block(pos_y_spin); pos_y_spin->setValue(transform.position.y); }
    if(pos_y_slider) { QSignalBlocker block(pos_y_slider); pos_y_slider->setValue((int)std::lround(transform.position.y * 10.0f)); }

    if(pos_z_spin) { QSignalBlocker block(pos_z_spin); pos_z_spin->setValue(transform.position.z); }
    if(pos_z_slider) { QSignalBlocker block(pos_z_slider); pos_z_slider->setValue((int)std::lround(transform.position.z * 10.0f)); }

    if(rot_x_spin) { QSignalBlocker block(rot_x_spin); rot_x_spin->setValue(transform.rotation.x); }
    if(rot_x_slider) { QSignalBlocker block(rot_x_slider); rot_x_slider->setValue((int)std::lround(transform.rotation.x)); }

    if(rot_y_spin) { QSignalBlocker block(rot_y_spin); rot_y_spin->setValue(transform.rotation.y); }
    if(rot_y_slider) { QSignalBlocker block(rot_y_slider); rot_y_slider->setValue((int)std::lround(transform.rotation.y)); }

    if(rot_z_spin) { QSignalBlocker block(rot_z_spin); rot_z_spin->setValue(transform.rotation.z); }
    if(rot_z_slider) { QSignalBlocker block(rot_z_slider); rot_z_slider->setValue((int)std::lround(transform.rotation.z)); }

    if(display_plane_name_edit)
    {
        QSignalBlocker block(display_plane_name_edit);
        display_plane_name_edit->setText(QString::fromStdString(plane->GetName()));
    }
    if(display_plane_width_spin)
    {
        QSignalBlocker block(display_plane_width_spin);
        display_plane_width_spin->setValue(plane->GetWidthMM());
    }
    if(display_plane_height_spin)
    {
        QSignalBlocker block(display_plane_height_spin);
        display_plane_height_spin->setValue(plane->GetHeightMM());
    }
    if(display_plane_bezel_spin)
    {
        QSignalBlocker block(display_plane_bezel_spin);
        display_plane_bezel_spin->setValue(plane->GetBezelMM());
    }
    if(display_plane_capture_combo)
    {
        QSignalBlocker block(display_plane_capture_combo);
        std::string current_source = plane->GetCaptureSourceId();

        // Try to find and select the current source
        int index = -1;
        for(int i = 0; i < display_plane_capture_combo->count(); i++)
        {
            if(display_plane_capture_combo->itemData(i).toString().toStdString() == current_source)
            {
                index = i;
                break;
            }
        }

        if(index >= 0)
        {
            display_plane_capture_combo->setCurrentIndex(index);
        }
        else if(!current_source.empty())
        {
            // Source not in list, but plane has one configured - add it as custom entry
            display_plane_capture_combo->addItem(QString::fromStdString(current_source) + " (custom)",
                                                  QString::fromStdString(current_source));
            display_plane_capture_combo->setCurrentIndex(display_plane_capture_combo->count() - 1);
        }
        else
        {
            // No source configured, select "(None)"
            display_plane_capture_combo->setCurrentIndex(0);
        }
    }
    if(display_plane_visible_check)
    {
        QSignalBlocker block(display_plane_visible_check);
        display_plane_visible_check->setCheckState(plane->IsVisible() ? Qt::Checked : Qt::Unchecked);
    }
}

void OpenRGB3DSpatialTab::UpdateDisplayPlanesList()
{
    if(!display_planes_list)
    {
        return;
    }

    int desired_index = current_display_plane_index;

    display_planes_list->blockSignals(true);
    display_planes_list->clear();
    for(size_t i = 0; i < display_planes.size(); i++)
    {
        const DisplayPlane3D* plane = display_planes[i].get();
        if(!plane) continue;
        QString label = QString::fromStdString(plane->GetName()) +
                        QString(" (%1 x %2 mm)")
                            .arg(plane->GetWidthMM(), 0, 'f', 0)
                            .arg(plane->GetHeightMM(), 0, 'f', 0);
        QListWidgetItem* item = new QListWidgetItem(label, display_planes_list);
        if(!plane->IsVisible())
        {
            item->setForeground(QColor("#888888"));
        }
    }
    display_planes_list->blockSignals(false);

    if(display_planes.empty())
    {
        current_display_plane_index = -1;
        if(remove_display_plane_button) remove_display_plane_button->setEnabled(false);
        if(viewport) viewport->SelectDisplayPlane(-1);
        RefreshDisplayPlaneDetails();
        return;
    }

    if(desired_index < 0 || desired_index >= (int)display_planes.size())
    {
        desired_index = 0;
    }

    current_display_plane_index = desired_index;
    display_planes_list->setCurrentRow(desired_index);
    if(viewport) viewport->SelectDisplayPlane(desired_index);
    RefreshDisplayPlaneDetails();
}


void OpenRGB3DSpatialTab::RefreshDisplayPlaneDetails()
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    bool has_plane = (plane != nullptr);

    if(remove_display_plane_button) remove_display_plane_button->setEnabled(has_plane);

    QList<QWidget*> widgets = {
        display_plane_name_edit,
        display_plane_width_spin,
        display_plane_height_spin,
        display_plane_bezel_spin,
        display_plane_capture_combo,
        display_plane_refresh_capture_btn,
        display_plane_visible_check
    };

    for(QWidget* w : widgets)
    {
        if(w) w->setEnabled(has_plane);
    }

    if(!has_plane)
    {
        if(display_plane_name_edit) display_plane_name_edit->setText("");
        if(display_plane_width_spin) display_plane_width_spin->setValue(1000.0);
        if(display_plane_height_spin) display_plane_height_spin->setValue(600.0);
        if(display_plane_bezel_spin) display_plane_bezel_spin->setValue(10.0);
        if(display_plane_capture_combo) display_plane_capture_combo->setCurrentIndex(0);
        if(display_plane_visible_check) display_plane_visible_check->setCheckState(Qt::Unchecked);
        return;
    }

    if(display_plane_name_edit)
    {
        QSignalBlocker block(display_plane_name_edit);
        display_plane_name_edit->setText(QString::fromStdString(plane->GetName()));
    }
    if(display_plane_width_spin)
    {
        QSignalBlocker block(display_plane_width_spin);
        display_plane_width_spin->setValue(plane->GetWidthMM());
    }
    if(display_plane_height_spin)
    {
        QSignalBlocker block(display_plane_height_spin);
        display_plane_height_spin->setValue(plane->GetHeightMM());
    }
    if(display_plane_bezel_spin)
    {
        QSignalBlocker block(display_plane_bezel_spin);
        display_plane_bezel_spin->setValue(plane->GetBezelMM());
    }
    if(display_plane_capture_combo)
    {
        QSignalBlocker block(display_plane_capture_combo);
        std::string current_source = plane->GetCaptureSourceId();

        // Try to find and select the current source
        int index = -1;
        for(int i = 0; i < display_plane_capture_combo->count(); i++)
        {
            if(display_plane_capture_combo->itemData(i).toString().toStdString() == current_source)
            {
                index = i;
                break;
            }
        }

        if(index >= 0)
        {
            display_plane_capture_combo->setCurrentIndex(index);
        }
        else if(!current_source.empty())
        {
            // Source not in list, but plane has one configured - add it as custom entry
            display_plane_capture_combo->addItem(QString::fromStdString(current_source) + " (custom)",
                                                  QString::fromStdString(current_source));
            display_plane_capture_combo->setCurrentIndex(display_plane_capture_combo->count() - 1);
        }
        else
        {
            // No source configured, select "(None)"
            display_plane_capture_combo->setCurrentIndex(0);
        }
    }
    if(display_plane_visible_check)
    {
        QSignalBlocker block(display_plane_visible_check);
        display_plane_visible_check->setCheckState(plane->IsVisible() ? Qt::Checked : Qt::Unchecked);
    }

    SyncDisplayPlaneControls(plane);
}

void OpenRGB3DSpatialTab::NotifyDisplayPlaneChanged()
{
    if(viewport)
    {
        viewport->NotifyDisplayPlaneChanged();
    }

    // Sync display planes to global manager for effects to access
    std::vector<DisplayPlane3D*> plane_ptrs;
    for(auto& plane : display_planes)
    {
        if(plane)
        {
            plane_ptrs.push_back(plane.get());
        }
    }
    DisplayPlaneManager::instance()->SetDisplayPlanes(plane_ptrs);

    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_selected(int index)
{
    current_display_plane_index = index;

    if(controller_list)
    {
        QSignalBlocker block(controller_list);
        controller_list->clearSelection();
    }
    if(reference_points_list)
    {
        QSignalBlocker block(reference_points_list);
        reference_points_list->clearSelection();
    }

    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    SyncDisplayPlaneControls(plane);
    RefreshDisplayPlaneDetails();
    if(viewport) viewport->SelectDisplayPlane(index);
}

void OpenRGB3DSpatialTab::on_add_display_plane_clicked()
{
    std::string base_name = "Display Plane";
    int suffix = (int)display_planes.size() + 1;
    std::unique_ptr<DisplayPlane3D> plane = std::make_unique<DisplayPlane3D>(base_name + " " + std::to_string(suffix));

    float room_depth_units = room_depth_spin ? ((float)room_depth_spin->value() / grid_scale_mm) : 100.0f;
    float room_height_units = room_height_spin ? ((float)room_height_spin->value() / grid_scale_mm) : 100.0f;

    plane->GetTransform().position.x = 0.0f;
    plane->GetTransform().position.y = -room_depth_units * 0.25f;
    plane->GetTransform().position.z = room_height_units * 0.5f;

    display_planes.push_back(std::move(plane));
    current_display_plane_index = (int)display_planes.size() - 1;
    UpdateDisplayPlanesList();
    DisplayPlane3D* new_plane = GetSelectedDisplayPlane();
    SyncDisplayPlaneControls(new_plane);
    RefreshDisplayPlaneDetails();
    if(viewport) viewport->SelectDisplayPlane(current_display_plane_index);
    NotifyDisplayPlaneChanged();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_remove_display_plane_clicked()
{
    if(current_display_plane_index < 0 ||
       current_display_plane_index >= (int)display_planes.size())
    {
        return;
    }

    display_planes.erase(display_planes.begin() + current_display_plane_index);
    if(current_display_plane_index >= (int)display_planes.size())
    {
        current_display_plane_index = (int)display_planes.size() - 1;
    }
    UpdateDisplayPlanesList();
    RefreshDisplayPlaneDetails();
    NotifyDisplayPlaneChanged();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_name_edited(const QString& text)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane)
    {
        return;
    }
    plane->SetName(text.toStdString());
    UpdateDisplayPlanesList();
    NotifyDisplayPlaneChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_width_changed(double value)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetWidthMM((float)value);
    UpdateDisplayPlanesList();
    NotifyDisplayPlaneChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_height_changed(double value)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetHeightMM((float)value);
    UpdateDisplayPlanesList();
    NotifyDisplayPlaneChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_bezel_changed(double value)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetBezelMM((float)value);
    NotifyDisplayPlaneChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_capture_changed(int index)
{
    if(!display_plane_capture_combo) return;

    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;

    QString capture_id = display_plane_capture_combo->itemData(index).toString();
    plane->SetCaptureSourceId(capture_id.toStdString());
    NotifyDisplayPlaneChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_refresh_capture_clicked()
{
    RefreshDisplayPlaneCaptureSourceList();
}
void OpenRGB3DSpatialTab::on_display_plane_position_signal(int index, float x, float y, float z)
{
    if(index < 0)
    {
        current_display_plane_index = -1;
        if(display_planes_list)
        {
            QSignalBlocker block(display_planes_list);
            display_planes_list->clearSelection();
        }
        RefreshDisplayPlaneDetails();
        return;
    }

    if(index >= (int)display_planes.size())
    {
        return;
    }

    current_display_plane_index = index;
    if(display_planes_list)
    {
        QSignalBlocker block(display_planes_list);
        display_planes_list->setCurrentRow(index);
    }
    if(controller_list)
    {
        QSignalBlocker block(controller_list);
        controller_list->clearSelection();
    }
    if(reference_points_list)
    {
        QSignalBlocker block(reference_points_list);
        reference_points_list->clearSelection();
    }

    DisplayPlane3D* plane = display_planes[index].get();
    if(!plane)
    {
        return;
    }

    Transform3D& transform = plane->GetTransform();
    transform.position.x = x;
    transform.position.y = y;
    transform.position.z = z;

    SyncDisplayPlaneControls(plane);
    RefreshDisplayPlaneDetails();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_rotation_signal(int index, float x, float y, float z)
{
    if(index < 0)
    {
        return;
    }

    if(index >= (int)display_planes.size())
    {
        return;
    }

    current_display_plane_index = index;
    if(display_planes_list)
    {
        QSignalBlocker block(display_planes_list);
        display_planes_list->setCurrentRow(index);
    }
    if(controller_list)
    {
        QSignalBlocker block(controller_list);
        controller_list->clearSelection();
    }
    if(reference_points_list)
    {
        QSignalBlocker block(reference_points_list);
        reference_points_list->clearSelection();
    }

    DisplayPlane3D* plane = display_planes[index].get();
    if(!plane)
    {
        return;
    }

    Transform3D& transform = plane->GetTransform();
    transform.rotation.x = x;
    transform.rotation.y = y;
    transform.rotation.z = z;

    SyncDisplayPlaneControls(plane);
    RefreshDisplayPlaneDetails();
    emit GridLayoutChanged();
}

void OpenRGB3DSpatialTab::on_display_plane_visible_toggled(int state)
{
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(!plane) return;
    plane->SetVisible(state == Qt::Checked);
    UpdateDisplayPlanesList();
    SyncDisplayPlaneControls(plane);
    NotifyDisplayPlaneChanged();
    emit GridLayoutChanged();
}

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
void OpenRGB3DSpatialTab::RefreshDisplayPlaneCaptureSourceList()
{
    if(!display_plane_capture_combo)
    {
        return;
    }

    // Get current selection before clearing
    QString current_selection;
    if(display_plane_capture_combo->currentIndex() >= 0)
    {
        current_selection = display_plane_capture_combo->currentData().toString();
    }

    // Initialize ScreenCaptureManager if needed
    auto& capture_mgr = ScreenCaptureManager::Instance();
    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }

    // Refresh available sources
    capture_mgr.RefreshSources();
    auto sources = capture_mgr.GetAvailableSources();

    // Clear and repopulate combo
    display_plane_capture_combo->clear();
    display_plane_capture_combo->addItem("(None)", "");

    for(const auto& source : sources)
    {
        QString label = QString::fromStdString(source.name);
        if(source.is_primary)
        {
            label += " [Primary]";
        }
        label += QString(" (%1x%2)").arg(source.width).arg(source.height);

        display_plane_capture_combo->addItem(label, QString::fromStdString(source.id));
    }

    // Try to restore previous selection
    if(!current_selection.isEmpty())
    {
        for(int i = 0; i < display_plane_capture_combo->count(); i++)
        {
            if(display_plane_capture_combo->itemData(i).toString() == current_selection)
            {
                display_plane_capture_combo->setCurrentIndex(i);
                return;
            }
        }
    }

    // If we get here and have a selected plane, sync its capture source
    DisplayPlane3D* plane = GetSelectedDisplayPlane();
    if(plane)
    {
        std::string plane_source = plane->GetCaptureSourceId();
        for(int i = 0; i < display_plane_capture_combo->count(); i++)
        {
            if(display_plane_capture_combo->itemData(i).toString().toStdString() == plane_source)
            {
                display_plane_capture_combo->setCurrentIndex(i);
                return;
            }
        }
    }
}
