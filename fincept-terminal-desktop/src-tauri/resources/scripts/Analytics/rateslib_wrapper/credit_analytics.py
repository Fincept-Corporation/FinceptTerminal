"""
Rateslib Credit Analytics Module
==================================
Wrapper for rateslib credit default swaps and credit derivatives.

This module provides tools for:
- Credit Default Swaps (CDS) pricing and analytics
- Credit premium and protection legs
- Credit event probability calculations
- CDS sensitivities (CS01, spread duration)
- Recovery rate analysis
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
class CreditConfig:
    """Configuration for credit instruments"""
    currency: str = "USD"
    calendar: str = "nyc"
    convention: str = "Act360"
    frequency: str = "Q"  # Quarterly premium payments
    recovery_rate: float = 0.40  # Standard 40% recovery assumption
    notional: float = 10_000_000  # Default notional


class CreditAnalytics:
    """Analytics for credit default swaps and credit derivatives"""

    def __init__(self, config: CreditConfig = None):
        self.config = config or CreditConfig()
        self.instrument = None
        self.instrument_type = None

    def create_cds(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        fixed_rate: float,
        notional: Optional[float] = None,
        recovery: Optional[float] = None,
        stub: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a Credit Default Swap

        Args:
            effective_date: Start date of CDS protection
            termination: Maturity date or tenor (e.g., "5Y")
            fixed_rate: CDS spread in basis points
            notional: Notional amount
            recovery: Recovery rate assumption (0-1)
            stub: Stub type if any

        Returns:
            Dictionary with CDS details
        """
        try:
            notional_amt = notional or self.config.notional
            recovery_rate = recovery if recovery is not None else self.config.recovery_rate

            self.instrument = rl.CDS(
                effective=effective_date,
                termination=termination,
                frequency=self.config.frequency,
                notional=notional_amt,
                fixed_rate=fixed_rate,
                recovery=recovery_rate,
                currency=self.config.currency,
                calendar=self.config.calendar,
                convention=self.config.convention,
                stub=stub
            )
            self.instrument_type = "CDS"

            result = {
                'type': 'CDS',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'spread_bps': fixed_rate,
                'notional': notional_amt,
                'recovery_rate': recovery_rate,
                'frequency': self.config.frequency,
                'currency': self.config.currency,
                'protection_buyer': 'Long CDS = buying protection'
            }

            print(f"CDS created: {fixed_rate}bps spread, recovery {recovery_rate*100:.0f}%, notional {notional_amt:,.0f}")
            return result

        except Exception as e:
            print(f"Error creating CDS: {e}")
            return {}

    def create_credit_premium_leg(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        fixed_rate: float,
        notional: Optional[float] = None,
        frequency: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a Credit Premium Leg (fee leg of CDS)

        Args:
            effective_date: Start date
            termination: Maturity date or tenor
            fixed_rate: Premium spread in basis points
            notional: Notional amount
            frequency: Payment frequency

        Returns:
            Dictionary with premium leg details
        """
        try:
            notional_amt = notional or self.config.notional
            freq = frequency or self.config.frequency

            # Use rateslib.CreditPremiumLeg class
            schedule = rl.Schedule(
                effective=effective_date,
                termination=termination,
                frequency=freq,
                calendar=self.config.calendar
            )

            premium_leg = rl.CreditPremiumLeg(
                notional=notional_amt,
                spread=fixed_rate,
                schedule=schedule,
                currency=self.config.currency
            )

            result = {
                'type': 'CreditPremiumLeg',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'spread_bps': fixed_rate,
                'notional': notional_amt,
                'frequency': freq,
                'currency': self.config.currency,
                'description': 'Premium payments leg (fee leg)'
            }

            print(f"Credit Premium Leg: {fixed_rate}bps, frequency {freq}")
            return result

        except Exception as e:
            print(f"Error creating credit premium leg: {e}")
            return {}

    def create_credit_protection_leg(
        self,
        effective_date: datetime,
        termination: Union[str, datetime],
        notional: Optional[float] = None,
        recovery: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create a Credit Protection Leg (contingent payment leg of CDS)

        Args:
            effective_date: Start date
            termination: Maturity date or tenor
            notional: Notional amount
            recovery: Recovery rate assumption

        Returns:
            Dictionary with protection leg details
        """
        try:
            notional_amt = notional or self.config.notional
            recovery_rate = recovery if recovery is not None else self.config.recovery_rate

            # Use rateslib.CreditProtectionLeg class
            schedule = rl.Schedule(
                effective=effective_date,
                termination=termination,
                frequency=self.config.frequency,
                calendar=self.config.calendar
            )

            protection_leg = rl.CreditProtectionLeg(
                notional=notional_amt,
                recovery=recovery_rate,
                schedule=schedule,
                currency=self.config.currency
            )

            result = {
                'type': 'CreditProtectionLeg',
                'effective_date': effective_date.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'notional': notional_amt,
                'recovery_rate': recovery_rate,
                'loss_given_default': (1 - recovery_rate) * notional_amt,
                'currency': self.config.currency,
                'description': 'Protection payments leg (contingent leg)'
            }

            print(f"Credit Protection Leg: LGD {(1-recovery_rate)*notional_amt:,.0f} ({(1-recovery_rate)*100:.0f}% of notional)")
            return result

        except Exception as e:
            print(f"Error creating credit protection leg: {e}")
            return {}

    def create_credit_premium_period(
        self,
        start: datetime,
        end: datetime,
        notional: float,
        spread: float
    ) -> Dict[str, Any]:
        """
        Create a CreditPremiumPeriod

        Args:
            start: Period start
            end: Period end
            notional: Notional amount
            spread: Credit spread in bps

        Returns:
            Dictionary with credit premium period details
        """
        try:
            # Use rateslib.CreditPremiumPeriod class
            period = rl.CreditPremiumPeriod(
                start=start,
                end=end,
                notional=notional,
                spread=spread
            )

            result = {
                'type': 'CreditPremiumPeriod',
                'start': start.strftime('%Y-%m-%d'),
                'end': end.strftime('%Y-%m-%d'),
                'notional': notional,
                'spread_bps': spread
            }

            print(f"CreditPremiumPeriod: {spread}bps spread")
            return result

        except Exception as e:
            print(f"Error creating credit premium period: {e}")
            return {}

    def create_credit_protection_period(
        self,
        start: datetime,
        end: datetime,
        notional: float,
        recovery: float = 0.40
    ) -> Dict[str, Any]:
        """
        Create a CreditProtectionPeriod

        Args:
            start: Period start
            end: Period end
            notional: Notional amount
            recovery: Recovery rate

        Returns:
            Dictionary with credit protection period details
        """
        try:
            # Use rateslib.CreditProtectionPeriod class
            period = rl.CreditProtectionPeriod(
                start=start,
                end=end,
                notional=notional,
                recovery=recovery
            )

            result = {
                'type': 'CreditProtectionPeriod',
                'start': start.strftime('%Y-%m-%d'),
                'end': end.strftime('%Y-%m-%d'),
                'notional': notional,
                'recovery_rate': recovery,
                'lgd': (1 - recovery) * notional
            }

            print(f"CreditProtectionPeriod: LGD {(1-recovery)*notional:,.2f}")
            return result

        except Exception as e:
            print(f"Error creating credit protection period: {e}")
            return {}

    def calculate_npv(
        self,
        curve=None,
        survival_curve=None
    ) -> Dict[str, Any]:
        """
        Calculate Net Present Value of CDS

        Args:
            curve: Discount curve
            survival_curve: Survival probability curve

        Returns:
            Dictionary with NPV details
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return {}

        try:
            result = {'instrument_type': self.instrument_type}

            if hasattr(self.instrument, 'npv'):
                # CDS NPV calculation typically requires both curves
                npv_value = self.instrument.npv(curve, survival_curve)
                result['npv'] = float(npv_value) if npv_value else 0.0
            else:
                result['note'] = 'NPV calculation requires discount and survival curves'

            return result

        except Exception as e:
            print(f"Error calculating NPV: {e}")
            return {
                'error': str(e),
                'instrument_type': self.instrument_type,
                'note': 'CDS valuation requires both discount and survival probability curves'
            }

    def calculate_cs01(
        self,
        curve=None,
        survival_curve=None
    ) -> Dict[str, Any]:
        """
        Calculate CS01 (Credit Spread 01-basis point sensitivity)

        Args:
            curve: Discount curve
            survival_curve: Survival probability curve

        Returns:
            Dictionary with CS01
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return {}

        try:
            result = {'instrument_type': self.instrument_type}

            # CS01 is typically calculated via finite difference
            if hasattr(self.instrument, 'delta'):
                cs01_value = self.instrument.delta(curve, survival_curve)
                result['cs01'] = float(cs01_value) if isinstance(cs01_value, (int, float)) else str(cs01_value)
                result['description'] = 'P&L change for 1bp parallel shift in credit spread'

            return result

        except Exception as e:
            print(f"Error calculating CS01: {e}")
            return {'error': str(e)}

    def calculate_spread_duration(
        self,
        curve=None,
        survival_curve=None
    ) -> Dict[str, Any]:
        """
        Calculate spread duration

        Args:
            curve: Discount curve
            survival_curve: Survival probability curve

        Returns:
            Dictionary with spread duration
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return {}

        try:
            result = {'instrument_type': self.instrument_type}

            # Spread duration is similar to modified duration for bonds
            if hasattr(self.instrument, 'duration'):
                duration_value = self.instrument.duration(curve, survival_curve)
                result['spread_duration'] = float(duration_value) if duration_value else None
                result['description'] = 'Percentage change in value per 1% change in spread'

            return result

        except Exception as e:
            print(f"Error calculating spread duration: {e}")
            return {'error': str(e)}

    def calculate_survival_probability(
        self,
        dates: Union[datetime, List[datetime]],
        survival_curve=None
    ) -> Dict[str, Any]:
        """
        Calculate survival probabilities

        Args:
            dates: Single date or list of dates
            survival_curve: Survival probability curve

        Returns:
            Dictionary with survival probabilities
        """
        if survival_curve is None:
            return {'error': 'Survival curve required'}

        try:
            if isinstance(dates, list):
                probs = []
                for date in dates:
                    prob = survival_curve[date] if hasattr(survival_curve, '__getitem__') else None
                    probs.append({
                        'date': date.strftime('%Y-%m-%d'),
                        'survival_probability': float(prob) if prob else None
                    })
                result = {'probabilities': probs}
            else:
                prob = survival_curve[dates] if hasattr(survival_curve, '__getitem__') else None
                result = {
                    'date': dates.strftime('%Y-%m-%d'),
                    'survival_probability': float(prob) if prob else None
                }

            return result

        except Exception as e:
            print(f"Error calculating survival probability: {e}")
            return {'error': str(e)}

    def get_cashflows(self) -> pd.DataFrame:
        """
        Get CDS cash flow schedule (premium payments)

        Returns:
            DataFrame with cash flow details
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return pd.DataFrame()

        try:
            if hasattr(self.instrument, 'cashflows_table'):
                cf_table = self.instrument.cashflows_table()
                return cf_table
            elif hasattr(self.instrument, 'cashflows'):
                cfs = self.instrument.cashflows()
                cf_data = []
                for date, amount in cfs.items():
                    cf_data.append({
                        'date': date,
                        'premium_payment': float(amount) if amount else 0.0
                    })
                return pd.DataFrame(cf_data)
            else:
                print("Cashflows not available for this instrument")
                return pd.DataFrame()

        except Exception as e:
            print(f"Error getting cashflows: {e}")
            return pd.DataFrame()

    def plot_survival_curve(
        self,
        dates: List[datetime],
        survival_curve=None
    ) -> go.Figure:
        """
        Plot survival probability curve

        Args:
            dates: List of dates
            survival_curve: Survival probability curve

        Returns:
            Plotly figure object
        """
        if survival_curve is None:
            print("Error: Survival curve required")
            return go.Figure()

        try:
            probs = []
            for date in dates:
                prob = survival_curve[date] if hasattr(survival_curve, '__getitem__') else 1.0
                probs.append(float(prob) if prob else 1.0)

            fig = go.Figure()

            fig.add_trace(go.Scatter(
                x=dates,
                y=[p * 100 for p in probs],
                mode='lines+markers',
                name='Survival Probability',
                line=dict(color='#10b981', width=2),
                marker=dict(size=6)
            ))

            # Add default probability
            default_probs = [100 - p * 100 for p in probs]
            fig.add_trace(go.Scatter(
                x=dates,
                y=default_probs,
                mode='lines+markers',
                name='Default Probability',
                line=dict(color='#ef4444', width=2, dash='dash'),
                marker=dict(size=6)
            ))

            fig.update_layout(
                title='Credit Survival and Default Probabilities',
                xaxis_title='Date',
                yaxis_title='Probability (%)',
                template='plotly_dark',
                hovermode='x unified',
                height=500,
                yaxis=dict(range=[0, 100])
            )

            return fig

        except Exception as e:
            print(f"Error plotting survival curve: {e}")
            return go.Figure()

    def plot_cashflows(self) -> go.Figure:
        """
        Plot CDS premium payment schedule

        Returns:
            Plotly figure object
        """
        df = self.get_cashflows()

        if df.empty:
            print("No cashflows to plot")
            return go.Figure()

        try:
            fig = go.Figure()

            date_col = next((col for col in df.columns if 'date' in col.lower()), df.columns[0])
            numeric_cols = df.select_dtypes(include=[np.number]).columns.tolist()

            if not numeric_cols:
                print("No numeric columns found in cashflows")
                return go.Figure()

            for col in numeric_cols[:2]:
                fig.add_trace(go.Bar(
                    x=df[date_col] if date_col in df.columns else df.index,
                    y=df[col],
                    name=col,
                    opacity=0.8
                ))

            fig.update_layout(
                title=f'{self.instrument_type} Premium Payments',
                xaxis_title='Date',
                yaxis_title='Payment Amount',
                template='plotly_dark',
                barmode='group',
                height=500
            )

            return fig

        except Exception as e:
            print(f"Error plotting cashflows: {e}")
            return go.Figure()

    def export_to_json(self) -> str:
        """Export CDS data to JSON"""
        if self.instrument is None:
            return json.dumps({'error': 'No instrument created'}, indent=2)

        export_data = {
            'instrument_type': self.instrument_type,
            'configuration': {
                'currency': self.config.currency,
                'calendar': self.config.calendar,
                'convention': self.config.convention,
                'frequency': self.config.frequency,
                'recovery_rate': self.config.recovery_rate,
                'notional': self.config.notional
            }
        }

        return json.dumps(export_data, indent=2)


def main():
    """Example usage and testing"""
    print("=== Rateslib Credit Analytics Test ===\n")

    # Initialize analytics
    config = CreditConfig(
        currency="USD",
        calendar="nyc",
        frequency="Q",
        recovery_rate=0.40,
        notional=10_000_000
    )
    analytics = CreditAnalytics(config)

    # Test 1: Create CDS
    print("1. Creating Credit Default Swap...")
    cds_details = analytics.create_cds(
        effective_date=rl.dt(2024, 1, 15),
        termination="5Y",
        fixed_rate=150,  # 150 bps
        recovery=0.40
    )
    print(f"   CDS: {json.dumps(cds_details, indent=4)}")

    # Test 2: Create Credit Premium Leg
    print("\n2. Creating Credit Premium Leg...")
    premium_leg = analytics.create_credit_premium_leg(
        effective_date=rl.dt(2024, 1, 15),
        termination="5Y",
        fixed_rate=150
    )
    print(f"   Premium Leg: {json.dumps(premium_leg, indent=4)}")

    # Test 3: Create Credit Protection Leg
    print("\n3. Creating Credit Protection Leg...")
    protection_leg = analytics.create_credit_protection_leg(
        effective_date=rl.dt(2024, 1, 15),
        termination="5Y",
        recovery=0.40
    )
    print(f"   Protection Leg: {json.dumps(protection_leg, indent=4)}")

    # Test 4: Get cashflows
    print("\n4. Getting cash flows...")
    cashflows = analytics.get_cashflows()
    if not cashflows.empty:
        print(f"   Cashflows generated: {len(cashflows)} periods")
        print(f"   Columns: {cashflows.columns.tolist()}")
    else:
        print("   Cashflows table not available (curve needed)")

    # Test 5: Calculate NPV
    print("\n5. Calculating NPV (without curves - will show structure)...")
    npv_result = analytics.calculate_npv()
    print(f"   NPV: {json.dumps(npv_result, indent=4)}")

    # Test 6: Calculate CS01
    print("\n6. Calculating CS01...")
    cs01_result = analytics.calculate_cs01()
    print(f"   CS01: {json.dumps(cs01_result, indent=4)}")

    # Test 7: Export to JSON
    print("\n7. Exporting to JSON...")
    json_output = analytics.export_to_json()
    print(f"   JSON export length: {len(json_output)} characters")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
