// SPDX-License-Identifier: GPL-2.0-only

#include "PositionAxisDragController.h"
#include "OpenRGB3DSpatialTab.h"

#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kSliderVisualRange = 1000;
}

PositionAxisDragController::PositionAxisDragController(OpenRGB3DSpatialTab* tab,
                                                       QSlider* slider,
                                                       QDoubleSpinBox* spin,
                                                       int axis,
                                                       bool clamp_non_negative)
    : tab_(tab)
    , slider_(slider)
    , spin_(spin)
    , axis_(axis)
    , clamp_non_negative_(clamp_non_negative)
{
    if(slider_)
    {
        slider_->setRange(0, kSliderVisualRange);
        slider_->setSingleStep(1);
        slider_->setPageStep(10);
        slider_->setTracking(false);
        slider_->installEventFilter(this);
    }

    if(spin_)
    {
        spin_->setDecimals(1);
        spin_->setSingleStep(0.1);
        spin_->setSuffix(QStringLiteral(" mm"));

        connect(spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value_mm) {
            if(clamp_non_negative_ && value_mm < 0.0)
            {
                value_mm = 0.0;
                QSignalBlocker block(spin_);
                spin_->setValue(0.0);
            }
            applyMm(value_mm);
        });
    }
}

void PositionAxisDragController::fetchLimits(double& min_mm, double& max_mm) const
{
    if(tab_)
    {
        tab_->ScenePositionAxisLimitsMm(axis_, min_mm, max_mm);
    }
    else
    {
        min_mm = 0.0;
        max_mm = 1000.0;
    }
}

void PositionAxisDragController::applyMm(double mm)
{
    if(!tab_)
    {
        return;
    }

    double min_mm = 0.0;
    double max_mm = 1000.0;
    fetchLimits(min_mm, max_mm);
    if(max_mm < min_mm)
    {
        std::swap(min_mm, max_mm);
    }

    if(clamp_non_negative_ && mm < 0.0)
    {
        mm = 0.0;
    }
    mm = std::clamp(mm, min_mm, max_mm);

    current_mm_ = mm;

    if(spin_)
    {
        QSignalBlocker block(spin_);
        spin_->setValue(mm);
    }

    tab_->ApplyScenePositionAbsoluteMm(axis_, mm);
    syncSliderVisual();
}

void PositionAxisDragController::setCurrentMm(double mm)
{
    current_mm_ = mm;
    if(spin_)
    {
        QSignalBlocker block(spin_);
        spin_->setValue(mm);
    }
    syncSliderVisual();
}

void PositionAxisDragController::syncSliderVisual()
{
    if(!slider_)
    {
        return;
    }

    double min_mm = 0.0;
    double max_mm = 1000.0;
    fetchLimits(min_mm, max_mm);
    const double span = max_mm - min_mm;

    int visual = 0;
    if(span > 1e-6)
    {
        visual = (int)std::lround((current_mm_ - min_mm) / span * (double)kSliderVisualRange);
    }
    visual = std::clamp(visual, 0, kSliderVisualRange);

    QSignalBlocker block(slider_);
    slider_->setRange(0, kSliderVisualRange);
    slider_->setValue(visual);
}

bool PositionAxisDragController::sliderHandleContains(const QPoint& pos) const
{
    if(!slider_)
    {
        return false;
    }

    QStyleOptionSlider opt;
    opt.initFrom(slider_);
    opt.subControls = QStyle::SC_All;
    opt.orientation = slider_->orientation();
    opt.minimum     = slider_->minimum();
    opt.maximum     = slider_->maximum();
    opt.sliderPosition = slider_->value();
    opt.sliderValue    = slider_->value();

    const QRect handle_rect =
        slider_->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, slider_);
    return handle_rect.contains(pos);
}

double PositionAxisDragController::mmPerPixel() const
{
    double min_mm = 0.0;
    double max_mm = 1000.0;
    fetchLimits(min_mm, max_mm);
    double span = max_mm - min_mm;
    if(span < 100.0)
    {
        span = 100.0;
    }

    const int width_px = std::max(1, slider_->width());
    return span / (double)width_px;
}

void PositionAxisDragController::onDragDelta(int delta_px, Qt::KeyboardModifiers modifiers)
{
    if(delta_px == 0)
    {
        return;
    }

    double sensitivity = mmPerPixel();
    if(modifiers & Qt::ShiftModifier)
    {
        sensitivity *= 0.1;
    }
    if(modifiers & Qt::ControlModifier)
    {
        sensitivity *= 0.25;
    }

    applyMm(current_mm_ + (double)delta_px * sensitivity);
}

void PositionAxisDragController::onWheel(QWheelEvent* event)
{
    if(!event)
    {
        return;
    }

    const QPoint angle = event->angleDelta();
    int steps          = 0;
    if(std::abs(angle.y()) >= std::abs(angle.x()))
    {
        steps = angle.y() > 0 ? 1 : (angle.y() < 0 ? -1 : 0);
    }
    else
    {
        steps = angle.x() > 0 ? 1 : (angle.x() < 0 ? -1 : 0);
    }
    if(steps == 0)
    {
        return;
    }

    double step_mm = 0.1;
    if(event->modifiers() & Qt::ShiftModifier)
    {
        step_mm = 1.0;
    }
    if(event->modifiers() & Qt::ControlModifier)
    {
        step_mm = 0.5;
    }

    applyMm(current_mm_ + (double)steps * step_mm);
    if(event)
    {
        event->accept();
    }
}

bool PositionAxisDragController::eventFilter(QObject* watched, QEvent* event)
{
    if(watched != slider_ || !slider_ || !tab_)
    {
        return QObject::eventFilter(watched, event);
    }

    switch(event->type())
    {
        case QEvent::MouseButtonPress:
        {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if(mouse->button() != Qt::LeftButton)
            {
                break;
            }
            if(!sliderHandleContains(mouse->pos()))
            {
                return true;
            }
            dragging_      = true;
            last_global_x_ = mouse->globalPosition().toPoint().x();
            return true;
        }
        case QEvent::MouseMove:
        {
            if(!dragging_)
            {
                break;
            }
            auto* mouse = static_cast<QMouseEvent*>(event);
            const int   global_x = mouse->globalPosition().toPoint().x();
            const int   delta_px = global_x - last_global_x_;
            last_global_x_       = global_x;
            onDragDelta(delta_px, mouse->modifiers());
            return true;
        }
        case QEvent::MouseButtonRelease:
        {
            if(dragging_ && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)
            {
                dragging_ = false;
                return true;
            }
            break;
        }
        case QEvent::Wheel:
            onWheel(static_cast<QWheelEvent*>(event));
            return true;
        default:
            break;
    }

    return QObject::eventFilter(watched, event);
}
