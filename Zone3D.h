// SPDX-License-Identifier: GPL-2.0-only

#ifndef ZONE3D_H
#define ZONE3D_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class Zone3D
{
public:
    Zone3D(const std::string& name);
    ~Zone3D();

    void SetName(const std::string& name);
    void AddController(int controller_idx);
    void RemoveController(int controller_idx);
    void ClearControllers();
    bool ContainsController(int controller_idx) const;

    std::string GetName() const { return zone_name; }
    const std::vector<int>& GetControllers() const { return controller_indices; }
    int GetControllerCount() const { return (int)controller_indices.size(); }

    nlohmann::json ToJSON() const;
    static Zone3D* FromJSON(const nlohmann::json& json);

private:
    std::string zone_name;
    std::vector<int> controller_indices;
};

#endif
