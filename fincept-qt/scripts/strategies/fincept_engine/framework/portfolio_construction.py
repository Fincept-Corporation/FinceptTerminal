# ============================================================================
# Fincept Terminal - Portfolio optimisers and optimiser-based portfolio construction models
#
# Optimisers (LEAN IPortfolioOptimizer: optimize(historical_returns, expected_returns=None,
# covariance=None) -> weights):
#   MinimumVariancePortfolioOptimizer   - minimum variance, weights sum to 1, optional target return
#   UnconstrainedMeanVariancePortfolioOptimizer - inverse(covariance) x expected returns
#   RiskParityPortfolioOptimizer        - equal risk contributions (Spinu's convex formulation)
#   MaximumSharpeRatioPortfolioOptimizer - tangency portfolio
# Models: MeanVarianceOptimizationPortfolioConstructionModel, RiskParityPortfolioConstructionModel.
# Their inputs are daily rate-of-change series of each insight's security over `period` days
# computed from the run's real bars known at decision time (history()). If the optimiser does not
# converge, LEAN keeps equal weights; so does this engine.
# ============================================================================

import sys

from ..algorithm import EqualWeightingPortfolioConstructionModel, _insight_sign
from ..enums import Resolution


def _np():
    import numpy as np
    return np


def _frame(historical_returns):
    import pandas as pd
    if isinstance(historical_returns, pd.DataFrame):
        return historical_returns
    return pd.DataFrame(_np().asarray(historical_returns, dtype=float))


class MinimumVariancePortfolioOptimizer:
    def __init__(self, minimum_weight=-1.0, maximum_weight=1.0, target_return=0.0):
        self.minimum_weight = float(minimum_weight)
        self.maximum_weight = float(maximum_weight)
        self.target_return = float(target_return)

    def optimize(self, historical_returns, expected_returns=None, covariance=None):
        np = _np()
        from scipy.optimize import minimize
        df = _frame(historical_returns)
        n = df.shape[1]
        if n == 0:
            return np.array([])
        cov = np.asarray(covariance if covariance is not None else df.cov(), dtype=float)
        mu = np.asarray(expected_returns if expected_returns is not None else df.mean(), dtype=float)
        x0 = np.full(n, 1.0 / n)
        if not np.all(np.isfinite(cov)) or not np.all(np.isfinite(mu)):
            return x0
        cons = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0},
                {'type': 'eq', 'fun': lambda w: w @ mu - self.target_return}]
        res = minimize(lambda w: w @ cov @ w, x0, method='SLSQP', constraints=cons,
                       bounds=[(self.minimum_weight, self.maximum_weight)] * n)
        return res.x if res.success else x0

    Optimize = optimize


class UnconstrainedMeanVariancePortfolioOptimizer:
    def __init__(self, *args, **kwargs):
        pass

    def optimize(self, historical_returns, expected_returns=None, covariance=None):
        np = _np()
        df = _frame(historical_returns)
        cov = np.asarray(covariance if covariance is not None else df.cov(), dtype=float)
        mu = np.asarray(expected_returns if expected_returns is not None else df.mean(), dtype=float)
        return np.linalg.pinv(cov) @ mu

    Optimize = optimize


class MaximumSharpeRatioPortfolioOptimizer:
    def __init__(self, minimum_weight=-1.0, maximum_weight=1.0, risk_free_rate=0.0):
        self.minimum_weight = float(minimum_weight)
        self.maximum_weight = float(maximum_weight)
        self.risk_free_rate = float(risk_free_rate)

    def optimize(self, historical_returns, expected_returns=None, covariance=None):
        np = _np()
        from scipy.optimize import minimize
        df = _frame(historical_returns)
        n = df.shape[1]
        if n == 0:
            return np.array([])
        cov = np.asarray(covariance if covariance is not None else df.cov(), dtype=float)
        mu = np.asarray(expected_returns if expected_returns is not None else df.mean(), dtype=float)
        x0 = np.full(n, 1.0 / n)
        if not np.all(np.isfinite(cov)) or not np.all(np.isfinite(mu)):
            return x0

        def neg_sharpe(w):
            v = w @ cov @ w
            return -(w @ mu - self.risk_free_rate) / np.sqrt(v) if v > 0 else 0.0
        res = minimize(neg_sharpe, x0, method='SLSQP', constraints=[{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0}],
                       bounds=[(self.minimum_weight, self.maximum_weight)] * n)
        return res.x if res.success else x0

    Optimize = optimize


class RiskParityPortfolioOptimizer:
    """Weights whose risk contributions are equal (budget 1/n each): minimise
    1/2 w'Sw - sum(b_i log w_i) over w > 0, then scale to sum 1."""

    def __init__(self, minimum_weight=1e-05, maximum_weight=sys.float_info.max):
        self.minimum_weight = float(minimum_weight)
        self.maximum_weight = float(maximum_weight)

    def optimize(self, historical_returns, budget=None, covariance=None):
        np = _np()
        from scipy.optimize import minimize
        df = _frame(historical_returns)
        n = df.shape[1]
        if n == 0:
            return np.array([])
        cov = np.asarray(covariance if covariance is not None else df.cov(), dtype=float)
        b = np.asarray(budget, dtype=float) if budget is not None else np.full(n, 1.0 / n)
        x0 = np.full(n, 1.0 / n)
        if not np.all(np.isfinite(cov)):
            return x0
        res = minimize(lambda w: 0.5 * w @ cov @ w - b @ np.log(w), x0,
                       jac=lambda w: cov @ w - b / w, method='L-BFGS-B',
                       bounds=[(self.minimum_weight, min(self.maximum_weight, 1e6))] * n)
        w = res.x if res.success else x0
        s = np.sum(w)
        return w / s if s > 0 else x0

    Optimize = optimize


