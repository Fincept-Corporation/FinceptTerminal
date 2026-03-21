"""convertible_bonds Module"""

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


class ConvertibleBondAnalyzer(AlternativeInvestmentBase):
    """
    Convertible Bond Analyzer

    CFA Standards: Fixed Income - Convertibles, Embedded Options

    Key Concepts from Key insight: - Hybrid security: Bond + equity call option
    - Conversion ratio and conversion price
    - Investment value (straight bond floor)
    - Conversion value (equity floor)
    - Conversion premium
    - Asymmetric payoff structure

    Verdict: "The Flawed" - Complex, expensive, limited upside capture
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.face_value = parameters.face_value if hasattr(parameters, 'face_value') else Decimal('1000')
        self.coupon_rate = parameters.coupon_rate if hasattr(parameters, 'coupon_rate') else Decimal('0.03')
        self.maturity_years = parameters.maturity_years if hasattr(parameters, 'maturity_years') else 5
        self.current_price = parameters.current_market_value if hasattr(parameters, 'current_market_value') else self.face_value

        # Conversion features
        self.conversion_ratio = parameters.conversion_ratio if hasattr(parameters, 'conversion_ratio') else Decimal('20')
        self.conversion_price = self.face_value / self.conversion_ratio
        self.stock_price = parameters.stock_price if hasattr(parameters, 'stock_price') else Decimal('45')

        # Credit parameters
        self.credit_spread = parameters.credit_spread if hasattr(parameters, 'credit_spread') else Decimal('0.02')

    def calculate_conversion_value(self, stock_price: Optional[Decimal] = None) -> Decimal:
        """
        Calculate conversion value (equity floor)

        CFA: Conversion Value = Conversion Ratio × Stock Price

        Args:
            stock_price: Current stock price (default: self.stock_price)

        Returns:
            Conversion value
        """
        if stock_price is None:
            stock_price = self.stock_price

        return self.conversion_ratio * stock_price

    def calculate_straight_bond_value(self, market_yield: Decimal) -> Decimal:
        """
        Calculate investment value (straight bond floor)

        CFA: Value convertible as if it were a plain bond
        Bond Value = PV(Coupons) + PV(Principal)

        Args:
            market_yield: Market yield for comparable non-convertible bond

        Returns:
            Straight bond value
        """
        annual_coupon = self.coupon_rate * self.face_value

        # Present value of coupons
        pv_coupons = Decimal('0')
        for t in range(1, self.maturity_years + 1):
            pv_coupons += annual_coupon / ((Decimal('1') + market_yield) ** Decimal(str(t)))

        # Present value of principal
        pv_principal = self.face_value / ((Decimal('1') + market_yield) ** Decimal(str(self.maturity_years)))

        return pv_coupons + pv_principal

    def calculate_conversion_premium(self, stock_price: Optional[Decimal] = None) -> Dict[str, Any]:
        """
        Calculate conversion premium

        CFA: Premium = (Convertible Price - Conversion Value) / Conversion Value
        Shows how much investor pays for the conversion option

        Args:
            stock_price: Current stock price

        Returns:
            Conversion premium metrics
        """
        conversion_value = self.calculate_conversion_value(stock_price)

        # Conversion premium (dollar and percentage)
        premium_dollar = self.current_price - conversion_value
        premium_percent = premium_dollar / conversion_value if conversion_value > 0 else Decimal('0')

        # Breakeven stock price increase
        breakeven_increase = premium_percent

        return {
            'current_bond_price': float(self.current_price),
            'conversion_value': float(conversion_value),
            'conversion_premium_dollar': float(premium_dollar),
            'conversion_premium_percent': float(premium_percent),
            'breakeven_stock_increase': float(breakeven_increase),
            'interpretation': self._interpret_conversion_premium(premium_percent)
        }

    def _interpret_conversion_premium(self, premium: Decimal) -> str:
        """Interpret conversion premium level"""
        if premium < Decimal('0.10'):
            return 'Low premium - In-the-money, equity-like behavior'
        elif premium < Decimal('0.20'):
            return 'Moderate premium - Balanced hybrid'
        elif premium < Decimal('0.30'):
            return 'High premium - Bond-like behavior, expensive option'
        else:
            return 'Very high premium - Expensive, limited equity participation'

    def calculate_bond_floor(self, market_yield: Decimal) -> Dict[str, Any]:
        """
        Calculate bond floor and downside protection

        CFA: Bond floor = max(Straight Bond Value, Conversion Value)
        Provides downside protection

        Args:
            market_yield: Market yield for comparable non-convertible

        Returns:
            Floor analysis
        """
        straight_bond_value = self.calculate_straight_bond_value(market_yield)
        conversion_value = self.calculate_conversion_value()

        bond_floor = max(straight_bond_value, conversion_value)

        # Downside protection
        downside_protection = (self.current_price - bond_floor) / self.current_price if self.current_price > 0 else Decimal('0')

        # Premium to floor
        premium_to_floor = (self.current_price - bond_floor) / bond_floor if bond_floor > 0 else Decimal('0')

        return {
            'straight_bond_value': float(straight_bond_value),
            'conversion_value': float(conversion_value),
            'bond_floor': float(bond_floor),
            'current_price': float(self.current_price),
            'downside_protection': float(downside_protection),
            'premium_to_floor': float(premium_to_floor),
            'primary_floor': 'Bond' if straight_bond_value > conversion_value else 'Equity'
        }

    def calculate_upside_participation(self, stock_price_scenarios: List[Decimal]) -> Dict[str, Any]:
        """
        Calculate upside participation vs direct equity

        Criticism: Convertibles capture only partial upside vs direct stock ownership
        Premium paid for conversion option reduces gains

        Args:
            stock_price_scenarios: List of potential stock prices

        Returns:
            Upside participation analysis
        """
        results = []

        initial_stock_price = self.stock_price
        initial_conversion_value = self.calculate_conversion_value(initial_stock_price)

        for future_stock_price in stock_price_scenarios:
            # Stock return
            stock_return = (future_stock_price - initial_stock_price) / initial_stock_price

            # Convertible bond value (assume converts if in-the-money)
            future_conversion_value = self.calculate_conversion_value(future_stock_price)

            # Convertible return (from current price, not conversion value)
            convertible_return = (future_conversion_value - self.current_price) / self.current_price

            # Participation rate
            participation = convertible_return / stock_return if stock_return != 0 else Decimal('0')

            results.append({
                'stock_price': float(future_stock_price),
                'stock_return': float(stock_return),
                'convertible_return': float(convertible_return),
                'participation_rate': float(participation),
                'upside_captured': float(convertible_return / stock_return) if stock_return > 0 else 0
            })

        # Calculate average participation
        avg_participation = sum(r['participation_rate'] for r in results) / len(results) if results else 0

        return {
            'scenarios': results,
            'average_participation_rate': avg_participation,
            'analysis_criticism': f'Convertibles capture only {avg_participation:.1%} of equity upside on average',
            'interpretation': self._interpret_upside_participation(avg_participation)
        }

    def _interpret_upside_participation(self, participation: float) -> str:
        """Interpret upside participation rate"""
        if participation > 0.90:
            return 'Excellent - Near-full equity participation'
        elif participation > 0.70:
            return 'Good - Most equity upside captured'
        elif participation > 0.50:
            return 'Moderate - Partial equity upside'
        else:
            return 'Poor - Limited equity participation (concern)'

    def calculate_delta(self, stock_price: Optional[Decimal] = None) -> Decimal:
        """
        Calculate delta (equity sensitivity)

        CFA: Delta = Change in Convertible Price / Change in Stock Price
        Approximation: Delta ≈ Conversion Ratio × (Stock Price / Convertible Price)

        Args:
            stock_price: Current stock price

        Returns:
            Delta estimate
        """
        if stock_price is None:
            stock_price = self.stock_price

        conversion_value = self.calculate_conversion_value(stock_price)

        # Simplified delta approximation
        if self.current_price > conversion_value:
            # Out-of-the-money: lower delta (bond-like)
            delta = conversion_value / self.current_price
        else:
            # In-the-money: higher delta (equity-like)
            delta = Decimal('0.85')  # Typical for deep ITM convertibles

        return delta

    def credit_risk_analysis(self, market_yield: Decimal,
                            default_probability: Decimal,
                            recovery_rate: Decimal = Decimal('0.40')) -> Dict[str, Any]:
        """
        Analyze credit risk of convertible

        Warning: Convertibles often issued by lower-credit companies
        Credit spread = additional yield for default risk

        Args:
            market_yield: Yield on comparable straight bond
            default_probability: Annual default probability
            recovery_rate: Expected recovery in default (40% typical)

        Returns:
            Credit risk analysis
        """
        # Expected loss
        expected_loss = default_probability * (Decimal('1') - recovery_rate)

        # Credit spread required
        required_spread = expected_loss

        # Actual spread
        risk_free_rate = self.config.RISK_FREE_RATE
        actual_spread = market_yield - risk_free_rate

        # Compare
        adequate_compensation = actual_spread >= required_spread

        return {
            'default_probability': float(default_probability),
            'recovery_rate': float(recovery_rate),
            'expected_loss': float(expected_loss),
            'required_credit_spread': float(required_spread),
            'actual_credit_spread': float(actual_spread),
            'adequate_compensation': adequate_compensation,
            'credit_quality_assessment': self._assess_credit_quality(default_probability),
            'analysis_warning': 'Many convertibles issued by lower-quality companies - credit risk significant'
        }

    def _assess_credit_quality(self, default_prob: Decimal) -> str:
        """Assess credit quality based on default probability"""
        if default_prob < Decimal('0.005'):
            return 'Investment Grade (AAA-BBB)'
        elif default_prob < Decimal('0.02'):
            return 'High Grade Speculative (BB)'
        elif default_prob < Decimal('0.05'):
            return 'Speculative (B)'
        else:
            return 'Highly Speculative / Distressed (CCC and below)'

    def compare_to_alternatives(self, straight_bond_yield: Decimal,
                                stock_expected_return: Decimal,
                                stock_volatility: Decimal) -> Dict[str, Any]:
        """
        Compare convertible to straight bond and direct stock

        Analysis: Convertibles often the WORST of both worlds
        - Less upside than stocks
        - Less safety than bonds
        - Higher fees/complexity

        Args:
            straight_bond_yield: Yield on straight bond from same issuer
            stock_expected_return: Expected return on stock
            stock_volatility: Stock volatility

        Returns:
            Comparative analysis
        """
        # Convertible yield
        annual_coupon = self.coupon_rate * self.face_value
        convertible_yield = annual_coupon / self.current_price

        # Expected returns (simplified)
        convertible_expected_return = convertible_yield + (stock_expected_return - convertible_yield) * self.calculate_delta()

        # Risk comparison
        delta = self.calculate_delta()
        convertible_volatility = delta * stock_volatility  # Approximate

        # Sharpe ratios
        rf = self.config.RISK_FREE_RATE
        bond_sharpe = (straight_bond_yield - rf) / Decimal('0.05')  # Assume 5% bond volatility
        stock_sharpe = (stock_expected_return - rf) / stock_volatility
        convertible_sharpe = (convertible_expected_return - rf) / convertible_volatility if convertible_volatility > 0 else Decimal('0')

        return {
            'straight_bond': {
                'yield': float(straight_bond_yield),
                'expected_return': float(straight_bond_yield),
                'volatility': 0.05,
                'sharpe_ratio': float(bond_sharpe)
            },
            'stock': {
                'yield': 0.0,  # Typically low/no dividend
                'expected_return': float(stock_expected_return),
                'volatility': float(stock_volatility),
                'sharpe_ratio': float(stock_sharpe)
            },
            'convertible': {
                'yield': float(convertible_yield),
                'expected_return': float(convertible_expected_return),
                'volatility': float(convertible_volatility),
                'sharpe_ratio': float(convertible_sharpe),
                'delta': float(delta)
            },
            'winner_by_metric': {
                'highest_yield': 'Straight Bond',
                'highest_expected_return': 'Stock',
                'lowest_risk': 'Straight Bond',
                'best_sharpe': self._determine_best_sharpe(bond_sharpe, stock_sharpe, convertible_sharpe)
            },
            'analysis_conclusion': (
                'Convertibles often underperform both alternatives: '
                'less upside than stocks, less safety than bonds, higher complexity/fees'
            )
        }

    def _determine_best_sharpe(self, bond: Decimal, stock: Decimal, convertible: Decimal) -> str:
        """Determine which has best Sharpe ratio"""
        max_sharpe = max(bond, stock, convertible)
        if max_sharpe == bond:
            return 'Straight Bond'
        elif max_sharpe == stock:
            return 'Stock'
        else:
            return 'Convertible'

    def analysis_verdict(self) -> Dict[str, Any]:
        """
        Complete analytical verdict on Convertible Bonds
        Based on "Alternative Investments Analysis"

        Category: "THE FLAWED"

        Returns:
            Complete verdict with recommendations
        """
        return {
            'asset_class': 'Convertible Bonds',
            'category': 'THE FLAWED',
            'overall_rating': '4/10 - Limited use cases',

            'the_good': [
                'Downside protection from bond floor',
                'Participation in equity upside (though limited)',
                'Lower volatility than direct stock ownership',
                'Can be attractive in specific market conditions'
            ],

            'the_bad': [
                'Partial upside capture only (60-80% typical)',
                'Conversion premium erodes returns',
                'Complex valuation and pricing',
                'Often issued by lower-credit companies',
                'Higher fees than straight bonds or stocks',
                'Illiquid market for many issues',
                'Tax inefficiency (ordinary income + potential capital gains)'
            ],

            'the_ugly': [
                'Worst of both worlds: less safety than bonds, less upside than stocks',
                'Marketed as "best of both" but delivers "mediocre of both"',
                'Complexity allows for mispricing against investor',
                'Conversion feature value often overstated by issuers'
            ],

            'key_findings': {
                'risk_return': 'Suboptimal - neither fish nor fowl',
                'diversification': 'Limited - correlated with both stocks and bonds',
                'complexity': 'High - difficult to value fairly',
                'costs': 'High - fees and spreads exceed simple alternatives',
                'suitability': 'Narrow - few investors truly benefit'
            },

            'analysis_quote': (
                '"Convertible bonds are often marketed as providing the best of both worlds—'
                'the safety of bonds and the upside of stocks. The reality is they often deliver '
                'the mediocre of both worlds: less safety than bonds and less upside than stocks, '
                'with higher complexity and costs than either."'
            ),

            'investment_recommendation': {
                'suitable_for': [
                    'Sophisticated investors with specific tactical needs',
                    'Situations requiring asymmetric payoffs',
                    'Investors restricted from direct equity (rare regulatory cases)',
                    'Very small allocation in highly diversified portfolios (<5%)'
                ],
                'not_suitable_for': [
                    'Core portfolio holdings',
                    'Investors seeking simple, low-cost exposure',
                    'Conservative investors (credit risk often high)',
                    'Investors without ability to analyze complex securities',
                    'Tax-inefficient accounts (taxable)'
                ],
                'better_alternatives': [
                    'For safety: Straight bonds or TIPS',
                    'For upside: Direct stock ownership',
                    'For hybrid exposure: Balanced fund (60/40 stocks/bonds)',
                    'For asymmetric payoffs: Options strategies (if sophisticated)'
                ]
            },

            'final_verdict': (
                'Convertible bonds are FLAWED for most investors. The theoretical appeal of '
                'combining bond safety with equity upside rarely materializes in practice. '
                'Partial upside participation, credit risk, complexity, and high costs make '
                'convertibles inferior to simple combinations of straight bonds and stocks. '
                'Only sophisticated investors with specific tactical needs should consider them, '
                'and even then, only in small allocations.'
            )
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """
        Calculate comprehensive convertible bond metrics

        Returns:
            All key metrics
        """
        # Assume market yield for comparable bond
        market_yield = self.config.RISK_FREE_RATE + self.credit_spread

        conversion_premium = self.calculate_conversion_premium()
        bond_floor = self.calculate_bond_floor(market_yield)
        delta = self.calculate_delta()

        return {
            'security_type': 'Convertible Bond',
            'face_value': float(self.face_value),
            'coupon_rate': float(self.coupon_rate),
            'maturity_years': self.maturity_years,
            'current_price': float(self.current_price),
            'conversion_ratio': float(self.conversion_ratio),
            'conversion_price': float(self.conversion_price),
            'current_stock_price': float(self.stock_price),
            'conversion_premium': conversion_premium,
            'bond_floor_analysis': bond_floor,
            'delta': float(delta),
            'analysis_category': 'THE FLAWED',
            'recommendation': 'Generally not recommended - use straight bonds + stocks instead'
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
            'observation_count': len(returns)
        }

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive convertible bond valuation summary"""
        return {
            "asset_overview": {
                "security_type": "Convertible Bond",
                "face_value": float(self.face_value),
                "coupon_rate": float(self.coupon_rate),
                "maturity_years": self.maturity_years,
                "current_price": float(self.current_price),
                "conversion_ratio": float(self.conversion_ratio),
                "stock_price": float(self.stock_price)
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE FLAWED",
            "recommendation": "Avoid - use straight bonds + stocks instead for simplicity and better returns"
        }


# Export
__all__ = ['ConvertibleBondAnalyzer']
