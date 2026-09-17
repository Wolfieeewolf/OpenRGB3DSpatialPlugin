// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPack.h"

namespace EffectPack
{
Pack MakeExampleRainbowWash()
{
    Pack pack;
    pack.id = "rainbow_wash";
    pack.name = "Rainbow wash";
    pack.duration_ms = 60000;
    pack.loop = LoopMode::Forever;
    pack.priority = 10;

    Track track;
    track.name = "All LEDs";
    track.target.kind = TargetKind::All;

    const RGBColor stops[] = {
        ToRGBColor(255, 0, 0),
        ToRGBColor(255, 128, 0),
        ToRGBColor(255, 255, 0),
        ToRGBColor(0, 255, 0),
        ToRGBColor(0, 128, 255),
        ToRGBColor(128, 0, 255),
        ToRGBColor(255, 0, 0),
    };
    const int segments = 6;
    const int seg_ms = pack.duration_ms / segments;
    for(int i = 0; i < segments; ++i)
    {
        Block block;
        block.type = BlockType::Fade;
        block.start_ms = i * seg_ms;
        block.end_ms = (i == segments - 1) ? pack.duration_ms : (i + 1) * seg_ms;
        block.color_from = stops[i];
        block.color_to = stops[i + 1];
        block.intensity = 1.0f;
        EnsureBlockGradient(&block);
        track.blocks.push_back(block);
    }
    pack.tracks.push_back(std::move(track));
    return pack;
}

Pack MakeExampleDeskRipple()
{
    Pack pack;
    pack.id = "desk_ripple";
    pack.name = "Desk ripple";
    pack.duration_ms = 4000;
    pack.loop = LoopMode::Once;
    pack.priority = 20;

    Track track;
    track.name = "All LEDs";
    track.target.kind = TargetKind::All;

    Block ripple;
    ripple.type = BlockType::Ripple;
    ripple.start_ms = 0;
    ripple.end_ms = 3500;
    ripple.axis_space = AxisSpace::Room;
    ripple.pulse_length = 0.18f;
    ripple.intensity = 1.0f;
    ApplyGradientPresetId(&ripple, "ice");
    track.blocks.push_back(ripple);

    Block burst;
    burst.type = BlockType::Burst;
    burst.start_ms = 800;
    burst.end_ms = 2800;
    burst.axis_space = AxisSpace::Room;
    burst.pulse_length = 0.22f;
    burst.intensity = 0.85f;
    ApplyGradientPresetId(&burst, "cyber");
    track.blocks.push_back(burst);

    pack.tracks.push_back(std::move(track));
    return pack;
}

Pack MakeExampleSequenceWipe()
{
    Pack pack;
    pack.id = "sequence_wipe";
    pack.name = "Sequence wipe";
    pack.duration_ms = 3000;
    pack.loop = LoopMode::Once;
    pack.priority = 15;

    Track track;
    track.name = "All LEDs";
    track.target.kind = TargetKind::All;

    Block wipe;
    wipe.type = BlockType::Wipe;
    wipe.start_ms = 0;
    wipe.end_ms = 2200;
    wipe.axis_space = AxisSpace::Sequence;
    wipe.direction = Direction::Right;
    wipe.intensity = 1.0f;
    ApplyGradientPresetId(&wipe, "sunset");
    ApplyBuiltinIntensityCurve(&wipe, "ease_out");
    track.blocks.push_back(wipe);

    Block scanner;
    scanner.type = BlockType::Scanner;
    scanner.start_ms = 400;
    scanner.end_ms = 3000;
    scanner.axis_space = AxisSpace::Sequence;
    scanner.direction = Direction::Right;
    scanner.pulse_length = 0.12f;
    scanner.intensity = 1.0f;
    ApplyGradientPresetId(&scanner, "fire");
    track.blocks.push_back(scanner);

    pack.tracks.push_back(std::move(track));
    return pack;
}

} // namespace EffectPack
