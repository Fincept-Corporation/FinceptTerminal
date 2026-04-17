#include "screens/polymarket/PolymarketStatusBar.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;

PolymarketStatusBar::PolymarketStatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(22);
    setStyleSheet(QString("background: %1; border-top: 1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(12);

    auto label_style = QString("color: %1; font-size: 9px; background: transparent;").arg(colors::TEXT_SECONDARY());
    auto highlight_style = QString("color: %1; font-size: 9px; background: transparent;").arg(colors::CYAN());

    auto* brand = new QLabel("POLYMARKET");
    brand->setStyleSheet(label_style);
    hl->addWidget(brand);
    hl->addStretch(1);

    view_label_ = new QLabel("MARKETS");
    view_label_->setStyleSheet(label_style);
    hl->addWidget(view_label_);

    count_label_ = new QLabel;
    count_label_->setStyleSheet(label_style);
    hl->addWidget(count_label_);

    selected_label_ = new QLabel;
    selected_label_->setStyleSheet(highlight_style);
    hl->addWidget(selected_label_);

    ws_label_ = new QLabel;
    ws_label_->setStyleSheet(label_style);
    hl->addWidget(ws_label_);
    set_ws_status(false);
}

void PolymarketStatusBar::set_view(const QString& view) {
    view_label_->setText(view);
}

void PolymarketStatusBar::set_count(int count, const QString& label) {
    count_label_->setText(QString::number(count) + " " + label);
}

void PolymarketStatusBar::set_selected(const QString& question) {
    QString q = question.left(50);
    if (question.size() > 50)
        q += "...";
    selected_label_->setText(q);
}

void PolymarketStatusBar::set_ws_status(bool connected) {
    if (connected) {
        ws_label_->setText("WS: LIVE");
        ws_label_->setStyleSheet(
            QString("color: %1; font-size: 9px; font-weight: 700; background: transparent;").arg(colors::POSITIVE()));
    } else {
        ws_label_->setText("WS: OFF");
        ws_label_->setStyleSheet(QString("color: %1; font-size: 9px; background: transparent;").arg(colors::TEXT_DIM()));
    }
}

} // namespace fincept::screens::polymarket
