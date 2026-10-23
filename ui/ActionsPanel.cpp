// SPDX-License-Identifier: GPL-2.0-only

#include "ActionsPanel.h"
#include "ActionEditorDialog.h"
#include "Actions/ActionLibrary.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginSettingsPaths.h"
#include "PluginUiUtils.h"
#include "ui_ActionsPanel.h"

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

bool IsActionFilename(const QString& name)
{
    return name.endsWith(QString::fromUtf8(Action::kFileSuffix), Qt::CaseInsensitive);
}
} // namespace

ActionsPanel::ActionsPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::ActionsPanel)
{
    ui->setupUi(this);
    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &ActionsPanel::onTick);
}

ActionsPanel::~ActionsPanel()
{
    stopPreview();
    if(editor_)
    {
        editor_->stopPreview();
        editor_->close();
    }
    delete ui;
}

void ActionsPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab || bound_)
    {
        return;
    }
    tab_ = tab;
    bound_ = true;

    PluginUiApplyMutedSecondaryLabel(ui->hintLabel);
    PluginUiApplyMutedSecondaryLabel(ui->statusLabel);

    connect(ui->refreshButton, &QPushButton::clicked, this, &ActionsPanel::onRefresh);
    connect(ui->previewButton, &QPushButton::clicked, this, &ActionsPanel::onPreview);
    connect(ui->stopButton, &QPushButton::clicked, this, &ActionsPanel::onStop);
    connect(ui->newButton, &QPushButton::clicked, this, &ActionsPanel::onNew);
    connect(ui->editButton, &QPushButton::clicked, this, &ActionsPanel::onEdit);
    connect(ui->importButton, &QPushButton::clicked, this, &ActionsPanel::onImport);
    connect(ui->exportButton, &QPushButton::clicked, this, &ActionsPanel::onExport);
    connect(ui->packList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onEdit();
    });

    populateList();
}

filesystem::path ActionsPanel::triggerEffectsDir() const
{
    if(!tab_ || !tab_->resource_manager)
    {
        return {};
    }
    return PluginSettingsPaths::TriggerEffectsDir(tab_->resource_manager);
}

QString ActionsPanel::pathFromItem(QListWidgetItem* item) const
{
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void ActionsPanel::selectPathInList(const QString& path)
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

void ActionsPanel::populateList()
{
    ui->packList->clear();
    const filesystem::path dir = triggerEffectsDir();
    if(dir.empty())
    {
        ui->statusLabel->setText(QStringLiteral("No plugin data path"));
        return;
    }

    Action::EnsureActionsDir(dir);
    const auto packs = Action::ListActions(dir);
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
        ui->statusLabel->setText(QStringLiteral("No actions"));
    }
    else if(!player_.IsPlaying())
    {
        ui->statusLabel->setText(QStringLiteral("%1 trigger action(s) in triggers/effects/").arg((int)packs.size()));
    }
}

void ActionsPanel::setPlayingUi(bool playing)
{
    ui->previewButton->setEnabled(!playing);
    ui->stopButton->setEnabled(playing);
    ui->refreshButton->setEnabled(!playing);
    ui->newButton->setEnabled(!playing);
    ui->editButton->setEnabled(!playing);
    ui->importButton->setEnabled(!playing);
    ui->exportButton->setEnabled(!playing);
}

void ActionsPanel::stopPreview()
{
    const bool was_playing = player_.IsPlaying();
    if(timer_ && timer_->isActive())
    {
        timer_->stop();
    }
    if(was_playing && tab_)
    {
        tab_->ApplyActionPreviewFrame(player_.GetPack(), player_.LocalMs(), true);
    }
    player_.Stop();
    setPlayingUi(false);
    if(ui)
    {
        ui->statusLabel->setText(QStringLiteral("Stopped"));
    }
}

ActionEditorDialog* ActionsPanel::ensureEditor()
{
    if(!tab_ || !tab_->resource_manager)
    {
        return nullptr;
    }
    if(!editor_)
    {
    editor_ = new ActionEditorDialog(tab_, tab_);
        connect(editor_, &ActionEditorDialog::packSaved, this, &ActionsPanel::onEditorSaved);
        connect(editor_, &ActionEditorDialog::previewStarted, this, &ActionsPanel::onEditorPreviewStarted);
    }
    return editor_;
}

void ActionsPanel::onRefresh()
{
    stopPreview();
    populateList();
}

