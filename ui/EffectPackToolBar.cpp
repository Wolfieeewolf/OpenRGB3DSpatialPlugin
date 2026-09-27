// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackToolBar.h"
#include "EffectPackCatalog.h"
#include "EffectPackUserCurves.h"
#include "EffectPackUserGradients.h"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QContextMenuEvent>
#include <QColorDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDrag>
#include <QFrame>
#include <QIcon>
#include <QLayout>
#include <QLayoutItem>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>
#include <memory>
#include <system_error>

namespace
{

class DragToolButton : public QToolButton
{
public:
    using QToolButton::QToolButton;

    void setMimeFactory(std::function<QMimeData*()> factory)
    {
        mime_factory_ = std::move(factory);
    }

    void setDoubleClicked(std::function<void()> fn)
    {
        double_clicked_ = std::move(fn);
    }

    void setContextMenu(std::function<void(const QPoint&)> fn)
    {
        context_menu_ = std::move(fn);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if(event->button() == Qt::LeftButton)
        {
            press_pos_ = event->position().toPoint();
            dragged_ = false;
        }
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if(!(event->buttons() & Qt::LeftButton) || !mime_factory_ || dragged_)
        {
            QToolButton::mouseMoveEvent(event);
            return;
        }
        if((event->position().toPoint() - press_pos_).manhattanLength() < QApplication::startDragDistance())
        {
            QToolButton::mouseMoveEvent(event);
            return;
        }
        QMimeData* mime = mime_factory_();
        if(!mime)
        {
            QToolButton::mouseMoveEvent(event);
            return;
        }
        dragged_ = true;
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        const QPixmap ghost = icon().isNull() ? grab() : icon().pixmap(22, 22);
        drag->setPixmap(ghost);
        drag->exec(Qt::CopyAction);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if(event->button() == Qt::LeftButton && double_clicked_)
        {
            pending_click_ = false;
            dragged_ = true;
            double_clicked_();
            event->accept();
            return;
        }
        QToolButton::mouseDoubleClickEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if(dragged_)
        {
            dragged_ = false;
            setDown(false);
            event->accept();
            return;
        }
        if(double_clicked_ && event->button() == Qt::LeftButton)
        {
            pending_click_ = true;
            QTimer::singleShot(QApplication::doubleClickInterval(), this, [this]() {
                if(!pending_click_)
                {
                    return;
                }
                pending_click_ = false;
                click();
            });
            setDown(false);
            event->accept();
            return;
        }
        QToolButton::mouseReleaseEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        if(context_menu_)
        {
            context_menu_(event->globalPos());
            event->accept();
            return;
        }
        QToolButton::contextMenuEvent(event);
    }

private:
    QPoint press_pos_;
    bool dragged_ = false;
    bool pending_click_ = false;
    std::function<QMimeData*()> mime_factory_;
    std::function<void()> double_clicked_;
    std::function<void(const QPoint&)> context_menu_;
};

void StylePaletteButton(QToolButton* btn)
{
    btn->setAutoRaise(true);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCursor(Qt::PointingHandCursor);
}

/** Left-to-right wrap layout (no horizontal scrollbar). */
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = 0, int h_spacing = 4, int v_spacing = 4)
        : QLayout(parent)
        , h_space_(h_spacing)
        , v_space_(v_spacing)
    {
        setContentsMargins(margin, margin, margin, margin);
    }

    ~FlowLayout() override
    {
        while(QLayoutItem* item = takeAt(0))
        {
            delete item;
        }
    }

    void addItem(QLayoutItem* item) override { items_.append(item); }
    int count() const override { return items_.size(); }
    QLayoutItem* itemAt(int index) const override
    {
        return (index >= 0 && index < items_.size()) ? items_.at(index) : nullptr;
    }
    QLayoutItem* takeAt(int index) override
    {
        return (index >= 0 && index < items_.size()) ? items_.takeAt(index) : nullptr;
    }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }
    void setGeometry(const QRect& rect) override
    {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override
    {
        QSize size;
        for(const QLayoutItem* item : items_)
        {
            size = size.expandedTo(item->minimumSize());
        }
        const QMargins m = contentsMargins();
        size += QSize(m.left() + m.right(), m.top() + m.bottom());
        return size;
    }

private:
    int horizontalSpacing() const
    {
        if(h_space_ >= 0)
        {
            return h_space_;
        }
        if(const QWidget* w = parentWidget())
        {
            return w->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
        }
        return 4;
    }

    int verticalSpacing() const
    {
        if(v_space_ >= 0)
        {
            return v_space_;
        }
        if(const QWidget* w = parentWidget())
        {
            return w->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
        }
        return 4;
    }

    int doLayout(const QRect& rect, bool test_only) const
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        getContentsMargins(&left, &top, &right, &bottom);
        const QRect effective = rect.adjusted(left, top, -right, -bottom);
        int x = effective.x();
        int y = effective.y();
        int line_height = 0;
        const int space_x = horizontalSpacing();
        const int space_y = verticalSpacing();

        for(QLayoutItem* item : items_)
        {
            QWidget* widget = item->widget();
            if(widget && widget->isHidden())
            {
                continue;
            }
            const QSize hint = item->sizeHint();
            int next_x = x + hint.width() + space_x;
            if(x > effective.x() && next_x - space_x > effective.right() + 1)
            {
                x = effective.x();
                y = y + line_height + space_y;
                next_x = x + hint.width() + space_x;
                line_height = 0;
            }
            if(!test_only)
            {
                item->setGeometry(QRect(QPoint(x, y), hint));
            }
            x = next_x;
            line_height = std::max(line_height, hint.height());
        }
        return y + line_height - rect.y() + bottom;
    }

    QList<QLayoutItem*> items_;
    int h_space_ = -1;
    int v_space_ = -1;
};

