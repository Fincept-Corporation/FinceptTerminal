"""
Derivatives Arbitrage Detection Module
====================================

Comprehensive arbitrage detection and synthetic instrument construction framework for derivatives markets. Implements various arbitrage strategies including put-call parity violations, carry arbitrage, box spreads, and volatility arbitrage opportunities.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Real-time market prices for options and underlying assets
  - Option chain data (calls, puts across strikes and expirations)
  - Forward and futures prices for carry arbitrage analysis
  - Interest rate curves and dividend yield data
  - Implied volatility surfaces and historical volatility data
  - Market bid-ask spreads and liquidity metrics

OUTPUT:
  - Arbitrage opportunity detection across multiple strategies
  - Synthetic instrument construction and replication analysis
  - Profit potential calculations and confidence assessments
  - Execution plans and risk factor identification
  - Arbitrage opportunity ranking and filtering
  - Trade execution timing and monitoring requirements

PARAMETERS:
  - confidence_threshold: Minimum confidence level for opportunities - default: 0.05
  - tolerance: Price tolerance for arbitrage detection - default: Constants.EPSILON
  - min_profit: Minimum profit threshold for opportunities - default: 0.0
  - min_confidence: Minimum confidence level - default: 0.0
  - max_complexity: Maximum execution complexity - default: "high"
  - allowed_types: Specific arbitrage types to scan for
  - market_vol: Historical volatility for volatility arbitrage
  - implied_vol: Implied volatility for comparison
"""

import numpy as np
from typing import Dict, List, Tuple, Optional, NamedTuple
from datetime import datetime
from dataclasses import dataclass
from enum import Enum
import logging

from .core import (
    DerivativeType, OptionType, UnderlyingType, MarketData,
    PricingResult, ValidationError, ModelValidator, Constants
)
from .forward_commitments import CarryArbitrageCalculator
from .options import VanillaOption, PutCallParity, BlackScholesPricingEngine

logger = logging.getLogger(__name__)


class ArbitrageType(Enum):
    """Types of arbitrage opportunities"""
    CARRY_ARBITRAGE = "carry_arbitrage"
    PUT_CALL_PARITY = "put_call_parity"
    FORWARD_PARITY = "forward_parity"
    BOX_SPREAD = "box_spread"
    CONVERSION = "conversion"
    REVERSAL = "reversal"
    CALENDAR_SPREAD = "calendar_spread"
    VOLATILITY_ARBITRAGE = "volatility_arbitrage"


class ArbitrageDirection(Enum):
    """Direction of arbitrage trade"""
    BUY_CHEAP_SELL_EXPENSIVE = "buy_cheap_sell_expensive"
    SELL_EXPENSIVE_BUY_CHEAP = "sell_expensive_buy_cheap"


@dataclass
class ArbitrageOpportunity:
    """Container for arbitrage opportunity details"""
    arbitrage_type: ArbitrageType
    direction: ArbitrageDirection
    profit_potential: float
    confidence_level: float
    instruments_involved: List[str]
    trade_details: Dict
    risk_factors: List[str]
    execution_complexity: str  # "low", "medium", "high"

    def __post_init__(self):
        if self.profit_potential < 0:
            raise ValidationError("Profit potential cannot be negative")
        if not (0 <= self.confidence_level <= 1):
            raise ValidationError("Confidence level must be between 0 and 1")


@dataclass
class SyntheticInstrument:
    """Synthetic instrument construction details"""
    target_instrument: str
    synthetic_components: List[Dict]
    cost_comparison: float
    replication_accuracy: float

    def __post_init__(self):
        if not (0 <= self.replication_accuracy <= 1):
            raise ValidationError("Replication accuracy must be between 0 and 1")


class ConversionStrategy:
    """Conversion arbitrage strategy implementation"""

    def __init__(self, spot_price: float, strike_price: float,
                 call_price: float, put_price: float,
                 risk_free_rate: float, time_to_expiry: float,
                 dividend_yield: float = 0.0):

        self.spot_price = spot_price
        self.strike_price = strike_price
        self.call_price = call_price
        self.put_price = put_price
        self.risk_free_rate = risk_free_rate
        self.time_to_expiry = time_to_expiry
        self.dividend_yield = dividend_yield

        ModelValidator.validate_positive(spot_price, "spot_price")
        ModelValidator.validate_positive(strike_price, "strike_price")
        ModelValidator.validate_positive(call_price, "call_price")
        ModelValidator.validate_positive(put_price, "put_price")

    def detect_arbitrage(self) -> Optional[ArbitrageOpportunity]:
        """Detect conversion arbitrage opportunity"""
        # Calculate synthetic call using put-call parity
        pv_strike = self.strike_price * np.exp(-self.risk_free_rate * self.time_to_expiry)
        pv_spot = self.spot_price * np.exp(-self.dividend_yield * self.time_to_expiry)

        synthetic_call = self.put_price + pv_spot - pv_strike

        # Check for arbitrage
        price_difference = self.call_price - synthetic_call

        if abs(price_difference) > Constants.EPSILON:
            if price_difference > 0:
                # Call is overpriced - sell call, buy synthetic call
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.CONVERSION,
                    direction=ArbitrageDirection.SELL_EXPENSIVE_BUY_CHEAP,
                    profit_potential=abs(price_difference),
                    confidence_level=0.95,
                    instruments_involved=["call", "put", "stock", "bond"],
                    trade_details={
                        "sell_call": self.call_price,
                        "buy_put": self.put_price,
                        "buy_stock": self.spot_price,
                        "sell_bond": pv_strike,
                        "net_profit": price_difference,
                        "synthetic_call_price": synthetic_call,
                        "actual_call_price": self.call_price
                    },
                    risk_factors=["early_exercise", "dividend_risk", "interest_rate_risk"],
                    execution_complexity="medium"
                )
            else:
                # Call is underpriced - buy call, sell synthetic call
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.REVERSAL,
                    direction=ArbitrageDirection.BUY_CHEAP_SELL_EXPENSIVE,
                    profit_potential=abs(price_difference),
                    confidence_level=0.95,
                    instruments_involved=["call", "put", "stock", "bond"],
                    trade_details={
                        "buy_call": self.call_price,
                        "sell_put": self.put_price,
                        "sell_stock": self.spot_price,
                        "buy_bond": pv_strike,
                        "net_profit": abs(price_difference),
                        "synthetic_call_price": synthetic_call,
                        "actual_call_price": self.call_price
                    },
                    risk_factors=["early_exercise", "dividend_risk", "interest_rate_risk"],
                    execution_complexity="medium"
                )

        return None


