// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerPreviewDialog.h"
#include "CustomControllerDialog.h"
#include "ControllerLayout3D.h"
#include "LEDViewport3D.h"
#include "GridSpaceUtils.h"
#include "PluginUiUtils.h"
#include "VirtualController3D.h"
#include "ui_CustomControllerPreviewDialog.h"
#include "viewport/ViewportGLFormat.h"
#include <QPushButton>
#include <QShowEvent>
#include <QHideEvent>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace
{
constexpr float PREVIEW_GRID_PADDING_UNITS = 5.0f;
constexpr int   PREVIEW_GRID_MIN_UNITS     = 6;

struct PreviewContentBounds
{
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
    bool  valid = false;
};

void ExpandBoundsPoint(PreviewContentBounds* bounds, float x, float y, float z)
{
    if(!bounds)
    {
        return;
    }

    if(!bounds->valid)
    {
        bounds->min_x = bounds->max_x = x;
        bounds->min_y = bounds->max_y = y;
        bounds->min_z = bounds->max_z = z;
        bounds->valid = true;
        return;
    }

    bounds->min_x = std::min(bounds->min_x, x);
    bounds->max_x = std::max(bounds->max_x, x);
    bounds->min_y = std::min(bounds->min_y, y);
    bounds->max_y = std::max(bounds->max_y, y);
    bounds->min_z = std::min(bounds->min_z, z);
    bounds->max_z = std::max(bounds->max_z, z);
}

void ExpandBoundsBox(PreviewContentBounds* bounds, float x0, float y0, float z0, float x1, float y1, float z1)
{
    ExpandBoundsPoint(bounds, x0, y0, z0);
    ExpandBoundsPoint(bounds, x1, y1, z1);
}

PreviewContentBounds ComputePreviewContentBoundsLocal(const VirtualController3D* layout,
                                                      const std::vector<LEDPosition3D>& positions,
                                                      float grid_scale_mm)
{
    PreviewContentBounds bounds{};

    for(const LEDPosition3D& position : positions)
    {
        const Vector3D& p = position.local_position;
        ExpandBoundsPoint(&bounds, p.x, p.y, p.z);
    }

    if(layout)
    {
        for(const CustomControllerLightBlocker& blocker : layout->GetLightBlockers())
        {
            Vector3D local_min{};
            Vector3D local_max{};
            layout->CellLocalBoundsMm(blocker.x, blocker.y, blocker.z, &local_min, &local_max);
            ExpandBoundsBox(&bounds,
                            MMToGridUnits(local_min.x, grid_scale_mm),
                            MMToGridUnits(local_min.y, grid_scale_mm),
                            MMToGridUnits(local_min.z, grid_scale_mm),
                            MMToGridUnits(local_max.x, grid_scale_mm),
                            MMToGridUnits(local_max.y, grid_scale_mm),
                            MMToGridUnits(local_max.z, grid_scale_mm));
        }

        if(!bounds.valid)
        {
            float width_mm  = 0.0f;
            float height_mm = 0.0f;
            float depth_mm  = 0.0f;
            for(float value : layout->GetColumnWidthsMm())
            {
                width_mm += value;
            }
            for(float value : layout->GetRowHeightsMm())
            {
                height_mm += value;
            }
            for(float value : layout->GetLayerDepthsMm())
            {
                depth_mm += value;
            }

            if(width_mm > 0.0f || height_mm > 0.0f || depth_mm > 0.0f)
            {
                ExpandBoundsBox(&bounds,
                                0.0f,
                                0.0f,
                                0.0f,
                                MMToGridUnits(width_mm, grid_scale_mm),
                                MMToGridUnits(height_mm, grid_scale_mm),
                                MMToGridUnits(depth_mm, grid_scale_mm));
            }
        }
    }

    return bounds;
}

int PaddedGridUnits(float span_units, float padding_units)
{
    return std::max(PREVIEW_GRID_MIN_UNITS, static_cast<int>(std::ceil(span_units + 2.0f * padding_units)));
}

void ComputePreviewGridDimensions(const PreviewContentBounds& content,
                                  float padding_units,
                                  int* grid_x,
                                  int* grid_y,
                                  int* grid_z)
{
    if(!grid_x || !grid_y || !grid_z)
    {
        return;
    }

    if(!content.valid)
    {
        *grid_x = PREVIEW_GRID_MIN_UNITS;
        *grid_y = PREVIEW_GRID_MIN_UNITS;
        *grid_z = PREVIEW_GRID_MIN_UNITS;
        return;
    }

    *grid_x = PaddedGridUnits(content.max_x - content.min_x, padding_units);
    *grid_y = PaddedGridUnits(content.max_y - content.min_y, padding_units);
    *grid_z = PaddedGridUnits(content.max_z - content.min_z, padding_units);
}

Vector3D ComputePreviewGridCenter(int grid_x, int grid_y, int grid_z)
{
    return {static_cast<float>(grid_x) * 0.5f,
            static_cast<float>(grid_y) * 0.5f,
            static_cast<float>(grid_z) * 0.5f};
}

void ClampTransformPositionToPreviewGrid(ControllerTransform* transform, int grid_x, int grid_y, int grid_z)
{
    if(!transform || transform->led_positions.empty())
    {
        return;
    }

    ControllerLayout3D::MarkWorldPositionsDirty(transform);
    ControllerLayout3D::UpdateWorldPositions(transform);

    float min_x = transform->led_positions[0].world_position.x;
    float max_x = min_x;
    float min_y = transform->led_positions[0].world_position.y;
    float max_y = min_y;
    float min_z = transform->led_positions[0].world_position.z;
    float max_z = min_z;

    for(unsigned int i = 1; i < transform->led_positions.size(); i++)
    {
        const Vector3D& p = transform->led_positions[i].world_position;
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
        min_z = std::min(min_z, p.z);
        max_z = std::max(max_z, p.z);
    }

    const float pad        = PREVIEW_GRID_PADDING_UNITS;
    const float grid_max_x = static_cast<float>(grid_x);
    const float grid_max_y = static_cast<float>(grid_y);
    const float grid_max_z = static_cast<float>(grid_z);

    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;

    if(min_x < pad)
    {
        dx = pad - min_x;
    }
    else if(max_x > grid_max_x - pad)
    {
        dx = (grid_max_x - pad) - max_x;
    }

    if(min_y < pad)
    {
        dy = pad - min_y;
    }
    else if(max_y > grid_max_y - pad)
    {
        dy = (grid_max_y - pad) - max_y;
    }

    if(min_z < pad)
    {
        dz = pad - min_z;
    }
    else if(max_z > grid_max_z - pad)
    {
        dz = (grid_max_z - pad) - max_z;
    }

    if(dx != 0.0f || dy != 0.0f || dz != 0.0f)
    {
        transform->transform.position.x += dx;
        transform->transform.position.y += dy;
        transform->transform.position.z += dz;
        ControllerLayout3D::MarkWorldPositionsDirty(transform);
        ControllerLayout3D::UpdateWorldPositions(transform);
    }
}

void CenterTransformOnPreviewGrid(ControllerTransform* transform,
                                const PreviewContentBounds& content,
                                int grid_x,
                                int grid_y,
                                int grid_z)
{
    if(!transform)
    {
        return;
    }

    const Vector3D grid_center = ComputePreviewGridCenter(grid_x, grid_y, grid_z);
    if(!content.valid)
    {
        transform->transform.position = grid_center;
        return;
    }

    const float content_cx = (content.min_x + content.max_x) * 0.5f;
    const float content_cy = (content.min_y + content.max_y) * 0.5f;
    const float content_cz = (content.min_z + content.max_z) * 0.5f;
    transform->transform.position.x = grid_center.x - content_cx;
    transform->transform.position.y = grid_center.y - content_cy;
    transform->transform.position.z = grid_center.z - content_cz;
}
} // namespace

