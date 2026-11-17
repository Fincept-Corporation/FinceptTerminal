// Unit tests for src/trading/OrderValidator.{h,cpp}
//
// OrderValidator.cpp is the PURE half of the class: it includes only its own
// header, which includes only trading/TradingTypes.h (Qt Core types + plain
// aggregate structs, no .cpp of its own). Settings persistence, logging, the
// account snapshot and pre_trade_risk() live in OrderRiskGate.cpp, which this
// suite deliberately does not compile. That is why it costs exactly one extra
// translation unit and links nothing from storage, the broker registry, the HTTP
// stack or any service.
//
// The headline cases:
//   * validate_smart()'s FLATTEN order: position_size == 0 is a legitimate target
//     ("close me out") and quantity == 0 alongside it must validate.
//   * the pre-trade risk rules (check_risk): every configured limit at, just
//     below and just above its boundary; 0 = off; fail-closed when a configured
//     limit cannot be evaluated; position-reducing orders exempt.
//   * NaN: it compares false against every limit, so it must be refused, never
//     waved through (validate_smart and check_risk used to let it pass).
//   * currencies: the order value (instrument currency, minor units such as GBp
//     pence) is converted into the account's / the limits' currency with
//     injected rates (no network, no FX service); a missing rate refuses with the
//     reason, never 1:1; stale and approximate (stablecoin) rates are used and
//     named. Rates are the live values recorded in the currency audit
//     (GBPUSD 1.3183, USDJPY 159.452, USDINR 93.912, USDT-USD 0.99903).

#include "trading/OrderValidator.h"
#include "trading/TradingTypes.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QTest>

#include <limits>
#include <optional>

using fincept::trading::BasketOrderRequest;
using fincept::trading::BrokerPosition;
using fincept::trading::OrderSide;
using fincept::trading::OrderType;
using fincept::trading::OrderValidator;
using fincept::trading::ProductType;
using fincept::trading::SmartOrder;
using fincept::trading::UnifiedOrder;

using Validation = OrderValidator::ValidationResult;
using RiskSettings = OrderValidator::RiskSettings;
using RiskContext = OrderValidator::RiskContext;
using RiskSnapshot = OrderValidator::RiskSnapshot;
using FxRate = OrderValidator::FxRate;

static constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
static constexpr double kInf = std::numeric_limits<double>::infinity();

static bool has_error(const Validation& r, const QString& needle) {
    for (const QString& e : r.errors) {
        if (e.contains(needle))
            return true;
    }
    return false;
}

static QString joined(const Validation& r) {
    return r.errors.join(QStringLiteral(" | "));
}

// A minimal order that must validate, so each test can perturb exactly one field.
static UnifiedOrder good_order() {
    UnifiedOrder o;
    o.symbol = QStringLiteral("RELIANCE");
    o.exchange = QStringLiteral("NSE");
    o.side = OrderSide::Buy;
    o.order_type = OrderType::Market;
    o.quantity = 10;
    o.product_type = ProductType::Intraday;
    return o;
}

static SmartOrder good_smart() {
    SmartOrder o;
    o.symbol = QStringLiteral("RELIANCE");
    o.exchange = QStringLiteral("NSE");
    o.action = OrderSide::Buy;
    o.order_type = OrderType::Market;
    o.quantity = 10;
    o.position_size = 100;
    return o;
}

// Settings with every configured limit off and the sanity guard on — the shipped
// default ("unconfigured").
static RiskSettings no_limits() {
    return RiskSettings{};
}

// A broker-routed order that adds risk, with a known price and plenty of funds.
static RiskContext adding_ctx(double ref_price = 100.0, double buying_power = 1'000'000.0) {
    RiskContext c;
    c.broker_routed = true;
    c.ref_price = ref_price;
    c.buying_power = buying_power;
    c.day_pnl_known = true;
    c.day_pnl = 0.0;
    c.reduces_position = false;
    return c;
}

// Injected FX rates: "FROM>TO" -> rate (units of TO per one quoted unit of FROM,
// minor units applied, exactly what the shell's lookup returns). Counts lookups.
struct FakeFx {
    QHash<QString, FxRate> rates;
    int calls = 0;

    void set(const QString& from, const QString& to, double rate, bool stale = false, bool approximate = false,
             const QString& note = QString()) {
        FxRate r;
        r.rate = rate;
        r.stale = stale;
        r.approximate = approximate;
        r.note = note;
        rates.insert(from + QLatin1Char('>') + to, r);
    }
    OrderValidator::FxLookup lookup() {
        return [this](const QString& from, const QString& to) -> std::optional<FxRate> {
            ++calls;
            const auto it = rates.constFind(from + QLatin1Char('>') + to);
            if (it == rates.constEnd())
                return std::nullopt;
            return *it;
        };
    }
};

// adding_ctx() with currencies.
static RiskContext ccy_ctx(double ref_price, double buying_power, const QString& price_ccy, const QString& account_ccy,
                           const OrderValidator::FxLookup& fx = {}) {
    RiskContext c = adding_ctx(ref_price, buying_power);
    c.price_currency = price_ccy;
    c.account_currency = account_ccy;
    c.fx = fx;
    return c;
}

static BrokerPosition position(const QString& symbol, double qty, const QString& side = QString(),
                               const QString& product = QStringLiteral("MIS"),
                               const QString& exchange = QStringLiteral("NSE"), double pnl = 0.0, double ltp = 0.0) {
    BrokerPosition p;
    p.symbol = symbol;
    p.exchange = exchange;
    p.product_type = product;
    p.quantity = qty;
    p.side = side;
    p.pnl = pnl;
    p.ltp = ltp;
    return p;
}

class TstOrderValidator : public QObject {
    Q_OBJECT

  private slots:
    // validate()
    void accepts_a_well_formed_market_order();
    void rejects_missing_or_blank_symbol();
    void rejects_missing_exchange();
    void rejects_unknown_exchange();
    void exchange_whitelist_spot_checks();
    void exchange_match_is_case_insensitive_but_not_whitespace_tolerant();
    void rejects_non_positive_quantity();
    void price_required_only_for_limit_family();
    void trigger_required_only_for_stop_family();
    void stop_loss_limit_requires_both_price_and_trigger();
    void errors_accumulate();
    void rejects_non_finite_numbers();

    // validate_smart()
    void smart_flatten_with_zero_quantity_is_valid();
    void smart_allows_zero_quantity_for_any_position_size();
    void smart_rejects_negative_quantity();
    void smart_validates_symbol_and_exchange();
    void smart_price_and_trigger_rules_match_validate();
    void smart_rejects_non_finite_numbers();
    void smart_rejects_negative_price_or_trigger();

    // validate_basket()
    void basket_rejects_empty();
    void basket_accepts_all_valid_legs();
    void basket_reports_the_failing_leg_by_index_and_symbol();

    // check_risk() — the pre-trade risk rules
    void risk_unconfigured_settings_pass_a_normal_order();
    void risk_zero_limits_are_off_even_for_huge_orders();
    void risk_max_order_value_boundary();
    void risk_max_order_value_fails_closed_without_a_price();
    void risk_max_order_quantity_boundary();
    void risk_daily_loss_limit_boundary();
    void risk_daily_loss_limit_fails_closed_when_pnl_unknown_or_nan();
    void risk_blocked_symbols();
    void risk_sanity_quantity_ceiling_boundary();
    void risk_sanity_buying_power_boundary();
    void risk_sanity_buying_power_exemptions();
    void risk_sanity_off_disables_only_the_sanity_guard();
    void risk_reducing_orders_skip_configured_limits();
    void risk_local_paper_skips_configured_limits_but_not_sanity();
    void risk_non_finite_quantity_is_refused();
    void risk_errors_accumulate();

    // check_risk() — currencies (injected rates)
    void risk_same_currency_needs_no_rate();
    void risk_cross_currency_buying_power_uses_the_rate();
    void risk_cross_currency_without_a_rate_is_refused_with_the_reason();
    void risk_stale_and_approximate_rates_are_used_and_named();
    void risk_minor_units_against_a_major_account();
    void risk_limits_currency_converts_value_and_day_pnl();
    void risk_unknown_currencies_keep_the_legacy_comparison();
    void risk_money_text_hook_formats_amounts();
    void risk_funds_currency_is_the_buying_power_currency();
    void risk_day_pnl_in_several_currencies_is_converted_part_by_part();

    // build_risk_context() — what the gate knows at order time
    void context_without_snapshot();
    void context_reference_price_precedence();
    void context_funds_and_day_pnl();
    void context_funds_and_position_currencies();
    void context_reduces_position_rules();

