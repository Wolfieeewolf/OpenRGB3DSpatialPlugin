// SPDX-License-Identifier: GPL-2.0-only

#include "ShaderField.h"
#include "Shaders/SpatialShaderCatalog.h"
#include "Shaders/SpatialOffscreenGlPool.h"
#include "MediaTextureEffectUtils.h"

#include <QVBoxLayout>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTextStream>
#include <QFileInfo>
#include <QUrl>
#include "EffectUiRows.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ShaderField::ShaderField(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetSpeed(45);
    SetFrequency(25);
    shader_engine = new SpatialShaderEngine(this);
    shader_engine->setTargetFps(30);
    shader_engine->setRenderSize(160, 90);
    connect(shader_engine,
            &SpatialShaderEngine::frameReady,
            this,
            &ShaderField::OnFrameReady,
            Qt::QueuedConnection);
    connect(shader_engine,
            &SpatialShaderEngine::compileMessage,
            this,
            &ShaderField::OnCompileMessage,
            Qt::QueuedConnection);

    RebuildPresetList();
    if(!preset_paths.empty())
    {
        LoadPresetAtIndex(0);
    }
}

ShaderField::~ShaderField()
{
    if(shader_engine)
    {
        shader_engine->stop();
    }
}

void ShaderField::EnsureShaderEngineRunning()
{
    if(!SpatialOffscreenGlPool::hostContextReady())
    {
        return;
    }
    if(shader_engine && !shader_engine->isRunning())
    {
        shader_engine->start();
    }
}

EffectInfo3D ShaderField::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Shader Field";
    info.effect_description =
        "Projects a 2D GPU shader pattern onto your room (like wrapping wallpaper around the LEDs). "
        "Pick a Preset for the look, Projection for which plane it maps to. "
        "Speed animates the shader, Frequency scrolls hue, Size zooms, Detail densifies the pattern.";
    info.category = "Spatial";
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_frequency = true;
    info.default_speed_scale = 14.0f;
    info.default_frequency_scale = 10.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = false;
    info.supports_height_bands = false;
    info.supports_strip_colormap = false;
    return info;
}

void ShaderField::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    QLabel* help = new QLabel(
        QStringLiteral(
            "Shader Field paints a moving 2D pattern, then samples it onto LEDs.\n"
            "Presets are different patterns (waves vs plasma vs checker vs ember rings).\n"
            "Add your own .fs files in the user shaders folder if you want custom looks."),
        w);
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);

    QVBoxLayout* shader_section = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Shader"));
    QVBoxLayout* shader_layout = shader_section ? shader_section : layout;

    EffectLabeledComboRow* preset_row = EffectUiRows::AppendComboRow(shader_layout, QStringLiteral("Preset:"));
    preset_row->setObjectName(QStringLiteral("presetRow"));
    preset_combo = preset_row->combo();
    preset_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for(const QString& path : preset_paths)
    {
        preset_combo->addItem(SpatialShaderCatalog::PresetDisplayName(path));
    }
    preset_combo->setToolTip(QStringLiteral("Each preset is a different GLSL pattern. Switching reloads the shader."));
    connect(preset_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShaderField::OnPresetChanged);

    EffectLabeledComboRow* projection_row = EffectUiRows::AppendComboRow(shader_layout, QStringLiteral("Projection:"));
    projection_row->setObjectName(QStringLiteral("projectionRow"));
    projection_combo = projection_row->combo();
    projection_combo->addItem(QStringLiteral("Floor (X-Z)"));
    projection_combo->addItem(QStringLiteral("Front (X-Y)"));
    projection_combo->addItem(QStringLiteral("Side (Y-Z)"));
    projection_combo->addItem(QStringLiteral("Sphere"));
    projection_combo->setCurrentIndex(projection_mode);
    projection_combo->setToolTip(QStringLiteral(
        "Which room plane the 2D pattern is mapped onto.\n"
        "Floor = looking down; Front/Side = walls; Sphere = wrap from the effect origin."));
    connect(projection_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ShaderField::OnProjectionModeChanged);

    EffectSliderRow* contrast_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Contrast:"),
        35,
        250,
        (int)std::lround(contrast * 100.0f),
        QStringLiteral("Sharpens or softens the pattern (works on every preset)."));
    contrast_row->setObjectName(QStringLiteral("contrastRow"));
    contrast_slider = contrast_row->slider();
    contrast_row->bindValueChanged(
        this,
        [this](int v) { contrast = std::clamp(v / 100.0f, 0.35f, 2.5f); },
        pct_format,
        on_changed);

    EffectSliderRow* hue_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Hue shift:"),
        0,
        1000,
        (int)std::lround(hue_shift * 1000.0f),
        QStringLiteral("Static color offset. Frequency also scrolls hue over time."));
    hue_row->setObjectName(QStringLiteral("hueShiftRow"));
    hue_slider = hue_row->slider();
    hue_row->bindValueChanged(
        this,
        [this](int v) { hue_shift = std::clamp(v / 1000.0f, 0.0f, 1.0f); },
        [this](int) { return QString::number(hue_shift, 'f', 3); },
        on_changed);

    auto* open_folder_button = new QPushButton(QStringLiteral("Open user shaders folder"), w);
    open_folder_button->setObjectName(QStringLiteral("openFolderButton"));
    open_folder_button->setToolTip(QStringLiteral(
        "Opens OpenRGB3DSpatialPlugin/spatial-shaders/ — drop custom .fs files that define spatialMain()."));
    shader_layout->addWidget(open_folder_button);
    connect(open_folder_button, &QPushButton::clicked, this, &ShaderField::OnOpenShadersFolder);

    compile_log_label = new QLabel(w);
    compile_log_label->setObjectName(QStringLiteral("compileLogLabel"));
    compile_log_label->setWordWrap(true);
    compile_log_label->setVisible(false);
    shader_layout->addWidget(compile_log_label);

    AddWidgetToParent(w, parent);
}

