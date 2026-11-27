// SPDX-License-Identifier: GPL-2.0-or-later

#include "MovieSync.h"

#include "EffectListManager3D.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "OpenRGB3DSpatialTab.h"
#include "PlayerEngines.h"
#include "PluginSettingsPaths.h"
#include "PluginUiUtils.h"
#include "SpatialEffectTypes.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <chrono>
#include <cmath>
#include <set>
#include <nlohmann/json.hpp>

namespace
{
// Stable local type id; library identity is class_name "MovieSync" (do not add to SpatialEffectTypes.h).
constexpr SpatialEffectType kMovieSyncEffectType = static_cast<SpatialEffectType>(36);
} // namespace

QString MovieSyncEffect::effectDisplayName(const QString& effect_id) const
{
    const MovieSync::CatalogEffect* meta = MovieSync::FindEffect(catalog_, effect_id.toStdString());
    if(meta && !meta->name.empty())
    {
        return QString::fromStdString(meta->name);
    }
    return effect_id;
}

QString MovieSyncEffect::humanizeRuntimeMessage(const std::string& msg) const
{
    if(msg.empty())
    {
        return {};
    }
    const QString q = QString::fromStdString(msg);
    if(msg == "effect_id required")
    {
        return tr("Look id required");
    }
    const QString unknown_prefix = QStringLiteral("unknown effect_id: ");
    if(q.startsWith(unknown_prefix))
    {
        return tr("Unknown look: %1").arg(effectDisplayName(q.mid(unknown_prefix.size())));
    }
    // "Blocked photosensitive effect 'id' (...)"
    if(q.startsWith(QStringLiteral("Blocked photosensitive effect '"))
       && q.contains(QLatin1Char('\'')))
    {
        const int start = QStringLiteral("Blocked photosensitive effect '").size();
        const int end = q.indexOf(QLatin1Char('\''), start);
        if(end > start)
        {
            const QString id = q.mid(start, end - start);
            return tr("Blocked photosensitive look \u201c%1\u201d (enable Allow photosensitive looks).")
                .arg(effectDisplayName(id));
        }
    }
    return q;
}

void RegisterMovieSyncEngine()
{
    EffectListManager3D::get()->RegisterEffect(
        "MovieSync",
        "Movie Sync",
        "Movie Sync",
        "",
        "",
        []() { return new MovieSyncEffect; },
        SpatialEffectSettingsLayout::CustomOnly);
}

MovieSyncEffect::MovieSyncEffect(QWidget* parent)
    : SpatialEffect3D(parent)
{
    listener_ = new MovieSync::ScriptListener(this);
    tick_timer_ = new QTimer(this);
    tick_timer_->setTimerType(Qt::PreciseTimer);
    tick_timer_->setInterval(Action::kPlaybackTickMs);
    connect(tick_timer_, &QTimer::timeout, this, &MovieSyncEffect::onTick);

    listener_->SetEffectHandler([this](const std::string& effect_id, const std::string& edge, bool active) {
        handleCueEdge(effect_id, edge, active);
    });
    listener_->SetCatalogJson([this]() { return catalogJson(); });

    /* Streaming: the browser extension reports here; the Windows media controls are read in the
       background; a stream's cue file is played on its clock. */
    listener_->SetStreamHandler([this](const std::string& body) { onStreamReport(body); });
    stream_player_.SetEdgeHandler([this](const std::string& effect_id, const std::string& edge, bool active) {
        handleCueEdge(effect_id, edge, active);
    });
    stream_timer_ = new QTimer(this);
    stream_timer_->setTimerType(Qt::PreciseTimer);
    stream_timer_->setInterval(20);
    connect(stream_timer_, &QTimer::timeout, this, &MovieSyncEffect::onStreamTick);

    QTimer::singleShot(0, this, [this]() {
        if(api_enabled_ && !listener_->isListening())
        {
            startListener();
        }
    });
}

MovieSyncEffect::~MovieSyncEffect()
{
    stream_player_.SetEdgeHandler(nullptr);
    stopListener();
    runtime_.StopAll();
}