CustomControllerPreviewDialog::CustomControllerPreviewDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::CustomControllerPreviewDialog),
      source_editor(nullptr)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() | Qt::Tool);
    resize(640, 520);
    PluginUiApplyMutedSecondaryLabel(ui->hintLabel);
    ui->mainLayout->setStretch(1, 1);

    auto* viewport_layout = new QVBoxLayout(ui->viewportHost);
    viewport_layout->setContentsMargins(0, 0, 0, 0);
    viewport = new LEDViewport3D(ui->viewportHost);
    ViewportGLFormat::ApplyToWidget(viewport);
    viewport->setFocusPolicy(Qt::StrongFocus);
    viewport->installViewportKeyboardShortcuts(this);
    viewport->SetShowRoomGuideLabels(false);
    viewport->SetShowRoomGridOverlay(false);
    viewport->SetReferencePoints(nullptr);
    viewport->SetDisplayPlanes(nullptr);
    viewport->SetRoomDimensions(0.0f, 0.0f, 0.0f, false);
    viewport_layout->addWidget(viewport);

    connect(viewport, &LEDViewport3D::ControllerPositionChanged, this,
            [this](int, float x, float y, float z)
            {
                if(transforms.empty() || !transforms[0])
                {
                    return;
                }

                transforms[0]->transform.position.x = x;
                transforms[0]->transform.position.y = y;
                transforms[0]->transform.position.z = z;
                ClampTransformToPreviewGrid();
                viewport->UpdateGizmoPosition();
                viewport->update();
            });
    connect(viewport, &LEDViewport3D::ControllerRotationChanged, this,
            [this](int, float x, float y, float z)
            {
                if(transforms.empty() || !transforms[0])
                {
                    return;
                }

                transforms[0]->transform.rotation.x = x;
                transforms[0]->transform.rotation.y = y;
                transforms[0]->transform.rotation.z = z;
                ControllerLayout3D::MarkWorldPositionsDirty(transforms[0].get());
                ControllerLayout3D::UpdateWorldPositions(transforms[0].get());
                viewport->update();
            });

    connect(ui->refreshButton, &QPushButton::clicked, this, [this]()
    {
        if(!source_editor)
        {
            return;
        }
        UpdatePreview(
            source_editor,
            source_editor->GetControllerName().trimmed().toStdString(),
            source_editor->GetGridWidth(),
            source_editor->GetGridHeight(),
            source_editor->GetGridDepth(),
            source_editor->GetSpacingX(),
            source_editor->GetSpacingY(),
            source_editor->GetSpacingZ(),
            source_editor->GetLayoutGridScaleMm(),
            source_editor->GetLEDMappings(),
            false);
    });
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::close);

    color_timer = new QTimer(this);
    color_timer->setTimerType(Qt::CoarseTimer);
    color_timer->setInterval(200);
    connect(color_timer, &QTimer::timeout, this, &CustomControllerPreviewDialog::RefreshPreviewColors);
}

