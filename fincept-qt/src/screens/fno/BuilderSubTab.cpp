#include "screens/fno/BuilderSubTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/BuilderAnalyticsRibbon.h"
#include "screens/fno/LegEditorTable.h"
#include "screens/fno/OrderConfirmDialog.h"
#include "screens/fno/PayoffChartWidget.h"
#include "screens/fno/TemplatePickerPanel.h"
#include "services/options/StrategyAnalytics.h"
#include "services/options/StrategyTemplates.h"
#include "storage/repositories/StrategiesRepository.h"
#include "trading/PaperTrading.h"
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
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>
#include <exception>

namespace fincept::screens::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::Strategy;
using fincept::services::options::StrategyAnalytics;
using fincept::services::options::StrategyInstantiationOptions;
using fincept::services::options::analytics::PayoffComputeOptions;
using namespace fincept::ui;

BuilderSubTab::BuilderSubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoBuilderTab");
    setStyleSheet(QString("#fnoBuilderTab { background:%1; }"
                          "#fnoBldrFooter { background:%2; border-top:1px solid %3; }"
                          "#fnoSaveBtn, #fnoLoadBtn { background:%2; color:%4; border:1px solid %3; "
                          "                            padding:5px 14px; font-size:10px; font-weight:700; "
                          "                            letter-spacing:0.4px; }"
                          "#fnoSaveBtn:hover, #fnoLoadBtn:hover { background:%5; }"
                          "#fnoTradeBtn { background:%3; color:%6; border:none; padding:5px 14px; "
                          "                font-size:10px; font-weight:700; letter-spacing:0.4px; }"
                          "QToolButton::menu-indicator { width:14px; }")
                      .arg(colors::BG_BASE(),     // %1 panel bg
                           colors::BG_RAISED(),   // %2 footer bg
                           colors::BORDER_DIM(),  // %3 border
                           colors::TEXT_PRIMARY(), // %4 primary text
                           colors::BG_HOVER(),    // %5 hover
                           colors::TEXT_SECONDARY())); // %6 disabled text

    setup_ui();
    connect(picker_, &TemplatePickerPanel::template_chosen, this, &BuilderSubTab::on_template_chosen);
    connect(legs_view_->leg_model(), &LegEditorModel::legs_changed, this, &BuilderSubTab::on_legs_changed);
    connect(save_btn_, &QPushButton::clicked, this, &BuilderSubTab::on_save_clicked);
    connect(load_btn_, &QToolButton::clicked, this, &BuilderSubTab::on_load_clicked);
    connect(trade_btn_, &QPushButton::clicked, this, &BuilderSubTab::on_trade_clicked);
}

BuilderSubTab::~BuilderSubTab() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

void BuilderSubTab::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top ribbon ────────────────────────────────────────────────────────
    ribbon_ = new BuilderAnalyticsRibbon(this);
    root->addWidget(ribbon_);

    // ── Body — left (picker over leg editor) | right (payoff chart) ──────
    auto* body = new QSplitter(Qt::Horizontal, this);
    body->setHandleWidth(1);
    body->setChildrenCollapsible(false);

    auto* left = new QSplitter(Qt::Vertical, body);
    left->setHandleWidth(1);
    left->setChildrenCollapsible(false);
    picker_ = new TemplatePickerPanel(left);
    legs_view_ = new LegEditorTable(left);
    left->addWidget(picker_);
    left->addWidget(legs_view_);
    left->setStretchFactor(0, 3);
    left->setStretchFactor(1, 2);

    chart_ = new PayoffChartWidget(body);

    body->addWidget(left);
    body->addWidget(chart_);
    body->setStretchFactor(0, 2);
    body->setStretchFactor(1, 3);
    root->addWidget(body, 1);

    // ── Footer — Save / Load / Trade ──────────────────────────────────────
    auto* footer = new QWidget(this);
    footer->setObjectName("fnoBldrFooter");
    auto* foot_lay = new QHBoxLayout(footer);
    foot_lay->setContentsMargins(8, 6, 8, 6);
    foot_lay->setSpacing(8);

    save_btn_ = new QPushButton("SAVE", footer);
    save_btn_->setObjectName("fnoSaveBtn");
    save_btn_->setCursor(Qt::PointingHandCursor);
    foot_lay->addWidget(save_btn_);

    load_btn_ = new QToolButton(footer);
    load_btn_->setObjectName("fnoLoadBtn");
    load_btn_->setText("LOAD ▾");
    load_btn_->setCursor(Qt::PointingHandCursor);
    load_btn_->setPopupMode(QToolButton::InstantPopup);
    foot_lay->addWidget(load_btn_);

    foot_lay->addSpacing(16);
    auto* tgt_label = new QLabel("TARGET +", footer);
    tgt_label->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; "
                                     "letter-spacing:0.4px; background:transparent;")
                                  .arg(colors::TEXT_SECONDARY()));
    days_to_target_spin_ = new QSpinBox(footer);
    days_to_target_spin_->setRange(0, 365);
    days_to_target_spin_->setValue(0);
    days_to_target_spin_->setSuffix(" d");
    days_to_target_spin_->setToolTip(
        "Days from today for the dashed target-day P/L curve. 0 = T+0 "
        "(curve coincides with today's mark-to-market). Capped at the "
        "strategy's nearest expiry.");
    days_to_target_spin_->setStyleSheet(QString(
        "QSpinBox { background:%1; color:%2; border:1px solid %3; padding:2px 4px; "
        "            font-size:11px; min-width:54px; }")
                                              .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(),
                                                   colors::BORDER_DIM()));
    connect(days_to_target_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { refresh_analytics(); });
    foot_lay->addWidget(tgt_label);
    foot_lay->addWidget(days_to_target_spin_);

    foot_lay->addStretch(1);
    trade_btn_ = new QPushButton("TRADE ALL (PAPER)", footer);
    trade_btn_->setObjectName("fnoTradeBtn");
    trade_btn_->setEnabled(false);
    trade_btn_->setToolTip("Build a strategy first — Trade All needs at least one active leg.");
    trade_btn_->setCursor(Qt::PointingHandCursor);
    foot_lay->addWidget(trade_btn_);
    root->addWidget(footer);
}

