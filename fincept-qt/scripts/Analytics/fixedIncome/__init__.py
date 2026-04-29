"""
Fixed Income Analytics Module
============================

Comprehensive fixed income analytics providing CFA Institute standard methodologies
for bond pricing, duration/convexity analysis, yield curve construction, credit analysis,
structured products evaluation, and sovereign credit analysis.

Modules:
--------
- bond_pricing: Bond valuation, YTM, YTC, YTW, spot rates
- duration_convexity: Macaulay, Modified, Effective duration and convexity
- yield_curve: Term structure, bootstrapping, spread analysis
- credit_analysis: Credit risk, default probability, recovery rates
- structured_products: MBS, ABS, prepayment models, WAL
- bond_portfolio: Portfolio analytics, immunization, liability matching
- bond_features: Bond types, covenants, contingency provisions
- market_structure: Fixed income market segments, repos, indexes
- floating_rate: FRN pricing, money market instruments
- sovereign_credit: Sovereign and municipal credit analysis
"""


__all__ = [
    # Bond Pricing
    'BondPricer',
    'BondType',
    'CouponFrequency',
    'DayCountConvention',
    # Duration & Convexity
    'DurationCalculator',
    'ConvexityCalculator',
    # Yield Curve
    'YieldCurveBuilder',
    'SpreadAnalyzer',
    # Credit Analysis
    'CreditAnalyzer',
    'DefaultProbabilityModel',
    # Structured Products
    'MBSAnalyzer',
    'ABSAnalyzer',
    'PrepaymentModel',
    # Portfolio
    'BondPortfolioAnalyzer',
    'ImmunizationStrategy',
    # Bond Features
    'BondFeaturesAnalyzer',
    # Market Structure
    'MarketStructureAnalyzer',
    # Floating Rate
    'FloatingRateAnalyzer',
    'MoneyMarketAnalyzer',
    # Sovereign Credit
    'SovereignCreditAnalyzer',
    'MunicipalCreditAnalyzer',
    'GovernmentVsCorporateComparison',
]

__version__ = '1.1.0'


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "BondPricer": ("bond_pricing", "BondPricer"),
    "BondType": ("bond_pricing", "BondType"),
    "CouponFrequency": ("bond_pricing", "CouponFrequency"),
    "DayCountConvention": ("bond_pricing", "DayCountConvention"),
    "DurationCalculator": ("duration_convexity", "DurationCalculator"),
    "ConvexityCalculator": ("duration_convexity", "ConvexityCalculator"),
    "YieldCurveBuilder": ("yield_curve", "YieldCurveBuilder"),
    "SpreadAnalyzer": ("yield_curve", "SpreadAnalyzer"),
    "CreditAnalyzer": ("credit_analysis", "CreditAnalyzer"),
    "DefaultProbabilityModel": ("credit_analysis", "DefaultProbabilityModel"),
    "MBSAnalyzer": ("structured_products", "MBSAnalyzer"),
    "ABSAnalyzer": ("structured_products", "ABSAnalyzer"),
    "PrepaymentModel": ("structured_products", "PrepaymentModel"),
    "BondPortfolioAnalyzer": ("bond_portfolio", "BondPortfolioAnalyzer"),
    "ImmunizationStrategy": ("bond_portfolio", "ImmunizationStrategy"),
    "BondFeaturesAnalyzer": ("bond_features", "BondFeaturesAnalyzer"),
    "MarketStructureAnalyzer": ("market_structure", "MarketStructureAnalyzer"),
    "FloatingRateAnalyzer": ("floating_rate", "FloatingRateAnalyzer"),
    "MoneyMarketAnalyzer": ("floating_rate", "MoneyMarketAnalyzer"),
    "SovereignCreditAnalyzer": ("sovereign_credit", "SovereignCreditAnalyzer"),
    "MunicipalCreditAnalyzer": ("sovereign_credit", "MunicipalCreditAnalyzer"),
    "GovernmentVsCorporateComparison": ("sovereign_credit", "GovernmentVsCorporateComparison"),
}


def __getattr__(name: str):  # PEP 562
    target = _LAZY_ATTRS.get(name)
    if target is None:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
    submodule, original_name = target
    import importlib
    mod = importlib.import_module(f".{submodule}", __name__)
    value = getattr(mod, original_name)
    globals()[name] = value  # cache for subsequent access
    return value


def __dir__() -> list[str]:
    return sorted(set(globals()) | set(_LAZY_ATTRS))
