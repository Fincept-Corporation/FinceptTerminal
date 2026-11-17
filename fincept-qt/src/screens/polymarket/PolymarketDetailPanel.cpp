#include "screens/polymarket/PolymarketDetailPanel.h"

#include "core/events/EventBus.h"
#include "screens/polymarket/PolymarketActivityFeed.h"
#include "screens/polymarket/PolymarketOrderBook.h"
#include "screens/polymarket/PolymarketPriceChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonValue>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QTimeZone>
#include <QVBoxLayout>

#include <cmath>

namespace pm_fmt = fincept::ui::formatting;

namespace fincept::screens::polymarket {

using namespace fincept::ui;
namespace pmx = fincept::services::polymarket;
using namespace fincept::services::prediction;

/// Subject of a prediction market for the News screen's text search: the longest run of
/// capitalised words in the question, minus question words, articles, months and weekdays
/// ("Will the Fed cut rates in December?" -> "Fed", "Will Donald Trump win ...?" -> "Donald Trump").
/// Falls back to the first tag; empty when nothing usable is found.
static QString polymarket_news_query(const PredictionMarket& market) {
    static const QSet<QString> kStop = {
        "will", "who", "what", "which", "when", "where", "how", "why", "is", "are", "was", "were", "does", "do",
        "did", "can", "could", "should", "would", "the", "a", "an", "in", "on", "at", "by", "before", "after",
        "of", "for", "to", "and", "or", "yes", "no", "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december", "jan", "feb", "mar", "apr", "jun",
        "jul", "aug", "sep", "sept", "oct", "nov", "dec", "monday", "tuesday", "wednesday", "thursday",
        "friday", "saturday", "sunday", "q1", "q2", "q3", "q4", "end", "next", "this"};
    auto clean = [](QString w) {
        while (!w.isEmpty() && !w.front().isLetterOrNumber() && w.front() != QLatin1Char('$'))
            w.remove(0, 1);
        while (!w.isEmpty() && !w.back().isLetterOrNumber())
            w.chop(1);
        if (w.endsWith(QLatin1String("'s")) || w.endsWith(QStringLiteral("’s")))
            w.chop(2);
        return w;
    };
    // Lower-case words that may sit inside a name ("Bank of Japan", "Bank of the West").
    static const QSet<QString> kConnectors = {"of", "the", "and", "de", "&"};
    QStringList best;
    QStringList run;
    QStringList pending; // connectors seen since the last capitalised word
    auto close_run = [&]() {
        pending.clear();
        while (!run.isEmpty() && kStop.contains(run.front().toLower()))
            run.removeFirst();
        while (!run.isEmpty() && kStop.contains(run.back().toLower()))
            run.removeLast();
        if (run.size() > best.size())
            best = run;
        run.clear();
    };
    for (const QString& raw : market.question.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const QString w = clean(raw);
        if (!w.isEmpty() && w.front().isUpper()) {
            run << pending << w;
            pending.clear();
            if (raw.endsWith(QLatin1Char(',')) || raw.endsWith(QLatin1Char('?')) || raw.endsWith(QLatin1Char('.')))
                close_run(); // punctuation ends a name
        } else if (!run.isEmpty() && kConnectors.contains(w) && pending.size() < 2) {
            pending << w; // kept only if another capitalised word follows
        } else {
            close_run();
        }
    }
    close_run();
    if (!best.isEmpty())
        return best.join(QLatin1Char(' '));
    return market.tags.isEmpty() ? QString() : market.tags.first().trimmed();
}

PolymarketDetailPanel::PolymarketDetailPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("polyDetailPanel");
    build_ui();
    // A number / currency format change re-renders the money already on screen from
    // the values held here (no re-fetch, no rebuild of the outcome rows).
    pm_fmt::on_format_changed(this, [this]() {
        if (has_last_market_) {
            volume_label_->setText(presentation_.format_volume(last_market_.volume));
            liquidity_label_->setText(presentation_.format_liquidity(last_market_.liquidity));
            if (last_open_interest_ >= 0.0)
                oi_label_->setText(presentation_.format_volume(last_open_interest_));
        }
        if (has_last_balance_)
            set_balance(last_balance_);
        refresh_ticket_cost();
    });
}

void PolymarketDetailPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // The selected market's tabs, or — nothing selected yet — a skeleton / designed
    // state in their place (never "Select a market" over a column of dashes). The
    // selection is pushed by the screen, so the pane runs in feed mode: nothing of
    // its own is fetched, and no stale clock runs (the per-tab loads have
    // load_watchdog_ below; the list has the screen's 45 s watchdog).
    market_pane_ = new ui::AutoLoadPane(this);
    market_pane_->set_feed_mode(true);
    market_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    market_pane_->set_loading_text(tr("Loading markets…"));
    // set_market() fills the tabs itself; the payload only says "a market is on screen".
    market_pane_->set_render([](const QJsonValue&, const QDateTime&, bool) {});
    root->addWidget(market_pane_, 1);
    market_view_ = new QWidget(market_pane_->content_parent());
    market_pane_->set_content(market_view_);
    auto* vl = new QVBoxLayout(market_view_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Tab bar ───────────────────────────────────────────────────────────
    auto* tab_bar = new QWidget(market_view_);
    tab_bar->setObjectName("polyDetailTabBar");
    tab_bar->setFixedHeight(34);
    tab_bar->setStyleSheet(QString("QWidget#polyDetailTabBar { background: %1; border-bottom: 1px solid %2; }")
                               .arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    auto* thl = new QHBoxLayout(tab_bar);
    thl->setContentsMargins(0, 0, 0, 0);
    thl->setSpacing(0);

    const QStringList tab_names = {tr("OVERVIEW"), tr("ORDER BOOK"), tr("CHART"),    tr("TRADE"),
                                   tr("TRADES"),   tr("HOLDERS"),    tr("COMMENTS"), tr("RELATED")};
    for (int i = 0; i < tab_names.size(); ++i) {
        auto* btn = new QPushButton(tab_names[i]);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        btn->setAccessibleName(tab_names[i]);
        btn->setAccessibleDescription(tr("Show the %1 panel for the selected market").arg(tab_names[i]));
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
    // Click-to-price: a level click loads that price into the trade ticket.
    // Without this connection the order book's price_clicked signal had no
    // receiver, so clicking a level did nothing at all.
    connect(orderbook_, &PolymarketOrderBook::price_clicked, this, &PolymarketDetailPanel::on_book_price_clicked);
    stack_->addWidget(orderbook_); // 1

    price_chart_ = new PolymarketPriceChart;
    connect(price_chart_, &PolymarketPriceChart::interval_changed, this, &PolymarketDetailPanel::interval_changed);
    connect(price_chart_, &PolymarketPriceChart::outcome_changed, this, &PolymarketDetailPanel::outcome_changed);
    stack_->addWidget(price_chart_); // 2

    stack_->addWidget(create_trade_page()); // 3

    activity_feed_ = new PolymarketActivityFeed;
    connect(activity_feed_, &PolymarketActivityFeed::retry_requested, this, &PolymarketDetailPanel::reload_requested);
    stack_->addWidget(activity_feed_); // 4

    stack_->addWidget(create_holders_page());  // 5
    stack_->addWidget(create_comments_page()); // 6
    stack_->addWidget(create_related_page());  // 7

    vl->addWidget(stack_, 1);

    market_pane_->feed_waiting(); // the screen selects the first market as soon as the list arrives

    load_watchdog_ = new QTimer(this);
    load_watchdog_->setSingleShot(true);
    load_watchdog_->setInterval(30000);
    connect(load_watchdog_, &QTimer::timeout, this, [this]() {
        const unsigned pending = pending_loads_;
        for (Load which : {Load::Book, Load::Chart, Load::Trades, Load::Holders, Load::Comments, Load::Related}) {
            if (pending & (1u << static_cast<unsigned>(which)))
                set_load_error(which, tr("No answer from the exchange."));
        }
    });
}

void PolymarketDetailPanel::show_no_market(bool loading, const QString& reason) {
    // A market shown earlier must not stay under the skeleton / page (in feed mode
    // data on screen would otherwise win over waiting / idle).
    market_pane_->set_loading_text(tr("Loading markets…"));
    market_pane_->clear_result();
    if (loading) {
        market_pane_->feed_waiting();
        return;
    }
    market_pane_->feed_idle(
        tr("No market to show"),
        reason.isEmpty() ? tr("Pick a market on the left to see its price, order book, chart, trades and holders.")
                         : reason);
}

void PolymarketDetailPanel::set_loading(Load which) {
    pending_loads_ |= 1u << static_cast<unsigned>(which);
    load_watchdog_->start();
    switch (which) {
    case Load::Book:
        orderbook_->set_loading();
        break;
    case Load::Chart:
        price_chart_->set_loading();
        break;
    case Load::Trades:
        activity_feed_->set_loading();
        break;
    case Load::Holders:
        holders_state_.show_loading();
        break;
    case Load::Comments:
        comments_state_.show_loading();
        break;
    case Load::Related:
        related_state_.show_loading();
        break;
    }
}

void PolymarketDetailPanel::set_load_unavailable(Load which, const QString& message) {
    settle_load(which);
    const ui::EmptyState::Spec none = ui::EmptyState::no_data_spec(tr("Not available here"), message);
    switch (which) {
    case Load::Chart:
        price_chart_->set_unavailable(message);
        break;
    case Load::Book:
        orderbook_->clear();
        break;
    case Load::Trades:
        activity_feed_->set_trades({});
        break;
    case Load::Holders:
        holders_state_.show_state(none, {}, {}, true);
        break;
    case Load::Comments:
        comments_state_.show_state(none, {}, {}, true);
        break;
    case Load::Related:
        related_state_.show_state(none, {}, {}, true);
        break;
    }
}

void PolymarketDetailPanel::set_load_error(Load which, const QString& message) {
    settle_load(which);
    const auto retry = [this]() { emit reload_requested(); };
    switch (which) {
    case Load::Book:
        orderbook_->set_error(message);
        break;
    case Load::Chart:
        price_chart_->set_error(message);
        break;
    case Load::Trades:
        activity_feed_->set_error(message);
        break;
    case Load::Holders:
        if (!holders_state_.showing_content() || holders_table_->rowCount() == 0)
            holders_state_.show_state(ui::EmptyState::error_spec(message, tr("Could not load the holders")), retry, {},
                                      true);
        break;
    case Load::Comments:
        if (!comments_state_.showing_content())
            comments_state_.show_state(ui::EmptyState::error_spec(message, tr("Could not load the comments")), retry,
                                       {}, true);
        break;
    case Load::Related:
        if (!related_state_.showing_content())
            related_state_.show_state(ui::EmptyState::error_spec(message, tr("Could not load related markets")),
                                      retry, {}, true);
        break;
    }
}

QWidget* PolymarketDetailPanel::create_overview_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: %1; }"
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
    question_label_ = new QLabel(tr("Select a market to view details"));
    question_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700; background: transparent; "
                                           "line-height: 1.4;")
                                       .arg(colors::TEXT_PRIMARY()));
    question_label_->setWordWrap(true);
    // Exchange-supplied text. QLabel's default Qt::AutoText renders anything
    // markup-shaped as rich text, so pin the untrusted fields to PlainText.
    question_label_->setTextFormat(Qt::PlainText);
    question_label_->setMinimumHeight(36);
    vl->addWidget(question_label_);

    vl->addSpacing(10);

    // ── Status row (badge + outcome labels) ───────────────────────────────
    auto* status_row = new QWidget;
    status_row->setStyleSheet(QString("QWidget { background: transparent; }"
                                      "QPushButton#pmNewsBtn { color: %1; border: 1px solid %2; padding: 2px 8px;"
                                      "  font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; letter-spacing: 0.5px; }"
                                      "QPushButton#pmNewsBtn:hover { border-color: %1; }"
                                      "QPushButton#pmNewsBtn:disabled { color: %3; }")
                                  .arg(colors::AMBER(), colors::BORDER_DIM(), colors::TEXT_DIM()));
    auto* srl = new QHBoxLayout(status_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(6);

    status_label_ = new QLabel;
    srl->addWidget(status_label_);
    srl->addStretch(1);
    // Hand the market's subject to the News screen (a text search, see polymarket_news_query).
    news_btn_ = new QPushButton(tr("NEWS ↗"));
    news_btn_->setObjectName("pmNewsBtn");
    news_btn_->setCursor(Qt::PointingHandCursor);
    news_btn_->setEnabled(false);
    news_btn_->setAccessibleName(tr("Open related news"));
    connect(news_btn_, &QPushButton::clicked, this, [this]() {
        if (news_query_.isEmpty())
            return;
        EventBus::instance().publish(QStringLiteral("nav.open_symbol"),
                                     {{QStringLiteral("screen_id"), QStringLiteral("news")},
                                      {QStringLiteral("symbol"), news_query_}});
    });
    srl->addWidget(news_btn_);
    vl->addWidget(status_row);

    vl->addSpacing(12);

    // ── Stats: two rows of 3-4 cells each ────────────────────────────────
    // Row 1 of stats
    auto* stats_row1 = new QWidget;
    stats_row1->setStyleSheet(QString("background: %1; border: 1px solid %2; border-bottom: none;")
                                  .arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* sr1l = new QHBoxLayout(stats_row1);
    sr1l->setContentsMargins(0, 0, 0, 0);
    sr1l->setSpacing(0);

    auto make_stat_in = [&](QWidget* parent_row, QHBoxLayout* row_layout, const QString& lbl, QLabel*& val,
                            bool last = false, QWidget** box_out = nullptr) {
        auto* box = new QWidget(parent_row);
        box->setStyleSheet(
            last ? QString("background: transparent;")
                 : QString("background: transparent; border-right: 1px solid %1;").arg(colors::BORDER_DIM()));
        auto* bvl = new QVBoxLayout(box);
        bvl->setContentsMargins(12, 8, 12, 8);
        bvl->setSpacing(3);

        auto* lbl_w = new QLabel(lbl, box);
        lbl_w->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; letter-spacing: 0.8px; "
                                     "background: transparent;")
                                 .arg(colors::TEXT_SECONDARY()));
        // Cache the caption label so retranslateUi can re-apply its text.
        stat_caption_lbls_.append(lbl_w);

        val = new QLabel("—", box);
        val->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent;")
                               .arg(colors::TEXT_PRIMARY()));

        bvl->addWidget(lbl_w);
        bvl->addWidget(val);
        row_layout->addWidget(box, 1);
        if (box_out)
            *box_out = box;
    };

    make_stat_in(stats_row1, sr1l, tr("VOLUME"), volume_label_);
    make_stat_in(stats_row1, sr1l, tr("LIQUIDITY"), liquidity_label_, false, &liquidity_box_);
    make_stat_in(stats_row1, sr1l, tr("OPEN INT"), oi_label_, false, &oi_box_);
    make_stat_in(stats_row1, sr1l, tr("END DATE"), end_date_label_, true);
    vl->addWidget(stats_row1);

    // Row 2 of stats
    auto* stats_row2 = new QWidget;
    stats_row2->setStyleSheet(
        QString("background: %1; border: 1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* sr2l = new QHBoxLayout(stats_row2);
    sr2l->setContentsMargins(0, 0, 0, 0);
    sr2l->setSpacing(0);

    make_stat_in(stats_row2, sr2l, tr("MIDPOINT"), midpoint_label_);
    make_stat_in(stats_row2, sr2l, tr("SPREAD"), spread_label_);
    make_stat_in(stats_row2, sr2l, tr("LAST TRADE"), last_trade_label_, true);
    vl->addWidget(stats_row2);

    vl->addSpacing(14);

    // ── Outcome probability bars ──────────────────────────────────────────
    outcomes_header_ = new QLabel(tr("OUTCOMES"));
    auto* outcomes_header = outcomes_header_;
    outcomes_header->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; letter-spacing: 0.8px; "
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
    description_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; line-height: 1.5; background: transparent; "
                                              "border-top: 1px solid %2; padding-top: 12px;")
                                          .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM()));
    description_label_->setWordWrap(true);
    description_label_->setTextFormat(Qt::PlainText); // untrusted market copy
    vl->addWidget(description_label_);

    vl->addStretch(1);
    scroll->setWidget(page);
    return scroll;
}

