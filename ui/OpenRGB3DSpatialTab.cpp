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
#ifndef NOMINMAX
#define NOMINMAX
#endif
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

    SetupUI();
    LoadDevices();
    LoadCustomControllers();

    /*---------------------------------------------------------*\
    | Initialize zone and effect combos                        |
    \*---------------------------------------------------------*/
    UpdateEffectZoneCombo();     // Initialize zone dropdown with "All Controllers"
    UpdateEffectOriginCombo();   // Initialize reference point dropdown

    auto_load_timer = new QTimer(this);
    auto_load_timer->setSingleShot(true);
    connect(auto_load_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::TryAutoLoadLayout);
    auto_load_timer->start(2000);


    effect_timer = new QTimer(this);
    connect(effect_timer, &QTimer::timeout, this, &OpenRGB3DSpatialTab::on_effect_timer_timeout);
    effect_running = false;
    effect_time = 0.0f;

    worker_thread = new EffectWorkerThread3D(this);
    connect(worker_thread, &EffectWorkerThread3D::ColorsReady, this, &OpenRGB3DSpatialTab::ApplyColorsFromWorker);
}

OpenRGB3DSpatialTab::~OpenRGB3DSpatialTab()
{
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
    ref_point_color_button->setStyleSheet(QString("background-color: #%1").arg(selected_ref_point_color & 0xFFFFFF, 6, 16, QChar('0')));
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
    middle_panel->addWidget(controls_label);

    viewport = new LEDViewport3D();
    viewport->SetControllerTransforms(&controller_transforms);
    viewport->SetGridDimensions(custom_grid_x, custom_grid_y, custom_grid_z);
    viewport->SetGridSnapEnabled(false);
    viewport->SetReferencePoints(&reference_points);

    connect(viewport, SIGNAL(ControllerSelected(int)), this, SLOT(on_controller_selected(int)));
    connect(viewport, SIGNAL(ControllerPositionChanged(int,float,float,float)),
            this, SLOT(on_controller_position_changed(int,float,float,float)));
    connect(viewport, SIGNAL(ControllerRotationChanged(int,float,float,float)),
            this, SLOT(on_controller_rotation_changed(int,float,float,float)));
    connect(viewport, SIGNAL(ControllerDeleteRequested(int)), this, SLOT(on_remove_controller_from_viewport(int)));
    connect(viewport, SIGNAL(ReferencePointSelected(int)), this, SLOT(on_ref_point_selected(int)));
    connect(viewport, SIGNAL(ReferencePointPositionChanged(int,float,float,float)),
            this, SLOT(on_ref_point_position_changed(int,float,float,float)));
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

    // Grid Snap Checkbox
    grid_snap_checkbox = new QCheckBox("Grid Snapping");
    grid_snap_checkbox->setToolTip("Snap controller positions to grid intersections");
    layout_layout->addWidget(grid_snap_checkbox, 1, 3, 1, 3);

    // Selection Info Label
    selection_info_label = new QLabel("No selection");
    selection_info_label->setStyleSheet("color: gray; font-size: 10px; font-weight: bold;");
    selection_info_label->setAlignment(Qt::AlignRight);
    layout_layout->addWidget(selection_info_label, 1, 3, 1, 3);

    // Add helpful labels with text wrapping
    QLabel* grid_help1 = new QLabel("LEDs mapped sequentially to grid positions (X, Y, Z)");
    grid_help1->setStyleSheet("color: gray; font-size: 10px;");
    grid_help1->setWordWrap(true);
    layout_layout->addWidget(grid_help1, 2, 0, 1, 6);

    QLabel* grid_help2 = new QLabel("Use Ctrl+Click for multi-select • Add User position in Reference Points tab");
    grid_help2->setStyleSheet("color: gray; font-size: 10px;");
    grid_help2->setWordWrap(true);
    layout_layout->addWidget(grid_help2, 3, 0, 1, 6);

    grid_settings_tab->setLayout(layout_layout);

    // Connect signals
    connect(grid_x_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_y_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_z_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_grid_dimensions_changed);
    connect(grid_snap_checkbox, &QCheckBox::toggled, this, &OpenRGB3DSpatialTab::on_grid_snap_toggled);

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

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.position.x = pos_value;
            viewport->NotifyControllerTransformChanged();
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
    pos_x_spin->setRange(-100, 100);
    pos_x_spin->setDecimals(1);
    pos_x_spin->setMaximumWidth(80);
    connect(pos_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        pos_x_slider->setValue((int)(value * 10));

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.position.x = value;
            viewport->NotifyControllerTransformChanged();
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
    pos_y_slider->setRange(0, 1000);  // Start from 0 (floor level)
    pos_y_slider->setValue(0);
    connect(pos_y_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        pos_y_spin->setValue(pos_value);

        // Check if a controller is selected first (higher priority, with floor constraint)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            if(pos_value < 0.0f) {
                pos_value = 0.0f;
                pos_y_slider->setValue((int)(pos_value * 10));
            }
            controller_transforms[ctrl_row]->transform.position.y = pos_value;
            // Enforce floor constraint after position change
            viewport->EnforceFloorConstraint(controller_transforms[ctrl_row].get());
            viewport->NotifyControllerTransformChanged();
            return;
        }

        // Otherwise check if a reference point is selected (no floor constraint for ref points)
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
    pos_y_spin->setRange(0, 100);  // Floor constraint
    pos_y_spin->setDecimals(1);
    pos_y_spin->setMaximumWidth(80);
    connect(pos_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        pos_y_slider->setValue((int)(value * 10));

        // Check if a controller is selected first (higher priority, with floor constraint)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            if(value < 0.0) {
                value = 0.0;
                pos_y_spin->setValue(value);
            }
            controller_transforms[ctrl_row]->transform.position.y = value;
            // Enforce floor constraint after position change
            viewport->EnforceFloorConstraint(controller_transforms[ctrl_row].get());
            viewport->NotifyControllerTransformChanged();
            return;
        }

        // Otherwise check if a reference point is selected (no floor constraint for ref points)
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
    pos_z_slider->setRange(-1000, 1000);
    pos_z_slider->setValue(0);
    connect(pos_z_slider, &QSlider::valueChanged, [this](int value) {
        double pos_value = value / 10.0;
        pos_z_spin->setValue(pos_value);

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.position.z = pos_value;
            viewport->NotifyControllerTransformChanged();
            return;
        }

        // Otherwise check if a reference point is selected
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
    pos_z_spin->setRange(-100, 100);
    pos_z_spin->setDecimals(1);
    pos_z_spin->setMaximumWidth(80);
    connect(pos_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        pos_z_slider->setValue((int)(value * 10));

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.position.z = value;
            viewport->NotifyControllerTransformChanged();
            return;
        }

        // Otherwise check if a reference point is selected
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
        rot_x_spin->setValue(rot_value);

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.x = rot_value;
            viewport->NotifyControllerTransformChanged();
            return;
        }

        // Otherwise check if a reference point is selected
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
        rot_x_slider->setValue((int)value);

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.x = value;
            viewport->NotifyControllerTransformChanged();
            return;
        }

        // Otherwise check if a reference point is selected
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
        rot_y_spin->setValue(rot_value);

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.y = rot_value;
            viewport->NotifyControllerTransformChanged();
            return;
        }

        // Otherwise check if a reference point is selected
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
        rot_y_slider->setValue((int)value);

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.y = value;
            viewport->NotifyControllerTransformChanged();
            return;
        }

        // Otherwise check if a reference point is selected
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
        rot_z_spin->setValue(rot_value);

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.z = rot_value;
            viewport->NotifyControllerTransformChanged();
            return;
        }

        // Otherwise check if a reference point is selected
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
        rot_z_slider->setValue((int)value);

        // Check if a controller is selected first (higher priority)
        int ctrl_row = controller_list->currentRow();
        if(ctrl_row >= 0 && ctrl_row < (int)controller_transforms.size())
        {
            controller_transforms[ctrl_row]->transform.rotation.z = value;
            viewport->NotifyControllerTransformChanged();
            return;
        }

        // Otherwise check if a reference point is selected
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

    QVBoxLayout* right_panel = new QVBoxLayout();

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
    LOG_WARNING("[OpenRGB3DSpatialPlugin] Connected effect_combo signal to on_effect_changed slot");

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
    effect_origin_combo->addItem("Room Center (0,0,0)", QVariant(-1)); // -1 = no reference point
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
}

