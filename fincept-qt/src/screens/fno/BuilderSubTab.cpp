#include "screens/fno/BuilderSubTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/BuilderAnalyticsRibbon.h"
#include "screens/fno/LegEditorTable.h"
#include "screens/fno/OrderConfirmDialog.h"
#include "screens/fno/PayoffStripWidget.h"
#include "screens/fno/TemplateToolbar.h"
#include "services/options/OptionChainService.h"
#include "services/options/StrategyAnalytics.h"
#include "services/options/StrategyTemplates.h"
#include "storage/repositories/StrategiesRepository.h"
#include "trading/AccountManager.h"
#include "trading/UnifiedTrading.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>
#include <exception>

namespace fincept::screens::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainService;
using fincept::services::options::Strategy;
using fincept::services::options::StrategyAnalytics;
using fincept::services::options::StrategyInstantiationOptions;
using fincept::services::options::StrategyLeg;
using fincept::services::options::analytics::PayoffComputeOptions;
using namespace fincept::ui;

BuilderSubTab::BuilderSubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoBuilderTab");
    setStyleSheet(QString(
        "#fnoBuilderTab { background:%1; }"
        "#fnoBldrFooter { background:%2; border-top:1px solid %3; }"
        "#fnoSaveBtn, #fnoLoadBtn { background:%2; color:%4; border:1px solid %3; "
        "  padding:5px 14px; font-size:10px; font-weight:700; letter-spacing:0.4px; }"
        "#fnoSaveBtn:hover, #fnoLoadBtn:hover { background:%5; }"
        "#fnoTradeBtn { background:%3; color:%6; border:none; padding:5px 14px; "
        "  font-size:10px; font-weight:700; letter-spacing:0.4px; }"
        "#fnoBldrOiLabel { color:%6; font-size:10px; font-weight:700; "
        "  letter-spacing:0.4px; background:transparent; }"
        "#fnoBldrOiValue { color:%4; font-size:10px; font-weight:700; background:transparent; }"
        "QToolButton::menu-indicator { width:14px; }")
                      .arg(colors::BG_BASE(),
                           colors::BG_RAISED(),
                           colors::BORDER_DIM(),
                           colors::TEXT_PRIMARY(),
                           colors::BG_HOVER(),
                           colors::TEXT_SECONDARY()));

    setup_ui();
    connect(toolbar_, &TemplateToolbar::template_chosen, this, &BuilderSubTab::on_template_chosen);
    connect(toolbar_, &TemplateToolbar::add_leg_requested, this, [this]() {
        StrategyLeg blank;
        blank.type = fincept::trading::InstrumentType::CE;
        blank.lots = 1;
        blank.lot_size = 1;
        legs_view_->leg_model()->append_leg(blank);
    });
    connect(legs_view_->leg_model(), &LegEditorModel::legs_changed, this, &BuilderSubTab::on_legs_changed);
    connect(save_btn_, &QPushButton::clicked, this, &BuilderSubTab::on_save_clicked);
    connect(load_btn_, &QToolButton::clicked, this, &BuilderSubTab::on_load_clicked);
    connect(trade_btn_, &QPushButton::clicked, this, &BuilderSubTab::on_trade_clicked);

    connect(&OptionChainService::instance(), &OptionChainService::chain_published,
            this, [this](const OptionChain& chain) {
                last_chain_ = chain;
                legs_view_->leg_model()->set_chain(chain);
                if (pcr_label_)
                    pcr_label_->setText(QString::number(chain.pcr, 'f', 2));
                if (ce_oi_label_)
                    ce_oi_label_->setText(QString::number(chain.total_ce_oi / 1000.0, 'f', 0) + "K");
                if (pe_oi_label_)
                    pe_oi_label_->setText(QString::number(chain.total_pe_oi / 1000.0, 'f', 0) + "K");
                refresh_analytics();
            });
    chain_subscribed_ = true;

    const auto& cached = OptionChainService::instance().last_chain();
    if (!cached.rows.isEmpty()) {
        last_chain_ = cached;
        legs_view_->leg_model()->set_chain(cached);
        refresh_analytics();
    }
}

BuilderSubTab::~BuilderSubTab() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

