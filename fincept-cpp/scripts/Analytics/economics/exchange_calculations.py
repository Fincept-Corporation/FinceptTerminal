"""
Exchange Rate Mathematical Calculations Module
=============================================

Professional exchange rate calculations implementing CFA Institute Level I curriculum. Provides mathematical operations for cross-rates, arbitrage relationships, forward calculations, and currency percentage changes with precision decimal arithmetic.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Spot exchange rates for major currency pairs
  - Forward exchange rates and forward points
  - Interest rate data for arbitrage calculations
  - Cross-currency exchange rate matrices
  - Historical exchange rate data for trend analysis
  - Market conventions and quote specifications
  - Currency quote base and terms specifications

OUTPUT:
  - Cross-rate calculations and implied exchange rates
  - Forward rate calculations from spot rates and interest differentials
  - Arbitrage opportunity identification and profit calculations
  - Forward premium/discount percentages and interpretations
  - Currency percentage change calculations and trend analysis
  - Bid-ask spread calculations and impact assessments
  - Professional exchange rate mathematical operations

PARAMETERS:
  - rate_a: Exchange rate for currency pair A
  - rate_b: Exchange rate for currency pair B
  - base_currency: Base currency for calculations
  - terms_currency: Terms currency for calculations
  - spot_rate: Current spot exchange rate
  - forward_points: Forward points for premium/discount
  - interest_rate_a: Interest rate for currency A
  - interest_rate_b: Interest rate for currency B
  - time_period: Time period for forward calculations
  - initial_rate: Starting exchange rate
  - final_rate: Ending exchange rate
  - bid_rate: Bid price for currency pair
  - ask_rate: Ask price for currency pair
"""

from decimal import Decimal
from typing import Dict, List, Tuple, Optional, Any
from datetime import datetime, timedelta
import pandas as pd
from .core import EconomicsBase, ValidationError, CalculationError, DataError


