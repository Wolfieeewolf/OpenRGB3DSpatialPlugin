// SPDX-License-Identifier: GPL-2.0-only

#include "ScenePropDialog.h"

#include "PluginSettingsPaths.h"
#include "ScenePropPreviewWidget.h"
#include "viewport/MeshImport.h"
#include "viewport/ScenePropMeshCache.h"
#include "viewport/ScenePropMeshPaths.h"

#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QDir>

#include <atomic>
#include <algorithm>
#include <filesystem>
#include <thread>

namespace
{
QDoubleSpinBox* MakeMmSpin(QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(1.0, 20000.0);
    spin->setDecimals(0);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setSingleStep(10.0);
    return spin;
}

const char* FaceLabel(ScenePropFace face)
{
    switch(face)
    {
        case ScenePropFace::Front: return "Front";
        case ScenePropFace::Back: return "Back";
        case ScenePropFace::Left: return "Left";
        case ScenePropFace::Right: return "Right";
        case ScenePropFace::Bottom: return "Bottom";
        case ScenePropFace::Top: return "Top";
        case ScenePropFace::Count:
        default: return "Face";
    }
}

QString UniqueMeshFileName(const filesystem::path& dir, const QString& base_name)
{
    QString candidate = base_name;
    int n = 1;
    while(filesystem::exists(dir / candidate.toStdString()))
    {
        const QFileInfo info(base_name);
        candidate = QStringLiteral("%1_%2.%3")
                        .arg(info.completeBaseName())
                        .arg(n++)
                        .arg(info.suffix());
    }
    return candidate;
}
} // namespace

