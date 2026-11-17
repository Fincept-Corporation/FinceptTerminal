#include "screens/algo_trading/AlgoDeployDialog.h"

#include "algo_engine/RiskLimits.h"
#include "services/algo_trading/AlgoTradingTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/algo/AlgoCurrency.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimeEdit>
#include <QUuid>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::trading;
using namespace fincept::services::algo;

AlgoDeployDialog::AlgoDeployDialog(const QString& strategy_id, const QString& strategy_name, QWidget* parent)
    : QDialog(parent), strategy_id_(strategy_id), strategy_name_(strategy_name) {
    setWindowTitle(tr("Deploy Strategy"));
    setMinimumWidth(420);
    setStyleSheet(QString("QDialog { background: %1; color: %2; }"
                          "QLabel { color: %3; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
                          "QLineEdit, QComboBox, QDoubleSpinBox { background: %4; color: %2;"
                          " border: 1px solid %5; padding: 4px 8px; font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                          "QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus { border-color: %6; }")
                      .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::TEXT_SECONDARY(), colors::BG_BASE(),
                           colors::BORDER_DIM(), colors::CYAN()));
    build_ui();
}

AlgoDeployDialog::AlgoDeployDialog(const AlgoStrategy& strategy, QWidget* parent)
    : AlgoDeployDialog(strategy.id, strategy.name, parent) {
    result_.strategy_id = strategy.id;
    result_.strategy_name = strategy.name;
    result_.timeframe = strategy.timeframe;
    last_backtest_ = strategy.last_backtest;
    cost_model_ = strategy.cost_model;
    backtested_side_ = strategy.entry_side.isEmpty() ? QStringLiteral("BUY") : strategy.entry_side;
    // Match the dialog's timeframe to the strategy so the deployment evaluates on
    // the same bars the strategy was designed/backtested on (build_ui already ran).
    if (timeframe_combo_ && !strategy.timeframe.isEmpty())
        timeframe_combo_->setCurrentText(strategy.timeframe);
    // Deploy the side the strategy was backtested on (a short-side backtest
    // deploys as SELL entries); changing it is allowed but flagged.
    if (side_combo_) {
        const int idx = side_combo_->findData(backtested_side_);
        if (idx >= 0)
            side_combo_->setCurrentIndex(idx);
    }
    refresh_backtest_note();
}

QString AlgoDeployDialog::backtest_summary_text() const {
    const QJsonObject& b = last_backtest_;
    if (b.isEmpty() || !b.contains(QStringLiteral("total_return")))
        return tr("No backtest has been run for this strategy.");
    namespace fmt = fincept::ui::formatting;
    const double ret = b.value(QStringLiteral("total_return")).toDouble();
    const double mdd = b.value(QStringLiteral("max_drawdown")).toDouble();
    const double sharpe = b.value(QStringLiteral("sharpe_ratio")).toDouble();
    const int trades = b.value(QStringLiteral("total_trades")).toInt();
    const bool net = b.value(QStringLiteral("costs_applied")).toBool();
    const QString period =
        QStringLiteral("%1 → %2")
            .arg(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(b.value(QStringLiteral("start_time")).toDouble()))
                     .date()
                     .toString(Qt::ISODate),
                 QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(b.value(QStringLiteral("end_time")).toDouble()))
                     .date()
                     .toString(Qt::ISODate));
    return tr("Last backtest — %1 %2 %3 (%4): return %5 %6, Sharpe %7, max drawdown %8, %9 trades.")
        .arg(b.value(QStringLiteral("symbol")).toString(), b.value(QStringLiteral("timeframe")).toString(),
             b.value(QStringLiteral("side")).toString().toLower(), period, fmt::pct(ret, 2, true),
             net ? tr("net of costs") : tr("GROSS, no costs"), fmt::number(sharpe, 2), fmt::pct(-std::abs(mdd), 2, true))
        .arg(trades);
}

void AlgoDeployDialog::refresh_backtest_note() {
    if (backtest_note_) {
        backtest_note_->setText(backtest_summary_text());
        const bool none = last_backtest_.isEmpty();
        backtest_note_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) +
                                              "px; background: %2; border: 1px solid %3; padding: 4px 8px;")
                                          .arg(colors::TEXT_SECONDARY(),
                                               none ? colors::tint(colors::WARNING, 0.10) : colors::BG_RAISED(),
                                               none ? colors::WARNING() : colors::BORDER_DIM()));
    }
    if (side_note_ && side_combo_) {
        const bool differs = side_combo_->currentData().toString() != backtested_side_;
        side_note_->setVisible(differs);
        side_note_->setText(tr("Differs from the side the strategy was backtested on (%1).").arg(backtested_side_));
    }
}

