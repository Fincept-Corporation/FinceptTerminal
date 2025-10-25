"""
Options Valuation Analytics Module
==================================

Comprehensive options pricing and analytics framework implementing binomial models, Black-Scholes-Merton, Greeks calculations, and put-call parity relationships. Supports European, American, and exotic options with full CFA Institute compliance.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Underlying asset spot prices and price series
  - Option contract specifications (strike, expiry, type)
  - Risk-free interest rates and yield curves
  - Dividend yields and payment schedules
  - Implied volatility surfaces and term structures
  - Historical volatility calculations and forecasts
  - Market prices for calibration and validation

OUTPUT:
  - Option fair values and theoretical prices
  - Complete Greek sensitivities (Delta, Gamma, Theta, Vega, Rho)
  - Implied volatility calculations and surfaces
  - Binomial tree price paths and probabilities
  - Put-call parity arbitrage analysis
  - Delta hedging strategies and recommendations

PARAMETERS:
  - spot_price: Current underlying asset price
  - strike_price: Option strike price
  - time_to_expiry: Time to expiration in years
  - risk_free_rate: Risk-free interest rate
  - dividend_yield: Dividend yield - default: 0.0
  - volatility: Volatility for pricing models
  - option_type: Call or Put option type
  - exercise_style: European or American exercise - default: EUROPEAN
  - steps: Number of binomial tree steps - default: 50
  - option_price: Market option price for implied vol
  - hedge_ratio: Delta hedge ratio for hedging
"""

import numpy as np
from typing import Optional, Dict, Tuple, List
from datetime import datetime
from dataclasses import dataclass
from scipy.stats import norm
from scipy.optimize import brentq
import logging

from .core import (
    ContingentClaim, OptionType, ExerciseStyle, UnderlyingType,
    MarketData, PricingResult, PricingEngine, ValidationError,
    ModelValidator, Constants
)

logger = logging.getLogger(__name__)


@dataclass
class OptionGreeks:
    """Container for option Greeks"""
    delta: float = 0.0  # Price sensitivity
    gamma: float = 0.0  # Delta sensitivity
    theta: float = 0.0  # Time decay
    vega: float = 0.0  # Volatility sensitivity
    rho: float = 0.0  # Interest rate sensitivity

    # Second-order Greeks
    vanna: float = 0.0  # Delta sensitivity to volatility
    volga: float = 0.0  # Vega sensitivity to volatility
    charm: float = 0.0  # Delta decay over time
    vomma: float = 0.0  # Vega convexity


@dataclass
class BinomialNode:
    """Single node in binomial tree"""
    time_step: int
    stock_price: float
    option_value: float
    probability_up: float = 0.0
    probability_down: float = 0.0


class VanillaOption(ContingentClaim):
    """Standard European/American call and put options"""

    def __init__(self,
                 option_type: OptionType,
                 underlying_type: UnderlyingType,
                 expiry_date: datetime,
                 strike_price: float,
                 exercise_style: ExerciseStyle = ExerciseStyle.EUROPEAN,
                 notional: float = 1.0):

        super().__init__(option_type, underlying_type, expiry_date,
                         strike_price, exercise_style, notional)

    def calculate_payoff(self, spot_price: float) -> float:
        """Calculate option payoff at expiration"""
        if self.option_type == OptionType.CALL:
            return max(0, spot_price - self.strike_price) * self.notional
        else:  # PUT
            return max(0, self.strike_price - spot_price) * self.notional

    def fair_value(self, market_data: MarketData) -> PricingResult:
        """Calculate fair value using appropriate model"""
        if self.exercise_style == ExerciseStyle.EUROPEAN:
            return self._black_scholes_price(market_data)
        else:  # American options
            return self._binomial_price(market_data, steps=100)

    def _black_scholes_price(self, market_data: MarketData) -> PricingResult:
        """Black-Scholes-Merton pricing for European options"""
        S = market_data.spot_price
        K = self.strike_price
        T = self.time_to_expiry()
        r = market_data.risk_free_rate
        q = market_data.dividend_yield
        sigma = market_data.volatility

        if T <= 0:
            return PricingResult(fair_value=self.calculate_payoff(S))

        # Calculate d1 and d2
        d1 = (np.log(S / K) + (r - q + 0.5 * sigma ** 2) * T) / (sigma * np.sqrt(T))
        d2 = d1 - sigma * np.sqrt(T)

        # Calculate option price
        if self.option_type == OptionType.CALL:
            price = S * np.exp(-q * T) * norm.cdf(d1) - K * np.exp(-r * T) * norm.cdf(d2)
        else:  # PUT
            price = K * np.exp(-r * T) * norm.cdf(-d2) - S * np.exp(-q * T) * norm.cdf(-d1)

        price *= self.notional
        intrinsic = self.intrinsic_value(S)

        return PricingResult(
            fair_value=price,
            intrinsic_value=intrinsic,
            time_value=price - intrinsic,
            calculation_details={
                "model": "Black-Scholes-Merton",
                "d1": d1,
                "d2": d2,
                "N_d1": norm.cdf(d1) if self.option_type == OptionType.CALL else norm.cdf(-d1),
                "N_d2": norm.cdf(d2) if self.option_type == OptionType.CALL else norm.cdf(-d2)
            }
        )

    def _binomial_price(self, market_data: MarketData, steps: int = 50) -> PricingResult:
        """Binomial tree pricing for American options"""
        engine = BinomialPricingEngine(steps)
        return engine.price(self, market_data)


