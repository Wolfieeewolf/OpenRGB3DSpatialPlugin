// SPDX-License-Identifier: GPL-2.0-only

#include "Matrix3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(Matrix3D);

Matrix3D::Matrix3D(QWidget* parent) : SpatialEffect3D(parent)
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
    density = 60;
    trail = 50;
    char_height = 15;  // Default character height
    char_gap = 15;     // Default gap size
    char_variation = 60; // Default variation
    char_spacing = 10; // Default spacing (denser stream)
    SetRainbowMode(false);
}

EffectInfo3D Matrix3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Matrix";
    info.effect_description = "Matrix-style code rain columns";
    info.category = "3D Spatial";
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

    info.default_speed_scale = 30.0f;  // Increased for better responsiveness (was 15.0f)
    info.default_frequency_scale = 8.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    // Rotation controls are in base class
    info.show_color_controls = true;
    return info;
}

void Matrix3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(new QLabel("Density:"), 0, 0);
    density_slider = new QSlider(Qt::Horizontal);
    density_slider->setRange(10, 100);
    density_slider->setValue(density);
    density_slider->setToolTip("Column density (higher = more columns)");
    layout->addWidget(density_slider, 0, 1);
    density_label = new QLabel(QString::number(density));
    density_label->setMinimumWidth(30);
    layout->addWidget(density_label, 0, 2);

    layout->addWidget(new QLabel("Trail Length:"), 1, 0);
    trail_slider = new QSlider(Qt::Horizontal);
    trail_slider->setRange(10, 100);
    trail_slider->setValue(trail);
    trail_slider->setToolTip("Trail length (higher = longer trails)");
    layout->addWidget(trail_slider, 1, 1);
    trail_label = new QLabel(QString::number(trail));
    trail_label->setMinimumWidth(30);
    layout->addWidget(trail_label, 1, 2);

    layout->addWidget(new QLabel("Char Height:"), 2, 0);
    char_height_slider = new QSlider(Qt::Horizontal);
    char_height_slider->setRange(5, 50);
    char_height_slider->setValue(char_height);
    char_height_slider->setToolTip("Character height (higher = taller characters)");
    layout->addWidget(char_height_slider, 2, 1);
    char_height_label = new QLabel(QString::number(char_height));
    char_height_label->setMinimumWidth(30);
    layout->addWidget(char_height_label, 2, 2);

    layout->addWidget(new QLabel("Char Gap:"), 3, 0);
    char_gap_slider = new QSlider(Qt::Horizontal);
    char_gap_slider->setRange(0, 50);
    char_gap_slider->setValue(char_gap);
    char_gap_slider->setToolTip("Gap between characters (higher = larger gaps)");
    layout->addWidget(char_gap_slider, 3, 1);
    char_gap_label = new QLabel(QString::number(char_gap));
    char_gap_label->setMinimumWidth(30);
    layout->addWidget(char_gap_label, 3, 2);

    layout->addWidget(new QLabel("Char Variation:"), 4, 0);
    char_variation_slider = new QSlider(Qt::Horizontal);
    char_variation_slider->setRange(0, 100);
    char_variation_slider->setValue(char_variation);
    char_variation_slider->setToolTip("Character brightness variation (higher = more variation)");
    layout->addWidget(char_variation_slider, 4, 1);
    char_variation_label = new QLabel(QString::number(char_variation));
    char_variation_label->setMinimumWidth(30);
    layout->addWidget(char_variation_label, 4, 2);

    layout->addWidget(new QLabel("Char Spacing:"), 5, 0);
    char_spacing_slider = new QSlider(Qt::Horizontal);
    char_spacing_slider->setRange(1, 50);
    char_spacing_slider->setValue(char_spacing);
    char_spacing_slider->setToolTip("Character spacing in stream (lower = denser, continuous stream)");
    layout->addWidget(char_spacing_slider, 5, 1);
    char_spacing_label = new QLabel(QString::number(char_spacing));
    char_spacing_label->setMinimumWidth(30);
    layout->addWidget(char_spacing_label, 5, 2);

    AddWidgetToParent(w, parent);

    connect(density_slider, &QSlider::valueChanged, this, &Matrix3D::OnMatrixParameterChanged);
    connect(trail_slider, &QSlider::valueChanged, this, &Matrix3D::OnMatrixParameterChanged);
    connect(char_height_slider, &QSlider::valueChanged, this, &Matrix3D::OnMatrixParameterChanged);
    connect(char_gap_slider, &QSlider::valueChanged, this, &Matrix3D::OnMatrixParameterChanged);
    connect(char_variation_slider, &QSlider::valueChanged, this, &Matrix3D::OnMatrixParameterChanged);
    connect(char_spacing_slider, &QSlider::valueChanged, this, &Matrix3D::OnMatrixParameterChanged);
}

