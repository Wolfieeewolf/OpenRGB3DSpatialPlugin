// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "SpatialEffect3D.h"
#include "MovieSyncCatalog.h"
#include "MovieSyncListener.h"
#include "MovieSyncRuntime.h"
#include "MovieSyncStream.h"
#include "MovieSyncSystemMedia.h"

class QCheckBox;
class QSpinBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
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

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void onApiToggled(bool on);
    void onAllowStrobeToggled(bool on);
    void onReloadCatalog();
    void onCategoryChanged(int index);
    void onPreview();
    void onStopPreview();
    void onEffectSelectionChanged();
    void onEffectDoubleClicked();
    void onTick();
    void onStreamTick();

private:
    /** One cue edge (VLC, or a stream's cue file): play / hold / release a look. */
    void handleCueEdge(const std::string& effect_id, const std::string& edge, bool active);
    /** Browser extension report (POST /v1/stream). */
    void onStreamReport(const std::string& json_body);
    void updateStreamLabel(std::int64_t now_ms);
    void bindTab();
    void refreshCategoryCombo();
    void refreshEffectList();
    void setStatus(const QString& text);
    void setPlayingUi(bool playing);
    void clearPreviewIfIdle();
    void restoreIdleStatus();
    QString effectDisplayName(const QString& effect_id) const;
    QString humanizeRuntimeMessage(const std::string& msg) const;
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
    std::int64_t last_tick_ms_ = 0;

    QCheckBox* api_check_ = nullptr;
    QCheckBox* allow_strobe_check_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* help_label_ = nullptr;
    QLabel* notes_label_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QListWidget* effect_list_ = nullptr;
    QPushButton* preview_button_ = nullptr;
    QPushButton* stop_button_ = nullptr;
    OpenRGB3DSpatialTab* tab_ = nullptr;
    bool api_enabled_ = true;
    bool allow_photosensitive_ = false;
    quint16 port_ = 27180;

    /* Streaming (Netflix, Prime, ...): reports from the browser extension or the Windows media
       controls, matched to a cue file in movie-sync/streams/, played on the stream's clock. */
    MovieSync::StreamCueLibrary stream_cues_;
    MovieSync::StreamCuePlayer stream_player_;
    MovieSync::SystemMediaReader system_media_;
    QTimer* stream_timer_ = nullptr;
    MovieSync::StreamState extension_state_;
    std::int64_t extension_last_ms_ = 0;
    std::string stream_match_key_;
    const MovieSync::StreamCueFile* stream_file_ = nullptr;
    std::int64_t stream_label_ms_ = 0;
    int lights_delay_ms_ = 0;
    QLabel* stream_label_ = nullptr;
    QSpinBox* lights_delay_spin_ = nullptr;
};
