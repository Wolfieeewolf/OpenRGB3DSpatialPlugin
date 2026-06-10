// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMCAMPFIREPARAMS_H
#define ROOMCAMPFIREPARAMS_H

struct RoomCampfireParams
{
    float flame_rise = 75.0f;
    /** Wobble and tongue motion in the flame column (0–100). */
    float flame_turbulence = 62.0f;
    float spark_amount = 45.0f;
    /** Optional faint ember halo (0–8). Does not use room-fill wash. */
    float spill_fill = 0.0f;
};

#endif
