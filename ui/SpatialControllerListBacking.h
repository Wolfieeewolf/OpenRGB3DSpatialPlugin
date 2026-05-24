// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALCONTROLLERLISTBACKING_H
#define SPATIALCONTROLLERLISTBACKING_H

#include "SpatialControllerEntryKey.h"

#include <QString>
#include <vector>

struct SpatialControllerListRow
{
    QString                 text;
    bool                    has_user_role = false;
    SpatialControllerEntryKey user_role;
};

class SpatialSceneControllerBacking
{
public:
    int count() const { return static_cast<int>(entries_.size()); }

    int currentRow() const { return current_row_; }

    void setCurrentRow(int row) { current_row_ = row; }

    void clear()
    {
        entries_.clear();
        current_row_ = -1;
    }

    void clearSelection() { current_row_ = -1; }

    bool blockSignals(bool) { return false; }

    void append(const QString& text) { entries_.push_back({text, false, {}}); }

    void append(const QString& text, const SpatialControllerEntryKey& key)
    {
        entries_.push_back({text, true, key});
    }

    void removeAt(int index)
    {
        if(index < 0 || index >= count())
        {
            return;
        }
        entries_.erase(entries_.begin() + index);
        if(current_row_ == index)
        {
            current_row_ = -1;
        }
        else if(current_row_ > index)
        {
            current_row_--;
        }
    }

    QString textAt(int row) const
    {
        if(row < 0 || row >= count())
        {
            return QString();
        }
        return entries_[static_cast<size_t>(row)].text;
    }

    bool hasUserRole(int row) const
    {
        if(row < 0 || row >= count())
        {
            return false;
        }
        return entries_[static_cast<size_t>(row)].has_user_role;
    }

    SpatialControllerEntryKey userRoleAt(int row) const
    {
        if(row < 0 || row >= count())
        {
            return {};
        }
        return entries_[static_cast<size_t>(row)].user_role;
    }

    void setTextAt(int row, const QString& text)
    {
        if(row >= 0 && row < count())
        {
            entries_[static_cast<size_t>(row)].text = text;
        }
    }

    void setUserRoleAt(int row, const SpatialControllerEntryKey& key)
    {
        if(row >= 0 && row < count())
        {
            entries_[static_cast<size_t>(row)].has_user_role = true;
            entries_[static_cast<size_t>(row)].user_role     = key;
        }
    }

    bool userRoleIsValid(int row) const { return hasUserRole(row); }

private:
    std::vector<SpatialControllerListRow> entries_;
    int                                   current_row_ = -1;
};

class SpatialAvailableControllerBacking
{
public:
    int count() const { return static_cast<int>(entries_.size()); }

    int currentRow() const { return current_row_; }

    void setCurrentRow(int row) { current_row_ = row; }

    void clear()
    {
        entries_.clear();
        current_row_ = -1;
    }

    bool blockSignals(bool) { return false; }

    void append(const QString& text, const SpatialControllerEntryKey& key)
    {
        entries_.push_back({text, true, key});
    }

    QString textAt(int row) const
    {
        if(row < 0 || row >= count())
        {
            return QString();
        }
        return entries_[static_cast<size_t>(row)].text;
    }

    SpatialControllerEntryKey userRoleAt(int row) const
    {
        if(row < 0 || row >= count())
        {
            return {};
        }
        return entries_[static_cast<size_t>(row)].user_role;
    }

    int findByKey(int type_code, int object_index) const
    {
        for(int row = 0; row < count(); row++)
        {
            const SpatialControllerEntryKey key = userRoleAt(row);
            if(key.first == type_code && key.second == object_index)
            {
                return row;
            }
        }
        return -1;
    }

private:
    std::vector<SpatialControllerListRow> entries_;
    int                                   current_row_ = -1;
};

#endif
