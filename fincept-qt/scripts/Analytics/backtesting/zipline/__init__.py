"""
Zipline-Reloaded Backtesting Provider
Event-driven backtesting via zipline-reloaded

Comprehensive Zipline API coverage:
- Order types: order, order_value, order_percent, order_target, order_target_value, order_target_percent
- Execution styles: MarketOrder, LimitOrder, StopOrder, StopLimitOrder
- Commission models: PerShare, PerTrade, PerDollar, NoCommission
- Slippage models: FixedSlippage, VolumeShareSlippage, NoSlippage, FixedBasisPointsSlippage
- Cancel policies: EODCancel, NeverCancel
- Asset restrictions: StaticRestrictions, HistoricalRestrictions, NoRestrictions
- Trading controls: set_long_only, set_max_leverage, set_max_order_count, etc.
- Scheduling: schedule_function with date_rules and time_rules (including every_minute)
- Recording: record() for custom metric tracking
- Callbacks: before_trading_start, analyze
- Pipeline API: Pipeline, CustomFactor, CustomFilter, CustomClassifier, built-in factors/filters
- Benchmark: set_benchmark
- Order management: get_order, get_open_orders, cancel_order
- Asset types: Asset, Equity, Future, ContinuousFuture
- Asset lookup: symbol, symbols, sid, continuous_future, future_symbol
- Bundle management: register, ingest, load, clean, unregister

The provider class and its re-exports are intentionally not loaded here. See
backtestingpy __init__.py for the rationale (avoids RuntimeWarning when
zipline_provider is run via `python -m`). Import the symbols you need directly
from zipline_provider / zl_pipeline.
"""
