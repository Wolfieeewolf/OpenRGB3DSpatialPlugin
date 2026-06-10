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
#include <climits>
#include <cmath>

namespace
{
constexpr int PREVIEW_GRID_MIN_CELLS = 6;

int PreviewGridPadding(int longest_extent)
{
    return std::max(5, (longest_extent + 1) / 2);
}

int ComputePreviewGridExtent(int grid_w, int grid_h, int grid_d, const std::vector<GridLEDMapping>& mappings)
{
    int longest = std::max({std::max(grid_w, 1), std::max(grid_h, 1), std::max(grid_d, 1)});

    if(!mappings.empty())
    {
        int min_x = INT_MAX;
        int max_x = INT_MIN;
        int min_y = INT_MAX;
        int max_y = INT_MIN;
        int min_z = INT_MAX;
        int max_z = INT_MIN;

        for(const GridLEDMapping& mapping : mappings)
        {
            if(!mapping.controller)
            {
                continue;
            }

            min_x = std::min(min_x, mapping.x);
            max_x = std::max(max_x, mapping.x);
            min_y = std::min(min_y, mapping.y);
            max_y = std::max(max_y, mapping.y);
            min_z = std::min(min_z, mapping.z);
            max_z = std::max(max_z, mapping.z);
        }

        if(min_x != INT_MAX)
        {
            const int span_x = max_x - min_x + 1;
            const int span_y = max_y - min_y + 1;
            const int span_z = max_z - min_z + 1;
            longest = std::max({longest, span_x, span_y, span_z});
        }
    }

    return std::max(PREVIEW_GRID_MIN_CELLS, longest + PreviewGridPadding(longest));
}

bool ComputeLocalPositionBounds(const std::vector<LEDPosition3D>& positions,
                                float* min_x,
                                float* max_x,
                                float* min_y,
                                float* max_y,
                                float* min_z,
                                float* max_z)
{
    if(!min_x || !max_x || !min_y || !max_y || !min_z || !max_z || positions.empty())
    {
        return false;
    }

    *min_x = *max_x = positions[0].local_position.x;
    *min_y = *max_y = positions[0].local_position.y;
    *min_z = *max_z = positions[0].local_position.z;

    for(unsigned int i = 1; i < positions.size(); i++)
    {
        const Vector3D& p = positions[i].local_position;
        *min_x = std::min(*min_x, p.x);
        *max_x = std::max(*max_x, p.x);
        *min_y = std::min(*min_y, p.y);
        *max_y = std::max(*max_y, p.y);
        *min_z = std::min(*min_z, p.z);
        *max_z = std::max(*max_z, p.z);
    }

    return true;
}

Vector3D ComputePreviewGridCenter(int preview_extent, float scale_x, float scale_y, float scale_z)
{
    const float half = static_cast<float>(preview_extent) * 0.5f;
    return {half * scale_x, half * scale_y, half * scale_z};
}

void ClampTransformPositionToPreviewGrid(ControllerTransform* transform,
                                         int preview_extent,
                                         float scale_x,
                                         float scale_y,
                                         float scale_z)
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

    const float pad = 0.25f;
    const float grid_min_x = 0.0f;
    const float grid_min_y = 0.0f;
    const float grid_min_z = 0.0f;
    const float grid_max_x = static_cast<float>(preview_extent) * scale_x;
    const float grid_max_y = static_cast<float>(preview_extent) * scale_y;
    const float grid_max_z = static_cast<float>(preview_extent) * scale_z;

    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;

    if(min_x < grid_min_x + pad)
    {
        dx = (grid_min_x + pad) - min_x;
    }
    else if(max_x > grid_max_x - pad)
    {
        dx = (grid_max_x - pad) - max_x;
    }

    if(min_y < grid_min_y + pad)
    {
        dy = (grid_min_y + pad) - min_y;
    }
    else if(max_y > grid_max_y - pad)
    {
        dy = (grid_max_y - pad) - max_y;
    }

