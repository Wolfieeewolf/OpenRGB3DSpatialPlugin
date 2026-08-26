// SPDX-License-Identifier: GPL-2.0-only

#include "ShaderField.h"
#include "ShaderFieldPresets.h"
#include "Shaders/SpatialShaderCatalog.h"
#include "MediaTextureEffectUtils.h"
#include "PluginUiUtils.h"

#include <QVBoxLayout>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
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
    shader_engine->setRenderSize(256, 144);
    connect(shader_engine,
            &SpatialShaderEngine::compileMessage,
            this,
            &ShaderField::OnCompileMessage,
            Qt::QueuedConnection);

    // Always install a working body first so Size/Contrast/Hue respond even if qrc presets miss.
    shader_engine->setFragmentBody(QString::fromUtf8(ShaderFieldPresets::kBundled[0].source));
    RebuildPresetList();
    LoadPresetAtIndex(0);

    connect(this, &SpatialEffect3D::ParametersChanged, this, [this]() {
        last_uniform_sequence = 0;
        last_uniform_time = -1.0f;
    });
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
    if(shader_engine)
    {
        shader_engine->start();
    }
}

void ShaderField::PrepareGpuFields(std::uint64_t /*render_sequence*/, float time_sec, const GridContext3D& /*grid*/)
{
    if(!shader_engine)
    {
        return;
    }
    shader_engine->start();
    SyncUniforms(time_sec);
    if(!shader_engine->ensureReady())
    {
        if(compile_log_label && !shader_engine->lastError().isEmpty())
        {
            compile_log_label->setVisible(true);
            compile_log_label->setText(shader_engine->lastError().left(280));
        }
        return;
    }
    const QImage img = shader_engine->latestFrame();
    if(!img.isNull())
    {
        QMutexLocker lock(&display_mutex);
        display_frame = std::make_shared<QImage>(img);
    }
}

EffectInfo3D ShaderField::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Shader Field";
    info.effect_description =
        "Projects a 2D GPU shader pattern onto your room (like wrapping wallpaper around the LEDs). "
        "Pick a Preset for the look, Projection for how it maps (planes, sphere, cylinder, triplanar, …). "
        "Speed animates the shader, Frequency scrolls hue, Size zooms, Detail densifies the pattern. "
        "Use Contrast and Hue with the common controls.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_SHADER_FIELD;
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

    // Refresh list when the panel opens (ctor may have run before resources were ready).
    RebuildPresetList();

    QLabel* help = new QLabel(
        QStringLiteral(
            "Shader Field paints a moving 2D pattern, then samples it onto LEDs.\n"
            "Presets are bundled patterns (waves, plasma, checker, ember, aurora…). "
            "\"Open user shaders folder\" is only for your own .fs files — bundled presets "
            "live inside the plugin, not that folder.\n"
            "Projection picks which room plane is mapped; Size/Detail/Contrast/Hue drive the shader."),
        w);
    help->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(help);
    layout->addWidget(help);

    QVBoxLayout* shader_section = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Shader"));
    QVBoxLayout* shader_layout = shader_section ? shader_section : layout;

    EffectLabeledComboRow* preset_row = EffectUiRows::AppendComboRow(shader_layout, QStringLiteral("Preset:"));
    preset_row->setObjectName(QStringLiteral("presetRow"));
    preset_combo = preset_row->combo();
    preset_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    preset_combo->clear();
    for(const QString& id : preset_ids)
    {
        const ShaderFieldPresets::Bundled* b = ShaderFieldPresets::Find(id);
        preset_combo->addItem(b ? QString::fromUtf8(b->title) : id, id);
    }
    if(preset_ids.empty())
    {
        preset_combo->addItem(QStringLiteral("Slow Waves — soft blue bands"), QStringLiteral("slow_waves"));
        preset_ids.push_back(QStringLiteral("slow_waves"));
    }
    preset_combo->setEnabled(true);
    preset_combo->setCurrentIndex(0);
    LoadPresetAtIndex(0);
    preset_combo->setToolTip(QStringLiteral("Each preset is a different GLSL pattern. Switching reloads the shader."));
    connect(preset_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShaderField::OnPresetChanged);

    EffectLabeledComboRow* projection_row = EffectUiRows::AppendComboRow(shader_layout, QStringLiteral("Projection:"));
    projection_row->setObjectName(QStringLiteral("projectionRow"));
    projection_combo = projection_row->combo();
    projection_combo->addItem(QStringLiteral("Floor (X–Z)"));
    projection_combo->addItem(QStringLiteral("Front wall (X–Y)"));
    projection_combo->addItem(QStringLiteral("Left wall (Y–Z)"));
    projection_combo->addItem(QStringLiteral("Sphere (from origin)"));
    projection_combo->addItem(QStringLiteral("Ceiling (X–Z)"));
    projection_combo->addItem(QStringLiteral("Back wall (X–Y)"));
    projection_combo->addItem(QStringLiteral("Right wall (Y–Z)"));
    projection_combo->addItem(QStringLiteral("Cylinder around Y"));
    projection_combo->addItem(QStringLiteral("Radial floor (polar XZ)"));
    projection_combo->addItem(QStringLiteral("Triplanar (dominant face)"));
    projection_combo->addItem(QStringLiteral("Nearest cube face"));
    projection_combo->setCurrentIndex(std::clamp(projection_mode, 0, PROJ_COUNT - 1));
    projection_combo->setToolTip(QStringLiteral(
        "How the 2D shader pattern maps onto the room.\n"
        "Planes = wallpaper on floor/ceiling/walls.\n"
        "Sphere / Cylinder / Radial = wrap from the Spatial Anchor.\n"
        "Triplanar / Nearest cube face = pick the strongest axis like a box unwrap."));
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
        [this](int v) {
            contrast = std::clamp(v / 100.0f, 0.35f, 2.5f);
            last_uniform_sequence = 0;
        },
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
        [this](int v) {
            hue_shift = std::clamp(v / 1000.0f, 0.0f, 1.0f);
            last_uniform_sequence = 0;
        },
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
    preset_ids.clear();
    for(int i = 0; i < ShaderFieldPresets::kBundledCount; ++i)
        preset_ids.push_back(QString::fromUtf8(ShaderFieldPresets::kBundled[i].id));

    SpatialShaderCatalog::EnsureUserShadersFolder();
    const QString custom_root = SpatialShaderCatalog::UserShadersFolderPath();
    if(!custom_root.isEmpty())
    {
        QDir custom_dir(custom_root);
        if(custom_dir.exists())
        {
            const QFileInfoList files =
                custom_dir.entryInfoList(QStringList() << QStringLiteral("*.fs"), QDir::Files, QDir::Name);
            for(const QFileInfo& fi : files)
                preset_ids.push_back(fi.absoluteFilePath());
        }
    }

    if(!preset_combo)
        return;

    const QString prev =
        (active_preset_index >= 0 && active_preset_index < (int)preset_ids.size())
            ? preset_ids[(size_t)active_preset_index]
            : QString();
    preset_combo->blockSignals(true);
    preset_combo->clear();
    for(const QString& id : preset_ids)
    {
        const ShaderFieldPresets::Bundled* b = ShaderFieldPresets::Find(id);
        if(b)
            preset_combo->addItem(QString::fromUtf8(b->title), id);
        else
            preset_combo->addItem(QFileInfo(id).fileName(), id);
    }
    int idx = 0;
    if(!prev.isEmpty())
    {
        const auto it = std::find(preset_ids.begin(), preset_ids.end(), prev);
        if(it != preset_ids.end())
            idx = (int)std::distance(preset_ids.begin(), it);
    }
    preset_combo->setCurrentIndex(idx);
    preset_combo->blockSignals(false);
    active_preset_index = idx;
}

