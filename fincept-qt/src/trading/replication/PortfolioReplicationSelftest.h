#pragma once
namespace fincept::trading::replication {
// Headless self-test for the pure replication planner. No GUI / DB / network /
// API keys. Returns 0 on success, 1 on any failed assertion.
int run_portfolio_replication_selftest();
}
