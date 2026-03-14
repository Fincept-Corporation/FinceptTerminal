"""market_neutral Module"""

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


class MarketNeutralAnalyzer(AlternativeInvestmentBase):
    """
    Market-Neutral Fund Analyzer

    CFA Standards: Alternative Investments - Long/Short Equity, Market Neutral

    Key Concepts from Key insight: - Equal dollar long and short positions (net beta ≈ 0)
    - Promised to deliver alpha without market risk
    - Reality: Difficult to achieve true neutrality
    - Factor exposures hidden (value, size, momentum)
    - High fees for mediocre performance
    - Leverage amplifies risks

    Verdict: "THE BAD" - Failed to deliver on promises, high costs
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.fund_name = parameters.name if hasattr(parameters, 'name') else 'Market Neutral Fund'

        # Long/Short positions
        self.long_exposure = parameters.long_exposure if hasattr(parameters, 'long_exposure') else Decimal('1.0')
        self.short_exposure = parameters.short_exposure if hasattr(parameters, 'short_exposure') else Decimal('1.0')
        self.net_exposure = self.long_exposure - self.short_exposure
        self.gross_exposure = self.long_exposure + self.short_exposure

        # Fees
        self.management_fee = parameters.management_fee if hasattr(parameters, 'management_fee') else Decimal('0.02')
        self.performance_fee = parameters.performance_fee if hasattr(parameters, 'performance_fee') else Decimal('0.20')

    def calculate_beta(self, market_returns: List[Decimal]) -> Dict[str, Any]:
        """
        Calculate portfolio beta relative to market

        CFA: Beta measures systematic risk
        Market neutral should have beta ≈ 0

        Args:
            market_returns: Market benchmark returns

        Returns:
            Beta analysis
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient fund return data'}

        # Calculate fund returns
        fund_returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            fund_returns.append(ret)

        # Ensure equal lengths
        min_length = min(len(fund_returns), len(market_returns))
        fund_returns = fund_returns[:min_length]
        market_returns = market_returns[:min_length]

        if min_length < 2:
            return {'error': 'Insufficient data for beta calculation'}

        # Convert to numpy
        fund_array = np.array([float(r) for r in fund_returns])
        market_array = np.array([float(r) for r in market_returns])

        # Calculate beta
        covariance = np.cov(fund_array, market_array)[0, 1]
        market_variance = np.var(market_array)
        beta = covariance / market_variance if market_variance != 0 else 0

        # Calculate alpha (Jensen's alpha)
        rf = float(self.config.RISK_FREE_RATE)
        fund_mean = np.mean(fund_array)
        market_mean = np.mean(market_array)
        alpha = fund_mean - (rf + beta * (market_mean - rf))

        # R-squared
        correlation = np.corrcoef(fund_array, market_array)[0, 1]
        r_squared = correlation ** 2

        return {
            'beta': float(beta),
            'alpha': float(alpha),
            'r_squared': float(r_squared),
            'correlation': float(correlation),
            'market_neutral_assessment': self._assess_neutrality(beta),
            'analysis_benchmark': 'True market neutral should have beta < |0.10|'
        }

    def _assess_neutrality(self, beta: float) -> str:
        """Assess how market neutral the fund actually is"""
        abs_beta = abs(beta)

        if abs_beta < 0.10:
            return 'Excellent neutrality - Beta near zero'
        elif abs_beta < 0.20:
            return 'Good neutrality - Low market exposure'
        elif abs_beta < 0.30:
            return 'Moderate neutrality - Some market exposure'
        else:
            return 'Poor neutrality - Significant market exposure (NOT truly neutral)'

    def factor_exposure_analysis(self, value_returns: List[Decimal],
                                 size_returns: List[Decimal],
                                 momentum_returns: List[Decimal]) -> Dict[str, Any]:
        """
        Analyze exposure to factor premiums

        Finding: "Market neutral" funds often have HIDDEN factor exposures
        - Long value stocks, short growth (value tilt)
        - Long small caps, short large caps (size tilt)
        - Long winners, short losers (momentum)

        These factor bets are NOT market neutral - they're factor bets!

        Args:
            value_returns: Value factor returns (HML)
            size_returns: Size factor returns (SMB)
            momentum_returns: Momentum factor returns (UMD)

        Returns:
            Factor exposure analysis
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient fund data'}

        # Calculate fund returns
        fund_returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            fund_returns.append(ret)

        # Ensure equal lengths
        min_length = min(len(fund_returns), len(value_returns), len(size_returns), len(momentum_returns))
        fund_returns = fund_returns[:min_length]
        value_returns = value_returns[:min_length]
        size_returns = size_returns[:min_length]
        momentum_returns = momentum_returns[:min_length]

        if min_length < 2:
            return {'error': 'Insufficient data'}

        # Convert to numpy
        fund_array = np.array([float(r) for r in fund_returns])
        value_array = np.array([float(r) for r in value_returns])
        size_array = np.array([float(r) for r in size_returns])
        momentum_array = np.array([float(r) for r in momentum_returns])

        # Calculate correlations
        corr_value = np.corrcoef(fund_array, value_array)[0, 1]
        corr_size = np.corrcoef(fund_array, size_array)[0, 1]
        corr_momentum = np.corrcoef(fund_array, momentum_array)[0, 1]

        return {
            'factor_correlations': {
                'value_factor': float(corr_value),
                'size_factor': float(corr_size),
                'momentum_factor': float(corr_momentum)
            },
            'interpretation': self._interpret_factor_exposures(corr_value, corr_size, corr_momentum),
            'analysis_finding': (
                'Many "market neutral" funds have significant factor tilts - they\'re really '
                'factor funds in disguise, not pure alpha generators'
            )
        }

    def _interpret_factor_exposures(self, value: float, size: float, momentum: float) -> Dict[str, str]:
        """Interpret factor exposures"""
        def classify_exposure(corr: float) -> str:
            abs_corr = abs(corr)
            if abs_corr < 0.20:
                return 'Minimal'
            elif abs_corr < 0.40:
                return 'Moderate'
            else:
                return 'Significant'

        return {
            'value_exposure': f"{classify_exposure(value)} ({'Long value' if value > 0 else 'Short value'})",
            'size_exposure': f"{classify_exposure(size)} ({'Small cap bias' if size > 0 else 'Large cap bias'})",
            'momentum_exposure': f"{classify_exposure(momentum)} ({'Momentum' if momentum > 0 else 'Contrarian'})",
            'overall_assessment': 'Hidden factor bets detected' if max(abs(value), abs(size), abs(momentum)) > 0.30 else 'Relatively clean alpha'
        }

    def short_squeeze_risk_analysis(self) -> Dict[str, Any]:
        """
        Analyze short squeeze risk

        Key insight: Short selling carries unique risks
        - Unlimited loss potential (price can go to infinity)
        - Forced covering in short squeezes
        - Borrow costs can spike
        - Timing risk (can be right but too early)

        Returns:
            Short squeeze risk assessment
        """
        short_position_size = self.short_exposure
        leverage = self.gross_exposure

        # Risk scenarios
        scenarios = []

        for squeeze_magnitude in [Decimal('0.20'), Decimal('0.50'), Decimal('1.00'), Decimal('2.00')]:
            # Loss on short position
            short_loss = short_position_size * squeeze_magnitude

            # Total portfolio impact (assuming long positions flat)
            portfolio_loss = short_loss / leverage

            scenarios.append({
                'squeeze_magnitude': float(squeeze_magnitude),
                'short_position_loss': float(short_loss),
                'portfolio_impact': float(portfolio_loss),
                'severity': 'Mild' if squeeze_magnitude < Decimal('0.30') else
                           'Moderate' if squeeze_magnitude < Decimal('0.60') else
                           'Severe' if squeeze_magnitude < Decimal('1.50') else 'Catastrophic'
            })

        return {
            'short_exposure': float(short_position_size),
            'gross_leverage': float(leverage),
            'squeeze_scenarios': scenarios,
            'risk_factors': [
                'Unlimited loss potential on short positions',
                'Forced covering can create cascading losses',
                'Borrow costs spike during squeezes',
                'Crowded shorts especially vulnerable',
                'Market-wide short covering (2020-2021 meme stocks)'
            ],
            'analysis_warning': 'Short squeeze risk is asymmetric and difficult to hedge'
        }

    def leverage_risk_analysis(self) -> Dict[str, Any]:
        """
        Analyze leverage risk

        Key insight: Market neutral funds often use leverage to boost returns
        Gross exposure of 2x-4x common (200-400% of capital)
        This AMPLIFIES risks even if net exposure is zero

        Returns:
            Leverage analysis
        """
        net_exposure = self.net_exposure
        gross_exposure = self.gross_exposure

        leverage_ratio = gross_exposure / Decimal('1')  # Assume capital = 1

        # Risk amplification
        # If longs +100% and shorts +100%, gross = 200%
        # A 10% adverse move on both sides = 20% portfolio loss

        adverse_move = Decimal('0.10')  # 10% adverse move
        amplified_loss = adverse_move * gross_exposure

        return {
            'net_exposure': float(net_exposure),
            'gross_exposure': float(gross_exposure),
            'leverage_ratio': float(leverage_ratio),
            'leverage_assessment': self._assess_leverage(leverage_ratio),
            'risk_amplification': {
                'adverse_scenario': '10% adverse move on both longs and shorts',
                'portfolio_impact': float(amplified_loss),
                'interpretation': f'{float(amplified_loss):.1%} portfolio loss from seemingly small move'
            },
            'analysis_warning': (
                'High gross exposure amplifies volatility even if net exposure is zero. '
                'Leverage magnifies errors and can cause forced liquidations.'
            )
        }

    def _assess_leverage(self, leverage: Decimal) -> str:
        """Assess leverage level"""
        if leverage < Decimal('1.5'):
            return 'Low leverage - Conservative'
        elif leverage < Decimal('2.5'):
            return 'Moderate leverage - Typical for market neutral'
        elif leverage < Decimal('4.0'):
            return 'High leverage - Significant risk amplification'
        else:
            return 'Very high leverage - Excessive risk'

    def performance_vs_expectations(self, expected_alpha: Decimal,
                                    stock_market_return: Decimal) -> Dict[str, Any]:
        """
        Compare actual performance to market neutral promises

        Finding: Market neutral funds FAILED to deliver promised alpha
        - Promised: Market-like returns (8-10%) with low volatility
        - Reality: Bond-like returns (4-6%) with moderate volatility
        - High fees consumed most alpha
        - Better off with simple stock/bond mix

        Args:
            expected_alpha: Promised/expected alpha
            stock_market_return: Stock market benchmark return

        Returns:
            Performance comparison
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient data'}

        # Calculate actual return
        returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            returns.append(ret)

        actual_return = sum(returns) / len(returns) if returns else Decimal('0')

        # Fees reduce return
        fee_drag = self.management_fee + (self.performance_fee * max(Decimal('0'), actual_return))
        net_return = actual_return - fee_drag

        # Compare to alternatives
        bond_return = self.config.RISK_FREE_RATE + Decimal('0.02')  # Assume 2% credit spread
        balanced_portfolio_return = (stock_market_return * Decimal('0.60')) + (bond_return * Decimal('0.40'))

        return {
            'promised_characteristics': {
                'expected_return': float(expected_alpha),
                'expected_volatility': 'Low',
                'market_exposure': 'Zero (market neutral)',
                'pitch': 'Stock-like returns with bond-like volatility'
            },
            'actual_results': {
                'gross_return': float(actual_return),
                'fee_drag': float(fee_drag),
                'net_return': float(net_return),
                'shortfall_vs_expectation': float(net_return - expected_alpha)
            },
            'alternative_comparisons': {
                'stock_market_return': float(stock_market_return),
                'bond_return': float(bond_return),
                'balanced_60_40_return': float(balanced_portfolio_return),
                'market_neutral_vs_balanced': float(net_return - balanced_portfolio_return)
            },
            'analysis_reality_check': (
                'Market neutral funds promised alpha without market risk. Reality: most delivered '
                'bond-like returns with moderate volatility and high fees. Simple 60/40 portfolio '
                'typically outperformed with lower costs and greater transparency.'
            )
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """
        Complete analytical verdict on Market-Neutral Funds
        Based on "Alternative Investments Analysis"

        Category: "THE BAD"

        Returns:
            Complete verdict
        """
        return {
            'asset_class': 'Market-Neutral Funds',
            'category': 'THE BAD',
            'overall_rating': '3/10 - Failed to deliver on promises',

            'the_good': [
                'Low correlation with stock market (when truly neutral)',
                'Theoretical appeal of pure alpha generation',
                'Can work in skilled hands (rare)'
            ],

            'the_bad': [
                'Failed to deliver promised returns - bond-like instead of stock-like',
                'High fees (2 and 20) consumed most alpha generated',
                'Difficult to achieve true market neutrality',
                'Hidden factor exposures (not pure alpha)',
                'Leverage amplifies risks despite zero net exposure',
                'Short squeeze risk asymmetric and dangerous',
                'Performance degraded as strategy became crowded',
                'Lack of transparency - hard to know what you own'
            ],

            'the_ugly': [
                'MASSIVE gap between promise and reality',
                'Sold as "stock returns with bond risk" - delivered "bond returns with moderate risk"',
                'Many funds closed after failing to deliver',
                'Factor tilts disguised as alpha (misleading)',
                'Leverage risks understated to investors',
                'High fees for mediocre performance inexcusable'
            ],

            'key_findings': {
                'promised_return': '8-10% (stock-like)',
                'actual_return': '4-6% (bond-like)',
                'true_market_neutrality': 'Difficult to achieve consistently',
                'alpha_generation': 'Mostly factor exposure, not pure alpha',
                'fee_impact': 'Devastating - 2-3% annual drag',
                'better_alternative': '60/40 stock/bond portfolio'
            },

            'analysis_quote': (
                '"Market-neutral funds promised to deliver alpha without beta - stock-like returns '
                'with bond-like risk. This was always too good to be true. The reality: they delivered '
                'bond-like returns with moderate risk and very high fees. Most alpha came from factor '
                'exposures (value, size, momentum), not manager skill. Investors were paying 2 and 20 '
                'for what they could get cheaper through factor funds. The promise was a lie."'
            ),

            'investment_recommendation': {
                'suitable_for': [
                    'NO ONE - avoid entirely',
                    'Better alternatives exist for every goal'
                ],
                'not_suitable_for': [
                    'Everyone - this strategy failed as a category',
                    'Investors seeking true market neutrality',
                    'Anyone paying 2 and 20 fees',
                    'Investors wanting transparency',
                    'Risk-averse investors (leverage and short risk)'
                ],
                'better_alternatives': [
                    'Simple 60/40 stock/bond portfolio (cheaper, transparent)',
                    'Factor funds (value, size, quality) - explicit exposure',
                    'Index funds (market beta at minimal cost)',
                    'TIPS + high-quality bonds (true safety)',
                    'Do nothing - save 2-3% annual fees'
                ]
            },

            'final_verdict': (
                'Market-neutral funds are THE BAD. They failed spectacularly to deliver on their '
                'promise of stock-like returns with bond-like risk. Instead, they delivered bond-like '
                'returns with moderate risk and very high fees. The "alpha" they generated was mostly '
                'factor exposure you can get cheaper elsewhere. Leverage and short squeeze risks were '
                'understated. The gap between marketing and reality was enormous. Avoid completely - '
                'there is NO good reason to own these funds. A simple 60/40 portfolio beats them on '
                'returns, transparency, and especially costs.'
            )
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate comprehensive market-neutral fund metrics"""
        return {
            'fund_name': self.fund_name,
            'exposures': {
                'long_exposure': float(self.long_exposure),
                'short_exposure': float(self.short_exposure),
                'net_exposure': float(self.net_exposure),
                'gross_exposure': float(self.gross_exposure)
            },
            'fees': {
                'management_fee': float(self.management_fee),
                'performance_fee': float(self.performance_fee)
            },
            'analysis_category': 'THE BAD',
            'promise': 'Stock returns with bond risk',
            'reality': 'Bond returns with moderate risk and high fees',
            'recommendation': 'Avoid - use simple 60/40 stock/bond portfolio instead'
        }

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV"""
        if not self.market_data:
            return Decimal('0')
        return self.market_data[-1].price

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive market-neutral fund valuation summary"""
        return {
            "asset_overview": {
                "asset_class": "Market-Neutral Fund",
                "fund_name": self.fund_name,
                "net_exposure": float(self.net_exposure),
                "gross_exposure": float(self.gross_exposure)
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE BAD",
            "recommendation": "Avoid entirely - failed strategy with high costs"
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
            'observation_count': len(returns),
            'note': 'Returns before fees - actual investor returns significantly lower'
        }


# Export
__all__ = ['MarketNeutralAnalyzer']