QWidget* PolymarketDetailPanel::create_trade_page() {
    // Two-state stack: 0 = no-account placeholder, 1 = ticket form.
    ticket_stack_ = new QStackedWidget;

    // ── State 0: no usable account — what this exchange needs + CONNECT ACCOUNT,
    //    or "could not read the stored credential" + RETRY (refresh_account_page) ──
    no_acct_state_ = new ui::EmptyState(ticket_stack_);
    connect(no_acct_state_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (!account_read_error_.isEmpty())
            emit retry_credentials_requested();
        else
            emit connect_account_requested();
    });
    connect(no_acct_state_, &ui::EmptyState::secondary_clicked, this, &PolymarketDetailPanel::connect_account_requested);
    ticket_stack_->addWidget(no_acct_state_); // index 0
    refresh_account_page();

    // ── State 1: ticket form ─────────────────────────────────────────────────
    auto* form = new QWidget;
    form->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* fl = new QVBoxLayout(form);
    fl->setContentsMargins(16, 14, 16, 14);
    fl->setSpacing(10);

    // Balance + position header
    auto* hdr = new QWidget;
    hdr->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 3px;")
                           .arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* hdrl = new QHBoxLayout(hdr);
    hdrl->setContentsMargins(10, 7, 10, 7);
    hdrl->setSpacing(0);

    auto* bal_col = new QVBoxLayout;
    bal_col->setSpacing(2);
    bal_caption_lbl_ = new QLabel(tr("AVAILABLE"));
    auto* bal_lbl = bal_caption_lbl_;
    bal_lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; letter-spacing: 0.8px; "
                                   "background: transparent;")
                               .arg(colors::TEXT_SECONDARY()));
    ticket_balance_lbl_ = new QLabel("—");
    ticket_balance_lbl_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent;").arg(colors::TEXT_PRIMARY()));
    bal_col->addWidget(bal_lbl);
    bal_col->addWidget(ticket_balance_lbl_);

    auto* pos_col = new QVBoxLayout;
    pos_col->setSpacing(2);
    pos_col->setAlignment(Qt::AlignRight);
    pos_caption_lbl_ = new QLabel(tr("POSITION"));
    auto* pos_lbl = pos_caption_lbl_;
    pos_lbl->setAlignment(Qt::AlignRight);
    pos_lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; letter-spacing: 0.8px; "
                                   "background: transparent;")
                               .arg(colors::TEXT_SECONDARY()));
    ticket_position_lbl_ = new QLabel("—");
    ticket_position_lbl_->setAlignment(Qt::AlignRight);
    ticket_position_lbl_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent;").arg(colors::TEXT_PRIMARY()));
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

    ticket_buy_btn_ = new QPushButton(tr("BUY"));
    ticket_sell_btn_ = new QPushButton(tr("SELL"));
    for (auto* b : {ticket_buy_btn_, ticket_sell_btn_}) {
        b->setFixedHeight(32);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    ticket_buy_btn_->setAccessibleName(tr("Buy side"));
    ticket_sell_btn_->setAccessibleName(tr("Sell side"));
    connect(ticket_buy_btn_, &QPushButton::clicked, this, [this]() {
        ticket_side_ = "BUY";
        refresh_ticket_side_style();
        refresh_ticket_cost();
    });
    connect(ticket_sell_btn_, &QPushButton::clicked, this, [this]() {
        ticket_side_ = "SELL";
        refresh_ticket_side_style();
        refresh_ticket_cost();
    });
    srl->addWidget(ticket_buy_btn_);
    srl->addWidget(ticket_sell_btn_);
    fl->addWidget(side_row);

    // Outcome selector
    auto make_label = [&](const QString& text) -> QLabel* {
        auto* l = new QLabel(text);
        l->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; letter-spacing: 0.8px; "
                                 "background: transparent;")
                             .arg(colors::TEXT_SECONDARY()));
        // Cache so retranslateUi can re-apply (these are fixed field captions).
        trade_form_caption_lbls_.append(l);
        return l;
    };
    const QString input_ss =
        QString("QComboBox, QLineEdit {"
                "  background: %1; color: %2; border: 1px solid %3;"
                "  border-radius: 2px; padding: 4px 8px; font-size: " + fincept::ui::fonts::px(-3) + "px;"
                "}"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView {"
                "  background: %1; color: %2; border: 1px solid %3; selection-background-color: %4; selection-color: %2;"
                "}")
            .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_MED(), colors::BG_HOVER());

    fl->addWidget(make_label(tr("OUTCOME")));
    ticket_outcome_cb_ = new QComboBox;
    ticket_outcome_cb_->setStyleSheet(input_ss);
    ticket_outcome_cb_->setAccessibleName(tr("Order outcome"));
    fl->addWidget(ticket_outcome_cb_);

    // Price + Size row
    auto* ps_row = new QWidget;
    ps_row->setStyleSheet("background: transparent;");
    auto* psl = new QHBoxLayout(ps_row);
    psl->setContentsMargins(0, 0, 0, 0);
    psl->setSpacing(8);

    auto* price_col = new QVBoxLayout;
    price_col->setSpacing(4);
    price_col->addWidget(make_label(tr("PRICE (0–1)")));
    ticket_price_edit_ = new QLineEdit;
    ticket_price_edit_->setPlaceholderText("0.50");
    ticket_price_edit_->setStyleSheet(input_ss);
    ticket_price_edit_->setAccessibleName(tr("Limit price between 0 and 1"));
    // Reject non-numeric / out-of-range keystrokes up front — the submit path
    // still re-validates, but this stops a fat-finger "5" ever reaching it.
    {
        auto* pv = new QDoubleValidator(0.0, 1.0, 6, ticket_price_edit_);
        pv->setNotation(QDoubleValidator::StandardNotation);
        pv->setLocale(QLocale::c());
        ticket_price_edit_->setValidator(pv);
    }
    price_col->addWidget(ticket_price_edit_);

    auto* size_col = new QVBoxLayout;
    size_col->setSpacing(4);
    size_col->addWidget(make_label(tr("SIZE")));
    ticket_size_edit_ = new QLineEdit;
    ticket_size_edit_->setPlaceholderText("10");
    ticket_size_edit_->setStyleSheet(input_ss);
    ticket_size_edit_->setAccessibleName(tr("Order size in shares"));
    {
        auto* sv = new QDoubleValidator(0.0, 1e9, 4, ticket_size_edit_);
        sv->setNotation(QDoubleValidator::StandardNotation);
        sv->setLocale(QLocale::c());
        ticket_size_edit_->setValidator(sv);
    }
    size_col->addWidget(ticket_size_edit_);

    psl->addLayout(price_col);
    psl->addLayout(size_col);
    fl->addWidget(ps_row);

    // Live notional readout — a prediction-market order's worst-case cost is
    // price × size, and without it the user is doing mental arithmetic before
    // committing real funds.
    ticket_cost_lbl_ = new QLabel(QStringLiteral("—"));
    ticket_cost_lbl_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; background: transparent;").arg(colors::TEXT_SECONDARY()));
    fl->addWidget(ticket_cost_lbl_);
    connect(ticket_price_edit_, &QLineEdit::textChanged, this, [this]() { refresh_ticket_cost(); });
    connect(ticket_size_edit_, &QLineEdit::textChanged, this, [this]() { refresh_ticket_cost(); });

    // Order type
    fl->addWidget(make_label(tr("ORDER TYPE")));
    ticket_type_cb_ = new QComboBox;
    // GTC/FOK/FAK are protocol order-type codes sent in the order request —
    // not translated.
    ticket_type_cb_->addItems({"GTC", "FOK", "FAK"});
    ticket_type_cb_->setStyleSheet(input_ss);
    ticket_type_cb_->setAccessibleName(tr("Order type"));
    fl->addWidget(ticket_type_cb_);

    // Submit button
    ticket_submit_btn_ = new QPushButton(tr("PLACE ORDER"));
    ticket_submit_btn_->setProperty("fincept_agent_risk", "order");
    ticket_submit_btn_->setFixedHeight(34);
    ticket_submit_btn_->setCursor(Qt::PointingHandCursor);
    ticket_submit_btn_->setAccessibleName(tr("Place order"));
    connect(ticket_submit_btn_, &QPushButton::clicked, this, &PolymarketDetailPanel::on_submit_clicked);
    fl->addWidget(ticket_submit_btn_);

    // Tab order across the ticket so the form is fully keyboard-drivable.
    QWidget::setTabOrder(ticket_buy_btn_, ticket_sell_btn_);
    QWidget::setTabOrder(ticket_sell_btn_, ticket_outcome_cb_);
    QWidget::setTabOrder(ticket_outcome_cb_, ticket_price_edit_);
    QWidget::setTabOrder(ticket_price_edit_, ticket_size_edit_);
    QWidget::setTabOrder(ticket_size_edit_, ticket_type_cb_);
    QWidget::setTabOrder(ticket_type_cb_, ticket_submit_btn_);

    // Status label (feedback)
    ticket_status_lbl_ = new QLabel;
    ticket_status_lbl_->setAlignment(Qt::AlignCenter);
    ticket_status_lbl_->setWordWrap(true);
    ticket_status_lbl_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(colors::TEXT_DIM()));
    fl->addWidget(ticket_status_lbl_);

    fl->addStretch(1);
    ticket_stack_->addWidget(form); // index 1

    refresh_ticket_side_style();
    return ticket_stack_;
}

void PolymarketDetailPanel::refresh_ticket_side_style() {
    if (!ticket_buy_btn_ || !ticket_sell_btn_ || !ticket_submit_btn_)
        return;
    const bool is_buy = (ticket_side_ == "BUY");
    const QString active_bg = is_buy ? colors::POSITIVE() : colors::NEGATIVE();
    const QString inactive_bg = colors::BG_RAISED();
    const QString active_text = colors::TEXT_ON_ACCENT();
    const QString inactive_text = colors::TEXT_SECONDARY();

    ticket_buy_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: none; "
                                           "font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; }"
                                           "QPushButton:hover { opacity: 0.9; }")
                                       .arg(is_buy ? active_bg : inactive_bg, is_buy ? active_text : inactive_text));
    ticket_sell_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: none; "
                                            "font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; }"
                                            "QPushButton:hover { opacity: 0.9; }")
                                        .arg(!is_buy ? active_bg : inactive_bg, !is_buy ? active_text : inactive_text));
    ticket_submit_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: none; "
                                              "border-radius: 2px; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; }"
                                              "QPushButton:hover { background: %3; }"
                                              "QPushButton:disabled { background: %4; color: %5; }")
                                          .arg(active_bg, active_text, colors::alpha(QColor(active_bg), 0.85),
                                               colors::BG_RAISED(), colors::TEXT_TERTIARY()));
}

