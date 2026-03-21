"""
Currency Exchange Rate Analytics Module
======================================

Comprehensive currency analysis implementing CFA Institute Level II curriculum for exchange rates, arbitrage, parity conditions, and carry trades. Provides advanced foreign exchange analytics with precision calculations and risk assessments.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Real-time FX market data (spot, forward, bid/ask rates)
  - Currency pair quotes and market depth information
  - Interest rate data for carry trade calculations
  - Inflation and price level data for PPP analysis
  - Balance of payments and current account data
  - Central bank policy and intervention data
  - Market volatility and liquidity indicators

OUTPUT:
  - Bid-offer spread analysis and impact assessments
  - Triangular arbitrage opportunity detection and profit calculations
  - Forward premium/discount calculations and mark-to-market valuations
  - International parity condition testing and violation analysis
  - Carry trade return calculations and risk assessments
  - Long-run fair value assessments and currency misalignment indicators
  - Currency crisis warning signals and vulnerability assessments

PARAMETERS:
  - bid: Bid price for currency pair
  - ask: Ask price for currency pair
  - spot_rate: Current spot exchange rate
  - forward_rate: Forward exchange rate for specific maturity
  - time_to_maturity: Time to forward contract maturity in years
  - domestic_rate: Domestic interest rate
  - foreign_rate: Foreign interest rate
  - domestic_inflation: Domestic inflation rate
  - foreign_inflation: Foreign inflation rate
  - leverage: Leverage factor for carry trades
  - investment_amount: Investment amount for arbitrage calculations
  - currency_quotes: Dictionary of currency pair bid/ask quotes
  - market_conditions: Market volatility and volume data
"""

from decimal import Decimal
from typing import Dict, List, Tuple, Optional, Any
from datetime import datetime, timedelta
import pandas as pd
import numpy as np
from .core import EconomicsBase, ValidationError, CalculationError, DataError


class CurrencyAnalyzer(EconomicsBase):
    """Main currency analysis coordinator"""

    def __init__(self, precision: int = 8, base_currency: str = 'USD'):
        super().__init__(precision, base_currency)
        self.spot_forward = SpotForwardAnalyzer(precision, base_currency)
        self.arbitrage = ArbitrageDetector(precision, base_currency)
        self.parity = ParityAnalyzer(precision, base_currency)
        self.carry_trade = CarryTradeAnalyzer(precision, base_currency)

    def calculate(self, analysis_type: str, **kwargs) -> Dict[str, Any]:
        """Route calculation to appropriate analyzer"""
        analyzers = {
            'spot_forward': self.spot_forward.calculate,
            'arbitrage': self.arbitrage.calculate,
            'parity': self.parity.calculate,
            'carry_trade': self.carry_trade.calculate
        }

        if analysis_type not in analyzers:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")

        return analyzers[analysis_type](**kwargs)


