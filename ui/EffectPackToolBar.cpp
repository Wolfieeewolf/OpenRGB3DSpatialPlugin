// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackToolBar.h"
#include "EffectPackCatalog.h"
#include "EffectPacks/EffectPack.h"
#include "PluginUiUtils.h"

#include <QApplication>
#include <QDrag>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
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
        }
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if(!(event->buttons() & Qt::LeftButton) || !mime_factory_)
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
            return;
        }
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->setPixmap(icon().pixmap(22, 22));
        drag->exec(Qt::CopyAction);
    }

private:
    QPoint press_pos_;
    std::function<QMimeData*()> mime_factory_;
};

DragToolButton* MakeSwatchButton(QWidget* parent, const QColor& color, int size = 18)
{
    auto* btn = new DragToolButton(parent);
    btn->setAutoRaise(true);
    btn->setFixedSize(size + 6, size + 6);
    QPixmap pm(size, size);
    pm.fill(color);
    btn->setIcon(QIcon(pm));
    btn->setIconSize(QSize(size, size));
    btn->setToolTip(color.name());
    return btn;
}

} // namespace

EffectPackToolBar::EffectPackToolBar(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void EffectPackToolBar::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(2);

    auto* effects_row = new QHBoxLayout();
    effects_row->setSpacing(4);
    effects_row->addWidget(new QLabel(QStringLiteral("Effects")));

    auto add_category = [&](EffectPackCatalog::Category cat) {
        effects_row->addSpacing(8);
        auto* lab = new QLabel(EffectPackCatalog::CategoryLabel(cat));
        PluginUiApplyMutedSecondaryLabel(lab);
        effects_row->addWidget(lab);
        for(const EffectPackCatalog::Entry& e : EffectPackCatalog::EntriesFor(cat))
        {
            auto* btn = new DragToolButton(this);
            btn->setAutoRaise(true);
            btn->setIcon(EffectPackCatalog::MakeEffectIcon(e));
            btn->setIconSize(QSize(22, 22));
            btn->setToolTip(EffectPackCatalog::EffectTooltip(e));
            btn->setMimeFactory([type = e.type]() {
                return EffectPackCatalog::MakeEffectMime(type);
            });
            connect(btn, &QToolButton::clicked, this, [this, type = e.type]() {
                emit effectClicked((int)type);
            });
            effects_row->addWidget(btn);
        }
    };
    add_category(EffectPackCatalog::Category::Basic);
    add_category(EffectPackCatalog::Category::Pixel);
    add_category(EffectPackCatalog::Category::Volume);
    effects_row->addStretch(1);
    root->addLayout(effects_row);

    auto* colors_row = new QHBoxLayout();
    colors_row->setSpacing(3);
    colors_row->addWidget(new QLabel(QStringLiteral("Colors")));
    for(const EffectPackCatalog::ColorEntry& c : EffectPackCatalog::ColorEntries())
    {
        auto* btn = MakeSwatchButton(this, c.color);
        const RGBColor rgb = ToRGBColor(c.color.red(), c.color.green(), c.color.blue());
        btn->setMimeFactory([rgb]() {
            return EffectPackCatalog::MakeColorMime(rgb);
        });
        const QString label = QString::fromUtf8(c.label ? c.label : "Color");
        btn->setToolTip(label + QStringLiteral(" — ") + c.color.name()
                        + QStringLiteral(" — drag onto an effect block"));
        connect(btn, &QToolButton::clicked, this, [this, rgb]() {
            emit colorClicked(rgb);
        });
        colors_row->addWidget(btn);
    }

    colors_row->addSpacing(10);
    colors_row->addWidget(new QLabel(QStringLiteral("Gradients")));
    for(const EffectPackCatalog::GradientEntry& g : EffectPackCatalog::GradientEntries())
    {
        auto* btn = new DragToolButton(this);
        btn->setAutoRaise(true);
        btn->setFixedSize(40, 24);
        btn->setIcon(QIcon(EffectPackCatalog::MakeGradientPreview(g.id)));
        btn->setIconSize(QSize(34, 16));
        const QString label = QString::fromUtf8(g.label ? g.label : "Gradient");
        btn->setToolTip(label + QStringLiteral(" — drag onto an effect block"));
        const QString id = QString::fromUtf8(g.id ? g.id : "");
        btn->setMimeFactory([id]() {
            return EffectPackCatalog::MakeGradientPresetMime(id);
        });
        connect(btn, &QToolButton::clicked, this, [this, id]() {
            emit gradientPresetClicked(id);
        });
        colors_row->addWidget(btn);
    }
    colors_row->addSpacing(10);
    colors_row->addWidget(new QLabel(QStringLiteral("Curves")));
    for(const EffectPackCatalog::CurveEntry& c : EffectPackCatalog::CurveEntries())
    {
        auto* btn = new DragToolButton(this);
        btn->setAutoRaise(true);
        const QString label = QString::fromUtf8(c.label ? c.label : "Curve");
        btn->setText(label);
        btn->setToolTip(label + QStringLiteral(" intensity — drag onto a block"));
        const QString id = QString::fromUtf8(c.id ? c.id : "");
        btn->setMimeFactory([id]() {
            return EffectPackCatalog::MakeCurvePresetMime(id);
        });
        connect(btn, &QToolButton::clicked, this, [this, id]() {
            emit curvePresetClicked(id);
        });
        colors_row->addWidget(btn);
    }
    colors_row->addStretch(1);
    root->addLayout(colors_row);
}