void PolymarketDetailPanel::refresh_ticket_cost() {
    if (!ticket_cost_lbl_)
        return;
    bool price_ok = false, size_ok = false;
    const double price = ticket_price_edit_ ? ticket_price_edit_->text().toDouble(&price_ok) : 0.0;
    const double size = ticket_size_edit_ ? ticket_size_edit_->text().toDouble(&size_ok) : 0.0;
    if (!price_ok || !size_ok || price <= 0.0 || size <= 0.0) {
        ticket_cost_lbl_->setText(QStringLiteral("—"));
        return;
    }
    const QString ccy = presentation_.currency.isEmpty() ? QStringLiteral("USD") : presentation_.currency;
    // BUY costs price × size up front; SELL of held shares releases that
    // notional. Either way it's the amount at stake on this ticket — in the
    // venue's settlement currency, to that currency's own decimals whatever the
    // user's decimals setting (money_precise).
    const QString amount = pm_fmt::money_precise(price * size, ccy);
    ticket_cost_lbl_->setText(ticket_side_ == QStringLiteral("BUY") ? tr("≈ %1 cost").arg(amount)
                                                                    : tr("≈ %1 proceeds").arg(amount));
}

void PolymarketDetailPanel::on_book_price_clicked(double price) {
    if (!ticket_price_edit_ || !ticket_stack_)
        return;
    if (ticket_stack_->currentIndex() != 1)
        return; // trading disabled — nothing to fill
    if (price <= 0.0 || price >= 1.0)
        return;
    // The book always shows the FIRST outcome's token; a price taken from it is only
    // meaningful for that outcome, so point the ticket at it too (it used to keep
    // whichever outcome was selected, e.g. NO, with a YES-book price).
    if (ticket_outcome_cb_ && ticket_outcome_cb_->count() > 0)
        ticket_outcome_cb_->setCurrentIndex(0);
    ticket_price_edit_->setText(QString::number(price, 'f', 4));
    set_active_tab(kTabTrade);
    ticket_price_edit_->setFocus();
}

void PolymarketDetailPanel::on_submit_clicked() {
    if (!has_last_market_)
        return;
    // Hard double-submit guard — the button is also disabled while in flight,
    // but a queued click (or a market refresh racing the disable) could still
    // land here.
    if (submit_in_flight_)
        return;

    auto reject = [this](const QString& msg) {
        ticket_status_lbl_->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(colors::DANGER()));
        ticket_status_lbl_->setText(msg);
    };

    bool price_ok = false, size_ok = false;
    const double price = ticket_price_edit_->text().toDouble(&price_ok);
    const double size = ticket_size_edit_->text().toDouble(&size_ok);
    if (!price_ok || price <= 0.0 || price >= 1.0) {
        reject(tr("Invalid price — enter a value between 0 and 1"));
        return;
    }
    if (!size_ok || size <= 0.0) {
        reject(tr("Invalid size — must be > 0"));
        return;
    }
    // Kalshi trades whole contracts and KalshiAdapter::place_order sends int(size), so
    // a fractional size was silently truncated: the confirm dialog read "5.90 shares"
    // while 5 were ordered.
    if (presentation_.exchange_id == QStringLiteral("kalshi") && std::abs(size - std::round(size)) > 1e-9) {
        reject(tr("Kalshi contracts are whole numbers — enter an integer size"));
        return;
    }
    // The book's own constraints win; until a book has supplied them, fall back to
    // the exchange-wide ones. Without the fallback a Kalshi ticket priced 0.555 was
    // confirmed as "Exact price: 0.5550" and then rounded to 56c by the adapter.
    const double tick_size = book_tick_size_ > 0.0 ? book_tick_size_ : exchange_tick_size_;
    const double min_size = book_min_size_ > 0.0 ? book_min_size_ : exchange_min_size_;
    if (min_size > 0.0 && size < min_size) {
        reject(tr("Size below the exchange minimum of %1").arg(min_size, 0, 'f', 2));
        return;
    }
    // Tick alignment — an off-tick limit price is rejected by the CLOB after
    // the round-trip (or rounded by the adapter), so catch it here and say
    // exactly what's wrong. The order sent must be the order confirmed.
    if (tick_size > 0.0) {
        const double ticks = price / tick_size;
        if (std::abs(ticks - std::round(ticks)) > 1e-6) {
            const double snapped = std::round(ticks) * tick_size;
            reject(tr("Price must be a multiple of the %1 tick — try %2")
                       .arg(tick_size, 0, 'f', 4)
                       .arg(snapped, 0, 'f', 4));
            return;
        }
    }

    const int outcome_idx = ticket_outcome_cb_->currentIndex();
    if (outcome_idx < 0 || outcome_idx >= last_market_.outcomes.size()) {
        reject(tr("Select an outcome before placing an order"));
        return;
    }
    const auto& outcome = last_market_.outcomes[outcome_idx];
    if (outcome.asset_id.isEmpty()) {
        // Without an asset id the exchange cannot know *which* side of the
        // market this order is for — refuse rather than send an ambiguous one.
        reject(tr("This outcome has no tradable asset id — cannot place order"));
        return;
    }
    if (last_market_.closed) {
        reject(tr("Market is closed — no further trading"));
        return;
    }

    // Explicit confirmation. This is a real-money order on a real exchange;
    // the previous flow fired straight from the button with no read-back of
    // what was about to be sent.
    const QString ccy = presentation_.currency.isEmpty() ? QStringLiteral("USD") : presentation_.currency;
    // Built line by line rather than one chained arg() chain: exchange-supplied
    // text (the question, the outcome name) can itself contain "%n", and a
    // later .arg() would happily substitute into it.
    QStringList lines;
    lines << tr("%1  %2  ×  %3 shares  @  %4")
                 .arg(ticket_side_, outcome.name, QString::number(size, 'f', 2),
                      presentation_.format_price(price));
    // The cents rendering above can round (and Kalshi's dollars view shows 2 dp) — state
    // the exact price that will be sent so the confirmation can't disagree with it.
    lines << tr("Exact price: %1").arg(QString::number(price, 'f', 4));
    lines << QString();
    lines << tr("Market: %1").arg(last_market_.question.left(120));
    lines << tr("Order type: %1").arg(ticket_type_cb_->currentText());
    lines << (ticket_side_ == QStringLiteral("BUY") ? tr("Cost: %1").arg(pm_fmt::money_precise(price * size, ccy))
                                                    : tr("Proceeds: %1").arg(pm_fmt::money_precise(price * size, ccy)));
    const QString summary = lines.join(QLatin1Char('\n'));
    QMessageBox confirm(this);
    confirm.setWindowTitle(tr("Confirm order"));
    confirm.setIcon(QMessageBox::Warning);
    confirm.setText(tr("Place this order on %1?").arg(presentation_.display_name));
    confirm.setInformativeText(summary);
    confirm.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    confirm.setDefaultButton(QMessageBox::Cancel);
    if (auto* ok_btn = confirm.button(QMessageBox::Ok))
        ok_btn->setText(tr("Place order"));
    if (auto* ok_btn = confirm.button(QMessageBox::Ok))
        ok_btn->setProperty("fincept_agent_risk", "order");
    if (confirm.exec() != QMessageBox::Ok) {
        ticket_status_lbl_->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(colors::TEXT_DIM()));
        ticket_status_lbl_->setText(tr("Cancelled — no order sent"));
        return;
    }

    OrderRequest req;
    req.key = last_market_.key;
    req.asset_id = outcome.asset_id;
    req.side = ticket_side_;
    req.order_type = ticket_type_cb_->currentText();
    req.price = price;
    req.size = size;

    ticket_status_lbl_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(colors::TEXT_DIM()));
    ticket_status_lbl_->setText(tr("Submitting…"));
    submit_in_flight_ = true;
    ticket_submit_btn_->setEnabled(false);
    emit place_order(req);
}

void PolymarketDetailPanel::set_balance(const AccountBalance& balance) {
    if (!ticket_balance_lbl_)
        return;
    last_balance_ = balance;
    has_last_balance_ = true;
    // The account's own currency as the adapter reports it (USDC / USD).
    ticket_balance_lbl_->setText(
        pm_fmt::money(balance.available, balance.currency.isEmpty() ? presentation_.currency : balance.currency));
}

