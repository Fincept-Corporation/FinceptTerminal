// OrderRiskGate.cpp — the shell half of OrderValidator (see OrderValidator.h):
// risk-settings persistence (SettingsRepository), logging, the thread-safe
// per-account snapshot fed by DataStreamManager, and pre_trade_risk(), which ties
// them to the pure rules in OrderValidator.cpp. Nothing here decides whether an
// order passes — every verdict comes from OrderValidator::check_risk().
#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "core/market/MarketSessions.h"
#include "services/markets/FxRateService.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/OrderValidator.h"
#include "trading/PaperTrading.h"
#include "ui/formatting/NumberFormat.h"

#include <QMutex>
#include <QMutexLocker>

#include <cmath>

namespace fincept::trading {

namespace {

// File-local names carry an orgate_ prefix: the release build is a unity build.
constexpr const char* orgate_kLog = "OrderValidator";

// Currency the order's price is quoted in. The order exchange's venue currency
// (NSE / NFO INR, LSE GBp pence, NYSE USD, XETRA EUR); a crypto pair's quote
// asset when the pair is written with a separator ("BTC-USDT" / "ETH/BTC" ->
// USDT / BTC; a stablecoin is its own currency). Empty when it cannot be told
// (MCX, CDS, FOREX, "BTCUSDT"): the validator then takes the account's currency
// (the documented fallback).
QString orgate_price_currency(const UnifiedOrder& order) {
    namespace mk = fincept::core::market;
    const QString exch = order.exchange.trimmed().toUpper();
    if (exch == QLatin1String("CRYPTO"))
        return fincept::ui::formatting::instrument_for_symbol(order.symbol.trimmed()).currency;
    if (const mk::Venue* v = mk::venue_by_code(exch))
        return v->currency;
    return {};
}

// Currency of the account's cash, buying power and P&L: the paper portfolio's
// for the local simulator, else the broker's. Empty when unknown (no account id
// on the legacy session path, an account or portfolio that cannot be read).
QString orgate_account_currency(const QString& account_id, bool broker_routed) {
    if (account_id.isEmpty())
        return {};
    auto& mgr = AccountManager::instance();
    const BrokerAccount acct = mgr.get_account(account_id);
    if (acct.account_id.isEmpty())
        return {};
    if (!broker_routed && !acct.paper_portfolio_id.isEmpty()) {
        try {
            return pt_get_portfolio(acct.paper_portfolio_id).currency.trimmed();
        } catch (...) {
            return {};
        }
    }
    if (IBroker* broker = mgr.broker_for(account_id))
        return broker->profile().currency.trimmed();
    return {};
}

// The validator's FX lookup: FxRateService (thread-safe, never blocks; a
// missing leg is requested in the background and the lookup says "no rate").
std::optional<OrderValidator::FxRate> orgate_fx(const QString& from, const QString& to) {
    namespace uifmt = fincept::ui::formatting;
    const uifmt::ConvertedMoney c = services::FxRateService::instance().convert_for_display(1.0, from, to);
    if (!c.ok() || !(c.amount > 0.0))
        return std::nullopt;
    OrderValidator::FxRate r;
    r.rate = c.amount; // `to` per one quoted unit of `from` (one penny for GBp)
    r.stale = c.stale;
    r.approximate = c.approximate;
    r.note = c.converted ? uifmt::fx_note(c) : QString();
    return r;
}

QString orgate_money_text(double amount, const QString& ccy) {
    return fincept::ui::formatting::money(amount, ccy);
}

// Thread-safe snapshot of what the risk gate needs per account. The source caches
// (AccountDataStream) are GUI-thread-only, while orders are placed from executor
// threads, so DataStreamManager mirrors funds / positions / last prices in here.
QMutex& orgate_snapshot_mutex() {
    static QMutex m;
    return m;
}

QHash<QString, OrderValidator::RiskSnapshot>& orgate_snapshots() {
    static QHash<QString, OrderValidator::RiskSnapshot> s;
    return s;
}

} // namespace

OrderValidator::RiskSettings OrderValidator::load_risk_settings(bool* ok_out) {
    auto res = SettingsRepository::instance().get_by_category(QStringLiteral("risk"));
    if (res.is_err()) {
        LOG_ERROR(orgate_kLog, QString("Risk settings unreadable: %1").arg(QString::fromStdString(res.error())));
        if (ok_out)
            *ok_out = false;
        return RiskSettings{};
    }
    QVector<QPair<QString, QString>> rows;
    rows.reserve(res.value().size());
    for (const auto& kv : res.value())
        rows.append({kv.key, kv.value});
    if (ok_out)
        *ok_out = true;
    return parse_risk_settings(rows);
}

bool OrderValidator::save_risk_settings(const RiskSettings& s) {
    auto& repo = SettingsRepository::instance();
    const QString cat = QStringLiteral("risk");
    bool ok = true;
    for (const auto& row : serialize_risk_settings(s))
        ok = repo.set(row.first, row.second, cat).is_ok() && ok; // every row is written, even after a failure
    LOG_INFO(orgate_kLog, QString("Risk limits saved: max value %1, max qty %2, daily loss %3, %4 blocked, sanity %5")
                              .arg(s.max_order_value)
                              .arg(s.max_order_quantity)
                              .arg(s.daily_loss_limit)
                              .arg(s.blocked_symbols.size())
                              .arg(s.sanity_checks ? "on" : "off"));
    return ok;
}

OrderValidator::ValidationResult OrderValidator::pre_trade_risk(const QString& account_id, const UnifiedOrder& order,
                                                                bool broker_routed) {
    bool settings_ok = true;
    const RiskSettings s = load_risk_settings(&settings_ok);
    if (!settings_ok) {
        ValidationResult r;
        r.valid = false;
        r.errors << QStringLiteral("Risk settings could not be read — order refused (fail closed)");
        LOG_AUDIT("Orders", "Risk limit rejected order",
                  {{"account", account_id.isEmpty() ? QString() : log_mask(account_id)},
                   {"symbol", order.symbol},
                   {"side", order_side_str(order.side)},
                   {"qty", order.quantity},
                   {"reason", "risk settings could not be read (fail closed)"}});
        return r;
    }

    RiskContext ctx;
    {
        QMutexLocker lock(&orgate_snapshot_mutex());
        const auto it = orgate_snapshots().constFind(account_id);
        ctx = build_risk_context(order, broker_routed, it != orgate_snapshots().constEnd() ? &it.value() : nullptr);
    }
    // Currencies. Without an account (legacy session path) the account currency is
    // unknown: buying power and P&L are not known there either, and an order value
    // is converted only into an explicitly chosen limits currency.
    ctx.account_currency = orgate_account_currency(account_id, broker_routed);
    ctx.price_currency = orgate_price_currency(order);
    ctx.fx = &orgate_fx;
    ctx.money_text = &orgate_money_text;

    ValidationResult r = check_risk(order, s, ctx);
    // A refusal is an order event: audited here, once, for every caller (order ticket,
    // basket / split / smart orders, approvals, GTTs, MCP tools, workflows, algos).
    if (!r.valid)
        LOG_AUDIT("Orders", "Risk limit rejected order",
                  {{"account", account_id.isEmpty() ? QString() : log_mask(account_id)},
                   {"mode", broker_routed ? "broker" : "paper"},
                   {"symbol", order.symbol},
                   {"exchange", order.exchange},
                   {"side", order_side_str(order.side)},
                   {"qty", order.quantity},
                   {"price", order.price},
                   {"reason", r.errors.join("; ")}});
    return r;
}

void OrderValidator::note_funds(const QString& account_id, double available_balance) {
    note_funds(account_id, available_balance, QString());
}

void OrderValidator::note_funds(const QString& account_id, double available_balance, const QString& currency) {
    if (account_id.isEmpty() || !std::isfinite(available_balance))
        return;
    QMutexLocker lock(&orgate_snapshot_mutex());
    auto& snap = orgate_snapshots()[account_id];
    snap.available = available_balance;
    snap.funds_currency = currency.trimmed();
    snap.funds_known = true;
}

void OrderValidator::note_positions(const QString& account_id, const QVector<BrokerPosition>& positions) {
    note_positions(account_id, positions, QStringList());
}

void OrderValidator::note_positions(const QString& account_id, const QVector<BrokerPosition>& positions,
                                    const QStringList& pnl_currencies) {
    if (account_id.isEmpty())
        return;
    QMutexLocker lock(&orgate_snapshot_mutex());
    auto& snap = orgate_snapshots()[account_id];
    snap.positions = positions;
    snap.position_currencies = pnl_currencies;
    snap.positions_known = true;
}

void OrderValidator::note_quote(const QString& account_id, const QString& symbol, double ltp) {
    if (account_id.isEmpty() || symbol.isEmpty() || !(ltp > 0.0) || !std::isfinite(ltp))
        return;
    QMutexLocker lock(&orgate_snapshot_mutex());
    orgate_snapshots()[account_id].ltp.insert(symbol.trimmed().toUpper(), ltp);
}

} // namespace fincept::trading
