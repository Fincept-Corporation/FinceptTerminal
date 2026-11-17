#include "screens/dashboard/widgets/OrderBookMiniWidget.h"

#include "trading/OrderThreadPool.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/DashboardFormat.h"
#include "trading/AccountManager.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "trading/UnifiedTrading.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QtConcurrent>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {
// The per-row cancel button: 18 px at the default font, taller when the "×"
// glyph's font needs it.
int ob_cancel_height() {
    return std::max(18, QFontMetrics(ui::fonts::font(-3)).height() + 2);
}
} // namespace

OrderBookMiniWidget::OrderBookMiniWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("WORKING ORDERS"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    header_hint_ = new QLabel("—");
    vl->addWidget(header_hint_);

    table_ = new QTableWidget(0, 6, this);
    table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Side"), tr("Qty"), tr("Price"), tr("Status"), ""});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    // Fitted once after each fill (populate), not re-measured on every cell set.
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    table_->setColumnWidth(5, 52);
    vl->addWidget(table_, 1);

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
        datahub::DataHub::instance().request(orders_topic(), /*force=*/true);
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

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QString OrderBookMiniWidget::orders_topic() const {
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return {};
    return trading::broker_topic(broker_id_, account_id_, QStringLiteral("orders"));
}

void OrderBookMiniWidget::update_session_state() {
    if (header_hint_ && !account_id_.isEmpty())
        header_hint_->setText(broker_account_hint(account_id_, orders_topic()));
}

QJsonObject OrderBookMiniWidget::config() const {
    QJsonObject o;
    if (!account_id_.isEmpty())
        o.insert("account_id", account_id_);
    return o;
}

void OrderBookMiniWidget::apply_config(const QJsonObject& cfg) {
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

void OrderBookMiniWidget::show_no_account_state() {
    // Class C (needs a broker login): explain and point to Equity Trading, never
    // connect anything here. An account that becomes active fills the tile.
    show_no_broker_account(tr("This tile lists the working orders of a broker account — side, quantity, price and "
                              "status, with one-click cancel."),
                           [this]() { apply_config({}); });
}

QString OrderBookMiniWidget::resolve_account_id() const {
    const auto active = trading::AccountManager::instance().active_accounts();
    return active.isEmpty() ? QString() : active.first().account_id;
}

void OrderBookMiniWidget::ensure_stream_running() {
    auto& mgr = trading::DataStreamManager::instance();
    if (!mgr.has_stream(account_id_))
        mgr.start_stream(account_id_);
}

bool OrderBookMiniWidget::is_working(const QString& status) {
    // Mirrors order_is_terminal() in screens/equity_trading/EquityBottomPanel.cpp
    // exactly (that one is file-local, so it cannot be shared from here) — keep
    // the two lists in sync. The old six-state list kept Alpaca's expired /
    // replaced / done_for_day (and "traded" / "executed" / "closed" from other
    // adapters, which pass the broker's raw status through) on screen as
    // "working", each with a live cancel button.
    static const QSet<QString> kTerminal = {
        QStringLiteral("complete"),   QStringLiteral("completed"), QStringLiteral("filled"),
        QStringLiteral("traded"),     QStringLiteral("executed"),  QStringLiteral("cancelled"),
        QStringLiteral("canceled"),   QStringLiteral("rejected"),  QStringLiteral("expired"),
        QStringLiteral("closed"),     QStringLiteral("replaced"),  QStringLiteral("done_for_day"),
        QStringLiteral("stopped"),    QStringLiteral("suspended"),
    };
    return !kTerminal.contains(status.trimmed().toLower());
}

void OrderBookMiniWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return;
    const QString topic = orders_topic();
    if (table_->rowCount() == 0)
        set_loading(true); // see OpenPositionsWidget::hub_resubscribe
    // Producer errors on the orders topic replace the spinner / mark the tile STALE.
    track_topics({topic});
    hub.subscribe(this, topic, [this, topic](const QVariant& v) {
        if (!v.canConvert<QVector<trading::BrokerOrderInfo>>())
            return;
        note_topic_value(topic);
        populate(v.value<QVector<trading::BrokerOrderInfo>>());
    });
    hub_active_ = true;
    update_session_state();
}

void OrderBookMiniWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void OrderBookMiniWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        if (!hub_active_)
            hub_resubscribe();
    }
}

void OrderBookMiniWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void OrderBookMiniWidget::populate(const QVector<trading::BrokerOrderInfo>& rows) {
    orders_ = rows;
    QVector<trading::BrokerOrderInfo> working;
    working.reserve(rows.size());
    for (const auto& o : rows) {
        if (is_working(o.status))
            working.append(o);
    }
    // Cell items and cancel buttons are reused between pushes instead of being
    // re-created for every row on every push. Switching between the empty-state
    // message and order rows starts from clean cells (this also drops row 0's
    // stale cells / cancel button that used to linger under the message span).
    const bool want_empty = working.isEmpty();
    if (want_empty != showing_empty_) {
        table_->clearContents();
        showing_empty_ = want_empty;
    }
    auto cell = [this](int row, int col) {
        QTableWidgetItem* item = table_->item(row, col);
        if (!item) {
            item = new QTableWidgetItem;
            table_->setItem(row, col, item);
        }
        return item;
    };

    // Empty state — an account with nothing working used to render a blank
    // grid that is indistinguishable from "still loading" / "broken".
    table_->clearSpans();
    if (want_empty) {
        table_->setRowCount(1);
        auto* msg = cell(0, 0);
        msg->setText(tr("No working orders"));
        msg->setTextAlignment(Qt::AlignCenter);
        msg->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
        table_->setSpan(0, 0, 1, table_->columnCount());
        set_loading(false);
        return;
    }

    table_->setRowCount(static_cast<int>(working.size()));
    const QColor buy_color(ui::colors::POSITIVE());
    const QColor sell_color(ui::colors::NEGATIVE());
    for (int i = 0; i < working.size(); ++i) {
        const auto& o = working[i];
        cell(i, 0)->setText(o.symbol);
        auto* side = cell(i, 1);
        side->setText(o.side);
        side->setForeground(o.side.compare("BUY", Qt::CaseInsensitive) == 0 ? buy_color : sell_color);
        auto* qty = cell(i, 2);
        // Fractional orders (0.5 share, 0.0123 BTC) used to print as "0" / "1".
        qty->setText(dash_fmt::quantity(o.quantity));
        qty->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* price = cell(i, 3);
        price->setText(o.price > 0 ? dash_fmt::grouped(o.price, dash_fmt::price_decimals(o.price)) : tr("MKT"));
        price->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cell(i, 4)->setText(o.status);

        // Styling comes from the table-level stylesheet (see apply_styles) via
        // the `obCancel` dynamic property — a per-row setStyleSheet here meant
        // one full CSS reparse per working order on every order-book push.
        auto* cancel_btn = qobject_cast<QPushButton*>(table_->cellWidget(i, 5));
        if (!cancel_btn) {
            cancel_btn = new QPushButton("×", table_);
            cancel_btn->setProperty("obCancel", true);
            cancel_btn->setProperty("fincept_agent_risk", "order");
            cancel_btn->setCursor(Qt::PointingHandCursor);
            cancel_btn->setFixedHeight(ob_cancel_height());
            // The row's order id is read at click time — the button is reused.
            connect(cancel_btn, &QPushButton::clicked, this,
                    [this, cancel_btn]() { cancel_order(cancel_btn->property("order_id").toString()); });
            table_->setCellWidget(i, 5, cancel_btn);
        }
        if (cancel_btn->property("order_id").toString() != o.order_id) {
            cancel_btn->setProperty("order_id", o.order_id);
            cancel_btn->setToolTip(tr("Cancel order %1").arg(o.order_id));
            cancel_btn->setAccessibleName(tr("Cancel order %1").arg(o.order_id));
        }
    }
    for (int c = 1; c <= 4; ++c)
        table_->resizeColumnToContents(c);
    set_loading(false);
}

