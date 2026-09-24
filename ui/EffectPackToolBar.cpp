// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackToolBar.h"
#include "EffectPackCatalog.h"
#include "EffectPackUserGradients.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QDrag>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPixmap>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

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

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if(dragged_)
        {
            dragged_ = false;
            setDown(false);
            event->accept();
            return;
        }
        QToolButton::mouseReleaseEvent(event);
    }

private:
    QPoint press_pos_;
    bool dragged_ = false;
    std::function<QMimeData*()> mime_factory_;
};

void StylePaletteButton(QToolButton* btn)
{
    btn->setAutoRaise(true);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCursor(Qt::PointingHandCursor);
}

QWidget* WrapPalette(QWidget* inner)
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(inner);
    scroll->setMinimumHeight(40);
    scroll->setMaximumHeight(52);
    return scroll;
}

QWidget* BuildEffectsPage(QWidget* owner, const std::function<void(EffectPack::BlockType)>& on_click)
{
    auto* page = new QWidget(owner);
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    auto* stack = new QStackedWidget(page);
    auto* cats = new QButtonGroup(page);
    cats->setExclusive(true);

    const EffectPackCatalog::Category order[] = {
        EffectPackCatalog::Category::Basic,
        EffectPackCatalog::Category::Pixel,
        EffectPackCatalog::Category::Volume,
    };

    for(EffectPackCatalog::Category cat : order)
    {
        auto* tab = new QToolButton(page);
        const QString full = EffectPackCatalog::CategoryLabel(cat);
        tab->setText(full.section(QLatin1Char(' '), 0, 0));
        tab->setToolTip(full);
        tab->setCheckable(true);
        tab->setAutoRaise(true);
        tab->setFocusPolicy(Qt::NoFocus);
        cats->addButton(tab);
        row->addWidget(tab);

        auto* icons = new QWidget();
        auto* icons_row = new QHBoxLayout(icons);
        icons_row->setContentsMargins(0, 0, 0, 0);
        icons_row->setSpacing(1);
        for(const EffectPackCatalog::Entry& e : EffectPackCatalog::EntriesFor(cat))
        {
            auto* btn = new DragToolButton(icons);
            StylePaletteButton(btn);
            btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            btn->setText(QString::fromUtf8(e.name ? e.name : "Effect"));
            btn->setIcon(EffectPackCatalog::MakeEffectIcon(e, 16));
            btn->setIconSize(QSize(16, 16));
            btn->setToolTip(EffectPackCatalog::EffectTooltip(e));
            btn->setMimeFactory([type = e.type]() {
                return EffectPackCatalog::MakeEffectMime(type);
            });
            QObject::connect(btn, &QToolButton::clicked, owner, [on_click, type = e.type]() {
                on_click(type);
            });
            icons_row->addWidget(btn);
        }
        icons_row->addStretch(1);
        auto* scroller = WrapPalette(icons);
        scroller->setMinimumHeight(32);
        scroller->setMaximumHeight(36);
        stack->addWidget(scroller);
        QObject::connect(tab, &QToolButton::clicked, stack, [stack, scroller]() {
            stack->setCurrentWidget(scroller);
        });
    }
    if(QAbstractButton* first = cats->buttons().value(0))
    {
        first->setChecked(true);
    }
    row->addWidget(stack, 1);
    return page;
}

QWidget* BuildColorsPage(QWidget* owner, const std::function<void(unsigned int)>& on_click)
{
    auto* page = new QWidget();
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(2, 2, 2, 2);
    row->setSpacing(4);
    for(const EffectPackCatalog::ColorEntry& c : EffectPackCatalog::ColorEntries())
    {
        auto* btn = new DragToolButton(page);
        StylePaletteButton(btn);
        btn->setFixedSize(26, 26);
        QPixmap pm(18, 18);
        pm.fill(c.color);
        btn->setIcon(QIcon(pm));
        btn->setIconSize(QSize(18, 18));
        const QString label = QString::fromUtf8(c.label ? c.label : "Color");
        btn->setToolTip(label + QStringLiteral(" — ") + c.color.name());
        const RGBColor rgb = ToRGBColor(c.color.red(), c.color.green(), c.color.blue());
        btn->setMimeFactory([rgb]() {
            return EffectPackCatalog::MakeColorMime(rgb);
        });
        QObject::connect(btn, &QToolButton::clicked, owner, [on_click, rgb]() {
            on_click(rgb);
        });
        row->addWidget(btn);
    }
    row->addStretch(1);
    return WrapPalette(page);
}

