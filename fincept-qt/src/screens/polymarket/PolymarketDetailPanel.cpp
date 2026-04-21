#include "screens/polymarket/PolymarketDetailPanel.h"

#include "screens/polymarket/PolymarketActivityFeed.h"
#include "screens/polymarket/PolymarketOrderBook.h"
#include "screens/polymarket/PolymarketPriceChart.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QTimeZone>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
namespace pmx = fincept::services::polymarket;
using namespace fincept::services::prediction;

static const QStringList TAB_NAMES = {
    "OVERVIEW", "ORDER BOOK", "CHART", "TRADE", "TRADES", "HOLDERS", "COMMENTS", "RELATED"
};
static const char* OUTCOME_COLORS[] = {"#00D66F", "#FF3B3B", "#FF8800", "#4F8EF7", "#A855F7"};

PolymarketDetailPanel::PolymarketDetailPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("polyDetailPanel");
    build_ui();
}

void PolymarketDetailPanel::build_ui() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Tab bar ───────────────────────────────────────────────────────────
    auto* tab_bar = new QWidget(this);
    tab_bar->setObjectName("polyDetailTabBar");
    tab_bar->setFixedHeight(34);
    tab_bar->setStyleSheet(
        QString("QWidget#polyDetailTabBar { background: %1; border-bottom: 1px solid %2; }")
            .arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    auto* thl = new QHBoxLayout(tab_bar);
    thl->setContentsMargins(0, 0, 0, 0);
    thl->setSpacing(0);

    for (int i = 0; i < TAB_NAMES.size(); ++i) {
        auto* btn = new QPushButton(TAB_NAMES[i]);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { set_active_tab(i); });
        thl->addWidget(btn);
        tab_btns_.append(btn);
    }
    thl->addStretch(1);
    vl->addWidget(tab_bar);
    apply_accent_to_tabs();

    // ── Stacked pages ─────────────────────────────────────────────────────
    stack_ = new QStackedWidget;
    stack_->addWidget(create_overview_page()); // 0

    orderbook_ = new PolymarketOrderBook;
    stack_->addWidget(orderbook_); // 1

    price_chart_ = new PolymarketPriceChart;
    connect(price_chart_, &PolymarketPriceChart::interval_changed,
            this, &PolymarketDetailPanel::interval_changed);
    connect(price_chart_, &PolymarketPriceChart::outcome_changed,
            this, &PolymarketDetailPanel::outcome_changed);
    stack_->addWidget(price_chart_); // 2

    stack_->addWidget(create_trade_page()); // 3

    activity_feed_ = new PolymarketActivityFeed;
    stack_->addWidget(activity_feed_); // 4

    stack_->addWidget(create_holders_page());  // 5
    stack_->addWidget(create_comments_page()); // 6
    stack_->addWidget(create_related_page());  // 7

    vl->addWidget(stack_, 1);
}

