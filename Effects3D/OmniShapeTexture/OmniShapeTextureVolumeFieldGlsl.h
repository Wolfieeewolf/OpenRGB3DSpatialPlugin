// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Omni shape texture: media on a crisp 3D shape envelope (not a spherical blob).
 *  Shapes: 0 sphere, 1 cube, 2 octahedron, 3 cylinder, 4 hex prism, 5 triangle prism.
 *  Morph blends spatial metric + UV toward the next shape.
 *  u_params: [0]=shape [1]=morph [2]=tile [3]=yaw_rate [4]=pitch_rate
 *            [5]=phase_mul [6]=amp [7]=detail
 *            [8]=ox [9]=oy [10]=oz [11]=fd [12]=curve [13]=edge
 *            [14]=R_local [15]=wrap*2+prop (wrap 0|1, prop 0..1)
 */
inline const char* OmniShapeTextureVolumeFieldGlsl()
{
    return R"(
float smstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(e1 - e0, 1e-5), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
float frac01(float x)
{
    return x - floor(x);
}
float hexMetric(vec2 p)
{
    vec2 a = abs(p);
    return max(dot(a, vec2(0.5, 0.8660254)), a.x) / 0.8660254;
}
float polyRadial(vec2 p, float n)
{
    float an = 6.2831853 / max(n, 3.0);
    float a = atan(p.y, p.x);
    float r = length(p);
    return cos(floor(0.5 + a / an) * an - a) * r / cos(3.14159265 / n);
}
float shapeMetric(vec3 l, int shape)
{
    if(shape == 1)
        return max(max(abs(l.x), abs(l.y)), abs(l.z));
    if(shape == 2)
        return (abs(l.x) + abs(l.y) + abs(l.z)) * 0.57735027;
    if(shape == 3)
        return max(length(l.xz), abs(l.y));
    if(shape == 4)
        return max(hexMetric(l.xz), abs(l.y));
    if(shape == 5)
        return max(polyRadial(l.xz, 3.0), abs(l.y));
    return length(l);
}
void rotateDir(inout vec3 d, float yaw, float pitch)
{
    float cy = cos(yaw);
    float sy = sin(yaw);
    float x1 = cy * d.x + sy * d.z;
    float z1 = -sy * d.x + cy * d.z;
    float y1 = d.y;
    float cx = cos(pitch);
    float sx = sin(pitch);
    d = vec3(x1, cx * y1 - sx * z1, sx * y1 + cx * z1);
}
void dirToSphereUV(vec3 d, out float u, out float v)
{
    float len = length(d);
    if(len < 1e-6) { u = 0.5; v = 0.5; return; }
    d /= len;
    u = atan(d.z, d.x) / 6.2831853 + 0.5;
    v = asin(clamp(d.y, -1.0, 1.0)) / 3.14159265 + 0.5;
}
void dirToCubeUV(vec3 d, out float u, out float v)
{
    vec3 a = abs(d);
    float m = max(a.x, max(a.y, a.z));
    if(m < 1e-8) { u = 0.5; v = 0.5; return; }
    vec3 p = d / m;
    if(a.x >= a.y && a.x >= a.z)
    {
        if(d.x > 0.0) { u = (-p.z + 1.0) * 0.5; v = (p.y + 1.0) * 0.5; }
        else { u = (p.z + 1.0) * 0.5; v = (p.y + 1.0) * 0.5; }
    }
    else if(a.y >= a.z)
    {
        if(d.y > 0.0) { u = (p.x + 1.0) * 0.5; v = (-p.z + 1.0) * 0.5; }
        else { u = (p.x + 1.0) * 0.5; v = (p.z + 1.0) * 0.5; }
    }
    else
    {
        if(d.z > 0.0) { u = (p.x + 1.0) * 0.5; v = (p.y + 1.0) * 0.5; }
        else { u = (-p.x + 1.0) * 0.5; v = (p.y + 1.0) * 0.5; }
    }
}
void dirToOctaUV(vec3 d, out float u, out float v)
{
    float l = abs(d.x) + abs(d.y) + abs(d.z);
    if(l < 1e-8) { u = 0.5; v = 0.5; return; }
    float nx = d.x / l;
    float ny = d.y / l;
    float nz = d.z / l;
    if(nz < 0.0)
    {
        float wx = (1.0 - abs(ny)) * (nx >= 0.0 ? 1.0 : -1.0);
        float wy = (1.0 - abs(nx)) * (ny >= 0.0 ? 1.0 : -1.0);
        nx = wx;
        ny = wy;
    }
    u = nx * 0.5 + 0.5;
    v = ny * 0.5 + 0.5;
}
void dirToCylUV(vec3 d, out float u, out float v)
{
    u = atan(d.z, d.x) / 6.2831853 + 0.5;
    v = clamp(d.y * 0.5 + 0.5, 0.0, 1.0);
}
void dirToPrismUV(vec3 d, float n, out float u, out float v)
{
    u = atan(d.z, d.x) / 6.2831853 + 0.5;
    v = clamp(d.y * 0.5 + 0.5, 0.0, 1.0);
    float an = 1.0 / max(n, 3.0);
    u = floor(u / an) * an + an * 0.5;
}
void shapeToUV(int shape, vec3 d, out float u, out float v)
{
    if(shape == 1) dirToCubeUV(d, u, v);
    else if(shape == 2) dirToOctaUV(d, u, v);
    else if(shape == 3) dirToCylUV(d, u, v);
    else if(shape == 4) dirToPrismUV(d, 6.0, u, v);
    else if(shape == 5) dirToPrismUV(d, 3.0, u, v);
    else dirToSphereUV(d, u, v);
}
float ambienceGain(float dist_n, float d_face, float fd, float c, float es)
{
    float g = 1.0;
    float amount = clamp(fd * 0.95 + c * 0.55, 0.0, 1.0);
    if(amount > 1e-4)
    {
        float t = clamp(dist_n, 0.0, 1.0);
        float linear = 1.0 - t * (0.12 + 0.88 * amount);
        float exponent = 0.70 + 3.8 * c;
        g *= pow(clamp(linear, 0.0, 1.0), exponent);
    }
    if(es > 1e-4)
    {
        float feather = 0.035 + 0.55 * es;
        g *= smstep(0.0, feather, d_face);
    }
    return clamp(g, 0.0, 1.0);
}
void volumeMain(out vec4 out_color, in vec3 p01)
{
    int shape_a = int(clamp(u_params[0], 0.0, 5.0) + 0.5);
    float morph = clamp(u_params[1], 0.0, 1.0);
    float tile = max(u_params[2], 0.12);
    float yaw_rate = u_params[3];
    float pitch_rate = u_params[4];
    float phase_mul = clamp(u_params[5], 0.0, 2.0);
    float amp = u_params[6];
    float detail = max(u_params[7], 0.05);
    float fd = clamp(u_params[8], 0.0, 1.0);
    float curve = clamp(u_params[9], 0.0, 1.0);
    float edge = clamp(u_params[10], 0.0, 1.0);
    float R = max(u_params[11], 0.05);
    float packed = u_params[12];
    float wrap = step(1.5, packed);
    float prop = (wrap > 0.5) ? (packed - 2.0) : packed;

    vec3 l = (p01 - vec3(0.5)) * 2.0;
    float dist_n = length(l) * 0.5 * 1.7320508;

    float yaw = u_time * yaw_rate;
    float pitch = u_time * pitch_rate;
    /* Propagation: spin/warp lag with distance. */
    float t_lag = prop * dist_n * 2.8;
    yaw -= t_lag * yaw_rate;
    pitch -= t_lag * pitch_rate;

    vec3 d = l;
    float llen = length(d);
    if(llen < 1e-5)
    {
        out_color = vec4(0.0);
        return;
    }
    d /= llen;
    rotateDir(d, yaw, pitch);
    rotateDir(l, yaw, pitch);

    int shape_b = int(mod(float(shape_a + 1), 6.0));
    float ma = shapeMetric(l, shape_a);
    float mb = shapeMetric(l, shape_b);
    float metric = mix(ma, mb, morph);

    /* Crisp shell mask — expands with Size as a hard shape, not a sphere blob. */
    float band = 0.018 + 0.008 * (1.0 - clamp(detail * 0.08, 0.0, 1.0));
    float mask = 1.0 - smstep(R - band * 0.1, R + band * 0.7, metric);
    if(metric > R + band)
    {
        out_color = vec4(0.0);
        return;
    }
    /* Prefer a thin bright shell surface for definition. */
    float surface = 1.0 - smstep(0.0, band * 0.75, abs(metric - R));
    float fill = step(metric, R) * 0.42;
    float shape_w = clamp(max(fill, surface * 1.25), 0.0, 1.0) * mask;

    float ua, va, ub, vb;
    shapeToUV(shape_a, d, ua, va);
    shapeToUV(shape_b, d, ub, vb);

    ua = (ua - 0.5) * tile + 0.5;
    va = (va - 0.5) * tile + 0.5;
    ub = (ub - 0.5) * tile + 0.5;
    vb = (vb - 0.5) * tile + 0.5;

    float t_eff = u_time - t_lag;
    float scroll_u = t_eff * (0.15 + 0.55 * phase_mul);
    float scroll_v = t_eff * (0.08 + 0.42 * phase_mul);
    ua += scroll_u; va += scroll_v;
    ub += scroll_u; vb += scroll_v;

    float warp_ph = t_eff * (0.6 + 4.0 * phase_mul);
    ua += sin(warp_ph + ua * 9.0 * detail * 0.1 + va * 6.0 * detail * 0.08) * amp;
    va += cos(warp_ph * 0.91 + ua * 7.0 * detail * 0.08 - va * 8.0 * detail * 0.1) * amp;
    ub += sin(warp_ph * 1.05 + ub * 9.0 * detail * 0.1 + vb * 6.0 * detail * 0.08) * amp;
    vb += cos(warp_ph * 0.94 + ub * 7.0 * detail * 0.08 - vb * 8.0 * detail * 0.1) * amp;

    float do_wrap = wrap;
    if(abs(phase_mul) > 0.02) do_wrap = 1.0;
    if(amp > 1e-4) do_wrap = 1.0;
    if(do_wrap > 0.5)
    {
        ua = frac01(ua); va = frac01(va);
        ub = frac01(ub); vb = frac01(vb);
    }
    else
    {
        ua = clamp(ua, 0.0, 1.0); va = clamp(va, 0.0, 1.0);
        ub = clamp(ub, 0.0, 1.0); vb = clamp(vb, 0.0, 1.0);
    }

    vec3 ca = texture2D(u_media, vec2(ua, 1.0 - va)).rgb;
    vec3 cb = texture2D(u_media, vec2(ub, 1.0 - vb)).rgb;
    vec3 rgb = mix(ca, cb, morph);

    float d_face = min(min(min(p01.x, 1.0 - p01.x), min(p01.y, 1.0 - p01.y)), min(p01.z, 1.0 - p01.z));
    float ag = ambienceGain(clamp(dist_n, 0.0, 1.0), d_face, fd, curve, edge);
    /* Edge ambience also sharpens the silhouette */
    float edge_boost = mix(1.0, 0.35 + 0.65 * surface, clamp(edge * 1.2, 0.0, 1.0));
    out_color = vec4(rgb * clamp(shape_w * ag * edge_boost, 0.0, 1.0), 1.0);
}
)";
}
