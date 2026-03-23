#include "ui/navigation/StatusBar.h"

#include "ui/theme/Theme.h"

namespace fincept::ui {

StatusBar::StatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(26);
    setStyleSheet("background:#0a0a0a;border-top:1px solid #1a1a1a;");
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

    hl->addWidget(mk("v4.0.0", "#333333"));
    hl->addWidget(mk("  |  ", "#1a1a1a"));
    const char* feeds[] = {"EQ", "FX", "CM", "FI", "CR"};
    for (auto& f : feeds) {
        hl->addWidget(mk(f, "#333333"));
        hl->addWidget(mk(" ", "#000"));
    }
    hl->addStretch();
    ready_label_ = mk("READY", "#16a34a", 8, true);
    hl->addWidget(ready_label_);
}

void StatusBar::set_ready(bool ready) {
    ready_label_->setText(ready ? "READY" : "BUSY");
    ready_label_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;"
                                        "font-family:'Consolas',monospace;")
                                    .arg(ready ? "#16a34a" : "#525252"));
}

} // namespace fincept::ui
