// SPDX-License-Identifier: GPL-2.0-only

#include "Matrix.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "EffectSliderRow.h"
#include "EffectUiRows.h"
#include <algorithm>
#include <cmath>

Matrix::Matrix(QWidget* parent) : SpatialEffect3D(parent)
{
    density_slider         = nullptr;
    trail_slider           = nullptr;
    char_height_slider     = nullptr;
    char_gap_slider        = nullptr;
    char_variation_slider  = nullptr;
    char_spacing_slider    = nullptr;
    head_brightness_slider = nullptr;
    density = 60;
    trail = 50;
    char_height = 15;
    char_gap = 15;
    char_variation = 60;
    char_spacing = 10;
    head_brightness = 35;
    SetRainbowMode(false);
    if(GetColors().empty())
        SetColors({ 0x0000FF00 });  
}

EffectInfo3D Matrix::GetEffectInfo() const
{
    EffectInfo3D info;
    info.info_version = 3;
    info.effect_name = "Matrix";
    info.effect_description =
        "Matrix-style digital rain on all surfaces. Use effect colors or Rainbow for custom/rainbow matrix; Head brightness sets how white the leading character is. Optional stratum band tuning.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_MATRIX;
    info.is_reversible = true;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 30.0f;
    info.default_frequency_scale = 8.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void Matrix::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };
    const auto bind_pct = [this, pct_format, layout](const char* name, const QString& caption, int min, int max,
                                                    int value, const QString& tip, unsigned int& target,
                                                    QSlider*& out_slider) {
        EffectSliderRow* row = EffectUiRows::AppendSliderRow(layout, caption, min, max, value, tip);
        row->setObjectName(name);
        out_slider = row->slider();
        row->bindValueChanged(
            this,
            [&target](int v) { target = (unsigned int)v; },
            pct_format,
            [this]() { emit ParametersChanged(); });
    };

    bind_pct("densityRow", QStringLiteral("Density (%):"), 10, 100, (int)density,
             QStringLiteral("Column density: higher = more columns (more matrix rain)"), density, density_slider);
    bind_pct("trailRow", QStringLiteral("Trail (%):"), 10, 100, (int)trail,
             QStringLiteral("Trail length: higher = longer fading tail behind each character"), trail, trail_slider);
    bind_pct("charHeightRow", QStringLiteral("Char height (%):"), 5, 100, (int)char_height,
             QStringLiteral("Character height: higher = taller glyphs in the stream"), char_height, char_height_slider);
    bind_pct("charGapRow", QStringLiteral("Char gap (%):"), 0, 100, (int)char_gap,
             QStringLiteral("Gap between characters in a column: higher = more space"), char_gap, char_gap_slider);
    bind_pct("variationRow", QStringLiteral("Variation (%):"), 0, 100, (int)char_variation,
             QStringLiteral("Brightness variation: higher = more random bright/dim characters"), char_variation,
             char_variation_slider);
    bind_pct("spacingRow", QStringLiteral("Spacing (%):"), 1, 100, (int)char_spacing,
             QStringLiteral("Vertical spacing in stream: lower = denser flow, higher = more spread out"), char_spacing,
             char_spacing_slider);

    EffectSliderRow* head_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Head brightness (%):"),
        0,
        100,
        (int)head_brightness,
        QStringLiteral(
            "How much the leading character is whitish (0 = no white, 100 = full white tip). "
            "Only the very first LED is affected."));
    head_row->setObjectName(QStringLiteral("headBrightnessRow"));
    head_brightness_slider = head_row->slider();
    head_row->bindValueChanged(
        this,
        [this](int v) { head_brightness = (unsigned int)std::clamp(v, 0, 100); },
        pct_format,
        [this]() { emit ParametersChanged(); });

    AddWidgetToParent(w, parent);
}

void Matrix::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_MATRIX;
}

void Matrix::OnMatrixParameterChanged()
{
    if(density_slider)
        density = (unsigned int)density_slider->value();
    if(trail_slider)
        trail = (unsigned int)trail_slider->value();
    if(char_height_slider)
        char_height = (unsigned int)char_height_slider->value();
    if(char_gap_slider)
        char_gap = (unsigned int)char_gap_slider->value();
    if(char_variation_slider)
        char_variation = (unsigned int)char_variation_slider->value();
    if(char_spacing_slider)
        char_spacing = (unsigned int)char_spacing_slider->value();
    if(head_brightness_slider)
        head_brightness = (unsigned int)std::clamp(head_brightness_slider->value(), 0, 100);
    emit ParametersChanged();
}