class SpotForwardAnalyzer(EconomicsBase):
    """Spot and forward rate analysis with bid-offer spreads"""

    def calculate_bid_offer_spread(self, bid: Decimal, ask: Decimal) -> Dict[str, Decimal]:
        """Calculate bid-offer spread metrics"""
        self.validator.validate_bid_ask_spread(bid, ask)

        spread_points = ask - bid
        spread_percentage = (spread_points / bid) * self.to_decimal(100)
        mid_rate = (bid + ask) / self.to_decimal(2)

        return {
            'bid': bid,
            'ask': ask,
            'mid_rate': mid_rate,
            'spread_points': spread_points,
            'spread_percentage': spread_percentage,
            'spread_basis_points': spread_percentage * self.to_decimal(100)
        }

    def factors_affecting_spread(self, currency_pair: str, market_conditions: Dict[str, Any]) -> Dict[str, str]:
        """Analyze factors affecting bid-offer spread"""
        factors = {
            'trading_volume': 'Higher volume = narrower spreads',
            'market_volatility': 'Higher volatility = wider spreads',
            'time_of_day': 'Active trading hours = narrower spreads',
            'liquidity': 'More liquid pairs = narrower spreads',
            'political_stability': 'Less stable = wider spreads',
            'central_bank_intervention': 'Active intervention = wider spreads'
        }

        assessment = {}
        volume = market_conditions.get('daily_volume', 0)
        volatility = market_conditions.get('volatility', 0)

        if volume > 1000000:  # High volume
            assessment['volume_impact'] = 'Narrow spread expected'
        else:
            assessment['volume_impact'] = 'Wide spread expected'

        if volatility > 0.02:  # High volatility (2%+)
            assessment['volatility_impact'] = 'Wide spread expected'
        else:
            assessment['volatility_impact'] = 'Narrow spread expected'

        assessment.update(factors)
        return assessment

    def calculate_forward_premium_discount(self, spot_rate: Decimal, forward_rate: Decimal,
                                           time_to_maturity: Decimal) -> Dict[str, Decimal]:
        """Calculate forward premium/discount"""
        self.validator.validate_exchange_rate(spot_rate)
        self.validator.validate_exchange_rate(forward_rate)
        self.validator.validate_time_period(time_to_maturity)

        # Annualized premium/discount
        premium_discount = ((forward_rate - spot_rate) / spot_rate) * (self.to_decimal(1) / time_to_maturity)
        premium_discount_percent = premium_discount * self.to_decimal(100)

        return {
            'spot_rate': spot_rate,
            'forward_rate': forward_rate,
            'time_to_maturity': time_to_maturity,
            'premium_discount': premium_discount,
            'premium_discount_percent': premium_discount_percent,
            'is_premium': forward_rate > spot_rate,
            'annualized_rate': premium_discount_percent
        }

    def mark_to_market_forward(self, contract_details: Dict[str, Any], current_market_data: Dict[str, Any]) -> Dict[
        str, Decimal]:
        """Calculate mark-to-market value of forward contract"""
        # Contract details
        notional = self.to_decimal(contract_details['notional_amount'])
        contract_rate = self.to_decimal(contract_details['contract_rate'])
        maturity = self.to_decimal(contract_details['time_to_maturity'])
        position = contract_details['position']  # 'long' or 'short'

        # Current market data
        current_forward = self.to_decimal(current_market_data['current_forward_rate'])
        risk_free_rate = self.to_decimal(current_market_data['risk_free_rate'])

        # Calculate present value of the difference
        rate_difference = current_forward - contract_rate
        if position == 'short':
            rate_difference = -rate_difference

        # Present value of gain/loss
        pv_factor = self.to_decimal(1) / ((self.to_decimal(1) + risk_free_rate) ** maturity)
        mtm_value = notional * rate_difference * pv_factor

        return {
            'mtm_value': mtm_value,
            'notional_amount': notional,
            'contract_rate': contract_rate,
            'current_forward_rate': current_forward,
            'rate_difference': rate_difference,
            'position': position,
            'pv_factor': pv_factor,
            'unrealized_pnl': mtm_value
        }

    def calculate(self, calculation_type: str, **kwargs) -> Dict[str, Any]:
        """Main calculation dispatcher"""
        calculations = {
            'bid_offer_spread': lambda: self.calculate_bid_offer_spread(
                self.to_decimal(kwargs['bid']), self.to_decimal(kwargs['ask'])
            ),
            'forward_premium_discount': lambda: self.calculate_forward_premium_discount(
                self.to_decimal(kwargs['spot_rate']),
                self.to_decimal(kwargs['forward_rate']),
                self.to_decimal(kwargs['time_to_maturity'])
            ),
            'mark_to_market': lambda: self.mark_to_market_forward(
                kwargs['contract_details'], kwargs['current_market_data']
            ),
            'spread_factors': lambda: self.factors_affecting_spread(
                kwargs['currency_pair'], kwargs['market_conditions']
            )
        }

        if calculation_type not in calculations:
            raise ValidationError(f"Unknown calculation type: {calculation_type}")

        result = calculations[calculation_type]()
        result['metadata'] = self.get_metadata()
        result['calculation_type'] = calculation_type

        return result


