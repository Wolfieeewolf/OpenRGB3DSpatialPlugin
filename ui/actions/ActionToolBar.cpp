// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionToolBar.h"
#include "ActionCatalog.h"
#include "ActionUserCurves.h"
#include "ActionUserGradients.h"
#include "EffectColorWheelDialog.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QEnterEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QToolButton>
#include <QWidgetAction>

#include <algorithm>
#include <functional>
#include <memory>
#include <system_error>

namespace
{

/* One menu entry that can be clicked, dragged out of the menu, or activated with Enter. */
class MenuItemRow : public QWidget
{
public:
    MenuItemRow(const QIcon& icon, const QSize& icon_size, const QString& text, QWidget* parent)
        : QWidget(parent)
        , icon_(icon)
        , icon_size_(icon_size)
        , text_(text)
    {
        setAttribute(Qt::WA_Hover, true);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(text);
    }

    std::function<QMimeData*()> mime;
    std::function<void()> activate;
    std::function<void(MenuItemRow*, const QPoint&)> context;

    void setIcon(const QIcon& icon)
    {
        icon_ = icon;
        update();
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        return QSize(std::max(170, icon_size_.width() + fm.horizontalAdvance(text_) + 36),
                     std::max(icon_size_.height() + 8, fm.height() + 10));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const bool hot = underMouse() || hasFocus();
        if(hot)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(palette().color(QPalette::Highlight));
            p.drawRoundedRect(QRectF(rect()).adjusted(3, 1, -3, -1), 4, 4);
        }
        const int icon_x = 10;
        const QRect icon_rect(icon_x, (height() - icon_size_.height()) / 2, icon_size_.width(), icon_size_.height());
        if(!icon_.isNull())
        {
            icon_.paint(&p, icon_rect);
        }
        p.setPen(palette().color(hot ? QPalette::HighlightedText : QPalette::Text));
        const QRect text_rect(icon_rect.right() + 9, 0, width() - icon_rect.right() - 16, height());
        p.drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft,
                   fontMetrics().elidedText(text_, Qt::ElideRight, text_rect.width()));
    }

    void enterEvent(QEnterEvent* event) override
    {
        update();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        update();
        QWidget::leaveEvent(event);
    }

    void focusInEvent(QFocusEvent* event) override
    {
        update();
        QWidget::focusInEvent(event);
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        update();
        QWidget::focusOutEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if(event->button() == Qt::LeftButton)
        {
            press_pos_ = event->position().toPoint();
            pressed_ = true;
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if(!pressed_ || !(event->buttons() & Qt::LeftButton) || !mime)
        {
            return;
        }
        if((event->position().toPoint() - press_pos_).manhattanLength() < QApplication::startDragDistance())
        {
            return;
        }
        pressed_ = false;
        QMimeData* data = mime();
        if(!data)
        {
            return;
        }
        /* Close the menus first so the timeline under them can take the drop. */
        closeMenus();
        auto* drag = new QDrag(this);
        drag->setMimeData(data);
        if(!icon_.isNull())
        {
            drag->setPixmap(icon_.pixmap(icon_size_));
        }
        drag->exec(Qt::CopyAction);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if(event->button() == Qt::LeftButton && pressed_)
        {
            pressed_ = false;
            fire();
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space)
        {
            fire();
            event->accept();
            return;
        }
        if(event->key() == Qt::Key_Menu && context)
        {
            context(this, mapToGlobal(QPoint(width() / 2, height() / 2)));
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        if(context)
        {
            context(this, event->globalPos());
            event->accept();
            return;
        }
        QWidget::contextMenuEvent(event);
    }

private:
    void fire()
    {
        closeMenus();
        if(activate)
        {
            activate();
        }
    }

    void closeMenus()
    {
        for(QWidget* w = parentWidget(); w; w = w->parentWidget())
        {
            if(QMenu* menu = qobject_cast<QMenu*>(w))
            {
                menu->close();
            }
        }
    }

    QIcon icon_;
    QSize icon_size_;
    QString text_;
    QPoint press_pos_;
    bool pressed_ = false;
};

MenuItemRow* AddRow(QMenu* menu, const QIcon& icon, const QSize& icon_size, const QString& text,
                    const QString& tooltip)
{
    auto* action = new QWidgetAction(menu);
    auto* row = new MenuItemRow(icon, icon_size, text, menu);
    row->setToolTip(tooltip);
    action->setDefaultWidget(row);
    action->setText(text);
    QObject::connect(action, &QAction::triggered, row, [row]() {
        if(row->activate)
        {
            row->activate();
        }
    });
    menu->addAction(action);
    return row;
}

void AddHint(QMenu* menu, const QString& text)
{
    QAction* hint = menu->addAction(text);
    hint->setEnabled(false);
    menu->addSeparator();
}

QString PathToQString(const filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

struct Swatch
{
    QString label;
    QColor color;
};

QVector<Swatch> DefaultSwatches()
{
    QVector<Swatch> out;
    for(const ActionCatalog::ColorEntry& c : ActionCatalog::ColorEntries())
    {
        out.push_back({QString::fromUtf8(c.label ? c.label : "Color"), c.color});
    }
    return out;
}

QVector<Swatch> LoadSwatches(const filesystem::path& path)
{
    QVector<Swatch> defaults = DefaultSwatches();
    QFile file(PathToQString(path));
    if(!file.open(QIODevice::ReadOnly))
    {
        return defaults;
    }
    constexpr qint64 kMaxSwatchBytes = 64 * 1024;
    if(file.size() > kMaxSwatchBytes)
    {
        return defaults;
    }
    const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("colors")).toArray();
    QVector<Swatch> loaded;
    for(const QJsonValue& item : arr)
    {
        const QJsonObject obj = item.toObject();
        if(!obj.contains(QStringLiteral("r")))
        {
            continue;
        }
        Swatch swatch;
        swatch.label = obj.value(QStringLiteral("label")).toString(QStringLiteral("Color"));
        swatch.color = QColor(obj.value(QStringLiteral("r")).toInt(),
                               obj.value(QStringLiteral("g")).toInt(),
                               obj.value(QStringLiteral("b")).toInt());
        loaded.push_back(swatch);
    }
    return loaded.isEmpty() ? defaults : loaded;
}

void SaveSwatches(const filesystem::path& path, const QVector<Swatch>& swatches)
{
    std::error_code ec;
    filesystem::create_directories(path.parent_path(), ec);
    QJsonArray arr;
    for(const Swatch& swatch : swatches)
    {
        arr.push_back(QJsonObject{
            {QStringLiteral("label"), swatch.label},
            {QStringLiteral("r"), swatch.color.red()},
            {QStringLiteral("g"), swatch.color.green()},
            {QStringLiteral("b"), swatch.color.blue()},
        });
    }
    QFile file(PathToQString(path));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("colors"), arr}}).toJson());
}