ScenePropDialog::ScenePropDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Scene Prop"));
    setMinimumSize(560, 400);
    resize(640, 460);

    preview_ = new ScenePropPreviewWidget(this);
    preview_->setFixedSize(220, 180);

    auto* preview_frame = new QFrame(this);
    preview_frame->setFrameShape(QFrame::StyledPanel);
    preview_frame->setFrameShadow(QFrame::Sunken);
    preview_frame->setFixedSize(224, 184);
    auto* preview_frame_layout = new QVBoxLayout(preview_frame);
    preview_frame_layout->setContentsMargins(2, 2, 2, 2);
    preview_frame_layout->addWidget(preview_);

    auto* preview_hint = new QLabel(tr("Drag to orbit · scroll to zoom"), this);
    preview_hint->setAlignment(Qt::AlignCenter);
    preview_hint->setStyleSheet(QStringLiteral("color: palette(mid);"));

    status_label_ = new QLabel(this);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QStringLiteral("color: palette(mid);"));

    auto* preview_column = new QVBoxLayout();
    preview_column->setSpacing(4);
    preview_column->addWidget(preview_frame, 0, Qt::AlignTop | Qt::AlignHCenter);
    preview_column->addWidget(preview_hint);
    preview_column->addWidget(status_label_);
    preview_column->addStretch(1);

    shape_combo_ = new QComboBox(this);
    shape_combo_->addItem(tr("Box"), (int)ScenePropShape::Box);
    shape_combo_->addItem(tr("Mesh"), (int)ScenePropShape::Mesh);

    name_edit_ = new QLineEdit(this);
    width_spin_ = MakeMmSpin(this);
    height_spin_ = MakeMmSpin(this);
    depth_spin_ = MakeMmSpin(this);
    color_button_ = new QPushButton(tr("Pick…"), this);
    color_button_->setMinimumHeight(28);
    glass_color_button_ = new QPushButton(tr("Pick…"), this);
    glass_color_button_->setMinimumHeight(28);
    glass_color_label_ = new QLabel(tr("Glass colour"), this);

    mesh_body_combo_ = makeFaceCombo();
    mesh_path_label_ = new QLabel(tr("No file imported"), this);
    mesh_path_label_->setWordWrap(true);
    import_mesh_button_ = new QPushButton(tr("Import mesh…"), this);
    mesh_body_label_ = new QLabel(tr("Body mode"), this);

    mesh_panel_ = new QWidget(this);
    auto* mesh_layout = new QVBoxLayout(mesh_panel_);
    mesh_layout->setContentsMargins(0, 0, 0, 0);
    mesh_layout->addWidget(mesh_path_label_);
    mesh_layout->addWidget(import_mesh_button_);
    auto* mesh_body_row = new QHBoxLayout();
    mesh_body_row->addWidget(mesh_body_label_);
    mesh_body_row->addWidget(mesh_body_combo_, 1);
    mesh_layout->addLayout(mesh_body_row);

    auto* form = new QFormLayout();
    form->addRow(tr("Shape"), shape_combo_);
    form->addRow(tr("Name"), name_edit_);
    form->addRow(tr("Width (X)"), width_spin_);
    form->addRow(tr("Height (Y)"), height_spin_);
    form->addRow(tr("Depth (Z)"), depth_spin_);
    form->addRow(tr("Solid colour"), color_button_);
    form->addRow(glass_color_label_, glass_color_button_);
    form->addRow(tr("Mesh"), mesh_panel_);

    faces_box_ = new QGroupBox(tr("Faces"), this);
    auto* faces_form = new QFormLayout(faces_box_);
    set_all_combo_ = makeFaceCombo();
    auto* set_all_btn = new QPushButton(tr("Apply to all"), faces_box_);
    auto* set_all_row = new QWidget(faces_box_);
    auto* set_all_layout = new QHBoxLayout(set_all_row);
    set_all_layout->setContentsMargins(0, 0, 0, 0);
    set_all_layout->addWidget(set_all_combo_, 1);
    set_all_layout->addWidget(set_all_btn);
    faces_form->addRow(tr("Set all"), set_all_row);
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        face_combos_[i] = makeFaceCombo();
        faces_form->addRow(tr(FaceLabel((ScenePropFace)i)), face_combos_[i]);
    }

    auto* controls = new QWidget(this);
    auto* controls_layout = new QVBoxLayout(controls);
    controls_layout->setContentsMargins(0, 0, 0, 0);
    controls_layout->addLayout(form);
    controls_layout->addWidget(faces_box_);
    controls_layout->addStretch(1);
    controls->setMinimumWidth(300);

    auto* body = new QHBoxLayout();
    body->setSpacing(12);
    body->addLayout(preview_column, 0);
    body->addWidget(controls, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(color_button_, &QPushButton::clicked, this, &ScenePropDialog::onPickSolidColor);
    connect(glass_color_button_, &QPushButton::clicked, this, &ScenePropDialog::onPickGlassColor);
    connect(set_all_btn, &QPushButton::clicked, this, &ScenePropDialog::onSetAllFaces);
    connect(shape_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScenePropDialog::onShapeChanged);
    connect(import_mesh_button_, &QPushButton::clicked, this, &ScenePropDialog::onImportMesh);

    auto refresh = [this](int) { refreshPreview(); };
    auto refresh_d = [this](double) { refreshPreview(); };
    connect(width_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, refresh_d);
    connect(height_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, refresh_d);
    connect(depth_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, refresh_d);
    connect(mesh_body_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, refresh);
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        connect(face_combos_[i], QOverload<int>::of(&QComboBox::currentIndexChanged), this, refresh);
    }

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(body, 1);
    layout->addWidget(buttons);

    syncColorButton(color_button_, color_rgb_);
    syncColorButton(glass_color_button_, glass_color_rgb_);
    updateShapeUi();
    refreshPreview();
}

QComboBox* ScenePropDialog::makeFaceCombo()
{
    auto* combo = new QComboBox(this);
    combo->addItem(tr("Solid"), (int)ScenePropFaceKind::Solid);
    combo->addItem(tr("Glass"), (int)ScenePropFaceKind::Glass);
    combo->addItem(tr("Blocker"), (int)ScenePropFaceKind::Blocker);
    return combo;
}

void ScenePropDialog::collectFaceKinds(ScenePropFaceKind out[SceneProp3D::kFaceCount]) const
{
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        out[i] = (ScenePropFaceKind)face_combos_[i]->currentData().toInt();
    }
}

void ScenePropDialog::setStatus(const QString& text)
{
    if(status_label_)
    {
        status_label_->setText(text);
    }
}