QWidget* PolymarketDetailPanel::create_overview_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea { border: none; background: %1; }"
                "QScrollBar:vertical { background: %2; width: 4px; border: none; }"
                "QScrollBar::handle:vertical { background: %3; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(colors::BG_BASE(), colors::BG_SURFACE(), colors::BORDER_BRIGHT()));

    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(16, 14, 16, 16);
    vl->setSpacing(0);

    // ── Market question / title ───────────────────────────────────────────
    question_label_ = new QLabel("Select a market to view details");
    question_label_->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: 700; background: transparent; "
                "line-height: 1.4;")
            .arg(colors::TEXT_PRIMARY()));
    question_label_->setWordWrap(true);
    question_label_->setMinimumHeight(36);
    vl->addWidget(question_label_);

    vl->addSpacing(10);

    // ── Status row (badge + outcome labels) ───────────────────────────────
    auto* status_row = new QWidget;
    status_row->setStyleSheet("background: transparent;");
    auto* srl = new QHBoxLayout(status_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(6);

    status_label_ = new QLabel;
    srl->addWidget(status_label_);
    srl->addStretch(1);
    vl->addWidget(status_row);

    vl->addSpacing(12);

    // ── Stats: two rows of 3-4 cells each ────────────────────────────────
    // Row 1 of stats
    auto* stats_row1 = new QWidget;
    stats_row1->setStyleSheet(
        QString("background: %1; border: 1px solid %2; border-bottom: none;")
            .arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* sr1l = new QHBoxLayout(stats_row1);
    sr1l->setContentsMargins(0, 0, 0, 0);
    sr1l->setSpacing(0);

    auto make_stat_in = [&](QWidget* parent_row, QHBoxLayout* row_layout,
                             const QString& lbl, QLabel*& val,
                             bool last = false, QWidget** box_out = nullptr) {
        auto* box = new QWidget(parent_row);
        box->setStyleSheet(
            last ? QString("background: transparent;")
                 : QString("background: transparent; border-right: 1px solid %1;")
                       .arg(colors::BORDER_DIM()));
        auto* bvl = new QVBoxLayout(box);
        bvl->setContentsMargins(12, 8, 12, 8);
        bvl->setSpacing(3);

        auto* lbl_w = new QLabel(lbl, box);
        lbl_w->setStyleSheet(
            QString("color: %1; font-size: 8px; font-weight: 700; letter-spacing: 0.8px; "
                    "background: transparent;")
                .arg(colors::TEXT_SECONDARY()));

        val = new QLabel("—", box);
        val->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: 700; background: transparent;")
                .arg(colors::TEXT_PRIMARY()));

        bvl->addWidget(lbl_w);
        bvl->addWidget(val);
        row_layout->addWidget(box, 1);
        if (box_out) *box_out = box;
    };

    make_stat_in(stats_row1, sr1l, "VOLUME",    volume_label_);
    make_stat_in(stats_row1, sr1l, "LIQUIDITY", liquidity_label_);
    make_stat_in(stats_row1, sr1l, "OPEN INT",  oi_label_, false, &oi_box_);
    make_stat_in(stats_row1, sr1l, "END DATE",  end_date_label_, true);
    vl->addWidget(stats_row1);

    // Row 2 of stats
    auto* stats_row2 = new QWidget;
    stats_row2->setStyleSheet(
        QString("background: %1; border: 1px solid %2;")
            .arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* sr2l = new QHBoxLayout(stats_row2);
    sr2l->setContentsMargins(0, 0, 0, 0);
    sr2l->setSpacing(0);

    make_stat_in(stats_row2, sr2l, "MIDPOINT",   midpoint_label_);
    make_stat_in(stats_row2, sr2l, "SPREAD",     spread_label_);
    make_stat_in(stats_row2, sr2l, "LAST TRADE", last_trade_label_, true);
    vl->addWidget(stats_row2);

    vl->addSpacing(14);

    // ── Outcome probability bars ──────────────────────────────────────────
    auto* outcomes_header = new QLabel("OUTCOMES");
    outcomes_header->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: 700; letter-spacing: 0.8px; "
                "background: transparent;")
            .arg(colors::TEXT_SECONDARY()));
    vl->addWidget(outcomes_header);
    vl->addSpacing(6);

    outcome_container_ = new QWidget;
    outcome_container_->setStyleSheet("background: transparent;");
    auto* ocl = new QVBoxLayout(outcome_container_);
    ocl->setContentsMargins(0, 0, 0, 0);
    ocl->setSpacing(6);
    vl->addWidget(outcome_container_);

    vl->addSpacing(14);

    // ── Description ───────────────────────────────────────────────────────
    description_label_ = new QLabel;
    description_label_->setStyleSheet(
        QString("color: %1; font-size: 10px; line-height: 1.5; background: transparent; "
                "border-top: 1px solid %2; padding-top: 12px;")
            .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM()));
    description_label_->setWordWrap(true);
    vl->addWidget(description_label_);

    vl->addStretch(1);
    scroll->setWidget(page);
    return scroll;
}

