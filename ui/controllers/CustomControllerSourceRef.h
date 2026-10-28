// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERSOURCEREF_H
#define CUSTOMCONTROLLERSOURCEREF_H

struct CustomControllerSourceRef
{
    int controller_index = -1;
    int granularity        = -1;
    int item_idx           = -1;

    bool isValid() const
    {
        return controller_index >= 0 && granularity >= 0 && item_idx >= 0;
    }

    bool operator==(const CustomControllerSourceRef& other) const
    {
        return controller_index == other.controller_index && granularity == other.granularity
               && item_idx == other.item_idx;
    }

    bool operator!=(const CustomControllerSourceRef& other) const
    {
        return !(*this == other);
    }
};

#endif
