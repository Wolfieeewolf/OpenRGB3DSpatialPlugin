// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerDialog.h"
#include "ControllerDisplayUtils.h"
#include "ui_CustomControllerDialog.h"
#include "CustomControllerDeviceList.h"
#include "CustomControllerPreviewDialog.h"
#include "custom-controller-grid/CustomControllerLayoutGrid.h"
#include "custom-controller-grid/CustomControllerGridCell.h"
#include "ControllerLayout3D.h"
#include "PluginUiUtils.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace
{

QVBoxLayout* NewCustomControllerLayerTabLayout(QWidget* tab)
{
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);
    return layout;
}

} // namespace
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QVector>
#include <QTabWidget>
#include <QTimer>
#include <QIcon>
#include <QPixmap>
#include <QShortcut>
#include <QSplitter>
#include <QFrame>
#include <cmath>
#include <algorithm>
#include <climits>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <tuple>

static bool TryGetDialogGlobalLedIndex(RGBController* controller,
                                       unsigned int zone_idx,
                                       unsigned int led_idx,
                                       unsigned int* global_led_idx)
{
    if(!controller || !global_led_idx)
    {
        return false;
    }
    if(zone_idx >= controller->zones.size())
    {
        return false;
    }
    if(led_idx >= controller->zones[zone_idx].leds_count)
    {
        return false;
    }

    *global_led_idx = controller->zones[zone_idx].start_idx + led_idx;
    return true;
}

static RGBController* GetControllerByRow(ResourceManagerInterface* resource_manager, int row)
{
    if(!resource_manager || row < 0)
    {
        return nullptr;
    }
    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    if(row < 0 || row >= (int)controllers.size())
    {
        return nullptr;
    }
    return controllers[row];
}

struct ProfileLayoutBounds
{
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    bool valid;
};

static ProfileLayoutBounds ComputeProfileLayoutBounds(const std::vector<LEDPosition3D>& positions)
{
    ProfileLayoutBounds bounds{0.0f, 0.0f, 0.0f, 0.0f, false};
    if(positions.empty())
    {
        return bounds;
    }

    bounds.min_x = bounds.max_x = positions[0].local_position.x;
    bounds.min_y = bounds.max_y = positions[0].local_position.y;
    bounds.valid = true;

    for(unsigned int i = 1; i < positions.size(); i++)
    {
        if(positions[i].local_position.x < bounds.min_x) bounds.min_x = positions[i].local_position.x;
        if(positions[i].local_position.x > bounds.max_x) bounds.max_x = positions[i].local_position.x;
        if(positions[i].local_position.y < bounds.min_y) bounds.min_y = positions[i].local_position.y;
        if(positions[i].local_position.y > bounds.max_y) bounds.max_y = positions[i].local_position.y;
    }

    return bounds;
}

static std::vector<LEDPosition3D> PositionsForLayoutBounds(const std::vector<LEDPosition3D>& positions,
                                                         int                               granularity,
                                                         int                               item_idx)
{
    if(granularity == 0)
    {
        return positions;
    }

    if(granularity == 1)
    {
        std::vector<LEDPosition3D> zone_positions;
        zone_positions.reserve(positions.size());
        const unsigned int zone_idx = static_cast<unsigned int>(item_idx);
        for(const LEDPosition3D& pos : positions)
        {
            if(pos.zone_idx == zone_idx)
            {
                zone_positions.push_back(pos);
            }
        }
        return zone_positions;
    }

    return {};
}

static bool IsLedMappedOnLayer(const std::vector<GridLEDMapping>& mappings,
                              RGBController*              controller,
                              unsigned int                zone_idx,
                              unsigned int                led_idx,
                              int                         layer)
{
    for(const GridLEDMapping& mapping : mappings)
    {
        if(mapping.controller == controller && mapping.zone_idx == zone_idx && mapping.led_idx == led_idx
           && mapping.z == layer)
        {
            return true;
        }
    }
    return false;
}

static bool ProfileCellToGrid(const ProfileLayoutBounds& bounds,
                              const LEDPosition3D& pos,
                              int start_x,
                              int start_y,
                              int grid_w,
                              int grid_h,
                              int* out_x,
                              int* out_y)
{
    if(!bounds.valid || !out_x || !out_y)
    {
        return false;
    }

    const int x = start_x + (int)std::lround(pos.local_position.x - bounds.min_x);
    const int y = start_y + (int)std::lround(pos.local_position.y - bounds.min_y);
    if(x < 0 || x >= grid_w || y < 0 || y >= grid_h)
    {
        return false;
    }

    *out_x = x;
    *out_y = y;
    return true;
}

static void CollectMappingsAtCell(const std::vector<GridLEDMapping>& mappings,
                                  int x,
                                  int y,
                                  int z,
                                  std::vector<GridLEDMapping>& out)
{
    out.clear();
    for(const GridLEDMapping& mapping : mappings)
    {
        if(mapping.x == x && mapping.y == y && mapping.z == z)
        {
            out.push_back(mapping);
        }
    }
}

static void RemoveMappingsAtCell(std::vector<GridLEDMapping>& mappings, int x, int y, int z)
{
    for(std::vector<GridLEDMapping>::iterator it = mappings.begin(); it != mappings.end();)
    {
        if(it->x == x && it->y == y && it->z == z)
        {
            it = mappings.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

static uint64_t GridCellKey(int x, int y)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
}

using LayerCellIndex = std::unordered_map<uint64_t, std::vector<size_t>>;

static LayerCellIndex BuildLayerCellIndex(const std::vector<GridLEDMapping>& mappings, int layer)
{
    LayerCellIndex index;
    for(size_t mapping_index = 0; mapping_index < mappings.size(); mapping_index++)
    {
        const GridLEDMapping& mapping = mappings[mapping_index];
        if(mapping.z != layer || !mapping.controller)
        {
            continue;
        }

        index[GridCellKey(mapping.x, mapping.y)].push_back(mapping_index);
    }

    return index;
}

static void CollectMappingsForCells(const std::set<std::pair<int, int>>& cells,
                                    int layer,
                                    const std::vector<GridLEDMapping>& all_mappings,
                                    std::vector<GridLEDMapping>& out)
{
    out.clear();
    for(const GridLEDMapping& m : all_mappings)
    {
        if(m.z != layer || !m.controller)
        {
            continue;
        }
        if(cells.count(std::make_pair(m.x, m.y)) > 0)
        {
            out.push_back(m);
        }
    }
}

enum class CellIdentifyState
{
    Empty,
    Off,
    On,
    Partial
};

enum class IdentifyUiState
{
    NoSelection,
    NoMapped,
    AllOff,
    AllOn,
    Mixed
};

static CellIdentifyState GetCellIdentifyState(int x,
                                             int y,
                                             int layer,
                                             const std::vector<GridLEDMapping>& mappings,
                                             const std::map<std::pair<RGBController*, unsigned int>, RGBColor>& identified)
{
    unsigned int mapped_count = 0;
    unsigned int identified_count = 0;

    for(const GridLEDMapping& mapping : mappings)
    {
        if(mapping.x != x || mapping.y != y || mapping.z != layer || !mapping.controller)
        {
            continue;
        }

        unsigned int global_led_idx = 0;
        if(!TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx))
        {
            continue;
        }

        mapped_count++;
        if(identified.count(std::make_pair(mapping.controller, global_led_idx)) > 0)
        {
            identified_count++;
        }
    }

    if(mapped_count == 0)
    {
        return CellIdentifyState::Empty;
    }
    if(identified_count == 0)
    {
        return CellIdentifyState::Off;
    }
    if(identified_count == mapped_count)
    {
        return CellIdentifyState::On;
    }
    return CellIdentifyState::Partial;
}

static IdentifyUiState EvaluateIdentifyUiState(const std::set<std::pair<int, int>>& cells,
                                               int layer,
                                               const std::vector<GridLEDMapping>& mappings,
                                               const std::map<std::pair<RGBController*, unsigned int>, RGBColor>& identified)
{
    if(cells.empty())
    {
        return IdentifyUiState::NoSelection;
    }

    bool saw_empty = false;
    bool saw_off = false;
    bool saw_on = false;

    for(const std::pair<int, int>& cell : cells)
    {
        switch(GetCellIdentifyState(cell.first, cell.second, layer, mappings, identified))
        {
        case CellIdentifyState::Empty:
            saw_empty = true;
            break;
        case CellIdentifyState::Off:
            saw_off = true;
            break;
        case CellIdentifyState::On:
            saw_on = true;
            break;
        case CellIdentifyState::Partial:
            return IdentifyUiState::Mixed;
        }
    }

    if(saw_on && (saw_off || saw_empty))
    {
        return IdentifyUiState::Mixed;
    }
    if(saw_off && saw_empty)
    {
        return IdentifyUiState::Mixed;
    }
    if(saw_on)
    {
        return IdentifyUiState::AllOn;
    }
    if(saw_off)
    {
        return IdentifyUiState::AllOff;
    }
    return IdentifyUiState::NoMapped;
}

CustomControllerDialog::CustomControllerDialog(ResourceManagerInterface* rm, QWidget *parent)
    : QDialog(parent),
      resource_manager(rm),
      preview_dialog(nullptr),
      layout_grid(nullptr),
      layout_grid_scale_mm(10.0f),
      current_layer(0),
      selected_row(-1),
      selected_col(-1)
{
    setWindowTitle(tr("Custom 3D Controller"));
    resize(1120, 720);
    SetupUI();
}

CustomControllerDialog::~CustomControllerDialog()
{
    shutting_down_ = true;

    if(color_refresh_timer)
    {
        color_refresh_timer->stop();
    }

    if(preview_dialog)
    {
        preview_dialog->blockSignals(true);
        preview_dialog->hide();
        preview_dialog->setParent(nullptr);
        delete preview_dialog;
        preview_dialog = nullptr;
    }

    RestoreAllIdentifiedLeds();

    if(layout_grid)
    {
        layout_grid->setParent(nullptr);
        layout_grid = nullptr;
    }

    delete ui;
    ui = nullptr;
}

void CustomControllerDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    if(color_refresh_timer && !color_refresh_timer->isActive())
    {
        refresh_colors();
        color_refresh_timer->start();
    }
}