QWidget* PolymarketDetailPanel::create_trade_page() {
    // Two-state stack: 0 = no-account placeholder, 1 = ticket form.
    ticket_stack_ = new QStackedWidget;

    // ── State 0: no account connected ────────────────────────────────────────
    auto* no_acct = new QWidget;
    no_acct->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* nal = new QVBoxLayout(no_acct);
    nal->setAlignment(Qt::AlignCenter);
    nal->setSpacing(8);
    auto* icon_lbl = new QLabel("🔒");
    icon_lbl->setAlignment(Qt::AlignCenter);
    icon_lbl->setStyleSheet("font-size: 28px; background: transparent;");
    auto* msg_lbl = new QLabel("Connect an account\nto place orders");
    msg_lbl->setAlignment(Qt::AlignCenter);
    msg_lbl->setStyleSheet(
        QString("color: %1; font-size: 11px; background: transparent;").arg(colors::TEXT_DIM()));
    nal->addWidget(icon_lbl);
    nal->addWidget(msg_lbl);
    ticket_stack_->addWidget(no_acct); // index 0

    // ── State 1: ticket form ─────────────────────────────────────────────────
    auto* form = new QWidget;
    form->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* fl = new QVBoxLayout(form);
    fl->setContentsMargins(16, 14, 16, 14);
    fl->setSpacing(10);

    // Balance + position header
    auto* hdr = new QWidget;
    hdr->setStyleSheet(
        QString("background: %1; border: 1px solid %2; border-radius: 3px;")
            .arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* hdrl = new QHBoxLayout(hdr);
    hdrl->setContentsMargins(10, 7, 10, 7);
    hdrl->setSpacing(0);

    auto* bal_col = new QVBoxLayout;
    bal_col->setSpacing(2);
    auto* bal_lbl = new QLabel("AVAILABLE");
    bal_lbl->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: 700; letter-spacing: 0.8px; "
                "background: transparent;").arg(colors::TEXT_SECONDARY()));
    ticket_balance_lbl_ = new QLabel("—");
    ticket_balance_lbl_->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: 700; background: transparent;")
            .arg(colors::TEXT_PRIMARY()));
    bal_col->addWidget(bal_lbl);
    bal_col->addWidget(ticket_balance_lbl_);

    auto* pos_col = new QVBoxLayout;
    pos_col->setSpacing(2);
    pos_col->setAlignment(Qt::AlignRight);
    auto* pos_lbl = new QLabel("POSITION");
    pos_lbl->setAlignment(Qt::AlignRight);
    pos_lbl->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: 700; letter-spacing: 0.8px; "
                "background: transparent;").arg(colors::TEXT_SECONDARY()));
    ticket_position_lbl_ = new QLabel("—");
    ticket_position_lbl_->setAlignment(Qt::AlignRight);
    ticket_position_lbl_->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: 700; background: transparent;")
            .arg(colors::TEXT_PRIMARY()));
    pos_col->addWidget(pos_lbl);
    pos_col->addWidget(ticket_position_lbl_);

    hdrl->addLayout(bal_col);
    hdrl->addStretch(1);
    hdrl->addLayout(pos_col);
    fl->addWidget(hdr);

    // Side selector: BUY / SELL
    auto* side_row = new QWidget;
    side_row->setStyleSheet("background: transparent;");
    auto* srl = new QHBoxLayout(side_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(0);

    ticket_buy_btn_  = new QPushButton("BUY");
    ticket_sell_btn_ = new QPushButton("SELL");
    for (auto* b : {ticket_buy_btn_, ticket_sell_btn_}) {
        b->setFixedHeight(32);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    connect(ticket_buy_btn_,  &QPushButton::clicked, this, [this]() {
        ticket_side_ = "BUY";
        refresh_ticket_side_style();
    });
    connect(ticket_sell_btn_, &QPushButton::clicked, this, [this]() {
        ticket_side_ = "SELL";
        refresh_ticket_side_style();
    });
    srl->addWidget(ticket_buy_btn_);
    srl->addWidget(ticket_sell_btn_);
    fl->addWidget(side_row);

    // Outcome selector
    auto make_label = [&](const QString& text) -> QLabel* {
        auto* l = new QLabel(text);
        l->setStyleSheet(
            QString("color: %1; font-size: 8px; font-weight: 700; letter-spacing: 0.8px; "
                    "background: transparent;").arg(colors::TEXT_SECONDARY()));
        return l;
    };
    const QString input_ss =
        QString("QComboBox, QLineEdit {"
                "  background: %1; color: %2; border: 1px solid %3;"
                "  border-radius: 2px; padding: 4px 8px; font-size: 11px;"
                "}"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView {"
                "  background: %1; color: %2; border: 1px solid %3; selection-background-color: %4;"
                "}")
            .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_MED(), colors::BG_HOVER());

    fl->addWidget(make_label("OUTCOME"));
    ticket_outcome_cb_ = new QComboBox;
    ticket_outcome_cb_->setStyleSheet(input_ss);
    fl->addWidget(ticket_outcome_cb_);

    // Price + Size row
    auto* ps_row = new QWidget;
    ps_row->setStyleSheet("background: transparent;");
    auto* psl = new QHBoxLayout(ps_row);
    psl->setContentsMargins(0, 0, 0, 0);
    psl->setSpacing(8);

    auto* price_col = new QVBoxLayout;
    price_col->setSpacing(4);
    price_col->addWidget(make_label("PRICE (0–1)"));
    ticket_price_edit_ = new QLineEdit;
    ticket_price_edit_->setPlaceholderText("0.50");
    ticket_price_edit_->setStyleSheet(input_ss);
    price_col->addWidget(ticket_price_edit_);

    auto* size_col = new QVBoxLayout;
    size_col->setSpacing(4);
    size_col->addWidget(make_label("SIZE"));
    ticket_size_edit_ = new QLineEdit;
    ticket_size_edit_->setPlaceholderText("10");
    ticket_size_edit_->setStyleSheet(input_ss);
    size_col->addWidget(ticket_size_edit_);

    psl->addLayout(price_col);
    psl->addLayout(size_col);
    fl->addWidget(ps_row);

    // Order type
    fl->addWidget(make_label("ORDER TYPE"));
    ticket_type_cb_ = new QComboBox;
    ticket_type_cb_->addItems({"GTC", "FOK", "FAK"});
    ticket_type_cb_->setStyleSheet(input_ss);
    fl->addWidget(ticket_type_cb_);

    // Submit button
    ticket_submit_btn_ = new QPushButton("PLACE ORDER");
    ticket_submit_btn_->setFixedHeight(34);
    ticket_submit_btn_->setCursor(Qt::PointingHandCursor);
    connect(ticket_submit_btn_, &QPushButton::clicked, this, &PolymarketDetailPanel::on_submit_clicked);
    fl->addWidget(ticket_submit_btn_);

    // Status label (feedback)
    ticket_status_lbl_ = new QLabel;
    ticket_status_lbl_->setAlignment(Qt::AlignCenter);
    ticket_status_lbl_->setWordWrap(true);
    ticket_status_lbl_->setStyleSheet(
        QString("color: %1; font-size: 10px; background: transparent;").arg(colors::TEXT_DIM()));
    fl->addWidget(ticket_status_lbl_);

    fl->addStretch(1);
    ticket_stack_->addWidget(form); // index 1

    refresh_ticket_side_style();
    return ticket_stack_;
}

void PolymarketDetailPanel::refresh_ticket_side_style() {
    if (!ticket_buy_btn_ || !ticket_sell_btn_ || !ticket_submit_btn_) return;
    const bool is_buy = (ticket_side_ == "BUY");
    const QString active_bg   = is_buy ? colors::POSITIVE() : colors::NEGATIVE();
    const QString inactive_bg = colors::BG_RAISED();
    const QString active_text   = "#0A0A0A";
    const QString inactive_text = colors::TEXT_DIM();

    ticket_buy_btn_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: none; "
                "font-size: 11px; font-weight: 700; }"
                "QPushButton:hover { opacity: 0.9; }")
            .arg(is_buy ? active_bg : inactive_bg,
                 is_buy ? active_text : inactive_text));
    ticket_sell_btn_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: none; "
                "font-size: 11px; font-weight: 700; }"
                "QPushButton:hover { opacity: 0.9; }")
            .arg(!is_buy ? active_bg : inactive_bg,
                 !is_buy ? active_text : inactive_text));
    ticket_submit_btn_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: none; "
                "border-radius: 2px; font-size: 12px; font-weight: 700; }"
                "QPushButton:hover { background: %3; }"
                "QPushButton:disabled { background: %4; color: %5; }")
            .arg(active_bg, active_text,
                 QColor(active_bg).lighter(115).name(),
                 colors::BG_RAISED(), colors::TEXT_DIM()));
}