class ReversalStrategy:
    """Reversal arbitrage strategy implementation"""

    def __init__(self, spot_price: float, strike_price: float,
                 call_price: float, put_price: float,
                 risk_free_rate: float, time_to_expiry: float,
                 dividend_yield: float = 0.0):

        self.spot_price = spot_price
        self.strike_price = strike_price
        self.call_price = call_price
        self.put_price = put_price
        self.risk_free_rate = risk_free_rate
        self.time_to_expiry = time_to_expiry
        self.dividend_yield = dividend_yield

        ModelValidator.validate_positive(spot_price, "spot_price")
        ModelValidator.validate_positive(strike_price, "strike_price")
        ModelValidator.validate_positive(call_price, "call_price")
        ModelValidator.validate_positive(put_price, "put_price")

    def detect_arbitrage(self) -> Optional[ArbitrageOpportunity]:
        """Detect reversal arbitrage opportunity"""
        # Calculate synthetic put using put-call parity
        pv_strike = self.strike_price * np.exp(-self.risk_free_rate * self.time_to_expiry)
        pv_spot = self.spot_price * np.exp(-self.dividend_yield * self.time_to_expiry)

        synthetic_put = self.call_price + pv_strike - pv_spot

        # Check for arbitrage
        price_difference = self.put_price - synthetic_put

        if abs(price_difference) > Constants.EPSILON:
            if price_difference > 0:
                # Put is overpriced - sell put, buy synthetic put
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.REVERSAL,
                    direction=ArbitrageDirection.SELL_EXPENSIVE_BUY_CHEAP,
                    profit_potential=abs(price_difference),
                    confidence_level=0.95,
                    instruments_involved=["call", "put", "stock", "bond"],
                    trade_details={
                        "buy_call": self.call_price,
                        "sell_put": self.put_price,
                        "sell_stock": self.spot_price,
                        "buy_bond": pv_strike,
                        "net_profit": price_difference,
                        "synthetic_put_price": synthetic_put,
                        "actual_put_price": self.put_price
                    },
                    risk_factors=["early_exercise", "dividend_risk", "interest_rate_risk"],
                    execution_complexity="medium"
                )
            else:
                # Put is underpriced - buy put, sell synthetic put
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.CONVERSION,
                    direction=ArbitrageDirection.BUY_CHEAP_SELL_EXPENSIVE,
                    profit_potential=abs(price_difference),
                    confidence_level=0.95,
                    instruments_involved=["call", "put", "stock", "bond"],
                    trade_details={
                        "sell_call": self.call_price,
                        "buy_put": self.put_price,
                        "buy_stock": self.spot_price,
                        "sell_bond": pv_strike,
                        "net_profit": abs(price_difference),
                        "synthetic_put_price": synthetic_put,
                        "actual_put_price": self.put_price
                    },
                    risk_factors=["early_exercise", "dividend_risk", "interest_rate_risk"],
                    execution_complexity="medium"
                )

        return None


