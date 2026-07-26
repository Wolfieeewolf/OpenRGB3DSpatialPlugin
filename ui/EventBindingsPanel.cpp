// SPDX-License-Identifier: GPL-2.0-only

#include "EventBindingsPanel.h"

#include "EventBindings/EventBinding.h"
#include "EventBindings/ManualEventSource.h"
#include "EffectPacks/EffectPackLibrary.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginSettingsPaths.h"
#include "PluginUiUtils.h"
#include "ui_EventBindingsPanel.h"

#include <algorithm>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

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
    registry_.SetListener([this](const std::string& source, const std::string& event, bool active) {
        runtime_.OnEvent(source, event, active);
        if(runtime_.IsPlaying() && !timer_->isActive())
        {
            timer_->start();
        }
        if(!runtime_.IsPlaying())
        {
            timer_->stop();
        }
        setStatus(runtime_.IsPlaying()
                      ? QStringLiteral("Playing bound packs…")
                      : QStringLiteral("Idle"));
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
    // Defer native HWND / WTS registration — calling winId() on the plugin tab during
    // construction hangs OpenRGB before the UI can appear.
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
    return PluginSettingsPaths::EffectPacksDir(tab_->resource_manager);
}

void EventBindingsPanel::reloadDocument()
{
    EffectBinding::Document doc;
    std::string err;
    EffectBinding::LoadOrEmpty(bindingsPath(), &doc, &err);
    runtime_.SetDocument(std::move(doc));
    populateList();
}

void EventBindingsPanel::saveDocument()
{
    std::string err;
    if(!EffectBinding::SaveToFile(bindingsPath(), runtime_.document(), &err))
    {
        setStatus(QStringLiteral("Save failed: %1").arg(QString::fromStdString(err)));
        return;
    }
    setStatus(QStringLiteral("Bindings saved"));
}

void EventBindingsPanel::populateList()
{
    ui->bindingsList->blockSignals(true);
    ui->bindingsList->clear();
    for(const EffectBinding::Binding& b : runtime_.document().bindings)
    {
        if(!registry_.HasSource(b.source))
        {
            // Keep on disk; hide foreign-OS bindings in the list.
            continue;
        }
        const QString label = QStringLiteral("%1 → %2 / %3%4")
                                  .arg(QString::fromStdString(b.pack_id))
                                  .arg(QString::fromStdString(b.source))
                                  .arg(QString::fromStdString(b.event))
                                  .arg(b.enabled ? QString() : QStringLiteral(" (off)"));
        auto* item = new QListWidgetItem(label, ui->bindingsList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(b.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, QString::fromStdString(b.id));
    }
    ui->bindingsList->blockSignals(false);
    onSelectionChanged();
}

void EventBindingsPanel::setStatus(const QString& text)
{
    ui->statusLabel->setText(text);
}

void EventBindingsPanel::onSelectionChanged()
{
    const bool ok = ui->bindingsList->currentRow() >= 0;
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
    for(EffectBinding::Binding& b : runtime_.mutableDocument()->bindings)
    {
        if(b.id == id)
        {
            b.enabled = item->checkState() == Qt::Checked;
            saveDocument();
            populateList();
            return;
        }
    }
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
            event_combo->addItem(QString::fromStdString(ev.display_name),
                                 QString::fromStdString(ev.id));
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

    if(dlg.exec() != QDialog::Accepted)
    {
        return false;
    }

    binding->source = source_combo->currentData().toString().toStdString();
    binding->event = event_combo->currentData().toString().toStdString();
    binding->pack_id = pack_combo->currentData().toString().toStdString();
    if(binding->id.empty())
    {
        binding->id = EffectBinding::MakeBindingId();
        binding->enabled = true;
    }
    return !binding->source.empty() && !binding->event.empty() && !binding->pack_id.empty();
}

void EventBindingsPanel::onAdd()
{
    EffectBinding::Binding b;
    if(!editBindingDialog(&b))
    {
        return;
    }
    runtime_.mutableDocument()->bindings.push_back(std::move(b));
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
    const std::string id = item->data(Qt::UserRole).toString().toStdString();
    for(EffectBinding::Binding& b : runtime_.mutableDocument()->bindings)
    {
        if(b.id == id)
        {
            if(editBindingDialog(&b))
            {
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
    const std::string id = item->data(Qt::UserRole).toString().toStdString();
    auto& bindings = runtime_.mutableDocument()->bindings;
    bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
                                  [&](const EffectBinding::Binding& b) { return b.id == id; }),
                   bindings.end());
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
    ui->holdButton->setChecked(false);
    runtime_.StopAll();
    timer_->stop();
    setStatus(QStringLiteral("Stopped"));
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
}