void MovieSyncEffect::handleCueEdge(const std::string& effect_id, const std::string& edge, bool active)
{
    bindTab();
    const bool was_playing = runtime_.IsPlaying();
    runtime_.OnEffect(effect_id, edge, active);
    const bool playing = runtime_.IsPlaying();
    if(!playing)
    {
        tick_timer_->stop();
        setPlayingUi(false);
        if(was_playing)
        {
            clearPreviewIfIdle();
        }
    }
    if(!runtime_.lastWarning().empty())
    {
        setStatus(humanizeRuntimeMessage(runtime_.lastWarning()));
    }
    else if(!runtime_.lastError().empty())
    {
        setStatus(humanizeRuntimeMessage(runtime_.lastError()));
    }
    else if(playing && active)
    {
        setStatus(tr("Playing cue: %1").arg(effectDisplayName(QString::fromStdString(effect_id))));
        setPlayingUi(true);
        if(!tick_timer_->isActive())
        {
            last_tick_ms_ = 0;
            tick_timer_->start();
        }
    }
    else if(listener_->isListening())
    {
        setStatus(tr("Listening on 127.0.0.1:%1").arg(listener_->port()));
    }
}

namespace
{
std::int64_t SteadyNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

QString FormatClock(double s)
{
    const int total = (int)std::max(0.0, s);
    const int h = total / 3600;
    const int m = (total % 3600) / 60;
    const int sec = total % 60;
    return h > 0 ? QString::asprintf("%d:%02d:%02d", h, m, sec) : QString::asprintf("%d:%02d", m, sec);
}
} // namespace

void MovieSyncEffect::onStreamReport(const std::string& json_body)
{
    try
    {
        const nlohmann::json j = nlohmann::json::parse(json_body.empty() ? "{}" : json_body);
        MovieSync::StreamState st;
        st.source = j.value("source", std::string("extension"));
        st.service = j.value("service", std::string());
        st.id = j.value("id", std::string());
        st.title = j.value("title", std::string());
        st.episode = j.value("episode", std::string());
        st.position_s = j.value("position_ms", 0.0) / 1000.0;
        st.duration_s = j.value("duration_ms", 0.0) / 1000.0;
        st.rate = j.value("rate", 1.0);
        st.paused = j.value("paused", true);
        st.ad = j.value("ad", false);
        st.received_ms = SteadyNowMs();
        if(j.value("stopped", false))
        {
            extension_last_ms_ = 0;
            return;
        }
        extension_state_ = st;
        extension_last_ms_ = st.received_ms;
    }
    catch(...)
    {
    }
}

void MovieSyncEffect::onStreamTick()
{
    const std::int64_t now = SteadyNowMs();
    /* The extension knows most (exact IDs, live position): it wins while it is reporting.
       Otherwise the Windows media controls, if something is playing there. */
    MovieSync::StreamState st;
    bool have = false;
    if(extension_last_ms_ > 0 && now - extension_last_ms_ < 5000)
    {
        st = extension_state_;
        have = true;
    }
    else if(system_media_.Latest(&st) && now - st.received_ms < 3000)
    {
        have = true;
    }
    if(!have)
    {
        if(!stream_match_key_.empty())
        {
            stream_player_.Stop();
            stream_file_ = nullptr;
            stream_match_key_.clear();
            stream_label_ms_ = 0;
        }
        /* Nothing playing: check four times a second instead of fifty. */
        if(stream_timer_->interval() != 250)
        {
            stream_timer_->setInterval(250);
        }
        updateStreamLabel(now);
        return;
    }
    if(stream_timer_->interval() != 20)
    {
        stream_timer_->setInterval(20);
    }

    /* Match a cue file once per title (service, ID, name, length). */
    const std::string key = st.service + "|" + st.id + "|" + st.title + "|" + st.episode + "|"
                            + std::to_string((long long)std::llround(st.duration_s));
    if(key != stream_match_key_)
    {
        stream_match_key_ = key;
        stream_file_ = stream_cues_.Match(st);
    }
    stream_player_.SetLightsDelayMs(lights_delay_ms_);
    if(st.received_ms != stream_player_.State().received_ms || !stream_player_.Active())
    {
        stream_player_.OnState(st, stream_file_, now);
    }
    else
    {
        stream_player_.Tick(now);
    }
    updateStreamLabel(now);
}

