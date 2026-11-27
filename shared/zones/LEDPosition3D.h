// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LEDPOSITION3D_H
#define LEDPOSITION3D_H

#include "RGBController.h"

struct Vector3D
{
    float x;
    float y;
    float z;
};

struct Rotation3D
{
    float x;
    float y;
    float z;
};

struct Transform3D
{
    Vector3D    position;
    Rotation3D  rotation;
    Vector3D    scale;
};

struct LEDPosition3D
{
    RGBControllerInterface*  controller;
    unsigned int    zone_idx;
    unsigned int    led_idx;
    Vector3D        local_position;
    Vector3D        world_position;
    Vector3D        room_position;
    RGBColor        preview_color;
    /* Action Editor afterglow: colour at trail_start_ms, fading to black over trail_ms. */
    RGBColor        trail_color = 0;
    int             trail_ms = 0;
    int             trail_start_ms = 0;
    /* Action frame this LED was last cleared in (composite frames clear once). */
    unsigned int    frame_stamp = 0;
    /* Action Editor mix in linear light for frame mix_stamp; preview_color is its 8-bit
       display copy. Stacked layers blend here at full precision. */
    float           mix_r = 0.0f;
    float           mix_g = 0.0f;
    float           mix_b = 0.0f;
    unsigned int    mix_stamp = 0;
};

class VirtualController3D;

struct ControllerTransform
{
    RGBControllerInterface*      controller;
    VirtualController3D* virtual_controller;
    Transform3D         transform;
    std::vector<LEDPosition3D> led_positions;
    RGBColor            display_color;
    bool                hidden_by_virtual;

    float               led_spacing_mm_x;
    float               led_spacing_mm_y;
    float               led_spacing_mm_z;

    int                 granularity;
    int                 item_idx;
    int                 linked_reference_point_index = -1;

    bool                world_positions_dirty;

    /** This placed device's light output at full white, lumens (0 = not set). For custom
        controllers their own setting wins; this covers regular devices. */
    float               light_output_lm = 0.0f;
};

#endif
