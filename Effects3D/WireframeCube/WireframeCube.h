// SPDX-License-Identifier: GPL-2.0-only

#ifndef WIREFRAMECUBE_H
#define WIREFRAMECUBE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class WireframeCube : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit WireframeCube(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("WireframeCube", "Wireframe Cube", "Spatial", [](){ return new WireframeCube; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    static float PointToSegmentDistance(float px, float py, float pz,
                                        float ax, float ay, float az,
                                        float bx, float by, float bz,
                                        float* out_t01 = nullptr);

    void RebuildRoomWireframeCache(const GridContext3D& grid);
    void ClosestOnRoomWireframe(float x, float y, float z,
                                float& out_dist, float& out_path01) const;

    float thickness = 0.08f;
    float line_brightness = 1.0f;

    uint64_t room_wf_cache_seq = 0;
    float room_wf_min_x = 0.0f;
    float room_wf_max_x = 0.0f;
    float room_wf_min_y = 0.0f;
    float room_wf_max_y = 0.0f;
    float room_wf_min_z = 0.0f;
    float room_wf_max_z = 0.0f;
    float room_wf_ax[12]{};
    float room_wf_ay[12]{};
    float room_wf_az[12]{};
    float room_wf_bx[12]{};
    float room_wf_by[12]{};
    float room_wf_bz[12]{};
    float room_wf_edge_len[12]{};
    float room_wf_prefix[13]{};
    float room_wf_total_len = 1.0f;
};

#endif
