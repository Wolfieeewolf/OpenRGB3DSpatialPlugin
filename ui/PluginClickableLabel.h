// SPDX-License-Identifier: GPL-2.0-only

#ifndef PLUGINCLICKABLELABEL_H
#define PLUGINCLICKABLELABEL_H

#include <QLabel>

class PluginClickableLabel : public QLabel
{
    Q_OBJECT

public:
    explicit PluginClickableLabel(QWidget* parent = nullptr);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
};

#endif
