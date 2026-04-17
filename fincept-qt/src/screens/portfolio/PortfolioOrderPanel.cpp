// src/screens/portfolio/PortfolioOrderPanel.cpp
#include "screens/portfolio/PortfolioOrderPanel.h"

#include "ui/theme/Theme.h"

#include <QVBoxLayout>

namespace fincept::screens {

PortfolioOrderPanel::PortfolioOrderPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(200);
    build_ui();
}

void PortfolioOrderPanel::build_ui() {
    setStyleSheet(
        QString("background:%1; border-left:2px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::POSITIVE()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(8);

    // Header + close
    auto* header_row = new QHBoxLayout;
    auto* title = new QLabel("ORDER ENTRY");
    title->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;").arg(ui::colors::POSITIVE()));
    header_row->addWidget(title);
    header_row->addStretch();

    close_btn_ = new QPushButton("\u2715");
    close_btn_->setFixedSize(18, 18);
    close_btn_->setCursor(Qt::PointingHandCursor);
    close_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none; font-size:11px; }"
                                      "QPushButton:hover { color:%2; }")
                                  .arg(ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_PRIMARY()));
    connect(close_btn_, &QPushButton::clicked, this, &PortfolioOrderPanel::close_requested);
    header_row->addWidget(close_btn_);

    layout->addLayout(header_row);

    // Side toggle: BUY | SELL
    auto* side_row = new QHBoxLayout;
    side_row->setSpacing(0);

    buy_tab_ = new QPushButton("BUY");
    buy_tab_->setFixedHeight(26);
    buy_tab_->setCursor(Qt::PointingHandCursor);
    buy_tab_->setCheckable(true);
    buy_tab_->setChecked(true);
    side_row->addWidget(buy_tab_);

    sell_tab_ = new QPushButton("SELL");
    sell_tab_->setFixedHeight(26);
    sell_tab_->setCursor(Qt::PointingHandCursor);
    sell_tab_->setCheckable(true);
    side_row->addWidget(sell_tab_);

    auto update_tabs = [this]() {
        bool is_buy = (side_ == "BUY");
        const char* active_color = is_buy ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        const char* inactive_color = ui::colors::TEXT_TERTIARY;

        buy_tab_->setChecked(is_buy);
        sell_tab_->setChecked(!is_buy);

        buy_tab_->setStyleSheet(
            QString("QPushButton { background:%1; color:%2; border:none;"
                    "  font-size:10px; font-weight:700; }")
                .arg(is_buy ? ui::colors::POSITIVE() : ui::colors::BG_RAISED(), is_buy ? "#000" : inactive_color));
        sell_tab_->setStyleSheet(
            QString("QPushButton { background:%1; color:%2; border:none;"
                    "  font-size:10px; font-weight:700; }")
                .arg(!is_buy ? ui::colors::NEGATIVE() : ui::colors::BG_RAISED(), !is_buy ? "#fff" : inactive_color));

        // Update header and border color
        setStyleSheet(QString("background:%1; border-left:2px solid %2;").arg(ui::colors::BG_SURFACE(), active_color));
        auto* title_w = findChild<QLabel*>();
        if (title_w && title_w->text() == "ORDER ENTRY") {
            // skip — the title might have been found differently
        }
    };

    connect(buy_tab_, &QPushButton::clicked, this, [this, update_tabs]() {
        side_ = "BUY";
        update_tabs();
    });
    connect(sell_tab_, &QPushButton::clicked, this, [this, update_tabs]() {
        side_ = "SELL";
        update_tabs();
    });

    layout->addLayout(side_row);

    // Selected holding info
    symbol_label_ = new QLabel;
    symbol_label_->setStyleSheet(QString("color:%1; font-size:16px; font-weight:700;").arg(ui::colors::CYAN()));
    layout->addWidget(symbol_label_);

    auto add_info = [&](QLabel*& lbl, const QString& prefix) {
        auto* row = new QHBoxLayout;
        auto* lab = new QLabel(prefix);
        lab->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
        row->addWidget(lab);
        lbl = new QLabel("--");
        lbl->setAlignment(Qt::AlignRight);
        lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:600;").arg(ui::colors::TEXT_PRIMARY()));
        row->addWidget(lbl);
        layout->addLayout(row);
    };

    add_info(price_label_, "PRICE");
    add_info(qty_label_, "QTY HELD");
    add_info(mv_label_, "MKT VAL");

    // Submit button
    submit_btn_ = new QPushButton("OPEN BUY ORDER");
    submit_btn_->setFixedHeight(32);
    submit_btn_->setCursor(Qt::PointingHandCursor);
    submit_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                       "  font-size:10px; font-weight:700; letter-spacing:0.5px; }"
                                       "QPushButton:hover { opacity:0.9; }")
                                   .arg(ui::colors::POSITIVE(), ui::colors::BG_BASE()));
    connect(submit_btn_, &QPushButton::clicked, this, [this]() {
        if (side_ == "BUY")
            emit buy_submitted();
        else
            emit sell_submitted();
    });
    layout->addWidget(submit_btn_);

    layout->addStretch();

    // Footer note
    auto* note = new QLabel("Orders are recorded\nin your portfolio");
    note->setWordWrap(true);
    note->setAlignment(Qt::AlignCenter);
    note->setStyleSheet(QString("color:%1; font-size:8px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(note);

    update_tabs();
}

void PortfolioOrderPanel::set_holding(const portfolio::HoldingWithQuote* holding) {
    holding_ = holding;
    update_display();
}

void PortfolioOrderPanel::set_currency(const QString& currency) {
    currency_ = currency;
}

void PortfolioOrderPanel::set_side(const QString& side) {
    side_ = side;
    // Trigger tab update
    buy_tab_->setChecked(side == "BUY");
    sell_tab_->setChecked(side == "SELL");
    buy_tab_->click(); // Will trigger the lambda and update styling
    if (side == "SELL")
        sell_tab_->click();
}

void PortfolioOrderPanel::update_display() {
    if (!holding_) {
        symbol_label_->setText("--");
        price_label_->setText("--");
        qty_label_->setText("--");
        mv_label_->setText("--");
        return;
    }

    symbol_label_->setText(holding_->symbol);
    price_label_->setText(QString::number(holding_->current_price, 'f', 2));
    qty_label_->setText(
        QString::number(holding_->quantity, 'f', holding_->quantity == std::floor(holding_->quantity) ? 0 : 2));
    mv_label_->setText(QString("%1 %2").arg(currency_).arg(QString::number(holding_->market_value, 'f', 2)));

    // Update submit button text
    submit_btn_->setText(QString("OPEN %1 ORDER").arg(side_));
    const char* btn_color = (side_ == "BUY") ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
    const char* btn_text_color = (side_ == "BUY") ? "#000" : "#fff";
    submit_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                       "  font-size:10px; font-weight:700; letter-spacing:0.5px; }"
                                       "QPushButton:hover { opacity:0.9; }")
                                   .arg(btn_color, btn_text_color));
}

} // namespace fincept::screens
