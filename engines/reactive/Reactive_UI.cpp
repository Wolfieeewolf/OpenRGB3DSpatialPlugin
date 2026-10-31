// SPDX-License-Identifier: GPL-2.0-only

#include "Reactive.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include "EffectCheckRow.h"
#include "EffectSliderRow.h"
#include "EffectLabeledComboRow.h"

#include <QComboBox>
#include <QCheckBox>
#include <QString>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
constexpr int kMinRepeatDeciHz = 1;
constexpr int kMaxRepeatDeciHz = 100;
QString FormatRepeatRate(int deci_hz)
{
    const int clamped = std::clamp(deci_hz, kMinRepeatDeciHz, kMaxRepeatDeciHz);
    return QString::number(clamped / 10.0, 'f', 1) + QStringLiteral(" Hz");
}

QString FormatTravel(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    const char* label = "Device";
    if(v <= 12)
    {
        label = "1 key";
    }
    else if(v <= 28)
    {
        label = "Nearby";
    }
    else if(v <= 58)
    {
        label = "Device";
    }
    else if(v <= 82)
    {
        label = "Around";
    }
    else
    {
        label = "Room";
    }
    return QString::number(v) + QStringLiteral(" · ") + QString::fromUtf8(label);
}

QString FormatAccel(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    if(v <= 0)
    {
        return QStringLiteral("0 · Off");
    }
    if(v <= 25)
    {
        return QString::number(v) + QStringLiteral(" · Mild");
    }
    if(v <= 60)
    {
        return QString::number(v) + QStringLiteral(" · Medium");
    }
    if(v <= 85)
    {
        return QString::number(v) + QStringLiteral(" · Strong");
    }
    return QString::number(v) + QStringLiteral(" · Max");
}

QString FormatFade(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    if(v <= 0)
    {
        return QStringLiteral("0 · Off");
    }
    return QString::number(v) + QStringLiteral("%");
}

QString FormatPressGlow(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    if(v <= 0)
    {
        return QStringLiteral("0 · Off");
    }
    return QString::number(v) + QStringLiteral("%");
}

QString FormatPressGlowTime(int deci_sec)
{
    const int v = std::clamp(deci_sec, 1, 120);
    return QString::number(v / 10.0, 'f', 1) + QStringLiteral(" s");
}

} // namespace

