// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackPanel.h"
#include "EffectPacks/EffectPackApplier.h"
#include "EffectPacks/EffectPackLibrary.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginSettingsPaths.h"
#include "PluginUiUtils.h"
#include "ui_EffectPackPanel.h"

#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <system_error>

EffectPackPanel::EffectPackPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::EffectPackPanel)
{
    ui->setupUi(this);
    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &EffectPackPanel::onTick);
}

EffectPackPanel::~EffectPackPanel()
{
    stopPreview();
    delete ui;
}

void EffectPackPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    tab_ = tab;
    if(!tab_)
    {
        return;
    }

    PluginUiApplyMutedSecondaryLabel(ui->hintLabel);
    PluginUiApplyMutedSecondaryLabel(ui->statusLabel);

    connect(ui->refreshButton, &QPushButton::clicked, this, &EffectPackPanel::onRefresh);
    connect(ui->previewButton, &QPushButton::clicked, this, &EffectPackPanel::onPreview);
    connect(ui->stopButton, &QPushButton::clicked, this, &EffectPackPanel::onStop);
    connect(ui->seedExampleButton, &QPushButton::clicked, this, &EffectPackPanel::onSeedExample);
    connect(ui->packList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onPreview();
    });

    populateList();
}

filesystem::path EffectPackPanel::packsDir() const
{
    if(!tab_ || !tab_->resource_manager)
    {
        return {};
    }
    return PluginSettingsPaths::EffectPacksDir(tab_->resource_manager);
}

void EffectPackPanel::populateList()
{
    ui->packList->clear();
    const filesystem::path dir = packsDir();
    if(dir.empty())
    {
        ui->statusLabel->setText(QStringLiteral("No plugin data path"));
        return;
    }

    EffectPack::EnsureLibrarySeeded(dir);
    const auto packs = EffectPack::ListPacks(dir);
    for(const auto& entry : packs)
    {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1 (%2 ms, %3)")
                .arg(QString::fromStdString(entry.name))
                .arg(entry.duration_ms)
                .arg(QString::fromStdString(entry.loop)));
        item->setData(Qt::UserRole, QString::fromStdString(entry.path.string()));
        item->setData(Qt::UserRole + 1, QString::fromStdString(entry.id));
        ui->packList->addItem(item);
    }

    if(packs.empty())
    {
        ui->statusLabel->setText(QStringLiteral("No packs — create the rainbow example"));
    }
    else if(!player_.IsPlaying())
    {
        ui->statusLabel->setText(QStringLiteral("%1 pack(s) in effect-packs/").arg((int)packs.size()));
    }
}

void EffectPackPanel::setPlayingUi(bool playing)
{
    ui->previewButton->setEnabled(!playing);
    ui->stopButton->setEnabled(playing);
    ui->refreshButton->setEnabled(!playing);
    ui->seedExampleButton->setEnabled(!playing);
}

void EffectPackPanel::stopPreview()
{
    if(timer_ && timer_->isActive())
    {
        timer_->stop();
    }
    player_.Stop();
    setPlayingUi(false);
    if(ui)
    {
        ui->statusLabel->setText(QStringLiteral("Stopped"));
    }
}

void EffectPackPanel::onRefresh()
{
    stopPreview();
    populateList();
}

void EffectPackPanel::onSeedExample()
{
    const filesystem::path dir = packsDir();
    if(dir.empty())
    {
        return;
    }
    std::error_code ec;
    filesystem::create_directories(dir, ec);
    const EffectPack::Pack example = EffectPack::MakeExampleRainbowWash();
    const filesystem::path out = dir / (example.id + EffectPack::kFileSuffix);
    std::string err;
    if(!EffectPack::SaveToFile(out.string(), example, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Packs"),
                             QStringLiteral("Failed to write example:\n%1").arg(QString::fromStdString(err)));
        return;
    }
    populateList();
    ui->statusLabel->setText(QStringLiteral("Wrote %1").arg(QString::fromStdString(out.filename().string())));
}

void EffectPackPanel::onPreview()
{
    if(!tab_ || !tab_->resource_manager)
    {
        return;
    }
    QListWidgetItem* item = ui->packList->currentItem();
    if(!item)
    {
        QMessageBox::information(this, QStringLiteral("Effect Packs"),
                                 QStringLiteral("Select a pack to preview."));
        return;
    }

    const QString path = item->data(Qt::UserRole).toString();
    EffectPack::Pack pack;
    std::string err;
    if(!EffectPack::LoadPackByPath(path.toStdString(), &pack, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Packs"),
                             QStringLiteral("Failed to load pack:\n%1").arg(QString::fromStdString(err)));
        return;
    }

    // Preview owns the devices — pause the spatial effect stack.
    tab_->stopEffectClicked();

    const auto controllers = tab_->resource_manager->GetRGBControllers();
    if(controllers.empty())
    {
        QMessageBox::warning(this, QStringLiteral("Effect Packs"),
                             QStringLiteral("No OpenRGB controllers available."));
        return;
    }
    EffectPack::PrepareControllersForPreview(controllers);

    player_.SetPack(pack);
    player_.Play();
    wall_.restart();
    last_elapsed_ms_ = 0;
    setPlayingUi(true);
    timer_->start();
    ui->statusLabel->setText(QStringLiteral("Playing “%1”…").arg(QString::fromStdString(pack.name)));
}

void EffectPackPanel::onStop()
{
    stopPreview();
}

void EffectPackPanel::onTick()
{
    if(!tab_ || !tab_->resource_manager || !player_.IsPlaying())
    {
        stopPreview();
        return;
    }

    const int elapsed = (int)wall_.elapsed();
    const int dt = std::max(0, elapsed - last_elapsed_ms_);
    last_elapsed_ms_ = elapsed;

    if(!player_.Tick(dt, true))
    {
        stopPreview();
        ui->statusLabel->setText(QStringLiteral("Finished"));
        return;
    }

    const auto controllers = tab_->resource_manager->GetRGBControllers();
    EffectPack::ApplyPackFrame(player_.GetPack(), player_.LocalMs(), controllers);

    const int local = player_.LocalMs();
    const int dur = std::max(1, player_.GetPack().duration_ms);
    ui->statusLabel->setText(
        QStringLiteral("Playing %1 / %2 ms")
            .arg(local)
            .arg(dur));
}
