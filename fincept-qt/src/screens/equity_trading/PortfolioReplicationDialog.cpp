#include "screens/equity_trading/PortfolioReplicationDialog.h"

#include "core/logging/Logger.h"
#include "screens/equity_trading/EquityTypes.h"
#include "services/markets/FxRateService.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/PaperTrading.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <cmath>
#include <memory>

using namespace fincept::trading;
using namespace fincept::trading::replication;

namespace fincept::screens {

namespace {
namespace fmt = fincept::ui::formatting;

// Preview columns. Table rows map 1:1 onto plan_.orders (the table is never sorted).
enum PrdColumn {
    kPrdCheck = 0,
    kPrdType,
    kPrdSource,
    kPrdTarget,
    kPrdSide,
    kPrdQty,
    kPrdPrice,
    kPrdValue,
    kPrdStatus,
    kPrdColumnCount
};

// A right-aligned number cell; NumericTableWidgetItem keeps the numeric value as its key.
QTableWidgetItem* prd_number_item(const QString& text, double value) {
    auto* it = new ui::NumericTableWidgetItem(text, value);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

// One KPI tile: label, value, tone (labels change between the plan and the result view).
void prd_put_tile(ui::KpiStrip* strip, int index, const QString& label, const QString& value,
                  ui::StatTile::Tone tone = ui::StatTile::Tone::Neutral, const QString& sub = QString()) {
    auto* t = strip ? strip->tile(index) : nullptr;
    if (!t)
        return;
    t->set_state(ui::StatTile::State::Normal);
    t->set_label(label);
    t->set_value(value);
    t->set_tone(tone);
    t->set_sub_label(sub);
}

// Tile labels while the table shows a plan (show_results() relabels them for a run).
QStringList prd_plan_labels() {
    return {PortfolioReplicationDialog::tr("ORDERS SELECTED"), PortfolioReplicationDialog::tr("REQUIRED"),
            PortfolioReplicationDialog::tr("PAPER AVAILABLE"), PortfolioReplicationDialog::tr("SHORTFALL")};
}

// The status line, coloured by what it reports (empty colour = default text).
void prd_set_status(QLabel* label, const QString& text, const QString& color = QString()) {
    label->setText(text);
    label->setStyleSheet(color.isEmpty() ? QString() : QStringLiteral("color:%1;").arg(color));
}
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
    refresh_btn_ = new QPushButton(tr("Refresh plan"));
    refresh_btn_->setToolTip(tr("Read the source account again and rebuild the plan"));
    scope->addWidget(refresh_btn_);
    root->addLayout(scope);

    // ── Capital at a glance (the run's outcome after REPLICATE) ──
    kpis_ = new ui::KpiStrip(this);
    kpis_->set_max_columns(4);
    for (const QString& label : prd_plan_labels()) {
        auto* tile = kpis_->add_tile(label);
        tile->set_compact(true);
        tile->set_state(ui::StatTile::State::Loading);
    }
    root->addWidget(kpis_);

    // ── Preview table ──
    table_ = new QTableWidget;
    table_->setColumnCount(kPrdColumnCount);
    table_->setHorizontalHeaderLabels({QString(), tr("Type"), tr("Source"), tr("Target"), tr("Side"), tr("Qty"),
                                       tr("Est Price"), tr("Est Value"), tr("Status")});
    for (int c : {int(kPrdQty), int(kPrdPrice), int(kPrdValue)}) {
        if (auto* h = table_->horizontalHeaderItem(c))
            h->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(table_, 1);
    // Item colours keep the value they were given; follow a theme switch.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { equity::retheme_items(table_); });

    // ── Status + cash top-up ──
    auto* foot = new QHBoxLayout;
    status_ = new QLabel;
    status_->setWordWrap(true);
    topup_btn_ = new QPushButton(tr("Top up paper balance"));
    topup_btn_->setProperty("fincept_agent_risk", "money");
    topup_btn_->setEnabled(false);
    foot->addWidget(status_, 1);
    foot->addWidget(topup_btn_);
    root->addLayout(foot);

    progress_ = new QProgressBar;
    progress_->setVisible(false);
    root->addWidget(progress_);

    // ── Buttons ──
    auto* btns = new QHBoxLayout;
    btns->addStretch(1);
    auto* cancel = new QPushButton(tr("Close"));
    replicate_btn_ = new QPushButton(tr("REPLICATE"));
    replicate_btn_->setProperty("fincept_agent_risk", "order");
    replicate_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:6px 18px;"
                "font-weight:700;letter-spacing:0.5px;}QPushButton:hover{background:%2;color:%5;}"
                "QPushButton:disabled{color:%4;}")
            .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::ORANGE(), fincept::ui::colors::BORDER(),
                 fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::TEXT_ON_ACCENT()));
    btns->addWidget(cancel);
    btns->addWidget(replicate_btn_);
    root->addLayout(btns);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(replicate_btn_, &QPushButton::clicked, this, &PortfolioReplicationDialog::do_replicate);
    connect(topup_btn_, &QPushButton::clicked, this, &PortfolioReplicationDialog::do_top_up);
    connect(source_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &PortfolioReplicationDialog::reload_plan);
    connect(target_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &PortfolioReplicationDialog::reload_plan);
    connect(refresh_btn_, &QPushButton::clicked, this, &PortfolioReplicationDialog::reload_plan);
    // The scope toggles only re-filter what is already loaded — re-planning from the cached
    // source items, not another round of broker calls per click.
    connect(inc_holdings_, &QCheckBox::toggled, this, [this](bool) { fill_table(); });
    connect(inc_positions_, &QCheckBox::toggled, this, [this](bool) { fill_table(); });
    // Lines left out for want of an FX rate: the plan asked for the rate; once rates
    // land, re-plan from the source already held (no broker call), keeping the rows
    // the user unticked (rows map 1:1 onto the same items while the scope is unchanged).
    connect(&fincept::services::FxRateService::instance(), &fincept::services::FxRateService::rates_changed, this,
            [this]() {
                if (showing_results_ || plan_.fx_missing.isEmpty() || source_items_.isEmpty())
                    return;
                // Only when a missing line can now be converted (not on every leg refresh).
                const FxFactor fx = make_fx_factor();
                bool any_now = false;
                for (const auto& po : plan_.orders) {
                    if (po.fx_missing && !po.currency.isEmpty() && fx(po.currency, currency_)) {
                        any_now = true;
                        break;
                    }
                }
                if (!any_now)
                    return;
                QSet<int> unticked;
                for (int i = 0; i < plan_.orders.size(); ++i)
                    if (!plan_.orders[i].included && plan_.orders[i].warning.isEmpty())
                        unticked.insert(i);
                fill_table();
                if (unticked.isEmpty())
                    return;
                {
                    const QSignalBlocker block(table_);
                    for (int i : unticked) {
                        if (i >= plan_.orders.size() || !plan_.orders[i].warning.isEmpty())
                            continue;
                        plan_.orders[i].included = false;
                        if (auto* chk = table_->item(i, kPrdCheck))
                            chk->setCheckState(Qt::Unchecked);
                    }
                }
                recompute_footer();
            });

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
    const QString src_id = source_combo_->currentData().toString();
    const QString tgt_id = target_combo_->currentData().toString();
    if (src_id.isEmpty() || tgt_id.isEmpty()) {
        // Nothing to plan (no source, or no paper target): placeholders, not "loading".
        const QStringList labels = prd_plan_labels();
        for (int i = 0; i < labels.size(); ++i)
            prd_put_tile(kpis_, i, labels[i], fmt::placeholder());
        return;
    }