class ExchangeCalculator(EconomicsBase):
    """Main exchange rate calculations coordinator"""

    def __init__(self, precision: int = 8, base_currency: str = 'USD'):
        super().__init__(precision, base_currency)
        self.cross_rate = CrossRateCalculator(precision, base_currency)
        self.forward_calc = ForwardCalculator(precision, base_currency)

    def calculate(self, calculation_type: str, **kwargs) -> Dict[str, Any]:
        """Route calculation to appropriate calculator"""
        calculators = {
            'cross_rate': self.cross_rate.calculate,
            'forward_rate': self.forward_calc.calculate,
            'percentage_change': self.calculate_percentage_change,
            'arbitrage_check': self.check_arbitrage_relationship
        }

        if calculation_type not in calculators:
            raise ValidationError(f"Unknown calculation type: {calculation_type}")

        return calculators[calculation_type](**kwargs)

    def calculate_percentage_change(self, initial_rate: Decimal, final_rate: Decimal,
                                    quote_convention: str = 'direct') -> Dict[str, Any]:
        """Calculate percentage change in currency relative to another"""
        initial = self.to_decimal(initial_rate)
        final = self.to_decimal(final_rate)

        self.validator.validate_exchange_rate(initial)
        self.validator.validate_exchange_rate(final)

        if quote_convention == 'direct':
            # Direct quote: domestic currency per unit of foreign currency
            # Increase means domestic currency weakening
            percentage_change = ((final - initial) / initial) * self.to_decimal(100)
            currency_movement = 'weakened' if percentage_change > 0 else 'strengthened'
        else:
            # Indirect quote: foreign currency per unit of domestic currency
            # Increase means domestic currency strengthening
            percentage_change = ((final - initial) / initial) * self.to_decimal(100)
            currency_movement = 'strengthened' if percentage_change > 0 else 'weakened'

        return {
            'initial_rate': initial,
            'final_rate': final,
            'percentage_change': percentage_change,
            'absolute_change': final - initial,
            'quote_convention': quote_convention,
            'currency_movement': currency_movement,
            'interpretation': self._interpret_currency_change(percentage_change, quote_convention)
        }

    def _interpret_currency_change(self, change: Decimal, convention: str) -> str:
        """Provide interpretation of currency movement"""
        abs_change = abs(change)

        if abs_change < self.to_decimal(0.5):
            magnitude = "minimal"
        elif abs_change < self.to_decimal(2):
            magnitude = "moderate"
        elif abs_change < self.to_decimal(5):
            magnitude = "significant"
        else:
            magnitude = "substantial"

        direction = "appreciation" if change > 0 else "depreciation"
        if convention == 'indirect':
            direction = "depreciation" if change > 0 else "appreciation"

        return f"{magnitude} {direction} ({abs_change:.2f}%)"

    def check_arbitrage_relationship(self, spot_rate: Decimal, forward_rate: Decimal,
                                     domestic_rate: Decimal, foreign_rate: Decimal,
                                     time_period: Decimal) -> Dict[str, Any]:
        """Check arbitrage relationship between spot/forward rates and interest rates"""
        spot = self.to_decimal(spot_rate)
        forward = self.to_decimal(forward_rate)
        r_domestic = self.to_decimal(domestic_rate)
        r_foreign = self.to_decimal(foreign_rate)
        t = self.to_decimal(time_period)

        # Theoretical forward rate based on interest rate parity
        theoretical_forward = spot * (
                (self.to_decimal(1) + r_domestic * t) /
                (self.to_decimal(1) + r_foreign * t)
        )

        deviation = forward - theoretical_forward
        deviation_percentage = (deviation / theoretical_forward) * self.to_decimal(100)

        # Arbitrage opportunity threshold (typical transaction costs)
        arbitrage_threshold = self.to_decimal(0.1)  # 0.1%
        arbitrage_exists = abs(deviation_percentage) > arbitrage_threshold

        arbitrage_strategy = self._determine_arbitrage_strategy(
            deviation, spot, forward, r_domestic, r_foreign, t
        ) if arbitrage_exists else None

        return {
            'spot_rate': spot,
            'forward_rate': forward,
            'theoretical_forward': theoretical_forward,
            'deviation': deviation,
            'deviation_percentage': deviation_percentage,
            'arbitrage_exists': arbitrage_exists,
            'arbitrage_strategy': arbitrage_strategy,
            'domestic_rate': r_domestic,
            'foreign_rate': r_foreign,
            'time_period': t,
            'relationship_holds': not arbitrage_exists
        }

    def _determine_arbitrage_strategy(self, deviation: Decimal, spot: Decimal,
                                      forward: Decimal, r_dom: Decimal, r_for: Decimal,
                                      t: Decimal) -> Dict[str, str]:
        """Determine arbitrage strategy when opportunity exists"""
        if deviation > 0:  # Forward overpriced
            return {
                'action': 'Sell forward, buy spot',
                'step1': 'Borrow domestic currency',
                'step2': 'Convert to foreign currency at spot rate',
                'step3': 'Invest foreign currency at foreign rate',
                'step4': 'Sell foreign currency forward',
                'step5': 'At maturity: collect foreign investment, deliver to forward contract',
                'profit_source': 'Forward rate higher than theoretical rate'
            }
        else:  # Forward underpriced
            return {
                'action': 'Buy forward, sell spot',
                'step1': 'Borrow foreign currency',
                'step2': 'Convert to domestic currency at spot rate',
                'step3': 'Invest domestic currency at domestic rate',
                'step4': 'Buy foreign currency forward',
                'step5': 'At maturity: collect domestic investment, buy foreign currency via forward',
                'profit_source': 'Forward rate lower than theoretical rate'
            }


