// SPDX-License-Identifier: GPL-2.0-only

#ifndef DISPLAYPLANEDIALOG_H
#define DISPLAYPLANEDIALOG_H

#include <QDialog>
#include <string>

class OpenRGB3DSpatialTab;
class DisplayPlane3D;

namespace Ui {
class DisplayPlaneDialog;
}

class DisplayPlaneDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DisplayPlaneDialog(OpenRGB3DSpatialTab* host_tab, QWidget* parent = nullptr);
    ~DisplayPlaneDialog() override;

    void setCreateMode();
    void setEditMode();
    void setCreateDefaults(const QString& suggested_name, float width_mm, float height_mm);
    void loadFrom(const DisplayPlane3D& plane);

    QString      name() const;
    float        widthMm() const;
    float        heightMm() const;
    std::string  captureSourceId() const;

private slots:
    void onRefreshCaptureClicked();
    void onOpenSpecsClicked();

private:
    void populateCaptureCombo(const std::string& prefer_source_id);

    Ui::DisplayPlaneDialog* ui = nullptr;
    OpenRGB3DSpatialTab*    host_tab_ = nullptr;
};

#endif
