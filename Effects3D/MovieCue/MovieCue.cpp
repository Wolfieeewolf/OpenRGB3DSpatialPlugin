// SPDX-License-Identifier: GPL-2.0-only

#include "MovieCue.h"

#include "EffectListManager3D.h"
#include "EffectPacks/EffectPackLibrary.h"
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
#include <QTimer>
#include <QVBoxLayout>
#include <set>
#include <nlohmann/json.hpp>

namespace
{
// Stable local type id; library identity is class_name "MovieCue" (do not add to SpatialEffectTypes.h).
constexpr SpatialEffectType kMovieCueEffectType = static_cast<SpatialEffectType>(36);
} // namespace

void RegisterMovieCueEngine()
{
    EffectListManager3D::get()->RegisterEffect(
        "MovieCue",
        "Movie Cue",
        "Movie Cue",
        "",
        "",
        []() { return new MovieCueEffect; },
        SpatialEffectSettingsLayout::CustomOnly);
}

MovieCueEffect::MovieCueEffect(QWidget* parent)
    : SpatialEffect3D(parent)
{
    listener_ = new MovieCue::ScriptListener(this);
    tick_timer_ = new QTimer(this);
    tick_timer_->setInterval(33);
    connect(tick_timer_, &QTimer::timeout, this, &MovieCueEffect::onTick);

    listener_->SetEffectHandler([this](const std::string& effect_id, const std::string& edge, bool active) {
        bindTab();
        runtime_.OnEffect(effect_id, edge, active);
        if(!runtime_.lastWarning().empty())
        {
            setStatus(QString::fromStdString(runtime_.lastWarning()));
        }
        else if(!runtime_.lastError().empty())
        {
            setStatus(QString::fromStdString(runtime_.lastError()));
        }
        else if(runtime_.IsPlaying())
        {
            setStatus(tr("Playing cue: %1").arg(QString::fromStdString(effect_id)));
            if(!tick_timer_->isActive())
            {
                tick_timer_->start();
            }
        }
        else if(listener_->isListening())
        {
            setStatus(tr("Listening on 127.0.0.1:%1").arg(listener_->port()));
            tick_timer_->stop();
        }
    });
    listener_->SetCatalogJson([this]() { return catalogJson(); });

    QTimer::singleShot(0, this, [this]() {
        if(api_enabled_ && !listener_->isListening())
        {
            startListener();
        }
    });
}

MovieCueEffect::~MovieCueEffect()
{
    stopListener();
    runtime_.StopAll();
}