void BuilderSubTab::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Row 1: analytics ribbon (2 rows, ~44px)
    ribbon_ = new BuilderAnalyticsRibbon(this);
    root->addWidget(ribbon_);

    // Row 2: template toolbar (28px)
    toolbar_ = new TemplateToolbar(this);
    root->addWidget(toolbar_);

    // Row 3: leg editor (stretch)
    legs_view_ = new LegEditorTable(this);
    root->addWidget(legs_view_, 1);

    // Row 4: payoff strip (fixed 80px)
    chart_ = new PayoffStripWidget(this);
    root->addWidget(chart_);

    // Row 5: footer
    auto* footer = new QWidget(this);
    footer->setObjectName("fnoBldrFooter");
    auto* foot_lay = new QHBoxLayout(footer);
    foot_lay->setContentsMargins(8, 6, 8, 6);
    foot_lay->setSpacing(8);

    save_btn_ = new QPushButton(tr("SAVE"), footer);
    save_btn_->setObjectName("fnoSaveBtn");
    save_btn_->setCursor(Qt::PointingHandCursor);
    foot_lay->addWidget(save_btn_);

    load_btn_ = new QToolButton(footer);
    load_btn_->setObjectName("fnoLoadBtn");
    load_btn_->setText(tr("LOAD") + " \xe2\x96\xbe");
    load_btn_->setCursor(Qt::PointingHandCursor);
    load_btn_->setPopupMode(QToolButton::InstantPopup);
    foot_lay->addWidget(load_btn_);

    foot_lay->addSpacing(16);
    target_label_ = new QLabel(tr("TARGET +"), footer);
    target_label_->setObjectName("fnoBldrOiLabel");
    days_to_target_spin_ = new QSpinBox(footer);
    days_to_target_spin_->setRange(0, 365);
    days_to_target_spin_->setValue(0);
    days_to_target_spin_->setSuffix(" d");
    days_to_target_spin_->setToolTip(
        tr("Days from today for the dashed target-day P/L curve. 0 = T+0."));
    days_to_target_spin_->setStyleSheet(QString(
        "QSpinBox { background:%1; color:%2; border:1px solid %3; padding:2px 4px; "
        "  font-size:11px; min-width:54px; }")
                                              .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(),
                                                   colors::BORDER_DIM()));
    connect(days_to_target_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { refresh_analytics(); });
    foot_lay->addWidget(target_label_);
    foot_lay->addWidget(days_to_target_spin_);

    foot_lay->addStretch(1);

    // PCR / OI labels on the right
    pcr_key_ = new QLabel(tr("PCR"), footer);
    pcr_key_->setObjectName("fnoBldrOiLabel");
    pcr_label_ = new QLabel(QString::fromUtf8("—"), footer);
    pcr_label_->setObjectName("fnoBldrOiValue");
    foot_lay->addWidget(pcr_key_);
    foot_lay->addWidget(pcr_label_);
    foot_lay->addSpacing(8);

    ce_key_ = new QLabel(tr("CE OI"), footer);
    ce_key_->setObjectName("fnoBldrOiLabel");
    ce_oi_label_ = new QLabel(QString::fromUtf8("—"), footer);
    ce_oi_label_->setObjectName("fnoBldrOiValue");
    foot_lay->addWidget(ce_key_);
    foot_lay->addWidget(ce_oi_label_);
    foot_lay->addSpacing(8);

    pe_key_ = new QLabel(tr("PE OI"), footer);
    pe_key_->setObjectName("fnoBldrOiLabel");
    pe_oi_label_ = new QLabel(QString::fromUtf8("—"), footer);
    pe_oi_label_->setObjectName("fnoBldrOiValue");
    foot_lay->addWidget(pe_key_);
    foot_lay->addWidget(pe_oi_label_);
    foot_lay->addSpacing(16);

    trade_btn_ = new QPushButton(tr("TRADE ALL (PAPER)"), footer);
    trade_btn_->setObjectName("fnoTradeBtn");
    trade_btn_->setEnabled(false);
    trade_btn_->setToolTip(tr("Build a strategy first — Trade All needs at least one active leg."));
    trade_btn_->setCursor(Qt::PointingHandCursor);
    foot_lay->addWidget(trade_btn_);
    root->addWidget(footer);
}

