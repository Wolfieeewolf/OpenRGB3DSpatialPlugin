// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERHISTORY_H
#define CUSTOMCONTROLLERHISTORY_H

#include "CustomControllerTypes.h"

#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

struct CustomControllerHistorySnapshot
{
    std::vector<GridLEDMapping> mappings;
    std::unordered_set<uint64_t> matrix_holes;
    std::unordered_set<uint64_t> light_blockers;
    std::vector<float> column_widths_mm;
    std::vector<float> row_heights_mm;
    std::vector<float> layer_depths_mm;
    std::vector<std::string> layer_names;
    int width = 1;
    int height = 1;
    int depth = 1;
    int leds_per_cluster = 1;
    int current_layer = 0;
    std::string name;
};

class CustomControllerHistory
{
public:
    static constexpr std::size_t kMaxDepth = 64;

    void Clear()
    {
        undo_.clear();
        redo_.clear();
    }

    void PushBaseline(CustomControllerHistorySnapshot snapshot)
    {
        baseline_ = std::move(snapshot);
    }

    const CustomControllerHistorySnapshot& Baseline() const
    {
        return baseline_;
    }

    void PushUndoFromBaseline()
    {
        undo_.push_back(baseline_);
        if(undo_.size() > kMaxDepth)
        {
            undo_.erase(undo_.begin());
        }
        redo_.clear();
    }

    void AbandonLastUndoPush()
    {
        if(!undo_.empty())
        {
            undo_.pop_back();
        }
    }

    void CommitBaseline(CustomControllerHistorySnapshot snapshot)
    {
        baseline_ = std::move(snapshot);
    }

    bool CanUndo() const
    {
        return !undo_.empty();
    }

    bool CanRedo() const
    {
        return !redo_.empty();
    }

    bool Undo(CustomControllerHistorySnapshot current, CustomControllerHistorySnapshot* out_restore)
    {
        if(!out_restore || undo_.empty())
        {
            return false;
        }
        redo_.push_back(std::move(current));
        *out_restore = std::move(undo_.back());
        undo_.pop_back();
        baseline_ = *out_restore;
        return true;
    }

    bool Redo(CustomControllerHistorySnapshot current, CustomControllerHistorySnapshot* out_restore)
    {
        if(!out_restore || redo_.empty())
        {
            return false;
        }
        undo_.push_back(std::move(current));
        *out_restore = std::move(redo_.back());
        redo_.pop_back();
        baseline_ = *out_restore;
        return true;
    }

private:
    CustomControllerHistorySnapshot baseline_;
    std::vector<CustomControllerHistorySnapshot> undo_;
    std::vector<CustomControllerHistorySnapshot> redo_;
};

#endif