void ActionsPanel::onExport()
{
    QListWidgetItem* item = ui->packList->currentItem();
    if(!item)
    {
        QMessageBox::information(this, QStringLiteral("Actions"),
                                 QStringLiteral("Select a pack to export."));
        return;
    }

    const filesystem::path src = QStringToPath(pathFromItem(item));
    const QString default_name = PathToQString(src.filename());
    const QString dest_q = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export Actions"),
        PathToQString(triggerEffectsDir() / src.filename()),
        QStringLiteral("Actions (*%1);;All Files (*)").arg(QString::fromUtf8(Action::kFileSuffix)));
    if(dest_q.isEmpty())
    {
        return;
    }

    QString dest_path = dest_q;
    if(!IsActionFilename(QFileInfo(dest_path).fileName()))
    {
        dest_path += QString::fromUtf8(Action::kFileSuffix);
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
        QMessageBox::warning(this, QStringLiteral("Actions"),
                             QStringLiteral("Failed to export:\n%1").arg(dest_path));
        return;
    }
    ui->statusLabel->setText(QStringLiteral("Exported %1").arg(QFileInfo(dest_path).fileName()));
}

void ActionsPanel::onImport()
{
    const filesystem::path dir = triggerEffectsDir();
    if(dir.empty())
    {
        return;
    }
    std::error_code ec;
    filesystem::create_directories(dir, ec);

    const QStringList sources = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Import Actions"),
        PathToQString(dir),
        QStringLiteral("Actions (*%1);;All Files (*)").arg(QString::fromUtf8(Action::kFileSuffix)));
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
        if(!src_info.exists() || !src_info.isFile() || !IsActionFilename(src_info.fileName()))
        {
            ++failed;
            continue;
        }

        Action::Pack pack;
        std::string err;
        if(!Action::LoadActionByPath(QStringToPath(src_q), &pack, &err))
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
                QStringLiteral("%1 is already in triggers/effects/.\n\nReplace it?").arg(src_info.fileName()),
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
        if(ActionEditorDialog* editor = ensureEditor())
        {
            editor->EditPack(QStringToPath(last_path));
        }
    }
}

void ActionsPanel::onNew()
{
    ActionEditorDialog* editor = ensureEditor();
    if(!editor)
    {
        return;
    }
    stopPreview();
    editor->NewPack(triggerEffectsDir());
}

void ActionsPanel::onEdit()
{
    QListWidgetItem* item = ui->packList->currentItem();
    if(!item)
    {
        QMessageBox::information(this, QStringLiteral("Actions"),
                                 QStringLiteral("Select a pack to edit, or click New."));
        return;
    }
    ActionEditorDialog* editor = ensureEditor();
    if(!editor)
    {
        return;
    }
    stopPreview();
    editor->EditPack(QStringToPath(pathFromItem(item)));
}

void ActionsPanel::onEditorSaved(const QString& path)
{
    populateList();
    selectPathInList(path);
}

void ActionsPanel::onEditorPreviewStarted()
{
    stopPreview();
    if(tab_)
    {
        tab_->PrepareActionPreview();
    }
}

void ActionsPanel::onPreview()
{
    if(!tab_ || !tab_->resource_manager)
    {
        return;
    }
    QListWidgetItem* item = ui->packList->currentItem();
    if(!item)
    {
        QMessageBox::information(this, QStringLiteral("Actions"),
                                 QStringLiteral("Select a pack to preview."));
        return;
    }

    if(editor_)
    {
        editor_->stopPreview();
    }

    const filesystem::path path = QStringToPath(pathFromItem(item));
    Action::Pack pack;
    std::string err;
    if(!Action::LoadActionByPath(path, &pack, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Actions"),
                             QStringLiteral("Failed to load pack:\n%1").arg(QString::fromStdString(err)));
        return;
    }

    tab_->PrepareActionPreview();

    const auto controllers = tab_->resource_manager->GetRGBControllers();
    if(controllers.empty())
    {
        QMessageBox::warning(this, QStringLiteral("Actions"),
                             QStringLiteral("No OpenRGB controllers available."));
        return;
    }

    player_.SetPack(pack);
    player_.Play();
    wall_.restart();
    last_elapsed_ms_ = 0;
    setPlayingUi(true);
    timer_->start();
    ui->statusLabel->setText(QStringLiteral("Playing â€œ%1â€â€¦").arg(QString::fromStdString(pack.name)));
}

void ActionsPanel::onStop()
{
    stopPreview();
}

void ActionsPanel::onTick()
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

    tab_->ApplyActionPreviewFrame(player_.GetPack(), player_.LocalMs());

    const int local = player_.LocalMs();
    const int dur = std::max(1, player_.GetPack().duration_ms);
    ui->statusLabel->setText(
        QStringLiteral("Playing %1 / %2 ms")
            .arg(local)
            .arg(dur));
}