    // parse / serialize_risk_settings() — the settings-table shape
    void settings_parse_defaults_when_empty();
    void settings_parse_values_and_sanitising();
    void settings_round_trip();
    void settings_limits_currency();
};

// ── validate() ───────────────────────────────────────────────────────────────

void TstOrderValidator::accepts_a_well_formed_market_order() {
    const auto r = OrderValidator::validate(good_order());
    QVERIFY2(r.valid, qPrintable(joined(r)));
    QVERIFY(r.errors.isEmpty());
}

void TstOrderValidator::rejects_missing_or_blank_symbol() {
    UnifiedOrder o = good_order();
    o.symbol.clear();
    auto r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Symbol is required")));

    // Whitespace is trimmed before the emptiness check.
    o.symbol = QStringLiteral("   ");
    r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Symbol is required")));
}

void TstOrderValidator::rejects_missing_exchange() {
    UnifiedOrder o = good_order();
    o.exchange.clear();
    auto r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Exchange is required")));
    // A blank exchange is "required", not "invalid" — only one of the two fires.
    QVERIFY(!has_error(r, QStringLiteral("Invalid exchange")));

    o.exchange = QStringLiteral("   ");
    r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Exchange is required")));
}

void TstOrderValidator::rejects_unknown_exchange() {
    UnifiedOrder o = good_order();
    o.exchange = QStringLiteral("NOTANEXCHANGE");
    const auto r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Invalid exchange: NOTANEXCHANGE")));
}

void TstOrderValidator::exchange_whitelist_spot_checks() {
    // India
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("NSE")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("BSE")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("NFO")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("BFO")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("MCX")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("CDS")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("NSE_INDEX")));
    // US / global
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("NYSE")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("NASDAQ")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("LSE")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("XETRA")));
    // crypto / forex
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("CRYPTO")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("FOREX")));

    // Not on the list.
    QVERIFY(!OrderValidator::is_valid_exchange(QString()));
    QVERIFY(!OrderValidator::is_valid_exchange(QStringLiteral("BINANCE")));
    QVERIFY(!OrderValidator::is_valid_exchange(QStringLiteral("NSEINDEX")));
    QVERIFY(!OrderValidator::is_valid_exchange(QStringLiteral("N")));
}

void TstOrderValidator::exchange_match_is_case_insensitive_but_not_whitespace_tolerant() {
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("nse")));
    QVERIFY(OrderValidator::is_valid_exchange(QStringLiteral("Nasdaq")));

    // Documented quirk, pinned so a change to it is deliberate: validate() trims
    // only for the emptiness check and then hands the RAW string to the
    // whitelist, so a padded exchange is rejected as invalid rather than
    // normalised. Callers must trim upstream.
    QVERIFY(!OrderValidator::is_valid_exchange(QStringLiteral(" NSE ")));

    UnifiedOrder o = good_order();
    o.exchange = QStringLiteral(" NSE ");
    const auto r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Invalid exchange")));
}

void TstOrderValidator::rejects_non_positive_quantity() {
    UnifiedOrder o = good_order();
    o.quantity = 0;
    auto r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Quantity must be a positive number")));

    o.quantity = -5;
    r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Quantity must be a positive number")));

    // Fractional quantities are permitted (crypto / fractional shares).
    o.quantity = 0.5;
    r = OrderValidator::validate(o);
    QVERIFY2(r.valid, qPrintable(joined(r)));
}

void TstOrderValidator::price_required_only_for_limit_family() {
    UnifiedOrder o = good_order();

    o.order_type = OrderType::Limit;
    o.price = 0;
    auto r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Price is required")));

    o.price = 2500.75;
    r = OrderValidator::validate(o);
    QVERIFY2(r.valid, qPrintable(joined(r)));

    // Market orders never need a price.
    o.order_type = OrderType::Market;
    o.price = 0;
    r = OrderValidator::validate(o);
    QVERIFY2(r.valid, qPrintable(joined(r)));

    // A negative price is as bad as a missing one.
    o.order_type = OrderType::Limit;
    o.price = -1;
    r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Price is required")));
}

void TstOrderValidator::trigger_required_only_for_stop_family() {
    UnifiedOrder o = good_order();

    o.order_type = OrderType::StopLoss;
    o.stop_price = 0;
    auto r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Trigger price is required")));

    o.stop_price = 2400;
    r = OrderValidator::validate(o);
    QVERIFY2(r.valid, qPrintable(joined(r)));

    // A plain Market order with no trigger is fine.
    o.order_type = OrderType::Market;
    o.stop_price = 0;
    r = OrderValidator::validate(o);
    QVERIFY2(r.valid, qPrintable(joined(r)));

    // ...and so is a Limit order (trigger belongs to the stop family only).
    o.order_type = OrderType::Limit;
    o.price = 2500;
    r = OrderValidator::validate(o);
    QVERIFY2(r.valid, qPrintable(joined(r)));
}

void TstOrderValidator::stop_loss_limit_requires_both_price_and_trigger() {
    UnifiedOrder o = good_order();
    o.order_type = OrderType::StopLossLimit;
    o.price = 0;
    o.stop_price = 0;

    auto r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QCOMPARE(r.errors.size(), qsizetype(2));
    QVERIFY(has_error(r, QStringLiteral("Price is required")));
    QVERIFY(has_error(r, QStringLiteral("Trigger price is required")));

    o.price = 2500;
    r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QCOMPARE(r.errors.size(), qsizetype(1));
    QVERIFY(has_error(r, QStringLiteral("Trigger price is required")));

    o.stop_price = 2490;
    r = OrderValidator::validate(o);
    QVERIFY2(r.valid, qPrintable(joined(r)));
}

void TstOrderValidator::errors_accumulate() {
    UnifiedOrder o;
    o.symbol.clear();
    o.exchange = QStringLiteral("NOPE");
    o.order_type = OrderType::Limit;
    o.quantity = 0;
    o.price = 0;

    const auto r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QCOMPARE(r.errors.size(), qsizetype(4));
    QVERIFY(has_error(r, QStringLiteral("Symbol is required")));
    QVERIFY(has_error(r, QStringLiteral("Invalid exchange: NOPE")));
    QVERIFY(has_error(r, QStringLiteral("Quantity must be a positive number")));
    QVERIFY(has_error(r, QStringLiteral("Price is required")));
}

// ── validate_smart() ─────────────────────────────────────────────────────────

void TstOrderValidator::smart_flatten_with_zero_quantity_is_valid() {
    // THE case this suite exists for. "Flatten": target net position 0, quantity
    // left at 0 because the engine reads the live book and derives it. Requiring
    // a positive quantity here rejects every flatten order.
    SmartOrder o = good_smart();
    o.position_size = 0;
    o.quantity = 0;

    const auto r = OrderValidator::validate_smart(o);
    QVERIFY2(r.valid, qPrintable(joined(r)));
    QVERIFY(r.errors.isEmpty());
    QVERIFY(!has_error(r, QStringLiteral("Quantity")));
}

void TstOrderValidator::smart_allows_zero_quantity_for_any_position_size() {
    // Not just the flatten case: quantity is always derivable, so 0 is never an
    // error on a smart order regardless of the target size.
    SmartOrder o = good_smart();
    o.quantity = 0;

    o.position_size = 100;
    QVERIFY(OrderValidator::validate_smart(o).valid);

    o.position_size = -100; // target short
    QVERIFY(OrderValidator::validate_smart(o).valid);

    o.position_size = 0;
    QVERIFY(OrderValidator::validate_smart(o).valid);

    // An explicit quantity is fine too.
    o.quantity = 25;
    QVERIFY(OrderValidator::validate_smart(o).valid);
}

void TstOrderValidator::smart_rejects_negative_quantity() {
    // Only a nonsensical NEGATIVE fallback quantity is an error.
    SmartOrder o = good_smart();
    o.quantity = -1;

    const auto r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Quantity cannot be negative")));
    // And specifically NOT the stricter validate() wording.
    QVERIFY(!has_error(r, QStringLiteral("Quantity must be a positive number")));
}

void TstOrderValidator::smart_validates_symbol_and_exchange() {
    SmartOrder o = good_smart();
    o.symbol = QStringLiteral("  ");
    auto r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Symbol is required")));

    o = good_smart();
    o.exchange.clear();
    r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Exchange is required")));

    o = good_smart();
    o.exchange = QStringLiteral("NOPE");
    r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Invalid exchange: NOPE")));
}

