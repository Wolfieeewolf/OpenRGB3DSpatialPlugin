// SPDX-License-Identifier: GPL-2.0-only

#include "ShaderField.h"
#include "Shaders/SpatialShaderCatalog.h"
#include "MediaTextureEffectUtils.h"

#include <QVBoxLayout>
#include <QCheckBox>
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
    shader_engine = new SpatialShaderEngine(this);
    shader_engine->setTargetFps(30);
    shader_engine->setRenderSize(128, 72);
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
    if(shader_engine && !shader_engine->isRunning())
    {
        shader_engine->start();
    }
}

EffectInfo3D ShaderField::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Shader Field";
    info.effect_description = "GPU field presets (spatialMain GLSL) sampled on the 3D layout.";
    info.category = "Audio";
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.default_speed_scale = 10.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
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

    QVBoxLayout* shader_section = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Shader"));
    QVBoxLayout* shader_layout = shader_section ? shader_section : layout;

    EffectLabeledComboRow* preset_row = EffectUiRows::AppendComboRow(shader_layout, QStringLiteral("Preset:"));
    preset_row->setObjectName(QStringLiteral("presetRow"));
    preset_combo = preset_row->combo();
    preset_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for(const QString& path : preset_paths)
    {
        preset_combo->addItem(QFileInfo(path).fileName());
    }
    connect(preset_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShaderField::OnPresetChanged);

    EffectLabeledComboRow* projection_row = EffectUiRows::AppendComboRow(shader_layout, QStringLiteral("Projection:"));
    projection_row->setObjectName(QStringLiteral("projectionRow"));
    projection_combo = projection_row->combo();
    projection_combo->addItem(QStringLiteral("Floor (X-Z)"));
    projection_combo->addItem(QStringLiteral("Front (X-Y)"));
    projection_combo->addItem(QStringLiteral("Side (Y-Z)"));
    projection_combo->addItem(QStringLiteral("Sphere"));
    projection_combo->setCurrentIndex(projection_mode);
    connect(projection_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ShaderField::OnProjectionModeChanged);

    use_audio_check = new QCheckBox(QStringLiteral("Feed spectrum to u_audio"), w);
    use_audio_check->setObjectName(QStringLiteral("useAudioCheck"));
    use_audio_check->setChecked(use_audio);
    shader_layout->addWidget(use_audio_check);
    connect(use_audio_check, &QCheckBox::stateChanged, this, &ShaderField::OnUseAudioChanged);

    auto* open_folder_button = new QPushButton(QStringLiteral("Open user shaders folder"), w);
    open_folder_button->setObjectName(QStringLiteral("openFolderButton"));
    open_folder_button->setToolTip(QStringLiteral(
        "Opens OpenRGB3DSpatialPlugin/spatial-shaders/ — add custom .fs fragment shaders for Shader Field"));
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
        return;
    }
    QFile file(preset_paths[(size_t)index]);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }
    QTextStream in(&file);
    shader_engine->setFragmentBody(in.readAll());
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

void ShaderField::OnUseAudioChanged(int state)
{
    use_audio = (state == Qt::Checked);
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
        shader_engine->setTargetFps(std::clamp((int)speed, 10, 60));
    }
}

void ShaderField::UpdateParams(SpatialEffectParams& /*params*/)
{
}

void ShaderField::SyncUniforms(float time)
{
    if(!shader_engine)
    {
        return;
    }
    EnsureShaderEngineRunning();
    SpatialShaderUniforms u;
    u.time_sec = time;
    if(use_audio && AudioInputManager::instance()->isRunning())
    {
        AudioInputManager::SpectrumSnapshot snap = AudioInputManager::instance()->getSpectrumSnapshot(128);
        const int n = (int)snap.bins.size();
        for(int i = 0; i < 128; ++i)
        {
            float v = 0.0f;
            if(n > 0)
            {
                const int idx = (i * n) / 128;
                v = snap.bins[(size_t)std::clamp(idx, 0, n - 1)];
            }
            audio_uniform[i] = std::clamp(v, 0.0f, 1.0f);
        }
        u.audio_ptr = audio_uniform;
        u.audio_count = 128;
    }
    else
    {
        u.audio_ptr = nullptr;
        u.audio_count = 0;
    }
    shader_engine->setUniforms(u);
}

void ShaderField::SampleUv(float x, float y, float z, const GridContext3D& grid, const Vector3D& origin, float& u, float& v) const
{
    const float inv_w = 1.0f / std::max(1e-4f, grid.max_x - grid.min_x);
    const float inv_h = 1.0f / std::max(1e-4f, grid.max_y - grid.min_y);
    const float inv_d = 1.0f / std::max(1e-4f, grid.max_z - grid.min_z);
    const float tile = std::max(0.25f, GetNormalizedSize());

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

    if(grid.render_sequence != last_uniform_sequence)
    {
        last_uniform_sequence = grid.render_sequence;
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
    j["use_audio"] = use_audio;
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
    if(settings.contains("use_audio"))
    {
        use_audio = settings["use_audio"].get<bool>();
        if(use_audio_check)
        {
            use_audio_check->setChecked(use_audio);
        }
    }
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
