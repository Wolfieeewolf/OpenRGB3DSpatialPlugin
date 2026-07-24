// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackToolBar.h"
#include "EffectPackCatalog.h"
#include "EffectPacks/EffectPack.h"

#include <QApplication>
#include <QDrag>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
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
        lab->setStyleSheet(QStringLiteral("color: #aaa; font-size: 11px;"));
        effects_row->addWidget(lab);
        for(const EffectPackCatalog::Entry& e : EffectPackCatalog::EntriesFor(cat))
        {
            auto* btn = new DragToolButton(this);
            btn->setAutoRaise(true);
            btn->setIcon(EffectPackCatalog::MakeEffectIcon(e));
            btn->setIconSize(QSize(22, 22));
            btn->setToolTip(QString::fromUtf8(e.name) + QStringLiteral(" — drag onto a timeline row"));
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
    effects_row->addStretch(1);
    root->addLayout(effects_row);

    auto* colors_row = new QHBoxLayout();
    colors_row->setSpacing(3);
    colors_row->addWidget(new QLabel(QStringLiteral("Colors")));
    const QColor solids[] = {
        QColor(255, 255, 255), QColor(255, 0, 0), QColor(0, 255, 0), QColor(0, 0, 255),
        QColor(255, 255, 0), QColor(255, 0, 255), QColor(0, 255, 255), QColor(255, 128, 0),
        QColor(128, 0, 255), QColor(0, 0, 0)
    };
    for(const QColor& c : solids)
    {
        auto* btn = MakeSwatchButton(this, c);
        const RGBColor rgb = ToRGBColor(c.red(), c.green(), c.blue());
        btn->setMimeFactory([rgb]() {
            return EffectPackCatalog::MakeColorMime(rgb);
        });
        btn->setToolTip(c.name() + QStringLiteral(" — drag onto an effect block"));
        connect(btn, &QToolButton::clicked, this, [this, rgb]() {
            emit colorClicked(rgb);
        });
        colors_row->addWidget(btn);
    }

    colors_row->addSpacing(10);
    colors_row->addWidget(new QLabel(QStringLiteral("Gradients")));
    const struct { const char* label; const char* id; QColor a; QColor b; } grads[] = {
        {"Rainbow", "rainbow", QColor(255, 0, 0), QColor(0, 0, 255)},
        {"Red→Blue", "red_blue", QColor(255, 0, 0), QColor(0, 0, 255)},
        {"White→Color", "white_color", QColor(255, 255, 255), QColor(255, 80, 40)},
    };
    for(const auto& g : grads)
    {
        auto* btn = new DragToolButton(this);
        btn->setAutoRaise(true);
        btn->setFixedSize(40, 24);
        QPixmap pm(34, 16);
        QPainter p(&pm);
        QLinearGradient grad(0, 0, 34, 0);
        if(QString(g.id) == QStringLiteral("rainbow"))
        {
            grad.setColorAt(0.0, QColor(255, 0, 0));
            grad.setColorAt(0.2, QColor(255, 128, 0));
            grad.setColorAt(0.4, QColor(255, 255, 0));
            grad.setColorAt(0.6, QColor(0, 255, 0));
            grad.setColorAt(0.8, QColor(0, 128, 255));
            grad.setColorAt(1.0, QColor(180, 0, 255));
        }
        else
        {
            grad.setColorAt(0.0, g.a);
            grad.setColorAt(1.0, g.b);
        }
        p.fillRect(pm.rect(), grad);
        btn->setIcon(QIcon(pm));
        btn->setIconSize(QSize(34, 16));
        btn->setToolTip(QString::fromUtf8(g.label) + QStringLiteral(" — drag onto an effect block"));
        const QString id = QString::fromUtf8(g.id);
        btn->setMimeFactory([id]() {
            return EffectPackCatalog::MakeGradientPresetMime(id);
        });
        connect(btn, &QToolButton::clicked, this, [this, id]() {
            emit gradientPresetClicked(id);
        });
        colors_row->addWidget(btn);
    }
    colors_row->addStretch(1);
    root->addLayout(colors_row);

    setStyleSheet(QStringLiteral(
        "EffectPackToolBar { background: #2a2a30; border: 1px solid #3a3a42; border-radius: 4px; }"
        "QLabel { color: #ddd; }"));
}