void Matrix3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_MATRIX;
}

void Matrix3D::OnMatrixParameterChanged()
{
    if(density_slider)
    {
        density = density_slider->value();
        if(density_label) density_label->setText(QString::number(density));
    }
    if(trail_slider)
    {
        trail = trail_slider->value();
        if(trail_label) trail_label->setText(QString::number(trail));
    }
    if(char_height_slider)
    {
        char_height = char_height_slider->value();
        if(char_height_label) char_height_label->setText(QString::number(char_height));
    }
    if(char_gap_slider)
    {
        char_gap = char_gap_slider->value();
        if(char_gap_label) char_gap_label->setText(QString::number(char_gap));
    }
    if(char_variation_slider)
    {
        char_variation = char_variation_slider->value();
        if(char_variation_label) char_variation_label->setText(QString::number(char_variation));
    }
    if(char_spacing_slider)
    {
        char_spacing = char_spacing_slider->value();
        if(char_spacing_label) char_spacing_label->setText(QString::number(char_spacing));
    }
    emit ParametersChanged();
}

RGBColor Matrix3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

float Matrix3D::ComputeFaceIntensity(int face,
                                     float x,
                                     float y,
                                     float z,
                                     float time,
                                     const GridContext3D& grid,
                                     float column_spacing,
                                     float size_normalized,
                                     float speed_scale) const
{
    // Early return if grid is invalid to prevent crashes
    if(grid.min_x >= grid.max_x || grid.min_y >= grid.max_y || grid.min_z >= grid.max_z)
    {
        return 0.0f; // Invalid grid, return no intensity
    }
    
    // Validate all input parameters to prevent crashes
    if(column_spacing < 0.001f) column_spacing = 0.001f;
    if(size_normalized < 0.001f) size_normalized = 0.001f;
    if(size_normalized > 10.0f) size_normalized = 10.0f;
    // Clamp time and coordinates to reasonable ranges
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
        case 0: // Left wall - code falls top to bottom (Y axis)
            u = z;  // Horizontal position along wall
            v = y;  // Vertical position (top to bottom)
            axis_value = y;  // Code falls along Y
            axis_min = grid.min_y;  // Bottom
            axis_max = grid.max_y;  // Top
            face_distance = fabsf(x - grid.min_x);
            break;
        case 1: // Right wall - code falls top to bottom (Y axis)
            u = z;  // Horizontal position along wall
            v = y;  // Vertical position (top to bottom)
            axis_value = y;  // Code falls along Y
            axis_min = grid.min_y;  // Bottom
            axis_max = grid.max_y;  // Top
            face_distance = fabsf(x - grid.max_x);
            break;
        case 2: // Front wall - code falls top to bottom (Y axis)
            u = x;  // Horizontal position along wall
            v = y;  // Vertical position (top to bottom)
            axis_value = y;  // Code falls along Y
            axis_min = grid.min_y;  // Bottom
            axis_max = grid.max_y;  // Top
            face_distance = fabsf(z - grid.min_z);
            break;
        case 3: // Back wall - code falls top to bottom (Y axis)
            u = x;  // Horizontal position along wall
            v = y;  // Vertical position (top to bottom)
            axis_value = y;  // Code falls along Y
            axis_min = grid.min_y;  // Bottom
            axis_max = grid.max_y;  // Top
            face_distance = fabsf(z - grid.max_z);
            break;
        case 4: // Floor - code moves toward viewer (Z axis, from back to front)
            u = x;  // Horizontal position
            v = z;  // Depth position (back to front)
            axis_value = z;  // Code moves along Z toward viewer
            axis_min = grid.min_z;  // Back
            axis_max = grid.max_z;  // Front (toward viewer)
            face_distance = fabsf(z - grid.min_z);
            break;
        case 5: // Ceiling - code moves toward viewer (Z axis, from back to front)
        default:
            u = x;  // Horizontal position
            v = z;  // Depth position (back to front)
            axis_value = z;  // Code moves along Z toward viewer
            axis_min = grid.min_z;  // Back
            axis_max = grid.max_z;  // Front (toward viewer)
            face_distance = fabsf(z - grid.max_z);
            break;
    }

    // Safety check for column_spacing
    if(column_spacing < 0.001f) column_spacing = 0.001f;
    
    // Validate u and v before division to prevent NaN/Inf issues
    if(u < -10000.0f || u > 10000.0f) u = 0.0f;
    if(v < -10000.0f || v > 10000.0f) v = 0.0f;
    
    // Calculate column indices with safety checks
    float col_u_float = u / column_spacing;
    float col_v_float = v / column_spacing;
    // Clamp before floorf to prevent overflow
    col_u_float = fmax(-1000.0f, fmin(1000.0f, col_u_float));
    col_v_float = fmax(-1000.0f, fmin(1000.0f, col_v_float));
    
    int column_u = (int)floorf(col_u_float);
    int column_v = (int)floorf(col_v_float);
    // Final clamp to prevent integer overflow
    column_u = (column_u < -1000) ? -1000 : (column_u > 1000 ? 1000 : column_u);
    column_v = (column_v < -1000) ? -1000 : (column_v > 1000 ? 1000 : column_v);
    // Fix operator precedence: use parentheses for clarity and correctness
    int column_id = (column_u * 73856093) ^ (column_v * 19349663);

    float offset = ((column_id & 255) / 255.0f) * 10.0f;
    offset = fmax(0.0f, fmin(10.0f, offset)); // Clamp offset
    
    // Read and validate all member variables at once to prevent race conditions
    // These are simple integer reads which should be atomic on most platforms
    unsigned int local_char_spacing = char_spacing;
    unsigned int local_char_height = char_height;
    unsigned int local_char_gap = char_gap;
    unsigned int local_char_variation = char_variation;
    unsigned int local_trail = trail;
    
    // Validate all member variables with safe defaults
    unsigned int safe_char_spacing = (local_char_spacing >= 1 && local_char_spacing <= 50) ? local_char_spacing : 10;
    unsigned int safe_char_height = (local_char_height >= 5 && local_char_height <= 50) ? local_char_height : 15;
    unsigned int safe_char_gap = (local_char_gap <= 50) ? local_char_gap : 15;
    unsigned int safe_char_variation = (local_char_variation <= 100) ? local_char_variation : 60;
    
    // Character height scales with size and user setting
    // Increased base range for better visibility (was 0.05-0.30, now 0.10-0.50)
    float char_height_base = 0.10f + (safe_char_height / 50.0f) * 0.40f; // 0.10 to 0.50 base
    float char_height_actual = char_height_base * size_normalized;
    
    // Safety check: prevent division by zero and clamp to reasonable range
    if(char_height_actual < 0.001f) char_height_actual = 0.001f;
    if(char_height_actual > 10.0f) char_height_actual = 10.0f;
    
    // Character spacing in the continuous stream (lower = denser)
    float spacing_factor = safe_char_spacing / 50.0f; // 0.02 to 1.0
    float char_spacing_actual = char_height_actual * (0.5f + spacing_factor * 1.5f); // Spacing between character centers
    
    // Safety check: prevent division by zero and clamp to reasonable range
    if(char_spacing_actual < 0.001f) char_spacing_actual = 0.001f;
    if(char_spacing_actual > 20.0f) char_spacing_actual = 20.0f;
    
    // Gap size as percentage of character height
    float gap_ratio = safe_char_gap / 100.0f; // 0.0 to 0.5 (gap can be up to 50% of char height)
    gap_ratio = fmax(0.0f, fmin(0.5f, gap_ratio)); // Clamp to 0-0.5
    float char_body_ratio = 1.0f - gap_ratio; // Rest is character body
    
    // Continuous endless stream: like rain - characters fall continuously without gaps
    // For walls (faces 0-3): code falls from top (axis_max) to bottom (axis_min)
    // For floor/ceiling (faces 4-5): code moves from back (axis_min) to front (axis_max) toward viewer
    
    // Safety checks
    if(char_spacing_actual < 0.001f) char_spacing_actual = 0.001f;
    if(char_spacing_actual > 100.0f) char_spacing_actual = 100.0f; // Reasonable upper limit
    
    // Wrap time to prevent overflow - use a very long period (24 hours) to avoid visible restarts
    // Safety check: ensure time is valid before fmodf
    float safe_time_for_wrap = fmax(-1000000.0f, fmin(1000000.0f, time));
    const float wrap_period = 86400.0f; // 24 hours in seconds
    float wrapped_time = fmodf(safe_time_for_wrap, wrap_period);
    if(wrapped_time < 0.0f) wrapped_time += wrap_period;
    // Final validation
    if(wrapped_time < 0.0f || wrapped_time >= wrap_period) wrapped_time = 0.0f;
    
    float fall_speed = fmax(0.1f, fmin(100.0f, speed_scale)); // Clamp speed to reasonable range
    float safe_offset = fmax(-100.0f, fmin(100.0f, offset)); // Clamp offset
    
    // Calculate stream time with wrapped time
    float stream_time = wrapped_time * fall_speed + safe_offset;
    
    // Validate axis values to prevent crashes
    if(axis_min > axis_max)
    {
        // Invalid axis range, swap them
        float temp = axis_min;
        axis_min = axis_max;
        axis_max = temp;
    }
    if(axis_max - axis_min < 0.001f)
    {
        // Very small or zero range, use defaults
        axis_min = 0.0f;
        axis_max = 1.0f;
    }
    
    // Clamp axis_value to valid range
    axis_value = fmax(axis_min - 10.0f, fmin(axis_max + 10.0f, axis_value));
    
    // Calculate position along the axis
    float position_along_axis;
    if(face >= 4) // Floor or ceiling
    {
        // Code moves from back to front (toward viewer)
        position_along_axis = axis_value - axis_min;
    }
    else // Walls
    {
        // Code falls from top to bottom
        position_along_axis = axis_max - axis_value;
    }
    
    // Clamp position_along_axis to prevent overflow
    position_along_axis = fmax(-1000.0f, fmin(1000.0f, position_along_axis));
    
    // Clamp stream_time to prevent overflow
    stream_time = fmax(-10000.0f, fmin(10000.0f, stream_time));
    
    // Calculate stream position: position minus time-based movement
    // This creates continuous falling/moving effect
    float stream_pos = position_along_axis - stream_time;
    
    // Clamp stream_pos before modulo to prevent issues
    stream_pos = fmax(-10000.0f, fmin(10000.0f, stream_pos));
    
    // Use modulo to create seamless, continuous wrapping
    // This ensures characters appear at regular intervals with no gaps
    // Safety check for char_spacing_actual before modulo
    if(char_spacing_actual < 0.001f) char_spacing_actual = 0.001f;
    if(char_spacing_actual > 100.0f) char_spacing_actual = 100.0f;
    
    // Validate stream_pos before fmodf to prevent NaN/Inf issues
    if(stream_pos < -100000.0f || stream_pos > 100000.0f) stream_pos = 0.0f;
    
    // Use fabsf to ensure positive value for fmodf, then adjust
    float abs_stream_pos = fabsf(stream_pos);
    abs_stream_pos = fmodf(abs_stream_pos, char_spacing_actual);
    stream_pos = abs_stream_pos;
    // Final clamp
    stream_pos = fmax(0.0f, fmin(char_spacing_actual - 0.0001f, stream_pos));
    
    // Normalized position within the character spacing unit (0 to 1)
    // Safety check before division
    if(char_spacing_actual < 0.001f) char_spacing_actual = 0.001f;
    float char_local = stream_pos / char_spacing_actual;
    char_local = fmax(0.0f, fmin(1.0f, char_local));
    
    // Calculate which character "slot" we're in (for variation and trail calculations)
    // Calculate this before the modulo for accurate trail distance
    // Safety check before division
    if(char_spacing_actual < 0.001f) char_spacing_actual = 0.001f;
    float char_index_value = (position_along_axis - stream_time) / char_spacing_actual;
    char_index_value = fmax(-10000.0f, fmin(10000.0f, char_index_value));
    float char_index = floorf(char_index_value);
    char_index = fmax(-1000.0f, fmin(1000.0f, char_index));
    
    float intensity = 0.0f;
    
    // Check if we're within a character (character occupies first portion of spacing unit)
    float char_portion = char_height_actual / char_spacing_actual; // What portion of spacing is character
    if(char_portion > 1.0f) char_portion = 1.0f; // Clamp to max 1.0
    if(char_portion < 0.001f) char_portion = 0.001f; // Safety check
    
    if(char_local < char_portion && char_portion > 0.001f)
    {
        // We're within a character - determine if body or gap
        float char_internal = char_local / char_portion; // Position within character (0-1)
        char_internal = fmax(0.0f, fmin(1.0f, char_internal)); // Clamp to 0-1
        
        if(char_internal < char_body_ratio)
        {
            // Character body - full brightness
            intensity = 1.0f;
        }
        else
        {
            // Character gap - dim
            intensity = 0.2f;
        }
        
        // Add character variation (some characters brighter/dimmer)
        int safe_char_index = (int)fmax(-10000.0f, fmin(10000.0f, char_index));
        float char_seed = (float)(safe_char_index * 131 + column_id);
        float variation_amount = safe_char_variation / 100.0f; // 0.0 to 1.0
        variation_amount = fmax(0.0f, fmin(1.0f, variation_amount)); // Clamp
        
        if(variation_amount > 0.01f)
        {
            // Safety check for char_seed before fmodf
            float safe_char_seed = fmax(-1000000.0f, fmin(1000000.0f, char_seed * 0.1f));
            float char_brightness = 0.5f + 0.5f * fmodf(safe_char_seed, 1.0f) * variation_amount;
            char_brightness = fmax(0.0f, fmin(1.0f, char_brightness)); // Clamp
            intensity *= char_brightness;
        }
        
        // Fade based on trail length (visible distance in the continuous stream)
        // For a continuous stream, we fade based on how many character slots behind we are
        // This creates a trailing fade effect while maintaining continuity
        unsigned int safe_trail = (local_trail <= 100) ? local_trail : 50; // Validate trail
        float trail_char_count = 3.0f + (safe_trail / 100.0f) * 12.0f; // 3-15 characters visible
        trail_char_count = fmax(1.0f, fmin(1000.0f, trail_char_count)); // Safety check with upper limit
        
        // Calculate distance in character slots (positive = behind wavefront, negative = ahead)
        // For continuous stream, we show characters in a repeating pattern
        // Trail fade applies to characters that are many slots behind
        // Clamp char_index before using fabsf
        float safe_char_index_float = fmax(-10000.0f, fmin(10000.0f, char_index));
        float slots_behind = fabsf(safe_char_index_float);
        slots_behind = fmax(0.0f, fmin(10000.0f, slots_behind)); // Clamp to prevent overflow
        
        if(slots_behind > trail_char_count && trail_char_count > 0.001f)
        {
            float fade_start = trail_char_count;
            float fade_end = trail_char_count * 2.0f;
            fade_end = fmax(fade_start + 0.001f, fmin(2000.0f, fade_end)); // Clamp fade_end
            
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
                intensity = 0.0f; // Too far behind, fade to zero
            }
        }
    }
    
    // Add gap variation (some columns have gaps)
    // Safety check: ensure column_id is valid
    int safe_column_id = column_id;
    if(safe_column_id < 0) safe_column_id = -safe_column_id; // Make positive
    float gap = fmodf((float)((safe_column_id >> 8) & 1023), 10.0f) / 10.0f;
    gap = fmax(0.0f, fmin(1.0f, gap)); // Clamp to valid range
    float gap_factor = 0.6f + 0.4f * (gap > 0.3f ? 1.0f : gap * 3.33f);
    gap_factor = fmax(0.0f, fmin(1.0f, gap_factor));
    intensity *= gap_factor;

    // Enhanced face falloff - much softer to work on devices anywhere in room
    // Devices positioned away from room boundaries will still show the Matrix effect
    // Clamp face_distance to prevent expf overflow
    face_distance = fmax(0.0f, fmin(1000.0f, face_distance));
    // Validate face_distance is not NaN or Inf
    if(face_distance != face_distance) face_distance = 0.0f; // NaN check
    float exp_arg = -face_distance * 0.5f;
    exp_arg = fmax(-100.0f, fmin(100.0f, exp_arg)); // Clamp exp argument to prevent overflow
    // Validate exp_arg before expf
    if(exp_arg != exp_arg) exp_arg = 0.0f; // NaN check
    float face_falloff = 0.3f + 0.7f * expf(exp_arg);
    // Validate result
    if(face_falloff != face_falloff || face_falloff < 0.0f) face_falloff = 0.3f; // NaN or negative check
    face_falloff = fmax(0.0f, fmin(1.0f, face_falloff));
    
    // Add ambient glow for whole-room Matrix presence including devices
    float ambient = 0.1f * (1.0f - fmin(1.0f, face_distance * 0.05f));
    ambient = fmax(0.0f, fmin(1.0f, ambient));

    // Apply face falloff and ambient
    intensity = intensity * face_falloff + ambient;
    
    // Validate intensity is not NaN or Inf before final operations
    if(intensity != intensity || intensity < -1000.0f || intensity > 1000.0f)
    {
        intensity = 0.0f; // Reset to safe value if invalid
    }
    
    // Clamp intensity to valid range before final operations
    intensity = fmax(0.0f, fmin(10.0f, intensity));
    
    // Increase brightness multiplier for better visibility (was 1.6, now 2.0)
    intensity *= 2.0f;
    
    // Final validation and clamp to 0-1 range
    if(intensity != intensity || intensity < 0.0f) intensity = 0.0f;
    if(intensity > 1.0f) intensity = 1.0f;
    intensity = fmax(0.0f, fmin(1.0f, intensity));
    return intensity;
}