void PolymarketDetailPanel::on_submit_clicked() {
    if (!has_last_market_) return;
    bool price_ok = false, size_ok = false;
    const double price = ticket_price_edit_->text().toDouble(&price_ok);
    const double size  = ticket_size_edit_->text().toDouble(&size_ok);
    if (!price_ok || price <= 0.0 || price >= 1.0) {
        ticket_status_lbl_->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;").arg(colors::NEGATIVE()));
        ticket_status_lbl_->setText("Invalid price — enter a value between 0 and 1");
        return;
    }
    if (!size_ok || size <= 0.0) {
        ticket_status_lbl_->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;").arg(colors::NEGATIVE()));
        ticket_status_lbl_->setText("Invalid size — must be > 0");
        return;
    }

    const int outcome_idx = ticket_outcome_cb_->currentIndex();
    const QString asset_id = (outcome_idx >= 0 && outcome_idx < last_market_.outcomes.size())
                                 ? last_market_.outcomes[outcome_idx].asset_id
                                 : QString{};

    OrderRequest req;
    req.key        = last_market_.key;
    req.asset_id   = asset_id;
    req.side       = ticket_side_;
    req.order_type = ticket_type_cb_->currentText();
    req.price      = price;
    req.size       = size;

    ticket_status_lbl_->setStyleSheet(
        QString("color: %1; font-size: 10px; background: transparent;").arg(colors::TEXT_DIM()));
    ticket_status_lbl_->setText("Submitting…");
    ticket_submit_btn_->setEnabled(false);
    emit place_order(req);
}

void PolymarketDetailPanel::set_balance(const AccountBalance& balance) {
    if (!ticket_balance_lbl_) return;
    ticket_balance_lbl_->setText(
        QString("%1 %2").arg(balance.available, 0, 'f', 2).arg(balance.currency));
}

void PolymarketDetailPanel::set_positions(const QVector<PredictionPosition>& positions) {
    if (!ticket_position_lbl_ || !has_last_market_) return;
    // Find the position matching the currently-displayed market.
    double total_size = 0.0;
    for (const auto& p : positions) {
        if (p.market_id == last_market_.key.market_id)
            total_size += p.size;
    }
    ticket_position_lbl_->setText(total_size > 0.0
                                      ? QString("%1 shares").arg(total_size, 0, 'f', 2)
                                      : "No position");
}