void ShaderField::RebuildPresetList()
{
    preset_paths = SpatialShaderCatalog::ListPresetPaths();
    SpatialShaderCatalog::EnsureUserShadersFolder();
}

void ShaderField::LoadPresetAtIndex(int index)
{
    if(!shader_engine || index < 0 || index >= (int)preset_paths.size())
    {
        if(compile_log_label)
        {
            compile_log_label->setVisible(true);
            compile_log_label->setText(QStringLiteral("No shader preset available."));
        }
        return;
    }
    QFile file(preset_paths[(size_t)index]);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if(compile_log_label)
        {
            compile_log_label->setVisible(true);
            compile_log_label->setText(QStringLiteral("Failed to open preset: %1").arg(QFileInfo(file).fileName()));
        }
        return;
    }
    QTextStream in(&file);
    const QString body = in.readAll();
    if(body.trimmed().isEmpty())
    {
        if(compile_log_label)
        {
            compile_log_label->setVisible(true);
            compile_log_label->setText(QStringLiteral("Preset is empty: %1").arg(QFileInfo(file).fileName()));
        }
        return;
    }
    {
        QMutexLocker lock(&display_mutex);
        display_frame.reset();
    }
    shader_engine->setFragmentBody(body);
    EnsureShaderEngineRunning();
}

void ShaderField::OnPresetChanged(int index)
{
    LoadPresetAtIndex(index);
    emit ParametersChanged();
}

void ShaderField::OnProjectionModeChanged(int index)
{
    projection_mode = std::clamp(index, 0, 3);
    emit ParametersChanged();
}

