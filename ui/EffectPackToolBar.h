// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "filesystem.h"
#include <QString>
#include <QWidget>

class QTabWidget;

class EffectPackToolBar : public QWidget
{
    Q_OBJECT

public:
    explicit EffectPackToolBar(const filesystem::path& user_gradients_path,
                               const filesystem::path& user_colors_path,
                               const filesystem::path& effect_files_dir,
                               const filesystem::path& user_curves_path,
                               QWidget* parent = nullptr);

    void reloadUserGradients();
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

    filesystem::path user_gradients_path_;
    filesystem::path user_colors_path_;
    filesystem::path effect_files_dir_;
    filesystem::path user_curves_path_;
    QTabWidget* tabs_ = nullptr;
    int gradients_page_index_ = -1;
    int curves_page_index_ = -1;
};
