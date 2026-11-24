// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EFFECTCOLORPANEL_H
#define EFFECTCOLORPANEL_H

#include <QWidget>

class QComboBox;
class QHBoxLayout;
class QPushButton;
class QVBoxLayout;

namespace Ui {
class EffectColorPanel;
}

class EffectColorPanel : public QWidget
{
public:
    /** @param material_rich 1 = Rich (default), 0 = Flat. @param color_source 0 Look, 1 Custom, 2 Rainbow. */
    explicit EffectColorPanel(bool rainbow_mode, QWidget* parent = nullptr);
    explicit EffectColorPanel(int color_source, int material_rich, QWidget* parent = nullptr);
    ~EffectColorPanel() override;

    QComboBox* sourceCombo() const;
    QComboBox* styleCombo() const;
    QPushButton* resetLookButton() const;
    QVBoxLayout* tintHostLayout() const;
    QWidget* colorButtonsWidget() const;
    QHBoxLayout* colorButtonsLayout() const;
    QWidget* patternHostWidget() const;
    QVBoxLayout* patternHostLayout() const;
    QPushButton* addColorButton() const;
    QPushButton* removeColorButton() const;

    /** Legacy: Rainbow checkbox removed — returns nullptr. Prefer sourceCombo(). */
    class QCheckBox* rainbowModeCheck() const { return nullptr; }

private:
    void initCombos(int color_source, int material_rich);

    Ui::EffectColorPanel* ui = nullptr;
};

#endif
