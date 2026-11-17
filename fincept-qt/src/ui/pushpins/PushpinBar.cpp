#include "ui/pushpins/PushpinBar.h"
#include "ui/theme/Theme.h"

#include "core/symbol/SymbolDragSource.h"
#include "services/pushpins/PushpinService.h"
#include "ui/pushpins/SymbolChip.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>

namespace fincept::ui {

PushpinBar::PushpinBar(QWidget* parent) : QWidget(parent) {
    setObjectName("pushpinBar");
    metrics::follow_fixed_height(this, 30); // grows with the user's font
    setStyleSheet(QString("#pushpinBar{background:%1;border-bottom:1px solid %2;}")
                      .arg(colors::BG_SURFACE(), colors::BORDER_MED()));

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

    empty_hint_ = new QLabel(tr("Drag any symbol here to pin"), strip_);
    empty_hint_->setStyleSheet(QString("color:%1;font-size:" + fonts::px(-3) + "px;font-style:italic;")
                                   .arg(colors::TEXT_TERTIARY()));
    empty_hint_->setToolTip(empty_hint_->text()); // the strip may be narrower than the hint

    // Dropping a symbol anywhere on the bar pins it.
    symbol_dnd::installDropFilter(this, [](const SymbolRef& ref, SymbolGroup) { PushpinService::instance().pin(ref); });

    connect(&PushpinService::instance(), &PushpinService::pins_changed, this, &PushpinBar::rebuild);
    rebuild();
}

void PushpinBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PushpinBar::retranslateUi() {
    if (empty_hint_) {
        empty_hint_->setText(tr("Drag any symbol here to pin"));
        empty_hint_->setToolTip(empty_hint_->text()); // the strip may be narrower than the hint
    }
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
    // The scrollbar is for pins that do not fit. With none there is only the
    // hint, and where the top bar leaves the strip less room than the hint
    // needs (15 px font and up) a scrollbar under a cut sentence is noise:
    // the hint is simply clipped and its tooltip carries the full text.
    scroll_->setHorizontalScrollBarPolicy(pins.isEmpty() ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
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
