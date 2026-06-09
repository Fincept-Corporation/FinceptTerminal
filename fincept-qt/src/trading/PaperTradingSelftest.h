#pragma once
// Headless self-test of the paper-trading engine (no GUI / network / broker /
// LLM). Runs on a throwaway portfolio and asserts the core invariants that the
// FNO + Equity paper-trading bugs violated: long/short mark-to-market P&L, the
// zero-tick phantom-profit guard (a 0/garbage quote on a short must NOT book the
// full premium as profit), square-off exit + realized P&L, opposite-fill netting
// (no phantom reverse position), multi-leg fills, and margin lock/release.
//
// Run headless:  QT_QPA_PLATFORM=offscreen FinceptTerminal --selftest-paper
// Returns 0 when every assertion passes, 1 otherwise (CI / dev-loop gate).

namespace fincept::trading {

int run_paper_trading_selftest();

} // namespace fincept::trading
