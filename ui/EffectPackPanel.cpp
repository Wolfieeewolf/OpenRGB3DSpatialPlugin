// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackPanel.h"
#include "EffectPackEditorDialog.h"
#include "EffectPacks/EffectPackLibrary.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginSettingsPaths.h"
#include "PluginUiUtils.h"
#include "ui_EffectPackPanel.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <system_error>

namespace
{
QString PathToQString(const filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

filesystem::path QStringToPath(const QString& s)
{
#ifdef _WIN32
    return filesystem::path(s.toStdWString());
#else
    return filesystem::path(s.toStdString());
#endif
}

bool IsEffectPackFilename(const QString& name)
{
    return name.endsWith(QString::fromUtf8(EffectPack::kFileSuffix), Qt::CaseInsensitive);
}
} // namespace

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
    if(editor_)
    {
        editor_->stopPreview();
        editor_->close();
    }
    delete ui;
}

void EffectPackPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab || bound_)
    {
        return;
    }
    tab_ = tab;
    bound_ = true;

    PluginUiApplyMutedSecondaryLabel(ui->hintLabel);
    PluginUiApplyMutedSecondaryLabel(ui->statusLabel);

    connect(ui->refreshButton, &QPushButton::clicked, this, &EffectPackPanel::onRefresh);
    connect(ui->previewButton, &QPushButton::clicked, this, &EffectPackPanel::onPreview);
    connect(ui->stopButton, &QPushButton::clicked, this, &EffectPackPanel::onStop);
    connect(ui->newButton, &QPushButton::clicked, this, &EffectPackPanel::onNew);
    connect(ui->editButton, &QPushButton::clicked, this, &EffectPackPanel::onEdit);
    connect(ui->importButton, &QPushButton::clicked, this, &EffectPackPanel::onImport);
    connect(ui->exportButton, &QPushButton::clicked, this, &EffectPackPanel::onExport);
    connect(ui->seedExampleButton, &QPushButton::clicked, this, &EffectPackPanel::onSeedExample);
    connect(ui->packList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onEdit();
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

QString EffectPackPanel::pathFromItem(QListWidgetItem* item) const
{
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void EffectPackPanel::selectPathInList(const QString& path)
{
    for(int i = 0; i < ui->packList->count(); ++i)
    {
        if(pathFromItem(ui->packList->item(i)) == path)
        {
            ui->packList->setCurrentRow(i);
            return;
        }
    }
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
        item->setData(Qt::UserRole, PathToQString(entry.path));
        item->setData(Qt::UserRole + 1, QString::fromStdString(entry.id));
        ui->packList->addItem(item);
    }

    if(packs.empty())
    {
        ui->statusLabel->setText(QStringLiteral("No packs found in effect-packs/"));
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
    ui->newButton->setEnabled(!playing);
    ui->editButton->setEnabled(!playing);
    ui->importButton->setEnabled(!playing);
    ui->exportButton->setEnabled(!playing);
}

void EffectPackPanel::stopPreview()
{
    const bool was_playing = player_.IsPlaying();
    if(timer_ && timer_->isActive())
    {
        timer_->stop();
    }
    if(was_playing && tab_)
    {
        tab_->ApplyEffectPackPreviewFrame(player_.GetPack(), player_.LocalMs(), true);
    }
    player_.Stop();
    setPlayingUi(false);
    if(ui)
    {
        ui->statusLabel->setText(QStringLiteral("Stopped"));
    }
}

EffectPackEditorDialog* EffectPackPanel::ensureEditor()
{
    if(!tab_ || !tab_->resource_manager)
    {
        return nullptr;
    }
    if(!editor_)
    {
    editor_ = new EffectPackEditorDialog(tab_, tab_);
        connect(editor_, &EffectPackEditorDialog::packSaved, this, &EffectPackPanel::onEditorSaved);
        connect(editor_, &EffectPackEditorDialog::previewStarted, this, &EffectPackPanel::onEditorPreviewStarted);
    }
    return editor_;
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
    if(!EffectPack::SaveToFile(out, example, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Packs"),
                             QStringLiteral("Failed to write example:\n%1").arg(QString::fromStdString(err)));
        return;
    }
    populateList();
    selectPathInList(PathToQString(out));
    ui->statusLabel->setText(QStringLiteral("Wrote %1").arg(PathToQString(out.filename())));
}

void EffectPackPanel::onExport()
{
    QListWidgetItem* item = ui->packList->currentItem();
    if(!item)
    {
        QMessageBox::information(this, QStringLiteral("Effect Packs"),
                                 QStringLiteral("Select a pack to export."));
        return;
    }

    const filesystem::path src = QStringToPath(pathFromItem(item));
    const QString default_name = PathToQString(src.filename());
    const QString dest_q = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export effect pack"),
        PathToQString(packsDir() / src.filename()),
        QStringLiteral("Effect pack (*%1);;All Files (*)").arg(QString::fromUtf8(EffectPack::kFileSuffix)));
    if(dest_q.isEmpty())
    {
        return;
    }

    QString dest_path = dest_q;
    if(!IsEffectPackFilename(QFileInfo(dest_path).fileName()))
    {
        dest_path += QString::fromUtf8(EffectPack::kFileSuffix);
    }

    const QString src_q = PathToQString(src);
    if(QFileInfo(src_q).canonicalFilePath().compare(QFileInfo(dest_path).canonicalFilePath(), Qt::CaseInsensitive) == 0)
    {
        ui->statusLabel->setText(QStringLiteral("Already at %1").arg(default_name));
        return;
    }

    if(QFile::exists(dest_path))
    {
        QFile::remove(dest_path);
    }
    if(!QFile::copy(src_q, dest_path))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Packs"),
                             QStringLiteral("Failed to export:\n%1").arg(dest_path));
        return;
    }
    ui->statusLabel->setText(QStringLiteral("Exported %1").arg(QFileInfo(dest_path).fileName()));
}