void TstOrderValidator::smart_price_and_trigger_rules_match_validate() {
    SmartOrder o = good_smart();

    o.order_type = OrderType::Limit;
    o.price = 0;
    auto r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Price is required")));

    o.price = 2500;
    QVERIFY(OrderValidator::validate_smart(o).valid);

    o = good_smart();
    o.order_type = OrderType::StopLoss;
    o.trigger_price = 0;
    r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Trigger price is required")));

    o.trigger_price = 2400;
    QVERIFY(OrderValidator::validate_smart(o).valid);

    // A flatten order with a stop type still needs its trigger.
    o = good_smart();
    o.position_size = 0;
    o.quantity = 0;
    o.order_type = OrderType::StopLoss;
    o.trigger_price = 0;
    r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Trigger price is required")));
    QVERIFY(!has_error(r, QStringLiteral("Quantity")));
}

// ── validate_basket() ────────────────────────────────────────────────────────

void TstOrderValidator::basket_rejects_empty() {
    BasketOrderRequest b;
    const auto r = OrderValidator::validate_basket(b);
    QVERIFY(!r.valid);
    QCOMPARE(r.errors.size(), qsizetype(1));
    QVERIFY(has_error(r, QStringLiteral("Basket has no orders")));
}

void TstOrderValidator::basket_accepts_all_valid_legs() {
    BasketOrderRequest b;
    b.strategy_name = QStringLiteral("pair");
    b.orders.append(good_order());

    UnifiedOrder leg2 = good_order();
    leg2.symbol = QStringLiteral("TCS");
    leg2.side = OrderSide::Sell;
    b.orders.append(leg2);

    const auto r = OrderValidator::validate_basket(b);
    QVERIFY2(r.valid, qPrintable(joined(r)));
    QVERIFY(r.errors.isEmpty());
}

void TstOrderValidator::basket_reports_the_failing_leg_by_index_and_symbol() {
    BasketOrderRequest b;
    b.orders.append(good_order()); // leg 1: fine

    UnifiedOrder bad = good_order();
    bad.symbol = QStringLiteral("TCS");
    bad.quantity = 0; // leg 2: broken
    b.orders.append(bad);

    const auto r = OrderValidator::validate_basket(b);
    QVERIFY(!r.valid);
    QCOMPARE(r.errors.size(), qsizetype(1));
    // 1-based index, then the leg's symbol, then the underlying message.
    QVERIFY2(has_error(r, QStringLiteral("Order 2 (TCS): Quantity must be a positive number")),
             qPrintable(joined(r)));
    QVERIFY(!has_error(r, QStringLiteral("Order 1")));
}

// ── Non-finite / negative numbers (validate, validate_smart) ─────────────────

void TstOrderValidator::rejects_non_finite_numbers() {
    UnifiedOrder o = good_order();
    o.quantity = kNaN;
    auto r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Quantity must be a positive number")));

    o = good_order();
    o.quantity = kInf;
    QVERIFY(!OrderValidator::validate(o).valid);

    // A NaN limit price would otherwise slip past "price <= 0".
    o = good_order();
    o.order_type = OrderType::Limit;
    o.price = kNaN;
    r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Price / trigger must be finite and not negative")));

    o = good_order();
    o.stop_price = -1;
    r = OrderValidator::validate(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Price / trigger must be finite and not negative")));
}

void TstOrderValidator::smart_rejects_non_finite_numbers() {
    // Regression: validate_smart() accepted NaN everywhere. A NaN position_size
    // made place_smart_order's risk quantity NaN, its `quantity > 0` guard false,
    // and so skipped the whole settings-driven risk gate on the way to the broker.
    SmartOrder o = good_smart();
    o.position_size = kNaN;
    auto r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Target position size must be a finite number")));

    o = good_smart();
    o.position_size = -kInf;
    QVERIFY(!OrderValidator::validate_smart(o).valid);

    o = good_smart();
    o.quantity = kNaN;
    r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Quantity must be a finite number")));
    QVERIFY(!has_error(r, QStringLiteral("cannot be negative")));

    o = good_smart();
    o.order_type = OrderType::Limit;
    o.price = kNaN;
    r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Price / trigger must be finite and not negative")));

    o = good_smart();
    o.order_type = OrderType::StopLoss;
    o.trigger_price = kNaN;
    QVERIFY(!OrderValidator::validate_smart(o).valid);

    o = good_smart();
    o.price = kInf; // market order carrying the LTP
    QVERIFY(!OrderValidator::validate_smart(o).valid);
}

void TstOrderValidator::smart_rejects_negative_price_or_trigger() {
    // Same rule as validate(): a negative price is never meaningful, even on a
    // market order (SmartOrder.price is the paper fill price there).
    SmartOrder o = good_smart();
    o.price = -1;
    auto r = OrderValidator::validate_smart(o);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Price / trigger must be finite and not negative")));

    o = good_smart();
    o.trigger_price = -5;
    QVERIFY(!OrderValidator::validate_smart(o).valid);

    // Zero means "not supplied" and stays valid on a market order.
    o = good_smart();
    o.price = 0;
    o.trigger_price = 0;
    QVERIFY(OrderValidator::validate_smart(o).valid);
}

// ── check_risk() ─────────────────────────────────────────────────────────────

void TstOrderValidator::risk_unconfigured_settings_pass_a_normal_order() {
    auto r = OrderValidator::check_risk(good_order(), no_limits(), adding_ctx());
    QVERIFY2(r.valid, qPrintable(joined(r)));

    // Nothing known about the account (no price, no funds, P&L not loaded) is fine
    // as long as no configured limit needs it.
    r = OrderValidator::check_risk(good_order(), no_limits(), RiskContext{});
    QVERIFY2(r.valid, qPrintable(joined(r)));
}

void TstOrderValidator::risk_zero_limits_are_off_even_for_huge_orders() {
    RiskSettings s = no_limits();
    s.sanity_checks = false;
    UnifiedOrder o = good_order();
    o.quantity = 1e12;
    RiskContext c = adding_ctx(1e6, 1.0);
    c.day_pnl = -1e12;
    auto r = OrderValidator::check_risk(o, s, c);
    QVERIFY2(r.valid, qPrintable(joined(r)));

    // Negative "limits" (never produced by parse_risk_settings) are off as well.
    s.max_order_value = -1;
    s.max_order_quantity = -1;
    s.daily_loss_limit = -1;
    r = OrderValidator::check_risk(o, s, c);
    QVERIFY2(r.valid, qPrintable(joined(r)));
}

void TstOrderValidator::risk_max_order_value_boundary() {
    RiskSettings s = no_limits();
    s.max_order_value = 5000;
    UnifiedOrder o = good_order();
    o.quantity = 50;

    // Exactly at the cap: allowed.
    auto r = OrderValidator::check_risk(o, s, adding_ctx(100.0));
    QVERIFY2(r.valid, qPrintable(joined(r)));
    // A cent below / above.
    QVERIFY(OrderValidator::check_risk(o, s, adding_ctx(99.9998)).valid);
    r = OrderValidator::check_risk(o, s, adding_ctx(100.0002));
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("exceeds your max order value")));
    // One more share.
    o.quantity = 51;
    r = OrderValidator::check_risk(o, s, adding_ctx(100.0));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value 5,100.00 exceeds your max order value 5,000.00")),
             qPrintable(joined(r)));
}

void TstOrderValidator::risk_max_order_value_fails_closed_without_a_price() {
    RiskSettings s = no_limits();
    s.max_order_value = 5000;
    // A configured cap that cannot be evaluated is not a pass.
    auto r = OrderValidator::check_risk(good_order(), s, adding_ctx(0.0));
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("no reference price is known")));

    RiskContext c = adding_ctx();
    c.ref_price = kNaN;
    r = OrderValidator::check_risk(good_order(), s, c);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("no reference price is known")));

    // Unconfigured: an unknown price is irrelevant.
    s.max_order_value = 0;
    QVERIFY(OrderValidator::check_risk(good_order(), s, adding_ctx(0.0)).valid);
}

void TstOrderValidator::risk_max_order_quantity_boundary() {
    RiskSettings s = no_limits();
    s.max_order_quantity = 100;
    UnifiedOrder o = good_order();

    o.quantity = 100;
    QVERIFY(OrderValidator::check_risk(o, s, adding_ctx()).valid);
    o.quantity = 99.999;
    QVERIFY(OrderValidator::check_risk(o, s, adding_ctx()).valid);
    o.quantity = 100.5;
    auto r = OrderValidator::check_risk(o, s, adding_ctx());
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("exceeds your max order quantity")));
    o.quantity = 101;
    QVERIFY(!OrderValidator::check_risk(o, s, adding_ctx()).valid);
}