void CustomControllerDialog::hideEvent(QHideEvent* event)
{
    if(color_refresh_timer)
    {
        color_refresh_timer->stop();
    }

    QDialog::hideEvent(event);
}

void CustomControllerDialog::closeEvent(QCloseEvent* event)
{
    shutting_down_ = true;

    if(color_refresh_timer)
    {
        color_refresh_timer->stop();
    }

    if(preview_dialog)
    {
        preview_dialog->hide();
    }

    QDialog::closeEvent(event);
}

void CustomControllerDialog::SetupUI()
{
    if(!resource_manager)
    {
        return;
    }

    ui = new Ui::CustomControllerDialog;
    ui->setupUi(this);

    PluginUiApplyBoldLabel(ui->titleLabel);
    summary_label = ui->summaryLabel;
    PluginUiApplyBoldLabel(summary_label);
    PluginUiApplyMutedSecondaryLabel(ui->helpLabel->label());

    name_edit         = ui->nameEdit;
    width_spin        = ui->widthSpin;
    height_spin       = ui->heightSpin;
    depth_spin        = ui->depthSpin;
    spacing_x_spin    = ui->spacingXSpin;
    spacing_y_spin    = ui->spacingYSpin;
    spacing_z_spin    = ui->spacingZSpin;
    fit_layout_button  = ui->fitLayoutButton;
    reset_view_button_ = ui->resetViewButton;
    connect(width_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomControllerDialog::on_dimension_changed);
    connect(height_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomControllerDialog::on_dimension_changed);
    connect(depth_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomControllerDialog::on_dimension_changed);
    connect(spacing_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CustomControllerDialog::on_layout_params_changed);
    connect(spacing_y_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CustomControllerDialog::on_layout_params_changed);
    connect(spacing_z_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CustomControllerDialog::on_layout_params_changed);
    connect(fit_layout_button, &QPushButton::clicked, this, &CustomControllerDialog::on_fit_device_layout_clicked);
    connect(reset_view_button_, &QPushButton::clicked, this, &CustomControllerDialog::on_reset_grid_view_clicked);

    PluginUiApplyBoldLabel(ui->devicesHeading);
    device_list = new CustomControllerDeviceList(ui->leftPanel);
    ui->deviceListLayout->addWidget(device_list, 1);
    connect(device_list, &CustomControllerDeviceList::selectionChanged, this,
            &CustomControllerDialog::on_source_selection_changed);
    connect(device_list, &CustomControllerDeviceList::enableToggled, this,
            &CustomControllerDialog::on_source_enable_toggled);

    identify_button = ui->identifyButton;
    connect(identify_button, &QPushButton::clicked, this, &CustomControllerDialog::on_identify_selection_clicked);

    clear_button            = ui->clearButton;
    remove_from_grid_button = ui->removeFromGridButton;
    connect(clear_button, &QPushButton::clicked, this, &CustomControllerDialog::on_clear_cell_clicked);
    connect(remove_from_grid_button, &QPushButton::clicked, this, &CustomControllerDialog::on_remove_all_leds_clicked);

    QFrame* left_wrapped = PluginUiWrapInSettingsPanel(ui->leftPanel, 8);
    ui->mainSplitter->insertWidget(0, left_wrapped);

    PluginUiApplyBoldLabel(ui->canvasHeading);
    PluginUiApplyBoldLabel(ui->depthCaption);
    layer_tabs = ui->layerTabs;
    connect(layer_tabs, QOverload<int>::of(&QTabWidget::currentChanged), this, &CustomControllerDialog::on_layer_tab_changed);

    layout_grid = new CustomControllerLayoutGrid();
    layout_grid->setMinimumHeight(280);
    layout_grid->SetSelectionColor(PluginUiGridSelectionColor(layout_grid));
    connect(layout_grid, &CustomControllerLayoutGrid::cellClicked,
            this, &CustomControllerDialog::on_grid_cell_clicked);
    connect(layout_grid, &CustomControllerLayoutGrid::cellDoubleClicked,
            this, &CustomControllerDialog::on_grid_cell_double_clicked);
    connect(layout_grid, &CustomControllerLayoutGrid::selectionChanged,
            this, &CustomControllerDialog::on_grid_selection_changed);

    connect(ui->addLayerButton, &QPushButton::clicked, this, &CustomControllerDialog::on_add_layer_clicked);
    connect(ui->removeLayerButton, &QPushButton::clicked, this, &CustomControllerDialog::on_remove_layer_clicked);
    RebuildLayerTabs();

    cell_info_label = ui->cellInfoLabel;
    PluginUiApplyMutedSecondaryLabel(cell_info_label);

    transform_group         = ui->transformGroup;
    rotate_90_button        = ui->rotate90Button;
    rotate_180_button       = ui->rotate180Button;
    rotate_270_button       = ui->rotate270Button;
    flip_horizontal_button  = ui->flipHorizontalButton;
    flip_vertical_button    = ui->flipVerticalButton;
    PluginUiApplyMutedSecondaryLabel(ui->transformHelp);
    connect(rotate_90_button, &QPushButton::clicked, this, &CustomControllerDialog::on_rotate_grid_90);
    connect(rotate_180_button, &QPushButton::clicked, this, &CustomControllerDialog::on_rotate_grid_180);
    connect(rotate_270_button, &QPushButton::clicked, this, &CustomControllerDialog::on_rotate_grid_270);
    connect(flip_horizontal_button, &QPushButton::clicked, this, &CustomControllerDialog::on_flip_grid_horizontal);
    connect(flip_vertical_button, &QPushButton::clicked, this, &CustomControllerDialog::on_flip_grid_vertical);

    QFrame* center_wrapped = PluginUiWrapInSettingsPanel(ui->centerPanel, 8);
    ui->mainSplitter->insertWidget(1, center_wrapped);
    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);
    ui->mainSplitter->setSizes({260, 760});

    save_button = ui->saveButton;
    PluginUiApplyPrimaryButton(save_button);
    connect(ui->previewButton, &QPushButton::clicked, this, &CustomControllerDialog::on_show_preview_3d_clicked);
    connect(save_button, &QPushButton::clicked, this, &CustomControllerDialog::on_save_clicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    device_list->rebuild(resource_manager, this);

    color_refresh_timer = new QTimer(this);
    color_refresh_timer->setTimerType(Qt::CoarseTimer);
    color_refresh_timer->setInterval(250);
    connect(color_refresh_timer, &QTimer::timeout, this, &CustomControllerDialog::refresh_colors);

    QShortcut* save_shortcut = new QShortcut(QKeySequence::Save, this);
    connect(save_shortcut, &QShortcut::activated, this, &CustomControllerDialog::on_save_clicked);
    QShortcut* escape_shortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape_shortcut, &QShortcut::activated, this, &QDialog::reject);
    QShortcut* delete_shortcut = new QShortcut(QKeySequence::Delete, this);
    connect(delete_shortcut, &QShortcut::activated, this, &CustomControllerDialog::on_clear_cell_clicked);

    UpdateGridDisplay();
    UpdateIdentifyButtonUi();
}

void CustomControllerDialog::on_source_selection_changed(const CustomControllerSourceRef& source)
{
    Q_UNUSED(source);
    UpdateCellInfo();
}

void CustomControllerDialog::on_source_enable_toggled(const CustomControllerSourceRef& source, bool enabled)
{
    if(enabled)
    {
        if(!assignSource(source))
        {
            refreshDeviceList();
        }
    }
    else
    {
        removeSourceFromGrid(source);
        refreshDeviceList();
        UpdateCellInfo();
        UpdateSummaryLabel();
        UpdateIdentifyButtonUi();
        UpdateGridColors();
    }
}

bool CustomControllerDialog::selectedGridCellValid() const
{
    return selected_row >= 0 && selected_col >= 0;
}

CustomControllerSourceRef CustomControllerDialog::currentSourceSelection() const
{
    if(device_list)
    {
        return device_list->selectedSource();
    }
    return {};
}

RGBController* CustomControllerDialog::controllerForSource(const CustomControllerSourceRef& source) const
{
    if(!source.isValid() || !resource_manager)
    {
        return nullptr;
    }
    return GetControllerByRow(resource_manager, source.controller_index);
}

