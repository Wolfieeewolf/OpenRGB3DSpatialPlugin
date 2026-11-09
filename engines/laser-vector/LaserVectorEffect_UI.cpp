// SPDX-License-Identifier: GPL-2.0-only

#include "LaserVectorEffect.h"

#include "EffectDoubleSliderRow.h"
#include "EffectSliderPromote.h"
#include "EffectSliderRow.h"
#include "EffectUiRows.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace
{
void SyncCheckBox(QCheckBox* box, bool on)
{
    if(!box)
    {
        return;
    }
    const QSignalBlocker block(box);
    box->setChecked(on);
}
} // namespace

void LaserVectorEffect::SetupCustomUI(QWidget* parent)
{
    if(!parent)
    {
        return;
    }

    settings_panel_ = EffectUiRows::NewEffectPanel("LaserVectorEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(settings_panel_);
    if(!layout)
    {
        return;
    }

    EffectUiRows::AppendInfoLabel(
        layout,
        tr(".ild / .heb store geometry + colour in abstract RGB — not the projector model. "
           "Machine profile maps RGB / RGY / mono / beam punch. "
           "Multi projector origins place each ShowEditor Spur (or ILDA scanner head) across the room."));

    QHBoxLayout* pick_row = new QHBoxLayout();
    pick_row->setContentsMargins(0, 0, 0, 0);
    browse_button_ = new QPushButton(tr("Open laser file…"), settings_panel_);
    browse_button_->setObjectName(QStringLiteral("browseIldaButton"));
    browse_button_->setToolTip(
        tr("Open .ild, .heb, or a ShowEditor .shw (timeline play of the show folder).\n"
           "Locked shows that cannot export ILDA still play via .seq + Key.ord + .heb.\n"
           "Mouse: click. Keyboard: Tab, Space/Enter, or Ctrl+O. Menu: right-click → Open…"));
    browse_button_->setAccessibleName(tr("Open laser file or ShowEditor show"));
    browse_button_->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    clear_button_ = new QPushButton(tr("Clear"), settings_panel_);
    clear_button_->setObjectName(QStringLiteral("clearIldaButton"));
    clear_button_->setToolTip(
        tr("Unload the current laser file.\nMouse: click Clear.\nKeyboard: Tab to Clear, Space/Enter, or Ctrl+W.\nMenu: right-click this panel → Clear file."));
    clear_button_->setAccessibleName(tr("Clear laser file"));
    clear_button_->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    clear_button_->setEnabled(!path_.isEmpty());
    path_label_ = new QLabel(settings_panel_);
    path_label_->setObjectName(QStringLiteral("ildaPathLabel"));
    path_label_->setWordWrap(true);
    path_label_->setMinimumWidth(120);
    UpdatePathLabel();
    pick_row->addWidget(browse_button_);
    pick_row->addWidget(clear_button_);
    pick_row->addWidget(path_label_, 1);
    layout->addLayout(pick_row);

    status_label_ = new QLabel(settings_panel_);
    status_label_->setObjectName(QStringLiteral("ildaStatusLabel"));
    status_label_->setWordWrap(true);
    layout->addWidget(status_label_);

    QHBoxLayout* machine_row = new QHBoxLayout();
    machine_row->setContentsMargins(0, 0, 0, 0);
    QLabel* machine_cap = new QLabel(tr("Machine profile"), settings_panel_);
    machine_cap->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    machine_combo_ = new QComboBox(settings_panel_);
    machine_combo_->setToolTip(
        tr("Not stored in .ild — mirrors ShowEditor/Beyond projector colour setup.\n"
           "RGB: full colour. RGY: no blue. RGBY: yellow diode bias. "
           "Mono: single laser. Beam punch: atmospheric beam look."));
    machine_combo_->setAccessibleName(tr("Laser machine colour profile"));
    machine_combo_->addItem(tr("RGB (full colour)"), static_cast<int>(MachineProfile::Rgb));
    machine_combo_->addItem(tr("RGY (no blue)"), static_cast<int>(MachineProfile::Rgy));
    machine_combo_->addItem(tr("RGBY (yellow diode)"), static_cast<int>(MachineProfile::Rgby));
    machine_combo_->addItem(tr("Mono green"), static_cast<int>(MachineProfile::MonoGreen));
    machine_combo_->addItem(tr("Mono white"), static_cast<int>(MachineProfile::MonoWhite));
    machine_combo_->addItem(tr("Beam punch"), static_cast<int>(MachineProfile::BeamPunch));
    machine_combo_->setCurrentIndex(static_cast<int>(machine_profile_));
    machine_row->addWidget(machine_cap);
    machine_row->addWidget(machine_combo_, 1);
    layout->addLayout(machine_row);

    QHBoxLayout* scanner_row = new QHBoxLayout();
    scanner_row->setContentsMargins(0, 0, 0, 0);
    QLabel* scanner_cap = new QLabel(tr("Scanner / projector"), settings_panel_);
    scanner_cap->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    scanner_combo_ = new QComboBox(settings_panel_);
    scanner_combo_->setToolTip(
        tr("ILDA header “scanner head” (byte 31): which scanner pair a frame is for.\n"
           "Show folders use Spur / track instead. All = every head (multi origins when enabled)."));
    scanner_combo_->setAccessibleName(tr("Scanner or projector channel"));
    scanner_combo_->addItem(tr("All scanners"), -1);
    scanner_row->addWidget(scanner_cap);
    scanner_row->addWidget(scanner_combo_, 1);
    layout->addLayout(scanner_row);

    beam_check_ = new QCheckBox(tr("Beam (projector wedges)"), settings_panel_);
    beam_check_->setChecked(show_beams_);
    beam_check_->setAccessibleName(tr("Beam projector wedges"));
    beam_check_->setToolTip(
        tr("ShowEditor Simulation “Beam”: filled wedges from the projector to the gallery\n"
           "(ilda-viewer / ShowEditor fog shafts). Mouse / Tab+Space; also in panel menu."));
    layout->addWidget(beam_check_);

    graphic_check_ = new QCheckBox(tr("Graphic (gallery figure)"), settings_panel_);
    graphic_check_->setChecked(show_graphic_);
    graphic_check_->setAccessibleName(tr("Graphic gallery figure"));
    graphic_check_->setToolTip(
        tr("ShowEditor Simulation “Graphic”: lit strokes on the far/gallery plane.\n"
           "Usually leave on together with Beam. Mouse / Tab+Space; also in panel menu."));
    layout->addWidget(graphic_check_);

    multi_projectors_check_ = new QCheckBox(tr("Multi projector origins"), settings_panel_);
    multi_projectors_check_->setChecked(multi_projectors_);
    multi_projectors_check_->setAccessibleName(tr("Multi projector origins"));
    multi_projectors_check_->setToolTip(
        tr("ShowEditor Spur / ILDA scanner heads throw from separate room origins.\n"
           "On: active tracks spread left→right on the projector-depth face (Womanizer A/B/C…).\n"
           "Off: every stroke shares one centre origin.\n"
           "Mouse / Tab+Space; also in panel menu."));
    layout->addWidget(multi_projectors_check_);

    join_path_check_ = new QCheckBox(tr("Join lit path (ring/outline)"), settings_panel_);
    join_path_check_->setChecked(join_lit_path_);
    join_path_check_->setAccessibleName(tr("Join lit path"));
    join_path_check_->setToolTip(
        tr("Default off — matches ilda-viewer (blanked travel stays invisible).\n"
           "On: connect successive lit dwells with strokes (RING.ild-style outlines).\n"
           "Mouse / Tab+Space; also in panel menu."));
    layout->addWidget(join_path_check_);

    intensity_widen_check_ = new QCheckBox(tr("Intensity widens beam"), settings_panel_);
    intensity_widen_check_->setChecked(intensity_widens_);
    intensity_widen_check_->setAccessibleName(tr("Intensity widens beam"));
    intensity_widen_check_->setToolTip(
        tr(".ild has no beam-width field (ILDA Beam Brush was never adopted).\n"
           "Brighter RGB ≈ fatter shaft in haze — use this to approximate thin/fat mixes.\n"
           "Mouse / Tab+Space; also in panel menu."));
    layout->addWidget(intensity_widen_check_);

    dwell_widen_check_ = new QCheckBox(tr("Dwell widens beam"), settings_panel_);
    dwell_widen_check_->setChecked(dwell_widens_);
    dwell_widen_check_->setAccessibleName(tr("Dwell widens beam"));
    dwell_widen_check_->setToolTip(
        tr("Repeated lit points at one spot (dwell) look brighter/fatter on real projectors.\n"
           "RING corners often have long dwells — those can read thicker than quick strokes.\n"
           "Mouse / Tab+Space; also in panel menu."));
    layout->addWidget(dwell_widen_check_);

    crisp_beams_check_ = new QCheckBox(tr("Crisp beams (no soft wash)"), settings_panel_);
    crisp_beams_check_->setChecked(crisp_beams_);
    crisp_beams_check_->setAccessibleName(tr("Crisp beams"));
    crisp_beams_check_->setToolTip(
        tr("Keeps the full .ild colour range. Soft falloff greys edges and washes toward white.\n"
           "On: hard shaft — file colour or black. Nearest beam wins (no stacking into white).\n"
           "Mouse / Tab+Space; also in panel menu."));
    layout->addWidget(crisp_beams_check_);

    thickness_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Beam thickness"),
        0.005, 0.40, beam_thickness_,
        3, 0.005,
        EffectSpinOverSliderTip(tr("Base beam radius. Dynamics (intensity/dwell) scale up from this.")));
    width_dyn_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Width dynamics"),
        1.0, 5.0, width_dynamics_,
        2, 0.1,
        EffectSpinOverSliderTip(
            tr("How much brighter/longer dwells fatten that beam’s colour into empty space "
               "(1 = flat, 3–4 = strong). Does not mix neighbours into white.")));
    projector_depth_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Projector depth"),
        0.0, 1.0, projector_depth_,
        2, 0.01,
        EffectSpinOverSliderTip(
            tr("Shared throw depth for all projector heads (0 = near face, 1 = far).\n"
               "With Multi projector origins, Spur tracks spread in X at this Z.")));
    projector_spread_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Projector spread"),
        0.0, 1.0, projector_spread_,
        2, 0.01,
        EffectSpinOverSliderTip(
            tr("How far apart Spur / scanner heads sit left→right.\n"
               "0 = stacked at centre; 1 = full width. Needs Multi projector origins on.")));
    if(projector_spread_row_)
    {
        projector_spread_row_->setEnabled(multi_projectors_);
    }
    fog_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Fog"),
        0.0, 1.0, fog_,
        2, 0.01,
        EffectSpinOverSliderTip(tr("ShowEditor Fog — atmospheric density of Beam wedges.")));
    transparency_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Transparency"),
        0.0, 1.0, transparency_,
        2, 0.01,
        EffectSpinOverSliderTip(tr("ShowEditor Transparency — overall Beam/Graphic opacity.")));
    beam_gain_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Beam opacity"),
        0.0, 1.0, beam_gain_,
        2, 0.01,
        EffectSpinOverSliderTip(tr("ShowEditor Beam slider — strength of projector wedges.")));
    screen_row_ = EffectUiRows::AppendDoubleSliderRow(
        layout,
        tr("Screen"),
        0.0, 1.0, screen_,
        2, 0.01,
        EffectSpinOverSliderTip(tr("ShowEditor Screen — strength of the gallery Graphic.")));
    fps_row_ = EffectUiRows::AppendSliderRow(
        layout,
        tr("Frame rate"),
        1, 120, fps_,
        EffectSpinOverSliderTip(tr("ILDA frames per second before Speed scales playback.")));
    fps_row_->setSuffix(tr(" fps"));
    max_beams_row_ = EffectUiRows::AppendSliderRow(
        layout,
        tr("Max beams"),
        0, 128, max_beams_,
        EffectSpinOverSliderTip(
            tr("Cap how many strokes draw per frame (thins busy frames). 0 = all.\n"
               "Multi projector origins still place each Spur at its own head — this only caps stroke count.")));
    max_beams_row_->setSuffix(tr(" (0=all)"));

    flatten_check_ = new QCheckBox(tr("Flatten Z (2D projection)"), settings_panel_);
    flatten_check_->setChecked(flatten_z_);
    flatten_check_->setAccessibleName(tr("Flatten Z"));
    flatten_check_->setToolTip(
        tr("Treat the figure as 2D on the throw plane.\nMouse / Tab+Space; also in panel menu."));
    layout->addWidget(flatten_check_);

    loop_check_ = new QCheckBox(tr("Loop show"), settings_panel_);
    loop_check_->setChecked(loop_);
    loop_check_->setAccessibleName(tr("Loop show"));
    loop_check_->setToolTip(
        tr("Restart the show when it reaches the end.\nMouse / Tab+Space; also in panel menu."));
    layout->addWidget(loop_check_);

    AlignHBoxRowCaptions({machine_row, scanner_row}, 110);
    AlignCaptionLabels({
        thickness_row_ ? thickness_row_->captionLabel() : nullptr,
        width_dyn_row_ ? width_dyn_row_->captionLabel() : nullptr,
        projector_depth_row_ ? projector_depth_row_->captionLabel() : nullptr,
        projector_spread_row_ ? projector_spread_row_->captionLabel() : nullptr,
        fog_row_ ? fog_row_->captionLabel() : nullptr,
        transparency_row_ ? transparency_row_->captionLabel() : nullptr,
        beam_gain_row_ ? beam_gain_row_->captionLabel() : nullptr,
        screen_row_ ? screen_row_->captionLabel() : nullptr,
        fps_row_ ? fps_row_->captionLabel() : nullptr,
        max_beams_row_ ? max_beams_row_->captionLabel() : nullptr,
    });

    settings_panel_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(settings_panel_, &QWidget::customContextMenuRequested,
            this, &LaserVectorEffect::OnPanelContextMenu);
    connect(browse_button_, &QPushButton::clicked, this, &LaserVectorEffect::OnBrowseIld);
    connect(clear_button_, &QPushButton::clicked, this, &LaserVectorEffect::OnClearIld);
    if(thickness_row_)
    {
        thickness_row_->bindValueChanged(
            this,
            [this](double v) { beam_thickness_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(width_dyn_row_)
    {
        width_dyn_row_->bindValueChanged(
            this,
            [this](double v) { width_dynamics_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(projector_depth_row_)
    {
        projector_depth_row_->bindValueChanged(
            this,
            [this](double v) { projector_depth_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(projector_spread_row_)
    {
        projector_spread_row_->bindValueChanged(
            this,
            [this](double v) { projector_spread_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(fog_row_)
    {
        fog_row_->bindValueChanged(
            this,
            [this](double v) { fog_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(transparency_row_)
    {
        transparency_row_->bindValueChanged(
            this,
            [this](double v) { transparency_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(beam_gain_row_)
    {
        beam_gain_row_->bindValueChanged(
            this,
            [this](double v) { beam_gain_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(screen_row_)
    {
        screen_row_->bindValueChanged(
            this,
            [this](double v) { screen_ = static_cast<float>(v); },
            [this]() { emit ParametersChanged(); });
    }
    if(fps_row_)
    {
        fps_row_->bindValueChanged(
            this,
            [this](int v) { fps_ = v; },
            nullptr,
            [this]() { emit ParametersChanged(); });
    }
    if(max_beams_row_)
    {
        max_beams_row_->bindValueChanged(
            this,
            [this](int v) { max_beams_ = v; },
            nullptr,
            [this]() { emit ParametersChanged(); });
    }
    connect(machine_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if(!machine_combo_)
        {
            return;
        }
        machine_profile_ = static_cast<MachineProfile>(machine_combo_->currentData().toInt());
        emit ParametersChanged();
    });
    connect(scanner_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if(!scanner_combo_)
        {
            return;
        }
        scanner_filter_ = scanner_combo_->currentData().toInt();
        RebuildPlayIndices();
        emit ParametersChanged();
    });
    connect(beam_check_, &QCheckBox::toggled, this, [this](bool on) {
        show_beams_ = on;
        emit ParametersChanged();
    });
    connect(graphic_check_, &QCheckBox::toggled, this, [this](bool on) {
        show_graphic_ = on;
        emit ParametersChanged();
    });
    connect(multi_projectors_check_, &QCheckBox::toggled, this, [this](bool on) {
        multi_projectors_ = on;
        if(projector_spread_row_)
        {
            projector_spread_row_->setEnabled(on);
        }
        UpdateStatusLabel();
        emit ParametersChanged();
    });
    connect(join_path_check_, &QCheckBox::toggled, this, [this](bool on) {
        join_lit_path_ = on;
        RebuildShowGeometry();
        UpdateStatusLabel();
        emit ParametersChanged();
    });
    connect(intensity_widen_check_, &QCheckBox::toggled, this, [this](bool on) {
        intensity_widens_ = on;
        emit ParametersChanged();
    });
    connect(dwell_widen_check_, &QCheckBox::toggled, this, [this](bool on) {
        dwell_widens_ = on;
        emit ParametersChanged();
    });
    connect(crisp_beams_check_, &QCheckBox::toggled, this, [this](bool on) {
        crisp_beams_ = on;
        emit ParametersChanged();
    });
    connect(flatten_check_, &QCheckBox::toggled, this, [this](bool on) {
        flatten_z_ = on;
        emit ParametersChanged();
    });
    connect(loop_check_, &QCheckBox::toggled, this, [this](bool on) {
        loop_ = on;
        emit ParametersChanged();
    });

    UpdateStatusLabel();
    AddWidgetToParent(settings_panel_, parent);
}

void LaserVectorEffect::OnPanelContextMenu(const QPoint& pos)
{
    if(!settings_panel_)
    {
        return;
    }
    QMenu menu(settings_panel_);
    QAction* open_act = menu.addAction(tr("Open laser file…"));
    open_act->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    QAction* clear_act = menu.addAction(tr("Clear file"));
    clear_act->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    clear_act->setEnabled(!path_.isEmpty());
    menu.addSeparator();

    auto addToggle = [&](const QString& text, bool checked) -> QAction* {
        QAction* act = menu.addAction(text);
        act->setCheckable(true);
        act->setChecked(checked);
        return act;
    };
    QAction* beam_act = addToggle(tr("Beam (projector wedges)"), show_beams_);
    QAction* graphic_act = addToggle(tr("Graphic (gallery figure)"), show_graphic_);
    QAction* multi_act = addToggle(tr("Multi projector origins"), multi_projectors_);
    menu.addSeparator();
    QAction* join_act = addToggle(tr("Join lit path (ring/outline)"), join_lit_path_);
    QAction* inten_act = addToggle(tr("Intensity widens beam"), intensity_widens_);
    QAction* dwell_act = addToggle(tr("Dwell widens beam"), dwell_widens_);
    QAction* crisp_act = addToggle(tr("Crisp beams (no soft wash)"), crisp_beams_);
    QAction* flatten_act = addToggle(tr("Flatten Z (2D projection)"), flatten_z_);
    QAction* loop_act = addToggle(tr("Loop show"), loop_);

    QAction* chosen = menu.exec(settings_panel_->mapToGlobal(pos));
    if(!chosen)
    {
        return;
    }
    if(chosen == open_act)
    {
        OnBrowseIld();
        return;
    }
    if(chosen == clear_act)
    {
        OnClearIld();
        return;
    }

    const bool on = chosen->isChecked();
    if(chosen == beam_act)
    {
        show_beams_ = on;
        SyncCheckBox(beam_check_, on);
    }
    else if(chosen == graphic_act)
    {
        show_graphic_ = on;
        SyncCheckBox(graphic_check_, on);
    }
    else if(chosen == multi_act)
    {
        multi_projectors_ = on;
        SyncCheckBox(multi_projectors_check_, on);
        if(projector_spread_row_)
        {
            projector_spread_row_->setEnabled(on);
        }
        UpdateStatusLabel();
    }
    else if(chosen == join_act)
    {
        join_lit_path_ = on;
        SyncCheckBox(join_path_check_, on);
        RebuildShowGeometry();
        UpdateStatusLabel();
    }
    else if(chosen == inten_act)
    {
        intensity_widens_ = on;
        SyncCheckBox(intensity_widen_check_, on);
    }
    else if(chosen == dwell_act)
    {
        dwell_widens_ = on;
        SyncCheckBox(dwell_widen_check_, on);
    }
    else if(chosen == crisp_act)
    {
        crisp_beams_ = on;
        SyncCheckBox(crisp_beams_check_, on);
    }
    else if(chosen == flatten_act)
    {
        flatten_z_ = on;
        SyncCheckBox(flatten_check_, on);
    }
    else if(chosen == loop_act)
    {
        loop_ = on;
        SyncCheckBox(loop_check_, on);
    }
    else
    {
        return;
    }
    emit ParametersChanged();
}
