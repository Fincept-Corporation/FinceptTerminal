"""
Equity Investment Market Structure Module
======================================

Market structure and microstructure analysis

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
import warnings

from ..base.base_models import (
    BaseMarketAnalysisModel, SecurityType, ValidationError,
    FinceptAnalyticsError
)


class MarketType(Enum):
    """Types of financial markets"""
    QUOTE_DRIVEN = "quote_driven"
    ORDER_DRIVEN = "order_driven"
    BROKERED = "brokered"
    HYBRID = "hybrid"


class OrderType(Enum):
    """Types of trading orders"""
    MARKET_ORDER = "market_order"
    LIMIT_ORDER = "limit_order"
    STOP_ORDER = "stop_order"
    STOP_LIMIT_ORDER = "stop_limit_order"


class OrderValidity(Enum):
    """Order validity instructions"""
    DAY_ORDER = "day_order"
    GOOD_TILL_CANCELED = "good_till_canceled"
    IMMEDIATE_OR_CANCEL = "immediate_or_cancel"
    FILL_OR_KILL = "fill_or_kill"


class PositionType(Enum):
    """Investment position types"""
    LONG = "long"
    SHORT = "short"


@dataclass
class SecurityCharacteristics:
    """Security characteristics structure"""
    security_type: SecurityType
    voting_rights: bool
    dividend_rights: bool
    residual_claim: bool
    maturity: Optional[str]
    coupon_rate: Optional[float]
    face_value: Optional[float]
    callable: bool
    convertible: bool


@dataclass
class MarginAccountData:
    """Margin account information"""
    initial_margin_requirement: float
    maintenance_margin_requirement: float
    account_equity: float
    total_value: float
    borrowed_amount: float
    leverage_ratio: float


@dataclass
class TradingOrder:
    """Trading order structure"""
    order_type: OrderType
    order_validity: OrderValidity
    security_symbol: str
    quantity: int
    price: Optional[float]
    stop_price: Optional[float]
    position_type: PositionType


class SecurityClassifier:
    """Classify and analyze different types of securities"""

    def __init__(self):
        # Security type characteristics
        self.security_characteristics = {
            SecurityType.COMMON_STOCK: {
                'voting_rights': True,
                'dividend_rights': True,
                'residual_claim': True,
                'maturity': None,
                'priority_in_liquidation': 'Last',
                'income_type': 'Variable',
                'risk_level': 'High'
            },
            SecurityType.PREFERRED_STOCK: {
                'voting_rights': False,
                'dividend_rights': True,
                'residual_claim': False,
                'maturity': None,
                'priority_in_liquidation': 'Before Common',
                'income_type': 'Fixed',
                'risk_level': 'Medium'
            },
            SecurityType.CORPORATE_BOND: {
                'voting_rights': False,
                'dividend_rights': False,
                'residual_claim': False,
                'maturity': 'Fixed',
                'priority_in_liquidation': 'Before Equity',
                'income_type': 'Fixed',
                'risk_level': 'Low-Medium'
            },
            SecurityType.GOVERNMENT_BOND: {
                'voting_rights': False,
                'dividend_rights': False,
                'residual_claim': False,
                'maturity': 'Fixed',
                'priority_in_liquidation': 'Highest',
                'income_type': 'Fixed',
                'risk_level': 'Low'
            }
        }

    def classify_equity_security(self, characteristics: Dict[str, Any]) -> Dict[str, Any]:
        """Classify equity security and its features"""

        voting_rights = characteristics.get('voting_rights', True)
        dividend_preference = characteristics.get('dividend_preference', False)
        cumulative_dividends = characteristics.get('cumulative_dividends', False)
        callable = characteristics.get('callable', False)
        convertible = characteristics.get('convertible', False)

        # Determine security type
        if dividend_preference:
            security_type = SecurityType.PREFERRED_STOCK
            risk_level = "Medium"
        else:
            security_type = SecurityType.COMMON_STOCK
            risk_level = "High"

        # Additional features
        features = []
        if cumulative_dividends:
            features.append("Cumulative Dividends")
        if callable:
            features.append("Callable")
        if convertible:
            features.append("Convertible")
        if not voting_rights:
            features.append("Non-Voting")

        return {
            'security_type': security_type,
            'risk_level': risk_level,
            'voting_rights': voting_rights,
            'dividend_preference': dividend_preference,
            'special_features': features,
            'investment_characteristics': self.get_investment_characteristics(security_type)
        }

    def classify_debt_security(self, characteristics: Dict[str, Any]) -> Dict[str, Any]:
        """Classify debt security and its features"""

        issuer_type = characteristics.get('issuer_type', 'Corporate')
        maturity_years = characteristics.get('maturity_years', 10)
        coupon_rate = characteristics.get('coupon_rate', 0.05)
        callable = characteristics.get('callable', False)
        convertible = characteristics.get('convertible', False)
        secured = characteristics.get('secured', False)

        # Determine security type and risk level
        if issuer_type.lower() in ['government', 'treasury', 'federal']:
            security_type = SecurityType.GOVERNMENT_BOND
            risk_level = "Low"
        else:
            security_type = SecurityType.CORPORATE_BOND
            risk_level = "Low-Medium"

        # Maturity classification
        if maturity_years <= 1:
            maturity_class = "Short-term"
        elif maturity_years <= 10:
            maturity_class = "Medium-term"
        else:
            maturity_class = "Long-term"

        # Special features
        features = []
        if callable:
            features.append("Callable")
        if convertible:
            features.append("Convertible")
        if secured:
            features.append("Secured")

        return {
            'security_type': security_type,
            'issuer_type': issuer_type,
            'maturity_classification': maturity_class,
            'risk_level': risk_level,
            'coupon_rate': coupon_rate,
            'special_features': features,
            'investment_characteristics': self.get_investment_characteristics(security_type)
        }

    def get_investment_characteristics(self, security_type: SecurityType) -> Dict[str, str]:
        """Get investment characteristics for security type"""
        return self.security_characteristics.get(security_type, {})

    def compare_security_types(self, security_types: List[SecurityType]) -> pd.DataFrame:
        """Compare characteristics of different security types"""

        comparison_data = []

        for sec_type in security_types:
            characteristics = self.security_characteristics.get(sec_type, {})
            characteristics['security_type'] = sec_type.value
            comparison_data.append(characteristics)

        return pd.DataFrame(comparison_data)


class MarginTradingCalculator:
    """Calculate margin trading metrics and requirements"""

    def __init__(self):
        # Standard margin requirements (can be customized)
        self.initial_margin_requirement = 0.50  # 50%
        self.maintenance_margin_requirement = 0.25  # 25%

    def calculate_margin_requirements(self, position_value: float,
                                      initial_margin: float = None,
                                      maintenance_margin: float = None) -> Dict[str, float]:
        """Calculate margin requirements for a position"""

        initial_req = initial_margin or self.initial_margin_requirement
        maintenance_req = maintenance_margin or self.maintenance_margin_requirement

        initial_margin_amount = position_value * initial_req
        maintenance_margin_amount = position_value * maintenance_req
        max_loan_amount = position_value - initial_margin_amount

        return {
            'position_value': position_value,
            'initial_margin_requirement': initial_req,
            'initial_margin_amount': initial_margin_amount,
            'maintenance_margin_requirement': maintenance_req,
            'maintenance_margin_amount': maintenance_margin_amount,
            'maximum_loan_amount': max_loan_amount,
            'leverage_ratio': position_value / initial_margin_amount if initial_margin_amount > 0 else 0
        }

    def calculate_margin_call_price(self, initial_price: float, initial_margin: float,
                                    loan_amount: float, shares: int,
                                    maintenance_margin: float = None) -> float:
        """Calculate price at which margin call occurs"""

        maintenance_req = maintenance_margin or self.maintenance_margin_requirement

        # Margin call price = (Loan Amount) / (Shares × (1 - Maintenance Margin))
        margin_call_price = loan_amount / (shares * (1 - maintenance_req))

        return margin_call_price

    def calculate_margin_return(self, initial_price: float, final_price: float,
                                initial_margin_percentage: float,
                                interest_rate: float = 0, time_period: float = 1) -> Dict[str, float]:
        """Calculate return on margin transaction"""

        # Price change
        price_change_percentage = (final_price - initial_price) / initial_price

        # Leverage effect
        leverage_ratio = 1 / initial_margin_percentage

        # Interest cost on borrowed funds
        borrowed_percentage = 1 - initial_margin_percentage
        interest_cost = borrowed_percentage * interest_rate * time_period

        # Return on margin transaction
        margin_return = (price_change_percentage * leverage_ratio) - interest_cost

        # Compare to unleveraged return
        unleveraged_return = price_change_percentage

        return {
            'unleveraged_return': unleveraged_return,
            'margin_return': margin_return,
            'leverage_ratio': leverage_ratio,
            'interest_cost': interest_cost,
            'leverage_benefit': margin_return - unleveraged_return
        }

    def analyze_margin_account(self, account_data: MarginAccountData) -> Dict[str, Any]:
        """Analyze margin account status"""

        # Current margin percentage
        current_margin = account_data.account_equity / account_data.total_value

        # Excess margin above maintenance requirement
        excess_margin = current_margin - account_data.maintenance_margin_requirement

        # Buying power
        buying_power = account_data.account_equity / account_data.initial_margin_requirement

        # Margin call status
        margin_call_needed = current_margin < account_data.maintenance_margin_requirement

        # Amount needed to meet maintenance requirement
        if margin_call_needed:
            required_equity = account_data.total_value * account_data.maintenance_margin_requirement
            margin_call_amount = required_equity - account_data.account_equity
        else:
            margin_call_amount = 0

        return {
            'current_margin_percentage': current_margin,
            'excess_margin': excess_margin,
            'buying_power': buying_power,
            'margin_call_needed': margin_call_needed,
            'margin_call_amount': margin_call_amount,
            'account_status': 'Margin Call' if margin_call_needed else 'Good Standing'
        }


class OrderAnalyzer:
    """Analyze trading orders and execution instructions"""

    def analyze_market_order(self, order: TradingOrder, current_bid: float,
                             current_ask: float) -> Dict[str, Any]:
        """Analyze market order characteristics"""

        if order.position_type == PositionType.LONG:
            expected_execution_price = current_ask
            price_impact = "Buy at ask price"
        else:  # SHORT
            expected_execution_price = current_bid
            price_impact = "Sell at bid price"

        bid_ask_spread = current_ask - current_bid
        spread_percentage = bid_ask_spread / ((current_bid + current_ask) / 2) * 100

        return {
            'order_type': order.order_type.value,
            'execution_certainty': 'High',
            'price_certainty': 'Low',
            'expected_execution_price': expected_execution_price,
            'bid_ask_spread': bid_ask_spread,
            'spread_percentage': spread_percentage,
            'price_impact': price_impact,
            'advantages': ['Immediate execution', 'High fill probability'],
            'disadvantages': ['Price uncertainty', 'Market impact cost']
        }

    def analyze_limit_order(self, order: TradingOrder, current_price: float) -> Dict[str, Any]:
        """Analyze limit order characteristics"""

        if not order.price:
            raise ValidationError("Limit order requires price specification")

        if order.position_type == PositionType.LONG:
            # Buy limit order
            executable = order.price >= current_price
            price_improvement = current_price - order.price if executable else 0
        else:
            # Sell limit order
            executable = order.price <= current_price
            price_improvement = order.price - current_price if executable else 0

        return {
            'order_type': order.order_type.value,
            'limit_price': order.price,
            'current_market_price': current_price,
            'immediately_executable': executable,
            'price_improvement': price_improvement,
            'execution_certainty': 'Medium',
            'price_certainty': 'High',
            'advantages': ['Price protection', 'Potential price improvement'],
            'disadvantages': ['Execution uncertainty', 'Opportunity cost if not filled']
        }

    def analyze_stop_order(self, order: TradingOrder, current_price: float) -> Dict[str, Any]:
        """Analyze stop order characteristics"""

        if not order.stop_price:
            raise ValidationError("Stop order requires stop price specification")

        if order.position_type == PositionType.LONG:
            # Buy stop (stop above current price)
            activated = current_price >= order.stop_price
            order_purpose = "Momentum/Breakout buying"
        else:
            # Sell stop (stop below current price)
            activated = current_price <= order.stop_price
            order_purpose = "Loss limitation/Risk management"

        return {
            'order_type': order.order_type.value,
            'stop_price': order.stop_price,
            'current_price': current_price,
            'activated': activated,
            'order_purpose': order_purpose,
            'execution_certainty': 'Medium' if activated else 'Low',
            'price_certainty': 'Low',
            'advantages': ['Risk management', 'No monitoring required'],
            'disadvantages': ['Price uncertainty when executed', 'Possible gap risk']
        }

    def compare_order_types(self, current_bid: float, current_ask: float) -> pd.DataFrame:
        """Compare characteristics of different order types"""

        order_comparison = [
            {
                'order_type': 'Market Order',
                'execution_certainty': 'High',
                'price_certainty': 'Low',
                'typical_use': 'Immediate execution needed',
                'price_paid_received': f'Ask ({current_ask}) / Bid ({current_bid})'
            },
            {
                'order_type': 'Limit Order',
                'execution_certainty': 'Medium',
                'price_certainty': 'High',
                'typical_use': 'Price protection desired',
                'price_paid_received': 'Limit price or better'
            },
            {
                'order_type': 'Stop Order',
                'execution_certainty': 'Medium',
                'price_certainty': 'Low',
                'typical_use': 'Risk management/Momentum',
                'price_paid_received': 'Market price when triggered'
            },
            {
                'order_type': 'Stop-Limit Order',
                'execution_certainty': 'Low',
                'price_certainty': 'High',
                'typical_use': 'Risk management with price protection',
                'price_paid_received': 'Limit price or better (if filled)'
            }
        ]

        return pd.DataFrame(order_comparison)


class MarketStructureAnalyzer(BaseMarketAnalysisModel):
    """Analyze market structure and trading mechanisms"""

    def __init__(self):
        super().__init__("Market Structure Analyzer", "Market organization and structure analysis")

    def validate_inputs(self, **kwargs) -> bool:
        """Validate inputs for market structure analysis"""
        return True

    def analyze_market_data(self, market_data: pd.DataFrame) -> Dict[str, Any]:
        """Analyze market structure characteristics"""

        return {
            'market_microstructure': self.analyze_market_microstructure(market_data),
            'liquidity_analysis': self.analyze_market_liquidity(market_data),
            'trading_costs': self.estimate_trading_costs(market_data),
            'market_efficiency_indicators': self.calculate_efficiency_indicators(market_data)
        }

    def analyze_market_microstructure(self, market_data: pd.DataFrame) -> Dict[str, Any]:
        """Analyze market microstructure characteristics"""

        if 'Bid' not in market_data.columns or 'Ask' not in market_data.columns:
            return {'error': 'Bid and Ask data required for microstructure analysis'}

        # Bid-ask spread analysis
        spreads = market_data['Ask'] - market_data['Bid']
        relative_spreads = spreads / ((market_data['Bid'] + market_data['Ask']) / 2)

        # Volume analysis
        if 'Volume' in market_data.columns:
            volume_stats = {
                'average_volume': market_data['Volume'].mean(),
                'volume_volatility': market_data['Volume'].std(),
                'high_volume_days': (market_data['Volume'] > market_data['Volume'].quantile(0.9)).sum()
            }
        else:
            volume_stats = {'error': 'Volume data not available'}

        return {
            'bid_ask_spreads': {
                'average_spread': spreads.mean(),
                'average_relative_spread': relative_spreads.mean(),
                'spread_volatility': spreads.std(),
                'min_spread': spreads.min(),
                'max_spread': spreads.max()
            },
            'volume_statistics': volume_stats,
            'market_depth_indicators': self.analyze_market_depth(market_data)
        }

    def analyze_market_depth(self, market_data: pd.DataFrame) -> Dict[str, Any]:
        """Analyze market depth characteristics"""

        # Simplified market depth analysis using bid-ask data
        if 'Bid' in market_data.columns and 'Ask' in market_data.columns:
            spreads = market_data['Ask'] - market_data['Bid']

            # Market depth proxy (inverse of spread)
            depth_proxy = 1 / spreads.replace(0, np.nan)

            return {
                'average_depth_proxy': depth_proxy.mean(),
                'depth_stability': 1 / depth_proxy.std() if depth_proxy.std() > 0 else float('inf'),
                'thin_market_periods': (spreads > spreads.quantile(0.9)).sum(),
                'market_quality': 'High' if spreads.mean() < 0.01 else 'Medium' if spreads.mean() < 0.05 else 'Low'
            }
        else:
            return {'error': 'Insufficient data for depth analysis'}

    def analyze_market_liquidity(self, market_data: pd.DataFrame) -> Dict[str, Any]:
        """Analyze market liquidity characteristics"""

        liquidity_measures = {}

        # Bid-ask spread (tightness)
        if 'Bid' in market_data.columns and 'Ask' in market_data.columns:
            spreads = market_data['Ask'] - market_data['Bid']
            mid_prices = (market_data['Bid'] + market_data['Ask']) / 2
            relative_spreads = spreads / mid_prices

            liquidity_measures['tightness'] = {
                'average_relative_spread': relative_spreads.mean(),
                'spread_consistency': 1 / relative_spreads.std() if relative_spreads.std() > 0 else float('inf')
            }

        # Volume-based measures (depth)
        if 'Volume' in market_data.columns:
            liquidity_measures['depth'] = {
                'average_daily_volume': market_data['Volume'].mean(),
                'volume_consistency': 1 / market_data['Volume'].std() * market_data['Volume'].mean() if market_data[
                                                                                                            'Volume'].std() > 0 else float(
                    'inf')
            }

        # Price impact measures (resilience)
        if 'Close' in market_data.columns:
            returns = market_data['Close'].pct_change().dropna()

            liquidity_measures['resilience'] = {
                'return_volatility': returns.std(),
                'price_resilience': 1 / returns.std() if returns.std() > 0 else float('inf')
            }

        # Overall liquidity score
        liquidity_score = self.calculate_liquidity_score(liquidity_measures)

        return {
            'liquidity_measures': liquidity_measures,
            'overall_liquidity_score': liquidity_score,
            'liquidity_rating': self.rate_liquidity(liquidity_score)
        }

    def calculate_liquidity_score(self, liquidity_measures: Dict[str, Any]) -> float:
        """Calculate overall liquidity score"""

        score = 0
        factors = 0

        # Tightness component (lower spreads = higher liquidity)
        if 'tightness' in liquidity_measures:
            spread = liquidity_measures['tightness']['average_relative_spread']
            if spread > 0:
                tightness_score = max(0, 1 - spread * 100)  # Convert to 0-1 scale
                score += tightness_score
                factors += 1

        # Depth component (higher volume = higher liquidity)
        if 'depth' in liquidity_measures:
            # Normalized volume score (simplified)
            depth_score = 0.7  # Placeholder - would need market benchmarks
            score += depth_score
            factors += 1

        # Resilience component (lower volatility = higher liquidity)
        if 'resilience' in liquidity_measures:
            volatility = liquidity_measures['resilience']['return_volatility']
            if volatility > 0:
                resilience_score = max(0, 1 - volatility * 10)  # Simplified scaling
                score += resilience_score
                factors += 1

        return score / factors if factors > 0 else 0

    def rate_liquidity(self, liquidity_score: float) -> str:
        """Rate liquidity based on score"""

        if liquidity_score >= 0.8:
            return "High Liquidity"
        elif liquidity_score >= 0.6:
            return "Medium Liquidity"
        elif liquidity_score >= 0.4:
            return "Low Liquidity"
        else:
            return "Very Low Liquidity"

    def estimate_trading_costs(self, market_data: pd.DataFrame) -> Dict[str, Any]:
        """Estimate trading costs"""

        trading_costs = {}

        # Bid-ask spread cost
        if 'Bid' in market_data.columns and 'Ask' in market_data.columns:
            spreads = market_data['Ask'] - market_data['Bid']
            mid_prices = (market_data['Bid'] + market_data['Ask']) / 2
            spread_cost = spreads / mid_prices / 2  # Half-spread cost

            trading_costs['bid_ask_cost'] = {
                'average_spread_cost': spread_cost.mean(),
                'min_spread_cost': spread_cost.min(),
                'max_spread_cost': spread_cost.max()
            }

        # Market impact cost estimation (simplified)
        if 'Volume' in market_data.columns and 'Close' in market_data.columns:
            returns = market_data['Close'].pct_change().abs()
            volume = market_data['Volume']

            # Proxy for market impact: higher returns with lower volume suggest higher impact
            impact_proxy = returns / (volume / volume.mean())

            trading_costs['market_impact'] = {
                'average_impact_proxy': impact_proxy.mean(),
                'impact_volatility': impact_proxy.std()
            }

        # Total estimated trading cost
        if 'bid_ask_cost' in trading_costs:
            estimated_total_cost = trading_costs['bid_ask_cost']['average_spread_cost']
            if 'market_impact' in trading_costs:
                estimated_total_cost += trading_costs['market_impact']['average_impact_proxy'] * 0.5

            trading_costs['estimated_total_cost'] = estimated_total_cost

        return trading_costs

    def calculate_efficiency_indicators(self, market_data: pd.DataFrame) -> Dict[str, Any]:
        """Calculate market efficiency indicators"""

        if 'Close' not in market_data.columns:
            return {'error': 'Price data required for efficiency analysis'}

        returns = market_data['Close'].pct_change().dropna()

        # Return predictability (lower = more efficient)
        autocorrelation = returns.autocorr(lag=1)

        # Volatility clustering
        squared_returns = returns ** 2
        volatility_clustering = squared_returns.autocorr(lag=1)

        # Price discovery efficiency
        if len(returns) > 20:
            # Variance ratio test (simplified)
            returns_2day = returns.rolling(2).sum().dropna()
            variance_ratio = (returns_2day.var() / 2) / returns.var()
        else:
            variance_ratio = 1

        return {
            'return_autocorrelation': autocorrelation,
            'volatility_clustering': volatility_clustering,
            'variance_ratio': variance_ratio,
            'efficiency_assessment': self.assess_efficiency(autocorrelation, variance_ratio)
        }

    def assess_efficiency(self, autocorr: float, variance_ratio: float) -> str:
        """Assess market efficiency based on indicators"""

        # Efficient markets should have low autocorrelation and variance ratio near 1
        if abs(autocorr) < 0.05 and abs(variance_ratio - 1) < 0.1:
            return "Highly Efficient"
        elif abs(autocorr) < 0.1 and abs(variance_ratio - 1) < 0.2:
            return "Moderately Efficient"
        else:
            return "Less Efficient"


class FinancialSystemAnalyzer:
    """Analyze financial system functions and intermediaries"""

    def __init__(self):
        self.intermediary_types = {
            'commercial_banks': {
                'primary_function': 'Deposit taking and lending',
                'services': ['Deposits', 'Loans', 'Payments', 'Credit cards'],
                'regulation': 'Heavy',
                'funding_source': 'Deposits'
            },
            'investment_banks': {
                'primary_function': 'Capital raising and advisory',
                'services': ['Underwriting', 'M&A Advisory', 'Trading', 'Research'],
                'regulation': 'Moderate',
                'funding_source': 'Proprietary capital'
            },
            'insurance_companies': {
                'primary_function': 'Risk transfer and pooling',
                'services': ['Life insurance', 'Property insurance', 'Annuities'],
                'regulation': 'Heavy',
                'funding_source': 'Premiums'
            },
            'mutual_funds': {
                'primary_function': 'Pooled investment management',
                'services': ['Diversification', 'Professional management', 'Liquidity'],
                'regulation': 'Moderate',
                'funding_source': 'Investor contributions'
            },
            'pension_funds': {
                'primary_function': 'Retirement savings management',
                'services': ['Long-term investing', 'Retirement planning'],
                'regulation': 'Heavy',
                'funding_source': 'Employee/employer contributions'
            }
        }

    def analyze_financial_system_functions(self) -> Dict[str, Any]:
        """Analyze main functions of the financial system"""

        return {
            'primary_functions': {
                'capital_allocation': {
                    'description': 'Direct savings to most productive uses',
                    'mechanisms': ['Primary markets', 'Secondary markets', 'Financial intermediaries'],
                    'importance': 'Economic growth and efficiency'
                },
                'risk_management': {
                    'description': 'Transfer and pool risks',
                    'mechanisms': ['Insurance', 'Derivatives', 'Diversification'],
                    'importance': 'Economic stability and confidence'
                },
                'liquidity_provision': {
                    'description': 'Convert illiquid assets to liquid form',
                    'mechanisms': ['Secondary markets', 'Financial intermediaries'],
                    'importance': 'Investment flexibility and efficiency'
                },
                'payment_system': {
                    'description': 'Facilitate exchange of goods and services',
                    'mechanisms': ['Banks', 'Payment processors', 'Central banks'],
                    'importance': 'Economic transactions and commerce'
                },
                'information_services': {
                    'description': 'Gather and disseminate financial information',
                    'mechanisms': ['Research', 'Rating agencies', 'Financial reporting'],
                    'importance': 'Informed decision making'
                }
            },
            'system_characteristics': {
                'well_functioning_indicators': [
                    'Complete markets',
                    'Liquidity',
                    'Transparency',
                    'Low transaction costs',
                    'Regulatory framework'
                ]
            }
        }

    def analyze_intermediary_services(self, intermediary_type: str) -> Dict[str, Any]:
        """Analyze services provided by specific intermediary type"""

        if intermediary_type not in self.intermediary_types:
            return {'error': f'Unknown intermediary type: {intermediary_type}'}

        intermediary_info = self.intermediary_types[intermediary_type]

        return {
            'intermediary_type': intermediary_type,
            'characteristics': intermediary_info,
            'value_proposition': self.determine_value_proposition(intermediary_type),
            'regulatory_considerations': self.get_regulatory_considerations(intermediary_type)
        }

    def determine_value_proposition(self, intermediary_type: str) -> List[str]:
        """Determine value proposition for intermediary type"""

        value_props = {
            'commercial_banks': [
                'Maturity transformation',
                'Risk pooling',
                'Transaction cost reduction',
                'Payment system access'
            ],
            'investment_banks': [
                'Capital raising expertise',
                'Market making',
                'Information production',
                'Risk management'
            ],
            'insurance_companies': [
                'Risk pooling',
                'Risk transfer',
                'Long-term savings',
                'Actuarial expertise'
            ],
            'mutual_funds': [
                'Professional management',
                'Diversification',
                'Economies of scale',
                'Liquidity'
            ],
            'pension_funds': [
                'Long-term investing',
                'Tax advantages',
                'Professional management',
                'Retirement planning'
            ]
        }

        return value_props.get(intermediary_type, [])

    def get_regulatory_considerations(self, intermediary_type: str) -> List[str]:
        """Get regulatory considerations for intermediary type"""

        regulations = {
            'commercial_banks': [
                'Capital requirements',
                'Deposit insurance',
                'Reserve requirements',
                'Lending restrictions'
            ],
            'investment_banks': [
                'Securities regulations',
                'Capital requirements',
                'Conduct rules',
                'Systemic risk oversight'
            ],
            'insurance_companies': [
                'Solvency requirements',
                'Reserve requirements',
                'Product regulations',
                'Consumer protection'
            ],
            'mutual_funds': [
                'Investment restrictions',
                'Disclosure requirements',
                'Valuation rules',
                'Investor protection'
            ],
            'pension_funds': [
                'Fiduciary duties',
                'Investment restrictions',
                'Funding requirements',
                'ERISA compliance'
            ]
        }

        return regulations.get(intermediary_type, [])


# Convenience functions for quick analysis
def quick_margin_analysis(position_value: float, initial_margin: float = 0.5) -> Dict[str, float]:
    """Quick margin requirement analysis"""
    calculator = MarginTradingCalculator()
    return calculator.calculate_margin_requirements(position_value, initial_margin)


def calculate_margin_call_price(initial_price: float, loan_amount: float,
                                shares: int, maintenance_margin: float = 0.25) -> float:
    """Quick margin call price calculation"""
    calculator = MarginTradingCalculator()
    return calculator.calculate_margin_call_price(initial_price, 0, loan_amount, shares, maintenance_margin)


def analyze_order_execution(order_type: str, limit_price: float = None,
                            current_bid: float = 100, current_ask: float = 101) -> Dict[str, Any]:
    """Quick order analysis"""
    analyzer = OrderAnalyzer()

    # Create mock order
    order = TradingOrder(
        order_type=OrderType(order_type.lower()),
        order_validity=OrderValidity.DAY_ORDER,
        security_symbol="MOCK",
        quantity=100,
        price=limit_price,
        stop_price=None,
        position_type=PositionType.LONG
    )

    if order_type.lower() == 'market_order':
        return analyzer.analyze_market_order(order, current_bid, current_ask)
    elif order_type.lower() == 'limit_order':
        return analyzer.analyze_limit_order(order, (current_bid + current_ask) / 2)
    else:
        return {'error': 'Unsupported order type for quick analysis'}


def classify_security(security_type: str, **characteristics) -> Dict[str, Any]:
    """Quick security classification"""
    classifier = SecurityClassifier()

    if security_type.lower() in ['stock', 'equity']:
        return classifier.classify_equity_security(characteristics)
    elif security_type.lower() in ['bond', 'debt']:
        return classifier.classify_debt_security(characteristics)
    else:
        return {'error': 'Unsupported security type'}