class OnePeriodBinomialModel:
    """One-period binomial option pricing model"""

    def __init__(self, up_factor: float, down_factor: float, risk_free_rate: float, time_period: float):
        self.u = up_factor
        self.d = down_factor
        self.r = risk_free_rate
        self.dt = time_period

        # Calculate risk-neutral probabilities
        self.risk_neutral_prob_up = (np.exp(self.r * self.dt) - self.d) / (self.u - self.d)
        self.risk_neutral_prob_down = 1 - self.risk_neutral_prob_up

        # Validate probabilities
        if not (0 <= self.risk_neutral_prob_up <= 1):
            raise ValidationError(f"Invalid risk-neutral probability: {self.risk_neutral_prob_up}")

    def price_option(self, spot_price: float, strike_price: float, option_type: OptionType) -> Dict:
        """Price option using one-period model"""
        # Calculate stock prices at expiration
        stock_up = spot_price * self.u
        stock_down = spot_price * self.d

        # Calculate option payoffs
        if option_type == OptionType.CALL:
            payoff_up = max(0, stock_up - strike_price)
            payoff_down = max(0, stock_down - strike_price)
        else:  # PUT
            payoff_up = max(0, strike_price - stock_up)
            payoff_down = max(0, strike_price - stock_down)

        # Calculate option price using risk-neutral valuation
        expected_payoff = (self.risk_neutral_prob_up * payoff_up +
                           self.risk_neutral_prob_down * payoff_down)
        option_price = expected_payoff * np.exp(-self.r * self.dt)

        return {
            "option_price": option_price,
            "stock_up": stock_up,
            "stock_down": stock_down,
            "payoff_up": payoff_up,
            "payoff_down": payoff_down,
            "risk_neutral_prob_up": self.risk_neutral_prob_up,
            "risk_neutral_prob_down": self.risk_neutral_prob_down
        }


class TwoPeriodBinomialModel:
    """Two-period binomial option pricing model"""

    def __init__(self, up_factor: float, down_factor: float, risk_free_rate: float, time_per_period: float):
        self.u = up_factor
        self.d = down_factor
        self.r = risk_free_rate
        self.dt = time_per_period

        # Risk-neutral probabilities
        self.q = (np.exp(self.r * self.dt) - self.d) / (self.u - self.d)

        if not (0 <= self.q <= 1):
            raise ValidationError(f"Invalid risk-neutral probability: {self.q}")

    def price_option(self, spot_price: float, strike_price: float, option_type: OptionType) -> Dict:
        """Price option using two-period model"""
        # Build stock price tree
        S_uu = spot_price * self.u * self.u
        S_ud = spot_price * self.u * self.d
        S_dd = spot_price * self.d * self.d

        # Calculate payoffs at expiration
        if option_type == OptionType.CALL:
            payoff_uu = max(0, S_uu - strike_price)
            payoff_ud = max(0, S_ud - strike_price)
            payoff_dd = max(0, S_dd - strike_price)
        else:  # PUT
            payoff_uu = max(0, strike_price - S_uu)
            payoff_ud = max(0, strike_price - S_ud)
            payoff_dd = max(0, strike_price - S_dd)

        # Work backwards through tree
        # Period 1 values
        S_u = spot_price * self.u
        S_d = spot_price * self.d

        V_u = (self.q * payoff_uu + (1 - self.q) * payoff_ud) * np.exp(-self.r * self.dt)
        V_d = (self.q * payoff_ud + (1 - self.q) * payoff_dd) * np.exp(-self.r * self.dt)

        # Period 0 value (today)
        option_price = (self.q * V_u + (1 - self.q) * V_d) * np.exp(-self.r * self.dt)

        return {
            "option_price": option_price,
            "tree_values": {
                "S_0": spot_price, "S_u": S_u, "S_d": S_d,
                "S_uu": S_uu, "S_ud": S_ud, "S_dd": S_dd,
                "V_0": option_price, "V_u": V_u, "V_d": V_d,
                "payoff_uu": payoff_uu, "payoff_ud": payoff_ud, "payoff_dd": payoff_dd
            },
            "risk_neutral_prob": self.q
        }


