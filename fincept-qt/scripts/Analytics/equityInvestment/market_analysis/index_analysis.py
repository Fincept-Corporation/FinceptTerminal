
"""Equity Investment Index Analysis Module
======================================

Market index and benchmark analysis

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements and SEC filings
  - Market price data and trading volume information
  - Industry reports and competitive analysis data
  - Management guidance and analyst estimates
  - Economic indicators affecting equity markets

OUTPUT:
  - Equity valuation models and fair value estimates
  - Fundamental analysis metrics and financial ratios
  - Investment recommendations and target prices
  - Risk assessments and portfolio implications
  - Sector and industry comparative analysis

PARAMETERS:
  - valuation_method: Primary valuation methodology (default: 'DCF')
  - discount_rate: Discount rate for valuation (default: 0.10)
  - terminal_growth: Terminal growth rate assumption (default: 0.025)
  - earnings_multiple: Target earnings multiple (default: 15.0)
  - reporting_currency: Reporting currency (default: 'USD')
"""



import numpy as np
import pandas as pd
from typing import List, Dict, Any, Optional, Tuple, Union
from dataclasses import dataclass
from enum import Enum
import math

from .base_models import (
    BaseMarketAnalysisModel, ValidationError, FinceptAnalyticsError
)


class IndexWeightingMethod(Enum):
    """Index weighting methodologies"""
    PRICE_WEIGHTED = "price_weighted"
    MARKET_CAP_WEIGHTED = "market_cap_weighted"
    EQUAL_WEIGHTED = "equal_weighted"
    FUNDAMENTAL_WEIGHTED = "fundamental_weighted"
    FLOAT_ADJUSTED = "float_adjusted"


class IndexType(Enum):
    """Types of market indexes"""
    EQUITY = "equity"
    FIXED_INCOME = "fixed_income"
    COMMODITY = "commodity"
    ALTERNATIVE = "alternative"
    MULTI_ASSET = "multi_asset"


@dataclass
class IndexConstituent:
    """Individual component of an index"""
    symbol: str
    name: str
    shares_outstanding: float
    float_shares: float
    current_price: float
    market_cap: float
    weight: float
    sector: str


@dataclass
class IndexPerformance:
    """Index performance metrics"""
    index_value: float
    price_return: float
    total_return: float
    dividend_yield: float
    volatility: float
    period_start: str
    period_end: str


