// SPDX-License-Identifier: GPL-2.0-only

#include "TriggersPanel.h"

#include "Triggers/TriggerBinding.h"
#include "Actions/ActionLibrary.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginSettingsPaths.h"
#include "PluginUiUtils.h"
#include "ui_TriggersPanel.h"

#include <algorithm>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

TriggersPanel::TriggersPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::TriggersPanel)
{
    ui->setupUi(this);
    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &TriggersPanel::onTick);
}

TriggersPanel::~TriggersPanel()
{
    stopAll();
    registry_.StopAll();
    delete ui;
}

void TriggersPanel::bindTab(OpenRGB3DSpatialTab* tab)
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
                                 Trigger::EventEdge edge) {
        const bool was_playing = runtime_.IsPlaying();
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
            if(was_playing)
            {
                clearPreviewIfIdle();
            }
        }
        if(!error.isEmpty())
        {
            setStatus(error);
        }
        else
        {
            setStatus(runtime_.IsPlaying()
                          ? tr("Playing bound actions…")
                          : tr("Idle"));
        }
    });
    runtime_.SetEffectsDir(triggerEffectsDir());
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
                tab_->ApplyActionPreviewFrame(pack, local_ms, force_hw);
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

    connect(ui->addButton, &QPushButton::clicked, this, &TriggersPanel::onAdd);
    connect(ui->editButton, &QPushButton::clicked, this, &TriggersPanel::onEdit);
    connect(ui->deleteButton, &QPushButton::clicked, this, &TriggersPanel::onDelete);
    connect(ui->fireButton, &QPushButton::clicked, this, &TriggersPanel::onFire);
    connect(ui->stopButton, &QPushButton::clicked, this, &TriggersPanel::onStop);
    connect(ui->holdButton, &QPushButton::toggled, this, &TriggersPanel::onHoldToggled);
    connect(ui->bindingsList, &QListWidget::currentRowChanged, this, &TriggersPanel::onSelectionChanged);
    connect(ui->bindingsList, &QListWidget::itemChanged, this, &TriggersPanel::onItemChanged);
    connect(ui->bindingsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onEdit();
    });
}

filesystem::path TriggersPanel::bindingsPath() const
{
    if(!tab_ || !tab_->resource_manager)
    {
        return {};
    }
    return PluginSettingsPaths::TriggersFile(tab_->resource_manager);
}

filesystem::path TriggersPanel::triggerEffectsDir() const
{
    if(!tab_ || !tab_->resource_manager)
    {
        return {};
    }
    return PluginSettingsPaths::TriggerEffectsDir(tab_->resource_manager);
}

filesystem::path TriggersPanel::triggersDir() const
{
    if(!tab_ || !tab_->resource_manager)
    {
        return {};
    }
    return PluginSettingsPaths::TriggersDir(tab_->resource_manager);
}

bool TriggersPanel::catalogEnabled(const std::string& id) const
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

bool TriggersPanel::isCatalogItem(const QListWidgetItem* item) const
{
    return item && item->data(Qt::UserRole + 1).toString() == QStringLiteral("catalog");
}