QVariantMap BuilderSubTab::save_state() const {
    QVariantMap m;
    // Phase 5 keeps state in-memory only — Saved Strategies tab covers
    // persistence for hand-built strategies. Splitter sizes etc. could be
    // stashed here later.
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
    if (!chain_subscribed_)
        return;
    fincept::datahub::DataHub::instance().unsubscribe_pattern(this, QStringLiteral("option:chain:*"));
    chain_subscribed_ = false;
}

void BuilderSubTab::on_chain_published(const QString& topic, const QVariant& v) {
    Q_UNUSED(topic);
    if (!v.canConvert<OptionChain>())
        return;
    last_chain_ = v.value<OptionChain>();
    refresh_analytics();
}

void BuilderSubTab::on_template_chosen(const QString& template_id, const StrategyInstantiationOptions& options) {
    if (last_chain_.rows.isEmpty()) {
        QMessageBox::information(this, "No chain yet",
                                 "Open the Chain tab first so a chain snapshot is loaded.");
        return;
    }
    const auto* tpl = fincept::services::options::find(template_id);
    if (!tpl) {
        LOG_WARN("FnoBuilder", QString("Unknown template id: %1").arg(template_id));
        return;
    }
    auto r = fincept::services::options::instantiate(*tpl, last_chain_, options);
    if (r.is_err()) {
        QMessageBox::warning(this, "Could not build strategy", QString::fromStdString(r.error()));
        return;
    }
    legs_view_->leg_model()->set_legs(r.value().legs);
    // Picking a template starts a new strategy — clear the loaded-id so
    // the next Save inserts a fresh row instead of overwriting whatever
    // was loaded before.
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
    s.name = loaded_strategy_name_.isEmpty() ? QStringLiteral("Custom") : loaded_strategy_name_;
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
    trade_btn_->setToolTip(ok ? QStringLiteral("Place all active legs as paper orders.")
                              : QStringLiteral("Build a strategy first — Trade All needs at least one active leg."));
}

QString BuilderSubTab::ensure_paper_portfolio() {
    const QString name = QStringLiteral("F&O Paper");
    const QString exchange = QStringLiteral("NFO");
    auto existing = fincept::trading::pt_find_portfolio(name, exchange);
    if (existing.has_value())
        return existing->id;
    // Create a fresh portfolio — INR ledger, no leverage so margin checks
    // mirror real-account behaviour, default fee 0.05% (broker average).
    auto p = fincept::trading::pt_create_portfolio(name, /*balance*/ 1'000'000.0,
                                                   /*currency*/ "INR",
                                                   /*leverage*/ 1.0,
                                                   /*margin_mode*/ "cross",
                                                   /*fee_rate*/ 0.0005,
                                                   /*exchange*/ exchange);
    LOG_INFO("FnoBuilder", QString("Created paper portfolio '%1' (id=%2)").arg(name, p.id));
    return p.id;
}

