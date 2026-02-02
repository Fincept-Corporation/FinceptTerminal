"""managed_futures Module"""

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


class ManagedFuturesAnalyzer(AlternativeInvestmentBase):
    """
    Managed Futures (Commodity Trading Advisors - CTAs) Analyzer

    CFA Standards: Alternative Investments - Managed Futures, Trend Following

    Key Concepts from Key insight: - Trend-following strategies (momentum)
    - Long/short both sides of markets
    - Crisis alpha potential (2008 performance)
    - High fees (2 and 20 or higher)
    - Capacity constraints
    - Replication available at lower cost

    Verdict: "The Flawed" - Crisis benefits don't justify high costs
    Better alternatives: Low-cost trend-following ETFs or skip entirely
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.fund_name = parameters.name if hasattr(parameters, 'name') else 'Managed Futures Fund'
        self.strategy_type = parameters.strategy_type if hasattr(parameters, 'strategy_type') else 'Systematic Trend-Following'

        # Fee structure
        self.management_fee = parameters.management_fee if hasattr(parameters, 'management_fee') else Decimal('0.02')
        self.performance_fee = parameters.performance_fee if hasattr(parameters, 'performance_fee') else Decimal('0.20')
        self.hurdle_rate = parameters.hurdle_rate if hasattr(parameters, 'hurdle_rate') else Decimal('0.0')

        # Performance tracking
        self.returns_history: List[Decimal] = []
        self.crisis_periods: List[Dict[str, Any]] = []

    def add_crisis_period(self, name: str, start_date: str, end_date: str,
                         cta_return: Decimal, stock_return: Decimal, bond_return: Decimal) -> None:
        """
        Record crisis period performance

        Args:
            name: Crisis name
            start_date: Start date
            end_date: End date
            cta_return: Managed futures return
            stock_return: Stock market return
            bond_return: Bond market return
        """
        self.crisis_periods.append({
            'name': name,
            'start_date': start_date,
            'end_date': end_date,
            'cta_return': cta_return,
            'stock_return': stock_return,
            'bond_return': bond_return,
            'timestamp': datetime.now().isoformat()
        })

    def trend_following_analysis(self, price_series: List[Decimal],
                                 lookback_periods: List[int] = None) -> Dict[str, Any]:
        """
        Analyze trend-following strategy performance

        CFA: Trend following = Buy rising assets, short falling assets
        Common lookback periods: 20, 50, 100, 200 days

        Args:
            price_series: Historical price data
            lookback_periods: List of lookback windows (default: [20, 50, 100, 200])

        Returns:
            Trend-following analysis
        """
        if lookback_periods is None:
            lookback_periods = [20, 50, 100, 200]

        if len(price_series) < max(lookback_periods) + 1:
            return {'error': 'Insufficient price data for trend analysis'}

        results = {}

        for period in lookback_periods:
            signals = []
            returns = []

            for i in range(period, len(price_series)):
                # Calculate moving average
                ma = sum(price_series[i-period:i]) / Decimal(str(period))
                current_price = price_series[i]

                # Generate signal
                if current_price > ma:
                    signal = 1  # Long
                elif current_price < ma:
                    signal = -1  # Short
                else:
                    signal = 0  # Neutral

                signals.append(signal)

                # Calculate return (if we have previous signal)
                if i > period:
                    price_change = (price_series[i] - price_series[i-1]) / price_series[i-1]
                    strategy_return = price_change * Decimal(str(signals[-2]))  # Use previous signal
                    returns.append(strategy_return)

            # Calculate statistics
            if returns:
                avg_return = sum(returns) / len(returns)
                volatility = self.math.calculate_volatility(returns, annualized=True)
                sharpe = self.math.sharpe_ratio(returns, self.config.RISK_FREE_RATE)

                # Win rate
                wins = sum(1 for r in returns if r > 0)
                win_rate = Decimal(str(wins)) / Decimal(str(len(returns)))

                results[f'{period}_day_ma'] = {
                    'average_return': float(avg_return),
                    'volatility': float(volatility),
                    'sharpe_ratio': float(sharpe),
                    'win_rate': float(win_rate),
                    'trades': len(returns)
                }

        return {
            'trend_following_strategies': results,
            'interpretation': 'Systematic trend-following across multiple timeframes',
            'analysis_note': 'Trend-following works in theory, but high fees erode returns'
        }

    def crisis_alpha_analysis(self) -> Dict[str, Any]:
        """
        Analyze performance during crisis periods (crisis alpha)

        Finding: Managed futures CAN provide crisis alpha
        - 2008: Many CTAs +10-20% while stocks -37%
        - BUT: Not consistent across all crises
        - Diversification benefit exists but expensive

        Returns:
            Crisis performance analysis
        """
        if not self.crisis_periods:
            return {
                'error': 'No crisis data recorded',
                'suggestion': 'Use add_crisis_period() to record historical crises'
            }

        results = []
        positive_crises = 0
        outperform_stocks = 0
        outperform_bonds = 0

        for crisis in self.crisis_periods:
            cta_ret = float(crisis['cta_return'])
            stock_ret = float(crisis['stock_return'])
            bond_ret = float(crisis['bond_return'])

            if cta_ret > 0:
                positive_crises += 1
            if cta_ret > stock_ret:
                outperform_stocks += 1
            if cta_ret > bond_ret:
                outperform_bonds += 1

            results.append({
                'crisis': crisis['name'],
                'period': f"{crisis['start_date']} to {crisis['end_date']}",
                'cta_return': f"{cta_ret:.2%}",
                'stock_return': f"{stock_ret:.2%}",
                'bond_return': f"{bond_ret:.2%}",
                'alpha_vs_stocks': f"{(cta_ret - stock_ret):.2%}",
                'alpha_vs_bonds': f"{(cta_ret - bond_ret):.2%}",
                'crisis_hedge_success': cta_ret > 0
            })

        total_crises = len(self.crisis_periods)
        reliability_score = positive_crises / total_crises if total_crises > 0 else 0

        return {
            'crisis_events_analyzed': total_crises,
            'performance_by_crisis': results,
            'crisis_alpha_reliability': {
                'positive_crises': positive_crises,
                'outperformed_stocks': outperform_stocks,
                'outperformed_bonds': outperform_bonds,
                'reliability_percentage': f"{reliability_score:.1%}",
                'analysis_conclusion': (
                    'Managed futures CAN provide crisis alpha, but not consistently. '
                    'High fees make cost-benefit questionable for most investors.'
                )
            }
        }

    def fee_impact_analysis(self, gross_return: Decimal, years: int = 10) -> Dict[str, Any]:
        """
        Calculate impact of fees on returns

        Key insight: 2 and 20 fee structure devastating to long-term wealth
        Management fee: 2% of assets
        Performance fee: 20% of profits above hurdle

        Args:
            gross_return: Annual gross return before fees
            years: Investment period

        Returns:
            Fee impact analysis
        """
        # Initialize
        gross_wealth = Decimal('100')
        net_wealth = Decimal('100')

        total_mgmt_fees = Decimal('0')
        total_perf_fees = Decimal('0')

        yearly_results = []

        for year in range(1, years + 1):
            # Gross return
            gross_gain = gross_wealth * gross_return
            gross_wealth += gross_gain

            # Management fee (2% of beginning balance)
            mgmt_fee = net_wealth * self.management_fee
            net_wealth -= mgmt_fee
            total_mgmt_fees += mgmt_fee

            # Net return before performance fee
            net_gain_before_perf = net_wealth * gross_return

            # Performance fee (20% of gains above hurdle)
            gains_above_hurdle = max(Decimal('0'), net_gain_before_perf - (net_wealth * self.hurdle_rate))
            perf_fee = gains_above_hurdle * self.performance_fee
            total_perf_fees += perf_fee

            # Final net wealth
            net_wealth = net_wealth + net_gain_before_perf - perf_fee

            yearly_results.append({
                'year': year,
                'gross_wealth': float(gross_wealth),
                'net_wealth': float(net_wealth),
                'mgmt_fee': float(mgmt_fee),
                'perf_fee': float(perf_fee),
                'cumulative_fees': float(total_mgmt_fees + total_perf_fees)
            })

        # Final calculations
        gross_cagr = (gross_wealth / Decimal('100')) ** (Decimal('1') / Decimal(str(years))) - Decimal('1')
        net_cagr = (net_wealth / Decimal('100')) ** (Decimal('1') / Decimal(str(years))) - Decimal('1')
        fee_drag = gross_cagr - net_cagr
        wealth_difference = gross_wealth - net_wealth

        return {
            'fee_structure': {
                'management_fee': float(self.management_fee),
                'performance_fee': float(self.performance_fee),
                'hurdle_rate': float(self.hurdle_rate)
            },
            'results': {
                'initial_investment': 100.0,
                'years': years,
                'gross_return_annual': float(gross_return),
                'gross_wealth_final': float(gross_wealth),
                'net_wealth_final': float(net_wealth),
                'gross_cagr': float(gross_cagr),
                'net_cagr': float(net_cagr),
                'fee_drag_annual': float(fee_drag),
                'total_fees_paid': float(total_mgmt_fees + total_perf_fees),
                'wealth_given_up': float(wealth_difference),
                'percentage_lost_to_fees': float(wealth_difference / gross_wealth)
            },
            'yearly_breakdown': yearly_results,
            'analysis_warning': f'Fees consume {float(wealth_difference/gross_wealth):.1%} of total wealth created - massive drag'
        }

    def correlation_with_traditional_assets(self, stock_returns: List[Decimal],
                                            bond_returns: List[Decimal]) -> Dict[str, Any]:
        """
        Analyze correlation with stocks and bonds

        Finding: Managed futures historically LOW correlation with stocks/bonds
        - Correlation with stocks: ~0.0 to -0.20 (crisis periods)
        - Correlation with bonds: ~0.0
        - This is the PRIMARY benefit - diversification

        Args:
            stock_returns: Equity returns
            bond_returns: Bond returns

        Returns:
            Correlation analysis
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient CTA return data'}

        # Calculate CTA returns
        cta_returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            cta_returns.append(ret)

        # Ensure equal lengths
        min_length = min(len(cta_returns), len(stock_returns), len(bond_returns))
        cta_returns = cta_returns[:min_length]
        stock_returns = stock_returns[:min_length]
        bond_returns = bond_returns[:min_length]

        if min_length < 2:
            return {'error': 'Insufficient data for correlation'}

        # Calculate correlations
        cta_array = np.array([float(r) for r in cta_returns])
        stock_array = np.array([float(r) for r in stock_returns])
        bond_array = np.array([float(r) for r in bond_returns])

        corr_cta_stocks = np.corrcoef(cta_array, stock_array)[0, 1]
        corr_cta_bonds = np.corrcoef(cta_array, bond_array)[0, 1]
        corr_stocks_bonds = np.corrcoef(stock_array, bond_array)[0, 1]

        return {
            'correlation_cta_stocks': float(corr_cta_stocks),
            'correlation_cta_bonds': float(corr_cta_bonds),
            'correlation_stocks_bonds': float(corr_stocks_bonds),
            'diversification_benefit': self._interpret_diversification(corr_cta_stocks, corr_cta_bonds),
            'analysis_benchmarks': {
                'expected_stock_correlation': '0.0 to -0.20 (low/negative)',
                'expected_bond_correlation': '~0.0 (uncorrelated)',
                'conclusion': 'Low correlation is THE benefit, but high fees negate value'
            }
        }

    def _interpret_diversification(self, corr_stocks: float, corr_bonds: float) -> str:
        """Interpret diversification quality"""
        avg_corr = abs(corr_stocks + corr_bonds) / 2

        if avg_corr < 0.10:
            return 'Excellent - Very low correlation with traditional assets'
        elif avg_corr < 0.30:
            return 'Good - Meaningful diversification benefit'
        elif avg_corr < 0.50:
            return 'Moderate - Some diversification'
        else:
            return 'Limited - Correlation too high for meaningful diversification'

    def replication_alternatives(self) -> Dict[str, Any]:
        """
        Compare to low-cost replication alternatives

        Key insight: Trend-following strategies can be replicated at much lower cost
        - Systematic trend-following ETFs: 0.60-0.90% fees
        - vs Managed futures funds: 2.0% + 20% performance fee
        - Similar exposure, dramatically lower cost

        Returns:
            Replication comparison
        """
        # Typical CTA fund: 2 and 20
        cta_mgmt_fee = Decimal('0.02')
        cta_perf_fee_effective = Decimal('0.015')  # ~1.5% on average
        total_cta_fee = cta_mgmt_fee + cta_perf_fee_effective

        # Replication ETF
        etf_fee = Decimal('0.0075')  # 0.75% typical

        # Fee savings
        annual_savings = total_cta_fee - etf_fee

        # 10-year wealth impact (assuming 8% gross return)
        initial_investment = Decimal('100000')
        years = 10
        gross_return = Decimal('0.08')

        # CTA outcome
        cta_wealth = initial_investment
        for _ in range(years):
            cta_wealth = cta_wealth * (Decimal('1') + gross_return - total_cta_fee)

        # ETF outcome
        etf_wealth = initial_investment
        for _ in range(years):
            etf_wealth = etf_wealth * (Decimal('1') + gross_return - etf_fee)

        wealth_difference = etf_wealth - cta_wealth

        return {
            'traditional_cta_fund': {
                'management_fee': '2.00%',
                'effective_performance_fee': '~1.50%',
                'total_annual_cost': float(total_cta_fee),
                'description': 'Typical managed futures fund'
            },
            'replication_etf': {
                'management_fee': float(etf_fee),
                'performance_fee': 0.0,
                'total_annual_cost': float(etf_fee),
                'description': 'Systematic trend-following ETF',
                'examples': ['AQF', 'KMLM', 'DBMF']
            },
            'savings': {
                'annual_fee_savings': float(annual_savings),
                'annual_savings_bps': float(annual_savings * Decimal('10000')),
                'ten_year_cta_wealth': float(cta_wealth),
                'ten_year_etf_wealth': float(etf_wealth),
                'wealth_difference': float(wealth_difference),
                'percentage_more_wealth': float((wealth_difference / cta_wealth))
            },
            'analysis_recommendation': (
                'If you want managed futures exposure, use low-cost systematic ETFs. '
                'Traditional 2 and 20 funds are wealth destroyers after fees.'
            )
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """
        Complete analytical verdict on Managed Futures
        Based on "Alternative Investments Analysis"

        Category: "THE FLAWED"

        Returns:
            Complete verdict with recommendations
        """
        return {
            'asset_class': 'Managed Futures (CTAs)',
            'category': 'THE FLAWED',
            'overall_rating': '5/10 - Benefits real but negated by costs',

            'the_good': [
                'Low/negative correlation with stocks and bonds',
                'Crisis alpha potential (2008: +10-20% vs stocks -37%)',
                'Diversification benefit in portfolio context',
                'Systematic trend-following has academic support',
                'Can profit in both rising and falling markets (long/short)'
            ],

            'the_bad': [
                'VERY HIGH FEES - 2 and 20 structure devastating',
                'Inconsistent crisis performance (not reliable)',
                'Capacity constraints - too much money chasing same trends',
                'Trend-following works until it doesn\'t (whipsaws)',
                'Performance degraded as AUM grew (strategy got crowded)',
                'Better alternatives available at 1/5th the cost'
            ],

            'the_ugly': [
                'Fees consume 35-50% of total wealth created over 10 years',
                'Sold as "crisis insurance" but not consistently effective',
                'Complex fee structures hide true costs',
                'Many funds closed after poor performance (survivorship bias)',
                'Industry marketing overstates reliability of crisis alpha'
            ],

            'key_findings': {
                'diversification': 'Excellent - low correlation with traditional assets',
                'crisis_alpha': 'EXISTS but inconsistent',
                'fee_impact': 'Catastrophic - 2 and 20 destroys long-term returns',
                'replication_available': 'Yes - systematic ETFs at 0.75% fees',
                'verdict': 'Benefits real but high fees make it flawed'
            },

            'analysis_quote': (
                '"Managed futures have provided genuine diversification benefits and crisis alpha. '
                'However, the 2 and 20 fee structure makes them a poor choice for investors. '
                'The exact same exposure can now be obtained through low-cost systematic ETFs '
                'at fees of 0.60-0.90%, saving investors 2-3% annually. This difference is massive '
                'over time. If you want managed futures exposure, use the ETFs."'
            ),

            'investment_recommendation': {
                'suitable_for': [
                    'NO ONE should use traditional 2 and 20 CTA funds',
                    'If seeking exposure: Use systematic trend-following ETFs instead',
                    'Sophisticated investors who understand whipsaw risk',
                    'As SMALL portfolio allocation (5-10% max)',
                    'Only if already well-diversified'
                ],
                'not_suitable_for': [
                    'Core portfolio holdings',
                    'Conservative investors (volatility)',
                    'Anyone using traditional 2 and 20 funds (fees too high)',
                    'Investors expecting consistent crisis protection (unreliable)',
                    'Those needing liquidity (some CTAs have lockups)'
                ],
                'better_alternatives': [
                    'Systematic trend-following ETFs (AQF, KMLM, DBMF) - 0.60-0.90% fees',
                    'Simple 60/40 stock/bond portfolio (lower cost, similar diversification)',
                    'TIPS + high-quality bonds (cheaper diversification)',
                    'Skip managed futures entirely (not essential)'
                ]
            },

            'final_verdict': (
                'Managed futures are FLAWED because of fees. The strategy has merit - low correlation '
                'with traditional assets and some crisis alpha - but traditional 2 and 20 funds are '
                'wealth destroyers. The arrival of low-cost systematic ETFs changes the game entirely. '
                'These ETFs provide similar exposure at 1/5th the cost. For most investors, managed '
                'futures are optional, not essential. If you want exposure, use low-cost ETFs, keep '
                'allocation small (5-10%), and understand you\'re adding volatility for diversification, '
                'not consistent crisis protection.'
            )
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """
        Calculate comprehensive managed futures metrics

        Returns:
            All key metrics
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient data'}

        performance = self.calculate_performance()

        return {
            'fund_name': self.fund_name,
            'strategy_type': self.strategy_type,
            'fee_structure': {
                'management_fee': float(self.management_fee),
                'performance_fee': float(self.performance_fee),
                'hurdle_rate': float(self.hurdle_rate)
            },
            'performance': performance,
            'analysis_category': 'THE FLAWED',
            'primary_benefit': 'Diversification (low correlation)',
            'primary_flaw': 'High fees (2 and 20 structure)',
            'recommendation': 'Use low-cost systematic ETFs instead of traditional CTA funds'
        }

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV"""
        if not self.market_data:
            return Decimal('0')
        return self.market_data[-1].price

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive managed futures valuation summary"""
        return {
            "asset_overview": {
                "asset_class": "Managed Futures (CTAs)",
                "fund_name": self.fund_name,
                "strategy_type": self.strategy_type,
                "fee_structure": f"{float(self.management_fee):.1%} mgmt + {float(self.performance_fee):.0%} perf"
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE FLAWED",
            "recommendation": "Avoid traditional 2 and 20 funds - use low-cost systematic ETFs if desired"
        }

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
            'note': 'Returns shown are gross - fees significantly reduce net returns'
        }


# Export
__all__ = ['ManagedFuturesAnalyzer']