void TriggersPanel::syncRuntime()
{
    Trigger::Document play = user_;
    for(Trigger::Binding b : catalog_)
    {
        bool owned = false;
        for(const Trigger::Binding& user_binding : user_.bindings)
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

void TriggersPanel::reloadDocument()
{
    Trigger::Document doc;
    std::string err;
    if(!Trigger::LoadOrEmpty(bindingsPath(), &doc, &err))
    {
        setStatus(tr("Load failed: %1").arg(QString::fromStdString(err)));
        return;
    }
    user_ = std::move(doc);
    std::vector<std::string> warnings;
    Trigger::LoadCatalog(triggersDir(), &catalog_, &warnings);
    syncRuntime();
    populateList();
    if(!warnings.empty())
    {
        setStatus(QString::fromStdString(warnings.front()));
        return;
    }
    setStatus(tr("Idle"));
}

void TriggersPanel::saveDocument()
{
    std::string err;
    if(!Trigger::SaveToFile(bindingsPath(), user_, &err))
    {
        setStatus(tr("Save failed: %1").arg(QString::fromStdString(err)));
        return;
    }
    if(!runtime_.IsPlaying())
    {
        setStatus(tr("Bindings saved"));
    }
}

void TriggersPanel::populateList()
{
    const QString selected = ui->bindingsList->currentItem()
                                 ? ui->bindingsList->currentItem()->data(Qt::UserRole).toString()
                                 : QString();

    std::vector<Action::ActionListEntry> packs = Action::ListActions(triggerEffectsDir());

    ui->bindingsList->blockSignals(true);
    ui->bindingsList->clear();
    int restore_row = -1;
    auto add_row = [&](const Trigger::Binding& b, bool catalog) {
        Trigger::EventSource* src = registry_.Find(b.source);
        if(!src)
        {
            return;
        }
        if(catalog)
        {
            for(const Trigger::Binding& user_binding : user_.bindings)
            {
                if(user_binding.id == b.id)
                {
                    return;
                }
            }
        }
        QString pack_label;
        for(const Action::ActionListEntry& p : packs)
        {
            if(p.id == b.pack_id)
            {
                pack_label = QString::fromStdString(p.name);
                break;
            }
        }
        if(pack_label.isEmpty())
        {
            pack_label = tr("Missing: %1").arg(QString::fromStdString(b.pack_id));
        }
        QString event_label = QString::fromStdString(b.event);
        for(const Trigger::EventInfo& ev : src->ListEvents())
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
    for(const Trigger::Binding& b : user_.bindings)
    {
        add_row(b, false);
    }
    for(const Trigger::Binding& b : catalog_)
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

void TriggersPanel::setStatus(const QString& text)
{
    ui->statusLabel->setText(text);
}

void TriggersPanel::onSelectionChanged()
{
    const bool ok = ui->bindingsList->currentRow() >= 0 && !isCatalogItem(ui->bindingsList->currentItem());
    ui->editButton->setEnabled(ok);
    ui->deleteButton->setEnabled(ok);
}

void TriggersPanel::onItemChanged(QListWidgetItem* item)
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
        for(Trigger::Binding& b : user_.bindings)
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
        const bool was_playing = runtime_.IsPlaying();
        runtime_.StopBinding(id);
        if(!runtime_.IsPlaying())
        {
            timer_->stop();
            if(was_playing)
            {
                clearPreviewIfIdle();
            }
            setStatus(tr("Idle"));
        }
    }
    syncRuntime();
    saveDocument();
}

bool TriggersPanel::editBindingDialog(Trigger::Binding* binding)
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
        Trigger::EventSource* src = registry_.Find(sid.toStdString());
        if(!src)
        {
            return;
        }
        for(const Trigger::EventInfo& ev : src->ListEvents())
        {
            QString label = QString::fromStdString(ev.display_name);
            if(ev.edge == Trigger::EventEdge::Pulse)
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

    for(const Action::ActionListEntry& p : Action::ListActions(triggerEffectsDir()))
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
    form->addRow(tr("Trigger action"), pack_combo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if(pack_combo->count() == 0)
    {
        QMessageBox::information(this, tr("No packs"),
                                 tr("Create a trigger action first (Object Creator → Actions)."));
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
        binding->id = Trigger::MakeBindingId();
        binding->enabled = true;
    }
    return true;
}

void TriggersPanel::onAdd()
{
    if(!tab_ || !tab_->resource_manager)
    {
        QMessageBox::warning(this, tr("Triggers"),
                             tr("Plugin data path is not available."));
        return;
    }
    Trigger::Binding b;
    if(!editBindingDialog(&b))
    {
        return;
    }
    user_.bindings.push_back(std::move(b));
    syncRuntime();
    saveDocument();
    populateList();
}

void TriggersPanel::onEdit()
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
    for(Trigger::Binding& b : user_.bindings)
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

void TriggersPanel::onDelete()
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
    if(QMessageBox::question(this, tr("Delete binding"), tr("Delete this Trigger?"))
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
                                  [&](const Trigger::Binding& b) { return b.id == id; }),
                   bindings.end());
    syncRuntime();
    saveDocument();
    populateList();
}

void TriggersPanel::onFire()
{
    if(registry_.manual())
    {
        registry_.manual()->Fire();
    }
}

void TriggersPanel::onStop()
{
    const bool was_playing = runtime_.IsPlaying();
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
        if(was_playing)
        {
            clearPreviewIfIdle();
        }
        setStatus(tr("Idle"));
    }
}

void TriggersPanel::onHoldToggled(bool checked)
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

void TriggersPanel::onTick()
{
    if(!runtime_.Tick(timer_->interval()))
    {
        timer_->stop();
        clearPreviewIfIdle();
        setStatus(tr("Idle"));
    }
}

void TriggersPanel::clearPreviewIfIdle()
{
    if(tab_ && !runtime_.IsPlaying())
    {
        tab_->ClearActionPreview(true);
    }
}

void TriggersPanel::stopAll()
{
    onStop();
    runtime_.StopAll();
    clearPreviewIfIdle();
    if(timer_)
    {
        timer_->stop();
    }
    if(ui)
    {
        setStatus(tr("Idle"));
    }
}