void ShaderField::OnOpenShadersFolder()
{
    SpatialShaderCatalog::EnsureUserShadersFolder();
    const QString path = SpatialShaderCatalog::UserShadersFolderPath();
    if(!path.isEmpty())
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void ShaderField::OnFrameReady(const QImage& image)
{
    if(image.isNull())
    {
        return;
    }
    QMutexLocker lock(&display_mutex);
    display_frame = std::make_shared<QImage>(image.convertToFormat(QImage::Format_RGB32));
}

void ShaderField::OnCompileMessage(const QString& message)
{
    if(compile_log_label)
    {
        const bool show = !message.isEmpty();
        compile_log_label->setVisible(show);
        compile_log_label->setText(message.left(280));
    }
}

void ShaderField::SetSpeed(unsigned int speed)
{
    SpatialEffect3D::SetSpeed(speed);
    if(shader_engine)
    {
        shader_engine->setTargetFps(30);
    }
}

void ShaderField::SyncUniforms(float time)
{
    if(!shader_engine)
    {
        return;
    }
    EnsureShaderEngineRunning();
    SpatialShaderUniforms u;
    u.time_sec = CalculateProgress(time);
    // Size → zoom, Detail → density, Frequency → hue scroll, local contrast/hue.
    const float zoom = std::clamp(GetNormalizedSize() * (0.55f + 0.9f * GetNormalizedScale()), 0.25f, 3.0f);
    const float detail = std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    const float freq_norm = std::clamp(GetNormalizedFrequency(), 0.0f, 1.0f);
    const float hue = std::fmod(hue_shift + time * freq_norm * 0.08f + 1.0f, 1.0f);
    u.params[0] = zoom;
    u.params[1] = std::clamp(contrast, 0.35f, 2.5f);
    u.params[2] = hue;
    u.params[3] = detail;
    u.param_count = 4;
    shader_engine->setUniforms(u);
}

void ShaderField::SampleUv(float x, float y, float z, const GridContext3D& grid, const Vector3D& origin, float& u, float& v) const
{
    const float inv_w = 1.0f / std::max(1e-4f, grid.max_x - grid.min_x);
    const float inv_h = 1.0f / std::max(1e-4f, grid.max_y - grid.min_y);
    const float inv_d = 1.0f / std::max(1e-4f, grid.max_z - grid.min_z);
    // Size already feeds shader zoom via u_params; keep UV tiling gentle so projection stays readable.
    const float tile = 1.0f;

    switch(projection_mode)
    {
    case 0:
        u = (x - grid.min_x) * inv_w;
        v = (z - grid.min_z) * inv_d;
        break;
    case 1:
        u = (x - grid.min_x) * inv_w;
        v = (y - grid.min_y) * inv_h;
        break;
    case 2:
        u = (y - grid.min_y) * inv_h;
        v = (z - grid.min_z) * inv_d;
        break;
    default:
    {
        float rx = x - origin.x;
        float ry = y - origin.y;
        float rz = z - origin.z;
        const float len = std::sqrt(rx * rx + ry * ry + rz * rz);
        if(len < 1e-4f)
        {
            u = 0.5f;
            v = 0.5f;
        }
        else
        {
            rx /= len;
            ry /= len;
            rz /= len;
            u = std::atan2(rz, rx) / (float)(2.0 * M_PI) + 0.5f;
            v = std::asin(std::clamp(ry, -1.0f, 1.0f)) / (float)M_PI + 0.5f;
        }
        break;
    }
    }

    u = MediaTextureEffect::Frac01((u - 0.5f) * tile + 0.5f);
    v = MediaTextureEffect::Frac01((v - 0.5f) * tile + 0.5f);
}

RGBColor ShaderField::SampleField(float u, float v) const
{
    QMutexLocker lock(&display_mutex);
    if(!display_frame || display_frame->isNull())
    {
        return 0x00000000;
    }
    return MediaTextureEffect::SampleImageBilinear(*display_frame, u, v);
}

RGBColor ShaderField::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }

    const bool same_sequence =
        (grid.render_sequence != 0 && grid.render_sequence == last_uniform_sequence);
    const bool same_preview_time =
        (grid.render_sequence == 0 && std::fabs(time - last_uniform_time) < 1e-4f);
    if(!same_sequence && !same_preview_time)
    {
        last_uniform_sequence = grid.render_sequence;
        last_uniform_time = time;
        SyncUniforms(time);
    }

    const Vector3D origin = GetEffectOriginGrid(grid);
    float u = 0.5f;
    float v = 0.5f;
    SampleUv(x, y, z, grid, origin, u, v);

    return BrightenAudioEffectColor(SampleField(u, v), 1.0f);
}

nlohmann::json ShaderField::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["projection_mode"] = projection_mode;
    j["shader_contrast"] = contrast;
    j["shader_hue_shift"] = hue_shift;
    if(preset_combo)
    {
        j["preset_index"] = preset_combo->currentIndex();
    }
    return j;
}

void ShaderField::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("projection_mode"))
    {
        projection_mode = settings["projection_mode"].get<int>();
        if(projection_combo)
        {
            projection_combo->setCurrentIndex(std::clamp(projection_mode, 0, 3));
        }
    }
    if(settings.contains("shader_contrast") && settings["shader_contrast"].is_number())
        contrast = std::clamp(settings["shader_contrast"].get<float>(), 0.35f, 2.5f);
    if(settings.contains("shader_hue_shift") && settings["shader_hue_shift"].is_number())
        hue_shift = std::clamp(settings["shader_hue_shift"].get<float>(), 0.0f, 1.0f);
    if(contrast_slider)
        contrast_slider->setValue((int)std::lround(contrast * 100.0f));
    if(hue_slider)
        hue_slider->setValue((int)std::lround(hue_shift * 1000.0f));
    if(settings.contains("preset_index") && preset_combo)
    {
        int idx = settings["preset_index"].get<int>();
        if(idx >= 0 && idx < preset_combo->count())
        {
            preset_combo->setCurrentIndex(idx);
            LoadPresetAtIndex(idx);
        }
    }
}

REGISTER_EFFECT_3D(ShaderField)
