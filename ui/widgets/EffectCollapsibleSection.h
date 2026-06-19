// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTCOLLAPSIBLESECTION_H
#define EFFECTCOLLAPSIBLESECTION_H

#include <QWidget>

class QVBoxLayout;

namespace Ui {
class EffectCollapsibleSection;
}

class EffectCollapsibleSection : public QWidget
{
    Q_OBJECT

public:
    explicit EffectCollapsibleSection(const QString& title,
                                      const QString& tooltip = QString(),
                                      QWidget* parent = nullptr);
    ~EffectCollapsibleSection() override;

    QVBoxLayout* bodyLayout() const;

    void setTitle(const QString& title);
    void setExpanded(bool expanded);
    bool isExpanded() const;

private:
    void refreshToggleText();

    Ui::EffectCollapsibleSection* ui = nullptr;
    QString                       title_;
};

#endif