void Reactive::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("ReactiveEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);
    if(!layout)
    {
        return;
    }
    const auto on_changed = [this]() { emit ParametersChanged(); };

    EffectUiRows::AppendInfoLabel(
        layout,
        QStringLiteral("Listens only while this effect is running. Presses are not saved or shown."));

    EffectCheckRow* keyboard_row = EffectUiRows::AppendCheckRow(
        layout,
        QStringLiteral("Keyboard"),
        listen_keyboard_,
        QStringLiteral("Waves from the named LED on the keyboard or keypad that sent the press."));
    keyboard_row->setObjectName(QStringLiteral("listenKeyboardRow"));
    if(keyboard_row->checkBox())
    {
        keyboard_row->checkBox()->setObjectName(QStringLiteral("listenKeyboardCheck"));
    }
    keyboard_row->bindToggled(this, [this](bool on) { listen_keyboard_ = on; }, on_changed);

    EffectCheckRow* mouse_row = EffectUiRows::AppendCheckRow(
        layout,
        QStringLiteral("Mouse"),
        listen_mouse_,
        QStringLiteral("Waves from the left, right, or middle mouse-button LED in the 3D layout."));
    mouse_row->setObjectName(QStringLiteral("listenMouseRow"));
    if(mouse_row->checkBox())
    {
        mouse_row->checkBox()->setObjectName(QStringLiteral("listenMouseCheck"));
    }
    mouse_row->bindToggled(this, [this](bool on) { listen_mouse_ = on; }, on_changed);

    EffectCheckRow* pad_row = EffectUiRows::AppendCheckRow(
        layout,
        QStringLiteral("Gamepad"),
        listen_gamepad_,
        QStringLiteral("Waves from the pressed controller button LED in the 3D layout."));
    pad_row->setObjectName(QStringLiteral("listenGamepadRow"));
    if(pad_row->checkBox())
    {
        pad_row->checkBox()->setObjectName(QStringLiteral("listenGamepadCheck"));
    }
    pad_row->bindToggled(this, [this](bool on) { listen_gamepad_ = on; }, on_changed);

    EffectLabeledComboRow* trigger_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Trigger:"));
    trigger_row->setObjectName(QStringLiteral("triggerRow"));
    QComboBox* trigger_combo = trigger_row->combo();
    for(int m = 0; m < TRIGGER_COUNT; ++m)
    {
        trigger_combo->addItem(QString::fromUtf8(TriggerName(m)));
    }
    trigger_combo->setCurrentIndex(std::clamp(trigger_mode_, 0, TRIGGER_COUNT - 1));
    trigger_combo->setToolTip(QStringLiteral(
        "On press: fire when the key or button goes down.\n"
        "On release: fire when it comes up.\n"
        "While held: treat the down state as the trigger (pairs with 2 pulses, 4 pulses, or Continual)."));
    connect(trigger_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        trigger_mode_ = std::clamp(idx, 0, TRIGGER_COUNT - 1);
        emit ParametersChanged();
    });

    EffectLabeledComboRow* wave_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Wave:"));
    wave_row->setObjectName(QStringLiteral("waveRow"));
    QComboBox* wave_combo = wave_row->combo();
    for(int m = 0; m < WAVE_COUNT; ++m)
    {
        wave_combo->addItem(QString::fromUtf8(WaveName(m)));
    }
    wave_combo->setCurrentIndex(std::clamp(wave_mode_, 0, WAVE_COUNT - 1));
    wave_combo->setToolTip(QStringLiteral(
        "Every press (and every extra pulse while held) starts a brand-new shell that keeps "
        "traveling on its own — older shells are never reset or reused.\n"
        "Once: one shell per press or release.\n"
        "2 pulses / 4 pulses: that many overlapping shells while you hold, at the pulse rate.\n"
        "Continual: keep spawning overlapping shells for as long as it is held."));

    EffectSliderRow* rate_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Pulse rate:"),
        kMinRepeatDeciHz,
        kMaxRepeatDeciHz,
        repeat_rate_deci_hz_,
        QStringLiteral(
            "How fast extra shells spawn while a key or button is held (0.1–10 Hz). "
            "Each tick adds another independent shell; earlier ones keep finishing. "
            "Used for 2 pulses, 4 pulses, and Continual. Once ignores this."));
    rate_row->setObjectName(QStringLiteral("repeatRateRow"));
    rate_row->bindValueChanged(
        this,
        [this](int v) { repeat_rate_deci_hz_ = std::clamp(v, kMinRepeatDeciHz, kMaxRepeatDeciHz); },
        FormatRepeatRate,
        on_changed);
    rate_row->setEnabled(wave_mode_ != WAVE_ONCE);
    connect(wave_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, rate_row](int idx) {
        wave_mode_ = std::clamp(idx, 0, WAVE_COUNT - 1);
        rate_row->setEnabled(wave_mode_ != WAVE_ONCE);
        emit ParametersChanged();
    });

    EffectLabeledComboRow* spread_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Spread:"));
    spread_row->setObjectName(QStringLiteral("spreadRow"));
    QComboBox* spread_combo = spread_row->combo();
    for(int m = 0; m < SPREAD_COUNT; ++m)
    {
        spread_combo->addItem(QString::fromUtf8(SpreadName(m)));
    }
    spread_combo->setCurrentIndex(std::clamp(spread_mode_, 0, SPREAD_COUNT - 1));
    spread_combo->setToolTip(QStringLiteral(
        "How distance is measured from the press. Each new pulse uses its own shell — "
        "Axis/Cross no longer flash the whole mid-plane when a pulse starts.\n"
        "Shell: full 3D (sphere, cube, …).\n"
        "Plane: 2D ripple in XY, XZ, or YZ.\n"
        "Axis wall: thin wall traveling on X, Y, or Z.\n"
        "Cross: two walls (X+Z, X+Y, or Y+Z).\n"
        "Triad: all three axis walls at once."));

    EffectLabeledComboRow* orient_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Orientation:"));
    orient_row->setObjectName(QStringLiteral("orientRow"));
    QComboBox* orient_combo = orient_row->combo();
    orient_combo->setToolTip(QStringLiteral(
        "Plane: XY (vertical sheet), XZ (keyboard/desk), YZ (side wall).\n"
        "Axis: travel along X, Y, or Z.\n"
        "Cross: pick the wall pair. Shell and Triad ignore this."));

    EffectLabeledComboRow* look_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Look:"));
    look_row->setObjectName(QStringLiteral("lookRow"));
    QComboBox* look_combo = look_row->combo();
    for(int m = 0; m < LOOK_COUNT; ++m)
    {
        look_combo->addItem(QString::fromUtf8(LookName(m)));
    }
    look_combo->setCurrentIndex(std::clamp(look_mode_, 0, LOOK_COUNT - 1));
    look_combo->setToolTip(QStringLiteral(
        "How each independent pulse is filled (older pulses keep their look until they finish).\n"
        "Ring: traveling band only.\n"
        "Solid: filled behind the front, then clears.\n"
        "Shock: fast blast and fireball.\n"
        "Flash: bright pop at the key, little travel.\n"
        "Double ring: two nested bands."));

    EffectLabeledComboRow* shape_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Shape:"));
    shape_row->setObjectName(QStringLiteral("shapeRow"));
    QComboBox* shape_combo = shape_row->combo();
    for(int m = 0; m < SHAPE_COUNT; ++m)
    {
        shape_combo->addItem(QString::fromUtf8(ShapeName(m)));
    }
    shape_combo->setCurrentIndex(std::clamp(footprint_shape_, 0, SHAPE_COUNT - 1));
    shape_combo->setToolTip(QStringLiteral(
        "Footprint for Shell (3D) and Plane (2D): sphere/circle, cube/square, hex, etc."));

    EffectSliderRow* travel_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Travel:"),
        0,
        100,
        travel_pct_,
        QStringLiteral(
            "How far the pulse travels. Pair with Accelerate for Room travel at low Speed. "
            "1 key stays on the pressed button, Nearby is two or three keys, "
            "Device covers that controller, Room fills the layer."));
    travel_row->setObjectName(QStringLiteral("travelRow"));
    travel_row->bindValueChanged(
        this,
        [this](int v) { travel_pct_ = std::clamp(v, 0, 100); },
        FormatTravel,
        on_changed);

    EffectSliderRow* accel_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Accelerate:"),
        0,
        100,
        accel_pct_,
        QStringLiteral(
            "Starts slow near the key, then picks up speed toward full travel. "
            "Use with Room travel and a low Speed so the press still feels soft but the "
            "shell reaches the edges without taking half a minute."));
    accel_row->setObjectName(QStringLiteral("accelRow"));
    accel_row->bindValueChanged(
        this,
        [this](int v) { accel_pct_ = std::clamp(v, 0, 100); },
        FormatAccel,
        on_changed);

    EffectSliderRow* fade_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Distance fade:"),
        0,
        100,
        distance_fade_pct_,
        QStringLiteral(
            "Dims the traveling shell as it gets farther from the press. "
            "0 keeps full brightness; 100 fades it out by the time it reaches full travel."));
    fade_row->setObjectName(QStringLiteral("distanceFadeRow"));
    fade_row->bindValueChanged(
        this,
        [this](int v) { distance_fade_pct_ = std::clamp(v, 0, 100); },
        FormatFade,
        on_changed);

    EffectSliderRow* glow_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Press glow:"),
        0,
        100,
        press_glow_pct_,
        QStringLiteral(
            "Sticky leftover light on/near the pressed key after the shell leaves. "
            "0 turns it off (recommended if you only want the traveling pulse). "
            "Higher values light about 2–3 keys around the press."));
    glow_row->setObjectName(QStringLiteral("pressGlowRow"));

    EffectSliderRow* glow_time_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Glow time:"),
        1,
        120,
        press_glow_deci_sec_,
        QStringLiteral(
            "How long the press glow sticks around (0.1–12 s). Only used when Press glow is above 0."));
    glow_time_row->setObjectName(QStringLiteral("pressGlowTimeRow"));
    glow_time_row->bindValueChanged(
        this,
        [this](int v) { press_glow_deci_sec_ = std::clamp(v, 1, 120); },
        FormatPressGlowTime,
        on_changed);
    glow_time_row->setEnabled(press_glow_pct_ > 0);
    glow_row->bindValueChanged(
        this,
        [this, glow_time_row](int v) {
            press_glow_pct_ = std::clamp(v, 0, 100);
            glow_time_row->setEnabled(press_glow_pct_ > 0);
        },
        FormatPressGlow,
        on_changed);

    const auto repopulate_orient = [orient_combo, this]() {
        orient_combo->blockSignals(true);
        orient_combo->clear();
        const int spread = std::clamp(spread_mode_, 0, SPREAD_COUNT - 1);
        if(spread == SPREAD_PLANE || spread == SPREAD_AXIS || spread == SPREAD_CROSS)
        {
            for(int o = 0; o < SPREAD_ORIENT_COUNT; ++o)
            {
                orient_combo->addItem(QString::fromUtf8(SpreadOrientName(spread, o)));
            }
            orient_combo->setCurrentIndex(std::clamp(spread_orient_, 0, SPREAD_ORIENT_COUNT - 1));
            orient_combo->setEnabled(true);
        }
        else
        {
            orient_combo->addItem(QString::fromUtf8(SpreadOrientName(spread, spread_orient_)));
            orient_combo->setCurrentIndex(0);
            orient_combo->setEnabled(false);
        }
        orient_combo->blockSignals(false);
    };
    const auto sync_shape_enabled = [shape_row, this]() {
        shape_row->setEnabled(spread_mode_ == SPREAD_SHELL || spread_mode_ == SPREAD_PLANE);
    };
    connect(spread_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, repopulate_orient, sync_shape_enabled](int idx) {
        spread_mode_ = std::clamp(idx, 0, SPREAD_COUNT - 1);
        repopulate_orient();
        sync_shape_enabled();
        emit ParametersChanged();
    });
    connect(orient_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        spread_orient_ = std::clamp(idx, 0, SPREAD_ORIENT_COUNT - 1);
        emit ParametersChanged();
    });
    connect(look_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        look_mode_ = std::clamp(idx, 0, LOOK_COUNT - 1);
        emit ParametersChanged();
    });
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        footprint_shape_ = std::clamp(idx, 0, SHAPE_COUNT - 1);
        emit ParametersChanged();
    });
    repopulate_orient();
    sync_shape_enabled();

    AddWidgetToParent(w, parent);
}