void MovieSyncEffect::updateStreamLabel(std::int64_t now_ms)
{
    if(!stream_label_ || now_ms - stream_label_ms_ < 400)
    {
        return;
    }
    stream_label_ms_ = now_ms;
    if(stream_match_key_.empty())
    {
        QString text = tr("Streaming: nothing playing (browser extension or Windows media controls). "
                          "%1 cue file(s) in movie-sync/streams.").arg(stream_cues_.Count());
        if(!stream_cues_.Errors().empty())
        {
            text += QLatin1Char('\n') + tr("Problem: %1").arg(QString::fromStdString(stream_cues_.Errors().front()));
        }
        stream_label_->setText(text);
        return;
    }
    const MovieSync::StreamState& st = stream_player_.State();
    QString what = QString::fromStdString(st.title);
    if(!st.episode.empty())
    {
        what += QStringLiteral(" — ") + QString::fromStdString(st.episode);
    }
    if(what.isEmpty())
    {
        what = st.id.empty() ? tr("(unknown title)") : QStringLiteral("#") + QString::fromStdString(st.id);
    }
    const QString state = st.ad ? tr("ad") : (st.paused ? tr("paused") : tr("playing"));
    const QString cues = stream_file_
                             ? tr("cue file: %1 (%2 cues)")
                                   .arg(QString::fromStdString(stream_file_->path.filename().string()))
                                   .arg(stream_file_->cues.size())
                             : tr("no cue file for this title (%1 in movie-sync/streams)").arg(stream_cues_.Count());
    stream_label_->setText(tr("Streaming (%1, %2): %3\n%4 / %5  %6\n%7")
                               .arg(QString::fromStdString(st.service), QString::fromStdString(st.source), what,
                                    FormatClock(stream_player_.EstimatedPosition(now_ms)),
                                    FormatClock(st.duration_s), state, cues));
}

EffectInfo3D MovieSyncEffect::GetEffectInfo() const
{
    EffectInfo3D info = {};
    info.effect_name = "Movie Sync";
    info.effect_description =
        "Script API listener for VLC movie sync. Sidecar .movie_sync.json next to the video; shared looks in movie-sync/.";
    info.category = "Movie Sync";
    info.effect_type = kMovieSyncEffectType;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 0;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_frequency = false;
    info.use_size_parameter = false;
    info.show_color_controls = false;
    info.show_speed_control = false;
    info.show_brightness_control = false;
    info.show_frequency_control = false;
    info.show_detail_control = false;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_fps_control = false;
    info.show_surface_control = false;
    info.show_position_offset_control = false;
    info.show_room_output_control = false;
    return info;
}