class ArbitrageDetector(EconomicsBase):
    """Triangular arbitrage detection and profit calculation"""

    def detect_triangular_arbitrage(self, currency_quotes: Dict[str, Dict[str, Decimal]],
                                    base_currency: str = None) -> Dict[str, Any]:
        """Detect triangular arbitrage opportunities"""
        base = base_currency or self.base_currency

        opportunities = []
        currencies = list(currency_quotes.keys())

        # Check all possible triangular combinations
        for i, curr1 in enumerate(currencies):
            for j, curr2 in enumerate(currencies[i + 1:], i + 1):
                for k, curr3 in enumerate(currencies[j + 1:], j + 1):
                    # Check triangular arbitrage for this combination
                    opportunity = self._check_triangle(curr1, curr2, curr3, currency_quotes)
                    if opportunity['arbitrage_exists']:
                        opportunities.append(opportunity)

        return {
            'opportunities': opportunities,
            'total_opportunities': len(opportunities),
            'base_currency': base,
            'quotes_analyzed': len(currencies),
            'timestamp': datetime.now().isoformat()
        }

    def _check_triangle(self, curr1: str, curr2: str, curr3: str,
                        quotes: Dict[str, Dict[str, Decimal]]) -> Dict[str, Any]:
        """Check specific triangular arbitrage opportunity"""
        try:
            # Get bid/ask rates for all pairs
            pair1 = f"{curr1}/{curr2}"
            pair2 = f"{curr2}/{curr3}"
            pair3 = f"{curr3}/{curr1}"

            # Forward path: curr1 -> curr2 -> curr3 -> curr1
            if pair1 in quotes and pair2 in quotes and pair3 in quotes:
                rate1 = quotes[pair1]['ask']  # Buy curr2 with curr1
                rate2 = quotes[pair2]['ask']  # Buy curr3 with curr2
                rate3 = quotes[pair3]['bid']  # Sell curr1 for curr3

                forward_result = rate1 * rate2 * rate3

                # Reverse path: curr1 -> curr3 -> curr2 -> curr1
                rate1_rev = quotes[pair3]['ask']  # Buy curr3 with curr1
                rate2_rev = self.to_decimal(1) / quotes[pair2]['bid']  # Buy curr2 with curr3
                rate3_rev = self.to_decimal(1) / quotes[pair1]['bid']  # Buy curr1 with curr2

                reverse_result = rate1_rev * rate2_rev * rate3_rev

                # Check for arbitrage
                arbitrage_forward = forward_result > self.to_decimal(1)
                arbitrage_reverse = reverse_result > self.to_decimal(1)

                if arbitrage_forward or arbitrage_reverse:
                    best_path = 'forward' if forward_result > reverse_result else 'reverse'
                    profit_factor = max(forward_result, reverse_result)

                    return {
                        'currencies': [curr1, curr2, curr3],
                        'arbitrage_exists': True,
                        'best_path': best_path,
                        'profit_factor': profit_factor,
                        'profit_percentage': (profit_factor - self.to_decimal(1)) * self.to_decimal(100),
                        'forward_result': forward_result,
                        'reverse_result': reverse_result
                    }

            return {
                'currencies': [curr1, curr2, curr3],
                'arbitrage_exists': False,
                'profit_factor': self.to_decimal(0),
                'profit_percentage': self.to_decimal(0)
            }

        except Exception as e:
            raise CalculationError(f"Error calculating triangular arbitrage: {e}")

    def calculate_arbitrage_profit(self, opportunity: Dict[str, Any],
                                   investment_amount: Decimal) -> Dict[str, Decimal]:
        """Calculate profit from arbitrage opportunity"""
        if not opportunity['arbitrage_exists']:
            raise ValidationError("No arbitrage opportunity exists")

        profit_factor = opportunity['profit_factor']
        gross_profit = investment_amount * (profit_factor - self.to_decimal(1))

        # Estimate transaction costs (typical 0.1% per transaction)
        transaction_cost_rate = self.to_decimal(0.001)
        num_transactions = self.to_decimal(3)  # Three currency exchanges
        transaction_costs = investment_amount * transaction_cost_rate * num_transactions

        net_profit = gross_profit - transaction_costs
        net_profit_percentage = (net_profit / investment_amount) * self.to_decimal(100)

        return {
            'investment_amount': investment_amount,
            'gross_profit': gross_profit,
            'transaction_costs': transaction_costs,
            'net_profit': net_profit,
            'net_profit_percentage': net_profit_percentage,
            'profit_factor': profit_factor,
            'viable': net_profit > self.to_decimal(0)
        }

    def calculate(self, **kwargs) -> Dict[str, Any]:
        """Main arbitrage calculation"""
        if 'currency_quotes' in kwargs:
            return self.detect_triangular_arbitrage(
                kwargs['currency_quotes'],
                kwargs.get('base_currency')
            )
        elif 'opportunity' in kwargs and 'investment_amount' in kwargs:
            return self.calculate_arbitrage_profit(
                kwargs['opportunity'],
                self.to_decimal(kwargs['investment_amount'])
            )
        else:
            raise ValidationError("Missing required parameters for arbitrage calculation")