bool CustomControllerDialog::IsSourceItemAvailable(const CustomControllerSourceRef& source) const
{
    RGBController* controller = controllerForSource(source);
    if(!controller)
    {
        return false;
    }

    return !IsItemAssigned(controller, source.granularity, source.item_idx);
}

bool CustomControllerDialog::CanAddSourceToGrid(const CustomControllerSourceRef& source) const
{
    return source.isValid() && selectedGridCellValid() && IsSourceItemAvailable(source);
}

bool CustomControllerDialog::IsSourceItemOnGrid(const CustomControllerSourceRef& source) const
{
    RGBController* controller = controllerForSource(source);
    if(!controller)
    {
        return false;
    }
    return IsItemAssigned(controller, source.granularity, source.item_idx);
}

QColor CustomControllerDialog::SourceItemColor(const CustomControllerSourceRef& source) const
{
    RGBController* controller = controllerForSource(source);
    if(!controller)
    {
        return QColor();
    }
    return GetItemColor(controller, source.granularity, source.item_idx);
}

void CustomControllerDialog::refreshDeviceList(int controller_index)
{
    if(device_list)
    {
        device_list->refreshFromHost(controller_index);
    }
}

void CustomControllerDialog::PopulateDeviceItemCombo(int controller_index, int granularity, QComboBox* combo) const
{
    if(!combo || !resource_manager || granularity <= 0)
    {
        return;
    }

    RGBController* controller = GetControllerByRow(resource_manager, controller_index);
    if(!controller)
    {
        return;
    }

    if(granularity == 1)
    {
        for(unsigned int i = 0; i < controller->zones.size(); i++)
        {
            if(!IsItemAssigned(controller, granularity, static_cast<int>(i)))
            {
                const QColor color = GetItemColor(controller, granularity, static_cast<int>(i));
                QPixmap pixmap(16, 16);
                pixmap.fill(color);
                combo->addItem(QIcon(pixmap), QString::fromStdString(controller->GetZoneName(i)),
                               static_cast<int>(i));
            }
        }
    }
    else if(granularity == 2)
    {
        for(unsigned int i = 0; i < controller->leds.size(); i++)
        {
            if(!IsItemAssigned(controller, granularity, static_cast<int>(i)))
            {
                const QColor color = GetItemColor(controller, granularity, static_cast<int>(i));
                QPixmap pixmap(16, 16);
                pixmap.fill(color);
                combo->addItem(QIcon(pixmap), QString::fromStdString(controller->GetLEDName(i)), static_cast<int>(i));
            }
        }
    }
}

void CustomControllerDialog::removeSourceFromGrid(const CustomControllerSourceRef& source)
{
    RGBController* controller = controllerForSource(source);
    if(!controller)
    {
        return;
    }

    std::vector<GridLEDMapping> removed_mappings;
    for(auto it = led_mappings.begin(); it != led_mappings.end();)
    {
        const GridLEDMapping& mapping = *it;
        if(mapping.controller != controller)
        {
            ++it;
            continue;
        }

        bool remove = false;
        if(source.granularity == 0)
        {
            remove = true;
        }
        else if(source.granularity == 1)
        {
            remove = mapping.zone_idx == static_cast<unsigned int>(source.item_idx);
        }
        else if(source.granularity == 2)
        {
            unsigned int global_led_idx = 0;
            if(TryGetDialogGlobalLedIndex(controller, mapping.zone_idx, mapping.led_idx, &global_led_idx)
               && global_led_idx == static_cast<unsigned int>(source.item_idx))
            {
                remove = true;
            }
        }

        if(remove)
        {
            removed_mappings.push_back(mapping);
            it = led_mappings.erase(it);
        }
        else
        {
            ++it;
        }
    }

    RestoreIdentifyForMappings(removed_mappings);
}

bool CustomControllerDialog::assignSource(const CustomControllerSourceRef& source)
{
    if(!source.isValid())
    {
        QMessageBox::warning(this, tr("Nothing selected"), tr("Select a device, zone, or LED from the list."));
        return false;
    }

    if(selected_row < 0 || selected_col < 0)
    {
        QMessageBox::warning(this, tr("No cell selected"), tr("Please select a canvas cell first."));
        return false;
    }

    RGBController* controller = controllerForSource(source);
    if(!controller)
    {
        return false;
    }

    if(!IsSourceItemAvailable(source))
    {
        QMessageBox::information(this, tr("Already on grid"),
                                 tr("That source is already placed. Remove it with − first or pick another."));
        return false;
    }

    std::vector<GridLEDMapping> replaced_mappings;
    CollectMappingsAtCell(led_mappings, selected_col, selected_row, current_layer, replaced_mappings);
    RestoreIdentifyForMappings(replaced_mappings);
    RemoveMappingsAtCell(led_mappings, selected_col, selected_row, current_layer);

    PlaceProfileLayout(controller, source.granularity, source.item_idx, selected_col, selected_row);
    UpdateCellInfo();
    UpdateSummaryLabel();
    UpdateIdentifyButtonUi();
    UpdateGridColors();
    refreshDeviceList(source.controller_index);
    syncPreviewLayoutIfVisible();
    return true;
}

void CustomControllerDialog::on_grid_cell_clicked(int column, int row)
{
    selected_col = column;
    selected_row = row;
    UpdateCellInfo();
    RefreshLayoutGridVisuals();
    UpdateIdentifyButtonUi();

    const CustomControllerSourceRef source = currentSourceSelection();
    if(source.isValid())
    {
        refreshDeviceList(source.controller_index);
    }
}

void CustomControllerDialog::on_grid_cell_double_clicked(int column, int row)
{
    selected_col = column;
    selected_row = row;
    UpdateCellInfo();

    const CustomControllerSourceRef source = currentSourceSelection();
    if(!CanAddSourceToGrid(source))
    {
        return;
    }

    assignSource(source);
}

void CustomControllerDialog::on_grid_selection_changed()
{
    if(layout_grid)
    {
        const std::set<std::pair<int, int>> cells = layout_grid->SelectedCells();
        if(!cells.empty())
        {
            const std::pair<int, int> first = *cells.begin();
            selected_col = first.first;
            selected_row = first.second;
            layout_grid->SetAnchorCell(selected_col, selected_row);
        }
    }

    UpdateCellInfo();
    RefreshLayoutGridVisuals();
    UpdateIdentifyButtonUi();

    const CustomControllerSourceRef source = currentSourceSelection();
    if(source.isValid())
    {
        refreshDeviceList(source.controller_index);
    }
}

void CustomControllerDialog::on_add_layer_clicked()
{
    if(!depth_spin)
    {
        return;
    }

    if(depth_spin->value() < depth_spin->maximum())
    {
        depth_spin->setValue(depth_spin->value() + 1);
    }
}

void CustomControllerDialog::on_remove_layer_clicked()
{
    if(!depth_spin || depth_spin->value() <= 1)
    {
        return;
    }

    depth_spin->setValue(depth_spin->value() - 1);
}

void CustomControllerDialog::on_layer_tab_changed(int index)
{
    if(index < 0)
    {
        return;
    }

    current_layer = index;
    AttachLayoutGridToLayerTab(index);
    UpdateGridDisplay();
    UpdateIdentifyButtonUi();
}

void CustomControllerDialog::on_dimension_changed()
{
    const int new_width = width_spin->value();
    const int new_height = height_spin->value();
    const int new_depth = depth_spin->value();

    auto trim_out_of_bounds = [&](std::vector<GridLEDMapping>& mappings) -> size_t
    {
        size_t count = 0;
        for(std::vector<GridLEDMapping>::iterator it = mappings.begin(); it != mappings.end();)
        {
            if(it->x >= new_width || it->y >= new_height || it->z >= new_depth)
            {
                it = mappings.erase(it);
                count++;
            }
            else
            {
                ++it;
            }
        }
        return count;
    };

    size_t removed = trim_out_of_bounds(led_mappings);
    if(removed > 0)
    {
        QMessageBox::information(this, "Grid Resized",
            QString("%1 mapping(s) were outside the new grid and were removed.").arg(static_cast<qlonglong>(removed)));
    }

    if(current_layer >= new_depth)
    {
        current_layer = new_depth - 1;
        if(layer_tabs) layer_tabs->setCurrentIndex(current_layer);
    }
    const int prev_tab_count = layer_tabs->count();
    RebuildLayerTabs();
    if(new_depth > prev_tab_count)
    {
        current_layer = new_depth - 1;
        layer_tabs->setCurrentIndex(current_layer);
        AttachLayoutGridToLayerTab(current_layer);
    }
    UpdateGridDisplay();
    UpdateCellInfo();
    refreshDeviceList();
}

void CustomControllerDialog::AttachLayoutGridToLayerTab(int layer_index)
{
    if(!layout_grid || !layer_tabs || layer_index < 0 || layer_index >= layer_tabs->count())
    {
        return;
    }

    layout_grid->setParent(nullptr);

    QWidget* tab = layer_tabs->widget(layer_index);
    if(!tab)
    {
        return;
    }

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(tab->layout());
    if(!layout)
    {
        layout = NewCustomControllerLayerTabLayout(tab);
    }

    layout->addWidget(layout_grid, 1);
}

