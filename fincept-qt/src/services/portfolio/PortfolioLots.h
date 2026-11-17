// src/services/portfolio/PortfolioLots.h
//
// Ledger replay: tax lots, realised gains, splits, income, fees, cash and the
// external flows behind time- and money-weighted returns. Pure logic — Qt Core
// only, no repository, no service — so it is unit-tested on its own
// (tests/tst_portfolio_lots.cpp) and safe on any thread.
//
// Conventions (see also namespace txn in PortfolioTypes.h)
//   * Every money field of a row is in that row's `currency` (minor units allowed);
//     an empty currency means the instrument's quoted currency (LedgerOptions::
//     instrument_currency) for instrument rows and the base currency for cash rows.
//     Internally everything is held in the MAJOR unit (pence -> pounds).
//   * Lot cost includes the purchase's fees and taxes; sale proceeds are net of the
//     sale's fees and taxes (allocated pro rata across the lots it consumes).
//   * Base-currency figures use the row's own fx_rate when recorded, else
//     LedgerOptions::historical_fx(date); when neither is known the base figure is
//     NaN (absent) — never the spot rate, never 0.
//   * Same-day ordering: SPLIT first (it is effective at the open of its ex-date),
//     then rows in the order they were entered (created_at, then input order).
//   * A SELL beyond the quantity the ledger shows (a position that pre-dates its
//     ledger) realises nothing for the unmatched part: its cost is unknown.
#pragma once

#include "screens/portfolio/PortfolioTypes.h"

#include <QDate>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::portfolio {

enum class LotMethod {
    Fifo,        ///< first in, first out
    Lifo,        ///< last in, first out
    Hifo,        ///< highest unit cost first (minimises realised gains)
    AverageCost, ///< pooled average cost; holding period of the consumed units taken FIFO
    SpecificLot, ///< the SELL row's lot_id names the lot; any remainder FIFO
};

/// "FIFO", "LIFO", "HIFO", "AVERAGE" (also "AVG", "WAC", "AVERAGE_COST"), "SPECIFIC"
/// (also "SPECIFIC_LOT", "SPEC"). Unknown / empty -> FIFO.
LotMethod lot_method_from_key(const QString& key);
/// Stable storage key: "FIFO", "LIFO", "HIFO", "AVERAGE", "SPECIFIC".
QString lot_method_key(LotMethod method);

struct TaxLot {
    QString lot_id; ///< id of the BUY row that opened the lot
    QString symbol;
    QString currency; ///< ISO major code of the purchase currency
    QDate open_date;
    double quantity = 0;          ///< units still open (split-adjusted)
    double original_quantity = 0; ///< units bought (split-adjusted)
    double cost_local = 0;        ///< remaining cost incl. fees/taxes, major units of `currency`
    double cost_base = kAbsent;   ///< remaining cost in base at the purchase-date FX; NaN when unknown
    double fx_rate = kAbsent;     ///< base units per 1 `currency` on the purchase date; NaN when unknown
    int holding_days = 0;         ///< open_date -> LedgerOptions::as_of
    bool long_term = false;       ///< holding_days > LedgerOptions::long_term_days
    double unit_cost_local() const { return quantity > 0 ? cost_local / quantity : 0.0; }
};

struct RealizedLot {
    QString sell_id; ///< the SELL row
    QString lot_id;  ///< the lot consumed
    QString symbol;
    QString currency; ///< ISO major (of the lot)
    QDate open_date;
    QDate close_date;
    double quantity = 0;
    double proceeds_local = 0; ///< net of the sale's fees/taxes, pro rata
    double cost_local = 0;
    double gain_local = 0;
    double proceeds_base = kAbsent; ///< at the sale-date FX
    double cost_base = kAbsent;     ///< at the purchase-date FX
    double gain_base = kAbsent;     ///< NaN when either FX is unknown
    int holding_days = 0;
    bool long_term = false;
};

struct LedgerPosition {
    QString symbol;
    QString currency; ///< ISO major of the lots ("" when no lot was ever opened)
    double quantity = 0;
    double cost_local = 0; ///< open lots; NaN when lots are in different currencies
    double cost_base = 0;  ///< open lots; NaN when any lot's FX is unknown
    bool mixed_currency = false;
    QVector<TaxLot> lots; ///< open lots, in purchase order
    QDate first_date;     ///< first BUY
    double realized_local = 0;  ///< matched sales
    double realized_base = 0;   ///< matched sales with known FX
    bool realized_base_complete = true;
    double dividends_local = 0; ///< net of withholding / fees
    double dividends_base = 0;  ///< net, known FX only
    double unmatched_sell_quantity = 0; ///< sold beyond what the ledger held (pre-ledger position)
};

/// One external flow for return calculations, in the BASE currency.
/// Positive = money into the book, negative = money out.
struct ExternalFlow {
    QDate date;
    double amount_base = kAbsent; ///< NaN when the row's FX is unknown
    QString type;                 ///< transaction type that caused it
    QString txn_id;
};

struct LedgerOptions {
    LotMethod method = LotMethod::Fifo;
    int long_term_days = 365;
    QString base_currency = QStringLiteral("USD"); ///< callers set the portfolio's (portfolio::base_currency_of)
    QDate as_of; ///< holding periods are measured to this date; invalid = today
    /// Quoted currency of an instrument (minor units allowed, "GBp") for rows with an
    /// empty currency. Default: core::market::currency_for_symbol().
    std::function<QString(const QString& symbol)> instrument_currency;
    /// Base units per ONE major unit of `ccy` on `date` (daily close, forward-filled).
    /// nullopt = unknown. Not needed when every row records fx_rate or the book is
    /// single-currency. Default: unknown for every foreign currency.
    std::function<std::optional<double>(const QString& ccy_major, const QDate& date)> historical_fx;
    /// See Portfolio::track_cash — decides which rows are external flows.
    bool track_cash = false;
};

struct LedgerResult {
    QHash<QString, LedgerPosition> positions; ///< every symbol seen (closed ones have quantity 0)
    QVector<RealizedLot> realized;            ///< chronological
    QHash<QString, double> cash;              ///< ISO major -> balance, major units
    QVector<ExternalFlow> flows;              ///< chronological; see external-flow rules in the .cpp

    double realized_base = 0;       ///< matched sales with known FX
    double realized_short_base = 0; ///< of which short-term lots
    double realized_long_base = 0;  ///< of which long-term lots
    bool realized_base_complete = true;
    double dividends_base = 0;   ///< net dividends, known FX
    double withholding_base = 0; ///< taxes withheld on dividends / interest
    double fees_base = 0;        ///< all fees (trades, dividends, FEE rows)
    double taxes_base = 0;       ///< all taxes (trades, withholding, TAX rows)
    double interest_base = 0;    ///< net interest
    int rows_used = 0;
    QStringList warnings; ///< unmatched sells, unknown FX, bad rows, unknown lot ids, ...
};

/// Replay the whole ledger (any order) under `opt`.
LedgerResult replay_ledger(const QVector<Transaction>& txns, const LedgerOptions& opt);

/// Units of `symbol` held at the START of `date` — every BUY/SELL dated before it,
/// with SPLITs dated on or before `date` applied (a split effective on the ex-date
/// gives the post-split entitlement). Used for dividend entitlement at the ex-date.
double quantity_held_before(const QVector<Transaction>& txns, const QString& symbol, const QDate& date);

/// Trade date of a row (first 10 characters, ISO); invalid when unparsable.
QDate ledger_row_date(const Transaction& t);

} // namespace fincept::portfolio
