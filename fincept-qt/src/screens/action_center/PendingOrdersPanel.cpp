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
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

namespace fincept::screens {

using fincept::trading::ActionCenter;
using fincept::trading::PendingOrder;

namespace {
constexpr int kColTime = 0;
constexpr int kColExpires = 1;
constexpr int kColAccount = 2;
constexpr int kColType = 3;
constexpr int kColSymbol = 4;
constexpr int kColSide = 5;
constexpr int kColQty = 6;
constexpr int kColPriceType = 7;
constexpr int kColStatus = 8;
constexpr int kColActions = 9;
constexpr int kColCount = 10;
// Item data role carrying a pending row's expiry (QDateTime) for the countdown.
constexpr int kExpiryRole = Qt::UserRole + 1;
} // namespace

PendingOrdersPanel::PendingOrdersPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("pendingOrdersPanel");
    // Frameless popup: auto-closes when the user clicks elsewhere.
    setWindowFlags(Qt::Popup);
    setFixedSize(900, 400);
    build_ui();

    // Countdown for the Expires column. Interval/connect only — started in
    // showEvent, stopped in hideEvent (P3).
    countdown_timer_ = new QTimer(this);
    countdown_timer_->setInterval(1000);
    connect(countdown_timer_, &QTimer::timeout, this, &PendingOrdersPanel::tick_countdown);

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
                          "#acRowReject:disabled{color:%5;border-color:%5;background:transparent;}"
                          "#acTtlSpin{background:%2;color:%6;border:1px solid %3;padding:2px 4px;font-size:11px;}"
                          "#acKillSwitch{background:%8;color:#000;border:1px solid %8;padding:4px 12px;"
                          "font-size:11px;font-weight:700;letter-spacing:1px;border-radius:2px;}"
                          "#acKillSwitch:hover{background:rgba(220,38,38,0.75);}"
                          "#acKillSwitch:disabled{background:transparent;color:%5;border-color:%5;}")
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

    // Approval window: a queued order not approved within this many minutes is
    // auto-rejected (the market it was priced against has moved on).
    ttl_caption_ = new QLabel(tr("Expire after:"));
    hlay->addWidget(ttl_caption_);
    ttl_spin_ = new QSpinBox;
    ttl_spin_->setObjectName("acTtlSpin");
    ttl_spin_->setRange(ActionCenter::kMinPendingTtlMinutes, ActionCenter::kMaxPendingTtlMinutes);
    ttl_spin_->setSuffix(tr(" min"));
    ttl_spin_->setValue(ActionCenter::pending_ttl_minutes());
    ttl_spin_->setToolTip(tr("Approval window for newly queued orders. An order not approved in time is "
                             "auto-rejected — it is never sent at a price that is minutes old."));
    ttl_spin_->setAccessibleName(tr("Pending order approval window in minutes"));
    hlay->addWidget(ttl_spin_);

    hlay->addStretch(1);

    kill_btn_ = new QPushButton(tr("KILL SWITCH"));
    kill_btn_->setObjectName("acKillSwitch");
    kill_btn_->setCursor(Qt::PointingHandCursor);
    kill_btn_->setToolTip(tr("Cancel every open order and square off every open position on all LIVE accounts"));
    kill_btn_->setAccessibleName(tr("Kill switch: cancel all orders and square off all positions on live accounts"));
    hlay->addWidget(kill_btn_);

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
    table_->setHorizontalHeaderLabels({tr("Time"), tr("Expires"), tr("Account"), tr("Type"), tr("Symbol"), tr("Side"),
                                       tr("Qty"), tr("Price Type"), tr("Status"), tr("Actions")});
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
    // Persist on edit-finish, not per keystroke/arrow tick.
    connect(ttl_spin_, &QSpinBox::editingFinished, this,
            [this]() { ActionCenter::instance().set_pending_ttl_minutes(ttl_spin_->value()); });
    connect(kill_btn_, &QPushButton::clicked, this, [this]() {
        QPointer<QPushButton> guard(kill_btn_);
        if (guard)
            guard->setEnabled(false);
        // Results are reported against the main window: this popover closes as
        // soon as focus moves to the confirmation dialog.
        QWidget* host = parentWidget() ? parentWidget() : this;
        if (!confirm_and_run_kill_switch(host) && guard)
            guard->setEnabled(true);
    });
    connect(&ActionCenter::instance(), &ActionCenter::kill_switch_finished, this,
            [this](const QVector<ActionCenter::KillSwitchAccountResult>&) {
                if (kill_btn_)
                    kill_btn_->setEnabled(true);
            });
    approve_all_btn_->setAccessibleName(tr("Approve all pending orders"));
    reject_all_btn_->setAccessibleName(tr("Reject all pending orders"));
    connect(approve_all_btn_, &QPushButton::clicked, this, [this]() {
        // Latch BOTH bulk buttons for the whole handler. Everything below spins a
        // nested event loop — the confirmation QMessageBox, and approve_all_pending
        // itself (one blocking broker round-trip per order, up to ~8 s each) — and
        // a nested loop keeps delivering clicks, so an un-latched button could fire
        // the same queue a second time while the first pass was still running. The
        // per-row buttons already latch (see refresh()); these did not.
        // QPointer + qScopeGuard: the guard restores on EVERY exit path (early
        // return, exception) and no-ops if the popover was destroyed meanwhile.
        QPointer<QPushButton> ap_guard(approve_all_btn_);
        QPointer<QPushButton> rj_guard(reject_all_btn_);
        auto set_bulk_enabled = [ap_guard, rj_guard](bool on) {
            if (ap_guard)
                ap_guard->setEnabled(on);
            if (rj_guard)
                rj_guard->setEnabled(on);
        };
        set_bulk_enabled(false);
        const auto restore = qScopeGuard([set_bulk_enabled]() { set_bulk_enabled(true); });

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
            if (ActionCenter::is_expired(p))
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
            body += tr("\n\n⚠  %1 of these are past their approval window and will be REFUSED\n"
                       "as expired rather than executed. Re-place them if you still want the trade.")
                        .arg(stale_count);
        const auto ans =
            QMessageBox::question(this, tr("Approve All"), body, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes)
            return;
        ActionCenter::instance().approve_all_pending(acct);
    });
    connect(reject_all_btn_, &QPushButton::clicked, this, [this]() {
        // Same latch as Approve All — QInputDialog spins a nested event loop too.
        QPointer<QPushButton> ap_guard(approve_all_btn_);
        QPointer<QPushButton> rj_guard(reject_all_btn_);
        auto set_bulk_enabled = [ap_guard, rj_guard](bool on) {
            if (ap_guard)
                ap_guard->setEnabled(on);
            if (rj_guard)
                rj_guard->setEnabled(on);
        };
        set_bulk_enabled(false);
        const auto restore = qScopeGuard([set_bulk_enabled]() { set_bulk_enabled(true); });

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
    if (ttl_spin_) {
        QSignalBlocker b(ttl_spin_);
        ttl_spin_->setValue(ActionCenter::pending_ttl_minutes());
    }
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

void PendingOrdersPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (countdown_timer_)
        countdown_timer_->start();
}

void PendingOrdersPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (countdown_timer_)
        countdown_timer_->stop();
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
    if (ttl_caption_)
        ttl_caption_->setText(tr("Expire after:"));
    if (ttl_spin_)
        ttl_spin_->setSuffix(tr(" min"));
    if (kill_btn_)
        kill_btn_->setText(tr("KILL SWITCH"));

    if (table_) {
        table_->setHorizontalHeaderLabels({tr("Time"), tr("Expires"), tr("Account"), tr("Type"), tr("Symbol"),
                                           tr("Side"), tr("Qty"), tr("Price Type"), tr("Status"), tr("Actions")});
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
        // Time column carries the age in its tooltip; the Expires column counts the
        // approval window down (tick_countdown() re-renders it every second).
        auto* time_item = new QTableWidgetItem(o.created_at.toString("HH:mm:ss"));
        time_item->setToolTip(tr("Queued %1").arg(order_age_text(o.created_at)));
        time_item->setForeground(QColor(fincept::ui::colors::TEXT_PRIMARY()));
        table_->setItem(row, kColTime, time_item);

        const bool is_pending = (o.status == QLatin1String("pending"));
        auto* exp_item = new QTableWidgetItem;
        if (is_pending) {
            exp_item->setData(kExpiryRole, o.expires_at);
            const qint64 left = ActionCenter::secs_until_expiry(o);
            exp_item->setText(remaining_text(left));
            exp_item->setForeground(left <= kExpiringSoonSecs ? QColor(fincept::ui::colors::WARNING())
                                                              : QColor(fincept::ui::colors::TEXT_PRIMARY()));
            exp_item->setToolTip(o.expires_at.isValid()
                                     ? tr("Auto-rejected at %1 if not approved")
                                           .arg(o.expires_at.toLocalTime().toString("HH:mm:ss"))
                                     : tr("Unknown queue time — this order cannot be approved"));
        } else {
            exp_item->setText(QStringLiteral("—"));
            exp_item->setForeground(QColor(fincept::ui::colors::TEXT_SECONDARY()));
        }
        table_->setItem(row, kColExpires, exp_item);
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
            // The 170px column elides the reason ("Expired: queued 22 minutes ago…", a broker
            // rejection, a basket's per-leg failures) — the full text is the point of the row.
            if (auto* reason_item = table_->item(row, kColActions))
                reason_item->setToolTip(o.rejection_reason);
            status_item->setToolTip(o.rejection_reason);
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

void PendingOrdersPanel::tick_countdown() {
    if (!table_)
        return;
    bool any_expired = false;
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* item = table_->item(row, kColExpires);
        if (!item)
            continue;
        const QVariant v = item->data(kExpiryRole);
        if (!v.isValid())
            continue; // not a pending row
        const QDateTime exp = v.toDateTime();
        const qint64 left = exp.isValid() ? QDateTime::currentDateTime().secsTo(exp) : 0;
        if (left <= 0)
            any_expired = true;
        const QString txt = remaining_text(left);
        if (item->text() != txt)
            item->setText(txt);
        item->setForeground(left <= kExpiringSoonSecs ? QColor(fincept::ui::colors::WARNING())
                                                      : QColor(fincept::ui::colors::TEXT_PRIMARY()));
    }
    // Don't wait for the service's sweep tick to drop the row — reject now; the
    // resulting order_rejected signal rebuilds the table.
    if (any_expired)
        ActionCenter::instance().expire_stale_orders();
}

QString PendingOrdersPanel::remaining_text(qint64 secs) {
    if (secs <= 0)
        return tr("EXPIRED");
    if (secs < 3600)
        return QStringLiteral("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QLatin1Char('0'));
    return tr("%1h %2m").arg(secs / 3600).arg((secs % 3600) / 60);
}

// Human-readable age of a queued order.
QString PendingOrdersPanel::order_age_text(const QDateTime& created) {
    const qint64 secs = created.isValid() ? created.secsTo(QDateTime::currentDateTime()) : 0;
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

    if (ActionCenter::is_expired(o)) {
        // Let the service record the refusal (with its reason) — nothing is sent.
        ActionCenter::instance().approve_order(pending_id);
        QMessageBox::information(this, tr("Approve Order"),
                                 tr("This order's approval window has closed — it was rejected as expired and "
                                    "nothing was sent. Re-place it if you still want the trade."));
        return true;
    }

    const auto acct_meta = trading::AccountManager::instance().get_account(o.account_id);
    const QString acct_label = acct_meta.display_name.isEmpty() ? o.account_id : acct_meta.display_name;
    const QString age = order_age_text(o.created_at);
    const qint64 left = ActionCenter::secs_until_expiry(o);
    const bool stale = left <= kExpiringSoonSecs;

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
    body += tr("\n  Expires in: %1").arg(remaining_text(left));
    if (stale) {
        body += tr("\n\n⚠  This order's approval window is about to close. If it closes while\n"
                   "this dialog is open the order is refused, not sent.");
    }
    body += tr("\n\nA MARKET order fills at the price when you approve, not the price\n"
               "that existed when it was queued.");

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

// ── Kill switch ─────────────────────────────────────────────────────────────

bool PendingOrdersPanel::confirm_and_run_kill_switch(QWidget* parent) {
    auto& ac = ActionCenter::instance();
    if (ac.kill_switch_running()) {
        QMessageBox::information(parent, tr("Kill Switch"),
                                 tr("The kill switch is already running — wait for its report."));
        return false;
    }
    const QStringList targets = ac.kill_switch_target_labels();
    if (targets.isEmpty()) {
        QMessageBox::information(parent, tr("Kill Switch"),
                                 tr("There is no active LIVE account — nothing to cancel or square off.\n\n"
                                    "Paper accounts are not touched by the kill switch; use SQUARE OFF ALL on the "
                                    "trading screen for those."));
        return false;
    }

    // Strong confirmation: spell out every consequence and every account, and make
    // the user type the word — a stray click or Enter can never fire it.
    const QString body =
        tr("KILL SWITCH — real money.\n\n"
           "On EVERY active LIVE account below this will:\n"
           "  1. reject every order still waiting for approval here\n"
           "  2. cancel ALL open / pending orders at the broker\n"
           "  3. square off ALL open positions with MARKET orders\n\n"
           "Holdings (delivery stock already in your demat) are NOT sold. Paper accounts are skipped.\n"
           "Algo deployments and agents are NOT stopped by this — pause them separately.\n\n"
           "Accounts (%1):\n%2\n\n"
           "Type KILL to confirm.")
            .arg(targets.size())
            .arg(QStringLiteral("  • ") + targets.join(QStringLiteral("\n  • ")));
    bool ok = false;
    const QString typed =
        QInputDialog::getText(parent, tr("Kill Switch"), body, QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || typed != QLatin1String("KILL")) {
        if (ok)
            QMessageBox::information(parent, tr("Kill Switch"), tr("Not confirmed — nothing was done."));
        return false;
    }

    // One-shot result report against the caller's window.
    QPointer<QWidget> host(parent);
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = QObject::connect(
        &ac, &ActionCenter::kill_switch_finished, &ac,
        [host, conn](const QVector<ActionCenter::KillSwitchAccountResult>& results) {
            QObject::disconnect(*conn);
            QStringList lines;
            bool all_ok = true;
            for (const auto& r : results) {
                if (r.skipped) {
                    lines << tr("• %1 [%2] — skipped (%3)%4")
                                 .arg(r.display_name, r.broker_id, r.skip_reason)
                                 .arg(r.queued_rejected > 0 ? tr(", %1 queued rejected").arg(r.queued_rejected)
                                                            : QString());
                    continue;
                }
                all_ok = all_ok && r.ok();
                QString line = tr("• %1 [%2] — %3: %4 orders cancelled (%5 failed), %6 positions closed (%7 failed), "
                                  "%8 queued rejected")
                                   .arg(r.display_name, r.broker_id, r.ok() ? tr("OK") : tr("CHECK BROKER"))
                                   .arg(r.orders_cancelled)
                                   .arg(r.orders_cancel_failed)
                                   .arg(r.positions_closed)
                                   .arg(r.positions_close_failed)
                                   .arg(r.queued_rejected);
                if (!r.errors.isEmpty())
                    line += QStringLiteral("\n     ") + r.errors.join(QStringLiteral("\n     "));
                lines << line;
            }
            const QString text = (all_ok ? tr("Kill switch finished. Every live account reported success.\n\n")
                                         : tr("Kill switch finished WITH ERRORS. Verify the accounts marked "
                                              "CHECK BROKER in the broker's own terminal now.\n\n")) +
                                 lines.join(QStringLiteral("\n"));
            QMessageBox box(host ? host.data() : nullptr);
            box.setWindowTitle(tr("Kill Switch Report"));
            box.setIcon(all_ok ? QMessageBox::Information : QMessageBox::Critical);
            box.setText(text);
            box.exec();
        });

    if (!ac.run_kill_switch()) {
        QObject::disconnect(*conn);
        QMessageBox::information(parent, tr("Kill Switch"), tr("The kill switch is already running."));
        return false;
    }
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
