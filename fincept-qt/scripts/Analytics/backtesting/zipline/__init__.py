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
"""

from .zipline_provider import (
    ZiplineProvider,
    # Execution styles
    MarketOrder,
    LimitOrder,
    StopOrder,
    StopLimitOrder,
    # Commission models
    PerShare,
    PerTrade,
    PerDollar,
    NoCommission,
    # Slippage models
    FixedSlippage,
    VolumeShareSlippage,
    NoSlippage,
    FixedBasisPointsSlippage,
    # Cancel policies
    EODCancel,
    NeverCancel,
    # Asset restrictions
    NoRestrictions,
    StaticRestrictions,
    HistoricalRestrictions,
    # Asset classes
    Asset,
    Equity,
    Future,
    ContinuousFuture,
    # Bundle management
    register_bundle,
    ingest_bundle,
    load_bundle,
    clean_bundle,
    unregister_bundle,
    bundles,
    # Scheduling
    date_rules,
    time_rules,
)

from .zl_pipeline import (
    # Pipeline core
    Pipeline,
    CustomFactor,
    CustomFilter,
    CustomClassifier,
    # Base classes
    Term,
    Factor,
    Filter,
    Classifier,
    # Data
    Column,
    BoundColumn,
    DataSet,
    DataSetFamily,
    EquityPricing,
    USEquityPricing,
    # Built-in factors
    Latest,
    AverageDollarVolume,
    BollingerBands,
    DailyReturns,
    Returns,
    SimpleMovingAverage,
    ExponentialWeightedMovingAverage,
    ExponentialWeightedMovingStdDev,
    VWAP,
    RSI,
    MACDSignal,
    MaxDrawdown,
    AnnualizedVolatility,
    PercentChange,
    RateOfChangePercentage,
    TrueRange,
    Aroon,
    FastStochasticOscillator,
    IchimokuKinkoHyo,
    SimpleBeta,
    RollingPearsonOfReturns,
    RollingSpearmanOfReturns,
    RollingLinearRegressionOfReturns,
    WeightedAverageValue,
    PeerCount,
    # Built-in filters
    StaticAssets,
    StaticSids,
    AllPresent,
    AtLeastN,
    SingleAsset,
    # Classifiers
    Quantiles,
    Everything,
    # Domains
    GENERIC,
    US_EQUITIES,
    UK_EQUITIES,
    CA_EQUITIES,
    DE_EQUITIES,
    FR_EQUITIES,
    JP_EQUITIES,
    KR_EQUITIES,
    HK_EQUITIES,
    CN_EQUITIES,
)

__all__ = [
    # Provider
    'ZiplineProvider',
    # Execution styles
    'MarketOrder', 'LimitOrder', 'StopOrder', 'StopLimitOrder',
    # Commission models
    'PerShare', 'PerTrade', 'PerDollar', 'NoCommission',
    # Slippage models
    'FixedSlippage', 'VolumeShareSlippage', 'NoSlippage', 'FixedBasisPointsSlippage',
    # Cancel policies
    'EODCancel', 'NeverCancel',
    # Asset restrictions
    'NoRestrictions', 'StaticRestrictions', 'HistoricalRestrictions',
    # Asset classes
    'Asset', 'Equity', 'Future', 'ContinuousFuture',
    # Bundle management
    'register_bundle', 'ingest_bundle', 'load_bundle', 'clean_bundle',
    'unregister_bundle', 'bundles',
    # Scheduling
    'date_rules', 'time_rules',
    # Pipeline core
    'Pipeline', 'CustomFactor', 'CustomFilter', 'CustomClassifier',
    'Term', 'Factor', 'Filter', 'Classifier',
    # Pipeline data
    'Column', 'BoundColumn', 'DataSet', 'DataSetFamily',
    'EquityPricing', 'USEquityPricing',
    # Built-in factors
    'Latest', 'AverageDollarVolume', 'BollingerBands', 'DailyReturns',
    'Returns', 'SimpleMovingAverage', 'ExponentialWeightedMovingAverage',
    'ExponentialWeightedMovingStdDev', 'VWAP', 'RSI', 'MACDSignal',
    'MaxDrawdown', 'AnnualizedVolatility', 'PercentChange',
    'RateOfChangePercentage', 'TrueRange', 'Aroon', 'FastStochasticOscillator',
    'IchimokuKinkoHyo', 'SimpleBeta', 'RollingPearsonOfReturns',
    'RollingSpearmanOfReturns', 'RollingLinearRegressionOfReturns',
    'WeightedAverageValue', 'PeerCount',
    # Built-in filters
    'StaticAssets', 'StaticSids', 'AllPresent', 'AtLeastN', 'SingleAsset',
    # Classifiers
    'Quantiles', 'Everything',
    # Domains
    'GENERIC', 'US_EQUITIES', 'UK_EQUITIES', 'CA_EQUITIES',
    'DE_EQUITIES', 'FR_EQUITIES', 'JP_EQUITIES', 'KR_EQUITIES',
    'HK_EQUITIES', 'CN_EQUITIES',
]
