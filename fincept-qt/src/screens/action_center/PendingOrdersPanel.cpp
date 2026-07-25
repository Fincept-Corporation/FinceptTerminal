// PendingOrdersPanel.cpp — queued-order approval popover.
#include "screens/action_center/PendingOrdersPanel.h"

#include "trading/AccountManager.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace fincept::screens {

using fincept::trading::ActionCenter;
using fincept::trading::PendingOrder;

namespace {
constexpr int kColTime = 0;
constexpr int kColAccount = 1;
constexpr int kColType = 2;
constexpr int kColSymbol = 3;
constexpr int kColSide = 4;
constexpr int kColQty = 5;
constexpr int kColPriceType = 6;
constexpr int kColStatus = 7;
constexpr int kColActions = 8;
constexpr int kColCount = 9;
} // namespace

PendingOrdersPanel::PendingOrdersPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("pendingOrdersPanel");
    // Frameless popup: auto-closes when the user clicks elsewhere.
    setWindowFlags(Qt::Popup);
    setFixedSize(760, 380);
    build_ui();

    // Live updates — wire ActionCenter signals once, here (a popup has no
    // persistent show/hide lifecycle to hang them on).
    auto& ac = ActionCenter::instance();
    connect(&ac, &ActionCenter::pending_order_created, this, &PendingOrdersPanel::on_pending_created);
    connect(&ac, &ActionCenter::order_approved, this, &PendingOrdersPanel::on_order_approved);
    connect(&ac, &ActionCenter::order_rejected, this, &PendingOrdersPanel::on_order_rejected);
    connect(&ac, &ActionCenter::stats_updated, this, &PendingOrdersPanel::on_stats_updated);
    connect(&trading::AccountManager::instance(), &trading::AccountManager::account_added, this,
            [this](const QString&) { reload_accounts(); });
    connect(&trading::AccountManager::instance(), &trading::AccountManager::account_removed, this,
            [this](const QString&) { reload_accounts(); });
    connect(&trading::AccountManager::instance(), &trading::AccountManager::account_updated, this,
            [this](const QString&) { reload_accounts(); });
}

void PendingOrdersPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Global stylesheet via descendant selectors (P7) — amber #d97706 accents.
    setStyleSheet(QString("#pendingOrdersPanel{background:%1;border:1px solid %3;}"
                          "#acHeader{background:%2;border-bottom:1px solid %3;}"
                          "#acTitle{color:%4;font-size:14px;font-weight:700;letter-spacing:0.5px;}"
                          "#acStatsBar{background:%2;border-bottom:1px solid %3;}"
                          "#acStatCaption{color:%5;font-size:10px;letter-spacing:0.5px;}"
                          "#acStatValue{color:%6;font-size:15px;font-weight:700;}"
                          "#acTable{background:%1;color:%6;gridline-color:%3;border:none;}"
                          "#acTable::item{padding:2px 6px;}"
                          "QHeaderView::section{background:%2;color:%5;border:none;border-bottom:1px solid %3;"
                          "padding:4px 6px;font-size:10px;font-weight:700;letter-spacing:0.5px;}"
                          "#acCombo{background:%2;color:%6;border:1px solid %3;padding:3px 8px;font-size:11px;}"
                          "#acApproveAll{background:rgba(34,197,94,0.14);color:%7;border:1px solid %7;"
                          "padding:5px 16px;font-size:11px;font-weight:700;letter-spacing:0.5px;border-radius:2px;}"
                          "#acApproveAll:hover{background:rgba(34,197,94,0.28);}"
                          "#acRejectAll{background:rgba(220,38,38,0.12);color:%8;border:1px solid %8;"
                          "padding:5px 16px;font-size:11px;font-weight:700;letter-spacing:0.5px;border-radius:2px;}"
                          "#acRejectAll:hover{background:rgba(220,38,38,0.25);}"
                          // Per-row action buttons: hoisted here (P7) so refresh()
                          // doesn't reparse two stylesheets per row on every update.
                          "#acRowApprove{background:rgba(34,197,94,0.14);color:%7;border:1px solid %7;"
                          "padding:2px 10px;font-size:10px;font-weight:700;border-radius:2px;}"
                          "#acRowApprove:hover{background:rgba(34,197,94,0.28);}"
                          "#acRowApprove:disabled{color:%5;border-color:%5;background:transparent;}"
                          "#acRowReject{background:rgba(220,38,38,0.12);color:%8;border:1px solid %8;"
                          "padding:2px 10px;font-size:10px;font-weight:700;border-radius:2px;}"
                          "#acRowReject:hover{background:rgba(220,38,38,0.25);}"
                          "#acRowReject:disabled{color:%5;border-color:%5;background:transparent;}")
                      .arg(fincept::ui::colors::DARK(), fincept::ui::colors::PANEL(), fincept::ui::colors::BORDER(),
                           fincept::ui::colors::AMBER(), fincept::ui::colors::TEXT_SECONDARY(),
                           fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::POSITIVE(),
                           fincept::ui::colors::NEGATIVE()));

    // ── Header: title + account selector + status filter ──────────────────────
    auto* header = new QWidget;
    header->setObjectName("acHeader");
    auto* hlay = new QHBoxLayout(header);
    hlay->setContentsMargins(14, 8, 14, 8);
    hlay->setSpacing(12);

    title_label_ = new QLabel(tr("PENDING ORDERS"));
    title_label_->setObjectName("acTitle");
    hlay->addWidget(title_label_);
    hlay->addSpacing(16);

    account_caption_ = new QLabel(tr("Account:"));
    hlay->addWidget(account_caption_);
    account_combo_ = new QComboBox;
    account_combo_->setObjectName("acCombo");
    account_combo_->setMinimumWidth(180);
    hlay->addWidget(account_combo_);

    show_caption_ = new QLabel(tr("Show:"));
    hlay->addWidget(show_caption_);
    status_filter_ = new QComboBox;
    status_filter_->setObjectName("acCombo");
    // Visible text is translatable; the userData key (e.g. "pending") drives logic.
    status_filter_->addItem(tr("Pending"), "pending");
    status_filter_->addItem(tr("Approved"), "approved");
    status_filter_->addItem(tr("Rejected"), "rejected");
    status_filter_->addItem(tr("All"), "all");
    hlay->addWidget(status_filter_);

    hlay->addStretch(1);

    root->addWidget(header);

    // ── Stats bar ─────────────────────────────────────────────────────────────
    auto* stats = new QWidget;
    stats->setObjectName("acStatsBar");
    auto* slay = new QHBoxLayout(stats);
    slay->setContentsMargins(14, 6, 14, 6);
    slay->setSpacing(28);

    auto make_stat = [&](const QString& caption, QLabel*& cap_out, QLabel*& out) {
        auto* cell = new QWidget;
        auto* cv = new QVBoxLayout(cell);
        cv->setContentsMargins(0, 0, 0, 0);
        cv->setSpacing(1);
        cap_out = new QLabel(caption);
        cap_out->setObjectName("acStatCaption");
        out = new QLabel("0");
        out->setObjectName("acStatValue");
        cv->addWidget(cap_out);
        cv->addWidget(out);
        slay->addWidget(cell);
    };
    make_stat(tr("PENDING"), stat_pending_cap_, stat_pending_);
    make_stat(tr("APPROVED"), stat_approved_cap_, stat_approved_);
    make_stat(tr("REJECTED"), stat_rejected_cap_, stat_rejected_);
    make_stat(tr("BUY"), stat_buy_cap_, stat_buy_);
    make_stat(tr("SELL"), stat_sell_cap_, stat_sell_);
    slay->addStretch(1);

    approve_all_btn_ = new QPushButton(tr("APPROVE ALL"));
    approve_all_btn_->setObjectName("acApproveAll");
    approve_all_btn_->setCursor(Qt::PointingHandCursor);
    slay->addWidget(approve_all_btn_);

    reject_all_btn_ = new QPushButton(tr("REJECT ALL"));
    reject_all_btn_->setObjectName("acRejectAll");
    reject_all_btn_->setCursor(Qt::PointingHandCursor);
    slay->addWidget(reject_all_btn_);

    root->addWidget(stats);

    // ── Table ─────────────────────────────────────────────────────────────────
    table_ = new QTableWidget;
    table_->setObjectName("acTable");
    table_->setColumnCount(kColCount);
    table_->setHorizontalHeaderLabels({tr("Time"), tr("Account"), tr("Type"), tr("Symbol"), tr("Side"), tr("Qty"),
                                       tr("Price Type"), tr("Status"), tr("Actions")});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->verticalHeader()->setDefaultSectionSize(28);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(kColSymbol, QHeaderView::Stretch);
    table_->setColumnWidth(kColActions, 170);
    root->addWidget(table_, 1);

    // ── Wiring ────────────────────────────────────────────────────────────────
    connect(account_combo_, &QComboBox::currentIndexChanged, this, [this](int) { refresh(); });
    connect(status_filter_, &QComboBox::currentIndexChanged, this, [this](int) { refresh(); });
    approve_all_btn_->setAccessibleName(tr("Approve all pending orders"));
    reject_all_btn_->setAccessibleName(tr("Reject all pending orders"));
    connect(approve_all_btn_, &QPushButton::clicked, this, [this]() {
        const QString acct = selected_account();
        // Bulk approval fires every queued order at the broker in a loop with no
        // per-order review. Say exactly how many, for which account, how many are
        // stale, and default to No.
        const auto pending = ActionCenter::instance().get_pending_orders(acct);
        if (pending.isEmpty()) {
            QMessageBox::information(this, tr("Approve All"), tr("There are no pending orders to approve."));
            return;
        }
        int stale_count = 0;
        int buys = 0;
        int sells = 0;
        for (const auto& p : pending) {
            bool s = false;
            order_age_text(p.created_at, &s);
            if (s)
                ++stale_count;
            const QString a = p.action.toUpper();
            if (a == QLatin1String("BUY"))
                ++buys;
            else if (a == QLatin1String("SELL"))
                ++sells;
        }
        const QString scope = acct.isEmpty() ? tr("ALL accounts") : account_combo_->currentText();
        QString body = tr("Send %1 pending order(s) to the broker now?\n\n"
                          "  Account scope: %2\n"
                          "  %3 BUY  ·  %4 SELL\n\n"
                          "Each order executes immediately. There is no per-order review.")
                           .arg(pending.size())
                           .arg(scope)
                           .arg(buys)
                           .arg(sells);
        if (stale_count > 0)
            body += tr("\n\n⚠  %1 of these have been waiting over %2 minutes and will be REFUSED\n"
                       "as expired rather than executed. Re-place them if you still want the trade.")
                        .arg(stale_count)
                        .arg(ActionCenter::kPendingOrderTtlMinutes);
        const auto ans =
            QMessageBox::question(this, tr("Approve All"), body, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes)
            return;
        ActionCenter::instance().approve_all_pending(acct);
    });
    connect(reject_all_btn_, &QPushButton::clicked, this, [this]() {
        const QString acct = selected_account();
        if (acct.isEmpty()) {
            QMessageBox::information(this, tr("Reject All"),
                                     tr("Select a specific account to reject all its pending orders."));
            return;
        }
        bool ok = false;
        const QString reason = QInputDialog::getText(this, tr("Reject All"), tr("Rejection reason:"), QLineEdit::Normal,
                                                     tr("Rejected by user"), &ok);
        if (!ok)
            return;
        ActionCenter::instance().reject_all_pending(acct, reason);
    });
}

