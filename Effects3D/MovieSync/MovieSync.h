// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "SpatialEffect3D.h"
#include "MovieSyncCatalog.h"
#include "MovieSyncListener.h"
#include "MovieSyncRuntime.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QTimer;
class OpenRGB3DSpatialTab;

class MovieSyncEffect : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit MovieSyncEffect(QWidget* parent = nullptr);
    ~MovieSyncEffect() override;

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }
    bool UsesSpatialSamplingQuantization() const override { return false; }
    bool SkipsSpatialSampleWarp() const override { return true; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void onApiToggled(bool on);
    void onAllowStrobeToggled(bool on);
    void onReloadCatalog();
    void onCategoryChanged(int index);
    void onPreview();
    void onEffectSelectionChanged();
    void onEffectDoubleClicked();
    void onTick();

private:
    void bindTab();
    void refreshCategoryCombo();
    void refreshEffectList();
    void setStatus(const QString& text);
    void startListener();
    void stopListener();
    void fireSelectedCatalogEffect();
    QString selectedEffectId() const;
    QString selectedCategory() const;
    filesystem::path movieEffectsDir() const;
    std::string catalogJson() const;

    MovieSync::Catalog catalog_;
    MovieSync::ScriptListener* listener_ = nullptr;
    MovieSync::CueRuntime runtime_;
    QTimer* tick_timer_ = nullptr;

    QCheckBox* api_check_ = nullptr;
    QCheckBox* allow_strobe_check_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* help_label_ = nullptr;
    QLabel* notes_label_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QListWidget* effect_list_ = nullptr;
    OpenRGB3DSpatialTab* tab_ = nullptr;
    bool api_enabled_ = true;
    bool allow_photosensitive_ = false;
    quint16 port_ = 27180;
};