QVariantMap BuilderSubTab::save_state() const {
    QVariantMap m;
    return m;
}

void BuilderSubTab::restore_state(const QVariantMap& state) {
    Q_UNUSED(state);
}

void BuilderSubTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (chain_subscribed_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    QPointer<BuilderSubTab> self = this;
    hub.subscribe_pattern(this, QStringLiteral("option:chain:*"),
                          [self](const QString& topic, const QVariant& v) {
                              if (!self) return;
                              self->on_chain_published(topic, v);
                          });
    chain_subscribed_ = true;
}

void BuilderSubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

void BuilderSubTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void BuilderSubTab::retranslateUi() {
    if (save_btn_)  save_btn_->setText(tr("SAVE"));
    if (load_btn_)  load_btn_->setText(tr("LOAD") + " \xe2\x96\xbe");
    if (target_label_) target_label_->setText(tr("TARGET +"));
    if (days_to_target_spin_)
        days_to_target_spin_->setToolTip(
            tr("Days from today for the dashed target-day P/L curve. 0 = T+0."));
    if (pcr_key_) pcr_key_->setText(tr("PCR"));
    if (ce_key_)  ce_key_->setText(tr("CE OI"));
    if (pe_key_)  pe_key_->setText(tr("PE OI"));
    // Re-applies the TRADE button text + its enabled/tooltip state.
    if (trade_btn_) trade_btn_->setText(tr("TRADE ALL (PAPER)"));
    update_trade_button_state();
}

void BuilderSubTab::on_chain_published(const QString& topic, const QVariant& v) {
    Q_UNUSED(topic);
    if (!v.canConvert<OptionChain>())
        return;
    last_chain_ = v.value<OptionChain>();
    legs_view_->leg_model()->set_chain(last_chain_);
    if (pcr_label_)
        pcr_label_->setText(QString::number(last_chain_.pcr, 'f', 2));
    if (ce_oi_label_)
        ce_oi_label_->setText(QString::number(last_chain_.total_ce_oi / 1000.0, 'f', 0) + "K");
    if (pe_oi_label_)
        pe_oi_label_->setText(QString::number(last_chain_.total_pe_oi / 1000.0, 'f', 0) + "K");
    refresh_analytics();
}

void BuilderSubTab::on_template_chosen(const QString& template_id, const StrategyInstantiationOptions& options) {
    if (last_chain_.rows.isEmpty()) {
        QMessageBox::information(this, tr("No chain yet"),
                                 tr("Open the Chain tab first so a chain snapshot is loaded."));
        return;
    }
    const auto* tpl = fincept::services::options::find(template_id);
    if (!tpl) {
        LOG_WARN("FnoBuilder", QString("Unknown template id: %1").arg(template_id));
        return;
    }
    auto r = fincept::services::options::instantiate(*tpl, last_chain_, options);
    if (r.is_err()) {
        QMessageBox::warning(this, tr("Could not build strategy"), QString::fromStdString(r.error()));
        return;
    }
    legs_view_->leg_model()->set_legs(r.value().legs);
    loaded_strategy_id_ = 0;
    loaded_strategy_name_.clear();
    refresh_analytics();
}

void BuilderSubTab::on_legs_changed() {
    refresh_analytics();
}

Strategy BuilderSubTab::current_strategy() const {
    Strategy s;
    s.legs = legs_view_->leg_model()->legs();
    s.underlying = last_chain_.underlying;
    s.expiry = last_chain_.expiry;
    s.created = QDateTime::currentDateTime();
    s.modified = s.created;
    s.name = loaded_strategy_name_.isEmpty() ? tr("Custom") : loaded_strategy_name_;
    return s;
}

void BuilderSubTab::refresh_analytics() {
    const auto& legs = legs_view_->leg_model()->legs();
    if (legs.isEmpty() || last_chain_.rows.isEmpty()) {
        ribbon_->clear();
        chart_->clear_payoff();
        update_trade_button_state();
        return;
    }
    Strategy s = current_strategy();

    PayoffComputeOptions opts;
    opts.current_spot = last_chain_.spot;
    opts.days_to_target = days_to_target_spin_ ? days_to_target_spin_->value() : 0;

    auto curve = fincept::services::options::analytics::compute_payoff(s, opts);
    auto bes = fincept::services::options::analytics::compute_breakevens(curve);
    auto a = fincept::services::options::analytics::compute_all(s, last_chain_, opts);

    chart_->set_payoff(curve, last_chain_.spot, bes);
    ribbon_->update_from(s, a);
    update_trade_button_state();
}

