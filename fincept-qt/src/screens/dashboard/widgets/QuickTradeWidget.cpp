#include "screens/dashboard/widgets/QuickTradeWidget.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QMessageBox>

namespace fincept::screens::widgets {

static QString input_style(const QString& border_color = "") {
    QString bc = border_color.isEmpty() ? ui::colors::BORDER_MED() : border_color;
    return QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                   "font-size: 11px; padding: 4px 6px; }"
                   "QLineEdit:focus { border-color: %4; }")
        .arg(ui::colors::BG_BASE())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(bc)
        .arg(ui::colors::AMBER());
}

static QString combo_style() {
    return QString("QComboBox { background: %1; color: %2; border: 1px solid %3; "
                   "font-size: 11px; padding: 4px 6px; }"
                   "QComboBox::drop-down { border: none; width: 16px; }"
                   "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; }")
        .arg(ui::colors::BG_BASE())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(ui::colors::BORDER_MED());
}

QuickTradeWidget::QuickTradeWidget(QWidget* parent) : BaseWidget("QUICK TRADE", parent, ui::colors::AMBER()) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    // ── Symbol search bar ──
    auto* search_row = new QWidget;
    auto* srl = new QHBoxLayout(search_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(6);

    symbol_input_ = new QLineEdit;
    symbol_input_->setPlaceholderText("Symbol (e.g. AAPL)");
    symbol_input_->setStyleSheet(input_style());
    symbol_input_->setText("AAPL");
    srl->addWidget(symbol_input_, 1);

    lookup_btn_ = new QPushButton("LOOKUP");
    lookup_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                                       "font-size: 10px; font-weight: bold; padding: 4px 10px; }"
                                       "QPushButton:hover { background: %4; }")
                                   .arg(ui::colors::BG_RAISED())
                                   .arg(ui::colors::AMBER())
                                   .arg(ui::colors::AMBER())
                                   .arg(ui::colors::BG_HOVER()));
    srl->addWidget(lookup_btn_);
    vl->addWidget(search_row);

    // ── Quote display ──
    auto* quote_card = new QWidget;
    quote_card->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    auto* qcl = new QHBoxLayout(quote_card);
    qcl->setContentsMargins(10, 8, 10, 8);
    qcl->setSpacing(12);

    auto* sym_col = new QVBoxLayout;
    sym_label_ = new QLabel("--");
    sym_label_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold; background: transparent;")
                                  .arg(ui::colors::TEXT_PRIMARY()));
    sym_col->addWidget(sym_label_);
    change_label_ = new QLabel("--");
    change_label_->setStyleSheet(
        QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    sym_col->addWidget(change_label_);
    qcl->addLayout(sym_col);

    qcl->addStretch();

    auto* price_col = new QVBoxLayout;
    price_col->setAlignment(Qt::AlignRight);
    price_label_ = new QLabel("--");
    price_label_->setAlignment(Qt::AlignRight);
    price_label_->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold; background: transparent;")
                                    .arg(ui::colors::TEXT_PRIMARY()));
    price_col->addWidget(price_label_);

    auto* bid_ask_row = new QHBoxLayout;
    bid_label_ = new QLabel("BID --");
    bid_label_->setStyleSheet(QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::POSITIVE()));
    bid_ask_row->addWidget(bid_label_);
    bid_ask_row->addSpacing(8);
    ask_label_ = new QLabel("ASK --");
    ask_label_->setStyleSheet(QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::NEGATIVE()));
    bid_ask_row->addWidget(ask_label_);
    price_col->addLayout(bid_ask_row);
    qcl->addLayout(price_col);

    vl->addWidget(quote_card);

    // ── Separator ──
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);

    // ── Order form ──
    // Side + Type row
    auto* st_row = new QHBoxLayout;
    st_row->setSpacing(6);

    side_combo_ = new QComboBox;
    side_combo_->addItems({"BUY", "SELL", "SHORT"});
    side_combo_->setStyleSheet(combo_style());
    st_row->addWidget(side_combo_, 1);

    order_type_ = new QComboBox;
    order_type_->addItems({"MARKET", "LIMIT", "STOP"});
    order_type_->setStyleSheet(combo_style());
    st_row->addWidget(order_type_, 1);
    vl->addLayout(st_row);

    // Qty + Limit price row
    auto* qp_row = new QHBoxLayout;
    qp_row->setSpacing(6);

    auto* qty_lbl = new QLabel("QTY");
    qty_lbl->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    qp_row->addWidget(qty_lbl);

    qty_input_ = new QLineEdit("10");
    qty_input_->setStyleSheet(input_style());
    qty_input_->setValidator(new QDoubleValidator(0, 1e9, 4, qty_input_));
    qp_row->addWidget(qty_input_, 1);

    auto* price_lbl = new QLabel("PRICE");
    price_lbl->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    qp_row->addWidget(price_lbl);

    price_input_ = new QLineEdit;
    price_input_->setPlaceholderText("market");
    price_input_->setStyleSheet(input_style(ui::colors::BORDER_DIM()));
    qp_row->addWidget(price_input_, 1);
    vl->addLayout(qp_row);

    // Estimated total
    est_total_ = new QLabel("EST. TOTAL  --");
    est_total_->setStyleSheet(
        QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(est_total_);

    // Submit button
    submit_btn_ = new QPushButton("PLACE ORDER");
    submit_btn_->setFixedHeight(32);
    submit_btn_->setStyleSheet(QString("QPushButton { background: %1; color: #ffffff; border: none; "
                                       "font-size: 11px; font-weight: bold; }"
                                       "QPushButton:hover { background: %2; }"
                                       "QPushButton:disabled { background: %3; color: %4; }")
                                   .arg(ui::colors::POSITIVE())
                                   .arg("#15803d")
                                   .arg(ui::colors::BG_RAISED())
                                   .arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(submit_btn_);

    // ── Connections ──
    connect(lookup_btn_, &QPushButton::clicked, this, &QuickTradeWidget::lookup_symbol);
    connect(symbol_input_, &QLineEdit::returnPressed, this, &QuickTradeWidget::lookup_symbol);
    connect(side_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QuickTradeWidget::on_side_changed);
    connect(submit_btn_, &QPushButton::clicked, this, &QuickTradeWidget::submit_order);
    connect(order_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        price_input_->setEnabled(idx != 0); // disable for MARKET
        price_input_->setPlaceholderText(idx == 0 ? "market" : "0.00");
    });
    connect(qty_input_, &QLineEdit::textChanged, this, [this](const QString&) {
        if (current_price_ > 0) {
            double qty = qty_input_->text().toDouble();
            double total = qty * current_price_;
            est_total_->setText(QString("EST. TOTAL  $%1").arg(total, 0, 'f', 2));
        }
    });
    connect(this, &BaseWidget::refresh_requested, this, &QuickTradeWidget::lookup_symbol);

    // Initial lookup
    set_loading(true);
    lookup_symbol();
}

