// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "SpatialEffect3D.h"
#include "MovieCueCatalog.h"
#include "MovieCueListener.h"
#include "MovieCueRuntime.h"
#include "EffectPacks/EffectPackLibrary.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QTimer;
class OpenRGB3DSpatialTab;

class MovieCueEffect : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit MovieCueEffect(QWidget* parent = nullptr);
    ~MovieCueEffect() override;

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
    void onSourceChanged(int index);
    void onCategoryChanged(int index);
    void onTestFire();
    void onEffectSelectionChanged();
    void onEffectDoubleClicked();
    void onTick();

private:
    enum class CatalogSource
    {
        MovieCue = 0,
        EffectPacks = 1,
    };

    void bindTab();
    void refreshSourceFilters();
    void refreshCategoryCombo();
    void refreshEffectList();
    void setStatus(const QString& text);
    void startListener();
    void stopListener();
    void fireSelectedCatalogEffect();
    QString selectedEffectId() const;
    QString selectedCategory() const;
    CatalogSource selectedSource() const;
    filesystem::path movieEffectsDir() const;
    filesystem::path timelinesDir() const;
    std::string catalogJson() const;

    MovieCue::Catalog catalog_;
    std::vector<EffectPack::PackListEntry> pack_entries_;
    MovieCue::ScriptListener* listener_ = nullptr;
    MovieCue::CueRuntime runtime_;
    QTimer* tick_timer_ = nullptr;

    QCheckBox* api_check_ = nullptr;
    QCheckBox* allow_strobe_check_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* help_label_ = nullptr;
    QLabel* notes_label_ = nullptr;
    QComboBox* source_combo_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QListWidget* effect_list_ = nullptr;
    OpenRGB3DSpatialTab* tab_ = nullptr;
    bool api_enabled_ = true;
    bool allow_photosensitive_ = false;
    quint16 port_ = 27180;
};