class BoxSpreadStrategy:
    """Box spread arbitrage strategy"""

    def __init__(self, strike_low: float, strike_high: float,
                 call_low_price: float, call_high_price: float,
                 put_low_price: float, put_high_price: float,
                 risk_free_rate: float, time_to_expiry: float):

        self.K1 = strike_low  # Lower strike
        self.K2 = strike_high  # Higher strike
        self.C1 = call_low_price  # Call with lower strike
        self.C2 = call_high_price  # Call with higher strike
        self.P1 = put_low_price  # Put with lower strike
        self.P2 = put_high_price  # Put with higher strike
        self.r = risk_free_rate
        self.T = time_to_expiry

        if strike_low >= strike_high:
            raise ValidationError("Lower strike must be less than higher strike")

    def detect_arbitrage(self) -> Optional[ArbitrageOpportunity]:
        """Detect box spread arbitrage"""
        # Box spread payoff is always (K2 - K1)
        guaranteed_payoff = self.K2 - self.K1
        present_value_payoff = guaranteed_payoff * np.exp(-self.r * self.T)

        # Cost of box spread = (C1 - C2) + (P2 - P1)
        box_cost = (self.C1 - self.C2) + (self.P2 - self.P1)

        # Arbitrage profit
        arbitrage_profit = present_value_payoff - box_cost

        if abs(arbitrage_profit) > Constants.EPSILON:
            if arbitrage_profit > 0:
                # Box is underpriced - buy box spread
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.BOX_SPREAD,
                    direction=ArbitrageDirection.BUY_CHEAP_SELL_EXPENSIVE,
                    profit_potential=arbitrage_profit,
                    confidence_level=0.99,  # Risk-free arbitrage
                    instruments_involved=[f"call_{self.K1}", f"call_{self.K2}",
                                          f"put_{self.K1}", f"put_{self.K2}"],
                    trade_details={
                        "buy_call_low": self.C1,
                        "sell_call_high": self.C2,
                        "sell_put_low": self.P1,
                        "buy_put_high": self.P2,
                        "box_cost": box_cost,
                        "guaranteed_payoff": guaranteed_payoff,
                        "present_value": present_value_payoff,
                        "arbitrage_profit": arbitrage_profit,
                        "call_spread_cost": self.C1 - self.C2,
                        "put_spread_cost": self.P2 - self.P1
                    },
                    risk_factors=["execution_risk", "bid_ask_spread"],
                    execution_complexity="high"
                )
            else:
                # Box is overpriced - sell box spread
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.BOX_SPREAD,
                    direction=ArbitrageDirection.SELL_EXPENSIVE_BUY_CHEAP,
                    profit_potential=abs(arbitrage_profit),
                    confidence_level=0.99,
                    instruments_involved=[f"call_{self.K1}", f"call_{self.K2}",
                                          f"put_{self.K1}", f"put_{self.K2}"],
                    trade_details={
                        "sell_call_low": self.C1,
                        "buy_call_high": self.C2,
                        "buy_put_low": self.P1,
                        "sell_put_high": self.P2,
                        "box_revenue": -box_cost,
                        "guaranteed_payout": -guaranteed_payoff,
                        "arbitrage_profit": abs(arbitrage_profit),
                        "call_spread_revenue": -(self.C1 - self.C2),
                        "put_spread_revenue": -(self.P2 - self.P1)
                    },
                    risk_factors=["execution_risk", "bid_ask_spread"],
                    execution_complexity="high"
                )

        return None


class CarryArbitrageDetector:
    """Detect carry arbitrage opportunities in forwards/futures"""

    def __init__(self, spot_price: float, forward_price: float,
                 risk_free_rate: float, time_to_expiry: float,
                 dividend_yield: float = 0.0, storage_cost: float = 0.0,
                 convenience_yield: float = 0.0):

        self.spot_price = spot_price
        self.forward_price = forward_price
        self.risk_free_rate = risk_free_rate
        self.time_to_expiry = time_to_expiry
        self.dividend_yield = dividend_yield
        self.storage_cost = storage_cost
        self.convenience_yield = convenience_yield

        ModelValidator.validate_positive(spot_price, "spot_price")
        ModelValidator.validate_positive(forward_price, "forward_price")

    def detect_arbitrage(self) -> Optional[ArbitrageOpportunity]:
        """Detect carry arbitrage opportunity"""
        # Calculate theoretical forward price
        carry_rate = (self.risk_free_rate - self.dividend_yield +
                      self.storage_cost - self.convenience_yield)

        theoretical_forward = self.spot_price * np.exp(carry_rate * self.time_to_expiry)

        # Check for arbitrage
        price_difference = self.forward_price - theoretical_forward

        if abs(price_difference) > Constants.EPSILON:
            # Present value of arbitrage profit
            arbitrage_profit = abs(price_difference) * np.exp(-self.risk_free_rate * self.time_to_expiry)

            if price_difference > 0:
                # Forward is overpriced - sell forward, buy underlying
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.CARRY_ARBITRAGE,
                    direction=ArbitrageDirection.SELL_EXPENSIVE_BUY_CHEAP,
                    profit_potential=arbitrage_profit,
                    confidence_level=0.90,
                    instruments_involved=["forward", "underlying", "bond"],
                    trade_details={
                        "sell_forward": self.forward_price,
                        "buy_underlying": self.spot_price,
                        "borrow_funds": self.spot_price,
                        "theoretical_forward": theoretical_forward,
                        "price_difference": price_difference,
                        "carry_rate": carry_rate,
                        "dividend_yield": self.dividend_yield,
                        "storage_cost": self.storage_cost,
                        "convenience_yield": self.convenience_yield
                    },
                    risk_factors=["storage_costs", "convenience_yield", "dividend_changes", "interest_rate_risk"],
                    execution_complexity="medium"
                )
            else:
                # Forward is underpriced - buy forward, sell underlying
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.CARRY_ARBITRAGE,
                    direction=ArbitrageDirection.BUY_CHEAP_SELL_EXPENSIVE,
                    profit_potential=arbitrage_profit,
                    confidence_level=0.90,
                    instruments_involved=["forward", "underlying", "bond"],
                    trade_details={
                        "buy_forward": self.forward_price,
                        "sell_underlying": self.spot_price,
                        "invest_proceeds": self.spot_price,
                        "theoretical_forward": theoretical_forward,
                        "price_difference": price_difference,
                        "carry_rate": carry_rate,
                        "dividend_yield": self.dividend_yield,
                        "storage_cost": self.storage_cost,
                        "convenience_yield": self.convenience_yield
                    },
                    risk_factors=["storage_costs", "convenience_yield", "dividend_changes", "interest_rate_risk"],
                    execution_complexity="medium"
                )

        return None