class IndexConstructor:
    """Index construction and management framework"""

    def __init__(self, name: str, weighting_method: IndexWeightingMethod):
        self.name = name
        self.weighting_method = weighting_method
        self.constituents = []
        self.base_value = 1000  # Default base index value
        self.base_date = None
        self.divisor = 1.0

    def add_constituent(self, constituent: IndexConstituent):
        """Add a constituent to the index"""
        self.constituents.append(constituent)

    def remove_constituent(self, symbol: str):
        """Remove a constituent from the index"""
        self.constituents = [c for c in self.constituents if c.symbol != symbol]

    def calculate_weights(self) -> Dict[str, float]:
        """Calculate weights based on weighting methodology"""

        if not self.constituents:
            return {}

        weights = {}

        if self.weighting_method == IndexWeightingMethod.PRICE_WEIGHTED:
            # Weight by price
            total_price = sum(c.current_price for c in self.constituents)
            for constituent in self.constituents:
                weights[constituent.symbol] = constituent.current_price / total_price

        elif self.weighting_method == IndexWeightingMethod.MARKET_CAP_WEIGHTED:
            # Weight by market capitalization
            total_market_cap = sum(c.market_cap for c in self.constituents)
            for constituent in self.constituents:
                weights[constituent.symbol] = constituent.market_cap / total_market_cap

        elif self.weighting_method == IndexWeightingMethod.EQUAL_WEIGHTED:
            # Equal weight for all constituents
            equal_weight = 1.0 / len(self.constituents)
            for constituent in self.constituents:
                weights[constituent.symbol] = equal_weight

        elif self.weighting_method == IndexWeightingMethod.FLOAT_ADJUSTED:
            # Weight by float-adjusted market cap
            total_float_cap = sum(c.float_shares * c.current_price for c in self.constituents)
            for constituent in self.constituents:
                float_cap = constituent.float_shares * constituent.current_price
                weights[constituent.symbol] = float_cap / total_float_cap

        elif self.weighting_method == IndexWeightingMethod.FUNDAMENTAL_WEIGHTED:
            # Weight by fundamental metrics (simplified: using market cap as proxy)
            total_fundamental = sum(c.market_cap for c in self.constituents)
            for constituent in self.constituents:
                weights[constituent.symbol] = constituent.market_cap / total_fundamental

        return weights

    def calculate_index_value(self, prices: Dict[str, float], base_prices: Dict[str, float] = None) -> float:
        """Calculate current index value"""

        if not self.constituents:
            return self.base_value

        if base_prices is None:
            base_prices = {c.symbol: c.current_price for c in self.constituents}

        if self.weighting_method == IndexWeightingMethod.PRICE_WEIGHTED:
            return self._calculate_price_weighted_value(prices, base_prices)
        elif self.weighting_method in [IndexWeightingMethod.MARKET_CAP_WEIGHTED,
                                       IndexWeightingMethod.FLOAT_ADJUSTED]:
            return self._calculate_cap_weighted_value(prices, base_prices)
        elif self.weighting_method == IndexWeightingMethod.EQUAL_WEIGHTED:
            return self._calculate_equal_weighted_value(prices, base_prices)
        else:
            return self._calculate_cap_weighted_value(prices, base_prices)  # Default

    def _calculate_price_weighted_value(self, prices: Dict[str, float],
                                        base_prices: Dict[str, float]) -> float:
        """Calculate price-weighted index value"""
        current_sum = sum(prices.get(c.symbol, c.current_price) for c in self.constituents)
        base_sum = sum(base_prices.get(c.symbol, c.current_price) for c in self.constituents)

        return self.base_value * (current_sum / base_sum) / self.divisor

    def _calculate_cap_weighted_value(self, prices: Dict[str, float],
                                      base_prices: Dict[str, float]) -> float:
        """Calculate market cap weighted index value"""
        current_cap = 0
        base_cap = 0

        for constituent in self.constituents:
            shares = (constituent.float_shares if self.weighting_method == IndexWeightingMethod.FLOAT_ADJUSTED
                      else constituent.shares_outstanding)

            current_price = prices.get(constituent.symbol, constituent.current_price)
            base_price = base_prices.get(constituent.symbol, constituent.current_price)

            current_cap += shares * current_price
            base_cap += shares * base_price

        return self.base_value * (current_cap / base_cap) if base_cap > 0 else self.base_value

    def _calculate_equal_weighted_value(self, prices: Dict[str, float],
                                        base_prices: Dict[str, float]) -> float:
        """Calculate equal-weighted index value"""
        returns = []

        for constituent in self.constituents:
            current_price = prices.get(constituent.symbol, constituent.current_price)
            base_price = base_prices.get(constituent.symbol, constituent.current_price)

            if base_price > 0:
                returns.append(current_price / base_price)

        if returns:
            avg_return = np.mean(returns)
            return self.base_value * avg_return
        else:
            return self.base_value


class IndexCalculator:
    """Calculate index returns and performance metrics"""

    @staticmethod
    def calculate_price_return(current_value: float, previous_value: float) -> float:
        """Calculate price return"""
        if previous_value <= 0:
            raise ValidationError("Previous value must be positive")
        return (current_value - previous_value) / previous_value

    @staticmethod
    def calculate_total_return(current_value: float, previous_value: float,
                               dividend_income: float) -> float:
        """Calculate total return including dividends"""
        if previous_value <= 0:
            raise ValidationError("Previous value must be positive")
        return ((current_value - previous_value) + dividend_income) / previous_value

    @staticmethod
    def calculate_index_dividend_yield(constituents: List[IndexConstituent],
                                       weights: Dict[str, float],
                                       dividend_yields: Dict[str, float]) -> float:
        """Calculate weighted average dividend yield"""
        total_yield = 0

        for constituent in constituents:
            weight = weights.get(constituent.symbol, 0)
            div_yield = dividend_yields.get(constituent.symbol, 0)
            total_yield += weight * div_yield

        return total_yield

    @staticmethod
    def calculate_index_volatility(returns: pd.Series, annualize: bool = True) -> float:
        """Calculate index volatility"""
        volatility = returns.std()
        if annualize:
            # Annualize assuming daily returns
            volatility *= np.sqrt(252)
        return volatility

    @staticmethod
    def calculate_tracking_error(index_returns: pd.Series, benchmark_returns: pd.Series) -> float:
        """Calculate tracking error between index and benchmark"""
        if len(index_returns) != len(benchmark_returns):
            raise ValidationError("Return series must have same length")

        excess_returns = index_returns - benchmark_returns
        return excess_returns.std() * np.sqrt(252)  # Annualized