void TstOrderValidator::risk_daily_loss_limit_boundary() {
    RiskSettings s = no_limits();
    s.daily_loss_limit = 5000;
    RiskContext c = adding_ctx();

    c.day_pnl = -4999.99;
    QVERIFY(OrderValidator::check_risk(good_order(), s, c).valid);
    // AT the limit counts as reached ("P&L <= -limit").
    c.day_pnl = -5000;
    auto r = OrderValidator::check_risk(good_order(), s, c);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("Daily loss limit reached")));
    c.day_pnl = -7500;
    QVERIFY(!OrderValidator::check_risk(good_order(), s, c).valid);
    c.day_pnl = 12000; // a profitable day
    QVERIFY(OrderValidator::check_risk(good_order(), s, c).valid);
}

void TstOrderValidator::risk_daily_loss_limit_fails_closed_when_pnl_unknown_or_nan() {
    RiskSettings s = no_limits();
    s.daily_loss_limit = 5000;
    RiskContext c = adding_ctx();

    c.day_pnl_known = false;
    auto r = OrderValidator::check_risk(good_order(), s, c);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("is not loaded yet")));

    // Regression: one position reporting NaN P&L made the sum NaN, which compares
    // false against the limit — every order passed the daily loss limit.
    c.day_pnl_known = true;
    c.day_pnl = kNaN;
    r = OrderValidator::check_risk(good_order(), s, c);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("could not be computed")));

    // Unconfigured: unknown P&L is irrelevant.
    s.daily_loss_limit = 0;
    c.day_pnl_known = false;
    QVERIFY(OrderValidator::check_risk(good_order(), s, c).valid);
}

void TstOrderValidator::risk_blocked_symbols() {
    RiskSettings s = no_limits();
    s.blocked_symbols = {QStringLiteral("YESBANK"), QStringLiteral("NSE:IDEA")};
    UnifiedOrder o = good_order();

    o.symbol = QStringLiteral("YESBANK");
    auto r = OrderValidator::check_risk(o, s, adding_ctx());
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("YESBANK is on your blocked-symbols list")));

    // The order side is trimmed and upper-cased; a bare entry blocks every exchange.
    o.symbol = QStringLiteral(" yesbank ");
    o.exchange = QStringLiteral("bse");
    QVERIFY(!OrderValidator::check_risk(o, s, adding_ctx()).valid);

    // An EXCHANGE:SYMBOL entry blocks that exchange only.
    o.symbol = QStringLiteral("IDEA");
    o.exchange = QStringLiteral("NSE");
    QVERIFY(!OrderValidator::check_risk(o, s, adding_ctx()).valid);
    o.exchange = QStringLiteral("nse");
    QVERIFY(!OrderValidator::check_risk(o, s, adding_ctx()).valid);
    o.exchange = QStringLiteral("BSE");
    QVERIFY(OrderValidator::check_risk(o, s, adding_ctx()).valid);

    // No partial matches.
    o.symbol = QStringLiteral("YESBANKX");
    o.exchange = QStringLiteral("NSE");
    QVERIFY(OrderValidator::check_risk(o, s, adding_ctx()).valid);

    // Empty list blocks nothing.
    s.blocked_symbols.clear();
    o.symbol = QStringLiteral("YESBANK");
    QVERIFY(OrderValidator::check_risk(o, s, adding_ctx()).valid);
}

void TstOrderValidator::risk_sanity_quantity_ceiling_boundary() {
    const RiskSettings s = no_limits(); // sanity on by default
    UnifiedOrder o = good_order();
    RiskContext c = adding_ctx(0.0); // price unknown: isolate the quantity rule

    o.quantity = OrderValidator::kSanityMaxQuantity;
    QVERIFY(OrderValidator::check_risk(o, s, c).valid);
    o.quantity = OrderValidator::kSanityMaxQuantity + 1;
    auto r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("beyond any sane single order")));

    // The ceiling applies to everything — closing orders and the local simulator too.
    c.reduces_position = true;
    QVERIFY(!OrderValidator::check_risk(o, s, c).valid);
    c.broker_routed = false;
    QVERIFY(!OrderValidator::check_risk(o, s, c).valid);
}

void TstOrderValidator::risk_sanity_buying_power_boundary() {
    const RiskSettings s = no_limits();
    UnifiedOrder o = good_order();
    const RiskContext c = adding_ctx(100.0, 1000.0); // 10x buying power = 10,000

    o.quantity = 100; // exactly 10x
    QVERIFY(OrderValidator::check_risk(o, s, c).valid);
    o.quantity = 100.01; // 10,001
    auto r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("more than 10x the available buying power")));
}

void TstOrderValidator::risk_sanity_buying_power_exemptions() {
    const RiskSettings s = no_limits();
    UnifiedOrder o = good_order();
    o.quantity = 1000; // notional 100,000 = 100x a buying power of 1,000
    RiskContext c = adding_ctx(100.0, 1000.0);

    QVERIFY(!OrderValidator::check_risk(o, s, c).valid); // cash segment: caught

    // Derivative segments are exempt (their notional / margin ratio is 10-50x).
    o.exchange = QStringLiteral("NFO");
    QVERIFY(OrderValidator::check_risk(o, s, c).valid);
    o.exchange = QStringLiteral(" mcx ");
    QVERIFY(OrderValidator::check_risk(o, s, c).valid);
    o.exchange = QStringLiteral("NSE");

    // Closing a position is exempt.
    c.reduces_position = true;
    QVERIFY(OrderValidator::check_risk(o, s, c).valid);
    c.reduces_position = false;

    // Unknown buying power or unknown price: nothing to compare.
    c.buying_power = 0;
    QVERIFY(OrderValidator::check_risk(o, s, c).valid);
    c = adding_ctx(0.0, 1000.0);
    QVERIFY(OrderValidator::check_risk(o, s, c).valid);
}

void TstOrderValidator::risk_sanity_off_disables_only_the_sanity_guard() {
    RiskSettings s = no_limits();
    s.sanity_checks = false;
    UnifiedOrder o = good_order();
    o.quantity = 2e7;
    QVERIFY(OrderValidator::check_risk(o, s, adding_ctx(100.0, 1.0)).valid);

    s.max_order_quantity = 100; // configured limits still apply
    QVERIFY(!OrderValidator::check_risk(o, s, adding_ctx(100.0, 1.0)).valid);
}

void TstOrderValidator::risk_reducing_orders_skip_configured_limits() {
    RiskSettings s = no_limits();
    s.max_order_value = 1000;
    s.max_order_quantity = 1;
    s.daily_loss_limit = 100;
    s.blocked_symbols = {QStringLiteral("RELIANCE")};
    UnifiedOrder o = good_order();
    o.quantity = 50;
    RiskContext c = adding_ctx(100.0);
    c.day_pnl = -1e6;

    auto r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QCOMPARE(r.errors.size(), qsizetype(4)); // blocked, quantity, value, daily loss

    // A limit must never trap the user in the position it exists to cap.
    c.reduces_position = true;
    r = OrderValidator::check_risk(o, s, c);
    QVERIFY2(r.valid, qPrintable(joined(r)));
}

void TstOrderValidator::risk_local_paper_skips_configured_limits_but_not_sanity() {
    RiskSettings s = no_limits();
    s.max_order_value = 1000;
    s.max_order_quantity = 1;
    s.daily_loss_limit = 100;
    s.blocked_symbols = {QStringLiteral("RELIANCE")};
    UnifiedOrder o = good_order();
    o.quantity = 50;
    RiskContext c = adding_ctx(100.0);
    c.day_pnl = -1e6;
    c.broker_routed = false;

    auto r = OrderValidator::check_risk(o, s, c);
    QVERIFY2(r.valid, qPrintable(joined(r)));

    o.quantity = OrderValidator::kSanityMaxQuantity * 2;
    QVERIFY(!OrderValidator::check_risk(o, s, c).valid);
}

void TstOrderValidator::risk_non_finite_quantity_is_refused() {
    // Regression: every limit is "quantity > cap", which NaN passes — a NaN order
    // that reached check_risk (the legacy session path runs it without validate())
    // went through every configured cap. It is refused whatever the settings.
    RiskSettings s = no_limits();
    s.sanity_checks = false;
    UnifiedOrder o = good_order();
    o.quantity = kNaN;
    auto r = OrderValidator::check_risk(o, s, adding_ctx());
    QVERIFY(!r.valid);
    QVERIFY(has_error(r, QStringLiteral("not a finite number")));

    s.max_order_quantity = 100;
    QVERIFY(!OrderValidator::check_risk(o, s, adding_ctx()).valid);

    o.quantity = kInf;
    QVERIFY(!OrderValidator::check_risk(o, s, adding_ctx()).valid);

    RiskContext c = adding_ctx();
    c.reduces_position = true;
    c.broker_routed = false;
    o.quantity = kNaN;
    QVERIFY(!OrderValidator::check_risk(o, s, c).valid);
}