void ShaderField::LoadPresetAtIndex(int index)
{
    if(!shader_engine)
    {
        return;
    }
    if(preset_ids.empty())
    {
        RebuildPresetList();
    }
    if(index < 0 || index >= (int)preset_ids.size())
    {
        index = 0;
    }
    active_preset_index = index;

    QString body;
    const ShaderFieldPresets::Bundled* b = ShaderFieldPresets::Find(preset_ids[(size_t)index]);
    if(b)
    {
        body = QString::fromUtf8(b->source);
    }
    else
    {
        QFile file(preset_ids[(size_t)index]);
        if(file.open(QIODevice::ReadOnly | QIODevice::Text))
            body = QTextStream(&file).readAll();
    }
    if(body.trimmed().isEmpty())
    {
        body = QString::fromUtf8(ShaderFieldPresets::kBundled[0].source);
        active_preset_index = 0;
    }
    {
        QMutexLocker lock(&display_mutex);
        display_frame.reset();
    }
    last_uniform_sequence = 0;
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
    projection_mode = std::clamp(index, 0, PROJ_COUNT - 1);
    emit ParametersChanged();
}

void ShaderField::OnOpenShadersFolder()
{
    SpatialShaderCatalog::EnsureUserShadersFolder();
    const QString path = SpatialShaderCatalog::UserShadersFolderPath();
    if(!path.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    RebuildPresetList();
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

void ShaderField::SyncUniforms(float time)
{
    if(!shader_engine)
    {
        return;
    }
    EnsureShaderEngineRunning();
    SpatialShaderUniforms u;
    // Drive animation from wall-clock effect time scaled by Speed (not progress wrap alone).
    const float spd = std::max(0.05f, GetScaledSpeed());
    u.time_sec = time * spd * 0.35f;
    // Size → zoom, Detail → density, Frequency → hue scroll, local contrast/hue.
    const float zoom = std::clamp(GetNormalizedSize() * (0.55f + 0.9f * GetNormalizedScale()), 0.25f, 3.0f);
    const float detail = std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    const float freq_norm = std::clamp(GetNormalizedFrequency(), 0.0f, 1.0f);
    const float hue = std::fmod(hue_shift + time * freq_norm * 0.08f * spd + 1.0f, 1.0f);
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
    const float nx = (x - grid.min_x) * inv_w;
    const float ny = (y - grid.min_y) * inv_h;
    const float nz = (z - grid.min_z) * inv_d;

    const int mode = std::clamp(projection_mode, 0, PROJ_COUNT - 1);
    switch(mode)
    {
    case PROJ_FLOOR:
        u = nx;
        v = nz;
        break;
    case PROJ_FRONT:
        u = nx;
        v = ny;
        break;
    case PROJ_SIDE_LEFT:
        u = ny;
        v = nz;
        break;
    case PROJ_CEILING:
        u = nx;
        v = 1.0f - nz;
        break;
    case PROJ_BACK:
        u = 1.0f - nx;
        v = ny;
        break;
    case PROJ_SIDE_RIGHT:
        u = 1.0f - ny;
        v = nz;
        break;
    case PROJ_CYLINDER_Y:
    {
        const float rx = x - origin.x;
        const float rz = z - origin.z;
        u = std::atan2(rz, rx) / (float)(2.0 * M_PI) + 0.5f;
        v = ny;
        break;
    }
    case PROJ_RADIAL_XZ:
    {
        const float rx = x - origin.x;
        const float rz = z - origin.z;
        const float span = 0.5f * std::max(grid.width, grid.depth);
        const float r = std::sqrt(rx * rx + rz * rz) / std::max(1e-4f, span);
        u = std::atan2(rz, rx) / (float)(2.0 * M_PI) + 0.5f;
        v = std::clamp(r, 0.0f, 1.0f);
        break;
    }
    case PROJ_TRIPLANAR:
    case PROJ_CUBE_FACE:
    {
        const float ax = std::fabs(nx - 0.5f);
        const float ay = std::fabs(ny - 0.5f);
        const float az = std::fabs(nz - 0.5f);
        if(ax >= ay && ax >= az)
        {
            u = (nx >= 0.5f) ? (1.0f - ny) : ny;
            v = nz;
        }
        else if(ay >= ax && ay >= az)
        {
            u = nx;
            v = (ny >= 0.5f) ? (1.0f - nz) : nz;
        }
        else
        {
            u = (nz >= 0.5f) ? (1.0f - nx) : nx;
            v = ny;
        }
        break;
    }
    case PROJ_SPHERE:
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
    default:
    {
        u = 0.5f;
        v = 0.5f;
        break;
    }
    }

    u = MediaTextureEffect::Frac01(u);
    v = MediaTextureEffect::Frac01(v);
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
    if(active_preset_index >= 0 && active_preset_index < (int)preset_ids.size())
        j["preset_id"] = preset_ids[(size_t)active_preset_index].toStdString();
    return j;
}

void ShaderField::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("projection_mode") && settings["projection_mode"].is_number_integer())
    {
        projection_mode = std::clamp(settings["projection_mode"].get<int>(), 0, PROJ_COUNT - 1);
        if(projection_combo)
            projection_combo->setCurrentIndex(std::clamp(projection_mode, 0, PROJ_COUNT - 1));
    }
    if(settings.contains("shader_contrast") && settings["shader_contrast"].is_number())
        contrast = std::clamp(settings["shader_contrast"].get<float>(), 0.35f, 2.5f);
    if(settings.contains("shader_hue_shift") && settings["shader_hue_shift"].is_number())
        hue_shift = std::clamp(settings["shader_hue_shift"].get<float>(), 0.0f, 1.0f);
    if(contrast_slider)
        contrast_slider->setValue((int)std::lround(contrast * 100.0f));
    if(hue_slider)
        hue_slider->setValue((int)std::lround(hue_shift * 1000.0f));

    RebuildPresetList();
    int idx = 0;
    if(settings.contains("preset_id") && settings["preset_id"].is_string())
    {
        const QString want = QString::fromStdString(settings["preset_id"].get<std::string>());
        const auto it = std::find(preset_ids.begin(), preset_ids.end(), want);
        if(it != preset_ids.end())
            idx = (int)std::distance(preset_ids.begin(), it);
        else
        {
            for(size_t i = 0; i < preset_ids.size(); ++i)
            {
                if(QFileInfo(preset_ids[i]).fileName() == QFileInfo(want).fileName())
                {
                    idx = (int)i;
                    break;
                }
            }
        }
    }
    if(preset_combo)
    {
        preset_combo->blockSignals(true);
        preset_combo->setCurrentIndex(idx);
        preset_combo->blockSignals(false);
    }
    LoadPresetAtIndex(idx);
}

REGISTER_EFFECT_3D(ShaderField)