void ScenePropDialog::refreshPreview()
{
    if(!preview_)
    {
        return;
    }

    if(shape() == ScenePropShape::Mesh)
    {
        QString abs_path;
        if(!mesh_asset_relative_.isEmpty())
        {
            if(resource_manager_)
            {
                PluginSettingsPaths::EnsurePluginDataLayout(resource_manager_);
                ScenePropMeshPaths::SetMeshesRoot(
                    PluginSettingsPaths::MeshesDir(resource_manager_));
            }
            abs_path = QString::fromStdString(
                ScenePropMeshPaths::Resolve(mesh_asset_relative_.toStdString()).string());
        }
        preview_->setMeshPreview(widthMm(), heightMm(), depthMm(),
                                 colorRgb(), glassColorRgb(),
                                 meshBodyKind(), abs_path);
        return;
    }

    ScenePropFaceKind faces[SceneProp3D::kFaceCount];
    collectFaceKinds(faces);
    preview_->setBoxPreview(widthMm(), heightMm(), depthMm(),
                            colorRgb(), glassColorRgb(), faces);
}

void ScenePropDialog::setCreateMode()
{
    setWindowTitle(tr("Create Scene Prop"));
}

void ScenePropDialog::setEditMode()
{
    setWindowTitle(tr("Edit Scene Prop"));
}

void ScenePropDialog::setCreateDefaults(const QString& suggested_name,
                                        float width_mm,
                                        float height_mm,
                                        float depth_mm,
                                        unsigned int color_rgb)
{
    shape_combo_->setCurrentIndex(0);
    name_edit_->setText(suggested_name);
    width_spin_->setValue(width_mm);
    height_spin_->setValue(height_mm);
    depth_spin_->setValue(depth_mm);
    color_rgb_ = color_rgb & 0x00FFFFFFu;
    glass_color_rgb_ = 0x4AA8C8u;
    mesh_asset_relative_.clear();
    mesh_path_label_->setText(tr("No file imported"));
    mesh_body_combo_->setCurrentIndex(0);
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        face_combos_[i]->setCurrentIndex(0);
    }
    set_all_combo_->setCurrentIndex(0);
    syncColorButton(color_button_, color_rgb_);
    syncColorButton(glass_color_button_, glass_color_rgb_);
    setStatus(QString());
    updateShapeUi();
    refreshPreview();
}

void ScenePropDialog::loadFrom(const SceneProp3D& prop)
{
    const int shape_idx = shape_combo_->findData((int)prop.GetShape());
    shape_combo_->setCurrentIndex(shape_idx >= 0 ? shape_idx : 0);
    name_edit_->setText(QString::fromStdString(prop.GetName()));
    width_spin_->setValue(prop.GetWidthMM());
    height_spin_->setValue(prop.GetHeightMM());
    depth_spin_->setValue(prop.GetDepthMM());
    color_rgb_ = prop.GetColor();
    glass_color_rgb_ = prop.GetGlassColor();
    mesh_asset_relative_ = QString::fromStdString(prop.GetMeshAsset());
    mesh_path_label_->setText(mesh_asset_relative_.isEmpty()
                                   ? tr("No file imported")
                                   : mesh_asset_relative_);
    const int body_idx = mesh_body_combo_->findData((int)prop.GetMeshBodyKind());
    mesh_body_combo_->setCurrentIndex(body_idx >= 0 ? body_idx : 0);
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        const int idx = face_combos_[i]->findData((int)prop.GetFaceKind((ScenePropFace)i));
        face_combos_[i]->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    syncColorButton(color_button_, color_rgb_);
    syncColorButton(glass_color_button_, glass_color_rgb_);
    setStatus(QString());
    updateShapeUi();
    refreshPreview();
}

void ScenePropDialog::applyTo(SceneProp3D* prop) const
{
    if(!prop)
    {
        return;
    }
    const QString n = name();
    if(!n.isEmpty())
    {
        prop->SetName(n.toStdString());
    }
    prop->SetShape(shape());
    prop->SetWidthMM(widthMm());
    prop->SetHeightMM(heightMm());
    prop->SetDepthMM(depthMm());
    prop->SetColor(colorRgb());
    prop->SetGlassColor(glassColorRgb());
    prop->SetMeshAsset(meshAssetRelative().toStdString());
    prop->SetMeshBodyKind(meshBodyKind());
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        prop->SetFaceKind((ScenePropFace)i,
                          (ScenePropFaceKind)face_combos_[i]->currentData().toInt());
    }
}

