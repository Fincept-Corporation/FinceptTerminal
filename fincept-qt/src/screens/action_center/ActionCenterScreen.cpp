// ActionCenterScreen.cpp — semi-auto order approval UI (Phase 3 §2).
#include "screens/action_center/ActionCenterScreen.h"

#include "trading/AccountManager.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace fincept::screens {

using fincept::trading::ActionCenter;
using fincept::trading::OrderMode;
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

ActionCenterScreen::ActionCenterScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("actionCenterScreen");
    build_ui();
}

void ActionCenterScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Global stylesheet via descendant selectors (P7) — amber #d97706 accents.
    setStyleSheet(QString(
        "#actionCenterScreen{background:%1;}"
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
        "#acRejectAll:hover{background:rgba(220,38,38,0.25);}")
        .arg(fincept::ui::colors::DARK(), fincept::ui::colors::PANEL(),
             fincept::ui::colors::BORDER(), fincept::ui::colors::AMBER(),
             fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::TEXT_PRIMARY(),
             fincept::ui::colors::POSITIVE(), fincept::ui::colors::NEGATIVE()));

    // ── Header: title + account selector + status filter + mode toggle ────────
    auto* header = new QWidget;
    header->setObjectName("acHeader");
    auto* hlay = new QHBoxLayout(header);
    hlay->setContentsMargins(14, 8, 14, 8);
    hlay->setSpacing(12);

    title_label_ = new QLabel(tr("ACTION CENTER"));
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

    mode_caption_ = new QLabel(tr("Mode:"));
    hlay->addWidget(mode_caption_);
    mode_combo_ = new QComboBox;
    mode_combo_->setObjectName("acCombo");
    mode_combo_->addItem(tr("Auto"), "auto");
    mode_combo_->addItem(tr("Semi-Auto"), "semi_auto");
    hlay->addWidget(mode_combo_);

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
    table_->setHorizontalHeaderLabels({tr("Time"), tr("Account"), tr("Type"), tr("Symbol"), tr("Side"),
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
    connect(account_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
        // Reflect the selected account's persisted mode, then reload.
        const QString acct = selected_account();
        if (!acct.isEmpty()) {
            const auto mode = ActionCenter::instance().get_order_mode(acct);
            const int idx = mode_combo_->findData(fincept::trading::order_mode_str(mode));
            if (idx >= 0) {
                QSignalBlocker b(mode_combo_);
                mode_combo_->setCurrentIndex(idx);
            }
            mode_combo_->setEnabled(true);
        } else {
            mode_combo_->setEnabled(false); // can't set a mode for "all accounts"
        }
        refresh();
    });
    connect(status_filter_, &QComboBox::currentIndexChanged, this, [this](int) { refresh(); });
    connect(mode_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
        const QString acct = selected_account();
        if (acct.isEmpty())
            return;
        ActionCenter::instance().set_order_mode(
            acct, fincept::trading::parse_order_mode(mode_combo_->currentData().toString()));
    });
    connect(approve_all_btn_, &QPushButton::clicked, this, [this]() {
        const QString acct = selected_account();
        auto ans = QMessageBox::question(this, tr("Approve All"),
                                         tr("Execute ALL pending orders now?"),
                                         QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
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
        const QString reason = QInputDialog::getText(this, tr("Reject All"),
                                                     tr("Rejection reason:"), QLineEdit::Normal,
                                                     tr("Rejected by user"), &ok);
        if (!ok)
            return;
        ActionCenter::instance().reject_all_pending(acct, reason);
    });
}

void ActionCenterScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ActionCenterScreen::retranslateUi() {
    if (title_label_)     title_label_->setText(tr("ACTION CENTER"));
    if (account_caption_) account_caption_->setText(tr("Account:"));
    if (show_caption_)    show_caption_->setText(tr("Show:"));
    if (mode_caption_)    mode_caption_->setText(tr("Mode:"));

    // Combo items: re-set visible text by index; userData keys are unchanged.
    if (status_filter_ && status_filter_->count() >= 4) {
        status_filter_->setItemText(0, tr("Pending"));
        status_filter_->setItemText(1, tr("Approved"));
        status_filter_->setItemText(2, tr("Rejected"));
        status_filter_->setItemText(3, tr("All"));
    }
    if (mode_combo_ && mode_combo_->count() >= 2) {
        mode_combo_->setItemText(0, tr("Auto"));
        mode_combo_->setItemText(1, tr("Semi-Auto"));
    }

    if (stat_pending_cap_)  stat_pending_cap_->setText(tr("PENDING"));
    if (stat_approved_cap_) stat_approved_cap_->setText(tr("APPROVED"));
    if (stat_rejected_cap_) stat_rejected_cap_->setText(tr("REJECTED"));
    if (stat_buy_cap_)      stat_buy_cap_->setText(tr("BUY"));
    if (stat_sell_cap_)     stat_sell_cap_->setText(tr("SELL"));

    if (approve_all_btn_) approve_all_btn_->setText(tr("APPROVE ALL"));
    if (reject_all_btn_)  reject_all_btn_->setText(tr("REJECT ALL"));

    if (table_) {
        table_->setHorizontalHeaderLabels({tr("Time"), tr("Account"), tr("Type"), tr("Symbol"), tr("Side"),
                                           tr("Qty"), tr("Price Type"), tr("Status"), tr("Actions")});
    }
    // Per-row Approve/Reject button widgets are rebuilt on the next refresh().
}

