// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTCUSTOMHOST_H
#define EFFECTCUSTOMHOST_H

#include <QWidget>

/** Empty vertical host for per-effect controls from SetupCustomUI. */
class EffectCustomHost : public QWidget
{
public:
    explicit EffectCustomHost(QWidget* parent = nullptr);
};

#endif
