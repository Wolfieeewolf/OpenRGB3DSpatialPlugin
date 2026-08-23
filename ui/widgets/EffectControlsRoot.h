// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTCONTROLSROOT_H
#define EFFECTCONTROLSROOT_H

#include <QWidget>

class QVBoxLayout;

class EffectControlsRoot : public QWidget
{
public:
    explicit EffectControlsRoot(QWidget* parent = nullptr);
    ~EffectControlsRoot() override;

    QVBoxLayout* mainLayout() const;

private:
    QVBoxLayout* main_layout_ = nullptr;
};

#endif
