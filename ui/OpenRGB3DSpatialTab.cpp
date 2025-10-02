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
#include "Effects3D/Wave3D/Wave3D.h"
#include "Effects3D/Wipe3D/Wipe3D.h"
#include "Effects3D/Plasma3D/Plasma3D.h"
#include "Effects3D/Spiral3D/Spiral3D.h"
#include "Effects3D/Spin3D/Spin3D.h"
#include "Effects3D/DNAHelix3D/DNAHelix3D.h"
#include "Effects3D/BreathingSphere3D/BreathingSphere3D.h"
#include "Effects3D/Explosion3D/Explosion3D.h"
#include <QColorDialog>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>
#include <fstream>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include <QFileInfo>
#include <QInputDialog>
#include <set>
#include <QFileDialog>
#include <filesystem>
#include <cmath>
#include "SettingsManager.h"
#include <nlohmann/json.hpp>

/*---------------------------------------------------------*\
| Helper function to calculate origin offset based on     |
| room preset and grid size                                |
\*---------------------------------------------------------*/
static void CalculateOriginOffset(OriginPreset preset, const UserPosition3D& user_pos, int grid_size, float& offset_x, float& offset_y, float& offset_z)
{
    float half_grid = grid_size / 2.0f;

    switch(preset)
    {
        case ORIGIN_USER_POSITION:
            offset_x = user_pos.x;
            offset_y = user_pos.y;
            offset_z = user_pos.z;
            break;
        case ORIGIN_ROOM_CENTER:
            offset_x = 0.0f;
            offset_y = 0.0f;
            offset_z = 0.0f;
            break;
        case ORIGIN_FLOOR_CENTER:
            offset_x = 0.0f;
            offset_y = 0.0f;
            offset_z = -half_grid;
            break;
        case ORIGIN_CEILING_CENTER:
            offset_x = 0.0f;
            offset_y = 0.0f;
            offset_z = half_grid;
            break;
        case ORIGIN_FRONT_WALL:
            offset_x = 0.0f;
            offset_y = half_grid;
            offset_z = 0.0f;
            break;
        case ORIGIN_BACK_WALL:
            offset_x = 0.0f;
            offset_y = -half_grid;
            offset_z = 0.0f;
            break;
        case ORIGIN_LEFT_WALL:
            offset_x = -half_grid;
            offset_y = 0.0f;
            offset_z = 0.0f;
            break;
        case ORIGIN_RIGHT_WALL:
            offset_x = half_grid;
            offset_y = 0.0f;
            offset_z = 0.0f;
            break;
        case ORIGIN_FLOOR_FRONT:
            offset_x = 0.0f;
            offset_y = -half_grid;
            offset_z = -half_grid;
            break;
        case ORIGIN_FLOOR_BACK:
            offset_x = 0.0f;
            offset_y = half_grid;
            offset_z = -half_grid;
            break;
        case ORIGIN_CUSTOM:
        default:
            offset_x = 0.0f;
            offset_y = 0.0f;
            offset_z = 0.0f;
            break;
    }
}

OpenRGB3DSpatialTab::OpenRGB3DSpatialTab(ResourceManagerInterface* rm, QWidget *parent) :
    QWidget(parent),
    resource_manager(rm),
    first_load(true)
{

    effect_controls_widget = nullptr;
    current_effect_ui = nullptr;
    wave3d_effect = nullptr;
    wipe3d_effect = nullptr;
    plasma3d_effect = nullptr;
    spiral3d_effect = nullptr;
    spin3d_effect = nullptr;
    explosion3d_effect = nullptr;
    breathingsphere3d_effect = nullptr;
    dnahelix3d_effect = nullptr;


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

    led_spacing_x_spin = nullptr;
    led_spacing_y_spin = nullptr;
    led_spacing_z_spin = nullptr;
    led_spacing_preset_combo = nullptr;

    SetupUI();
    LoadDevices();
    LoadCustomControllers();

    auto_load_timer = new QTimer(this);
    auto_load_timer->setSingleShot(true);
    connect(auto_load_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::TryAutoLoadLayout);
    auto_load_timer->start(2000);


    effect_timer = new QTimer(this);
    connect(effect_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::on_effect_timer_timeout);
    effect_running = false;
    effect_time = 0.0f;
}

OpenRGB3DSpatialTab::~OpenRGB3DSpatialTab()
{
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


    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        delete controller_transforms[i];
    }
    controller_transforms.clear();

    for(unsigned int i = 0; i < virtual_controllers.size(); i++)
    {
        delete virtual_controllers[i];
    }
    virtual_controllers.clear();
}

