"""preferred_stocks Module"""

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


class PreferredStockAnalyzer(AlternativeInvestmentBase):
    """
    Preferred Stock Analyzer

    CFA Standards: Hybrid securities, Fixed income characteristics

    Key Findings (- Long maturity risk without bond-like protections
    - Call risk significantly reduces upside
    - Credit risk similar to bonds but worse terms
    - Dividend suspension risk
    - Tax advantage only for corporations (70% dividend exclusion)
    - Individuals should avoid - no compelling reason to own
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.par_value = parameters.acquisition_price if hasattr(parameters, 'acquisition_price') else Decimal('25')
        self.dividend_rate = parameters.dividend_rate if hasattr(parameters, 'dividend_rate') else Decimal('0.06')
        self.current_price = parameters.current_market_value if hasattr(parameters, 'current_market_value') else self.par_value
        self.call_price = parameters.call_price if hasattr(parameters, 'call_price') else self.par_value
        self.call_date = parameters.call_date if hasattr(parameters, 'call_date') else None
        self.is_cumulative = parameters.is_cumulative if hasattr(parameters, 'is_cumulative') else True
        self.credit_rating = parameters.credit_rating if hasattr(parameters, 'credit_rating') else 'BBB'
        self.perpetual = parameters.perpetual if hasattr(parameters, 'perpetual') else True

    def calculate_current_yield(self) -> Decimal:
        """
        Calculate current yield

        Formula: Annual Dividend / Current Price

        Returns:
            Current yield
        """
        annual_dividend = self.dividend_rate * self.par_value
        current_yield = annual_dividend / self.current_price if self.current_price > 0 else Decimal('0')
        return current_yield

    def calculate_yield_to_call(self) -> Optional[Dict[str, Any]]:
        """
        Calculate yield to call (if callable)

        Issue: Call feature caps upside potential

        Returns:
            Yield to call metrics or None if not callable
        """
        if not self.call_date:
            return None

        try:
            call_date = datetime.strptime(self.call_date, '%Y-%m-%d')
            years_to_call = (call_date - datetime.now()).days / 365.25

            if years_to_call <= 0:
                return {'status': 'Already callable'}

            annual_dividend = self.dividend_rate * self.par_value

            # Approximate YTC
            # YTC ≈ [Annual Dividend + (Call Price - Current Price) / Years] / [(Call Price + Current Price) / 2]
            capital_gain = (self.call_price - self.current_price) / Decimal(str(years_to_call))
            average_price = (self.call_price + self.current_price) / Decimal('2')

            ytc = (annual_dividend + capital_gain) / average_price if average_price > 0 else Decimal('0')

            return {
                'yield_to_call': float(ytc),
                'years_to_call': years_to_call,
                'call_price': float(self.call_price),
                'current_price': float(self.current_price),
                'capital_gain_potential': float(self.call_price - self.current_price),
                'analysis_warning': 'Call feature limits upside - issuer wins, you lose'
            }
        except Exception as e:
            logger.error(f"Error calculating YTC: {e}")
            return None

    def analyze_call_risk(self) -> Dict[str, Any]:
        """
        Analyze call risk implications

        Finding: Preferreds called when rates fall (bad for investors)
        Issuers win, investors lose

        Returns:
            Call risk analysis
        """
        ytc_analysis = self.calculate_yield_to_call()
        current_yield = self.calculate_current_yield()

        if not ytc_analysis:
            return {
                'callable': False,
                'risk_level': 'N/A - Perpetual preferred',
                'analysis_note': 'Perpetual preferreds have duration risk instead'
            }

        # If trading above par and callable soon, high call risk
        premium = self.current_price - self.par_value
        call_risk_score = 'High' if premium > 0 and ytc_analysis['years_to_call'] < 5 else \
                         'Moderate' if premium > 0 else 'Low'

        return {
            'callable': True,
            'call_risk_level': call_risk_score,
            'call_date': self.call_date,
            'years_to_call': ytc_analysis['years_to_call'],
            'trading_premium': float(premium),
            'yield_to_call': ytc_analysis['yield_to_call'],
            'current_yield': float(current_yield),
            'yield_compression': float(current_yield - Decimal(str(ytc_analysis['yield_to_call']))),
            'investor_risk': 'Capital loss if called' if premium > 0 else 'Reinvestment risk',
            'analysis_insight': 'Issuer calls when rates fall - you lose high-yielding asset and must reinvest at lower rates',
            'asymmetric_outcome': {
                'if_rates_rise': 'Price falls, you lose',
                'if_rates_fall': 'Security called, you lose',
                'conclusion': 'Heads they win, tails you lose'
            }
        }

    def analyze_credit_risk(self) -> Dict[str, Any]:
        """
        Analyze credit risk of preferred stock

        Finding: Credit risk similar to bonds but:
        - Subordinated to all debt (worse recovery)
        - No bond covenants protection
        - Dividend can be suspended
        - Long/perpetual maturity increases risk

        Returns:
            Credit risk analysis
        """
        # Default probabilities by rating (simplified)
        default_probs = {
            'AAA': Decimal('0.0001'),
            'AA': Decimal('0.0005'),
            'A': Decimal('0.0015'),
            'BBB': Decimal('0.0050'),
            'BB': Decimal('0.0250'),
            'B': Decimal('0.0800')
        }

        default_prob = default_probs.get(self.credit_rating, Decimal('0.02'))

        # Recovery rates (lower than bonds due to subordination)
        recovery_rate = Decimal('0.20')  # 20% typical for preferreds vs 50% for bonds

        expected_loss = default_prob * (Decimal('1') - recovery_rate)

        return {
            'credit_rating': self.credit_rating,
            'default_probability': float(default_prob),
            'recovery_rate': float(recovery_rate),
            'expected_loss': float(expected_loss),
            'subordination': 'Below all debt holders',
            'creditor_priority': 'Above common stock only',
            'bond_comparison': {
                'bond_recovery': '50% typical',
                'preferred_recovery': '20% typical',
                'disadvantage': 'Preferreds recover 60% LESS than bonds'
            },
            'analysis_warning': 'Same credit risk as bonds but worse terms and lower recovery',
            'no_covenants': 'Unlike bonds, preferreds lack protective covenants',
            'risk_assessment': 'Higher risk than bonds of same issuer'
        }

    def analyze_dividend_suspension_risk(self) -> Dict[str, Any]:
        """
        Analyze dividend suspension risk

        Issue: Dividends can be suspended, unlike bond coupons
        Cumulative vs non-cumulative matters

        Returns:
            Suspension risk analysis
        """
        return {
            'preferred_type': 'Cumulative' if self.is_cumulative else 'Non-Cumulative',
            'dividend_suspension_allowed': True,
            'cumulative_feature': {
                'if_suspended': 'Missed dividends accumulate' if self.is_cumulative else 'Missed dividends LOST FOREVER',
                'protection_level': 'Moderate' if self.is_cumulative else 'Very Poor',
                'investor_risk': 'Must wait for payment' if self.is_cumulative else 'Permanent loss of income'
            },
            'vs_bonds': {
                'bond_coupon': 'Cannot be suspended - default if missed',
                'preferred_dividend': 'Can be suspended without default',
                'advantage': 'BONDS - mandatory payment vs optional dividend'
            },
            'financial_stress_scenario': {
                'bonds': 'Must pay or default',
                'preferred': 'Suspend dividend, no consequences',
                'common_stock': 'Dividends already cut',
                'result': 'Preferreds suffer like common stock holders'
            },
            'analysis_verdict': 'Dividend suspension risk makes preferreds less reliable than bonds',
            'recommendation': 'Avoid non-cumulative preferreds entirely'
        }

    def analyze_maturity_risk(self) -> Dict[str, Any]:
        """
        Analyze long maturity and duration risk

        Finding: Most preferreds perpetual or very long maturity
        = High interest rate risk

        Returns:
            Maturity risk analysis
        """
        if self.perpetual:
            # Perpetual preferred duration = (1 + y) / y
            current_yield = self.calculate_current_yield()
            duration = (Decimal('1') + current_yield) / current_yield if current_yield > 0 else Decimal('20')
        else:
            # Approximate duration for fixed maturity
            duration = Decimal('0.75') * Decimal(str(self.maturity_years))

        # Price change for 1% rate increase
        rate_shock = Decimal('0.01')  # 1%
        price_change = -duration * rate_shock * self.current_price

        return {
            'maturity_type': 'Perpetual' if self.perpetual else f'{self.maturity_years} years',
            'duration': float(duration),
            'interest_rate_sensitivity': 'Very High' if duration > 15 else
                                        'High' if duration > 10 else 'Moderate',
            'rate_shock_analysis': {
                'if_rates_rise_1_percent': f"Price falls {float(abs(price_change)):.2f} ({float(abs(price_change/self.current_price)*100):.1f}%)",
                'if_rates_rise_2_percent': f"Price falls {float(abs(price_change)*2):.2f} ({float(abs(price_change/self.current_price)*200):.1f}%)",
                'current_price': float(self.current_price)
            },
            'vs_bonds': {
                'typical_bond_duration': '5-7 years',
                'preferred_duration': f'{float(duration):.1f} years',
                'risk_ratio': f'{float(duration/Decimal("6")):.1f}x riskier than typical bond'
            },
            'analysis_warning': 'Long/perpetual maturity = extreme interest rate risk',
            'historical_example': {
                'period': '1980s',
                'rate_environment': 'Rising rates',
                'preferred_performance': 'Many fell 40-50%',
                'lesson': 'Duration risk is real and painful'
            }
        }

    def tax_advantage_analysis(self, investor_type: str, tax_bracket: Decimal) -> Dict[str, Any]:
        """
        Analyze tax implications

        Finding: Tax advantage ONLY for corporations (70% dividend exclusion)
        NO advantage for individual investors

        Args:
            investor_type: 'individual' or 'corporate'
            tax_bracket: Tax rate

        Returns:
            Tax analysis
        """
        annual_dividend = self.dividend_rate * self.par_value

        if investor_type == 'corporate':
            # Corporations get 70% dividend exclusion (50% for some)
            exclusion_rate = Decimal('0.70')
            taxable_portion = annual_dividend * (Decimal('1') - exclusion_rate)
            tax_owed = taxable_portion * tax_bracket
            after_tax_dividend = annual_dividend - tax_owed
            effective_tax_rate = tax_owed / annual_dividend

        else:  # individual
            # Qualified dividend tax rate (typically 15-20%) OR ordinary income
            # Most preferred dividends are qualified
            tax_owed = annual_dividend * tax_bracket
            after_tax_dividend = annual_dividend - tax_owed
            effective_tax_rate = tax_bracket

        after_tax_yield = after_tax_dividend / self.current_price

        return {
            'investor_type': investor_type,
            'annual_dividend': float(annual_dividend),
            'pre_tax_yield': float(self.calculate_current_yield()),
            'tax_treatment': {
                'corporate': {
                    'advantage': '70% dividend exclusion',
                    'effective_rate': f'{float(effective_tax_rate)*100:.1f}%' if investor_type == 'corporate' else 'N/A',
                    'makes_sense': True
                },
                'individual': {
                    'advantage': 'None - same as regular stocks',
                    'effective_rate': f'{float(tax_bracket)*100:.1f}%',
                    'makes_sense': False
                }
            },
            'after_tax_yield': float(after_tax_yield),
            'analysis_insight': 'Preferreds designed for corporate investors, not individuals',
            'individual_investor_verdict': 'NO TAX ADVANTAGE - avoid preferreds'
        }

    def compare_to_alternatives(self, bond_yield: Decimal, stock_dividend_yield: Decimal) -> Dict[str, Any]:
        """
        Compare preferred stock to better alternatives

        Recommendation: Just buy bonds OR common stocks - not hybrid

        Args:
            bond_yield: Comparable bond yield
            stock_dividend_yield: Common stock dividend yield

        Returns:
            Alternative comparison
        """
        preferred_yield = self.calculate_current_yield()

        return {
            'preferred_stock': {
                'yield': float(preferred_yield),
                'risks': ['Credit risk', 'Call risk', 'Duration risk', 'Dividend suspension', 'Subordination'],
                'benefits': ['Higher yield than bonds... sometimes'],
                'protections': 'Minimal'
            },
            'investment_grade_bond': {
                'yield': float(bond_yield),
                'risks': ['Credit risk', 'Duration risk'],
                'benefits': ['Mandatory coupon', 'Covenants', 'Higher recovery', 'Clearer maturity'],
                'protections': 'Strong',
                'advantage_vs_preferred': 'Better protection, mandatory payments, higher recovery'
            },
            'common_stock': {
                'yield': float(stock_dividend_yield),
                'risks': ['Equity risk', 'Dividend cuts'],
                'benefits': ['Upside potential', 'Dividend growth', 'Inflation hedge'],
                'protections': None,
                'advantage_vs_preferred': 'Unlimited upside, potential dividend growth'
            },
            'analysis_verdict': {
                'preferred_position': 'Worst of both worlds',
                'bond_comparison': 'Less protection, suspended dividends',
                'stock_comparison': 'No upside, capped returns',
                'conclusion': 'Preferreds combine bond and stock RISKS without their BENEFITS'
            },
            'recommended_alternative': {
                'for_income': 'Investment-grade bonds (better protection)',
                'for_growth': 'Common stocks (upside potential)',
                'for_hybrid': 'Convertible bonds (better structure) OR 60/40 bonds/stocks',
                'never': 'Preferred stocks (for individual investors)'
            }
        }

    def analysis_final_verdict(self) -> Dict[str, Any]:
        """
        Summary of conclusions on preferred stocks

        Returns:
            Complete verdict
        """
        return {
            'analysis_topic': 'Preferred Stocks',
            'category': 'THE FLAWED',
            'rating': '1/10 for individual investors',
            'key_problems': [
                '1. Long/perpetual maturity = extreme duration risk',
                '2. Call risk = capped upside, issuer wins',
                '3. Credit risk with worse terms than bonds',
                '4. Subordinated = low recovery in default',
                '5. Dividends can be suspended (not mandatory like coupons)',
                '6. No tax advantage for individuals (only corporations)',
                '7. No protective covenants like bonds have'
            ],
            'the_hybrid_problem': {
                'supposed_benefit': 'Combines bond and stock features',
                'actual_reality': 'Combines bond and stock RISKS without their BENEFITS',
                'bond_downside': 'No covenant protection, low recovery, suspended payments',
                'stock_downside': 'No upside potential, called when profitable',
                'result': 'Worst of both worlds'
            },
            'who_should_buy': {
                'individual_investors': 'NO - no compelling reason',
                'corporate_investors': 'Maybe - 70% dividend exclusion creates tax advantage',
                'analysis_quote': 'There is no compelling reason for individual investors to own preferred stocks'
            },
            'better_alternatives': [
                'Need income? Buy investment-grade bonds (better protection)',
                'Want growth? Buy common stocks (upside potential)',
                'Want hybrid? Buy convertibles OR split allocation (60/40 bonds/stocks)',
                'Want tax efficiency? Buy municipal bonds (if high bracket)'
            ],
            'historical_performance': {
                '1973_2007': 'Underperformed both bonds AND stocks',
                'sharpe_ratio': 'Inferior to alternatives',
                'risk_adjusted': 'Not compensated for risks taken'
            },
            'implementation_warning': 'Even if ignoring advice, preferreds are tax-inefficient in taxable accounts',
            'asset_location': 'No good location - tax-deferred wastes space, taxable is inefficient',
            'final_recommendation': 'AVOID PREFERRED STOCKS - they are flawed for individual investors'
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """
        Calculate comprehensive preferred stock metrics

        Returns:
            All key metrics
        """
        current_yield = self.calculate_current_yield()
        call_risk = self.analyze_call_risk()
        credit_risk = self.analyze_credit_risk()
        maturity_risk = self.analyze_maturity_risk()

        return {
            'security_type': 'Preferred Stock',
            'par_value': float(self.par_value),
            'current_price': float(self.current_price),
            'dividend_rate': float(self.dividend_rate),
            'current_yield': float(current_yield),
            'credit_rating': self.credit_rating,
            'cumulative': self.is_cumulative,
            'perpetual': self.perpetual,
            'callable': self.call_date is not None,
            'call_risk_analysis': call_risk,
            'credit_risk_analysis': credit_risk,
            'maturity_risk_analysis': maturity_risk,
            'analysis_category': 'FLAWED',
            'analysis_rating': '1/10 for individuals',
            'analysis_recommendation': 'AVOID - No compelling reason to own',
            'better_alternatives': 'Investment-grade bonds OR common stocks'
        }

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV"""
        return self.current_price

    def calculate_performance(self) -> Dict[str, Any]:
        """Calculate performance metrics"""
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient data'}

        returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            dividend = self.dividend_rate * self.par_value / Decimal('4')  # Quarterly
            total_return = (curr_price - prev_price + dividend) / prev_price
            returns.append(total_return)

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
            'analysis_note': 'Historically underperformed bonds and stocks on risk-adjusted basis'
        }

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive preferred stock valuation summary"""
        return {
            "asset_overview": {
                "security_type": "Preferred Stock",
                "par_value": float(self.par_value),
                "dividend_rate": float(self.dividend_rate),
                "current_price": float(self.current_price),
                "callable": self.callable,
                "perpetual": self.perpetual
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE FLAWED",
            "recommendation": "Avoid - use bonds for safety or common stocks for growth"
        }


# Export
__all__ = ['PreferredStockAnalyzer']