class IndexRebalancer:
    """Handle index rebalancing and reconstitution"""

    def __init__(self, index_constructor: IndexConstructor):
        self.index_constructor = index_constructor
        self.rebalancing_frequency = "quarterly"  # monthly, quarterly, annually
        self.last_rebalance_date = None

    def needs_rebalancing(self, current_date: str, weights: Dict[str, float],
                          tolerance: float = 0.05) -> bool:
        """Determine if index needs rebalancing"""

        # Check weight drift
        target_weights = self.index_constructor.calculate_weights()

        for symbol in weights:
            if symbol in target_weights:
                weight_drift = abs(weights[symbol] - target_weights[symbol])
                if weight_drift > tolerance:
                    return True

        return False

    def rebalance_index(self, new_prices: Dict[str, float],
                        target_weights: Dict[str, float] = None) -> Dict[str, Any]:
        """Perform index rebalancing"""

        if target_weights is None:
            target_weights = self.index_constructor.calculate_weights()

        # Update constituent prices
        for constituent in self.index_constructor.constituents:
            if constituent.symbol in new_prices:
                constituent.current_price = new_prices[constituent.symbol]
                constituent.market_cap = constituent.shares_outstanding * constituent.current_price

        # Recalculate weights
        new_weights = self.index_constructor.calculate_weights()

        # Calculate rebalancing adjustments
        adjustments = {}
        for symbol in target_weights:
            if symbol in new_weights:
                adjustments[symbol] = target_weights[symbol] - new_weights[symbol]

        return {
            'new_weights': new_weights,
            'target_weights': target_weights,
            'adjustments': adjustments,
            'rebalancing_date': self.last_rebalance_date
        }

    def reconstitute_index(self, additions: List[IndexConstituent] = None,
                           deletions: List[str] = None) -> Dict[str, Any]:
        """Perform index reconstitution (adding/removing constituents)"""

        changes = {
            'additions': [],
            'deletions': [],
            'before_count': len(self.index_constructor.constituents),
            'after_count': 0
        }

        # Remove constituents
        if deletions:
            for symbol in deletions:
                self.index_constructor.remove_constituent(symbol)
                changes['deletions'].append(symbol)

        # Add new constituents
        if additions:
            for constituent in additions:
                self.index_constructor.add_constituent(constituent)
                changes['additions'].append(constituent.symbol)

        changes['after_count'] = len(self.index_constructor.constituents)

        # Recalculate weights after reconstitution
        new_weights = self.index_constructor.calculate_weights()
        changes['new_weights'] = new_weights

        return changes