void CustomControllerDialog::UpdateLayerTabControls()
{
    if(ui->removeLayerButton)
    {
        ui->removeLayerButton->setEnabled(depth_spin && depth_spin->value() > 1);
    }
    if(ui->addLayerButton && depth_spin)
    {
        ui->addLayerButton->setEnabled(depth_spin->value() < depth_spin->maximum());
    }
}

void CustomControllerDialog::RebuildLayerTabs()
{
    if(!layer_tabs || !depth_spin)
    {
        return;
    }

    const int old_layer     = current_layer;
    const int current_count = layer_tabs->count();
    const int new_depth     = depth_spin->value();

    if(layout_grid)
    {
        layout_grid->setParent(nullptr);
    }

    if(new_depth > current_count)
    {
        for(int i = current_count; i < new_depth; i++)
        {
            QWidget* tab = new QWidget();
            NewCustomControllerLayerTabLayout(tab);
            layer_tabs->addTab(tab, tr("Layer %1").arg(i));
        }
    }
    else if(new_depth < current_count)
    {
        while(layer_tabs->count() > new_depth)
        {
            const int last_idx = layer_tabs->count() - 1;
            QWidget* tab = layer_tabs->widget(last_idx);
            layer_tabs->removeTab(last_idx);
            delete tab;
        }
    }

    const int target_layer = (old_layer < new_depth) ? old_layer : std::max(0, new_depth - 1);
    current_layer = target_layer;

    if(layer_tabs->currentIndex() != target_layer)
    {
        layer_tabs->blockSignals(true);
        layer_tabs->setCurrentIndex(target_layer);
        layer_tabs->blockSignals(false);
    }

    AttachLayoutGridToLayerTab(target_layer);
    UpdateLayerTabControls();
}

std::set<std::pair<int, int>> CustomControllerDialog::SelectedGridCells() const
{
    if(layout_grid)
    {
        std::set<std::pair<int, int>> cells = layout_grid->SelectedCells();
        if(!cells.empty())
        {
            return cells;
        }
    }

    if(selected_col >= 0 && selected_row >= 0)
    {
        return {std::make_pair(selected_col, selected_row)};
    }

    return {};
}

bool CustomControllerDialog::LayoutCellsCacheMatchesGrid() const
{
    if(!layout_grid || !width_spin || !height_spin)
    {
        return false;
    }

    return layout_cells_cache_w_ == width_spin->value()
        && layout_cells_cache_h_ == height_spin->value()
        && layout_cells_cache_layer_ == current_layer
        && layout_cells_cache_.size() == layout_cells_cache_w_ * layout_cells_cache_h_;
}

QColor CustomControllerDialog::ComputeCellBaseColor(const std::vector<GridLEDMapping>& cell_mappings,
                                                    bool is_hole) const
{
    if(!layout_grid)
    {
        return QColor();
    }

    if(is_hole)
    {
        return PluginUiGridHoleCellColor(layout_grid);
    }

    if(cell_mappings.empty())
    {
        return PluginUiGridEmptyCellColor(layout_grid);
    }

    if(cell_mappings.size() == 1)
    {
        return GetMappingColor(cell_mappings[0]);
    }

    unsigned int total_r = 0;
    unsigned int total_g = 0;
    unsigned int total_b = 0;

    for(const GridLEDMapping& mapping : cell_mappings)
    {
        const QColor led_color = GetMappingColor(mapping);
        total_r += led_color.red();
        total_g += led_color.green();
        total_b += led_color.blue();
    }

    return QColor(static_cast<int>(total_r / cell_mappings.size()),
                  static_cast<int>(total_g / cell_mappings.size()),
                  static_cast<int>(total_b / cell_mappings.size()));
}

void CustomControllerDialog::ApplyCellFillAndText(CustomControllerGridCellVisual& visual,
                                                  const QColor& base_color,
                                                  const std::vector<GridLEDMapping>& cell_mappings,
                                                  bool is_selected) const
{
    if(!layout_grid)
    {
        return;
    }

    if(is_selected)
    {
        const QColor selection_color = PluginUiGridSelectionColor(layout_grid);
        visual.fill = cell_mappings.empty()
            ? selection_color
            : PluginUiBlendColors(base_color, selection_color, 0.7f);
    }
    else
    {
        visual.fill = base_color;
    }

    if(!cell_mappings.empty() || is_selected)
    {
        visual.text = PluginUiReadableTextOn(visual.fill, layout_grid);
    }
    else
    {
        visual.text = QColor();
    }
}

void CustomControllerDialog::PopulateCellVisual(int col,
                                                int row,
                                                const std::vector<GridLEDMapping>& cell_mappings,
                                                CustomControllerGridCellVisual& visual,
                                                bool is_selected) const
{
    visual.is_empty = cell_mappings.empty();
    visual.is_hole  = visual.is_empty && IsMatrixHoleCell(col, row);

    if(!cell_mappings.empty())
    {
        if(cell_mappings.size() == 1)
        {
            visual.tooltip = GetMappingTooltip(cell_mappings[0]);
        }
        else
        {
            visual.tooltip = tr("Multiple LEDs (%1):\n").arg(cell_mappings.size());
            for(size_t m = 0; m < cell_mappings.size() && m < 5; m++)
            {
                visual.tooltip += QString(QChar(0x2022)) + QLatin1Char(' ')
                    + GetMappingTooltip(cell_mappings[m]) + QLatin1Char('\n');
            }
            if(cell_mappings.size() > 5)
            {
                visual.tooltip += tr("... and %1 more").arg(cell_mappings.size() - 5);
            }
        }

        visual.label = GetMappingCellLabel(cell_mappings);
    }
    else if(visual.is_hole)
    {
        visual.tooltip = tr("Matrix gap (no LED on this device)");
        visual.label.clear();
    }
    else
    {
        visual.tooltip = tr("Empty — click to assign");
        visual.label.clear();
    }

    const QColor base_color = ComputeCellBaseColor(cell_mappings, visual.is_hole);
    ApplyCellFillAndText(visual, base_color, cell_mappings, is_selected);
}

void CustomControllerDialog::RefreshLayoutGridVisuals()
{
    if(!layout_grid)
    {
        return;
    }

    const int grid_w = width_spin->value();
    const int grid_h = height_spin->value();
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();

    layout_grid->SetGridSize(grid_w, grid_h);
    layout_grid->SetSelectionColor(PluginUiGridSelectionColor(layout_grid));

    const LayerCellIndex layer_index = BuildLayerCellIndex(led_mappings, current_layer);

    layout_cells_cache_.resize(grid_w * grid_h);
    for(int row = 0; row < grid_h; row++)
    {
        for(int col = 0; col < grid_w; col++)
        {
            const int index = row * grid_w + col;
            CustomControllerGridCellVisual& visual = layout_cells_cache_[index];

            std::vector<GridLEDMapping> cell_mappings;
            const auto cell_it = layer_index.find(GridCellKey(col, row));
            if(cell_it != layer_index.end())
            {
                cell_mappings.reserve(cell_it->second.size());
                for(size_t mapping_index : cell_it->second)
                {
                    cell_mappings.push_back(led_mappings[mapping_index]);
                }
            }

            const bool is_selected = selected_cells.count(std::make_pair(col, row)) > 0;
            PopulateCellVisual(col, row, cell_mappings, visual, is_selected);
        }
    }

    layout_cells_cache_w_     = grid_w;
    layout_cells_cache_h_     = grid_h;
    layout_cells_cache_layer_ = current_layer;

    layout_grid->SetCells(layout_cells_cache_);
    layout_grid->SetSelectedCells(selected_cells);
    layout_grid->updateGeometry();
}

void CustomControllerDialog::UpdateGridDisplay()
{
    RefreshLayoutGridVisuals();
    UpdateSummaryLabel();
    syncPreviewLayoutIfVisible();
}

void CustomControllerDialog::UpdateSummaryLabel()
{
    const std::vector<GridLEDMapping>& mappings_ref = led_mappings;
    std::set<std::tuple<int, int, int>> cells;
    for(const GridLEDMapping& m : mappings_ref)
    {
        cells.insert(std::make_tuple(m.x, m.y, m.z));
    }
    summary_label->setText(tr("Assigned: %1 cells · Layer %2").arg(cells.size()).arg(current_layer));

    if(transform_group)
    {
        transform_group->setVisible(!mappings_ref.empty());
    }
}

bool CustomControllerDialog::IsMatrixHoleCell(int x, int y) const
{
    return matrix_hole_cells.find(GridCellKey(x, y)) != matrix_hole_cells.end();
}