void PolymarketDetailPanel::on_order_result(const OrderResult& result) {
    if (!ticket_status_lbl_ || !ticket_submit_btn_) return;
    ticket_submit_btn_->setEnabled(true);
    if (result.ok) {
        ticket_status_lbl_->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;").arg(colors::POSITIVE()));
        ticket_status_lbl_->setText(
            QString("Order placed ✓  ID: %1").arg(result.order_id.left(12)));
        ticket_price_edit_->clear();
        ticket_size_edit_->clear();
    } else {
        ticket_status_lbl_->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;").arg(colors::NEGATIVE()));
        ticket_status_lbl_->setText(
            result.error_message.isEmpty() ? result.error_code : result.error_message);
    }
}

void PolymarketDetailPanel::set_trading_enabled(bool enabled) {
    if (!ticket_stack_) return;
    ticket_stack_->setCurrentIndex(enabled ? 1 : 0);
}

QWidget* PolymarketDetailPanel::create_holders_page() {
    holders_table_ = new QTableWidget;
    holders_table_->setColumnCount(4);
    holders_table_->setHorizontalHeaderLabels({"RANK", "TRADER", "SIZE", "AVG PRICE"});
    holders_table_->horizontalHeader()->setStretchLastSection(true);
    holders_table_->verticalHeader()->setVisible(false);
    holders_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    holders_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    holders_table_->setShowGrid(false);
    holders_table_->setStyleSheet(
        QString("QTableWidget { background: %1; color: %2; border: none; font-size: 10px; }"
                "QTableWidget::item { padding: 4px 8px; border-bottom: 1px solid %3; }"
                "QTableWidget::item:selected { background: %4; color: %2; }"
                "QHeaderView::section {"
                "  background: %5; color: %6; border: none;"
                "  border-bottom: 1px solid %3;"
                "  padding: 5px 8px; font-size: 8px; font-weight: 700; letter-spacing: 0.5px;"
                "}"
                "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                "QScrollBar::handle:vertical { background: %3; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(),
                 colors::BG_HOVER(), colors::BG_RAISED(), colors::TEXT_SECONDARY()));
    return holders_table_;
}

QWidget* PolymarketDetailPanel::create_comments_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea { border: none; background: %1; }"
                "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                "QScrollBar::handle:vertical { background: %2; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(colors::BG_BASE(), colors::BORDER_BRIGHT()));
    comments_container_ = new QWidget;
    comments_container_->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* vl = new QVBoxLayout(comments_container_);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(8);
    auto* empty = new QLabel("No comments yet");
    empty->setStyleSheet(
        QString("color: %1; font-size: 12px; background: transparent;").arg(colors::TEXT_DIM()));
    empty->setAlignment(Qt::AlignCenter);
    vl->addWidget(empty);
    vl->addStretch(1);
    scroll->setWidget(comments_container_);
    return scroll;
}

QWidget* PolymarketDetailPanel::create_related_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea { border: none; background: %1; }"
                "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                "QScrollBar::handle:vertical { background: %2; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(colors::BG_BASE(), colors::BORDER_BRIGHT()));
    related_container_ = new QWidget;
    related_container_->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* vl = new QVBoxLayout(related_container_);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(6);
    auto* empty = new QLabel("No related markets");
    empty->setStyleSheet(
        QString("color: %1; font-size: 12px; background: transparent;").arg(colors::TEXT_DIM()));
    empty->setAlignment(Qt::AlignCenter);
    vl->addWidget(empty);
    vl->addStretch(1);
    scroll->setWidget(related_container_);
    return scroll;
}

void PolymarketDetailPanel::set_active_tab(int tab) {
    stack_->setCurrentIndex(tab);
    for (int i = 0; i < tab_btns_.size(); ++i) {
        tab_btns_[i]->setProperty("active", i == tab);
        tab_btns_[i]->style()->unpolish(tab_btns_[i]);
        tab_btns_[i]->style()->polish(tab_btns_[i]);
    }
    emit tab_changed(tab);
}

// ── Data setters ────────────────────────────────────────────────────────────

