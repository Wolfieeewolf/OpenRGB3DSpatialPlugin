// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "SpatialEffect3D.h"
#include "MovieSyncCatalog.h"
#include "MovieSyncListener.h"
#include "MovieSyncRuntime.h"
#include "MovieSyncStream.h"
#include "MovieSyncSystemMedia.h"
#include "MovieSyncPanel.h"

#include <QPointer>
#include <map>

class QCheckBox;
class QSpinBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTimer;
class QNetworkAccessManager;
class QNetworkReply;
class QPixmap;
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
    /** One cue edge from VLC or a stream's cue file: play / hold / release a look (and log it). */
    void handleCueEdge(const std::string& effect_id, const std::string& edge, bool active, bool from_vlc);
    /** Browser extension report (POST /v1/stream). */
    void onStreamReport(const std::string& json_body);
    /** Now Playing card, timeline, next cue and connection lights (a few times a second). */
    void refreshNowPlaying(std::int64_t now_ms);
    void logEvent(MovieSyncLog::Kind kind, const QString& text, bool with_film_time = true);
    /** Card artwork: an image beside the cue file, else the Windows thumbnail, else the
        extension's https link (downloaded once, kept in memory). */
    void updateArtwork(const MovieSync::StreamState& st);
    void showArtwork(const QPixmap& pixmap);
    void tryArtwork(std::size_t index);
    /** Cue file + the looks chosen for the service's intro / recap / credits, as one list for
        the player; rebuilt when any of it changes. Returns what to play (or nullptr). */
    const MovieSync::StreamCueFile* rebuildPlayFile(const MovieSync::StreamState& st, bool* changed);
    /** Title details into the log; photosensitive safety when the title is marked (if on). */
    void noteTitleDetails(const MovieSync::StreamState& st);
    void restoreTitleSafety();
    void refreshMarkerCombos();
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
    QSpinBox* lights_delay_spin_ = nullptr;

    /* Panel: Now Playing card, cue timeline, activity log, connection lights. */
    QLabel* now_source_ = nullptr;
    QLabel* now_title_ = nullptr;
    QLabel* now_time_ = nullptr;
    QLabel* now_cuefile_ = nullptr;
    QLabel* now_next_ = nullptr;
    MovieSyncTimeline* timeline_ = nullptr;
    QListWidget* log_list_ = nullptr;
    QLabel* connections_label_ = nullptr;
    std::string match_how_;
    std::string timeline_key_;
    /* What the log last saw, to report only changes. */
    std::string seen_source_;
    bool seen_paused_ = true;
    bool seen_ad_ = false;
    double seen_pos_s_ = -1.0;
    std::int64_t seen_pos_ms_ = 0;
    std::int64_t vlc_last_ms_ = 0;
    std::string vlc_last_cue_;
    QLabel* now_art_ = nullptr;
    QNetworkAccessManager* net_ = nullptr;
    QPointer<QNetworkReply> art_reply_;
    std::string art_key_;
    std::map<std::string, QPixmap> art_cache_;
    std::vector<std::string> art_candidates_;
    std::string art_warned_key_;
    void artworkFailed(std::size_t index, const std::string& url, const std::string& why);
    QLabel* now_meta_ = nullptr;
    QLabel* now_synopsis_ = nullptr;

    /* Service marks (intro / recap / credits) and what lights do in them. */
    QComboBox* marker_combo_[3] = {nullptr, nullptr, nullptr};
    std::string marker_look_[3]; /* intro, recap, credits: catalog effect id, "" = nothing */
    MovieSync::StreamCueFile play_file_;
    bool play_has_ = false;
    std::string play_sig_;
    std::string details_sig_;
    std::string markers_sig_;
    std::string seen_marker_;

    /* Photosensitive safety for titles the service marks (only if the user turns it on). */
    QCheckBox* auto_safety_check_ = nullptr;
    bool auto_safety_ = false;
    bool safety_forced_ = false;
    std::string safety_key_;
    std::string safety_override_key_;
};