void CustomControllerDialog::RebuildMatrixHoleMask(RGBController* controller, int anchor_x, int anchor_y)
{
    matrix_hole_cells.clear();
    if(!controller)
    {
        return;
    }

    const int grid_w = width_spin->value();
    const int grid_h = height_spin->value();

    for(unsigned int zone_idx = 0; zone_idx < controller->zones.size(); zone_idx++)
    {
        zone* current_zone = &controller->zones[zone_idx];
        if(current_zone->type != ZONE_TYPE_MATRIX || current_zone->matrix_map == nullptr)
        {
            continue;
        }

        matrix_map_type* map = current_zone->matrix_map;
        for(unsigned int led_y = 0; led_y < map->height; led_y++)
        {
            for(unsigned int led_x = 0; led_x < map->width; led_x++)
            {
                const unsigned int map_idx = led_y * map->width + led_x;
                if(map->map[map_idx] == 0xFFFFFFFFu)
                {
                    const int gx = anchor_x + static_cast<int>(led_x);
                    const int gy = anchor_y + static_cast<int>(led_y);
                    if(gx >= 0 && gx < grid_w && gy >= 0 && gy < grid_h)
                    {
                        matrix_hole_cells.insert(GridCellKey(gx, gy));
                    }
                }
            }
        }
    }
}

void CustomControllerDialog::UpdateCellInfo()
{
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    if(selected_cells.empty())
    {
        cell_info_label->setText("Click a cell to select it");
        return;
    }

    if(selected_cells.size() > 1)
    {
        std::vector<GridLEDMapping> cell_mappings;
        CollectMappingsForCells(selected_cells, current_layer, led_mappings, cell_mappings);
        QString info = tr("Selected %1 cells on layer %2").arg(selected_cells.size()).arg(current_layer);
        if(cell_mappings.empty())
        {
            info += tr(" — empty");
        }
        else
        {
            info += tr(" — %1 LED(s)").arg(cell_mappings.size());
        }
        cell_info_label->setText(info);
        return;
    }

    const std::pair<int, int> cell = *selected_cells.begin();
    const int col = cell.first;
    const int row = cell.second;

    QString info = QString("Selected: X=%1, Y=%2, Z=%3")
                    .arg(col)
                    .arg(row)
                    .arg(current_layer);

    const std::vector<GridLEDMapping>& mappings_ref = led_mappings;
    std::vector<GridLEDMapping> cell_mappings;
    for(unsigned int i = 0; i < mappings_ref.size(); i++)
    {
        if(mappings_ref[i].x == col && mappings_ref[i].y == row && mappings_ref[i].z == current_layer)
        {
            cell_mappings.push_back(mappings_ref[i]);
        }
    }

    if(cell_mappings.empty())
    {
        info += " - Empty";
    }
    else if(cell_mappings.size() == 1)
    {
        info += " - " + GetMappingDescription(cell_mappings[0]);
    }
    else
    {
        info += tr(" - Multiple LEDs (%1)").arg(cell_mappings.size());
    }

    cell_info_label->setText(info);
}

bool CustomControllerDialog::PlaceProfileLayout(RGBController* controller, int granularity, int item_idx, int start_x, int start_y)
{
    if(!controller)
    {
        return false;
    }

    int grid_w = width_spin->value();
    int grid_h = height_spin->value();

    const std::vector<LEDPosition3D> positions =
        ControllerLayout3D::GenerateCustomGridLayout(controller, grid_w, grid_h, false);

    const ProfileLayoutBounds bounds =
        ComputeProfileLayoutBounds(PositionsForLayoutBounds(positions, granularity, item_idx));

    auto place_profile_position = [&](const LEDPosition3D& pos) -> bool
    {
        int x = 0;
        int y = 0;
        if(!ProfileCellToGrid(bounds, pos, start_x, start_y, grid_w, grid_h, &x, &y))
        {
            return false;
        }
        if(IsMatrixHoleCell(x, y))
        {
            return false;
        }

        if(IsLedMappedOnLayer(led_mappings, controller, pos.zone_idx, pos.led_idx, current_layer))
        {
            return false;
        }

        std::vector<GridLEDMapping> replaced_mappings;
        CollectMappingsAtCell(led_mappings, x, y, current_layer, replaced_mappings);
        RestoreIdentifyForMappings(replaced_mappings);
        RemoveMappingsAtCell(led_mappings, x, y, current_layer);

        GridLEDMapping mapping;
        mapping.x = x;
        mapping.y = y;
        mapping.z = current_layer;
        mapping.controller = controller;
        mapping.zone_idx = pos.zone_idx;
        mapping.led_idx = pos.led_idx;
        mapping.granularity = 2;
        led_mappings.push_back(mapping);
        return true;
    };

    if(granularity == 0)
    {
        int placed = 0;
        int skipped = 0;
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            if(place_profile_position(positions[p]))
            {
                placed++;
            }
            else
            {
                skipped++;
            }
        }

        if(skipped > 0)
        {
            QMessageBox::information(this, tr("Grid too small"),
                                     tr("Placed %1 of %2 LEDs using the device profile (%3 could not fit). Move the anchor cell or use Fit layout.")
                                     .arg(placed).arg(static_cast<int>(positions.size())).arg(skipped));
        }

        if(placed > 0)
        {
            RebuildMatrixHoleMask(controller, start_x, start_y);
        }
    }
    else if(granularity == 1)
    {
        int placed = 0;
        int skipped = 0;
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            if(positions[p].zone_idx != static_cast<unsigned int>(item_idx))
            {
                continue;
            }

            if(place_profile_position(positions[p]))
            {
                placed++;
            }
            else
            {
                skipped++;
            }
        }

        if(skipped > 0)
        {
            QMessageBox::information(this, tr("Grid too small"),
                                     tr("Placed %1 zone LEDs (%2 could not fit). Move the anchor cell or use Fit layout.")
                                     .arg(placed).arg(skipped));
        }

        if(placed > 0)
        {
            RebuildMatrixHoleMask(controller, start_x, start_y);
        }
    }
    else if(granularity == 2)
    {
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            unsigned int global_led_idx = 0;
            if(!TryGetDialogGlobalLedIndex(controller, positions[p].zone_idx, positions[p].led_idx, &global_led_idx))
            {
                continue;
            }
            if(global_led_idx == static_cast<unsigned int>(item_idx))
            {
                if(IsLedMappedOnLayer(led_mappings, controller, positions[p].zone_idx, positions[p].led_idx,
                                     current_layer))
                {
                    return false;
                }

                std::vector<GridLEDMapping> replaced_mappings;
                CollectMappingsAtCell(led_mappings, start_x, start_y, current_layer, replaced_mappings);
                RestoreIdentifyForMappings(replaced_mappings);
                RemoveMappingsAtCell(led_mappings, start_x, start_y, current_layer);

                GridLEDMapping mapping;
                mapping.x = start_x;
                mapping.y = start_y;
                mapping.z = current_layer;
                mapping.controller = controller;
                mapping.zone_idx = positions[p].zone_idx;
                mapping.led_idx = positions[p].led_idx;
                mapping.granularity = 2;
                led_mappings.push_back(mapping);
                return true;
            }
        }
    }

    UpdateGridColors();

    return true;
}

void CustomControllerDialog::on_reset_grid_view_clicked()
{
    if(layout_grid)
    {
        layout_grid->FitGridInView();
    }
}

void CustomControllerDialog::on_fit_device_layout_clicked()
{
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    bool has_mappings = false;

    for(const GridLEDMapping& mapping : led_mappings)
    {
        if(mapping.z != current_layer)
        {
            continue;
        }

        if(!has_mappings)
        {
            min_x = max_x = mapping.x;
            min_y = max_y = mapping.y;
            has_mappings = true;
        }
        else
        {
            min_x = std::min(min_x, mapping.x);
            min_y = std::min(min_y, mapping.y);
            max_x = std::max(max_x, mapping.x);
            max_y = std::max(max_y, mapping.y);
        }
    }

    if(!has_mappings)
    {
        QMessageBox::warning(this, tr("No LEDs placed"),
                             tr("Place LEDs on this layer first, then use Fit layout."));
        return;
    }

    const int new_w = max_x - min_x + 1;
    const int new_h = max_y - min_y + 1;

    if(min_x != 0 || min_y != 0)
    {
        for(GridLEDMapping& mapping : led_mappings)
        {
            if(mapping.z == current_layer)
            {
                mapping.x -= min_x;
                mapping.y -= min_y;
            }
        }

        if(!matrix_hole_cells.empty())
        {
            std::unordered_set<uint64_t> shifted_holes;
            shifted_holes.reserve(matrix_hole_cells.size());
            for(const uint64_t key : matrix_hole_cells)
            {
                const int x = static_cast<int>(static_cast<uint32_t>(key >> 32));
                const int y = static_cast<int>(static_cast<uint32_t>(key));
                const int nx = x - min_x;
                const int ny = y - min_y;
                if(nx >= 0 && nx < new_w && ny >= 0 && ny < new_h)
                {
                    shifted_holes.insert(GridCellKey(nx, ny));
                }
            }
            matrix_hole_cells = std::move(shifted_holes);
        }
    }
    else if(!matrix_hole_cells.empty())
    {
        std::unordered_set<uint64_t> clipped_holes;
        clipped_holes.reserve(matrix_hole_cells.size());
        for(const uint64_t key : matrix_hole_cells)
        {
            const int x = static_cast<int>(static_cast<uint32_t>(key >> 32));
            const int y = static_cast<int>(static_cast<uint32_t>(key));
            if(x >= 0 && x < new_w && y >= 0 && y < new_h)
            {
                clipped_holes.insert(key);
            }
        }
        matrix_hole_cells = std::move(clipped_holes);
    }

    width_spin->blockSignals(true);
    height_spin->blockSignals(true);
    width_spin->setValue(std::max(1, new_w));
    height_spin->setValue(std::max(1, new_h));
    width_spin->blockSignals(false);
    height_spin->blockSignals(false);

    selected_col = 0;
    selected_row = 0;
    if(layout_grid)
    {
        layout_grid->SetAnchorCell(0, 0);
        layout_grid->SetSelectedCells({std::make_pair(0, 0)});
    }

    RebuildLayerTabs();
    UpdateGridDisplay();
    UpdateCellInfo();
    refreshDeviceList();
    UpdateSummaryLabel();
    UpdateIdentifyButtonUi();
    syncPreviewLayoutIfVisible();
}

