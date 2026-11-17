"""
"VectorBT" engine of the Fincept backtesting screen - rebased on the shared core (base/).
The vectorbt LIBRARY is not used; every result says what ran (engine / engineImplementation).

Modules in use:
- vectorbt_provider: CLI entry point (<command> --stdin), command dispatch
- vbt_engine:        run_backtest / optimize / walk_forward on base.price_store + base.simulator +
                     base.robustness + base.result_normalizer
- vbt_evaluate:      module-level evaluate() for base.robustness (spawn-pool safe)
- vbt_strategies:    the 23 catalogue strategies (signal builders + allocation) and their specs
- vbt_indicators:    the ONE indicator id table and memoised, causal indicators
- vbt_research:      calculate_indicator, indicator_signals, indicator_param_sweep, generate_signals,
                     generate_labels, labels_to_signals, generate_splits, analyze_returns
- vbt_signals, vbt_labels, vbt_splitters, vbt_generic: helpers used by the research commands

No longer used (kept until the owner removes them): vbt_portfolio, vbt_metrics, vbt_optimization,
vbt_returns - the pre-rebase per-bar simulator, metrics and optimiser.

The provider class is intentionally not re-exported here (avoids RuntimeWarning when
vectorbt_provider is run via `python -m`).
"""