QWidget* BuildEffectsPage(QWidget* owner, const filesystem::path& effect_dir, const std::function<void(const QString&)>& on_click)
{
    auto* page = new QWidget(owner);
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);

    auto* cats_host = new QWidget(page);
    auto* cats_flow = new FlowLayout(cats_host, 0, 6, 2);
    auto* stack = new QStackedWidget(page);
    auto* cats = new QButtonGroup(page);
    cats->setExclusive(true);

    const QList<EffectPackCatalog::Entry> entries = EffectPackCatalog::LoadEntries(effect_dir);
    for(const QString& section : EffectPackCatalog::SectionOrder(entries))
    {
        auto* tab = new QToolButton(cats_host);
        const QString full = EffectPackCatalog::SectionLabel(section);
        tab->setText(full);
        tab->setToolTip(full);
        tab->setCheckable(true);
        tab->setAutoRaise(true);
        tab->setFocusPolicy(Qt::NoFocus);
        cats->addButton(tab);
        cats_flow->addWidget(tab);

        auto* icons = new QWidget();
        auto* icons_flow = new FlowLayout(icons, 0, 4, 4);
        for(const EffectPackCatalog::Entry& e : EffectPackCatalog::EntriesFor(entries, section))
        {
            auto* btn = new DragToolButton(icons);
            StylePaletteButton(btn);
            btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            btn->setText(e.name.isEmpty() ? QStringLiteral("Effect") : e.name);
            btn->setIcon(EffectPackCatalog::MakeEffectIcon(e, 16));
            btn->setIconSize(QSize(16, 16));
            btn->setToolTip(EffectPackCatalog::EffectTooltip(e));
            btn->setMimeFactory([id = e.id]() {
                return EffectPackCatalog::MakeEffectMime(id);
            });
            QObject::connect(btn, &QToolButton::clicked, owner, [on_click, id = e.id]() {
                on_click(id);
            });
            icons_flow->addWidget(btn);
        }
        stack->addWidget(icons);
        QObject::connect(tab, &QToolButton::clicked, stack, [stack, icons]() {
            stack->setCurrentWidget(icons);
        });
    }
    if(QAbstractButton* first = cats->buttons().value(0))
    {
        first->setChecked(true);
    }
    outer->addWidget(cats_host);
    outer->addWidget(stack, 1);
    return page;
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
    for(const EffectPackCatalog::ColorEntry& c : EffectPackCatalog::ColorEntries())
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

void PaintSwatch(DragToolButton* btn, const Swatch& swatch)
{
    QPixmap pm(18, 18);
    pm.fill(swatch.color);
    btn->setIcon(QIcon(pm));
    btn->setToolTip(swatch.label + QStringLiteral(" — ") + swatch.color.name()
                    + QStringLiteral(" — double-click to change"));
}