void CustomControllerDialog::RestoreAllIdentifiedLeds()
{
    if(identified_leds.empty())
    {
        return;
    }

    std::set<RGBController*> updated_controllers;
    for(const std::pair<const std::pair<RGBController*, unsigned int>, RGBColor>& entry : identified_leds)
    {
        RGBController* controller = entry.first.first;
        if(!controller)
        {
            continue;
        }

        controller->SetLED(entry.first.second, entry.second);
        updated_controllers.insert(controller);
    }

    identified_leds.clear();

    for(RGBController* controller : updated_controllers)
    {
        controller->UpdateLEDs();
    }
}

void CustomControllerDialog::RestoreIdentifyForMappings(const std::vector<GridLEDMapping>& mappings)
{
    if(identified_leds.empty() || mappings.empty())
    {
        return;
    }

    std::set<RGBController*> updated_controllers;
    for(const GridLEDMapping& mapping : mappings)
    {
        if(!mapping.controller)
        {
            continue;
        }

        unsigned int global_led_idx = 0;
        if(!TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx))
        {
            continue;
        }

        const auto led_key = std::make_pair(mapping.controller, global_led_idx);
        const auto it = identified_leds.find(led_key);
        if(it == identified_leds.end())
        {
            continue;
        }

        mapping.controller->SetLED(global_led_idx, it->second);
        identified_leds.erase(it);
        updated_controllers.insert(mapping.controller);
    }

    for(RGBController* controller : updated_controllers)
    {
        controller->UpdateLEDs();
    }
}

void CustomControllerDialog::SetIdentifyForCells(const std::set<std::pair<int, int>>& cells, bool enabled)
{
    if(cells.empty())
    {
        return;
    }

    std::vector<GridLEDMapping> cell_mappings;
    CollectMappingsForCells(cells, current_layer, led_mappings, cell_mappings);
    if(cell_mappings.empty())
    {
        return;
    }

    std::set<RGBController*> updated_controllers;
    std::set<std::pair<RGBController*, unsigned int>> seen_leds;

    for(const GridLEDMapping& mapping : cell_mappings)
    {
        unsigned int global_led_idx = 0;
        if(!TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx))
        {
            continue;
        }

        const auto led_key = std::make_pair(mapping.controller, global_led_idx);
        if(!seen_leds.insert(led_key).second)
        {
            continue;
        }

        if(enabled)
        {
            if(identified_leds.count(led_key) > 0)
            {
                continue;
            }

            identified_leds[led_key] = mapping.controller->GetLED(global_led_idx);
            mapping.controller->SetLED(global_led_idx, ToRGBColor(0, 255, 0));
            updated_controllers.insert(mapping.controller);
        }
        else
        {
            const auto it = identified_leds.find(led_key);
            if(it == identified_leds.end())
            {
                continue;
            }

            mapping.controller->SetLED(global_led_idx, it->second);
            identified_leds.erase(it);
            updated_controllers.insert(mapping.controller);
        }
    }

    for(RGBController* controller : updated_controllers)
    {
        controller->UpdateLEDs();
    }

    UpdateIdentifyButtonUi();
    UpdateGridColors();
}

void CustomControllerDialog::UpdateIdentifyButtonUi()
{
    if(!identify_button)
    {
        return;
    }

    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    const IdentifyUiState ui_state = EvaluateIdentifyUiState(selected_cells, current_layer, led_mappings, identified_leds);

    switch(ui_state)
    {
    case IdentifyUiState::NoSelection:
        identify_button->setEnabled(false);
        identify_button->setText(tr("Identify · Off"));
        break;
    case IdentifyUiState::NoMapped:
        identify_button->setEnabled(false);
        identify_button->setText(tr("Identify · Off"));
        break;
    case IdentifyUiState::AllOff:
        identify_button->setEnabled(true);
        identify_button->setText(tr("Identify · Off"));
        break;
    case IdentifyUiState::AllOn:
        identify_button->setEnabled(true);
        identify_button->setText(tr("Identify · On"));
        break;
    case IdentifyUiState::Mixed:
        identify_button->setEnabled(true);
        identify_button->setText(tr("Identify"));
        break;
    }
}

void CustomControllerDialog::on_identify_selection_clicked()
{
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    const IdentifyUiState ui_state = EvaluateIdentifyUiState(selected_cells, current_layer, led_mappings, identified_leds);

    if(ui_state == IdentifyUiState::NoSelection)
    {
        QMessageBox::warning(this, tr("No cell selected"), tr("Select a mapped cell to identify on hardware."));
        return;
    }

    if(ui_state == IdentifyUiState::NoMapped)
    {
        QMessageBox::information(this, tr("Nothing to identify"),
                                 selected_cells.size() > 1
                                     ? tr("None of the selected cells have LED assignments.")
                                     : tr("The selected cell has no LED assignments."));
        return;
    }

    if(ui_state == IdentifyUiState::Mixed)
    {
        SetIdentifyForCells(selected_cells, false);
        return;
    }

    SetIdentifyForCells(selected_cells, ui_state == IdentifyUiState::AllOff);
}

void CustomControllerDialog::on_clear_cell_clicked()
{
    if(selected_row < 0 || selected_col < 0)
    {
        QMessageBox::warning(this, "No Cell Selected", "Please select a grid cell first");
        return;
    }

    std::vector<GridLEDMapping> removed_mappings;
    std::vector<GridLEDMapping>& target = led_mappings;
    for(std::vector<GridLEDMapping>::iterator it = target.begin(); it != target.end();)
    {
        if(it->x == selected_col && it->y == selected_row && it->z == current_layer)
        {
            removed_mappings.push_back(*it);
            it = target.erase(it);
        }
        else
        {
            ++it;
        }
    }

    RestoreIdentifyForMappings(removed_mappings);
    UpdateGridDisplay();
    UpdateCellInfo();
    refreshDeviceList();
    UpdateIdentifyButtonUi();
}

void CustomControllerDialog::on_remove_all_leds_clicked()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, "Grid Empty", "The grid is already empty - no LEDs to remove");
        return;
    }

    int reply = QMessageBox::question(this, tr("Clear Grid"),
                                      tr("Remove every LED assignment from the grid? (%1 assignment(s) will be cleared.)").arg(led_mappings.size()),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        size_t removed_count = led_mappings.size();
        RestoreAllIdentifiedLeds();
        led_mappings.clear();
        matrix_hole_cells.clear();

        QMessageBox::information(this, tr("Removed"), tr("Removed all %1 LED assignment(s) from the grid").arg(static_cast<int>(removed_count)));

        UpdateGridDisplay();
        UpdateCellInfo();
        refreshDeviceList();
        UpdateIdentifyButtonUi();
    }
}

void CustomControllerDialog::on_save_clicked()
{
    if(name_edit->text().isEmpty())
    {
        QMessageBox::warning(this, "No Name", "Please enter a name for the custom controller");
        return;
    }

    int w = width_spin->value();
    int h = height_spin->value();
    int d = depth_spin->value();
    size_t removed = 0;

    for(std::vector<GridLEDMapping>::iterator it = led_mappings.begin(); it != led_mappings.end(); )
    {
        if(it->x < 0 || it->x >= w || it->y < 0 || it->y >= h || it->z < 0 || it->z >= d)
        {
            it = led_mappings.erase(it);
            removed++;
        }
        else
        {
            ++it;
        }
    }
    if(removed > 0)
    {
        QMessageBox::information(this, "Mappings Cleaned",
            QString("Some invalid mappings (outside current grid bounds) were removed."));
    }

    if(led_mappings.empty())
    {
        QMessageBox::warning(this, "No LEDs Assigned", "Please assign at least one LED to the grid");
        return;
    }

    accept();
}

QString CustomControllerDialog::GetControllerName() const
{
    return name_edit->text();
}

int CustomControllerDialog::GetGridWidth() const
{
    return width_spin->value();
}

int CustomControllerDialog::GetGridHeight() const
{
    return height_spin->value();
}

int CustomControllerDialog::GetGridDepth() const
{
    return depth_spin->value();
}

float CustomControllerDialog::GetSpacingX() const
{
    return (float)spacing_x_spin->value();
}

float CustomControllerDialog::GetSpacingY() const
{
    return (float)spacing_y_spin->value();
}

float CustomControllerDialog::GetSpacingZ() const
{
    return (float)spacing_z_spin->value();
}