QJsonObject AlgoDeployDialog::risk_limits_json() const {
    fincept::algo::DeploymentRiskLimits r;
    r.max_drawdown = max_dd_spin_ ? max_dd_spin_->value() : 0.0;
    r.max_trades_per_day = max_trades_spin_ ? max_trades_spin_->value() : 0;
    if (window_check_ && window_check_->isChecked() && window_start_ && window_end_) {
        r.window_start = window_start_->time().toString(QStringLiteral("HH:mm"));
        r.window_end = window_end_->time().toString(QStringLiteral("HH:mm"));
    }
    r.regular_session_only = session_only_check_ && session_only_check_->isChecked();
    r.flatten_on_stop = flatten_check_ && flatten_check_->isChecked();
    return r.to_json();
}

QString AlgoDeployDialog::deploy_currency() const {
    QString broker_id;
    if (account_combo_) {
        const QString account_id = account_combo_->currentData().toString();
        if (!account_id.isEmpty())
            broker_id = AccountManager::instance().get_account(account_id).broker_id;
    }
    const bool is_fno = fno_instrument_type_ != QLatin1String("equity");
    const QString sym = is_fno ? fno_underlying_ : (symbol_edit_ ? symbol_edit_->text().trimmed().toUpper() : QString());
    QString exchange = exchange_combo_ ? exchange_combo_->currentText() : QString();
    return fincept::ui::algo::algo_currency_for(sym, exchange, broker_id);
}

void AlgoDeployDialog::refresh_money_units() {
    const QString ccy = deploy_currency();
    for (QDoubleSpinBox* s : {max_order_spin_, max_loss_spin_, max_dd_spin_})
        fincept::ui::algo::algo_bind_money_spin(s, ccy);
}

QString AlgoDeployDialog::risk_limits_text() const {
    namespace fmt = fincept::ui::formatting;
    const auto r = fincept::algo::DeploymentRiskLimits::from_json(risk_limits_json());
    QStringList parts;
    if (r.max_drawdown > 0) // P&L below its peak, in the deployment's currency
        parts << tr("max drawdown %1").arg(fmt::money(r.max_drawdown, deploy_currency()));
    if (r.max_trades_per_day > 0)
        parts << tr("max %1 trade(s)/day").arg(r.max_trades_per_day);
    if (r.has_window())
        parts << tr("entries %1-%2 exchange time").arg(r.window_start, r.window_end);
    if (r.regular_session_only)
        parts << tr("regular session only");
    if (r.flatten_on_stop)
        parts << tr("close position on stop");
    return parts.isEmpty() ? tr("none") : parts.join(QStringLiteral(", "));
}

void AlgoDeployDialog::set_symbol(const QString& symbol) {
    if (symbol_edit_)
        symbol_edit_->setText(symbol.trimmed().toUpper());
}

void AlgoDeployDialog::set_fno_context(const QString& instrument_type, const QString& underlying,
                                       const QString& expiry_rule,
                                       const QVector<fincept::algo::fno::AlgoFnoLeg>& legs) {
    fno_instrument_type_ = instrument_type;
    fno_underlying_ = underlying;
    fno_expiry_rule_ = expiry_rule;
    fno_legs_ = legs;

    if (instrument_type == QLatin1String("equity"))
        return;

    // Each leg carries its own side and lots × lot size; the single-symbol Entry
    // Side / Quantity are not used for an F&O deployment, so they are not asked.
    for (QWidget* w : {static_cast<QWidget*>(side_label_), static_cast<QWidget*>(side_combo_),
                       static_cast<QWidget*>(quantity_label_), static_cast<QWidget*>(quantity_spin_)})
        if (w)
            w->setVisible(false);

    // F&O deployment: the symbol field is not a free-text instrument symbol.
    // Show the underlying and expiry rule as read-only context; disable editing.
    if (symbol_edit_) {
        QString label = underlying;
        if (!underlying.isEmpty() && !expiry_rule.isEmpty())
            label = underlying + QStringLiteral(" · ") + expiry_rule;
        else if (underlying.isEmpty())
            label = expiry_rule;
        symbol_edit_->setText(label);
        symbol_edit_->setEnabled(false);
    }
    if (symbol_label_)
        symbol_label_->setText(tr("Underlying:"));

    // Default exchange to NFO for F&O if available.
    if (exchange_combo_) {
        const int nfo_idx = exchange_combo_->findText(QStringLiteral("NFO"));
        if (nfo_idx >= 0)
            exchange_combo_->setCurrentIndex(nfo_idx);
    }

    // Default product type to NRML/Margin if available (case-insensitive search).
    if (product_type_combo_) {
        int nrml_idx =
            product_type_combo_->findText(QStringLiteral("NRML"), Qt::MatchFixedString | Qt::MatchCaseSensitive);
        if (nrml_idx < 0)
            nrml_idx =
                product_type_combo_->findText(QStringLiteral("Margin"), Qt::MatchFixedString | Qt::MatchCaseSensitive);
        if (nrml_idx >= 0)
            product_type_combo_->setCurrentIndex(nrml_idx);
    }
    refresh_money_units(); // the underlying's currency (an NFO underlying: INR)
}

void AlgoDeployDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(16, 16, 16, 16);

    // strategy_name_ is user data; only the "DEPLOY:" prefix is translatable.
    title_label_ = new QLabel(tr("DEPLOY: %1").arg(strategy_name_), this);
    title_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: 700; font-family: %2;")
                                    .arg(colors::AMBER(), fonts::DATA_FAMILY()));
    root->addWidget(title_label_);

    auto* id_label = new QLabel(strategy_id_, this);
    id_label->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-family: %2;").arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY()));
    root->addWidget(id_label);

    // Live banner — hidden in paper mode, impossible to miss in live mode.
    live_warning_ = new QLabel(tr("⚠  LIVE MODE — this strategy will place REAL orders with REAL money."), this);
    live_warning_->setWordWrap(true);
    live_warning_->setStyleSheet(QString("color: %1; background: %3;"
                                         " border: 1px solid %1; padding: 6px 10px;"
                                         " font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; font-family: %2;")
                                     .arg(colors::DANGER(), fonts::DATA_FAMILY(), colors::alpha(colors::DANGER, 0.10)));
    live_warning_->setVisible(false);
    root->addWidget(live_warning_);

    // What the strategy did in its last backtest — deploy decisions should not be
    // made blind (filled by refresh_backtest_note()).
    backtest_note_ = new QLabel(this);
    backtest_note_->setWordWrap(true);
    root->addWidget(backtest_note_);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    symbol_edit_ = new QLineEdit(this);
    symbol_edit_->setPlaceholderText(tr("e.g. RELIANCE"));
    symbol_label_ = new QLabel(tr("Symbol:"), this);
    form->addRow(symbol_label_, symbol_edit_);

    mode_combo_ = new QComboBox(this);
    // Visible labels translatable; userData ("paper"/"live") drives logic.
    mode_combo_->addItem(tr("Paper"), "paper");
    mode_combo_->addItem(tr("Live"), "live");
    mode_combo_->setProperty("fincept_agent_risk", "account"); // paper <-> live switch
    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AlgoDeployDialog::on_mode_changed);
    mode_label_ = new QLabel(tr("Mode:"), this);
    form->addRow(mode_label_, mode_combo_);

    // Entry Side — userData ("BUY"/"SELL") is the order side passed to the engine.
    side_combo_ = new QComboBox(this);
    side_combo_->addItem(tr("BUY"), "BUY");
    side_combo_->addItem(tr("SELL"), "SELL");
    side_label_ = new QLabel(tr("Entry Side:"), this);
    form->addRow(side_label_, side_combo_);
    side_note_ = new QLabel(this);
    side_note_->setWordWrap(true);
    side_note_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px;")
                                  .arg(colors::WARNING()));
    side_note_->setVisible(false);
    form->addRow(QString(), side_note_);
    connect(side_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { refresh_backtest_note(); });

    account_label_ = new QLabel(tr("Account:"), this);
    account_combo_ = new QComboBox(this);
    connect(account_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AlgoDeployDialog::on_account_changed);
    form->addRow(account_label_, account_combo_);

    exchange_label_ = new QLabel(tr("Exchange:"), this);
    exchange_combo_ = new QComboBox(this);
    form->addRow(exchange_label_, exchange_combo_);

    product_type_label_ = new QLabel(tr("Product Type:"), this);
    product_type_combo_ = new QComboBox(this);
    form->addRow(product_type_label_, product_type_combo_);

    timeframe_combo_ = new QComboBox(this);
    for (const auto& tf : algo_timeframes())
        timeframe_combo_->addItem(tf);
    // Default 5m. (A hard-coded index 4 landed on "10m" — not 5m — once "live" was prepended
    // to the list, so a bare deploy silently ran on the wrong bars.)
    timeframe_combo_->setCurrentIndex(qMax(0, timeframe_combo_->findText(QStringLiteral("5m"))));
    timeframe_label_ = new QLabel(tr("Timeframe:"), this);
    form->addRow(timeframe_label_, timeframe_combo_);

    quantity_spin_ = new QDoubleSpinBox(this);
    quantity_spin_->setRange(0.01, 999999);
    quantity_spin_->setValue(1.0);
    quantity_spin_->setDecimals(2);
    quantity_label_ = new QLabel(tr("Quantity:"), this);
    form->addRow(quantity_label_, quantity_spin_);

    max_order_spin_ = new QDoubleSpinBox(this);
    max_order_spin_->setRange(0, 99999999);
    max_order_spin_->setValue(0);
    max_order_spin_->setDecimals(0);
    max_order_spin_->setSpecialValueText(tr("No limit"));
    max_order_label_ = new QLabel(tr("Max Order Value:"), this);
    form->addRow(max_order_label_, max_order_spin_);

    max_loss_spin_ = new QDoubleSpinBox(this);
    max_loss_spin_->setRange(0, 99999999);
    max_loss_spin_->setValue(0);
    max_loss_spin_->setDecimals(0);
    max_loss_spin_->setSpecialValueText(tr("No limit"));
    max_loss_label_ = new QLabel(tr("Max Daily Loss:"), this);
    max_loss_spin_->setToolTip(tr("Realized loss today PLUS the open position's unrealized loss. Counted on the "
                                  "exchange's trading day; survives a restart."));
    form->addRow(max_loss_label_, max_loss_spin_);

    // ── Extra risk limits — each can only make the deployment more conservative ──
    max_dd_spin_ = new QDoubleSpinBox(this);
    max_dd_spin_->setRange(0, 99999999);
    max_dd_spin_->setDecimals(0);
    max_dd_spin_->setSpecialValueText(tr("No limit"));
    max_dd_spin_->setToolTip(tr("Close the position and halt new entries once realized + unrealized P&L falls this far "
                                "below its peak. Stays halted until you resume the deployment."));
    max_dd_label_ = new QLabel(tr("Max Drawdown:"), this);
    form->addRow(max_dd_label_, max_dd_spin_);

    max_trades_spin_ = new QSpinBox(this);
    max_trades_spin_->setRange(0, 1000);
    max_trades_spin_->setSpecialValueText(tr("Unlimited"));
    max_trades_spin_->setToolTip(tr("Maximum entries per exchange trading day"));
    max_trades_label_ = new QLabel(tr("Max Trades / Day:"), this);
    form->addRow(max_trades_label_, max_trades_spin_);

    {
        auto* w = new QWidget(this);
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        window_check_ = new QCheckBox(tr("Only"), w);
        window_start_ = new QTimeEdit(QTime(9, 30), w);
        window_end_ = new QTimeEdit(QTime(15, 0), w);
        window_start_->setDisplayFormat(QStringLiteral("HH:mm"));
        window_end_->setDisplayFormat(QStringLiteral("HH:mm"));
        window_start_->setEnabled(false);
        window_end_->setEnabled(false);
        h->addWidget(window_check_);
        h->addWidget(window_start_);
        h->addWidget(new QLabel(QStringLiteral("–"), w));
        h->addWidget(window_end_);
        h->addStretch();
        connect(window_check_, &QCheckBox::toggled, this, [this](bool on) {
            window_start_->setEnabled(on);
            window_end_->setEnabled(on);
        });
        w->setToolTip(tr("New entries only inside this window, in the exchange's local time. Exits are never "
                         "blocked."));
        window_label_ = new QLabel(tr("Entry Window:"), this);
        form->addRow(window_label_, w);
    }
    session_only_check_ = new QCheckBox(tr("No new entries outside the regular session"), this);
    form->addRow(QString(), session_only_check_);
    flatten_check_ = new QCheckBox(tr("STOP closes the open position first"), this);
    flatten_check_->setToolTip(tr("The close goes through the normal order path: the paper simulator, or the broker "
                                  "with the Semi-Auto approval queue and fill confirmation."));
    form->addRow(QString(), flatten_check_);

    root->addLayout(form);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons_->button(QDialogButtonBox::Ok)->setText(tr("DEPLOY"));
    buttons_->button(QDialogButtonBox::Ok)->setProperty("fincept_agent_risk", "order");
    buttons_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                " font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; padding: 6px 20px; }"
                "QPushButton:hover { border-color: %4; color: %4; }")
            .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::CYAN()));
    connect(buttons_, &QDialogButtonBox::accepted, this, &AlgoDeployDialog::on_ok);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons_);

    // ── Accessibility ────────────────────────────────────────────────────────
    // Screen readers otherwise announce these as bare "combo box" / "spin box".
    symbol_edit_->setAccessibleName(tr("Trading symbol"));
    mode_combo_->setAccessibleName(tr("Deployment mode, paper or live"));
    side_combo_->setAccessibleName(tr("Entry side, buy or sell"));
    account_combo_->setAccessibleName(tr("Broker account"));
    exchange_combo_->setAccessibleName(tr("Exchange"));
    product_type_combo_->setAccessibleName(tr("Product type"));
    timeframe_combo_->setAccessibleName(tr("Bar timeframe"));
    quantity_spin_->setAccessibleName(tr("Order quantity"));
    max_order_spin_->setAccessibleName(tr("Maximum order value"));
    max_loss_spin_->setAccessibleName(tr("Maximum daily loss"));
    if (auto* ok_btn = buttons_->button(QDialogButtonBox::Ok))
        ok_btn->setAccessibleName(tr("Deploy strategy"));

    // ── Keyboard order ───────────────────────────────────────────────────────
    // Mode comes second on purpose: paper-vs-live is the decision that matters.
    setTabOrder(symbol_edit_, mode_combo_);
    setTabOrder(mode_combo_, side_combo_);
    setTabOrder(side_combo_, account_combo_);
    setTabOrder(account_combo_, exchange_combo_);
    setTabOrder(exchange_combo_, product_type_combo_);
    setTabOrder(product_type_combo_, timeframe_combo_);
    setTabOrder(timeframe_combo_, quantity_spin_);
    setTabOrder(quantity_spin_, max_order_spin_);
    setTabOrder(max_order_spin_, max_loss_spin_);
    setTabOrder(max_loss_spin_, max_dd_spin_);
    setTabOrder(max_dd_spin_, max_trades_spin_);
    setTabOrder(max_trades_spin_, window_check_);
    setTabOrder(window_check_, session_only_check_);
    setTabOrder(session_only_check_, flatten_check_);
    setTabOrder(flatten_check_, buttons_);

    // The money limits are in the deployment's quote currency, which follows the symbol,
    // the exchange and the account's broker: keep their unit in step.
    connect(symbol_edit_, &QLineEdit::textChanged, this, [this](const QString&) { refresh_money_units(); });
    connect(exchange_combo_, &QComboBox::currentTextChanged, this, [this](const QString&) { refresh_money_units(); });

    on_mode_changed(0);
    populate_accounts();
    refresh_money_units();
    refresh_backtest_note();
}

