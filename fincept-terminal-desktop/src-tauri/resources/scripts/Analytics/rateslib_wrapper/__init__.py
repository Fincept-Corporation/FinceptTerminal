"""
Rateslib Wrapper Package for Fincept Terminal
==============================================

A comprehensive Python wrapper for the rateslib fixed income library,
providing CFA-level analytics for interest rate derivatives, bonds,
FX products, and more.

Modules:
    - swaps_analytics: Interest rate swaps (IRS, XCS, FRA, SBS, ZCIS, ZCS, IIRS, NDF, Spread, Fly)
    - bonds_analytics: Bond pricing and analytics (Fixed, Float, Index, Bills)
    - fx_analytics: FX rates, forwards, and options (FXRates, FXForwards, Options, Strategies)
    - derivatives_analytics: Futures and portfolio management (STIR, Bond Futures, Portfolios)
    - scheduling_analytics: Date scheduling and calendars (Schedule, IMM dates, Business days)
    - credit_analytics: Credit default swaps and credit derivatives (CDS, Premium/Protection legs)
    - legs_periods: Legs and periods building blocks (Fixed, Float, Zero, Index, MTM legs)
    - volatility_analytics: FX volatility surfaces (Delta Vol, SABR smiles and surfaces)
    - dual_analytics: Automatic differentiation with dual numbers (Dual, Dual2, AD operations)
    - splines_analytics: Spline interpolation (PPSpline, B-splines)
    - utilities: Helper functions (from_json, index operations, Defaults)
    - curves_analytics: Yield curves (Curve, LineCurve, CompositeCurve, ProxyCurve, MultiCsaCurve)

Example:
    >>> from rateslib_wrapper.swaps_analytics import SwapsAnalytics
    >>> analytics = SwapsAnalytics()
    >>> irs = analytics.create_irs(...)

Version: 2.0.0 (Extended Coverage)
Rateslib Version: 2.1.1
Coverage: 75+ classes, 12+ standalone functions
"""

__version__ = "2.0.0"
__rateslib_version__ = "2.1.1"
__author__ = "Fincept Corporation"
__license__ = "MIT"

# Import analytics classes
from .swaps_analytics import SwapsAnalytics, SwapConfig
from .bonds_analytics import BondsAnalytics, BondConfig
from .fx_analytics import FXAnalytics, FXConfig
from .derivatives_analytics import DerivativesAnalytics, DerivativesConfig
from .scheduling_analytics import SchedulingAnalytics, SchedulingConfig
from .credit_analytics import CreditAnalytics, CreditConfig
from .legs_periods import LegsPeriodsAnalytics, LegConfig
from .volatility_analytics import VolatilityAnalytics, VolatilityConfig
from .dual_analytics import DualAnalytics, DualConfig
from .splines_analytics import SplinesAnalytics, SplineConfig
from .utilities import Utilities, UtilConfig
from .curves_analytics import CurvesAnalytics, CurveConfig

# Define public API
__all__ = [
    # Swaps
    'SwapsAnalytics',
    'SwapConfig',

    # Bonds
    'BondsAnalytics',
    'BondConfig',

    # FX
    'FXAnalytics',
    'FXConfig',

    # Derivatives
    'DerivativesAnalytics',
    'DerivativesConfig',

    # Scheduling
    'SchedulingAnalytics',
    'SchedulingConfig',

    # Credit
    'CreditAnalytics',
    'CreditConfig',

    # Legs and Periods
    'LegsPeriodsAnalytics',
    'LegConfig',

    # Volatility
    'VolatilityAnalytics',
    'VolatilityConfig',

    # Dual Numbers (AD)
    'DualAnalytics',
    'DualConfig',

    # Splines
    'SplinesAnalytics',
    'SplineConfig',

    # Utilities
    'Utilities',
    'UtilConfig',

    # Curves
    'CurvesAnalytics',
    'CurveConfig',

    # Package metadata
    '__version__',
    '__rateslib_version__',
    '__author__',
    '__license__',
]


def get_version_info():
    """Get version information for the wrapper and rateslib"""
    import rateslib as rl

    return {
        'wrapper_version': __version__,
        'rateslib_version': __rateslib_version__,
        'rateslib_installed': hasattr(rl, '__version__') and rl.__version__ or 'unknown',
        'author': __author__,
        'license': __license__
    }


