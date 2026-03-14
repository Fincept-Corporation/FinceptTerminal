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


class CoveredCallStrategy:
    """
    Covered Call Strategy Implementation

    CFA Standards: Options strategies, income generation, downside protection analysis

    Swedroe Perspective (from "The Only Guide"):
    - Covered calls = Giving up upside for premium income
    - NOT a free lunch - opportunity cost when stock rises strongly
    - Asymmetric payoff: Limited upside, full downside (minus premium)
    - Tax inefficiency (short-term gains from premiums)
    - Better alternatives: Buy-and-hold, index funds

    When suitable:
    - Neutral to slightly bullish outlook
    - Stock considered overvalued short-term
    - Need income in flat markets
    - Willing to cap upside

    Position: Long 100 shares + Short 1 call option
    """

    def __init__(self, stock_price: float, shares: int = 100):
        """
        Initialize covered call strategy

        Args:
            stock_price: Current stock price
            shares: Number of shares owned (typically 100 per contract)
        """
        self.stock_price = stock_price
        self.shares = shares
        self.stock_value = stock_price * shares
        self.call_option = None
        self.premium_received = 0.0

    def write_call(self, strike_price: float, premium: float, expiry_date: datetime,
                   option_type: OptionType = OptionType.CALL) -> Dict:
        """
        Write (sell) call option against stock position

        Args:
            strike_price: Strike price of call
            premium: Premium received per share
            expiry_date: Option expiration date
            option_type: Must be CALL

        Returns:
            Position summary
        """
        if option_type != OptionType.CALL:
            raise ValueError("Covered call must be a call option")

        self.call_option = VanillaOption(
            option_type=option_type,
            underlying_type=UnderlyingType.EQUITY,
            expiry_date=expiry_date,
            strike_price=strike_price,
            exercise_style=ExerciseStyle.AMERICAN,
            notional=self.shares
        )

        self.premium_received = premium * self.shares

        return {
            'strategy': 'Covered Call',
            'stock_position': self.shares,
            'stock_price': self.stock_price,
            'stock_value': self.stock_value,
            'call_strike': strike_price,
            'premium_per_share': premium,
            'total_premium': self.premium_received,
            'net_basis': self.stock_value - self.premium_received,
            'expiry': expiry_date.strftime('%Y-%m-%d')
        }

    def calculate_payoff(self, stock_price_at_expiry: float) -> Dict:
        """
        Calculate payoff at expiration

        CFA: Payoff = Stock Value + Option Value (negative for short)

        Args:
            stock_price_at_expiry: Stock price at expiration

        Returns:
            Payoff analysis
        """
        if self.call_option is None:
            raise ValueError("No call option written")

        # Stock value at expiry
        stock_value_final = stock_price_at_expiry * self.shares
        stock_pnl = stock_value_final - self.stock_value

        # Call option payoff (SHORT position)
        call_payoff = -self.call_option.calculate_payoff(stock_price_at_expiry)

        # Total payoff
        total_payoff = stock_pnl + call_payoff + self.premium_received

        # Return calculations
        total_return = total_payoff / self.stock_value

        # Assignment check
        assigned = stock_price_at_expiry > self.call_option.strike_price

        return {
            'stock_price_at_expiry': stock_price_at_expiry,
            'stock_value_final': stock_value_final,
            'stock_pnl': stock_pnl,
            'call_payoff': call_payoff,
            'premium_kept': self.premium_received,
            'total_payoff': total_payoff,
            'total_return': total_return,
            'total_return_pct': total_return * 100,
            'option_assigned': assigned,
            'shares_called_away': self.shares if assigned else 0
        }

    def payoff_profile(self, price_range: Tuple[float, float], num_points: int = 50) -> List[Dict]:
        """
        Generate complete payoff profile across price range

        Args:
            price_range: (min_price, max_price) tuple
            num_points: Number of points to calculate

        Returns:
            List of payoff points
        """
        if self.call_option is None:
            raise ValueError("No call option written")

        prices = np.linspace(price_range[0], price_range[1], num_points)
        payoffs = []

        for price in prices:
            result = self.calculate_payoff(price)
            payoffs.append({
                'stock_price': price,
                'total_payoff': result['total_payoff'],
                'total_return_pct': result['total_return_pct']
            })

        return payoffs

    def breakeven_analysis(self) -> Dict:
        """
        Calculate breakeven points

        Breakeven = Initial stock price - Premium received

        Returns:
            Breakeven analysis
        """
        if self.call_option is None:
            raise ValueError("No call option written")

        breakeven_price = self.stock_price - (self.premium_received / self.shares)
        downside_protection = self.premium_received / self.stock_value

        return {
            'initial_stock_price': self.stock_price,
            'premium_per_share': self.premium_received / self.shares,
            'breakeven_price': breakeven_price,
            'downside_protection_pct': downside_protection * 100,
            'interpretation': f"Protected down to ${breakeven_price:.2f} ({downside_protection*100:.2f}% below current)"
        }

    def max_profit_and_loss(self) -> Dict:
        """
        Calculate maximum profit and loss

        CFA:
        - Max Profit = (Strike - Stock Price) + Premium (if stock >= strike at expiry)
        - Max Loss = Stock Price - Premium (if stock goes to $0)

        Returns:
            Max profit/loss analysis
        """
        if self.call_option is None:
            raise ValueError("No call option written")

        strike = self.call_option.strike_price
        premium_per_share = self.premium_received / self.shares

        # Max profit: stock called away at strike
        max_profit_per_share = (strike - self.stock_price) + premium_per_share
        max_profit_total = max_profit_per_share * self.shares
        max_profit_pct = max_profit_per_share / self.stock_price

        # Max loss: stock goes to zero (keep premium)
        max_loss_per_share = self.stock_price - premium_per_share
        max_loss_total = max_loss_per_share * self.shares
        max_loss_pct = max_loss_per_share / self.stock_price

        return {
            'max_profit': {
                'per_share': max_profit_per_share,
                'total': max_profit_total,
                'return_pct': max_profit_pct * 100,
                'occurs_when': f'Stock >= ${strike} at expiry'
            },
            'max_loss': {
                'per_share': max_loss_per_share,
                'total': max_loss_total,
                'loss_pct': max_loss_pct * 100,
                'occurs_when': 'Stock goes to $0'
            },
            'risk_reward_ratio': abs(max_profit_per_share / max_loss_per_share)
        }

    def return_if_unchanged(self) -> Dict:
        """
        Calculate return if stock price unchanged at expiration

        Returns:
            Return analysis for flat market
        """
        if self.call_option is None:
            raise ValueError("No call option written")

        result = self.calculate_payoff(self.stock_price)

        return {
            'scenario': 'Stock Unchanged',
            'stock_price_at_expiry': self.stock_price,
            'total_return': result['total_return'],
            'total_return_pct': result['total_return_pct'],
            'annualized_return_pct': self._annualize_return(result['total_return']),
            'source': 'Premium income only'
        }

    def _annualize_return(self, return_period: float) -> float:
        """Annualize return based on option time to expiry"""
        if self.call_option is None:
            return 0.0

        days_to_expiry = (self.call_option.expiry_date - datetime.now()).days
        if days_to_expiry <= 0:
            return 0.0

        years = days_to_expiry / 365.25
        return (1 + return_period) ** (1 / years) - 1 if years > 0 else 0.0

    def moneyness_analysis(self) -> Dict:
        """
        Analyze strike selection (moneyness)

        ITM: Strike < Stock Price (more premium, less upside)
        ATM: Strike ≈ Stock Price (balanced)
        OTM: Strike > Stock Price (less premium, more upside)

        Returns:
            Moneyness classification and implications
        """
        if self.call_option is None:
            raise ValueError("No call option written")

        strike = self.call_option.strike_price
        stock = self.stock_price

        moneyness_ratio = strike / stock

        if moneyness_ratio < 0.98:
            classification = 'ITM (In-The-Money)'
            characteristics = {
                'premium': 'High',
                'upside': 'Limited (likely called away)',
                'probability_assignment': 'High',
                'strategy_outlook': 'Bearish to neutral'
            }
        elif moneyness_ratio < 1.02:
            classification = 'ATM (At-The-Money)'
            characteristics = {
                'premium': 'Moderate',
                'upside': 'Moderate',
                'probability_assignment': 'Medium',
                'strategy_outlook': 'Neutral'
            }
        else:
            classification = 'OTM (Out-of-The-Money)'
            characteristics = {
                'premium': 'Lower',
                'upside': 'More (less likely called away)',
                'probability_assignment': 'Lower',
                'strategy_outlook': 'Neutral to bullish'
            }

        return {
            'stock_price': stock,
            'strike_price': strike,
            'moneyness_ratio': moneyness_ratio,
            'classification': classification,
            'characteristics': characteristics,
            'upside_room': max(0, strike - stock),
            'upside_room_pct': max(0, (strike - stock) / stock) * 100
        }

    def compare_to_buy_and_hold(self, stock_return_scenarios: List[float]) -> Dict:
        """
        Compare covered call strategy to simple buy-and-hold

        Swedroe Insight: Covered calls underperform in strong bull markets
        They give up unlimited upside for limited premium income

        Args:
            stock_return_scenarios: List of potential stock returns (e.g., [-0.20, 0.0, 0.10, 0.30])

        Returns:
            Comparison analysis
        """
        if self.call_option is None:
            raise ValueError("No call option written")

        comparisons = []

        for stock_return in stock_return_scenarios:
            final_stock_price = self.stock_price * (1 + stock_return)

            # Buy-and-hold return
            bh_return = stock_return

            # Covered call return
            cc_result = self.calculate_payoff(final_stock_price)
            cc_return = cc_result['total_return']

            # Difference
            return_difference = cc_return - bh_return

            comparisons.append({
                'stock_return': stock_return * 100,
                'final_stock_price': final_stock_price,
                'buy_hold_return_pct': bh_return * 100,
                'covered_call_return_pct': cc_return * 100,
                'difference_pct': return_difference * 100,
                'winner': 'Covered Call' if cc_return > bh_return else 'Buy-and-Hold',
                'option_assigned': final_stock_price > self.call_option.strike_price
            })

        return {
            'scenarios': comparisons,
            'swedroe_insight': (
                'Covered calls outperform in flat/down markets (premium cushion) '
                'but underperform in strong bull markets (capped upside). '
                'Over long term, buy-and-hold typically wins due to unlimited upside.'
            )
        }

    def tax_considerations(self, holding_period_days: int, tax_rate_short: float = 0.37,
                          tax_rate_long: float = 0.20) -> Dict:
        """
        Analyze tax implications

        Swedroe: Covered calls create tax inefficiency
        - Premium income = SHORT-TERM capital gain (higher rate)
        - If assigned, can convert long-term gains to short-term
        - Holding period rules complex

        Args:
            holding_period_days: Days stock held before writing call
            tax_rate_short: Short-term capital gains rate
            tax_rate_long: Long-term capital gains rate

        Returns:
            Tax analysis
        """
        if self.call_option is None:
            raise ValueError("No call option written")

        # Premium is always short-term income
        premium_tax = self.premium_received * tax_rate_short

        # Stock gain/loss tax treatment
        qualified_long_term = holding_period_days >= 365

        return {
            'premium_received': self.premium_received,
            'premium_tax': premium_tax,
            'premium_after_tax': self.premium_received - premium_tax,
            'stock_holding_period_days': holding_period_days,
            'qualified_for_ltcg': qualified_long_term,
            'applicable_tax_rate': tax_rate_long if qualified_long_term else tax_rate_short,
            'swedroe_warning': (
                'Covered calls create tax drag: (1) Premium taxed as short-term income, '
                '(2) Can suspend long-term holding period, converting LTCG to STCG, '
                '(3) Best used in tax-deferred accounts (IRA, 401k)'
            ),
            'recommendation': 'Use in tax-deferred accounts to avoid tax inefficiency'
        }

    def swedroe_verdict(self) -> Dict:
        """
        Larry Swedroe's complete verdict on Covered Call strategies

        Category: Generally NOT recommended for most investors

        Returns:
            Complete verdict
        """
        return {
            'strategy': 'Covered Call Writing',
            'category': 'THE FLAWED (for most investors)',
            'overall_rating': '4/10 - Limited use cases',

            'the_good': [
                'Generates income in flat/declining markets',
                'Provides small downside cushion (premium)',
                'Can be profitable in sideways markets',
                'Psychologically satisfying (collecting premiums)'
            ],

            'the_bad': [
                'CAPS UPSIDE - gives up unlimited stock gains',
                'Full downside exposure (minus small premium)',
                'Asymmetric payoff: limited gain, large loss potential',
                'Tax inefficient (premiums = short-term gains)',
                'Transaction costs reduce returns',
                'Timing risk - hard to optimize consistently',
                'Underperforms buy-and-hold in bull markets'
            ],

            'the_ugly': [
                'Often sold by brokers for commissions, not client benefit',
                'Marketed as "free money" but comes with real costs',
                'Complexity allows for mistakes and suboptimal execution',
                'Can suspend long-term capital gains status',
                'Behavioral trap: selling winners too early'
            ],

            'key_findings': {
                'expected_return': 'Lower than buy-and-hold long-term',
                'risk_reduction': 'Minimal (premium cushion only)',
                'tax_efficiency': 'Poor (short-term income treatment)',
                'complexity': 'High (timing, strike selection, rolling)',
                'better_alternative': 'Buy-and-hold index funds'
            },

            'swedroe_quote': (
                '"Covered call writing is not a free lunch. You are selling away your upside '
                'in exchange for premium income. In bull markets, you give up significant gains. '
                'The small premium does not compensate for unlimited upside forgone. '
                'For most investors, simple buy-and-hold is superior."'
            ),

            'suitable_for': [
                'Sophisticated investors with neutral short-term outlook',
                'Stocks considered temporarily overvalued',
                'Tax-deferred accounts only (avoid tax drag)',
                'Investors willing to actively manage positions',
                'Those who would sell stock at strike price anyway'
            ],

            'not_suitable_for': [
                'Long-term buy-and-hold investors',
                'Taxable accounts (tax inefficiency)',
                'Bull market environments (capped upside hurts)',
                'Investors unable to monitor and roll positions',
                'Anyone expecting strong stock appreciation'
            ],

            'better_alternatives': [
                'Buy-and-hold diversified portfolio',
                'Index funds (lower cost, tax efficient)',
                'If need income: dividend-paying stocks or bond allocation',
                'If bearish: just sell the stock (don\'t half-commit)'
            ],

            'final_verdict': (
                'Covered calls are FLAWED for most investors. They sacrifice unlimited upside '
                'for limited premium income, creating an asymmetric bet. Tax inefficiency, '
                'transaction costs, and behavioral pitfalls make them inferior to simple buy-and-hold '
                'for long-term wealth building. Use only in tax-deferred accounts with specific '
                'short-term neutral outlook, and be prepared to actively manage positions.'
            )
        }


# Export main classes
__all__ = [
    'OptionGreeks', 'BinomialNode', 'VanillaOption', 'OnePeriodBinomialModel',
    'TwoPeriodBinomialModel', 'BinomialPricingEngine', 'BlackScholesPricingEngine',
    'BlackModelPricingEngine', 'PutCallParity', 'ImpliedVolatilityCalculator',
    'DeltaHedging', 'CoveredCallStrategy'
]