void EffectPackPanel::onImport()
{
    const filesystem::path dir = packsDir();
    if(dir.empty())
    {
        return;
    }
    std::error_code ec;
    filesystem::create_directories(dir, ec);

    const QStringList sources = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Import effect packs"),
        PathToQString(dir),
        QStringLiteral("Effect pack (*%1);;All Files (*)").arg(QString::fromUtf8(EffectPack::kFileSuffix)));
    if(sources.isEmpty())
    {
        return;
    }

    int copied = 0;
    int failed = 0;
    QString last_path;
    for(const QString& src_q : sources)
    {
        const QFileInfo src_info(src_q);
        if(!src_info.exists() || !src_info.isFile() || !IsEffectPackFilename(src_info.fileName()))
        {
            ++failed;
            continue;
        }

        EffectPack::Pack pack;
        std::string err;
        if(!EffectPack::LoadPackByPath(QStringToPath(src_q), &pack, &err))
        {
            ++failed;
            continue;
        }

        const filesystem::path dest = dir / QStringToPath(src_info.fileName());
        const QString dest_q = PathToQString(dest);
        if(src_info.canonicalFilePath().compare(QFileInfo(dest_q).canonicalFilePath(), Qt::CaseInsensitive) == 0)
        {
            last_path = dest_q;
            ++copied;
            continue;
        }

        if(QFile::exists(dest_q))
        {
            const auto reply = QMessageBox::question(
                this,
                QStringLiteral("Replace pack?"),
                QStringLiteral("%1 is already in effect-packs.\n\nReplace it?").arg(src_info.fileName()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if(reply != QMessageBox::Yes)
            {
                continue;
            }
            QFile::remove(dest_q);
        }

        if(!QFile::copy(src_q, dest_q))
        {
            ++failed;
            continue;
        }
        last_path = dest_q;
        ++copied;
    }

    populateList();
    if(!last_path.isEmpty())
    {
        selectPathInList(last_path);
    }
    ui->statusLabel->setText(QStringLiteral("Imported %1 pack(s).").arg(copied)
                              + (failed > 0 ? QStringLiteral(" %1 failed.").arg(failed) : QString()));

    if(copied == 1 && !last_path.isEmpty())
    {
        stopPreview();
        if(EffectPackEditorDialog* editor = ensureEditor())
        {
            editor->EditPack(QStringToPath(last_path));
        }
    }
}

void EffectPackPanel::onNew()
{
    EffectPackEditorDialog* editor = ensureEditor();
    if(!editor)
    {
        return;
    }
    stopPreview();
    editor->NewPack(packsDir());
}

void EffectPackPanel::onEdit()
{
    QListWidgetItem* item = ui->packList->currentItem();
    if(!item)
    {
        QMessageBox::information(this, QStringLiteral("Effect Packs"),
                                 QStringLiteral("Select a pack to edit, or click New."));
        return;
    }
    EffectPackEditorDialog* editor = ensureEditor();
    if(!editor)
    {
        return;
    }
    stopPreview();
    editor->EditPack(QStringToPath(pathFromItem(item)));
}

void EffectPackPanel::onEditorSaved(const QString& path)
{
    populateList();
    selectPathInList(path);
}

void EffectPackPanel::onEditorPreviewStarted()
{
    stopPreview();
    if(tab_)
    {
        tab_->PrepareEffectPackPreview();
    }
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

    if(editor_)
    {
        editor_->stopPreview();
    }

    const filesystem::path path = QStringToPath(pathFromItem(item));
    EffectPack::Pack pack;
    std::string err;
    if(!EffectPack::LoadPackByPath(path, &pack, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Packs"),
                             QStringLiteral("Failed to load pack:\n%1").arg(QString::fromStdString(err)));
        return;
    }

    tab_->PrepareEffectPackPreview();

    const auto controllers = tab_->resource_manager->GetRGBControllers();
    if(controllers.empty())
    {
        QMessageBox::warning(this, QStringLiteral("Effect Packs"),
                             QStringLiteral("No OpenRGB controllers available."));
        return;
    }

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

    tab_->ApplyEffectPackPreviewFrame(player_.GetPack(), player_.LocalMs());

    const int local = player_.LocalMs();
    const int dur = std::max(1, player_.GetPack().duration_ms);
    ui->statusLabel->setText(
        QStringLiteral("Playing %1 / %2 ms")
            .arg(local)
            .arg(dur));
}
