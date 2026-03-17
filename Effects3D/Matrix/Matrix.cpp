// SPDX-License-Identifier: GPL-2.0-only

#include "Matrix.h"
#include <QGridLayout>
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(Matrix);

Matrix::Matrix(QWidget* parent) : SpatialEffect3D(parent)
{
    density_slider = nullptr;
    density_label = nullptr;
    trail_slider = nullptr;
    trail_label = nullptr;
    char_height_slider = nullptr;
    char_height_label = nullptr;
    char_gap_slider = nullptr;
    char_gap_label = nullptr;
    char_variation_slider = nullptr;
    char_variation_label = nullptr;
    char_spacing_slider = nullptr;
    char_spacing_label = nullptr;
    head_brightness_slider = nullptr;
    head_brightness_label = nullptr;
    density = 60;
    trail = 50;
    char_height = 15;
    char_gap = 15;
    char_variation = 60;
    char_spacing = 10;
    head_brightness = 35;
    SetRainbowMode(false);
    if(GetColors().empty())
        SetColors({ 0x0000FF00 });  /* default green for classic matrix look */
}

EffectInfo3D Matrix::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "Matrix";
    info.effect_description = "Matrix-style digital rain on all surfaces. Use effect colors or Rainbow for custom/rainbow matrix; Head brightness sets how white the leading character is.";
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
    info.show_fps_control = true;
    info.show_color_controls = true;
    return info;
}

void Matrix::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(new QLabel("Density (%):"), 0, 0);
    density_slider = new QSlider(Qt::Horizontal);
    density_slider->setRange(10, 100);
    density_slider->setValue(density);
    density_slider->setToolTip("Column density: higher = more columns (more matrix rain)");
    layout->addWidget(density_slider, 0, 1);
    density_label = new QLabel(QString::number(density) + "%");
    density_label->setMinimumWidth(36);
    layout->addWidget(density_label, 0, 2);

    layout->addWidget(new QLabel("Trail (%):"), 1, 0);
    trail_slider = new QSlider(Qt::Horizontal);
    trail_slider->setRange(10, 100);
    trail_slider->setValue(trail);
    trail_slider->setToolTip("Trail length: higher = longer fading tail behind each character");
    layout->addWidget(trail_slider, 1, 1);
    trail_label = new QLabel(QString::number(trail) + "%");
    trail_label->setMinimumWidth(36);
    layout->addWidget(trail_label, 1, 2);

    layout->addWidget(new QLabel("Char height (%):"), 2, 0);
    char_height_slider = new QSlider(Qt::Horizontal);
    char_height_slider->setRange(5, 100);
    char_height_slider->setValue(char_height);
    char_height_slider->setToolTip("Character height: higher = taller glyphs in the stream");
    layout->addWidget(char_height_slider, 2, 1);
    char_height_label = new QLabel(QString::number(char_height) + "%");
    char_height_label->setMinimumWidth(36);
    layout->addWidget(char_height_label, 2, 2);

    layout->addWidget(new QLabel("Char gap (%):"), 3, 0);
    char_gap_slider = new QSlider(Qt::Horizontal);
    char_gap_slider->setRange(0, 100);
    char_gap_slider->setValue(char_gap);
    char_gap_slider->setToolTip("Gap between characters in a column: higher = more space");
    layout->addWidget(char_gap_slider, 3, 1);
    char_gap_label = new QLabel(QString::number(char_gap) + "%");
    char_gap_label->setMinimumWidth(36);
    layout->addWidget(char_gap_label, 3, 2);

    layout->addWidget(new QLabel("Variation (%):"), 4, 0);
    char_variation_slider = new QSlider(Qt::Horizontal);
    char_variation_slider->setRange(0, 100);
    char_variation_slider->setValue(char_variation);
    char_variation_slider->setToolTip("Brightness variation: higher = more random bright/dim characters");
    layout->addWidget(char_variation_slider, 4, 1);
    char_variation_label = new QLabel(QString::number(char_variation) + "%");
    char_variation_label->setMinimumWidth(36);
    layout->addWidget(char_variation_label, 4, 2);

    layout->addWidget(new QLabel("Spacing (%):"), 5, 0);
    char_spacing_slider = new QSlider(Qt::Horizontal);
    char_spacing_slider->setRange(1, 100);
    char_spacing_slider->setValue(char_spacing);
    char_spacing_slider->setToolTip("Vertical spacing in stream: lower = denser flow, higher = more spread out");
    layout->addWidget(char_spacing_slider, 5, 1);
    char_spacing_label = new QLabel(QString::number(char_spacing) + "%");
    char_spacing_label->setMinimumWidth(36);
    layout->addWidget(char_spacing_label, 5, 2);

    layout->addWidget(new QLabel("Head brightness (%):"), 6, 0);
    head_brightness_slider = new QSlider(Qt::Horizontal);
    head_brightness_slider->setRange(0, 100);
    head_brightness_slider->setValue((int)head_brightness);
    head_brightness_slider->setToolTip("How much the leading character is whitish (0 = no white, 100 = full white tip). Only the very first LED is affected.");
    layout->addWidget(head_brightness_slider, 6, 1);
    head_brightness_label = new QLabel(QString::number(head_brightness) + "%");
    head_brightness_label->setMinimumWidth(36);
    layout->addWidget(head_brightness_label, 6, 2);

    AddWidgetToParent(w, parent);

    connect(density_slider, &QSlider::valueChanged, this, &Matrix::OnMatrixParameterChanged);
    connect(trail_slider, &QSlider::valueChanged, this, &Matrix::OnMatrixParameterChanged);
    connect(char_height_slider, &QSlider::valueChanged, this, &Matrix::OnMatrixParameterChanged);
    connect(char_gap_slider, &QSlider::valueChanged, this, &Matrix::OnMatrixParameterChanged);
    connect(char_variation_slider, &QSlider::valueChanged, this, &Matrix::OnMatrixParameterChanged);
    connect(char_spacing_slider, &QSlider::valueChanged, this, &Matrix::OnMatrixParameterChanged);
    connect(head_brightness_slider, &QSlider::valueChanged, this, &Matrix::OnMatrixParameterChanged);
}

