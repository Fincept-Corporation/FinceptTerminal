"""
"zipline" backtesting engine of the Fincept Terminal (engine slug "zipline").

What it is (every result says the same in engine / engineImplementation / assumptions):
  strategies are zipline-STYLE algorithms - initialize(context) / handle_data(context, data) /
  schedule_function / order_target_percent (zl_api.py, zl_strategies.py) - that read full-history
  indicators and record their orders; base.simulator executes the orders with the shared cost model
  (zl_engine.py); optimise / walk-forward run through base.robustness (zl_evaluate.py); research
  commands are the shared Fincept research tools (vectorbt\\vbt_research.py).
  zipline-reloaded is NOT used (see zipline_provider.py for why).

Entry point: zipline_provider.py <command> --stdin. zl_data.py and zl_metrics.py are v1 modules that
nothing imports any more (yfinance downloads with UTC-shifted dates; v1 metrics).

The provider is intentionally not imported here (running it with `python -m` would otherwise warn).
"""