void PolymarketDetailPanel::set_market(const PredictionMarket& market) {
    last_market_ = market;
    has_last_market_ = true;

    question_label_->setText(market.question);
    volume_label_->setText(presentation_.format_volume(market.volume));
    liquidity_label_->setText(presentation_.format_liquidity(market.liquidity));
    end_date_label_->setText(market.end_date_iso.left(10));
    midpoint_label_->setText("—");
    spread_label_->setText("—");
    last_trade_label_->setText("—");
    oi_label_->setText("—");

    render_status_badge(market);

    // ── Populate ticket outcome combo ─────────────────────────────────────
    if (ticket_outcome_cb_) {
        ticket_outcome_cb_->clear();
        for (const auto& o : market.outcomes)
            ticket_outcome_cb_->addItem(o.name);
    }
    if (ticket_status_lbl_) ticket_status_lbl_->clear();
    if (ticket_submit_btn_) ticket_submit_btn_->setEnabled(true);
    if (ticket_position_lbl_) ticket_position_lbl_->setText("—");

    // ── Rebuild outcome probability bars ──────────────────────────────────
    auto* layout = qobject_cast<QVBoxLayout*>(outcome_container_->layout());
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < market.outcomes.size(); ++i) {
        const auto& outcome = market.outcomes[i];
        const double pct = qBound(0.0, outcome.price, 1.0);
        const int pct_int = qRound(pct * 100.0);

        QColor bar_color;
        if (i == 0) bar_color = presentation_.accent;
        else if (i < 5) bar_color = QColor(OUTCOME_COLORS[i]);
        else bar_color = QColor(colors::TEXT_SECONDARY());

        // Outcome row: name + bar + price
        auto* row = new QWidget(outcome_container_);
        row->setStyleSheet("background: transparent;");
        auto* rl = new QVBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(3);

        // Top line: name + percentage
        auto* top_line = new QWidget(row);
        top_line->setStyleSheet("background: transparent;");
        auto* tll = new QHBoxLayout(top_line);
        tll->setContentsMargins(0, 0, 0, 0);
        tll->setSpacing(6);

        auto* name_lbl = new QLabel(outcome.name, top_line);
        name_lbl->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: 600; background: transparent;")
                .arg(colors::TEXT_PRIMARY()));

        auto* pct_lbl = new QLabel(QString("%1%").arg(pct_int), top_line);
        pct_lbl->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: 700; background: transparent;")
                .arg(bar_color.name()));

        // Full formatted price (right side)
        auto* price_lbl = new QLabel(presentation_.format_price(pct), top_line);
        price_lbl->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: 600; background: transparent;")
                .arg(colors::TEXT_SECONDARY()));

        tll->addWidget(name_lbl);
        tll->addStretch(1);
        tll->addWidget(pct_lbl);
        tll->addSpacing(8);
        tll->addWidget(price_lbl);
        rl->addWidget(top_line);

        // Progress bar — a simple fixed-height widget
        auto* bar_track = new QWidget(row);
        bar_track->setFixedHeight(6);
        bar_track->setStyleSheet(
            QString("background: %1; border-radius: 0px;").arg(colors::BG_RAISED()));
        // Fill widget sits inside the track with proportional width — use
        // a layout that we stretch manually via resize.
        auto* bar_fill = new QWidget(bar_track);
        bar_fill->setStyleSheet(
            QString("background: %1; border-radius: 0px;").arg(bar_color.name()));
        // Store pct on the fill widget so it can be re-sized on layout.
        bar_fill->setProperty("fill_pct", pct);

        // We need the fill to track the track width — use a nested layout.
        auto* bar_layout = new QHBoxLayout(bar_track);
        bar_layout->setContentsMargins(0, 0, 0, 0);
        bar_layout->setSpacing(0);
        bar_layout->addWidget(bar_fill, qRound(pct * 1000));
        if (pct < 1.0) bar_layout->addStretch(qRound((1.0 - pct) * 1000));

        rl->addWidget(bar_track);
        layout->addWidget(row);
    }

    description_label_->setText(market.description.left(600));

    QStringList labels;
    labels.reserve(market.outcomes.size());
    for (const auto& o : market.outcomes)
        labels.append(o.name);
    price_chart_->set_outcome_labels(labels);

    set_active_tab(0);
}

void PolymarketDetailPanel::set_price_summary(const pmx::PriceSummary& summary) {
    midpoint_label_->setText(presentation_.format_price(summary.midpoint));
    spread_label_->setText(presentation_.format_price(summary.spread));
    last_trade_label_->setText(presentation_.format_price(summary.last_trade_price));
}

void PolymarketDetailPanel::set_order_book(const PredictionOrderBook& book) {
    orderbook_->set_data(book);
}

void PolymarketDetailPanel::set_price_history(const PriceHistory& history) {
    price_chart_->set_price_history(history);
}

void PolymarketDetailPanel::set_trades(const QVector<PredictionTrade>& trades) {
    activity_feed_->set_trades(trades);
}

void PolymarketDetailPanel::set_top_holders(const QVector<pmx::TopHolder>& holders) {
    holders_table_->setSortingEnabled(false);
    holders_table_->setRowCount(holders.size());
    for (int i = 0; i < holders.size(); ++i) {
        const pmx::TopHolder& h = holders[i];
        auto* rank = new QTableWidgetItem(QString::number(h.rank > 0 ? h.rank : i + 1));
        rank->setTextAlignment(Qt::AlignCenter);
        holders_table_->setItem(i, 0, rank);
        holders_table_->setItem(i, 1, new QTableWidgetItem(h.display_name));
        auto* size_item = new QTableWidgetItem(QString::number(h.position_size, 'f', 2));
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        holders_table_->setItem(i, 2, size_item);
        auto* price_item = new QTableWidgetItem(QString::number(h.entry_price, 'f', 4));
        price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        holders_table_->setItem(i, 3, price_item);
        if (i % 2 == 1) {
            for (int c = 0; c < 4; ++c) {
                if (auto* it = holders_table_->item(i, c))
                    it->setBackground(QColor(colors::ROW_ALT()));
            }
        }
    }
    holders_table_->resizeColumnsToContents();
    holders_table_->setSortingEnabled(true);
}