void BuilderSubTab::update_trade_button_state() {
    const auto& legs = legs_view_->leg_model()->legs();
    int active = 0;
    for (const auto& l : legs)
        if (l.is_active && l.lots != 0)
            ++active;
    const bool ok = active > 0 && !last_chain_.rows.isEmpty();
    trade_btn_->setEnabled(ok);
    trade_btn_->setToolTip(ok ? tr("Place all active legs as paper orders.")
                              : tr("Build a strategy first — Trade All needs at least one active leg."));
}

namespace {
// Resolve the trading account for the chain's broker so Builder strategy trades
// land in the SAME paper portfolio as the chain's right-click Buy/Sell (one
// unified F&O blotter), rather than a disconnected standalone "F&O Paper"
// portfolio. Prefer an active account, else the first configured one. Mirrors
// ChainSubTab::find_account_for_broker.
QString find_account_for_broker(const QString& broker_id) {
    auto accounts = fincept::trading::AccountManager::instance().list_accounts(broker_id);
    for (const auto& acct : accounts)
        if (acct.is_active)
            return acct.account_id;
    return accounts.isEmpty() ? QString{} : accounts[0].account_id;
}
} // namespace

void BuilderSubTab::on_trade_clicked() {
    using namespace fincept::trading;
    const auto& legs = legs_view_->leg_model()->legs();
    if (legs.isEmpty() || last_chain_.rows.isEmpty())
        return;
    Strategy s = current_strategy();

    fincept::services::options::analytics::PayoffComputeOptions opts;
    opts.current_spot = last_chain_.spot;
    auto a = fincept::services::options::analytics::compute_all(s, last_chain_, opts);

    OrderConfirmDialog dlg(s, last_chain_, a.premium_paid, a.max_profit, a.max_loss, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    if (auto m = dlg.basket_margin(); m.has_value()) {
        const double val = m->final_margin > 0 ? m->final_margin : m->initial_margin;
        ribbon_->set_margin(val, /*estimated*/ false);
    }

    // Route every leg through the account-aware UnifiedTrading path (identical to
    // the chain's right-click Buy/Sell). For paper this fills market orders
    // immediately at the current premium, tags the position with its product
    // (NRML), stores the bare symbol so it marks to market, and keeps every F&O
    // paper position in ONE account portfolio so square-off and the shared
    // blotter can see them. Previously each leg was a raw "limit" order that was
    // never filled and lived in a separate portfolio nothing else could reach.
    const QString broker = last_chain_.broker_id;
    const QString account_id = find_account_for_broker(broker);
    if (account_id.isEmpty()) {
        QMessageBox::warning(
            this, tr("Trade Strategy"),
            tr("No connected %1 account. Connect one in Equity Trading to paper-trade this strategy.")
                .arg(broker.isEmpty() ? tr("trading") : broker));
        return;
    }

    int placed = 0;
    int failed = 0;
    QStringList failures;
    for (const auto& leg : legs) {
        if (!leg.is_active || leg.lots == 0)
            continue;
        UnifiedOrder order;
        order.symbol = leg.symbol;
        order.exchange = QStringLiteral("NFO");
        order.side = leg.lots > 0 ? OrderSide::Buy : OrderSide::Sell;
        order.order_type = OrderType::Market;       // paper: execute the strategy at current premiums
        order.quantity = std::abs(double(leg.lots) * double(leg.lot_size));
        order.price = leg.entry_price;              // current leg premium → paper fill price
        order.product_type = ProductType::Margin;   // NRML (positional) for F&O
        order.validity = QStringLiteral("DAY");
        auto resp = UnifiedTrading::instance().place_order(account_id, order);
        if (resp.success) {
            ++placed;
        } else {
            ++failed;
            failures.append(QString("%1: %2").arg(leg.symbol, resp.message));
        }
    }

    QString msg;
    if (failed == 0) {
        msg = tr("Placed %1 paper orders for %2 (%3).")
                  .arg(placed)
                  .arg(s.underlying, s.expiry);
    } else {
        msg = tr("Placed %1 of %2 paper orders. %3 failed:\n%4")
                  .arg(placed).arg(placed + failed).arg(failed).arg(failures.join("\n"));
    }
    LOG_INFO("FnoBuilder", msg.split('\n').first());
    QMessageBox::information(this, tr("Paper orders dispatched"), msg);
}

void BuilderSubTab::on_save_clicked() {
    const auto& legs = legs_view_->leg_model()->legs();
    if (legs.isEmpty()) {
        QMessageBox::information(this, tr("Nothing to save"), tr("Build a strategy first."));
        return;
    }

    auto& repo = fincept::StrategiesRepository::instance();
    if (loaded_strategy_id_ != 0) {
        Strategy s = current_strategy();
        s.name = loaded_strategy_name_.isEmpty() ? tr("Custom") : loaded_strategy_name_;
        auto r = repo.update(loaded_strategy_id_, s);
        if (r.is_err()) {
            QMessageBox::warning(this, tr("Save failed"), QString::fromStdString(r.error()));
            return;
        }
        LOG_INFO("FnoBuilder", QString("Updated strategy '%1' (id=%2)")
                                    .arg(s.name).arg(loaded_strategy_id_));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save strategy"), tr("Name"),
                                                QLineEdit::Normal, tr("My strategy"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    Strategy s = current_strategy();
    s.name = name.trimmed();
    auto r = repo.save(s);
    if (r.is_err()) {
        QMessageBox::warning(this, tr("Save failed"), QString::fromStdString(r.error()));
        return;
    }
    loaded_strategy_id_ = r.value();
    loaded_strategy_name_ = s.name;
    LOG_INFO("FnoBuilder", QString("Saved new strategy '%1' (id=%2)").arg(s.name).arg(r.value()));
}

void BuilderSubTab::on_load_clicked() {
    auto* menu = new QMenu(this);
    auto rows_r = fincept::StrategiesRepository::instance().list_all();
    if (rows_r.is_err() || rows_r.value().isEmpty()) {
        auto* a = menu->addAction(tr("(no saved strategies)"));
        a->setEnabled(false);
    } else {
        for (const auto& row : rows_r.value()) {
            QString label = row.strategy.name;
            if (!row.strategy.underlying.isEmpty())
                label += "  —  " + row.strategy.underlying;
            if (row.strategy.modified.isValid()) {
                const QString iso = row.strategy.modified.toString("yyyy-MM-dd HH:mm");
                label += "    [" + iso + "]";
            }
            auto* act = menu->addAction(label);
            const qint64 id = row.id;
            const QString name_for_msg = row.strategy.name;
            connect(act, &QAction::triggered, this, [this, id, name_for_msg]() {
                auto r = fincept::StrategiesRepository::instance().get(id);
                if (r.is_err()) {
                    QMessageBox::warning(this, tr("Load failed"), QString::fromStdString(r.error()));
                    return;
                }
                legs_view_->leg_model()->set_legs(r.value().strategy.legs);
                loaded_strategy_id_ = id;
                loaded_strategy_name_ = name_for_msg;
                refresh_analytics();
                LOG_INFO("FnoBuilder", QString("Loaded strategy '%1' (id=%2)").arg(name_for_msg).arg(id));
            });
        }
        menu->addSeparator();
        for (const auto& row : rows_r.value()) {
            auto* del = menu->addAction(tr("Delete: %1").arg(row.strategy.name));
            const qint64 id = row.id;
            const QString name = row.strategy.name;
            connect(del, &QAction::triggered, this, [this, id, name]() {
                const auto choice = QMessageBox::question(
                    this, tr("Delete saved strategy"),
                    tr("Delete '%1'? This can't be undone.").arg(name),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (choice != QMessageBox::Yes)
                    return;
                fincept::StrategiesRepository::instance().remove(id);
            });
        }
    }
    menu->exec(load_btn_->mapToGlobal(QPoint(0, load_btn_->height())));
    menu->deleteLater();
}

} // namespace fincept::screens::fno
