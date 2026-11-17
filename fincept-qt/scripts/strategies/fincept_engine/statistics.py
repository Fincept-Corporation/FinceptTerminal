# ============================================================================
# Fincept Terminal - Strategy Engine: algorithm statistics (LEAN self.statistics)
#
# self.statistics.summary is LEAN's summary statistics dictionary (string values keyed by
# PerformanceMetrics names), computed on demand from the run so far: the daily equity points
# the loop records and the broker's fills. Custom entries from set_summary_statistic are merged.
# ============================================================================

import math


class PerformanceMetrics:
    ALPHA = "Alpha"
    ANNUAL_STANDARD_DEVIATION = "Annual Standard Deviation"
    ANNUAL_VARIANCE = "Annual Variance"
    AVERAGE_LOSS = "Average Loss"
    AVERAGE_WIN = "Average Win"
    BETA = "Beta"
    COMPOUNDING_ANNUAL_RETURN = "Compounding Annual Return"
    DRAWDOWN = "Drawdown"
    ESTIMATED_STRATEGY_CAPACITY = "Estimated Strategy Capacity"
    EXPECTANCY = "Expectancy"
    START_EQUITY = "Start Equity"
    END_EQUITY = "End Equity"
    INFORMATION_RATIO = "Information Ratio"
    LOSS_RATE = "Loss Rate"
    NET_PROFIT = "Net Profit"
    PROBABILISTIC_SHARPE_RATIO = "Probabilistic Sharpe Ratio"
    PROFIT_LOSS_RATIO = "Profit-Loss Ratio"
    SHARPE_RATIO = "Sharpe Ratio"
    SORTINO_RATIO = "Sortino Ratio"
    TOTAL_FEES = "Total Fees"
    TOTAL_ORDERS = "Total Orders"
    TRACKING_ERROR = "Tracking Error"
    TREYNOR_RATIO = "Treynor Ratio"
    WIN_RATE = "Win Rate"
    LOWEST_CAPACITY_ASSET = "Lowest Capacity Asset"
    PORTFOLIO_TURNOVER = "Portfolio Turnover"
    DRAWDOWN_RECOVERY = "Drawdown Recovery"


class _Pair:
    __slots__ = ('key', 'value')

    def __init__(self, k, v):
        self.key, self.value = k, v

    def __iter__(self):
        yield self.key
        yield self.value

    Key = property(lambda self: self.key)
    Value = property(lambda self: self.value)


class SummaryStatistics(dict):
    """dict of statistic name -> string; iterating yields key/value pairs as LEAN's dictionary does."""

    def __iter__(self):
        return iter([_Pair(k, v) for k, v in dict.items(self)])

    def keys(self):
        return list(dict.keys(self))

    def __contains__(self, key):
        return dict.__contains__(self, key)


class AlgorithmStatistics:
    def __init__(self, algorithm):
        self._algo = algorithm
        self._custom = {}

    def _equity(self):
        pts = getattr(self._algo, '_equity_points', None) or []
        return [p for p in pts if p > 0]

    @property
    def summary(self):
        a = self._algo
        broker = a._broker
        eq = self._equity()
        start = float(getattr(a.portfolio, '_initial_cash', 0.0) or 0.0)
        end = float(a.portfolio.total_portfolio_value)
        out = SummaryStatistics()
        n_orders = len([t for t in a.transactions._orders.values()])
        out[PerformanceMetrics.TOTAL_ORDERS] = str(n_orders)
        out[PerformanceMetrics.START_EQUITY] = f"{start:.2f}"
        out[PerformanceMetrics.END_EQUITY] = f"{end:.2f}"
        out[PerformanceMetrics.NET_PROFIT] = f"{(end / start - 1) * 100 if start else 0:.3f}%"
        out[PerformanceMetrics.TOTAL_FEES] = f"${(broker.total_fees if broker else 0.0):,.2f}"
        rets = [eq[i] / eq[i - 1] - 1 for i in range(1, len(eq)) if eq[i - 1]]
        sharpe = sortino = sd = 0.0
        if len(rets) > 1:
            m = sum(rets) / len(rets)
            var = sum((r - m) ** 2 for r in rets) / (len(rets) - 1)
            sd = math.sqrt(var) * math.sqrt(252)
            if var > 0:
                sharpe = m / math.sqrt(var) * math.sqrt(252)
            dvar = sum(min(r, 0) ** 2 for r in rets) / len(rets)
            if dvar > 0:
                sortino = m / math.sqrt(dvar) * math.sqrt(252)
        out[PerformanceMetrics.SHARPE_RATIO] = f"{sharpe:.3f}"
        out[PerformanceMetrics.SORTINO_RATIO] = f"{sortino:.3f}"
        out[PerformanceMetrics.ANNUAL_STANDARD_DEVIATION] = f"{sd:.3f}"
        out[PerformanceMetrics.ANNUAL_VARIANCE] = f"{sd * sd:.3f}"
        peak, dd = 0.0, 0.0
        for v in eq:
            peak = max(peak, v)
            if peak:
                dd = max(dd, 1 - v / peak)
        out[PerformanceMetrics.DRAWDOWN] = f"{dd * 100:.1f}%"
        years = max(1, len(eq) - 1) / 252.0
        car = ((end / start) ** (1 / years) - 1) if start and end > 0 and len(eq) > 1 else 0.0
        out[PerformanceMetrics.COMPOUNDING_ANNUAL_RETURN] = f"{car * 100:.3f}%"
        out.update(self._custom)
        return out

    @property
    def total_performance(self):
        from types import SimpleNamespace
        s = self.summary
        return SimpleNamespace(portfolio_statistics=s, trade_statistics=s, closed_trades=[])

    Summary = summary
    TotalPerformance = total_performance
