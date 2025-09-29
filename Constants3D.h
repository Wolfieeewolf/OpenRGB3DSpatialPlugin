/*---------------------------------------------------------*\
| Constants3D.h                                             |
|                                                           |
|   Constants for 3D spatial effects                       |
|                                                           |
|   Date: 2025-09-29                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef CONSTANTS3D_H
#define CONSTANTS3D_H

#include "Colors.h"

/*---------------------------------------------------------*\
| Default Effect Parameters                                 |
\*---------------------------------------------------------*/
#define DEFAULT_EFFECT_SPEED                50
#define DEFAULT_EFFECT_BRIGHTNESS          100
#define DEFAULT_COLOR_START                0xFF0000    // Red
#define DEFAULT_COLOR_END                  0x0000FF    // Blue

/*---------------------------------------------------------*\
| Speed Ranges                                             |
\*---------------------------------------------------------*/
#define MIN_EFFECT_SPEED                    1
#define MAX_EFFECT_SPEED                  100

/*---------------------------------------------------------*\
| Brightness Ranges                                        |
\*---------------------------------------------------------*/
#define MIN_EFFECT_BRIGHTNESS               1
#define MAX_EFFECT_BRIGHTNESS             100

/*---------------------------------------------------------*\
| Wave Effect Constants                                    |
\*---------------------------------------------------------*/
#define DEFAULT_WAVE_FREQUENCY             10
#define MIN_WAVE_FREQUENCY                  1
#define MAX_WAVE_FREQUENCY                 50

/*---------------------------------------------------------*\
| Plasma Effect Constants                                  |
\*---------------------------------------------------------*/
#define DEFAULT_PLASMA_SCALE               1.0f
#define DEFAULT_PLASMA_TURBULENCE          0.5f

/*---------------------------------------------------------*\
| Spiral Effect Constants                                  |
\*---------------------------------------------------------*/
#define DEFAULT_SPIRAL_ARMS                 3
#define MIN_SPIRAL_ARMS                     1
#define MAX_SPIRAL_ARMS                    12

/*---------------------------------------------------------*\
| Common Colors                                            |
\*---------------------------------------------------------*/
#define COLOR_RED                          0xFF0000
#define COLOR_GREEN                        0x00FF00
#define COLOR_BLUE                         0x0000FF
#define COLOR_WHITE                        0xFFFFFF
#define COLOR_BLACK                        0x000000
#define COLOR_YELLOW                       0xFFFF00
#define COLOR_CYAN                         0x00FFFF
#define COLOR_MAGENTA                      0xFF00FF

/*---------------------------------------------------------*\
| UI Constants                                             |
\*---------------------------------------------------------*/
#define DEFAULT_BUTTON_HEIGHT              30
#define DEFAULT_BUTTON_WIDTH               80
#define DEFAULT_SLIDER_WIDTH              200
#define DEFAULT_SPACING                     5

#endif