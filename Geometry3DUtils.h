/*---------------------------------------------------------*\
| Geometry3DUtils.h                                         |
|                                                           |
|   3D geometry utilities for spatial calculations         |
|                                                           |
|   Date: 2025-10-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef GEOMETRY3DUTILS_H
#define GEOMETRY3DUTILS_H

#include "LEDPosition3D.h"
#include "DisplayPlane3D.h"
#include <algorithm>
#include <cmath>

namespace Geometry3D
{
    /**
     * @brief Result of projecting a point onto a plane
     */
    struct PlaneProjection
    {
        float   u;              // Horizontal coordinate on plane [0,1]
        float   v;              // Vertical coordinate on plane [0,1]
        float   distance;       // Distance from point to plane (mm)
        bool    is_in_front;    // Is the point in front of the plane?
        bool    is_valid;       // Is this projection valid?
    };

    /**
     * @brief Compute a rotation matrix from Euler angles (XYZ order)
     * @param rotation_deg Rotation in degrees {x, y, z}
     * @param matrix Output 3x3 matrix (row-major)
     */
    inline void ComputeRotationMatrix(const Rotation3D& rotation_deg, float matrix[9])
    {
        const float deg_to_rad = 3.14159265359f / 180.0f;
        float rx = rotation_deg.x * deg_to_rad;
        float ry = rotation_deg.y * deg_to_rad;
        float rz = rotation_deg.z * deg_to_rad;

        float cx = cosf(rx), sx = sinf(rx);
        float cy = cosf(ry), sy = sinf(ry);
        float cz = cosf(rz), sz = sinf(rz);

        // XYZ Euler rotation matrix
        matrix[0] = cy * cz;
        matrix[1] = -cy * sz;
        matrix[2] = sy;

        matrix[3] = cx * sz + sx * sy * cz;
        matrix[4] = cx * cz - sx * sy * sz;
        matrix[5] = -sx * cy;

        matrix[6] = sx * sz - cx * sy * cz;
        matrix[7] = sx * cz + cx * sy * sz;
        matrix[8] = cx * cy;
    }

    /**
     * @brief Transform a point by rotation matrix
     */
    inline Vector3D RotateVector(const Vector3D& v, const float matrix[9])
    {
        Vector3D result;
        result.x = matrix[0] * v.x + matrix[1] * v.y + matrix[2] * v.z;
        result.y = matrix[3] * v.x + matrix[4] * v.y + matrix[5] * v.z;
        result.z = matrix[6] * v.x + matrix[7] * v.y + matrix[8] * v.z;
        return result;
    }

    /**
     * @brief Project an LED world position onto a display plane
     *
     * @param led_position World position of the LED (in mm)
     * @param plane The display plane to project onto
     * @return PlaneProjection result containing UV coordinates and distance
     *
     * The plane is defined by its transform (position + rotation) and dimensions.
     * The plane's local coordinate system:
     *   - Local +X is right (increasing U)
     *   - Local +Y is up (increasing V)
     *   - Local +Z is forward (plane normal, pointing away from screen)
     *
     * U and V are normalized coordinates [0,1] relative to the plane's width/height.
     */
    inline PlaneProjection ProjectPointOntoPlane(const Vector3D& led_position, const DisplayPlane3D& plane)
    {
        PlaneProjection result;
        result.is_valid = false;
        result.u = 0.0f;
        result.v = 0.0f;
        result.distance = 0.0f;
        result.is_in_front = false;

        const Transform3D& transform = plane.GetTransform();

        // Compute rotation matrix for the plane
        float rotation_matrix[9];
        ComputeRotationMatrix(transform.rotation, rotation_matrix);

        // Plane normal is local +Z axis after rotation
        Vector3D plane_normal;
        plane_normal.x = rotation_matrix[2];
        plane_normal.y = rotation_matrix[5];
        plane_normal.z = rotation_matrix[8];

        // Vector from plane center to LED
        Vector3D to_led;
        to_led.x = led_position.x - transform.position.x;
        to_led.y = led_position.y - transform.position.y;
        to_led.z = led_position.z - transform.position.z;

        // Distance from LED to plane (signed, positive = in front)
        float dot = to_led.x * plane_normal.x + to_led.y * plane_normal.y + to_led.z * plane_normal.z;

        // Use full 3D distance for ambilight falloff, not just perpendicular distance
        result.distance = sqrtf(to_led.x * to_led.x + to_led.y * to_led.y + to_led.z * to_led.z);
        result.is_in_front = (dot > 0.0f);

        // Project LED onto plane (find intersection of LED-to-plane perpendicular)
        Vector3D point_on_plane;
        point_on_plane.x = led_position.x - plane_normal.x * dot;
        point_on_plane.y = led_position.y - plane_normal.y * dot;
        point_on_plane.z = led_position.z - plane_normal.z * dot;

        // Transform point to plane's local space
        Vector3D relative;
        relative.x = point_on_plane.x - transform.position.x;
        relative.y = point_on_plane.y - transform.position.y;
        relative.z = point_on_plane.z - transform.position.z;

        // Inverse rotate (transpose of rotation matrix for orthonormal matrix)
        Vector3D local;
        local.x = rotation_matrix[0] * relative.x + rotation_matrix[3] * relative.y + rotation_matrix[6] * relative.z;
        local.y = rotation_matrix[1] * relative.x + rotation_matrix[4] * relative.y + rotation_matrix[7] * relative.z;
        local.z = rotation_matrix[2] * relative.x + rotation_matrix[5] * relative.y + rotation_matrix[8] * relative.z;

        // Convert to UV coordinates [0,1]
        // Plane extends from [-width/2, +width/2] in X and [-height/2, +height/2] in Y
        float half_width = plane.GetWidthMM() * 0.5f;
        float half_height = plane.GetHeightMM() * 0.5f;

        result.u = (local.x + half_width) / plane.GetWidthMM();
        result.v = (local.y + half_height) / plane.GetHeightMM();

        // UV coordinates are correct as-is for transparent screen viewing
        // LEDs behind screen naturally see mirrored view (like looking through glass)

        result.is_valid = true;
        return result;
    }

    /**
     * @brief Ray-trace from LED toward screen to find intersection point
     *
     * @param led_position World position of the LED (in mm)
     * @param view_direction Direction the LED is "looking" (normalized)
     * @param plane The display plane to intersect with
     * @return PlaneProjection result containing UV coordinates at intersection
     *
     * This performs true ray-tracing: cast a ray from the LED in the viewing direction
     * and find where it hits the screen plane. This is what the LED actually "sees".
     */
    inline PlaneProjection RayTracePlane(const Vector3D& led_position, const Vector3D& view_direction, const DisplayPlane3D& plane)
    {
        PlaneProjection result;
        result.is_valid = false;
        result.u = 0.0f;
        result.v = 0.0f;
        result.distance = 0.0f;
        result.is_in_front = false;

        const Transform3D& transform = plane.GetTransform();

        // Compute rotation matrix for the plane
        float rotation_matrix[9];
        ComputeRotationMatrix(transform.rotation, rotation_matrix);

        // Plane normal is local +Z axis after rotation (pointing toward viewer/LED)
        Vector3D plane_normal;
        plane_normal.x = rotation_matrix[2];
        plane_normal.y = rotation_matrix[5];
        plane_normal.z = rotation_matrix[8];

        // Ray-plane intersection
        // Ray: P(t) = led_position + t * view_direction
        // Plane: dot(P - plane_center, plane_normal) = 0
        // Solve: dot(led_position + t*view_direction - plane_center, plane_normal) = 0

        float denominator = view_direction.x * plane_normal.x +
                           view_direction.y * plane_normal.y +
                           view_direction.z * plane_normal.z;

        // Check if ray is parallel to plane
        if (fabsf(denominator) < 0.0001f)
        {
            return result; // No intersection
        }

        Vector3D to_plane;
        to_plane.x = transform.position.x - led_position.x;
        to_plane.y = transform.position.y - led_position.y;
        to_plane.z = transform.position.z - led_position.z;

        float numerator = to_plane.x * plane_normal.x +
                         to_plane.y * plane_normal.y +
                         to_plane.z * plane_normal.z;

        float t = numerator / denominator;

        // Check if intersection is behind the LED (negative t)
        if (t < 0.0f)
        {
            return result; // Intersection is behind the LED
        }

        // Calculate intersection point
        Vector3D intersection;
        intersection.x = led_position.x + view_direction.x * t;
        intersection.y = led_position.y + view_direction.y * t;
        intersection.z = led_position.z + view_direction.z * t;

        // Distance from LED to intersection
        result.distance = t;
        result.is_in_front = true;

        // Transform intersection point to plane's local space
        Vector3D relative;
        relative.x = intersection.x - transform.position.x;
        relative.y = intersection.y - transform.position.y;
        relative.z = intersection.z - transform.position.z;

        // Inverse rotate (transpose of rotation matrix for orthonormal matrix)
        Vector3D local;
        local.x = rotation_matrix[0] * relative.x + rotation_matrix[3] * relative.y + rotation_matrix[6] * relative.z;
        local.y = rotation_matrix[1] * relative.x + rotation_matrix[4] * relative.y + rotation_matrix[7] * relative.z;
        local.z = rotation_matrix[2] * relative.x + rotation_matrix[5] * relative.y + rotation_matrix[8] * relative.z;

        // Convert to UV coordinates [0,1]
        float half_width = plane.GetWidthMM() * 0.5f;
        float half_height = plane.GetHeightMM() * 0.5f;

        result.u = (local.x + half_width) / plane.GetWidthMM();
        result.v = (local.y + half_height) / plane.GetHeightMM();

        result.is_valid = true;
        return result;
    }

    /**
     * @brief Compute ambilight falloff with feathered edge
     *
     * Creates a soft, feathered fade at the edge of the light range - perfect for ambilight.
     * Light is at full brightness up to (max_range - feather_width), then smoothly fades to black.
     *
     * @param distance Distance from LED to screen (mm)
     * @param max_range Maximum light range (mm) - where light reaches 0%
     * @param feather_percent Percentage of range to use for feathering (0-100)
     * @return Intensity multiplier [0,1]
     */
    inline float ComputeFalloff(float distance, float max_range, float feather_percent = 30.0f)
    {
        if (max_range <= 0.0f)
        {
            return 1.0f;
        }

        // Calculate feather width (default 30% of range)
        float feather_width = max_range * (feather_percent / 100.0f);
        float core_range = max_range - feather_width;

        // Full brightness in core range
        if (distance <= core_range)
        {
            return 1.0f;
        }

        // Completely dark beyond max range
        if (distance >= max_range)
        {
            return 0.0f;
        }

        // Smooth feathered transition using smoothstep
        float t = (distance - core_range) / feather_width;
        t = fmaxf(0.0f, fminf(1.0f, t));

        // Smoothstep for natural fade (creates that "fluffy" edge)
        float fade = 1.0f - (t * t * (3.0f - 2.0f * t));

        return fade;
    }

    /**
     * @brief Compute angular/wrap falloff factor for immersive curved effect
     *
     * This calculates how much an LED is "off to the side" of a screen based on
     * the viewing angle, creating a curved/wrapped immersive feeling.
     *
     * @param led_position World position of the LED (mm)
     * @param plane The display plane
     * @param horizontal_wrap_angle Max horizontal wrap angle in degrees (0-180)
     * @param vertical_wrap_angle Max vertical wrap angle in degrees (0-90)
     * @param wrap_strength How aggressively to fade outside wrap angle (1.0 = normal)
     * @return Angular intensity multiplier [0,1]
     */
    inline float ComputeAngularFalloff(const Vector3D& led_position, const DisplayPlane3D& plane,
                                      float horizontal_wrap_angle, float vertical_wrap_angle,
                                      float wrap_strength = 1.0f)
    {
        const Transform3D& transform = plane.GetTransform();

        // Compute rotation matrix for the plane
        float rotation_matrix[9];
        ComputeRotationMatrix(transform.rotation, rotation_matrix);

        // Plane normal is local +Z axis (pointing away from screen)
        Vector3D plane_normal;
        plane_normal.x = rotation_matrix[2];
        plane_normal.y = rotation_matrix[5];
        plane_normal.z = rotation_matrix[8];

        // Vector from plane center to LED
        Vector3D to_led;
        to_led.x = led_position.x - transform.position.x;
        to_led.y = led_position.y - transform.position.y;
        to_led.z = led_position.z - transform.position.z;

        // Normalize to_led
        float led_dist = sqrtf(to_led.x * to_led.x + to_led.y * to_led.y + to_led.z * to_led.z);
        if (led_dist < 0.001f) return 1.0f; // LED at plane center

        to_led.x /= led_dist;
        to_led.y /= led_dist;
        to_led.z /= led_dist;

        // Compute angle between LED direction and plane normal
        float dot_normal = to_led.x * plane_normal.x + to_led.y * plane_normal.y + to_led.z * plane_normal.z;

        // Get plane's right and up vectors
        Vector3D plane_right;
        plane_right.x = rotation_matrix[0];
        plane_right.y = rotation_matrix[3];
        plane_right.z = rotation_matrix[6];

        Vector3D plane_up;
        plane_up.x = rotation_matrix[1];
        plane_up.y = rotation_matrix[4];
        plane_up.z = rotation_matrix[7];

        // Project LED direction onto horizontal and vertical axes
        float dot_right = to_led.x * plane_right.x + to_led.y * plane_right.y + to_led.z * plane_right.z;
        float dot_up = to_led.x * plane_up.x + to_led.y * plane_up.y + to_led.z * plane_up.z;

        // Calculate horizontal angle (left/right)
        // Use fabsf(dot_normal) to treat behind-screen LEDs symmetrically
        float horizontal_angle = atan2f(fabsf(dot_right), fabsf(dot_normal) + 0.001f);
        horizontal_angle = horizontal_angle * 180.0f / 3.14159265359f; // Convert to degrees

        // Calculate vertical angle (up/down)
        float vertical_angle = atan2f(fabsf(dot_up), fabsf(dot_normal) + 0.001f);
        vertical_angle = vertical_angle * 180.0f / 3.14159265359f; // Convert to degrees

        // Compute falloff based on how much LED exceeds wrap angles
        float h_falloff = 1.0f;
        if (horizontal_angle > horizontal_wrap_angle)
        {
            float overshoot = (horizontal_angle - horizontal_wrap_angle) / fmaxf(1.0f, horizontal_wrap_angle);
            h_falloff = expf(-overshoot * wrap_strength * 2.0f);
        }

        float v_falloff = 1.0f;
        if (vertical_angle > vertical_wrap_angle)
        {
            float overshoot = (vertical_angle - vertical_wrap_angle) / fmaxf(1.0f, vertical_wrap_angle);
            v_falloff = expf(-overshoot * wrap_strength * 2.0f);
        }

        // Combine both falloffs (multiplicative)
        return h_falloff * v_falloff;
    }

    /**
     * @brief Spatial mapping for perceptually correct 3D ambilight
     *
     * Maps LED position to screen UV based on spatial relationship.
     * This creates a "fake but perceptually correct" ambilight effect:
     * - LEDs below screen sample bottom edge (spread by X position)
     * - LEDs to the left sample left edge (spread by Z position)
     * - LEDs to the right sample right edge (spread by Z position)
     * - LEDs above screen sample top edge (spread by X position)
     * - LEDs behind screen sample based on X/Z offset from center
     *
     * @param led_position World position of the LED (in mm)
     * @param plane The display plane to map to
     * @param edge_zone_depth Depth of edge zones (0.1 = 10% of screen)
     * @param user_position Optional user/viewer position for distance falloff (if null, uses screen center)
     * @return PlaneProjection with UV coordinates and distance
     */
    inline PlaneProjection SpatialMapToScreen(const Vector3D& led_position, const DisplayPlane3D& plane, float edge_zone_depth = 0.15f, const Vector3D* user_position = nullptr, float grid_scale_mm = 10.0f)
    {
        PlaneProjection result;
        result.is_valid = false;
        result.u = 0.5f;
        result.v = 0.5f;
        result.distance = 0.0f;
        result.is_in_front = false;

        const Transform3D& transform = plane.GetTransform();

        // ===== DISTANCE CALCULATION (for falloff) =====
        if (user_position)
        {
            // Distance from user to LED (in grid units)
            Vector3D user_to_led;
            user_to_led.x = led_position.x - user_position->x;
            user_to_led.y = led_position.y - user_position->y;
            user_to_led.z = led_position.z - user_position->z;
            result.distance = sqrtf(user_to_led.x * user_to_led.x + user_to_led.y * user_to_led.y + user_to_led.z * user_to_led.z);
        }
        else
        {
            // Distance from screen center to LED (in grid units)
            Vector3D screen_to_led;
            screen_to_led.x = led_position.x - transform.position.x;
            screen_to_led.y = led_position.y - transform.position.y;
            screen_to_led.z = led_position.z - transform.position.z;
            result.distance = sqrtf(screen_to_led.x * screen_to_led.x + screen_to_led.y * screen_to_led.y + screen_to_led.z * screen_to_led.z);
        }

        // Convert distance from grid units to millimeters for falloff calculation
        result.distance *= grid_scale_mm;

        // ===== ROTATION-AWARE UV MAPPING =====
        // Transform LED from world space to screen's local coordinate system
        // This handles screens at ANY orientation (tilted, rotated, angled, etc.)

        // Calculate LED's offset from screen center in world space (grid units)
        // World space uses Y-up: X=width, Y=height(vertical), Z=depth
        Vector3D world_offset;
        world_offset.x = led_position.x - transform.position.x;
        world_offset.y = led_position.y - transform.position.y;
        world_offset.z = led_position.z - transform.position.z;

        // Calculate rotation matrix for the screen
        float rotation_matrix[9];
        ComputeRotationMatrix(transform.rotation, rotation_matrix);

        // Transform world offset to screen's local coordinate system
        // Use INVERSE rotation (transpose of rotation matrix for orthonormal matrix)
        // NOTE: Screen local space is Z-up, so we need to swap Y/Z from world Y-up to local Z-up
        Vector3D local_offset;
        local_offset.x = rotation_matrix[0] * world_offset.x + rotation_matrix[3] * world_offset.z + rotation_matrix[6] * world_offset.y;
        local_offset.y = rotation_matrix[1] * world_offset.x + rotation_matrix[4] * world_offset.z + rotation_matrix[7] * world_offset.y;
        local_offset.z = rotation_matrix[2] * world_offset.x + rotation_matrix[5] * world_offset.z + rotation_matrix[8] * world_offset.y;

        // In screen's local space:
        // local X = left(-) to right(+) across screen surface
        // local Y = behind(-) to front(+) perpendicular to screen
        // local Z = bottom(-) to top(+) on screen surface
        result.is_in_front = (local_offset.y < 0.0f);

        // Convert screen dimensions from millimeters to grid units
        float screen_width_units = plane.GetWidthMM() / grid_scale_mm;
        float screen_height_units = plane.GetHeightMM() / grid_scale_mm;

        // Map local offset to UV coordinates [0, 1]
        // LED at screen center (local 0,0,0) → UV (0.5, 0.5)
        result.u = (local_offset.x + screen_width_units * 0.5f) / screen_width_units;
        result.v = (local_offset.z + screen_height_units * 0.5f) / screen_height_units;

        // Clamp UV to valid range [0, 1]
        if (result.u < 0.0f) result.u = 0.0f;
        if (result.u > 1.0f) result.u = 1.0f;
        if (result.v < 0.0f) result.v = 0.0f;
        if (result.v > 1.0f) result.v = 1.0f;

        // Apply edge sampling inset if requested (pulls sampling in from absolute edges)
        float inset = edge_zone_depth;
        if(inset > 0.0f)
        {
            inset = std::clamp(inset, 0.0f, 0.49f);
            float min_uv = inset;
            float max_uv = 1.0f - inset;
            float span = std::max(0.0f, max_uv - min_uv);
            if(span > 0.0f)
            {
                result.u = min_uv + span * result.u;
                result.v = min_uv + span * result.v;
            }
            else
            {
                result.u = result.v = 0.5f;
            }
        }

        // NOTE: V coordinate is NOT flipped here because screen capture is already
        // in the correct orientation (verified with 3D viewport texture display)

        result.is_valid = true;
        return result;
    }

    /**
     * @brief Sample a color from a frame buffer using UV coordinates
     *
     * @param frame_data RGBA pixel data (row-major, top-left origin)
     * @param frame_width Width of frame buffer
     * @param frame_height Height of frame buffer
     * @param u Horizontal coordinate [0,1]
     * @param v Vertical coordinate [0,1]
     * @param use_bilinear Use bilinear filtering (true) or nearest neighbor (false)
     * @return Sampled RGBA color (or black if out of bounds)
     */
    inline RGBColor SampleFrame(const uint8_t* frame_data, int frame_width, int frame_height,
                               float u, float v, bool use_bilinear = true)
    {
        // Clamp UV to valid range for ambilight edge extension
        // LEDs outside screen bounds sample the nearest edge pixel
        if (u < 0.0f) u = 0.0f;
        if (u > 1.0f) u = 1.0f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;

        float x = u * (frame_width - 1);
        float y = v * (frame_height - 1);

        if (!use_bilinear)
        {
            // Nearest neighbor
            int ix = (int)(x + 0.5f);
            int iy = (int)(y + 0.5f);
            int index = (iy * frame_width + ix) * 4;

            uint8_t r = frame_data[index + 0];
            uint8_t g = frame_data[index + 1];
            uint8_t b = frame_data[index + 2];
            return ToRGBColor(r, g, b);
        }
        else
        {
            // Bilinear filtering
            int x0 = (int)x;
            int y0 = (int)y;
            int x1 = (x0 + 1 < frame_width) ? x0 + 1 : x0;
            int y1 = (y0 + 1 < frame_height) ? y0 + 1 : y0;

            float fx = x - x0;
            float fy = y - y0;

            int idx00 = (y0 * frame_width + x0) * 4;
            int idx10 = (y0 * frame_width + x1) * 4;
            int idx01 = (y1 * frame_width + x0) * 4;
            int idx11 = (y1 * frame_width + x1) * 4;

            float r00 = frame_data[idx00 + 0];
            float g00 = frame_data[idx00 + 1];
            float b00 = frame_data[idx00 + 2];

            float r10 = frame_data[idx10 + 0];
            float g10 = frame_data[idx10 + 1];
            float b10 = frame_data[idx10 + 2];

            float r01 = frame_data[idx01 + 0];
            float g01 = frame_data[idx01 + 1];
            float b01 = frame_data[idx01 + 2];

            float r11 = frame_data[idx11 + 0];
            float g11 = frame_data[idx11 + 1];
            float b11 = frame_data[idx11 + 2];

            float r = r00 * (1 - fx) * (1 - fy) + r10 * fx * (1 - fy) + r01 * (1 - fx) * fy + r11 * fx * fy;
            float g = g00 * (1 - fx) * (1 - fy) + g10 * fx * (1 - fy) + g01 * (1 - fx) * fy + g11 * fx * fy;
            float b = b00 * (1 - fx) * (1 - fy) + b10 * fx * (1 - fy) + b01 * (1 - fx) * fy + b11 * fx * fy;

            return ToRGBColor((uint8_t)(r + 0.5f), (uint8_t)(g + 0.5f), (uint8_t)(b + 0.5f));
        }
    }

    /**
     * @brief Extract edge band average color from a frame
     *
     * @param frame_data RGBA pixel data
     * @param frame_width Frame width
     * @param frame_height Frame height
     * @param edge Edge to sample (0=top, 1=right, 2=bottom, 3=left)
     * @param band_thickness Thickness of band as fraction of dimension [0,1]
     * @return Average color of the edge band
     */
    inline RGBColor ExtractEdgeBandColor(const uint8_t* frame_data, int frame_width, int frame_height,
                                        int edge, float band_thickness = 0.1f)
    {
        if (!frame_data || frame_width <= 0 || frame_height <= 0)
        {
            return ToRGBColor(0, 0, 0);
        }

        int sum_r = 0, sum_g = 0, sum_b = 0;
        int count = 0;

        switch (edge)
        {
        case 0: // Top
        {
            int band_height = (int)(frame_height * band_thickness);
            if (band_height < 1) band_height = 1;

            for (int y = 0; y < band_height && y < frame_height; y++)
            {
                for (int x = 0; x < frame_width; x++)
                {
                    int idx = (y * frame_width + x) * 4;
                    sum_r += frame_data[idx + 0];
                    sum_g += frame_data[idx + 1];
                    sum_b += frame_data[idx + 2];
                    count++;
                }
            }
            break;
        }
        case 1: // Right
        {
            int band_width = (int)(frame_width * band_thickness);
            if (band_width < 1) band_width = 1;

            for (int y = 0; y < frame_height; y++)
            {
                for (int x = frame_width - band_width; x < frame_width; x++)
                {
                    int idx = (y * frame_width + x) * 4;
                    sum_r += frame_data[idx + 0];
                    sum_g += frame_data[idx + 1];
                    sum_b += frame_data[idx + 2];
                    count++;
                }
            }
            break;
        }
        case 2: // Bottom
        {
            int band_height = (int)(frame_height * band_thickness);
            if (band_height < 1) band_height = 1;

            for (int y = frame_height - band_height; y < frame_height; y++)
            {
                for (int x = 0; x < frame_width; x++)
                {
                    int idx = (y * frame_width + x) * 4;
                    sum_r += frame_data[idx + 0];
                    sum_g += frame_data[idx + 1];
                    sum_b += frame_data[idx + 2];
                    count++;
                }
            }
            break;
        }
        case 3: // Left
        {
            int band_width = (int)(frame_width * band_thickness);
            if (band_width < 1) band_width = 1;

            for (int y = 0; y < frame_height; y++)
            {
                for (int x = 0; x < band_width; x++)
                {
                    int idx = (y * frame_width + x) * 4;
                    sum_r += frame_data[idx + 0];
                    sum_g += frame_data[idx + 1];
                    sum_b += frame_data[idx + 2];
                    count++;
                }
            }
            break;
        }
        }

        if (count == 0)
        {
            return ToRGBColor(0, 0, 0);
        }

        uint8_t r = sum_r / count;
        uint8_t g = sum_g / count;
        uint8_t b = sum_b / count;
        return ToRGBColor(r, g, b);
    }
}

#endif // GEOMETRY3DUTILS_H