EffectInfo3D MovieCueEffect::GetEffectInfo() const
{
    EffectInfo3D info = {};
    info.effect_name = "Movie Cue";
    info.effect_description =
        "Script API listener for VLC movie cues. Sidecar .movie_cue.json next to the video; shared looks in library/movie-cue/.";
    info.category = "Movie Cue";
    info.effect_type = kMovieCueEffectType;
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

void MovieCueEffect::SetupCustomUI(QWidget* parent)
{
    if(!parent)
    {
        return;
    }
    bindTab();

    // CustomOnly layout: parent is the settings body (already has a layout). No common menus.
    QWidget* panel = new QWidget();
    panel->setObjectName(QStringLiteral("MovieCueEffectSettings"));
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    help_label_ = new QLabel(
        tr("Listener for VLC sidecar cues. Put Movie.movie_cue.json next to Movie.mp4, keep Script API on, then play "
           "in VLC. Browse Movie Cue effects or Effect Pack maker packs below — Test fire previews without a video.\n"
           "Uses this layer's Zone, Spatial Anchor (origin / reference point), Bounds, and Scale from Effect global settings."),
        panel);
    help_label_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(help_label_);
    layout->addWidget(help_label_);

    api_check_ = new QCheckBox(tr("Enable Script API (127.0.0.1)"), panel);
    api_check_->setChecked(api_enabled_);
    connect(api_check_, &QCheckBox::toggled, this, &MovieCueEffect::onApiToggled);
    layout->addWidget(api_check_);

    allow_strobe_check_ = new QCheckBox(tr("Allow photosensitive / strobe catalog effects"), panel);
    allow_strobe_check_->setChecked(allow_photosensitive_);
    connect(allow_strobe_check_, &QCheckBox::toggled, this, &MovieCueEffect::onAllowStrobeToggled);
    layout->addWidget(allow_strobe_check_);

    QLabel* catalog_heading = new QLabel(tr("Shared effect catalog"), panel);
    QFont heading_font = catalog_heading->font();
    heading_font.setBold(true);
    catalog_heading->setFont(heading_font);
    layout->addWidget(catalog_heading);

    QFormLayout* filter_form = new QFormLayout();
    filter_form->setContentsMargins(0, 0, 0, 0);
    source_combo_ = new QComboBox(panel);
    source_combo_->addItem(tr("Movie Cue effects"), static_cast<int>(CatalogSource::MovieCue));
    source_combo_->addItem(tr("Effect packs"), static_cast<int>(CatalogSource::EffectPacks));
    source_combo_->setToolTip(
        tr("Movie Cue effects: library/movie-cue/ folders.\n"
           "Effect packs: library/packs/ from the Effect Pack maker."));
    connect(source_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MovieCueEffect::onSourceChanged);
    filter_form->addRow(tr("Source"), source_combo_);

    category_combo_ = new QComboBox(panel);
    category_combo_->setToolTip(tr("Filter by type / folder (explosions, washes, …)."));
    connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MovieCueEffect::onCategoryChanged);
    filter_form->addRow(tr("Category"), category_combo_);
    layout->addLayout(filter_form);

    effect_list_ = new QListWidget(panel);
    effect_list_->setMinimumHeight(200);
    effect_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(effect_list_, &QListWidget::currentRowChanged, this, &MovieCueEffect::onEffectSelectionChanged);
    connect(effect_list_, &QListWidget::itemDoubleClicked, this, &MovieCueEffect::onEffectDoubleClicked);
    layout->addWidget(effect_list_, 1);

    notes_label_ = new QLabel(panel);
    notes_label_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(notes_label_);
    notes_label_->setText(tr("Notes: —"));
    layout->addWidget(notes_label_);

    QHBoxLayout* row = new QHBoxLayout();
    QPushButton* reload = new QPushButton(tr("Reload"), panel);
    connect(reload, &QPushButton::clicked, this, &MovieCueEffect::onReloadCatalog);
    row->addWidget(reload);
    QPushButton* test_fire = new QPushButton(tr("Test fire"), panel);
    test_fire->setToolTip(tr("Play the selected catalog effect once (stops any previous test fire)."));
    connect(test_fire, &QPushButton::clicked, this, &MovieCueEffect::onTestFire);
    row->addWidget(test_fire);
    row->addStretch(1);
    layout->addLayout(row);

    status_label_ = new QLabel(panel);
    PluginUiApplyMutedSecondaryLabel(status_label_);
    layout->addWidget(status_label_);

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

RGBColor MovieCueEffect::CalculateColorGrid(float, float, float, float, const GridContext3D&)
{
    return ToRGBColor(0, 0, 0);
}

nlohmann::json MovieCueEffect::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["movie_cue_api"] = api_enabled_;
    j["movie_cue_allow_strobe"] = allow_photosensitive_;
    j["movie_cue_port"] = port_;
    return j;
}

void MovieCueEffect::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    api_enabled_ = settings.value("movie_cue_api", true);
    allow_photosensitive_ = settings.value("movie_cue_allow_strobe", false);
    port_ = static_cast<quint16>(settings.value("movie_cue_port", 27180));
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

void MovieCueEffect::onApiToggled(bool on)
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
        setStatus(tr("Script API off"));
    }
}

void MovieCueEffect::onAllowStrobeToggled(bool on)
{
    allow_photosensitive_ = on;
    runtime_.SetAllowPhotosensitive(on);
}