void AlgoDeployDialog::set_risk_defaults(double quantity, double max_order_value) {
    if (quantity_spin_ && quantity > 0.0)
        quantity_spin_->setValue(quantity);
    if (max_order_spin_ && max_order_value >= 0.0)
        max_order_spin_->setValue(max_order_value);
}

void AlgoDeployDialog::on_mode_changed(int index) {
    // The broker/account is shown in BOTH modes: it is the real market-data source
    // for the deployment (warm-up history + live quotes). Live additionally routes
    // real orders through it; paper simulates fills against the same data. Hiding it
    // for paper is what made users ask "which broker is it even using?".
    Q_UNUSED(index);
    account_label_->setVisible(true);
    account_combo_->setVisible(true);
    exchange_label_->setVisible(true);
    exchange_combo_->setVisible(true);
    product_type_label_->setVisible(true);
    product_type_combo_->setVisible(true);

    const bool is_live = (mode_combo_->currentData().toString() == "live");
    account_label_->setText(is_live ? tr("Broker (orders + data):") : tr("Broker (data source):"));

    // Live vs paper must be legible at a glance, not inferred from a dropdown:
    // red banner + red destructive-styled action button + explicit button text.
    if (live_warning_)
        live_warning_->setVisible(is_live);
    if (buttons_) {
        if (auto* ok_btn = buttons_->button(QDialogButtonBox::Ok)) {
            ok_btn->setText(is_live ? tr("DEPLOY LIVE") : tr("DEPLOY (PAPER)"));
            ok_btn->setStyleSheet(
                is_live ? QString("QPushButton { background: %3; color: %1;"
                                  " border: 1px solid %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; padding: 6px 20px; }"
                                  "QPushButton:hover { background: %1; color: %2; }")
                              .arg(colors::DANGER(), colors::TEXT_ON_ACCENT(), colors::alpha(colors::DANGER, 0.12))
                        : QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                                  " font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; padding: 6px 20px; }"
                                  "QPushButton:hover { border-color: %4; color: %4; }")
                              .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::CYAN()));
        }
    }
    setWindowTitle(is_live ? tr("Deploy Strategy — LIVE") : tr("Deploy Strategy — Paper"));
}

