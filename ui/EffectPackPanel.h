// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPackPlayer.h"
#include "filesystem.h"
#include <QElapsedTimer>
#include <QGroupBox>

class QTimer;

namespace Ui {
class EffectPackPanel;
}

class OpenRGB3DSpatialTab;

class EffectPackPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit EffectPackPanel(QWidget* parent = nullptr);
    ~EffectPackPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);
    void stopPreview();

private slots:
    void onRefresh();
    void onPreview();
    void onStop();
    void onSeedExample();
    void onTick();

private:
    void setPlayingUi(bool playing);
    void populateList();
    filesystem::path packsDir() const;

    Ui::EffectPackPanel* ui = nullptr;
    OpenRGB3DSpatialTab* tab_ = nullptr;
    QTimer* timer_ = nullptr;
    EffectPack::Player player_;
    QElapsedTimer wall_;
    int last_elapsed_ms_ = 0;
};
