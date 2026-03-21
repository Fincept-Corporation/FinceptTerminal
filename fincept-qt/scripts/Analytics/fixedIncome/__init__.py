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

from .bond_pricing import (
    BondPricer,
    BondType,
    CouponFrequency,
    DayCountConvention,
)

from .duration_convexity import (
    DurationCalculator,
    ConvexityCalculator,
)

from .yield_curve import (
    YieldCurveBuilder,
    SpreadAnalyzer,
)

from .credit_analysis import (
    CreditAnalyzer,
    DefaultProbabilityModel,
)

from .structured_products import (
    MBSAnalyzer,
    ABSAnalyzer,
    PrepaymentModel,
)

from .bond_portfolio import (
    BondPortfolioAnalyzer,
    ImmunizationStrategy,
)

from .bond_features import (
    BondFeaturesAnalyzer,
)

from .market_structure import (
    MarketStructureAnalyzer,
)

from .floating_rate import (
    FloatingRateAnalyzer,
    MoneyMarketAnalyzer,
)

from .sovereign_credit import (
    SovereignCreditAnalyzer,
    MunicipalCreditAnalyzer,
    GovernmentVsCorporateComparison,
)

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
