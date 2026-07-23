// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackEditorDialog.h"
#include "EffectPacks/EffectPackApplier.h"
#include "PluginUiUtils.h"
#include "ui_EffectPackEditorDialog.h"

#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>

#include <algorithm>
#include <cctype>
#include <system_error>

namespace
{

QColor RgbToQColor(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

RGBColor QColorToRgb(const QColor& c)
{
    return ToRGBColor(c.red(), c.green(), c.blue());
}

QString BlockSummary(const EffectPack::Block& b)
{
    const char* type = "solid";
    switch(b.type)
    {
        case EffectPack::BlockType::Solid: type = "solid"; break;
        case EffectPack::BlockType::Fade: type = "fade"; break;
        case EffectPack::BlockType::Pulse: type = "pulse"; break;
        default: break;
    }
    return QStringLiteral("%1  %2–%3 ms")
        .arg(QString::fromUtf8(type))
        .arg(b.start_ms)
        .arg(b.end_ms);
}

} // namespace

EffectPackEditorDialog::EffectPackEditorDialog(OpenRGBPluginAPIInterface* rm, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::EffectPackEditorDialog)
    , resource_manager(rm)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() | Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose, false);

    ui->loopCombo->addItem(QStringLiteral("Once"), QStringLiteral("once"));
    ui->loopCombo->addItem(QStringLiteral("Forever"), QStringLiteral("forever"));
    ui->loopCombo->addItem(QStringLiteral("While active"), QStringLiteral("while_active"));

    PluginUiApplyMutedSecondaryLabel(ui->statusLabel);

    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &EffectPackEditorDialog::onTick);

    connect(ui->addSolidButton, &QPushButton::clicked, this, &EffectPackEditorDialog::onAddSolid);
    connect(ui->addFadeButton, &QPushButton::clicked, this, &EffectPackEditorDialog::onAddFade);
    connect(ui->addPulseButton, &QPushButton::clicked, this, &EffectPackEditorDialog::onAddPulse);
    connect(ui->removeBlockButton, &QPushButton::clicked, this, &EffectPackEditorDialog::onRemoveBlock);
    connect(ui->blockList, &QListWidget::currentRowChanged, this, &EffectPackEditorDialog::onBlockSelected);
    connect(ui->startSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(ui->endSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(ui->periodSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(ui->intensitySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(ui->colorButton, &QPushButton::clicked, this, &EffectPackEditorDialog::onPickColor);
    connect(ui->colorToButton, &QPushButton::clicked, this, &EffectPackEditorDialog::onPickColorTo);
    connect(ui->saveButton, &QPushButton::clicked, this, &EffectPackEditorDialog::onSave);
    connect(ui->previewButton, &QPushButton::clicked, this, &EffectPackEditorDialog::onPreview);
    connect(ui->stopButton, &QPushButton::clicked, this, &EffectPackEditorDialog::stopPreview);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::close);

    setColorButton(ui->colorButton, ToRGBColor(255, 0, 0));
    setColorButton(ui->colorToButton, ToRGBColor(0, 128, 255));
}

EffectPackEditorDialog::~EffectPackEditorDialog()
{
    stopPreview();
    delete ui;
}

void EffectPackEditorDialog::NewPack(const filesystem::path& packs_dir)
{
    packs_dir_ = packs_dir;
    pack_path_.clear();
    pack_ = EffectPack::Pack();
    pack_.id = "new_pack";
    pack_.name = "New pack";
    pack_.duration_ms = 5000;
    pack_.loop = EffectPack::LoopMode::Once;
    pack_.priority = 10;
    EffectPack::Track track;
    track.name = "All LEDs";
    track.target.kind = EffectPack::TargetKind::All;
    EffectPack::Block block;
    block.type = EffectPack::BlockType::Solid;
    block.start_ms = 0;
    block.end_ms = pack_.duration_ms;
    block.color = ToRGBColor(255, 64, 0);
    block.intensity = 1.0f;
    track.blocks.push_back(block);
    pack_.tracks.push_back(std::move(track));
    loadIntoUi(pack_);
    setWindowTitle(QStringLiteral("Effect Pack Editor — New"));
    ui->statusLabel->setText(QStringLiteral("New pack — save to write into effect-packs/"));
    show();
    raise();
    activateWindow();
}

void EffectPackEditorDialog::EditPack(const filesystem::path& path)
{
    EffectPack::Pack pack;
    std::string err;
    if(!EffectPack::LoadFromFile(path, &pack, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Failed to load:\n%1").arg(QString::fromStdString(err)));
        return;
    }
    packs_dir_ = path.parent_path();
    pack_path_ = path;
    pack_ = std::move(pack);
    if(pack_.tracks.empty())
    {
        EffectPack::Track track;
        track.name = "All LEDs";
        track.target.kind = EffectPack::TargetKind::All;
        pack_.tracks.push_back(std::move(track));
    }
    loadIntoUi(pack_);
    setWindowTitle(QStringLiteral("Effect Pack Editor — %1").arg(QString::fromStdString(pack_.name)));
    ui->statusLabel->setText(QStringLiteral("Editing %1").arg(QString::fromStdString(path.filename().string())));
    show();
    raise();
    activateWindow();
}

void EffectPackEditorDialog::loadIntoUi(const EffectPack::Pack& pack)
{
    suppress_block_ui_ = true;
    ui->nameEdit->setText(QString::fromStdString(pack.name));
    ui->durationSpin->setValue(pack.duration_ms);
    const QString loop = (pack.loop == EffectPack::LoopMode::Forever) ? QStringLiteral("forever")
        : (pack.loop == EffectPack::LoopMode::WhileActive) ? QStringLiteral("while_active")
        : QStringLiteral("once");
    const int loop_idx = ui->loopCombo->findData(loop);
    ui->loopCombo->setCurrentIndex(loop_idx >= 0 ? loop_idx : 0);
    suppress_block_ui_ = false;
    refreshBlockList(pack.tracks.empty() || pack.tracks[0].blocks.empty() ? -1 : 0);
}

EffectPack::Pack EffectPackEditorDialog::packFromUi() const
{
    EffectPack::Pack pack = pack_;
    pack.name = ui->nameEdit->text().trimmed().toStdString();
    if(pack.name.empty())
    {
        pack.name = "Untitled";
    }
    pack.duration_ms = ui->durationSpin->value();
    const QString loop = ui->loopCombo->currentData().toString();
    if(loop == QStringLiteral("forever"))
    {
        pack.loop = EffectPack::LoopMode::Forever;
    }
    else if(loop == QStringLiteral("while_active"))
    {
        pack.loop = EffectPack::LoopMode::WhileActive;
    }
    else
    {
        pack.loop = EffectPack::LoopMode::Once;
    }
    if(pack.id.empty() || pack_path_.empty())
    {
        pack.id = sanitizeId(QString::fromStdString(pack.name)).toStdString();
    }
    if(pack.tracks.empty())
    {
        EffectPack::Track track;
        track.name = "All LEDs";
        track.target.kind = EffectPack::TargetKind::All;
        pack.tracks.push_back(track);
    }
    pack.tracks[0].name = "All LEDs";
    pack.tracks[0].target.kind = EffectPack::TargetKind::All;
    return pack;
}

void EffectPackEditorDialog::refreshBlockList(int select_row)
{
    suppress_block_ui_ = true;
    ui->blockList->clear();
    if(!pack_.tracks.empty())
    {
        for(const auto& block : pack_.tracks[0].blocks)
        {
            ui->blockList->addItem(BlockSummary(block));
        }
    }
    if(select_row < 0 && ui->blockList->count() > 0)
    {
        select_row = 0;
    }
    if(select_row >= 0 && select_row < ui->blockList->count())
    {
        ui->blockList->setCurrentRow(select_row);
    }
    suppress_block_ui_ = false;
    applyBlockToForm();
}

void EffectPackEditorDialog::applyBlockToForm()
{
    suppress_block_ui_ = true;
    const int row = ui->blockList->currentRow();
    const bool ok = !pack_.tracks.empty() && row >= 0
        && row < (int)pack_.tracks[0].blocks.size();
    ui->blockEditGroup->setEnabled(ok);
    if(!ok)
    {
        suppress_block_ui_ = false;
        return;
    }
    const EffectPack::Block& b = pack_.tracks[0].blocks[(size_t)row];
    ui->startSpin->setValue(b.start_ms);
    ui->endSpin->setValue(b.end_ms);
    ui->periodSpin->setValue(std::max(50, b.period_ms));
    ui->intensitySpin->setValue((int)std::lround(std::clamp(b.intensity, 0.0f, 1.0f) * 100.0f));
    setColorButton(ui->colorButton, b.type == EffectPack::BlockType::Fade ? b.color_from : b.color);
    setColorButton(ui->colorToButton, b.color_to);
    const bool fade = b.type == EffectPack::BlockType::Fade;
    const bool pulse = b.type == EffectPack::BlockType::Pulse;
    ui->colorToLabel->setEnabled(fade);
    ui->colorToButton->setEnabled(fade);
    ui->periodLabel->setEnabled(pulse);
    ui->periodSpin->setEnabled(pulse);
    suppress_block_ui_ = false;
}

void EffectPackEditorDialog::applyFormToSelectedBlock()
{
    if(suppress_block_ui_ || pack_.tracks.empty())
    {
        return;
    }
    const int row = ui->blockList->currentRow();
    if(row < 0 || row >= (int)pack_.tracks[0].blocks.size())
    {
        return;
    }
    EffectPack::Block& b = pack_.tracks[0].blocks[(size_t)row];
    b.start_ms = ui->startSpin->value();
    b.end_ms = std::max(b.start_ms + 1, ui->endSpin->value());
    b.period_ms = ui->periodSpin->value();
    b.intensity = ui->intensitySpin->value() / 100.0f;
    const RGBColor c = colorFromButton(ui->colorButton);
    const RGBColor c2 = colorFromButton(ui->colorToButton);
    if(b.type == EffectPack::BlockType::Fade)
    {
        b.color_from = c;
        b.color_to = c2;
        b.color = c;
    }
    else
    {
        b.color = c;
        b.color_from = c;
        b.color_to = c2;
    }
    suppress_block_ui_ = true;
    ui->blockList->item(row)->setText(BlockSummary(b));
    suppress_block_ui_ = false;
}

void EffectPackEditorDialog::setColorButton(QPushButton* button, RGBColor color)
{
    if(!button)
    {
        return;
    }
    const QColor qc = RgbToQColor(color);
    button->setProperty("rgbColor", (uint)color);
    button->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2;")
            .arg(qc.name())
            .arg(qc.lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black")));
    button->setText(qc.name().toUpper());
}

RGBColor EffectPackEditorDialog::colorFromButton(QPushButton* button) const
{
    if(!button)
    {
        return ToRGBColor(255, 0, 0);
    }
    return (RGBColor)button->property("rgbColor").toUInt();
}

QString EffectPackEditorDialog::sanitizeId(const QString& name) const
{
    QString out;
    out.reserve(name.size());
    for(QChar ch : name.toLower())
    {
        if(ch.isLetterOrNumber())
        {
            out.append(ch);
        }
        else if(ch.isSpace() || ch == '-' || ch == '_')
        {
            if(!out.isEmpty() && out.back() != '_')
            {
                out.append('_');
            }
        }
    }
    while(out.endsWith('_'))
    {
        out.chop(1);
    }
    if(out.isEmpty())
    {
        out = QStringLiteral("pack");
    }
    return out;
}

void EffectPackEditorDialog::onAddSolid()
{
    if(pack_.tracks.empty())
    {
        EffectPack::Track track;
        track.name = "All LEDs";
        track.target.kind = EffectPack::TargetKind::All;
        pack_.tracks.push_back(track);
    }
    applyFormToSelectedBlock();
    EffectPack::Block block;
    block.type = EffectPack::BlockType::Solid;
    const int start = pack_.tracks[0].blocks.empty() ? 0 : pack_.tracks[0].blocks.back().end_ms;
    block.start_ms = start;
    block.end_ms = std::min(EffectPack::kMaxDurationMs, start + 1000);
    block.color = ToRGBColor(255, 0, 0);
    block.intensity = 1.0f;
    pack_.tracks[0].blocks.push_back(block);
    refreshBlockList((int)pack_.tracks[0].blocks.size() - 1);
}

void EffectPackEditorDialog::onAddFade()
{
    if(pack_.tracks.empty())
    {
        onAddSolid();
    }
    applyFormToSelectedBlock();
    EffectPack::Block block;
    block.type = EffectPack::BlockType::Fade;
    const int start = pack_.tracks[0].blocks.empty() ? 0 : pack_.tracks[0].blocks.back().end_ms;
    block.start_ms = start;
    block.end_ms = std::min(EffectPack::kMaxDurationMs, start + 2000);
    block.color_from = ToRGBColor(255, 0, 0);
    block.color_to = ToRGBColor(0, 128, 255);
    block.color = block.color_from;
    block.intensity = 1.0f;
    pack_.tracks[0].blocks.push_back(block);
    refreshBlockList((int)pack_.tracks[0].blocks.size() - 1);
}

void EffectPackEditorDialog::onAddPulse()
{
    if(pack_.tracks.empty())
    {
        onAddSolid();
    }
    applyFormToSelectedBlock();
    EffectPack::Block block;
    block.type = EffectPack::BlockType::Pulse;
    const int start = pack_.tracks[0].blocks.empty() ? 0 : pack_.tracks[0].blocks.back().end_ms;
    block.start_ms = start;
    block.end_ms = std::min(EffectPack::kMaxDurationMs, start + 3000);
    block.color = ToRGBColor(0, 255, 128);
    block.period_ms = 800;
    block.min_intensity = 0.15f;
    block.max_intensity = 1.0f;
    block.intensity = 1.0f;
    pack_.tracks[0].blocks.push_back(block);
    refreshBlockList((int)pack_.tracks[0].blocks.size() - 1);
}

void EffectPackEditorDialog::onRemoveBlock()
{
    if(pack_.tracks.empty())
    {
        return;
    }
    const int row = ui->blockList->currentRow();
    if(row < 0 || row >= (int)pack_.tracks[0].blocks.size())
    {
        return;
    }
    pack_.tracks[0].blocks.erase(pack_.tracks[0].blocks.begin() + row);
    refreshBlockList(std::min(row, (int)pack_.tracks[0].blocks.size() - 1));
}

void EffectPackEditorDialog::onBlockSelected()
{
    if(suppress_block_ui_)
    {
        return;
    }
    applyBlockToForm();
}

void EffectPackEditorDialog::onBlockFieldChanged()
{
    applyFormToSelectedBlock();
}

void EffectPackEditorDialog::onPickColor()
{
    const QColor current = RgbToQColor(colorFromButton(ui->colorButton));
    const QColor picked = QColorDialog::getColor(current, this, QStringLiteral("Block color"));
    if(!picked.isValid())
    {
        return;
    }
    setColorButton(ui->colorButton, QColorToRgb(picked));
    applyFormToSelectedBlock();
}

void EffectPackEditorDialog::onPickColorTo()
{
    const QColor current = RgbToQColor(colorFromButton(ui->colorToButton));
    const QColor picked = QColorDialog::getColor(current, this, QStringLiteral("Fade end color"));
    if(!picked.isValid())
    {
        return;
    }
    setColorButton(ui->colorToButton, QColorToRgb(picked));
    applyFormToSelectedBlock();
}

void EffectPackEditorDialog::onSave()
{
    stopPreview();
    applyFormToSelectedBlock();
    pack_ = packFromUi();
    if(pack_.tracks.empty() || pack_.tracks[0].blocks.empty())
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Add at least one timeline block before saving."));
        return;
    }
    if(packs_dir_.empty())
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("No effect-packs folder available."));
        return;
    }
    std::error_code ec;
    filesystem::create_directories(packs_dir_, ec);
    if(pack_path_.empty())
    {
        pack_path_ = packs_dir_ / (pack_.id + EffectPack::kFileSuffix);
    }
    std::string err;
    if(!EffectPack::SaveToFile(pack_path_, pack_, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Save failed:\n%1").arg(QString::fromStdString(err)));
        return;
    }
    setWindowTitle(QStringLiteral("Effect Pack Editor — %1").arg(QString::fromStdString(pack_.name)));
    ui->statusLabel->setText(QStringLiteral("Saved %1").arg(QString::fromStdString(pack_path_.filename().string())));
    emit packSaved(QString::fromStdString(pack_path_.string()));
}

void EffectPackEditorDialog::setPlayingUi(bool playing)
{
    ui->previewButton->setEnabled(!playing);
    ui->stopButton->setEnabled(playing);
    ui->saveButton->setEnabled(!playing);
}

void EffectPackEditorDialog::stopPreview()
{
    if(timer_ && timer_->isActive())
    {
        timer_->stop();
    }
    player_.Stop();
    setPlayingUi(false);
    emit previewStopped();
    if(ui)
    {
        ui->statusLabel->setText(QStringLiteral("Preview stopped"));
    }
}

void EffectPackEditorDialog::onPreview()
{
    if(!resource_manager)
    {
        return;
    }
    applyFormToSelectedBlock();
    pack_ = packFromUi();
    if(pack_.tracks.empty() || pack_.tracks[0].blocks.empty())
    {
        QMessageBox::information(this, QStringLiteral("Effect Pack Editor"),
                                 QStringLiteral("Add a block before previewing."));
        return;
    }

    emit previewStarted();

    const auto controllers = resource_manager->GetRGBControllers();
    if(controllers.empty())
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("No OpenRGB controllers available."));
        return;
    }
    EffectPack::PrepareControllersForPreview(controllers);
    player_.SetPack(pack_);
    player_.Play();
    wall_.restart();
    last_elapsed_ms_ = 0;
    setPlayingUi(true);
    timer_->start();
    ui->statusLabel->setText(QStringLiteral("Previewing…"));
}

void EffectPackEditorDialog::onTick()
{
    if(!resource_manager || !player_.IsPlaying())
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
        ui->statusLabel->setText(QStringLiteral("Preview finished"));
        return;
    }
    EffectPack::ApplyPackFrame(player_.GetPack(), player_.LocalMs(), resource_manager->GetRGBControllers());
    ui->statusLabel->setText(
        QStringLiteral("Preview %1 / %2 ms")
            .arg(player_.LocalMs())
            .arg(std::max(1, player_.GetPack().duration_ms)));
}
