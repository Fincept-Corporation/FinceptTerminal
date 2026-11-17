#include "screens/dashboard/widgets/BrokerHoldingsWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderThreadPool.h"
#include "trading/UnifiedTrading.h"
#include "screens/dashboard/widgets/DashboardFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QStringList>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens::widgets {

namespace {
// Build a MARKET SELL / CNC order that exits the full quantity of one holding.
trading::UnifiedOrder make_square_off_order(const trading::BrokerHolding& h) {
    trading::UnifiedOrder o;
    o.symbol = h.symbol;
    o.exchange = h.exchange;
    o.side = trading::OrderSide::Sell;
    o.order_type = trading::OrderType::Market;
    o.quantity = h.quantity;
    // Market sells need a reference price for the paper engine to fill; the live
    // market path ignores price. Fall back to avg cost if no LTP is cached yet.
    o.price = h.ltp > 0 ? h.ltp : h.avg_price;
    o.product_type = trading::ProductType::Delivery; // CNC / delivery
    o.validity = QStringLiteral("DAY");
    return o;
}
} // namespace

BrokerHoldingsWidget::BrokerHoldingsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("HOLDINGS"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    auto* header_row = new QHBoxLayout();
    header_row->setContentsMargins(0, 0, 0, 0);
    header_row->setSpacing(6);
    header_hint_ = new QLabel("—");
    header_row->addWidget(header_hint_, 1);
    square_off_all_btn_ = new QPushButton(tr("SQUARE OFF ALL"));
    square_off_all_btn_->setProperty("fincept_agent_risk", "order");
    square_off_all_btn_->setCursor(Qt::PointingHandCursor);
    square_off_all_btn_->setEnabled(false);
    connect(square_off_all_btn_, &QPushButton::clicked, this, &BrokerHoldingsWidget::square_off_all);
    header_row->addWidget(square_off_all_btn_, 0);
    vl->addLayout(header_row);

    // Columns: Symbol, Qty, Avg, LTP, P&L %, and a trailing per-row exit button.
    table_ = new QTableWidget(0, 6, this);
    table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Qty"), tr("Avg"), tr("LTP"), tr("P&L %"), QString()});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    // Fitted once after each fill (populate), not re-measured on every cell set.
    for (int c = 1; c <= 5; ++c)
        table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Interactive);
    table_->setToolTip(tr("Double-click a holding to open it in Equity Trading"));
    vl->addWidget(table_, 1);

    // Row → symbol link (selection only — it never places an order). The
    // exchange rides on the SYMBOL cell; the "No holdings" placeholder row
    // carries none and is ignored.
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        auto* it = table_->item(row, 0);
        if (it && it->data(Qt::UserRole).isValid())
            open_symbol(it->text(), QStringLiteral("equity_trading"), QStringLiteral("equity"),
                        it->data(Qt::UserRole).toString());
    });
    // Single click → linked panels (same placeholder-row guard).
    enable_symbol_link(/*follows=*/false);
    connect(table_, &QTableWidget::cellClicked, this, [this](int row, int /*col*/) {
        auto* it = table_->item(row, 0);
        if (it && it->data(Qt::UserRole).isValid())
            publish_symbol(it->text(), it->data(Qt::UserRole).toString());
    });

    // Title-bar refresh = "retry". It used to be wired to nothing, so the 20 s
    // "No data yet - click refresh to retry" prompt pointed at a dead button.
    // The account stream polls on its own cadence, so the only thing a retry can
    // do is (re)start that stream, show the loading state again, and ask the hub.
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (broker_id_.isEmpty() || account_id_.isEmpty())
            return;
        ensure_stream_running();
        if (table_->rowCount() == 0)
            set_loading(true);
        datahub::DataHub::instance().request(holdings_topic(), /*force=*/true);
        update_session_state(); // a still-dead session says so again instead of spinning
    });

    // A dead broker session (expired token, connection error) used to end in the
    // generic "No data yet": the poll fails silently. Name it in the header and,
    // with nothing on screen, on the overlay.
    connect(&trading::AccountManager::instance(), &trading::AccountManager::connection_state_changed, this,
            [this](const QString& account_id, trading::ConnectionState) {
                if (account_id == account_id_)
                    update_session_state();
            });

    // Settings > Currency & Number Format: re-format the holdings on screen, no fetch.
    ui::formatting::on_format_changed(this, [this]() {
        if (!holdings_.isEmpty())
            populate(holdings_);
    });

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QString BrokerHoldingsWidget::holdings_topic() const {
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return {};
    return trading::broker_topic(broker_id_, account_id_, QStringLiteral("holdings"));
}

void BrokerHoldingsWidget::update_session_state() {
    if (header_hint_ && !account_id_.isEmpty())
        header_hint_->setText(broker_account_hint(account_id_, holdings_topic()));
}