CustomControllerPreviewDialog::~CustomControllerPreviewDialog()
{
    if(color_timer)
    {
        color_timer->stop();
    }

    source_editor = nullptr;

    delete ui;
    ui = nullptr;
}

void CustomControllerPreviewDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if(viewport)
    {
        viewport->SetViewportPaintingEnabled(true);
        viewport->setFocus(Qt::OtherFocusReason);
        viewport->update();
    }
    if(color_timer && !color_timer->isActive())
    {
        color_timer->start();
    }
}

void CustomControllerPreviewDialog::hideEvent(QHideEvent* event)
{
    QDialog::hideEvent(event);
    if(viewport)
    {
        viewport->SetViewportPaintingEnabled(false);
    }
    if(color_timer)
    {
        color_timer->stop();
    }
}

void CustomControllerPreviewDialog::UpdatePreview(CustomControllerDialog* editor,
                                                  const std::string& name,
                                                  int grid_w,
                                                  int grid_h,
                                                  int grid_d,
                                                  float spacing_x_mm,
                                                  float spacing_y_mm,
                                                  float spacing_z_mm,
                                                  float grid_scale_mm,
                                                  const std::vector<GridLEDMapping>& mappings,
                                                  bool recenter_transform)
{
    source_editor = editor;
    RebuildPreviewScene(name, grid_w, grid_h, grid_d, spacing_x_mm, spacing_y_mm, spacing_z_mm, grid_scale_mm, mappings,
                        recenter_transform);
}