void Matrix::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_MATRIX;
}

void Matrix::OnMatrixParameterChanged()
{
    if(density_slider)
    {
        density = density_slider->value();
        if(density_label) density_label->setText(QString::number(density) + "%");
    }
    if(trail_slider)
    {
        trail = trail_slider->value();
        if(trail_label) trail_label->setText(QString::number(trail) + "%");
    }
    if(char_height_slider)
    {
        char_height = char_height_slider->value();
        if(char_height_label) char_height_label->setText(QString::number(char_height) + "%");
    }
    if(char_gap_slider)
    {
        char_gap = char_gap_slider->value();
        if(char_gap_label) char_gap_label->setText(QString::number(char_gap) + "%");
    }
    if(char_variation_slider)
    {
        char_variation = char_variation_slider->value();
        if(char_variation_label) char_variation_label->setText(QString::number(char_variation) + "%");
    }
    if(char_spacing_slider)
    {
        char_spacing = char_spacing_slider->value();
        if(char_spacing_label) char_spacing_label->setText(QString::number(char_spacing) + "%");
    }
    if(head_brightness_slider)
    {
        head_brightness = (unsigned int)std::clamp(head_brightness_slider->value(), 0, 100);
        if(head_brightness_label) head_brightness_label->setText(QString::number(head_brightness) + "%");
    }
    emit ParametersChanged();
}

RGBColor Matrix::CalculateColor(float, float, float, float)
{
    return 0x00000000;
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
            /* Floor (y = min_y): stream along z, distance from floor */
            u = x;
            v = z;
            axis_value = z;
            axis_min = grid.min_z;
            axis_max = grid.max_z;
            face_distance = fabsf(y - grid.min_y);
            break;
        case 5:
        default:
            /* Ceiling (y = max_y): stream along z, distance from ceiling */
            u = x;
            v = z;
            axis_value = z;
            axis_min = grid.min_z;
            axis_max = grid.max_z;
            face_distance = fabsf(y - grid.max_y);
            break;
    }

    /* Let matrix rain extend into the room so it feels on everything (walls, floor, ceiling, and volume) */
    float room_extent = (grid.width + grid.height + grid.depth) / 3.0f;
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

    /* Fall downward: front of stream moves toward smaller position_along_axis on walls (high Y -> low Y) */
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
            /* Only the very first LED (leading edge) is the "head" – smooth blend so it’s not all white */
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
    float speed = GetScaledSpeed();
    float size_m = GetNormalizedSize();
    float color_cycle = time * GetScaledFrequency() * 0.02f;
    float detail = std::max(0.05f, GetScaledDetail());

    float col_spacing = 1.0f + (100.0f - density) * 0.04f;
    col_spacing = col_spacing / (0.6f + 0.4f * detail);

    float intensity = 0.0f;
    float head = 0.0f;

    for(int face_index = 0; face_index < 6; ++face_index)
    {
        float face_head = 0.0f;
        float face_value = ComputeFaceIntensity(face_index, x, y, z, time, grid, col_spacing, size_m, speed, &face_head);
        if(face_value > intensity)
        {
            intensity = face_value;
            head = face_head;
        }
    }

    /* Trail uses effect colors (green default) or rainbow when enabled */
    float color_pos = fmodf(color_cycle + (float)(((int)(x * 31 + y * 17 + z * 7) % 1000)) / 1000.0f, 1.0f);
    if(color_pos < 0.0f) color_pos += 1.0f;
    RGBColor trail_color = GetColorAtPosition(color_pos);
    unsigned char tr = (unsigned char)(trail_color & 0xFF);
    unsigned char tg = (unsigned char)((trail_color >> 8) & 0xFF);
    unsigned char tb = (unsigned char)((trail_color >> 16) & 0xFF);
    float bright = intensity;
    tr = (unsigned char)(tr * bright);
    tg = (unsigned char)(tg * bright);
    tb = (unsigned char)(tb * bright);

    /* Only the very first LED is whitish; blend with trail based on head brightness slider */
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

    if(density_slider) { density_slider->setValue(density); if(density_label) density_label->setText(QString::number(density) + "%"); }
    if(trail_slider) { trail_slider->setValue(trail); if(trail_label) trail_label->setText(QString::number(trail) + "%"); }
    if(char_height_slider) { char_height_slider->setValue(char_height); if(char_height_label) char_height_label->setText(QString::number(char_height) + "%"); }
    if(char_gap_slider) { char_gap_slider->setValue(char_gap); if(char_gap_label) char_gap_label->setText(QString::number(char_gap) + "%"); }
    if(char_variation_slider) { char_variation_slider->setValue(char_variation); if(char_variation_label) char_variation_label->setText(QString::number(char_variation) + "%"); }
    if(char_spacing_slider) { char_spacing_slider->setValue(char_spacing); if(char_spacing_label) char_spacing_label->setText(QString::number(char_spacing) + "%"); }
    if(head_brightness_slider) { head_brightness_slider->setValue((int)head_brightness); if(head_brightness_label) head_brightness_label->setText(QString::number(head_brightness) + "%"); }
}
