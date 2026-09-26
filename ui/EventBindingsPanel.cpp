// SPDX-License-Identifier: GPL-2.0-only

#include "EventBindingsPanel.h"

#include "EventBindings/EventBinding.h"
#include "EffectPacks/EffectPackLibrary.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginSettingsPaths.h"
#include "PluginUiUtils.h"
#include "ui_EventBindingsPanel.h"

#include <algorithm>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

EventBindingsPanel::EventBindingsPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::EventBindingsPanel)
{
    ui->setupUi(this);
    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &EventBindingsPanel::onTick);
}

EventBindingsPanel::~EventBindingsPanel()
{
    stopAll();
    registry_.StopAll();
    delete ui;
}

void EventBindingsPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab || bound_)
    {
        return;
    }
    tab_ = tab;
    bound_ = true;

    PluginUiApplyMutedSecondaryLabel(ui->helpLabel->label());
    PluginUiApplyMutedSecondaryLabel(ui->statusLabel);

    registry_.BuildForPlatform();
    registry_.SetListener([this](const std::string& source,
                                 const std::string& event,
                                 bool active,
                                 EffectBinding::EventEdge edge) {
        runtime_.OnEvent(source, event, active, edge);
        const QString error = QString::fromStdString(runtime_.lastError());
        if(runtime_.IsPlaying())
        {
            runtime_.Tick(0);
        }
        if(runtime_.IsPlaying())
        {
            if(!timer_->isActive())
            {
                timer_->start();
            }
        }
        else
        {
            timer_->stop();
        }
        if(!error.isEmpty())
        {
            setStatus(error);
        }
        else
        {
            setStatus(runtime_.IsPlaying()
                          ? QStringLiteral("Playing bound packs…")
                          : QStringLiteral("Idle"));
        }
    });
    runtime_.SetPacksDir(packsDir());
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
                tab_->ApplyEffectPackPreviewFrame(pack, local_ms, force_hw);
            }
        });

    reloadDocument();
    /* Defer native registration — winId() during tab construction hangs the host UI. */
    QTimer::singleShot(0, this, [this]() {
        if(bound_)
        {
            registry_.StartAll();
        }
    });

    connect(ui->addButton, &QPushButton::clicked, this, &EventBindingsPanel::onAdd);
    connect(ui->editButton, &QPushButton::clicked, this, &EventBindingsPanel::onEdit);
    connect(ui->deleteButton, &QPushButton::clicked, this, &EventBindingsPanel::onDelete);
    connect(ui->fireButton, &QPushButton::clicked, this, &EventBindingsPanel::onFire);
    connect(ui->stopButton, &QPushButton::clicked, this, &EventBindingsPanel::onStop);
    connect(ui->holdButton, &QPushButton::toggled, this, &EventBindingsPanel::onHoldToggled);
    connect(ui->bindingsList, &QListWidget::currentRowChanged, this, &EventBindingsPanel::onSelectionChanged);
    connect(ui->bindingsList, &QListWidget::itemChanged, this, &EventBindingsPanel::onItemChanged);
    connect(ui->bindingsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onEdit();
    });
}

filesystem::path EventBindingsPanel::bindingsPath() const
{
    if(!tab_ || !tab_->resource_manager)
    {
        return {};
    }
    return PluginSettingsPaths::EffectBindingsFile(tab_->resource_manager);
}

filesystem::path EventBindingsPanel::packsDir() const
{
    if(!tab_ || !tab_->resource_manager)
    {
        return {};
    }
    return PluginSettingsPaths::TimelinesDir(tab_->resource_manager);
}

filesystem::path EventBindingsPanel::catalogDir() const
{
    if(!tab_ || !tab_->resource_manager)
    {
        return {};
    }
    return PluginSettingsPaths::BindingsDir(tab_->resource_manager);
}

bool EventBindingsPanel::catalogEnabled(const std::string& id) const
{
    for(const std::string& have : user_.catalog_enabled)
    {
        if(have == id)
        {
            return true;
        }
    }
    return false;
}