void AlgoDeployDialog::populate_accounts() {
    account_combo_->clear();
    auto accounts = AccountManager::instance().active_accounts();
    for (const auto& acct : accounts) {
        if (AccountManager::instance().connection_state(acct.account_id) == ConnectionState::Connected) {
            account_combo_->addItem(acct.display_name, acct.account_id);
        }
    }
    if (account_combo_->count() == 0) {
        account_combo_->addItem(tr("No connected accounts"), "");
    }
}

void AlgoDeployDialog::on_account_changed(int index) {
    if (index < 0)
        return;
    populate_broker_fields(account_combo_->itemData(index).toString());
}

void AlgoDeployDialog::populate_broker_fields(const QString& account_id) {
    exchange_combo_->clear();
    product_type_combo_->clear();

    if (account_id.isEmpty())
        return;

    auto* broker = AccountManager::instance().broker_for(account_id);
    if (!broker)
        return;

    auto profile = broker->profile();
    for (const auto& ex : profile.exchanges)
        exchange_combo_->addItem(ex);
    for (const auto& pt : profile.product_types) {
        // Cover / bracket orders need stop + target legs the algo engine does not
        // send (it would have placed them as plain intraday orders) — not offered.
        if (pt.value == ProductType::CoverOrder || pt.value == ProductType::BracketOrder)
            continue;
        product_type_combo_->addItem(pt.label, static_cast<int>(pt.value));
    }
}