def print_info():
    """Print package information"""
    info = get_version_info()
    print("=" * 80)
    print("Rateslib Wrapper for Fincept Terminal - EXTENDED COVERAGE")
    print("=" * 80)
    print(f"Wrapper Version:    {info['wrapper_version']}")
    print(f"Rateslib Version:   {info['rateslib_version']}")
    print(f"Installed Version:  {info['rateslib_installed']}")
    print(f"Author:             {info['author']}")
    print(f"License:            {info['license']}")
    print(f"Coverage:           75+ classes, 12+ functions")
    print("=" * 80)
    print("\nAvailable Modules (12 Total):")
    print("  1. SwapsAnalytics        - IRS, XCS, FRA, SBS, ZCIS, ZCS, IIRS, NDF, Spread, Fly")
    print("  2. BondsAnalytics        - FixedRateBond, FloatRateNote, Bill, IndexFixedRateBond")
    print("  3. FXAnalytics           - FXRates, FXForwards, FXExchange, FXSwap, Options, FXBrokerFly")
    print("  4. DerivativesAnalytics  - STIRFuture, BondFuture, Portfolio, Value, VolValue")
    print("  5. SchedulingAnalytics   - Schedule, IMM dates, Calendars, DCF, Business days")
    print("  6. CreditAnalytics       - CDS, CreditPremiumLeg, CreditProtectionLeg, CS01")
    print("  7. LegsPeriodsAnalytics  - FixedLeg, FloatLeg, ZeroLeg, IndexLeg, MTM Legs, Periods")
    print("  8. VolatilityAnalytics   - FXDeltaVolSmile, FXDeltaVolSurface, FXSabrSmile, FXSabrSurface")
    print("  9. DualAnalytics         - Dual, Dual2, Variable, AD operations, Gradient")
    print(" 10. SplinesAnalytics      - PPSplineF64, PPSplineDual, PPSplineDual2, B-splines")
    print(" 11. Utilities             - from_json, index_left, index_value, Defaults")
    print(" 12. CurvesAnalytics       - Curve, LineCurve, CompositeCurve, ProxyCurve, MultiCsaCurve")
    print("=" * 80)
    print("\nClass Coverage Summary:")
    print("  Swaps & Derivatives:  IRS, XCS, FRA, SBS, ZCIS, ZCS, IIRS, NDF, Spread, Fly")
    print("  Bonds:                FixedRateBond, FloatRateNote, Bill, IndexFixedRateBond")
    print("  FX:                   FXRates, FXForwards, FXExchange, FXSwap, FXCall, FXPut,")
    print("                        FXStraddle, FXRiskReversal, FXStrangle, FXBrokerFly")
    print("  Futures:              STIRFuture, BondFuture, Portfolio")
    print("  Credit:               CDS, CreditPremiumLeg, CreditProtectionLeg")
    print("  Legs:                 FixedLeg, FloatLeg, IndexFixedLeg, ZeroFixedLeg,")
    print("                        ZeroFloatLeg, ZeroIndexLeg, FixedLegMtm, FloatLegMtm")
    print("  Periods:              FixedPeriod, FloatPeriod, Cashflow, IndexCashflow")
    print("  Volatility:           FXDeltaVolSmile, FXDeltaVolSurface, FXSabrSmile, FXSabrSurface")
    print("  AD:                   Dual, Dual2, Variable")
    print("  Splines:              PPSplineF64, PPSplineDual, PPSplineDual2")
    print("  Curves:               Curve, LineCurve, CompositeCurve, ProxyCurve, MultiCsaCurve")
    print("  Scheduling:           Schedule, Cal, NamedCal, UnionCal, Frequency, Imm, RollDay")
    print("=" * 80)
    print("\nFunction Coverage:")
    print("  - get_imm, next_imm, from_json, index_left, index_value")
    print("  - dual_exp, dual_log, dual_solve, gradient")
    print("  - bsplev_single, bspldnev_single")
    print("  - dcf, add_tenor, get_calendar, is_bus_day, adjust")
    print("=" * 80)


# Initialize message (optional - can be commented out for production)
if __name__ != "__main__":
    # Only show in interactive mode
    pass
