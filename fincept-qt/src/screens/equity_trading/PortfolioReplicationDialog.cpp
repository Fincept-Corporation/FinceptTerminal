#include "screens/equity_trading/PortfolioReplicationDialog.h"

#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/PaperTrading.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QPointer>
#include <QtConcurrent>

#include <cmath>

using namespace fincept::trading;
using namespace fincept::trading::replication;

namespace fincept::screens {

namespace {
QString fmt_money(double v) { return QStringLiteral("₹%L1").arg(v, 0, 'f', 2); }
} // namespace

PortfolioReplicationDialog::PortfolioReplicationDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Replicate Portfolio → Paper"));
    setMinimumSize(820, 560);

    auto* root = new QVBoxLayout(this);

    // ── Pickers ──
    auto* form = new QFormLayout;
    source_combo_ = new QComboBox;
    target_combo_ = new QComboBox;
    form->addRow(tr("Source account"), source_combo_);
    form->addRow(tr("Target (paper)"), target_combo_);
    root->addLayout(form);

    // ── Scope toggles ──
    auto* scope = new QHBoxLayout;
    inc_holdings_ = new QCheckBox(tr("Holdings"));
    inc_positions_ = new QCheckBox(tr("Positions"));
    inc_holdings_->setChecked(true);
    inc_positions_->setChecked(true);
    scope->addWidget(new QLabel(tr("Include:")));
    scope->addWidget(inc_holdings_);
    scope->addWidget(inc_positions_);
    scope->addStretch(1);
    root->addLayout(scope);

    // ── Preview table ──
    table_ = new QTableWidget;
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels({tr(""), tr("Type"), tr("Source"), tr("Target"),
                                       tr("Side"), tr("Qty"), tr("Est Price"), tr("Est Value / Note")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(table_, 1);

    // ── Footer (capital) ──
    auto* foot = new QHBoxLayout;
    footer_ = new QLabel;
    topup_btn_ = new QPushButton(tr("Top up paper balance"));
    topup_btn_->setEnabled(false);
    foot->addWidget(footer_, 1);
    foot->addWidget(topup_btn_);
    root->addLayout(foot);

    progress_ = new QProgressBar;
    progress_->setVisible(false);
    root->addWidget(progress_);
    status_ = new QLabel;
    root->addWidget(status_);

    // ── Buttons ──
    auto* btns = new QHBoxLayout;
    btns->addStretch(1);
    auto* cancel = new QPushButton(tr("Close"));
    replicate_btn_ = new QPushButton(tr("REPLICATE"));
    replicate_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:6px 18px;"
                "font-weight:700;letter-spacing:0.5px;}QPushButton:hover{background:%2;color:#000;}"
                "QPushButton:disabled{color:%4;}")
            .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::ORANGE(),
                 fincept::ui::colors::BORDER(), fincept::ui::colors::TEXT_SECONDARY()));
    btns->addWidget(cancel);
    btns->addWidget(replicate_btn_);
    root->addLayout(btns);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(replicate_btn_, &QPushButton::clicked, this, &PortfolioReplicationDialog::do_replicate);
    connect(topup_btn_, &QPushButton::clicked, this, &PortfolioReplicationDialog::do_top_up);
    connect(source_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &PortfolioReplicationDialog::reload_plan);
    connect(target_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &PortfolioReplicationDialog::reload_plan);
    connect(inc_holdings_, &QCheckBox::toggled, this, &PortfolioReplicationDialog::reload_plan);
    connect(inc_positions_, &QCheckBox::toggled, this, &PortfolioReplicationDialog::reload_plan);

    populate_accounts();
}

