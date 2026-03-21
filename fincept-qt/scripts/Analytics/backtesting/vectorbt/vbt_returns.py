"""
VBT Returns Accessor Module

Full implementation of vectorbt's ReturnsAccessor with 30+ methods:

Core Returns:
  - total, annualized, cumulative, daily, annual

Rolling Methods:
  - rolling_total, rolling_annualized, rolling_annualized_volatility
  - rolling_sharpe_ratio, rolling_sortino_ratio, rolling_calmar_ratio
  - rolling_omega_ratio, rolling_information_ratio, rolling_downside_risk

Risk-Adjusted:
  - sharpe_ratio, sortino_ratio, calmar_ratio
  - omega_ratio, information_ratio
  - deflated_sharpe_ratio, downside_risk

Capture Ratios:
  - up_capture, down_capture, up_down_ratio
"""

import numpy as np
import pandas as pd
from typing import Optional


class ReturnsAccessor:
    """
    Pure numpy/pandas implementation of vectorbt's ReturnsAccessor.

    Accepts a return series and optional benchmark returns.
    All methods work without requiring vectorbt to be installed.
    """

    def __init__(
        self,
        returns: pd.Series,
        benchmark_rets: Optional[pd.Series] = None,
        freq: str = '1D',
        year_freq: int = 252,
    ):
        self._returns = returns.fillna(0.0)
        self._benchmark = benchmark_rets.fillna(0.0) if benchmark_rets is not None else None
        self._freq = freq
        self._ann_factor = year_freq

    @property
    def values(self) -> np.ndarray:
        return self._returns.values.astype(float)

    @property
    def index(self) -> pd.Index:
        return self._returns.index

    # ------------------------------------------------------------------
    # Core Returns
    # ------------------------------------------------------------------

    def total(self) -> float:
        """Total compounded return."""
        return float(np.prod(1 + self.values) - 1)

    def annualized(self) -> float:
        """Annualized return (CAGR)."""
        n = len(self.values)
        if n < 2:
            return 0.0
        total = self.total()
        years = n / self._ann_factor
        if years <= 0 or total <= -1:
            return 0.0
        return float((1 + total) ** (1 / years) - 1)

    def cumulative(self) -> pd.Series:
        """Cumulative return series."""
        cum = np.cumprod(1 + self.values) - 1
        return pd.Series(cum, index=self.index, name='Cumulative Returns')

    def daily(self) -> pd.Series:
        """Daily return series (same as input for daily data)."""
        return self._returns.copy()

    def annual(self) -> float:
        """Same as annualized()."""
        return self.annualized()

    # ------------------------------------------------------------------
    # Volatility
    # ------------------------------------------------------------------

    def annualized_volatility(self) -> float:
        """Annualized volatility."""
        if len(self.values) < 2:
            return 0.0
        return float(np.std(self.values, ddof=1) * np.sqrt(self._ann_factor))

    def downside_risk(self, min_acceptable_return: float = 0.0) -> float:
        """Annualized downside deviation."""
        diff = self.values - min_acceptable_return
        neg = diff[diff < 0]
        if len(neg) < 2:
            return 0.0
        return float(np.sqrt(np.mean(neg ** 2)) * np.sqrt(self._ann_factor))

    # ------------------------------------------------------------------
    # Risk-Adjusted Ratios
    # ------------------------------------------------------------------

    def sharpe_ratio(self, risk_free: float = 0.0) -> float:
        """Annualized Sharpe ratio."""
        vol = self.annualized_volatility()
        if vol < 1e-10:
            return 0.0
        excess = self.annualized() - risk_free
        return float(excess / vol)

    def sortino_ratio(self, risk_free: float = 0.0) -> float:
        """Annualized Sortino ratio."""
        dr = self.downside_risk()
        if dr < 1e-10:
            return 0.0
        return float((self.annualized() - risk_free) / dr)

    def calmar_ratio(self) -> float:
        """Calmar ratio = annualized return / max drawdown."""
        md = self.max_drawdown()
        if abs(md) < 1e-10:
            return 0.0
        return float(self.annualized() / abs(md))

    def omega_ratio(self, threshold: float = 0.0) -> float:
        """
        Omega ratio: probability-weighted gain-to-loss ratio.

        Omega = integral(1 - F(r)) for r > threshold / integral(F(r)) for r < threshold
        Simplified: sum of excess gains / sum of excess losses
        """
        excess = self.values - threshold
        gains = excess[excess > 0].sum()
        losses = -excess[excess < 0].sum()
        if losses < 1e-10:
            return 99.0
        return float(gains / losses)

    def information_ratio(self) -> float:
        """Information ratio vs benchmark."""
        if self._benchmark is None:
            return 0.0
        active = self.values - self._benchmark.values[:len(self.values)]
        if len(active) < 2:
            return 0.0
        tracking_error = float(np.std(active, ddof=1) * np.sqrt(self._ann_factor))
        if tracking_error < 1e-10:
            return 0.0
        active_return = float(np.mean(active) * self._ann_factor)
        return float(active_return / tracking_error)

    def deflated_sharpe_ratio(self, n_trials: int = 1) -> float:
        """
        Deflated Sharpe Ratio (Bailey & Lopez de Prado, 2014).

        Adjusts Sharpe for skewness, kurtosis, and multiple testing.
        """
        n = len(self.values)
        if n < 10:
            return 0.0

        sr = self.sharpe_ratio()
        skew = self._skewness()
        kurt = self._kurtosis()

        # Variance of Sharpe estimator
        sr_var = (1 - skew * sr + (kurt - 1) / 4 * sr ** 2) / n

        if sr_var <= 0:
            return sr

        # Expected max Sharpe under null (multiple testing)
        if n_trials > 1:
            e_max_sr = np.sqrt(2 * np.log(n_trials)) * (
                1 - np.euler_gamma / (2 * np.log(n_trials))
            )
        else:
            e_max_sr = 0.0

        # DSR = P(SR > E[max(SR)])
        z = (sr - e_max_sr) / np.sqrt(sr_var)
        # CDF approximation
        dsr = 0.5 * (1 + _erf(z / np.sqrt(2)))
        return float(dsr)

    # ------------------------------------------------------------------
    # Max Drawdown
    # ------------------------------------------------------------------

    def max_drawdown(self) -> float:
        """Maximum drawdown from cumulative returns."""
        cum = np.cumprod(1 + self.values)
        peak = np.maximum.accumulate(cum)
        dd = (cum - peak) / np.where(peak > 0, peak, 1.0)
        return float(np.min(dd)) if len(dd) > 0 else 0.0

    # ------------------------------------------------------------------
    # Rolling Methods
    # ------------------------------------------------------------------

    def rolling_total(self, window: int = 252) -> pd.Series:
        """Rolling total return over window."""
        cum = pd.Series(np.cumprod(1 + self.values), index=self.index)
        shifted = cum.shift(window)
        rolling = (cum / shifted.replace(0, np.nan)) - 1
        rolling.name = 'Rolling Total Return'
        return rolling

    def rolling_annualized(self, window: int = 252) -> pd.Series:
        """Rolling annualized return."""
        rt = self.rolling_total(window)
        years = window / self._ann_factor
        rolling = (1 + rt) ** (1 / years) - 1
        rolling.name = 'Rolling Annualized Return'
        return rolling

    def rolling_annualized_volatility(self, window: int = 252) -> pd.Series:
        """Rolling annualized volatility."""
        vol = self._returns.rolling(window).std() * np.sqrt(self._ann_factor)
        vol.name = 'Rolling Volatility'
        return vol

    def rolling_sharpe_ratio(self, window: int = 252, risk_free: float = 0.0) -> pd.Series:
        """Rolling Sharpe ratio."""
        roll_ret = self._returns.rolling(window).mean() * self._ann_factor - risk_free
        roll_vol = self._returns.rolling(window).std() * np.sqrt(self._ann_factor)
        sharpe = roll_ret / roll_vol.replace(0, np.nan)
        sharpe.name = 'Rolling Sharpe'
        return sharpe.fillna(0.0)

    def rolling_sortino_ratio(self, window: int = 252, risk_free: float = 0.0) -> pd.Series:
        """Rolling Sortino ratio."""
        neg_rets = self._returns.where(self._returns < 0, 0.0)
        roll_ret = self._returns.rolling(window).mean() * self._ann_factor - risk_free
        roll_downside = neg_rets.rolling(window).apply(
            lambda x: np.sqrt(np.mean(x ** 2)) * np.sqrt(self._ann_factor), raw=True
        )
        sortino = roll_ret / roll_downside.replace(0, np.nan)
        sortino.name = 'Rolling Sortino'
        return sortino.fillna(0.0)

    def rolling_calmar_ratio(self, window: int = 252) -> pd.Series:
        """Rolling Calmar ratio."""
        cum = pd.Series(np.cumprod(1 + self.values), index=self.index)

        def _calmar_window(w):
            if len(w) < 2:
                return 0.0
            peak = np.maximum.accumulate(w)
            dd = (w - peak) / np.where(peak > 0, peak, 1.0)
            max_dd = abs(np.min(dd))
            if max_dd < 1e-10:
                return 0.0
            total_r = w[-1] / w[0] - 1 if w[0] > 0 else 0.0
            years = len(w) / self._ann_factor
            ann_r = (1 + total_r) ** (1 / years) - 1 if years > 0 else 0.0
            return ann_r / max_dd

        calmar = cum.rolling(window).apply(_calmar_window, raw=True)
        calmar.name = 'Rolling Calmar'
        return calmar.fillna(0.0)

    def rolling_omega_ratio(self, window: int = 252, threshold: float = 0.0) -> pd.Series:
        """Rolling Omega ratio."""
        def _omega_window(w):
            excess = w - threshold
            gains = excess[excess > 0].sum()
            losses = -excess[excess < 0].sum()
            return gains / losses if losses > 1e-10 else 99.0

        omega = self._returns.rolling(window).apply(_omega_window, raw=True)
        omega.name = 'Rolling Omega'
        return omega.fillna(0.0)

    def rolling_information_ratio(self, window: int = 252) -> pd.Series:
        """Rolling Information ratio vs benchmark."""
        if self._benchmark is None:
            return pd.Series(0.0, index=self.index, name='Rolling IR')

        active = self._returns - self._benchmark
        roll_mean = active.rolling(window).mean() * self._ann_factor
        roll_std = active.rolling(window).std() * np.sqrt(self._ann_factor)
        ir = roll_mean / roll_std.replace(0, np.nan)
        ir.name = 'Rolling Information Ratio'
        return ir.fillna(0.0)

    def rolling_downside_risk(self, window: int = 252) -> pd.Series:
        """Rolling annualized downside risk."""
        neg_rets = self._returns.where(self._returns < 0, 0.0)
        dr = neg_rets.rolling(window).apply(
            lambda x: np.sqrt(np.mean(x ** 2)) * np.sqrt(self._ann_factor), raw=True
        )
        dr.name = 'Rolling Downside Risk'
        return dr.fillna(0.0)

    # ------------------------------------------------------------------
    # Capture Ratios
    # ------------------------------------------------------------------

    def up_capture(self) -> float:
        """Up capture ratio vs benchmark."""
        if self._benchmark is None:
            return 0.0
        bm = self._benchmark.values[:len(self.values)]
        mask = bm > 0
        if not np.any(mask):
            return 0.0
        port_up = np.mean(self.values[mask])
        bm_up = np.mean(bm[mask])
        return float(port_up / bm_up) if abs(bm_up) > 1e-10 else 0.0

    def down_capture(self) -> float:
        """Down capture ratio vs benchmark."""
        if self._benchmark is None:
            return 0.0
        bm = self._benchmark.values[:len(self.values)]
        mask = bm < 0
        if not np.any(mask):
            return 0.0
        port_down = np.mean(self.values[mask])
        bm_down = np.mean(bm[mask])
        return float(port_down / bm_down) if abs(bm_down) > 1e-10 else 0.0

    def up_down_ratio(self) -> float:
        """Up/Down capture ratio."""
        dc = self.down_capture()
        if abs(dc) < 1e-10:
            return 0.0
        return float(self.up_capture() / dc)

    # ------------------------------------------------------------------
    # Distribution Stats
    # ------------------------------------------------------------------

    def _skewness(self) -> float:
        v = self.values
        if len(v) < 3:
            return 0.0
        m = np.mean(v)
        s = np.std(v, ddof=1)
        if s < 1e-10:
            return 0.0
        return float(np.mean((v - m) ** 3) / (s ** 3))

    def _kurtosis(self) -> float:
        v = self.values
        if len(v) < 4:
            return 3.0
        m = np.mean(v)
        s = np.std(v, ddof=1)
        if s < 1e-10:
            return 3.0
        return float(np.mean((v - m) ** 4) / (s ** 4))

    # ------------------------------------------------------------------
    # Summary
    # ------------------------------------------------------------------

    def stats(self) -> dict:
        """Return a summary dict of all key metrics."""
        return {
            'Total Return': self.total(),
            'Annualized Return': self.annualized(),
            'Annualized Volatility': self.annualized_volatility(),
            'Sharpe Ratio': self.sharpe_ratio(),
            'Sortino Ratio': self.sortino_ratio(),
            'Calmar Ratio': self.calmar_ratio(),
            'Omega Ratio': self.omega_ratio(),
            'Max Drawdown': self.max_drawdown(),
            'Downside Risk': self.downside_risk(),
            'Information Ratio': self.information_ratio(),
            'Deflated Sharpe': self.deflated_sharpe_ratio(),
            'Skewness': self._skewness(),
            'Kurtosis': self._kurtosis(),
            'Up Capture': self.up_capture(),
            'Down Capture': self.down_capture(),
        }


# ============================================================================
# Helper
# ============================================================================

def _erf(x: float) -> float:
    """Approximate error function."""
    sign = 1 if x >= 0 else -1
    x = abs(x)
    t = 1.0 / (1.0 + 0.3275911 * x)
    y = 1.0 - (
        (((1.061405429 * t - 1.453152027) * t + 1.421413741) * t - 0.284496736) * t + 0.254829592
    ) * t * np.exp(-x * x)
    return sign * y