void MovieCueEffect::onReloadCatalog()
{
    const filesystem::path movie_dir = movieEffectsDir();
    const filesystem::path packs_dir = timelinesDir();
    MovieCue::EnsureCatalogSeeded(movie_dir);
    EffectPack::EnsureLibrarySeeded(packs_dir);

    catalog_ = {};
    std::string err;
    MovieCue::LoadCatalog(movie_dir / "catalog.json", &catalog_, &err);
    MovieCue::ScanCategoryFolders(movie_dir, &catalog_);
    pack_entries_ = EffectPack::ListPacks(packs_dir);

    runtime_.SetMovieEffectsDir(movie_dir);
    runtime_.SetTimelinesDir(packs_dir);
    runtime_.SetCatalog(&catalog_);

    refreshSourceFilters();
    if(listener_ && listener_->isListening())
    {
        setStatus(tr("Listening on 127.0.0.1:%1 — %2 movie / %3 packs")
                      .arg(listener_->port())
                      .arg(catalog_.effects.size())
                      .arg(pack_entries_.size()));
    }
    else
    {
        setStatus(tr("Catalog: %1 movie / %2 packs")
                      .arg(catalog_.effects.size())
                      .arg(pack_entries_.size()));
    }
}

void MovieCueEffect::onSourceChanged(int)
{
    refreshCategoryCombo();
    refreshEffectList();
}

void MovieCueEffect::onCategoryChanged(int)
{
    refreshEffectList();
}

void MovieCueEffect::onTestFire()
{
    fireSelectedCatalogEffect();
}

void MovieCueEffect::onEffectSelectionChanged()
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
    if(selectedSource() == CatalogSource::EffectPacks)
    {
        QString loop;
        int duration = 0;
        for(const EffectPack::PackListEntry& e : pack_entries_)
        {
            if(e.id == id.toStdString())
            {
                loop = QString::fromStdString(e.loop);
                duration = e.duration_ms;
                break;
            }
        }
        notes_label_->setText(tr("Notes: effect pack  ·  id: %1  ·  %2 ms  ·  %3")
                                  .arg(id)
                                  .arg(duration)
                                  .arg(loop.isEmpty() ? tr("once") : loop));
        return;
    }
    const MovieCue::CatalogEffect* meta = MovieCue::FindEffect(catalog_, id.toStdString());
    if(!meta)
    {
        notes_label_->setText(tr("Notes: id: %1").arg(id));
        return;
    }
    const QString notes = meta->photosensitive ? tr("photosensitive") : tr("none");
    notes_label_->setText(tr("Notes: %1  ·  id: %2").arg(notes, id));
}

void MovieCueEffect::onEffectDoubleClicked()
{
    fireSelectedCatalogEffect();
}

void MovieCueEffect::onTick()
{
    if(!runtime_.Tick(33))
    {
        tick_timer_->stop();
        if(listener_ && listener_->isListening())
        {
            setStatus(tr("Listening on 127.0.0.1:%1 — %2 movie / %3 packs")
                          .arg(listener_->port())
                          .arg(catalog_.effects.size())
                          .arg(pack_entries_.size()));
        }
    }
}

void MovieCueEffect::bindTab()
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
                tab_->PrepareEffectPackPreview();
            }
        },
        [this](const EffectPack::Pack& pack, int local_ms, bool force_hw) {
            if(tab_)
            {
                tab_->ApplyEffectPackPreviewFrame(pack, local_ms, force_hw, this);
            }
        });
}

void MovieCueEffect::refreshSourceFilters()
{
    refreshCategoryCombo();
    refreshEffectList();
}