QIcon SwatchIcon(const QColor& color)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(color.darker(170), 1));
    p.setBrush(color);
    p.drawEllipse(QRectF(1.5, 1.5, 13, 13));
    return QIcon(pm);
}

QIcon CurveIcon(const std::vector<Action::CurvePoint>& points, const QColor& color)
{
    QPixmap pm(36, 16);
    pm.fill(Qt::transparent);
    if(points.empty())
    {
        return QIcon(pm);
    }
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    for(int i = 0; i <= 34; ++i)
    {
        const float t = i / 34.0f;
        const float v = std::clamp(Action::SampleCurve(points, t), 0.0f, 1.0f);
        const QPointF pt(1 + i, 14.5 - v * 13.0);
        if(i == 0)
        {
            path.moveTo(pt);
        }
        else
        {
            path.lineTo(pt);
        }
    }
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
    return QIcon(pm);
}

} // namespace

ActionToolBar::ActionToolBar(const filesystem::path& user_gradients_path,
                                       const filesystem::path& user_colors_path,
                                       const filesystem::path& effect_files_dir,
                                       const filesystem::path& user_curves_path,
                                       QWidget* parent)
    : QWidget(parent)
    , user_gradients_path_(user_gradients_path)
    , user_colors_path_(user_colors_path)
    , effect_files_dir_(effect_files_dir)
    , user_curves_path_(user_curves_path)
{
    buildUi();
}