    // A new selection supersedes anything still loading for the old one. The previous
    // `loading_` guard instead IGNORED the change while a fetch was in flight, so the
    // table ended up showing the old source's holdings under the newly picked account.
    const quint64 gen = ++plan_gen_;
    source_items_.clear();
    // Drop the old plan too: REPLICATE stayed enabled over the cleared table and would
    // have executed the previous selection's orders.
    plan_ = trading::replication::ReplicationPlan{};
    showing_results_ = false;
    table_->setRowCount(0);
    const QStringList labels = prd_plan_labels();
    for (int i = 0; i < kpis_->count() && i < labels.size(); ++i) {
        if (auto* t = kpis_->tile(i)) {
            t->set_label(labels[i]);
            t->clear_delta();
            t->set_sub_label(QString());
            t->set_tone(ui::StatTile::Tone::Neutral);
            t->set_state(ui::StatTile::State::Loading);
        }
    }
    replicate_btn_->setEnabled(false);
    topup_btn_->setEnabled(false);
    prd_set_status(status_, tr("Loading source portfolio…"));

    const auto src = AccountManager::instance().get_account(src_id);
    const auto tgt = AccountManager::instance().get_account(tgt_id);

    // The symbol resolver needs both brokers' instrument masters in memory.
    // load_from_db() is an async shim now (a cold master loads on a worker), so the plan
    // must wait for the load instead of resolving against an empty cache — which flagged
    // every row as unmappable the first time a broker was used. Callbacks fire
    // immediately on the UI thread when a cache is already warm.
    QStringList broker_ids{src.broker_id};
    if (tgt.broker_id != src.broker_id)
        broker_ids << tgt.broker_id;
    auto remaining = std::make_shared<int>(int(broker_ids.size()));
    QPointer<PortfolioReplicationDialog> self = this;
    for (const QString& bid : broker_ids) {
        InstrumentService::instance().load_from_db_async(bid, [self, gen, src_id, remaining](int) {
            if (!self || self->plan_gen_ != gen)
                return;
            if (--*remaining == 0)
                self->fetch_source(src_id, gen);
        });
    }
}

