// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTGLOBALSETTINGSPANEL_H
#define EFFECTGLOBALSETTINGSPANEL_H

#include <QGroupBox>

class QComboBox;
class QLabel;

namespace Ui {
class EffectGlobalSettingsPanel;
}

class OpenRGB3DSpatialTab;

class EffectGlobalSettingsPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit EffectGlobalSettingsPanel(QWidget* parent = nullptr);
    ~EffectGlobalSettingsPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);

    QLabel*   effectRowLabel() const;
    QComboBox* effectCombo() const;
    QLabel*   zoneLabel() const;
    QComboBox* zoneCombo() const;
    QLabel*   originLabelWidget() const;
    QComboBox* originCombo() const;
    QLabel*   boundsLabel() const;
    QComboBox* boundsCombo() const;
    QComboBox* stackEffectTypeCombo() const;
    QComboBox* stackEffectZoneCombo() const;
    QLabel*    roomOutputSectionLabel() const;
    QWidget*   roomOutputHost() const;

    /** Show/hide the Room output label and host together (grid row). */
    void setRoomOutputSectionVisible(bool visible);

private:
    void alignSettingsLabelColumn();

    Ui::EffectGlobalSettingsPanel* ui = nullptr;
};

#endif
