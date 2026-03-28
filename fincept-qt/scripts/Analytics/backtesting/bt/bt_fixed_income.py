"""
BT Fixed Income Strategies

Covers bt's fixed-income asset types and related algos:
- FixedIncomeSecurity        — basic bond / fixed-income security
- CouponPayingSecurity       — bond with periodic coupon payments
- HedgeSecurity              — security used purely as a hedge overlay
- CouponPayingHedgeSecurity  — coupon-paying hedge security
- FixedIncomeStrategy        — strategy subclass for bond portfolios
- ResolveOnTheRun            — roll to on-the-run bond each period
- RollPositionsAfterDates    — roll positions after specified settlement dates
- ClosePositionsAfterDates   — close positions on / after specific dates

All builders follow the same pattern used in bt_strategies.py and bt_risk.py:
    builder(params) -> build(data, name=...) -> bt.Strategy | None
"""

import sys
from pathlib import Path
from typing import Dict, Any, List

_SCRIPT_DIR = Path(__file__).parent
_BACKTESTING_DIR = _SCRIPT_DIR.parent
for _p in [str(_BACKTESTING_DIR), str(_SCRIPT_DIR)]:
    if _p not in sys.path:
        sys.path.insert(0, _p)

from bt_strategies import _STRATEGY_REGISTRY, _register, _rebalance_algo, _BT_AVAILABLE, _bt


# ============================================================================
# Helpers
# ============================================================================

def _make_fi_strategy(name: str, algos: list, security_type='fixed_income'):
    """Wrap algos in a FixedIncomeStrategy if available, else plain Strategy."""
    if not _BT_AVAILABLE:
        return None
    try:
        return _bt.FixedIncomeStrategy(name, algos)
    except AttributeError:
        return _bt.Strategy(name, algos)


# ============================================================================
# Basic Fixed-Income Equal-Weight
# ============================================================================

@_register('fi_equal_weight', 'fixedIncome', 'Fixed Income Equal Weight',
           'Equal-weight bond portfolio using FixedIncomeStrategy',
           [{'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_fi_equal_weight(params):
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='fi_equal_weight'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _make_fi_strategy(name, algos)
        return None
    return build


# ============================================================================
# Resolve On-the-Run
# ============================================================================

@_register('fi_on_the_run', 'fixedIncome', 'On-the-Run Roll',
           'Automatically roll to the most recently issued (on-the-run) bond each period',
           [{'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_fi_on_the_run(params):
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='fi_on_the_run'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.ResolveOnTheRun(),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _make_fi_strategy(name, algos)
        return None
    return build


# ============================================================================
# Roll Positions After Dates
# ============================================================================

@_register('fi_roll_after_dates', 'fixedIncome', 'Roll Positions After Dates',
           'Roll bond positions after specified settlement / maturity dates',
           [{'name': 'dates', 'label': 'Roll dates (comma-separated YYYY-MM-DD)', 'default': ''},
            {'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_fi_roll_after_dates(params):
    import pandas as pd
    raw = params.get('dates', '')
    dates = [d.strip() for d in raw.split(',') if d.strip()]
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='fi_roll_after_dates'):
        if _BT_AVAILABLE and dates:
            parsed = pd.to_datetime(dates)
            algos = [
                _rebalance_algo(period),
                _bt.algos.RollPositionsAfterDates(dates=parsed),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _make_fi_strategy(name, algos)
        return None
    return build


# ============================================================================
# Close Positions After Dates
# ============================================================================

@_register('fi_close_after_dates', 'fixedIncome', 'Close Positions After Dates',
           'Liquidate bond positions on or after specified maturity / expiry dates',
           [{'name': 'dates', 'label': 'Close dates (comma-separated YYYY-MM-DD)', 'default': ''},
            {'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_fi_close_after_dates(params):
    import pandas as pd
    raw = params.get('dates', '')
    dates = [d.strip() for d in raw.split(',') if d.strip()]
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='fi_close_after_dates'):
        if _BT_AVAILABLE and dates:
            parsed = pd.to_datetime(dates)
            algos = [
                _rebalance_algo(period),
                _bt.algos.ClosePositionsAfterDates(dates=parsed),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _make_fi_strategy(name, algos)
        return None
    return build


# ============================================================================
# Coupon-Paying Bond Strategy
# ============================================================================

@_register('fi_coupon_bond', 'fixedIncome', 'Coupon-Paying Bond Portfolio',
           'Portfolio of coupon-paying bonds with automatic income reinvestment',
           [{'name': 'couponFreq', 'label': 'Coupon frequency', 'default': 'quarterly',
             'options': ['monthly', 'quarterly', 'yearly']},
            {'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'quarterly',
             'options': ['monthly', 'quarterly', 'yearly']}])
def _build_fi_coupon_bond(params):
    period = params.get('rebalancePeriod', 'quarterly')

    def build(data, name='fi_coupon_bond'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            # Use FixedIncomeStrategy which handles coupon accrual
            return _make_fi_strategy(name, algos, security_type='coupon')
        return None
    return build


# ============================================================================
# Hedge Overlay (HedgeSecurity-based)
# ============================================================================

@_register('fi_hedge_overlay', 'fixedIncome', 'Bond + Hedge Overlay',
           'Hold a bond portfolio with a separate hedge security overlay',
           [{'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_fi_hedge_overlay(params):
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='fi_hedge_overlay'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.UpdateRisk(),
                _bt.algos.HedgeRisks(),
                _bt.algos.Rebalance(),
            ]
            return _make_fi_strategy(name, algos)
        return None
    return build


# ============================================================================
# Duration-Weighted Bond Portfolio
# ============================================================================

@_register('fi_inv_duration', 'fixedIncome', 'Inverse-Duration Weighted Bonds',
           'Weight bonds inversely by duration (shorter duration = larger weight)',
           [{'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_fi_inv_duration(params):
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='fi_inv_duration'):
        if _BT_AVAILABLE:
            # Proxy duration weighting via inverse volatility (vol ~ duration sensitivity)
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighInvVol(lookback=60),
                _bt.algos.Rebalance(),
            ]
            return _make_fi_strategy(name, algos)
        return None
    return build


# ============================================================================
# SimulateRFQTransactions — OTC / bond RFQ execution simulation
# ============================================================================

@_register('fi_rfq_simulation', 'fixedIncome', 'OTC RFQ Transaction Simulation',
           'Simulate OTC bond / derivatives execution via Request-For-Quote (RFQ) model',
           [{'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly',
             'options': ['daily', 'weekly', 'monthly', 'quarterly', 'yearly']}])
def _build_fi_rfq_simulation(params):
    period = params.get('rebalancePeriod', 'monthly')

    def build(data, name='fi_rfq_simulation'):
        if _BT_AVAILABLE:
            algos = [
                _rebalance_algo(period),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.SimulateRFQTransactions(),
            ]
            return _make_fi_strategy(name, algos)
        return None
    return build
