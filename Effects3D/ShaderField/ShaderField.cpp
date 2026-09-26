// SPDX-License-Identifier: GPL-2.0-only

#include "ShaderField.h"
#include "Shaders/SpatialShaderCatalog.h"
#include "MediaTextureEffectUtils.h"
#include "PluginUiUtils.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "PluginSettingsPaths.h"
#include "EffectListManager3D.h"
#include "PlayerEngines.h"
#include "EffectUiRows.h"

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
#include <QDirIterator>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{

QString ShaderBody(const QString& path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString();
    }
    QString body;
    QTextStream stream(&file);
    bool header = true;
    while(!stream.atEnd())
    {
        const QString line = stream.readLine();
        const QString trimmed = line.trimmed();
        if(header)
        {
            if(trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            {
                continue;
            }
            const int colon = trimmed.indexOf(QLatin1Char(':'));
            if(colon > 0)
            {
                const QString key = trimmed.left(colon).trimmed();
                if(key == QStringLiteral("name") || key == QStringLiteral("description")
                   || key == QStringLiteral("section") || key == QStringLiteral("class")
                   || key == QStringLiteral("category"))
                {
                    continue;
                }
            }
            header = false;
        }
        body += line;
        body += QLatin1Char('\n');
    }
    return body;
}

ShaderFieldSpec ReadShaderFieldSpec(const QString& path)
{
    ShaderFieldSpec spec;
    const QFileInfo fi(path);
    spec.path = fi.absoluteFilePath();
    spec.class_name = fi.completeBaseName().toStdString();
    spec.ui_name = spec.class_name;

    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return spec;
    }
    QTextStream stream(&file);
    while(!stream.atEnd())
    {
        const QString line = stream.readLine().trimmed();
        if(line.isEmpty() || line.startsWith(QLatin1Char('#')))
        {
            continue;
        }
        const int colon = line.indexOf(QLatin1Char(':'));
        if(colon <= 0)
        {
            break;
        }
        const QString key = line.left(colon).trimmed();
        const QString val = line.mid(colon + 1).trimmed();
        if(key == QStringLiteral("class") && !val.isEmpty())
        {
            spec.class_name = val.toStdString();
        }
        else if(key == QStringLiteral("name") && !val.isEmpty())
        {
            spec.ui_name = val.toStdString();
        }
        else if(key == QStringLiteral("description") && !val.isEmpty())
        {
            spec.description = val.toStdString();
        }
    }
    return spec;
}

} // namespace

void RegisterShaderFieldEffects()
{
    if(!OpenRGB3DSpatialPlugin::APIPointer)
    {
        return;
    }
    SpatialShaderCatalog::EnsureUserShadersFolder();
    const QString root = SpatialShaderCatalog::UserShadersFolderPath();
    if(root.isEmpty())
    {
        return;
    }
    QDir dir(root);
    if(!dir.exists())
    {
        return;
    }
    const QFileInfoList files =
        dir.entryInfoList(QStringList() << QStringLiteral("*.fs"), QDir::Files, QDir::Name);
    for(const QFileInfo& fi : files)
    {
        ShaderFieldSpec spec = ReadShaderFieldSpec(fi.absoluteFilePath());
        if(spec.class_name.empty())
        {
            continue;
        }
        const std::string class_name = spec.class_name;
        const std::string ui_name = spec.ui_name.empty() ? class_name : spec.ui_name;
        EffectListManager3D::get()->RegisterEffect(
            class_name,
            ui_name,
            "Shader Field",
            "",
            "",
            [spec]() { return new ShaderField(spec); });
    }
}