QToolButton* ActionToolBar::addMenuButton(const QString& text, const QString& tooltip, QMenu* menu)
{
    auto* button = new QToolButton(this);
    button->setText(text);
    button->setToolTip(tooltip);
    button->setAccessibleName(text);
    button->setPopupMode(QToolButton::InstantPopup);
    // Styled by PluginTheme::ApplyFlatToolBar as a boxed menu button with room for the arrow.
    button->setProperty("pluginMenuButton", true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setAutoRaise(true);
    button->setFixedHeight(28);
    button->setMenu(menu);
    layout()->addWidget(button);
    return button;
}

void ActionToolBar::buildUi()
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    effects_menu_ = new QMenu(this);
    colors_menu_ = new QMenu(this);
    gradients_menu_ = new QMenu(this);
    curves_menu_ = new QMenu(this);
    for(QMenu* menu : {effects_menu_, colors_menu_, gradients_menu_, curves_menu_})
    {
        menu->setToolTipsVisible(true);
    }

    addMenuButton(QStringLiteral("Effects"),
                  QStringLiteral("Pick an effect, then click a row to place it (Shift+click places several, "
                                 "Esc cancels). Or drag it straight onto a row."),
                  effects_menu_);
    addMenuButton(QStringLiteral("Colours"),
                  QStringLiteral("Drag a colour onto the timeline, or click to colour the selected clip. "
                                 "Right-click a colour to change it."),
                  colors_menu_);
    addMenuButton(QStringLiteral("Gradients"),
                  QStringLiteral("Drag a gradient onto the timeline or a clip. Right-click for more."),
                  gradients_menu_);
    curves_button_ = addMenuButton(QStringLiteral("Curves"),
                                   QStringLiteral("Drag a brightness curve onto a clip."),
                                   curves_menu_);

    fillEffectsMenu();
    fillColorsMenu();
    fillGradientsMenu();
    fillCurvesMenu();
    setCurvesEnabled(false);
}

void ActionToolBar::fillEffectsMenu()
{
    effects_menu_->clear();
    AddHint(effects_menu_, QStringLiteral("Click, then click a row · or drag onto a row"));
    const QList<ActionCatalog::Entry> entries = ActionCatalog::LoadEntries(effect_files_dir_);
    for(const QString& section : ActionCatalog::SectionOrder(entries))
    {
        QMenu* sub = effects_menu_->addMenu(ActionCatalog::SectionLabel(section));
        sub->setToolTipsVisible(true);
        for(const ActionCatalog::Entry& e : ActionCatalog::EntriesFor(entries, section))
        {
            MenuItemRow* row = AddRow(sub, ActionCatalog::MakeEffectIcon(e, 16), QSize(16, 16),
                                      e.name.isEmpty() ? QStringLiteral("Effect") : e.name,
                                      ActionCatalog::EffectTooltip(e));
            const QString id = e.id;
            row->mime = [id]() { return ActionCatalog::MakeEffectMime(id); };
            row->activate = [this, id]() { emit effectClicked(id); };
        }
    }
}

void ActionToolBar::fillColorsMenu()
{
    colors_menu_->clear();
    auto swatches = std::make_shared<QVector<Swatch>>(LoadSwatches(user_colors_path_));
    const filesystem::path path = user_colors_path_;
    for(int i = 0; i < swatches->size(); ++i)
    {
        const Swatch& sw = swatches->at(i);
        MenuItemRow* row = AddRow(colors_menu_, SwatchIcon(sw.color), QSize(16, 16), sw.label,
                                  sw.color.name() + QStringLiteral(" — right-click to change"));
        row->mime = [swatches, i]() {
            const QColor c = swatches->at(i).color;
            return ActionCatalog::MakeColorMime(ToRGBColor(c.red(), c.green(), c.blue()));
        };
        row->activate = [this, swatches, i]() {
            const QColor c = swatches->at(i).color;
            emit colorClicked(ToRGBColor(c.red(), c.green(), c.blue()));
        };
        row->context = [swatches, i, path](MenuItemRow* source, const QPoint& pos) {
            QMenu menu(source);
            QAction* change = menu.addAction(QStringLiteral("Change colour…"));
            if(menu.exec(pos) != change)
            {
                return;
            }
            QColor picked = swatches->at(i).color;
            if(!EffectColorWheelDialog::pickColor(source, picked, QStringLiteral("Quick pick colour")))
            {
                return;
            }
            (*swatches)[i].color = picked;
            source->setIcon(SwatchIcon(picked));
            source->setToolTip(picked.name() + QStringLiteral(" — right-click to change"));
            SaveSwatches(path, *swatches);
        };
    }
}

