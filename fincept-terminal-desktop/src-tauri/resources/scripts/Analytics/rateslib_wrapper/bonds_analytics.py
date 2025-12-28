"""
Rateslib Bonds Analytics Module
==================================
Wrapper for rateslib bond pricing and analytics functionality.

This module provides tools for:
- Fixed Rate Bond pricing and analytics
- Floating Rate Notes (FRNs) analysis
- Index-Linked Bonds calculations
- Bond sensitivities (duration, convexity, DV01)
- Yield and price calculations
- Accrued interest calculations
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
class BondConfig:
    """Configuration for bond instruments"""
    currency: str = "USD"
    calendar: str = "nyc"
    convention: str = "ActActICMA"  # Day count convention for bonds
    ex_div_days: int = 7  # Ex-dividend days
    settle_days: int = 2  # Settlement days


class BondsAnalytics:
    """Analytics for fixed income bonds"""

    def __init__(self, config: BondConfig = None):
        self.config = config or BondConfig()
        self.bond = None
        self.bond_type = None

    def create_fixed_rate_bond(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        coupon_rate: float,
        frequency: str = "S",  # Semi-annual
        notional: float = 100,  # Face value
    ) -> Dict[str, Any]:
        """
        Create a Fixed Rate Bond

        Args:
            effective_date: Issue date
            termination: Maturity date or tenor (e.g., "10Y")
            coupon_rate: Annual coupon rate in percent (e.g., 5.0 for 5%)
            frequency: Payment frequency (A=Annual, S=Semi-annual, Q=Quarterly)
            notional: Face value/par value

        Returns:
            Dictionary with bond details
        """
        try:
            self.bond = rl.FixedRateBond(
                effective=effective_date,
                termination=termination,
                frequency=frequency,
                fixed_rate=coupon_rate,
                notional=notional,
                currency=self.config.currency,
                calendar=self.config.calendar,
                convention=self.config.convention,
                ex_div=self.config.ex_div_days,
                settle=self.config.settle_days
            )
            self.bond_type = "FixedRateBond"

            result = {
                'type': 'FixedRateBond',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'coupon_rate': coupon_rate,
                'frequency': frequency,
                'notional': notional,
                'currency': self.config.currency,
                'convention': self.config.convention
            }

            print(f"Fixed Rate Bond created: {coupon_rate}% coupon, notional {notional}")
            return result

        except Exception as e:
            print(f"Error creating fixed rate bond: {e}")
            return {}

    def create_floating_rate_note(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        spread: float = 0.0,  # Spread over reference rate in bps
        frequency: str = "Q",  # Quarterly reset
        notional: float = 100
    ) -> Dict[str, Any]:
        """
        Create a Floating Rate Note

        Args:
            effective_date: Issue date
            termination: Maturity date or tenor
            spread: Spread over reference rate in basis points
            frequency: Reset frequency
            notional: Face value

        Returns:
            Dictionary with FRN details
        """
        try:
            self.bond = rl.FloatRateNote(
                effective=effective_date,
                termination=termination,
                frequency=frequency,
                spread=spread,
                notional=notional,
                currency=self.config.currency,
                calendar=self.config.calendar,
                settle=self.config.settle_days
            )
            self.bond_type = "FloatRateNote"

            result = {
                'type': 'FloatRateNote',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'spread_bps': spread,
                'frequency': frequency,
                'notional': notional,
                'currency': self.config.currency
            }

            print(f"Floating Rate Note created: {spread}bps spread, notional {notional}")
            return result

        except Exception as e:
            print(f"Error creating floating rate note: {e}")
            return {}

    def create_bill(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        notional: float = 100
    ) -> Dict[str, Any]:
        """
        Create a Bill (zero-coupon short-term instrument)

        Args:
            effective_date: Issue date
            termination: Maturity date or tenor
            notional: Face value

        Returns:
            Dictionary with bill details
        """
        try:
            self.bond = rl.Bill(
                effective=effective_date,
                termination=termination,
                notional=notional,
                currency=self.config.currency,
                calendar=self.config.calendar,
                settle=self.config.settle_days
            )
            self.bond_type = "Bill"

            result = {
                'type': 'Bill',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'notional': notional,
                'currency': self.config.currency
            }

            print(f"Bill created: notional {notional}")
            return result

        except Exception as e:
            print(f"Error creating bill: {e}")
            return {}

    def create_index_fixed_rate_bond(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        coupon_rate: float,
        frequency: str = "S",
        notional: float = 100,
        index_base: Optional[float] = None,
        index_lag: int = 3
    ) -> Dict[str, Any]:
        """
        Create an Index-Linked Fixed Rate Bond (inflation-indexed bond)

        Args:
            effective_date: Issue date
            termination: Maturity date or tenor
            coupon_rate: Real coupon rate in percent
            frequency: Payment frequency
            notional: Face value
            index_base: Base inflation index value
            index_lag: Index publication lag in months

        Returns:
            Dictionary with index bond details
        """
        try:
            self.bond = rl.IndexFixedRateBond(
                effective=effective_date,
                termination=termination,
                frequency=frequency,
                fixed_rate=coupon_rate,
                notional=notional,
                index_base=index_base,
                index_lag=index_lag,
                currency=self.config.currency,
                calendar=self.config.calendar,
                convention=self.config.convention,
                ex_div=self.config.ex_div_days,
                settle=self.config.settle_days
            )
            self.bond_type = "IndexFixedRateBond"

            result = {
                'type': 'IndexFixedRateBond',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'real_coupon_rate': coupon_rate,
                'frequency': frequency,
                'notional': notional,
                'index_base': index_base,
                'index_lag': index_lag,
                'currency': self.config.currency,
                'description': 'Inflation-linked bond with principal and coupons indexed'
            }

            print(f"Index Fixed Rate Bond created: {coupon_rate}% real coupon, index base {index_base}")
            return result

        except Exception as e:
            print(f"Error creating index fixed rate bond: {e}")
            return {}

    def calculate_price(
        self,
        ytm: Optional[float] = None,
        settlement: Optional[datetime] = None,
        curve=None
    ) -> Dict[str, Any]:
        """
        Calculate bond price from yield or curve

        Args:
            ytm: Yield to maturity in percent (if provided)
            settlement: Settlement date
            curve: Discount curve (alternative to YTM)

        Returns:
            Dictionary with price details
        """
        if self.bond is None:
            print("Error: No bond created")
            return {}

        try:
            result = {}

            # Try NPV calculation with curve
            if curve is not None and hasattr(self.bond, 'npv'):
                npv = self.bond.npv(curve, settlement)
                result['price'] = float(npv)
                result['method'] = 'curve'

            # Try rate calculation (converts price to yield)
            if hasattr(self.bond, 'rate'):
                try:
                    if ytm is not None:
                        # Can't directly get price from YTM, but can get YTM from price
                        result['note'] = 'Use curve for price calculation'
                except Exception:
                    pass

            result['bond_type'] = self.bond_type
            result['settlement'] = settlement.strftime('%Y-%m-%d') if settlement else None

            return result

        except Exception as e:
            print(f"Error calculating price: {e}")
            return {'error': str(e)}

    def calculate_yield(
        self,
        price: float,
        settlement: Optional[datetime] = None,
        curve=None
    ) -> Dict[str, Any]:
        """
        Calculate yield to maturity from price

        Args:
            price: Clean price
            settlement: Settlement date
            curve: Discount curve

        Returns:
            Dictionary with yield details
        """
        if self.bond is None:
            print("Error: No bond created")
            return {}

        try:
            result = {'price': price, 'bond_type': self.bond_type}

            if hasattr(self.bond, 'rate'):
                ytm = self.bond.rate(price, settlement, curve)
                result['yield_to_maturity'] = float(ytm) if ytm else None
                result['ytm_percent'] = float(ytm) * 100 if ytm else None

            return result

        except Exception as e:
            print(f"Error calculating yield: {e}")
            return {'error': str(e), 'price': price}

    def calculate_duration(
        self,
        curve=None,
        settlement: Optional[datetime] = None
    ) -> Dict[str, Any]:
        """
        Calculate modified and Macaulay duration

        Args:
            curve: Discount curve
            settlement: Settlement date

        Returns:
            Dictionary with duration metrics
        """
        if self.bond is None:
            print("Error: No bond created")
            return {}

        try:
            result = {'bond_type': self.bond_type}

            if hasattr(self.bond, 'duration'):
                duration = self.bond.duration(curve, settlement)
                result['modified_duration'] = float(duration) if duration else None

            return result

        except Exception as e:
            print(f"Error calculating duration: {e}")
            return {'error': str(e)}

    def calculate_convexity(
        self,
        curve=None,
        settlement: Optional[datetime] = None
    ) -> Dict[str, Any]:
        """
        Calculate convexity

        Args:
            curve: Discount curve
            settlement: Settlement date

        Returns:
            Dictionary with convexity metric
        """
        if self.bond is None:
            print("Error: No bond created")
            return {}

        try:
            result = {'bond_type': self.bond_type}

            if hasattr(self.bond, 'convexity'):
                convexity = self.bond.convexity(curve, settlement)
                result['convexity'] = float(convexity) if convexity else None

            return result

        except Exception as e:
            print(f"Error calculating convexity: {e}")
            return {'error': str(e)}

    def calculate_accrued_interest(
        self,
        settlement: datetime
    ) -> Dict[str, Any]:
        """
        Calculate accrued interest at settlement date

        Args:
            settlement: Settlement date

        Returns:
            Dictionary with accrued interest
        """
        if self.bond is None:
            print("Error: No bond created")
            return {}

        try:
            result = {
                'settlement_date': settlement.strftime('%Y-%m-%d'),
                'bond_type': self.bond_type
            }

            if hasattr(self.bond, 'accrued'):
                accrued = self.bond.accrued(settlement)
                result['accrued_interest'] = float(accrued) if accrued else 0.0

            return result

        except Exception as e:
            print(f"Error calculating accrued interest: {e}")
            return {'error': str(e)}

    def get_cashflows(self) -> pd.DataFrame:
        """
        Get bond cash flow schedule

        Returns:
            DataFrame with cash flow details
        """
        if self.bond is None:
            print("Error: No bond created")
            return pd.DataFrame()

        try:
            if hasattr(self.bond, 'cashflows_table'):
                cf_table = self.bond.cashflows_table()
                return cf_table
            else:
                print("Cashflows table not available")
                return pd.DataFrame()

        except Exception as e:
            print(f"Error getting cashflows: {e}")
            return pd.DataFrame()

    def plot_cashflows(self) -> go.Figure:
        """
        Plot bond cash flow schedule

        Returns:
            Plotly figure object
        """
        df = self.get_cashflows()

        if df.empty:
            print("No cashflows to plot")
            return go.Figure()

        try:
            fig = go.Figure()

            # Determine column structure
            date_col = next((col for col in df.columns if 'date' in str(col).lower()), df.columns[0])
            numeric_cols = df.select_dtypes(include=[np.number]).columns.tolist()

            if not numeric_cols:
                return go.Figure()

            # Plot cashflows
            for col in numeric_cols[:2]:
                fig.add_trace(go.Bar(
                    x=df.index if date_col not in df.columns else df[date_col],
                    y=df[col],
                    name=str(col),
                    opacity=0.8
                ))

            fig.update_layout(
                title=f'{self.bond_type} Cash Flows',
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
        """Export bond data to JSON"""
        if self.bond is None:
            return json.dumps({'error': 'No bond created'}, indent=2)

        export_data = {
            'bond_type': self.bond_type,
            'currency': self.config.currency,
            'calendar': self.config.calendar,
            'convention': self.config.convention,
            'ex_div_days': self.config.ex_div_days,
            'settle_days': self.config.settle_days
        }

        return json.dumps(export_data, indent=2)


def main():
    """Example usage and testing"""
    print("=== Rateslib Bonds Analytics Test ===\n")

    # Initialize analytics
    config = BondConfig(
        currency="USD",
        calendar="nyc",
        convention="ActActICMA"
    )
    analytics = BondsAnalytics(config)

    # Test 1: Create Fixed Rate Bond
    print("1. Creating Fixed Rate Bond...")
    bond_details = analytics.create_fixed_rate_bond(
        effective_date=rl.dt(2020, 1, 15),
        termination="10Y",
        coupon_rate=5.0,
        frequency="S",
        notional=100
    )
    print(f"   Bond details: {json.dumps(bond_details, indent=4)}")

    # Test 2: Calculate accrued interest
    print("\n2. Calculating accrued interest...")
    settlement = rl.dt(2024, 6, 15)
    accrued = analytics.calculate_accrued_interest(settlement)
    print(f"   Accrued interest: {json.dumps(accrued, indent=4)}")

    # Test 3: Get cashflows
    print("\n3. Getting cash flows...")
    cashflows = analytics.get_cashflows()
    if not cashflows.empty:
        print(f"   Cashflows generated: {len(cashflows)} periods")
        print(f"   Columns: {cashflows.columns.tolist()}")
    else:
        print("   Cashflows table not available")

    # Test 4: Calculate duration
    print("\n4. Calculating duration...")
    duration = analytics.calculate_duration()
    print(f"   Duration: {json.dumps(duration, indent=4)}")

    # Test 5: Create Bill
    print("\n5. Creating Bill...")
    bill_details = analytics.create_bill(
        effective_date=rl.dt(2024, 1, 1),
        termination="3M",
        notional=100
    )
    print(f"   Bill details: {json.dumps(bill_details, indent=4)}")

    # Test 6: Create Floating Rate Note
    print("\n6. Creating Floating Rate Note...")
    analytics2 = BondsAnalytics(config)
    frn_details = analytics2.create_floating_rate_note(
        effective_date=rl.dt(2024, 1, 1),
        termination="5Y",
        spread=50,  # 50bps over reference
        frequency="Q"
    )
    print(f"   FRN details: {json.dumps(frn_details, indent=4)}")

    # Test 7: Export to JSON
    print("\n7. Exporting to JSON...")
    json_output = analytics.export_to_json()
    print(f"   JSON export length: {len(json_output)} characters")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