class ParityAnalyzer(EconomicsBase):
    """International parity conditions analysis"""

    def covered_interest_rate_parity(self, spot_rate: Decimal, forward_rate: Decimal,
                                     domestic_rate: Decimal, foreign_rate: Decimal,
                                     time_period: Decimal) -> Dict[str, Any]:
        """Test covered interest rate parity condition"""
        # CIP: F/S = (1 + r_domestic * t) / (1 + r_foreign * t)
        theoretical_forward = spot_rate * (
                (self.to_decimal(1) + domestic_rate * time_period) /
                (self.to_decimal(1) + foreign_rate * time_period)
        )

        deviation = forward_rate - theoretical_forward
        deviation_percentage = (deviation / theoretical_forward) * self.to_decimal(100)

        # Arbitrage opportunity exists if deviation > transaction costs
        arbitrage_threshold = self.to_decimal(0.1)  # 0.1%
        arbitrage_opportunity = abs(deviation_percentage) > arbitrage_threshold

        return {
            'spot_rate': spot_rate,
            'forward_rate': forward_rate,
            'theoretical_forward': theoretical_forward,
            'deviation': deviation,
            'deviation_percentage': deviation_percentage,
            'parity_holds': abs(deviation_percentage) < self.to_decimal(0.05),
            'arbitrage_opportunity': arbitrage_opportunity,
            'domestic_rate': domestic_rate,
            'foreign_rate': foreign_rate,
            'time_period': time_period
        }

    def uncovered_interest_rate_parity(self, spot_rate: Decimal, expected_spot: Decimal,
                                       domestic_rate: Decimal, foreign_rate: Decimal,
                                       time_period: Decimal) -> Dict[str, Any]:
        """Test uncovered interest rate parity condition"""
        # UIP: E(S_t+1)/S_t = (1 + r_domestic * t) / (1 + r_foreign * t)
        theoretical_expected = spot_rate * (
                (self.to_decimal(1) + domestic_rate * time_period) /
                (self.to_decimal(1) + foreign_rate * time_period)
        )

        deviation = expected_spot - theoretical_expected
        deviation_percentage = (deviation / theoretical_expected) * self.to_decimal(100)

        return {
            'spot_rate': spot_rate,
            'expected_spot': expected_spot,
            'theoretical_expected': theoretical_expected,
            'deviation': deviation,
            'deviation_percentage': deviation_percentage,
            'parity_holds': abs(deviation_percentage) < self.to_decimal(5),
            'risk_premium': deviation_percentage
        }

    def purchasing_power_parity(self, spot_rate: Decimal, domestic_inflation: Decimal,
                                foreign_inflation: Decimal, time_period: Decimal) -> Dict[str, Any]:
        """Test purchasing power parity"""
        # PPP: S_t+1/S_t = (1 + π_domestic) / (1 + π_foreign)
        theoretical_rate = spot_rate * (
                (self.to_decimal(1) + domestic_inflation * time_period) /
                (self.to_decimal(1) + foreign_inflation * time_period)
        )

        return {
            'current_spot': spot_rate,
            'theoretical_rate': theoretical_rate,
            'domestic_inflation': domestic_inflation,
            'foreign_inflation': foreign_inflation,
            'inflation_differential': domestic_inflation - foreign_inflation,
            'time_period': time_period,
            'expected_change_percentage': ((theoretical_rate - spot_rate) / spot_rate) * self.to_decimal(100)
        }

    def international_fisher_effect(self, domestic_nominal: Decimal, foreign_nominal: Decimal,
                                    domestic_real: Decimal, foreign_real: Decimal) -> Dict[str, Any]:
        """Test International Fisher Effect"""
        # IFE: (1 + r_nominal_domestic) / (1 + r_nominal_foreign) = (1 + r_real_domestic) / (1 + r_real_foreign)
        nominal_ratio = (self.to_decimal(1) + domestic_nominal) / (self.to_decimal(1) + foreign_nominal)
        real_ratio = (self.to_decimal(1) + domestic_real) / (self.to_decimal(1) + foreign_real)

        deviation = nominal_ratio - real_ratio
        deviation_percentage = (deviation / real_ratio) * self.to_decimal(100)

        return {
            'domestic_nominal_rate': domestic_nominal,
            'foreign_nominal_rate': foreign_nominal,
            'domestic_real_rate': domestic_real,
            'foreign_real_rate': foreign_real,
            'nominal_ratio': nominal_ratio,
            'real_ratio': real_ratio,
            'deviation': deviation,
            'deviation_percentage': deviation_percentage,
            'fisher_effect_holds': abs(deviation_percentage) < self.to_decimal(1)
        }

    def calculate(self, parity_type: str, **kwargs) -> Dict[str, Any]:
        """Calculate specific parity condition"""
        parity_functions = {
            'covered_interest_parity': self.covered_interest_rate_parity,
            'uncovered_interest_parity': self.uncovered_interest_rate_parity,
            'purchasing_power_parity': self.purchasing_power_parity,
            'international_fisher_effect': self.international_fisher_effect
        }

        if parity_type not in parity_functions:
            raise ValidationError(f"Unknown parity type: {parity_type}")

        # Convert all numeric inputs to Decimal
        decimal_kwargs = {}
        for key, value in kwargs.items():
            if isinstance(value, (int, float, str)) and key != 'parity_type':
                try:
                    decimal_kwargs[key] = self.to_decimal(value)
                except:
                    decimal_kwargs[key] = value
            else:
                decimal_kwargs[key] = value

        result = parity_functions[parity_type](**decimal_kwargs)
        result['metadata'] = self.get_metadata()
        result['parity_type'] = parity_type

        return result