class VolatilityArbitrageDetector:
    """Detect volatility arbitrage opportunities"""

    def __init__(self, market_vol: float, implied_vol: float,
                 option: VanillaOption, market_data: MarketData,
                 confidence_threshold: float = 0.05):

        self.market_vol = market_vol
        self.implied_vol = implied_vol
        self.option = option
        self.market_data = market_data
        self.confidence_threshold = confidence_threshold

        ModelValidator.validate_volatility(market_vol)
        ModelValidator.validate_volatility(implied_vol)

    def detect_arbitrage(self) -> Optional[ArbitrageOpportunity]:
        """Detect volatility arbitrage based on vol differential"""
        vol_difference = self.implied_vol - self.market_vol
        vol_spread_pct = abs(vol_difference) / self.market_vol

        if vol_spread_pct > self.confidence_threshold:
            # Calculate option values at different volatilities
            market_data_market_vol = MarketData(
                spot_price=self.market_data.spot_price,
                risk_free_rate=self.market_data.risk_free_rate,
                dividend_yield=self.market_data.dividend_yield,
                volatility=self.market_vol,
                time_to_expiry=self.market_data.time_to_expiry
            )

            engine = BlackScholesPricingEngine()
            market_vol_price = engine.price(self.option, market_data_market_vol).fair_value

            market_data_implied_vol = MarketData(
                spot_price=self.market_data.spot_price,
                risk_free_rate=self.market_data.risk_free_rate,
                dividend_yield=self.market_data.dividend_yield,
                volatility=self.implied_vol,
                time_to_expiry=self.market_data.time_to_expiry
            )

            implied_vol_price = engine.price(self.option, market_data_implied_vol).fair_value

            price_difference = implied_vol_price - market_vol_price

            if vol_difference > 0:
                # Implied vol > realized vol - sell option, delta hedge
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.VOLATILITY_ARBITRAGE,
                    direction=ArbitrageDirection.SELL_EXPENSIVE_BUY_CHEAP,
                    profit_potential=abs(price_difference),
                    confidence_level=min(0.95, vol_spread_pct * 5),  # Higher spread = higher confidence
                    instruments_involved=["option", "underlying"],
                    trade_details={
                        "sell_option": implied_vol_price,
                        "market_vol": self.market_vol,
                        "implied_vol": self.implied_vol,
                        "vol_difference": vol_difference,
                        "price_difference": price_difference,
                        "strategy": "sell_option_delta_hedge",
                        "vol_spread_pct": vol_spread_pct,
                        "market_vol_price": market_vol_price,
                        "implied_vol_price": implied_vol_price
                    },
                    risk_factors=["gamma_risk", "vol_risk", "time_decay", "model_risk"],
                    execution_complexity="high"
                )
            else:
                # Implied vol < realized vol - buy option, delta hedge
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.VOLATILITY_ARBITRAGE,
                    direction=ArbitrageDirection.BUY_CHEAP_SELL_EXPENSIVE,
                    profit_potential=abs(price_difference),
                    confidence_level=min(0.95, vol_spread_pct * 5),
                    instruments_involved=["option", "underlying"],
                    trade_details={
                        "buy_option": implied_vol_price,
                        "market_vol": self.market_vol,
                        "implied_vol": self.implied_vol,
                        "vol_difference": vol_difference,
                        "price_difference": price_difference,
                        "strategy": "buy_option_delta_hedge",
                        "vol_spread_pct": vol_spread_pct,
                        "market_vol_price": market_vol_price,
                        "implied_vol_price": implied_vol_price
                    },
                    risk_factors=["gamma_risk", "vol_risk", "time_decay", "model_risk"],
                    execution_complexity="high"
                )

        return None