void AlgoDeployDialog::on_ok() {
    // For F&O deployments the symbol field holds a read-only context label, not a
    // tradeable symbol — skip the "symbol required" guard.
    const bool is_fno = (fno_instrument_type_ != QLatin1String("equity"));
    if (!is_fno && symbol_edit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"), tr("Symbol is required."));
        return;
    }
    if (is_fno && fno_underlying_.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Deploy"), tr("Select an underlying for the F&O deployment."));
        return;
    }
    if (quantity_spin_->value() <= 0) {
        QMessageBox::warning(this, tr("Validation"), tr("Quantity must be > 0."));
        return;
    }

    QString mode = mode_combo_->currentData().toString();

    // BOTH modes take their market data from a connected broker account (DeploymentRunner
    // subscribes the quote feed through it). With none selected the deployment starts, gets no
    // quotes and is marked "error" by the 30s watchdog — so refuse here, with the reason.
    if (account_combo_->currentData().toString().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"),
                             tr("No connected broker account is selected. Deployments — paper ones too — read their "
                                "live prices from a connected broker.\n\nConnect a broker in Equity Trading, then "
                                "deploy again."));
        return;
    }

    if (mode == "live") {
        QString account_id = account_combo_->currentData().toString();
        if (account_id.isEmpty()) {
            QMessageBox::warning(this, tr("Validation"), tr("Select a connected broker account for live mode."));
            return;
        }
        auto creds = AccountManager::instance().load_credentials(account_id);
        if (creds.access_token.isEmpty()) {
            QMessageBox::warning(this, tr("Validation"),
                                 tr("Account credentials expired. Re-authenticate in Equity Trading."));
            return;
        }

        // ── Live confirmation gate ───────────────────────────────────────────
        // Going live is irreversible from this dialog's point of view: the engine
        // starts a runner that will place real broker orders unattended. Restate
        // the exact terms and require a deliberate second confirmation, defaulted
        // to Cancel.
        const auto account = AccountManager::instance().get_account(account_id);
        const QString acct_label = account.display_name.isEmpty() ? account_id : account.display_name;
        const bool is_fno_confirm = (fno_instrument_type_ != QLatin1String("equity"));
        const QString instrument =
            is_fno_confirm ? tr("%1 %2 (%3)").arg(fno_underlying_, fno_expiry_rule_, fno_instrument_type_)
                           : symbol_edit_->text().trimmed().toUpper();
        // Limits in the deployment's quote currency (no unit when it is not known).
        namespace fmt = fincept::ui::formatting;
        const QString limit_ccy = deploy_currency();
        const QString max_order_text =
            max_order_spin_->value() > 0 ? fmt::money(max_order_spin_->value(), limit_ccy) : tr("no limit");
        const QString max_loss_text =
            max_loss_spin_->value() > 0 ? fmt::money(max_loss_spin_->value(), limit_ccy) : tr("no limit");

        // What is actually traded: the single symbol's side × quantity, or — for
        // F&O — every leg's side, type, strike rule and lots (the dialog's Entry
        // Side / Quantity are not used for a basket and are not shown).
        QString trade_lines;
        if (is_fno_confirm) {
            trade_lines = tr("  Legs (each lots × the contract's lot size):\n");
            for (int i = 0; i < fno_legs_.size(); ++i) {
                const auto& l = fno_legs_[i];
                QString strike = l.strike_mode;
                if (l.strike_mode != QLatin1String("ATM"))
                    strike += QStringLiteral(" ") + QString::number(l.strike_value);
                trade_lines += tr("    %1. %2 %3 %4 × %5 lot(s)\n")
                                   .arg(i + 1)
                                   .arg(l.side, l.kind, strike)
                                   .arg(l.lots);
            }
            if (fno_legs_.isEmpty())
                trade_lines += tr("    (no legs)\n");
        } else {
            trade_lines = tr("  Entry side:      %1\n"
                             "  Quantity:        %2\n")
                              .arg(side_combo_->currentData().toString(), fmt::quantity(quantity_spin_->value(), 2));
        }

        const QString terms = tr("You are about to deploy \"%1\" to LIVE trading.\n\n"
                                 "It will place REAL orders on a REAL account, unattended,\n"
                                 "until you stop it from the Dashboard.\n\n"
                                 "  Instrument:      %2\n"
                                 "%3"
                                 "  Timeframe:       %4\n"
                                 "  Broker account:  %5 (%6)\n"
                                 "  Exchange:        %7\n"
                                 "  Max order value: %8\n"
                                 "  Max daily loss:  %9\n")
                                  .arg(strategy_name_, instrument, trade_lines, timeframe_combo_->currentText(),
                                       acct_label, account.broker_id, exchange_combo_->currentText(), max_order_text,
                                       max_loss_text);
        // The extra risk limits and what the strategy did in its last backtest,
        // so the confirmation states every term the deployment runs under.
        const QString extra = tr("  Risk limits:     %1\n\n%2\n\nDeploy live?")
                                  .arg(risk_limits_text(), backtest_summary_text());
        const auto answer = QMessageBox::warning(this, tr("Confirm LIVE deployment"), terms + extra,
                                                 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
    }

    // Mint a stable id for this deployment — used as the runner key, the
    // algo_deployments primary key, and the WHERE of every status UPDATE.
    if (result_.id.isEmpty())
        result_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    result_.strategy_id = strategy_id_;
    result_.strategy_name = strategy_name_;
    result_.strategy_kind = kind_to_string(kind_from_id(strategy_id_));
    result_.symbol = symbol_edit_->text().trimmed().toUpper();
    result_.mode = mode;
    result_.entry_side = side_combo_->currentData().toString();
    result_.timeframe = timeframe_combo_->currentText();
    result_.quantity = quantity_spin_->value();
    result_.max_order_value = max_order_spin_->value();
    result_.max_daily_loss = max_loss_spin_->value();
    result_.risk_limits = risk_limits_json();
    // The strategy's cost model rides with the deployment: paper fills charge it,
    // live P&L deducts it as an estimate.
    result_.cost_model = cost_model_;

    // Attach the chosen connected account in BOTH modes — it's the market-data
    // source. Live additionally routes real orders through it (backend=equity_broker);
    // paper simulates fills (backend=paper) but still sources real data from it.
    const QString account_id = account_combo_->currentData().toString();
    if (!account_id.isEmpty()) {
        auto account = AccountManager::instance().get_account(account_id);
        result_.broker_id = account.broker_id;
        result_.broker_account_id = account_id;
        result_.exchange = exchange_combo_->currentText();
        int pt_index = product_type_combo_->currentData().toInt();
        result_.product_type = product_type_str(static_cast<trading::ProductType>(pt_index));
    }
    result_.backend = backend_to_string(mode == "live" ? TradingBackend::EquityBroker : TradingBackend::Paper);

    // F&O override: set instrument_type/underlying/resolved_expiry and clear symbol.
    // Concrete legs and expiry are resolved at entry-time by the runner (P3).
    // The equity branch (is_fno == false) is unaffected.
    if (is_fno) {
        result_.instrument_type = fno_instrument_type_;
        result_.underlying = fno_underlying_;
        result_.resolved_expiry = fno_expiry_rule_; // expiry RULE; concrete expiry resolved at entry (P3)
        // Multi-leg F&O has no single symbol; concrete legs are resolved at entry by the runner (P3).
        result_.symbol.clear();
    }

    accept();
}

void AlgoDeployDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void AlgoDeployDialog::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("DEPLOY: %1").arg(strategy_name_));
    if (live_warning_)
        live_warning_->setText(tr("⚠  LIVE MODE — this strategy will place REAL orders with REAL money."));
    if (symbol_edit_)
        symbol_edit_->setPlaceholderText(tr("e.g. RELIANCE"));
    if (symbol_label_)
        symbol_label_->setText(tr("Symbol:"));
    if (mode_label_)
        mode_label_->setText(tr("Mode:"));
    if (side_label_)
        side_label_->setText(tr("Entry Side:"));
    if (account_label_)
        account_label_->setText(tr("Account:"));
    if (exchange_label_)
        exchange_label_->setText(tr("Exchange:"));
    if (product_type_label_)
        product_type_label_->setText(tr("Product Type:"));
    if (timeframe_label_)
        timeframe_label_->setText(tr("Timeframe:"));
    if (quantity_label_)
        quantity_label_->setText(tr("Quantity:"));
    if (max_order_label_)
        max_order_label_->setText(tr("Max Order Value:"));
    if (max_loss_label_)
        max_loss_label_->setText(tr("Max Daily Loss:"));
    if (max_dd_label_)
        max_dd_label_->setText(tr("Max Drawdown:"));
    if (max_trades_label_)
        max_trades_label_->setText(tr("Max Trades / Day:"));
    if (window_label_)
        window_label_->setText(tr("Entry Window:"));
    if (window_check_)
        window_check_->setText(tr("Only"));
    if (session_only_check_)
        session_only_check_->setText(tr("No new entries outside the regular session"));
    if (flatten_check_)
        flatten_check_->setText(tr("STOP closes the open position first"));
    if (max_dd_spin_)
        max_dd_spin_->setSpecialValueText(tr("No limit"));
    if (max_trades_spin_)
        max_trades_spin_->setSpecialValueText(tr("Unlimited"));
    refresh_backtest_note();

    // Combo visible labels (userData keys unchanged).
    if (mode_combo_ && mode_combo_->count() >= 2) {
        mode_combo_->setItemText(0, tr("Paper"));
        mode_combo_->setItemText(1, tr("Live"));
    }
    if (side_combo_ && side_combo_->count() >= 2) {
        side_combo_->setItemText(0, tr("BUY"));
        side_combo_->setItemText(1, tr("SELL"));
    }
    if (max_order_spin_)
        max_order_spin_->setSpecialValueText(tr("No limit"));
    if (max_loss_spin_)
        max_loss_spin_->setSpecialValueText(tr("No limit"));
    // Window title and the OK button's text/skin are mode-dependent (paper vs
    // live) — re-derive them rather than resetting to a neutral "DEPLOY", which
    // would erase the live-mode warning styling on a language switch.
    if (mode_combo_)
        on_mode_changed(mode_combo_->currentIndex());
    // account_combo_ "No connected accounts" placeholder is rebuilt by populate_accounts().
}

AlgoDeployment AlgoDeployDialog::result() const {
    return result_;
}

} // namespace fincept::screens
