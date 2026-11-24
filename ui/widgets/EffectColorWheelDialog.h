// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EFFECTCOLORWHEELDIALOG_H
#define EFFECTCOLORWHEELDIALOG_H

#include <QColor>
#include <QDialog>
#include <QString>

class ColorWheel;
class QPushButton;

/** Modal OpenRGB ColorWheel picker for effect colour stops (not QColorDialog). */
class EffectColorWheelDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EffectColorWheelDialog(QWidget* parent = nullptr);

    void setColor(const QColor& color);
    QColor color() const;

    /** Returns true if the user accepted. Updates @p color in place. */
    static bool pickColor(QWidget* parent, QColor& color,
                          const QString& title = QString());

private slots:
    void onWheelColorChanged(const QColor& color);

private:
    void refreshSwatch(const QColor& color);

    ColorWheel* wheel_ = nullptr;
    QPushButton* swatch_ = nullptr;
};

#endif
