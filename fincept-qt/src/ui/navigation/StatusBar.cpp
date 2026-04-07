#include "ui/navigation/StatusBar.h"

#include "ui/theme/Theme.h"

namespace fincept::ui {

StatusBar::StatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(26);
    setStyleSheet(QString("background:%1;border-top:1px solid %2;")
                      .arg(colors::BG_BASE).arg(colors::BORDER_DIM));
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    auto mk = [](const QString& t, const QString& c, int sz = 12, bool b = false) {
        auto* l = new QLabel(t);
        l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;"
                                 "font-family:'Consolas',monospace;")
                             .arg(c)
                             .arg(sz)
                             .arg(b ? "font-weight:700;" : ""));
        return l;
    };

    hl->addWidget(mk("v4.0.0", colors::TEXT_DIM.get()));
    hl->addWidget(mk("  |  ", colors::BORDER_DIM.get()));
    const char* feeds[] = {"EQ", "FX", "CM", "FI", "CR"};
    for (auto& f : feeds) {
        hl->addWidget(mk(f, colors::TEXT_DIM.get()));
        hl->addWidget(mk(" ", colors::BG_BASE.get()));
    }
    hl->addStretch();
    ready_label_ = mk("READY", colors::POSITIVE.get(), 8, true);
    hl->addWidget(ready_label_);
}

void StatusBar::set_ready(bool ready) {
    ready_label_->setText(ready ? "READY" : "BUSY");
    ready_label_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
                                        "font-family:'Consolas',monospace;")
                                    .arg(ready ? colors::POSITIVE.get() : colors::TEXT_TERTIARY.get()));
}

} // namespace fincept::ui