void PortfolioReplicationDialog::populate_accounts() {
    source_combo_->blockSignals(true);
    target_combo_->blockSignals(true);
    source_combo_->clear();
    target_combo_->clear();
    for (const auto& a : AccountManager::instance().list_accounts()) {
        if (!a.is_active)
            continue;
        const QString label = QStringLiteral("%1  (%2)").arg(a.display_name, a.trading_mode);
        source_combo_->addItem(label, a.account_id);
        if (a.trading_mode == QStringLiteral("paper") && !a.paper_portfolio_id.isEmpty())
            target_combo_->addItem(a.display_name, a.account_id);
    }
    source_combo_->blockSignals(false);
    target_combo_->blockSignals(false);

    if (target_combo_->count() == 0) {
        status_->setText(tr("No paper accounts found. Create one via ACCOUNTS first."));
        replicate_btn_->setEnabled(false);
    }
    reload_plan();
}

ReplicationOptions PortfolioReplicationDialog::current_options() const {
    return {inc_holdings_->isChecked(), inc_positions_->isChecked()};
}

void PortfolioReplicationDialog::reload_plan() {
    if (loading_)
        return;
    const QString src_id = source_combo_->currentData().toString();
    const QString tgt_id = target_combo_->currentData().toString();
    if (src_id.isEmpty() || tgt_id.isEmpty())
        return;

    source_items_.clear();
    table_->setRowCount(0);
    status_->setText(tr("Loading source portfolio…"));

    const auto src = AccountManager::instance().get_account(src_id);
    const auto tgt = AccountManager::instance().get_account(tgt_id);

    // Make sure both brokers' instruments are available for symbol mapping.
    InstrumentService::instance().load_from_db(src.broker_id);
    InstrumentService::instance().load_from_db(tgt.broker_id);

    if (src.trading_mode == QStringLiteral("paper")) {
        apply_items(paper_source_items(src.paper_portfolio_id), QString());
        return;
    }

    // Live source: load creds on GUI thread, fetch on a worker, deliver back.
    auto creds = AccountManager::instance().load_credentials(src_id);
    if (creds.api_key.isEmpty()) {
        apply_items({}, tr("No credentials for source account"));
        return;
    }
    loading_ = true;
    const QString bid = src.broker_id;
    QPointer<PortfolioReplicationDialog> self = this;
    (void)QtConcurrent::run([self, bid, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        QVector<BrokerHolding> holdings;
        QVector<BrokerPosition> positions;
        QString err;
        if (broker) {
            auto h = broker->get_holdings(creds);
            if (h.success && h.data)
                holdings = *h.data;
            auto p = broker->get_positions(creds);
            if (p.success && p.data)
                positions = *p.data;
            if (!h.success && !p.success)
                err = h.error.isEmpty() ? p.error : h.error;
        } else {
            err = QStringLiteral("Broker unavailable");
        }
        auto items = to_source_items(holdings, positions);
        QMetaObject::invokeMethod(self, [self, items, err]() {
            if (!self)
                return;
            self->loading_ = false;
            self->apply_items(items, err);
        }, Qt::QueuedConnection);
    });
}

void PortfolioReplicationDialog::apply_items(const QVector<SourceItem>& items, const QString& error) {
    source_items_ = items;
    if (!error.isEmpty())
        status_->setText(error);
    else if (items.isEmpty())
        status_->setText(tr("Source account has no holdings or positions."));
    else
        status_->clear();
    fill_table();
}

