// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "filesystem.h"
#include <QString>
#include <QWidget>

class QMenu;
class QToolButton;

/**
 * Slim menu strip for the Action Editor: Effects ▾ (by category), Colours ▾, Gradients ▾ and
 * Curves ▾. Every item can be clicked, dragged straight out of the menu onto the timeline, or
 * picked with the keyboard (Enter); right-click an item for its own tools.
 */
class ActionToolBar : public QWidget
{
    Q_OBJECT

public:
    explicit ActionToolBar(const filesystem::path& user_gradients_path,
                               const filesystem::path& user_colors_path,
                               const filesystem::path& effect_files_dir,
                               const filesystem::path& user_curves_path,
                               QWidget* parent = nullptr);

    void reloadUserGradients();
    void reloadUserCurves();
    void setCurvesEnabled(bool enabled);

signals:
    void effectClicked(const QString& effect_id);
    void colorClicked(unsigned int rgb);
    void gradientPresetClicked(const QString& preset_id);
    void gradientPresetOverwriteRequested(const QString& preset_id);
    void gradientPresetDeleteRequested(const QString& preset_id);
    void gradientPresetResetRequested(const QString& preset_id);
    void curvePresetClicked(const QString& preset_id);

private:
    void buildUi();
    QToolButton* addMenuButton(const QString& text, const QString& tooltip, QMenu* menu);
    void fillEffectsMenu();
    void fillColorsMenu();
    void fillGradientsMenu();
    void fillCurvesMenu();

    filesystem::path user_gradients_path_;
    filesystem::path user_colors_path_;
    filesystem::path effect_files_dir_;
    filesystem::path user_curves_path_;
    QMenu* effects_menu_ = nullptr;
    QMenu* colors_menu_ = nullptr;
    QMenu* gradients_menu_ = nullptr;
    QMenu* curves_menu_ = nullptr;
    QToolButton* curves_button_ = nullptr;
};