class BinomialPricingEngine(PricingEngine):
    """Multi-period binomial tree pricing engine"""

    def __init__(self, steps: int = 50):
        self.steps = steps

    def price(self, instrument: VanillaOption, market_data: MarketData) -> PricingResult:
        """Price option using binomial tree"""
        if not self.validate_inputs(instrument, market_data):
            raise ValidationError("Invalid inputs for binomial pricing")

        S = market_data.spot_price
        K = instrument.strike_price
        T = instrument.time_to_expiry()
        r = market_data.risk_free_rate
        q = market_data.dividend_yield
        sigma = market_data.volatility

        if T <= 0:
            return PricingResult(fair_value=instrument.calculate_payoff(S))

        # Calculate tree parameters
        dt = T / self.steps
        u = np.exp(sigma * np.sqrt(dt))
        d = 1 / u
        prob_up = (np.exp((r - q) * dt) - d) / (u - d)

        # Build stock price tree
        stock_tree = np.zeros((self.steps + 1, self.steps + 1))
        option_tree = np.zeros((self.steps + 1, self.steps + 1))

        # Initialize stock prices at expiration
        for j in range(self.steps + 1):
            stock_tree[self.steps, j] = S * (u ** (self.steps - j)) * (d ** j)

        # Calculate option values at expiration
        for j in range(self.steps + 1):
            option_tree[self.steps, j] = instrument.calculate_payoff(stock_tree[self.steps, j])

        # Work backwards through tree
        for i in range(self.steps - 1, -1, -1):
            for j in range(i + 1):
                stock_tree[i, j] = S * (u ** (i - j)) * (d ** j)

                # European value
                european_value = (prob_up * option_tree[i + 1, j] +
                                  (1 - prob_up) * option_tree[i + 1, j + 1]) * np.exp(-r * dt)

                # American exercise value
                if instrument.exercise_style == ExerciseStyle.AMERICAN:
                    exercise_value = instrument.calculate_payoff(stock_tree[i, j])
                    option_tree[i, j] = max(european_value, exercise_value)
                else:
                    option_tree[i, j] = european_value

        fair_value = option_tree[0, 0] * instrument.notional
        intrinsic = instrument.intrinsic_value(S)

        return PricingResult(
            fair_value=fair_value,
            intrinsic_value=intrinsic,
            time_value=fair_value - intrinsic,
            calculation_details={
                "model": "Binomial Tree",
                "steps": self.steps,
                "u": u,
                "d": d,
                "prob_up": prob_up,
                "dt": dt
            }
        )

    def validate_inputs(self, instrument: VanillaOption, market_data: MarketData) -> bool:
        """Validate inputs for binomial pricing"""
        try:
            ModelValidator.validate_positive(market_data.spot_price, "spot_price")
            ModelValidator.validate_positive(instrument.strike_price, "strike_price")
            ModelValidator.validate_volatility(market_data.volatility)
            ModelValidator.validate_rate(market_data.risk_free_rate, "risk_free_rate")
            return True
        except ValidationError:
            return False