void CustomControllerPreviewDialog::RefreshPreviewColors()
{
    if(!viewport)
    {
        return;
    }

    viewport->UpdateColors();
    viewport->update();
}

void CustomControllerPreviewDialog::RefreshPreviewFromEditor()
{
    if(!source_editor || !viewport)
    {
        return;
    }

    RebuildPreviewScene(
        source_editor->GetControllerName().trimmed().toStdString(),
        source_editor->GetGridWidth(),
        source_editor->GetGridHeight(),
        source_editor->GetGridDepth(),
        source_editor->GetSpacingX(),
        source_editor->GetSpacingY(),
        source_editor->GetSpacingZ(),
        source_editor->GetLayoutGridScaleMm(),
        source_editor->GetLEDMappings(),
        false);
}

void CustomControllerPreviewDialog::RebuildPreviewScene(const std::string& name,
                                                         int grid_w,
                                                         int grid_h,
                                                         int grid_d,
                                                         float spacing_x_mm,
                                                         float spacing_y_mm,
                                                         float spacing_z_mm,
                                                         float grid_scale_mm,
                                                         const std::vector<GridLEDMapping>& mappings,
                                                         bool recenter_transform)
{
    Vector3D saved_position{0.0f, 0.0f, 0.0f};
    Rotation3D saved_rotation{0.0f, 0.0f, 0.0f};
    const bool had_transform = !transforms.empty() && transforms[0];
    if(had_transform)
    {
        saved_position = transforms[0]->transform.position;
        saved_rotation = transforms[0]->transform.rotation;
    }

    transforms.clear();
    virtual_controller.reset();

    const std::string display_name = name.empty() ? std::string("[Preview]") : name;
    std::vector<float> column_widths;
    std::vector<float> row_heights;
    std::vector<float> layer_depths;
    std::vector<std::string> layer_names;
    std::vector<CustomControllerLightBlocker> blockers;
    if(source_editor)
    {
        column_widths = source_editor->GetColumnWidthsMm();
        row_heights   = source_editor->GetRowHeightsMm();
        layer_depths  = source_editor->GetLayerDepthsMm();
        layer_names   = source_editor->GetLayerNames();
        blockers      = source_editor->GetLightBlockers();
    }
    virtual_controller = std::make_unique<VirtualController3D>(
        display_name, grid_w, grid_h, grid_d, mappings, spacing_x_mm, spacing_y_mm, spacing_z_mm, blockers,
        std::move(column_widths), std::move(row_heights), std::move(layer_depths), std::move(layer_names),
        source_editor ? source_editor->GetLedsPerCluster() : 1);

    std::unique_ptr<ControllerTransform> transform = std::make_unique<ControllerTransform>();
    transform->controller = nullptr;
    transform->virtual_controller = virtual_controller.get();
    transform->transform.position = had_transform ? saved_position : Vector3D{0.0f, 0.0f, 0.0f};
    transform->transform.rotation = had_transform ? saved_rotation : Rotation3D{0.0f, 0.0f, 0.0f};
    transform->transform.scale = {1.0f, 1.0f, 1.0f};
    transform->hidden_by_virtual = false;
    transform->led_spacing_mm_x = spacing_x_mm;
    transform->led_spacing_mm_y = spacing_y_mm;
    transform->led_spacing_mm_z = spacing_z_mm;
    transform->granularity = -1;
    transform->item_idx = -1;
    transform->display_color = 0x00FFFFFF;
    transform->led_positions = virtual_controller->GenerateLEDPositions(grid_scale_mm);

    const PreviewContentBounds content_bounds =
        ComputePreviewContentBoundsLocal(virtual_controller.get(), transform->led_positions, grid_scale_mm);
    ComputePreviewGridDimensions(content_bounds,
                                 PREVIEW_GRID_PADDING_UNITS,
                                 &preview_grid_x,
                                 &preview_grid_y,
                                 &preview_grid_z);

    if(recenter_transform || !had_transform)
    {
        CenterTransformOnPreviewGrid(transform.get(), content_bounds, preview_grid_x, preview_grid_y, preview_grid_z);
    }
    else
    {
        ClampTransformPositionToPreviewGrid(transform.get(), preview_grid_x, preview_grid_y, preview_grid_z);
    }

    ControllerLayout3D::MarkWorldPositionsDirty(transform.get());
    ControllerLayout3D::UpdateWorldPositions(transform.get());

    transforms.push_back(std::move(transform));

    viewport->SetGridDimensions(preview_grid_x, preview_grid_y, preview_grid_z);
    viewport->SetGridScaleMM(grid_scale_mm);
    viewport->SetGridSnapEnabled(false);
    viewport->SetControllerTransforms(&transforms);
    viewport->SelectController(0);
    viewport->UpdateGizmoPosition();
    RefreshPreviewColors();
    FrameCameraOnLayout();

    ui->hintLabel->setText(tr("%1 · %2 LED(s) · %3×%4×%5 preview grid · G gizmo · Drag to move · Orbit · Right pan · Wheel zoom")
                            .arg(QString::fromStdString(display_name))
                            .arg(static_cast<int>(mappings.size()))
                            .arg(preview_grid_x)
                            .arg(preview_grid_y)
                            .arg(preview_grid_z));
}