void PendingOrdersPanel::popup_at(const QPoint& global_anchor) {
    reload_accounts();
    refresh();
    move(global_anchor.x() - width(), global_anchor.y() - height());
    show();
    raise();
}

void PendingOrdersPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PendingOrdersPanel::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("PENDING ORDERS"));
    if (account_caption_)
        account_caption_->setText(tr("Account:"));
    if (show_caption_)
        show_caption_->setText(tr("Show:"));

    // Combo items: re-set visible text by index; userData keys are unchanged.
    if (status_filter_ && status_filter_->count() >= 4) {
        status_filter_->setItemText(0, tr("Pending"));
        status_filter_->setItemText(1, tr("Approved"));
        status_filter_->setItemText(2, tr("Rejected"));
        status_filter_->setItemText(3, tr("All"));
    }

    if (stat_pending_cap_)
        stat_pending_cap_->setText(tr("PENDING"));
    if (stat_approved_cap_)
        stat_approved_cap_->setText(tr("APPROVED"));
    if (stat_rejected_cap_)
        stat_rejected_cap_->setText(tr("REJECTED"));
    if (stat_buy_cap_)
        stat_buy_cap_->setText(tr("BUY"));
    if (stat_sell_cap_)
        stat_sell_cap_->setText(tr("SELL"));

    if (approve_all_btn_)
        approve_all_btn_->setText(tr("APPROVE ALL"));
    if (reject_all_btn_)
        reject_all_btn_->setText(tr("REJECT ALL"));

    if (table_) {
        table_->setHorizontalHeaderLabels({tr("Time"), tr("Account"), tr("Type"), tr("Symbol"), tr("Side"), tr("Qty"),
                                           tr("Price Type"), tr("Status"), tr("Actions")});
    }
    // Per-row Approve/Reject button widgets are rebuilt on the next refresh().
}