QWidget* BuildColorsPage(QWidget* owner, const filesystem::path& path, const std::function<void(unsigned int)>& on_click)
{
    auto* page = new QWidget();
    auto* flow = new FlowLayout(page, 2, 4, 4);
    auto swatches = std::make_shared<QVector<Swatch>>(LoadSwatches(path));
    for(int i = 0; i < swatches->size(); ++i)
    {
        auto* btn = new DragToolButton(page);
        StylePaletteButton(btn);
        btn->setFixedSize(26, 26);
        btn->setIconSize(QSize(18, 18));
        PaintSwatch(btn, swatches->at(i));
        btn->setMimeFactory([swatches, i]() {
            const QColor c = swatches->at(i).color;
            return EffectPackCatalog::MakeColorMime(ToRGBColor(c.red(), c.green(), c.blue()));
        });
        QObject::connect(btn, &QToolButton::clicked, owner, [on_click, swatches, i]() {
            const QColor c = swatches->at(i).color;
            on_click(ToRGBColor(c.red(), c.green(), c.blue()));
        });
        btn->setDoubleClicked([btn, swatches, i, path]() {
            const QColor picked = QColorDialog::getColor(swatches->at(i).color, btn, QStringLiteral("Quick pick color"));
            if(!picked.isValid())
            {
                return;
            }
            (*swatches)[i].color = picked;
            PaintSwatch(btn, swatches->at(i));
            SaveSwatches(path, *swatches);
        });
        flow->addWidget(btn);
    }
    return page;
}

void AddGradientButton(QWidget* page, QLayout* row, QWidget* owner,
                      const QString& label, const QString& id, const QPixmap& preview,
                      const std::function<void(const QString&)>& on_click,
                      const std::function<void(const QString&)>& on_overwrite,
                      const std::function<void(const QString&)>& on_delete,
                      const std::function<void(const QString&)>& on_reset,
                      bool customized)
{
    auto* btn = new DragToolButton(page);
    StylePaletteButton(btn);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setText(label);
    btn->setIcon(QIcon(preview));
    btn->setIconSize(QSize(36, 14));
    btn->setToolTip(label + QStringLiteral(" — drag onto the timeline. Double-click to replace with the selected gradient. Right-click to delete."));
    btn->setMimeFactory([id]() {
        return EffectPackCatalog::MakeGradientPresetMime(id);
    });
    QObject::connect(btn, &QToolButton::clicked, owner, [on_click, id]() {
        on_click(id);
    });
    btn->setDoubleClicked([on_overwrite, id]() {
        if(on_overwrite)
        {
            on_overwrite(id);
        }
    });
    btn->setContextMenu([btn, id, on_delete, on_reset, customized](const QPoint& pos) {
        QMenu menu(btn);
        QAction* remove = menu.addAction(QStringLiteral("Delete"));
        QAction* reset = nullptr;
        if(EffectPackUserGradients::IsBuiltin(id))
        {
            reset = menu.addAction(QStringLiteral("Reset to default"));
            reset->setEnabled(customized);
        }
        QAction* chosen = menu.exec(pos);
        if(chosen == remove && on_delete)
        {
            on_delete(id);
        }
        else if(reset && chosen == reset && on_reset)
        {
            on_reset(id);
        }
    });
    row->addWidget(btn);
}

QWidget* BuildGradientsPage(QWidget* owner, const std::function<void(const QString&)>& on_click,
                            const std::function<void(const QString&)>& on_overwrite,
                            const std::function<void(const QString&)>& on_delete,
                            const std::function<void(const QString&)>& on_reset,
                            const filesystem::path& user_path)
{
    auto* page = new QWidget();
    auto* flow = new FlowLayout(page, 2, 6, 4);
    for(const EffectPackUserGradients::Entry& entry : EffectPackUserGradients::Visible(user_path))
    {
        const bool customized = EffectPackUserGradients::IsCustomized(user_path, entry.id);
        const QPixmap preview = customized
            ? EffectPackUserGradients::Preview(entry.stops)
            : EffectPackCatalog::MakeGradientPreview(entry.id.toUtf8().constData(), 36, 14);
        AddGradientButton(page, flow, owner, entry.label, entry.id, preview, on_click, on_overwrite, on_delete, on_reset, customized);
    }
    for(const EffectPackUserGradients::Entry& entry : EffectPackUserGradients::Hidden(user_path))
    {
        auto* btn = new DragToolButton(page);
        StylePaletteButton(btn);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setText(entry.label);
        btn->setToolTip(entry.label + QStringLiteral(" was removed. Right-click to restore the default."));
        btn->setContextMenu([on_reset, id = entry.id](const QPoint&) {
            if(on_reset)
            {
                on_reset(id);
            }
        });
        flow->addWidget(btn);
    }
    return page;
}