void OrderBookMiniWidget::cancel_order(const QString& order_id) {
    if (account_id_.isEmpty() || order_id.isEmpty())
        return;
    // Non-blocking confirm (BaseWidget::confirm_async): QMessageBox::question()
    // ran a nested event loop, and a tile destroyed while it was open (layout
    // reload, MCP remove) left this code running on a dead object. The account
    // is the one the row belonged to when it was clicked.
    const QString acct_id = account_id_;
    confirm_async(QMessageBox::Question, tr("Cancel Order"), tr("Cancel order %1?").arg(order_id),
                  [this, acct_id, order_id]() { send_cancel(acct_id, order_id); });
}

void OrderBookMiniWidget::send_cancel(const QString& acct_id, const QString& order_id) {
    // BrokerHttp blocking trap (P1) — always wrap live broker calls in QtConcurrent::run.
    // Runs on the dedicated order pool so a busy global pool can't delay it, and
    // completes even if this widget is closed meanwhile (the user confirmed it).
    // The result used to be discarded: a refused cancel (already filled, token
    // expired, network) left the row in place with no word to the user.
    QPointer<OrderBookMiniWidget> self = this;
    (void)QtConcurrent::run(&trading::order_thread_pool(), [self, acct_id, order_id]() {
        const auto resp = trading::UnifiedTrading::instance().cancel_order(acct_id, order_id);
        // Marshal through the application object (alive for the whole run) and
        // check the tile there, on the GUI thread — not from this worker.
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, resp, acct_id, order_id]() {
                if (!self)
                    return;
                if (!resp.success) {
                    const QString why = resp.message.trimmed().isEmpty() ? tr("the broker gave no reason")
                                                                         : resp.message.trimmed();
                    self->notify_async(QMessageBox::Critical, tr("Cancel Order"),
                                       tr("Could not cancel %1: %2").arg(order_id, why));
                    return;
                }
                // Pull the order list now so the row goes promptly instead of at
                // the stream's next poll.
                if (!self->broker_id_.isEmpty() && self->account_id_ == acct_id)
                    datahub::DataHub::instance().request(
                        trading::broker_topic(self->broker_id_, acct_id, QStringLiteral("orders")), /*force=*/true);
            },
            Qt::QueuedConnection);
    });
}

QDialog* OrderBookMiniWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Working Orders"));
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

void OrderBookMiniWidget::on_theme_changed() {
    apply_styles();
    for (int r = 0; r < table_->rowCount(); ++r) {
        if (auto* btn = table_->cellWidget(r, 5))
            btn->setFixedHeight(ob_cancel_height());
    }
    // Side colours are baked into the table items: re-render what is on screen.
    if (table_->rowCount() > 0)
        populate(orders_);
}

void OrderBookMiniWidget::apply_styles() {
    set_css_cached(header_hint_, 
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:transparent;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    set_css_cached(table_, 
        QString("QTableWidget{background:transparent;color:%1;gridline-color:%2;font-size:" + fincept::ui::fonts::px(-4) + "px;border:none;}"
                "QHeaderView::section{background:%3;color:%4;border:none;border-bottom:1px solid %2;"
                "padding:2px 4px;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:bold;}"
                "QTableWidget::item{padding:2px 4px;}"
                "QPushButton[obCancel=\"true\"]{color:%4;background:transparent;border:1px solid %2;"
                "border-radius:2px;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:bold;}"
                "QPushButton[obCancel=\"true\"]:hover{color:%5;border-color:%5;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
                 ui::colors::TEXT_SECONDARY(), ui::colors::DANGER()));
}

void OrderBookMiniWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("WORKING ORDERS"));
    // Re-apply the actual strings (the old body called hub_resubscribe(),
    // which rebuilds subscriptions and translates nothing).
    if (table_)
        table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Side"), tr("Qty"), tr("Price"), tr("Status"), QString()});
    if (header_hint_ && account_id_.isEmpty()) {
        header_hint_->setText(tr("No active account — click gear to configure"));
        show_no_account_state();
    } else {
        update_session_state();
    }
}

} // namespace fincept::screens::widgets