void PortfolioReplicationDialog::fetch_source(const QString& src_id, quint64 gen) {
    const auto src = AccountManager::instance().get_account(src_id);

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
    const QString bid = src.broker_id;
    QPointer<PortfolioReplicationDialog> self = this;
    (void)QtConcurrent::run([self, gen, bid, creds]() {
        auto* broker = BrokerRegistry::instance().get(bid);
        QVector<BrokerHolding> holdings;
        QVector<BrokerPosition> positions;
        QString err;
        // Exactly one of the two calls failing still yields a plan — but only half of
        // the account; say so instead of presenting it as complete.
        bool holdings_failed = false;
        bool positions_failed = false;
        QString holdings_err;
        QString positions_err;
        if (broker) {
            auto h = broker->get_holdings(creds);
            if (h.success && h.data)
                holdings = *h.data;
            auto p = broker->get_positions(creds);
            if (p.success && p.data)
                positions = *p.data;
            if (!h.success && !p.success) {
                err = h.error.isEmpty() ? p.error : h.error;
            } else {
                holdings_failed = !h.success;
                positions_failed = !p.success;
                holdings_err = h.error;
                positions_err = p.error;
            }
        } else {
            err = QStringLiteral("Broker unavailable");
        }
        auto items = to_source_items(holdings, positions);
        // The dialog can be closed while the (blocking) broker calls run, and
        // QMetaObject::invokeMethod asserts on a null receiver.
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, gen, items, err, holdings_failed, positions_failed, holdings_err, positions_err]() {
                if (!self || self->plan_gen_ != gen)
                    return; // the user picked another source/target while this was in flight
                QString note;
                if (holdings_failed || positions_failed) {
                    const QString why = (holdings_failed ? holdings_err : positions_err).trimmed();
                    note = (holdings_failed
                                ? self->tr("The source account's holdings could not be read%1 — the plan below covers "
                                           "its positions only.")
                                : self->tr("The source account's positions could not be read%1 — the plan below covers "
                                           "its holdings only."))
                               .arg(why.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(why));
                }
                self->apply_items(items, err, note);
            },
            Qt::QueuedConnection);
    });
}

QString PortfolioReplicationDialog::money(double v) const {
    return fmt::money(v, currency_);
}

