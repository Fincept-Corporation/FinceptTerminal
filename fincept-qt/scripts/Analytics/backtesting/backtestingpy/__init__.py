"""
"Backtesting.py" engine of the Fincept backtesting screen - rebased on the shared core (base/).
Every run is executed by the backtesting.py LIBRARY; every result says so (engine /
engineImplementation / provenance.library + libraryVersion).

Modules:
- backtestingpy_provider: CLI entry point (<command> --stdin), command dispatch
- btp_engine:     request parsing, price-store data, run_backtest / optimize / walk_forward on
                  base.robustness + base.result_normalizer
- btp_executor:   the backtesting.py side - library loader (plotting stubbed), SignalExecutor
                  (the Strategy that trades the catalogue signals), FinceptBroker hooks (slippage on
                  every fill, borrow, fill-price stops, fill log), FinceptBacktest, custom code sandbox
- btp_evaluate:   module-level evaluate() for base.robustness (spawn-pool safe)
- btp_strategies: the 27 catalogue strategies (vectorised causal signals) and their specs
- btp_indicators: the indicator table of get_indicators / calculate_indicator

The provider is intentionally not re-exported here (avoids a RuntimeWarning when
backtestingpy_provider is run via `python -m`).
"""
