"""
Yield Curve Analytics Module
============================

Term structure analysis and yield curve construction implementing CFA Institute
standard methodologies for fixed income analysis.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Treasury yields or benchmark rates
  - Bond prices for bootstrapping
  - Swap rates for curve construction
  - Credit spreads by rating and maturity

OUTPUT:
  - Spot rate curves (zero curves)
  - Forward rate curves
  - Par rate curves
  - Spread analysis (G-spread, Z-spread, OAS, I-spread)
  - Curve interpolation and fitting

PARAMETERS:
  - maturities: List of maturities in years
  - yields: List of yields corresponding to maturities
  - interpolation_method: cubic, linear, nelson_siegel - default: cubic
  - spread_type: g_spread, z_spread, oas, i_spread
"""

from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional, Tuple, Callable
from enum import Enum
import numpy as np
from scipy import interpolate, optimize
import logging

logging.basicConfig(level=logging.INFO)
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


class YieldCurveBuilder:
    """
    Yield curve construction and analysis engine.

    Provides comprehensive yield curve analytics including:
    - Bootstrapping spot curves from bond prices
    - Forward rate derivation
    - Curve interpolation and fitting
    - Nelson-Siegel and Svensson models
    """

    def __init__(self):
        self._spot_curve: Optional[Callable] = None
        self._forward_curve: Optional[Callable] = None

    def bootstrap_spot_curve(
        self,
        bonds: List[Dict[str, float]],
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Bootstrap spot rate curve from coupon bond prices.

        Args:
            bonds: List of bonds with keys: price, coupon_rate, maturity, face_value
                   Must be sorted by maturity
            frequency: Coupon frequency

        Returns:
            Dictionary with bootstrapped spot curve
        """
        spot_rates = []
        discount_factors = []

        for bond in sorted(bonds, key=lambda x: x.get('maturity', 0)):
            price = bond.get('price', 1000)
            coupon_rate = bond.get('coupon_rate', 0)
            maturity = bond.get('maturity', 1)
            face_value = bond.get('face_value', 1000)

            coupon = (coupon_rate * face_value) / frequency
            periods = int(maturity * frequency)

            if coupon_rate == 0 or len(spot_rates) == 0:
                # Zero coupon or first bond
                spot = (face_value / price) ** (1 / maturity) - 1
            else:
                # Bootstrap using known spot rates
                pv_coupons = 0
                for i, sr in enumerate(spot_rates):
                    t = (i + 1) / frequency
                    if t < maturity:
                        pv_coupons += coupon / ((1 + sr) ** t)

                # Solve for current spot rate
                remaining = price - pv_coupons
                final_cf = coupon + face_value

                spot = (final_cf / remaining) ** (1 / maturity) - 1

            spot_rates.append(spot)
            discount_factors.append(1 / ((1 + spot) ** maturity))

        curve_points = [
            {
                'maturity': bonds[i].get('maturity', i + 1),
                'spot_rate': round(spot_rates[i], 6),
                'spot_rate_pct': round(spot_rates[i] * 100, 4),
                'discount_factor': round(discount_factors[i], 6)
            }
            for i in range(len(spot_rates))
        ]

        return {
            'spot_curve': curve_points,
            'num_points': len(curve_points),
            'method': 'bootstrap'
        }

    def calculate_forward_curve(
        self,
        spot_rates: List[Tuple[float, float]],
        forward_periods: List[Tuple[float, float]] = None,
    ) -> Dict[str, Any]:
        """
        Calculate forward rates from spot rate curve.

        f(t1,t2) = [(1+s2)^t2 / (1+s1)^t1]^(1/(t2-t1)) - 1

        Args:
            spot_rates: List of (maturity, rate) tuples
            forward_periods: List of (start, end) periods for forward rates

        Returns:
            Dictionary with forward curve
        """
        # Default forward periods if not specified
        if forward_periods is None:
            maturities = [sr[0] for sr in spot_rates]
            forward_periods = [(maturities[i], maturities[i + 1])
                               for i in range(len(maturities) - 1)]

        # Create interpolator for spot rates
        mat_array = np.array([sr[0] for sr in spot_rates])
        rate_array = np.array([sr[1] for sr in spot_rates])
        spot_interp = interpolate.interp1d(mat_array, rate_array, kind='linear', fill_value='extrapolate')

        forward_rates = []

        for t1, t2 in forward_periods:
            s1 = float(spot_interp(t1))
            s2 = float(spot_interp(t2))

            if t2 > t1:
                forward = ((1 + s2) ** t2 / (1 + s1) ** t1) ** (1 / (t2 - t1)) - 1

                forward_rates.append({
                    'start': t1,
                    'end': t2,
                    'period': f'{t1}y x {t2}y',
                    'forward_rate': round(forward, 6),
                    'forward_rate_pct': round(forward * 100, 4),
                    'spot_t1': round(s1, 6),
                    'spot_t2': round(s2, 6)
                })

        return {
            'forward_curve': forward_rates,
            'num_points': len(forward_rates)
        }

    def fit_nelson_siegel(
        self,
        maturities: List[float],
        yields: List[float],
    ) -> Dict[str, Any]:
        """
        Fit Nelson-Siegel model to yield curve.

        y(t) = b0 + b1*[(1-exp(-t/tau))/(t/tau)] + b2*[(1-exp(-t/tau))/(t/tau) - exp(-t/tau)]

        Args:
            maturities: List of maturities
            yields: List of yields

        Returns:
            Dictionary with fitted parameters and curve
        """
        maturities = np.array(maturities)
        yields = np.array(yields)

        def nelson_siegel(t, b0, b1, b2, tau):
            if tau <= 0:
                return np.full_like(t, np.inf)
            x = t / tau
            with np.errstate(divide='ignore', invalid='ignore'):
                factor1 = np.where(x > 0, (1 - np.exp(-x)) / x, 1)
                factor2 = factor1 - np.exp(-x)
            return b0 + b1 * factor1 + b2 * factor2

        def objective(params):
            return np.sum((nelson_siegel(maturities, *params) - yields) ** 2)

        # Initial guess
        b0_init = yields[-1]  # Long-term level
        b1_init = yields[0] - yields[-1]  # Slope
        b2_init = 0  # Curvature
        tau_init = 2  # Time constant

        try:
            result = optimize.minimize(
                objective,
                [b0_init, b1_init, b2_init, tau_init],
                method='Nelder-Mead',
                options={'maxiter': 1000}
            )
            b0, b1, b2, tau = result.x
        except:
            return {'error': 'Failed to fit Nelson-Siegel model'}

        # Generate fitted curve
        fitted_maturities = np.linspace(0.25, max(maturities), 50)
        fitted_yields = nelson_siegel(fitted_maturities, b0, b1, b2, tau)

        # Calculate fit statistics
        fitted_at_data = nelson_siegel(maturities, b0, b1, b2, tau)
        rmse = np.sqrt(np.mean((yields - fitted_at_data) ** 2))
        r_squared = 1 - np.sum((yields - fitted_at_data) ** 2) / np.sum((yields - np.mean(yields)) ** 2)

        return {
            'parameters': {
                'beta0': round(b0, 6),  # Long-term level
                'beta1': round(b1, 6),  # Short-term component
                'beta2': round(b2, 6),  # Medium-term component
                'tau': round(tau, 4)    # Time decay
            },
            'interpretation': {
                'long_term_rate': round(b0, 4),
                'slope': round(b1, 4),
                'curvature': round(b2, 4)
            },
            'fitted_curve': [
                {'maturity': round(m, 2), 'yield': round(y, 6)}
                for m, y in zip(fitted_maturities, fitted_yields)
            ],
            'fit_statistics': {
                'rmse': round(rmse, 6),
                'r_squared': round(r_squared, 4)
            }
        }

    def fit_svensson(
        self,
        maturities: List[float],
        yields: List[float],
    ) -> Dict[str, Any]:
        """
        Fit Svensson model (extended Nelson-Siegel) to yield curve.

        Adds second hump term for better medium-term fitting.

        Args:
            maturities: List of maturities
            yields: List of yields

        Returns:
            Dictionary with fitted parameters and curve
        """
        maturities = np.array(maturities)
        yields = np.array(yields)

        def svensson(t, b0, b1, b2, b3, tau1, tau2):
            if tau1 <= 0 or tau2 <= 0:
                return np.full_like(t, np.inf)
            x1 = t / tau1
            x2 = t / tau2
            with np.errstate(divide='ignore', invalid='ignore'):
                factor1 = np.where(x1 > 0, (1 - np.exp(-x1)) / x1, 1)
                factor2 = factor1 - np.exp(-x1)
                factor3 = np.where(x2 > 0, (1 - np.exp(-x2)) / x2 - np.exp(-x2), 0)
            return b0 + b1 * factor1 + b2 * factor2 + b3 * factor3

        def objective(params):
            return np.sum((svensson(maturities, *params) - yields) ** 2)

        # Initial guess
        b0_init = yields[-1]
        b1_init = yields[0] - yields[-1]
        b2_init = 0
        b3_init = 0
        tau1_init = 2
        tau2_init = 5

        try:
            result = optimize.minimize(
                objective,
                [b0_init, b1_init, b2_init, b3_init, tau1_init, tau2_init],
                method='Nelder-Mead',
                options={'maxiter': 2000}
            )
            b0, b1, b2, b3, tau1, tau2 = result.x
        except:
            return {'error': 'Failed to fit Svensson model'}

        # Generate fitted curve
        fitted_maturities = np.linspace(0.25, max(maturities), 50)
        fitted_yields = svensson(fitted_maturities, b0, b1, b2, b3, tau1, tau2)

        # Fit statistics
        fitted_at_data = svensson(maturities, b0, b1, b2, b3, tau1, tau2)
        rmse = np.sqrt(np.mean((yields - fitted_at_data) ** 2))
        r_squared = 1 - np.sum((yields - fitted_at_data) ** 2) / np.sum((yields - np.mean(yields)) ** 2)

        return {
            'parameters': {
                'beta0': round(b0, 6),
                'beta1': round(b1, 6),
                'beta2': round(b2, 6),
                'beta3': round(b3, 6),
                'tau1': round(tau1, 4),
                'tau2': round(tau2, 4)
            },
            'fitted_curve': [
                {'maturity': round(m, 2), 'yield': round(y, 6)}
                for m, y in zip(fitted_maturities, fitted_yields)
            ],
            'fit_statistics': {
                'rmse': round(rmse, 6),
                'r_squared': round(r_squared, 4)
            }
        }

    def interpolate_curve(
        self,
        maturities: List[float],
        yields: List[float],
        target_maturities: List[float],
        method: InterpolationMethod = InterpolationMethod.CUBIC_SPLINE,
    ) -> Dict[str, Any]:
        """
        Interpolate yield curve to get rates at specific maturities.

        Args:
            maturities: Known maturities
            yields: Known yields
            target_maturities: Maturities to interpolate
            method: Interpolation method

        Returns:
            Dictionary with interpolated yields
        """
        maturities = np.array(maturities)
        yields = np.array(yields)
        target_maturities = np.array(target_maturities)

        if method == InterpolationMethod.LINEAR:
            interp_func = interpolate.interp1d(maturities, yields, kind='linear', fill_value='extrapolate')
        elif method == InterpolationMethod.CUBIC_SPLINE:
            interp_func = interpolate.interp1d(maturities, yields, kind='cubic', fill_value='extrapolate')
        else:
            interp_func = interpolate.interp1d(maturities, yields, kind='linear', fill_value='extrapolate')

        interpolated_yields = interp_func(target_maturities)

        return {
            'interpolated_points': [
                {'maturity': round(m, 2), 'yield': round(float(y), 6)}
                for m, y in zip(target_maturities, interpolated_yields)
            ],
            'method': method.value,
            'original_points': len(maturities),
            'interpolated_points_count': len(target_maturities)
        }

    def analyze_curve_shape(
        self,
        maturities: List[float],
        yields: List[float],
    ) -> Dict[str, Any]:
        """
        Analyze yield curve shape and characteristics.

        Args:
            maturities: List of maturities
            yields: List of yields

        Returns:
            Dictionary with curve shape analysis
        """
        maturities = np.array(maturities)
        yields = np.array(yields)

        # Determine curve shape
        short_rate = yields[0]
        long_rate = yields[-1]
        mid_idx = len(yields) // 2
        mid_rate = yields[mid_idx]

        slope = long_rate - short_rate

        if slope > 0.005:  # 50bp
            shape = "Normal (Upward Sloping)"
        elif slope < -0.005:
            shape = "Inverted (Downward Sloping)"
        else:
            shape = "Flat"

        # Check for hump
        if mid_rate > short_rate and mid_rate > long_rate:
            shape = "Humped"
        elif mid_rate < short_rate and mid_rate < long_rate:
            shape = "U-Shaped"

        # Calculate key spreads
        spread_2s10s = None
        spread_3m10y = None

        for i, m in enumerate(maturities):
            if abs(m - 2) < 0.1:
                rate_2y = yields[i]
            if abs(m - 10) < 0.1:
                rate_10y = yields[i]
            if abs(m - 0.25) < 0.1:
                rate_3m = yields[i]

        try:
            spread_2s10s = rate_10y - rate_2y
        except:
            pass

        try:
            spread_3m10y = rate_10y - rate_3m
        except:
            pass

        # Steepness
        steepness = slope / (maturities[-1] - maturities[0]) if len(maturities) > 1 else 0

        return {
            'curve_shape': shape,
            'short_rate': round(short_rate, 4),
            'mid_rate': round(mid_rate, 4),
            'long_rate': round(long_rate, 4),
            'slope': round(slope, 4),
            'slope_bps': round(slope * 10000, 1),
            'steepness_per_year': round(steepness, 4),
            'spread_2s10s': round(spread_2s10s * 10000, 1) if spread_2s10s else None,
            'spread_3m10y': round(spread_3m10y * 10000, 1) if spread_3m10y else None,
            'market_signal': self._interpret_curve_shape(shape, slope)
        }

    def _interpret_curve_shape(self, shape: str, slope: float) -> str:
        """Interpret yield curve shape for market signals."""
        if "Inverted" in shape:
            return "Potential recession signal - markets expect rate cuts"
        elif "Normal" in shape and slope > 0.02:
            return "Healthy economy expected - normal growth outlook"
        elif "Flat" in shape:
            return "Uncertain outlook - possible transition period"
        elif "Humped" in shape:
            return "Near-term uncertainty with longer-term stability expected"
        else:
            return "Standard yield curve configuration"


class SpreadAnalyzer:
    """
    Yield spread analysis for credit and relative value.
    """

    def calculate_g_spread(
        self,
        bond_ytm: float,
        treasury_ytm: float,
    ) -> Dict[str, Any]:
        """
        Calculate G-spread (Government spread).

        G-Spread = Bond YTM - Treasury YTM (at same maturity)

        Args:
            bond_ytm: Corporate bond YTM
            treasury_ytm: Treasury bond YTM

        Returns:
            Dictionary with G-spread
        """
        g_spread = bond_ytm - treasury_ytm

        return {
            'g_spread': round(g_spread, 6),
            'g_spread_bps': round(g_spread * 10000, 1),
            'bond_ytm': round(bond_ytm, 6),
            'treasury_ytm': round(treasury_ytm, 6),
            'interpretation': f"Bond yields {round(g_spread * 10000, 1)}bps over comparable Treasury"
        }

    def calculate_i_spread(
        self,
        bond_ytm: float,
        swap_rate: float,
    ) -> Dict[str, Any]:
        """
        Calculate I-spread (Interpolated spread over swaps).

        I-Spread = Bond YTM - Swap Rate (at same maturity)

        Args:
            bond_ytm: Corporate bond YTM
            swap_rate: Interest rate swap rate

        Returns:
            Dictionary with I-spread
        """
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
        Calculate Z-spread (Zero-volatility spread).

        Constant spread added to each spot rate to match bond price.

        Args:
            bond_price: Market price of bond
            cash_flows: List of (time, amount) tuples
            spot_rates: List of (maturity, rate) tuples

        Returns:
            Dictionary with Z-spread
        """
        # Create spot rate interpolator
        mat_array = np.array([sr[0] for sr in spot_rates])
        rate_array = np.array([sr[1] for sr in spot_rates])
        spot_interp = interpolate.interp1d(mat_array, rate_array, kind='linear', fill_value='extrapolate')

        def price_with_spread(z_spread):
            pv = 0
            for t, cf in cash_flows:
                spot = float(spot_interp(t))
                pv += cf / ((1 + spot + z_spread) ** t)
            return pv

        def objective(z_spread):
            return (price_with_spread(z_spread[0]) - bond_price) ** 2

        try:
            result = optimize.minimize(objective, [0.01], method='Nelder-Mead')
            z_spread = result.x[0]
        except:
            return {'error': 'Failed to calculate Z-spread'}

        return {
            'z_spread': round(z_spread, 6),
            'z_spread_bps': round(z_spread * 10000, 1),
            'bond_price': round(bond_price, 4),
            'calculated_price': round(price_with_spread(z_spread), 4),
            'interpretation': f"Constant spread of {round(z_spread * 10000, 1)}bps over spot curve"
        }

    def calculate_oas(
        self,
        bond_price: float,
        cash_flows: List[Tuple[float, float]],
        spot_rates: List[Tuple[float, float]],
        option_value: float = 0,
    ) -> Dict[str, Any]:
        """
        Calculate Option-Adjusted Spread (OAS).

        OAS = Z-Spread - Option Cost (for callable bonds)

        Args:
            bond_price: Market price of bond
            cash_flows: List of (time, amount) tuples
            spot_rates: List of (maturity, rate) tuples
            option_value: Value of embedded option (negative for calls)

        Returns:
            Dictionary with OAS
        """
        # First calculate Z-spread
        z_result = self.calculate_z_spread(bond_price, cash_flows, spot_rates)

        if 'error' in z_result:
            return z_result

        z_spread = z_result['z_spread']
        oas = z_spread - option_value

        return {
            'oas': round(oas, 6),
            'oas_bps': round(oas * 10000, 1),
            'z_spread': round(z_spread, 6),
            'z_spread_bps': round(z_spread * 10000, 1),
            'option_cost': round(option_value, 6),
            'option_cost_bps': round(option_value * 10000, 1),
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
        """
        Calculate and compare multiple spread measures.

        Args:
            bond_ytm: Corporate bond YTM
            treasury_ytm: Treasury YTM
            swap_rate: Swap rate
            bond_price: Optional - for Z-spread
            cash_flows: Optional - for Z-spread
            spot_rates: Optional - for Z-spread

        Returns:
            Dictionary with spread comparison
        """
        spreads = {}

        # G-spread
        g_result = self.calculate_g_spread(bond_ytm, treasury_ytm)
        spreads['g_spread'] = g_result

        # I-spread
        i_result = self.calculate_i_spread(bond_ytm, swap_rate)
        spreads['i_spread'] = i_result

        # Z-spread (if inputs provided)
        if bond_price and cash_flows and spot_rates:
            z_result = self.calculate_z_spread(bond_price, cash_flows, spot_rates)
            spreads['z_spread'] = z_result

        return {
            'spread_comparison': spreads,
            'summary': {
                'g_spread_bps': spreads['g_spread']['g_spread_bps'],
                'i_spread_bps': spreads['i_spread']['i_spread_bps'],
                'z_spread_bps': spreads.get('z_spread', {}).get('z_spread_bps')
            }
        }


def run_yield_curve_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main entry point for yield curve analysis.

    Args:
        params: Analysis parameters

    Returns:
        Analysis results
    """
    analysis_type = params.get('analysis_type', 'bootstrap')

    try:
        if analysis_type == 'bootstrap':
            builder = YieldCurveBuilder()
            return builder.bootstrap_spot_curve(
                bonds=params.get('bonds', []),
                frequency=params.get('frequency', 2)
            )

        elif analysis_type == 'forward_curve':
            builder = YieldCurveBuilder()
            return builder.calculate_forward_curve(
                spot_rates=params.get('spot_rates', []),
                forward_periods=params.get('forward_periods')
            )

        elif analysis_type == 'nelson_siegel':
            builder = YieldCurveBuilder()
            return builder.fit_nelson_siegel(
                maturities=params.get('maturities', []),
                yields=params.get('yields', [])
            )

        elif analysis_type == 'svensson':
            builder = YieldCurveBuilder()
            return builder.fit_svensson(
                maturities=params.get('maturities', []),
                yields=params.get('yields', [])
            )

        elif analysis_type == 'interpolate':
            builder = YieldCurveBuilder()
            method = InterpolationMethod(params.get('method', 'cubic_spline'))
            return builder.interpolate_curve(
                maturities=params.get('maturities', []),
                yields=params.get('yields', []),
                target_maturities=params.get('target_maturities', []),
                method=method
            )

        elif analysis_type == 'curve_shape':
            builder = YieldCurveBuilder()
            return builder.analyze_curve_shape(
                maturities=params.get('maturities', []),
                yields=params.get('yields', [])
            )

        elif analysis_type == 'g_spread':
            analyzer = SpreadAnalyzer()
            return analyzer.calculate_g_spread(
                bond_ytm=params.get('bond_ytm', 0.05),
                treasury_ytm=params.get('treasury_ytm', 0.03)
            )

        elif analysis_type == 'z_spread':
            analyzer = SpreadAnalyzer()
            return analyzer.calculate_z_spread(
                bond_price=params.get('bond_price', 1000),
                cash_flows=params.get('cash_flows', []),
                spot_rates=params.get('spot_rates', [])
            )

        elif analysis_type == 'oas':
            analyzer = SpreadAnalyzer()
            return analyzer.calculate_oas(
                bond_price=params.get('bond_price', 1000),
                cash_flows=params.get('cash_flows', []),
                spot_rates=params.get('spot_rates', []),
                option_value=params.get('option_value', 0)
            )

        elif analysis_type == 'compare_spreads':
            analyzer = SpreadAnalyzer()
            return analyzer.compare_spreads(
                bond_ytm=params.get('bond_ytm', 0.05),
                treasury_ytm=params.get('treasury_ytm', 0.03),
                swap_rate=params.get('swap_rate', 0.035),
                bond_price=params.get('bond_price'),
                cash_flows=params.get('cash_flows'),
                spot_rates=params.get('spot_rates')
            )

        else:
            return {'error': f'Unknown analysis type: {analysis_type}'}

    except Exception as e:
        logger.error(f"Yield curve analysis error: {str(e)}")
        return {'error': str(e)}


if __name__ == "__main__":
    import sys
    import json

    if len(sys.argv) > 1:
        try:
            params = json.loads(sys.argv[1])
            result = run_yield_curve_analysis(params)
            print(json.dumps(result, indent=2))
        except json.JSONDecodeError as e:
            print(json.dumps({'error': f'Invalid JSON: {str(e)}'}))
    else:
        # Demo
        print("Yield Curve Demo:")

        builder = YieldCurveBuilder()

        # Sample data
        maturities = [0.25, 0.5, 1, 2, 3, 5, 7, 10, 20, 30]
        yields = [0.045, 0.046, 0.047, 0.048, 0.049, 0.050, 0.051, 0.052, 0.053, 0.054]

        # Curve shape analysis
        result = builder.analyze_curve_shape(maturities, yields)
        print(f"\nCurve Shape: {result['curve_shape']}")
        print(f"Slope: {result['slope_bps']}bps")

        # Nelson-Siegel fit
        ns_result = builder.fit_nelson_siegel(maturities, yields)
        print(f"\nNelson-Siegel R-squared: {ns_result['fit_statistics']['r_squared']}")
