"""
Yield Curve Analytics Module
============================

Term structure construction and analysis (CFA-curriculum methods).

Conventions
-----------
* Rates are decimals. Maturities are years from today (settlement).
* Spot (zero) rates reported as `spot_rate` use ANNUAL compounding: DF(t) = (1 + s)^-t.
  bootstrap also reports the same rate compounded at the bond frequency
  (`spot_rate_periodic`) and continuously (`spot_rate_continuous`).
* Bootstrap: one knot per input bond; the zero curve is linear in the continuously-compounded
  zero rate between knots and flat outside them; every knot is solved exactly, so the curve
  reprices every input bond. Bond prices are clean unless price_type = "dirty".
  Par-yield input (maturities + yields): par bonds; maturities up to one coupon period are
  treated as zero-coupon with the yield compounded at the bond frequency.
* Forward rates: annual compounding, f(t1,t2) = [(1+s2)^t2/(1+s1)^t1]^(1/(t2-t1)) - 1.
* Nelson-Siegel / Svensson: least squares on the yields as given (betas solved exactly for each
  decay parameter, decay parameters by grid + golden-section search).
* Z-spread: constant spread added to the annually-compounded spot rates, solved exactly.

PARAMETERS:
  maturities, yields, bonds, frequency, spot_rates ([[t, rate], ...]), forward_periods
"""

import logging
import math
import os
import sys
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fi_common as fc  # noqa: E402
from fi_common import FIError, num, r  # noqa: E402

logger = logging.getLogger(__name__)


class CurveType(Enum):
    """Types of yield curves"""
    SPOT = "spot"
    FORWARD = "forward"
    PAR = "par"
    ZERO = "zero"


class InterpolationMethod(Enum):
    """Curve interpolation methods"""
    LINEAR = "linear"
    CUBIC_SPLINE = "cubic_spline"
    NELSON_SIEGEL = "nelson_siegel"
    SVENSSON = "svensson"


class SpreadType(Enum):
    """Types of yield spreads"""
    G_SPREAD = "g_spread"
    I_SPREAD = "i_spread"
    Z_SPREAD = "z_spread"
    OAS = "oas"
    ASW = "asset_swap_spread"


@dataclass
class YieldCurvePoint:
    """Single point on yield curve"""
    maturity: float
    yield_rate: float
    spot_rate: Optional[float] = None
    forward_rate: Optional[float] = None
    discount_factor: Optional[float] = None


@dataclass
class YieldCurve:
    """Complete yield curve representation"""
    curve_date: str
    currency: str = "USD"
    curve_type: CurveType = CurveType.SPOT
    points: List[YieldCurvePoint] = field(default_factory=list)
    interpolation: InterpolationMethod = InterpolationMethod.CUBIC_SPLINE


# ---------------------------------------------------------------------------
# Nelson-Siegel / Svensson machinery (numpy only)
# ---------------------------------------------------------------------------

def _ns_loadings(t, tau):
    import numpy as np
    t = np.asarray(t, dtype=float)
    x = t / tau
    with np.errstate(divide='ignore', invalid='ignore'):
        f1 = np.where(x > 1e-12, (1 - np.exp(-x)) / np.where(x > 1e-12, x, 1.0), 1.0)
    f2 = f1 - np.exp(-x)
    return f1, f2


def _design(t, taus):
    import numpy as np
    t = np.asarray(t, dtype=float)
    cols = [np.ones_like(t)]
    f1, f2 = _ns_loadings(t, taus[0])
    cols += [f1, f2]
    if len(taus) == 2:
        _, g2 = _ns_loadings(t, taus[1])
        cols.append(g2)
    return np.column_stack(cols)


def _fit_betas(t, y, taus):
    import numpy as np
    X = _design(t, taus)
    beta, *_ = np.linalg.lstsq(X, y, rcond=None)
    resid = y - X @ beta
    return beta, float(resid @ resid)


def _golden(fn: Callable[[float], float], a: float, b: float, iters: int = 80) -> float:
    g = (math.sqrt(5) - 1) / 2
    c, d = b - g * (b - a), a + g * (b - a)
    fc_, fd = fn(c), fn(d)
    for _ in range(iters):
        if fc_ < fd:
            b, d, fd = d, c, fc_
            c = b - g * (b - a)
            fc_ = fn(c)
        else:
            a, c, fc_ = c, d, fd
            d = a + g * (b - a)
            fd = fn(d)
    return (a + b) / 2