class CalendarSpreadArbitrage:
    """Calendar spread arbitrage detector"""

    def __init__(self, near_option_price: float, far_option_price: float,
                 near_time_to_expiry: float, far_time_to_expiry: float,
                 strike_price: float, option_type: OptionType,
                 market_data: MarketData):

        self.near_option_price = near_option_price
        self.far_option_price = far_option_price
        self.near_time = near_time_to_expiry
        self.far_time = far_time_to_expiry
        self.strike_price = strike_price
        self.option_type = option_type
        self.market_data = market_data

        if near_time_to_expiry >= far_time_to_expiry:
            raise ValidationError("Near expiry must be less than far expiry")

    def detect_arbitrage(self) -> Optional[ArbitrageOpportunity]:
        """Detect calendar spread arbitrage"""
        # Calculate theoretical time decay value
        time_decay_value = self.far_option_price - self.near_option_price

        # Calculate theoretical time decay using Black-Scholes
        engine = BlackScholesPricingEngine()

        near_option = VanillaOption(
            option_type=self.option_type,
            underlying_type=UnderlyingType.EQUITY,
            expiry_date=datetime.now(),
            strike_price=self.strike_price
        )

        far_option = VanillaOption(
            option_type=self.option_type,
            underlying_type=UnderlyingType.EQUITY,
            expiry_date=datetime.now(),
            strike_price=self.strike_price
        )

        # Create market data for different times
        near_market_data = MarketData(
            spot_price=self.market_data.spot_price,
            risk_free_rate=self.market_data.risk_free_rate,
            dividend_yield=self.market_data.dividend_yield,
            volatility=self.market_data.volatility,
            time_to_expiry=self.near_time
        )

        far_market_data = MarketData(
            spot_price=self.market_data.spot_price,
            risk_free_rate=self.market_data.risk_free_rate,
            dividend_yield=self.market_data.dividend_yield,
            volatility=self.market_data.volatility,
            time_to_expiry=self.far_time
        )

        theoretical_near_price = engine.price(near_option, near_market_data).fair_value
        theoretical_far_price = engine.price(far_option, far_market_data).fair_value
        theoretical_time_decay = theoretical_far_price - theoretical_near_price

        # Check for arbitrage
        price_difference = time_decay_value - theoretical_time_decay

        if abs(price_difference) > Constants.EPSILON:
            if price_difference > 0:
                # Calendar spread is overpriced - sell calendar spread
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.CALENDAR_SPREAD,
                    direction=ArbitrageDirection.SELL_EXPENSIVE_BUY_CHEAP,
                    profit_potential=abs(price_difference),
                    confidence_level=0.80,
                    instruments_involved=[f"{self.option_type.value}_near", f"{self.option_type.value}_far"],
                    trade_details={
                        "sell_far_option": self.far_option_price,
                        "buy_near_option": self.near_option_price,
                        "calendar_spread_cost": time_decay_value,
                        "theoretical_time_decay": theoretical_time_decay,
                        "arbitrage_profit": price_difference,
                        "near_time": self.near_time,
                        "far_time": self.far_time
                    },
                    risk_factors=["volatility_risk", "time_decay", "pin_risk"],
                    execution_complexity="medium"
                )
            else:
                # Calendar spread is underpriced - buy calendar spread
                return ArbitrageOpportunity(
                    arbitrage_type=ArbitrageType.CALENDAR_SPREAD,
                    direction=ArbitrageDirection.BUY_CHEAP_SELL_EXPENSIVE,
                    profit_potential=abs(price_difference),
                    confidence_level=0.80,
                    instruments_involved=[f"{self.option_type.value}_near", f"{self.option_type.value}_far"],
                    trade_details={
                        "buy_far_option": self.far_option_price,
                        "sell_near_option": self.near_option_price,
                        "calendar_spread_cost": time_decay_value,
                        "theoretical_time_decay": theoretical_time_decay,
                        "arbitrage_profit": abs(price_difference),
                        "near_time": self.near_time,
                        "far_time": self.far_time
                    },
                    risk_factors=["volatility_risk", "time_decay", "pin_risk"],
                    execution_complexity="medium"
                )

        return None


class SyntheticInstrumentBuilder:
    """Build synthetic instruments using replication strategies"""

    @staticmethod
    def synthetic_call(put_price: float, spot_price: float, strike_price: float,
                       risk_free_rate: float, time_to_expiry: float,
                       dividend_yield: float = 0.0) -> SyntheticInstrument:
        """Create synthetic call using put-call parity: C = P + S - K*e^(-rT)"""
        pv_strike = strike_price * np.exp(-risk_free_rate * time_to_expiry)
        pv_spot = spot_price * np.exp(-dividend_yield * time_to_expiry)

        synthetic_call_price = put_price + pv_spot - pv_strike

        return SyntheticInstrument(
            target_instrument="call",
            synthetic_components=[
                {"instrument": "put", "position": "long", "price": put_price, "quantity": 1},
                {"instrument": "stock", "position": "long", "price": spot_price, "quantity": 1},
                {"instrument": "bond", "position": "short", "price": pv_strike, "quantity": 1}
            ],
            cost_comparison=synthetic_call_price,
            replication_accuracy=0.99
        )

    @staticmethod
    def synthetic_put(call_price: float, spot_price: float, strike_price: float,
                      risk_free_rate: float, time_to_expiry: float,
                      dividend_yield: float = 0.0) -> SyntheticInstrument:
        """Create synthetic put using put-call parity: P = C + K*e^(-rT) - S"""
        pv_strike = strike_price * np.exp(-risk_free_rate * time_to_expiry)
        pv_spot = spot_price * np.exp(-dividend_yield * time_to_expiry)

        synthetic_put_price = call_price + pv_strike - pv_spot

        return SyntheticInstrument(
            target_instrument="put",
            synthetic_components=[
                {"instrument": "call", "position": "long", "price": call_price, "quantity": 1},
                {"instrument": "bond", "position": "long", "price": pv_strike, "quantity": 1},
                {"instrument": "stock", "position": "short", "price": spot_price, "quantity": 1}
            ],
            cost_comparison=synthetic_put_price,
            replication_accuracy=0.99
        )

    @staticmethod
    def synthetic_stock(call_price: float, put_price: float, strike_price: float,
                        risk_free_rate: float, time_to_expiry: float,
                        dividend_yield: float = 0.0) -> SyntheticInstrument:
        """Create synthetic stock using options: S = C - P + K*e^(-rT)"""
        pv_strike = strike_price * np.exp(-risk_free_rate * time_to_expiry)

        synthetic_stock_price = call_price - put_price + pv_strike

        return SyntheticInstrument(
            target_instrument="stock",
            synthetic_components=[
                {"instrument": "call", "position": "long", "price": call_price, "quantity": 1},
                {"instrument": "put", "position": "short", "price": put_price, "quantity": 1},
                {"instrument": "bond", "position": "long", "price": pv_strike, "quantity": 1}
            ],
            cost_comparison=synthetic_stock_price,
            replication_accuracy=0.95
        )

    @staticmethod
    def synthetic_forward(spot_price: float, risk_free_rate: float,
                          time_to_expiry: float, dividend_yield: float = 0.0) -> SyntheticInstrument:
        """Create synthetic forward position: F = S*e^((r-q)*T)"""
        pv_dividends = spot_price * (1 - np.exp(-dividend_yield * time_to_expiry))
        borrowing_cost = (spot_price - pv_dividends) * (np.exp(risk_free_rate * time_to_expiry) - 1)

        forward_price = spot_price * np.exp((risk_free_rate - dividend_yield) * time_to_expiry)

        return SyntheticInstrument(
            target_instrument="forward",
            synthetic_components=[
                {"instrument": "stock", "position": "long", "price": spot_price, "quantity": 1},
                {"instrument": "bond", "position": "short", "price": spot_price - pv_dividends, "quantity": 1}
            ],
            cost_comparison=forward_price,
            replication_accuracy=0.99
        )

    @staticmethod
    def synthetic_bond(strike_price: float, call_price: float, put_price: float,
                       spot_price: float, dividend_yield: float = 0.0) -> SyntheticInstrument:
        """Create synthetic bond using put-call parity: Bond = C - P + S"""
        synthetic_bond_price = strike_price
        cost_to_replicate = call_price - put_price + spot_price * np.exp(-dividend_yield)

        return SyntheticInstrument(
            target_instrument="bond",
            synthetic_components=[
                {"instrument": "call", "position": "long", "price": call_price, "quantity": 1},
                {"instrument": "put", "position": "short", "price": put_price, "quantity": 1},
                {"instrument": "stock", "position": "long", "price": spot_price, "quantity": 1}
            ],
            cost_comparison=cost_to_replicate,
            replication_accuracy=0.98
        )