QString ScenePropDialog::name() const
{
    return name_edit_->text().trimmed();
}

float ScenePropDialog::widthMm() const
{
    return (float)width_spin_->value();
}

float ScenePropDialog::heightMm() const
{
    return (float)height_spin_->value();
}

float ScenePropDialog::depthMm() const
{
    return (float)depth_spin_->value();
}

unsigned int ScenePropDialog::colorRgb() const
{
    return color_rgb_;
}

unsigned int ScenePropDialog::glassColorRgb() const
{
    return glass_color_rgb_;
}

ScenePropShape ScenePropDialog::shape() const
{
    return (ScenePropShape)shape_combo_->currentData().toInt();
}

QString ScenePropDialog::meshAssetRelative() const
{
    return mesh_asset_relative_;
}

ScenePropFaceKind ScenePropDialog::meshBodyKind() const
{
    return (ScenePropFaceKind)mesh_body_combo_->currentData().toInt();
}

void ScenePropDialog::onShapeChanged(int)
{
    updateShapeUi();
    refreshPreview();
}

void ScenePropDialog::updateShapeUi()
{
    const bool is_mesh = shape() == ScenePropShape::Mesh;
    faces_box_->setVisible(!is_mesh);
    mesh_panel_->setVisible(is_mesh);
}

