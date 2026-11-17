#pragma once
// PaperBookSymbol.h — which row of a local paper book (pt_get_positions) a symbol means.
//
// The paper engine (PaperTrading) keeps one position per exact symbol string and nets an
// order only against the row stored under exactly the order's symbol. A row is stored under
// the BARE symbol ("RELIANCE": the account path, UnifiedTrading::place_paper_order_for_account,
// which the Equity screen and every account order use) or, from the session path and older
// smart orders, as "EXCHANGE:SYMBOL" ("NSE:RELIANCE").
//
// A close or a flatten therefore finds the position under either spelling — the rule the
// Equity screen's exit has always used (UnifiedTrading::close_position) — and then acts on
// the row's OWN symbol string, so its order nets against that row:
//   close_position ......... the Equity screen / blotter exit
//   place_smart_order ...... a smart order / workflow flatten on a local-paper account
// A plain order (place_order) is booked under its own symbol, so it can only ever reduce the
// row stored under exactly that symbol: the approval gate's reduce-only check verifies that
// row (UnifiedTrading.cpp, ut_check_reduce_only) and only uses this rule to say why a row in
// the other spelling does not count.
//
// Qt Core only; no state. Unit-tested by tests/tst_paper_book_symbol.cpp.

#include <QString>

namespace fincept::trading::paper_book {

/// True when a paper-book row stored under `stored` is the position of `symbol` on
/// `exchange`: the bare symbol, or "EXCHANGE:SYMBOL". Exact, case-sensitive strings — the
/// engine's own spelling, never a substring ("RELIANCE" is not "RELIANCEPP").
inline bool symbol_matches(const QString& stored, const QString& symbol, const QString& exchange) {
    return stored == symbol || stored == exchange + QLatin1Char(':') + symbol;
}

} // namespace fincept::trading::paper_book