void OpenRGB3DSpatialTab::on_controller_position_changed(int index, float x, float y, float z)
{
    if(index >= 0 && index < (int)controller_transforms.size())
    {
        ControllerTransform* ctrl = controller_transforms[index].get();
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
        ControllerTransform* ctrl = controller_transforms[index].get();
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
                LOG_WARNING("[OpenRGB3DSpatialPlugin] Loading %u effects from preset '%s'",
                         (unsigned int)preset->effect_instances.size(), preset->name.c_str());

                for(unsigned int i = 0; i < preset->effect_instances.size(); i++)
                {
                    nlohmann::json instance_json = preset->effect_instances[i]->ToJson();
                    std::unique_ptr<EffectInstance3D> copied_instance = EffectInstance3D::FromJson(instance_json);
                    if(copied_instance)
                    {
                        LOG_WARNING("[OpenRGB3DSpatialPlugin] Added effect to stack: enabled=%d, has_effect=%d, zone_index=%d",
                                 copied_instance->enabled, copied_instance->effect.get() != nullptr, copied_instance->zone_index);
                        effect_stack.push_back(std::move(copied_instance));
                    }
                    else
                    {
                        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to create effect instance from JSON");
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

                if(!has_valid_controller)
                {
                    LOG_WARNING("[OpenRGB3DSpatialPlugin] No valid controllers found for stack preset");
                }
                else
                {
                    LOG_WARNING("[OpenRGB3DSpatialPlugin] Set controllers to Direct mode for stack preset");
                }

                /*---------------------------------------------------------*\
                | Start effect timer                                       |
                \*---------------------------------------------------------*/
                if(effect_timer && !effect_timer->isActive())
                {
                    effect_timer->start(33);
                    LOG_WARNING("[OpenRGB3DSpatialPlugin] Started effect timer for stack preset");
                }
                else
                {
                    LOG_WARNING("[OpenRGB3DSpatialPlugin] Effect timer already running or null");
                }

                /*---------------------------------------------------------*\
                | Update button states                                     |
                \*---------------------------------------------------------*/
                start_effect_button->setEnabled(false);
                stop_effect_button->setEnabled(true);

                LOG_WARNING("[OpenRGB3DSpatialPlugin] Started stack preset: %s", preset->name.c_str());
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

            LOG_INFO("[OpenRGB3DSpatialPlugin] Stopped stack preset");
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
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Rendering effect stack (%u effects)", (unsigned int)effect_stack.size());
        RenderEffectStack();
        return;
    }
    else
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Not rendering stack - has_stack_effects=false, stack size=%u", (unsigned int)effect_stack.size());
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

    /*---------------------------------------------------------*\
    | Get effect origin from the selected reference point     |
    \*---------------------------------------------------------*/
    float origin_offset_x = 0.0f;
    float origin_offset_y = 0.0f;
    float origin_offset_z = 0.0f;

    if(effect_origin_combo)
    {
        // Get the selected reference point index (-1 means room center)
        int index = effect_origin_combo->currentIndex();
        int ref_point_idx = effect_origin_combo->itemData(index).toInt();

        if(ref_point_idx >= 0 && ref_point_idx < (int)reference_points.size())
        {
            // Use selected reference point position
            VirtualReferencePoint3D* ref_point = reference_points[ref_point_idx].get();
            Vector3D origin = ref_point->GetPosition();
            origin_offset_x = origin.x;
            origin_offset_y = origin.y;
            origin_offset_z = origin.z;
            LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Using reference point origin: %.1f, %.1f, %.1f", origin_offset_x, origin_offset_y, origin_offset_z);
        }
        else
        {
            // Room Center (0,0,0) selected
            origin_offset_x = 0.0f;
            origin_offset_y = 0.0f;
            origin_offset_z = 0.0f;
            LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Using room center origin: 0,0,0");
        }
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

        SaveCustomControllers();
        UpdateAvailableControllersList();

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

    LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Layout saved successfully to: %s", filename.c_str());
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

        // User position UI controls have been removed - values stored for legacy compatibility
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
                LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Loaded %d zones from layout", zone_manager->GetZoneCount());
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
            LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Layout has no zones (old format) - starting with empty zone list");
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
        LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Layout loaded successfully from: %s", filename.c_str());
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

    LOG_INFO("[OpenRGB3DSpatialPlugin] Saved layout settings: profile='%s', auto_load=%d",
             profile_name.c_str(), auto_load_enabled);
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

    LOG_INFO("[OpenRGB3DSpatialPlugin] Loaded layout settings: profile='%s', auto_load=%d",
             saved_profile.c_str(), auto_load_enabled);

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

            if(file.fail())
            {
                LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write custom controller: %s", filepath.c_str());
                // Don't show error dialog here - too noisy during auto-save
            }
            else
            {
                LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Saved custom controller: %s", safe_name.c_str());
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

                        std::unique_ptr<VirtualController3D> virtual_ctrl = VirtualController3D::FromJson(ctrl_json, controllers);
                        if(virtual_ctrl)
                        {
                            std::string ctrl_name = virtual_ctrl->GetName();
                            available_controllers_list->addItem(QString("[Custom] ") + QString::fromStdString(ctrl_name));
                            virtual_controllers.push_back(std::move(virtual_ctrl));
                            loaded_count++;
                            LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Loaded custom controller: %s",
                                      ctrl_name.c_str());
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

    LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Setting up effect UI for effect type %d", effect_type);

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
        "Explosion3D"       // 7
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
    LOG_WARNING("[OpenRGB3DSpatialPlugin] Creating effect: %s (type %d)", effect_names[effect_type], effect_type);
    SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(effect_names[effect_type]);
    if(!effect)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create effect: %s", effect_names[effect_type]);
        return;
    }
    LOG_WARNING("[OpenRGB3DSpatialPlugin] Effect created successfully: %p", effect);

    /*---------------------------------------------------------*\
    | Setup effect UI (same for ALL effects)                  |
    \*---------------------------------------------------------*/
    effect->setParent(effect_controls_widget);
    effect->CreateCommonEffectControls(effect_controls_widget);
    effect->SetupCustomUI(effect_controls_widget);
    current_effect_ui = effect;

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

    LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Setting up stack preset UI");

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
    LOG_WARNING("[OpenRGB3DSpatialPlugin] on_effect_changed called with index: %d", index);

    /*---------------------------------------------------------*\
    | Validate index range                                     |
    \*---------------------------------------------------------*/
    if(index < 0)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] on_effect_changed: invalid index %d", index);
        return;
    }

    /*---------------------------------------------------------*\
    | Remember if effect was running so we can restart it      |
    \*---------------------------------------------------------*/
    bool was_running = effect_running;
    LOG_WARNING("[OpenRGB3DSpatialPlugin] Effect was running: %d", was_running);

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
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Item data valid: %d, value: %d",
                 data.isValid(), data.isValid() ? data.toInt() : 0);

        if(data.isValid() && data.toInt() < 0)
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Setting up stack preset UI");
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
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Setting up regular effect UI for effect_type: %d", index - 1);
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
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Auto-starting new effect since previous was running");
            on_start_effect_clicked();
        }
    }
    else
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Index is 0 (None), clearing effect UI");

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

    // Always add "Room Center" as first option
    effect_origin_combo->addItem("Room Center (0,0,0)", QVariant(-1));

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