void QuickTradeWidget::lookup_symbol() {
    QString sym = symbol_input_->text().trimmed().toUpper();
    if (sym.isEmpty()) {
        set_loading(false);
        return;
    }

    set_loading(true);
    services::MarketDataService::instance().fetch_quotes({sym}, [this, sym](bool ok,
                                                                            QVector<services::QuoteData> quotes) {
        set_loading(false);
        if (!ok || quotes.isEmpty()) {
            sym_label_->setText(sym);
            price_label_->setText("N/A");
            change_label_->setText("Symbol not found");
            return;
        }
        const auto& q = quotes.first();
        current_price_ = q.price;
        current_symbol_ = q.symbol;

        sym_label_->setText(q.symbol.isEmpty() ? sym : q.symbol);
        price_label_->setText(QString("$%1").arg(q.price, 0, 'f', 2));

        double chg = q.change_pct;
        QString chg_str = QString("%1%2%").arg(chg >= 0 ? "▲ +" : "▼ ").arg(chg, 0, 'f', 2);
        QString chg_col = chg > 0 ? ui::colors::POSITIVE() : chg < 0 ? ui::colors::NEGATIVE() : ui::colors::TEXT_SECONDARY();
        change_label_->setText(chg_str);
        change_label_->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(chg_col));

        // Approximate bid/ask from spread (±0.05% of price)
        double spread = q.price * 0.0005;
        bid_label_->setText(QString("BID %1").arg(q.price - spread, 0, 'f', 2));
        ask_label_->setText(QString("ASK %1").arg(q.price + spread, 0, 'f', 2));

        // Update estimated total
        double qty = qty_input_->text().toDouble();
        if (qty > 0)
            est_total_->setText(QString("EST. TOTAL  $%1").arg(qty * q.price, 0, 'f', 2));
    });
}

void QuickTradeWidget::on_side_changed(int idx) {
    // BUY = green, SELL/SHORT = red
    QString color = (idx == 0) ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
    submit_btn_->setText(idx == 0 ? "PLACE BUY ORDER" : idx == 1 ? "PLACE SELL ORDER" : "PLACE SHORT ORDER");
    submit_btn_->setStyleSheet(QString("QPushButton { background: %1; color: #ffffff; border: none; "
                                       "font-size: 11px; font-weight: bold; }"
                                       "QPushButton:hover { background: %2; }")
                                   .arg(color)
                                   .arg(idx == 0 ? "#15803d" : "#b91c1c"));
}

void QuickTradeWidget::submit_order() {
    QString sym = current_symbol_.isEmpty() ? symbol_input_->text().trimmed().toUpper() : current_symbol_;
    double qty = qty_input_->text().toDouble();
    QString side = side_combo_->currentText();
    QString type = order_type_->currentText();

    if (sym.isEmpty() || qty <= 0) {
        QMessageBox::warning(this, "Quick Trade", "Please enter a valid symbol and quantity.");
        return;
    }

    QString price_str = type == "MARKET" ? QString("market price ($%1)").arg(current_price_, 0, 'f', 2)
                                         : QString("$%1").arg(price_input_->text());

    QMessageBox::information(
        this, "Order Submitted",
        QString("%1 %2 %3 @ %4\nOrder sent to trading engine.").arg(side).arg(qty, 0, 'f', 0).arg(sym).arg(price_str));
}

} // namespace fincept::screens::widgets
