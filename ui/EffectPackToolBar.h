// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "filesystem.h"
#include <QWidget>

class QTabWidget;

class EffectPackToolBar : public QWidget
{
    Q_OBJECT

public:
    explicit EffectPackToolBar(const filesystem::path& user_gradients_path, QWidget* parent = nullptr);

    void reloadUserGradients();

signals:
    void effectClicked(int block_type);
    void colorClicked(unsigned int rgb);
    void gradientPresetClicked(const QString& preset_id);
    void curvePresetClicked(const QString& preset_id);

private:
    void buildUi();

    filesystem::path user_gradients_path_;
    QTabWidget* tabs_ = nullptr;
    int gradients_page_index_ = -1;
};
