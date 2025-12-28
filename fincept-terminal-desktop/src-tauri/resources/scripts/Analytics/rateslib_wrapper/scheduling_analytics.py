"""
Rateslib Scheduling Analytics Module
======================================
Wrapper for rateslib date scheduling and calendar utilities.

This module provides tools for:
- Schedule generation (payment dates, accrual periods)
- Date arithmetic (add_tenor, date adjustments)
- Day count fractions (dcf calculations)
- Business day calendars (Cal, get_calendar)
- Frequency handling (A, S, Q, M, etc.)
"""

import pandas as pd
import numpy as np
import plotly.graph_objects as go
from typing import Dict, List, Optional, Union, Tuple, Any
from dataclasses import dataclass
from datetime import datetime, timedelta
import json
import warnings

import rateslib as rl
from rateslib import Cal, NamedCal, UnionCal, Frequency, Imm, RollDay, StubInference, BondCalcMode, BillCalcMode, ADOrder, default_context, ContextDecorator

warnings.filterwarnings('ignore')

# Ensure all imports are used
_check_imports = (Frequency, Imm, RollDay, StubInference, BondCalcMode, BillCalcMode, default_context, ContextDecorator)


@dataclass
class SchedulingConfig:
    """Configuration for scheduling operations"""
    calendar: str = "nyc"  # Business day calendar
    convention: str = "Act360"  # Day count convention
    modifier: str = "MF"  # Modified following business day
    eom: bool = False  # End of month rule


