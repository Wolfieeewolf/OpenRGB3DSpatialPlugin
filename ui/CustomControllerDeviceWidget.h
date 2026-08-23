// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERDEVICEWIDGET_H
#define CUSTOMCONTROLLERDEVICEWIDGET_H

#include "CustomControllerSourceRef.h"
#include <QString>
#include <QWidget>

class QResizeEvent;
class QComboBox;
class PluginClickableLabel;
class QToolButton;
class RGBControllerInterface;
class CustomControllerDialog;

namespace Ui {
class CustomControllerDeviceWidget;
}

class CustomControllerDeviceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CustomControllerDeviceWidget(RGBControllerInterface* controller,
                                          int controller_index,
                                          CustomControllerDialog* host,
                                          QWidget* parent = nullptr);
    ~CustomControllerDeviceWidget() override;

    RGBControllerInterface* controller() const { return controller_; }
    int            controllerIndex() const { return controller_index_; }

    CustomControllerSourceRef currentSource() const;
    void                      applySource(const CustomControllerSourceRef& source);

    void setRowSelected(bool selected);
    void refreshFromHost();
    void setPlusEnabled(bool enabled);

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void deviceActivated(int controller_index);
    void enableToggled(const CustomControllerSourceRef& source, bool enabled);
    void sourceChanged(const CustomControllerSourceRef& source);

private slots:
    void nameClicked();
    void granularityChanged(int index);
    void itemChanged(int index);
    void handleEnableButtonToggled(bool checked);

private:
    void applyCompactComboStyle(QComboBox* combo);
    void applyGranularityComboStyle(QComboBox* combo);
    void applyItemComboStyle(QComboBox* combo);
    void rebuildItemCombo();
    void updatePlusFromSource();
    void updateEnableIcon();
    void notifySourceChanged();
    void updateNameLabelElide();

    static constexpr int kActionColumn = 2;

    QString                   device_name_text_;
    RGBControllerInterface*            controller_;
    int                       controller_index_;
    CustomControllerDialog*   host_;
    bool                      row_selected_;

    Ui::CustomControllerDeviceWidget* ui = nullptr;
};

#endif
