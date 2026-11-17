#include "screens/dashboard/widgets/QuickTradeWidget.h"

#include "core/events/EventBus.h"
#include "core/market/MarketProfile.h"
#include "core/symbol/SymbolContext.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/DashboardFormat.h"
#include "screens/equity_research/EquityResearchLinks.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVariantMap>

namespace fincept::screens::widgets {

static QString input_style(const QString& border_color = "") {
    QString bc = border_color.isEmpty() ? ui::colors::BORDER_MED() : border_color;
    return QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                   "font-size: " + fincept::ui::fonts::px(-3) + "px; padding: 4px 6px; }"
                   "QLineEdit:focus { border-color: %4; }")
        .arg(ui::colors::BG_BASE())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(bc)
        .arg(ui::colors::AMBER());
}

static QString combo_style() {
    return QString("QComboBox { background: %1; color: %2; border: 1px solid %3; "
                   "font-size: " + fincept::ui::fonts::px(-3) + "px; padding: 4px 6px; }"
                   "QComboBox::drop-down { border: none; width: 16px; }"
                   "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; }")
        .arg(ui::colors::BG_BASE())
        .arg(ui::colors::TEXT_PRIMARY())
        .arg(ui::colors::BORDER_MED());
}

QuickTradeWidget::QuickTradeWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("QUICK TRADE"), parent, ui::colors::AMBER()) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    // Order entry: follows a link group only once the user links the tile.
    enable_symbol_link(/*follows=*/true, /*joins_default_group=*/false);

    // ── Symbol search bar ──
    auto* search_row = new QWidget(this);
    auto* srl = new QHBoxLayout(search_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(6);

    symbol_input_ = new QLineEdit;
    symbol_input_->setPlaceholderText(tr("Symbol (e.g. AAPL)"));
    // A fresh tile opens on a quote, not an empty card: the region's largest
    // stock (the config's symbol, or the link group's on the first show, wins).
    symbol_input_->setText(region_default_symbol());
    srl->addWidget(symbol_input_, 1);

    lookup_btn_ = new QPushButton(tr("LOOKUP"));
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

    // No BID / ASK row: the quote feed (yfinance) carries no bid/ask, so it only
    // ever read "—". The order ticket this tile opens shows the live market.
    auto* price_col = new QVBoxLayout;
    price_col->setAlignment(Qt::AlignRight);
    price_label_ = new QLabel("--");
    price_label_->setAlignment(Qt::AlignRight);
    price_col->addWidget(price_label_);
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

    // BUY / SELL only: the Equity Trading order ticket this hands off to has no
    // short side.
    side_combo_ = new QComboBox;
    side_combo_->addItems({tr("BUY"), tr("SELL")});
    st_row->addWidget(side_combo_, 1);

    order_type_ = new QComboBox;
    order_type_->addItems({tr("MARKET"), tr("LIMIT"), tr("STOP")});
    st_row->addWidget(order_type_, 1);
    vl->addLayout(st_row);

    // Qty + Limit price row
    auto* qp_row = new QHBoxLayout;
    qp_row->setSpacing(6);

    qty_lbl_ = new QLabel(tr("QTY"));
    qp_row->addWidget(qty_lbl_);

    qty_input_ = new QLineEdit("10");
    qty_input_->setValidator(new QDoubleValidator(0, 1e9, 4, qty_input_));
    qp_row->addWidget(qty_input_, 1);

    price_lbl_ = new QLabel(tr("PRICE"));
    qp_row->addWidget(price_lbl_);

    price_input_ = new QLineEdit;
    price_input_->setPlaceholderText(tr("market"));
    qp_row->addWidget(price_input_, 1);
    vl->addLayout(qp_row);

    // Estimated total
    est_total_ = new QLabel(tr("EST. TOTAL  --"));
    vl->addWidget(est_total_);

    // Hand-off button: opens the Equity Trading order ticket pre-filled with
    // this symbol, side and reference price. The ticket owns account selection,
    // paper/live routing, quantity / type / limit price and the confirmation —
    // this tile never places an order itself.
    submit_btn_ = new QPushButton(tr("OPEN BUY TICKET"));
    submit_btn_->setFixedHeight(32);
    submit_btn_->setToolTip(tr("Open the Equity Trading order ticket for this symbol and side. Quantity, order "
                               "type and limit price are confirmed on the ticket."));
    vl->addWidget(submit_btn_);

    // ── Accessibility ──
    symbol_input_->setAccessibleName(tr("Symbol"));
    lookup_btn_->setAccessibleName(tr("Look up symbol"));
    side_combo_->setAccessibleName(tr("Order side"));
    order_type_->setAccessibleName(tr("Order type"));
    qty_input_->setAccessibleName(tr("Quantity"));
    price_input_->setAccessibleName(tr("Limit / stop price"));
    submit_btn_->setAccessibleName(tr("Open order ticket"));
    setTabOrder(symbol_input_, lookup_btn_);
    setTabOrder(lookup_btn_, side_combo_);
    setTabOrder(side_combo_, order_type_);
    setTabOrder(order_type_, qty_input_);
    setTabOrder(qty_input_, price_input_);
    setTabOrder(price_input_, submit_btn_);

    // ── Connections ──
    connect(lookup_btn_, &QPushButton::clicked, this, &QuickTradeWidget::lookup_symbol);
    connect(symbol_input_, &QLineEdit::returnPressed, this, &QuickTradeWidget::lookup_symbol);
    connect(side_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QuickTradeWidget::on_side_changed);
    connect(submit_btn_, &QPushButton::clicked, this, &QuickTradeWidget::submit_order);
    connect(order_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        price_input_->setEnabled(idx != 0); // disable for MARKET
        price_input_->setPlaceholderText(idx == 0 ? tr("market") : QStringLiteral("0.00"));
        update_estimate();
    });
    // The type starts on MARKET, but the box was only disabled after the combo
    // changed once — a price could be typed into a market order.
    price_input_->setEnabled(order_type_->currentIndex() != 0);
    price_input_->setValidator(new QDoubleValidator(0, 1e9, 6, price_input_));
    // The estimate used to follow only the quantity box and always priced at
    // the last trade — wrong for a LIMIT/STOP order with a price entered.
    connect(qty_input_, &QLineEdit::textChanged, this, [this](const QString&) { update_estimate(); });
    connect(price_input_, &QLineEdit::textChanged, this, [this](const QString&) { update_estimate(); });
    connect(this, &BaseWidget::refresh_requested, this, &QuickTradeWidget::lookup_symbol);

    apply_styles();
    apply_config(cfg);

    // Initial lookup
    set_loading(true);
    // Hub path: initial lookup happens from showEvent.
}