QJsonObject BrokerHoldingsWidget::config() const {
    QJsonObject o;
    if (!account_id_.isEmpty())
        o.insert("account_id", account_id_);
    return o;
}

void BrokerHoldingsWidget::apply_config(const QJsonObject& cfg) {
    account_id_ = cfg.value("account_id").toString();
    if (account_id_.isEmpty())
        account_id_ = resolve_account_id();

    broker_id_.clear();
    if (!account_id_.isEmpty()) {
        hide_empty_state();
        broker_id_ = trading::AccountManager::instance().get_account(account_id_).broker_id;
        update_session_state();
    } else {
        header_hint_->setText(tr("No active account — click gear to configure"));
        show_no_account_state();
    }

    if (isVisible() && !broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        hub_resubscribe();
    }
}

void BrokerHoldingsWidget::show_no_account_state() {
    // Class C (needs a broker login): explain and point to Equity Trading, never
    // connect anything here. An account that becomes active fills the tile.
    show_no_broker_account(tr("This tile lists the long-term holdings of a broker account — quantity, average cost, "
                              "last price and return."),
                           [this]() { apply_config({}); });
}

QString BrokerHoldingsWidget::resolve_account_id() const {
    const auto active = trading::AccountManager::instance().active_accounts();
    return active.isEmpty() ? QString() : active.first().account_id;
}

void BrokerHoldingsWidget::ensure_stream_running() {
    auto& mgr = trading::DataStreamManager::instance();
    if (!mgr.has_stream(account_id_))
        mgr.start_stream(account_id_);
}

void BrokerHoldingsWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return;
    const QString topic = holdings_topic();
    if (table_->rowCount() == 0)
        set_loading(true); // see OpenPositionsWidget::hub_resubscribe
    // Producer errors on the holdings topic replace the spinner / mark the tile STALE.
    track_topics({topic});
    hub.subscribe(this, topic, [this, topic](const QVariant& v) {
        if (!v.canConvert<QVector<trading::BrokerHolding>>())
            return;
        note_topic_value(topic);
        populate(v.value<QVector<trading::BrokerHolding>>());
    });
    hub_active_ = true;
    update_session_state();
}

void BrokerHoldingsWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void BrokerHoldingsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        if (!hub_active_)
            hub_resubscribe();
    }
}

void BrokerHoldingsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void BrokerHoldingsWidget::populate(const QVector<trading::BrokerHolding>& rows) {
    holdings_ = rows;

    // Switching between the empty-state message and holding rows starts from
    // clean cells (same as OrderBookMiniWidget). Without this, squaring off the
    // last holding left row 0's old cells and its "Exit" button under the
    // "No holdings" span — still clickable, offering a MARKET SELL of a holding
    // the account no longer has. clearContents() also deletes the cell widgets.
    const bool want_empty = rows.isEmpty();
    if (want_empty != showing_empty_) {
        table_->clearContents();
        showing_empty_ = want_empty;
    }

    // Empty state — an account with no holdings used to render a blank grid.
    table_->clearSpans();
    if (want_empty) {
        table_->setRowCount(1);
        auto* msg = new QTableWidgetItem(tr("No holdings"));
        msg->setTextAlignment(Qt::AlignCenter);
        msg->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
        table_->setItem(0, 0, msg);
        table_->setSpan(0, 0, 1, table_->columnCount());
        square_off_all_btn_->setEnabled(false);
        set_loading(false);
        return;
    }

    table_->setRowCount(static_cast<int>(rows.size()));
    int sellable = 0;
    // The P&L amount (tooltip) in the broker account's own currency; unknown broker -> no unit.
    const auto* broker = trading::BrokerRegistry::instance().get(broker_id_);
    const QString ccy = broker ? broker->profile().currency : QString();
    for (int i = 0; i < rows.size(); ++i) {
        const auto& h = rows[i];
        auto* sym = new QTableWidgetItem(h.symbol);
        sym->setData(Qt::UserRole, h.exchange);
        // Fractional holdings (Alpaca / IBKR fractional shares, crypto) used to
        // print 0.5 as "0" / "1" — and the Exit confirmation said the same.
        auto* qty = new QTableWidgetItem(dash_fmt::quantity(h.quantity));
        auto* avg = new QTableWidgetItem(dash_fmt::grouped(h.avg_price, dash_fmt::price_decimals(h.avg_price)));
        auto* ltp = new QTableWidgetItem(h.ltp > 0 ? dash_fmt::grouped(h.ltp, dash_fmt::price_decimals(h.ltp))
                                                   : QStringLiteral("--"));
        auto* pnl_pct = new QTableWidgetItem(ui::formatting::pct(h.pnl_pct, 2));
        pnl_pct->setToolTip(tr("P&L %1").arg(ui::formatting::signed_money(h.pnl, ccy)));
        const QColor col(h.pnl_pct >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
        pnl_pct->setForeground(col);
        qty->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        avg->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ltp->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pnl_pct->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 0, sym);
        table_->setItem(i, 1, qty);
        table_->setItem(i, 2, avg);
        table_->setItem(i, 3, ltp);
        table_->setItem(i, 4, pnl_pct);

        // Per-row exit button — squares off this single holding. Capture the
        // holding by value so a click always acts on the row it was drawn for.
        auto* exit_btn = new QPushButton(tr("Exit"), table_);
        exit_btn->setCursor(Qt::PointingHandCursor);
        exit_btn->setProperty("holdingsExit", true);
        exit_btn->setProperty("fincept_agent_risk", "order");
        const bool can_sell = h.quantity > 0;
        exit_btn->setEnabled(can_sell);
        connect(exit_btn, &QPushButton::clicked, this, [this, h]() { square_off_holding(h); });
        table_->setCellWidget(i, 5, exit_btn);
        if (can_sell)
            ++sellable;
    }
    for (int c = 1; c <= 5; ++c)
        table_->resizeColumnToContents(c);
    square_off_all_btn_->setEnabled(sellable > 0);
    set_loading(false);
}