# The curvature loading (1-e^-x)/x - e^-x peaks at x = 1.7933. Decay parameters are restricted so that
# the hump sits inside the observed maturity range; outside it the loadings become collinear and the
# betas explode (a perfect but meaningless fit).
_HUMP_X = 1.7933


def _tau_bounds(t) -> Tuple[float, float]:
    lo_m = max(float(min(t)), 0.25)
    hi_m = max(float(max(t)), lo_m * 2)
    return lo_m / _HUMP_X, hi_m / _HUMP_X


def _log_grid(lo: float, hi: float, n: int) -> List[float]:
    return [lo * (hi / lo) ** (i / (n - 1)) for i in range(n)]


def _fit_ns(t, y):
    """Least-squares Nelson-Siegel fit: betas exact for each tau, tau by grid + golden section in log(tau)."""
    tau_lo, tau_hi = _tau_bounds(t)
    grid = _log_grid(tau_lo, tau_hi, 120)
    sse = lambda lt: _fit_betas(t, y, (math.exp(lt),))[1]
    errs = [sse(math.log(tau)) for tau in grid]
    i = min(range(len(errs)), key=errs.__getitem__)
    lo = math.log(grid[max(i - 1, 0)])
    hi = math.log(grid[min(i + 1, len(grid) - 1)])
    lt = _golden(sse, lo, hi) if hi > lo else lo
    if sse(lt) > errs[i]:
        lt = math.log(grid[i])
    tau = math.exp(lt)
    beta, s = _fit_betas(t, y, (tau,))
    return beta, (tau,), s


def _fit_svensson(t, y):
    tau_lo, tau_hi = _tau_bounds(t)
    grid = _log_grid(tau_lo, tau_hi, 30)
    best = None
    for i, t1 in enumerate(grid):
        for t2 in grid[i + 1:]:
            if t2 < t1 * 1.2:
                continue
            s = _fit_betas(t, y, (t1, t2))[1]
            if best is None or s < best[0]:
                best = (s, t1, t2)
    if best is None:
        raise FIError('maturity range too narrow for a Svensson fit')
    _, t1, t2 = best
    l1, l2 = math.log(t1), math.log(t2)
    llo, lhi = math.log(tau_lo), math.log(tau_hi)
    step = math.log(grid[1] / grid[0])
    for _ in range(6):  # coordinate descent in log(tau), kept inside the bounds and tau2 > 1.2 tau1
        a_lo, a_hi = max(l1 - step, llo), min(l1 + step, l2 - math.log(1.2))
        if a_hi > a_lo:
            l1 = _golden(lambda a: _fit_betas(t, y, (math.exp(a), math.exp(l2)))[1], a_lo, a_hi)
        b_lo, b_hi = max(l2 - step, l1 + math.log(1.2)), min(l2 + step, lhi)
        if b_hi > b_lo:
            l2 = _golden(lambda b: _fit_betas(t, y, (math.exp(l1), math.exp(b)))[1], b_lo, b_hi)
        step /= 2
    taus = (math.exp(l1), math.exp(l2))
    beta, s = _fit_betas(t, y, taus)
    if s > best[0]:
        taus = (best[1], best[2])
        beta, s = _fit_betas(t, y, taus)
    return beta, taus, s


def _fit_warnings(beta) -> List[str]:
    if any(abs(float(b)) > 1.0 for b in beta[1:]):
        return ['slope/curvature loadings exceed 100% in absolute value: the curve is over-fitted '
                '(too few points or noisy yields); treat the parameters with caution']
    return []


def _fit_stats(y, fitted):
    import numpy as np
    resid = y - fitted
    sse = float(resid @ resid)
    sst = float(((y - y.mean()) ** 2).sum())
    return {
        'rmse': round(float(np.sqrt(sse / len(y))), 8),
        'r_squared': round(1 - sse / sst, 6) if sst > 1e-24 else None,
        'r_squared_note': None if sst > 1e-24 else 'undefined: all input yields are equal',
        'max_abs_error_bps': round(float(np.max(np.abs(resid))) * 10000, 3),
    }


