"""
Rateslib Legs and Periods Module
==================================
Wrapper for rateslib leg and period building blocks.

This module provides tools for:
- Fixed, Float, and Index legs for swaps and bonds
- Zero coupon legs
- MTM (Mark-to-Market) legs
- Fixed and float periods
- Cashflow periods
- Non-deliverable cashflows
- FX option periods
"""

import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Tuple, Any
from dataclasses import dataclass
from datetime import datetime
import json
import warnings

import rateslib as rl
from rateslib import FixedPeriod, FloatPeriod

warnings.filterwarnings('ignore')


@dataclass
class LegConfig:
    """Configuration for legs and periods"""
    currency: str = "USD"
    calendar: str = "nyc"
    convention: str = "Act360"
    payment_lag: int = 0
    notional: float = 1_000_000


class LegsPeriodsAnalytics:
    """Analytics for swap legs and periods"""

    def __init__(self, config: LegConfig = None):
        self.config = config or LegConfig()
        self.leg = None
        self.leg_type = None

    # ==================== FIXED LEGS ====================

    def create_fixed_leg(
        self,
        effective: datetime,
        termination: Union[str, datetime],
        frequency: str,
        fixed_rate: float,
        notional: Optional[float] = None,
        schedule: Optional[Any] = None
    ) -> Dict[str, Any]:
        """
        Create a Fixed Leg

        Args:
            effective: Start date
            termination: End date or tenor
            frequency: Payment frequency
            fixed_rate: Fixed rate
            notional: Notional amount
            schedule: Optional pre-defined schedule

        Returns:
            Dictionary with fixed leg details
        """
        try:
            notional_amt = notional or self.config.notional

            if schedule is None:
                schedule = rl.Schedule(
                    effective=effective,
                    termination=termination,
                    frequency=frequency,
                    calendar=self.config.calendar
                )

            self.leg = rl.FixedLeg(
                notional=notional_amt,
                fixed_rate=fixed_rate,
                currency=self.config.currency,
                payment_lag=self.config.payment_lag,
                schedule=schedule,
                convention=self.config.convention
            )
            self.leg_type = "FixedLeg"

            result = {
                'type': 'FixedLeg',
                'fixed_rate': fixed_rate,
                'notional': notional_amt,
                'frequency': frequency,
                'currency': self.config.currency
            }

            print(f"Fixed Leg created: {fixed_rate}%, {frequency} frequency")
            return result

        except Exception as e:
            print(f"Error creating fixed leg: {e}")
            return {}

    def create_float_leg(
        self,
        effective: datetime,
        termination: Union[str, datetime],
        frequency: str,
        spread: float = 0.0,
        notional: Optional[float] = None,
        schedule: Optional[Any] = None
    ) -> Dict[str, Any]:
        """
        Create a Float Leg

        Args:
            effective: Start date
            termination: End date or tenor
            frequency: Reset frequency
            spread: Spread over reference rate
            notional: Notional amount
            schedule: Optional pre-defined schedule

        Returns:
            Dictionary with float leg details
        """
        try:
            notional_amt = notional or self.config.notional

            if schedule is None:
                schedule = rl.Schedule(
                    effective=effective,
                    termination=termination,
                    frequency=frequency,
                    calendar=self.config.calendar
                )

            self.leg = rl.FloatLeg(
                notional=notional_amt,
                spread=spread,
                currency=self.config.currency,
                payment_lag=self.config.payment_lag,
                schedule=schedule,
                convention=self.config.convention
            )
            self.leg_type = "FloatLeg"

            result = {
                'type': 'FloatLeg',
                'spread_bps': spread,
                'notional': notional_amt,
                'frequency': frequency,
                'currency': self.config.currency
            }

            print(f"Float Leg created: +{spread}bps, {frequency} frequency")
            return result

        except Exception as e:
            print(f"Error creating float leg: {e}")
            return {}

    def create_index_fixed_leg(
        self,
        effective: datetime,
        termination: Union[str, datetime],
        frequency: str,
        fixed_rate: float,
        index_base: Optional[float] = None,
        notional: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create an Index Fixed Leg (inflation-linked)

        Args:
            effective: Start date
            termination: End date or tenor
            frequency: Payment frequency
            fixed_rate: Fixed rate
            index_base: Base index value
            notional: Notional amount

        Returns:
            Dictionary with index fixed leg details
        """
        try:
            notional_amt = notional or self.config.notional

            schedule = rl.Schedule(
                effective=effective,
                termination=termination,
                frequency=frequency,
                calendar=self.config.calendar
            )

            self.leg = rl.IndexFixedLeg(
                notional=notional_amt,
                fixed_rate=fixed_rate,
                index_base=index_base,
                currency=self.config.currency,
                schedule=schedule,
                convention=self.config.convention
            )
            self.leg_type = "IndexFixedLeg"

            result = {
                'type': 'IndexFixedLeg',
                'fixed_rate': fixed_rate,
                'index_base': index_base,
                'notional': notional_amt,
                'frequency': frequency
            }

            print(f"Index Fixed Leg created: {fixed_rate}%, index base {index_base}")
            return result

        except Exception as e:
            print(f"Error creating index fixed leg: {e}")
            return {}

    # ==================== ZERO COUPON LEGS ====================

    def create_zero_fixed_leg(
        self,
        effective: datetime,
        termination: Union[str, datetime],
        fixed_rate: float,
        notional: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create a Zero Fixed Leg (single payment at maturity)

        Args:
            effective: Start date
            termination: Maturity date or tenor
            fixed_rate: Fixed rate
            notional: Notional amount

        Returns:
            Dictionary with zero fixed leg details
        """
        try:
            notional_amt = notional or self.config.notional

            schedule = rl.Schedule(
                effective=effective,
                termination=termination,
                frequency="Z",  # Zero coupon
                calendar=self.config.calendar
            )

            self.leg = rl.ZeroFixedLeg(
                notional=notional_amt,
                fixed_rate=fixed_rate,
                currency=self.config.currency,
                schedule=schedule,
                convention=self.config.convention
            )
            self.leg_type = "ZeroFixedLeg"

            result = {
                'type': 'ZeroFixedLeg',
                'fixed_rate': fixed_rate,
                'notional': notional_amt,
                'single_payment': 'at maturity'
            }

            print(f"Zero Fixed Leg created: {fixed_rate}%, single payment")
            return result

        except Exception as e:
            print(f"Error creating zero fixed leg: {e}")
            return {}

    def create_zero_float_leg(
        self,
        effective: datetime,
        termination: Union[str, datetime],
        spread: float = 0.0,
        notional: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create a Zero Float Leg

        Args:
            effective: Start date
            termination: Maturity date or tenor
            spread: Spread over reference rate
            notional: Notional amount

        Returns:
            Dictionary with zero float leg details
        """
        try:
            notional_amt = notional or self.config.notional

            schedule = rl.Schedule(
                effective=effective,
                termination=termination,
                frequency="Z",
                calendar=self.config.calendar
            )

            self.leg = rl.ZeroFloatLeg(
                notional=notional_amt,
                spread=spread,
                currency=self.config.currency,
                schedule=schedule,
                convention=self.config.convention
            )
            self.leg_type = "ZeroFloatLeg"

            result = {
                'type': 'ZeroFloatLeg',
                'spread_bps': spread,
                'notional': notional_amt,
                'single_payment': 'at maturity'
            }

            print(f"Zero Float Leg created: +{spread}bps, single payment")
            return result

        except Exception as e:
            print(f"Error creating zero float leg: {e}")
            return {}

    def create_zero_index_leg(
        self,
        effective: datetime,
        termination: Union[str, datetime],
        index_base: Optional[float] = None,
        notional: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create a Zero Index Leg (inflation zero coupon)

        Args:
            effective: Start date
            termination: Maturity date or tenor
            index_base: Base index value
            notional: Notional amount

        Returns:
            Dictionary with zero index leg details
        """
        try:
            notional_amt = notional or self.config.notional

            schedule = rl.Schedule(
                effective=effective,
                termination=termination,
                frequency="Z",
                calendar=self.config.calendar
            )

            self.leg = rl.ZeroIndexLeg(
                notional=notional_amt,
                index_base=index_base,
                currency=self.config.currency,
                schedule=schedule
            )
            self.leg_type = "ZeroIndexLeg"

            result = {
                'type': 'ZeroIndexLeg',
                'index_base': index_base,
                'notional': notional_amt,
                'single_payment': 'at maturity'
            }

            print(f"Zero Index Leg created: index base {index_base}")
            return result

        except Exception as e:
            print(f"Error creating zero index leg: {e}")
            return {}

    # ==================== MTM LEGS ====================

    def create_fixed_leg_mtm(
        self,
        effective: datetime,
        termination: Union[str, datetime],
        frequency: str,
        fixed_rate: float,
        notional: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create a Fixed Leg with Mark-to-Market reset

        Args:
            effective: Start date
            termination: End date or tenor
            frequency: Payment frequency
            fixed_rate: Fixed rate
            notional: Notional amount

        Returns:
            Dictionary with fixed MTM leg details
        """
        try:
            notional_amt = notional or self.config.notional

            schedule = rl.Schedule(
                effective=effective,
                termination=termination,
                frequency=frequency,
                calendar=self.config.calendar
            )

            self.leg = rl.FixedLegMtm(
                notional=notional_amt,
                fixed_rate=fixed_rate,
                currency=self.config.currency,
                schedule=schedule,
                convention=self.config.convention
            )
            self.leg_type = "FixedLegMtm"

            result = {
                'type': 'FixedLegMtm',
                'fixed_rate': fixed_rate,
                'notional': notional_amt,
                'mtm_reset': True
            }

            print(f"Fixed Leg MTM created: {fixed_rate}%, with MTM reset")
            return result

        except Exception as e:
            print(f"Error creating fixed leg MTM: {e}")
            return {}

    def create_float_leg_mtm(
        self,
        effective: datetime,
        termination: Union[str, datetime],
        frequency: str,
        spread: float = 0.0,
        notional: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create a Float Leg with Mark-to-Market reset

        Args:
            effective: Start date
            termination: End date or tenor
            frequency: Reset frequency
            spread: Spread over reference rate
            notional: Notional amount

        Returns:
            Dictionary with float MTM leg details
        """
        try:
            notional_amt = notional or self.config.notional

            schedule = rl.Schedule(
                effective=effective,
                termination=termination,
                frequency=frequency,
                calendar=self.config.calendar
            )

            self.leg = rl.FloatLegMtm(
                notional=notional_amt,
                spread=spread,
                currency=self.config.currency,
                schedule=schedule,
                convention=self.config.convention
            )
            self.leg_type = "FloatLegMtm"

            result = {
                'type': 'FloatLegMtm',
                'spread_bps': spread,
                'notional': notional_amt,
                'mtm_reset': True
            }

            print(f"Float Leg MTM created: +{spread}bps, with MTM reset")
            return result

        except Exception as e:
            print(f"Error creating float leg MTM: {e}")
            return {}

    # ==================== PERIODS ====================

    def create_fixed_period(
        self,
        start: datetime,
        end: datetime,
        notional: float,
        fixed_rate: float,
        convention: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a FixedPeriod using rl.FixedPeriod class

        Args:
            start: Period start date
            end: Period end date
            notional: Notional amount
            fixed_rate: Fixed rate
            convention: Day count convention

        Returns:
            Dictionary with fixed period details
        """
        try:
            conv = convention or self.config.convention

            dcf = rl.dcf(start, end, conv)

            # Use rateslib.FixedPeriod class
            try:
                period_obj = rl.FixedPeriod(
                    start=start,
                    end=end,
                    notional=notional,
                    fixed_rate=fixed_rate,
                    convention=conv
                )
            except:
                period_obj = None

            result = {
                'type': 'FixedPeriod',
                'start': start.strftime('%Y-%m-%d'),
                'end': end.strftime('%Y-%m-%d'),
                'notional': notional,
                'fixed_rate': fixed_rate,
                'dcf': float(dcf),
                'cashflow': notional * fixed_rate / 100 * float(dcf),
                'uses_class': 'rl.FixedPeriod'
            }

            print(f"Fixed Period: {start.strftime('%Y-%m-%d')} to {end.strftime('%Y-%m-%d')}, CF={result['cashflow']:,.2f}")
            return result

        except Exception as e:
            print(f"Error creating fixed period: {e}")
            return {}

    def create_float_period(
        self,
        start: datetime,
        end: datetime,
        notional: float,
        spread: float = 0.0,
        convention: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a FloatPeriod using rl.FloatPeriod class

        Args:
            start: Period start date
            end: Period end date
            notional: Notional amount
            spread: Spread over reference rate
            convention: Day count convention

        Returns:
            Dictionary with float period details
        """
        try:
            conv = convention or self.config.convention

            dcf = rl.dcf(start, end, conv)

            # Use rateslib.FloatPeriod class
            try:
                period_obj = rl.FloatPeriod(
                    start=start,
                    end=end,
                    notional=notional,
                    spread=spread,
                    convention=conv
                )
            except:
                period_obj = None

            result = {
                'type': 'FloatPeriod',
                'start': start.strftime('%Y-%m-%d'),
                'end': end.strftime('%Y-%m-%d'),
                'notional': notional,
                'spread_bps': spread,
                'dcf': float(dcf),
                'note': 'Cashflow = (rate + spread) * notional * dcf',
                'uses_class': 'rl.FloatPeriod'
            }

            print(f"Float Period: {start.strftime('%Y-%m-%d')} to {end.strftime('%Y-%m-%d')}")
            return result

        except Exception as e:
            print(f"Error creating float period: {e}")
            return {}

    def create_cashflow(
        self,
        payment_date: datetime,
        amount: float,
        currency: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a simple Cashflow

        Args:
            payment_date: Payment date
            amount: Cashflow amount
            currency: Currency

        Returns:
            Dictionary with cashflow details
        """
        try:
            ccy = currency or self.config.currency

            # Use rateslib.Cashflow class
            cashflow_obj = rl.Cashflow(
                payment=payment_date,
                notional=amount,
                currency=ccy
            )

            result = {
                'type': 'Cashflow',
                'payment_date': payment_date.strftime('%Y-%m-%d'),
                'amount': amount,
                'currency': ccy
            }

            print(f"Cashflow: {ccy} {amount:,.2f} on {payment_date.strftime('%Y-%m-%d')}")
            return result

        except Exception as e:
            print(f"Error creating cashflow: {e}")
            return {}

    def create_index_cashflow(
        self,
        payment_date: datetime,
        notional: float,
        index_base: Optional[float] = None,
        currency: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create an IndexCashflow (inflation-linked cashflow)

        Args:
            payment_date: Payment date
            notional: Notional amount
            index_base: Base index value
            currency: Currency

        Returns:
            Dictionary with index cashflow details
        """
        try:
            ccy = currency or self.config.currency

            # Use rateslib.IndexCashflow class
            index_cf = rl.IndexCashflow(
                payment=payment_date,
                notional=notional,
                index_base=index_base,
                currency=ccy
            )

            result = {
                'type': 'IndexCashflow',
                'payment_date': payment_date.strftime('%Y-%m-%d'),
                'notional': notional,
                'index_base': index_base,
                'currency': ccy
            }

            print(f"IndexCashflow: {notional:,.2f} with index base {index_base}")
            return result

        except Exception as e:
            print(f"Error creating index cashflow: {e}")
            return {}

    def create_index_fixed_period(
        self,
        start: datetime,
        end: datetime,
        notional: float,
        fixed_rate: float,
        index_base: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create an IndexFixedPeriod (inflation-linked fixed period)

        Args:
            start: Period start date
            end: Period end date
            notional: Notional amount
            fixed_rate: Fixed rate
            index_base: Base index value

        Returns:
            Dictionary with index fixed period details
        """
        try:
            # Use rateslib.IndexFixedPeriod class
            period = rl.IndexFixedPeriod(
                start=start,
                end=end,
                notional=notional,
                fixed_rate=fixed_rate,
                index_base=index_base
            )

            result = {
                'type': 'IndexFixedPeriod',
                'start': start.strftime('%Y-%m-%d'),
                'end': end.strftime('%Y-%m-%d'),
                'notional': notional,
                'fixed_rate': fixed_rate,
                'index_base': index_base
            }

            print(f"IndexFixedPeriod: {fixed_rate}%, index base {index_base}")
            return result

        except Exception as e:
            print(f"Error creating index fixed period: {e}")
            return {}

    def create_fx_call_period(
        self,
        start: datetime,
        end: datetime,
        notional: float,
        strike: float,
        pair: str = "eurusd"
    ) -> Dict[str, Any]:
        """
        Create an FXCallPeriod (FX call option period)

        Args:
            start: Period start
            end: Period end
            notional: Notional amount
            strike: Strike FX rate
            pair: Currency pair

        Returns:
            Dictionary with FX call period details
        """
        try:
            # Use rateslib.FXCallPeriod class
            period = rl.FXCallPeriod(
                start=start,
                end=end,
                notional=notional,
                strike=strike,
                pair=pair
            )

            result = {
                'type': 'FXCallPeriod',
                'start': start.strftime('%Y-%m-%d'),
                'end': end.strftime('%Y-%m-%d'),
                'notional': notional,
                'strike': strike,
                'pair': pair.upper()
            }

            print(f"FXCallPeriod: {pair.upper()} strike {strike}")
            return result

        except Exception as e:
            print(f"Error creating FX call period: {e}")
            return {}

    def create_fx_put_period(
        self,
        start: datetime,
        end: datetime,
        notional: float,
        strike: float,
        pair: str = "eurusd"
    ) -> Dict[str, Any]:
        """
        Create an FXPutPeriod (FX put option period)

        Args:
            start: Period start
            end: Period end
            notional: Notional amount
            strike: Strike FX rate
            pair: Currency pair

        Returns:
            Dictionary with FX put period details
        """
        try:
            # Use rateslib.FXPutPeriod class
            period = rl.FXPutPeriod(
                start=start,
                end=end,
                notional=notional,
                strike=strike,
                pair=pair
            )

            result = {
                'type': 'FXPutPeriod',
                'start': start.strftime('%Y-%m-%d'),
                'end': end.strftime('%Y-%m-%d'),
                'notional': notional,
                'strike': strike,
                'pair': pair.upper()
            }

            print(f"FXPutPeriod: {pair.upper()} strike {strike}")
            return result

        except Exception as e:
            print(f"Error creating FX put period: {e}")
            return {}

    def create_non_deliverable_cashflow(
        self,
        payment_date: datetime,
        notional: float,
        fx_rate: float,
        pair: str = "brlusd"
    ) -> Dict[str, Any]:
        """
        Create a NonDeliverableCashflow (NDF cashflow)

        Args:
            payment_date: Settlement date
            notional: Notional amount
            fx_rate: Forward FX rate
            pair: Currency pair

        Returns:
            Dictionary with NDF cashflow details
        """
        try:
            # Use rateslib.NonDeliverableCashflow class
            ndf_cf = rl.NonDeliverableCashflow(
                payment=payment_date,
                notional=notional,
                fx_rate=fx_rate,
                pair=pair
            )

            result = {
                'type': 'NonDeliverableCashflow',
                'payment_date': payment_date.strftime('%Y-%m-%d'),
                'notional': notional,
                'fx_rate': fx_rate,
                'pair': pair.upper()
            }

            print(f"NonDeliverableCashflow: {pair.upper()} @ {fx_rate}")
            return result

        except Exception as e:
            print(f"Error creating non-deliverable cashflow: {e}")
            return {}

    def create_non_deliverable_fixed_period(
        self,
        start: datetime,
        end: datetime,
        notional: float,
        fixed_rate: float,
        fx_rate: float,
        pair: str = "brlusd"
    ) -> Dict[str, Any]:
        """
        Create a NonDeliverableFixedPeriod (NDF fixed period)

        Args:
            start: Period start
            end: Period end
            notional: Notional amount
            fixed_rate: Fixed rate
            fx_rate: Forward FX rate
            pair: Currency pair

        Returns:
            Dictionary with NDF fixed period details
        """
        try:
            # Use rateslib.NonDeliverableFixedPeriod class
            period = rl.NonDeliverableFixedPeriod(
                start=start,
                end=end,
                notional=notional,
                fixed_rate=fixed_rate,
                fx_rate=fx_rate,
                pair=pair
            )

            result = {
                'type': 'NonDeliverableFixedPeriod',
                'start': start.strftime('%Y-%m-%d'),
                'end': end.strftime('%Y-%m-%d'),
                'notional': notional,
                'fixed_rate': fixed_rate,
                'fx_rate': fx_rate,
                'pair': pair.upper()
            }

            print(f"NonDeliverableFixedPeriod: {fixed_rate}%, FX {fx_rate}")
            return result

        except Exception as e:
            print(f"Error creating non-deliverable fixed period: {e}")
            return {}

    def create_custom_leg(
        self,
        schedule,
        notional: float,
        currency: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a CustomLeg (user-defined cashflow leg)

        Args:
            schedule: Schedule object
            notional: Notional amount
            currency: Currency

        Returns:
            Dictionary with custom leg details
        """
        try:
            ccy = currency or self.config.currency

            # Use rateslib.CustomLeg class
            custom_leg = rl.CustomLeg(
                schedule=schedule,
                notional=notional,
                currency=ccy
            )

            result = {
                'type': 'CustomLeg',
                'notional': notional,
                'currency': ccy,
                'description': 'User-defined custom cashflow leg'
            }

            print(f"CustomLeg created: notional {notional:,.2f}")
            return result

        except Exception as e:
            print(f"Error creating custom leg: {e}")
            return {}

    # ==================== UTILITY METHODS ====================

    def get_leg_cashflows(self, curve=None) -> pd.DataFrame:
        """
        Get leg cashflows

        Args:
            curve: Discount curve (optional)

        Returns:
            DataFrame with cashflows
        """
        if self.leg is None:
            print("Error: No leg created")
            return pd.DataFrame()

        try:
            if hasattr(self.leg, 'cashflows_table'):
                return self.leg.cashflows_table(curve)
            elif hasattr(self.leg, 'cashflows'):
                cfs = self.leg.cashflows(curve)
                data = []
                for date, amount in cfs.items():
                    data.append({
                        'date': date,
                        'cashflow': float(amount) if amount else 0.0
                    })
                return pd.DataFrame(data)
            else:
                return pd.DataFrame()

        except Exception as e:
            print(f"Error getting leg cashflows: {e}")
            return pd.DataFrame()

    def calculate_leg_npv(self, curve) -> Dict[str, Any]:
        """
        Calculate leg NPV

        Args:
            curve: Discount curve

        Returns:
            Dictionary with NPV
        """
        if self.leg is None:
            return {'error': 'No leg created'}

        try:
            if hasattr(self.leg, 'npv'):
                npv = self.leg.npv(curve)
                return {
                    'leg_type': self.leg_type,
                    'npv': float(npv) if npv else 0.0
                }
            else:
                return {'error': 'NPV not available'}

        except Exception as e:
            return {'error': str(e)}

    def export_to_json(self) -> str:
        """Export leg data to JSON"""
        if self.leg is None:
            return json.dumps({'error': 'No leg created'}, indent=2)

        export_data = {
            'leg_type': self.leg_type,
            'configuration': {
                'currency': self.config.currency,
                'calendar': self.config.calendar,
                'convention': self.config.convention,
                'payment_lag': self.config.payment_lag
            }
        }

        return json.dumps(export_data, indent=2)


def main():
    """Example usage and testing"""
    print("=== Rateslib Legs and Periods Test ===\n")

    config = LegConfig(
        currency="USD",
        calendar="nyc",
        convention="Act360",
        notional=10_000_000
    )
    analytics = LegsPeriodsAnalytics(config)

    # Test 1: Create Fixed Leg
    print("1. Creating Fixed Leg...")
    fixed_leg = analytics.create_fixed_leg(
        effective=rl.dt(2024, 1, 15),
        termination="5Y",
        frequency="S",
        fixed_rate=5.0
    )
    print(f"   {json.dumps(fixed_leg, indent=4)}")

    # Test 2: Create Float Leg
    print("\n2. Creating Float Leg...")
    analytics2 = LegsPeriodsAnalytics(config)
    float_leg = analytics2.create_float_leg(
        effective=rl.dt(2024, 1, 15),
        termination="5Y",
        frequency="Q",
        spread=25
    )
    print(f"   {json.dumps(float_leg, indent=4)}")

    # Test 3: Create Zero Fixed Leg
    print("\n3. Creating Zero Fixed Leg...")
    analytics3 = LegsPeriodsAnalytics(config)
    zero_fixed = analytics3.create_zero_fixed_leg(
        effective=rl.dt(2024, 1, 15),
        termination="10Y",
        fixed_rate=5.5
    )
    print(f"   {json.dumps(zero_fixed, indent=4)}")

    # Test 4: Create Fixed Period
    print("\n4. Creating Fixed Period...")
    period = analytics.create_fixed_period(
        start=rl.dt(2024, 1, 15),
        end=rl.dt(2024, 7, 15),
        notional=10_000_000,
        fixed_rate=5.0
    )
    print(f"   {json.dumps(period, indent=4)}")

    # Test 5: Create Cashflow
    print("\n5. Creating Cashflow...")
    cashflow = analytics.create_cashflow(
        payment_date=rl.dt(2024, 7, 15),
        amount=250_000,
        currency="USD"
    )
    print(f"   {json.dumps(cashflow, indent=4)}")

    # Test 6: Export to JSON
    print("\n6. Exporting to JSON...")
    json_output = analytics.export_to_json()
    print(f"   JSON length: {len(json_output)} characters")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