QJsonObject QuickTradeWidget::config() const {
    QJsonObject o;
    const QString sym = current_symbol_.isEmpty() ? symbol_input_->text().trimmed().toUpper() : current_symbol_;
    if (!sym.isEmpty())
        o.insert("symbol", sym);
    return o;
}

QString QuickTradeWidget::region_default_symbol() {
    for (const QString& s : core::market::MarketProfileStore::instance().current().movers) {
        const QString sym = s.trimmed().toUpper();
        if (!sym.isEmpty() && is_tradable_equity(sym))
            return sym;
    }
    return QStringLiteral("AAPL");
}

void QuickTradeWidget::apply_config(const QJsonObject& cfg) {
    const QString sym = cfg.value("symbol").toString().trimmed().toUpper();
    if (!sym.isEmpty())
        symbol_from_config_ = true;
    if (sym.isEmpty() || sym == symbol_input_->text().trimmed().toUpper())
        return;
    symbol_input_->setText(sym);
    if (isVisible())
        lookup_symbol();
}

void QuickTradeWidget::on_linked_symbol(const SymbolRef& ref) {
    const QString sym = ref.symbol.trimmed().toUpper();
    // Indices, FX, futures and crypto pairs cannot be ordered here.
    if (sym.isEmpty() || !is_tradable_equity(sym) || sym == current_symbol_)
        return;
    symbol_input_->setText(sym);
    lookup_symbol(); // persists the symbol; subscribes only while visible
}

SymbolRef QuickTradeWidget::linked_symbol() const {
    const QString sym = current_symbol_.isEmpty() ? symbol_input_->text().trimmed() : current_symbol_;
    return sym.isEmpty() ? SymbolRef{} : SymbolRef::equity(sym);
}

void QuickTradeWidget::update_estimate() {
    const double qty = qty_input_->text().toDouble();
    const bool priced_order = order_type_->currentIndex() != 0;
    const double entered = price_input_->text().toDouble();
    const double px = (priced_order && entered > 0) ? entered : current_price_;
    if (qty > 0 && px > 0)
        est_total_->setText(tr("EST. TOTAL  %1").arg(dash_fmt::grouped(qty * px, 2)));
    else
        est_total_->setText(tr("EST. TOTAL  --"));
}

void QuickTradeWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!first_show_done_) {
        first_show_done_ = true;
        // A fresh tile in a link group (duplicate, template) starts on the group's
        // symbol — known only now: the canvas restores the group after construction.
        if (!symbol_from_config_ && link_group() != SymbolGroup::None) {
            const SymbolRef ref = SymbolContext::instance().group_symbol(link_group());
            const QString sym = ref.is_valid() ? ref.symbol.trimmed().toUpper() : QString();
            if (!sym.isEmpty() && is_tradable_equity(sym))
                symbol_input_->setText(sym);
        }
    }
    // Quote only (class B, < 2 s); the order ticket is never opened by itself.
    if (!hub_active_)
        lookup_symbol();
}

void QuickTradeWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void QuickTradeWidget::apply_styles() {
    set_css_cached(symbol_input_, input_style());
    set_css_cached(lookup_btn_, QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                                       "font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: bold; padding: 4px 10px; }"
                                       "QPushButton:hover { background: %4; }")
                                   .arg(ui::colors::BG_RAISED())
                                   .arg(ui::colors::AMBER())
                                   .arg(ui::colors::AMBER())
                                   .arg(ui::colors::BG_HOVER()));

    set_css_cached(quote_card_, QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    set_css_cached(sym_label_, QString("color: %1; font-size: " + fincept::ui::fonts::px(0) + "px; font-weight: bold; background: transparent;")
                                  .arg(ui::colors::TEXT_PRIMARY()));
    // The change label carries the quote's direction colour (apply_quote); only
    // a card with no quote yet gets the neutral one here.
    if (current_price_ <= 0) {
        set_css_cached(change_label_, QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;")
                                         .arg(ui::colors::TEXT_SECONDARY()));
        reset_css_cache(change_label_);
    }
    set_css_cached(price_label_, QString("color: %1; font-size: " + fincept::ui::fonts::px(6) + "px; font-weight: bold; background: transparent;")
                                    .arg(ui::colors::TEXT_PRIMARY()));

    set_css_cached(separator_, QString("background: %1;").arg(ui::colors::BORDER_DIM()));

    set_css_cached(side_combo_, combo_style());
    set_css_cached(order_type_, combo_style());

    set_css_cached(qty_lbl_, 
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
    set_css_cached(qty_input_, input_style());
    set_css_cached(price_lbl_, 
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
    set_css_cached(price_input_, input_style(ui::colors::BORDER_DIM()));

    set_css_cached(est_total_, 
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));

    // Submit button color depends on current side selection
    on_side_changed(side_combo_->currentIndex());
}

void QuickTradeWidget::on_theme_changed() {
    apply_styles();
}

void QuickTradeWidget::lookup_symbol() {
    QString sym = symbol_input_->text().trimmed().toUpper();
    if (sym.isEmpty()) {
        untrack_topics();
        set_loading(false);
        return;
    }

    // The first lookup on show just confirms the configured symbol.
    const bool symbol_changed = !current_symbol_.isEmpty() && sym != current_symbol_;
    // The last price belongs to the previous symbol: forget it until this one's
    // quote lands, so neither the estimate nor the order ticket's reference
    // price can use it.
    if (sym != current_symbol_)
        current_price_ = 0;
    current_symbol_ = sym;
    if (symbol_changed) {
        emit config_changed(config());
        publish_symbol(sym); // no-op when unlinked or already the group's symbol
    }
    // Subscribing while hidden (e.g. an MCP "refresh all") would keep the
    // topic polled until the next hide; showEvent() looks up again.
    if (!isVisible())
        return;
    auto& hub = datahub::DataHub::instance();
    // Swap subscription to the new symbol; drop the old one first.
    hub.unsubscribe(this);
    const QString topic = QStringLiteral("market:quote:") + sym;
    // Spinner only when this symbol's quote is not on screen yet (new symbol —
    // also one changed while hidden — or nothing delivered), and BEFORE
    // subscribing: subscribe() hands over a fresh cached quote synchronously,
    // and the old trailing set_loading(true) put the spinner back over it until
    // the watchdog's "No data yet" fired on a tile that had data.
    if (!topic_has_data(topic))
        set_loading(true);
    // Producer errors: with no quote yet the reason replaces the spinner; once
    // a quote is up a failed refresh only marks the tile STALE (BaseWidget).
    track_topics({topic});
    hub.subscribe(this, topic, [this, topic](const QVariant& v) {
        if (!v.canConvert<services::QuoteData>())
            return;
        note_topic_value(topic);
        set_loading(false);
        apply_quote(v.value<services::QuoteData>());
    });
    // Kick a fresh fetch so the card updates immediately even if the
    // cached TTL hasn't expired.
    hub.request(topic);
    hub_active_ = true;
}

void QuickTradeWidget::apply_quote(const services::QuoteData& q) {
    current_price_ = q.price;
    current_symbol_ = q.symbol.isEmpty() ? current_symbol_ : q.symbol;

    sym_label_->setText(current_symbol_);
    price_label_->setText(dash_fmt::price(q.price, current_symbol_));

    const double chg = q.change_pct;
    const QString arrow = chg > 0 ? QStringLiteral("▲ ") : chg < 0 ? QStringLiteral("▼ ") : QString();
    QString chg_col = chg > 0   ? ui::colors::POSITIVE()
                      : chg < 0 ? ui::colors::NEGATIVE()
                                : ui::colors::TEXT_SECONDARY();
    change_label_->setText(arrow + dash_fmt::signed_pct(chg));
    set_css_cached(change_label_, QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(chg_col));

    update_estimate();
}

void QuickTradeWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void QuickTradeWidget::on_side_changed(int idx) {
    // BUY = green, SELL = red
    QString color = (idx == 0) ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
    submit_btn_->setText(idx == 0 ? tr("OPEN BUY TICKET") : tr("OPEN SELL TICKET"));
    submit_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %3; border: none; "
                                       "font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; }"
                                       "QPushButton:hover { background: %2; }")
                                   .arg(color, color, ui::colors::TEXT_ON_ACCENT()));
}