void ActionToolBar::fillGradientsMenu()
{
    gradients_menu_->clear();
    for(const ActionUserGradients::Entry& entry : ActionUserGradients::Visible(user_gradients_path_))
    {
        const bool customized = ActionUserGradients::IsCustomized(user_gradients_path_, entry.id);
        const QPixmap preview = customized
            ? ActionUserGradients::Preview(entry.stops)
            : ActionCatalog::MakeGradientPreview(entry.id.toUtf8().constData(), 36, 14);
        MenuItemRow* row = AddRow(gradients_menu_, QIcon(preview), QSize(36, 14), entry.label,
                                  entry.label + QStringLiteral(" — drag onto the timeline or a clip, or click to apply "
                                                               "to the selected clip; right-click for more"));
        const QString id = entry.id;
        row->mime = [id]() { return ActionCatalog::MakeGradientPresetMime(id); };
        row->activate = [this, id]() { emit gradientPresetClicked(id); };
        row->context = [this, id, customized](MenuItemRow* source, const QPoint& pos) {
            QMenu menu(source);
            QAction* overwrite = menu.addAction(QStringLiteral("Replace with the selected clip's gradient"));
            QAction* remove = menu.addAction(QStringLiteral("Delete"));
            QAction* reset = nullptr;
            if(ActionUserGradients::IsBuiltin(id))
            {
                reset = menu.addAction(QStringLiteral("Reset to default"));
                reset->setEnabled(customized);
            }
            QAction* chosen = menu.exec(pos);
            if(chosen == overwrite)
            {
                emit gradientPresetOverwriteRequested(id);
            }
            else if(chosen == remove)
            {
                emit gradientPresetDeleteRequested(id);
            }
            else if(reset && chosen == reset)
            {
                emit gradientPresetResetRequested(id);
            }
        };
    }

    const std::vector<ActionUserGradients::Entry> hidden = ActionUserGradients::Hidden(user_gradients_path_);
    if(!hidden.empty())
    {
        gradients_menu_->addSeparator();
        QMenu* restore = gradients_menu_->addMenu(QStringLiteral("Removed presets"));
        for(const ActionUserGradients::Entry& entry : hidden)
        {
            const QString id = entry.id;
            restore->addAction(QStringLiteral("Restore %1").arg(entry.label), this,
                               [this, id]() { emit gradientPresetResetRequested(id); });
        }
    }
}

void ActionToolBar::fillCurvesMenu()
{
    curves_menu_->clear();
    const QColor ink = palette().color(QPalette::Text);
    for(const ActionUserCurves::Entry& c : ActionUserCurves::Load(user_curves_path_))
    {
        const QString label = c.label.isEmpty() ? c.id : c.label;
        MenuItemRow* row = AddRow(curves_menu_, CurveIcon(c.points, ink), QSize(36, 16), label,
                                  label + QStringLiteral(" — drag onto a clip, or click to apply to the selected clip"));
        const QString id = c.id;
        row->mime = [id]() { return ActionCatalog::MakeCurvePresetMime(id); };
        row->activate = [this, id]() { emit curvePresetClicked(id); };
    }
}

void ActionToolBar::reloadUserGradients()
{
    fillGradientsMenu();
}

void ActionToolBar::reloadUserCurves()
{
    fillCurvesMenu();
}

void ActionToolBar::setCurvesEnabled(bool enabled)
{
    if(curves_button_)
    {
        curves_button_->setEnabled(enabled);
        curves_button_->setToolTip(enabled ? QStringLiteral("Drag a brightness curve onto a clip.")
                                           : QStringLiteral("Add a clip first, then drag a curve onto it."));
    }
}
