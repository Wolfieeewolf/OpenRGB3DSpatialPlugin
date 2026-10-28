// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTCONTROLSHOSTPANEL_H
#define EFFECTCONTROLSHOSTPANEL_H

#include <QWidget>

class QVBoxLayout;

class EffectControlsHostPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EffectControlsHostPanel(QWidget* parent = nullptr);
    ~EffectControlsHostPanel() override;

    QVBoxLayout* contentLayout() const;

private:
    QVBoxLayout* content_layout_ = nullptr;
};

#endif