void PortfolioReplicationDialog::fill_table() {
    const QString src_id = source_combo_->currentData().toString();
    const QString tgt_id = target_combo_->currentData().toString();
    const auto src = AccountManager::instance().get_account(src_id);
    const auto tgt = AccountManager::instance().get_account(tgt_id);

    double available = 0.0;
    if (!tgt.paper_portfolio_id.isEmpty()) {
        try {
            available = pt_get_portfolio(tgt.paper_portfolio_id).balance;
        } catch (...) {
        }
    }

    plan_ = build_plan(source_items_, current_options(), available, src_id, tgt_id,
                       tgt.paper_portfolio_id, make_instrument_resolver(src.broker_id, tgt.broker_id));

    table_->setRowCount(plan_.orders.size());
    for (int row = 0; row < plan_.orders.size(); ++row) {
        const auto& po = plan_.orders[row];
        auto* chk = new QTableWidgetItem;
        chk->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        chk->setCheckState(po.included ? Qt::Checked : Qt::Unchecked);
        table_->setItem(row, 0, chk);
        table_->setItem(row, 1, new QTableWidgetItem(po.kind == ItemKind::Holding ? tr("Holding") : tr("Position")));
        table_->setItem(row, 2, new QTableWidgetItem(po.src_symbol));
        table_->setItem(row, 3, new QTableWidgetItem(po.norm_symbol));
        table_->setItem(row, 4, new QTableWidgetItem(po.side.toUpper()));
        table_->setItem(row, 5, new QTableWidgetItem(QString::number(po.quantity, 'f', 0)));
        table_->setItem(row, 6, new QTableWidgetItem(QString::number(po.est_price, 'f', 2)));
        table_->setItem(row, 7, new QTableWidgetItem(po.warning.isEmpty() ? fmt_money(po.est_value) : po.warning));
    }

    // Re-derive included flags from checkboxes whenever the user toggles a row.
    disconnect(table_, &QTableWidget::itemChanged, nullptr, nullptr);
    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* it) {
        if (it->column() != 0 || it->row() >= plan_.orders.size())
            return;
        plan_.orders[it->row()].included = (it->checkState() == Qt::Checked);
        recompute_footer();
    });

    recompute_footer();
}

void PortfolioReplicationDialog::recompute_footer() {
    double required = 0.0;
    int n = 0;
    for (const auto& po : plan_.orders)
        if (po.included) {
            required += po.est_value;
            ++n;
        }
    plan_.required_capital = required;
    const double avail = plan_.target_available;
    footer_->setText(tr("%1 stock(s) selected  •  Required %2  vs  Paper available %3")
                         .arg(n)
                         .arg(fmt_money(required))
                         .arg(fmt_money(avail)));
    const bool short_cash = required > avail;
    topup_btn_->setEnabled(short_cash && !plan_.target_paper_portfolio_id.isEmpty());
    topup_btn_->setText(short_cash ? tr("Top up paper balance to %1").arg(fmt_money(required))
                                   : tr("Top up paper balance"));
    replicate_btn_->setEnabled(n > 0);
}

void PortfolioReplicationDialog::do_top_up() {
    if (plan_.target_paper_portfolio_id.isEmpty())
        return;
    try {
        pt_set_balance(plan_.target_paper_portfolio_id, plan_.required_capital);
        plan_.target_available = plan_.required_capital;
        recompute_footer();
        status_->setText(tr("Paper balance set to %1").arg(fmt_money(plan_.required_capital)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Top up failed"), QString::fromUtf8(e.what()));
    }
}

void PortfolioReplicationDialog::do_replicate() {
    int n = 0;
    for (const auto& po : plan_.orders)
        if (po.included)
            ++n;
    if (n == 0)
        return;

    const auto ans = QMessageBox::question(
        this, tr("Confirm replication"),
        tr("Place %1 paper order(s) into the target account?\nThis is paper trading — no real money.").arg(n),
        QMessageBox::Yes | QMessageBox::No);
    if (ans != QMessageBox::Yes)
        return;

    progress_->setVisible(true);
    progress_->setRange(0, n);
    progress_->setValue(0);
    replicate_btn_->setEnabled(false);

    // GUI-thread execution (paper engine is not proven thread-safe off-main).
    ReplicationResult res = execute_plan(plan_);
    progress_->setValue(n);

    status_->setText(tr("Placed %1  •  Failed %2  •  Skipped %3")
                         .arg(res.placed)
                         .arg(res.failed)
                         .arg(res.skipped));

    QString detail;
    for (const auto& r : res.rows)
        if (!r.ok && !r.error.isEmpty())
            detail += QStringLiteral("• %1: %2\n").arg(r.symbol, r.error);
    if (!detail.isEmpty())
        QMessageBox::information(this, tr("Replication results"),
                                 tr("Placed %1, failed %2, skipped %3.\n\n%4")
                                     .arg(res.placed).arg(res.failed).arg(res.skipped).arg(detail));

    replicate_btn_->setEnabled(true);
    reload_plan(); // refresh against new paper balance/positions
}

} // namespace fincept::screens