void PolymarketDetailPanel::set_comments(const QVector<pmx::Comment>& comments) {
    auto* vl = qobject_cast<QVBoxLayout*>(comments_container_->layout());
    while (vl->count() > 0) {
        auto* item = vl->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (comments.isEmpty()) {
        auto* empty = new QLabel("No comments yet");
        empty->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent;").arg(colors::TEXT_DIM()));
        empty->setAlignment(Qt::AlignCenter);
        vl->addWidget(empty);
    } else {
        for (const auto& c : comments) {
            auto* card = new QWidget(comments_container_);
            card->setStyleSheet(
                QString("background: %1; border: 1px solid %2; border-left: 2px solid %3;")
                    .arg(colors::BG_SURFACE(), colors::BORDER_DIM(), colors::AMBER()));
            auto* cvl = new QVBoxLayout(card);
            cvl->setContentsMargins(10, 8, 10, 8);
            cvl->setSpacing(4);

            auto* author = new QLabel(c.author.isEmpty() ? c.author_address.left(12) + "…" : c.author);
            author->setStyleSheet(
                QString("color: %1; font-size: 9px; font-weight: 700; background: transparent;")
                    .arg(colors::AMBER()));

            auto* body = new QLabel(c.body.left(280));
            body->setStyleSheet(
                QString("color: %1; font-size: 10px; background: transparent; line-height: 1.4;")
                    .arg(colors::TEXT_PRIMARY()));
            body->setWordWrap(true);

            auto* meta = new QLabel(
                QDateTime::fromSecsSinceEpoch(c.created_at, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm") +
                (c.likes > 0 ? QString("  · %1 likes").arg(c.likes) : ""));
            meta->setStyleSheet(
                QString("color: %1; font-size: 8px; background: transparent;").arg(colors::TEXT_DIM()));

            cvl->addWidget(author);
            cvl->addWidget(body);
            cvl->addWidget(meta);
            vl->addWidget(card);
        }
    }
    vl->addStretch(1);
}

void PolymarketDetailPanel::set_related_markets(const QVector<PredictionMarket>& markets) {
    auto* vl = qobject_cast<QVBoxLayout*>(related_container_->layout());
    while (vl->count() > 0) {
        auto* item = vl->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (markets.isEmpty()) {
        auto* empty = new QLabel("No related markets");
        empty->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent;").arg(colors::TEXT_DIM()));
        empty->setAlignment(Qt::AlignCenter);
        vl->addWidget(empty);
    } else {
        for (const auto& m : markets) {
            auto* card = new QPushButton(related_container_);
            card->setStyleSheet(
                QString("QPushButton {"
                        "  background: %1;"
                        "  color: %2;"
                        "  border: 1px solid %3;"
                        "  text-align: left;"
                        "  padding: 8px 12px;"
                        "  font-size: 10px;"
                        "  font-weight: 600;"
                        "}"
                        "QPushButton:hover { background: %4; color: %5; border-color: %6; }")
                    .arg(colors::BG_SURFACE(), colors::TEXT_SECONDARY(), colors::BORDER_DIM(),
                         colors::BG_HOVER(), colors::TEXT_PRIMARY(), colors::BORDER_BRIGHT()));
            card->setText(m.question.left(70) + (m.question.size() > 70 ? "…" : ""));
            card->setCursor(Qt::PointingHandCursor);
            connect(card, &QPushButton::clicked, this, [this, m]() { emit related_market_clicked(m); });
            vl->addWidget(card);
        }
    }
    vl->addStretch(1);
}

void PolymarketDetailPanel::set_open_interest(double oi) {
    oi_label_->setText(presentation_.format_volume(oi));
}

void PolymarketDetailPanel::set_series_tooltip(const QString& tooltip) {
    if (question_label_) question_label_->setToolTip(tooltip);
}

void PolymarketDetailPanel::set_polymarket_extras_enabled(bool enabled) {
    const int extra_tabs[] = {kTabHolders, kTabComments, kTabRelated};
    for (int idx : extra_tabs) {
        if (idx >= 0 && idx < tab_btns_.size())
            tab_btns_[idx]->setVisible(enabled);
    }
    if (!enabled) {
        if (holders_table_) holders_table_->setRowCount(0);

        auto clear_container = [](QWidget* container, const QString& msg) {
            if (!container) return;
            auto* vl = qobject_cast<QVBoxLayout*>(container->layout());
            if (!vl) return;
            while (vl->count() > 0) {
                auto* item = vl->takeAt(0);
                if (item->widget()) item->widget()->deleteLater();
                delete item;
            }
            auto* empty = new QLabel(msg);
            empty->setStyleSheet(
                QString("color: %1; font-size: 12px; background: transparent;")
                    .arg(colors::TEXT_DIM()));
            empty->setAlignment(Qt::AlignCenter);
            vl->addWidget(empty);
            vl->addStretch(1);
        };
        clear_container(comments_container_, tr("Comments are Polymarket-only"));
        clear_container(related_container_, tr("Related markets are Polymarket-only"));

        const int current = stack_ ? stack_->currentIndex() : 0;
        if (current == kTabHolders || current == kTabComments || current == kTabRelated)
            set_active_tab(0);
    }
}

void PolymarketDetailPanel::clear() {
    has_last_market_ = false;
    last_market_ = {};
    question_label_->setText("Select a market to view details");
    volume_label_->setText("—");
    liquidity_label_->setText("—");
    end_date_label_->setText("—");
    midpoint_label_->setText("—");
    spread_label_->setText("—");
    last_trade_label_->setText("—");
    oi_label_->setText("—");
    status_label_->clear();
    description_label_->clear();
    orderbook_->clear();
    activity_feed_->clear();

    // Clear outcomes
    if (auto* vl = qobject_cast<QVBoxLayout*>(outcome_container_->layout())) {
        while (vl->count() > 0) {
            auto* item = vl->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }
}

// ── Presentation wiring ─────────────────────────────────────────────────────

void PolymarketDetailPanel::set_presentation(const ExchangePresentation& p) {
    const bool accent_changed = p.accent != presentation_.accent;
    const bool oi_changed = p.has_open_interest != presentation_.has_open_interest;
    const bool extras_changed = p.has_polymarket_extras != presentation_.has_polymarket_extras;

    presentation_ = p;

    if (accent_changed) apply_accent_to_tabs();
    if (oi_changed) apply_presentation_to_stats();
    if (extras_changed) set_polymarket_extras_enabled(p.has_polymarket_extras);

    if (has_last_market_) set_market(last_market_);
}

void PolymarketDetailPanel::apply_accent_to_tabs() {
    const QColor& a = presentation_.accent;
    const QString accent_str =
        QStringLiteral("rgba(%1,%2,%3,1.0)").arg(a.red()).arg(a.green()).arg(a.blue());
    const QString accent_bg =
        QStringLiteral("rgba(%1,%2,%3,0.12)").arg(a.red()).arg(a.green()).arg(a.blue());

    const QString css =
        QStringLiteral(
            "QPushButton {"
            "  background: transparent;"
            "  color: %1;"
            "  border: none;"
            "  border-bottom: 2px solid transparent;"
            "  font-size: 9px;"
            "  font-weight: 700;"
            "  letter-spacing: 0.5px;"
            "  padding: 0 14px;"
            "  min-height: 34px;"
            "}"
            "QPushButton:hover {"
            "  color: %2;"
            "  border-bottom-color: %3;"
            "}"
            "QPushButton[active=\"true\"] {"
            "  color: %4;"
            "  border-bottom-color: %4;"
            "  background: %5;"
            "}")
        .arg(colors::TEXT_SECONDARY())  // inactive text
        .arg(colors::TEXT_PRIMARY())    // hover text
        .arg(colors::BORDER_BRIGHT())   // hover underline
        .arg(accent_str)                // active text + underline
        .arg(accent_bg);                // active background tint

    for (auto* btn : tab_btns_) {
        btn->setStyleSheet(css);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void PolymarketDetailPanel::apply_presentation_to_stats() {
    if (oi_box_) oi_box_->setVisible(presentation_.has_open_interest);
}

void PolymarketDetailPanel::render_status_badge(const PredictionMarket& market) {
    const auto badge = presentation_.status_badge(market);
    status_label_->setText(badge.text);
    status_label_->setToolTip(badge.tooltip);

    const QString bg_css = badge.bg.alpha() > 0
        ? QStringLiteral("rgba(%1,%2,%3,%4)")
              .arg(badge.bg.red()).arg(badge.bg.green()).arg(badge.bg.blue())
              .arg(badge.bg.alphaF(), 0, 'f', 2)
        : QStringLiteral("transparent");

    status_label_->setStyleSheet(
        QString("color: %1; background: %2; font-size: 8px; font-weight: 700; "
                "padding: 2px 8px; border: 1px solid %3; letter-spacing: 0.5px;")
            .arg(badge.fg.name(), bg_css, badge.fg.name()));
}

} // namespace fincept::screens::polymarket