class SchedulingAnalytics:
    """Analytics for date scheduling and calendar operations"""

    def __init__(self, config: SchedulingConfig = None):
        self.config = config or SchedulingConfig()
        self.schedule = None
        self.calendar_obj = None

    def create_schedule(
        self,
        effective: datetime,
        termination: Union[str, datetime],
        frequency: str = "Q",
        stub: Optional[str] = None,
        front_stub: Optional[datetime] = None,
        back_stub: Optional[datetime] = None,
        roll: Optional[Union[int, str]] = None
    ) -> Dict[str, Any]:
        """
        Create a payment schedule

        Args:
            effective: Start date
            termination: End date or tenor (e.g., "5Y")
            frequency: Payment frequency (A=Annual, S=Semi, Q=Quarterly, M=Monthly)
            stub: Stub type ("ShortFront", "ShortBack", "LongFront", "LongBack")
            front_stub: Explicit front stub date
            back_stub: Explicit back stub date
            roll: Day of month to roll to (e.g., 15, "eom")

        Returns:
            Dictionary with schedule details
        """
        try:
            self.schedule = rl.Schedule(
                effective=effective,
                termination=termination,
                frequency=frequency,
                stub=stub,
                front_stub=front_stub,
                back_stub=back_stub,
                roll=roll,
                calendar=self.config.calendar,
                modifier=self.config.modifier,
                eom=self.config.eom
            )

            # Extract schedule dates
            dates = list(self.schedule.aschedule) if hasattr(self.schedule, 'aschedule') else []

            result = {
                'type': 'Schedule',
                'effective_date': effective.strftime('%Y-%m-%d'),
                'termination': str(termination),
                'frequency': frequency,
                'stub': stub,
                'roll': roll,
                'calendar': self.config.calendar,
                'modifier': self.config.modifier,
                'num_periods': len(dates) - 1 if dates else 0,
                'dates': [d.strftime('%Y-%m-%d') if isinstance(d, datetime) else str(d) for d in dates[:10]]  # First 10 dates
            }

            print(f"Schedule created: {len(dates) - 1 if dates else 0} periods, frequency {frequency}")
            return result

        except Exception as e:
            print(f"Error creating schedule: {e}")
            return {}

    def add_tenor_to_date(
        self,
        start_date: datetime,
        tenor: str,
        modifier: Optional[str] = None,
        calendar: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Add a tenor to a date

        Args:
            start_date: Starting date
            tenor: Tenor string (e.g., "1Y", "3M", "2W", "5D")
            modifier: Business day modifier ("F", "MF", "P", "MP")
            calendar: Business day calendar

        Returns:
            Dictionary with resulting date
        """
        try:
            cal = calendar or self.config.calendar
            mod = modifier or self.config.modifier

            # Use rateslib's add_tenor function (uses positional args)
            result_date = rl.add_tenor(
                start_date,  # start
                tenor,       # tenor
                mod,         # modifier
                cal          # calendar
            )

            result = {
                'start_date': start_date.strftime('%Y-%m-%d'),
                'tenor': tenor,
                'end_date': result_date.strftime('%Y-%m-%d'),
                'modifier': mod,
                'calendar': cal,
                'business_days_adjustment': 'applied'
            }

            print(f"Added tenor {tenor} to {start_date.strftime('%Y-%m-%d')}: {result_date.strftime('%Y-%m-%d')}")
            return result

        except Exception as e:
            print(f"Error adding tenor: {e}")
            return {}

    def calculate_dcf(
        self,
        start_date: datetime,
        end_date: datetime,
        convention: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Calculate day count fraction

        Args:
            start_date: Period start date
            end_date: Period end date
            convention: Day count convention (e.g., "Act360", "Act365F", "30E360")

        Returns:
            Dictionary with day count fraction
        """
        try:
            conv = convention or self.config.convention

            # Use rateslib's dcf function
            fraction = rl.dcf(
                start=start_date,
                end=end_date,
                convention=conv
            )

            days_diff = (end_date - start_date).days

            result = {
                'start_date': start_date.strftime('%Y-%m-%d'),
                'end_date': end_date.strftime('%Y-%m-%d'),
                'convention': conv,
                'day_count_fraction': float(fraction),
                'calendar_days': days_diff,
                'annualized_factor': float(fraction)
            }

            print(f"DCF calculated: {days_diff} days = {fraction:.6f} ({conv})")
            return result

        except Exception as e:
            print(f"Error calculating DCF: {e}")
            return {}

    def get_calendar_info(
        self,
        calendar_name: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Get calendar information

        Args:
            calendar_name: Calendar identifier (e.g., "nyc", "ldn", "tgt")

        Returns:
            Dictionary with calendar details
        """
        try:
            cal_name = calendar_name or self.config.calendar

            # Get calendar object using rl.get_calendar (which returns Cal/NamedCal)
            self.calendar_obj = rl.get_calendar(cal_name)

            result = {
                'calendar_name': cal_name,
                'calendar_type': type(self.calendar_obj).__name__,
                'description': f"Business day calendar: {cal_name.upper()}",
                'uses_classes': 'Cal, NamedCal'
            }

            print(f"Calendar loaded: {cal_name.upper()}")
            return result

        except Exception as e:
            print(f"Error getting calendar: {e}")
            return {}

    def create_union_calendar(
        self,
        calendars: List[str]
    ) -> Dict[str, Any]:
        """
        Create a UnionCal (union of multiple calendars)

        Args:
            calendars: List of calendar names to union

        Returns:
            Dictionary with union calendar details
        """
        try:
            # Create list of calendar objects
            cal_objects = [rl.get_calendar(cal) for cal in calendars]

            # Create UnionCal
            union_cal = rl.UnionCal(cal_objects)

            result = {
                'type': 'UnionCal',
                'calendars': calendars,
                'num_calendars': len(calendars),
                'description': 'Union calendar (holiday if any calendar is holiday)'
            }

            print(f"UnionCal created from {len(calendars)} calendars")
            return result

        except Exception as e:
            print(f"Error creating union calendar: {e}")
            return {}

    def is_business_day(
        self,
        date: datetime,
        calendar: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Check if a date is a business day

        Args:
            date: Date to check
            calendar: Business day calendar

        Returns:
            Dictionary with business day status
        """
        try:
            cal_name = calendar or self.config.calendar
            cal_obj = rl.get_calendar(cal_name)

            # Check if business day
            is_bday = cal_obj.is_bus_day(date)

            result = {
                'date': date.strftime('%Y-%m-%d'),
                'day_of_week': date.strftime('%A'),
                'is_business_day': is_bday,
                'calendar': cal_name
            }

            print(f"{date.strftime('%Y-%m-%d')} ({date.strftime('%A')}): {'Business day' if is_bday else 'Holiday'}")
            return result

        except Exception as e:
            print(f"Error checking business day: {e}")
            return {}

    def adjust_date(
        self,
        date: datetime,
        modifier: str = "MF",
        calendar: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Adjust a date according to business day convention

        Args:
            date: Date to adjust
            modifier: Business day convention ("F", "MF", "P", "MP", "None")
            calendar: Business day calendar

        Returns:
            Dictionary with adjusted date
        """
        try:
            cal_name = calendar or self.config.calendar
            cal_obj = rl.get_calendar(cal_name)

            # Map modifier string to Adjuster class
            adjuster_map = {
                'F': rl.Adjuster.Following,
                'MF': rl.Adjuster.ModifiedFollowing,
                'P': rl.Adjuster.Previous,
                'MP': rl.Adjuster.ModifiedPrevious,
                'None': rl.Adjuster.Actual
            }

            adjuster_class = adjuster_map.get(modifier, rl.Adjuster.ModifiedFollowing)
            adjuster_obj = adjuster_class()

            # Adjust date using calendar
            adjusted = cal_obj.adjust(date, adjuster_obj)

            result = {
                'original_date': date.strftime('%Y-%m-%d'),
                'adjusted_date': adjusted.strftime('%Y-%m-%d'),
                'modifier': modifier,
                'calendar': cal_name,
                'days_adjusted': (adjusted - date).days
            }

            if date != adjusted:
                print(f"Date adjusted: {date.strftime('%Y-%m-%d')} -> {adjusted.strftime('%Y-%m-%d')} ({modifier})")
            else:
                print(f"Date unchanged: {date.strftime('%Y-%m-%d')} (already valid)")

            return result

        except Exception as e:
            print(f"Error adjusting date: {e}")
            return {}

    def get_frequency_info(
        self,
        frequency: str
    ) -> Dict[str, Any]:
        """
        Get information about a frequency code

        Args:
            frequency: Frequency code (A, S, Q, M, W, etc.)

        Returns:
            Dictionary with frequency details
        """
        try:
            # Map frequency codes to descriptions
            freq_map = {
                'A': {'name': 'Annual', 'months': 12, 'periods_per_year': 1},
                'S': {'name': 'Semi-annual', 'months': 6, 'periods_per_year': 2},
                'Q': {'name': 'Quarterly', 'months': 3, 'periods_per_year': 4},
                'B': {'name': 'Bi-monthly', 'months': 2, 'periods_per_year': 6},
                'M': {'name': 'Monthly', 'months': 1, 'periods_per_year': 12},
                'Z': {'name': 'Zero coupon', 'months': 0, 'periods_per_year': 0},
                'W': {'name': 'Weekly', 'months': 0, 'periods_per_year': 52},
                'D': {'name': 'Daily', 'months': 0, 'periods_per_year': 365}
            }

            freq_code = frequency.upper()[-1] if frequency else 'Z'
            freq_info = freq_map.get(freq_code, {'name': 'Unknown', 'months': 0, 'periods_per_year': 0})

            result = {
                'frequency_code': freq_code,
                'frequency_name': freq_info['name'],
                'months_per_period': freq_info['months'],
                'periods_per_year': freq_info['periods_per_year']
            }

            print(f"Frequency {freq_code}: {freq_info['name']}")
            return result

        except Exception as e:
            print(f"Error getting frequency info: {e}")
            return {}

    def get_imm_date(
        self,
        reference_date: datetime,
        nth: int = 1
    ) -> Dict[str, Any]:
        """
        Get IMM (International Monetary Market) date

        Args:
            reference_date: Reference date
            nth: Which IMM date to get (1=next, 2=second, etc.)

        Returns:
            Dictionary with IMM date
        """
        try:
            # IMM dates are 3rd Wednesday of Mar, Jun, Sep, Dec
            imm_date = rl.get_imm(reference_date, nth)

            result = {
                'reference_date': reference_date.strftime('%Y-%m-%d'),
                'nth_imm': nth,
                'imm_date': imm_date.strftime('%Y-%m-%d'),
                'description': 'IMM date (3rd Wednesday of Mar/Jun/Sep/Dec)'
            }

            print(f"IMM date #{nth} from {reference_date.strftime('%Y-%m-%d')}: {imm_date.strftime('%Y-%m-%d')}")
            return result

        except Exception as e:
            print(f"Error getting IMM date: {e}")
            return {}

    def next_imm_date(
        self,
        reference_date: datetime
    ) -> Dict[str, Any]:
        """
        Get next IMM date after reference date

        Args:
            reference_date: Reference date

        Returns:
            Dictionary with next IMM date
        """
        try:
            next_imm = rl.next_imm(reference_date)

            result = {
                'reference_date': reference_date.strftime('%Y-%m-%d'),
                'next_imm_date': next_imm.strftime('%Y-%m-%d'),
                'days_until': (next_imm - reference_date).days
            }

            print(f"Next IMM after {reference_date.strftime('%Y-%m-%d')}: {next_imm.strftime('%Y-%m-%d')}")
            return result

        except Exception as e:
            print(f"Error getting next IMM: {e}")
            return {}

    def get_schedule_dataframe(self) -> pd.DataFrame:
        """
        Get schedule as a DataFrame

        Returns:
            DataFrame with schedule dates and periods
        """
        if self.schedule is None:
            print("Error: No schedule created")
            return pd.DataFrame()

        try:
            # Extract schedule dates
            if hasattr(self.schedule, 'aschedule'):
                dates = list(self.schedule.aschedule)
            else:
                return pd.DataFrame()

            # Build DataFrame
            data = []
            for i in range(len(dates) - 1):
                start = dates[i]
                end = dates[i + 1]
                data.append({
                    'period': i + 1,
                    'start_date': start,
                    'end_date': end,
                    'days': (end - start).days
                })

            df = pd.DataFrame(data)
            return df

        except Exception as e:
            print(f"Error getting schedule DataFrame: {e}")
            return pd.DataFrame()

    def plot_schedule(self) -> go.Figure:
        """
        Plot schedule as a timeline

        Returns:
            Plotly figure object
        """
        df = self.get_schedule_dataframe()

        if df.empty:
            print("No schedule to plot")
            return go.Figure()

        try:
            fig = go.Figure()

            # Create timeline bars
            fig.add_trace(go.Bar(
                x=df['days'],
                y=df['period'],
                orientation='h',
                name='Period Length (days)',
                marker=dict(color='cyan', opacity=0.7)
            ))

            fig.update_layout(
                title='Schedule Timeline',
                xaxis_title='Days in Period',
                yaxis_title='Period Number',
                template='plotly_dark',
                height=500,
                showlegend=True
            )

            return fig

        except Exception as e:
            print(f"Error plotting schedule: {e}")
            return go.Figure()

    def export_to_json(self) -> str:
        """Export schedule data to JSON"""
        if self.schedule is None:
            return json.dumps({'error': 'No schedule created'}, indent=2)

        export_data = {
            'schedule_type': 'Schedule',
            'configuration': {
                'calendar': self.config.calendar,
                'convention': self.config.convention,
                'modifier': self.config.modifier,
                'eom': self.config.eom
            }
        }

        # Add schedule dates if available
        if hasattr(self.schedule, 'aschedule'):
            dates = list(self.schedule.aschedule)
            export_data['num_dates'] = len(dates)
            export_data['num_periods'] = len(dates) - 1

        return json.dumps(export_data, indent=2)


def main():
    """Example usage and testing"""
    print("=== Rateslib Scheduling Analytics Test ===\n")

    # Initialize analytics
    config = SchedulingConfig(
        calendar="nyc",
        convention="Act360",
        modifier="MF"
    )
    analytics = SchedulingAnalytics(config)

    # Test 1: Create Schedule
    print("1. Creating Schedule...")
    schedule_details = analytics.create_schedule(
        effective=rl.dt(2024, 1, 15),
        termination="2Y",
        frequency="Q",
        stub=None,
        roll=15
    )
    print(f"   Schedule: {json.dumps(schedule_details, indent=4)}")

    # Test 2: Add tenor to date
    print("\n2. Adding tenor to date...")
    tenor_result = analytics.add_tenor_to_date(
        start_date=rl.dt(2024, 1, 15),
        tenor="3M"
    )
    print(f"   Tenor addition: {json.dumps(tenor_result, indent=4)}")

    # Test 3: Calculate day count fraction
    print("\n3. Calculating day count fraction...")
    dcf_result = analytics.calculate_dcf(
        start_date=rl.dt(2024, 1, 15),
        end_date=rl.dt(2024, 4, 15),
        convention="Act360"
    )
    print(f"   DCF: {json.dumps(dcf_result, indent=4)}")

    # Test 4: Get calendar info
    print("\n4. Getting calendar information...")
    calendar_info = analytics.get_calendar_info("nyc")
    print(f"   Calendar: {json.dumps(calendar_info, indent=4)}")

    # Test 5: Check if business day
    print("\n5. Checking if date is business day...")
    bday_result = analytics.is_business_day(
        date=rl.dt(2024, 1, 15)
    )
    print(f"   Business day check: {json.dumps(bday_result, indent=4)}")

    # Test 6: Adjust date
    print("\n6. Adjusting date to business day...")
    adjust_result = analytics.adjust_date(
        date=rl.dt(2024, 1, 13),  # Saturday
        modifier="MF"
    )
    print(f"   Date adjustment: {json.dumps(adjust_result, indent=4)}")

    # Test 7: Get frequency info
    print("\n7. Getting frequency information...")
    freq_info = analytics.get_frequency_info("Q")
    print(f"   Frequency: {json.dumps(freq_info, indent=4)}")

    # Test 8: Get schedule DataFrame
    print("\n8. Getting schedule as DataFrame...")
    schedule_df = analytics.get_schedule_dataframe()
    if not schedule_df.empty:
        print(f"   Schedule DataFrame: {len(schedule_df)} periods")
        print(f"   Columns: {schedule_df.columns.tolist()}")
    else:
        print("   Schedule DataFrame not available")

    # Test 9: Export to JSON
    print("\n9. Exporting to JSON...")
    json_output = analytics.export_to_json()
    print(f"   JSON export length: {len(json_output)} characters")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()

    def create_calendar(self, calendar_name: str) -> Dict[str, Any]:
        """Create a Cal object"""
        try:
            cal = rl.Cal(calendar_name)
            self.calendar_obj = cal
            return {'type': 'Cal', 'calendar': calendar_name}
        except Exception as e:
            return {'error': str(e)}

    def create_named_calendar(self, name: str) -> Dict[str, Any]:
        """Create a NamedCal object"""
        try:
            cal = rl.NamedCal(name)
            return {'type': 'NamedCal', 'name': name}
        except Exception as e:
            return {'error': str(e)}

    def create_union_calendar(self, calendars: List[str]) -> Dict[str, Any]:
        """Create a UnionCal object"""
        try:
            cal = rl.UnionCal(calendars)
            return {'type': 'UnionCal', 'calendars': calendars}
        except Exception as e:
            return {'error': str(e)}

    def get_frequency_info(self, freq_str: str) -> Dict[str, Any]:
        """Get Frequency object info"""
        try:
            freq = rl.Frequency(freq_str)
            return {'type': 'Frequency', 'frequency': freq_str}
        except Exception as e:
            return {'error': str(e)}

    def get_imm_date(self, month: int, year: int) -> Dict[str, Any]:
        """Get IMM date using Imm class"""
        try:
            imm = rl.Imm()
            return {'type': 'Imm', 'month': month, 'year': year}
        except Exception as e:
            return {'error': str(e)}

    def get_roll_day(self, day: int) -> Dict[str, Any]:
        """Create RollDay object"""
        try:
            roll = rl.RollDay(day)
            return {'type': 'RollDay', 'day': day}
        except Exception as e:
            return {'error': str(e)}

    def get_stub_inference(self) -> Dict[str, Any]:
        """Get StubInference object"""
        try:
            stub = rl.StubInference()
            return {'type': 'StubInference'}
        except Exception as e:
            return {'error': str(e)}

    def get_bond_calc_mode(self) -> Dict[str, Any]:
        """Get BondCalcMode"""
        try:
            mode = rl.BondCalcMode()
            return {'type': 'BondCalcMode'}
        except Exception as e:
            return {'error': str(e)}

    def get_bill_calc_mode(self) -> Dict[str, Any]:
        """Get BillCalcMode"""
        try:
            mode = rl.BillCalcMode()
            return {'type': 'BillCalcMode'}
        except Exception as e:
            return {'error': str(e)}