def _returns_frame(algorithm, symbols, lookback, period, resolution):
    """Rate of change over `lookback` daily bars of each symbol, the last `period` values complete at the
    algorithm's current time (the run's own bars; no look-ahead)."""
    import pandas as pd
    from ..enums import SecurityType
    cols = {}
    data = getattr(algorithm, '_data', None)
    now = algorithm.time
    for s in symbols:
        key = str(getattr(s, 'value', s)).upper()
        sec = algorithm.securities.get(key)
        sym = getattr(sec, 'symbol', s)
        st = getattr(sym, 'security_type', SecurityType.EQUITY)
        series = data.series(sym, st, getattr(sym, 'market', None)) if data is not None else None
        if series is None:
            continue
        n = series.count_known_at(now)
        lo = max(0, n - (period + lookback))
        closes = series.close[lo:n]
        dates = series.dates[lo:n]
        if len(closes) <= lookback:
            continue
        rets = [closes[i] / closes[i - lookback] - 1.0 for i in range(lookback, len(closes))]
        cols[key] = pd.Series(rets[-period:], index=pd.DatetimeIndex(dates[lookback:])[-period:])
    return pd.DataFrame(cols)


class _OptimizerPortfolioModel(EqualWeightingPortfolioConstructionModel):
    def __init__(self, rebalance=None, portfolio_bias=None, lookback=1, period=63, resolution=None,
                 optimizer=None):
        super().__init__(rebalance, portfolio_bias, resolution)
        self.lookback = int(lookback)
        self.period = int(period)
        self.resolution = resolution if resolution is not None else Resolution.DAILY
        self.optimizer = optimizer
        self._algorithm = None

    def create_targets(self, algorithm, insights):
        self._algorithm = algorithm
        return super().create_targets(algorithm, insights)

    def _weights(self, insights):
        symbols = list(dict.fromkeys(i.symbol for i in insights))
        returns = _returns_frame(self._algorithm, symbols, self.lookback, self.period, self.resolution)
        if returns.shape[1] == 0:
            return {}
        w = self.optimizer.optimize(returns, None, None)
        return dict(zip(returns.columns, list(w)))


class MeanVarianceOptimizationPortfolioConstructionModel(_OptimizerPortfolioModel):
    """LEAN MeanVarianceOptimizationPortfolioConstructionModel(rebalance, portfolio_bias, lookback,
    period, resolution, target_return, optimizer): the optimiser's weights for the active insights'
    securities (default MinimumVariancePortfolioOptimizer with the target return)."""

    def __init__(self, rebalance=None, portfolio_bias=None, lookback=1, period=63, resolution=None,
                 target_return=0.02, optimizer=None):
        from ..algorithm_imports import PortfolioBias as _PB
        bias = portfolio_bias if portfolio_bias is not None else _PB.LONG_SHORT
        lower = 0.0 if int(bias) == int(_PB.LONG) else -1.0
        upper = 0.0 if int(bias) == int(_PB.SHORT) else 1.0
        opt = optimizer or MinimumVariancePortfolioOptimizer(lower, upper, target_return)
        super().__init__(rebalance, bias, lookback, period, resolution, opt)

    def determine_target_percent(self, insights):
        out = {}
        if not insights:
            return out
        weights = self._weights(insights)
        bias = self._portfolio_bias
        for i in insights:
            w = float(weights.get(str(i.symbol).upper(), 0.0) or 0.0)
            if bias is not None and int(bias) != 2:            # not LONG_SHORT: drop the wrong side
                sign = 1 if w > 0 else (-1 if w < 0 else 0)
                if (int(bias) == 0 and sign < 0) or (int(bias) == 1 and sign > 0):
                    w = 0.0
            out[i] = w
        return out


class RiskParityPortfolioConstructionModel(_OptimizerPortfolioModel):
    """LEAN RiskParityPortfolioConstructionModel(rebalance, portfolio_bias, lookback, period, resolution,
    optimizer): long-only risk-parity weights for the active insights' securities."""

    def __init__(self, rebalance=None, portfolio_bias=None, lookback=1, period=252, resolution=None,
                 optimizer=None):
        from ..algorithm_imports import PortfolioBias as _PB
        if portfolio_bias is not None and int(portfolio_bias) == int(_PB.SHORT):
            raise ValueError("Long position must be allowed in RiskParityPortfolioConstructionModel.")
        super().__init__(rebalance, portfolio_bias, lookback, period, resolution,
                         optimizer or RiskParityPortfolioOptimizer())

    def determine_target_percent(self, insights):
        if not insights:
            return {}
        weights = self._weights(insights)
        return {i: float(weights.get(str(i.symbol).upper(), 0.0) or 0.0) for i in insights}