void ScenePropDialog::onImportMesh()
{
    if(!resource_manager_)
    {
        QMessageBox::warning(this, tr("Import Mesh"),
                             tr("Plugin settings path is not available."));
        return;
    }

    PluginSettingsPaths::EnsurePluginDataLayout(resource_manager_);
    const QString filter = QString::fromStdString(MeshImport::OpenFileFilter());
    const QString path = QFileDialog::getOpenFileName(this, tr("Import 3D Mesh"),
                                                     QString(), filter);
    if(path.isEmpty())
    {
        return;
    }

    if(MeshImport::IsBlockedExtension(path.toStdString()))
    {
        QMessageBox::warning(
            this,
            tr("Import Mesh"),
            tr("Native Blender (.blend) files are not supported — Assimp's Blender "
               "importer is deprecated and can freeze the app.\n\n"
               "In Blender: File → Export → glTF 2.0 (.glb) or Wavefront (.obj), "
               "then import that file here."));
        return;
    }

    QProgressDialog progress(tr("Importing mesh…"), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setCancelButton(nullptr);
    progress.show();
    QApplication::processEvents();

    std::string err;
    MeshImport::TriangleMesh probe;
    std::atomic<bool> done{false};
    bool ok = false;
    std::thread worker([&]() {
        ok = MeshImport::LoadTriangleMesh(path.toStdString(), &probe, &err);
        done.store(true, std::memory_order_release);
    });

    while(!done.load(std::memory_order_acquire))
    {
        QThread::msleep(40);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    worker.join();
    progress.close();

    if(!ok)
    {
        QMessageBox::warning(this, tr("Import Mesh"),
                             tr("Could not load mesh:\n%1")
                                 .arg(QString::fromStdString(err)));
        setStatus(tr("Import failed."));
        return;
    }

    const filesystem::path meshes_dir = PluginSettingsPaths::MeshesDir(resource_manager_);
    std::error_code ec;
    filesystem::create_directories(meshes_dir, ec);
    ScenePropMeshPaths::SetMeshesRoot(meshes_dir);

    const QFileInfo src_info(path);
    const QString dest_name = UniqueMeshFileName(meshes_dir, src_info.fileName());
    const filesystem::path dest_path = meshes_dir / dest_name.toStdString();

    if(!QFile::copy(path, QString::fromStdString(dest_path.string())))
    {
        QMessageBox::warning(this, tr("Import Mesh"), tr("Failed to copy mesh into plugin data."));
        return;
    }

    const QDir src_dir = src_info.absoluteDir();
    const QStringList sidecars = src_dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for(const QString& sibling : sidecars)
    {
        if(sibling.compare(src_info.fileName(), Qt::CaseInsensitive) == 0)
        {
            continue;
        }
        const QString stem = src_info.completeBaseName();
        if(!sibling.startsWith(stem, Qt::CaseInsensitive)
           && !sibling.endsWith(QStringLiteral(".mtl"), Qt::CaseInsensitive)
           && !sibling.endsWith(QStringLiteral(".bin"), Qt::CaseInsensitive))
        {
            continue;
        }
        const QString sibling_dest = UniqueMeshFileName(meshes_dir, sibling);
        QFile::copy(src_dir.filePath(sibling),
                    QString::fromStdString((meshes_dir / sibling_dest.toStdString()).string()));
    }

    mesh_asset_relative_ = dest_name;
    mesh_path_label_->setText(mesh_asset_relative_);

    const size_t tri_count = probe.triangleCount();

    if(name_edit_->text().trimmed().isEmpty()
       || name_edit_->text().startsWith(QStringLiteral("Prop ")))
    {
        name_edit_->setText(src_info.completeBaseName());
    }

    ScenePropMeshCache::instance()->Put(dest_path.string(), std::move(probe));
    setStatus(tr("Imported %1 (%2 tris). Set Width / Height / Depth to size it.")
                  .arg(dest_name)
                  .arg((qulonglong)tri_count));
    refreshPreview();
}

void ScenePropDialog::setRoomSizeMm(float width_mm, float height_mm, float depth_mm)
{
    room_width_mm_ = (width_mm > 1.0f) ? width_mm : 1000.0f;
    room_height_mm_ = (height_mm > 1.0f) ? height_mm : 1000.0f;
    room_depth_mm_ = (depth_mm > 1.0f) ? depth_mm : 1000.0f;
}

void ScenePropDialog::accept()
{
    if(shape() == ScenePropShape::Mesh && mesh_asset_relative_.isEmpty())
    {
        QMessageBox::warning(this, tr("Scene Prop"),
                             tr("Import a mesh file before creating a mesh prop."));
        return;
    }
    QDialog::accept();
}

void ScenePropDialog::onPickSolidColor()
{
    const QColor current((color_rgb_ >> 16) & 0xFF,
                         (color_rgb_ >> 8) & 0xFF,
                         color_rgb_ & 0xFF);
    const QColor picked = QColorDialog::getColor(current, this, tr("Solid colour"));
    if(!picked.isValid())
    {
        return;
    }
    color_rgb_ = ((picked.red() & 0xFF) << 16)
               | ((picked.green() & 0xFF) << 8)
               | (picked.blue() & 0xFF);
    syncColorButton(color_button_, color_rgb_);
    refreshPreview();
}

void ScenePropDialog::onPickGlassColor()
{
    const QColor current((glass_color_rgb_ >> 16) & 0xFF,
                         (glass_color_rgb_ >> 8) & 0xFF,
                         glass_color_rgb_ & 0xFF);
    const QColor picked = QColorDialog::getColor(current, this, tr("Glass colour"));
    if(!picked.isValid())
    {
        return;
    }
    glass_color_rgb_ = ((picked.red() & 0xFF) << 16)
                     | ((picked.green() & 0xFF) << 8)
                     | (picked.blue() & 0xFF);
    syncColorButton(glass_color_button_, glass_color_rgb_);
    refreshPreview();
}

void ScenePropDialog::onSetAllFaces()
{
    const int data = set_all_combo_->currentData().toInt();
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        const int idx = face_combos_[i]->findData(data);
        if(idx >= 0)
        {
            face_combos_[i]->setCurrentIndex(idx);
        }
    }
    refreshPreview();
}

void ScenePropDialog::syncColorButton(QPushButton* button, unsigned int rgb)
{
    if(!button)
    {
        return;
    }
    const int r = (rgb >> 16) & 0xFF;
    const int g = (rgb >> 8) & 0xFF;
    const int b = rgb & 0xFF;
    button->setStyleSheet(
        QStringLiteral("QPushButton { background-color: rgb(%1,%2,%3); color: %4; }")
            .arg(r).arg(g).arg(b)
            .arg((r * 299 + g * 587 + b * 114) > 140000 ? QStringLiteral("black") : QStringLiteral("white")));
    button->setText(QStringLiteral("#%1%2%3")
                        .arg(r, 2, 16, QLatin1Char('0'))
                        .arg(g, 2, 16, QLatin1Char('0'))
                        .arg(b, 2, 16, QLatin1Char('0'))
                        .toUpper());
}