void TstOrderValidator::risk_errors_accumulate() {
    RiskSettings s = no_limits(); // sanity on
    s.max_order_value = 1000;
    s.max_order_quantity = 10;
    s.daily_loss_limit = 100;
    s.blocked_symbols = {QStringLiteral("RELIANCE")};
    UnifiedOrder o = good_order();
    o.quantity = 50; // notional 5,000 > 10 x buying power 10
    RiskContext c = adding_ctx(100.0, 10.0);
    c.day_pnl = -500;

    const auto r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QCOMPARE(r.errors.size(), qsizetype(5));
    QVERIFY(has_error(r, QStringLiteral("buying power")));
    QVERIFY(has_error(r, QStringLiteral("blocked-symbols")));
    QVERIFY(has_error(r, QStringLiteral("max order quantity")));
    QVERIFY(has_error(r, QStringLiteral("max order value")));
    QVERIFY(has_error(r, QStringLiteral("Daily loss limit reached")));
}

// ── check_risk(): currencies ─────────────────────────────────────────────────

void TstOrderValidator::risk_same_currency_needs_no_rate() {
    FakeFx fx;
    UnifiedOrder o = good_order(); // 10 x 100 INR
    auto r = OrderValidator::check_risk(o, no_limits(), ccy_ctx(100.0, 1'000'000.0, "INR", "INR", fx.lookup()));
    QVERIFY2(r.valid, qPrintable(joined(r)));

    RiskSettings s = no_limits();
    s.max_order_value = 5000;
    s.limits_currency = QStringLiteral("INR");
    o.quantity = 51;
    r = OrderValidator::check_risk(o, s, ccy_ctx(100.0, 1'000'000.0, "INR", "INR", fx.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value 5,100.00 INR exceeds your max order value 5,000.00 INR")),
             qPrintable(joined(r)));
    QCOMPARE(fx.calls, 0); // same code: never asks for a rate
}

void TstOrderValidator::risk_cross_currency_buying_power_uses_the_rate() {
    FakeFx fx;
    fx.set("GBp", "USD", 1.3183 / 100.0);
    fx.set("JPY", "USD", 1.0 / 159.452);
    fx.set("USD", "INR", 93.912);
    const RiskSettings s = no_limits(); // the sanity guard (10x buying power) is on
    UnifiedOrder o = good_order();

    // VOD.L at 117.70p x 1,000 = 117,700 GBp = 1,177 GBP = 1,551.64 USD. Against a
    // USD account the old code compared 117,700 with 10 x 200 and refused it.
    o.exchange = QStringLiteral("LSE");
    o.quantity = 1000;
    auto r = OrderValidator::check_risk(o, s, ccy_ctx(117.70, 200.0, "GBp", "USD", fx.lookup()));
    QVERIFY2(r.valid, qPrintable(joined(r)));
    r = OrderValidator::check_risk(o, s, ccy_ctx(117.70, 150.0, "GBp", "USD", fx.lookup())); // cap 1,500
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value 1,551.64 USD (117,700.00 GBp at 1 GBp = 0.013183 USD) is more "
                                         "than 10x the available buying power 150.00 USD")),
             qPrintable(joined(r)));

    // 7203.T at 3,428 JPY x 100 = 342,800 JPY = 2,149.84 USD: inside 10 x 1,000 USD.
    o.exchange = QStringLiteral("NSE");
    o.quantity = 100;
    r = OrderValidator::check_risk(o, s, ccy_ctx(3428.0, 1000.0, "JPY", "USD", fx.lookup()));
    QVERIFY2(r.valid, qPrintable(joined(r)));

    // The other direction was under-checked 94x: 100 x 351.71 USD = 35,171 USD =
    // 3,302,978.95 INR against an INR account with 300,000 INR (cap 3,000,000).
    o.exchange = QStringLiteral("NASDAQ");
    r = OrderValidator::check_risk(o, s, ccy_ctx(351.71, 300'000.0, "USD", "INR", fx.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("3,302,978.95 INR (35,171.00 USD at 1 USD = 93.912 INR)")), qPrintable(joined(r)));
}

void TstOrderValidator::risk_cross_currency_without_a_rate_is_refused_with_the_reason() {
    FakeFx fx; // knows no rate at all
    RiskSettings s = no_limits();
    UnifiedOrder o = good_order();
    o.exchange = QStringLiteral("LSE");
    o.quantity = 1000;

    // The buying-power check needs GBp -> USD and has no rate: refused, never 1:1.
    auto r = OrderValidator::check_risk(o, s, ccy_ctx(117.70, 200.0, "GBp", "USD", fx.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value 117,700.00 GBp cannot be compared with the available buying "
                                         "power 200.00 USD: no GBp to USD exchange rate is available yet")),
             qPrintable(joined(r)));
    QVERIFY(has_error(r, QStringLiteral("order refused")));
    QCOMPARE(fx.calls, 1);

    // No lookup at all behaves the same.
    QVERIFY(!OrderValidator::check_risk(o, s, ccy_ctx(117.70, 200.0, "GBp", "USD")).valid);

    // Nothing to convert -> no rate needed: unknown buying power, a closing order, a
    // derivative segment, the guard switched off.
    QVERIFY(OrderValidator::check_risk(o, s, ccy_ctx(117.70, 0.0, "GBp", "USD", fx.lookup())).valid);
    RiskContext c = ccy_ctx(117.70, 200.0, "GBp", "USD", fx.lookup());
    c.reduces_position = true;
    QVERIFY(OrderValidator::check_risk(o, s, c).valid);
    UnifiedOrder fut = o;
    fut.exchange = QStringLiteral("NFO");
    QVERIFY(OrderValidator::check_risk(fut, s, ccy_ctx(117.70, 200.0, "GBp", "USD", fx.lookup())).valid);
    s.sanity_checks = false;
    QVERIFY(OrderValidator::check_risk(o, s, ccy_ctx(117.70, 200.0, "GBp", "USD", fx.lookup())).valid);

    // A configured max order value that cannot be converted fails closed too.
    s.max_order_value = 1e9;
    r = OrderValidator::check_risk(o, s, ccy_ctx(117.70, 200.0, "GBp", "USD", fx.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Max order value 1,000,000,000.00 USD is set but the order value 117,700.00 GBp "
                                         "cannot be converted: no GBp to USD exchange rate")),
             qPrintable(joined(r)));
}

void TstOrderValidator::risk_stale_and_approximate_rates_are_used_and_named() {
    const RiskSettings s = no_limits();
    UnifiedOrder o = good_order();
    o.exchange = QStringLiteral("LSE");
    o.quantity = 1000;

    // A stale rate is still a rate: used (the order passes) ...
    FakeFx stale;
    stale.set("GBp", "USD", 1.3183 / 100.0, /*stale=*/true);
    QVERIFY(OrderValidator::check_risk(o, s, ccy_ctx(117.70, 200.0, "GBp", "USD", stale.lookup())).valid);
    // ... and named when it causes a refusal.
    auto r = OrderValidator::check_risk(o, s, ccy_ctx(117.70, 150.0, "GBp", "USD", stale.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("1 GBp = 0.013183 USD, stale rate")), qPrintable(joined(r)));

    // The shell's note (rate time from the FX service) replaces the plain rate text.
    FakeFx noted;
    noted.set("GBp", "USD", 1.3183 / 100.0, true, false, QStringLiteral("1 GBP = 1.3183 USD · Nov 16, 2026 14:32 · stale"));
    r = OrderValidator::check_risk(o, s, ccy_ctx(117.70, 150.0, "GBp", "USD", noted.lookup()));
    QVERIFY2(has_error(r, QStringLiteral("(117,700.00 GBp at 1 GBP = 1.3183 USD · Nov 16, 2026 14:32 · stale)")),
             qPrintable(joined(r)));

    // Stablecoins are their own currency: BTC/USDT 65,000 USDT on a USD account.
    UnifiedOrder btc = good_order();
    btc.symbol = QStringLiteral("BTC-USDT");
    btc.exchange = QStringLiteral("CRYPTO");
    btc.quantity = 1;
    FakeFx usdt;
    usdt.set("USDT", "USD", 0.99903); // the real USDT-USD leg: 64,936.95 USD
    QVERIFY(OrderValidator::check_risk(btc, s, ccy_ctx(65000.0, 6500.0, "USDT", "USD", usdt.lookup())).valid);
    // No USDT leg yet: 1:1 only as a labelled approximation.
    FakeFx peg;
    peg.set("USDT", "USD", 1.0, false, /*approximate=*/true);
    QVERIFY(OrderValidator::check_risk(btc, s, ccy_ctx(65000.0, 6500.0, "USDT", "USD", peg.lookup())).valid);
    r = OrderValidator::check_risk(btc, s, ccy_ctx(65000.0, 6000.0, "USDT", "USD", peg.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("1 USDT = 1 USD, approximate (1:1)")), qPrintable(joined(r)));
}