bool CustomControllerDialog::IsItemAssigned(RGBController* controller, int granularity, int item_idx) const
{
    if(!controller)
    {
        return false;
    }

    const std::vector<GridLEDMapping>& mappings = led_mappings;

    if(granularity == 0)
    {
        for(unsigned int i = 0; i < mappings.size(); i++)
        {
            if(mappings[i].controller == controller)
            {
                return true;
            }
        }
    }
    else if(granularity == 1)
    {
        for(unsigned int i = 0; i < mappings.size(); i++)
        {
            if(mappings[i].controller == controller && mappings[i].zone_idx == (unsigned int)item_idx)
            {
                return true;
            }
        }
    }
    else if(granularity == 2)
    {
        for(unsigned int i = 0; i < mappings.size(); i++)
        {
            if(mappings[i].controller != controller)
            {
                continue;
            }
            unsigned int global_led_idx = 0;
            if(!TryGetDialogGlobalLedIndex(controller, mappings[i].zone_idx, mappings[i].led_idx, &global_led_idx))
                continue;
            if(global_led_idx == (unsigned int)item_idx)
            {
                return true;
            }
        }
    }
    return false;
}

void CustomControllerDialog::LoadExistingController(const std::string& name,
                                                    int width,
                                                    int height,
                                                    int depth,
                                                    const std::vector<GridLEDMapping>& mappings,
                                                    float spacing_x_mm,
                                                    float spacing_y_mm,
                                                    float spacing_z_mm)
{
    setWindowTitle(tr("Edit Custom 3D Controller"));
    name_edit->setText(QString::fromStdString(name));

    const QSignalBlocker width_block(width_spin);
    const QSignalBlocker height_block(height_spin);
    const QSignalBlocker depth_block(depth_spin);
    width_spin->setValue(width);
    height_spin->setValue(height);
    depth_spin->setValue(depth);
    led_mappings = mappings;

    auto set_spacing_spin = [](QDoubleSpinBox* spin, double value_mm)
    {
        if(!spin)
        {
            return;
        }

        if(value_mm < spin->minimum())
        {
            value_mm = spin->minimum();
        }
        if(value_mm > spin->maximum())
        {
            value_mm = spin->maximum();
        }
        const QSignalBlocker spin_block(spin);
        spin->setValue(value_mm);
    };

    set_spacing_spin(spacing_x_spin, spacing_x_mm);
    set_spacing_spin(spacing_y_spin, spacing_y_mm);
    set_spacing_spin(spacing_z_spin, spacing_z_mm);

    InferMappingGranularity();
    UpdateGridDisplay();
    refreshDeviceList();
}

QColor CustomControllerDialog::GetItemColor(RGBController* controller, int granularity, int item_idx) const
{
    if(!controller) return QColor(128, 128, 128);

    if(granularity == 0)
    {
        return GetAverageDeviceColor(controller);
    }
    else if(granularity == 1)
    {
        if(item_idx >= 0 && item_idx < (int)controller->zones.size())
        {
            return GetAverageZoneColor(controller, item_idx);
        }
    }
    else if(granularity == 2)
    {
        if(item_idx >= 0 && item_idx < (int)controller->colors.size())
        {
            return RGBToQColor(controller->colors[item_idx]);
        }
    }
    return QColor(128, 128, 128);
}

QColor CustomControllerDialog::GetAverageZoneColor(RGBController* controller, unsigned int zone_idx) const
{
    if(zone_idx >= controller->zones.size()) return QColor(128, 128, 128);

    const zone& z = controller->zones[zone_idx];
    if(z.leds_count == 0) return QColor(128, 128, 128);

    unsigned int total_r = 0, total_g = 0, total_b = 0;
    unsigned int led_count = 0;

    for(unsigned int i = 0; i < z.leds_count && (z.start_idx + i) < controller->colors.size(); i++)
    {
        unsigned int color = controller->colors[z.start_idx + i];
        total_r += (color >> 0) & 0xFF;
        total_g += (color >> 8) & 0xFF;
        total_b += (color >> 16) & 0xFF;
        led_count++;
    }

    if(led_count == 0) return QColor(128, 128, 128);

    return QColor(static_cast<int>(total_r / led_count), static_cast<int>(total_g / led_count), static_cast<int>(total_b / led_count));
}

QColor CustomControllerDialog::GetAverageDeviceColor(RGBController* controller) const
{
    if(!controller || controller->colors.empty()) return QColor(128, 128, 128);

    unsigned long long total_r = 0, total_g = 0, total_b = 0;

    for(unsigned int i = 0; i < controller->colors.size(); i++)
    {
        total_r += (controller->colors[i] >> 0) & 0xFF;
        total_g += (controller->colors[i] >> 8) & 0xFF;
        total_b += (controller->colors[i] >> 16) & 0xFF;
    }

    size_t count = controller->colors.size();
    if(count == 0)
    {
        return QColor(0, 0, 0);
    }
    return QColor(static_cast<int>(total_r / count), static_cast<int>(total_g / count), static_cast<int>(total_b / count));
}

QColor CustomControllerDialog::GetMappingColor(const GridLEDMapping& mapping) const
{
    if(!mapping.controller)
        return QColor(128, 128, 128);

    if(mapping.zone_idx >= mapping.controller->zones.size())
        return QColor(128, 128, 128);

    const zone& z = mapping.controller->zones[mapping.zone_idx];
    unsigned int global_led_idx = z.start_idx + mapping.led_idx;

    if(global_led_idx >= mapping.controller->colors.size())
        return QColor(128, 128, 128);

    return RGBToQColor(mapping.controller->colors[global_led_idx]);
}

static bool MappingHasZoneLed(const GridLEDMapping& mapping)
{
    if(!mapping.controller || mapping.zone_idx >= mapping.controller->zones.size())
    {
        return false;
    }

    return mapping.led_idx < mapping.controller->zones[mapping.zone_idx].leds_count;
}

static unsigned int MappingDisplayLedNumber(const GridLEDMapping& mapping)
{
    return mapping.led_idx + 1;
}

QString CustomControllerDialog::GetMappingCellLabel(const std::vector<GridLEDMapping>& cell_mappings) const
{
    if(cell_mappings.empty())
    {
        return QString();
    }

    std::vector<unsigned int> display_numbers;
    display_numbers.reserve(cell_mappings.size());
    for(const GridLEDMapping& mapping : cell_mappings)
    {
        if(MappingHasZoneLed(mapping))
        {
            display_numbers.push_back(MappingDisplayLedNumber(mapping));
        }
    }

    if(display_numbers.empty())
    {
        return QString::number(cell_mappings.size());
    }

    std::sort(display_numbers.begin(), display_numbers.end());
    display_numbers.erase(std::unique(display_numbers.begin(), display_numbers.end()), display_numbers.end());

    if(display_numbers.size() == 1)
    {
        return QString::number(display_numbers.front());
    }

    if(display_numbers.size() <= 2)
    {
        QStringList parts;
        for(unsigned int number : display_numbers)
        {
            parts << QString::number(number);
        }
        return parts.join(QLatin1Char(','));
    }

    return QStringLiteral("%1+%2").arg(display_numbers.front()).arg(display_numbers.size() - 1);
}

QString CustomControllerDialog::GetMappingTooltip(const GridLEDMapping& mapping) const
{
    if(!mapping.controller)
    {
        return tr("Unknown device (not found on this system)");
    }

    const QString grid_pos = tr("Grid X=%1, Y=%2, Z=%3").arg(mapping.x).arg(mapping.y).arg(mapping.z);
    const QString device_name = ControllerDisplay::FormatRgbControllerTitle(mapping.controller);

    if(!MappingHasZoneLed(mapping))
    {
        if(mapping.granularity == 0)
        {
            return tr("%1\nWhole device\n%2").arg(device_name, grid_pos);
        }
        if(mapping.granularity == 1)
        {
            const QString zone_name = mapping.zone_idx < mapping.controller->zones.size()
                ? QString::fromStdString(mapping.controller->GetZoneName(mapping.zone_idx))
                : tr("Unknown zone");
            return tr("%1\nZone: %2\n%3").arg(device_name, zone_name, grid_pos);
        }
        return tr("%1\n%2").arg(device_name, grid_pos);
    }

    QString led_name = tr("Unknown LED");
    unsigned int global_led_idx = 0;
    const bool has_global = TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx)
                            && global_led_idx < mapping.controller->leds.size();
    if(has_global)
    {
        led_name = QString::fromStdString(mapping.controller->GetLEDName(global_led_idx));
    }

    const QString zone_name = mapping.zone_idx < mapping.controller->zones.size()
        ? QString::fromStdString(mapping.controller->GetZoneName(mapping.zone_idx))
        : tr("Unknown zone");
    const unsigned int display_led = MappingDisplayLedNumber(mapping);

    if(has_global)
    {
        return tr("%1\n%2\nZone LED %3 (global %4)\n%5\n%6")
            .arg(device_name, zone_name)
            .arg(display_led)
            .arg(global_led_idx)
            .arg(led_name, grid_pos);
    }

    return tr("%1\n%2\nZone LED %3\n%4\n%5")
        .arg(device_name, zone_name)
        .arg(display_led)
        .arg(led_name, grid_pos);
}

