"""high_yield_bonds Module"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging
from enum import Enum

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config
)
from base_analytics import AlternativeInvestmentBase, FinancialMath

logger = logging.getLogger(__name__)


class CreditRating(Enum):
    """Credit rating categories"""
    # Investment Grade
    AAA = "AAA"
    AA = "AA"
    A = "A"
    BBB = "BBB"

    # High Yield (Junk)
    BB = "BB"
    B = "B"
    CCC = "CCC"
    CC = "CC"
    C = "C"
    D = "D"  # Default


class HighYieldBondAnalyzer(AlternativeInvestmentBase):
    """
    High-Yield (Junk) Bond Analyzer

    CFA Standards: Credit risk, Default risk, Recovery rates

    Key Findings:
    - High-yield bonds behave more like equities than bonds
    - Low correlation with Treasuries misleading - it's equity-like behavior
    - Credit risk is not well compensated historically
    - Better alternatives exist (investment-grade bonds + small-cap value stocks)
    - Asset location problem (hybrid nature)
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.face_value = parameters.acquisition_price if hasattr(parameters, 'acquisition_price') else Decimal('1000')
        self.coupon_rate = parameters.coupon_rate if hasattr(parameters, 'coupon_rate') else Decimal('0.08')
        self.current_price = parameters.current_market_value if hasattr(parameters, 'current_market_value') else self.face_value
        self.credit_rating = parameters.credit_rating if hasattr(parameters, 'credit_rating') else 'BB'
        self.maturity_years = parameters.maturity_years if hasattr(parameters, 'maturity_years') else 5
        self.sector = parameters.sector if hasattr(parameters, 'sector') else 'Industrial'

    def get_rating_tier(self) -> str:
        """Determine if investment grade or high yield"""
        investment_grade = ['AAA', 'AA', 'A', 'BBB']
        return 'Investment Grade' if self.credit_rating in investment_grade else 'High Yield (Junk)'

    def calculate_yield_to_maturity(self) -> Decimal:
        """
        Calculate yield to maturity

        CFA: YTM = IRR of bond's cash flows

        Returns:
            Yield to maturity
        """
        annual_coupon = self.coupon_rate * self.face_value
        years = Decimal(str(self.maturity_years))

        # Approximate YTM formula
        # YTM ≈ [C + (F - P) / n] / [(F + P) / 2]
        numerator = annual_coupon + (self.face_value - self.current_price) / years
        denominator = (self.face_value + self.current_price) / Decimal('2')

        ytm = numerator / denominator if denominator > 0 else Decimal('0')
        return ytm

    def calculate_yield_spread(self, treasury_yield: Decimal) -> Dict[str, Any]:
        """
        Calculate yield spread over Treasuries

        CFA: Credit Spread = YTM - Risk-free rate
        Compensation for credit risk

        Args:
            treasury_yield: Comparable maturity Treasury yield

        Returns:
            Spread analysis
        """
        ytm = self.calculate_yield_to_maturity()
        credit_spread = ytm - treasury_yield
        spread_bps = credit_spread * Constants.BASIS_POINTS

        # Typical spreads by rating
        typical_spreads = {
            'BBB': (100, 200),
            'BB': (200, 400),
            'B': (400, 700),
            'CCC': (700, 1500),
            'CC': (1500, 3000)
        }

        expected_range = typical_spreads.get(self.credit_rating, (200, 500))

        return {
            'yield_to_maturity': float(ytm),
            'treasury_yield': float(treasury_yield),
            'credit_spread': float(credit_spread),
            'spread_basis_points': float(spread_bps),
            'typical_spread_range_bps': expected_range,
            'spread_assessment': 'Wide' if spread_bps > expected_range[1] else
                                'Narrow' if spread_bps < expected_range[0] else 'Normal'
        }

    def estimate_default_probability(self, historical_default_rates: Dict[str, Decimal] = None) -> Dict[str, Any]:
        """
        Estimate probability of default based on credit rating

        CFA: Historical default rates by rating

        Args:
            historical_default_rates: Custom default rates (optional)

        Returns:
            Default probability analysis
        """
        # Historical cumulative default rates (5-year average from Moody's/S&P)
        default_rates = historical_default_rates or {
            'AAA': Decimal('0.0001'),
            'AA': Decimal('0.0005'),
            'A': Decimal('0.0015'),
            'BBB': Decimal('0.0050'),
            'BB': Decimal('0.0250'),
            'B': Decimal('0.0800'),
            'CCC': Decimal('0.2500'),
            'CC': Decimal('0.4000'),
            'C': Decimal('0.6000')
        }

        default_prob = default_rates.get(self.credit_rating, Decimal('0.10'))
        survival_prob = Decimal('1') - default_prob

        # Expected recovery rate (% of face value recovered in default)
        recovery_rates = {
            'Senior Secured': Decimal('0.65'),
            'Senior Unsecured': Decimal('0.50'),
            'Subordinated': Decimal('0.35'),
            'Junior Subordinated': Decimal('0.25')
        }
        recovery_rate = recovery_rates.get('Senior Unsecured', Decimal('0.50'))

        # Expected loss
        expected_loss = default_prob * (Decimal('1') - recovery_rate)

        return {
            'credit_rating': self.credit_rating,
            'default_probability_5yr': float(default_prob),
            'survival_probability': float(survival_prob),
            'expected_recovery_rate': float(recovery_rate),
            'expected_loss_rate': float(expected_loss),
            'risk_level': 'Very High' if default_prob > 0.15 else
                         'High' if default_prob > 0.05 else
                         'Moderate' if default_prob > 0.01 else 'Low'
        }

    def equity_risk_analysis(self, equity_returns: List[Decimal]) -> Dict[str, Any]:
        """
        Analyze equity-like behavior of high-yield bonds

        Finding: High-yield bonds have high equity correlation
        Analysis shows: "High-yield bonds are more equity-like than bond-like"

        Args:
            equity_returns: Stock market returns for correlation

        Returns:
            Equity risk metrics
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient market data'}

        # Calculate bond returns
        bond_returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            bond_returns.append(ret)

        min_length = min(len(bond_returns), len(equity_returns))
        if min_length < 2:
            return {'error': 'Insufficient data for correlation'}

        bond_returns = bond_returns[:min_length]
        equity_returns = equity_returns[:min_length]

        # Calculate correlation
        bond_array = np.array([float(r) for r in bond_returns])
        equity_array = np.array([float(r) for r in equity_returns])

        correlation = np.corrcoef(bond_array, equity_array)[0, 1]

        # Calculate equity beta
        covariance = np.cov(bond_array, equity_array)[0, 1]
        equity_variance = np.var(equity_array)
        beta = covariance / equity_variance if equity_variance > 0 else 0

        return {
            'correlation_with_equities': float(correlation),
            'equity_beta': float(beta),
            'equity_risk_exposure': f"{abs(beta) * 100:.1f}% of equity market risk",
            'key_insight': 'High correlation confirms equity-like behavior',
            'interpretation': 'Acts more like stocks than bonds' if abs(correlation) > 0.6 else
                            'Hybrid behavior' if abs(correlation) > 0.3 else
                            'Bond-like behavior',
            'diversification_benefit': 'Low - already exposed via equity allocation' if abs(correlation) > 0.6 else 'Moderate'
        }

    def compare_to_alternatives(self,
                                treasury_yield: Decimal,
                                ig_corporate_yield: Decimal,
                                small_cap_value_return: Decimal) -> Dict[str, Any]:
        """
        Compare high-yield to better alternatives

        Recommendation: Investment-grade bonds + small-cap value stocks
        is more efficient than high-yield bonds

        Args:
            treasury_yield: Treasury bond yield
            ig_corporate_yield: Investment-grade corporate yield
            small_cap_value_return: Expected return on small-cap value stocks

        Returns:
            Alternative comparison
        """
        hy_yield = self.calculate_yield_to_maturity()

        # Alternative portfolio: 60% IG corporate, 40% small-cap value
        alternative_expected_return = (
            Decimal('0.60') * ig_corporate_yield +
            Decimal('0.40') * small_cap_value_return
        )

        # Risk comparison (approximate)
        hy_volatility = Decimal('0.12')  # Typical 12% volatility
        ig_volatility = Decimal('0.05')   # 5% for investment grade
        equity_volatility = Decimal('0.20') # 20% for small-cap value

        # Alternative portfolio volatility (assume 0.3 correlation)
        alternative_volatility = (
            (Decimal('0.60')**2 * ig_volatility**2 +
             Decimal('0.40')**2 * equity_volatility**2 +
             Decimal('2') * Decimal('0.60') * Decimal('0.40') * ig_volatility * equity_volatility * Decimal('0.30')
            ) ** Decimal('0.5')
        )

        hy_sharpe = (hy_yield - treasury_yield) / hy_volatility
        alt_sharpe = (alternative_expected_return - treasury_yield) / alternative_volatility

        return {
            'high_yield_bond': {
                'expected_return': float(hy_yield),
                'volatility': float(hy_volatility),
                'sharpe_ratio': float(hy_sharpe)
            },
            'alternative_portfolio': {
                'composition': '60% Investment-Grade Bonds + 40% Small-Cap Value',
                'expected_return': float(alternative_expected_return),
                'volatility': float(alternative_volatility),
                'sharpe_ratio': float(alt_sharpe)
            },
            'advantage_alternative': float(alt_sharpe - hy_sharpe),
            'analysis_verdict': 'Alternative portfolio is more efficient' if alt_sharpe > hy_sharpe else 'High-yield competitive',
            'recommendation': 'Avoid high-yield, use alternative' if alt_sharpe > hy_sharpe else 'High-yield acceptable',
            'rationale': 'Better risk-adjusted returns with clearer asset allocation control'
        }

    def asset_location_problem(self) -> Dict[str, Any]:
        """
        Analyze asset location challenges for high-yield bonds

        Key Issue: Hybrid nature creates placement problem
        - Too risky for bond allocation
        - Too bond-like for equity allocation
        - Can't split efficiently between accounts

        Returns:
            Location analysis
        """
        return {
            'problem': 'Hybrid equity/bond characteristics',
            'equity_component': '60-70% (credit risk behaves like equity risk)',
            'bond_component': '30-40% (interest rate sensitivity)',
            'optimal_location_conflict': {
                'for_equity_component': 'Taxable account (equities location)',
                'for_bond_component': 'Tax-deferred account (bonds location)',
                'reality': 'Cannot split single security across accounts'
            },
            'practical_solutions': [
                'Hold entirely in tax-deferred (suboptimal for equity component)',
                'Hold entirely in taxable (suboptimal for bond component)',
                'Avoid high-yield bonds entirely (recommended approach)'
            ],
            'analysis_verdict': 'Asset location inefficiency is another reason to avoid high-yield bonds',
            'better_approach': 'Separate allocations - IG bonds in tax-deferred, equities in taxable'
        }

    def historical_performance_reality_check(self) -> Dict[str, Any]:
        """
        Reality check on high-yield performance claims

        Historical Data (1979-2007):
        - High-yield bonds underperformed various alternatives
        - Higher risk not compensated with higher returns

        Returns:
            Performance reality check
        """
        # Historical data analysis (1979-2007)
        historical_comparison = {
            'high_yield_bonds': {
                'annualized_return': 0.099,
                'standard_deviation': 0.105,
                'sharpe_ratio': 0.66
            },
            'investment_grade_bonds': {
                'annualized_return': 0.094,
                'standard_deviation': 0.078,
                'sharpe_ratio': 0.79
            },
            'small_cap_value': {
                'annualized_return': 0.168,
                'standard_deviation': 0.204,
                'sharpe_ratio': 0.68
            },
            'alternative_60_40': {
                'annualized_return': 0.124,
                'standard_deviation': 0.113,
                'sharpe_ratio': 0.77
            }
        }

        return {
            'period': '1979-2007',
            'data_source': 'Historical Performance Analysis',
            'historical_results': historical_comparison,
            'key_finding': 'High-yield had WORST Sharpe ratio (0.66)',
            'winner': 'Investment-grade bonds (0.79 Sharpe)',
            'analysis_conclusion': 'Credit risk was not rewarded',
            'investor_lesson': 'Higher yield does NOT mean higher risk-adjusted returns',
            'recommendation': 'Avoid high-yield bonds, use more efficient alternatives'
        }

    def calculate_credit_risk_premium(self, default_rate: Decimal, recovery_rate: Decimal,
                                     treasury_yield: Decimal) -> Dict[str, Any]:
        """
        Calculate if credit risk premium is adequate

        CFA: Required spread = (Default Rate × Loss Rate) / (1 - Default Rate)

        Args:
            default_rate: Annual default probability
            recovery_rate: Recovery rate in default
            treasury_yield: Risk-free rate

        Returns:
            Credit risk premium analysis
        """
        loss_given_default = Decimal('1') - recovery_rate
        expected_loss = default_rate * loss_given_default

        # Required spread to compensate for expected loss
        required_spread = expected_loss / (Decimal('1') - default_rate)

        # Actual spread
        ytm = self.calculate_yield_to_maturity()
        actual_spread = ytm - treasury_yield

        # Risk premium (actual - required)
        risk_premium = actual_spread - required_spread

        return {
            'default_rate': float(default_rate),
            'recovery_rate': float(recovery_rate),
            'expected_loss': float(expected_loss),
            'required_spread': float(required_spread),
            'actual_spread': float(actual_spread),
            'risk_premium': float(risk_premium),
            'adequacy': 'Adequate' if risk_premium > 0.01 else
                       'Marginal' if risk_premium > 0 else
                       'Inadequate',
            'interpretation': 'Compensated for credit risk' if risk_premium > 0.01 else
                            'Barely compensated' if risk_premium > 0 else
                            'NOT compensated for credit risk taken'
        }

    def analysis_final_verdict(self) -> Dict[str, Any]:
        """
        Summary of analytical conclusions on high-yield bonds

        Returns:
            Complete verdict and recommendations
        """
        return {
            'analysis_topic': 'High-Yield (Junk) Bonds',
            'category': 'THE FLAWED',
            'key_findings': [
                '1. High-yield bonds behave more like equities than bonds',
                '2. Correlation with equities is high (~0.60), reducing diversification',
                '3. Credit risk has NOT been well compensated historically',
                '4. Sharpe ratio inferior to alternatives (0.66 vs 0.79 for IG bonds)',
                '5. Asset location creates tax inefficiency problem',
                '6. Better alternatives exist with superior risk-adjusted returns'
            ],
            'historical_evidence': {
                'period': '1979-2007',
                'return': '9.9% (vs 9.4% IG bonds)',
                'volatility': '10.5% (vs 7.8% IG bonds)',
                'sharpe': '0.66 (WORST among alternatives)',
                'conclusion': 'Higher risk NOT rewarded'
            },
            'analysis_recommendation': 'AVOID HIGH-YIELD BONDS',
            'better_alternative': {
                'portfolio': '60% Investment-Grade Bonds + 40% Small-Cap Value Stocks',
                'benefits': [
                    'Higher risk-adjusted returns',
                    'Better diversification',
                    'Clear asset allocation',
                    'Efficient tax location',
                    'Pure exposures to bond and equity risk factors'
                ]
            },
            'when_acceptable': 'Only if you understand equity-like risk and cannot access alternatives',
            'implementation_warning': 'If investing despite advice, use diversified fund, not individual bonds',
            'final_word': 'High-yield bonds are a flawed alternative investment - avoid them'
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """
        Calculate comprehensive high-yield bond metrics

        Returns:
            All key metrics
        """
        ytm = self.calculate_yield_to_maturity()
        rating_tier = self.get_rating_tier()
        default_analysis = self.estimate_default_probability()

        return {
            'security_type': 'High-Yield (Junk) Bond',
            'credit_rating': self.credit_rating,
            'rating_tier': rating_tier,
            'face_value': float(self.face_value),
            'current_price': float(self.current_price),
            'coupon_rate': float(self.coupon_rate),
            'yield_to_maturity': float(ytm),
            'maturity_years': self.maturity_years,
            'default_analysis': default_analysis,
            'analysis_category': 'FLAWED',
            'analysis_recommendation': 'AVOID - Use investment-grade bonds + small-cap value instead',
            'key_problems': [
                'Equity-like risk without equity-like returns',
                'Poor Sharpe ratio historically',
                'Asset location tax inefficiency',
                'Credit risk inadequately compensated'
            ]
        }

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV"""
        return self.current_price

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive high-yield bond valuation summary"""
        return {
            "asset_overview": {
                "security_type": "High-Yield (Junk) Bond",
                "face_value": float(self.face_value),
                "coupon_rate": float(self.coupon_rate),
                "maturity_years": self.maturity_years,
                "current_price": float(self.current_price),
                "credit_rating": str(self.credit_rating.value) if hasattr(self.credit_rating, 'value') else str(self.credit_rating)
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE FLAWED",
            "recommendation": "Avoid - use investment-grade bonds or small-cap value stocks instead"
        }

    def calculate_performance(self) -> Dict[str, Any]:
        """Calculate performance metrics"""
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
            'historical_benchmark_sharpe': 0.66,
            'performance_vs_benchmark': 'Above' if sharpe > 0.66 else 'Below',
            'observation_count': len(returns)
        }


# Export
__all__ = ['HighYieldBondAnalyzer', 'CreditRating']