void CustomControllerPreviewDialog::ClampTransformToPreviewGrid()
{
    if(transforms.empty() || !transforms[0])
    {
        return;
    }

    ClampTransformPositionToPreviewGrid(transforms[0].get(), preview_grid_x, preview_grid_y, preview_grid_z);
}

void CustomControllerPreviewDialog::FrameCameraOnLayout()
{
    const Vector3D grid_center =
        ComputePreviewGridCenter(preview_grid_x, preview_grid_y, preview_grid_z);

    float extent_units = static_cast<float>(std::max({preview_grid_x, preview_grid_y, preview_grid_z, 1}));

    if(!transforms.empty() && transforms[0] && !transforms[0]->led_positions.empty())
    {
        const ControllerTransform* transform = transforms[0].get();
        float min_x = transform->led_positions[0].world_position.x;
        float max_x = min_x;
        float min_y = transform->led_positions[0].world_position.y;
        float max_y = min_y;
        float min_z = transform->led_positions[0].world_position.z;
        float max_z = min_z;

        for(unsigned int i = 1; i < transform->led_positions.size(); i++)
        {
            const Vector3D& p = transform->led_positions[i].world_position;
            min_x = std::min(min_x, p.x);
            max_x = std::max(max_x, p.x);
            min_y = std::min(min_y, p.y);
            max_y = std::max(max_y, p.y);
            min_z = std::min(min_z, p.z);
            max_z = std::max(max_z, p.z);
        }

        const float led_extent = std::max({max_x - min_x, max_y - min_y, max_z - min_z, 1.0f});
        extent_units = std::max(extent_units, led_extent);
    }

    const float distance = std::max(12.0f, extent_units * 2.0f);
    viewport->SetCamera(distance, 45.0f, 28.0f, grid_center.x, grid_center.y, grid_center.z);
}