class IndexAnalyzer(BaseMarketAnalysisModel):
    """Comprehensive index analysis and comparison"""

    def __init__(self):
        super().__init__("Index Analyzer", "Comprehensive index analysis")

    def validate_inputs(self, **kwargs) -> bool:
        """Validate inputs for index analysis"""
        index_data = kwargs.get('index_data')
        if index_data is None or index_data.empty:
            raise ValidationError("Index data is required for analysis")
        return True

    def analyze_market_data(self, market_data: pd.DataFrame) -> Dict[str, Any]:
        """Analyze index market data"""
        return self.comprehensive_index_analysis(market_data)

    def comprehensive_index_analysis(self, index_data: pd.DataFrame) -> Dict[str, Any]:
        """Perform comprehensive index analysis"""

        # Calculate returns
        price_returns = index_data['Close'].pct_change().dropna()

        # Performance metrics
        total_return = (index_data['Close'].iloc[-1] / index_data['Close'].iloc[0]) - 1
        annualized_return = ((1 + total_return) ** (252 / len(index_data))) - 1
        volatility = IndexCalculator.calculate_index_volatility(price_returns)
        sharpe_ratio = annualized_return / volatility if volatility > 0 else 0

        # Risk metrics
        max_drawdown = self.calculate_maximum_drawdown(index_data['Close'])
        var_95 = np.percentile(price_returns, 5)
        var_99 = np.percentile(price_returns, 1)

        # Performance by periods
        performance_periods = self.calculate_period_performance(index_data)

        return {
            'performance_summary': {
                'total_return': total_return,
                'annualized_return': annualized_return,
                'volatility': volatility,
                'sharpe_ratio': sharpe_ratio,
                'max_drawdown': max_drawdown,
                'var_95': var_95,
                'var_99': var_99
            },
            'period_performance': performance_periods,
            'statistical_measures': {
                'skewness': price_returns.skew(),
                'kurtosis': price_returns.kurtosis(),
                'positive_days': (price_returns > 0).sum() / len(price_returns),
                'average_positive_return': price_returns[price_returns > 0].mean(),
                'average_negative_return': price_returns[price_returns < 0].mean()
            }
        }

    def calculate_maximum_drawdown(self, price_series: pd.Series) -> float:
        """Calculate maximum drawdown"""
        cumulative = (1 + price_series.pct_change()).cumprod()
        running_max = cumulative.expanding().max()
        drawdown = (cumulative - running_max) / running_max
        return drawdown.min()

    def calculate_period_performance(self, index_data: pd.DataFrame) -> Dict[str, float]:
        """Calculate performance over different periods"""

        current_price = index_data['Close'].iloc[-1]

        periods = {
            '1_day': 1,
            '1_week': 5,
            '1_month': 21,
            '3_months': 63,
            '6_months': 126,
            '1_year': 252,
            '3_years': 756,
            '5_years': 1260
        }

        performance = {}

        for period_name, days in periods.items():
            if len(index_data) > days:
                past_price = index_data['Close'].iloc[-days - 1]
                period_return = (current_price / past_price) - 1
                performance[period_name] = period_return

        return performance

    def compare_indexes(self, index_data_dict: Dict[str, pd.DataFrame]) -> Dict[str, Any]:
        """Compare multiple indexes"""

        comparison_results = {}

        for index_name, data in index_data_dict.items():
            analysis = self.comprehensive_index_analysis(data)
            comparison_results[index_name] = analysis['performance_summary']

        # Create comparison matrix
        metrics = ['total_return', 'annualized_return', 'volatility', 'sharpe_ratio', 'max_drawdown']
        comparison_matrix = pd.DataFrame(
            {metric: {name: results[metric] for name, results in comparison_results.items()}
             for metric in metrics}
        ).T

        # Rankings
        rankings = {}
        for metric in metrics:
            ascending = metric in ['volatility', 'max_drawdown']  # Lower is better for these
            rankings[metric] = comparison_matrix.loc[metric].rank(ascending=ascending)

        return {
            'individual_analysis': comparison_results,
            'comparison_matrix': comparison_matrix.to_dict(),
            'rankings': rankings
        }

    def analyze_index_composition(self, constituents: List[IndexConstituent]) -> Dict[str, Any]:
        """Analyze index composition and concentration"""

        if not constituents:
            return {'error': 'No constituents provided'}

        # Sector analysis
        sector_weights = {}
        for constituent in constituents:
            sector = constituent.sector
            if sector in sector_weights:
                sector_weights[sector] += constituent.weight
            else:
                sector_weights[sector] = constituent.weight

        # Concentration metrics
        weights = [c.weight for c in constituents]

        # Herfindahl-Hirschman Index (concentration measure)
        hhi = sum(w ** 2 for w in weights)

        # Top holdings concentration
        sorted_weights = sorted(weights, reverse=True)
        top_5_concentration = sum(sorted_weights[:5]) if len(sorted_weights) >= 5 else sum(sorted_weights)
        top_10_concentration = sum(sorted_weights[:10]) if len(sorted_weights) >= 10 else sum(sorted_weights)

        # Effective number of stocks
        effective_stocks = 1 / hhi if hhi > 0 else len(constituents)

        return {
            'composition_metrics': {
                'total_constituents': len(constituents),
                'herfindahl_index': hhi,
                'effective_number_of_stocks': effective_stocks,
                'top_5_concentration': top_5_concentration,
                'top_10_concentration': top_10_concentration
            },
            'sector_allocation': sector_weights,
            'largest_holdings': [
                {
                    'symbol': c.symbol,
                    'name': c.name,
                    'weight': c.weight,
                    'sector': c.sector
                }
                for c in sorted(constituents, key=lambda x: x.weight, reverse=True)[:10]
            ]
        }