    if(min_z < grid_min_z + pad)
    {
        dz = (grid_min_z + pad) - min_z;
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
                                int preview_extent,
                                float scale_x,
                                float scale_y,
                                float scale_z)
{
    if(!transform)
    {
        return;
    }

    const Vector3D grid_center = ComputePreviewGridCenter(preview_extent, scale_x, scale_y, scale_z);

    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
    if(!ComputeLocalPositionBounds(transform->led_positions, &min_x, &max_x, &min_y, &max_y, &min_z, &max_z))
    {
        transform->transform.position = grid_center;
        return;
    }

    const float content_cx = (min_x + max_x) * 0.5f;
    const float content_cy = (min_y + max_y) * 0.5f;
    const float content_cz = (min_z + max_z) * 0.5f;
    transform->transform.position.x = grid_center.x - content_cx;
    transform->transform.position.y = grid_center.y - content_cy;
    transform->transform.position.z = grid_center.z - content_cz;
}
}

CustomControllerPreviewDialog::CustomControllerPreviewDialog(QWidget* parent)
    : QDialog(parent),
      preview_grid_extent(PREVIEW_GRID_MIN_CELLS),
      preview_scale_x(1.0f),
      preview_scale_y(1.0f),
      preview_scale_z(1.0f),
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
    ViewportGLFormat::ApplyToWidget(viewport, ViewportGLFormat::Backend::LegacyFixedFunction);
    viewport->setFocusPolicy(Qt::StrongFocus);
    viewport->installViewportKeyboardShortcuts(this);
    viewport->SetShowRoomGuideLabels(false);
    viewport->SetShowRoomGridOverlay(false);
    viewport->SetReferencePoints(nullptr);
    viewport->SetDisplayPlanes(nullptr);
    viewport->SetPreferGpuScene(false);
    viewport->SetPreferGpuLabelOverlay(false);
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
    virtual_controller = std::make_unique<VirtualController3D>(
        display_name, grid_w, grid_h, grid_d, mappings, spacing_x_mm, spacing_y_mm, spacing_z_mm);

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

    preview_scale_x = (spacing_x_mm > 0.001f) ? MMToGridUnits(spacing_x_mm, grid_scale_mm) : 1.0f;
    preview_scale_y = (spacing_y_mm > 0.001f) ? MMToGridUnits(spacing_y_mm, grid_scale_mm) : 1.0f;
    preview_scale_z = (spacing_z_mm > 0.001f) ? MMToGridUnits(spacing_z_mm, grid_scale_mm) : 1.0f;
    preview_grid_extent = ComputePreviewGridExtent(grid_w, grid_h, grid_d, mappings);

    if(recenter_transform || !had_transform)
    {
        CenterTransformOnPreviewGrid(transform.get(), preview_grid_extent, preview_scale_x, preview_scale_y, preview_scale_z);
    }
    else
    {
        ClampTransformPositionToPreviewGrid(transform.get(), preview_grid_extent, preview_scale_x, preview_scale_y,
                                            preview_scale_z);
    }

    ControllerLayout3D::MarkWorldPositionsDirty(transform.get());
    ControllerLayout3D::UpdateWorldPositions(transform.get());

    transforms.push_back(std::move(transform));

    viewport->SetGridDimensions(preview_grid_extent, preview_grid_extent, preview_grid_extent);
    viewport->SetGridScaleMM(grid_scale_mm);
    viewport->SetGridSnapEnabled(false);
    viewport->SetControllerTransforms(&transforms);
    viewport->SelectController(0);
    viewport->UpdateGizmoPosition();
    RefreshPreviewColors();

    if(recenter_transform || !had_transform)
    {
        FrameCameraOnLayout();
    }

    ui->hintLabel->setText(tr("%1 · %2 LED(s) · %3×%3×%3 preview grid · G gizmo · Drag to move · Orbit · Right pan · Wheel zoom")
                            .arg(QString::fromStdString(display_name))
                            .arg(static_cast<int>(mappings.size()))
                            .arg(preview_grid_extent));
}

void CustomControllerPreviewDialog::ClampTransformToPreviewGrid()
{
    if(transforms.empty() || !transforms[0])
    {
        return;
    }

    ClampTransformPositionToPreviewGrid(transforms[0].get(), preview_grid_extent, preview_scale_x, preview_scale_y,
                                        preview_scale_z);
}

void CustomControllerPreviewDialog::FrameCameraOnLayout()
{
    const Vector3D grid_center = ComputePreviewGridCenter(
        preview_grid_extent, preview_scale_x, preview_scale_y, preview_scale_z);

    float extent_units = static_cast<float>(preview_grid_extent)
                         * std::max({preview_scale_x, preview_scale_y, preview_scale_z, 1.0f});

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