void TstOrderValidator::risk_minor_units_against_a_major_account() {
    // A pence-quoted share against a GBP account: 117.70p x 1,000 = 1,177.00 GBP,
    // not 117,700 GBP. The lookup supplies the 1/100 (as FxRateService does).
    FakeFx fx;
    fx.set("GBp", "GBP", 0.01);
    const RiskSettings s = no_limits();
    UnifiedOrder o = good_order();
    o.exchange = QStringLiteral("LSE");
    o.quantity = 1000;
    QVERIFY(OrderValidator::check_risk(o, s, ccy_ctx(117.70, 200.0, "GBp", "GBP", fx.lookup())).valid);
    const auto r = OrderValidator::check_risk(o, s, ccy_ctx(117.70, 100.0, "GBp", "GBP", fx.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value 1,177.00 GBP (117,700.00 GBp at 1 GBp = 0.01 GBP)")),
             qPrintable(joined(r)));
}

void TstOrderValidator::risk_limits_currency_converts_value_and_day_pnl() {
    FakeFx fx;
    fx.set("INR", "USD", 1.0 / 93.912);
    RiskSettings s = no_limits();
    s.limits_currency = QStringLiteral("USD");
    s.max_order_value = 1000;
    UnifiedOrder o = good_order();
    o.quantity = 100;

    // 100 x 900 INR = 90,000 INR = 958.34 USD: inside a 1,000 USD cap.
    auto r = OrderValidator::check_risk(o, s, ccy_ctx(900.0, 1e9, "INR", "INR", fx.lookup()));
    QVERIFY2(r.valid, qPrintable(joined(r)));
    // 100 x 1,000 INR = 1,064.83 USD: over it.
    r = OrderValidator::check_risk(o, s, ccy_ctx(1000.0, 1e9, "INR", "INR", fx.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value 1,064.83 USD (100,000.00 INR at 1 INR = 0.0106483 USD) exceeds "
                                         "your max order value 1,000.00 USD")),
             qPrintable(joined(r)));

    // The daily loss limit is in USD too; today's P&L is in the account's INR.
    s.max_order_value = 0;
    s.daily_loss_limit = 1000;
    RiskContext c = ccy_ctx(900.0, 1e9, "INR", "INR", fx.lookup());
    c.day_pnl = -90'000; // -958.34 USD
    QVERIFY2(OrderValidator::check_risk(o, s, c).valid, qPrintable(joined(OrderValidator::check_risk(o, s, c))));
    c.day_pnl = -100'000; // -1,064.83 USD
    r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Daily loss limit reached: today's P&L -1,064.83 USD")), qPrintable(joined(r)));

    // No INR -> USD rate: neither limit can be evaluated, so the order is refused.
    FakeFx none;
    c.fx = none.lookup();
    c.day_pnl = -10;
    r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("today's P&L -10.00 INR cannot be converted: no INR to USD exchange rate")),
             qPrintable(joined(r)));

    // Limits without a currency mean each account's own (the pre-4.6 meaning):
    // 1,000 is then INR and 90,000 INR is over it, with no rate needed.
    s.limits_currency.clear();
    s.daily_loss_limit = 0;
    s.max_order_value = 1000;
    r = OrderValidator::check_risk(o, s, ccy_ctx(900.0, 1e9, "INR", "INR", none.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value 90,000.00 INR exceeds your max order value 1,000.00 INR")),
             qPrintable(joined(r)));
    QCOMPARE(none.calls, 1); // only the refused P&L conversion above asked
}

void TstOrderValidator::risk_unknown_currencies_keep_the_legacy_comparison() {
    FakeFx fx;
    const RiskSettings s = no_limits();
    UnifiedOrder o = good_order();
    o.quantity = 1000; // 100,000 against 10 x 1,000

    // Instrument currency unknown: taken as the account's, no rate asked.
    auto r = OrderValidator::check_risk(o, s, ccy_ctx(100.0, 1000.0, QString(), "INR", fx.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value 100,000.00 INR is more than 10x the available buying power "
                                         "1,000.00 INR")),
             qPrintable(joined(r)));
    // Account currency unknown (legacy session path): compared as they are.
    r = OrderValidator::check_risk(o, s, ccy_ctx(100.0, 1000.0, "GBp", QString(), fx.lookup()));
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value 100,000.00 GBp is more than 10x")), qPrintable(joined(r)));
    QCOMPARE(fx.calls, 0);
}

void TstOrderValidator::risk_money_text_hook_formats_amounts() {
    RiskSettings s = no_limits();
    s.max_order_value = 5000;
    UnifiedOrder o = good_order();
    o.quantity = 51;
    RiskContext c = ccy_ctx(100.0, 1e9, "INR", "INR");
    c.money_text = [](double v, const QString& ccy) { return QStringLiteral("<%1 %2>").arg(ccy).arg(v); };
    const auto r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value <INR 5100> exceeds your max order value <INR 5000>")),
             qPrintable(joined(r)));
}

void TstOrderValidator::risk_funds_currency_is_the_buying_power_currency() {
    // The broker profile says USD, but the broker reported its funds in EUR: the
    // order value is compared with buying power in EUR, not in the profile's USD.
    FakeFx fx;
    fx.set("USD", "EUR", 0.9);
    const RiskSettings s = no_limits();
    UnifiedOrder o = good_order();
    o.exchange = QStringLiteral("NASDAQ");
    o.quantity = 100; // 100 x 100 USD = 10,000 USD = 9,000 EUR, under 10 x 1,000 EUR
    RiskContext c = ccy_ctx(100.0, 1000.0, "USD", "USD", fx.lookup());
    c.funds_currency = QStringLiteral("EUR");
    auto r = OrderValidator::check_risk(o, s, c);
    QVERIFY2(r.valid, qPrintable(joined(r)));

    o.quantity = 120; // 12,000 USD = 10,800 EUR: over 10 x 1,000 EUR
    r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Order value 10,800.00 EUR (12,000.00 USD at 1 USD = 0.9 EUR) is more than "
                                         "10x the available buying power 1,000.00 EUR")),
             qPrintable(joined(r)));

    // No USD -> EUR rate: refused with the reason, never compared 1:1.
    FakeFx none;
    c.fx = none.lookup();
    r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("cannot be compared with the available buying power 1,000.00 EUR: no USD to "
                                         "EUR exchange rate")),
             qPrintable(joined(r)));

    // Funds currency equal to the price currency: nothing to convert.
    c.funds_currency = QStringLiteral("USD");
    o.quantity = 90; // 9,000 USD against 10 x 1,000 USD
    QVERIFY2(OrderValidator::check_risk(o, s, c).valid, qPrintable(joined(OrderValidator::check_risk(o, s, c))));
    QCOMPARE(none.calls, 1);
}

void TstOrderValidator::risk_day_pnl_in_several_currencies_is_converted_part_by_part() {
    FakeFx fx;
    fx.set("GBP", "USD", 1.3183);
    RiskSettings s = no_limits();
    s.daily_loss_limit = 1000; // in the account's USD
    UnifiedOrder o = good_order();
    o.exchange = QStringLiteral("NASDAQ");
    RiskContext c = ccy_ctx(100.0, 1e9, "USD", "USD", fx.lookup());
    // -500 USD (rows without a currency of their own) and -300 GBP (an LSE row).
    c.day_pnl = -800; // the raw sum the gate must NOT compare
    c.day_pnl_by_currency.insert(QString(), -500.0);
    c.day_pnl_by_currency.insert(QStringLiteral("GBP"), -300.0);
    auto r = OrderValidator::check_risk(o, s, c); // -500 + -395.49 = -895.49 USD
    QVERIFY2(r.valid, qPrintable(joined(r)));

    c.day_pnl_by_currency.insert(QStringLiteral("GBP"), -400.0); // -500 + -527.32 = -1,027.32 USD
    r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("Daily loss limit reached: today's P&L -1,027.32 USD (-400.00 GBP at 1 GBP = "
                                         "1.3183 USD + -500.00 USD) <= -1,000.00 USD")),
             qPrintable(joined(r)));

    // The raw sum (-900) would pass a 1,020 limit; the converted -1,027.32 USD does not.
    c.day_pnl = -900;
    s.daily_loss_limit = 1020;
    QVERIFY(!OrderValidator::check_risk(o, s, c).valid);

    // One part without a rate: the limit cannot be evaluated, so the order is refused.
    FakeFx none;
    c.fx = none.lookup();
    r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("today's P&L (in 2 currencies) cannot be converted: no GBP to USD exchange "
                                         "rate")),
             qPrintable(joined(r)));

    // A single part keeps the one-currency message.
    c.fx = fx.lookup();
    c.day_pnl_by_currency.clear();
    c.day_pnl_by_currency.insert(QStringLiteral("GBP"), -800.0); // -1,054.64 USD
    s.daily_loss_limit = 1000;
    r = OrderValidator::check_risk(o, s, c);
    QVERIFY(!r.valid);
    QVERIFY2(has_error(r, QStringLiteral("today's P&L -1,054.64 USD (-800.00 GBP at 1 GBP = 1.3183 USD)")),
             qPrintable(joined(r)));
}