void BuilderSubTab::on_trade_clicked() {
    const auto& legs = legs_view_->leg_model()->legs();
    if (legs.isEmpty() || last_chain_.rows.isEmpty())
        return;
    Strategy s = current_strategy();

    // Recompute analytics for accurate dialog values.
    fincept::services::options::analytics::PayoffComputeOptions opts;
    opts.current_spot = last_chain_.spot;
    auto a = fincept::services::options::analytics::compute_all(s, last_chain_, opts);

    OrderConfirmDialog dlg(s, last_chain_, a.premium_paid, a.max_profit, a.max_loss, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // Push margin into the ribbon if the dialog managed to fetch one.
    if (auto m = dlg.basket_margin(); m.has_value()) {
        const double val = m->final_margin > 0 ? m->final_margin : m->initial_margin;
        ribbon_->set_margin(val, /*estimated*/ false);
    }

    const QString portfolio_id = ensure_paper_portfolio();
    int placed = 0;
    int failed = 0;
    QStringList failures;
    for (const auto& leg : legs) {
        if (!leg.is_active || leg.lots == 0)
            continue;
        const QString side = leg.lots > 0 ? "buy" : "sell";
        const double qty = std::abs(double(leg.lots) * double(leg.lot_size));
        try {
            fincept::trading::pt_place_order(portfolio_id, leg.symbol, side, "limit", qty,
                                             leg.entry_price);
            ++placed;
        } catch (const std::exception& e) {
            ++failed;
            failures.append(QString("%1: %2").arg(leg.symbol, e.what()));
        }
    }

    QString msg;
    if (failed == 0) {
        msg = QString("Placed %1 paper orders for %2 (%3).")
                  .arg(placed)
                  .arg(s.underlying, s.expiry);
    } else {
        msg = QString("Placed %1 of %2 paper orders. %3 failed:\n%4")
                  .arg(placed).arg(placed + failed).arg(failed).arg(failures.join("\n"));
    }
    LOG_INFO("FnoBuilder", msg.split('\n').first());
    QMessageBox::information(this, "Paper orders dispatched", msg);
}

void BuilderSubTab::on_save_clicked() {
    const auto& legs = legs_view_->leg_model()->legs();
    if (legs.isEmpty()) {
        QMessageBox::information(this, "Nothing to save", "Build a strategy first.");
        return;
    }

    // Branch: existing row → UPDATE silently; new strategy → prompt for a
    // name and INSERT.
    auto& repo = fincept::StrategiesRepository::instance();
    if (loaded_strategy_id_ != 0) {
        Strategy s = current_strategy();
        s.name = loaded_strategy_name_.isEmpty() ? QStringLiteral("Custom") : loaded_strategy_name_;
        auto r = repo.update(loaded_strategy_id_, s);
        if (r.is_err()) {
            QMessageBox::warning(this, "Save failed", QString::fromStdString(r.error()));
            return;
        }
        LOG_INFO("FnoBuilder", QString("Updated strategy '%1' (id=%2)")
                                    .arg(s.name).arg(loaded_strategy_id_));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this, "Save strategy", "Name",
                                                QLineEdit::Normal, "My strategy", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    Strategy s = current_strategy();
    s.name = name.trimmed();
    auto r = repo.save(s);
    if (r.is_err()) {
        QMessageBox::warning(this, "Save failed", QString::fromStdString(r.error()));
        return;
    }
    // Stamp the new id so subsequent Save clicks update in place.
    loaded_strategy_id_ = r.value();
    loaded_strategy_name_ = s.name;
    LOG_INFO("FnoBuilder", QString("Saved new strategy '%1' (id=%2)").arg(s.name).arg(r.value()));
}

void BuilderSubTab::on_load_clicked() {
    auto* menu = new QMenu(this);
    auto rows_r = fincept::StrategiesRepository::instance().list_all();
    if (rows_r.is_err() || rows_r.value().isEmpty()) {
        auto* a = menu->addAction("(no saved strategies)");
        a->setEnabled(false);
    } else {
        for (const auto& row : rows_r.value()) {
            QString label = row.strategy.name;
            if (!row.strategy.underlying.isEmpty())
                label += "  —  " + row.strategy.underlying;
            // Modified-at suffix in muted relative form ("today, 2h ago, ...").
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
                    QMessageBox::warning(this, "Load failed", QString::fromStdString(r.error()));
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
            auto* del = menu->addAction("Delete: " + row.strategy.name);
            const qint64 id = row.id;
            const QString name = row.strategy.name;
            connect(del, &QAction::triggered, this, [this, id, name]() {
                const auto choice = QMessageBox::question(
                    this, "Delete saved strategy",
                    QString("Delete '%1'? This can't be undone.").arg(name),
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