void PolymarketDetailPanel::set_positions(const QVector<PredictionPosition>& positions) {
    if (!ticket_position_lbl_ || !has_last_market_)
        return;
    // Positions are per-outcome on a binary market. Summing YES and NO into a
    // single "N shares" figure told the user they were long something they may
    // in fact be flat on — break it out by outcome, and prefer the outcome
    // currently selected in the ticket.
    const int sel = ticket_outcome_cb_ ? ticket_outcome_cb_->currentIndex() : -1;
    const QString sel_asset =
        (sel >= 0 && sel < last_market_.outcomes.size()) ? last_market_.outcomes[sel].asset_id : QString();

    double selected_size = 0.0;
    double selected_avg = 0.0;
    QStringList others;
    for (const auto& p : positions) {
        if (p.market_id != last_market_.key.market_id)
            continue;
        if (p.size <= 0.0)
            continue;
        if (!sel_asset.isEmpty() && p.asset_id == sel_asset) {
            selected_size += p.size;
            selected_avg = p.avg_price;
        } else {
            others << tr("%1 %2").arg(p.size, 0, 'f', 2).arg(p.outcome.isEmpty() ? tr("other") : p.outcome);
        }
    }

    QString text;
    if (selected_size > 0.0) {
        text = selected_avg > 0.0 ? tr("%1 @ %2").arg(selected_size, 0, 'f', 2).arg(presentation_.format_price(
                                        selected_avg))
                                  : tr("%1 shares").arg(selected_size, 0, 'f', 2);
    } else {
        text = tr("No position");
    }
    if (!others.isEmpty())
        text += QStringLiteral("  (") + others.join(QStringLiteral(", ")) + QStringLiteral(")");
    ticket_position_lbl_->setText(text);
    ticket_position_lbl_->setToolTip(tr("Position in the outcome selected in the ticket; other outcomes in brackets."));
}

void PolymarketDetailPanel::on_order_result(const OrderResult& result) {
    if (!ticket_status_lbl_ || !ticket_submit_btn_)
        return;
    submit_in_flight_ = false;
    ticket_submit_btn_->setEnabled(true);
    if (result.ok) {
        ticket_status_lbl_->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(colors::SUCCESS()));
        ticket_status_lbl_->setText(tr("Order placed ✓  ID: %1").arg(result.order_id.left(12)));
        ticket_price_edit_->clear();
        ticket_size_edit_->clear();
        refresh_ticket_cost();
    } else {
        ticket_status_lbl_->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(colors::DANGER()));
        ticket_status_lbl_->setText(result.error_message.isEmpty() ? result.error_code : result.error_message);
    }
}

void PolymarketDetailPanel::set_trading_enabled(bool enabled) {
    if (!ticket_stack_)
        return;
    ticket_stack_->setCurrentIndex(enabled ? 1 : 0);
}

void PolymarketDetailPanel::set_account_read_error(const QString& reason) {
    if (reason == account_read_error_)
        return;
    account_read_error_ = reason;
    refresh_account_page();
}

void PolymarketDetailPanel::refresh_account_page() {
    if (!no_acct_state_)
        return;
    ui::EmptyState::Spec spec;
    if (!account_read_error_.isEmpty()) {
        // Stored but unreadable is not "no account": never ask for a key that may exist.
        spec = ui::EmptyState::error_spec(account_read_error_ + QLatin1Char(' ') +
                                              tr("Nothing was changed — the stored account is still there."),
                                          tr("Could not read the stored credential"), tr("RETRY"));
        spec.secondary = tr("ACCOUNT SETTINGS");
    } else if (presentation_.exchange_id == QStringLiteral("kalshi")) {
        spec = ui::EmptyState::no_data_spec(
            tr("Connect a Kalshi account to trade"),
            tr("Kalshi orders are signed with an API key. Create one on kalshi.com (Account > API keys), then add "
               "its key id and RSA private key here."),
            tr("CONNECT ACCOUNT"));
        spec.glyph = QStringLiteral("⇄"); // ⇄
        spec.glyph_tone = ui::EmptyState::Tone::Accent;
        spec.details = {{tr("Needs"), tr("API key id + RSA private key")},
                        {tr("Stored in"), tr("the terminal's secure storage")}};
        spec.details_heading = tr("WHAT YOU NEED");
    } else if (presentation_.exchange_id == QStringLiteral("fincept")) {
        spec = ui::EmptyState::no_data_spec(
            tr("Trading is not open on Fincept markets yet"),
            tr("These are sample markets for previewing the exchange. Prices, order book and charts work; orders "
               "will open when the Fincept matching engine launches."));
        spec.note = tr("PREVIEW");
        spec.note_tone = ui::EmptyState::Tone::Info;
    } else {
        spec = ui::EmptyState::no_data_spec(
            tr("Connect a Polymarket account to trade"),
            tr("Polymarket orders are signed with your wallet key. Add the key and your Polymarket account "
               "(funder) address once."),
            tr("CONNECT ACCOUNT"));
        spec.glyph = QStringLiteral("⇄"); // ⇄
        spec.glyph_tone = ui::EmptyState::Tone::Accent;
        spec.details = {{tr("Needs"), tr("Wallet private key + funder address")},
                        {tr("Stored in"), tr("the terminal's secure storage")}};
        spec.details_heading = tr("WHAT YOU NEED");
    }
    no_acct_state_->apply(spec);
}

void PolymarketDetailPanel::set_order_types(const QStringList& types) {
    if (!ticket_type_cb_ || types.isEmpty())
        return;
    QStringList current;
    for (int i = 0; i < ticket_type_cb_->count(); ++i)
        current << ticket_type_cb_->itemText(i);
    if (current == types)
        return;
    const QString prev = ticket_type_cb_->currentText();
    ticket_type_cb_->clear();
    ticket_type_cb_->addItems(types);
    const int idx = ticket_type_cb_->findText(prev);
    ticket_type_cb_->setCurrentIndex(idx >= 0 ? idx : 0);
}

QWidget* PolymarketDetailPanel::create_holders_page() {
    auto* page = new QStackedWidget;
    holders_table_ = new QTableWidget(page);
    holders_table_->setColumnCount(5);
    holders_table_->setHorizontalHeaderLabels({tr("RANK"), tr("OUTCOME"), tr("TRADER"), tr("SIZE"), tr("AVG PRICE")});
    holders_table_->horizontalHeader()->setStretchLastSection(true);
    holders_table_->verticalHeader()->setVisible(false);
    holders_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    holders_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    holders_table_->setShowGrid(false);
    // Alternate rows come from the view (and the theme's row_alt colour), so the
    // banding survives sorting and theme changes.
    holders_table_->setAlternatingRowColors(true);
    holders_table_->setStyleSheet(QString("QTableWidget { background: %1; alternate-background-color: %7; color: %2;"
                                          " border: none; font-size: " + fincept::ui::fonts::px(-4) + "px; }"
                                          "QTableWidget::item { padding: 4px 8px; border-bottom: 1px solid %3; }"
                                          "QTableWidget::item:selected { background: %4; color: %2; }"
                                          "QHeaderView::section {"
                                          "  background: %5; color: %6; border: none;"
                                          "  border-bottom: 1px solid %3;"
                                          "  padding: 5px 8px; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; letter-spacing: 0.5px;"
                                          "}"
                                          "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                                          "QScrollBar::handle:vertical { background: %3; min-height: 20px; }"
                                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                                      .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(),
                                           colors::BG_HOVER(), colors::BG_RAISED(), colors::TEXT_SECONDARY(),
                                           colors::ROW_ALT()));
    page->addWidget(holders_table_);
    holders_state_ = PmViewState(page, holders_table_, ui::LoadingSkeleton::Shape::Table, tr("Loading top holders…"));
    holders_state_.show_loading();
    return page;
}

QWidget* PolymarketDetailPanel::create_comments_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: %1; }"
                                  "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                                  "QScrollBar::handle:vertical { background: %2; min-height: 20px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(colors::BG_BASE(), colors::BORDER_BRIGHT()));
    comments_container_ = new QWidget;
    comments_container_->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* vl = new QVBoxLayout(comments_container_);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(8);
    vl->addStretch(1);
    scroll->setWidget(comments_container_);
    // Cards, or a skeleton until the event's comments land / a designed "none" state.
    auto* page = new QStackedWidget;
    page->addWidget(scroll);
    comments_state_ = PmViewState(page, scroll, ui::LoadingSkeleton::Shape::Text, tr("Loading comments…"));
    comments_state_.show_loading();
    return page;
}

QWidget* PolymarketDetailPanel::create_related_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: %1; }"
                                  "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                                  "QScrollBar::handle:vertical { background: %2; min-height: 20px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(colors::BG_BASE(), colors::BORDER_BRIGHT()));
    related_container_ = new QWidget;
    related_container_->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* vl = new QVBoxLayout(related_container_);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(6);
    vl->addStretch(1);
    scroll->setWidget(related_container_);
    auto* page = new QStackedWidget;
    page->addWidget(scroll);
    related_state_ = PmViewState(page, scroll, ui::LoadingSkeleton::Shape::Cards, tr("Loading related markets…"));
    related_state_.show_loading();
    return page;
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