QString CustomControllerDialog::GetMappingDescription(const GridLEDMapping& mapping) const
{
    if(!mapping.controller)
    {
        return tr("Unknown device (not found on this system)");
    }

    const QString name = ControllerDisplay::FormatRgbControllerTitle(mapping.controller);
    if(MappingHasZoneLed(mapping))
    {
        QString led_name = tr("Unknown LED");
        unsigned int global_led_idx = 0;
        if(TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx) &&
           global_led_idx < mapping.controller->leds.size())
        {
            led_name = QString::fromStdString(mapping.controller->GetLEDName(global_led_idx));
        }

        if(TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx))
        {
            return tr("Assigned: %1, LED %3: %2 (global %4)")
                .arg(name, led_name)
                .arg(MappingDisplayLedNumber(mapping))
                .arg(global_led_idx);
        }
        return tr("Assigned: %1, LED %3: %2")
            .arg(name, led_name)
            .arg(MappingDisplayLedNumber(mapping));
    }

    if(mapping.granularity == 0)
    {
        return tr("Assigned: %1 (Whole Device)").arg(name);
    }
    if(mapping.granularity == 1)
    {
        const QString zone_name = mapping.zone_idx < mapping.controller->zones.size()
            ? QString::fromStdString(mapping.controller->GetZoneName(mapping.zone_idx))
            : tr("Unknown Zone");
        return tr("Assigned: %1, Zone: %2").arg(name, zone_name);
    }
    return tr("Assigned: %1").arg(name);
}

void CustomControllerDialog::InferMappingGranularity()
{
    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        if(led_mappings[i].granularity < 0 || led_mappings[i].granularity > 2)
        {
            led_mappings[i].granularity = 2;
        }
        if(MappingHasZoneLed(led_mappings[i]))
        {
            led_mappings[i].granularity = 2;
        }
    }
}

QColor CustomControllerDialog::RGBToQColor(unsigned int rgb_value)
{
    unsigned int r = (rgb_value >> 0) & 0xFF;
    unsigned int g = (rgb_value >> 8) & 0xFF;
    unsigned int b = (rgb_value >> 16) & 0xFF;
    return QColor(r, g, b);
}

ColorComboDelegate::ColorComboDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void ColorComboDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    if(index.data(Qt::DecorationRole).canConvert<QIcon>())
    {
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect icon_rect = option.rect;
        icon_rect.setWidth(20);
        icon_rect.adjust(4, 2, 0, -2);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        QPixmap pixmap = icon.pixmap(icon_rect.size());
        painter->drawPixmap(icon_rect, pixmap);

        painter->restore();
    }
}

QSize ColorComboDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), 24));
    return size;
}

void CustomControllerDialog::UpdateGridColors()
{
    if(!layout_grid)
    {
        return;
    }

    if(!LayoutCellsCacheMatchesGrid())
    {
        RefreshLayoutGridVisuals();
    }
    else
    {
        const LayerCellIndex layer_index = BuildLayerCellIndex(led_mappings, current_layer);
        const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
        bool changed = false;

        auto update_cell = [&](int col, int row)
        {
            const int cache_index = row * layout_cells_cache_w_ + col;
            if(cache_index < 0 || cache_index >= layout_cells_cache_.size())
            {
                return;
            }

            std::vector<GridLEDMapping> cell_mappings;
            const auto cell_it = layer_index.find(GridCellKey(col, row));
            if(cell_it != layer_index.end())
            {
                cell_mappings.reserve(cell_it->second.size());
                for(size_t mapping_index : cell_it->second)
                {
                    cell_mappings.push_back(led_mappings[mapping_index]);
                }
            }

            CustomControllerGridCellVisual& visual = layout_cells_cache_[cache_index];
            const bool is_selected = selected_cells.count(std::make_pair(col, row)) > 0;
            const bool is_hole     = cell_mappings.empty() && IsMatrixHoleCell(col, row);
            const QColor base_color = ComputeCellBaseColor(cell_mappings, is_hole);

            const QColor previous_fill = visual.fill;
            const QColor previous_text = visual.text;
            ApplyCellFillAndText(visual, base_color, cell_mappings, is_selected);

            if(visual.fill != previous_fill || visual.text != previous_text)
            {
                changed = true;
            }
        };

        for(const std::pair<const uint64_t, std::vector<size_t>>& entry : layer_index)
        {
            const int col = static_cast<int>(entry.first >> 32);
            const int row = static_cast<int>(static_cast<uint32_t>(entry.first));
            update_cell(col, row);
        }

        for(const std::pair<int, int>& cell : selected_cells)
        {
            update_cell(cell.first, cell.second);
        }

        if(changed)
        {
            layout_grid->SetCells(layout_cells_cache_);
        }
    }

    if(preview_dialog && preview_dialog->isVisible())
    {
        preview_dialog->RefreshPreviewColors();
    }
}

void CustomControllerDialog::on_layout_params_changed()
{
    syncPreviewLayoutIfVisible();
}

void CustomControllerDialog::syncPreviewLayoutIfVisible()
{
    if(preview_dialog && preview_dialog->isVisible())
    {
        preview_dialog->RefreshPreviewFromEditor();
    }
}

void CustomControllerDialog::refresh_colors()
{
    if(shutting_down_ || !isVisible())
    {
        return;
    }

    UpdateGridColors();
}

void CustomControllerDialog::on_show_preview_3d_clicked()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Nothing to preview"), tr("Assign at least one LED to the grid first."));
        return;
    }

    if(!preview_dialog)
    {
        preview_dialog = new CustomControllerPreviewDialog(this);
    }

    preview_dialog->UpdatePreview(
        this,
        GetControllerName().trimmed().toStdString(),
        GetGridWidth(),
        GetGridHeight(),
        GetGridDepth(),
        GetSpacingX(),
        GetSpacingY(),
        GetSpacingZ(),
        layout_grid_scale_mm,
        GetLEDMappings());

    preview_dialog->show();
    preview_dialog->raise();
    preview_dialog->activateWindow();
}

void CustomControllerDialog::WarnIfMappingCollisions() const
{
    const std::vector<GridLEDMapping>& mappings = led_mappings;
    std::map<std::tuple<int, int, int>, int> cell_counts;

    for(const GridLEDMapping& m : mappings)
    {
        cell_counts[std::make_tuple(m.x, m.y, m.z)]++;
    }

    int collision_cells = 0;
    for(const auto& entry : cell_counts)
    {
        if(entry.second > 1)
        {
            collision_cells++;
        }
    }

    if(collision_cells > 0)
    {
        QMessageBox::warning(
            const_cast<CustomControllerDialog*>(this),
            tr("Overlapping assignments"),
            tr("%1 grid cell(s) have more than one LED mapped. Effects may blend colors in those cells.")
            .arg(collision_cells));
    }
}

void CustomControllerDialog::on_rotate_grid_90()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("No LEDs to rotate."));
        return;
    }

    const int width = width_spin->value();
    const int height = height_spin->value();

    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        const int old_x = led_mappings[i].x;
        const int old_y = led_mappings[i].y;
        led_mappings[i].x = old_y;
        led_mappings[i].y = width - 1 - old_x;
    }

    width_spin->blockSignals(true);
    height_spin->blockSignals(true);
    width_spin->setValue(height);
    height_spin->setValue(width);
    width_spin->blockSignals(false);
    height_spin->blockSignals(false);

    matrix_hole_cells.clear();
    UpdateGridDisplay();
    WarnIfMappingCollisions();
}

void CustomControllerDialog::on_rotate_grid_180()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("No LEDs to rotate."));
        return;
    }

    const int width = width_spin->value();
    const int height = height_spin->value();

    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        led_mappings[i].x = width - 1 - led_mappings[i].x;
        led_mappings[i].y = height - 1 - led_mappings[i].y;
    }

    matrix_hole_cells.clear();
    UpdateGridDisplay();
    WarnIfMappingCollisions();
}

void CustomControllerDialog::on_rotate_grid_270()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("No LEDs to rotate."));
        return;
    }

    const int width = width_spin->value();
    const int height = height_spin->value();

    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        const int old_x = led_mappings[i].x;
        const int old_y = led_mappings[i].y;
        led_mappings[i].x = height - 1 - old_y;
        led_mappings[i].y = old_x;
    }

    width_spin->blockSignals(true);
    height_spin->blockSignals(true);
    width_spin->setValue(height);
    height_spin->setValue(width);
    width_spin->blockSignals(false);
    height_spin->blockSignals(false);

    matrix_hole_cells.clear();
    UpdateGridDisplay();
    WarnIfMappingCollisions();
}

void CustomControllerDialog::on_flip_grid_horizontal()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("No LEDs to flip."));
        return;
    }

    const int width = width_spin->value();
    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        led_mappings[i].x = width - 1 - led_mappings[i].x;
    }

    UpdateGridDisplay();
    WarnIfMappingCollisions();
}

void CustomControllerDialog::on_flip_grid_vertical()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("No LEDs to flip."));
        return;
    }

    const int height = height_spin->value();
    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        led_mappings[i].y = height - 1 - led_mappings[i].y;
    }

    UpdateGridDisplay();
    WarnIfMappingCollisions();
}