void MovieSyncEffect::SetupCustomUI(QWidget* parent)
{
    if(!parent)
    {
        return;
    }
    bindTab();

    QWidget* panel = new QWidget();
    panel->setObjectName(QStringLiteral("MovieSyncEffectSettings"));
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    help_label_ = new QLabel(
        tr("VLC sidecar cues: Movie.movie_sync.json next to the video. Enable Script API, then play in VLC."),
        panel);
    help_label_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(help_label_);
    layout->addWidget(help_label_);

    api_check_ = new QCheckBox(tr("Enable Script API (127.0.0.1)"), panel);
    api_check_->setChecked(api_enabled_);
    connect(api_check_, &QCheckBox::toggled, this, &MovieSyncEffect::onApiToggled);
    layout->addWidget(api_check_);

    allow_strobe_check_ = new QCheckBox(tr("Allow photosensitive looks"), panel);
    allow_strobe_check_->setChecked(allow_photosensitive_);
    connect(allow_strobe_check_, &QCheckBox::toggled, this, &MovieSyncEffect::onAllowStrobeToggled);
    layout->addWidget(allow_strobe_check_);

    QLabel* catalog_heading = new QLabel(tr("Movie Sync looks"), panel);
    QFont heading_font = catalog_heading->font();
    heading_font.setBold(true);
    catalog_heading->setFont(heading_font);
    layout->addWidget(catalog_heading);

    QFormLayout* filter_form = new QFormLayout();
    filter_form->setContentsMargins(0, 0, 0, 0);
    category_combo_ = new QComboBox(panel);
    category_combo_->setToolTip(tr("Filter by category folder"));
    connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MovieSyncEffect::onCategoryChanged);
    filter_form->addRow(tr("Category"), category_combo_);
    layout->addLayout(filter_form);

    effect_list_ = new QListWidget(panel);
    effect_list_->setMinimumHeight(200);
    effect_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(effect_list_, &QListWidget::currentRowChanged, this, &MovieSyncEffect::onEffectSelectionChanged);
    connect(effect_list_, &QListWidget::itemDoubleClicked, this, &MovieSyncEffect::onEffectDoubleClicked);
    layout->addWidget(effect_list_, 1);

    notes_label_ = new QLabel(panel);
    notes_label_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(notes_label_);
    notes_label_->setText(tr("Notes: —"));
    layout->addWidget(notes_label_);

    QHBoxLayout* row = new QHBoxLayout();
    QPushButton* reload = new QPushButton(tr("Reload"), panel);
    connect(reload, &QPushButton::clicked, this, &MovieSyncEffect::onReloadCatalog);
    row->addWidget(reload);
    preview_button_ = new QPushButton(tr("Preview"), panel);
    preview_button_->setToolTip(tr("Play the selected look once"));
    connect(preview_button_, &QPushButton::clicked, this, &MovieSyncEffect::onPreview);
    row->addWidget(preview_button_);
    stop_button_ = new QPushButton(tr("Stop"), panel);
    stop_button_->setEnabled(false);
    stop_button_->setToolTip(tr("Stop the current preview or cue"));
    connect(stop_button_, &QPushButton::clicked, this, &MovieSyncEffect::onStopPreview);
    row->addWidget(stop_button_);
    row->addStretch(1);
    layout->addLayout(row);

    status_label_ = new QLabel(panel);
    PluginUiApplyMutedSecondaryLabel(status_label_);
    layout->addWidget(status_label_);

    /* Streaming: Netflix, Prime Video, Crunchyroll, ... in a browser (extension) or a Windows app. */
    QLabel* stream_heading = new QLabel(tr("Streaming"), panel);
    stream_heading->setFont(heading_font);
    layout->addWidget(stream_heading);
    QLabel* stream_help = new QLabel(
        tr("Netflix, Prime Video, Crunchyroll, Disney+ and others: install the OpenRGB browser extension "
           "(integrations/browser-extension); it also works in the Netflix / Prime Video apps on Windows. "
           "Without it, Windows apps that report to the media controls (Crunchyroll, Prime Video) still "
           "work. Cue files go in movie-sync/streams/ (VLC sidecar format plus \"streams\": "
           "[{\"service\", \"id\"}]); press Reload after adding one."),
        panel);
    stream_help->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(stream_help);
    layout->addWidget(stream_help);
    stream_label_ = new QLabel(panel);
    stream_label_->setWordWrap(true);
    stream_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(stream_label_);
    QFormLayout* stream_form = new QFormLayout();
    stream_form->setContentsMargins(0, 0, 0, 0);
    lights_delay_spin_ = new QSpinBox(panel);
    lights_delay_spin_->setRange(-2000, 2000);
    lights_delay_spin_->setSingleStep(25);
    lights_delay_spin_->setSuffix(tr(" ms"));
    lights_delay_spin_->setValue(lights_delay_ms_);
    lights_delay_spin_->setToolTip(tr("Positive: cues fire later (your TV or Bluetooth makes the picture / "
                                      "sound lag behind). Negative: cues fire earlier (your LED devices are "
                                      "slow to change)."));
    connect(lights_delay_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        lights_delay_ms_ = v;
    });
    stream_form->addRow(tr("Lights delay"), lights_delay_spin_);
    layout->addLayout(stream_form);

    AddWidgetToParent(panel, parent);

    onReloadCatalog();
    if(api_enabled_)
    {
        startListener();
    }
    else
    {
        setStatus(tr("Script API off"));
    }
}

RGBColor MovieSyncEffect::CalculateColorGrid(float, float, float, float, const GridContext3D&)
{
    return ToRGBColor(0, 0, 0);
}

nlohmann::json MovieSyncEffect::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["movie_sync_api"] = api_enabled_;
    j["movie_sync_allow_strobe"] = allow_photosensitive_;
    j["movie_sync_port"] = port_;
    j["movie_sync_lights_delay_ms"] = lights_delay_ms_;
    return j;
}