class YieldCurveBuilder:
    """
    Yield curve construction and analysis: bootstrapping, forwards, Nelson-Siegel / Svensson
    fits, interpolation and shape analysis.
    """

    # -- bootstrap -----------------------------------------------------------------------

    def bootstrap_spot_curve(
        self,
        bonds: Optional[List[Dict[str, float]]] = None,
        frequency: int = 2,
        maturities: Optional[List[float]] = None,
        yields: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """
        Bootstrap the spot (zero) curve.

        bonds: [{maturity, coupon_rate, price[, face_value, frequency, price_type]}] in any order and at any
        maturities; or maturities + yields = par yields (e.g. Treasury constant-maturity yields).
        """
        fc.check_frequency(frequency)
        if bonds:
            instruments = fc.bond_instruments(bonds, frequency)
            source = 'bond prices'
        elif maturities is not None and yields is not None:
            m, y = fc.sorted_curve(maturities, yields, min_points=1)
            if any(t <= 0 for t in m):
                raise FIError('maturities must be > 0')
            instruments = fc.par_yield_instruments(m, y, frequency)
            source = 'par yields'
        else:
            raise FIError('give bonds [{maturity, coupon_rate, price}], or maturities + yields (par yields)')

        curve, inst = fc.bootstrap_zero_curve(instruments)
        f = frequency or 1
        points = []
        max_err = 0.0
        for d in inst:
            T = d['maturity']
            z = d['zero_rate_cc']
            dfac = curve.df(T)
            repriced_dirty = fc.price_on_curve(d['flows'], curve)
            err = repriced_dirty - d['dirty_price']
            max_err = max(max_err, abs(err))
            points.append({
                'maturity': T,
                'spot_rate': round(math.exp(z) - 1, 8),
                'spot_rate_pct': round((math.exp(z) - 1) * 100, 6),
                'spot_rate_periodic': round(f * (math.exp(z / f) - 1), 8),
                'spot_rate_continuous': round(z, 8),
                'discount_factor': round(dfac, 8),
                'coupon_rate': d['coupon_rate'],
                'input_price': d['price'],
                'input_price_type': d['price_type'],
                'repriced_price': round(repriced_dirty - d['accrued_interest'], 6),
            })
        return {
            'spot_curve': points,
            'num_points': len(points),
            'method': 'bootstrap',
            'source': source,
            'max_repricing_error': r(max_err, 10),
            'conventions': {
                'spot_rate': 'annual compounding, DF = (1+s)^-t',
                'spot_rate_periodic': f'compounded {f}x per year',
                'spot_rate_continuous': 'continuous compounding, DF = exp(-z t)',
                'interpolation': 'linear in the continuous zero rate between maturities; flat before the '
                                 'first and after the last',
                'prices': 'clean unless price_type is dirty; accrued interest added for fractional periods',
            },
        }

    # -- forwards ------------------------------------------------------------------------

    def calculate_forward_curve(
        self,
        spot_rates: List[Tuple[float, float]],
        forward_periods: Optional[List[Tuple[float, float]]] = None,
    ) -> Dict[str, Any]:
        """
        Forward rates from annually-compounded spot rates [(maturity, rate), ...].
        f(t1,t2) = [(1+s2)^t2 / (1+s1)^t1]^(1/(t2-t1)) - 1; spots between points linearly interpolated,
        flat beyond the ends (flagged `extrapolated`).
        """
        mats, rates = fc.sorted_curve([p[0] for p in spot_rates], [p[1] for p in spot_rates],
                                      min_points=2, names=('spot_rates maturities', 'spot_rates rates'))
        if any(x <= -1 for x in rates):
            raise FIError('spot rates must be > -1')
        if forward_periods is None:
            forward_periods = [(mats[i], mats[i + 1]) for i in range(len(mats) - 1)]
        forward_rates = []
        for i, fp in enumerate(forward_periods):
            if not isinstance(fp, (list, tuple)) or len(fp) != 2:
                raise FIError(f'forward_periods[{i}] must be [start, end]')
            t1 = fc.as_num(fp[0], f'forward_periods[{i}] start', lo=0)
            t2 = fc.as_num(fp[1], f'forward_periods[{i}] end', lo=0)
            if t2 <= t1:
                raise FIError(f'forward_periods[{i}]: end must be greater than start')
            s1, x1 = fc.linear_interp(mats, rates, t1)
            s2, x2 = fc.linear_interp(mats, rates, t2)
            forward = ((1 + s2) ** t2 / (1 + s1) ** t1) ** (1 / (t2 - t1)) - 1
            row = {
                'start': t1,
                'end': t2,
                'period': f'{t1:g}y x {t2:g}y',
                'forward_rate': round(forward, 6),
                'forward_rate_pct': round(forward * 100, 4),
                'spot_t1': round(s1, 6),
                'spot_t2': round(s2, 6),
            }
            if (x1 and t1 > 0) or x2:
                row['extrapolated'] = True
            forward_rates.append(row)
        return {
            'forward_curve': forward_rates,
            'num_points': len(forward_rates),
            'conventions': {'compounding': 'annual', 'input': 'annually-compounded spot rates'},
        }

    # -- parametric fits -----------------------------------------------------------------

    def fit_nelson_siegel(
        self,
        maturities: List[float],
        yields: List[float],
    ) -> Dict[str, Any]:
        """
        y(t) = b0 + b1*[(1-exp(-t/tau))/(t/tau)] + b2*[(1-exp(-t/tau))/(t/tau) - exp(-t/tau)]
        """
        import numpy as np
        m, y = fc.sorted_curve(maturities, yields, min_points=4)
        t = np.array(m)
        yy = np.array(y)
        beta, (tau,), _ = _fit_ns(t, yy)
        b0, b1, b2 = (float(v) for v in beta)
        fitted_at = _design(t, (tau,)) @ beta
        grid = np.linspace(min(m), max(m), 50)
        fitted = _design(grid, (tau,)) @ beta
        warnings = _fit_warnings(beta)
        lo, hi = _tau_bounds(t)
        out = {
            'parameters': {'beta0': round(b0, 6), 'beta1': round(b1, 6), 'beta2': round(b2, 6),
                           'tau': round(tau, 4)},
            'tau_search_range': [round(lo, 4), round(hi, 4)],
            'interpretation': {
                'long_term_rate': round(b0, 4),
                'short_term_rate': round(b0 + b1, 4),
                'slope': round(b1, 4),
                'curvature': round(b2, 4),
            },
            'fitted_curve': [{'maturity': round(float(a), 4), 'yield': round(float(b), 6)}
                             for a, b in zip(grid, fitted)],
            'fitted_at_inputs': [{'maturity': a, 'yield': b, 'fitted': round(float(c), 6),
                                  'residual_bps': round((b - float(c)) * 10000, 3)}
                                 for a, b, c in zip(m, y, fitted_at)],
            'fit_statistics': _fit_stats(yy, fitted_at),
        }
        if warnings:
            out['warnings'] = warnings
        return out

    def fit_svensson(
        self,
        maturities: List[float],
        yields: List[float],
    ) -> Dict[str, Any]:
        """
        Svensson: Nelson-Siegel plus a second hump term b3*[(1-exp(-t/tau2))/(t/tau2) - exp(-t/tau2)].
        """
        import numpy as np
        m, y = fc.sorted_curve(maturities, yields, min_points=6)
        t = np.array(m)
        yy = np.array(y)
        beta, taus, _ = _fit_svensson(t, yy)
        b0, b1, b2, b3 = (float(v) for v in beta)
        fitted_at = _design(t, taus) @ beta
        grid = np.linspace(min(m), max(m), 50)
        fitted = _design(grid, taus) @ beta
        out = {
            'parameters': {'beta0': round(b0, 6), 'beta1': round(b1, 6), 'beta2': round(b2, 6),
                           'beta3': round(b3, 6), 'tau1': round(taus[0], 4), 'tau2': round(taus[1], 4)},
            'fitted_curve': [{'maturity': round(float(a), 4), 'yield': round(float(b), 6)}
                             for a, b in zip(grid, fitted)],
            'fitted_at_inputs': [{'maturity': a, 'yield': b, 'fitted': round(float(c), 6),
                                  'residual_bps': round((b - float(c)) * 10000, 3)}
                                 for a, b, c in zip(m, y, fitted_at)],
            'fit_statistics': _fit_stats(yy, fitted_at),
        }
        warnings = _fit_warnings(beta)
        if warnings:
            out['warnings'] = warnings
        return out

    # -- interpolation -------------------------------------------------------------------

    def interpolate_curve(
        self,
        maturities: List[float],
        yields: List[float],
        target_maturities: List[float],
        method: InterpolationMethod = InterpolationMethod.CUBIC_SPLINE,
    ) -> Dict[str, Any]:
        """
        Yields at target maturities. linear / cubic_spline (not-a-knot, >= 4 points) interpolate between
        the inputs and hold the end value flat outside them (flagged); nelson_siegel / svensson evaluate a
        fitted model (no flat extrapolation).
        """
        if not target_maturities:
            raise FIError('target_maturities must be a non-empty list')
        min_pts = {InterpolationMethod.LINEAR: 2, InterpolationMethod.CUBIC_SPLINE: 4,
                   InterpolationMethod.NELSON_SIEGEL: 4, InterpolationMethod.SVENSSON: 6}[method]
        m, y = fc.sorted_curve(maturities, yields, min_points=min_pts)
        targets = [float(x) for x in target_maturities]
        out = []
        if method in (InterpolationMethod.LINEAR, InterpolationMethod.CUBIC_SPLINE):
            spline = None
            if method == InterpolationMethod.CUBIC_SPLINE:
                try:
                    from scipy.interpolate import CubicSpline
                except ImportError:
                    raise FIError('method cubic_spline needs scipy, which is not installed; '
                                  'use linear or nelson_siegel') from None
                spline = CubicSpline(m, y)
            for x in targets:
                if x < m[0] or x > m[-1]:
                    val, extra = (y[0] if x < m[0] else y[-1]), True
                elif spline is not None:
                    val, extra = float(spline(x)), False
                else:
                    val, extra = fc.linear_interp(m, y, x)
                row = {'maturity': round(x, 4), 'yield': round(val, 6)}
                if extra:
                    row['extrapolated'] = True
                out.append(row)
        else:
            import numpy as np
            fit = _fit_ns if method == InterpolationMethod.NELSON_SIEGEL else _fit_svensson
            beta, taus, _ = fit(np.array(m), np.array(y))
            vals = _design(np.array(targets), taus) @ beta
            out = [{'maturity': round(x, 4), 'yield': round(float(v), 6)} for x, v in zip(targets, vals)]
        return {
            'interpolated_points': out,
            'method': method.value,
            'original_points': len(m),
            'interpolated_points_count': len(out),
        }

    # -- shape ---------------------------------------------------------------------------

    def analyze_curve_shape(
        self,
        maturities: List[float],
        yields: List[float],
    ) -> Dict[str, Any]:
        """
        Curve shape from the short end, the long end and the belly (yield interpolated at 5y, or at the
        geometric middle of the maturity range when 5y is outside it). Humped / U-shaped need the belly to
        sit at least 10 bp above / below both ends; the slope regime is reported separately.
        """
        m, y = fc.sorted_curve(maturities, yields, min_points=2)
        short_rate, long_rate = y[0], y[-1]
        slope = long_rate - short_rate
        # Classify on the rounded basis-point value that is reported, so a slope of
        # exactly 50 bp gets the same label whatever float noise the inputs carry.
        slope_bps_rounded = round(slope * 10000, 1)
        if slope_bps_rounded > 50.0:
            regime = "Normal (Upward Sloping)"
        elif slope_bps_rounded < -50.0:
            regime = "Inverted (Downward Sloping)"
        else:
            regime = "Flat"
        shape = regime

        belly_t = None
        belly = None
        if len(m) >= 3:
            if m[0] < 5.0 < m[-1]:
                belly_t = 5.0
            elif m[0] > 0:
                belly_t = math.sqrt(m[0] * m[-1])
            else:
                belly_t = (m[0] + m[-1]) / 2
            belly, _ = fc.linear_interp(m, y, belly_t)
            if belly > max(short_rate, long_rate) + 0.001:
                shape = "Humped"
            elif belly < min(short_rate, long_rate) - 0.001:
                shape = "U-Shaped"

        notes = []

        def rate_at(tenor: float, label: str):
            for t, v in zip(m, y):
                if abs(t - tenor) < 1e-6:
                    return v
            if m[0] <= tenor <= m[-1]:
                v, _ = fc.linear_interp(m, y, tenor)
                notes.append(f'{label} yield interpolated (no {label} point given)')
                return v
            notes.append(f'{label} is outside the curve range {m[0]:g}-{m[-1]:g}y')
            return None

        r3m, r2y, r10y = rate_at(0.25, '3m'), rate_at(2.0, '2y'), rate_at(10.0, '10y')
        spread_2s10s = (r10y - r2y) if (r10y is not None and r2y is not None) else None
        spread_3m10y = (r10y - r3m) if (r10y is not None and r3m is not None) else None
        steepness = slope / (m[-1] - m[0]) if m[-1] > m[0] else None

        out = {
            'curve_shape': shape,
            'slope_regime': regime,
            'short_rate': round(short_rate, 6),
            'short_maturity': m[0],
            'mid_rate': r(belly, 6),
            'mid_maturity': r(belly_t, 4),
            'long_rate': round(long_rate, 6),
            'long_maturity': m[-1],
            'slope': round(slope, 6),
            'slope_bps': round(slope * 10000, 1),
            'butterfly_bps': round((2 * belly - short_rate - long_rate) * 10000, 1) if belly is not None else None,
            'steepness_per_year': r(steepness, 6),
            'spread_2s10s': round(spread_2s10s * 10000, 1) if spread_2s10s is not None else None,
            'spread_3m10y': round(spread_3m10y * 10000, 1) if spread_3m10y is not None else None,
            'market_signal': self._interpret_curve_shape(shape, slope),
            'units': {'rates': 'decimal', 'slope_bps / spreads': 'basis points'},
        }
        if notes:
            out['notes'] = notes
        return out

    def _interpret_curve_shape(self, shape: str, slope: float) -> str:
        """Interpret yield curve shape for market signals."""
        if "Inverted" in shape:
            return "Potential recession signal - markets expect rate cuts"
        if "Humped" in shape:
            return "Near-term uncertainty with longer-term stability expected"
        if "U-Shaped" in shape:
            return ("Short end above the belly - markets price near-term rate cuts, with term premium "
                    "lifting the long end")
        if "Flat" in shape:
            return "Uncertain outlook - possible transition period"
        if slope > 0.02:
            return "Steep curve - strong growth / reflation expectations or a large term premium"
        return "Upward sloping - normal term premium, typical of an expansion"


class SpreadAnalyzer:
    """Yield spread analysis for credit and relative value."""

    def calculate_g_spread(self, bond_ytm: float, treasury_ytm: float) -> Dict[str, Any]:
        """G-spread = bond YTM - government YTM at the same maturity."""
        g_spread = bond_ytm - treasury_ytm
        return {
            'g_spread': round(g_spread, 6),
            'g_spread_bps': round(g_spread * 10000, 1),
            'bond_ytm': round(bond_ytm, 6),
            'treasury_ytm': round(treasury_ytm, 6),
            'interpretation': f"Bond yields {round(g_spread * 10000, 1)}bps over comparable Treasury"
        }

    def calculate_i_spread(self, bond_ytm: float, swap_rate: float) -> Dict[str, Any]:
        """I-spread = bond YTM - swap rate at the same maturity."""
        i_spread = bond_ytm - swap_rate
        return {
            'i_spread': round(i_spread, 6),
            'i_spread_bps': round(i_spread * 10000, 1),
            'bond_ytm': round(bond_ytm, 6),
            'swap_rate': round(swap_rate, 6),
            'interpretation': f"Bond yields {round(i_spread * 10000, 1)}bps over swap curve"
        }

    def calculate_z_spread(
        self,
        bond_price: float,
        cash_flows: List[Tuple[float, float]],
        spot_rates: List[Tuple[float, float]],
    ) -> Dict[str, Any]:
        """
        Z-spread: constant z with sum CF_t / (1 + s(t) + z)^t = price (annually-compounded spots,
        linear interpolation, flat beyond the ends). Price is the full (dirty) price.
        """
        if bond_price <= 0:
            raise FIError('bond_price must be > 0')
        if not cash_flows:
            raise FIError('cash_flows must be a non-empty list of [time, amount]')
        for i, (t, cf) in enumerate(cash_flows):
            if t <= 0:
                raise FIError(f'cash_flows[{i}] time must be > 0')
        mats, rates = fc.sorted_curve([p[0] for p in spot_rates], [p[1] for p in spot_rates], min_points=1,
                                      names=('spot_rates maturities', 'spot_rates rates'))
        spots = [fc.linear_interp(mats, rates, t)[0] for t, _ in cash_flows]

        def pv(z):
            total = 0.0
            for (t, cf), s in zip(cash_flows, spots):
                base = 1 + s + z
                if base <= 0:
                    return float('inf')
                total += cf * base ** -t
            return total

        lo = -min(1 + s for s in spots) + 1e-9
        hi = 1.0
        fn = fc.overflow_safe(lambda z: pv(z) - bond_price)
        if fn(lo) < 0:
            raise FIError('bond_price is above the value of the cash flows at any spread')
        while fn(hi) > 0:
            hi *= 2
            if hi > 1e4:
                raise FIError('bond_price is too low for any reasonable spread')
        z = fc.find_root(fn, lo, hi, tol=1e-14)
        return {
            'z_spread': round(z, 6),
            'z_spread_bps': round(z * 10000, 2),
            'bond_price': round(bond_price, 4),
            'calculated_price': round(pv(z), 4),
            'interpretation': f"Constant spread of {round(z * 10000, 1)}bps over spot curve",
            'conventions': {'compounding': 'annual', 'interpolation': 'linear in spot rate, flat beyond the ends'},
        }

    def calculate_oas(
        self,
        bond_price: float,
        cash_flows: List[Tuple[float, float]],
        spot_rates: List[Tuple[float, float]],
        option_value: Optional[float] = None,
    ) -> Dict[str, Any]:
        """
        Option-adjusted spread as Z-spread minus the embedded option's cost expressed in spread terms.

        There is no interest-rate tree here, so the option cost must be supplied (option_value, decimal
        spread, e.g. 0.0035 = 35 bp; positive for an option the issuer holds, e.g. a call).
        """
        if option_value is None:
            raise FIError('oas needs option_value: the embedded option cost as a spread (decimal, e.g. 0.0035). '
                          'This module has no rate-tree model to derive it; use z_spread for the spread over the '
                          'spot curve')
        z_result = self.calculate_z_spread(bond_price, cash_flows, spot_rates)
        z_spread = z_result['z_spread']
        oas = z_spread - option_value
        return {
            'oas': round(oas, 6),
            'oas_bps': round(oas * 10000, 1),
            'z_spread': round(z_spread, 6),
            'z_spread_bps': round(z_spread * 10000, 1),
            'option_cost': round(option_value, 6),
            'option_cost_bps': round(option_value * 10000, 1),
            'method': 'Z-spread minus the supplied option cost (no rate-tree model)',
            'interpretation': 'OAS removes embedded option effect for better comparison'
        }

    def compare_spreads(
        self,
        bond_ytm: float,
        treasury_ytm: float,
        swap_rate: float,
        bond_price: float = None,
        cash_flows: List[Tuple[float, float]] = None,
        spot_rates: List[Tuple[float, float]] = None,
    ) -> Dict[str, Any]:
        """G-spread, I-spread and (when price, cash flows and spot rates are given) Z-spread."""
        spreads = {
            'g_spread': self.calculate_g_spread(bond_ytm, treasury_ytm),
            'i_spread': self.calculate_i_spread(bond_ytm, swap_rate),
        }
        if bond_price and cash_flows and spot_rates:
            spreads['z_spread'] = self.calculate_z_spread(bond_price, cash_flows, spot_rates)
        return {
            'spread_comparison': spreads,
            'summary': {
                'g_spread_bps': spreads['g_spread']['g_spread_bps'],
                'i_spread_bps': spreads['i_spread']['i_spread_bps'],
                'z_spread_bps': spreads.get('z_spread', {}).get('z_spread_bps')
            }
        }


ANALYSIS_TYPES = ('bootstrap', 'forward_curve', 'nelson_siegel', 'svensson', 'interpolate', 'curve_shape',
                  'g_spread', 'z_spread', 'oas', 'compare_spreads')


def _curve_points(params: Dict[str, Any], key: str = 'spot_rates', min_len: int = 2):
    """[(t, rate)] from spot_rates ([[t, r]] or [{maturity, spot_rate}]) or from maturities + yields."""
    if params.get(key) is not None:
        return fc.pairs(params, key, min_len=min_len, what='[maturity, rate]')
    if params.get('maturities') is not None and params.get('yields') is not None:
        m = fc.num_list(params, 'maturities')
        y = fc.num_list(params, 'yields')
        if len(m) != len(y):
            raise FIError(f'maturities and yields must have the same length ({len(m)} vs {len(y)})')
        if len(m) < min_len:
            raise FIError(f'need at least {min_len} curve point(s)')
        return list(zip(m, y))
    raise FIError(f'{key} is required: a list of [maturity, rate] pairs (or give maturities + yields)')


def run_yield_curve_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Main entry point for yield curve analysis (params['analysis_type'] in ANALYSIS_TYPES)."""
    analysis_type = params.get('analysis_type', 'bootstrap')

    try:
        builder = YieldCurveBuilder()
        spreads = SpreadAnalyzer()
        if analysis_type == 'bootstrap':
            bonds = params.get('bonds')
            if bonds is not None and not isinstance(bonds, list):
                raise FIError('bonds must be a list of {maturity, coupon_rate, price}')
            freq = fc.check_frequency(num(params, 'frequency', 2, integer=True))
            if bonds:
                return builder.bootstrap_spot_curve(bonds=bonds, frequency=freq)
            m = fc.num_list(params, 'maturities', None) if params.get('maturities') is not None else None
            y = fc.num_list(params, 'yields', None) if params.get('yields') is not None else None
            return builder.bootstrap_spot_curve(frequency=freq, maturities=m, yields=y)

        elif analysis_type == 'forward_curve':
            fps = params.get('forward_periods')
            if fps is not None and not isinstance(fps, list):
                raise FIError('forward_periods must be a list of [start, end] pairs')
            return builder.calculate_forward_curve(spot_rates=_curve_points(params), forward_periods=fps)

        elif analysis_type == 'nelson_siegel':
            return builder.fit_nelson_siegel(maturities=fc.num_list(params, 'maturities'),
                                             yields=fc.num_list(params, 'yields'))

        elif analysis_type == 'svensson':
            return builder.fit_svensson(maturities=fc.num_list(params, 'maturities'),
                                        yields=fc.num_list(params, 'yields'))

        elif analysis_type == 'interpolate':
            method_name = fc.text(params, 'method', 'cubic_spline')
            method_name = {'cubic': 'cubic_spline', 'spline': 'cubic_spline', 'ns': 'nelson_siegel'}.get(
                method_name, method_name)
            try:
                method = InterpolationMethod(method_name)
            except ValueError:
                raise FIError('method must be one of: linear, cubic_spline, nelson_siegel, svensson') from None
            return builder.interpolate_curve(
                maturities=fc.num_list(params, 'maturities'), yields=fc.num_list(params, 'yields'),
                target_maturities=fc.num_list(params, 'target_maturities', min_len=1, lo=0), method=method)

        elif analysis_type == 'curve_shape':
            return builder.analyze_curve_shape(maturities=fc.num_list(params, 'maturities'),
                                               yields=fc.num_list(params, 'yields'))

        elif analysis_type == 'g_spread':
            return spreads.calculate_g_spread(bond_ytm=num(params, 'bond_ytm', 0.05),
                                              treasury_ytm=num(params, 'treasury_ytm', 0.03))

        elif analysis_type == 'z_spread':
            return spreads.calculate_z_spread(
                bond_price=num(params, 'bond_price', 1000, lo=0, lo_open=True),
                cash_flows=fc.pairs(params, 'cash_flows', what='[time, amount]'),
                spot_rates=_curve_points(params, min_len=1))

        elif analysis_type == 'oas':
            return spreads.calculate_oas(
                bond_price=num(params, 'bond_price', 1000, lo=0, lo_open=True),
                cash_flows=fc.pairs(params, 'cash_flows', what='[time, amount]'),
                spot_rates=_curve_points(params, min_len=1),
                option_value=num(params, 'option_value', None, allow_none=True))

        elif analysis_type == 'compare_spreads':
            has_z = params.get('bond_price') is not None and params.get('cash_flows') and params.get('spot_rates')
            return spreads.compare_spreads(
                bond_ytm=num(params, 'bond_ytm', 0.05), treasury_ytm=num(params, 'treasury_ytm', 0.03),
                swap_rate=num(params, 'swap_rate', 0.035),
                bond_price=num(params, 'bond_price', lo=0, lo_open=True) if has_z else None,
                cash_flows=fc.pairs(params, 'cash_flows', what='[time, amount]') if has_z else None,
                spot_rates=_curve_points(params, min_len=1) if has_z else None)

        else:
            return {'error': f'Unknown analysis type: {analysis_type}', 'analysis_types': list(ANALYSIS_TYPES)}

    except FIError as e:
        return {'error': str(e)}
    except Exception as e:
        logger.error(f"Yield curve analysis error: {e}")
        return {'error': str(e) or type(e).__name__}


if __name__ == "__main__":
    fc.import_fincept_json()
    fc.emit_usage_or_run(sys.argv, run_yield_curve_analysis, ANALYSIS_TYPES, 'yield_curve')