void PortfolioReplicationDialog::apply_items(const QVector<SourceItem>& items, const QString& error,
                                             const QString& note) {
    source_items_ = items;
    // A segment the venue table does not list (MCX, CDS ...) is quoted in the source
    // broker's currency; the plan converts every line into the target's.
    QString src_ccy;
    if (auto* b = BrokerRegistry::instance().get(
            AccountManager::instance().get_account(source_combo_->currentData().toString()).broker_id))
        src_ccy = b->profile().currency;
    for (auto& it : source_items_) {
        if (it.currency.isEmpty())
            it.currency = equity::exchange_currency(it.exchange.isEmpty() ? QStringLiteral("NSE") : it.exchange, src_ccy);
    }
    if (!error.isEmpty())
        prd_set_status(status_, error, fincept::ui::colors::DANGER());
    else if (!note.isEmpty())
        prd_set_status(status_, note, fincept::ui::colors::WARNING());
    else if (items.isEmpty())
        prd_set_status(status_, tr("Source account has no holdings or positions."));
    else
        prd_set_status(status_, QString());
    fill_table();
}

void PortfolioReplicationDialog::fill_table() {
    const QString src_id = source_combo_->currentData().toString();
    const QString tgt_id = target_combo_->currentData().toString();
    const auto src = AccountManager::instance().get_account(src_id);
    const auto tgt = AccountManager::instance().get_account(tgt_id);

    double available = 0.0;
    currency_.clear();
    if (!tgt.paper_portfolio_id.isEmpty()) {
        try {
            const auto pf = pt_get_portfolio(tgt.paper_portfolio_id);
            available = pf.balance;
            currency_ = pf.currency;
        } catch (...) {
            LOG_WARN("PortfolioReplication", fincept::logf("Target paper portfolio could not be read",
                                                           {{"portfolio", fincept::log_mask(tgt.paper_portfolio_id)}}));
        }
    }
    // The paper portfolio is created in its broker's currency; fall back to that.
    if (currency_.isEmpty()) {
        if (auto* b = BrokerRegistry::instance().get(tgt.broker_id))
            currency_ = b->profile().currency;
    }

    // Every line's value is converted into the paper book's currency before it is summed
    // and compared with the paper balance; a line with no rate is left out and listed.
    plan_ = build_plan(source_items_, current_options(), available, src_id, tgt_id, tgt.paper_portfolio_id,
                       make_instrument_resolver(src.broker_id, tgt.broker_id), currency_, make_fx_factor());
    showing_results_ = false; // a fresh plan replaces any result view

    // Est Value is in the paper book's currency; Est Price mixes currencies (each cell names its own).
    if (auto* h = table_->horizontalHeaderItem(kPrdValue))
        h->setText(currency_.isEmpty() ? tr("Est Value") : fmt::header(tr("Est Value"), currency_));
    const QColor warn(fincept::ui::colors::WARNING());
    {
        // Populating is not a user toggle: keep itemChanged quiet until the end.
        const QSignalBlocker block(table_);
        table_->setRowCount(plan_.orders.size());
        for (int row = 0; row < plan_.orders.size(); ++row) {
            const auto& po = plan_.orders[row];
            const bool unusable = !po.warning.isEmpty();
            auto* chk = new QTableWidgetItem;
            // A row the plan cannot place (not tradable on the target, no whole share,
            // no price) gets a disabled box — it can't be ticked into the run.
            Qt::ItemFlags chk_flags = Qt::ItemIsUserCheckable;
            if (!unusable)
                chk_flags |= Qt::ItemIsEnabled;
            chk->setFlags(chk_flags);
            chk->setCheckState(po.included ? Qt::Checked : Qt::Unchecked);
            if (unusable)
                chk->setToolTip(po.warning);
            table_->setItem(row, kPrdCheck, chk);
            table_->setItem(row, kPrdType,
                            new QTableWidgetItem(po.kind == ItemKind::Holding ? tr("Holding") : tr("Position")));
            table_->setItem(row, kPrdSource, new QTableWidgetItem(po.src_symbol));
            table_->setItem(row, kPrdTarget, new QTableWidgetItem(po.norm_symbol));
            table_->setItem(row, kPrdSide, new QTableWidgetItem(po.side.toUpper()));
            table_->setItem(row, kPrdQty, prd_number_item(equity::format_quantity(po.quantity), po.quantity));
            // The source price in its own currency (never converted).
            table_->setItem(row, kPrdPrice,
                            prd_number_item(po.est_price > 0
                                                ? fmt::price_with_symbol(po.est_price,
                                                                         fmt::instrument_for_symbol(po.norm_symbol, po.currency))
                                                : fmt::placeholder(),
                                            po.est_price));
            // The value in the paper book's currency: "≈" + rate / time in the tooltip
            // when converted; "—" when there is no rate (the row is then left out).
            const bool has_target = std::isfinite(po.est_value_target);
            QString value_text = fmt::placeholder();
            QString value_tip;
            if (po.est_value > 0 && has_target) {
                value_text = po.fx_converted ? QStringLiteral("\xe2\x89\x88 ") + money(po.est_value_target)
                                             : money(po.est_value_target);
                if (po.fx_converted) {
                    const fmt::ConvertedMoney c =
                        fincept::services::fx::convert_for_display(po.est_value, po.currency, currency_);
                    const QString src_amount = fmt::money(fmt::to_major_units(po.est_value, po.currency),
                                                          fmt::major_currency(po.currency));
                    const QString note = fmt::fx_note(c);
                    value_tip = note.isEmpty() ? src_amount : tr("%1 at %2").arg(src_amount, note);
                }
            } else if (po.est_value > 0 && po.fx_missing) {
                value_tip = po.warning;
            }
            auto* value_item = prd_number_item(value_text, has_target ? po.est_value_target : 0.0);
            if (!value_tip.isEmpty())
                value_item->setToolTip(value_tip);
            table_->setItem(row, kPrdValue, value_item);
            auto* note = new QTableWidgetItem(po.warning);
            if (unusable) {
                note->setForeground(warn);
                note->setToolTip(po.warning);
            }
            table_->setItem(row, kPrdStatus, note);
        }
    }
    table_->resizeColumnsToContents();

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
    // Summed in the paper book's currency (each line converted by the plan). A line
    // without a rate is never in here — it cannot be ticked — so the total is partial.
    double required = 0.0;
    int n = 0;
    for (const auto& po : plan_.orders)
        if (po.included && std::isfinite(po.est_value_target)) {
            required += po.est_value_target;
            ++n;
        }
    plan_.required_capital = required;
    const double avail = plan_.target_available;
    const bool short_cash = required > avail;
    const int missing = int(plan_.fx_missing.size());
    const QString partial =
        missing > 0 ? tr("partial: %n line(s) without an FX rate not counted", "", missing) : QString();
    if (!showing_results_) {
        using Tone = ui::StatTile::Tone;
        const QStringList labels = prd_plan_labels();
        prd_put_tile(kpis_, 0, labels.value(0), fmt::number(n, 0), Tone::Neutral,
                     tr("of %1 in the plan").arg(fmt::number(plan_.orders.size(), 0)));
        prd_put_tile(kpis_, 1, labels.value(1), money(required), missing > 0 ? Tone::Warning : Tone::Neutral, partial);
        prd_put_tile(kpis_, 2, labels.value(2), money(avail));
        prd_put_tile(kpis_, 3, labels.value(3), money(short_cash ? required - avail : 0.0),
                     short_cash ? Tone::Negative : Tone::Neutral,
                     short_cash ? tr("top up to place every selected order") : partial);
        if (auto* t = kpis_->tile(1))
            t->setToolTip(missing > 0 ? tr("Not counted (no FX rate into %1 yet): %2")
                                            .arg(currency_, plan_.fx_missing.join(QStringLiteral(", ")))
                                      : QString());
    }
    // A result view is not a plan: no top-up or second run until it is refreshed.
    topup_btn_->setEnabled(short_cash && !plan_.target_paper_portfolio_id.isEmpty() && !showing_results_);
    topup_btn_->setText(short_cash ? tr("Top up paper balance to %1").arg(money(required))
                                   : tr("Top up paper balance"));
    replicate_btn_->setEnabled(n > 0 && !showing_results_);
}