void MovieSyncEffect::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    api_enabled_ = settings.value("movie_sync_api", true);
    allow_photosensitive_ = settings.value("movie_sync_allow_strobe", false);
    port_ = static_cast<quint16>(settings.value("movie_sync_port", 27180));
    lights_delay_ms_ = std::clamp(settings.value("movie_sync_lights_delay_ms", 0), -2000, 2000);
    if(lights_delay_spin_)
    {
        QSignalBlocker b(lights_delay_spin_);
        lights_delay_spin_->setValue(lights_delay_ms_);
    }
    if(api_check_)
    {
        QSignalBlocker b(api_check_);
        api_check_->setChecked(api_enabled_);
    }
    if(allow_strobe_check_)
    {
        QSignalBlocker b(allow_strobe_check_);
        allow_strobe_check_->setChecked(allow_photosensitive_);
    }
    runtime_.SetAllowPhotosensitive(allow_photosensitive_);
    if(api_enabled_)
    {
        onReloadCatalog();
        startListener();
    }
    else
    {
        stopListener();
    }
}

void MovieSyncEffect::onApiToggled(bool on)
{
    api_enabled_ = on;
    if(on)
    {
        startListener();
    }
    else
    {
        stopListener();
        runtime_.StopAll();
        tick_timer_->stop();
        setPlayingUi(false);
        clearPreviewIfIdle();
        setStatus(tr("Script API off"));
    }
}

void MovieSyncEffect::onAllowStrobeToggled(bool on)
{
    allow_photosensitive_ = on;
    runtime_.SetAllowPhotosensitive(on);
    if(!runtime_.IsPlaying())
    {
        tick_timer_->stop();
        setPlayingUi(false);
        clearPreviewIfIdle();
    }
}

void MovieSyncEffect::onReloadCatalog()
{
    const filesystem::path movie_dir = movieEffectsDir();
    catalog_ = {};
    std::string err;
    MovieSync::LoadCatalog(MovieSync::CatalogFilePath(movie_dir), &catalog_, &err);
    MovieSync::ScanCategoryFolders(movie_dir, &catalog_);

    runtime_.SetMovieEffectsDir(movie_dir);
    runtime_.SetCatalog(&catalog_);

    /* Stream cue files: re-matched against whatever plays next tick. */
    stream_player_.Stop();
    stream_file_ = nullptr;
    stream_match_key_.clear();
    /* movie_dir is movie-sync/effects; stream cue files live beside it in movie-sync/streams. */
    filesystem::path base = movie_dir.lexically_normal();
    if(base.filename().empty())
    {
        base = base.parent_path(); /* trailing slash */
    }
    stream_cues_.Load((base.filename() == "effects" ? base.parent_path() : base) / "streams");

    refreshCategoryCombo();
    refreshEffectList();
    if(listener_ && listener_->isListening())
    {
        setStatus(tr("Listening on 127.0.0.1:%1 — %2 Movie Sync look(s)")
                      .arg(listener_->port())
                      .arg(catalog_.effects.size()));
    }
    else
    {
        if(catalog_.effects.empty())
        {
            setStatus(tr("No looks in movie-sync/"));
        }
        else
        {
            setStatus(tr("Catalog: %1 Movie Sync look(s)").arg(catalog_.effects.size()));
        }
    }
}

void MovieSyncEffect::onCategoryChanged(int)
{
    refreshEffectList();
}

void MovieSyncEffect::onPreview()
{
    fireSelectedCatalogEffect();
}

void MovieSyncEffect::onStopPreview()
{
    runtime_.StopAll();
    stream_player_.ForgetLevels();
    tick_timer_->stop();
    setPlayingUi(false);
    clearPreviewIfIdle();
    restoreIdleStatus();
}

void MovieSyncEffect::onEffectSelectionChanged()
{
    const QString id = selectedEffectId();
    if(!notes_label_)
    {
        return;
    }
    if(id.isEmpty())
    {
        notes_label_->setText(tr("Notes: —"));
        return;
    }
    const MovieSync::CatalogEffect* meta = MovieSync::FindEffect(catalog_, id.toStdString());
    if(!meta)
    {
        notes_label_->setText(tr("Notes: unknown look (%1)").arg(id));
        return;
    }
    const QString notes = meta->photosensitive ? tr("photosensitive") : tr("none");
    // Sidecar authors need the stable Movie Sync id next to the display name.
    notes_label_->setText(tr("Notes: %1  ·  id: %2").arg(notes, id));
}

