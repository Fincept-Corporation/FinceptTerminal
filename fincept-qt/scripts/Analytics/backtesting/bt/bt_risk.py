"""
BT Risk & Constraint Algos

Strategies and helpers that use bt's risk management algo blocks:
- TargetVol      — scale portfolio to hit a target annualised volatility
- LimitWeights   — cap any single asset weight
- LimitDeltas    — cap turnover / weight change per rebalance
- PTE_Rebalance  — rebalance when tracking error exceeds threshold
- CapitalFlow    — inject / withdraw capital on a schedule
- SetNotional    — fix portfolio notional value
- UpdateRisk     — compute per-asset risk metrics
- HedgeRisks     — hedge residual portfolio risk
- Margin         — apply margin / leverage constraints

All builders follow the same pattern as bt_strategies.py:
    builder(params) -> build(data, name=...) -> bt.Strategy | None
"""

import sys
from pathlib import Path
from typing import Dict, Any

_SCRIPT_DIR = Path(__file__).parent
_BACKTESTING_DIR = _SCRIPT_DIR.parent
for _p in [str(_BACKTESTING_DIR), str(_SCRIPT_DIR)]:
    if _p not in sys.path:
        sys.path.insert(0, _p)

from bt_strategies import _STRATEGY_REGISTRY, _register, _rebalance_algo, _BT_AVAILABLE, _bt


# ============================================================================
# TargetVol — scale to annualised volatility target
# ============================================================================

@_register('risk_target_vol', 'risk', 'Target Volatility',
           'Scale portfolio weights so realised vol matches a target',
           [{'name': 'targetVol', 'label': 'Target Ann. Vol (%)', 'default': 10,
             'min': 1, 'max': 50},
            {'name': 'lookback', 'label': 'Vol Lookback (days)', 'default': 60},
            {'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_risk_target_vol(params):
    target = float(params.get('targetVol', 10)) / 100.0
    lookback = int(params.get('lookback', 60))
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='risk_target_vol'):
        if _BT_AVAILABLE:
            import pandas as pd
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.TargetVol(
                    target=target,
                    lookback=pd.DateOffset(days=lookback),
                    annualization_factor=252,
                ),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


# ============================================================================
# LimitWeights — cap maximum single-asset weight
# ============================================================================

@_register('risk_limit_weights', 'risk', 'Limit Max Weight',
           'Cap each asset weight at a maximum fraction',
           [{'name': 'maxWeight', 'label': 'Max Weight (fraction)', 'default': 0.2,
             'min': 0.01, 'max': 1.0},
            {'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_risk_limit_weights(params):
    max_w = float(params.get('maxWeight', 0.2))
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='risk_limit_weights'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.LimitWeights(limit=max_w),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


# ============================================================================
# LimitDeltas — cap turnover per rebalance
# ============================================================================

@_register('risk_limit_deltas', 'risk', 'Limit Weight Changes (Turnover Control)',
           'Restrict how much any weight can change in a single rebalance',
           [{'name': 'maxDelta', 'label': 'Max Weight Delta', 'default': 0.1,
             'min': 0.01, 'max': 1.0},
            {'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_risk_limit_deltas(params):
    max_d = float(params.get('maxDelta', 0.1))
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='risk_limit_deltas'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.LimitDeltas(limit=max_d),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


# ============================================================================
# PTE_Rebalance — rebalance when tracking error threshold is breached
# ============================================================================

@_register('risk_pte_rebalance', 'risk', 'PTE Tracking-Error Rebalance',
           'Trigger rebalance only when portfolio tracking error exceeds threshold',
           [{'name': 'pteThreshold', 'label': 'PTE Threshold', 'default': 0.02,
             'min': 0.001, 'max': 0.5},
            {'name': 'lookback', 'label': 'Lookback (days)', 'default': 60}])
def _build_risk_pte_rebalance(params):
    pte = float(params.get('pteThreshold', 0.02))
    lookback = int(params.get('lookback', 60))

    def build(data, name='risk_pte_rebalance'):
        if _BT_AVAILABLE:
            import pandas as pd
            algos = [
                _bt.algos.PTE_Rebalance(
                    PTE_volatility_target=pte,
                    lookback=pd.DateOffset(days=lookback),
                ),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


# ============================================================================
# CapitalFlow — inject / withdraw capital on a schedule
# ============================================================================

@_register('risk_capital_flow', 'risk', 'Scheduled Capital Flow',
           'Inject or withdraw a fixed cash amount each period',
           [{'name': 'amount', 'label': 'Flow amount (positive=inject)', 'default': 0.0},
            {'name': 'rebalancePeriod', 'label': 'Flow Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_risk_capital_flow(params):
    amount = float(params.get('amount', 0.0))
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='risk_capital_flow'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.CapitalFlow(amount),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


# ============================================================================
# SetNotional — fix notional portfolio value
# ============================================================================

@_register('risk_set_notional', 'risk', 'Fixed Notional Value',
           'Pin portfolio notional to a fixed dollar amount each rebalance',
           [{'name': 'notional', 'label': 'Notional ($)', 'default': 100000.0, 'min': 1.0},
            {'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_risk_set_notional(params):
    notional = float(params.get('notional', 100000.0))
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='risk_set_notional'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.SetNotional(notional_value=notional),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


# ============================================================================
# Combined: TargetVol + LimitWeights (common real-world combo)
# ============================================================================

@_register('risk_vol_capped', 'risk', 'Target Vol + Weight Cap',
           'Target volatility with per-asset weight ceiling',
           [{'name': 'targetVol', 'label': 'Target Ann. Vol (%)', 'default': 10,
             'min': 1, 'max': 50},
            {'name': 'maxWeight', 'label': 'Max Weight', 'default': 0.25,
             'min': 0.05, 'max': 1.0},
            {'name': 'lookback', 'label': 'Vol Lookback (days)', 'default': 60},
            {'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_risk_vol_capped(params):
    target = float(params.get('targetVol', 10)) / 100.0
    max_w = float(params.get('maxWeight', 0.25))
    lookback = int(params.get('lookback', 60))
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='risk_vol_capped'):
        if _BT_AVAILABLE:
            import pandas as pd
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighInvVol(lookback=lookback),
                _bt.algos.LimitWeights(limit=max_w),
                _bt.algos.TargetVol(
                    target=target,
                    lookback=pd.DateOffset(days=lookback),
                    annualization_factor=252,
                ),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


# ============================================================================
# UpdateRisk + HedgeRisks — risk attribution and hedging pipeline
# ============================================================================

@_register('risk_hedge', 'risk', 'Risk Attribution + Hedge',
           'Compute per-asset risk metrics then hedge residual portfolio risk',
           [{'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_risk_hedge(params):
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='risk_hedge'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.UpdateRisk(),
                _bt.algos.HedgeRisks(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


# ============================================================================
# Margin / Leverage
# ============================================================================

@_register('risk_margin', 'risk', 'Leveraged Portfolio',
           'Apply margin/leverage to an equal-weight strategy',
           [{'name': 'leverage', 'label': 'Leverage (1=no leverage)', 'default': 1.0,
             'min': 1.0, 'max': 5.0},
            {'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_risk_margin(params):
    leverage = float(params.get('leverage', 1.0))
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='risk_margin'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.ScaleWeights(leverage),
                _bt.algos.Margin(ratio=leverage),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build