ShaderField::ShaderField(ShaderFieldSpec spec, QWidget* parent)
    : SpatialEffect3D(parent)
    , spec_(std::move(spec))
{
    shader_engine = new SpatialShaderEngine(this);
    shader_engine->setRenderSize(256, 144);
    connect(shader_engine,
            &SpatialShaderEngine::compileMessage,
            this,
            &ShaderField::OnCompileMessage,
            Qt::QueuedConnection);

    LoadShaderBody();

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

void ShaderField::LoadShaderBody()
{
    if(!shader_engine)
    {
        return;
    }
    const QString body = ShaderBody(spec_.path);
    if(body.trimmed().isEmpty())
    {
        return;
    }
    {
        QMutexLocker lock(&display_mutex);
        display_frame.reset();
    }
    last_uniform_sequence = 0;
    last_uniform_time = -1.0f;
    shader_engine->setFragmentBody(body);
    EnsureShaderEngineRunning();
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
    info.effect_name = spec_.ui_name.empty() ? "Shader Field" : spec_.ui_name.c_str();
    info.effect_description =
        spec_.description.empty()
            ? "Projects a 2D GPU shader pattern onto your room. "
              "Projection picks how it maps; Speed/Frequency/Size/Detail plus Contrast/Hue drive the look."
            : spec_.description.c_str();
    info.category = "Shader Field";
    info.effect_type = SPATIAL_EFFECT_SHADER_FIELD;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_frequency = true;
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
            "This look is a .fs file in effects/shader-field (spatialMain). "
            "Projection picks which room plane is mapped; Size/Detail/Contrast/Hue drive the shader."),
        w);
    help->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(help);
    layout->addWidget(help);

    QVBoxLayout* shader_section = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Shader"));
    QVBoxLayout* shader_layout = shader_section ? shader_section : layout;

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
        "How the 2D shader pattern maps onto the Spatial Anchor occupancy box "
        "(target bounds AABB × Scale).\n"
        "Planes = wallpaper on that box's floor/ceiling/walls.\n"
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
        QStringLiteral("Sharpens or softens the pattern."));
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
        "Opens the effects/shader-field folder. Drop .fs files that define spatialMain(). Restart OpenRGB to pick up new files."));
    shader_layout->addWidget(open_folder_button);
    connect(open_folder_button, &QPushButton::clicked, this, &ShaderField::OnOpenShadersFolder);

    compile_log_label = new QLabel(w);
    compile_log_label->setObjectName(QStringLiteral("compileLogLabel"));
    compile_log_label->setWordWrap(true);
    compile_log_label->setVisible(false);
    shader_layout->addWidget(compile_log_label);

    AddWidgetToParent(w, parent);
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
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
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
    u.time_sec = time * GetMotionHz() * (float)(2.0 * M_PI);
    const float zoom = std::clamp(GetNormalizedSize(), 0.25f, 3.0f);
    const float detail = std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    const float hue = std::fmod(hue_shift + time * GetColorCycleHz() + 1.0f, 1.0f);
    u.params[0] = zoom;
    u.params[1] = std::clamp(contrast, 0.35f, 2.5f);
    u.params[2] = hue;
    u.params[3] = detail;
    u.param_count = 4;
    shader_engine->setUniforms(u);
}

void ShaderField::SampleUv(float nx, float ny, float nz, float& u, float& v) const
{
    const int mode = std::clamp(projection_mode, 0, PROJ_COUNT - 1);
    bool wrap_u = false;
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
        const float rx = nx * 2.0f - 1.0f;
        const float rz = nz * 2.0f - 1.0f;
        u = std::atan2(rz, rx) / (float)(2.0 * M_PI) + 0.5f;
        v = ny;
        wrap_u = true;
        break;
    }
    case PROJ_RADIAL_XZ:
    {
        const float rx = nx * 2.0f - 1.0f;
        const float rz = nz * 2.0f - 1.0f;
        u = std::atan2(rz, rx) / (float)(2.0 * M_PI) + 0.5f;
        v = std::sqrt(rx * rx + rz * rz);
        wrap_u = true;
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
        float rx = nx * 2.0f - 1.0f;
        float ry = ny * 2.0f - 1.0f;
        float rz = nz * 2.0f - 1.0f;
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
            wrap_u = true;
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

    if(wrap_u)
    {
        u = MediaTextureEffect::Frac01(u);
    }
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
    float nx = 0.5f, ny = 0.5f, nz = 0.5f;
    if(!SampleGpuVolumeOriginLocal01(x, y, z, grid, origin, GetNormalizedScale(), &nx, &ny, &nz))
    {
        return 0x00000000;
    }
    float u = 0.5f;
    float v = 0.5f;
    SampleUv(nx, ny, nz, u, v);
    if(u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
    {
        return 0x00000000;
    }

    return BrightenAudioEffectColor(SampleField(u, v), 1.0f);
}

nlohmann::json ShaderField::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["projection_mode"] = projection_mode;
    j["shader_contrast"] = contrast;
    j["shader_hue_shift"] = hue_shift;
    j["shader_path"] = spec_.path.toStdString();
    return j;
}

void ShaderField::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("projection_mode") && settings["projection_mode"].is_number_integer())
    {
        projection_mode = std::clamp(settings["projection_mode"].get<int>(), 0, PROJ_COUNT - 1);
        if(projection_combo)
        {
            projection_combo->setCurrentIndex(std::clamp(projection_mode, 0, PROJ_COUNT - 1));
        }
    }
    if(settings.contains("shader_contrast") && settings["shader_contrast"].is_number())
    {
        contrast = std::clamp(settings["shader_contrast"].get<float>(), 0.35f, 2.5f);
    }
    if(settings.contains("shader_hue_shift") && settings["shader_hue_shift"].is_number())
    {
        hue_shift = std::clamp(settings["shader_hue_shift"].get<float>(), 0.0f, 1.0f);
    }
    if(contrast_slider)
    {
        contrast_slider->setValue((int)std::lround(contrast * 100.0f));
    }
    if(hue_slider)
    {
        hue_slider->setValue((int)std::lround(hue_shift * 1000.0f));
    }
}