class CarryTradeAnalyzer(EconomicsBase):
    """Carry trade analysis and profit calculations"""

    def calculate_carry_trade_return(self, funding_currency_rate: Decimal,
                                     target_currency_rate: Decimal,
                                     exchange_rate_change: Decimal,
                                     time_period: Decimal,
                                     leverage: Decimal = None) -> Dict[str, Any]:
        """Calculate carry trade returns"""
        leverage = leverage or self.to_decimal(1)

        # Interest rate differential
        rate_differential = target_currency_rate - funding_currency_rate

        # Interest income (annualized)
        interest_income = rate_differential * time_period

        # Exchange rate return
        fx_return = exchange_rate_change

        # Total return before leverage
        total_return = interest_income + fx_return

        # Leveraged return
        leveraged_return = total_return * leverage

        # Risk assessment
        sharpe_ratio = self._calculate_carry_trade_sharpe(
            rate_differential, exchange_rate_change, time_period
        )

        return {
            'funding_rate': funding_currency_rate,
            'target_rate': target_currency_rate,
            'rate_differential': rate_differential,
            'interest_income': interest_income,
            'fx_return': fx_return,
            'total_return': total_return,
            'leverage': leverage,
            'leveraged_return': leveraged_return,
            'annualized_return': leveraged_return / time_period,
            'time_period': time_period,
            'sharpe_ratio': sharpe_ratio,
            'risk_level': self._assess_carry_trade_risk(rate_differential, leverage)
        }

    def _calculate_carry_trade_sharpe(self, rate_diff: Decimal, fx_change: Decimal,
                                      time_period: Decimal) -> Decimal:
        """Simplified Sharpe ratio calculation for carry trade"""
        # Assume historical volatility of 10% for FX
        assumed_volatility = self.to_decimal(0.10)

        expected_return = rate_diff * time_period
        risk_free_rate = self.to_decimal(0.02)  # Assume 2% risk-free rate

        excess_return = expected_return - (risk_free_rate * time_period)
        sharpe = excess_return / (assumed_volatility * (time_period ** self.to_decimal(0.5)))

        return sharpe

    def _assess_carry_trade_risk(self, rate_differential: Decimal, leverage: Decimal) -> str:
        """Assess carry trade risk level"""
        risk_score = abs(rate_differential) * leverage

        if risk_score < self.to_decimal(0.02):
            return "Low"
        elif risk_score < self.to_decimal(0.05):
            return "Medium"
        else:
            return "High"

    def carry_trade_uip_violation(self, rate_differential: Decimal,
                                  actual_fx_change: Decimal,
                                  time_period: Decimal) -> Dict[str, Any]:
        """Analyze carry trade in context of UIP violation"""
        # Under UIP, exchange rate should change to offset interest differential
        uip_predicted_change = -rate_differential * time_period

        # Actual violation
        uip_violation = actual_fx_change - uip_predicted_change

        # Carry trade profit from UIP violation
        carry_profit = rate_differential * time_period + uip_violation

        return {
            'rate_differential': rate_differential,
            'uip_predicted_fx_change': uip_predicted_change,
            'actual_fx_change': actual_fx_change,
            'uip_violation': uip_violation,
            'carry_trade_profit': carry_profit,
            'uip_violation_percentage': (uip_violation / abs(uip_predicted_change)) * self.to_decimal(
                100) if uip_predicted_change != 0 else self.to_decimal(0),
            'profitable': carry_profit > self.to_decimal(0)
        }

    def calculate(self, calculation_type: str = 'return', **kwargs) -> Dict[str, Any]:
        """Main carry trade calculation"""
        if calculation_type == 'return':
            return self.calculate_carry_trade_return(
                self.to_decimal(kwargs['funding_rate']),
                self.to_decimal(kwargs['target_rate']),
                self.to_decimal(kwargs['fx_change']),
                self.to_decimal(kwargs['time_period']),
                self.to_decimal(kwargs.get('leverage', 1))
            )
        elif calculation_type == 'uip_violation':
            return self.carry_trade_uip_violation(
                self.to_decimal(kwargs['rate_differential']),
                self.to_decimal(kwargs['actual_fx_change']),
                self.to_decimal(kwargs['time_period'])
            )
        else:
            raise ValidationError(f"Unknown calculation type: {calculation_type}")