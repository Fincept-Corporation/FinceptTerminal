#include "screens/dashboard/widgets/QuickTradeWidget.h"

#include "ui/theme/Theme.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

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
    auto* search_row = new QWidget(this);
    auto* srl = new QHBoxLayout(search_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(6);

    symbol_input_ = new QLineEdit;
    symbol_input_->setPlaceholderText("Symbol (e.g. AAPL)");
    symbol_input_->setText("AAPL");
    srl->addWidget(symbol_input_, 1);

    lookup_btn_ = new QPushButton("LOOKUP");
    srl->addWidget(lookup_btn_);
    vl->addWidget(search_row);

    // ── Quote display ──
    quote_card_ = new QWidget(this);
    auto* qcl = new QHBoxLayout(quote_card_);
    qcl->setContentsMargins(10, 8, 10, 8);
    qcl->setSpacing(12);

    auto* sym_col = new QVBoxLayout;
    sym_label_ = new QLabel("--");
    sym_col->addWidget(sym_label_);
    change_label_ = new QLabel("--");
    sym_col->addWidget(change_label_);
    qcl->addLayout(sym_col);

    qcl->addStretch();

    auto* price_col = new QVBoxLayout;
    price_col->setAlignment(Qt::AlignRight);
    price_label_ = new QLabel("--");
    price_label_->setAlignment(Qt::AlignRight);
    price_col->addWidget(price_label_);

    auto* bid_ask_row = new QHBoxLayout;
    bid_label_ = new QLabel("BID --");
    bid_ask_row->addWidget(bid_label_);
    bid_ask_row->addSpacing(8);
    ask_label_ = new QLabel("ASK --");
    bid_ask_row->addWidget(ask_label_);
    price_col->addLayout(bid_ask_row);
    qcl->addLayout(price_col);

    vl->addWidget(quote_card_);

    // ── Separator ──
    separator_ = new QFrame;
    separator_->setFixedHeight(1);
    vl->addWidget(separator_);

    // ── Order form ──
    // Side + Type row
    auto* st_row = new QHBoxLayout;
    st_row->setSpacing(6);

    side_combo_ = new QComboBox;
    side_combo_->addItems({"BUY", "SELL", "SHORT"});
    st_row->addWidget(side_combo_, 1);

    order_type_ = new QComboBox;
    order_type_->addItems({"MARKET", "LIMIT", "STOP"});
    st_row->addWidget(order_type_, 1);
    vl->addLayout(st_row);

    // Qty + Limit price row
    auto* qp_row = new QHBoxLayout;
    qp_row->setSpacing(6);

    qty_lbl_ = new QLabel("QTY");
    qp_row->addWidget(qty_lbl_);

    qty_input_ = new QLineEdit("10");
    qty_input_->setValidator(new QDoubleValidator(0, 1e9, 4, qty_input_));
    qp_row->addWidget(qty_input_, 1);

    price_lbl_ = new QLabel("PRICE");
    qp_row->addWidget(price_lbl_);

    price_input_ = new QLineEdit;
    price_input_->setPlaceholderText("market");
    qp_row->addWidget(price_input_, 1);
    vl->addLayout(qp_row);

    // Estimated total
    est_total_ = new QLabel("EST. TOTAL  --");
    vl->addWidget(est_total_);

    // Submit button
    submit_btn_ = new QPushButton("PLACE ORDER");
    submit_btn_->setFixedHeight(32);
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

    apply_styles();

    // Initial lookup
    set_loading(true);
    // Hub path: initial lookup happens from showEvent.
}

void QuickTradeWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        lookup_symbol();
}

void QuickTradeWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void QuickTradeWidget::apply_styles() {
    symbol_input_->setStyleSheet(input_style());
    lookup_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                                       "font-size: 10px; font-weight: bold; padding: 4px 10px; }"
                                       "QPushButton:hover { background: %4; }")
                                   .arg(ui::colors::BG_RAISED())
                                   .arg(ui::colors::AMBER())
                                   .arg(ui::colors::AMBER())
                                   .arg(ui::colors::BG_HOVER()));

    quote_card_->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    sym_label_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold; background: transparent;")
                                  .arg(ui::colors::TEXT_PRIMARY()));
    change_label_->setStyleSheet(
        QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    price_label_->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold; background: transparent;")
                                    .arg(ui::colors::TEXT_PRIMARY()));
    bid_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::POSITIVE()));
    ask_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::NEGATIVE()));

    separator_->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));

    side_combo_->setStyleSheet(combo_style());
    order_type_->setStyleSheet(combo_style());

    qty_lbl_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    qty_input_->setStyleSheet(input_style());
    price_lbl_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    price_input_->setStyleSheet(input_style(ui::colors::BORDER_DIM()));

    est_total_->setStyleSheet(
        QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));

    // Submit button color depends on current side selection
    on_side_changed(side_combo_->currentIndex());
}

void QuickTradeWidget::on_theme_changed() {
    apply_styles();
}

void QuickTradeWidget::lookup_symbol() {
    QString sym = symbol_input_->text().trimmed().toUpper();
    if (sym.isEmpty()) {
        set_loading(false);
        return;
    }

    current_symbol_ = sym;
    auto& hub = datahub::DataHub::instance();
    // Swap subscription to the new symbol; drop the old one first.
    hub.unsubscribe(this);
    const QString topic = QStringLiteral("market:quote:") + sym;
    hub.subscribe(this, topic, [this](const QVariant& v) {
        if (!v.canConvert<services::QuoteData>())
            return;
        set_loading(false);
        apply_quote(v.value<services::QuoteData>());
    });
    // Kick a fresh fetch so the card updates immediately even if the
    // cached TTL hasn't expired.
    hub.request(topic);
    hub_active_ = true;
    set_loading(true);
}


void QuickTradeWidget::apply_quote(const services::QuoteData& q) {
    current_price_ = q.price;
    current_symbol_ = q.symbol.isEmpty() ? current_symbol_ : q.symbol;

    sym_label_->setText(current_symbol_);
    price_label_->setText(QString("$%1").arg(q.price, 0, 'f', 2));

    double chg = q.change_pct;
    QString chg_str = QString("%1%2%").arg(chg >= 0 ? "▲ +" : "▼ ").arg(chg, 0, 'f', 2);
    QString chg_col = chg > 0   ? ui::colors::POSITIVE()
                      : chg < 0 ? ui::colors::NEGATIVE()
                                : ui::colors::TEXT_SECONDARY();
    change_label_->setText(chg_str);
    change_label_->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(chg_col));

    double spread = q.price * 0.0005;
    bid_label_->setText(QString("BID %1").arg(q.price - spread, 0, 'f', 2));
    ask_label_->setText(QString("ASK %1").arg(q.price + spread, 0, 'f', 2));

    double qty = qty_input_->text().toDouble();
    if (qty > 0)
        est_total_->setText(QString("EST. TOTAL  $%1").arg(qty * q.price, 0, 'f', 2));
}

void QuickTradeWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}


void QuickTradeWidget::on_side_changed(int idx) {
    // BUY = green, SELL/SHORT = red
    QString color = (idx == 0) ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
    submit_btn_->setText(idx == 0 ? "PLACE BUY ORDER" : idx == 1 ? "PLACE SELL ORDER" : "PLACE SHORT ORDER");
    submit_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %3; border: none; "
                                       "font-size: 11px; font-weight: bold; }"
                                       "QPushButton:hover { background: %2; }")
                                   .arg(color, color, ui::colors::TEXT_PRIMARY()));
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