QString PendingOrdersPanel::selected_account() const {
    return account_combo_ ? account_combo_->currentData().toString() : QString();
}

void PendingOrdersPanel::reload_accounts() {
    const QString prev = selected_account();
    QSignalBlocker b(account_combo_);
    account_combo_->clear();
    account_combo_->addItem(tr("All Accounts"), QString());
    for (const auto& acct : trading::AccountManager::instance().list_accounts()) {
        const QString label = acct.display_name.isEmpty() ? acct.account_id : acct.display_name;
        account_combo_->addItem(label, acct.account_id);
    }
    const int idx = account_combo_->findData(prev);
    account_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void PendingOrdersPanel::refresh() {
    const QString acct = selected_account();
    const QString status = status_filter_->currentData().toString();

    QVector<PendingOrder> orders = ActionCenter::instance().get_all_orders(acct, 200);
    if (status != "all") {
        QVector<PendingOrder> filtered;
        for (const auto& o : orders)
            if (o.status == status)
                filtered.append(o);
        orders = filtered;
    }

    table_->setRowCount(0);
    table_->setRowCount(orders.size());
    for (int row = 0; row < orders.size(); ++row) {
        const PendingOrder& o = orders[row];

        auto set = [&](int col, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setForeground(QColor(fincept::ui::colors::TEXT_PRIMARY()));
            table_->setItem(row, col, item);
        };
        // Time column carries the age too: a queued order's staleness is the
        // single most important thing to know before executing it at market.
        bool stale = false;
        const QString age = order_age_text(o.created_at, &stale);
        auto* time_item = new QTableWidgetItem(o.created_at.toString("HH:mm:ss"));
        time_item->setToolTip(tr("Queued %1").arg(age));
        const bool flag_stale = stale && o.status == QLatin1String("pending");
        time_item->setForeground(flag_stale ? QColor(fincept::ui::colors::WARNING())
                                            : QColor(fincept::ui::colors::TEXT_PRIMARY()));
        if (flag_stale)
            time_item->setText(o.created_at.toString("HH:mm:ss") + QStringLiteral(" ⚠"));
        table_->setItem(row, kColTime, time_item);
        // Resolve account display name.
        const auto acct_meta = trading::AccountManager::instance().get_account(o.account_id);
        set(kColAccount, acct_meta.display_name.isEmpty() ? o.account_id : acct_meta.display_name);
        set(kColType, o.order_type);
        set(kColSymbol, o.symbol);

        auto* side_item = new QTableWidgetItem(o.action);
        const QString actUp = o.action.toUpper();
        side_item->setForeground(actUp == "BUY"    ? QColor(fincept::ui::colors::POSITIVE())
                                 : actUp == "SELL" ? QColor(fincept::ui::colors::NEGATIVE())
                                                   : QColor(fincept::ui::colors::TEXT_PRIMARY()));
        table_->setItem(row, kColSide, side_item);

        set(kColQty, o.quantity);
        set(kColPriceType, o.price_type);

        auto* status_item = new QTableWidgetItem(o.status.toUpper());
        status_item->setForeground(o.status == "approved"   ? QColor(fincept::ui::colors::POSITIVE())
                                   : o.status == "rejected" ? QColor(fincept::ui::colors::NEGATIVE())
                                                            : QColor(fincept::ui::colors::AMBER()));
        table_->setItem(row, kColStatus, status_item);

        // Per-row Approve / Reject buttons (only for pending rows).
        if (o.status == "pending") {
            auto* cell = new QWidget;
            auto* cl = new QHBoxLayout(cell);
            cl->setContentsMargins(4, 2, 4, 2);
            cl->setSpacing(6);
            auto* approve = new QPushButton(tr("Approve"));
            approve->setObjectName(QStringLiteral("acRowApprove"));
            approve->setCursor(Qt::PointingHandCursor);
            approve->setAccessibleName(
                tr("Approve %1 %2 %3").arg(o.action.toUpper(), o.quantity, o.symbol));
            auto* reject = new QPushButton(tr("Reject"));
            reject->setObjectName(QStringLiteral("acRowReject"));
            reject->setCursor(Qt::PointingHandCursor);
            reject->setAccessibleName(tr("Reject %1 %2 %3").arg(o.action.toUpper(), o.quantity, o.symbol));
            const QString id = o.id;
            // Latch BOTH buttons off the moment either is clicked. approve_order()
            // executes against the broker synchronously; a second click before the
            // table refreshes would otherwise fire a second approval attempt on the
            // same queued order.
            // QPointer: approving triggers ActionCenter signals that call refresh(),
            // which retires these cell widgets while we are still inside the click
            // handler. Guard before touching them again.
            QPointer<QPushButton> approve_guard(approve);
            QPointer<QPushButton> reject_guard(reject);
            auto set_row_enabled = [approve_guard, reject_guard](bool on) {
                if (approve_guard)
                    approve_guard->setEnabled(on);
                if (reject_guard)
                    reject_guard->setEnabled(on);
            };
            connect(approve, &QPushButton::clicked, this, [this, id, set_row_enabled]() {
                set_row_enabled(false);
                if (!approve_row(id)) // user cancelled — restore the row
                    set_row_enabled(true);
            });
            connect(reject, &QPushButton::clicked, this, [this, id, set_row_enabled]() {
                set_row_enabled(false);
                if (!reject_row(id))
                    set_row_enabled(true);
            });
            cl->addWidget(approve);
            cl->addWidget(reject);
            cl->addStretch(1);
            table_->setCellWidget(row, kColActions, cell);
        } else if (!o.broker_order_id.isEmpty()) {
            set(kColActions, "→ " + o.broker_order_id);
        } else if (!o.rejection_reason.isEmpty()) {
            set(kColActions, o.rejection_reason);
        }
    }

    refresh_stats();
}

void PendingOrdersPanel::refresh_stats() {
    const auto s = ActionCenter::instance().get_stats(selected_account());
    stat_pending_->setText(QString::number(s.total_pending));
    stat_approved_->setText(QString::number(s.total_approved));
    stat_rejected_->setText(QString::number(s.total_rejected));
    stat_buy_->setText(QString::number(s.total_buy));
    stat_sell_->setText(QString::number(s.total_sell));
}

// Human-readable age of a queued order, plus whether it is stale enough that
// approving it could execute against a market that has moved on.
QString PendingOrdersPanel::order_age_text(const QDateTime& created, bool* stale_out) {
    const qint64 secs = created.isValid() ? created.secsTo(QDateTime::currentDateTime()) : 0;
    if (stale_out)
        *stale_out = (secs >= kStaleOrderSecs);
    if (!created.isValid())
        return tr("unknown age");
    if (secs < 60)
        return tr("%1s ago").arg(secs);
    if (secs < 3600)
        return tr("%1m ago").arg(secs / 60);
    if (secs < 86400)
        return tr("%1h %2m ago").arg(secs / 3600).arg((secs % 3600) / 60);
    return tr("%1d ago").arg(secs / 86400);
}

// Returns true when the action was dispatched (or definitively consumed), false
// when the user backed out and the row should become interactive again.
bool PendingOrdersPanel::approve_row(const QString& pending_id) {
    // Re-read the order rather than trusting the row snapshot: the table is
    // rebuilt asynchronously from ActionCenter signals, so a row widget can
    // outlive the state it was rendered from.
    const auto po_opt = ActionCenter::instance().get_order(pending_id);
    if (!po_opt) {
        QMessageBox::warning(this, tr("Approve Order"),
                             tr("This order is no longer in the queue. Refreshing the list."));
        refresh();
        return true;
    }
    const PendingOrder& o = *po_opt;
    if (o.status != QLatin1String("pending")) {
        QMessageBox::information(this, tr("Approve Order"),
                                 tr("This order was already %1 — nothing was sent.").arg(o.status));
        refresh();
        return true;
    }

    const auto acct_meta = trading::AccountManager::instance().get_account(o.account_id);
    const QString acct_label = acct_meta.display_name.isEmpty() ? o.account_id : acct_meta.display_name;
    bool stale = false;
    const QString age = order_age_text(o.created_at, &stale);

    // An approval sends a REAL order. Restate every field the user is agreeing
    // to — previously a single click on a 10px button executed it with no
    // confirmation and no visibility of price type, exchange or account.
    QString body = tr("Send this order to the broker now?\n\n"
                      "  Symbol:     %1\n"
                      "  Exchange:   %2\n"
                      "  Side:       %3\n"
                      "  Quantity:   %4\n"
                      "  Price type: %5\n"
                      "  Order type: %6\n"
                      "  Account:    %7\n"
                      "  Strategy:   %8\n"
                      "  Queued:     %9")
                       .arg(o.symbol, o.exchange.isEmpty() ? tr("—") : o.exchange, o.action.toUpper(), o.quantity,
                            o.price_type, o.order_type, acct_label, o.strategy, age);
    if (stale) {
        body += tr("\n\n⚠  This order has been waiting a long time. The market has almost\n"
                   "certainly moved since it was queued — a MARKET order will fill at\n"
                   "today's price, not the price that triggered it.");
    }

    const auto ans = QMessageBox::question(this, tr("Approve Order"), body,
                                           QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (ans != QMessageBox::Yes)
        return false;

    ActionCenter::instance().approve_order(pending_id);
    return true;
}

bool PendingOrdersPanel::reject_row(const QString& pending_id) {
    bool ok = false;
    const QString reason = QInputDialog::getText(this, tr("Reject Order"), tr("Rejection reason:"), QLineEdit::Normal,
                                                 tr("Rejected by user"), &ok);
    if (!ok)
        return false;
    ActionCenter::instance().reject_order(pending_id, reason);
    return true;
}

// ── ActionCenter signal slots ───────────────────────────────────────────────

void PendingOrdersPanel::on_pending_created(const PendingOrder&) {
    if (isVisible())
        refresh();
}
void PendingOrdersPanel::on_order_approved(const QString&, const QString&) {
    if (isVisible())
        refresh();
}
void PendingOrdersPanel::on_order_rejected(const QString&, const QString&) {
    if (isVisible())
        refresh();
}
void PendingOrdersPanel::on_stats_updated(const QString&) {
    if (isVisible())
        refresh_stats();
}

} // namespace fincept::screens
