# Name
name: Ocean horizon
description: Lower room rolls like water; upper room softens into sky
section: volume
alias: floating_ocean

# Look
swatch: 40 140 200
icon: brush dim
icon: round 0.08 0.55 0.84 0.37 0.08
icon: brush hot
icon: round 0.08 0.08 0.84 0.42 0.08

# Playback
space: world
knobs: speed pulse

# Effect
t = time_ms * 0.001 * max(0.15, speed)
wave = 0.5 + 0.5 * sin(nx * 7.5 + t * 1.15 + nz * 4.2)
wave2 = 0.5 + 0.5 * sin(nx * 3.2 - t * 0.65 + nz * 2.4)
swell = 0.55 * wave + 0.45 * wave2
ripple = 0.5 + 0.5 * sin((nx + nz) * 10.0 - t * 1.8)
water = clamp((0.52 - ny) / 0.14, 0, 1)
sky = 1 - water
ocean_t = clamp(0.02 + swell * 0.32 + ny * 0.2 + ripple * 0.06, 0, 0.48)
sky_t = clamp(0.55 + (ny - 0.48) * 0.9 + wave * 0.04, 0.52, 1)
paint_mix(ocean_t, sky_t, sky)
ocean_i = 0.38 + 0.45 * swell + 0.17 * ripple
sky_i = 0.32 + 0.22 * wave + 0.28 * ny
intensity = intensity * (water * ocean_i + sky * sky_i)