QDialog* BrokerHoldingsWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Holdings"));
    auto* form = new QFormLayout(dlg);

    auto* combo = new QComboBox(dlg);
    const auto accts = trading::AccountManager::instance().list_accounts();
    for (const auto& a : accts) {
        const QString label = a.display_name.isEmpty() ? (a.broker_id + " — " + a.account_id) : a.display_name;
        combo->addItem(label, a.account_id);
        if (a.account_id == account_id_)
            combo->setCurrentIndex(combo->count() - 1);
    }
    form->addRow(tr("Broker account"), combo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, combo]() {
        const QString picked = combo->currentData().toString();
        if (!picked.isEmpty() && picked != account_id_) {
            QJsonObject cfg;
            cfg.insert("account_id", picked);
            apply_config(cfg);
            emit config_changed(cfg);
        }
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void BrokerHoldingsWidget::square_off_holding(const trading::BrokerHolding& h) {
    if (account_id_.isEmpty() || h.quantity <= 0)
        return;

    // The order is fixed at click time, so Yes places exactly what the box names.
    const trading::UnifiedOrder order = make_square_off_order(h);
    const QString account = account_id_;
    const QString sym = h.symbol;
    // Non-blocking confirm (BaseWidget::confirm_async): QMessageBox::warning()
    // ran a nested event loop, and a tile destroyed while it was open (layout
    // reload, MCP remove) left the code after it running on a dead object.
    confirm_async(
        QMessageBox::Warning, tr("Square Off Holding"),
        tr("Place a MARKET SELL order for %1 %2 (CNC)?").arg(dash_fmt::quantity(h.quantity), h.symbol),
        [this, account, order, sym]() {
            // place_order blocks on a synchronous broker REST call for a live
            // account — run it off the UI thread (P1) and marshal the result back
            // through the application object, checking the tile on the GUI thread.
            QPointer<BrokerHoldingsWidget> self = this;
            (void)QtConcurrent::run(&trading::order_thread_pool(), [self, account, order, sym]() {
                const auto resp = trading::UnifiedTrading::instance().place_order(account, order);
                QMetaObject::invokeMethod(
                    QCoreApplication::instance(),
                    [self, resp, sym]() {
                        if (!self)
                            return;
                        if (resp.success)
                            self->notify_async(QMessageBox::Information, tr("Square Off Holding"),
                                               tr("Sell order placed for %1 (order %2).").arg(sym, resp.order_id));
                        else
                            self->notify_async(QMessageBox::Critical, tr("Square Off Holding"),
                                               tr("Failed to square off %1: %2").arg(sym, resp.message));
                    },
                    Qt::QueuedConnection);
            });
        });
}

void BrokerHoldingsWidget::square_off_all() {
    if (account_id_.isEmpty())
        return;

    // Snapshot sellable holdings now — the holdings subscription may refresh
    // (and rebuild the table) while the confirm dialog / orders are in flight.
    QVector<trading::BrokerHolding> targets;
    for (const auto& h : holdings_)
        if (h.quantity > 0)
            targets.append(h);
    if (targets.isEmpty()) {
        notify_async(QMessageBox::Information, tr("Square Off All Holdings"), tr("No holdings to square off."));
        return;
    }

    // Pre-build every order on the UI thread, then place them off the UI thread:
    // each place_order is a blocking broker REST round-trip for a live account,
    // so looping synchronously here froze the whole terminal for seconds (P1).
    // Built before the confirm, so Yes places exactly the snapshot it counted.
    struct SquareOffJob {
        QString symbol;
        trading::UnifiedOrder order;
    };
    QVector<SquareOffJob> jobs;
    jobs.reserve(targets.size());
    for (const auto& h : targets)
        jobs.append({h.symbol, make_square_off_order(h)});
    const QString account = account_id_;

    // Non-blocking confirm (BaseWidget::confirm_async) — see square_off_holding().
    confirm_async(
        QMessageBox::Warning, tr("Square Off All Holdings"),
        tr("This will place MARKET SELL orders to exit ALL %1 holding(s) in this account.\n\n"
           "Positions are NOT affected. Continue?")
            .arg(targets.size()),
        [this, account, jobs]() {
            QPointer<BrokerHoldingsWidget> self = this;
            (void)QtConcurrent::run(&trading::order_thread_pool(), [self, account, jobs]() {
                int placed = 0;
                QStringList failures;
                for (const auto& j : jobs) {
                    const auto resp = trading::UnifiedTrading::instance().place_order(account, j.order);
                    if (resp.success)
                        ++placed;
                    else
                        failures.append(QStringLiteral("%1 (%2)").arg(j.symbol, resp.message));
                }
                // Marshal through the application object; check the tile on the GUI thread.
                QMetaObject::invokeMethod(
                    QCoreApplication::instance(),
                    [self, placed, failures]() {
                        if (!self)
                            return;
                        if (failures.isEmpty())
                            self->notify_async(QMessageBox::Information, tr("Square Off All Holdings"),
                                               tr("Placed %1 sell order(s).").arg(placed));
                        else
                            self->notify_async(QMessageBox::Warning, tr("Square Off All Holdings"),
                                               tr("Placed %1 order(s). %2 failed:\n%3")
                                                   .arg(placed)
                                                   .arg(failures.size())
                                                   .arg(failures.join(QStringLiteral("\n"))));
                    },
                    Qt::QueuedConnection);
            });
        });
}

void BrokerHoldingsWidget::on_theme_changed() {
    apply_styles();
    // P&L colours are baked into the table items: rebuild what is on screen.
    if (table_->rowCount() > 0)
        populate(holdings_);
}

void BrokerHoldingsWidget::apply_styles() {
    set_css_cached(header_hint_, 
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    set_css_cached(table_, 
        QString("QTableWidget{background:transparent;color:%1;gridline-color:%2;font-size:" + fincept::ui::fonts::px(-4) + "px;border:none;}"
                "QHeaderView::section{background:%3;color:%4;border:none;border-bottom:1px solid %2;"
                "padding:2px 4px;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:bold;}"
                "QTableWidget::item{padding:2px 4px;}"
                "QPushButton[holdingsExit=\"true\"]{color:%5;background:transparent;border:1px solid %5;"
                "border-radius:2px;padding:0 6px;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:bold;}"
                "QPushButton[holdingsExit=\"true\"]:hover{color:%6;background:%5;}"
                "QPushButton[holdingsExit=\"true\"]:disabled{color:%7;border-color:%2;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
                 ui::colors::TEXT_SECONDARY(), ui::colors::NEGATIVE(), ui::colors::TEXT_ON_ACCENT(),
                 ui::colors::TEXT_TERTIARY()));

    if (square_off_all_btn_)
        set_css_cached(square_off_all_btn_, 
            QString("QPushButton{color:%1;background:transparent;border:1px solid %1;border-radius:2px;"
                    "padding:2px 8px;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:bold;}"
                    "QPushButton:hover{color:%2;background:%1;}"
                    "QPushButton:disabled{color:%4;border-color:%3;}")
                .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_ON_ACCENT(), ui::colors::BORDER_DIM(),
                     ui::colors::TEXT_TERTIARY()));
}

void BrokerHoldingsWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("HOLDINGS"));
    // Re-apply the actual strings (the old body called hub_resubscribe(),
    // which rebuilds subscriptions and translates nothing).
    if (table_) {
        table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Qty"), tr("Avg"), tr("LTP"), tr("P&L %"), QString()});
        for (int r = 0; r < table_->rowCount(); ++r) {
            if (auto* btn = qobject_cast<QPushButton*>(table_->cellWidget(r, 5)))
                btn->setText(tr("Exit"));
        }
    }
    if (square_off_all_btn_)
        square_off_all_btn_->setText(tr("SQUARE OFF ALL"));
    if (header_hint_ && account_id_.isEmpty()) {
        header_hint_->setText(tr("No active account — click gear to configure"));
        show_no_account_state();
    } else {
        update_session_state();
    }
}

} // namespace fincept::screens::widgets