float Matrix::ComputeFaceIntensity(int face,
                                     float x,
                                     float y,
                                     float z,
                                     float time,
                                     const GridContext3D& grid,
                                     float column_spacing,
                                     float size_normalized,
                                     float speed_scale,
                                     float* out_head) const
{
    if(out_head) *out_head = 0.0f;
    if(grid.min_x >= grid.max_x || grid.min_y >= grid.max_y || grid.min_z >= grid.max_z)
    {
        return 0.0f;
    }

    if(column_spacing < 0.001f) column_spacing = 0.001f;
    if(size_normalized < 0.001f) size_normalized = 0.001f;
    if(size_normalized > 10.0f) size_normalized = 10.0f;
    if(time < -100000.0f || time > 100000.0f) time = 0.0f;
    if(x < -10000.0f || x > 10000.0f) x = 0.0f;
    if(y < -10000.0f || y > 10000.0f) y = 0.0f;
    if(z < -10000.0f || z > 10000.0f) z = 0.0f;
    
    float u = 0.0f;
    float v = 0.0f;
    float axis_value = 0.0f;
    float axis_min = 0.0f;
    float axis_max = 0.0f;
    float face_distance = 0.0f;

    switch(face)
    {
        case 0:
            u = z;
            v = y;
            axis_value = y;
            axis_min = grid.min_y;
            axis_max = grid.max_y;
            face_distance = fabsf(x - grid.min_x);
            break;
        case 1:
            u = z;
            v = y;
            axis_value = y;
            axis_min = grid.min_y;
            axis_max = grid.max_y;
            face_distance = fabsf(x - grid.max_x);
            break;
        case 2:
            u = x;
            v = y;
            axis_value = y;
            axis_min = grid.min_y;
            axis_max = grid.max_y;
            face_distance = fabsf(z - grid.min_z);
            break;
        case 3:
            u = x;
            v = y;
            axis_value = y;
            axis_min = grid.min_y;
            axis_max = grid.max_y;
            face_distance = fabsf(z - grid.max_z);
            break;
        case 4:
            
            u = x;
            v = z;
            axis_value = z;
            axis_min = grid.min_z;
            axis_max = grid.max_z;
            face_distance = fabsf(y - grid.min_y);
            break;
        case 5:
        default:
            
            u = x;
            v = z;
            axis_value = z;
            axis_min = grid.min_z;
            axis_max = grid.max_z;
            face_distance = fabsf(y - grid.max_y);
            break;
    }

    
    float room_extent = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    if(room_extent > 0.001f && face_distance > room_extent * 0.75f)
        return 0.0f;

    if(column_spacing < 0.001f) column_spacing = 0.001f;

    if(u < -10000.0f || u > 10000.0f) u = 0.0f;
    if(v < -10000.0f || v > 10000.0f) v = 0.0f;
    
    float col_u_float = u / column_spacing;
    float col_v_float = v / column_spacing;
    col_u_float = fmax(-1000.0f, fmin(1000.0f, col_u_float));
    col_v_float = fmax(-1000.0f, fmin(1000.0f, col_v_float));
    
    int column_u = (int)floorf(col_u_float);
    int column_v = (int)floorf(col_v_float);
    column_u = (column_u < -1000) ? -1000 : (column_u > 1000 ? 1000 : column_u);
    column_v = (column_v < -1000) ? -1000 : (column_v > 1000 ? 1000 : column_v);
    int column_id = (column_u * 73856093) ^ (column_v * 19349663);

    float offset = ((column_id & 255) / 255.0f) * 10.0f;
    offset = fmax(0.0f, fmin(10.0f, offset));

    unsigned int local_char_spacing = char_spacing;
    unsigned int local_char_height = char_height;
    unsigned int local_char_gap = char_gap;
    unsigned int local_char_variation = char_variation;
    unsigned int local_trail = trail;
    
    unsigned int safe_char_spacing = (local_char_spacing >= 1 && local_char_spacing <= 100) ? local_char_spacing : 10;
    unsigned int safe_char_height = (local_char_height >= 5 && local_char_height <= 100) ? local_char_height : 15;
    unsigned int safe_char_gap = (local_char_gap <= 100) ? local_char_gap : 15;
    unsigned int safe_char_variation = (local_char_variation <= 100) ? local_char_variation : 60;
    
    float char_height_base = 0.10f + (safe_char_height / 100.0f) * 0.40f;
    float char_height_actual = char_height_base * size_normalized;
    
    if(char_height_actual < 0.001f) char_height_actual = 0.001f;
    if(char_height_actual > 10.0f) char_height_actual = 10.0f;

    float spacing_factor = safe_char_spacing / 100.0f;
    float char_spacing_actual = char_height_actual * (0.5f + spacing_factor * 1.5f);

    if(char_spacing_actual < 0.001f) char_spacing_actual = 0.001f;
    if(char_spacing_actual > 20.0f) char_spacing_actual = 20.0f;
    
    float gap_ratio = safe_char_gap / 100.0f;
    gap_ratio = fmax(0.0f, fmin(0.5f, gap_ratio));
    float char_body_ratio = 1.0f - gap_ratio;

    if(char_spacing_actual < 0.001f) char_spacing_actual = 0.001f;
    if(char_spacing_actual > 100.0f) char_spacing_actual = 100.0f;

    float safe_time_for_wrap = fmax(-1000000.0f, fmin(1000000.0f, time));
    const float wrap_period = 86400.0f;
    float wrapped_time = fmodf(safe_time_for_wrap, wrap_period);
    if(wrapped_time < 0.0f) wrapped_time += wrap_period;
    if(wrapped_time < 0.0f || wrapped_time >= wrap_period) wrapped_time = 0.0f;

    float fall_speed = fmax(0.1f, fmin(100.0f, speed_scale));
    float safe_offset = fmax(-100.0f, fmin(100.0f, offset));

    float stream_time = wrapped_time * fall_speed + safe_offset;

    if(axis_min > axis_max)
    {
        float temp = axis_min;
        axis_min = axis_max;
        axis_max = temp;
    }
    if(axis_max - axis_min < 0.001f)
    {
        axis_min = 0.0f;
        axis_max = 1.0f;
    }

    axis_value = fmax(axis_min - 10.0f, fmin(axis_max + 10.0f, axis_value));

    float position_along_axis;
    if(face >= 4)
    {
        position_along_axis = axis_value - axis_min;
    }
    else
    {
        position_along_axis = axis_max - axis_value;
    }

    position_along_axis = fmax(-1000.0f, fmin(1000.0f, position_along_axis));
    stream_time = fmax(-10000.0f, fmin(10000.0f, stream_time));

    
    float stream_pos = stream_time - position_along_axis;

    stream_pos = fmax(-10000.0f, fmin(10000.0f, stream_pos));

    if(char_spacing_actual < 0.001f) char_spacing_actual = 0.001f;
    if(char_spacing_actual > 100.0f) char_spacing_actual = 100.0f;

    if(stream_pos < -100000.0f || stream_pos > 100000.0f) stream_pos = 0.0f;

    float abs_stream_pos = fabsf(stream_pos);
    abs_stream_pos = fmodf(abs_stream_pos, char_spacing_actual);
    stream_pos = abs_stream_pos;
    stream_pos = fmax(0.0f, fmin(char_spacing_actual - 0.0001f, stream_pos));

    if(char_spacing_actual < 0.001f) char_spacing_actual = 0.001f;
    float char_local = stream_pos / char_spacing_actual;
    char_local = fmax(0.0f, fmin(1.0f, char_local));

    if(char_spacing_actual < 0.001f) char_spacing_actual = 0.001f;
    float char_index_value = (stream_time - position_along_axis) / char_spacing_actual;
    char_index_value = fmax(-10000.0f, fmin(10000.0f, char_index_value));
    float char_index = floorf(char_index_value);
    char_index = fmax(-1000.0f, fmin(1000.0f, char_index));
    
    float intensity = 0.0f;

    float char_portion = char_height_actual / char_spacing_actual;
    if(char_portion > 1.0f) char_portion = 1.0f;
    if(char_portion < 0.001f) char_portion = 0.001f;

    if(char_local < char_portion && char_portion > 0.001f)
    {
        float char_internal = char_local / char_portion;
        char_internal = fmax(0.0f, fmin(1.0f, char_internal));

        if(char_internal < char_body_ratio)
        {
            intensity = 1.0f;
            
            if(out_head && char_internal < 0.1f)
                *out_head = 1.0f - char_internal / 0.1f;
        }
        else
        {
            intensity = 0.2f;
        }

        int safe_char_index = (int)fmax(-10000.0f, fmin(10000.0f, char_index));
        float char_seed = (float)(safe_char_index * 131 + column_id);
        float variation_amount = safe_char_variation / 100.0f;
        variation_amount = fmax(0.0f, fmin(1.0f, variation_amount));
        
        if(variation_amount > 0.01f)
        {
            float safe_char_seed = fmax(-1000000.0f, fmin(1000000.0f, char_seed * 0.1f));
            float char_brightness = 0.5f + 0.5f * fmodf(safe_char_seed, 1.0f) * variation_amount;
            char_brightness = fmax(0.0f, fmin(1.0f, char_brightness));
            intensity *= char_brightness;
        }

        unsigned int safe_trail = (local_trail <= 100) ? local_trail : 50;
        float trail_char_count = 3.0f + (safe_trail / 100.0f) * 12.0f;
        trail_char_count = fmax(1.0f, fmin(1000.0f, trail_char_count));

        float safe_char_index_float = fmax(-10000.0f, fmin(10000.0f, char_index));
        float slots_behind = fabsf(safe_char_index_float);
        slots_behind = fmax(0.0f, fmin(10000.0f, slots_behind));
        
        if(slots_behind > trail_char_count && trail_char_count > 0.001f)
        {
            float fade_start = trail_char_count;
            float fade_end = trail_char_count * 2.0f;
            fade_end = fmax(fade_start + 0.001f, fmin(2000.0f, fade_end));
            
            if(slots_behind < fade_end && fade_end > fade_start)
            {
                float fade_range = fade_end - fade_start;
                if(fade_range > 0.001f)
                {
                    float trail_fade = 1.0f - ((slots_behind - fade_start) / fade_range);
                    trail_fade = fmax(0.0f, fmin(1.0f, trail_fade));
                    intensity *= trail_fade;
                }
            }
            else if(slots_behind >= fade_end)
            {
                intensity = 0.0f;
            }
        }
    }

    int safe_column_id = column_id;
    if(safe_column_id < 0) safe_column_id = -safe_column_id;
    float gap = fmodf((float)((safe_column_id >> 8) & 1023), 10.0f) / 10.0f;
    gap = fmax(0.0f, fmin(1.0f, gap));
    float gap_factor = 0.6f + 0.4f * (gap > 0.3f ? 1.0f : gap * 3.33f);
    gap_factor = fmax(0.0f, fmin(1.0f, gap_factor));
    intensity *= gap_factor;

    face_distance = fmax(0.0f, fmin(1000.0f, face_distance));
    if(face_distance != face_distance) face_distance = 0.0f;
    float exp_arg = -face_distance * 0.5f;
    exp_arg = fmax(-100.0f, fmin(100.0f, exp_arg));
    if(exp_arg != exp_arg) exp_arg = 0.0f;
    float face_falloff = 0.3f + 0.7f * expf(exp_arg);
    if(face_falloff != face_falloff || face_falloff < 0.0f) face_falloff = 0.3f;
    face_falloff = fmax(0.0f, fmin(1.0f, face_falloff));

    float ambient = 0.1f * (1.0f - fmin(1.0f, face_distance * 0.05f));
    ambient = fmax(0.0f, fmin(1.0f, ambient));

    intensity = intensity * face_falloff + ambient;

    if(intensity != intensity || intensity < -1000.0f || intensity > 1000.0f)
    {
        intensity = 0.0f;
    }

    intensity = fmax(0.0f, fmin(10.0f, intensity));
    intensity *= 2.0f;

    if(intensity != intensity || intensity < 0.0f) intensity = 0.0f;
    if(intensity > 1.0f) intensity = 1.0f;
    intensity = fmax(0.0f, fmin(1.0f, intensity));
    return intensity;
}