class CrossRateCalculator(EconomicsBase):
    """Currency cross-rate calculations and interpretations"""

    def calculate_cross_rate(self, base_quote_rates: Dict[str, Decimal],
                             currency_pair: str) -> Dict[str, Any]:
        """Calculate cross-rate between two currencies using base currency"""
        pair_parts = currency_pair.split('/')
        if len(pair_parts) != 2:
            raise ValidationError("Currency pair must be in format 'CUR1/CUR2'")

        base_currency, quote_currency = pair_parts
        base_currency = base_currency.upper()
        quote_currency = quote_currency.upper()

        # Get rates vs base currency (typically USD)
        base_vs_reference = base_quote_rates.get(base_currency)
        quote_vs_reference = base_quote_rates.get(quote_currency)

        if base_vs_reference is None:
            raise DataError(f"Missing rate for {base_currency}")
        if quote_vs_reference is None:
            raise DataError(f"Missing rate for {quote_currency}")

        # Calculate cross rate
        cross_rate = quote_vs_reference / base_vs_reference

        # Calculate inverse rate
        inverse_rate = base_vs_reference / quote_vs_reference
        inverse_pair = f"{quote_currency}/{base_currency}"

        return {
            'currency_pair': currency_pair,
            'cross_rate': cross_rate,
            'inverse_pair': inverse_pair,
            'inverse_rate': inverse_rate,
            'base_currency_rate': base_vs_reference,
            'quote_currency_rate': quote_vs_reference,
            'reference_currency': self.base_currency,
            'calculation_method': f"{quote_currency}/{self.base_currency} ÷ {base_currency}/{self.base_currency}",
            'interpretation': f"1 {base_currency} = {cross_rate:.6f} {quote_currency}"
        }

    def calculate_triangular_cross_rates(self, currency_rates: Dict[str, Decimal]) -> Dict[str, Any]:
        """Calculate all possible cross-rates from given currency rates"""
        currencies = list(currency_rates.keys())
        cross_rates = {}

        for i, base_curr in enumerate(currencies):
            for j, quote_curr in enumerate(currencies):
                if i != j:
                    pair = f"{base_curr}/{quote_curr}"
                    try:
                        result = self.calculate_cross_rate(currency_rates, pair)
                        cross_rates[pair] = {
                            'rate': result['cross_rate'],
                            'calculation': result['calculation_method']
                        }
                    except Exception as e:
                        cross_rates[pair] = {'error': str(e)}

        return {
            'cross_rates': cross_rates,
            'total_pairs': len(cross_rates),
            'input_currencies': currencies,
            'reference_currency': self.base_currency,
            'timestamp': datetime.now().isoformat()
        }

    def verify_cross_rate_consistency(self, rates: Dict[str, Decimal]) -> Dict[str, Any]:
        """Verify cross-rate consistency (no arbitrage condition)"""
        currencies = list(rates.keys())
        inconsistencies = []

        # Check triangular consistency for all combinations
        for i in range(len(currencies)):
            for j in range(i + 1, len(currencies)):
                for k in range(j + 1, len(currencies)):
                    curr1, curr2, curr3 = currencies[i], currencies[j], currencies[k]

                    # Calculate cross rates
                    rate_12 = rates[curr2] / rates[curr1]  # curr1/curr2
                    rate_23 = rates[curr3] / rates[curr2]  # curr2/curr3
                    rate_31 = rates[curr1] / rates[curr3]  # curr3/curr1

                    # Check if rate_12 * rate_23 * rate_31 = 1
                    product = rate_12 * rate_23 * rate_31
                    deviation = abs(product - self.to_decimal(1))

                    if deviation > self.to_decimal(0.0001):  # 0.01% tolerance
                        inconsistencies.append({
                            'currencies': [curr1, curr2, curr3],
                            'rates': [rate_12, rate_23, rate_31],
                            'product': product,
                            'deviation': deviation,
                            'deviation_percentage': deviation * self.to_decimal(100)
                        })

        return {
            'consistent': len(inconsistencies) == 0,
            'inconsistencies': inconsistencies,
            'total_combinations_checked': len(currencies) * (len(currencies) - 1) * (len(currencies) - 2) // 6,
            'currencies_analyzed': currencies
        }

    def calculate(self, calculation_type: str = 'single', **kwargs) -> Dict[str, Any]:
        """Main cross-rate calculation dispatcher"""
        if calculation_type == 'single':
            return self.calculate_cross_rate(
                kwargs['base_quote_rates'],
                kwargs['currency_pair']
            )
        elif calculation_type == 'all_pairs':
            return self.calculate_triangular_cross_rates(kwargs['currency_rates'])
        elif calculation_type == 'consistency_check':
            return self.verify_cross_rate_consistency(kwargs['rates'])
        else:
            raise ValidationError(f"Unknown calculation type: {calculation_type}")