QWidget* BuildCurvesPage(QWidget* owner, const filesystem::path& curves_path, const std::function<void(const QString&)>& on_click)
{
    auto* page = new QWidget();
    auto* flow = new FlowLayout(page, 2, 4, 4);
    for(const EffectPackUserCurves::Entry& c : EffectPackUserCurves::Load(curves_path))
    {
        auto* btn = new DragToolButton(page);
        StylePaletteButton(btn);
        const QString label = c.label.isEmpty() ? c.id : c.label;
        const QString id = c.id;
        btn->setText(label);
        btn->setToolTip(label + QStringLiteral(" — drag onto a block"));
        btn->setMimeFactory([id]() {
            return EffectPackCatalog::MakeCurvePresetMime(id);
        });
        QObject::connect(btn, &QToolButton::clicked, owner, [on_click, id]() {
            on_click(id);
        });
        flow->addWidget(btn);
    }
    return page;
}

} // namespace

EffectPackToolBar::EffectPackToolBar(const filesystem::path& user_gradients_path,
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

void EffectPackToolBar::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    outer->addWidget(tabs_);

    struct Page
    {
        const char* title;
        const char* tip;
        QWidget* widget;
    };

    const Page defs[] = {
        {"Effects", "Drag onto a timeline row, or click to add at the playhead.",
         BuildEffectsPage(this, effect_files_dir_, [this](const QString& id) { emit effectClicked(id); })},
        {"Colors", "Drag onto the timeline. Double-click a swatch to change that quick pick.",
         BuildColorsPage(this, user_colors_path_, [this](unsigned int rgb) { emit colorClicked(rgb); })},
        {"Gradients", "Drag onto the timeline. Double-click to replace that preset with the selected gradient. Right-click to delete or reset.",
         BuildGradientsPage(this,
                            [this](const QString& id) { emit gradientPresetClicked(id); },
                            [this](const QString& id) { emit gradientPresetOverwriteRequested(id); },
                            [this](const QString& id) { emit gradientPresetDeleteRequested(id); },
                            [this](const QString& id) { emit gradientPresetResetRequested(id); },
                            user_gradients_path_)},
        {"Curves", "Drag onto a block once the timeline has one.",
         BuildCurvesPage(this, user_curves_path_, [this](const QString& id) { emit curvePresetClicked(id); })},
    };

    for(const Page& page : defs)
    {
        if(QString::fromUtf8(page.title) == QStringLiteral("Gradients"))
        {
            gradients_page_index_ = tabs_->count();
        }
        if(QString::fromUtf8(page.title) == QStringLiteral("Curves"))
        {
            curves_page_index_ = tabs_->count();
        }
        const int index = tabs_->addTab(page.widget, QString::fromUtf8(page.title));
        tabs_->setTabToolTip(index, QString::fromUtf8(page.tip));
    }
    setCurvesEnabled(false);
}

void EffectPackToolBar::reloadUserGradients()
{
    if(!tabs_ || gradients_page_index_ < 0 || gradients_page_index_ >= tabs_->count())
    {
        return;
    }
    QWidget* old = tabs_->widget(gradients_page_index_);
    const bool was_current = tabs_->currentIndex() == gradients_page_index_;
    QWidget* replacement = BuildGradientsPage(
        this,
        [this](const QString& id) { emit gradientPresetClicked(id); },
        [this](const QString& id) { emit gradientPresetOverwriteRequested(id); },
        [this](const QString& id) { emit gradientPresetDeleteRequested(id); },
        [this](const QString& id) { emit gradientPresetResetRequested(id); },
        user_gradients_path_);
    tabs_->removeTab(gradients_page_index_);
    tabs_->insertTab(gradients_page_index_, replacement, QStringLiteral("Gradients"));
    tabs_->setTabToolTip(gradients_page_index_,
                         QStringLiteral("Drag onto the timeline. Double-click to replace that preset with the selected gradient. Right-click to delete or reset."));
    old->deleteLater();
    if(was_current)
    {
        tabs_->setCurrentIndex(gradients_page_index_);
    }
}

void EffectPackToolBar::setCurvesEnabled(bool enabled)
{
    if(!tabs_ || curves_page_index_ < 0 || curves_page_index_ >= tabs_->count())
    {
        return;
    }
    tabs_->setTabEnabled(curves_page_index_, enabled);
    if(QWidget* page = tabs_->widget(curves_page_index_))
    {
        page->setEnabled(enabled);
    }
}