void AddGradientButton(QWidget* page, QHBoxLayout* row, QWidget* owner,
                      const QString& label, const QString& id, const QPixmap& preview,
                      const std::function<void(const QString&)>& on_click)
{
    auto* btn = new DragToolButton(page);
    StylePaletteButton(btn);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setText(label);
    btn->setIcon(QIcon(preview));
    btn->setIconSize(QSize(36, 14));
    btn->setToolTip(label + QStringLiteral(" — drag onto an effect block"));
    btn->setMimeFactory([id]() {
        return EffectPackCatalog::MakeGradientPresetMime(id);
    });
    QObject::connect(btn, &QToolButton::clicked, owner, [on_click, id]() {
        on_click(id);
    });
    row->addWidget(btn);
}

QWidget* BuildGradientsPage(QWidget* owner, const std::function<void(const QString&)>& on_click, const filesystem::path& user_path)
{
    auto* page = new QWidget();
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(2, 2, 2, 2);
    row->setSpacing(6);
    for(const EffectPackCatalog::GradientEntry& g : EffectPackCatalog::GradientEntries())
    {
        const QString label = QString::fromUtf8(g.label ? g.label : "Gradient");
        const QString id = QString::fromUtf8(g.id ? g.id : "");
        AddGradientButton(page, row, owner, label, id, EffectPackCatalog::MakeGradientPreview(g.id, 36, 14), on_click);
    }
    for(const EffectPackUserGradients::Entry& entry : EffectPackUserGradients::Load(user_path))
    {
        AddGradientButton(page, row, owner, entry.label, entry.id, EffectPackUserGradients::Preview(entry.stops), on_click);
    }
    row->addStretch(1);
    return WrapPalette(page);
}

QWidget* BuildCurvesPage(QWidget* owner, const std::function<void(const QString&)>& on_click)
{
    auto* page = new QWidget();
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(2, 2, 2, 2);
    row->setSpacing(4);
    for(const EffectPackCatalog::CurveEntry& c : EffectPackCatalog::CurveEntries())
    {
        auto* btn = new DragToolButton(page);
        StylePaletteButton(btn);
        const QString label = QString::fromUtf8(c.label ? c.label : "Curve");
        const QString id = QString::fromUtf8(c.id ? c.id : "");
        btn->setText(label);
        btn->setToolTip(label + QStringLiteral(" — drag onto a block"));
        btn->setMimeFactory([id]() {
            return EffectPackCatalog::MakeCurvePresetMime(id);
        });
        QObject::connect(btn, &QToolButton::clicked, owner, [on_click, id]() {
            on_click(id);
        });
        row->addWidget(btn);
    }
    row->addStretch(1);
    return WrapPalette(page);
}

} // namespace

EffectPackToolBar::EffectPackToolBar(const filesystem::path& user_gradients_path, QWidget* parent)
    : QWidget(parent)
    , user_gradients_path_(user_gradients_path)
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
         BuildEffectsPage(this, [this](EffectPack::BlockType type) { emit effectClicked((int)type); })},
        {"Colors", "Drag onto a block, or click to colour the selected block.",
         BuildColorsPage(this, [this](unsigned int rgb) { emit colorClicked(rgb); })},
        {"Gradients", "Drag onto a block, or click to apply to the selected block.",
         BuildGradientsPage(this, [this](const QString& id) { emit gradientPresetClicked(id); }, user_gradients_path_)},
        {"Curves", "Drag onto a block, or click to apply to the selected block.",
         BuildCurvesPage(this, [this](const QString& id) { emit curvePresetClicked(id); })},
    };

    for(const Page& page : defs)
    {
        if(QString::fromUtf8(page.title) == QStringLiteral("Gradients"))
        {
            gradients_page_index_ = tabs_->count();
        }
        const int index = tabs_->addTab(page.widget, QString::fromUtf8(page.title));
        tabs_->setTabToolTip(index, QString::fromUtf8(page.tip));
    }
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
        user_gradients_path_);
    tabs_->removeTab(gradients_page_index_);
    tabs_->insertTab(gradients_page_index_, replacement, QStringLiteral("Gradients"));
    tabs_->setTabToolTip(gradients_page_index_,
                         QStringLiteral("Drag onto a block, or click to apply to the selected block."));
    old->deleteLater();
    if(was_current)
    {
        tabs_->setCurrentIndex(gradients_page_index_);
    }
}