void MovieSyncEffect::onEffectDoubleClicked()
{
    fireSelectedCatalogEffect();
}

void MovieSyncEffect::onTick()
{
    int dt = Action::MeasureTickMs(&last_tick_ms_);
    /* The film is paused: running looks freeze with it (and carry on when it plays). */
    if(stream_player_.Active() && stream_player_.State().paused)
    {
        dt = 0;
    }
    if(!runtime_.Tick(dt))
    {
        tick_timer_->stop();
        setPlayingUi(false);
        // Once packs must go black when they finish (do not leave the last lit frame).
        clearPreviewIfIdle();
        restoreIdleStatus();
    }
}

void MovieSyncEffect::bindTab()
{
    if(!tab_)
    {
        for(QWidget* w = parentWidget(); w; w = w->parentWidget())
        {
            if(OpenRGB3DSpatialTab* t = qobject_cast<OpenRGB3DSpatialTab*>(w))
            {
                tab_ = t;
                break;
            }
        }
    }
    if(!tab_ && qApp)
    {
        const QWidgetList tops = qApp->topLevelWidgets();
        for(QWidget* top : tops)
        {
            if(OpenRGB3DSpatialTab* t = qobject_cast<OpenRGB3DSpatialTab*>(top))
            {
                tab_ = t;
                break;
            }
            if(OpenRGB3DSpatialTab* t = top->findChild<OpenRGB3DSpatialTab*>())
            {
                tab_ = t;
                break;
            }
        }
    }
    if(!tab_)
    {
        return;
    }
    runtime_.SetApplyCallbacks(
        [this]() {
            if(tab_)
            {
                tab_->PrepareActionPreview();
            }
        },
        [this](const Action::Pack& pack, int local_ms, bool force_hw) {
            if(tab_)
            {
                tab_->ApplyActionPreviewFrame(pack, local_ms, force_hw, this);
            }
        });
}

void MovieSyncEffect::refreshCategoryCombo()
{
    if(!category_combo_)
    {
        return;
    }
    const QString previous = category_combo_->currentData().toString();
    QSignalBlocker blocker(category_combo_);
    category_combo_->clear();
    category_combo_->setEnabled(true);

    std::set<std::string> cats;
    for(const MovieSync::CatalogEffect& e : catalog_.effects)
    {
        cats.insert(e.category.empty() ? "uncategorized" : e.category);
    }
    for(const std::string& cat : cats)
    {
        category_combo_->addItem(QString::fromStdString(cat), QString::fromStdString(cat));
    }
    if(category_combo_->count() == 0)
    {
        category_combo_->addItem(tr("(empty)"), QString());
        return;
    }
    int idx = category_combo_->findData(previous);
    if(idx < 0)
    {
        idx = 0;
    }
    category_combo_->setCurrentIndex(idx);
}

void MovieSyncEffect::refreshEffectList()
{
    if(!effect_list_)
    {
        return;
    }
    const QString previous_id = selectedEffectId();
    QSignalBlocker blocker(effect_list_);
    effect_list_->clear();

    const QString cat = selectedCategory();
    for(const MovieSync::CatalogEffect& e : catalog_.effects)
    {
        const QString ecat = QString::fromStdString(e.category.empty() ? "uncategorized" : e.category);
        if(!cat.isEmpty() && ecat != cat)
        {
            continue;
        }
        auto* item = new QListWidgetItem(QString::fromStdString(e.name));
        item->setData(Qt::UserRole, QString::fromStdString(e.id));
        QString tip = tr("Movie Sync id: %1").arg(QString::fromStdString(e.id));
        if(e.photosensitive)
        {
            tip += tr(" · photosensitive");
        }
        item->setToolTip(tip);
        effect_list_->addItem(item);
    }

    int restore = -1;
    for(int i = 0; i < effect_list_->count(); ++i)
    {
        if(effect_list_->item(i)->data(Qt::UserRole).toString() == previous_id)
        {
            restore = i;
            break;
        }
    }
    if(restore < 0 && effect_list_->count() > 0)
    {
        restore = 0;
    }
    effect_list_->setCurrentRow(restore);
    onEffectSelectionChanged();
}

void MovieSyncEffect::setStatus(const QString& text)
{
    if(status_label_)
    {
        status_label_->setText(text);
    }
}

