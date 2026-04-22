#include "ui/pushpins/PushpinBar.h"

#include "core/symbol/SymbolDragSource.h"
#include "services/pushpins/PushpinService.h"
#include "ui/pushpins/SymbolChip.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>

namespace fincept::ui {

PushpinBar::PushpinBar(QWidget* parent) : QWidget(parent) {
    setObjectName("pushpinBar");
    setFixedHeight(30);
    setStyleSheet("#pushpinBar{background:#111827;border-bottom:1px solid #374151;}");

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(6, 2, 6, 2);
    outer->setSpacing(0);

    scroll_ = new QScrollArea(this);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidgetResizable(true);
    scroll_->setStyleSheet("QScrollArea{background:transparent;}"
                           "QScrollBar:horizontal{height:6px;}");
    outer->addWidget(scroll_, 1);

    strip_ = new QWidget;
    strip_->setObjectName("pushpinStrip");
    strip_->setStyleSheet("#pushpinStrip{background:transparent;}");
    strip_layout_ = new QHBoxLayout(strip_);
    strip_layout_->setContentsMargins(0, 0, 0, 0);
    strip_layout_->setSpacing(4);
    strip_layout_->addStretch(1);
    scroll_->setWidget(strip_);

    empty_hint_ = new QLabel("Drag any symbol here to pin", strip_);
    empty_hint_->setStyleSheet("color:#6b7280;font-size:11px;font-style:italic;");

    // Dropping a symbol anywhere on the bar pins it.
    symbol_dnd::installDropFilter(this, [](const SymbolRef& ref, SymbolGroup) {
        PushpinService::instance().pin(ref);
    });

    connect(&PushpinService::instance(), &PushpinService::pins_changed, this, &PushpinBar::rebuild);
    rebuild();
}

void PushpinBar::rebuild() {
    // Tear down existing chip widgets (keep the trailing stretch + hint).
    QLayoutItem* item;
    while ((item = strip_layout_->takeAt(0)) != nullptr) {
        if (auto* w = item->widget()) {
            if (w != empty_hint_)
                w->deleteLater();
        }
        delete item;
    }

    const auto pins = PushpinService::instance().pins();
    if (pins.isEmpty()) {
        strip_layout_->addWidget(empty_hint_);
        empty_hint_->show();
    } else {
        empty_hint_->hide();
        for (const SymbolRef& p : pins) {
            auto* chip = new SymbolChip(p, strip_);
            strip_layout_->addWidget(chip);
        }
    }
    strip_layout_->addStretch(1);
}

} // namespace fincept::ui