void PolymarketDetailPanel::sync_ticket_outcomes(const PredictionMarket& market, bool same_market) {
    if (!ticket_outcome_cb_)
        return;
    // Preserve the user's outcome choice across a same-market refresh. A naive
    // clear()+addItem() silently snapped the selection back to index 0, which
    // on a binary market turns a pending "buy NO" ticket into "buy YES".
    const QString prev = same_market ? ticket_outcome_cb_->currentText() : QString();
    QStringList names;
    names.reserve(market.outcomes.size());
    for (const auto& o : market.outcomes)
        names << o.name;
    if (same_market && names == ticket_outcome_names_)
        return; // nothing changed — leave the widget (and selection) alone

    const QSignalBlocker block(ticket_outcome_cb_);
    ticket_outcome_cb_->clear();
    ticket_outcome_cb_->addItems(names);
    ticket_outcome_names_ = names;
    if (!prev.isEmpty()) {
        const int idx = ticket_outcome_cb_->findText(prev);
        if (idx >= 0)
            ticket_outcome_cb_->setCurrentIndex(idx);
    }
}

void PolymarketDetailPanel::set_market(const PredictionMarket& market) {
    // A live WS price tick re-enters here with the *same* market. Treat that
    // as a price refresh, not a fresh selection: keep the trade ticket, the
    // active tab, and any in-flight submit intact.
    // Require a non-empty id: two different markets that both lack an id must
    // never be treated as the same one (that would carry a stale ticket over).
    const bool same_market = has_last_market_ && !market.key.market_id.isEmpty() &&
                             market.key.market_id == last_market_.key.market_id &&
                             market.key.exchange_id == last_market_.key.exchange_id;

    last_market_ = market;
    has_last_market_ = true;
    market_pane_->feed_data(QJsonValue(true)); // a market is on screen: the tabs, not the skeleton / page

    question_label_->setText(market.question);
    news_query_ = polymarket_news_query(market);
    if (news_btn_) {
        news_btn_->setEnabled(!news_query_.isEmpty());
        news_btn_->setToolTip(news_query_.isEmpty() ? tr("No subject found in this market's question")
                                                    : tr("Search News for \"%1\"").arg(news_query_));
    }
    volume_label_->setText(presentation_.format_volume(market.volume));
    liquidity_label_->setText(presentation_.format_liquidity(market.liquidity));
    end_date_label_->setText(market.end_date_iso.left(10));
    if (!same_market) {
        midpoint_label_->setText("—");
        spread_label_->setText("—");
        last_trade_label_->setText("—");
        oi_label_->setText("—");
        last_open_interest_ = -1.0;
    }
    if (presentation_.exchange_id == QStringLiteral("kalshi")) {
        // Kalshi's market object already carries these — but only Polymarket's
        // separate open-interest / price-summary replies ever filled the boxes,
        // so on Kalshi they stayed "—". Quotes are YES-side dollars.
        const QLocale loc;
        oi_label_->setText(market.open_interest > 0.0
                               ? tr("%1 ct").arg(loc.toString(market.open_interest, 'f', 0))
                               : QStringLiteral("—"));
        last_open_interest_ = -1.0; // contracts, not money
        // A Kalshi contract pays one unit of its settlement currency (USD).
        oi_label_->setToolTip(tr("Open interest in contracts (each pays %1 if it settles in the money).")
                                  .arg(pm_fmt::money(1.0,
                                                     presentation_.currency.isEmpty() ? QStringLiteral("USD")
                                                                                      : presentation_.currency,
                                                     pm_fmt::Compact::None, 0)));
        const double last = market.extras.value(QStringLiteral("last_price_dollars")).toDouble();
        last_trade_label_->setText(last > 0.0 ? presentation_.format_price(last) : QStringLiteral("—"));
        const double bid = market.extras.contains(QStringLiteral("yes_bid_dollars"))
                               ? market.extras.value(QStringLiteral("yes_bid_dollars")).toDouble()
                               : (market.outcomes.isEmpty() ? 0.0 : market.outcomes.first().price);
        const double ask = market.extras.value(QStringLiteral("yes_ask_dollars")).toDouble();
        if (bid > 0.0 && ask > 0.0 && ask < 1.0 && ask >= bid) {
            midpoint_label_->setText(presentation_.format_price((bid + ask) / 2.0));
            spread_label_->setText(presentation_.format_price(ask - bid));
        } else {
            // One side has no quote: a midpoint / spread would be fiction.
            midpoint_label_->setText(QStringLiteral("—"));
            spread_label_->setText(QStringLiteral("—"));
        }
        midpoint_label_->setToolTip(tr("YES bid %1 / ask %2")
                                        .arg(bid > 0.0 ? presentation_.format_price(bid) : QStringLiteral("—"),
                                             ask > 0.0 && ask < 1.0 ? presentation_.format_price(ask)
                                                                    : QStringLiteral("—")));
    } else {
        oi_label_->setToolTip(QString());
        midpoint_label_->setToolTip(QString());
    }

    render_status_badge(market);

    // ── Populate ticket outcome combo ─────────────────────────────────────
    sync_ticket_outcomes(market, same_market);
    if (!same_market) {
        if (ticket_status_lbl_)
            ticket_status_lbl_->clear();
        if (ticket_submit_btn_)
            ticket_submit_btn_->setEnabled(true);
        submit_in_flight_ = false;
        if (ticket_position_lbl_)
            ticket_position_lbl_->setText("—");
        // Constraints belong to the previous market's book — drop them until
        // the new market's book arrives.
        book_tick_size_ = 0.0;
        book_min_size_ = 0.0;
        if (ticket_price_edit_)
            ticket_price_edit_->clear();
        if (ticket_size_edit_)
            ticket_size_edit_->clear();
        refresh_ticket_cost();
        // The previous market's holders must not sit under this market's title
        // while its own list loads (the holders reply names no market).
        if (holders_table_) {
            holders_table_->clearSpans();
            holders_table_->setRowCount(0);
        }
    }

    // ── Rebuild outcome probability bars ──────────────────────────────────
    auto* layout = qobject_cast<QVBoxLayout*>(outcome_container_->layout());
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < market.outcomes.size(); ++i) {
        const auto& outcome = market.outcomes[i];
        // Kalshi prices are bids; see ExchangePresentation::display_probability.
        const double pct = presentation_.display_probability(market, i);
        const int pct_int = qRound(pct * 100.0);

        // First outcome in the exchange accent, the next ones in the theme's
        // categorical chart colours (outcomes are categories, not up / down).
        QColor bar_color;
        if (i == 0)
            bar_color = presentation_.accent;
        else if (i < 5)
            bar_color = colors::chart_color(i);
        else
            bar_color = QColor(colors::TEXT_SECONDARY());

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
        name_lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 600; background: transparent;")
                                    .arg(colors::TEXT_PRIMARY()));

        auto* pct_lbl = new QLabel(QString("%1%").arg(pct_int), top_line);
        pct_lbl->setStyleSheet(
            QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; background: transparent;").arg(bar_color.name()));

        // Full formatted price (right side)
        auto* price_lbl = new QLabel(presentation_.format_price(pct), top_line);
        price_lbl->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 600; background: transparent;")
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
        bar_track->setStyleSheet(QString("background: %1; border-radius: 0px;").arg(colors::BG_RAISED()));
        // Fill widget sits inside the track with proportional width — use
        // a layout that we stretch manually via resize.
        auto* bar_fill = new QWidget(bar_track);
        bar_fill->setStyleSheet(QString("background: %1; border-radius: 0px;").arg(bar_color.name()));
        // Store pct on the fill widget so it can be re-sized on layout.
        bar_fill->setProperty("fill_pct", pct);

        // We need the fill to track the track width — use a nested layout.
        auto* bar_layout = new QHBoxLayout(bar_track);
        bar_layout->setContentsMargins(0, 0, 0, 0);
        bar_layout->setSpacing(0);
        bar_layout->addWidget(bar_fill, qRound(pct * 1000));
        if (pct < 1.0)
            bar_layout->addStretch(qRound((1.0 - pct) * 1000));

        rl->addWidget(bar_track);
        layout->addWidget(row);
    }

    description_label_->setText(market.description.left(600));

    QStringList labels;
    labels.reserve(market.outcomes.size());
    for (const auto& o : market.outcomes)
        labels.append(o.name);
    // set_outcome_labels() resets the chart's outcome combo to index 0, which
    // on a same-market refresh would silently pull the chart back to the first
    // outcome while the user is looking at another one.
    if (!same_market)
        price_chart_->set_outcome_labels(labels);

    // Only jump back to OVERVIEW on an actual market change — a WS price tick
    // used to yank the user out of the TRADE / ORDER BOOK tab mid-entry.
    if (!same_market)
        set_active_tab(0);
}