void Reactive::SyncCustomUiFromModel()
{
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "ReactiveEffectSettings"))
        {
            EffectUiSync::setCheckBox(fx, "listenKeyboardCheck", listen_keyboard_);
            EffectUiSync::setCheckBox(fx, "listenMouseCheck", listen_mouse_);
            EffectUiSync::setCheckBox(fx, "listenGamepadCheck", listen_gamepad_);
            EffectUiSync::setComboIndex(fx, "triggerRow", trigger_mode_);
            EffectUiSync::setComboIndex(fx, "waveRow", wave_mode_);
            EffectUiSync::setComboIndex(fx, "spreadRow", spread_mode_);
            EffectUiSync::setComboIndex(fx, "lookRow", look_mode_);
            EffectUiSync::setComboIndex(fx, "shapeRow", footprint_shape_);
            EffectUiSync::setSliderValue(fx, "repeatRateRow", repeat_rate_deci_hz_, FormatRepeatRate);
            EffectUiSync::setSliderValue(fx, "travelRow", travel_pct_, FormatTravel);
            EffectUiSync::setSliderValue(fx, "accelRow", accel_pct_, FormatAccel);
            EffectUiSync::setSliderValue(fx, "distanceFadeRow", distance_fade_pct_, FormatFade);
            EffectUiSync::setSliderValue(fx, "pressGlowRow", press_glow_pct_, FormatPressGlow);
            EffectUiSync::setSliderValue(fx, "pressGlowTimeRow", press_glow_deci_sec_, FormatPressGlowTime);
            if(EffectSliderRow* rate_row = EffectUiSync::sliderRow(fx, "repeatRateRow"))
            {
                rate_row->setEnabled(wave_mode_ != WAVE_ONCE);
            }
            if(EffectSliderRow* glow_time_row = EffectUiSync::sliderRow(fx, "pressGlowTimeRow"))
            {
                glow_time_row->setEnabled(press_glow_pct_ > 0);
            }
            if(EffectLabeledComboRow* orient_row = EffectUiSync::comboRow(fx, "orientRow"))
            {
                QComboBox* orient_combo = orient_row->combo();
                if(orient_combo)
                {
                    orient_combo->blockSignals(true);
                    orient_combo->clear();
                    const int spread = std::clamp(spread_mode_, 0, SPREAD_COUNT - 1);
                    if(spread == SPREAD_PLANE || spread == SPREAD_AXIS || spread == SPREAD_CROSS)
                    {
                        for(int o = 0; o < SPREAD_ORIENT_COUNT; ++o)
                        {
                            orient_combo->addItem(QString::fromUtf8(SpreadOrientName(spread, o)));
                        }
                        orient_combo->setCurrentIndex(std::clamp(spread_orient_, 0, SPREAD_ORIENT_COUNT - 1));
                        orient_combo->setEnabled(true);
                    }
                    else
                    {
                        orient_combo->addItem(QString::fromUtf8(SpreadOrientName(spread, spread_orient_)));
                        orient_combo->setCurrentIndex(0);
                        orient_combo->setEnabled(false);
                    }
                    orient_combo->blockSignals(false);
                }
            }
            if(EffectLabeledComboRow* shape_row = EffectUiSync::comboRow(fx, "shapeRow"))
            {
                shape_row->setEnabled(spread_mode_ == SPREAD_SHELL || spread_mode_ == SPREAD_PLANE);
            }
        }
    }
}