// ── build_risk_context() ─────────────────────────────────────────────────────

void TstOrderValidator::context_without_snapshot() {
    auto c = OrderValidator::build_risk_context(good_order(), true, nullptr);
    QVERIFY(c.broker_routed);
    QCOMPARE(c.ref_price, 0.0);
    QCOMPARE(c.buying_power, 0.0);
    QVERIFY(!c.day_pnl_known);
    QVERIFY(!c.reduces_position);

    c = OrderValidator::build_risk_context(good_order(), false, nullptr);
    QVERIFY(!c.broker_routed);
}

void TstOrderValidator::context_reference_price_precedence() {
    RiskSnapshot snap;
    snap.ltp.insert(QStringLiteral("RELIANCE"), 2500.0);
    snap.positions_known = true;
    snap.positions = {position(QStringLiteral("RELIANCE"), 10, QString(), QStringLiteral("MIS"),
                               QStringLiteral("NSE"), 0.0, 2400.0)};
    UnifiedOrder o = good_order();

    o.order_type = OrderType::Limit;
    o.price = 2450;
    QCOMPARE(OrderValidator::build_risk_context(o, true, &snap).ref_price, 2450.0);

    o.order_type = OrderType::StopLossLimit; // the limit price, not the trigger
    o.price = 2460;
    o.stop_price = 2455;
    QCOMPARE(OrderValidator::build_risk_context(o, true, &snap).ref_price, 2460.0);

    o.order_type = OrderType::StopLoss; // SL-market: the trigger
    o.price = 0;
    o.stop_price = 2440;
    QCOMPARE(OrderValidator::build_risk_context(o, true, &snap).ref_price, 2440.0);

    o.order_type = OrderType::Market; // a caller-supplied LTP (paper market orders)
    o.stop_price = 0;
    o.price = 2470;
    QCOMPARE(OrderValidator::build_risk_context(o, true, &snap).ref_price, 2470.0);

    o.price = 0; // the account's last quote, symbol matched trimmed + upper-cased
    o.symbol = QStringLiteral(" reliance ");
    QCOMPARE(OrderValidator::build_risk_context(o, true, &snap).ref_price, 2500.0);

    snap.ltp.clear(); // no quote: the open position's LTP
    QCOMPARE(OrderValidator::build_risk_context(o, true, &snap).ref_price, 2400.0);

    snap.positions.clear(); // nothing known
    QCOMPARE(OrderValidator::build_risk_context(o, true, &snap).ref_price, 0.0);
}

void TstOrderValidator::context_funds_and_day_pnl() {
    RiskSnapshot snap;
    snap.funds_known = true;
    snap.available = 12345.5;
    snap.positions_known = true;
    snap.positions = {
        position(QStringLiteral("RELIANCE"), 10, QString(), QStringLiteral("MIS"), QStringLiteral("NSE"), -300.0),
        position(QStringLiteral("TCS"), -5, QString(), QStringLiteral("MIS"), QStringLiteral("NSE"), 120.0),
        // Closed earlier today (quantity 0): its realised P&L still counts.
        position(QStringLiteral("INFY"), 0, QString(), QStringLiteral("MIS"), QStringLiteral("NSE"), -50.0),
    };

    auto c = OrderValidator::build_risk_context(good_order(), true, &snap);
    QCOMPARE(c.buying_power, 12345.5);
    QVERIFY(c.day_pnl_known);
    QCOMPARE(c.day_pnl, -230.0);

    snap.funds_known = false; // a stale "available" is not used
    QCOMPARE(OrderValidator::build_risk_context(good_order(), true, &snap).buying_power, 0.0);

    snap.positions_known = false; // positions never loaded: P&L unknown, not zero
    c = OrderValidator::build_risk_context(good_order(), true, &snap);
    QVERIFY(!c.day_pnl_known);
    QCOMPARE(c.day_pnl, 0.0);
}

void TstOrderValidator::context_funds_and_position_currencies() {
    RiskSnapshot snap;
    snap.funds_known = true;
    snap.available = 5000.0;
    snap.positions_known = true;
    snap.positions = {
        position(QStringLiteral("AAPL"), 10, QString(), QStringLiteral("CNC"), QStringLiteral("NASDAQ"), -100.0),
        position(QStringLiteral("VOD"), 50, QString(), QStringLiteral("CNC"), QStringLiteral("LSE"), -40.0),
        position(QStringLiteral("MSFT"), 5, QString(), QStringLiteral("CNC"), QString(), 25.0),
    };

    // Nothing says which currency: everything is the account's (legacy behaviour).
    auto c = OrderValidator::build_risk_context(good_order(), true, &snap);
    QVERIFY(c.funds_currency.isEmpty());
    QVERIFY(c.day_pnl_by_currency.isEmpty());
    QCOMPARE(c.day_pnl, -115.0);

    // Funds in EUR; rows report USD / GBP; the row without one is in the funds' EUR.
    snap.funds_currency = QStringLiteral(" EUR ");
    snap.position_currencies = {QStringLiteral("USD"), QStringLiteral("GBP")}; // shorter than positions
    c = OrderValidator::build_risk_context(good_order(), true, &snap);
    QCOMPARE(c.funds_currency, QStringLiteral("EUR"));
    QCOMPARE(c.buying_power, 5000.0);
    QCOMPARE(c.day_pnl_by_currency.size(), 3);
    QCOMPARE(c.day_pnl_by_currency.value(QStringLiteral("USD")), -100.0);
    QCOMPARE(c.day_pnl_by_currency.value(QStringLiteral("GBP")), -40.0);
    QCOMPARE(c.day_pnl_by_currency.value(QStringLiteral("EUR")), 25.0);
    QCOMPARE(c.day_pnl, -115.0); // the raw total is kept for callers that print it, never compared

    // Row currencies but no funds currency: unlabelled rows go under "" (= the account's).
    snap.funds_currency.clear();
    c = OrderValidator::build_risk_context(good_order(), true, &snap);
    QCOMPARE(c.day_pnl_by_currency.value(QString()), 25.0);
    QCOMPARE(c.day_pnl_by_currency.size(), 3);
}