void PolymarketDetailPanel::set_price_summary(const pmx::PriceSummary& summary) {
    midpoint_label_->setText(presentation_.format_price(summary.midpoint));
    spread_label_->setText(presentation_.format_price(summary.spread));
    last_trade_label_->setText(presentation_.format_price(summary.last_trade_price));
}

void PolymarketDetailPanel::set_exchange_constraints(double tick_size, double min_order_size) {
    exchange_tick_size_ = tick_size > 0.0 ? tick_size : 0.0;
    exchange_min_size_ = min_order_size > 0.0 ? min_order_size : 0.0;
}

void PolymarketDetailPanel::set_order_book(const PredictionOrderBook& book) {
    settle_load(Load::Book);
    orderbook_->set_data(book);
    // Capture the exchange's price/size constraints so the ticket can reject
    // an off-tick or sub-minimum order locally instead of after a round-trip.
    if (book.tick_size > 0.0)
        book_tick_size_ = book.tick_size;
    if (book.min_order_size > 0.0)
        book_min_size_ = book.min_order_size;
}

void PolymarketDetailPanel::set_price_history(const PriceHistory& history) {
    settle_load(Load::Chart);
    price_chart_->set_price_history(history);
}

void PolymarketDetailPanel::set_trades(const QVector<PredictionTrade>& trades) {
    settle_load(Load::Trades);
    activity_feed_->set_trades(trades);
}

void PolymarketDetailPanel::set_top_holders(const QVector<pmx::TopHolder>& holders) {
    // The Data API returns one ranked list per outcome token; they used to be
    // concatenated with no outcome column (two rank-1 rows, two rank-2 rows, …)
    // and SIZE / AVG PRICE sorted as text ("99.99" above "1500.00").
    settle_load(Load::Holders);
    holders_table_->setSortingEnabled(false);
    holders_table_->clearSpans();
    if (holders.isEmpty()) {
        holders_table_->setRowCount(0);
        holders_state_.show_state(ui::EmptyState::no_data_spec(tr("No holder data for this market"),
                                                               tr("Polymarket lists no open positions for this "
                                                                  "market yet.")),
                                  {}, {}, /*compact=*/true);
        return;
    }
    holders_state_.show_content();
    const QLocale loc;
    holders_table_->setRowCount(holders.size());
    for (int i = 0; i < holders.size(); ++i) {
        const pmx::TopHolder& h = holders[i];
        const int rank_v = h.rank > 0 ? h.rank : i + 1;
        auto* rank = new ui::NumericTableWidgetItem(QString::number(rank_v), rank_v);
        rank->setTextAlignment(Qt::AlignCenter);
        holders_table_->setItem(i, 0, rank);

        // Outcome held, named from the selected market; sorts by outcome index so
        // the default view is outcome 1's ranking, then outcome 2's.
        QString outcome_name = QStringLiteral("—");
        if (h.outcome_index >= 0 && h.outcome_index < last_market_.outcomes.size())
            outcome_name = last_market_.outcomes[h.outcome_index].name;
        else if (h.outcome_index >= 0)
            outcome_name = tr("Outcome %1").arg(h.outcome_index + 1);
        auto* outcome = new ui::NumericTableWidgetItem(outcome_name, h.outcome_index >= 0 ? h.outcome_index : 99);
        if (h.outcome_index >= 0) {
            outcome->setForeground(h.outcome_index == 0 ? presentation_.accent
                                                        : colors::chart_color(qMin(h.outcome_index, 4)));
        }
        holders_table_->setItem(i, 1, outcome);

        holders_table_->setItem(i, 2, new QTableWidgetItem(h.display_name));

        auto* size_item = new ui::NumericTableWidgetItem(loc.toString(h.position_size, 'f', 2), h.position_size);
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        holders_table_->setItem(i, 3, size_item);

        // Same price convention as the rest of the panel (cents / dollars).
        auto* price_item = new ui::NumericTableWidgetItem(
            h.entry_price > 0.0 ? presentation_.format_price(h.entry_price) : QStringLiteral("—"), h.entry_price);
        price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        holders_table_->setItem(i, 4, price_item);
    }
    holders_table_->resizeColumnsToContents();
    // Start grouped by outcome; the sort is stable, so each outcome keeps the
    // API's rank order.
    holders_table_->horizontalHeader()->setSortIndicator(1, Qt::AscendingOrder);
    holders_table_->setSortingEnabled(true);
}

void PolymarketDetailPanel::set_comments(const QVector<pmx::Comment>& comments) {
    settle_load(Load::Comments);
    auto* vl = qobject_cast<QVBoxLayout*>(comments_container_->layout());
    while (vl->count() > 0) {
        auto* item = vl->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (comments.isEmpty()) {
        vl->addStretch(1);
        comments_state_.show_state(ui::EmptyState::no_data_spec(tr("No comments yet"),
                                                                tr("Nobody has commented on this event on Polymarket "
                                                                   "yet.")),
                                   {}, {}, /*compact=*/true);
        return;
    } else {
        comments_state_.show_content();
        for (const auto& c : comments) {
            auto* card = new QWidget(comments_container_);
            card->setStyleSheet(QString("background: %1; border: 1px solid %2; border-left: 2px solid %3;")
                                    .arg(colors::BG_SURFACE(), colors::BORDER_DIM(), colors::AMBER()));
            auto* cvl = new QVBoxLayout(card);
            cvl->setContentsMargins(10, 8, 10, 8);
            cvl->setSpacing(4);

            // Comments are arbitrary user-generated text from the exchange —
            // the single most hostile input on this screen. Render as plain
            // text so markup can't restyle or spoof the panel.
            auto* author = new QLabel(c.author.isEmpty() ? c.author_address.left(12) + "…" : c.author);
            author->setTextFormat(Qt::PlainText);
            author->setStyleSheet(
                QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; background: transparent;").arg(colors::AMBER()));

            auto* body = new QLabel(c.body.left(280));
            body->setTextFormat(Qt::PlainText);
            body->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent; line-height: 1.4;")
                                    .arg(colors::TEXT_PRIMARY()));
            body->setWordWrap(true);

            auto* meta =
                new QLabel(QDateTime::fromSecsSinceEpoch(c.created_at, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm") +
                           (c.likes > 0 ? tr("  · %1 likes").arg(c.likes) : QString()));
            meta->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-6) + "px; background: transparent;").arg(colors::TEXT_DIM()));

            cvl->addWidget(author);
            cvl->addWidget(body);
            cvl->addWidget(meta);
            vl->addWidget(card);
        }
    }
    vl->addStretch(1);
}