void QuickTradeWidget::submit_order() {
    const QString sym = current_symbol_.isEmpty() ? symbol_input_->text().trimmed().toUpper() : current_symbol_;
    // Non-blocking boxes (BaseWidget::notify_async): a static QMessageBox here
    // ran a nested event loop on a tile that could be destroyed under it.
    if (sym.isEmpty()) {
        notify_async(QMessageBox::Warning, tr("Quick Trade"), tr("Please enter a symbol."));
        return;
    }
    // Index levels, FX / futures and crypto pairs have no equity broker route.
    const auto route = equity_links::trade_route(sym);
    if (!route.routable) {
        notify_async(QMessageBox::Information, tr("Quick Trade"),
                     tr("%1 cannot be traded from the equity order ticket — index levels, FX, futures and crypto "
                        "pairs have no broker route.")
                         .arg(sym));
        return;
    }

    // This tile has no broker/account binding, so it never places or stages an
    // order itself. It used to show "preview widget — no order was placed" and
    // only navigate, dropping the side the user picked. Hand off to the Equity
    // Trading order ticket instead — the same `equity.open_order_ticket` event
    // Equity Research's BUY / SELL uses. The ticket picks a connected/paper
    // account whose broker serves this market (match_exchanges), owns
    // quantity, MARKET/LIMIT and limit price, paper/live routing and the
    // explicit confirmation. `price` is the ticket's reference price — also the
    // paper fill price of a MARKET order — so it is the last quote of this
    // symbol (0 until one arrives: the ticket then refuses a paper market fill
    // rather than using a stale price), never a limit typed here.
    EventBus::instance().publish(QStringLiteral("equity.open_order_ticket"),
                                 QVariantMap{{"symbol", route.bare},
                                             {"exchange", route.order_exchange},
                                             {"match_exchanges", route.match_exchanges},
                                             {"is_buy", side_combo_->currentIndex() == 0},
                                             {"price", current_price_}});
}

void QuickTradeWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("QUICK TRADE"));
    if (lookup_btn_)
        lookup_btn_->setText(tr("LOOKUP"));
    if (qty_lbl_)
        qty_lbl_->setText(tr("QTY"));
    if (price_lbl_)
        price_lbl_->setText(tr("PRICE"));
    if (symbol_input_)
        symbol_input_->setPlaceholderText(tr("Symbol (e.g. AAPL)"));
    // Combo items and the bid/ask/total labels were previously left in the old
    // language after a live switch.
    if (order_type_) {
        const int cur = order_type_->currentIndex();
        const QStringList items = {tr("MARKET"), tr("LIMIT"), tr("STOP")};
        for (int i = 0; i < order_type_->count() && i < items.size(); ++i)
            order_type_->setItemText(i, items[i]);
        order_type_->setCurrentIndex(cur);
        if (price_input_)
            price_input_->setPlaceholderText(cur == 0 ? tr("market") : QStringLiteral("0.00"));
    }
    if (side_combo_) {
        const int cur = side_combo_->currentIndex();
        const QStringList items = {tr("BUY"), tr("SELL")};
        for (int i = 0; i < side_combo_->count() && i < items.size(); ++i)
            side_combo_->setItemText(i, items[i]);
        side_combo_->setCurrentIndex(cur);
    }
    if (est_total_ && qty_input_ && order_type_ && price_input_)
        update_estimate();
    if (submit_btn_) {
        submit_btn_->setToolTip(tr("Open the Equity Trading order ticket for this symbol and side. Quantity, order "
                                   "type and limit price are confirmed on the ticket."));
        submit_btn_->setAccessibleName(tr("Open order ticket"));
    }
    // submit_btn_ text is side-aware ("OPEN BUY TICKET" etc.) — let on_side_changed re-derive it.
    if (side_combo_)
        on_side_changed(side_combo_->currentIndex());
}

} // namespace fincept::screens::widgets