RGBColor Matrix::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rp = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);


    float speed = GetScaledSpeed() * bb.speed_mul;
    float size_m = GetNormalizedSize();
    float t_sample = time + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * (2.0f / std::max(0.1f, speed));
    float color_cycle = t_sample * GetScaledFrequency() * 0.02f;
    float detail = std::max(0.05f, GetScaledDetail());

    float col_spacing =(1.0f + (100.0f - density) * 0.04f) / (0.6f + 0.4f * detail);
    col_spacing /= std::max(0.25f, bb.tight_mul);

    float intensity = 0.0f;
    float head = 0.0f;

    for(int face_index = 0; face_index < 6; ++face_index)
    {
        float face_head = 0.0f;
        float face_value = ComputeFaceIntensity(face_index, x, y, z, t_sample, grid, col_spacing, size_m, speed, &face_head);
        if(face_value > intensity)
        {
            intensity = face_value;
            head = face_head;
        }
    }

    const float mtx_phase01 = std::fmod(color_cycle + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);
    float strip_p01 = 0.0f;
    if(UseEffectStripColormap())
    {
        strip_p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                               GetEffectStripColormapRepeats(),
                                               GetEffectStripColormapUnfold(),
                                               GetEffectStripColormapDirectionDeg(),
                                               mtx_phase01,
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               rp);
    }

    
    float color_pos = fmodf(color_cycle + (float)(((int)(x * 31 + y * 17 + z * 7) % 1000)) / 1000.0f, 1.0f);
    if(UseEffectStripColormap())
        color_pos = strip_p01;
    if(color_pos < 0.0f) color_pos += 1.0f;
    color_pos = EffectStratumBlend::ApplyMotionToPhase01(color_pos, stratum_mot01, 0.35f);

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.09f + 0.08f * (1.0f - detail), 0.05f, 0.20f);
    map.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
    map.directional_sharpness = std::clamp(0.95f + detail * 0.1f, 0.85f, 2.2f);
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
    color_pos = ApplySpatialPalette01(color_pos, basis, sp, map, time, &grid);
    color_pos = ApplyVoxelDriveToPalette01(color_pos, x, y, z, time, grid);
    color_pos = std::fmod(color_pos, 1.0f);
    if(color_pos < 0.0f)
        color_pos += 1.0f;

    RGBColor trail_color = GetColorAtPosition(color_pos);
    unsigned char tr = (unsigned char)(trail_color & 0xFF);
    unsigned char tg = (unsigned char)((trail_color >> 8) & 0xFF);
    unsigned char tb = (unsigned char)((trail_color >> 16) & 0xFF);
    float bright = intensity;
    tr = (unsigned char)(tr * bright);
    tg = (unsigned char)(tg * bright);
    tb = (unsigned char)(tb * bright);

    
    float head_strength = head * (head_brightness / 100.0f);
    head_strength = std::min(1.0f, std::max(0.0f, head_strength));
    unsigned char r = (unsigned char)(tr * (1.0f - head_strength) + 255 * head_strength);
    unsigned char g = (unsigned char)(tg * (1.0f - head_strength) + 255 * head_strength);
    unsigned char b = (unsigned char)(tb * (1.0f - head_strength) + 255 * head_strength);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Matrix::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["density"] = density;
    j["trail"] = trail;
    j["char_height"] = char_height;
    j["char_gap"] = char_gap;
    j["char_variation"] = char_variation;
    j["char_spacing"] = char_spacing;
    j["head_brightness"] = head_brightness;
return j;
}

void Matrix::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("density")) density = settings["density"];
    if(settings.contains("trail")) trail = settings["trail"];
    if(settings.contains("char_height")) char_height = settings["char_height"];
    if(settings.contains("char_gap")) char_gap = settings["char_gap"];
    if(settings.contains("char_variation")) char_variation = settings["char_variation"];
    if(settings.contains("char_spacing")) char_spacing = settings["char_spacing"];
    if(settings.contains("head_brightness")) head_brightness = std::clamp(settings["head_brightness"].get<unsigned int>(), 0u, 100u);
    if(density_slider)
        density_slider->setValue((int)density);
    if(trail_slider)
        trail_slider->setValue((int)trail);
    if(char_height_slider)
        char_height_slider->setValue((int)char_height);
    if(char_gap_slider)
        char_gap_slider->setValue((int)char_gap);
    if(char_variation_slider)
        char_variation_slider->setValue((int)char_variation);
    if(char_spacing_slider)
        char_spacing_slider->setValue((int)char_spacing);
    if(head_brightness_slider)
        head_brightness_slider->setValue((int)head_brightness);
}

REGISTER_EFFECT_3D(Matrix)