void OpenRGB3DSpatialTab::SetupUI()
{
    QHBoxLayout* main_layout = new QHBoxLayout(this);
    main_layout->setSpacing(8);
    main_layout->setContentsMargins(8, 8, 8, 8);

    /*---------------------------------------------------------*\
    | Left panel with scroll area                              |
    \*---------------------------------------------------------*/
    QScrollArea* left_scroll = new QScrollArea();
    left_scroll->setWidgetResizable(true);
    left_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    left_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    left_scroll->setMinimumWidth(300);
    left_scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QWidget* left_content = new QWidget();
    QVBoxLayout* left_panel = new QVBoxLayout(left_content);
    left_panel->setSpacing(8);

    /*---------------------------------------------------------*\
    | Tab Widget for left panel                                |
    \*---------------------------------------------------------*/
    QTabWidget* left_tabs = new QTabWidget();

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

    // Create button
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

    // Add tabs to left panel
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
    | Layout Profiles (moved from right panel)                |
    \*---------------------------------------------------------*/
    QGroupBox* profile_group = new QGroupBox("Layout Profiles");
    QVBoxLayout* profile_layout = new QVBoxLayout();

    layout_profiles_combo = new QComboBox();
    connect(layout_profiles_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_layout_profile_changed(int)));
    profile_layout->addWidget(layout_profiles_combo);

    QHBoxLayout* save_load_layout = new QHBoxLayout();
    QPushButton* save_button = new QPushButton("Save Layout");
    connect(save_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_save_layout_clicked);
    save_load_layout->addWidget(save_button);

    QPushButton* load_button = new QPushButton("Load Layout");
    connect(load_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_load_layout_clicked);
    save_load_layout->addWidget(load_button);
    profile_layout->addLayout(save_load_layout);

    QPushButton* delete_button = new QPushButton("Delete Profile");
    connect(delete_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_delete_layout_clicked);
    profile_layout->addWidget(delete_button);

    auto_load_checkbox = new QCheckBox("Auto-load selected profile on startup");

    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    bool auto_load_enabled = false;
    if(settings.contains("AutoLoad"))
    {
        auto_load_enabled = settings["AutoLoad"];
    }
    auto_load_checkbox->setChecked(auto_load_enabled);

    connect(auto_load_checkbox, &QCheckBox::toggled, [this](bool checked) {
        nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
        settings["AutoLoad"] = checked;
        resource_manager->GetSettingsManager()->SetSettings("3DSpatialPlugin", settings);
        resource_manager->GetSettingsManager()->SaveSettings();
    });
    profile_layout->addWidget(auto_load_checkbox);

    PopulateLayoutDropdown();

    profile_group->setLayout(profile_layout);
    left_panel->addWidget(profile_group);

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

    QLabel* controls_label = new QLabel("Camera: Middle mouse = Rotate | Right/Shift+Middle = Pan | Scroll = Zoom | Left click = Select/Move device");
    middle_panel->addWidget(controls_label);

    viewport = new LEDViewport3D();
    viewport->SetControllerTransforms(&controller_transforms);
    viewport->SetGridDimensions(custom_grid_x, custom_grid_y, custom_grid_z);
    viewport->SetGridSnapEnabled(false); // Initialize with snapping disabled
    connect(viewport, SIGNAL(ControllerSelected(int)), this, SLOT(on_controller_selected(int)));
    connect(viewport, SIGNAL(ControllerPositionChanged(int,float,float,float)),
            this, SLOT(on_controller_position_changed(int,float,float,float)));
    connect(viewport, SIGNAL(ControllerRotationChanged(int,float,float,float)),
            this, SLOT(on_controller_rotation_changed(int,float,float,float)));
    connect(viewport, SIGNAL(ControllerDeleteRequested(int)), this, SLOT(on_remove_controller_from_viewport(int)));
    middle_panel->addWidget(viewport, 1);

    /*---------------------------------------------------------*\
    | Tab Widget for Grid Settings and Position/Rotation      |
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

    // Grid Snap Checkbox
    grid_snap_checkbox = new QCheckBox("Grid Snapping");
    grid_snap_checkbox->setToolTip("Snap controller positions to grid intersections");
    layout_layout->addWidget(grid_snap_checkbox, 1, 3, 1, 3);

    // Selection Info Label
    selection_info_label = new QLabel("No selection");
    selection_info_label->setStyleSheet("color: gray; font-size: 10px; font-weight: bold;");
    selection_info_label->setAlignment(Qt::AlignRight);
    layout_layout->addWidget(selection_info_label, 1, 3, 1, 3);

    // User Position Controls (Row 2)
    layout_layout->addWidget(new QLabel("User X:"), 2, 0);
    user_pos_x_spin = new QDoubleSpinBox();
    user_pos_x_spin->setRange(-50.0, 50.0);
    user_pos_x_spin->setSingleStep(0.5);
    user_pos_x_spin->setValue(user_position.x);
    user_pos_x_spin->setToolTip("User head X position (left/right) - perception origin for effects");
    layout_layout->addWidget(user_pos_x_spin, 2, 1);

    layout_layout->addWidget(new QLabel("User Y:"), 2, 2);
    user_pos_y_spin = new QDoubleSpinBox();
    user_pos_y_spin->setRange(-50.0, 50.0);
    user_pos_y_spin->setSingleStep(0.5);
    user_pos_y_spin->setValue(user_position.y);
    user_pos_y_spin->setToolTip("User head Y position (floor/ceiling) - perception origin for effects");
    layout_layout->addWidget(user_pos_y_spin, 2, 3);

    layout_layout->addWidget(new QLabel("User Z:"), 2, 4);
    user_pos_z_spin = new QDoubleSpinBox();
    user_pos_z_spin->setRange(-50.0, 50.0);
    user_pos_z_spin->setSingleStep(0.5);
    user_pos_z_spin->setValue(user_position.z);
    user_pos_z_spin->setToolTip("User head Z position (front/back) - perception origin for effects");
    layout_layout->addWidget(user_pos_z_spin, 2, 5);

    // User controls row (Row 3)
    user_visible_checkbox = new QCheckBox("Show User Head");
    user_visible_checkbox->setChecked(user_position.visible);
    user_visible_checkbox->setToolTip("Show/hide user head (green smiley face) in viewport");
    layout_layout->addWidget(user_visible_checkbox, 3, 0, 1, 2);

    user_center_button = new QPushButton("Center User");
    user_center_button->setToolTip("Move user head to room center (0,0,0)");
    layout_layout->addWidget(user_center_button, 3, 2, 1, 2);

    // User reference point row (Row 4)
    use_user_reference_checkbox = new QCheckBox("Use User Head as Effect Origin");
    use_user_reference_checkbox->setChecked(false);
    use_user_reference_checkbox->setToolTip("When enabled, effects originate from user head position instead of room center (0,0,0)");
    layout_layout->addWidget(use_user_reference_checkbox, 4, 0, 1, 6);

    // Add helpful labels with text wrapping
    QLabel* grid_help1 = new QLabel("LEDs mapped sequentially to grid positions (X, Y, Z)");
    grid_help1->setStyleSheet("color: gray; font-size: 10px;");
    grid_help1->setWordWrap(true);
    layout_layout->addWidget(grid_help1, 4, 0, 1, 6);

    QLabel* grid_help2 = new QLabel("Effects originate from user position • Use Ctrl+Click for multi-select");
    grid_help2->setStyleSheet("color: gray; font-size: 10px;");
    grid_help2->setWordWrap(true);
    layout_layout->addWidget(grid_help2, 5, 0, 1, 6);

    grid_settings_tab->setLayout(layout_layout);
    settings_tabs->addTab(grid_settings_tab, "Grid Settings");

    // Connect signals
    connect(grid_x_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_y_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_z_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_snap_checkbox, &QCheckBox::toggled, this, &OpenRGB3DSpatialTab::on_grid_snap_toggled);

    // User position signals
    connect(user_pos_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_user_position_changed);
    connect(user_pos_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_user_position_changed);
    connect(user_pos_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_user_position_changed);
    connect(user_visible_checkbox, &QCheckBox::toggled, this, &OpenRGB3DSpatialTab::on_user_visibility_toggled);
    connect(user_center_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_user_center_clicked);
    connect(use_user_reference_checkbox, &QCheckBox::toggled, this, &OpenRGB3DSpatialTab::on_use_user_reference_toggled);

    /*---------------------------------------------------------*\
    | Position & Rotation Tab                                  |
    \*---------------------------------------------------------*/
    QWidget* transform_tab = new QWidget();
    QGridLayout* position_layout = new QGridLayout();
    position_layout->setSpacing(5);

    position_layout->addWidget(new QLabel("Position X:"), 0, 0);

    pos_x_slider = new QSlider(Qt::Horizontal);
    pos_x_slider->setRange(-1000, 1000);
    pos_x_slider->setValue(0);
    connect(pos_x_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        pos_x_spin->setValue(pos_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.x = pos_value;
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(pos_x_slider, 0, 1);

    pos_x_spin = new QDoubleSpinBox();
    pos_x_spin->setRange(-100, 100);
    pos_x_spin->setDecimals(1);
    pos_x_spin->setMaximumWidth(80);
    connect(pos_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        pos_x_slider->setValue((int)(value * 10));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.x = value;
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(pos_x_spin, 0, 2);

    position_layout->addWidget(new QLabel("Position Y:"), 1, 0);

    pos_y_slider = new QSlider(Qt::Horizontal);
    pos_y_slider->setRange(0, 1000);  // Start from 0 (floor level)
    pos_y_slider->setValue(0);
    connect(pos_y_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        // Ensure Y position never goes below floor level
        if(pos_value < 0.0f) {
            pos_value = 0.0f;
            pos_y_slider->setValue((int)(pos_value * 10));
        }
        pos_y_spin->setValue(pos_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.y = pos_value;
            // Enforce floor constraint after position change
            viewport->EnforceFloorConstraint(controller_transforms[row]);
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(pos_y_slider, 1, 1);

    pos_y_spin = new QDoubleSpinBox();
    pos_y_spin->setRange(0, 100);  // Floor constraint
    pos_y_spin->setDecimals(1);
    pos_y_spin->setMaximumWidth(80);
    connect(pos_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        // Ensure Y position never goes below floor level
        if(value < 0.0) {
            value = 0.0;
            pos_y_spin->setValue(value);
        }
        pos_y_slider->setValue((int)(value * 10));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.y = value;
            // Enforce floor constraint after position change
            viewport->EnforceFloorConstraint(controller_transforms[row]);
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(pos_y_spin, 1, 2);

    position_layout->addWidget(new QLabel("Position Z:"), 2, 0);

    pos_z_slider = new QSlider(Qt::Horizontal);
    pos_z_slider->setRange(-1000, 1000);
    pos_z_slider->setValue(0);
    connect(pos_z_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        pos_z_spin->setValue(pos_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.z = pos_value;
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(pos_z_slider, 2, 1);

    pos_z_spin = new QDoubleSpinBox();
    pos_z_spin->setRange(-100, 100);
    pos_z_spin->setDecimals(1);
    pos_z_spin->setMaximumWidth(80);
    connect(pos_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        pos_z_slider->setValue((int)(value * 10));
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.position.z = value;
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(pos_z_spin, 2, 2);

    position_layout->addWidget(new QLabel("Rotation X:"), 3, 0);

    rot_x_slider = new QSlider(Qt::Horizontal);
    rot_x_slider->setRange(-180, 180);
    rot_x_slider->setValue(0);
    connect(rot_x_slider, &QSlider::valueChanged, [this](int value) {
        double rot_value = value;
        rot_x_spin->setValue(rot_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.x = rot_value;
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(rot_x_slider, 3, 1);

    rot_x_spin = new QDoubleSpinBox();
    rot_x_spin->setRange(-180, 180);
    rot_x_spin->setDecimals(1);
    rot_x_spin->setMaximumWidth(80);
    connect(rot_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        rot_x_slider->setValue((int)value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.x = value;
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(rot_x_spin, 3, 2);

    position_layout->addWidget(new QLabel("Rotation Y:"), 4, 0);

    rot_y_slider = new QSlider(Qt::Horizontal);
    rot_y_slider->setRange(-180, 180);
    rot_y_slider->setValue(0);
    connect(rot_y_slider, &QSlider::valueChanged, [this](int value) {
        double rot_value = value;
        rot_y_spin->setValue(rot_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.y = rot_value;
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(rot_y_slider, 4, 1);

    rot_y_spin = new QDoubleSpinBox();
    rot_y_spin->setRange(-180, 180);
    rot_y_spin->setDecimals(1);
    rot_y_spin->setMaximumWidth(80);
    connect(rot_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        rot_y_slider->setValue((int)value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.y = value;
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(rot_y_spin, 4, 2);

    position_layout->addWidget(new QLabel("Rotation Z:"), 5, 0);

    rot_z_slider = new QSlider(Qt::Horizontal);
    rot_z_slider->setRange(-180, 180);
    rot_z_slider->setValue(0);
    connect(rot_z_slider, &QSlider::valueChanged, [this](int value) {
        double rot_value = value;
        rot_z_spin->setValue(rot_value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.z = rot_value;
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(rot_z_slider, 5, 1);

    rot_z_spin = new QDoubleSpinBox();
    rot_z_spin->setRange(-180, 180);
    rot_z_spin->setDecimals(1);
    rot_z_spin->setMaximumWidth(80);
    connect(rot_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        rot_z_slider->setValue((int)value);
        int row = controller_list->currentRow();
        if(row >= 0 && row < (int)controller_transforms.size())
        {
            controller_transforms[row]->transform.rotation.z = value;
            viewport->NotifyControllerTransformChanged();
        }
    });
    position_layout->addWidget(rot_z_spin, 5, 2);

    transform_tab->setLayout(position_layout);
    settings_tabs->addTab(transform_tab, "Position & Rotation");

    // Add the tab widget to middle panel
    middle_panel->addWidget(settings_tabs);

    main_layout->addLayout(middle_panel, 3);  // Give middle panel more space

    QVBoxLayout* right_panel = new QVBoxLayout();

    /*---------------------------------------------------------*\
    | Effects Section                                          |
    \*---------------------------------------------------------*/
    QGroupBox* effects_group = new QGroupBox("Effects");
    QVBoxLayout* effects_layout = new QVBoxLayout();

    // Effect selection
    effect_combo = new QComboBox();
    effect_combo->blockSignals(true);  // Block signals during initialization
    effect_combo->addItem("None");
    effect_combo->addItem("Wave 3D");         // effect_type 0
    effect_combo->addItem("Wipe 3D");         // effect_type 1
    effect_combo->addItem("Plasma 3D");       // effect_type 2
    effect_combo->addItem("Spiral 3D");       // effect_type 3
    effect_combo->addItem("Spin 3D");         // effect_type 4
    effect_combo->addItem("DNA Helix 3D");    // effect_type 5
    effect_combo->addItem("Breathing Sphere 3D"); // effect_type 6
    effect_combo->addItem("Explosion 3D");    // effect_type 7
    effect_combo->blockSignals(false); // Re-enable signals after initialization

    connect(effect_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_effect_changed);

    effects_layout->addWidget(new QLabel("Effect:"));
    effects_layout->addWidget(effect_combo);

    // Effect-specific controls container
    effect_controls_widget = new QWidget();
    effect_controls_layout = new QVBoxLayout();
    effect_controls_widget->setLayout(effect_controls_layout);
    effects_layout->addWidget(effect_controls_widget);

    effects_group->setLayout(effects_layout);
    right_panel->addWidget(effects_group);


    /*---------------------------------------------------------*\
    | Add stretch to push content to top of right panel       |
    \*---------------------------------------------------------*/
    right_panel->addStretch();

    /*---------------------------------------------------------*\
    | Create right scroll area and add to main layout         |
    \*---------------------------------------------------------*/
    QScrollArea* right_scroll = new QScrollArea();
    right_scroll->setWidgetResizable(true);
    right_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    right_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    right_scroll->setMinimumWidth(300);
    right_scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QWidget* right_content = new QWidget();
    right_content->setLayout(right_panel);
    right_scroll->setWidget(right_content);

    main_layout->addWidget(right_scroll, 1);

    setLayout(main_layout);
}

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
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        controller_list->setCurrentRow(index);

        ControllerTransform* ctrl = controller_transforms[index];

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
}

void OpenRGB3DSpatialTab::on_controller_position_changed(int index, float x, float y, float z)
{
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        ControllerTransform* ctrl = controller_transforms[index];
        ctrl->transform.position.x = x;
        ctrl->transform.position.y = y;
        ctrl->transform.position.z = z;

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
        ControllerTransform* ctrl = controller_transforms[index];
        ctrl->transform.rotation.x = x;
        ctrl->transform.rotation.y = y;
        ctrl->transform.rotation.z = z;

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
        ControllerTransform* transform = controller_transforms[ctrl_idx];
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

    /*---------------------------------------------------------*\
    | Set timer interval based on FPS (default 30 FPS = 33ms) |
    \*---------------------------------------------------------*/
    effect_timer->start(33);

    /*---------------------------------------------------------*\
    | Update UI                                                |
    \*---------------------------------------------------------*/
    start_effect_button->setEnabled(false);
    stop_effect_button->setEnabled(true);

}

void OpenRGB3DSpatialTab::on_stop_effect_clicked()
{

    /*---------------------------------------------------------*\
    | Stop the effect                                          |
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

    /*---------------------------------------------------------*\
    | Update effect time                                       |
    \*---------------------------------------------------------*/
    effect_time += 0.033f; // ~30 FPS

    /*---------------------------------------------------------*\
    | Apply effect over the entire grid space                 |
    \*---------------------------------------------------------*/
    // Safety check: ensure grid dimensions are valid
    if(custom_grid_x < 1) custom_grid_x = 10;
    if(custom_grid_y < 1) custom_grid_y = 10;
    if(custom_grid_z < 1) custom_grid_z = 10;

    // Calculate room-centered grid bounds (user at center)
    int half_x = custom_grid_x / 2;
    int half_y = custom_grid_y / 2;
    int half_z = custom_grid_z / 2;

    float grid_min_x = -half_x;
    float grid_max_x = custom_grid_x - half_x - 1;
    float grid_min_y = -half_y;
    float grid_max_y = custom_grid_y - half_y - 1;
    float grid_min_z = -half_z;
    float grid_max_z = custom_grid_z - half_z - 1;

    // Create grid context for effects
    GridContext3D grid_context(grid_min_x, grid_max_x, grid_min_y, grid_max_y, grid_min_z, grid_max_z);

    // Get origin offset for room-based effects (default: user position)
    OriginPreset origin_preset = ORIGIN_USER_POSITION; // Default: user position
    Explosion3D* explosion_effect = dynamic_cast<Explosion3D*>(current_effect_ui);
    if(explosion_effect)
    {
        origin_preset = explosion_effect->GetOriginPreset();
    }

    float origin_offset_x, origin_offset_y, origin_offset_z;
    int max_grid_size = std::max({custom_grid_x, custom_grid_y, custom_grid_z});
    CalculateOriginOffset(origin_preset, user_position, max_grid_size, origin_offset_x, origin_offset_y, origin_offset_z);

    // Now map each controller's LEDs to the unified grid and apply effects
    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        ControllerTransform* transform = controller_transforms[ctrl_idx];
        if(!transform)
        {
            continue;
        }

        // Handle virtual controllers
        if(transform->virtual_controller && !transform->controller)
        {
            VirtualController3D* virtual_ctrl = transform->virtual_controller;
            const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();

            // Apply effects to each virtual LED
            for(unsigned int mapping_idx = 0; mapping_idx < mappings.size(); mapping_idx++)
            {
                const GridLEDMapping& mapping = mappings[mapping_idx];
                if(!mapping.controller) continue;

                // Calculate virtual LED world position in unified grid space using proper transformation
                Vector3D local_pos;
                local_pos.x = (float)mapping.x;
                local_pos.y = (float)mapping.y;
                local_pos.z = (float)mapping.z;
                Vector3D world_pos = ControllerLayout3D::CalculateWorldPosition(local_pos, transform->transform);
                float x = world_pos.x;
                float y = world_pos.y;
                float z = world_pos.z;

                // Adjust coordinates relative to the effect origin (user position)
                float relative_x = x - origin_offset_x;
                float relative_y = y - origin_offset_y;
                float relative_z = z - origin_offset_z;

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

                    // Calculate effect color using grid-aware method
                    RGBColor color = current_effect_ui->CalculateColorGrid(relative_x, relative_y, relative_z, effect_time, grid_context);

                    // Apply color to the mapped physical LED (with bounds checking)
                    if(mapping.zone_idx < mapping.controller->zones.size())
                    {
                        unsigned int led_global_idx = mapping.controller->zones[mapping.zone_idx].start_idx + mapping.led_idx;
                        if(led_global_idx < mapping.controller->colors.size())
                        {
                            mapping.controller->colors[led_global_idx] = color;
                        }
                        else
                        {
                        }
                    }
                    else
                    {
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
        | Calculate colors for each LED using actual positions    |
        \*---------------------------------------------------------*/
        for(unsigned int led_pos_idx = 0; led_pos_idx < transform->led_positions.size(); led_pos_idx++)
        {
            LEDPosition3D& led_position = transform->led_positions[led_pos_idx];

            /*---------------------------------------------------------*\
            | Get LED 3D position from the actual layout              |
            \*---------------------------------------------------------*/
            // Calculate LED world position in unified grid space using proper transformation
            Vector3D world_pos = ControllerLayout3D::CalculateWorldPosition(led_position.local_position, transform->transform);
            float x = world_pos.x;
            float y = world_pos.y;
            float z = world_pos.z;

            // Validate zone index before accessing
            if(led_position.zone_idx >= controller->zones.size())
            {
                continue; // Skip invalid zone
            }

            // Get the actual LED index for color updates
            unsigned int led_global_idx = controller->zones[led_position.zone_idx].start_idx + led_position.led_idx;

            // Adjust coordinates relative to the effect origin (user position)
            float relative_x = x - origin_offset_x;
            float relative_y = y - origin_offset_y;
            float relative_z = z - origin_offset_z;

            // Only apply effects to LEDs within the room-centered grid bounds
            if(x >= grid_min_x && x <= grid_max_x &&
               y >= grid_min_y && y <= grid_max_y &&
               z >= grid_min_z && z <= grid_max_z)
            {
                /*---------------------------------------------------------*\
                | Calculate effect color using grid-aware method          |
                \*---------------------------------------------------------*/
                RGBColor color = current_effect_ui->CalculateColorGrid(relative_x, relative_y, relative_z, effect_time, grid_context);

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

        VirtualController3D* virtual_ctrl = virtual_controllers[item_row];


        ControllerTransform* ctrl_transform = new ControllerTransform();
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

        int hue = (controller_transforms.size() * 137) % 360;
        QColor color = QColor::fromHsv(hue, 200, 255);
        ctrl_transform->display_color = (color.blue() << 16) | (color.green() << 8) | color.red();

        controller_transforms.push_back(ctrl_transform);

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

    ControllerTransform* ctrl_transform = new ControllerTransform();
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
            delete ctrl_transform;
            return;
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
            delete ctrl_transform;
            return;
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

    controller_transforms.push_back(ctrl_transform);

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

    delete controller_transforms[selected_row];
    controller_transforms.erase(controller_transforms.begin() + selected_row);

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

    delete controller_transforms[index];
    controller_transforms.erase(controller_transforms.begin() + index);

    controller_list->takeItem(index);

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::on_clear_all_clicked()
{
    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        delete controller_transforms[i];
    }
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

    ControllerTransform* ctrl = controller_transforms[selected_row];

    // Update LED spacing values
    ctrl->led_spacing_mm_x = edit_led_spacing_x_spin ? (float)edit_led_spacing_x_spin->value() : 10.0f;
    ctrl->led_spacing_mm_y = edit_led_spacing_y_spin ? (float)edit_led_spacing_y_spin->value() : 0.0f;
    ctrl->led_spacing_mm_z = edit_led_spacing_z_spin ? (float)edit_led_spacing_z_spin->value() : 0.0f;

    // Regenerate LED positions with new spacing
    RegenerateLEDPositions(ctrl);

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

    if(user_pos_x_spin) user_position.x = (float)user_pos_x_spin->value();
    if(user_pos_y_spin) user_position.y = (float)user_pos_y_spin->value();
    if(user_pos_z_spin) user_position.z = (float)user_pos_z_spin->value();
    if(user_visible_checkbox) user_position.visible = user_visible_checkbox->isChecked();

    bool ok;
    QString profile_name = QInputDialog::getText(this, "Save Layout Profile",
                                                 "Profile name:", QLineEdit::Normal,
                                                 layout_profiles_combo->currentText(), &ok);

    if(!ok || profile_name.isEmpty())
    {
        return;
    }

    std::string layout_path = GetLayoutPath(profile_name.toStdString());
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
        VirtualController3D* virtual_ctrl = new VirtualController3D(
            dialog.GetControllerName().toStdString(),
            dialog.GetGridWidth(),
            dialog.GetGridHeight(),
            dialog.GetGridDepth(),
            dialog.GetLEDMappings(),
            dialog.GetSpacingX(),
            dialog.GetSpacingY(),
            dialog.GetSpacingZ()
        );

        virtual_controllers.push_back(virtual_ctrl);


        available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName()));

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

    VirtualController3D* ctrl = virtual_controllers[list_row];

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
                                .arg(selected).arg(filename));
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
        VirtualController3D* virtual_ctrl = VirtualController3D::FromJson(import_data, controllers);

        if(virtual_ctrl)
        {
            for(unsigned int i = 0; i < virtual_controllers.size(); i++)
            {
                if(virtual_controllers[i]->GetName() == virtual_ctrl->GetName())
                {
                    QMessageBox::StandardButton reply = QMessageBox::question(this, "Duplicate Name",
                        QString("A custom controller named '%1' already exists.\n\nDo you want to replace it?")
                        .arg(QString::fromStdString(virtual_ctrl->GetName())),
                        QMessageBox::Yes | QMessageBox::No);

                    if(reply == QMessageBox::No)
                    {
                        delete virtual_ctrl;
                        return;
                    }
                    else
                    {
                        for(unsigned int j = 0; j < virtual_controllers.size(); j++)
                        {
                            if(virtual_controllers[j]->GetName() == virtual_ctrl->GetName())
                            {
                                delete virtual_controllers[j];
                                virtual_controllers.erase(virtual_controllers.begin() + j);
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            virtual_controllers.push_back(virtual_ctrl);
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

    VirtualController3D* virtual_ctrl = virtual_controllers[list_row];

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
            std::string custom_dir = config_dir + "/plugins/3d_spatial_custom_controllers";

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

        delete virtual_ctrl;
        virtual_controllers[virtual_idx] = new VirtualController3D(
            new_name,
            dialog.GetGridWidth(),
            dialog.GetGridHeight(),
            dialog.GetGridDepth(),
            dialog.GetLEDMappings(),
            dialog.GetSpacingX(),
            dialog.GetSpacingY(),
            dialog.GetSpacingZ()
        );

        SaveCustomControllers();
        UpdateAvailableControllersList();


        QMessageBox::information(this, "Custom Controller Updated",
                                QString("Custom controller '%1' updated successfully!")
                                .arg(QString::fromStdString(virtual_controllers[virtual_idx]->GetName())));
    }
}

void OpenRGB3DSpatialTab::SaveLayout(const std::string& filename)
{

    nlohmann::json layout_json;

    /*---------------------------------------------------------*\
    | Header Information                                       |
    \*---------------------------------------------------------*/
    layout_json["format"] = "OpenRGB3DSpatialLayout";
    layout_json["version"] = 5;

    /*---------------------------------------------------------*\
    | Grid Settings                                            |
    \*---------------------------------------------------------*/
    layout_json["grid"]["dimensions"]["x"] = custom_grid_x;
    layout_json["grid"]["dimensions"]["y"] = custom_grid_y;
    layout_json["grid"]["dimensions"]["z"] = custom_grid_z;
    layout_json["grid"]["snap_enabled"] = (viewport && viewport->IsGridSnapEnabled());
    layout_json["grid"]["scale_mm"] = grid_scale_mm;

    /*---------------------------------------------------------*\
    | User Position                                            |
    \*---------------------------------------------------------*/
    layout_json["user_position"]["x"] = user_position.x;
    layout_json["user_position"]["y"] = user_position.y;
    layout_json["user_position"]["z"] = user_position.z;
    layout_json["user_position"]["visible"] = user_position.visible;

    /*---------------------------------------------------------*\
    | Controllers                                              |
    \*---------------------------------------------------------*/
    layout_json["controllers"] = nlohmann::json::array();

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ct = controller_transforms[i];
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
    | Write JSON to file                                       |
    \*---------------------------------------------------------*/
    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream out(&file);
    out << QString::fromStdString(layout_json.dump(4));
    file.close();

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
    | Load User Position                                       |
    \*---------------------------------------------------------*/
    if(layout_json.contains("user_position"))
    {
        user_position.x = layout_json["user_position"]["x"].get<float>();
        user_position.y = layout_json["user_position"]["y"].get<float>();
        user_position.z = layout_json["user_position"]["z"].get<float>();
        user_position.visible = layout_json["user_position"]["visible"].get<bool>();

        if(user_pos_x_spin)
        {
            user_pos_x_spin->blockSignals(true);
            user_pos_x_spin->setValue(user_position.x);
            user_pos_x_spin->blockSignals(false);
        }
        if(user_pos_y_spin)
        {
            user_pos_y_spin->blockSignals(true);
            user_pos_y_spin->setValue(user_position.y);
            user_pos_y_spin->blockSignals(false);
        }
        if(user_pos_z_spin)
        {
            user_pos_z_spin->blockSignals(true);
            user_pos_z_spin->setValue(user_position.z);
            user_pos_z_spin->blockSignals(false);
        }
        if(user_visible_checkbox)
        {
            user_visible_checkbox->blockSignals(true);
            user_visible_checkbox->setChecked(user_position.visible);
            user_visible_checkbox->blockSignals(false);
        }

        if(viewport) viewport->SetUserPosition(user_position);
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
        for(const auto& controller_json : layout_json["controllers"])
        {
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

            ControllerTransform* ctrl_transform = new ControllerTransform();
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
                        virtual_ctrl = virtual_controllers[i];
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
                    delete ctrl_transform;
                    continue;
                }
            }
            else
            {
                // Load LED mappings for physical controllers
                for(const auto& led_mapping : controller_json["led_mappings"])
                {
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
                            LOG_WARNING("[OpenRGB3DSpatialPlugin] Correcting granularity for '%s': saved as %s but has all %d LEDs (changing to Whole Device)",
                                        controller->name.c_str(),
                                        (original_granularity == 1 ? "Zone" : (original_granularity == 2 ? "LED" : "Unknown")),
                                        (int)ctrl_transform->led_positions.size());
                            ctrl_transform->granularity = 0;
                            ctrl_transform->item_idx = 0;
                        }
                    }
                    else if(ctrl_transform->led_positions.size() == 1)
                    {
                        // Single LED - granularity should be 2
                        if(ctrl_transform->granularity != 2)
                        {
                            LOG_WARNING("[OpenRGB3DSpatialPlugin] Correcting granularity for '%s': saved as %s but has only 1 LED (changing to LED level)",
                                        controller->name.c_str(),
                                        (original_granularity == 0 ? "Whole Device" : (original_granularity == 1 ? "Zone" : "Unknown")));
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
                                LOG_WARNING("[OpenRGB3DSpatialPlugin] Correcting granularity for '%s': saved as %s but has %d LEDs from zone %d (changing to Zone level)",
                                            controller->name.c_str(),
                                            (original_granularity == 0 ? "Whole Device" : (original_granularity == 2 ? "LED" : "Unknown")),
                                            (int)ctrl_transform->led_positions.size(),
                                            first_zone);
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

                    // Log successful validation
                    if(ctrl_transform->granularity == original_granularity && ctrl_transform->item_idx == original_item_idx)
                    {
                        LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Granularity validated OK for '%s': %s with %d LEDs",
                                    controller->name.c_str(),
                                    (ctrl_transform->granularity == 0 ? "Whole Device" : (ctrl_transform->granularity == 1 ? "Zone" : "LED")),
                                    (int)ctrl_transform->led_positions.size());
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

            controller_transforms.push_back(ctrl_transform);

            QColor color;
            color.setRgb(ctrl_transform->display_color & 0xFF,
                         (ctrl_transform->display_color >> 8) & 0xFF,
                         (ctrl_transform->display_color >> 16) & 0xFF);

            QString name;
            if(is_virtual)
            {
                name = QString::fromStdString(ctrl_name);
            }
            else
            {
                // Use granularity info to create proper name with prefix
                if(ctrl_transform->granularity == 0)
                {
                    name = QString("[Device] ") + QString::fromStdString(controller->name);
                }
                else if(ctrl_transform->granularity == 1)
                {
                    name = QString("[Zone] ") + QString::fromStdString(controller->name);
                    if(ctrl_transform->item_idx >= 0 && ctrl_transform->item_idx < (int)controller->zones.size())
                    {
                        name += " - " + QString::fromStdString(controller->zones[ctrl_transform->item_idx].name);
                    }
                }
                else if(ctrl_transform->granularity == 2)
                {
                    name = QString("[LED] ") + QString::fromStdString(controller->name);
                    if(ctrl_transform->item_idx >= 0 && ctrl_transform->item_idx < (int)controller->leds.size())
                    {
                        name += " - " + QString::fromStdString(controller->leds[ctrl_transform->item_idx].name);
                    }
                }
                else
                {
                    // Fallback for old files without granularity
                    name = QString::fromStdString(controller->name);
                    if(ctrl_transform->led_positions.size() < controller->leds.size())
                    {
                        if(ctrl_transform->led_positions.size() == 1)
                        {
                            unsigned int led_global_idx = controller->zones[ctrl_transform->led_positions[0].zone_idx].start_idx +
                                                          ctrl_transform->led_positions[0].led_idx;
                            name = QString("[LED] ") + name + " - " + QString::fromStdString(controller->leds[led_global_idx].name);
                        }
                        else
                        {
                            name = QString("[Zone] ") + name + " - " + QString::fromStdString(controller->zones[ctrl_transform->led_positions[0].zone_idx].name);
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

    viewport->SetControllerTransforms(&controller_transforms);
    viewport->update();
    UpdateAvailableControllersList();
    UpdateAvailableItemCombo();
}

void OpenRGB3DSpatialTab::LoadLayout(const std::string& filename)
{

    QFile file(QString::fromStdString(filename));

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    try
    {
        nlohmann::json layout_json = nlohmann::json::parse(content.toStdString());
        LoadLayoutFromJSON(layout_json);
        return;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to parse JSON: %s", e.what());
        QMessageBox::critical(nullptr, "Invalid Layout File",
                            QString("Failed to load layout file:\n%1\n\nError: %2")
                            .arg(QString::fromStdString(filename))
                            .arg(e.what()));
        return;
    }
}

std::string OpenRGB3DSpatialTab::GetLayoutPath(const std::string& layout_name)
{
    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path plugins_dir = config_dir / "plugins" / "3d_spatial_layouts";

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
    filesystem::path layouts_dir = config_dir / "plugins" / "3d_spatial_layouts";


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
    std::string profile_name = layout_profiles_combo->currentText().toStdString();

    nlohmann::json settings = resource_manager->GetSettingsManager()->GetSettings("3DSpatialPlugin");
    settings["SelectedProfile"] = profile_name;
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

    if(auto_load_checkbox->isChecked())
    {
        QString profile_name = layout_profiles_combo->currentText();

        if(!profile_name.isEmpty())
        {
            std::string layout_path = GetLayoutPath(profile_name.toStdString());
            QFileInfo check_file(QString::fromStdString(layout_path));

            if(check_file.exists())
            {
                LoadLayout(layout_path);
            }
        }
    }
}

void OpenRGB3DSpatialTab::SaveCustomControllers()
{
    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string custom_dir = config_dir + "/plugins/3d_spatial_custom_controllers";

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
        }
        else
        {
        }
    }
}

void OpenRGB3DSpatialTab::LoadCustomControllers()
{
    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string custom_dir = config_dir + "/plugins/3d_spatial_custom_controllers";

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

                        VirtualController3D* virtual_ctrl = VirtualController3D::FromJson(ctrl_json, controllers);
                        if(virtual_ctrl)
                        {
                            virtual_controllers.push_back(virtual_ctrl);
                            available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(virtual_ctrl->GetName()));
                            loaded_count++;
                        }
                    }
                    catch(const std::exception&)
                    {
                    }
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
        ControllerTransform* ct = controller_transforms[i];
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
    if(!effect_controls_widget)
    {
        return;
    }

    /*---------------------------------------------------------*\
    | Create appropriate effect UI based on type              |
    \*---------------------------------------------------------*/
    if(effect_type == 0)  // Wave effect
    {
        /*---------------------------------------------------------*\
        | Create Wave3D effect UI                                  |
        \*---------------------------------------------------------*/
        wave3d_effect = new Wave3D(effect_controls_widget);
        wave3d_effect->CreateCommonEffectControls(effect_controls_widget);
        wave3d_effect->SetupCustomUI(effect_controls_widget);
        current_effect_ui = wave3d_effect;

        /*---------------------------------------------------------*\
        | Get buttons from effect and store references            |
        \*---------------------------------------------------------*/
        start_effect_button = wave3d_effect->GetStartButton();
        stop_effect_button = wave3d_effect->GetStopButton();

        /*---------------------------------------------------------*\
        | Connect start/stop buttons to tab handlers              |
        \*---------------------------------------------------------*/
        connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
        connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);

        /*---------------------------------------------------------*\
        | Connect parameter change signals                         |
        \*---------------------------------------------------------*/
        connect(wave3d_effect, &SpatialEffect3D::ParametersChanged, [this]() {
        });

    }
    else if(effect_type == 1)  // Wipe effect
    {
        /*---------------------------------------------------------*\
        | Create Wipe3D effect UI                                  |
        \*---------------------------------------------------------*/
        wipe3d_effect = new Wipe3D(effect_controls_widget);
        wipe3d_effect->CreateCommonEffectControls(effect_controls_widget);
        wipe3d_effect->SetupCustomUI(effect_controls_widget);
        current_effect_ui = wipe3d_effect;

        start_effect_button = wipe3d_effect->GetStartButton();
        stop_effect_button = wipe3d_effect->GetStopButton();

        connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
        connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
        connect(wipe3d_effect, &SpatialEffect3D::ParametersChanged, [this]() {
        });

    }
    else if(effect_type == 2)  // Plasma effect
    {
        /*---------------------------------------------------------*\
        | Create Plasma3D effect UI                               |
        \*---------------------------------------------------------*/
        plasma3d_effect = new Plasma3D(effect_controls_widget);
        plasma3d_effect->CreateCommonEffectControls(effect_controls_widget);
        plasma3d_effect->SetupCustomUI(effect_controls_widget);
        current_effect_ui = plasma3d_effect;

        start_effect_button = plasma3d_effect->GetStartButton();
        stop_effect_button = plasma3d_effect->GetStopButton();

        connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
        connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
        connect(plasma3d_effect, &SpatialEffect3D::ParametersChanged, [this]() {
        });

    }
    else if(effect_type == 3)  // Spiral
    {
        spiral3d_effect = new Spiral3D(effect_controls_widget);
        spiral3d_effect->CreateCommonEffectControls(effect_controls_widget);
        spiral3d_effect->SetupCustomUI(effect_controls_widget);
        current_effect_ui = spiral3d_effect;

        start_effect_button = spiral3d_effect->GetStartButton();
        stop_effect_button = spiral3d_effect->GetStopButton();

        connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
        connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
        SetupEffectSignals(spiral3d_effect, effect_type);
    }
    else if(effect_type == 4)  // Spin
    {
        spin3d_effect = new Spin3D(effect_controls_widget);
        spin3d_effect->CreateCommonEffectControls(effect_controls_widget);
        spin3d_effect->SetupCustomUI(effect_controls_widget);
        current_effect_ui = spin3d_effect;

        start_effect_button = spin3d_effect->GetStartButton();
        stop_effect_button = spin3d_effect->GetStopButton();

        connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
        connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
        SetupEffectSignals(spin3d_effect, effect_type);
    }
    else if(effect_type == 5)  // DNA Helix
    {
        dnahelix3d_effect = new DNAHelix3D(effect_controls_widget);
        dnahelix3d_effect->CreateCommonEffectControls(effect_controls_widget);
        dnahelix3d_effect->SetupCustomUI(effect_controls_widget);
        current_effect_ui = dnahelix3d_effect;

        start_effect_button = dnahelix3d_effect->GetStartButton();
        stop_effect_button = dnahelix3d_effect->GetStopButton();

        connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
        connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
        SetupEffectSignals(dnahelix3d_effect, effect_type);
    }
    else if(effect_type == 6)  // Breathing Sphere
    {
        breathingsphere3d_effect = new BreathingSphere3D(effect_controls_widget);
        breathingsphere3d_effect->CreateCommonEffectControls(effect_controls_widget);
        breathingsphere3d_effect->SetupCustomUI(effect_controls_widget);
        current_effect_ui = breathingsphere3d_effect;

        start_effect_button = breathingsphere3d_effect->GetStartButton();
        stop_effect_button = breathingsphere3d_effect->GetStopButton();

        connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
        connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
        SetupEffectSignals(breathingsphere3d_effect, effect_type);
    }
    else if(effect_type == 7)  // Explosion
    {
        explosion3d_effect = new Explosion3D(effect_controls_widget);
        explosion3d_effect->CreateCommonEffectControls(effect_controls_widget);
        explosion3d_effect->SetupCustomUI(effect_controls_widget);
        current_effect_ui = explosion3d_effect;

        start_effect_button = explosion3d_effect->GetStartButton();
        stop_effect_button = explosion3d_effect->GetStopButton();

        connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
        connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
        SetupEffectSignals(explosion3d_effect, effect_type);
    }
    else
    {
        QLabel* unknown_label = new QLabel("Unknown effect type.");
        unknown_label->setAlignment(Qt::AlignCenter);
        unknown_label->setStyleSheet("color: #666; font-style: italic; padding: 20px;");
        effect_controls_layout->addWidget(unknown_label);
    }

    // Update the layout to show the new controls
    if(effect_controls_widget && effect_controls_layout)
    {
        effect_controls_widget->updateGeometry();
        effect_controls_widget->update();
    }
}

/*---------------------------------------------------------*\
| NEW: Simplified effect creation using auto-registration |
| This replaces the 100+ lines of if/else above          |
\*---------------------------------------------------------*/
void OpenRGB3DSpatialTab::SetupCustomEffectUI_NEW(const std::string& effect_class_name)
{
    if(!effect_controls_widget)
    {
        return;
    }

    // Create effect using the manager
    SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(effect_class_name);
    if(!effect)
    {
        QLabel* error_label = new QLabel(QString("Failed to create effect: %1").arg(QString::fromStdString(effect_class_name)));
        error_label->setAlignment(Qt::AlignCenter);
        error_label->setStyleSheet("color: red; font-style: italic; padding: 20px;");
        effect_controls_layout->addWidget(error_label);
        return;
    }

    // Setup effect UI (same 3 lines for ALL effects!)
    effect->setParent(effect_controls_widget);
    effect->CreateCommonEffectControls(effect_controls_widget);
    effect->SetupCustomUI(effect_controls_widget);
    current_effect_ui = effect;

    // Get and connect buttons
    start_effect_button = effect->GetStartButton();
    stop_effect_button = effect->GetStopButton();
    connect(start_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
    connect(stop_effect_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
    connect(effect, &SpatialEffect3D::ParametersChanged, [this]() {});

    // Update layout
    if(effect_controls_widget && effect_controls_layout)
    {
        effect_controls_widget->updateGeometry();
        effect_controls_widget->update();
    }
}

void OpenRGB3DSpatialTab::SetupEffectSignals(SpatialEffect3D* effect, int effect_type)
{
    /*---------------------------------------------------------*\
    | Connect parameter change signals                         |
    \*---------------------------------------------------------*/
    connect(effect, &SpatialEffect3D::ParametersChanged, [this, effect_type]() {
    });

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
    wave3d_effect = nullptr;
    plasma3d_effect = nullptr;
    spiral3d_effect = nullptr;
    spin3d_effect = nullptr;
    explosion3d_effect = nullptr;
    breathingsphere3d_effect = nullptr;
    dnahelix3d_effect = nullptr;
    wipe3d_effect = nullptr;
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
        RegenerateLEDPositions(controller_transforms[i]);
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
void OpenRGB3DSpatialTab::on_user_position_changed()
{
    // Update user position from spin boxes
    user_position.x = (float)user_pos_x_spin->value();
    user_position.y = (float)user_pos_y_spin->value();
    user_position.z = (float)user_pos_z_spin->value();

    // Update viewport display
    if(viewport)
    {
        viewport->SetUserPosition(user_position);
        viewport->update();
    }

    // Update effect reference point if using user position
    if(current_effect_ui && use_user_reference_checkbox && use_user_reference_checkbox->isChecked())
    {
        Vector3D user_pos = {user_position.x, user_position.y, user_position.z};
        current_effect_ui->SetGlobalReferencePoint(user_pos);
    }
}

void OpenRGB3DSpatialTab::on_user_visibility_toggled(bool visible)
{
    user_position.visible = visible;

    // Update viewport display
    if(viewport)
    {
        viewport->SetUserPosition(user_position);
        viewport->update();
    }
}

void OpenRGB3DSpatialTab::on_user_center_clicked()
{
    // Reset user to room center (0,0,0)
    user_position.x = 0.0f;
    user_position.y = 0.0f;
    user_position.z = 0.0f;

    // Update spin boxes
    user_pos_x_spin->setValue(0.0);
    user_pos_y_spin->setValue(0.0);
    user_pos_z_spin->setValue(0.0);

    // Update viewport display
    if(viewport)
    {
        viewport->SetUserPosition(user_position);
        viewport->update();
    }
}

void OpenRGB3DSpatialTab::on_use_user_reference_toggled(bool enabled)
{
    if(current_effect_ui)
    {
        if(enabled)
        {
            current_effect_ui->SetReferenceMode(REF_MODE_USER_POSITION);
            Vector3D user_pos = {user_position.x, user_position.y, user_position.z};
            current_effect_ui->SetGlobalReferencePoint(user_pos);
        }
        else
        {
            current_effect_ui->SetReferenceMode(REF_MODE_ROOM_CENTER);
            current_effect_ui->SetGlobalReferencePoint({0.0f, 0.0f, 0.0f});
        }
    }
}

void OpenRGB3DSpatialTab::on_effect_changed(int index)
{
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
        SetupCustomEffectUI(index - 1);  // Adjust for "None" offset
    }
}


