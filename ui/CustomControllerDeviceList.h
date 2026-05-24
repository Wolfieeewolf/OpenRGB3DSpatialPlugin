// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERDEVICELIST_H
#define CUSTOMCONTROLLERDEVICELIST_H

#include "CustomControllerSourceRef.h"
#include <QWidget>
#include <vector>

class QScrollArea;
class QVBoxLayout;
class CustomControllerDeviceWidget;
class CustomControllerDialog;
class ResourceManagerInterface;

namespace Ui {
class CustomControllerDeviceList;
}

class CustomControllerDeviceList : public QWidget
{
    Q_OBJECT

public:
    explicit CustomControllerDeviceList(QWidget* parent = nullptr);
    ~CustomControllerDeviceList() override;

    void rebuild(ResourceManagerInterface* resource_manager, CustomControllerDialog* host);
    void refreshFromHost(int only_controller_index = -1);

    CustomControllerSourceRef selectedSource() const;
    void                      setSelectedControllerIndex(int controller_index);

signals:
    void selectionChanged(const CustomControllerSourceRef& source);
    void enableToggled(const CustomControllerSourceRef& source, bool enabled);

private:
    void clearWidgets();
    void applySelection();
    CustomControllerDeviceWidget* widgetForController(int controller_index) const;

    Ui::CustomControllerDeviceList*            ui = nullptr;
    QScrollArea*                               scroll_area_;
    QWidget*                                   content_widget_;
    QVBoxLayout*                               content_layout_;
    std::vector<CustomControllerDeviceWidget*> device_widgets_;
    int                                        selected_controller_index_  = -1;
    bool                                       suppress_row_activation_    = false;
};

#endif