class SpecializedIndexes:
    """Implementation of specialized index types"""

    @staticmethod
    def create_equity_index(constituents: List[IndexConstituent],
                            weighting_method: IndexWeightingMethod = IndexWeightingMethod.MARKET_CAP_WEIGHTED,
                            name: str = "Custom Equity Index") -> IndexConstructor:
        """Create equity index"""

        index = IndexConstructor(name, weighting_method)
        for constituent in constituents:
            index.add_constituent(constituent)

        return index

    @staticmethod
    def create_fixed_income_index(bond_data: List[Dict[str, Any]],
                                  name: str = "Custom Bond Index") -> Dict[str, Any]:
        """Create fixed income index (simplified)"""

        total_market_value = sum(bond['market_value'] for bond in bond_data)

        weighted_yield = sum(
            (bond['market_value'] / total_market_value) * bond['yield']
            for bond in bond_data
        )

        weighted_duration = sum(
            (bond['market_value'] / total_market_value) * bond['duration']
            for bond in bond_data
        )

        weighted_credit_quality = sum(
            (bond['market_value'] / total_market_value) * bond.get('credit_score', 0)
            for bond in bond_data
        )

        return {
            'name': name,
            'total_market_value': total_market_value,
            'weighted_yield': weighted_yield,
            'weighted_duration': weighted_duration,
            'weighted_credit_quality': weighted_credit_quality,
            'number_of_bonds': len(bond_data)
        }

    @staticmethod
    def create_commodity_index(commodity_data: List[Dict[str, Any]],
                               name: str = "Custom Commodity Index") -> Dict[str, Any]:
        """Create commodity index"""

        total_weight = sum(commodity['weight'] for commodity in commodity_data)

        # Normalize weights
        for commodity in commodity_data:
            commodity['normalized_weight'] = commodity['weight'] / total_weight

        # Calculate index value (simplified)
        index_value = sum(
            commodity['normalized_weight'] * commodity['price_index']
            for commodity in commodity_data
        )

        return {
            'name': name,
            'index_value': index_value,
            'constituents': commodity_data,
            'sectors': list(set(c.get('sector', 'Unknown') for c in commodity_data))
        }


# Convenience functions
def create_price_weighted_index(symbols: List[str], prices: List[float],
                                names: List[str] = None) -> IndexConstructor:
    """Quick price-weighted index creation"""

    index = IndexConstructor("Price Weighted Index", IndexWeightingMethod.PRICE_WEIGHTED)

    for i, symbol in enumerate(symbols):
        name = names[i] if names and i < len(names) else symbol
        constituent = IndexConstituent(
            symbol=symbol,
            name=name,
            shares_outstanding=1000000,  # Default
            float_shares=1000000,
            current_price=prices[i],
            market_cap=prices[i] * 1000000,
            weight=0,  # Will be calculated
            sector="Unknown"
        )
        index.add_constituent(constituent)

    return index


def create_market_cap_index(symbols: List[str], prices: List[float],
                            shares: List[float], names: List[str] = None) -> IndexConstructor:
    """Quick market cap weighted index creation"""

    index = IndexConstructor("Market Cap Weighted Index", IndexWeightingMethod.MARKET_CAP_WEIGHTED)

    for i, symbol in enumerate(symbols):
        name = names[i] if names and i < len(names) else symbol
        constituent = IndexConstituent(
            symbol=symbol,
            name=name,
            shares_outstanding=shares[i],
            float_shares=shares[i] * 0.8,  # Assume 80% float
            current_price=prices[i],
            market_cap=prices[i] * shares[i],
            weight=0,  # Will be calculated
            sector="Unknown"
        )
        index.add_constituent(constituent)

    return index