class BlackScholesPricingEngine(PricingEngine):
    """Black-Scholes-Merton pricing engine with Greeks"""

    def price(self, instrument: VanillaOption, market_data: MarketData) -> PricingResult:
        """Price European option using Black-Scholes-Merton"""
        if not self.validate_inputs(instrument, market_data):
            raise ValidationError("Invalid inputs for Black-Scholes pricing")

        if instrument.exercise_style != ExerciseStyle.EUROPEAN:
            raise ValueError("Black-Scholes only valid for European options")

        result = instrument._black_scholes_price(market_data)

        # Calculate Greeks
        greeks = self.calculate_greeks(instrument, market_data)
        result.greeks = greeks.__dict__

        return result

    def calculate_greeks(self, instrument: VanillaOption, market_data: MarketData) -> OptionGreeks:
        """Calculate all option Greeks"""
        S = market_data.spot_price
        K = instrument.strike_price
        T = instrument.time_to_expiry()
        r = market_data.risk_free_rate
        q = market_data.dividend_yield
        sigma = market_data.volatility

        if T <= 0:
            return OptionGreeks()

        # Calculate d1 and d2
        d1 = (np.log(S / K) + (r - q + 0.5 * sigma ** 2) * T) / (sigma * np.sqrt(T))
        d2 = d1 - sigma * np.sqrt(T)

        # Standard normal PDF and CDF
        n_d1 = norm.pdf(d1)
        N_d1 = norm.cdf(d1)
        N_d2 = norm.cdf(d2)

        greeks = OptionGreeks()

        # First-order Greeks
        if instrument.option_type == OptionType.CALL:
            greeks.delta = np.exp(-q * T) * N_d1
            greeks.theta = (-S * n_d1 * sigma * np.exp(-q * T) / (2 * np.sqrt(T))
                            - r * K * np.exp(-r * T) * N_d2
                            + q * S * np.exp(-q * T) * N_d1) / 365
            greeks.rho = K * T * np.exp(-r * T) * N_d2 / 100
        else:  # PUT
            greeks.delta = -np.exp(-q * T) * norm.cdf(-d1)
            greeks.theta = (-S * n_d1 * sigma * np.exp(-q * T) / (2 * np.sqrt(T))
                            + r * K * np.exp(-r * T) * norm.cdf(-d2)
                            - q * S * np.exp(-q * T) * norm.cdf(-d1)) / 365
            greeks.rho = -K * T * np.exp(-r * T) * norm.cdf(-d2) / 100

        # Common Greeks
        greeks.gamma = n_d1 * np.exp(-q * T) / (S * sigma * np.sqrt(T))
        greeks.vega = S * n_d1 * np.sqrt(T) * np.exp(-q * T) / 100

        # Second-order Greeks
        greeks.vanna = -greeks.vega * d2 / sigma
        greeks.volga = greeks.vega * d1 * d2 / sigma
        greeks.charm = (q * np.exp(-q * T) * norm.cdf(d1 if instrument.option_type == OptionType.CALL else -d1)
                        - np.exp(-q * T) * n_d1 * (2 * (r - q) * T - d2 * sigma * np.sqrt(T)) / (
                                    2 * T * sigma * np.sqrt(T))) / 365

        return greeks

    def validate_inputs(self, instrument: VanillaOption, market_data: MarketData) -> bool:
        """Validate inputs for Black-Scholes pricing"""
        try:
            ModelValidator.validate_positive(market_data.spot_price, "spot_price")
            ModelValidator.validate_positive(instrument.strike_price, "strike_price")
            ModelValidator.validate_volatility(market_data.volatility)
            ModelValidator.validate_rate(market_data.risk_free_rate, "risk_free_rate")
            ModelValidator.validate_non_negative(market_data.dividend_yield, "dividend_yield")

            if instrument.time_to_expiry() < 0:
                return False

            return True
        except ValidationError:
            return False


class BlackModelPricingEngine(PricingEngine):
    """Black model for options on futures and forwards"""

    def price(self, instrument: VanillaOption, market_data: MarketData) -> PricingResult:
        """Price option on futures using Black model"""
        if not self.validate_inputs(instrument, market_data):
            raise ValidationError("Invalid inputs for Black model pricing")

        F = market_data.forward_price or market_data.spot_price  # Forward/Futures price
        K = instrument.strike_price
        T = instrument.time_to_expiry()
        r = market_data.risk_free_rate
        sigma = market_data.volatility

        if T <= 0:
            return PricingResult(fair_value=instrument.calculate_payoff(F))

        # Black model d1 and d2
        d1 = (np.log(F / K) + 0.5 * sigma ** 2 * T) / (sigma * np.sqrt(T))
        d2 = d1 - sigma * np.sqrt(T)

        # Calculate option price
        if instrument.option_type == OptionType.CALL:
            price = np.exp(-r * T) * (F * norm.cdf(d1) - K * norm.cdf(d2))
        else:  # PUT
            price = np.exp(-r * T) * (K * norm.cdf(-d2) - F * norm.cdf(-d1))

        price *= instrument.notional

        return PricingResult(
            fair_value=price,
            calculation_details={
                "model": "Black Model",
                "forward_price": F,
                "d1": d1,
                "d2": d2
            }
        )

    def validate_inputs(self, instrument: VanillaOption, market_data: MarketData) -> bool:
        """Validate inputs for Black model"""
        try:
            forward_price = market_data.forward_price or market_data.spot_price
            ModelValidator.validate_positive(forward_price, "forward_price")
            ModelValidator.validate_positive(instrument.strike_price, "strike_price")
            ModelValidator.validate_volatility(market_data.volatility)
            ModelValidator.validate_rate(market_data.risk_free_rate, "risk_free_rate")
            return True
        except ValidationError:
            return False