class ArbitrageScanner:
    """Comprehensive arbitrage opportunity scanner"""

    def __init__(self, tolerance: float = Constants.EPSILON):
        self.tolerance = tolerance
        self.detected_opportunities = []

    def scan_put_call_parity(self, call_price: float, put_price: float,
                             spot_price: float, strike_price: float,
                             risk_free_rate: float, time_to_expiry: float,
                             dividend_yield: float = 0.0) -> List[ArbitrageOpportunity]:
        """Scan for put-call parity arbitrage"""
        opportunities = []

        # Check conversion strategy
        conversion = ConversionStrategy(
            spot_price, strike_price, call_price, put_price,
            risk_free_rate, time_to_expiry, dividend_yield
        )

        conversion_opportunity = conversion.detect_arbitrage()
        if conversion_opportunity:
            opportunities.append(conversion_opportunity)

        # Check reversal strategy
        reversal = ReversalStrategy(
            spot_price, strike_price, call_price, put_price,
            risk_free_rate, time_to_expiry, dividend_yield
        )

        reversal_opportunity = reversal.detect_arbitrage()
        if reversal_opportunity:
            opportunities.append(reversal_opportunity)

        return opportunities

    def scan_carry_arbitrage(self, spot_price: float, forward_price: float,
                             risk_free_rate: float, time_to_expiry: float,
                             **kwargs) -> List[ArbitrageOpportunity]:
        """Scan for carry arbitrage opportunities"""
        opportunities = []

        detector = CarryArbitrageDetector(
            spot_price, forward_price, risk_free_rate, time_to_expiry, **kwargs
        )

        opportunity = detector.detect_arbitrage()
        if opportunity:
            opportunities.append(opportunity)

        return opportunities

    def scan_box_spread(self, strikes: Tuple[float, float],
                        call_prices: Tuple[float, float],
                        put_prices: Tuple[float, float],
                        risk_free_rate: float, time_to_expiry: float) -> List[ArbitrageOpportunity]:
        """Scan for box spread arbitrage"""
        opportunities = []

        box_spread = BoxSpreadStrategy(
            strikes[0], strikes[1], call_prices[0], call_prices[1],
            put_prices[0], put_prices[1], risk_free_rate, time_to_expiry
        )

        opportunity = box_spread.detect_arbitrage()
        if opportunity:
            opportunities.append(opportunity)

        return opportunities

    def scan_volatility_arbitrage(self, market_vol: float, implied_vol: float,
                                  option: VanillaOption, market_data: MarketData,
                                  confidence_threshold: float = 0.05) -> List[ArbitrageOpportunity]:
        """Scan for volatility arbitrage opportunities"""
        opportunities = []

        detector = VolatilityArbitrageDetector(
            market_vol, implied_vol, option, market_data, confidence_threshold
        )

        opportunity = detector.detect_arbitrage()
        if opportunity:
            opportunities.append(opportunity)

        return opportunities

    def scan_calendar_spread(self, near_option_price: float, far_option_price: float,
                             near_time: float, far_time: float, strike_price: float,
                             option_type: OptionType, market_data: MarketData) -> List[ArbitrageOpportunity]:
        """Scan for calendar spread arbitrage"""
        opportunities = []

        detector = CalendarSpreadArbitrage(
            near_option_price, far_option_price, near_time, far_time,
            strike_price, option_type, market_data
        )

        opportunity = detector.detect_arbitrage()
        if opportunity:
            opportunities.append(opportunity)

        return opportunities

    def comprehensive_scan(self, market_data: Dict) -> List[ArbitrageOpportunity]:
        """Perform comprehensive arbitrage scan"""
        all_opportunities = []

        try:
            # Put-call parity scan
            if all(key in market_data for key in ['call_price', 'put_price', 'spot_price', 'strike_price']):
                pcp_opportunities = self.scan_put_call_parity(
                    market_data['call_price'], market_data['put_price'],
                    market_data['spot_price'], market_data['strike_price'],
                    market_data.get('risk_free_rate', 0.02),
                    market_data.get('time_to_expiry', 0.25),
                    market_data.get('dividend_yield', 0.0)
                )
                all_opportunities.extend(pcp_opportunities)

            # Carry arbitrage scan
            if all(key in market_data for key in ['spot_price', 'forward_price']):
                carry_opportunities = self.scan_carry_arbitrage(
                    market_data['spot_price'], market_data['forward_price'],
                    market_data.get('risk_free_rate', 0.02),
                    market_data.get('time_to_expiry', 0.25),
                    dividend_yield=market_data.get('dividend_yield', 0.0),
                    storage_cost=market_data.get('storage_cost', 0.0),
                    convenience_yield=market_data.get('convenience_yield', 0.0)
                )
                all_opportunities.extend(carry_opportunities)

            # Box spread scan
            if all(key in market_data for key in ['strikes', 'call_prices', 'put_prices']):
                box_opportunities = self.scan_box_spread(
                    market_data['strikes'], market_data['call_prices'],
                    market_data['put_prices'],
                    market_data.get('risk_free_rate', 0.02),
                    market_data.get('time_to_expiry', 0.25)
                )
                all_opportunities.extend(box_opportunities)

            # Volatility arbitrage scan
            if all(key in market_data for key in ['market_vol', 'implied_vol', 'option']):
                vol_opportunities = self.scan_volatility_arbitrage(
                    market_data['market_vol'], market_data['implied_vol'],
                    market_data['option'], market_data.get('market_data_obj'),
                    market_data.get('confidence_threshold', 0.05)
                )
                all_opportunities.extend(vol_opportunities)

            # Calendar spread scan
            if all(key in market_data for key in ['near_option_price', 'far_option_price', 'near_time', 'far_time']):
                calendar_opportunities = self.scan_calendar_spread(
                    market_data['near_option_price'], market_data['far_option_price'],
                    market_data['near_time'], market_data['far_time'],
                    market_data.get('strike_price', 100),
                    market_data.get('option_type', OptionType.CALL),
                    market_data.get('market_data_obj')
                )
                all_opportunities.extend(calendar_opportunities)

        except Exception as e:
            logger.error(f"Error in arbitrage scan: {e}")

        self.detected_opportunities = all_opportunities
        return all_opportunities

    def rank_opportunities(self, opportunities: List[ArbitrageOpportunity]) -> List[ArbitrageOpportunity]:
        """Rank arbitrage opportunities by attractiveness"""

        def opportunity_score(opp):
            # Score based on profit potential, confidence, and execution complexity
            complexity_weights = {"low": 1.0, "medium": 0.7, "high": 0.5}
            complexity_weight = complexity_weights.get(opp.execution_complexity, 0.5)

            # Base score: profit * confidence * complexity adjustment
            base_score = opp.profit_potential * opp.confidence_level * complexity_weight

            # Bonus for risk-free arbitrage (box spreads)
            if opp.arbitrage_type == ArbitrageType.BOX_SPREAD and opp.confidence_level >= 0.99:
                base_score *= 1.5

            # Penalty for high-risk strategies
            if len(opp.risk_factors) > 3:
                base_score *= 0.8

            return base_score

        return sorted(opportunities, key=opportunity_score, reverse=True)

    def filter_opportunities(self, opportunities: List[ArbitrageOpportunity],
                             min_profit: float = 0.0, min_confidence: float = 0.0,
                             max_complexity: str = "high",
                             allowed_types: List[ArbitrageType] = None) -> List[ArbitrageOpportunity]:
        """Filter opportunities based on criteria"""
        filtered = []

        complexity_levels = {"low": 1, "medium": 2, "high": 3}
        max_complexity_level = complexity_levels.get(max_complexity, 3)

        for opp in opportunities:
            # Check profit threshold
            if opp.profit_potential < min_profit:
                continue

            # Check confidence threshold
            if opp.confidence_level < min_confidence:
                continue

            # Check complexity
            opp_complexity_level = complexity_levels.get(opp.execution_complexity, 3)
            if opp_complexity_level > max_complexity_level:
                continue

            # Check allowed types
            if allowed_types and opp.arbitrage_type not in allowed_types:
                continue

            filtered.append(opp)

        return filtered

    def generate_execution_plan(self, opportunity: ArbitrageOpportunity) -> Dict[str, Any]:
        """Generate detailed execution plan for arbitrage opportunity"""
        execution_steps = []

        if opportunity.arbitrage_type == ArbitrageType.CONVERSION:
            execution_steps = [
                {"step": 1, "action": "Sell call option", "details": opportunity.trade_details.get("sell_call")},
                {"step": 2, "action": "Buy put option", "details": opportunity.trade_details.get("buy_put")},
                {"step": 3, "action": "Buy underlying stock", "details": opportunity.trade_details.get("buy_stock")},
                {"step": 4, "action": "Sell bonds (borrow)", "details": opportunity.trade_details.get("sell_bond")}
            ]
        elif opportunity.arbitrage_type == ArbitrageType.REVERSAL:
            execution_steps = [
                {"step": 1, "action": "Buy call option", "details": opportunity.trade_details.get("buy_call")},
                {"step": 2, "action": "Sell put option", "details": opportunity.trade_details.get("sell_put")},
                {"step": 3, "action": "Sell underlying stock", "details": opportunity.trade_details.get("sell_stock")},
                {"step": 4, "action": "Buy bonds (lend)", "details": opportunity.trade_details.get("buy_bond")}
            ]
        elif opportunity.arbitrage_type == ArbitrageType.BOX_SPREAD:
            if opportunity.direction == ArbitrageDirection.BUY_CHEAP_SELL_EXPENSIVE:
                execution_steps = [
                    {"step": 1, "action": "Buy call (low strike)",
                     "details": opportunity.trade_details.get("buy_call_low")},
                    {"step": 2, "action": "Sell call (high strike)",
                     "details": opportunity.trade_details.get("sell_call_high")},
                    {"step": 3, "action": "Sell put (low strike)",
                     "details": opportunity.trade_details.get("sell_put_low")},
                    {"step": 4, "action": "Buy put (high strike)",
                     "details": opportunity.trade_details.get("buy_put_high")}
                ]
            else:
                execution_steps = [
                    {"step": 1, "action": "Sell call (low strike)",
                     "details": opportunity.trade_details.get("sell_call_low")},
                    {"step": 2, "action": "Buy call (high strike)",
                     "details": opportunity.trade_details.get("buy_call_high")},
                    {"step": 3, "action": "Buy put (low strike)",
                     "details": opportunity.trade_details.get("buy_put_low")},
                    {"step": 4, "action": "Sell put (high strike)",
                     "details": opportunity.trade_details.get("sell_put_high")}
                ]
        elif opportunity.arbitrage_type == ArbitrageType.CARRY_ARBITRAGE:
            if opportunity.direction == ArbitrageDirection.SELL_EXPENSIVE_BUY_CHEAP:
                execution_steps = [
                    {"step": 1, "action": "Sell forward contract",
                     "details": opportunity.trade_details.get("sell_forward")},
                    {"step": 2, "action": "Buy underlying asset",
                     "details": opportunity.trade_details.get("buy_underlying")},
                    {"step": 3, "action": "Borrow funds", "details": opportunity.trade_details.get("borrow_funds")}
                ]
            else:
                execution_steps = [
                    {"step": 1, "action": "Buy forward contract",
                     "details": opportunity.trade_details.get("buy_forward")},
                    {"step": 2, "action": "Sell underlying asset",
                     "details": opportunity.trade_details.get("sell_underlying")},
                    {"step": 3, "action": "Invest proceeds",
                     "details": opportunity.trade_details.get("invest_proceeds")}
                ]

        return {
            "opportunity_id": f"{opportunity.arbitrage_type.value}_{datetime.now().strftime('%Y%m%d_%H%M%S')}",
            "execution_steps": execution_steps,
            "estimated_profit": opportunity.profit_potential,
            "confidence_level": opportunity.confidence_level,
            "risk_factors": opportunity.risk_factors,
            "execution_complexity": opportunity.execution_complexity,
            "required_capital": self._calculate_required_capital(opportunity),
            "time_to_expiration": opportunity.trade_details.get("time_to_expiry", "N/A"),
            "monitoring_requirements": self._get_monitoring_requirements(opportunity)
        }

    def _calculate_required_capital(self, opportunity: ArbitrageOpportunity) -> float:
        """Calculate required capital for arbitrage execution"""
        # Simplified calculation - in practice would be more sophisticated
        if opportunity.arbitrage_type in [ArbitrageType.CONVERSION, ArbitrageType.REVERSAL]:
            return opportunity.trade_details.get("buy_stock", 0) + opportunity.trade_details.get("buy_put", 0)
        elif opportunity.arbitrage_type == ArbitrageType.BOX_SPREAD:
            return opportunity.trade_details.get("box_cost", 0)
        elif opportunity.arbitrage_type == ArbitrageType.CARRY_ARBITRAGE:
            return opportunity.trade_details.get("buy_underlying", 0)
        else:
            return 0.0

    def _get_monitoring_requirements(self, opportunity: ArbitrageOpportunity) -> List[str]:
        """Get monitoring requirements for arbitrage position"""
        monitoring = ["market_prices", "position_delta"]

        if opportunity.arbitrage_type == ArbitrageType.VOLATILITY_ARBITRAGE:
            monitoring.extend(["realized_volatility", "implied_volatility", "gamma_exposure"])

        if opportunity.arbitrage_type in [ArbitrageType.CONVERSION, ArbitrageType.REVERSAL]:
            monitoring.extend(["dividend_announcements", "early_exercise_risk"])

        if opportunity.arbitrage_type == ArbitrageType.CARRY_ARBITRAGE:
            monitoring.extend(["interest_rates", "storage_costs", "convenience_yield"])

        if opportunity.arbitrage_type == ArbitrageType.CALENDAR_SPREAD:
            monitoring.extend(["time_decay", "pin_risk", "volatility_term_structure"])

        return monitoring


# Export main classes
__all__ = [
    'ArbitrageType', 'ArbitrageDirection', 'ArbitrageOpportunity', 'SyntheticInstrument',
    'ConversionStrategy', 'ReversalStrategy', 'BoxSpreadStrategy', 'CarryArbitrageDetector',
    'VolatilityArbitrageDetector', 'CalendarSpreadArbitrage', 'SyntheticInstrumentBuilder',
    'ArbitrageScanner'
]