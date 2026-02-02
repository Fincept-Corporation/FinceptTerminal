"""emerging_market_bonds Module"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config
)
from base_analytics import AlternativeInvestmentBase, FinancialMath

logger = logging.getLogger(__name__)


class EmergingMarketBondAnalyzer(AlternativeInvestmentBase):
    """
    Emerging Market Bond Analyzer

    CFA Standards: Fixed Income - Emerging Market Debt, Sovereign Risk

    Key Concepts from Key insight: - EM bonds ≈ High-yield bonds (similar risk/return profile)
    - Currency risk (local vs hard currency)
    - Sovereign default risk
    - Political and economic instability
    - Correlation with EM equities (diversification limited)
    - Better alternative: Treasury bonds for safety, EM equities for EM exposure

    Verdict: "The Flawed" - Inferior to alternatives, equity-like risk without equity returns
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.face_value = parameters.face_value if hasattr(parameters, 'face_value') else Decimal('1000')
        self.coupon_rate = parameters.coupon_rate if hasattr(parameters, 'coupon_rate') else Decimal('0.06')
        self.maturity_years = parameters.maturity_years if hasattr(parameters, 'maturity_years') else 10
        self.current_price = parameters.current_market_value if hasattr(parameters, 'current_market_value') else self.face_value

        # Emerging market specific
        self.currency = parameters.currency if hasattr(parameters, 'currency') else 'USD'  # Hard currency
        self.country = parameters.country if hasattr(parameters, 'country') else 'Generic EM'
        self.sovereign_rating = parameters.sovereign_rating if hasattr(parameters, 'sovereign_rating') else 'BB'

        # Risk metrics
        self.credit_spread = parameters.credit_spread if hasattr(parameters, 'credit_spread') else Decimal('0.04')

    def calculate_yield_metrics(self) -> Dict[str, Any]:
        """
        Calculate yield to maturity and spreads

        CFA: YTM includes coupon income + capital gain/loss
        EM spread over Treasuries reflects credit and political risk

        Returns:
            Yield analysis
        """
        annual_coupon = self.coupon_rate * self.face_value

        # Current yield
        current_yield = annual_coupon / self.current_price

        # Approximate YTM using bond pricing formula
        years_to_maturity = Decimal(str(self.maturity_years))
        capital_gain_per_year = (self.face_value - self.current_price) / years_to_maturity
        average_price = (self.face_value + self.current_price) / Decimal('2')

        approximate_ytm = (annual_coupon + capital_gain_per_year) / average_price

        # Spread over Treasuries
        treasury_yield = self.config.RISK_FREE_RATE
        spread_over_treasuries = approximate_ytm - treasury_yield

        return {
            'coupon_rate': float(self.coupon_rate),
            'current_yield': float(current_yield),
            'yield_to_maturity': float(approximate_ytm),
            'treasury_yield': float(treasury_yield),
            'spread_over_treasuries': float(spread_over_treasuries),
            'spread_basis_points': float(spread_over_treasuries * Decimal('10000')),
            'interpretation': self._interpret_spread(spread_over_treasuries)
        }

    def _interpret_spread(self, spread: Decimal) -> str:
        """Interpret spread level"""
        if spread < Decimal('0.02'):
            return 'Low spread - Investment grade quality'
        elif spread < Decimal('0.04'):
            return 'Moderate spread - Lower investment grade / high BB'
        elif spread < Decimal('0.06'):
            return 'High spread - Speculative grade (B)'
        else:
            return 'Very high spread - Highly speculative / distressed'

    def sovereign_default_risk_analysis(self, historical_default_rate: Decimal,
                                       recovery_rate: Decimal = Decimal('0.30')) -> Dict[str, Any]:
        """
        Analyze sovereign default risk

        Finding: EM bonds have REAL default risk
        Historical default rates: ~3-5% annually for speculative grade

        Args:
            historical_default_rate: Annual default probability for rating class
            recovery_rate: Expected recovery in default (30% typical for sovereigns)

        Returns:
            Default risk analysis
        """
        # Expected loss
        expected_loss = historical_default_rate * (Decimal('1') - recovery_rate)

        # Risk premium required to compensate
        required_spread = expected_loss

        # Actual spread
        actual_spread = self.credit_spread

        # Excess spread (compensation beyond expected loss)
        excess_spread = actual_spread - required_spread

        # Probability of default over holding period
        cumulative_default_prob = Decimal('1') - ((Decimal('1') - historical_default_rate) ** Decimal(str(self.maturity_years)))

        return {
            'sovereign_rating': self.sovereign_rating,
            'annual_default_probability': float(historical_default_rate),
            'cumulative_default_probability': float(cumulative_default_prob),
            'recovery_rate': float(recovery_rate),
            'expected_loss': float(expected_loss),
            'required_spread': float(required_spread),
            'actual_spread': float(actual_spread),
            'excess_spread': float(excess_spread),
            'adequately_compensated': excess_spread > 0,
            'risk_assessment': self._assess_sovereign_risk(self.sovereign_rating, historical_default_rate)
        }

    def _assess_sovereign_risk(self, rating: str, default_prob: Decimal) -> str:
        """Assess sovereign risk level"""
        if default_prob < Decimal('0.005'):
            return 'Low risk - Investment grade sovereign'
        elif default_prob < Decimal('0.02'):
            return 'Moderate risk - Lower investment grade'
        elif default_prob < Decimal('0.05'):
            return 'High risk - Speculative grade'
        else:
            return 'Very high risk - Distressed sovereign'

    def currency_risk_analysis(self, local_currency_volatility: Decimal,
                              fx_correlation_with_returns: Decimal = Decimal('-0.30')) -> Dict[str, Any]:
        """
        Analyze currency risk for local currency bonds

        Key insight: Currency risk AMPLIFIES volatility
        Local currency bonds have FX risk + interest rate risk + credit risk

        Args:
            local_currency_volatility: Volatility of EM currency vs USD
            fx_correlation_with_returns: Correlation between FX and bond returns

        Returns:
            Currency risk analysis
        """
        if self.currency == 'USD' or self.currency == 'EUR':
            return {
                'bond_currency': self.currency,
                'currency_risk': 'None - Hard currency (USD or EUR) denominated',
                'recommendation': 'No FX hedging needed'
            }

        # Estimate total volatility with currency risk
        # σ_total² = σ_bond² + σ_fx² + 2ρσ_bondσ_fx

        # Assume bond-only volatility
        bond_volatility = Decimal('0.10')  # 10% typical for EM hard currency bonds

        # Total volatility with FX
        variance_total = (
            bond_volatility ** 2 +
            local_currency_volatility ** 2 +
            Decimal('2') * fx_correlation_with_returns * bond_volatility * local_currency_volatility
        )

        total_volatility = variance_total ** Decimal('0.5')

        # Additional risk from currency
        additional_risk = total_volatility - bond_volatility

        return {
            'bond_currency': self.currency,
            'currency_type': 'Local Currency (EM)',
            'bond_volatility_only': float(bond_volatility),
            'fx_volatility': float(local_currency_volatility),
            'fx_bond_correlation': float(fx_correlation_with_returns),
            'total_volatility_with_fx': float(total_volatility),
            'additional_risk_from_fx': float(additional_risk),
            'risk_increase_percentage': float(additional_risk / bond_volatility),
            'analysis_warning': 'Local currency bonds have AMPLIFIED risk from FX volatility',
            'recommendation': 'Consider hard currency (USD) EM bonds or hedge FX exposure'
        }

    def correlation_with_em_equities(self, em_equity_returns: List[Decimal]) -> Dict[str, Any]:
        """
        Analyze correlation with EM equities

        Finding: EM bonds have HIGH correlation with EM stocks (~0.50-0.70)
        This LIMITS diversification benefit

        Args:
            em_equity_returns: EM equity returns

        Returns:
            Correlation analysis
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient bond data'}

        # Calculate EM bond returns
        em_bond_returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            em_bond_returns.append(ret)

        # Ensure equal lengths
        min_length = min(len(em_bond_returns), len(em_equity_returns))
        em_bond_returns = em_bond_returns[:min_length]
        em_equity_returns = em_equity_returns[:min_length]

        if min_length < 2:
            return {'error': 'Insufficient data for correlation'}

        # Calculate correlation
        bond_array = np.array([float(r) for r in em_bond_returns])
        equity_array = np.array([float(r) for r in em_equity_returns])

        correlation = np.corrcoef(bond_array, equity_array)[0, 1]

        return {
            'correlation_em_bonds_em_equities': float(correlation),
            'correlation_interpretation': self._interpret_em_correlation(correlation),
            'diversification_benefit': 'Low' if correlation > 0.60 else 'Moderate',
            'analysis_finding': 'EM bonds correlate 0.50-0.70 with EM equities - limited diversification',
            'implication': 'If holding EM equities, EM bonds add little diversification value'
        }

    def _interpret_em_correlation(self, corr: float) -> str:
        """Interpret EM bond-equity correlation"""
        if corr > 0.70:
            return 'Very High - EM bonds move closely with EM stocks'
        elif corr > 0.50:
            return 'High - Significant co-movement (finding)'
        elif corr > 0.30:
            return 'Moderate - Some diversification benefit'
        else:
            return 'Low - Good diversification'

    def compare_to_high_yield_bonds(self, hy_yield: Decimal, hy_default_rate: Decimal,
                                    hy_volatility: Decimal) -> Dict[str, Any]:
        """
        Compare EM bonds to High-Yield corporate bonds

        Key insight: EM bonds ≈ High-yield bonds (similar risk/return profile)
        But EM bonds have ADDITIONAL risks: political, currency, legal

        Args:
            hy_yield: High-yield bond yield
            hy_default_rate: HY default rate
            hy_volatility: HY volatility

        Returns:
            Comparison analysis
        """
        em_yield = self.calculate_yield_metrics()['yield_to_maturity']

        # Assume EM default rate slightly higher than HY
        em_default_rate = hy_default_rate * Decimal('1.20')

        # EM volatility typically similar or higher
        em_volatility = hy_volatility * Decimal('1.10') if self.currency != 'USD' else hy_volatility

        # Sharpe ratios
        rf = self.config.RISK_FREE_RATE
        em_sharpe = (Decimal(str(em_yield)) - rf) / em_volatility if em_volatility > 0 else Decimal('0')
        hy_sharpe = (hy_yield - rf) / hy_volatility if hy_volatility > 0 else Decimal('0')

        return {
            'em_bonds': {
                'yield': em_yield,
                'default_rate': float(em_default_rate),
                'volatility': float(em_volatility),
                'sharpe_ratio': float(em_sharpe),
                'additional_risks': ['Political risk', 'Currency risk', 'Legal/enforcement risk', 'Concentration risk']
            },
            'high_yield_bonds': {
                'yield': float(hy_yield),
                'default_rate': float(hy_default_rate),
                'volatility': float(hy_volatility),
                'sharpe_ratio': float(hy_sharpe),
                'additional_risks': ['Corporate governance']
            },
            'winner': 'High-Yield' if hy_sharpe > em_sharpe else 'EM Bonds',
            'analysis_conclusion': 'EM bonds and HY bonds have SIMILAR profiles - but EM has MORE risks',
            'recommendation': 'If choosing between them, HY bonds preferable (less political/FX risk)'
        }

    def compare_to_alternatives(self, treasury_yield: Decimal,
                                em_equity_expected_return: Decimal) -> Dict[str, Any]:
        """
        Compare EM bonds to better alternatives

        Recommendation:
        - For SAFETY: Use Treasury bonds (eliminate credit/political risk)
        - For EM EXPOSURE: Use EM equities (better risk-adjusted returns, cleaner EM exposure)

        Args:
            treasury_yield: US Treasury yield
            em_equity_expected_return: Expected return on EM equities

        Returns:
            Alternative analysis
        """
        em_bond_yield = Decimal(str(self.calculate_yield_metrics()['yield_to_maturity']))

        return {
            'em_bonds': {
                'expected_return': float(em_bond_yield),
                'risk_level': 'High (credit + political + possibly FX)',
                'role': 'EM exposure + fixed income',
                'problems': [
                    'Equity-like risk without equity returns',
                    'High correlation with EM stocks (poor diversifier)',
                    'Default risk',
                    'Currency risk if local currency'
                ]
            },
            'treasury_bonds': {
                'expected_return': float(treasury_yield),
                'risk_level': 'Very Low (credit risk-free)',
                'role': 'Pure fixed income / safety',
                'advantages': [
                    'No credit risk',
                    'No political risk',
                    'Highly liquid',
                    'Negative correlation with stocks (true diversifier)'
                ]
            },
            'em_equities': {
                'expected_return': float(em_equity_expected_return),
                'risk_level': 'High (equity risk)',
                'role': 'EM exposure / growth',
                'advantages': [
                    'Higher expected returns than EM bonds',
                    'Direct EM economic exposure',
                    'Better risk-adjusted returns historically',
                    'No maturity/duration constraints'
                ]
            },
            'analysis_recommendation': (
                'AVOID EM bonds. Use Treasuries for safety, EM equities for EM exposure. '
                'EM bonds are the WORST of both worlds: equity-like risk with bond-like returns.'
            ),
            'optimal_portfolio': 'US Treasuries (safety) + EM Equities (growth) > EM Bonds'
        }

    def political_and_legal_risk_assessment(self) -> Dict[str, Any]:
        """
        Assess political and legal risks unique to EM bonds

        Key insight: EM bonds have risks absent in developed market bonds
        - Government regime changes
        - Capital controls
        - Expropriation
        - Weak legal systems
        - Corruption

        Returns:
            Political risk analysis
        """
        # Risk factors by country development stage
        risk_factors = {
            'political_stability': 'Moderate to Low',
            'legal_system_strength': 'Weak enforcement of creditor rights',
            'capital_controls_risk': 'Moderate - some EMs impose controls in crisis',
            'expropriation_risk': 'Low but non-zero',
            'corruption': 'Elevated vs developed markets',
            'regime_change_risk': 'Higher than developed markets'
        }

        return {
            'country': self.country,
            'sovereign_rating': self.sovereign_rating,
            'political_risk_factors': risk_factors,
            'legal_enforcement': 'Weak - difficult to enforce claims in default',
            'capital_controls_history': 'Several EMs imposed controls during crises',
            'creditor_recovery': 'Lower and slower than corporate defaults',
            'analysis_warning': (
                'EM sovereign bonds have risks absent in corporate or developed market bonds: '
                'political instability, weak legal systems, potential capital controls'
            ),
            'implication': 'These risks are NOT adequately compensated by modest yield pickup'
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """
        Complete analytical verdict on Emerging Market Bonds
        Based on "Alternative Investments Analysis"

        Category: "THE FLAWED"

        Returns:
            Complete verdict with recommendations
        """
        return {
            'asset_class': 'Emerging Market Bonds',
            'category': 'THE FLAWED',
            'overall_rating': '4/10 - Not recommended',

            'the_good': [
                'Higher yields than Treasury bonds',
                'Exposure to EM economies',
                'Diversification from US assets (geographic)',
                'Can outperform in specific periods (EM growth phases)'
            ],

            'the_bad': [
                'Equity-like risk with bond-like returns (worst of both)',
                'HIGH correlation with EM equities (0.50-0.70) - poor diversifier',
                'Significant default risk (3-5% annual for speculative grade)',
                'Currency risk for local currency bonds (amplifies volatility)',
                'Political and legal risks absent in developed markets',
                'Weak creditor rights and enforcement',
                'Liquidity risk (harder to sell in crisis)',
                'Lower recovery rates in default vs corporate bonds'
            ],

            'the_ugly': [
                'Marketed as "bond diversification" but behaves like EM equities',
                'Investors get equity-like volatility without equity returns',
                'Better alternatives exist: Treasuries (safety) + EM Equities (EM exposure)',
                'Additional risks (political, legal, FX) NOT adequately compensated',
                'During crises, EM bonds fall WITH EM stocks (fails diversification test)'
            ],

            'key_findings': {
                'risk_adjusted_returns': 'Poor - high risk, modest returns',
                'diversification_vs_treasuries': 'Negative - gives up safety for small yield pickup',
                'diversification_vs_em_equities': 'Low - high correlation (0.50-0.70)',
                'default_risk': 'Real and significant',
                'currency_risk': 'Amplifies volatility (local currency bonds)',
                'better_alternative': 'Treasury bonds (safety) + EM Equities (EM exposure)'
            },

            'analysis_quote': (
                '"Emerging market bonds are the investment equivalent of being between a rock and '
                'a hard place. They have equity-like risk but bond-like returns. They don\'t provide '
                'the safety of Treasuries, and they don\'t provide the returns of EM equities. '
                'Investors seeking safety should use Treasury bonds. Investors seeking EM exposure '
                'should use EM equities. EM bonds are simply the worst of both worlds."'
            ),

            'investment_recommendation': {
                'suitable_for': [
                    'Sophisticated investors with SPECIFIC tactical needs',
                    'Very small allocation (<5%) in highly diversified portfolios',
                    'Investors already overweight Treasuries seeking incremental yield',
                    'ONLY hard currency (USD) EM bonds (avoid local currency FX risk)'
                ],
                'not_suitable_for': [
                    'Core fixed income allocation (use Treasuries)',
                    'EM exposure (use EM equities instead)',
                    'Conservative investors (default risk too high)',
                    'Investors seeking crisis protection (EM bonds fall in crises)',
                    'Anyone considering local currency EM bonds (FX risk excessive)'
                ],
                'better_alternatives': [
                    'US Treasury bonds (safety, liquidity, no default risk)',
                    'TIPS (inflation protection)',
                    'EM Equities (better risk-adjusted returns, cleaner EM exposure)',
                    'Investment-grade corporate bonds (lower risk than EM sovereigns)'
                ]
            },

            'final_verdict': (
                'Emerging market bonds are FLAWED investments for most portfolios. The modest yield '
                'pickup over Treasuries does NOT compensate for credit risk, political risk, currency '
                'risk (local currency), and high correlation with EM equities. Optimal strategy: '
                'Use Treasury bonds for safety and EM equities for EM exposure. EM bonds deliver '
                'neither adequately and add complexity, risk, and costs without commensurate benefits.'
            )
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """
        Calculate comprehensive EM bond metrics

        Returns:
            All key metrics
        """
        yield_metrics = self.calculate_yield_metrics()

        return {
            'security_type': 'Emerging Market Bond',
            'country': self.country,
            'currency': self.currency,
            'sovereign_rating': self.sovereign_rating,
            'face_value': float(self.face_value),
            'coupon_rate': float(self.coupon_rate),
            'maturity_years': self.maturity_years,
            'current_price': float(self.current_price),
            'yield_metrics': yield_metrics,
            'analysis_category': 'THE FLAWED',
            'risk_level': 'High - credit + political + possibly FX',
            'recommendation': 'Avoid - use Treasuries (safety) + EM Equities (EM exposure) instead'
        }

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV"""
        return self.current_price

    def calculate_performance(self) -> Dict[str, Any]:
        """
        Calculate performance metrics

        Returns:
            Performance analysis
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient data'}

        returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            returns.append(ret)

        if not returns:
            return {'error': 'No returns calculated'}

        avg_return = sum(returns) / len(returns)
        volatility = self.math.calculate_volatility(returns, annualized=True)
        sharpe = self.math.sharpe_ratio(returns, self.config.RISK_FREE_RATE)

        return {
            'average_return': float(avg_return),
            'volatility': float(volatility),
            'sharpe_ratio': float(sharpe),
            'observation_count': len(returns),
            'note': 'Equity-like volatility with bond-like returns - poor risk/return profile'
        }

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive EM bond valuation summary"""
        return {
            "asset_overview": {
                "security_type": "Emerging Market Bond",
                "country": self.country,
                "currency": self.currency,
                "sovereign_rating": self.sovereign_rating,
                "face_value": float(self.face_value),
                "coupon_rate": float(self.coupon_rate),
                "maturity_years": self.maturity_years
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE FLAWED",
            "recommendation": "Avoid - use Treasuries (safety) + EM Equities (EM exposure) instead"
        }


# Export
__all__ = ['EmergingMarketBondAnalyzer']
