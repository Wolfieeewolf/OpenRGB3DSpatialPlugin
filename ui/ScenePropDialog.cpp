// SPDX-License-Identifier: GPL-2.0-only

#include "ScenePropDialog.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>

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
    /* Object-local sides. At rot 0, Front faces the user (+Z). */
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
} // namespace

ScenePropDialog::ScenePropDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Scene Prop"));
    setMinimumWidth(420);

    name_edit_ = new QLineEdit(this);
    width_spin_ = MakeMmSpin(this);
    height_spin_ = MakeMmSpin(this);
    depth_spin_ = MakeMmSpin(this);
    color_button_ = new QPushButton(tr("Pick…"), this);
    color_button_->setMinimumHeight(28);
    glass_color_button_ = new QPushButton(tr("Pick…"), this);
    glass_color_button_->setMinimumHeight(28);

    auto* hint = new QLabel(
        tr("Faces are sides of this prop (front of the case, desk drawers, screen, etc.). "
           "At default orientation, Front faces you (toward the back of the room). "
           "Rotate to re-aim. Solid = body, Glass = tinted see-through, Blocker = dark occluding panel."),
        this);
    hint->setWordWrap(true);

    auto* form = new QFormLayout();
    form->addRow(tr("Name"), name_edit_);
    form->addRow(tr("Width (X)"), width_spin_);
    form->addRow(tr("Height (Y)"), height_spin_);
    form->addRow(tr("Depth (Z)"), depth_spin_);
    form->addRow(tr("Solid colour"), color_button_);
    form->addRow(tr("Glass colour"), glass_color_button_);

    auto* faces_box = new QGroupBox(tr("Faces"), this);
    auto* faces_form = new QFormLayout(faces_box);
    set_all_combo_ = makeFaceCombo();
    auto* set_all_btn = new QPushButton(tr("Apply to all faces"), faces_box);
    auto* set_all_row = new QWidget(faces_box);
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

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(color_button_, &QPushButton::clicked, this, &ScenePropDialog::onPickSolidColor);
    connect(glass_color_button_, &QPushButton::clicked, this, &ScenePropDialog::onPickGlassColor);
    connect(set_all_btn, &QPushButton::clicked, this, &ScenePropDialog::onSetAllFaces);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addLayout(form);
    layout->addWidget(faces_box);
    layout->addWidget(buttons);

    syncColorButton(color_button_, color_rgb_);
    syncColorButton(glass_color_button_, glass_color_rgb_);
}

QComboBox* ScenePropDialog::makeFaceCombo()
{
    auto* combo = new QComboBox(this);
    combo->addItem(tr("Solid"), (int)ScenePropFaceKind::Solid);
    combo->addItem(tr("Glass"), (int)ScenePropFaceKind::Glass);
    combo->addItem(tr("Blocker"), (int)ScenePropFaceKind::Blocker);
    return combo;
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
    name_edit_->setText(suggested_name);
    width_spin_->setValue(width_mm);
    height_spin_->setValue(height_mm);
    depth_spin_->setValue(depth_mm);
    color_rgb_ = color_rgb & 0x00FFFFFFu;
    glass_color_rgb_ = 0x4AA8C8u;
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        face_combos_[i]->setCurrentIndex(0);
    }
    set_all_combo_->setCurrentIndex(0);
    syncColorButton(color_button_, color_rgb_);
    syncColorButton(glass_color_button_, glass_color_rgb_);
}

void ScenePropDialog::loadFrom(const SceneProp3D& prop)
{
    name_edit_->setText(QString::fromStdString(prop.GetName()));
    width_spin_->setValue(prop.GetWidthMM());
    height_spin_->setValue(prop.GetHeightMM());
    depth_spin_->setValue(prop.GetDepthMM());
    color_rgb_ = prop.GetColor();
    glass_color_rgb_ = prop.GetGlassColor();
    for(int i = 0; i < SceneProp3D::kFaceCount; ++i)
    {
        const int idx = face_combos_[i]->findData((int)prop.GetFaceKind((ScenePropFace)i));
        face_combos_[i]->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    syncColorButton(color_button_, color_rgb_);
    syncColorButton(glass_color_button_, glass_color_rgb_);
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
    prop->SetWidthMM(widthMm());
    prop->SetHeightMM(heightMm());
    prop->SetDepthMM(depthMm());
    prop->SetColor(colorRgb());
    prop->SetGlassColor(glassColorRgb());
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