void MovieSyncEffect::setPlayingUi(bool playing)
{
    if(preview_button_)
    {
        preview_button_->setEnabled(!playing);
    }
    if(stop_button_)
    {
        stop_button_->setEnabled(playing);
    }
}

void MovieSyncEffect::clearPreviewIfIdle()
{
    if(tab_ && !runtime_.IsPlaying())
    {
        tab_->ClearActionPreview(true, this);
    }
}

void MovieSyncEffect::restoreIdleStatus()
{
    if(listener_ && listener_->isListening())
    {
        setStatus(tr("Listening on 127.0.0.1:%1 — %2 Movie Sync look(s)")
                      .arg(listener_->port())
                      .arg(catalog_.effects.size()));
    }
    else if(!api_enabled_)
    {
        setStatus(tr("Script API off"));
    }
    else if(catalog_.effects.empty())
    {
        setStatus(tr("No looks in movie-sync/"));
    }
    else
    {
        setStatus(tr("Catalog: %1 Movie Sync look(s)").arg(catalog_.effects.size()));
    }
}

void MovieSyncEffect::startListener()
{
    bindTab();
    if(catalog_.effects.empty())
    {
        onReloadCatalog();
    }
    QString err;
    if(!listener_->Start(port_, &err))
    {
        setStatus(tr("API failed: %1").arg(err));
        return;
    }
    port_ = listener_->port();
    setStatus(tr("Listening on 127.0.0.1:%1 — %2 Movie Sync look(s)")
                  .arg(port_)
                  .arg(catalog_.effects.size()));
    system_media_.Start();
    if(stream_timer_ && !stream_timer_->isActive())
    {
        stream_timer_->start();
    }
}

void MovieSyncEffect::stopListener()
{
    if(listener_)
    {
        listener_->Stop();
    }
    if(stream_timer_)
    {
        stream_timer_->stop();
    }
    system_media_.Stop();
    stream_player_.Stop();
    stream_file_ = nullptr;
    stream_match_key_.clear();
}

void MovieSyncEffect::fireSelectedCatalogEffect()
{
    const QString id = selectedEffectId();
    if(id.isEmpty())
    {
        setStatus(tr("Select a look first"));
        return;
    }
    if(!api_enabled_ || !listener_->isListening())
    {
        startListener();
    }
    bindTab();
    runtime_.SetAllowPhotosensitive(allow_photosensitive_);
    runtime_.StopAll();
    stream_player_.ForgetLevels();
    clearPreviewIfIdle();
    runtime_.OnEffect(id.toStdString(), "pulse", true);
    if(!runtime_.lastWarning().empty())
    {
        setPlayingUi(false);
        setStatus(humanizeRuntimeMessage(runtime_.lastWarning()));
        return;
    }
    if(!runtime_.lastError().empty())
    {
        setPlayingUi(false);
        setStatus(humanizeRuntimeMessage(runtime_.lastError()));
        return;
    }
    setStatus(tr("Preview: %1").arg(effectDisplayName(id)));
    setPlayingUi(runtime_.IsPlaying());
    if(runtime_.IsPlaying() && !tick_timer_->isActive())
    {
        last_tick_ms_ = 0;
        tick_timer_->start();
    }
}

QString MovieSyncEffect::selectedEffectId() const
{
    if(!effect_list_)
    {
        return {};
    }
    QListWidgetItem* item = effect_list_->currentItem();
    if(!item)
    {
        return {};
    }
    return item->data(Qt::UserRole).toString();
}

QString MovieSyncEffect::selectedCategory() const
{
    if(!category_combo_)
    {
        return {};
    }
    return category_combo_->currentData().toString();
}

filesystem::path MovieSyncEffect::movieEffectsDir() const
{
    return PluginSettingsPaths::MovieSyncEffectsDir(OpenRGB3DSpatialPlugin::APIPointer);
}

std::string MovieSyncEffect::catalogJson() const
{
    nlohmann::json j;
    j["effects"] = nlohmann::json::array();
    for(const MovieSync::CatalogEffect& e : catalog_.effects)
    {
        j["effects"].push_back({
            {"id", e.id},
            {"name", e.name},
            {"category", e.category},
            {"source", "movie_sync"},
            {"photosensitive", e.photosensitive},
        });
    }
    return j.dump();
}