bool EventBindingsPanel::isCatalogItem(const QListWidgetItem* item) const
{
    return item && item->data(Qt::UserRole + 1).toString() == QStringLiteral("catalog");
}

void EventBindingsPanel::syncRuntime()
{
    EffectBinding::Document play = user_;
    for(EffectBinding::Binding b : catalog_)
    {
        bool owned = false;
        for(const EffectBinding::Binding& user_binding : user_.bindings)
        {
            if(user_binding.id == b.id)
            {
                owned = true;
                break;
            }
        }
        if(owned || !catalogEnabled(b.id))
        {
            continue;
        }
        b.enabled = true;
        play.bindings.push_back(std::move(b));
    }
    runtime_.SetDocument(std::move(play));
}

void EventBindingsPanel::reloadDocument()
{
    EffectBinding::Document doc;
    std::string err;
    if(!EffectBinding::LoadOrEmpty(bindingsPath(), &doc, &err))
    {
        setStatus(QStringLiteral("Load failed: %1").arg(QString::fromStdString(err)));
        return;
    }
    user_ = std::move(doc);
    std::vector<std::string> warnings;
    EffectBinding::LoadCatalog(catalogDir(), &catalog_, &warnings);
    syncRuntime();
    populateList();
    if(!warnings.empty())
    {
        setStatus(QString::fromStdString(warnings.front()));
        return;
    }
    setStatus(QStringLiteral("Idle"));
}

void EventBindingsPanel::saveDocument()
{
    std::string err;
    if(!EffectBinding::SaveToFile(bindingsPath(), user_, &err))
    {
        setStatus(QStringLiteral("Save failed: %1").arg(QString::fromStdString(err)));
        return;
    }
    if(!runtime_.IsPlaying())
    {
        setStatus(QStringLiteral("Bindings saved"));
    }
}

void EventBindingsPanel::populateList()
{
    const QString selected = ui->bindingsList->currentItem()
                                 ? ui->bindingsList->currentItem()->data(Qt::UserRole).toString()
                                 : QString();

    std::vector<EffectPack::PackListEntry> packs = EffectPack::ListPacks(packsDir());

    ui->bindingsList->blockSignals(true);
    ui->bindingsList->clear();
    int restore_row = -1;
    auto add_row = [&](const EffectBinding::Binding& b, bool catalog) {
        EffectBinding::EventSource* src = registry_.Find(b.source);
        if(!src)
        {
            return;
        }
        if(catalog)
        {
            for(const EffectBinding::Binding& user_binding : user_.bindings)
            {
                if(user_binding.id == b.id)
                {
                    return;
                }
            }
        }
        QString pack_label = QString::fromStdString(b.pack_id);
        for(const EffectPack::PackListEntry& p : packs)
        {
            if(p.id == b.pack_id)
            {
                pack_label = QString::fromStdString(p.name);
                break;
            }
        }
        QString event_label = QString::fromStdString(b.event);
        for(const EffectBinding::EventInfo& ev : src->ListEvents())
        {
            if(ev.id == b.event)
            {
                event_label = QString::fromStdString(ev.display_name);
                break;
            }
        }
        QString label = QStringLiteral("%1  →  %2 / %3")
                            .arg(pack_label, QString::fromUtf8(src->displayName()), event_label);
        if(catalog)
        {
            label += QStringLiteral("  (catalog)");
        }
        auto* item = new QListWidgetItem(label, ui->bindingsList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        const bool on = catalog ? catalogEnabled(b.id) : b.enabled;
        item->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, QString::fromStdString(b.id));
        item->setData(Qt::UserRole + 1, catalog ? QStringLiteral("catalog") : QStringLiteral("user"));
        if(item->data(Qt::UserRole).toString() == selected)
        {
            restore_row = ui->bindingsList->row(item);
        }
    };
    for(const EffectBinding::Binding& b : user_.bindings)
    {
        add_row(b, false);
    }
    for(const EffectBinding::Binding& b : catalog_)
    {
        add_row(b, true);
    }
    ui->bindingsList->blockSignals(false);
    if(restore_row >= 0)
    {
        ui->bindingsList->setCurrentRow(restore_row);
    }
    onSelectionChanged();
}

