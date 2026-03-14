"""precious_metals Module"""

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


class PreciousMetalsEquityAnalyzer(AlternativeInvestmentBase):
    """
    Precious Metals Equities (PME) Analyzer

    CFA Standards: Alternative Investments - Natural Resources, Commodities

    Key Concepts from (- PME = Stocks of gold/silver mining companies, NOT physical metal
    - High correlation with stocks, LOW correlation with inflation
    - Extreme volatility and drawdowns (>35% common)
    - Crisis performance mixed (not reliable safe haven)
    - Rebalancing bonus potential from volatility
    - Compared to CCF (Collateralized Commodity Futures)

    Verdict: "The Flawed" - High risk, no inflation hedge, crisis unreliable
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.asset_name = parameters.asset_name if hasattr(parameters, 'asset_name') else 'PME Index'
        self.benchmark_stock = 'S&P 500'  # Equity benchmark
        self.benchmark_gold = 'Gold Spot'  # Physical gold benchmark
        self.crisis_events: List[Dict[str, Any]] = []

    def add_crisis_event(self, name: str, start_date: str, end_date: str,
                        stock_return: Decimal, pme_return: Decimal, gold_return: Decimal) -> None:
        """
        Record crisis performance data

        Args:
            name: Crisis name (e.g., "2008 Financial Crisis")
            start_date: Crisis start
            end_date: Crisis end
            stock_return: S&P 500 return
            pme_return: PME return
            gold_return: Physical gold return
        """
        self.crisis_events.append({
            'name': name,
            'start_date': start_date,
            'end_date': end_date,
            'stock_return': stock_return,
            'pme_return': pme_return,
            'gold_return': gold_return,
            'timestamp': datetime.now().isoformat()
        })

    def correlation_analysis(self, stock_returns: List[Decimal],
                            inflation_rates: List[Decimal],
                            gold_returns: Optional[List[Decimal]] = None) -> Dict[str, Any]:
        """
        Analyze PME correlation with stocks, inflation, and physical gold

        Finding (- PME correlation with stocks: ~0.45-0.60 (MODERATE-HIGH)
        - PME correlation with inflation: ~0.10-0.20 (LOW - NOT an inflation hedge)
        - PME correlation with gold: ~0.30-0.40 (MODERATE)

        Args:
            stock_returns: Equity market returns
            inflation_rates: Inflation data
            gold_returns: Physical gold returns (optional)

        Returns:
            Correlation metrics with interpretation
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient market data'}

        # Calculate PME returns
        pme_returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            pme_returns.append(ret)

        # Ensure equal lengths
        min_length = min(len(pme_returns), len(stock_returns), len(inflation_rates))
        pme_returns = pme_returns[:min_length]
        stock_returns = stock_returns[:min_length]
        inflation_rates = inflation_rates[:min_length]

        if gold_returns:
            min_length = min(min_length, len(gold_returns))
            gold_returns = gold_returns[:min_length]

        if min_length < 2:
            return {'error': 'Insufficient data for correlation'}

        # Convert to numpy arrays
        pme_array = np.array([float(r) for r in pme_returns])
        stock_array = np.array([float(r) for r in stock_returns])
        inflation_array = np.array([float(r) for r in inflation_rates])

        # Calculate correlations
        corr_pme_stocks = np.corrcoef(pme_array, stock_array)[0, 1]
        corr_pme_inflation = np.corrcoef(pme_array, inflation_array)[0, 1]

        result = {
            'correlation_pme_stocks': float(corr_pme_stocks),
            'correlation_pme_inflation': float(corr_pme_inflation),
            'stocks_interpretation': self._interpret_stock_correlation(corr_pme_stocks),
            'inflation_hedge_quality': self._interpret_inflation_hedge(corr_pme_inflation),
            'analysis_benchmarks': {
                'expected_stock_correlation': '0.45-0.60 (Moderate-High)',
                'expected_inflation_correlation': '0.10-0.20 (Low - NOT a hedge)',
                'conclusion': 'PME behaves more like stocks than commodities'
            }
        }

        # Add gold correlation if available
        if gold_returns:
            gold_array = np.array([float(r) for r in gold_returns])
            corr_pme_gold = np.corrcoef(pme_array, gold_array)[0, 1]
            result['correlation_pme_gold'] = float(corr_pme_gold)
            result['gold_interpretation'] = f'Moderate correlation ({corr_pme_gold:.2f}) - PME ≠ Physical Gold'

        return result

    def _interpret_stock_correlation(self, corr: float) -> str:
        """Interpret stock correlation"""
        if corr > 0.60:
            return 'Very High - PME moves closely with stocks (diversification limited)'
        elif corr > 0.40:
            return 'Moderate-High - PME has significant equity exposure'
        elif corr > 0.20:
            return 'Moderate - Some equity-like behavior'
        else:
            return 'Low - Good diversification from stocks'

    def _interpret_inflation_hedge(self, corr: float) -> str:
        """Interpret inflation hedge quality"""
        if corr < 0.20:
            return 'Poor - NOT an effective inflation hedge (Key insight: Major finding)'
        elif corr < 0.40:
            return 'Weak - Limited inflation protection'
        elif corr < 0.60:
            return 'Moderate - Some inflation hedge characteristics'
        else:
            return 'Strong - Good inflation hedge'

    def calculate_drawdowns(self) -> Dict[str, Any]:
        """
        Calculate maximum drawdown and drawdown frequency

        Finding: PME experiences frequent drawdowns exceeding 35%
        This is EXTREME volatility, comparable to or worse than equities

        Returns:
            Drawdown analysis with severity classification
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient data'}

        prices = [float(md.price) for md in self.market_data]

        # Calculate running maximum and drawdowns
        running_max = []
        drawdowns = []
        max_so_far = prices[0]

        for price in prices:
            max_so_far = max(max_so_far, price)
            running_max.append(max_so_far)
            drawdown = (price - max_so_far) / max_so_far
            drawdowns.append(drawdown)

        max_drawdown = min(drawdowns)

        # Count severe drawdowns
        drawdown_35_count = sum(1 for dd in drawdowns if dd < -0.35)
        drawdown_25_count = sum(1 for dd in drawdowns if dd < -0.25)
        drawdown_50_count = sum(1 for dd in drawdowns if dd < -0.50)

        # Current drawdown
        current_drawdown = drawdowns[-1] if drawdowns else 0

        return {
            'max_drawdown': float(max_drawdown),
            'current_drawdown': float(current_drawdown),
            'severe_drawdowns_35pct': drawdown_35_count,
            'severe_drawdowns_25pct': drawdown_25_count,
            'extreme_drawdowns_50pct': drawdown_50_count,
            'total_observations': len(drawdowns),
            'analysis_warning': 'PME frequently experiences >35% drawdowns - extreme volatility',
            'severity_rating': self._classify_drawdown_severity(max_drawdown)
        }

    def _classify_drawdown_severity(self, max_dd: float) -> str:
        """Classify drawdown severity"""
        if max_dd > -0.10:
            return 'Low Volatility'
        elif max_dd > -0.20:
            return 'Moderate Volatility'
        elif max_dd > -0.35:
            return 'High Volatility (equity-like)'
        elif max_dd > -0.50:
            return 'Extreme Volatility (worse than stocks)'
        else:
            return 'Crisis-Level Volatility (>50% loss)'

    def crisis_performance_analysis(self) -> Dict[str, Any]:
        """
        Analyze PME performance during crisis periods

        Finding: PME is NOT a reliable crisis hedge
        - Sometimes PME rises (e.g., 1970s stagflation)
        - Sometimes PME falls WITH stocks (e.g., 2008 crash)
        - Unreliable as portfolio insurance

        Returns:
            Crisis performance summary
        """
        if not self.crisis_events:
            return {
                'error': 'No crisis data recorded',
                'suggestion': 'Use add_crisis_event() to record historical crises'
            }

        results = []
        for event in self.crisis_events:
            pme_ret = float(event['pme_return'])
            stock_ret = float(event['stock_return'])
            gold_ret = float(event['gold_return'])

            # Analyze PME behavior
            pme_vs_stocks = pme_ret - stock_ret
            pme_vs_gold = pme_ret - gold_ret

            behavior = self._classify_crisis_behavior(pme_ret, stock_ret, gold_ret)

            results.append({
                'crisis': event['name'],
                'period': f"{event['start_date']} to {event['end_date']}",
                'pme_return': f"{pme_ret:.2%}",
                'stock_return': f"{stock_ret:.2%}",
                'gold_return': f"{gold_ret:.2%}",
                'pme_outperformance_vs_stocks': f"{pme_vs_stocks:.2%}",
                'pme_outperformance_vs_gold': f"{pme_vs_gold:.2%}",
                'behavior': behavior
            })

        # Calculate reliability as crisis hedge
        hedge_successes = sum(1 for r in self.crisis_events
                             if float(r['pme_return']) > 0 and float(r['stock_return']) < 0)
        hedge_failures = sum(1 for r in self.crisis_events
                            if float(r['pme_return']) < 0 and float(r['stock_return']) < 0)

        reliability_score = hedge_successes / len(self.crisis_events) if self.crisis_events else 0

        return {
            'crisis_events_analyzed': len(self.crisis_events),
            'performance_by_crisis': results,
            'hedge_reliability': {
                'successful_hedges': hedge_successes,
                'failed_hedges': hedge_failures,
                'reliability_percentage': f"{reliability_score:.1%}",
                'analysis_conclusion': 'UNRELIABLE - PME cannot be counted on for crisis protection'
            }
        }

    def _classify_crisis_behavior(self, pme_ret: float, stock_ret: float, gold_ret: float) -> str:
        """Classify PME behavior during crisis"""
        if pme_ret > 0 and stock_ret < 0:
            return 'Safe Haven - PME rose while stocks fell'
        elif pme_ret < 0 and stock_ret < 0 and abs(pme_ret) > abs(stock_ret):
            return 'Amplified Decline - PME fell MORE than stocks'
        elif pme_ret < 0 and stock_ret < 0:
            return 'Correlated Decline - PME fell WITH stocks'
        elif pme_ret > gold_ret:
            return 'Outperformed Gold - Better than physical metal'
        else:
            return 'Mixed Performance'

    def rebalancing_bonus_analysis(self, volatility_pme: Decimal,
                                   volatility_stocks: Decimal,
                                   correlation: Decimal,
                                   portfolio_weight_pme: Decimal = Decimal('0.05')) -> Dict[str, Any]:
        """
        Calculate potential rebalancing bonus from PME volatility

        Insight: High volatility can create rebalancing opportunities
        BUT: Only works if correlation is low AND investor has discipline to rebalance

        Formula: Rebalancing Bonus ≈ 0.5 × w₁ × w₂ × (σ₁² + σ₂² - 2ρσ₁σ₂)
        where w = weights, σ = volatilities, ρ = correlation

        Args:
            volatility_pme: PME annualized volatility
            volatility_stocks: Stock annualized volatility
            correlation: PME-stock correlation
            portfolio_weight_pme: PME allocation (default 5%)

        Returns:
            Rebalancing bonus estimate and feasibility
        """
        weight_stocks = Decimal('1') - portfolio_weight_pme

        # Rebalancing bonus formula
        variance_term = (
            volatility_pme ** 2 +
            volatility_stocks ** 2 -
            Decimal('2') * correlation * volatility_pme * volatility_stocks
        )

        rebalancing_bonus = (
            Decimal('0.5') *
            portfolio_weight_pme *
            weight_stocks *
            variance_term
        )

        # Annualized expected bonus
        annual_bonus = rebalancing_bonus

        # Assess feasibility
        feasible = correlation < Decimal('0.70')  # Need low enough correlation

        return {
            'pme_weight': float(portfolio_weight_pme),
            'stock_weight': float(weight_stocks),
            'pme_volatility': float(volatility_pme),
            'stock_volatility': float(volatility_stocks),
            'correlation': float(correlation),
            'estimated_rebalancing_bonus': float(annual_bonus),
            'annualized_bonus_bps': float(annual_bonus * Decimal('10000')),  # basis points
            'feasibility': 'Feasible' if feasible else 'Not Feasible (correlation too high)',
            'requirements': [
                'Disciplined annual or semi-annual rebalancing',
                'Sufficient volatility spread',
                'Correlation below 0.70',
                'Tax-efficient rebalancing (use cash flows first)'
            ],
            'analysis_caveat': 'Bonus only materializes with DISCIPLINED rebalancing - many investors fail this'
        }

    def compare_to_ccf(self, ccf_return: Decimal, ccf_volatility: Decimal,
                      ccf_correlation_stocks: Decimal) -> Dict[str, Any]:
        """
        Compare PME to CCF (Collateralized Commodity Futures)

        Preference: CCF > PME because:
        - CCF has LOWER correlation with stocks
        - CCF provides BETTER inflation hedge
        - CCF avoids company-specific risks (no bankruptcy, management issues)
        - CCF fully collateralized (T-bills) = safer

        Args:
            ccf_return: CCF historical return
            ccf_volatility: CCF volatility
            ccf_correlation_stocks: CCF-stock correlation

        Returns:
            Comparison analysis
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient PME data'}

        # Calculate PME metrics
        pme_returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            pme_returns.append(ret)

        pme_avg_return = sum(pme_returns) / len(pme_returns)
        pme_volatility = self.math.calculate_volatility(pme_returns, annualized=True)

        # Assume moderate PME-stock correlation from research
        pme_correlation_stocks = Decimal('0.52')  # Typical from studies

        # Sharpe ratios
        rf = self.config.RISK_FREE_RATE
        pme_sharpe = (pme_avg_return - rf) / pme_volatility if pme_volatility > 0 else Decimal('0')
        ccf_sharpe = (ccf_return - rf) / ccf_volatility if ccf_volatility > 0 else Decimal('0')

        # Diversification benefit (lower correlation = better)
        pme_div_score = Decimal('1') - pme_correlation_stocks
        ccf_div_score = Decimal('1') - ccf_correlation_stocks

        return {
            'pme_metrics': {
                'avg_return': float(pme_avg_return),
                'volatility': float(pme_volatility),
                'sharpe_ratio': float(pme_sharpe),
                'stock_correlation': float(pme_correlation_stocks),
                'diversification_score': float(pme_div_score)
            },
            'ccf_metrics': {
                'avg_return': float(ccf_return),
                'volatility': float(ccf_volatility),
                'sharpe_ratio': float(ccf_sharpe),
                'stock_correlation': float(ccf_correlation_stocks),
                'diversification_score': float(ccf_div_score)
            },
            'winner_by_metric': {
                'return': 'PME' if pme_avg_return > ccf_return else 'CCF',
                'risk_adjusted_return': 'PME' if pme_sharpe > ccf_sharpe else 'CCF',
                'diversification': 'PME' if pme_div_score > ccf_div_score else 'CCF',
                'inflation_hedge': 'CCF (finding)',
                'crisis_reliability': 'CCF (more consistent)',
                'structural_safety': 'CCF (fully collateralized, no bankruptcy risk)'
            },
            'analysis_recommendation': 'CCF preferred over PME for most investors',
            'rationale': [
                'CCF has lower stock correlation (better diversifier)',
                'CCF provides actual commodity exposure (inflation hedge)',
                'CCF avoids company-specific risks',
                'PME = equity exposure with commodity label (misleading)'
            ]
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """
        Complete analytical verdict on PME
        Based on "Alternative Investments Analysis"

        Category: "THE FLAWED"

        Returns:
            Complete verdict with recommendations
        """
        return {
            'asset_class': 'Precious Metals Equities (PME)',
            'category': 'THE FLAWED',
            'overall_rating': '3/10 - Not recommended for most investors',

            'the_good': [
                'Potential rebalancing bonus from high volatility',
                'Some diversification benefit (correlation < 1.0)',
                'Liquid and easily tradable',
                'Lower costs than physical metal storage'
            ],

            'the_bad': [
                'HIGH correlation with stocks (0.45-0.60) - limited diversification',
                'LOW correlation with inflation (0.10-0.20) - NOT an inflation hedge',
                'Extreme volatility - frequent >35% drawdowns',
                'Unreliable crisis performance - sometimes helps, sometimes hurts',
                'Company-specific risks (management, bankruptcy, operational)',
                'Not true commodity exposure - equity exposure in disguise'
            ],

            'the_ugly': [
                'Marketed as "gold exposure" but behaves like stocks',
                'Investors buy expecting inflation hedge, get equity risk instead',
                'Crisis unreliability creates false sense of security',
                'Rebalancing bonus requires discipline most investors lack'
            ],

            'key_findings': {
                'inflation_hedge': 'NO - correlation too low (0.10-0.20)',
                'crisis_hedge': 'UNRELIABLE - inconsistent performance',
                'diversification': 'LIMITED - moderate correlation with stocks',
                'rebalancing_opportunity': 'POSSIBLE - but requires discipline',
                'better_alternative': 'CCF (Collateralized Commodity Futures)'
            },

            'analysis_quote': (
                '"Precious metals equities are stocks in disguise. Investors seeking commodity exposure '
                'or inflation protection should look to CCF, not PME. The correlation data is clear: '
                'PME behaves far more like equities than commodities."'
            ),

            'investment_recommendation': {
                'suitable_for': [
                    'Sophisticated investors understanding true risks',
                    'Portfolios already well-diversified',
                    'Investors with rebalancing discipline',
                    'Small allocation only (≤5% if used at all)'
                ],
                'not_suitable_for': [
                    'Inflation hedging (use TIPS or CCF instead)',
                    'Crisis protection (unreliable)',
                    'Conservative investors (too volatile)',
                    'Core portfolio holdings (equity risk already covered)'
                ],
                'better_alternatives': [
                    'TIPS (for inflation protection)',
                    'CCF (for commodity exposure)',
                    'Broad equity index (for equity exposure)',
                    'Gold ETF (for direct gold exposure without company risk)'
                ]
            },

            'final_verdict': (
                'PME is a FLAWED alternative investment. While not outright bad, it fails to deliver '
                'on its implicit promises (inflation hedge, crisis protection). The high correlation '
                'with stocks and extreme volatility make it unsuitable for most portfolios. Investors '
                'seeking true commodity exposure should use CCF instead.'
            )
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """
        Calculate comprehensive PME metrics

        Returns:
            All key metrics
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient data'}

        # Performance metrics
        performance = self.calculate_performance()

        # Drawdown analysis
        drawdowns = self.calculate_drawdowns()

        return {
            'asset_class': 'Precious Metals Equities (PME)',
            'asset_name': self.asset_name,
            'performance': performance,
            'drawdown_analysis': drawdowns,
            'analysis_category': 'THE FLAWED',
            'risk_level': 'High - Equity-like volatility',
            'inflation_hedge': 'NO - Low correlation with inflation',
            'crisis_reliability': 'Unreliable - Inconsistent performance',
            'recommended_allocation': '0-5% maximum (if used at all)',
            'better_alternatives': ['CCF', 'TIPS', 'Physical Gold ETF']
        }

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV"""
        if not self.market_data:
            return Decimal('0')
        return self.market_data[-1].price

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
            'risk_rating': 'High - comparable to or exceeding equity volatility'
        }

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive PME valuation summary"""
        return {
            "asset_overview": {
                "asset_class": "Precious Metals Equities (PME)",
                "asset_name": self.asset_name,
                "benchmark_stock": self.benchmark_stock,
                "benchmark_gold": self.benchmark_gold
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE FLAWED",
            "recommendation": "Avoid - use CCF for commodity exposure or TIPS for inflation protection"
        }


# Export
__all__ = ['PreciousMetalsEquityAnalyzer']
