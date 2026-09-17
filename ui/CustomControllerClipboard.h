// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERCLIPBOARD_H
#define CUSTOMCONTROLLERCLIPBOARD_H

#include "CustomControllerTypes.h"

#include <utility>
#include <vector>

struct CustomControllerClipboardRegion
{
    bool valid = false;
    int  min_col = 0;
    int  min_row = 0;
    int  max_col = 0;
    int  max_row = 0;
    int  source_layer = 0;
    std::vector<GridLEDMapping> mappings;
    /** Blocker offsets relative to min_col/min_row on source_layer. */
    std::vector<std::pair<int, int>> blocker_offsets;
};

#endif