void EventBindingsPanel::setStatus(const QString& text)
{
    ui->statusLabel->setText(text);
}

void EventBindingsPanel::onSelectionChanged()
{
    const bool ok = ui->bindingsList->currentRow() >= 0 && !isCatalogItem(ui->bindingsList->currentItem());
    ui->editButton->setEnabled(ok);
    ui->deleteButton->setEnabled(ok);
}

void EventBindingsPanel::onItemChanged(QListWidgetItem* item)
{
    if(!item)
    {
        return;
    }
    const std::string id = item->data(Qt::UserRole).toString().toStdString();
    const bool on = item->checkState() == Qt::Checked;
    if(isCatalogItem(item))
    {
        auto& ids = user_.catalog_enabled;
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
        if(on)
        {
            ids.push_back(id);
        }
    }
    else
    {
        for(EffectBinding::Binding& b : user_.bindings)
        {
            if(b.id == id)
            {
                b.enabled = on;
                break;
            }
        }
    }
    if(!on)
    {
        runtime_.StopBinding(id);
        if(!runtime_.IsPlaying())
        {
            timer_->stop();
            setStatus(QStringLiteral("Idle"));
        }
    }
    syncRuntime();
    saveDocument();
}

bool EventBindingsPanel::editBindingDialog(EffectBinding::Binding* binding)
{
    if(!binding || !tab_)
    {
        return false;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(binding->id.empty() ? tr("Add binding") : tr("Edit binding"));
    auto* form = new QFormLayout(&dlg);

    auto* source_combo = new QComboBox(&dlg);
    auto* event_combo = new QComboBox(&dlg);
    auto* pack_combo = new QComboBox(&dlg);

    for(const auto& src : registry_.sources())
    {
        if(!src)
        {
            continue;
        }
        source_combo->addItem(QString::fromUtf8(src->displayName()), QString::fromUtf8(src->id()));
    }

    auto refill_events = [&]() {
        event_combo->clear();
        const QString sid = source_combo->currentData().toString();
        EffectBinding::EventSource* src = registry_.Find(sid.toStdString());
        if(!src)
        {
            return;
        }
        for(const EffectBinding::EventInfo& ev : src->ListEvents())
        {
            QString label = QString::fromStdString(ev.display_name);
            if(ev.edge == EffectBinding::EventEdge::Pulse)
            {
                label += QStringLiteral(" (pulse)");
            }
            event_combo->addItem(label, QString::fromStdString(ev.id));
        }
    };
    connect(source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int) {
        refill_events();
    });
    refill_events();

    EffectPack::EnsureLibrarySeeded(packsDir());
    for(const EffectPack::PackListEntry& p : EffectPack::ListPacks(packsDir()))
    {
        pack_combo->addItem(QString::fromStdString(p.name), QString::fromStdString(p.id));
    }

    if(!binding->source.empty())
    {
        const int si = source_combo->findData(QString::fromStdString(binding->source));
        if(si >= 0)
        {
            source_combo->setCurrentIndex(si);
        }
        refill_events();
    }
    if(!binding->event.empty())
    {
        const int ei = event_combo->findData(QString::fromStdString(binding->event));
        if(ei >= 0)
        {
            event_combo->setCurrentIndex(ei);
        }
    }
    if(!binding->pack_id.empty())
    {
        const int pi = pack_combo->findData(QString::fromStdString(binding->pack_id));
        if(pi >= 0)
        {
            pack_combo->setCurrentIndex(pi);
        }
    }

    form->addRow(tr("Source"), source_combo);
    form->addRow(tr("Event"), event_combo);
    form->addRow(tr("Effect pack"), pack_combo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if(pack_combo->count() == 0)
    {
        QMessageBox::information(this, tr("No packs"),
                                 tr("Create an effect pack first (Object Creator → Effect Pack)."));
        return false;
    }

    if(!binding->pack_id.empty() && pack_combo->findData(QString::fromStdString(binding->pack_id)) < 0)
    {
        pack_combo->addItem(tr("Missing: %1").arg(QString::fromStdString(binding->pack_id)),
                            QString::fromStdString(binding->pack_id));
        pack_combo->setCurrentIndex(pack_combo->count() - 1);
    }

    if(dlg.exec() != QDialog::Accepted)
    {
        return false;
    }

    const std::string source = source_combo->currentData().toString().toStdString();
    const std::string event = event_combo->currentData().toString().toStdString();
    const std::string pack_id = pack_combo->currentData().toString().toStdString();
    if(source.empty() || event.empty() || pack_id.empty())
    {
        return false;
    }
    binding->source = source;
    binding->event = event;
    binding->pack_id = pack_id;
    if(binding->id.empty())
    {
        binding->id = EffectBinding::MakeBindingId();
        binding->enabled = true;
    }
    return true;
}

void EventBindingsPanel::onAdd()
{
    EffectBinding::Binding b;
    if(!editBindingDialog(&b))
    {
        return;
    }
    user_.bindings.push_back(std::move(b));
    syncRuntime();
    saveDocument();
    populateList();
}

void EventBindingsPanel::onEdit()
{
    QListWidgetItem* item = ui->bindingsList->currentItem();
    if(!item)
    {
        return;
    }
    if(isCatalogItem(item))
    {
        return;
    }
    const std::string id = item->data(Qt::UserRole).toString().toStdString();
    for(EffectBinding::Binding& b : user_.bindings)
    {
        if(b.id == id)
        {
            if(editBindingDialog(&b))
            {
                runtime_.StopBinding(b.id);
                if(!runtime_.IsPlaying())
                {
                    timer_->stop();
                }
                syncRuntime();
                saveDocument();
                populateList();
            }
            return;
        }
    }
}

void EventBindingsPanel::onDelete()
{
    QListWidgetItem* item = ui->bindingsList->currentItem();
    if(!item)
    {
        return;
    }
    if(isCatalogItem(item))
    {
        return;
    }
    const std::string id = item->data(Qt::UserRole).toString().toStdString();
    if(QMessageBox::question(this, tr("Delete binding"), tr("Delete this event binding?"))
       != QMessageBox::Yes)
    {
        return;
    }
    runtime_.StopBinding(id);
    if(!runtime_.IsPlaying())
    {
        timer_->stop();
    }
    auto& bindings = user_.bindings;
    bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
                                  [&](const EffectBinding::Binding& b) { return b.id == id; }),
                   bindings.end());
    syncRuntime();
    saveDocument();
    populateList();
}

void EventBindingsPanel::onFire()
{
    if(registry_.manual())
    {
        registry_.manual()->Fire();
    }
}

void EventBindingsPanel::onStop()
{
    if(registry_.manual())
    {
        registry_.manual()->StopFire();
        registry_.manual()->EndHold();
    }
    ui->holdButton->blockSignals(true);
    ui->holdButton->setChecked(false);
    ui->holdButton->blockSignals(false);
    if(!runtime_.IsPlaying())
    {
        timer_->stop();
        setStatus(QStringLiteral("Idle"));
    }
}

void EventBindingsPanel::onHoldToggled(bool checked)
{
    if(!registry_.manual())
    {
        return;
    }
    if(checked)
    {
        registry_.manual()->BeginHold();
    }
    else
    {
        registry_.manual()->EndHold();
    }
}

void EventBindingsPanel::onTick()
{
    if(!runtime_.Tick(timer_->interval()))
    {
        timer_->stop();
        setStatus(QStringLiteral("Idle"));
    }
}

void EventBindingsPanel::stopAll()
{
    onStop();
    runtime_.StopAll();
    if(timer_)
    {
        timer_->stop();
    }
    if(ui)
    {
        setStatus(QStringLiteral("Idle"));
    }
}
