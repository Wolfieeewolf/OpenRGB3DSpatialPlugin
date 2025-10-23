/*---------------------------------------------------------*\
| DiagnosticTest3D.h                                        |
|                                                           |
|   Diagnostic effect to test 3D grid system               |
|                                                           |
|   Date: 2025-10-10                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef DIAGNOSTICTEST3D_H
#define DIAGNOSTICTEST3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include <QComboBox>
#include <QPushButton>

class DiagnosticTest3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    DiagnosticTest3D(QWidget* parent = nullptr);
    ~DiagnosticTest3D();

    /*---------------------------------------------------------*\
    | Auto-registration system                                 |
    \*---------------------------------------------------------*/
    EFFECT_REGISTERER_3D("DiagnosticTest3D", "Diagnostic Test 3D", "Diagnostic", [](){return new DiagnosticTest3D;});

    static std::string const ClassName() { return "DiagnosticTest3D"; }
    static std::string const UIName() { return "Diagnostic Test 3D"; }

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    /*---------------------------------------------------------*\
    | Settings persistence                                     |
    \*---------------------------------------------------------*/
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnTestModeChanged();
    void OnLogDiagnostics();

private:
    QComboBox* test_mode_combo;
    QPushButton* log_button;

    int test_mode;  // 0=X-Axis, 1=Y-Axis, 2=Z-Axis, 3=Radial, 4=Grid Corners, 5=Distance from Origin

    // Diagnostic methods
    void LogGridBounds(float x, float y, float z);
    void LogWorldPosition(float x, float y, float z);
};

#endif