class PutCallParity:
    """Put-call parity relationships"""

    @staticmethod
    def european_parity(call_price: float, put_price: float, spot_price: float,
                        strike_price: float, risk_free_rate: float, time_to_expiry: float,
                        dividend_yield: float = 0.0) -> Dict[str, float]:
        """Verify European put-call parity: C + K*e^(-rT) = P + S*e^(-qT)"""

        pv_strike = strike_price * np.exp(-risk_free_rate * time_to_expiry)
        pv_spot = spot_price * np.exp(-dividend_yield * time_to_expiry)

        left_side = call_price + pv_strike
        right_side = put_price + pv_spot

        arbitrage_profit = abs(left_side - right_side)

        return {
            "call_synthetic": put_price + pv_spot - pv_strike,
            "put_synthetic": call_price + pv_strike - pv_spot,
            "arbitrage_profit": arbitrage_profit,
            "parity_holds": arbitrage_profit < Constants.EPSILON
        }

    @staticmethod
    def forward_parity(call_price: float, put_price: float, forward_price: float,
                       strike_price: float, risk_free_rate: float, time_to_expiry: float) -> Dict[str, float]:
        """Put-call forward parity: C - P = (F - K) * e^(-rT)"""

        pv_diff = (forward_price - strike_price) * np.exp(-risk_free_rate * time_to_expiry)
        actual_diff = call_price - put_price

        arbitrage_profit = abs(actual_diff - pv_diff)

        return {
            "theoretical_diff": pv_diff,
            "actual_diff": actual_diff,
            "arbitrage_profit": arbitrage_profit,
            "parity_holds": arbitrage_profit < Constants.EPSILON
        }


class ImpliedVolatilityCalculator:
    """Calculate implied volatility from option prices"""

    @staticmethod
    def calculate_iv(option_price: float, spot_price: float, strike_price: float,
                     time_to_expiry: float, risk_free_rate: float,
                     option_type: OptionType, dividend_yield: float = 0.0) -> float:
        """Calculate implied volatility using Brent's method"""

        def objective_function(vol):
            """Objective function for root finding"""
            market_data = MarketData(
                spot_price=spot_price,
                risk_free_rate=risk_free_rate,
                dividend_yield=dividend_yield,
                volatility=vol,
                time_to_expiry=time_to_expiry
            )

            option = VanillaOption(
                option_type=option_type,
                underlying_type=UnderlyingType.EQUITY,
                expiry_date=datetime.now(),
                strike_price=strike_price
            )

            theoretical_price = option._black_scholes_price(market_data).fair_value
            return theoretical_price - option_price

        try:
            # Brent's method for root finding
            implied_vol = brentq(objective_function, 0.001, 5.0, xtol=1e-6)
            return implied_vol
        except ValueError:
            logger.warning("Could not calculate implied volatility")
            return np.nan


class DeltaHedging:
    """Delta hedging implementation"""

    def __init__(self, option: VanillaOption, hedge_ratio: float = None):
        self.option = option
        self.hedge_ratio = hedge_ratio
        self.position_delta = 0.0
        self.hedge_position = 0.0

    def calculate_hedge_ratio(self, market_data: MarketData) -> float:
        """Calculate delta hedge ratio"""
        engine = BlackScholesPricingEngine()
        greeks = engine.calculate_greeks(self.option, market_data)
        return -greeks.delta  # Opposite sign for hedging

    def rebalance_hedge(self, market_data: MarketData, option_position: float = 1.0) -> Dict:
        """Rebalance delta hedge"""
        new_hedge_ratio = self.calculate_hedge_ratio(market_data)
        new_hedge_position = new_hedge_ratio * option_position

        hedge_adjustment = new_hedge_position - self.hedge_position

        self.hedge_ratio = new_hedge_ratio
        self.hedge_position = new_hedge_position

        return {
            "new_hedge_ratio": new_hedge_ratio,
            "new_hedge_position": new_hedge_position,
            "hedge_adjustment": hedge_adjustment,
            "cost_of_adjustment": abs(hedge_adjustment) * market_data.spot_price
        }


# Export main classes
__all__ = [
    'OptionGreeks', 'BinomialNode', 'VanillaOption', 'OnePeriodBinomialModel',
    'TwoPeriodBinomialModel', 'BinomialPricingEngine', 'BlackScholesPricingEngine',
    'BlackModelPricingEngine', 'PutCallParity', 'ImpliedVolatilityCalculator',
    'DeltaHedging'
]