void MovieCueEffect::refreshCategoryCombo()
{
    if(!category_combo_)
    {
        return;
    }
    const QString previous = category_combo_->currentData().toString();
    QSignalBlocker blocker(category_combo_);
    category_combo_->clear();

    if(selectedSource() == CatalogSource::EffectPacks)
    {
        category_combo_->addItem(tr("All packs"), QStringLiteral("all"));
        category_combo_->setEnabled(false);
        return;
    }

    category_combo_->setEnabled(true);
    std::set<std::string> cats;
    for(const MovieCue::CatalogEffect& e : catalog_.effects)
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

void MovieCueEffect::refreshEffectList()
{
    if(!effect_list_)
    {
        return;
    }
    const QString previous_id = selectedEffectId();
    QSignalBlocker blocker(effect_list_);
    effect_list_->clear();

    if(selectedSource() == CatalogSource::EffectPacks)
    {
        for(const EffectPack::PackListEntry& e : pack_entries_)
        {
            auto* item = new QListWidgetItem(QString::fromStdString(e.name.empty() ? e.id : e.name));
            item->setData(Qt::UserRole, QString::fromStdString(e.id));
            item->setToolTip(tr("Effect pack id: %1").arg(QString::fromStdString(e.id)));
            effect_list_->addItem(item);
        }
    }
    else
    {
        const QString cat = selectedCategory();
        for(const MovieCue::CatalogEffect& e : catalog_.effects)
        {
            const QString ecat = QString::fromStdString(e.category.empty() ? "uncategorized" : e.category);
            if(!cat.isEmpty() && ecat != cat)
            {
                continue;
            }
            auto* item = new QListWidgetItem(QString::fromStdString(e.name));
            item->setData(Qt::UserRole, QString::fromStdString(e.id));
            QString tip = tr("Movie Cue id: %1").arg(QString::fromStdString(e.id));
            if(e.photosensitive)
            {
                tip += tr(" · photosensitive");
            }
            item->setToolTip(tip);
            effect_list_->addItem(item);
        }
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

void MovieCueEffect::setStatus(const QString& text)
{
    if(status_label_)
    {
        status_label_->setText(text);
    }
}

void MovieCueEffect::startListener()
{
    bindTab();
    if(catalog_.effects.empty() && pack_entries_.empty())
    {
        onReloadCatalog();
    }
    runtime_.SetTimelinesDir(timelinesDir());
    QString err;
    if(!listener_->Start(port_, &err))
    {
        setStatus(tr("API failed: %1").arg(err));
        return;
    }
    port_ = listener_->port();
    setStatus(tr("Listening on 127.0.0.1:%1 — %2 movie / %3 packs")
                  .arg(port_)
                  .arg(catalog_.effects.size())
                  .arg(pack_entries_.size()));
}

void MovieCueEffect::stopListener()
{
    if(listener_)
    {
        listener_->Stop();
    }
}

void MovieCueEffect::fireSelectedCatalogEffect()
{
    const QString id = selectedEffectId();
    if(id.isEmpty())
    {
        setStatus(tr("Select a catalog effect first"));
        return;
    }
    if(!api_enabled_ || !listener_->isListening())
    {
        // Test fire still works locally through the runtime without VLC.
        startListener();
    }
    bindTab();
    runtime_.SetAllowPhotosensitive(allow_photosensitive_);
    // Exclusive preview: stop any prior test-fire / cue play so only this look runs.
    runtime_.StopAll();
    runtime_.OnEffect(id.toStdString(), "pulse", true);
    if(!runtime_.lastWarning().empty())
    {
        setStatus(QString::fromStdString(runtime_.lastWarning()));
        return;
    }
    if(!runtime_.lastError().empty())
    {
        setStatus(QString::fromStdString(runtime_.lastError()));
        return;
    }
    setStatus(tr("Test fire: %1").arg(id));
    if(runtime_.IsPlaying() && !tick_timer_->isActive())
    {
        tick_timer_->start();
    }
}

QString MovieCueEffect::selectedEffectId() const
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

QString MovieCueEffect::selectedCategory() const
{
    if(!category_combo_)
    {
        return {};
    }
    return category_combo_->currentData().toString();
}

MovieCueEffect::CatalogSource MovieCueEffect::selectedSource() const
{
    if(!source_combo_)
    {
        return CatalogSource::MovieCue;
    }
    return static_cast<CatalogSource>(source_combo_->currentData().toInt());
}

filesystem::path MovieCueEffect::movieEffectsDir() const
{
    return PluginSettingsPaths::MovieCueLibraryDir(OpenRGB3DSpatialPlugin::APIPointer);
}

filesystem::path MovieCueEffect::timelinesDir() const
{
    return PluginSettingsPaths::PacksDir(OpenRGB3DSpatialPlugin::APIPointer);
}

std::string MovieCueEffect::catalogJson() const
{
    nlohmann::json j;
    j["effects"] = nlohmann::json::array();
    for(const MovieCue::CatalogEffect& e : catalog_.effects)
    {
        j["effects"].push_back({
            {"id", e.id},
            {"name", e.name},
            {"category", e.category},
            {"source", "movie_cue"},
            {"photosensitive", e.photosensitive},
        });
    }
    for(const EffectPack::PackListEntry& e : pack_entries_)
    {
        j["effects"].push_back({
            {"id", e.id},
            {"name", e.name.empty() ? e.id : e.name},
            {"category", "packs"},
            {"source", "effect_pack"},
            {"photosensitive", false},
        });
    }
    return j.dump();
}