void TstOrderValidator::context_reduces_position_rules() {
    RiskSnapshot snap;
    snap.positions_known = true;
    auto reduces = [&snap](const UnifiedOrder& o, const QVector<BrokerPosition>& ps) {
        snap.positions = ps;
        return OrderValidator::build_risk_context(o, true, &snap).reduces_position;
    };
    const QString sym = QStringLiteral("RELIANCE");

    UnifiedOrder sell = good_order(); // RELIANCE / NSE / Intraday
    sell.side = OrderSide::Sell;
    sell.quantity = 10;
    UnifiedOrder buy = sell;
    buy.side = OrderSide::Buy;

    // Long 10 (MIS = intraday): selling up to 10 reduces it.
    QVERIFY(reduces(sell, {position(sym, 10)}));
    sell.quantity = 4;
    QVERIFY(reduces(sell, {position(sym, 10)}));
    sell.quantity = 10.5; // more than held: it would open a short
    QVERIFY(!reduces(sell, {position(sym, 10)}));
    sell.quantity = 10;
    QVERIFY(!reduces(buy, {position(sym, 10)})); // same side adds

    // Product must match (Indian brokers net per product).
    QVERIFY(!reduces(sell, {position(sym, 10, QString(), QStringLiteral("CNC"))}));
    UnifiedOrder sell_cnc = sell;
    sell_cnc.product_type = ProductType::Delivery;
    QVERIFY(reduces(sell_cnc, {position(sym, 10, QString(), QStringLiteral("CNC"))}));

    // Exchange must match when both are known.
    QVERIFY(!reduces(sell, {position(sym, 10, QString(), QStringLiteral("MIS"), QStringLiteral("BSE"))}));
    QVERIFY(reduces(sell, {position(sym, 10, QString(), QStringLiteral("MIS"), QString())}));
    UnifiedOrder sell_no_exch = sell;
    sell_no_exch.exchange.clear();
    QVERIFY(reduces(sell_no_exch, {position(sym, 10, QString(), QStringLiteral("MIS"), QStringLiteral("BSE"))}));

    // Shorts: a signed negative quantity, or an unsigned one with side "SELL".
    QVERIFY(reduces(buy, {position(sym, -10)}));
    QVERIFY(!reduces(sell, {position(sym, -10)}));
    QVERIFY(reduces(buy, {position(sym, 10, QStringLiteral("SELL"))}));

    // Flat, other symbol, symbol spelled with spaces / lower case.
    QVERIFY(!reduces(sell, {position(sym, 0)}));
    QVERIFY(!reduces(sell, {position(QStringLiteral("TCS"), 10)}));
    QVERIFY(reduces(sell, {position(QStringLiteral(" reliance "), 10)}));
}

// ── parse / serialize_risk_settings() ────────────────────────────────────────

void TstOrderValidator::settings_parse_defaults_when_empty() {
    const RiskSettings s = OrderValidator::parse_risk_settings({});
    QCOMPARE(s.max_order_value, 0.0);
    QCOMPARE(s.max_order_quantity, 0.0);
    QCOMPARE(s.daily_loss_limit, 0.0);
    QVERIFY(s.sanity_checks); // the only check that is on by default
    QVERIFY(s.blocked_symbols.isEmpty());
    QVERIFY(s.limits_currency.isEmpty()); // each account's own currency
}

void TstOrderValidator::settings_parse_values_and_sanitising() {
    using Rows = QVector<QPair<QString, QString>>;
    RiskSettings s = OrderValidator::parse_risk_settings(Rows{
        {QStringLiteral("risk.max_order_value"), QStringLiteral(" 250000.50 ")},
        {QStringLiteral("risk.max_order_quantity"), QStringLiteral("500")},
        {QStringLiteral("risk.daily_loss_limit"), QStringLiteral("-7500")}, // stored as a positive amount
        {QStringLiteral("risk.sanity_checks"), QStringLiteral("0")},
        {QStringLiteral("risk.blocked_symbols"), QStringLiteral(" yesbank, nse:idea ,,  ")},
        {QStringLiteral("risk.unknown_key"), QStringLiteral("9")},
        {QStringLiteral("max_order_value"), QStringLiteral("1")}, // not a risk.* key
    });
    QCOMPARE(s.max_order_value, 250000.5);
    QCOMPARE(s.max_order_quantity, 500.0);
    QCOMPARE(s.daily_loss_limit, 7500.0);
    QVERIFY(!s.sanity_checks);
    QCOMPARE(s.blocked_symbols, (QStringList{QStringLiteral("YESBANK"), QStringLiteral("NSE:IDEA")}));

    // Negative caps, garbage and non-finite text all read as 0 = off.
    for (const QString& bad : {QStringLiteral("-100"), QStringLiteral("abc"), QStringLiteral("nan"),
                               QStringLiteral("inf"), QStringLiteral("-inf"), QString()}) {
        s = OrderValidator::parse_risk_settings(Rows{{QStringLiteral("risk.max_order_value"), bad},
                                                     {QStringLiteral("risk.max_order_quantity"), bad}});
        QCOMPARE(s.max_order_value, 0.0);
        QCOMPARE(s.max_order_quantity, 0.0);
        if (bad != QLatin1String("-100")) {
            s = OrderValidator::parse_risk_settings(Rows{{QStringLiteral("risk.daily_loss_limit"), bad}});
            QCOMPARE(s.daily_loss_limit, 0.0);
        }
    }

    // The sanity guard is off only for an explicit "0" (whitespace ignored).
    for (const QString& on : {QStringLiteral("1"), QString(), QStringLiteral("yes"), QStringLiteral("00")})
        QVERIFY(OrderValidator::parse_risk_settings(Rows{{QStringLiteral("risk.sanity_checks"), on}}).sanity_checks);
    QVERIFY(!OrderValidator::parse_risk_settings(Rows{{QStringLiteral("risk.sanity_checks"), QStringLiteral(" 0 ")}})
                 .sanity_checks);
}

void TstOrderValidator::settings_round_trip() {
    RiskSettings s;
    s.max_order_value = 250000.5;
    s.max_order_quantity = 500;
    s.daily_loss_limit = 7500.25;
    s.sanity_checks = false;
    s.blocked_symbols = {QStringLiteral("YESBANK"), QStringLiteral("NSE:IDEA")};
    s.limits_currency = QStringLiteral("USD");

    const auto rows = OrderValidator::serialize_risk_settings(s);
    QCOMPARE(rows.size(), qsizetype(6));
    // The stored text format the settings table has always used.
    QHash<QString, QString> by_key;
    for (const auto& r : rows)
        by_key.insert(r.first, r.second);
    QCOMPARE(by_key.value(QStringLiteral("risk.max_order_value")), QStringLiteral("250000.50"));
    QCOMPARE(by_key.value(QStringLiteral("risk.max_order_quantity")), QStringLiteral("500"));
    QCOMPARE(by_key.value(QStringLiteral("risk.daily_loss_limit")), QStringLiteral("7500.25"));
    QCOMPARE(by_key.value(QStringLiteral("risk.sanity_checks")), QStringLiteral("0"));
    QCOMPARE(by_key.value(QStringLiteral("risk.blocked_symbols")), QStringLiteral("YESBANK,NSE:IDEA"));
    QCOMPARE(by_key.value(QStringLiteral("risk.limits_currency")), QStringLiteral("USD"));

    const RiskSettings back = OrderValidator::parse_risk_settings(rows);
    QCOMPARE(back.max_order_value, s.max_order_value);
    QCOMPARE(back.max_order_quantity, s.max_order_quantity);
    QCOMPARE(back.daily_loss_limit, s.daily_loss_limit);
    QCOMPARE(back.sanity_checks, s.sanity_checks);
    QCOMPARE(back.blocked_symbols, s.blocked_symbols);
    QCOMPARE(back.limits_currency, s.limits_currency);

    // The defaults survive a round trip unchanged (sanity stays on).
    const auto default_rows = OrderValidator::serialize_risk_settings({});
    const RiskSettings d = OrderValidator::parse_risk_settings(default_rows);
    QCOMPARE(d.max_order_value, 0.0);
    QCOMPARE(d.max_order_quantity, 0.0);
    QCOMPARE(d.daily_loss_limit, 0.0);
    QVERIFY(d.sanity_checks);
    QVERIFY(d.blocked_symbols.isEmpty());
    QVERIFY(d.limits_currency.isEmpty());
    // "Each account's own currency" is written too (an empty row), so choosing it
    // again clears a currency saved earlier.
    bool has_ccy_row = false;
    for (const auto& row : default_rows)
        has_ccy_row = has_ccy_row || (row.first == QLatin1String("risk.limits_currency") && row.second.isEmpty());
    QVERIFY(has_ccy_row);
}

void TstOrderValidator::settings_limits_currency() {
    using Rows = QVector<QPair<QString, QString>>;
    auto parsed = [](const QString& text) {
        return OrderValidator::parse_risk_settings(Rows{{QStringLiteral("risk.limits_currency"), text}}).limits_currency;
    };
    QCOMPARE(parsed(QStringLiteral(" usd ")), QStringLiteral("USD"));
    QCOMPARE(parsed(QStringLiteral("GBp")), QStringLiteral("GBP")); // a limit is never in pence
    QCOMPARE(parsed(QStringLiteral("usdt")), QStringLiteral("USDT"));
    QCOMPARE(parsed(QStringLiteral("FDUSD")), QStringLiteral("FDUSD"));
    // Anything else reads as "each account's own currency".
    for (const QString& bad : {QString(), QStringLiteral("EU"), QStringLiteral("US$"), QStringLiteral("123"),
                               QStringLiteral("ABCDEF"), QStringLiteral("€")})
        QVERIFY2(parsed(bad).isEmpty(), qPrintable(bad));
}

QTEST_GUILESS_MAIN(TstOrderValidator)
#include "tst_order_validator.moc"
