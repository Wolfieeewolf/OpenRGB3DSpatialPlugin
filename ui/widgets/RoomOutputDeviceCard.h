// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMOUTPUTDEVICECARD_H
#define ROOMOUTPUTDEVICECARD_H

#include <QWidget>

class QFrame;
class QLabel;
class QResizeEvent;
class QShowEvent;
class QToolButton;

class RoomOutputDeviceCard : public QWidget
{
    Q_OBJECT

public:
    explicit RoomOutputDeviceCard(const QString& title, QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setAdded(bool added);
    void setInteractionEnabled(bool enabled);

signals:
    void actionToggled(bool added);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void updateActionIcon();
    void updateNameLabelLayout();
    int  nameLabelContentWidth() const;

    QFrame*      card_frame_    = nullptr;
    QLabel*      name_label_    = nullptr;
    QToolButton* action_button_ = nullptr;
    QString      title_text_;
    bool         added_ = false;
};

#endif