class ForwardCalculator(EconomicsBase):
    """Forward rate calculations using points and percentage terms"""

    def calculate_forward_rate_from_points(self, spot_rate: Decimal, forward_points: Decimal,
                                           point_convention: str = 'standard') -> Dict[str, Any]:
        """Calculate forward rate from forward points"""
        spot = self.to_decimal(spot_rate)
        points = self.to_decimal(forward_points)

        self.validator.validate_exchange_rate(spot)

        if point_convention == 'standard':
            # Standard: points are in the last decimal place (typically 4th for major pairs)
            divisor = self.to_decimal(10000)  # Standard 4 decimal places
        elif point_convention == 'big_figure':
            # Big figure: points are in pips (5th decimal place)
            divisor = self.to_decimal(100000)
        else:
            raise ValidationError(f"Unknown point convention: {point_convention}")

        # Calculate forward rate
        forward_rate = spot + (points / divisor)

        # Determine if premium or discount
        is_premium = forward_rate > spot
        premium_discount = forward_rate - spot
        premium_discount_percentage = (premium_discount / spot) * self.to_decimal(100)

        return {
            'spot_rate': spot,
            'forward_points': points,
            'forward_rate': forward_rate,
            'point_convention': point_convention,
            'divisor': divisor,
            'premium_discount': premium_discount,
            'premium_discount_percentage': premium_discount_percentage,
            'is_premium': is_premium,
            'calculation': f"{spot} + ({points}/{divisor}) = {forward_rate}"
        }

    def calculate_forward_points_from_rate(self, spot_rate: Decimal, forward_rate: Decimal,
                                           point_convention: str = 'standard') -> Dict[str, Any]:
        """Calculate forward points from spot and forward rates"""
        spot = self.to_decimal(spot_rate)
        forward = self.to_decimal(forward_rate)

        self.validator.validate_exchange_rate(spot)
        self.validator.validate_exchange_rate(forward)

        if point_convention == 'standard':
            multiplier = self.to_decimal(10000)
        elif point_convention == 'big_figure':
            multiplier = self.to_decimal(100000)
        else:
            raise ValidationError(f"Unknown point convention: {point_convention}")

        # Calculate forward points
        forward_points = (forward - spot) * multiplier

        return {
            'spot_rate': spot,
            'forward_rate': forward,
            'forward_points': forward_points,
            'point_convention': point_convention,
            'multiplier': multiplier,
            'is_premium': forward > spot,
            'calculation': f"({forward} - {spot}) × {multiplier} = {forward_points} points"
        }

    def calculate_forward_rate_percentage(self, spot_rate: Decimal, premium_discount_percent: Decimal,
                                          time_to_maturity: Decimal,
                                          annualized: bool = True) -> Dict[str, Any]:
        """Calculate forward rate from percentage premium/discount"""
        spot = self.to_decimal(spot_rate)
        premium_percent = self.to_decimal(premium_discount_percent)
        time_period = self.to_decimal(time_to_maturity)

        self.validator.validate_exchange_rate(spot)
        self.validator.validate_time_period(time_period)

        if annualized:
            # Convert annualized rate to period rate
            period_premium = premium_percent * time_period
        else:
            # Already a period rate
            period_premium = premium_percent

        # Calculate forward rate
        forward_rate = spot * (self.to_decimal(1) + period_premium / self.to_decimal(100))

        # Calculate equivalent annualized rate if input was period rate
        if not annualized:
            annualized_premium = period_premium / time_period
        else:
            annualized_premium = premium_percent

        return {
            'spot_rate': spot,
            'forward_rate': forward_rate,
            'premium_discount_percent': premium_percent,
            'time_to_maturity': time_period,
            'period_premium': period_premium,
            'annualized_premium': annualized_premium,
            'is_annualized_input': annualized,
            'is_premium': premium_percent > 0,
            'calculation': f"{spot} × (1 + {period_premium}%) = {forward_rate}"
        }

    def interpret_forward_discount_premium(self, spot_rate: Decimal, forward_rate: Decimal,
                                           time_to_maturity: Decimal) -> Dict[str, Any]:
        """Interpret forward discount or premium"""
        spot = self.to_decimal(spot_rate)
        forward = self.to_decimal(forward_rate)
        time_period = self.to_decimal(time_to_maturity)

        # Calculate premium/discount
        absolute_difference = forward - spot
        percentage_difference = (absolute_difference / spot) * self.to_decimal(100)
        annualized_percentage = percentage_difference / time_period

        # Interpretation
        if forward > spot:
            interpretation = f"Forward premium of {percentage_difference:.4f}% ({annualized_percentage:.4f}% annualized)"
            market_expectation = "Base currency expected to weaken"
            interest_rate_implication = "Base currency likely has lower interest rates"
        elif forward < spot:
            interpretation = f"Forward discount of {abs(percentage_difference):.4f}% ({abs(annualized_percentage):.4f}% annualized)"
            market_expectation = "Base currency expected to strengthen"
            interest_rate_implication = "Base currency likely has higher interest rates"
        else:
            interpretation = "Forward rate equals spot rate (no premium or discount)"
            market_expectation = "No expected currency movement"
            interest_rate_implication = "Interest rates likely equal between currencies"

        return {
            'spot_rate': spot,
            'forward_rate': forward,
            'absolute_difference': absolute_difference,
            'percentage_difference': percentage_difference,
            'annualized_percentage': annualized_percentage,
            'time_to_maturity': time_period,
            'interpretation': interpretation,
            'market_expectation': market_expectation,
            'interest_rate_implication': interest_rate_implication,
            'is_premium': forward > spot,
            'is_discount': forward < spot
        }

    def calculate(self, calculation_type: str, **kwargs) -> Dict[str, Any]:
        """Main forward calculation dispatcher"""
        calculations = {
            'from_points': lambda: self.calculate_forward_rate_from_points(
                self.to_decimal(kwargs['spot_rate']),
                self.to_decimal(kwargs['forward_points']),
                kwargs.get('point_convention', 'standard')
            ),
            'to_points': lambda: self.calculate_forward_points_from_rate(
                self.to_decimal(kwargs['spot_rate']),
                self.to_decimal(kwargs['forward_rate']),
                kwargs.get('point_convention', 'standard')
            ),
            'from_percentage': lambda: self.calculate_forward_rate_percentage(
                self.to_decimal(kwargs['spot_rate']),
                self.to_decimal(kwargs['premium_discount_percent']),
                self.to_decimal(kwargs['time_to_maturity']),
                kwargs.get('annualized', True)
            ),
            'interpret_premium_discount': lambda: self.interpret_forward_discount_premium(
                self.to_decimal(kwargs['spot_rate']),
                self.to_decimal(kwargs['forward_rate']),
                self.to_decimal(kwargs['time_to_maturity'])
            )
        }

        if calculation_type not in calculations:
            raise ValidationError(f"Unknown calculation type: {calculation_type}")

        result = calculations[calculation_type]()
        result['metadata'] = self.get_metadata()
        result['calculation_type'] = calculation_type

        return result