RGBColor Matrix3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    // Matrix-style rain on surfaces (room walls/floor/ceiling and device surfaces)
    // Walls: code falls top to bottom (Y axis)
    // Floor/Ceiling: code moves toward viewer (Z axis)
    // Devices: effect works based on which surface face they're closest to
    float speed = GetScaledSpeed();
    float size_m = GetNormalizedSize();

    // Column spacing: higher density -> smaller spacing
    float col_spacing = 1.0f + (100.0f - density) * 0.04f; // 1..5 units

    float intensity = 0.0f;
    
    // Check all room faces (walls, floor, ceiling)
    // This also works for devices positioned in the room - the effect will apply
    // based on which surface face the LED is closest to, matching the device's orientation
    for(int face_index = 0; face_index < 6; ++face_index)
    {
        float face_value = ComputeFaceIntensity(face_index, x, y, z, time, grid, col_spacing, size_m, speed);
        intensity = fmax(intensity, face_value);
    }

    // Matrix-green color
    unsigned char r = 0;
    // Global brightness is applied by PostProcessColorGrid
    unsigned char g = (unsigned char)(255 * intensity);
    unsigned char b = 0;
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Matrix3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["density"] = density;
    j["trail"] = trail;
    j["char_height"] = char_height;
    j["char_gap"] = char_gap;
    j["char_variation"] = char_variation;
    j["char_spacing"] = char_spacing;
    return j;
}

void Matrix3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("density")) density = settings["density"];
    if(settings.contains("trail")) trail = settings["trail"];
    if(settings.contains("char_height")) char_height = settings["char_height"];
    if(settings.contains("char_gap")) char_gap = settings["char_gap"];
    if(settings.contains("char_variation")) char_variation = settings["char_variation"];
    if(settings.contains("char_spacing")) char_spacing = settings["char_spacing"];

    if(density_slider) density_slider->setValue(density);
    if(trail_slider) trail_slider->setValue(trail);
    if(char_height_slider) char_height_slider->setValue(char_height);
    if(char_gap_slider) char_gap_slider->setValue(char_gap);
    if(char_variation_slider) char_variation_slider->setValue(char_variation);
    if(char_spacing_slider) char_spacing_slider->setValue(char_spacing);
}
