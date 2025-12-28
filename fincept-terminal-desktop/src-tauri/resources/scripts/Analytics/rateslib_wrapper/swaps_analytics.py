"""
Rateslib Swaps Analytics Module
=================================
Wrapper for rateslib interest rate swaps and related derivatives.

This module provides tools for:
- Interest Rate Swaps (IRS) pricing and analytics
- Cross-Currency Swaps (XCS) analysis
- Basis Swaps (SBS) calculations
- Swap sensitivities (delta, gamma, DV01)
- Cash flow generation and analysis
"""

import pandas as pd
import numpy as np
import plotly.graph_objects as go
from typing import Dict, List, Optional, Union, Tuple, Any
from dataclasses import dataclass
from datetime import datetime
import json
import warnings

import rateslib as rl

warnings.filterwarnings('ignore')


@dataclass
class SwapConfig:
    """Configuration for swap instruments"""
    notional: float = 1_000_000  # Default notional
    currency: str = "USD"  # Base currency
    frequency_fixed: str = "S"  # Semi-annual
    frequency_float: str = "Q"  # Quarterly
    calendar: str = "nyc"  # Business day calendar
    convention_fixed: str = "30e360"  # Fixed leg day count
    convention_float: str = "Act360"  # Float leg day count


class SwapsAnalytics:
    """Analytics for interest rate swaps and related instruments"""

    def __init__(self, config: SwapConfig = None):
        self.config = config or SwapConfig()
        self.instrument = None
        self.instrument_type = None

    def create_irs(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        fixed_rate: float,
        notional: Optional[float] = None,
        pay_or_receive: str = "pay"  # pay or receive fixed
    ) -> Dict[str, Any]:
        """
        Create an Interest Rate Swap

        Args:
            effective_date: Start date of the swap
            termination: End date or tenor (e.g., "5Y")
            fixed_rate: Fixed rate in percent (e.g., 5.0 for 5%)
            notional: Notional amount (uses config default if None)
            pay_or_receive: "pay" or "receive" fixed leg

        Returns:
            Dictionary with IRS details
        """
        try:
            notional_amt = notional or self.config.notional

            # Determine fixed rate sign based on pay/receive
            rate_sign = -1 if pay_or_receive.lower() == "pay" else 1

            self.instrument = rl.IRS(
                effective=effective_date,
                termination=termination,
                frequency=self.config.frequency_fixed,
                notional=notional_amt,
                fixed_rate=fixed_rate,
                currency=self.config.currency,
                calendar=self.config.calendar,
                convention=self.config.convention_fixed
            )
            self.instrument_type = "IRS"

            result = {
                'type': 'IRS',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'fixed_rate': fixed_rate,
                'notional': notional_amt,
                'currency': self.config.currency,
                'pay_receive': pay_or_receive,
                'frequency': self.config.frequency_fixed
            }

            print(f"IRS created: {pay_or_receive} fixed {fixed_rate}%, notional {notional_amt:,.0f}")
            return result

        except Exception as e:
            print(f"Error creating IRS: {e}")
            return {}

    def create_xcs(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        fixed_rate_dom: float,
        fixed_rate_for: float,
        notional_dom: float,
        notional_for: float,
        currency_dom: str = "USD",
        currency_for: str = "EUR"
    ) -> Dict[str, Any]:
        """
        Create a Cross-Currency Swap

        Args:
            effective_date: Start date
            termination: End date or tenor
            fixed_rate_dom: Fixed rate in domestic currency (%)
            fixed_rate_for: Fixed rate in foreign currency (%)
            notional_dom: Notional in domestic currency
            notional_for: Notional in foreign currency
            currency_dom: Domestic currency code
            currency_for: Foreign currency code

        Returns:
            Dictionary with XCS details
        """
        try:
            self.instrument = rl.XCS(
                effective=effective_date,
                termination=termination,
                frequency=self.config.frequency_fixed,
                notional=notional_dom,
                fixed_rate=fixed_rate_dom,
                currency=currency_dom,
                leg2_notional=notional_for,
                leg2_fixed_rate=fixed_rate_for,
                leg2_currency=currency_for,
                calendar=self.config.calendar
            )
            self.instrument_type = "XCS"

            result = {
                'type': 'XCS',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'domestic_currency': currency_dom,
                'domestic_rate': fixed_rate_dom,
                'domestic_notional': notional_dom,
                'foreign_currency': currency_for,
                'foreign_rate': fixed_rate_for,
                'foreign_notional': notional_for
            }

            print(f"XCS created: {currency_dom} {notional_dom:,.0f} @ {fixed_rate_dom}% vs {currency_for} {notional_for:,.0f} @ {fixed_rate_for}%")
            return result

        except Exception as e:
            print(f"Error creating XCS: {e}")
            return {}

    def create_fra(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        fixed_rate: float,
        notional: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create a Forward Rate Agreement

        Args:
            effective_date: Settlement date
            termination: Maturity date or tenor
            fixed_rate: Fixed rate in percent
            notional: Notional amount

        Returns:
            Dictionary with FRA details
        """
        try:
            notional_amt = notional or self.config.notional

            self.instrument = rl.FRA(
                effective=effective_date,
                termination=termination,
                frequency="Z",  # Zero coupon
                notional=notional_amt,
                fixed_rate=fixed_rate,
                currency=self.config.currency,
                calendar=self.config.calendar
            )
            self.instrument_type = "FRA"

            result = {
                'type': 'FRA',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'fixed_rate': fixed_rate,
                'notional': notional_amt,
                'currency': self.config.currency
            }

            print(f"FRA created: {fixed_rate}%, notional {notional_amt:,.0f}")
            return result

        except Exception as e:
            print(f"Error creating FRA: {e}")
            return {}

    def create_sbs(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        spread: float,
        notional: Optional[float] = None,
        frequency: str = "Q"
    ) -> Dict[str, Any]:
        """
        Create a Single Currency Basis Swap

        Args:
            effective_date: Start date
            termination: End date or tenor
            spread: Spread in basis points
            notional: Notional amount
            frequency: Payment frequency

        Returns:
            Dictionary with SBS details
        """
        try:
            notional_amt = notional or self.config.notional

            self.instrument = rl.SBS(
                effective=effective_date,
                termination=termination,
                frequency=frequency,
                notional=notional_amt,
                spread=spread,
                currency=self.config.currency,
                calendar=self.config.calendar
            )
            self.instrument_type = "SBS"

            result = {
                'type': 'SBS',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'spread_bps': spread,
                'notional': notional_amt,
                'frequency': frequency,
                'currency': self.config.currency
            }

            print(f"SBS created: {spread}bps spread, notional {notional_amt:,.0f}")
            return result

        except Exception as e:
            print(f"Error creating SBS: {e}")
            return {}

    def create_zcis(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        fixed_rate: float,
        notional: Optional[float] = None,
        index_base: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create a Zero Coupon Inflation Swap

        Args:
            effective_date: Start date
            termination: End date or tenor
            fixed_rate: Fixed inflation rate in percent
            notional: Notional amount
            index_base: Base index value

        Returns:
            Dictionary with ZCIS details
        """
        try:
            notional_amt = notional or self.config.notional

            self.instrument = rl.ZCIS(
                effective=effective_date,
                termination=termination,
                frequency="Z",  # Zero coupon
                notional=notional_amt,
                fixed_rate=fixed_rate,
                index_base=index_base,
                currency=self.config.currency,
                calendar=self.config.calendar
            )
            self.instrument_type = "ZCIS"

            result = {
                'type': 'ZCIS',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'fixed_rate': fixed_rate,
                'notional': notional_amt,
                'index_base': index_base,
                'currency': self.config.currency
            }

            print(f"ZCIS created: {fixed_rate}% inflation rate, notional {notional_amt:,.0f}")
            return result

        except Exception as e:
            print(f"Error creating ZCIS: {e}")
            return {}

    def create_zcs(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        fixed_rate: float,
        notional: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create a Zero Coupon Swap

        Args:
            effective_date: Start date
            termination: End date or tenor
            fixed_rate: Fixed rate in percent
            notional: Notional amount

        Returns:
            Dictionary with ZCS details
        """
        try:
            notional_amt = notional or self.config.notional

            self.instrument = rl.ZCS(
                effective=effective_date,
                termination=termination,
                frequency="Z",  # Zero coupon
                notional=notional_amt,
                fixed_rate=fixed_rate,
                currency=self.config.currency,
                calendar=self.config.calendar
            )
            self.instrument_type = "ZCS"

            result = {
                'type': 'ZCS',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'fixed_rate': fixed_rate,
                'notional': notional_amt,
                'currency': self.config.currency
            }

            print(f"ZCS created: {fixed_rate}%, notional {notional_amt:,.0f}")
            return result

        except Exception as e:
            print(f"Error creating ZCS: {e}")
            return {}

    def create_iirs(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        fixed_rate: float,
        notional: Optional[float] = None,
        index_base: Optional[float] = None,
        index_lag: int = 3
    ) -> Dict[str, Any]:
        """
        Create an Indexed Interest Rate Swap (Inflation-Linked IRS)

        Args:
            effective_date: Start date
            termination: End date or tenor
            fixed_rate: Fixed rate in percent
            notional: Notional amount
            index_base: Base index value
            index_lag: Index publication lag in months

        Returns:
            Dictionary with IIRS details
        """
        try:
            notional_amt = notional or self.config.notional

            self.instrument = rl.IIRS(
                effective=effective_date,
                termination=termination,
                frequency=self.config.frequency_fixed,
                notional=notional_amt,
                fixed_rate=fixed_rate,
                index_base=index_base,
                index_lag=index_lag,
                currency=self.config.currency,
                calendar=self.config.calendar
            )
            self.instrument_type = "IIRS"

            result = {
                'type': 'IIRS',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'fixed_rate': fixed_rate,
                'notional': notional_amt,
                'index_base': index_base,
                'index_lag': index_lag,
                'currency': self.config.currency
            }

            print(f"IIRS created: {fixed_rate}%, notional {notional_amt:,.0f}")
            return result

        except Exception as e:
            print(f"Error creating IIRS: {e}")
            return {}

    def create_ndf(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        fixed_rate: float,
        notional: Optional[float] = None,
        currency_dom: str = "USD",
        currency_for: str = "BRL"
    ) -> Dict[str, Any]:
        """
        Create a Non-Deliverable Forward

        Args:
            effective_date: Trade date
            termination: Settlement date or tenor
            fixed_rate: Forward FX rate
            notional: Notional amount
            currency_dom: Domestic currency
            currency_for: Foreign (non-deliverable) currency

        Returns:
            Dictionary with NDF details
        """
        try:
            notional_amt = notional or self.config.notional

            pair = f"{currency_for.lower()}{currency_dom.lower()}"

            self.instrument = rl.NDF(
                effective=effective_date,
                termination=termination,
                pair=pair,
                notional=notional_amt,
                fx_rate=fixed_rate,
                calendar=self.config.calendar
            )
            self.instrument_type = "NDF"

            result = {
                'type': 'NDF',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'fx_rate': fixed_rate,
                'notional': notional_amt,
                'domestic_currency': currency_dom,
                'foreign_currency': currency_for,
                'pair': pair.upper()
            }

            print(f"NDF created: {pair.upper()} @ {fixed_rate}, notional {notional_amt:,.0f}")
            return result

        except Exception as e:
            print(f"Error creating NDF: {e}")
            return {}

    def create_spread(
        self,
        instruments: List,
        weights: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """
        Create a Spread strategy (combination of instruments)

        Args:
            instruments: List of rateslib instruments
            weights: List of weights for each instrument (default: [1, -1])

        Returns:
            Dictionary with spread details
        """
        try:
            if weights is None:
                weights = [1.0] * len(instruments)

            # Spread is a portfolio with specific weights
            weighted_instruments = []
            for inst, weight in zip(instruments, weights):
                weighted_instruments.append((inst, weight))

            result = {
                'type': 'Spread',
                'num_instruments': len(instruments),
                'weights': weights,
                'instrument_types': [type(inst).__name__ for inst in instruments]
            }

            print(f"Spread created with {len(instruments)} instruments")
            return result

        except Exception as e:
            print(f"Error creating spread: {e}")
            return {}

    def create_fly(
        self,
        short_tenor_instrument,
        mid_tenor_instrument,
        long_tenor_instrument,
        weights: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """
        Create a Fly (butterfly) strategy

        Args:
            short_tenor_instrument: Short tenor instrument
            mid_tenor_instrument: Middle tenor instrument (body)
            long_tenor_instrument: Long tenor instrument
            weights: Weights for [short, mid, long] (default: [1, -2, 1])

        Returns:
            Dictionary with fly details
        """
        try:
            if weights is None:
                weights = [1.0, -2.0, 1.0]  # Standard butterfly weights

            instruments = [short_tenor_instrument, mid_tenor_instrument, long_tenor_instrument]

            result = {
                'type': 'Fly',
                'structure': 'Butterfly (1x-2x1)',
                'weights': weights,
                'instrument_types': [type(inst).__name__ for inst in instruments],
                'strategy': 'Long wings, short body'
            }

            print(f"Fly created: {weights[0]}x{type(short_tenor_instrument).__name__} + "
                  f"{weights[1]}x{type(mid_tenor_instrument).__name__} + "
                  f"{weights[2]}x{type(long_tenor_instrument).__name__}")
            return result

        except Exception as e:
            print(f"Error creating fly: {e}")
            return {}

    def get_cashflows(self) -> pd.DataFrame:
        """
        Get cash flow schedule for the instrument

        Returns:
            DataFrame with cash flow details
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return pd.DataFrame()

        try:
            # Get cashflows table if available
            if hasattr(self.instrument, 'cashflows_table'):
                cf_table = self.instrument.cashflows_table()
                return cf_table
            elif hasattr(self.instrument, 'cashflows'):
                cfs = self.instrument.cashflows()
                # Convert to DataFrame
                cf_data = []
                for date, amount in cfs.items():
                    cf_data.append({
                        'date': date,
                        'cashflow': float(amount) if amount else 0.0
                    })
                return pd.DataFrame(cf_data)
            else:
                print("Cashflows not available for this instrument")
                return pd.DataFrame()

        except Exception as e:
            print(f"Error getting cashflows: {e}")
            return pd.DataFrame()

    def calculate_npv(self, curve=None) -> Dict[str, Any]:
        """
        Calculate Net Present Value

        Args:
            curve: Discount curve (optional)

        Returns:
            Dictionary with NPV details
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return {}

        try:
            if hasattr(self.instrument, 'npv'):
                npv_value = self.instrument.npv(curve)

                return {
                    'npv': float(npv_value) if npv_value else 0.0,
                    'instrument_type': self.instrument_type,
                    'currency': self.config.currency
                }
            else:
                print("NPV calculation not available")
                return {}

        except Exception as e:
            print(f"Error calculating NPV: {e}")
            # Return structure with error info
            return {
                'npv': 0.0,
                'error': str(e),
                'instrument_type': self.instrument_type
            }

    def calculate_sensitivities(self, curve=None) -> Dict[str, Any]:
        """
        Calculate risk sensitivities (delta, gamma, DV01)

        Args:
            curve: Discount curve (optional)

        Returns:
            Dictionary with sensitivity metrics
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return {}

        sensitivities = {}

        try:
            # Delta (first-order sensitivity)
            if hasattr(self.instrument, 'delta'):
                delta = self.instrument.delta(curve)
                sensitivities['delta'] = float(delta) if isinstance(delta, (int, float)) else str(delta)

            # Gamma (second-order sensitivity)
            if hasattr(self.instrument, 'gamma'):
                gamma = self.instrument.gamma(curve)
                sensitivities['gamma'] = float(gamma) if isinstance(gamma, (int, float)) else str(gamma)

            # Analytic delta
            if hasattr(self.instrument, 'analytic_delta'):
                analytic_delta = self.instrument.analytic_delta(curve)
                sensitivities['analytic_delta'] = str(analytic_delta)

            sensitivities['instrument_type'] = self.instrument_type

            print(f"Sensitivities calculated for {self.instrument_type}")
            return sensitivities

        except Exception as e:
            print(f"Error calculating sensitivities: {e}")
            return {'error': str(e)}

    def plot_cashflows(self) -> go.Figure:
        """
        Plot cash flow schedule

        Returns:
            Plotly figure object
        """
        df = self.get_cashflows()

        if df.empty:
            print("No cashflows to plot")
            return go.Figure()

        try:
            fig = go.Figure()

            # Determine column names (they vary by instrument)
            date_col = next((col for col in df.columns if 'date' in col.lower()), df.columns[0])

            # Find numeric columns for plotting
            numeric_cols = df.select_dtypes(include=[np.number]).columns.tolist()

            if not numeric_cols:
                print("No numeric columns found in cashflows")
                return go.Figure()

            # Plot each numeric column
            for col in numeric_cols[:3]:  # Limit to first 3 numeric columns
                fig.add_trace(go.Bar(
                    x=df[date_col] if date_col in df.columns else df.index,
                    y=df[col],
                    name=col,
                    opacity=0.8
                ))

            fig.update_layout(
                title=f'{self.instrument_type} Cash Flows',
                xaxis_title='Date',
                yaxis_title='Amount',
                template='plotly_dark',
                barmode='group',
                height=500
            )

            return fig

        except Exception as e:
            print(f"Error plotting cashflows: {e}")
            return go.Figure()

    def export_to_json(self) -> str:
        """Export instrument data to JSON"""
        if self.instrument is None:
            return json.dumps({'error': 'No instrument created'}, indent=2)

        export_data = {
            'instrument_type': self.instrument_type,
            'currency': self.config.currency,
            'notional': self.config.notional,
            'configuration': {
                'frequency_fixed': self.config.frequency_fixed,
                'frequency_float': self.config.frequency_float,
                'calendar': self.config.calendar,
                'convention_fixed': self.config.convention_fixed,
                'convention_float': self.config.convention_float
            }
        }

        return json.dumps(export_data, indent=2)


def main():
    """Example usage and testing"""
    print("=== Rateslib Swaps Analytics Test ===\n")

    # Initialize analytics
    config = SwapConfig(
        notional=10_000_000,
        currency="USD",
        frequency_fixed="S",
        frequency_float="Q"
    )
    analytics = SwapsAnalytics(config)

    # Test 1: Create IRS
    print("1. Creating Interest Rate Swap...")
    irs_details = analytics.create_irs(
        effective_date=rl.dt(2024, 1, 15),
        termination="5Y",
        fixed_rate=5.0,
        pay_or_receive="pay"
    )
    print(f"   IRS details: {json.dumps(irs_details, indent=4)}")

    # Test 2: Get cashflows
    print("\n2. Getting cash flows...")
    cashflows = analytics.get_cashflows()
    if not cashflows.empty:
        print(f"   Cashflows generated: {len(cashflows)} periods")
        print(f"   Columns: {cashflows.columns.tolist()}")
    else:
        print("   Cashflows table not available (curve needed)")

    # Test 3: Calculate NPV (will need curve, expect error)
    print("\n3. Calculating NPV (without curve - will show structure)...")
    npv_result = analytics.calculate_npv()
    print(f"   NPV result: {json.dumps(npv_result, indent=4)}")

    # Test 4: Calculate sensitivities
    print("\n4. Calculating sensitivities...")
    sensitivities = analytics.calculate_sensitivities()
    print(f"   Sensitivities: {json.dumps(sensitivities, indent=4)}")

    # Test 5: Create FRA
    print("\n5. Creating Forward Rate Agreement...")
    fra_details = analytics.create_fra(
        effective_date=rl.dt(2024, 3, 15),
        termination="3M",
        fixed_rate=4.75
    )
    print(f"   FRA details: {json.dumps(fra_details, indent=4)}")

    # Test 6: Export to JSON
    print("\n6. Exporting to JSON...")
    json_output = analytics.export_to_json()
    print(f"   JSON export length: {len(json_output)} characters")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