void PortfolioReplicationDialog::do_top_up() {
    if (plan_.target_paper_portfolio_id.isEmpty())
        return;
    try {
        pt_set_balance(plan_.target_paper_portfolio_id, plan_.required_capital);
        plan_.target_available = plan_.required_capital;
        recompute_footer();
        prd_set_status(status_, tr("Paper balance set to %1").arg(money(plan_.required_capital)),
                       fincept::ui::colors::SUCCESS());
    } catch (const std::exception& e) {
        LOG_ERROR("PortfolioReplication",
                  fincept::logf("Paper balance top-up failed",
                                {{"portfolio", fincept::log_mask(plan_.target_paper_portfolio_id)}, {"error", e.what()}}));
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

    // Each row's outcome goes into the table it was planned in; "Refresh plan"
    // re-reads the source and the new paper balance / positions.
    show_results(res);
}

void PortfolioReplicationDialog::show_results(const ReplicationResult& res) {
    namespace c = fincept::ui::colors;
    using Tone = ui::StatTile::Tone;
    showing_results_ = true;

    {
        // Not user toggles: keep itemChanged (the plan's include flags) out of it.
        const QSignalBlocker block(table_);
        // execute_plan answers one row per planned order, in plan order.
        for (int row = 0; row < table_->rowCount(); ++row) {
            if (auto* chk = table_->item(row, kPrdCheck))
                chk->setFlags(chk->flags() & ~(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled)); // the plan has run
            if (row >= res.rows.size() || row >= plan_.orders.size())
                continue;
            const auto& r = res.rows[row];
            const bool was_included = plan_.orders[row].included;
            QString text;
            QColor color;
            if (r.ok) {
                text = tr("PLACED");
                color = QColor(c::SUCCESS());
            } else if (was_included) {
                text = r.error.isEmpty() ? tr("NOT PLACED") : tr("NOT PLACED · %1").arg(r.error);
                color = QColor(c::DANGER());
            } else {
                text = r.error.isEmpty() ? tr("SKIPPED") : tr("SKIPPED · %1").arg(r.error);
                color = QColor(c::TEXT_SECONDARY());
            }
            auto* status = table_->item(row, kPrdStatus);
            if (!status) {
                status = new QTableWidgetItem;
                table_->setItem(row, kPrdStatus, status);
            }
            status->setText(text);
            status->setForeground(color);
            status->setToolTip(r.error);
        }
    }

    // Headline: the run's outcome, and the paper cash left after it.
    double balance = plan_.target_available;
    if (!plan_.target_paper_portfolio_id.isEmpty()) {
        try {
            balance = pt_get_portfolio(plan_.target_paper_portfolio_id).balance;
        } catch (...) {
            LOG_WARN("PortfolioReplication",
                     fincept::logf("Target paper balance could not be re-read",
                                   {{"portfolio", fincept::log_mask(plan_.target_paper_portfolio_id)}}));
        }
    }
    prd_put_tile(kpis_, 0, tr("PLACED"), fmt::number(res.placed, 0), res.placed > 0 ? Tone::Positive : Tone::Neutral);
    prd_put_tile(kpis_, 1, tr("FAILED"), fmt::number(res.failed, 0), res.failed > 0 ? Tone::Negative : Tone::Neutral);
    prd_put_tile(kpis_, 2, tr("SKIPPED"), fmt::number(res.skipped, 0));
    prd_put_tile(kpis_, 3, tr("PAPER CASH NOW"), money(balance));

    const QString summary = tr("Placed %1 · failed %2 · skipped %3 — each row's outcome is in the Status column. "
                               "Use Refresh plan to replicate again.")
                                .arg(res.placed)
                                .arg(res.failed)
                                .arg(res.skipped);
    prd_set_status(status_, summary,
                   res.failed > 0 ? QString(c::DANGER()) : res.placed > 0 ? QString(c::SUCCESS()) : QString());
    recompute_footer(); // REPLICATE / top-up stay off over the result view
}

} // namespace fincept::screens
