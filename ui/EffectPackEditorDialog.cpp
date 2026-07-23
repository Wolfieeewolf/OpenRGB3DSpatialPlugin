// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackEditorDialog.h"
#include "EffectPackTimelineWidget.h"
#include "EffectPacks/EffectPackApplier.h"
#include "LEDPosition3D.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "VirtualController3D.h"

#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <system_error>

namespace
{

constexpr int kRoleKind = Qt::UserRole;
constexpr int kRoleDevice = Qt::UserRole + 1;
constexpr int kRoleZone = Qt::UserRole + 2;
constexpr int kRoleLed = Qt::UserRole + 3;

QColor RgbToQColor(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

RGBColor QColorToRgb(const QColor& c)
{
    return ToRGBColor(c.red(), c.green(), c.blue());
}

QString ControllerLabel(const ControllerTransform* transform, int index)
{
    if(!transform)
    {
        return QStringLiteral("Controller %1").arg(index);
    }
    if(transform->virtual_controller)
    {
        return QString::fromStdString(transform->virtual_controller->GetName());
    }
    if(transform->controller)
    {
        const std::string display = transform->controller->GetDisplayName();
        if(!display.empty())
        {
            return QString::fromStdString(display);
        }
        return QString::fromStdString(transform->controller->GetName());
    }
    return QStringLiteral("Controller %1").arg(index);
}

std::string ControllerKeyName(const ControllerTransform* transform, int index)
{
    if(transform && transform->virtual_controller)
    {
        return transform->virtual_controller->GetName();
    }
    if(transform && transform->controller)
    {
        const std::string display = transform->controller->GetDisplayName();
        if(!display.empty())
        {
            return display;
        }
        return transform->controller->GetName();
    }
    return std::string("controller_") + std::to_string(index);
}

} // namespace

EffectPackEditorDialog::EffectPackEditorDialog(OpenRGB3DSpatialTab* tab, QWidget* parent)
    : QDialog(parent)
    , tab_(tab)
{
    setWindowTitle(QStringLiteral("Effect Pack Editor"));
    setWindowFlags(windowFlags() | Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(1100, 640);
    buildUi();

    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &EffectPackEditorDialog::onTick);
}

EffectPackEditorDialog::~EffectPackEditorDialog()
{
    stopPreview();
}

void EffectPackEditorDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);

    auto* meta_row = new QHBoxLayout();
    name_edit_ = new QLineEdit();
    duration_spin_ = new QSpinBox();
    duration_spin_->setRange(100, EffectPack::kMaxDurationMs);
    duration_spin_->setSingleStep(100);
    duration_spin_->setValue(5000);
    loop_combo_ = new QComboBox();
    loop_combo_->addItem(QStringLiteral("Once"), QStringLiteral("once"));
    loop_combo_->addItem(QStringLiteral("Forever"), QStringLiteral("forever"));
    loop_combo_->addItem(QStringLiteral("While active"), QStringLiteral("while_active"));
    meta_row->addWidget(new QLabel(QStringLiteral("Name")));
    meta_row->addWidget(name_edit_, 2);
    meta_row->addWidget(new QLabel(QStringLiteral("Duration ms")));
    meta_row->addWidget(duration_spin_);
    meta_row->addWidget(new QLabel(QStringLiteral("Loop")));
    meta_row->addWidget(loop_combo_);
    root->addLayout(meta_row);

    auto* splitter = new QSplitter(Qt::Horizontal);

    scene_tree_ = new QTreeWidget();
    scene_tree_->setHeaderLabel(QStringLiteral("Models"));
    scene_tree_->setMinimumWidth(200);
    scene_tree_->setUniformRowHeights(true);
    splitter->addWidget(scene_tree_);

    auto* timeline_scroll = new QScrollArea();
    timeline_scroll->setWidgetResizable(true);
    timeline_scroll->setFrameShape(QFrame::NoFrame);
    timeline_ = new EffectPackTimelineWidget();
    timeline_scroll->setWidget(timeline_);
    splitter->addWidget(timeline_scroll);

    auto* props_box = new QGroupBox(QStringLiteral("Selected block"));
    auto* props_form = new QFormLayout(props_box);
    start_spin_ = new QSpinBox();
    start_spin_->setRange(0, EffectPack::kMaxDurationMs);
    end_spin_ = new QSpinBox();
    end_spin_->setRange(1, EffectPack::kMaxDurationMs);
    color_button_ = new QPushButton(QStringLiteral("Pick…"));
    color_to_button_ = new QPushButton(QStringLiteral("Pick…"));
    color_to_label_ = new QLabel(QStringLiteral("Color to (fade)"));
    period_spin_ = new QSpinBox();
    period_spin_->setRange(50, EffectPack::kMaxDurationMs);
    period_spin_->setValue(800);
    period_label_ = new QLabel(QStringLiteral("Pulse period ms"));
    intensity_spin_ = new QSpinBox();
    intensity_spin_->setRange(1, 100);
    intensity_spin_->setValue(100);
    props_form->addRow(QStringLiteral("Start ms"), start_spin_);
    props_form->addRow(QStringLiteral("End ms"), end_spin_);
    props_form->addRow(QStringLiteral("Color"), color_button_);
    props_form->addRow(color_to_label_, color_to_button_);
    props_form->addRow(period_label_, period_spin_);
    props_form->addRow(QStringLiteral("Intensity %"), intensity_spin_);

    auto* block_btns = new QHBoxLayout();
    auto* add_solid = new QPushButton(QStringLiteral("Solid"));
    auto* add_fade = new QPushButton(QStringLiteral("Fade"));
    auto* add_pulse = new QPushButton(QStringLiteral("Pulse"));
    auto* remove_block = new QPushButton(QStringLiteral("Remove"));
    block_btns->addWidget(add_solid);
    block_btns->addWidget(add_fade);
    block_btns->addWidget(add_pulse);
    block_btns->addWidget(remove_block);
    auto* props_wrap = new QWidget();
    auto* props_layout = new QVBoxLayout(props_wrap);
    props_layout->setContentsMargins(0, 0, 0, 0);
    props_layout->addWidget(props_box);
    props_layout->addLayout(block_btns);
    props_layout->addStretch(1);
    props_wrap->setMinimumWidth(220);
    splitter->addWidget(props_wrap);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    root->addWidget(splitter, 1);

    status_label_ = new QLabel(QStringLiteral("Idle"));
    PluginUiApplyMutedSecondaryLabel(status_label_);
    root->addWidget(status_label_);

    auto* action_row = new QHBoxLayout();
    preview_button_ = new QPushButton(QStringLiteral("Preview"));
    stop_button_ = new QPushButton(QStringLiteral("Stop"));
    stop_button_->setEnabled(false);
    save_button_ = new QPushButton(QStringLiteral("Save"));
    auto* close_button = new QPushButton(QStringLiteral("Close"));
    action_row->addWidget(preview_button_);
    action_row->addWidget(stop_button_);
    action_row->addStretch(1);
    action_row->addWidget(save_button_);
    action_row->addWidget(close_button);
    root->addLayout(action_row);

    setColorButton(color_button_, ToRGBColor(255, 0, 0));
    setColorButton(color_to_button_, ToRGBColor(0, 128, 255));

    connect(duration_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onDurationChanged);
    connect(scene_tree_, &QTreeWidget::itemExpanded, this, &EffectPackEditorDialog::onTreeExpandedOrCollapsed);
    connect(scene_tree_, &QTreeWidget::itemCollapsed, this, &EffectPackEditorDialog::onTreeExpandedOrCollapsed);
    connect(timeline_, &EffectPackTimelineWidget::playheadChanged, this, &EffectPackEditorDialog::onPlayheadChanged);
    connect(timeline_, &EffectPackTimelineWidget::blockSelected, this, &EffectPackEditorDialog::onBlockSelected);
    connect(timeline_, &EffectPackTimelineWidget::emptyCellClicked, this, &EffectPackEditorDialog::onEmptyCellClicked);
    connect(add_solid, &QPushButton::clicked, this, &EffectPackEditorDialog::onAddSolid);
    connect(add_fade, &QPushButton::clicked, this, &EffectPackEditorDialog::onAddFade);
    connect(add_pulse, &QPushButton::clicked, this, &EffectPackEditorDialog::onAddPulse);
    connect(remove_block, &QPushButton::clicked, this, &EffectPackEditorDialog::onRemoveBlock);
    connect(start_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(end_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(period_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(intensity_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(color_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onPickColor);
    connect(color_to_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onPickColorTo);
    connect(preview_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onPreview);
    connect(stop_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::stopPreview);
    connect(save_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onSave);
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);
}

void EffectPackEditorDialog::NewPack(const filesystem::path& packs_dir)
{
    packs_dir_ = packs_dir;
    pack_path_.clear();
    pack_ = EffectPack::Pack();
    pack_.id = "new_pack";
    pack_.name = "New pack";
    pack_.duration_ms = 5000;
    pack_.loop = EffectPack::LoopMode::Once;
    pack_.priority = 10;
    EffectPack::Track track;
    track.name = "All LEDs";
    track.target.kind = EffectPack::TargetKind::All;
    EffectPack::Block block;
    block.type = EffectPack::BlockType::Solid;
    block.start_ms = 0;
    block.end_ms = 1000;
    block.color = ToRGBColor(255, 64, 0);
    block.intensity = 1.0f;
    track.blocks.push_back(block);
    pack_.tracks.push_back(std::move(track));
    loadIntoUi(pack_);
    setWindowTitle(QStringLiteral("Effect Pack Editor — New"));
    status_label_->setText(QStringLiteral("Click empty timeline cells to place blocks on device/zone/LED rows"));
    show();
    raise();
    activateWindow();
}

void EffectPackEditorDialog::EditPack(const filesystem::path& path)
{
    EffectPack::Pack pack;
    std::string err;
    if(!EffectPack::LoadFromFile(path, &pack, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Failed to load:\n%1").arg(QString::fromStdString(err)));
        return;
    }
    packs_dir_ = path.parent_path();
    pack_path_ = path;
    pack_ = std::move(pack);
    loadIntoUi(pack_);
    setWindowTitle(QStringLiteral("Effect Pack Editor — %1").arg(QString::fromStdString(pack_.name)));
    status_label_->setText(QStringLiteral("Editing %1").arg(QString::fromStdString(path.filename().string())));
    show();
    raise();
    activateWindow();
}

void EffectPackEditorDialog::loadIntoUi(const EffectPack::Pack& pack)
{
    suppress_ui_ = true;
    name_edit_->setText(QString::fromStdString(pack.name));
    duration_spin_->setValue(pack.duration_ms);
    const QString loop = (pack.loop == EffectPack::LoopMode::Forever) ? QStringLiteral("forever")
        : (pack.loop == EffectPack::LoopMode::WhileActive) ? QStringLiteral("while_active")
        : QStringLiteral("once");
    const int loop_idx = loop_combo_->findData(loop);
    loop_combo_->setCurrentIndex(loop_idx >= 0 ? loop_idx : 0);
    selected_track_ = -1;
    selected_block_ = -1;
    suppress_ui_ = false;
    timeline_->setPack(&pack_);
    timeline_->setDurationMs(pack_.duration_ms);
    timeline_->setPlayheadMs(0);
    refreshSceneTree();
    onRebuildTimelineRows();
    applyBlockToForm();
}

void EffectPackEditorDialog::refreshSceneTree()
{
    scene_tree_->clear();
    auto* all_item = new QTreeWidgetItem(QStringList{QStringLiteral("All LEDs")});
    all_item->setData(0, kRoleKind, (int)EffectPack::TargetKind::All);
    scene_tree_->addTopLevelItem(all_item);

    if(!tab_)
    {
        return;
    }
    const auto& transforms = tab_->GetControllerTransforms();
    for(int i = 0; i < (int)transforms.size(); ++i)
    {
        ControllerTransform* transform = transforms[(size_t)i].get();
        if(!transform || transform->hidden_by_virtual)
        {
            continue;
        }
        const QString label = ControllerLabel(transform, i);
        const std::string key = ControllerKeyName(transform, i);
        auto* ctrl_item = new QTreeWidgetItem(QStringList{label});
        ctrl_item->setData(0, kRoleKind, (int)EffectPack::TargetKind::Device);
        ctrl_item->setData(0, kRoleDevice, QString::fromStdString(key));
        ctrl_item->setData(0, kRoleLed, -1);

        RGBControllerInterface* rgb = transform->controller;
        if(!rgb && !transform->led_positions.empty())
        {
            rgb = transform->led_positions.front().controller;
        }
        if(rgb)
        {
            for(unsigned int z = 0; z < rgb->GetZoneCount(); ++z)
            {
                QString zone_name = QString::fromStdString(rgb->GetZoneDisplayName(z));
                if(zone_name.isEmpty())
                {
                    zone_name = QString::fromStdString(rgb->GetZoneName(z));
                }
                if(zone_name.isEmpty())
                {
                    zone_name = QStringLiteral("Zone %1").arg(z);
                }
                auto* zone_item = new QTreeWidgetItem(QStringList{zone_name});
                zone_item->setData(0, kRoleKind, (int)EffectPack::TargetKind::Zone);
                zone_item->setData(0, kRoleDevice, QString::fromStdString(key));
                zone_item->setData(0, kRoleZone, zone_name);
                zone_item->setData(0, kRoleLed, -1);

                const unsigned int zone_leds = rgb->GetZoneLEDsCount(z);
                const unsigned int start = rgb->GetZoneStartIndex(z);
                // Cap LED children so huge strips stay usable; still placeable via zone row.
                const unsigned int led_cap = std::min(zone_leds, 64u);
                for(unsigned int li = 0; li < led_cap; ++li)
                {
                    auto* led_item = new QTreeWidgetItem(QStringList{QStringLiteral("LED %1").arg(li)});
                    led_item->setData(0, kRoleKind, (int)EffectPack::TargetKind::Leds);
                    led_item->setData(0, kRoleDevice, QString::fromStdString(key));
                    led_item->setData(0, kRoleZone, zone_name);
                    led_item->setData(0, kRoleLed, (int)(start + li));
                    zone_item->addChild(led_item);
                }
                if(zone_leds > led_cap)
                {
                    auto* more = new QTreeWidgetItem(
                        QStringList{QStringLiteral("… %1 more LEDs (use zone row)").arg(zone_leds - led_cap)});
                    more->setDisabled(true);
                    zone_item->addChild(more);
                }
                ctrl_item->addChild(zone_item);
            }
        }
        scene_tree_->addTopLevelItem(ctrl_item);
    }
    scene_tree_->expandToDepth(0);
}

EffectPack::Target EffectPackEditorDialog::targetFromTreeItem(QTreeWidgetItem* item) const
{
    EffectPack::Target target;
    if(!item)
    {
        target.kind = EffectPack::TargetKind::All;
        return target;
    }
    target.kind = (EffectPack::TargetKind)item->data(0, kRoleKind).toInt();
    target.device_name = item->data(0, kRoleDevice).toString().toStdString();
    target.zone_name = item->data(0, kRoleZone).toString().toStdString();
    const int led = item->data(0, kRoleLed).toInt();
    if(target.kind == EffectPack::TargetKind::Leds && led >= 0)
    {
        target.led_indices = {led};
    }
    return target;
}

void EffectPackEditorDialog::onRebuildTimelineRows()
{
    QVector<EffectPackTimelineWidget::Row> rows;

    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* item) {
        if(!item || item->isDisabled())
        {
            return;
        }
        const int kind = item->data(0, kRoleKind).toInt();
        const bool is_target = kind == (int)EffectPack::TargetKind::All
            || kind == (int)EffectPack::TargetKind::Device
            || kind == (int)EffectPack::TargetKind::Zone
            || kind == (int)EffectPack::TargetKind::Leds;
        // Show All + device always; zone/LED only when that branch is expanded into view
        // (zone visible if parent expanded; LED if zone expanded).
        bool include = false;
        if(kind == (int)EffectPack::TargetKind::All || kind == (int)EffectPack::TargetKind::Device)
        {
            include = true;
        }
        else if(kind == (int)EffectPack::TargetKind::Zone)
        {
            include = item->parent() && item->parent()->isExpanded();
        }
        else if(kind == (int)EffectPack::TargetKind::Leds)
        {
            include = item->parent() && item->parent()->isExpanded();
        }
        if(include && is_target)
        {
            EffectPackTimelineWidget::Row row;
            row.label = item->text(0);
            row.target = targetFromTreeItem(item);
            rows.push_back(row);
        }
        for(int i = 0; i < item->childCount(); ++i)
        {
            walk(item->child(i));
        }
    };

    for(int i = 0; i < scene_tree_->topLevelItemCount(); ++i)
    {
        walk(scene_tree_->topLevelItem(i));
    }

    timeline_->setRows(rows);
}

void EffectPackEditorDialog::onTreeExpandedOrCollapsed()
{
    onRebuildTimelineRows();
}

void EffectPackEditorDialog::onDurationChanged(int value)
{
    if(suppress_ui_)
    {
        return;
    }
    pack_.duration_ms = value;
    timeline_->setDurationMs(value);
}

void EffectPackEditorDialog::onPlayheadChanged(int ms)
{
    timeline_->setPlayheadMs(ms);
}

int EffectPackEditorDialog::ensureTrackForTarget(const EffectPack::Target& target, const QString& label)
{
    for(int i = 0; i < (int)pack_.tracks.size(); ++i)
    {
        const EffectPack::Target& existing = pack_.tracks[(size_t)i].target;
        if(existing.kind == target.kind
           && existing.device_name == target.device_name
           && existing.zone_name == target.zone_name
           && existing.led_indices == target.led_indices)
        {
            return i;
        }
    }
    EffectPack::Track track;
    track.name = label.toStdString();
    track.target = target;
    pack_.tracks.push_back(std::move(track));
    return (int)pack_.tracks.size() - 1;
}

void EffectPackEditorDialog::addBlockAt(int row_index, int ms, EffectPack::BlockType type)
{
    const QVector<EffectPackTimelineWidget::Row>& built = timeline_->rows();
    if(row_index < 0 || row_index >= built.size())
    {
        // Fall back to All LEDs row 0 if present.
        if(built.isEmpty())
        {
            return;
        }
        row_index = 0;
    }

    const auto& row = built[row_index];
    const int track = ensureTrackForTarget(row.target, row.label);
    EffectPack::Block block;
    block.type = type;
    block.start_ms = std::clamp(ms, 0, std::max(0, pack_.duration_ms - 1));
    block.end_ms = std::min(pack_.duration_ms, block.start_ms + (type == EffectPack::BlockType::Fade ? 2000 : 1000));
    if(block.end_ms <= block.start_ms)
    {
        block.end_ms = block.start_ms + 1;
    }
    block.color = ToRGBColor(255, 0, 0);
    block.color_from = ToRGBColor(255, 0, 0);
    block.color_to = ToRGBColor(0, 128, 255);
    block.period_ms = 800;
    block.min_intensity = 0.15f;
    block.max_intensity = 1.0f;
    block.intensity = 1.0f;
    pack_.tracks[(size_t)track].blocks.push_back(block);
    selected_track_ = track;
    selected_block_ = (int)pack_.tracks[(size_t)track].blocks.size() - 1;
    timeline_->setPack(&pack_);
    timeline_->setSelectedBlock(selected_track_, selected_block_);
    applyBlockToForm();
    timeline_->update();
}

void EffectPackEditorDialog::onEmptyCellClicked(int row_index, int ms)
{
    addBlockAt(row_index, ms, EffectPack::BlockType::Solid);
}

void EffectPackEditorDialog::onBlockSelected(int track_index, int block_index)
{
    selected_track_ = track_index;
    selected_block_ = block_index;
    applyBlockToForm();
}

int EffectPackEditorDialog::currentTimelineRow() const
{
    int row = 0;
    if(QTreeWidgetItem* item = scene_tree_->currentItem())
    {
        const EffectPack::Target target = targetFromTreeItem(item);
        const auto& rows = timeline_->rows();
        for(int i = 0; i < rows.size(); ++i)
        {
            if(rows[i].target.kind == target.kind
               && rows[i].target.device_name == target.device_name
               && rows[i].target.zone_name == target.zone_name
               && rows[i].target.led_indices == target.led_indices)
            {
                return i;
            }
        }
    }
    return row;
}

void EffectPackEditorDialog::onAddSolid()
{
    addBlockAt(currentTimelineRow(), timeline_->playheadMs(), EffectPack::BlockType::Solid);
}

void EffectPackEditorDialog::onAddFade()
{
    addBlockAt(currentTimelineRow(), timeline_->playheadMs(), EffectPack::BlockType::Fade);
}

void EffectPackEditorDialog::onAddPulse()
{
    addBlockAt(currentTimelineRow(), timeline_->playheadMs(), EffectPack::BlockType::Pulse);
}

void EffectPackEditorDialog::onRemoveBlock()
{
    if(selected_track_ < 0 || selected_track_ >= (int)pack_.tracks.size())
    {
        return;
    }
    auto& blocks = pack_.tracks[(size_t)selected_track_].blocks;
    if(selected_block_ < 0 || selected_block_ >= (int)blocks.size())
    {
        return;
    }
    blocks.erase(blocks.begin() + selected_block_);
    selected_block_ = std::min(selected_block_, (int)blocks.size() - 1);
    if(blocks.empty())
    {
        selected_track_ = -1;
        selected_block_ = -1;
    }
    timeline_->setSelectedBlock(selected_track_, selected_block_);
    timeline_->update();
    applyBlockToForm();
}

void EffectPackEditorDialog::applyBlockToForm()
{
    suppress_ui_ = true;
    const bool ok = selected_track_ >= 0 && selected_track_ < (int)pack_.tracks.size()
        && selected_block_ >= 0
        && selected_block_ < (int)pack_.tracks[(size_t)selected_track_].blocks.size();
    start_spin_->setEnabled(ok);
    end_spin_->setEnabled(ok);
    color_button_->setEnabled(ok);
    intensity_spin_->setEnabled(ok);
    if(!ok)
    {
        color_to_button_->setEnabled(false);
        period_spin_->setEnabled(false);
        suppress_ui_ = false;
        return;
    }
    const EffectPack::Block& b = pack_.tracks[(size_t)selected_track_].blocks[(size_t)selected_block_];
    start_spin_->setValue(b.start_ms);
    end_spin_->setValue(b.end_ms);
    period_spin_->setValue(std::max(50, b.period_ms));
    intensity_spin_->setValue((int)std::lround(std::clamp(b.intensity, 0.0f, 1.0f) * 100.0f));
    setColorButton(color_button_, b.type == EffectPack::BlockType::Fade ? b.color_from : b.color);
    setColorButton(color_to_button_, b.color_to);
    const bool fade = b.type == EffectPack::BlockType::Fade;
    const bool pulse = b.type == EffectPack::BlockType::Pulse;
    color_to_label_->setEnabled(fade);
    color_to_button_->setEnabled(fade);
    period_label_->setEnabled(pulse);
    period_spin_->setEnabled(pulse);
    suppress_ui_ = false;
}

void EffectPackEditorDialog::applyFormToSelectedBlock()
{
    if(suppress_ui_)
    {
        return;
    }
    if(selected_track_ < 0 || selected_track_ >= (int)pack_.tracks.size())
    {
        return;
    }
    auto& blocks = pack_.tracks[(size_t)selected_track_].blocks;
    if(selected_block_ < 0 || selected_block_ >= (int)blocks.size())
    {
        return;
    }
    EffectPack::Block& b = blocks[(size_t)selected_block_];
    b.start_ms = start_spin_->value();
    b.end_ms = std::max(b.start_ms + 1, end_spin_->value());
    b.period_ms = period_spin_->value();
    b.intensity = intensity_spin_->value() / 100.0f;
    const RGBColor c = colorFromButton(color_button_);
    const RGBColor c2 = colorFromButton(color_to_button_);
    if(b.type == EffectPack::BlockType::Fade)
    {
        b.color_from = c;
        b.color_to = c2;
        b.color = c;
    }
    else
    {
        b.color = c;
        b.color_from = c;
        b.color_to = c2;
    }
    timeline_->update();
}

void EffectPackEditorDialog::onBlockFieldChanged()
{
    applyFormToSelectedBlock();
}

void EffectPackEditorDialog::setColorButton(QPushButton* button, RGBColor color)
{
    if(!button)
    {
        return;
    }
    const QColor qc = RgbToQColor(color);
    button->setProperty("rgbColor", (uint)color);
    button->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2;")
            .arg(qc.name())
            .arg(qc.lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black")));
    button->setText(qc.name().toUpper());
}

RGBColor EffectPackEditorDialog::colorFromButton(QPushButton* button) const
{
    if(!button)
    {
        return ToRGBColor(255, 0, 0);
    }
    return (RGBColor)button->property("rgbColor").toUInt();
}

void EffectPackEditorDialog::onPickColor()
{
    const QColor picked = QColorDialog::getColor(RgbToQColor(colorFromButton(color_button_)), this, QStringLiteral("Block color"));
    if(!picked.isValid())
    {
        return;
    }
    setColorButton(color_button_, QColorToRgb(picked));
    applyFormToSelectedBlock();
}

void EffectPackEditorDialog::onPickColorTo()
{
    const QColor picked = QColorDialog::getColor(RgbToQColor(colorFromButton(color_to_button_)), this, QStringLiteral("Fade end color"));
    if(!picked.isValid())
    {
        return;
    }
    setColorButton(color_to_button_, QColorToRgb(picked));
    applyFormToSelectedBlock();
}

QString EffectPackEditorDialog::sanitizeId(const QString& name) const
{
    QString out;
    for(QChar ch : name.toLower())
    {
        if(ch.isLetterOrNumber())
        {
            out.append(ch);
        }
        else if(ch.isSpace() || ch == '-' || ch == '_')
        {
            if(!out.isEmpty() && out.back() != '_')
            {
                out.append('_');
            }
        }
    }
    while(out.endsWith('_'))
    {
        out.chop(1);
    }
    return out.isEmpty() ? QStringLiteral("pack") : out;
}

void EffectPackEditorDialog::applyMetaToPack()
{
    pack_.name = name_edit_->text().trimmed().toStdString();
    if(pack_.name.empty())
    {
        pack_.name = "Untitled";
    }
    pack_.duration_ms = duration_spin_->value();
    const QString loop = loop_combo_->currentData().toString();
    if(loop == QStringLiteral("forever"))
    {
        pack_.loop = EffectPack::LoopMode::Forever;
    }
    else if(loop == QStringLiteral("while_active"))
    {
        pack_.loop = EffectPack::LoopMode::WhileActive;
    }
    else
    {
        pack_.loop = EffectPack::LoopMode::Once;
    }
    if(pack_path_.empty())
    {
        pack_.id = sanitizeId(QString::fromStdString(pack_.name)).toStdString();
    }
}

void EffectPackEditorDialog::onSave()
{
    stopPreview();
    applyFormToSelectedBlock();
    applyMetaToPack();
    if(pack_.tracks.empty())
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Add at least one block on the timeline before saving."));
        return;
    }
    bool any_blocks = false;
    for(const auto& t : pack_.tracks)
    {
        if(!t.blocks.empty())
        {
            any_blocks = true;
            break;
        }
    }
    if(!any_blocks)
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Add at least one block on the timeline before saving."));
        return;
    }
    if(packs_dir_.empty())
    {
        return;
    }
    std::error_code ec;
    filesystem::create_directories(packs_dir_, ec);
    if(pack_path_.empty())
    {
        pack_path_ = packs_dir_ / (pack_.id + EffectPack::kFileSuffix);
    }
    std::string err;
    if(!EffectPack::SaveToFile(pack_path_, pack_, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Save failed:\n%1").arg(QString::fromStdString(err)));
        return;
    }
    setWindowTitle(QStringLiteral("Effect Pack Editor — %1").arg(QString::fromStdString(pack_.name)));
    status_label_->setText(QStringLiteral("Saved %1").arg(QString::fromStdString(pack_path_.filename().string())));
#ifdef _WIN32
    emit packSaved(QString::fromStdWString(pack_path_.wstring()));
#else
    emit packSaved(QString::fromStdString(pack_path_.string()));
#endif
}

void EffectPackEditorDialog::setPlayingUi(bool playing)
{
    preview_button_->setEnabled(!playing);
    stop_button_->setEnabled(playing);
    save_button_->setEnabled(!playing);
}

void EffectPackEditorDialog::stopPreview()
{
    if(timer_ && timer_->isActive())
    {
        timer_->stop();
    }
    player_.Stop();
    setPlayingUi(false);
    emit previewStopped();
    if(status_label_)
    {
        status_label_->setText(QStringLiteral("Preview stopped"));
    }
}

void EffectPackEditorDialog::onPreview()
{
    if(!tab_)
    {
        return;
    }
    applyFormToSelectedBlock();
    applyMetaToPack();
    emit previewStarted();
    tab_->PrepareEffectPackPreview();
    if(tab_->resource_manager && tab_->resource_manager->GetRGBControllers().empty())
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("No OpenRGB controllers available."));
        return;
    }
    player_.SetPack(pack_);
    player_.Play();
    wall_.restart();
    last_elapsed_ms_ = 0;
    setPlayingUi(true);
    timer_->start();
    status_label_->setText(QStringLiteral("Previewing…"));
}

void EffectPackEditorDialog::onTick()
{
    if(!tab_ || !player_.IsPlaying())
    {
        stopPreview();
        return;
    }
    const int elapsed = (int)wall_.elapsed();
    const int dt = std::max(0, elapsed - last_elapsed_ms_);
    last_elapsed_ms_ = elapsed;
    if(!player_.Tick(dt, true))
    {
        stopPreview();
        status_label_->setText(QStringLiteral("Preview finished"));
        return;
    }
    tab_->ApplyEffectPackPreviewFrame(player_.GetPack(), player_.LocalMs());
    timeline_->setPlayheadMs(player_.LocalMs());
    status_label_->setText(
        QStringLiteral("Preview %1 / %2 ms")
            .arg(player_.LocalMs())
            .arg(std::max(1, player_.GetPack().duration_ms)));
}