void PolymarketDetailPanel::set_related_markets(const QVector<PredictionMarket>& markets) {
    settle_load(Load::Related);
    auto* vl = qobject_cast<QVBoxLayout*>(related_container_->layout());
    while (vl->count() > 0) {
        auto* item = vl->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (markets.isEmpty()) {
        vl->addStretch(1);
        related_state_.show_state(ui::EmptyState::no_data_spec(tr("No related markets"),
                                                               tr("This event has no other markets.")),
                                  {}, {}, /*compact=*/true);
        return;
    } else {
        related_state_.show_content();
        for (const auto& m : markets) {
            auto* card = new QPushButton(related_container_);
            card->setStyleSheet(QString("QPushButton {"
                                        "  background: %1;"
                                        "  color: %2;"
                                        "  border: 1px solid %3;"
                                        "  text-align: left;"
                                        "  padding: 8px 12px;"
                                        "  font-size: " + fincept::ui::fonts::px(-4) + "px;"
                                        "  font-weight: 600;"
                                        "}"
                                        "QPushButton:hover { background: %4; color: %5; border-color: %6; }")
                                    .arg(colors::BG_SURFACE(), colors::TEXT_SECONDARY(), colors::BORDER_DIM(),
                                         colors::BG_HOVER(), colors::TEXT_PRIMARY(), colors::BORDER_BRIGHT()));
            // Escape '&' so a question containing one doesn't turn into a
            // button mnemonic (and lose the character).
            card->setText(QString(m.question.left(70) + (m.question.size() > 70 ? "…" : "")).replace('&', "&&"));
            card->setToolTip(m.question);
            card->setAccessibleName(tr("Related market: %1").arg(m.question.left(80)));
            card->setCursor(Qt::PointingHandCursor);
            connect(card, &QPushButton::clicked, this, [this, m]() { emit related_market_clicked(m); });
            vl->addWidget(card);
        }
    }
    vl->addStretch(1);
}

void PolymarketDetailPanel::set_open_interest(double oi) {
    last_open_interest_ = oi >= 0.0 ? oi : -1.0;
    oi_label_->setText(presentation_.format_volume(oi));
}

void PolymarketDetailPanel::set_series_tooltip(const QString& tooltip) {
    if (question_label_)
        question_label_->setToolTip(tooltip);
}

void PolymarketDetailPanel::set_polymarket_extras_enabled(bool enabled) {
    const int extra_tabs[] = {kTabHolders, kTabComments, kTabRelated};
    for (int idx : extra_tabs) {
        if (idx >= 0 && idx < tab_btns_.size())
            tab_btns_[idx]->setVisible(enabled);
    }
    if (!enabled) {
        if (holders_table_)
            holders_table_->setRowCount(0);

        auto clear_container = [](QWidget* container, const QString& msg) {
            if (!container)
                return;
            auto* vl = qobject_cast<QVBoxLayout*>(container->layout());
            if (!vl)
                return;
            while (vl->count() > 0) {
                auto* item = vl->takeAt(0);
                if (item->widget())
                    item->widget()->deleteLater();
                delete item;
            }
            auto* empty = new QLabel(msg);
            empty->setStyleSheet(
                QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent;").arg(colors::TEXT_DIM()));
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
    news_query_.clear();
    if (news_btn_) {
        news_btn_->setEnabled(false);
        news_btn_->setToolTip(QString());
    }
    submit_in_flight_ = false;
    book_tick_size_ = 0.0;
    book_min_size_ = 0.0;
    ticket_outcome_names_.clear();
    if (ticket_outcome_cb_)
        ticket_outcome_cb_->clear();
    if (ticket_submit_btn_)
        ticket_submit_btn_->setEnabled(true);
    if (ticket_status_lbl_)
        ticket_status_lbl_->clear();
    if (ticket_price_edit_)
        ticket_price_edit_->clear();
    if (ticket_size_edit_)
        ticket_size_edit_->clear();
    refresh_ticket_cost();
    question_label_->setText(tr("Select a market to view details"));
    volume_label_->setText("—");
    liquidity_label_->setText("—");
    end_date_label_->setText("—");
    midpoint_label_->setText("—");
    spread_label_->setText("—");
    last_trade_label_->setText("—");
    oi_label_->setText("—");
    last_open_interest_ = -1.0;
    status_label_->clear();
    description_label_->clear();
    orderbook_->clear();
    activity_feed_->clear();
    // Nothing selected now: the screen reloads the list and selects its first market.
    pending_loads_ = 0;
    if (load_watchdog_)
        load_watchdog_->stop();
    market_pane_->set_loading_text(tr("Loading markets…"));
    market_pane_->clear_result(); // the previous market must not stay under the skeleton
    market_pane_->feed_waiting();

    // Clear outcomes
    if (auto* vl = qobject_cast<QVBoxLayout*>(outcome_container_->layout())) {
        while (vl->count() > 0) {
            auto* item = vl->takeAt(0);
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }
}

// ── Presentation wiring ─────────────────────────────────────────────────────

void PolymarketDetailPanel::set_presentation(const ExchangePresentation& p) {
    const bool accent_changed = p.accent != presentation_.accent;
    const bool oi_changed = p.has_open_interest != presentation_.has_open_interest;
    const bool exchange_changed = p.exchange_id != presentation_.exchange_id;
    const bool extras_changed = p.has_polymarket_extras != presentation_.has_polymarket_extras;

    presentation_ = p;

    if (accent_changed)
        apply_accent_to_tabs();
    if (oi_changed || exchange_changed)
        apply_presentation_to_stats();
    if (extras_changed)
        set_polymarket_extras_enabled(p.has_polymarket_extras);
    if (exchange_changed)
        refresh_account_page(); // what the TRADE tab asks for depends on the exchange

    if (has_last_market_)
        set_market(last_market_);
}

void PolymarketDetailPanel::apply_accent_to_tabs() {
    const QColor& a = presentation_.accent;
    const QString accent_str = QStringLiteral("rgba(%1,%2,%3,1.0)").arg(a.red()).arg(a.green()).arg(a.blue());
    const QString accent_bg = QStringLiteral("rgba(%1,%2,%3,0.12)").arg(a.red()).arg(a.green()).arg(a.blue());

    const QString css = QString("QPushButton {"
                                       "  background: transparent;"
                                       "  color: %1;"
                                       "  border: none;"
                                       "  border-bottom: 2px solid transparent;"
                                       "  font-size: " + fincept::ui::fonts::px(-5) + "px;"
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
                            .arg(colors::TEXT_SECONDARY()) // inactive text
                            .arg(colors::TEXT_PRIMARY())   // hover text
                            .arg(colors::BORDER_BRIGHT())  // hover underline
                            .arg(accent_str)               // active text + underline
                            .arg(accent_bg);               // active background tint

    // Only when the text changed: re-run on every appearance change (the screen's
    // appearance_settled handler), and a re-set sheet re-polishes the button.
    for (auto* btn : tab_btns_) {
        if (btn->styleSheet() == css)
            continue;
        btn->setStyleSheet(css);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void PolymarketDetailPanel::apply_presentation_to_stats() {
    if (oi_box_)
        oi_box_->setVisible(presentation_.has_open_interest);
    // Kalshi publishes no liquidity figure (liquidity_dollars is null / absent),
    // so the box only ever read "$0".
    if (liquidity_box_)
        liquidity_box_->setVisible(presentation_.exchange_id != QStringLiteral("kalshi"));
}

void PolymarketDetailPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PolymarketDetailPanel::retranslateUi() {
    // Tab bar labels (logical tab is index-based; labels are display-only).
    const QStringList tab_names = {tr("OVERVIEW"), tr("ORDER BOOK"), tr("CHART"),    tr("TRADE"),
                                   tr("TRADES"),   tr("HOLDERS"),    tr("COMMENTS"), tr("RELATED")};
    for (int i = 0; i < tab_btns_.size() && i < tab_names.size(); ++i)
        tab_btns_[i]->setText(tab_names[i]);

    // Overview stat captions (declared order matches make_stat_in calls).
    const QStringList stat_caps = {tr("VOLUME"),   tr("LIQUIDITY"), tr("OPEN INT"),  tr("END DATE"),
                                   tr("MIDPOINT"), tr("SPREAD"),    tr("LAST TRADE")};
    for (int i = 0; i < stat_caption_lbls_.size() && i < stat_caps.size(); ++i)
        if (stat_caption_lbls_[i])
            stat_caption_lbls_[i]->setText(stat_caps[i]);

    if (outcomes_header_)
        outcomes_header_->setText(tr("OUTCOMES"));

    // Trade page captions.
    refresh_account_page();
    if (bal_caption_lbl_)
        bal_caption_lbl_->setText(tr("AVAILABLE"));
    if (pos_caption_lbl_)
        pos_caption_lbl_->setText(tr("POSITION"));
    const QStringList form_caps = {tr("OUTCOME"), tr("PRICE (0–1)"), tr("SIZE"), tr("ORDER TYPE")};
    for (int i = 0; i < trade_form_caption_lbls_.size() && i < form_caps.size(); ++i)
        if (trade_form_caption_lbls_[i])
            trade_form_caption_lbls_[i]->setText(form_caps[i]);
    if (ticket_buy_btn_)
        ticket_buy_btn_->setText(tr("BUY"));
    if (ticket_sell_btn_)
        ticket_sell_btn_->setText(tr("SELL"));
    if (ticket_submit_btn_)
        ticket_submit_btn_->setText(tr("PLACE ORDER"));

    // Holders table headers.
    if (holders_table_)
        holders_table_->setHorizontalHeaderLabels(
            {tr("RANK"), tr("OUTCOME"), tr("TRADER"), tr("SIZE"), tr("AVG PRICE")});

    // Re-render question/badge/outcome bars for the current market so the
    // status badge text picks up the new language. Per-row data refreshes
    // through the normal data path.
    if (has_last_market_) {
        render_status_badge(last_market_);
    } else if (question_label_) {
        question_label_->setText(tr("Select a market to view details"));
    }
}

void PolymarketDetailPanel::render_status_badge(const PredictionMarket& market) {
    const auto badge = presentation_.status_badge(market);
    status_label_->setText(badge.text);
    status_label_->setToolTip(badge.tooltip);

    const QString bg_css = badge.bg.alpha() > 0 ? QStringLiteral("rgba(%1,%2,%3,%4)")
                                                      .arg(badge.bg.red())
                                                      .arg(badge.bg.green())
                                                      .arg(badge.bg.blue())
                                                      .arg(badge.bg.alphaF(), 0, 'f', 2)
                                                : QStringLiteral("transparent");

    const QString sheet = QStringLiteral("color: %1; background: %2; font-size: %3px; font-weight: 700; "
                                         "padding: 2px 8px; border: 1px solid %1; letter-spacing: 0.5px;")
                              .arg(badge.fg.name(), bg_css)
                              .arg(fincept::ui::fonts::px(-6));
    if (status_label_->styleSheet() != sheet) // every market refresh re-renders the badge
        status_label_->setStyleSheet(sheet);
}

} // namespace fincept::screens::polymarket