QString ActionCenterScreen::selected_account() const {
    return account_combo_ ? account_combo_->currentData().toString() : QString();
}

void ActionCenterScreen::reload_accounts() {
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
    mode_combo_->setEnabled(!selected_account().isEmpty());
}

void ActionCenterScreen::refresh() {
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
        set(kColTime, o.created_at.toString("HH:mm:ss"));
        // Resolve account display name.
        const auto acct_meta = trading::AccountManager::instance().get_account(o.account_id);
        set(kColAccount, acct_meta.display_name.isEmpty() ? o.account_id : acct_meta.display_name);
        set(kColType, o.order_type);
        set(kColSymbol, o.symbol);

        auto* side_item = new QTableWidgetItem(o.action);
        const QString actUp = o.action.toUpper();
        side_item->setForeground(actUp == "BUY"   ? QColor(fincept::ui::colors::POSITIVE())
                                 : actUp == "SELL" ? QColor(fincept::ui::colors::NEGATIVE())
                                                   : QColor(fincept::ui::colors::TEXT_PRIMARY()));
        table_->setItem(row, kColSide, side_item);

        set(kColQty, o.quantity);
        set(kColPriceType, o.price_type);

        auto* status_item = new QTableWidgetItem(o.status.toUpper());
        status_item->setForeground(o.status == "approved" ? QColor(fincept::ui::colors::POSITIVE())
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
            approve->setCursor(Qt::PointingHandCursor);
            approve->setStyleSheet(QString("QPushButton{background:rgba(34,197,94,0.14);color:%1;"
                                           "border:1px solid %1;padding:2px 10px;font-size:10px;"
                                           "font-weight:700;border-radius:2px;}"
                                           "QPushButton:hover{background:rgba(34,197,94,0.28);}")
                                       .arg(fincept::ui::colors::POSITIVE()));
            auto* reject = new QPushButton(tr("Reject"));
            reject->setCursor(Qt::PointingHandCursor);
            reject->setStyleSheet(QString("QPushButton{background:rgba(220,38,38,0.12);color:%1;"
                                          "border:1px solid %1;padding:2px 10px;font-size:10px;"
                                          "font-weight:700;border-radius:2px;}"
                                          "QPushButton:hover{background:rgba(220,38,38,0.25);}")
                                      .arg(fincept::ui::colors::NEGATIVE()));
            const QString id = o.id;
            connect(approve, &QPushButton::clicked, this, [this, id]() { approve_row(id); });
            connect(reject, &QPushButton::clicked, this, [this, id]() { reject_row(id); });
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

void ActionCenterScreen::refresh_stats() {
    const auto s = ActionCenter::instance().get_stats(selected_account());
    stat_pending_->setText(QString::number(s.total_pending));
    stat_approved_->setText(QString::number(s.total_approved));
    stat_rejected_->setText(QString::number(s.total_rejected));
    stat_buy_->setText(QString::number(s.total_buy));
    stat_sell_->setText(QString::number(s.total_sell));
}

void ActionCenterScreen::approve_row(const QString& pending_id) {
    ActionCenter::instance().approve_order(pending_id);
}

void ActionCenterScreen::reject_row(const QString& pending_id) {
    bool ok = false;
    const QString reason = QInputDialog::getText(this, tr("Reject Order"), tr("Rejection reason:"),
                                                 QLineEdit::Normal, tr("Rejected by user"), &ok);
    if (!ok)
        return;
    ActionCenter::instance().reject_order(pending_id, reason);
}

// ── ActionCenter signal slots ───────────────────────────────────────────────

void ActionCenterScreen::on_pending_created(const PendingOrder&) { refresh(); }
void ActionCenterScreen::on_order_approved(const QString&, const QString&) { refresh(); }
void ActionCenterScreen::on_order_rejected(const QString&, const QString&) { refresh(); }
void ActionCenterScreen::on_stats_updated(const QString&) { refresh_stats(); }

// ── Visibility lifecycle (P3) ───────────────────────────────────────────────

void ActionCenterScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!wired_) {
        wired_ = true;
        auto& ac = ActionCenter::instance();
        connect(&ac, &ActionCenter::pending_order_created, this,
                &ActionCenterScreen::on_pending_created);
        connect(&ac, &ActionCenter::order_approved, this, &ActionCenterScreen::on_order_approved);
        connect(&ac, &ActionCenter::order_rejected, this, &ActionCenterScreen::on_order_rejected);
        connect(&ac, &ActionCenter::stats_updated, this, &ActionCenterScreen::on_stats_updated);
        // Refresh the account list when accounts change.
        connect(&trading::AccountManager::instance(), &trading::AccountManager::account_added, this,
                [this](const QString&) { reload_accounts(); });
        connect(&trading::AccountManager::instance(), &trading::AccountManager::account_removed,
                this, [this](const QString&) { reload_accounts(); });
        connect(&trading::AccountManager::instance(), &trading::AccountManager::account_updated,
                this, [this](const QString&) { reload_accounts(); });
    }
    reload_accounts();
    refresh();
}

void ActionCenterScreen::hideEvent(QHideEvent* e) { QWidget::hideEvent(e); }

} // namespace fincept::screens
