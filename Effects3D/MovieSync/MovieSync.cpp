// SPDX-License-Identifier: GPL-2.0-only

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
#include <QTimer>
#include <QVBoxLayout>
#include <set>
#include <nlohmann/json.hpp>

namespace
{
// Stable local type id; library identity is class_name "MovieSync" (do not add to SpatialEffectTypes.h).
constexpr SpatialEffectType kMovieSyncEffectType = static_cast<SpatialEffectType>(36);
} // namespace

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
    tick_timer_->setInterval(33);
    connect(tick_timer_, &QTimer::timeout, this, &MovieSyncEffect::onTick);

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

MovieSyncEffect::~MovieSyncEffect()
{
    stopListener();
    runtime_.StopAll();
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

    // CustomOnly layout: parent is the settings body (already has a layout). No common menus.
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
    QPushButton* preview = new QPushButton(tr("Preview"), panel);
    preview->setToolTip(tr("Play the selected look once"));
    connect(preview, &QPushButton::clicked, this, &MovieSyncEffect::onPreview);
    row->addWidget(preview);
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
    return j;
}

void MovieSyncEffect::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    api_enabled_ = settings.value("movie_sync_api", true);
    allow_photosensitive_ = settings.value("movie_sync_allow_strobe", false);
    port_ = static_cast<quint16>(settings.value("movie_sync_port", 27180));
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
        if(tab_)
        {
            tab_->ClearActionPreview(true, this);
        }
        setStatus(tr("Script API off"));
    }
}

void MovieSyncEffect::onAllowStrobeToggled(bool on)
{
    allow_photosensitive_ = on;
    runtime_.SetAllowPhotosensitive(on);
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
        notes_label_->setText(tr("Notes: id: %1").arg(id));
        return;
    }
    const QString notes = meta->photosensitive ? tr("photosensitive") : tr("none");
    notes_label_->setText(tr("Notes: %1  ·  id: %2").arg(notes, id));
}

void MovieSyncEffect::onEffectDoubleClicked()
{
    fireSelectedCatalogEffect();
}

void MovieSyncEffect::onTick()
{
    if(!runtime_.Tick(33))
    {
        tick_timer_->stop();
        // Once packs must go black when they finish (do not leave the last lit frame).
        if(tab_)
        {
            tab_->ClearActionPreview(true, this);
        }
        if(listener_ && listener_->isListening())
        {
            setStatus(tr("Listening on 127.0.0.1:%1 — %2 Movie Sync look(s)")
                          .arg(listener_->port())
                          .arg(catalog_.effects.size()));
        }
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
}

void MovieSyncEffect::stopListener()
{
    if(listener_)
    {
        listener_->Stop();
    }
}

void MovieSyncEffect::fireSelectedCatalogEffect()
{
    const QString id = selectedEffectId();
    if(id.isEmpty())
    {
        setStatus(tr("Select a catalog effect first"));
        return;
    }
    if(!api_enabled_ || !listener_->isListening())
    {
        startListener();
    }
    bindTab();
    runtime_.SetAllowPhotosensitive(allow_photosensitive_);
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
    setStatus(tr("Preview: %1").arg(id));
    if(runtime_.IsPlaying() && !tick_timer_->isActive())
    {
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
