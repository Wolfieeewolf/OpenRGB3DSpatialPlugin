// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Action.h"

#include <cstddef>
#include <deque>
#include <string>

/* Undo / redo for the Action Editor. Each step is a whole-pack snapshot: packs are small, and
   a snapshot is always consistent, whatever mix of timeline drags and form edits made it.
   Edits are grouped by the editor (a short pause or the end of a drag closes a step). */
class ActionEditorHistory
{
public:
    struct Snapshot
    {
        Action::Pack pack;
        int          selected_track = -1;
        int          selected_clip  = -1;
        std::string  key; // serialized pack, for cheap "did anything change" checks
    };

    static Snapshot Make(const Action::Pack& pack, int selected_track, int selected_clip)
    {
        Snapshot s;
        s.pack           = pack;
        s.selected_track = selected_track;
        s.selected_clip  = selected_clip;
        s.key            = Action::ToJson(pack).dump();
        return s;
    }

    /* Starts a fresh history at this state (new or loaded pack); it also counts as saved. */
    void Reset(Snapshot initial)
    {
        undo_.clear();
        redo_.clear();
        current_   = std::move(initial);
        saved_key_ = current_.key;
        has_current_ = true;
    }

    /* Records a new state. Returns false when nothing changed since the last step. */
    bool Commit(Snapshot next)
    {
        if(!has_current_)
        {
            Reset(std::move(next));
            return true;
        }
        if(next.key == current_.key)
        {
            /* Same pack: keep the newer selection so undo lands where the user last was. */
            current_.selected_track = next.selected_track;
            current_.selected_clip  = next.selected_clip;
            return false;
        }
        undo_.push_back(std::move(current_));
        if(undo_.size() > kLimit)
        {
            undo_.pop_front();
        }
        current_ = std::move(next);
        redo_.clear();
        return true;
    }

    bool HasState() const { return has_current_; }
    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }

    const Snapshot& Undo()
    {
        redo_.push_back(std::move(current_));
        current_ = std::move(undo_.back());
        undo_.pop_back();
        return current_;
    }

    const Snapshot& Redo()
    {
        undo_.push_back(std::move(current_));
        current_ = std::move(redo_.back());
        redo_.pop_back();
        return current_;
    }

    void MarkSaved(const std::string& key) { saved_key_ = key; }
    bool IsSaved(const std::string& key) const { return key == saved_key_; }

private:
    static constexpr std::size_t kLimit = 200;

    std::deque<Snapshot> undo_;
    std::deque<Snapshot> redo_;
    Snapshot             current_;
    std::string          saved_key_;
    bool                 has_current_ = false;
};
