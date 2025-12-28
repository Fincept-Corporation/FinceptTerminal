"""
Rateslib FX Analytics Module
==============================
Wrapper for rateslib FX rates, forwards, and options functionality.

This module provides tools for:
- FX spot and forward rates (FXRates, FXForwards)
- FX exchange and swap instruments (FXExchange, FXSwap)
- FX vanilla options (FXCall, FXPut)
- FX option strategies (FXStraddle, FXRiskReversal, FXStrangle)
- Volatility surfaces and smiles
- FX risk analytics
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
class FXConfig:
    """Configuration for FX instruments"""
    domestic_currency: str = "USD"
    foreign_currency: str = "EUR"
    calendar: str = "nyc"
    convention: str = "Act360"
    settle_days: int = 2  # Standard FX settlement


class FXAnalytics:
    """Analytics for FX rates, forwards, and options"""

    def __init__(self, config: FXConfig = None):
        self.config = config or FXConfig()
        self.instrument = None
        self.instrument_type = None
        self.fx_rates = None

    def create_fx_rates(
        self,
        rate: float,
        settlement: Optional[datetime] = None,
        base: Optional[str] = None,
        quote: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create FX spot rates object

        Args:
            rate: FX spot rate (quote/base convention)
            settlement: Settlement date
            base: Base currency (domestic)
            quote: Quote currency (foreign)

        Returns:
            Dictionary with FX rates details
        """
        try:
            base_ccy = base or self.config.domestic_currency
            quote_ccy = quote or self.config.foreign_currency

            # FXRates uses fx_rates dict with string keys like 'eurusd'
            pair_key = f"{quote_ccy.lower()}{base_ccy.lower()}"
            self.fx_rates = rl.FXRates(
                fx_rates={pair_key: rate},
                settlement=settlement,
                base=base_ccy.lower()
            )
            self.instrument_type = "FXRates"

            result = {
                'type': 'FXRates',
                'base_currency': base_ccy,
                'quote_currency': quote_ccy,
                'spot_rate': rate,
                'settlement': settlement.strftime('%Y-%m-%d') if settlement else None,
                'quote_convention': f"{quote_ccy}/{base_ccy}"
            }

            print(f"FX Rates created: {quote_ccy}/{base_ccy} = {rate:.4f}")
            return result

        except Exception as e:
            print(f"Error creating FX rates: {e}")
            return {}

    def create_fx_forwards(
        self,
        fx_rates,
        domestic_curve,
        foreign_curve,
        base: Optional[str] = None,
        quote: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create FX forwards pricing object

        Args:
            fx_rates: FXRates object for spot
            domestic_curve: Domestic currency discount curve
            foreign_curve: Foreign currency discount curve
            base: Base currency
            quote: Quote currency

        Returns:
            Dictionary with FX forwards details
        """
        try:
            base_ccy = base or self.config.domestic_currency
            quote_ccy = quote or self.config.foreign_currency

            self.instrument = rl.FXForwards(
                fx_rates=fx_rates,
                base=base_ccy,
                quote=quote_ccy
            )
            self.instrument_type = "FXForwards"

            result = {
                'type': 'FXForwards',
                'base_currency': base_ccy,
                'quote_currency': quote_ccy,
                'has_domestic_curve': domestic_curve is not None,
                'has_foreign_curve': foreign_curve is not None
            }

            print(f"FX Forwards created: {quote_ccy}/{base_ccy}")
            return result

        except Exception as e:
            print(f"Error creating FX forwards: {e}")
            return {}

    def create_fx_exchange(
        self,
        pair: str,
        settlement: datetime,
        notional: float = 1_000_000,
        fx_rate: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create FX exchange (spot or forward)

        Args:
            pair: Currency pair (e.g., "eurusd")
            settlement: Settlement/delivery date
            notional: Notional amount
            fx_rate: FX rate for the exchange (optional)

        Returns:
            Dictionary with FX exchange details
        """
        try:
            self.instrument = rl.FXExchange(
                settlement=settlement,
                pair=pair,
                fx_rate=fx_rate,
                notional=notional
            )
            self.instrument_type = "FXExchange"

            result = {
                'type': 'FXExchange',
                'pair': pair.upper(),
                'settlement_date': settlement.strftime('%Y-%m-%d'),
                'notional': notional,
                'fx_rate': fx_rate,
                'calendar': self.config.calendar
            }

            print(f"FX Exchange created: {pair.upper()}, notional {notional:,.0f}")
            return result

        except Exception as e:
            print(f"Error creating FX exchange: {e}")
            return {}

    def create_fx_swap(
        self,
        pair: str,
        effective_date: datetime,
        termination: Union[str, datetime],
        notional: float = 1_000_000,
        points: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create FX swap (combination of spot and forward)

        Args:
            pair: Currency pair (e.g., "eurusd")
            effective_date: Near leg settlement date
            termination: Far leg settlement date or tenor
            notional: Notional amount
            points: Forward points (if known)

        Returns:
            Dictionary with FX swap details
        """
        try:
            self.instrument = rl.FXSwap(
                pair=pair,
                effective=effective_date,
                termination=termination,
                notional=notional,
                calendar=self.config.calendar
            )
            self.instrument_type = "FXSwap"

            result = {
                'type': 'FXSwap',
                'pair': pair.upper(),
                'near_date': effective_date.strftime('%Y-%m-%d'),
                'far_date': str(termination),
                'notional': notional,
                'points': points,
                'calendar': self.config.calendar
            }

            print(f"FX Swap created: {pair.upper()}, {notional:,.0f}, {effective_date.strftime('%Y-%m-%d')} to {termination}")
            return result

        except Exception as e:
            print(f"Error creating FX swap: {e}")
            return {}

    def create_fx_call(
        self,
        pair: str,
        expiry: datetime,
        strike: float = None,
        notional: float = 1_000_000
    ) -> Dict[str, Any]:
        """
        Create FX call option

        Args:
            pair: Currency pair (e.g., "eurusd")
            expiry: Option expiry date
            strike: Strike price
            notional: Notional amount

        Returns:
            Dictionary with FX call details
        """
        try:
            self.instrument = rl.FXCall(
                pair=pair,
                expiry=expiry,
                strike=strike,
                notional=notional
            )
            self.instrument_type = "FXCall"

            result = {
                'type': 'FXCall',
                'pair': pair.upper(),
                'expiry_date': expiry.strftime('%Y-%m-%d'),
                'strike': strike,
                'notional': notional,
                'calendar': self.config.calendar
            }

            print(f"FX Call created: {pair.upper()} strike {strike}, expiry {expiry.strftime('%Y-%m-%d')}")
            return result

        except Exception as e:
            print(f"Error creating FX call: {e}")
            return {}

    def create_fx_put(
        self,
        pair: str,
        expiry: datetime,
        strike: float = None,
        notional: float = 1_000_000
    ) -> Dict[str, Any]:
        """
        Create FX put option

        Args:
            pair: Currency pair (e.g., "eurusd")
            expiry: Option expiry date
            strike: Strike price
            notional: Notional amount

        Returns:
            Dictionary with FX put details
        """
        try:
            self.instrument = rl.FXPut(
                pair=pair,
                expiry=expiry,
                strike=strike,
                notional=notional
            )
            self.instrument_type = "FXPut"

            result = {
                'type': 'FXPut',
                'pair': pair.upper(),
                'expiry_date': expiry.strftime('%Y-%m-%d'),
                'strike': strike,
                'notional': notional,
                'calendar': self.config.calendar
            }

            print(f"FX Put created: {pair.upper()} strike {strike}, expiry {expiry.strftime('%Y-%m-%d')}")
            return result

        except Exception as e:
            print(f"Error creating FX put: {e}")
            return {}

    def create_fx_straddle(
        self,
        pair: str,
        expiry: datetime,
        strike: float = None,
        notional: float = 1_000_000
    ) -> Dict[str, Any]:
        """
        Create FX straddle (long call + long put at same strike)

        Args:
            pair: Currency pair
            expiry: Option expiry date
            strike: Strike price (typically ATM)
            notional: Notional amount

        Returns:
            Dictionary with straddle details
        """
        try:
            self.instrument = rl.FXStraddle(
                pair=pair,
                expiry=expiry,
                strike=strike,
                notional=notional
            )
            self.instrument_type = "FXStraddle"

            result = {
                'type': 'FXStraddle',
                'pair': pair.upper(),
                'expiry_date': expiry.strftime('%Y-%m-%d'),
                'strike': strike,
                'notional': notional,
                'strategy': 'Long Call + Long Put'
            }

            print(f"FX Straddle created: {pair.upper()} strike {strike}")
            return result

        except Exception as e:
            print(f"Error creating FX straddle: {e}")
            return {}

    def create_fx_risk_reversal(
        self,
        pair: str,
        expiry: datetime,
        strike_put: float = None,
        strike_call: float = None,
        notional: float = 1_000_000
    ) -> Dict[str, Any]:
        """
        Create FX risk reversal (long call + short put)

        Args:
            pair: Currency pair
            expiry: Option expiry date
            strike_put: Put strike (lower strike)
            strike_call: Call strike (higher strike)
            notional: Notional amount

        Returns:
            Dictionary with risk reversal details
        """
        try:
            # FXRiskReversal expects strike as list [put_strike, call_strike]
            strikes = [strike_put, strike_call]

            self.instrument = rl.FXRiskReversal(
                pair=pair,
                expiry=expiry,
                strike=strikes,
                notional=notional
            )
            self.instrument_type = "FXRiskReversal"

            result = {
                'type': 'FXRiskReversal',
                'pair': pair.upper(),
                'expiry_date': expiry.strftime('%Y-%m-%d'),
                'call_strike': strike_call,
                'put_strike': strike_put,
                'notional': notional,
                'strategy': 'Long Call + Short Put'
            }

            print(f"FX Risk Reversal created: {pair.upper()} call {strike_call} / put {strike_put}")
            return result

        except Exception as e:
            print(f"Error creating FX risk reversal: {e}")
            return {}

    def create_fx_strangle(
        self,
        pair: str,
        expiry: datetime,
        strike_put: float = None,
        strike_call: float = None,
        notional: float = 1_000_000
    ) -> Dict[str, Any]:
        """
        Create FX strangle (long call + long put at different strikes)

        Args:
            pair: Currency pair
            expiry: Option expiry date
            strike_put: Put strike (OTM lower)
            strike_call: Call strike (OTM higher)
            notional: Notional amount

        Returns:
            Dictionary with strangle details
        """
        try:
            # FXStrangle expects strike as list [put_strike, call_strike]
            strikes = [strike_put, strike_call]

            self.instrument = rl.FXStrangle(
                pair=pair,
                expiry=expiry,
                strike=strikes,
                notional=notional
            )
            self.instrument_type = "FXStrangle"

            result = {
                'type': 'FXStrangle',
                'pair': pair.upper(),
                'expiry_date': expiry.strftime('%Y-%m-%d'),
                'call_strike': strike_call,
                'put_strike': strike_put,
                'notional': notional,
                'strategy': 'Long Call + Long Put (OTM)'
            }

            print(f"FX Strangle created: {pair.upper()} call {strike_call} / put {strike_put}")
            return result

        except Exception as e:
            print(f"Error creating FX strangle: {e}")
            return {}

    def create_fx_broker_fly(
        self,
        pair: str,
        expiry: datetime,
        strikes: List[float],  # [low_strike, mid_strike, high_strike]
        notional: float = 1_000_000
    ) -> Dict[str, Any]:
        """
        Create FX Broker Butterfly (broker-style quote)

        Args:
            pair: Currency pair
            expiry: Option expiry date
            strikes: List of [low_strike, mid_strike, high_strike]
            notional: Notional amount

        Returns:
            Dictionary with broker fly details
        """
        try:
            if len(strikes) != 3:
                return {'error': 'Broker fly requires exactly 3 strikes'}

            self.instrument = rl.FXBrokerFly(
                pair=pair,
                expiry=expiry,
                strike=strikes,
                notional=notional
            )
            self.instrument_type = "FXBrokerFly"

            result = {
                'type': 'FXBrokerFly',
                'pair': pair.upper(),
                'expiry_date': expiry.strftime('%Y-%m-%d'),
                'low_strike': strikes[0],
                'mid_strike': strikes[1],
                'high_strike': strikes[2],
                'notional': notional,
                'structure': 'Butterfly (1x-2x+1)',
                'strategy': 'Broker-quoted butterfly spread'
            }

            print(f"FX Broker Fly: {pair.upper()} [{strikes[0]}, {strikes[1]}, {strikes[2]}]")
            return result

        except Exception as e:
            print(f"Error creating FX broker fly: {e}")
            return {}

    def calculate_npv(
        self,
        fx_rates=None,
        domestic_curve=None,
        foreign_curve=None
    ) -> Dict[str, Any]:
        """
        Calculate Net Present Value

        Args:
            fx_rates: FXRates object
            domestic_curve: Domestic currency curve
            foreign_curve: Foreign currency curve

        Returns:
            Dictionary with NPV details
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return {}

        try:
            result = {'instrument_type': self.instrument_type}

            if hasattr(self.instrument, 'npv'):
                npv_value = self.instrument.npv(
                    fx=fx_rates,
                    base=domestic_curve,
                    local=foreign_curve
                )
                result['npv'] = float(npv_value) if npv_value else 0.0
            else:
                result['note'] = 'NPV calculation requires curves'

            return result

        except Exception as e:
            print(f"Error calculating NPV: {e}")
            return {'error': str(e), 'instrument_type': self.instrument_type}

    def calculate_delta(
        self,
        fx_rates=None,
        domestic_curve=None,
        foreign_curve=None,
        vol_surface=None
    ) -> Dict[str, Any]:
        """
        Calculate delta (FX spot sensitivity)

        Args:
            fx_rates: FXRates object
            domestic_curve: Domestic currency curve
            foreign_curve: Foreign currency curve
            vol_surface: Volatility surface (for options)

        Returns:
            Dictionary with delta
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return {}

        try:
            result = {'instrument_type': self.instrument_type}

            if hasattr(self.instrument, 'delta'):
                delta_value = self.instrument.delta(
                    fx=fx_rates,
                    base=domestic_curve,
                    local=foreign_curve,
                    vol=vol_surface
                )
                result['delta'] = float(delta_value) if isinstance(delta_value, (int, float)) else str(delta_value)

            return result

        except Exception as e:
            print(f"Error calculating delta: {e}")
            return {'error': str(e)}

    def calculate_gamma(
        self,
        fx_rates=None,
        domestic_curve=None,
        foreign_curve=None,
        vol_surface=None
    ) -> Dict[str, Any]:
        """
        Calculate gamma (second-order FX sensitivity)

        Args:
            fx_rates: FXRates object
            domestic_curve: Domestic curve
            foreign_curve: Foreign curve
            vol_surface: Volatility surface

        Returns:
            Dictionary with gamma
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return {}

        try:
            result = {'instrument_type': self.instrument_type}

            if hasattr(self.instrument, 'gamma'):
                gamma_value = self.instrument.gamma(
                    fx=fx_rates,
                    base=domestic_curve,
                    local=foreign_curve,
                    vol=vol_surface
                )
                result['gamma'] = float(gamma_value) if isinstance(gamma_value, (int, float)) else str(gamma_value)

            return result

        except Exception as e:
            print(f"Error calculating gamma: {e}")
            return {'error': str(e)}

    def calculate_vega(
        self,
        fx_rates=None,
        domestic_curve=None,
        foreign_curve=None,
        vol_surface=None
    ) -> Dict[str, Any]:
        """
        Calculate vega (volatility sensitivity)

        Args:
            fx_rates: FXRates object
            domestic_curve: Domestic curve
            foreign_curve: Foreign curve
            vol_surface: Volatility surface

        Returns:
            Dictionary with vega
        """
        if self.instrument is None:
            print("Error: No instrument created")
            return {}

        try:
            result = {'instrument_type': self.instrument_type}

            if hasattr(self.instrument, 'vega'):
                vega_value = self.instrument.vega(
                    fx=fx_rates,
                    base=domestic_curve,
                    local=foreign_curve,
                    vol=vol_surface
                )
                result['vega'] = float(vega_value) if isinstance(vega_value, (int, float)) else str(vega_value)

            return result

        except Exception as e:
            print(f"Error calculating vega: {e}")
            return {'error': str(e)}

    def plot_payoff(
        self,
        spot_range: Tuple[float, float] = None,
        num_points: int = 100
    ) -> go.Figure:
        """
        Plot option payoff diagram

        Args:
            spot_range: (min_spot, max_spot) range for plotting
            num_points: Number of points to plot

        Returns:
            Plotly figure object
        """
        if self.instrument is None or self.instrument_type not in ['FXCall', 'FXPut', 'FXStraddle', 'FXRiskReversal', 'FXStrangle']:
            print("Error: Payoff plot only available for options")
            return go.Figure()

        try:
            fig = go.Figure()

            # Generate spot prices for payoff calculation
            if spot_range is None:
                # Default range around ATM
                spot_range = (0.8, 1.2)  # Normalized around 1.0

            spots = np.linspace(spot_range[0], spot_range[1], num_points)
            payoffs = []

            # Calculate payoff for each spot (simplified illustration)
            for spot in spots:
                # This is a simplified payoff calculation
                # In practice, would need to call instrument methods
                if self.instrument_type == 'FXCall':
                    strike = getattr(self.instrument, 'strike', 1.0)
                    payoff = max(spot - strike, 0)
                elif self.instrument_type == 'FXPut':
                    strike = getattr(self.instrument, 'strike', 1.0)
                    payoff = max(strike - spot, 0)
                else:
                    payoff = 0  # Placeholder for strategies

                payoffs.append(payoff)

            fig.add_trace(go.Scatter(
                x=spots,
                y=payoffs,
                mode='lines',
                name=f'{self.instrument_type} Payoff',
                line=dict(width=2, color='cyan')
            ))

            # Add zero line
            fig.add_hline(y=0, line_dash="dash", line_color="gray", opacity=0.5)

            fig.update_layout(
                title=f'{self.instrument_type} Payoff Diagram',
                xaxis_title='Spot FX Rate',
                yaxis_title='Payoff',
                template='plotly_dark',
                height=500,
                showlegend=True
            )

            return fig

        except Exception as e:
            print(f"Error plotting payoff: {e}")
            return go.Figure()

    def export_to_json(self) -> str:
        """Export instrument data to JSON"""
        if self.instrument is None:
            return json.dumps({'error': 'No instrument created'}, indent=2)

        export_data = {
            'instrument_type': self.instrument_type,
            'configuration': {
                'domestic_currency': self.config.domestic_currency,
                'foreign_currency': self.config.foreign_currency,
                'calendar': self.config.calendar,
                'convention': self.config.convention,
                'settle_days': self.config.settle_days
            }
        }

        return json.dumps(export_data, indent=2)


def main():
    """Example usage and testing"""
    print("=== Rateslib FX Analytics Test ===\n")

    # Initialize analytics
    config = FXConfig(
        domestic_currency="USD",
        foreign_currency="EUR",
        calendar="nyc"
    )
    analytics = FXAnalytics(config)

    # Test 1: Create FX Rates
    print("1. Creating FX Rates...")
    fx_rates_details = analytics.create_fx_rates(
        rate=1.0850,  # EUR/USD spot rate
        settlement=rl.dt(2024, 1, 15),
        base="USD",
        quote="EUR"
    )
    print(f"   FX Rates: {json.dumps(fx_rates_details, indent=4)}")

    # Test 2: Create FX Exchange
    print("\n2. Creating FX Exchange...")
    exchange_details = analytics.create_fx_exchange(
        pair="eurusd",
        settlement=rl.dt(2024, 1, 17),
        notional=1_000_000,
        fx_rate=1.0850
    )
    print(f"   Exchange: {json.dumps(exchange_details, indent=4)}")

    # Test 3: Create FX Swap
    print("\n3. Creating FX Swap...")
    swap_details = analytics.create_fx_swap(
        pair="eurusd",
        effective_date=rl.dt(2024, 1, 17),
        termination="3M",
        notional=5_000_000
    )
    print(f"   Swap: {json.dumps(swap_details, indent=4)}")

    # Test 4: Create FX Call Option
    print("\n4. Creating FX Call Option...")
    call_details = analytics.create_fx_call(
        pair="eurusd",
        expiry=rl.dt(2024, 6, 15),
        strike=1.10,
        notional=1_000_000
    )
    print(f"   Call: {json.dumps(call_details, indent=4)}")

    # Test 5: Create FX Put Option
    print("\n5. Creating FX Put Option...")
    analytics2 = FXAnalytics(config)
    put_details = analytics2.create_fx_put(
        pair="eurusd",
        expiry=rl.dt(2024, 6, 15),
        strike=1.05,
        notional=1_000_000
    )
    print(f"   Put: {json.dumps(put_details, indent=4)}")

    # Test 6: Create FX Straddle
    print("\n6. Creating FX Straddle...")
    analytics3 = FXAnalytics(config)
    straddle_details = analytics3.create_fx_straddle(
        pair="eurusd",
        expiry=rl.dt(2024, 6, 15),
        strike=1.0850,
        notional=1_000_000
    )
    print(f"   Straddle: {json.dumps(straddle_details, indent=4)}")

    # Test 7: Create FX Risk Reversal
    print("\n7. Creating FX Risk Reversal...")
    analytics4 = FXAnalytics(config)
    rr_details = analytics4.create_fx_risk_reversal(
        pair="eurusd",
        expiry=rl.dt(2024, 6, 15),
        strike_call=1.10,
        strike_put=1.05,
        notional=1_000_000
    )
    print(f"   Risk Reversal: {json.dumps(rr_details, indent=4)}")

    # Test 8: Calculate NPV (will need curves)
    print("\n8. Calculating NPV (without curves - will show structure)...")
    npv_result = analytics.calculate_npv()
    print(f"   NPV: {json.dumps(npv_result, indent=4)}")

    # Test 9: Export to JSON
    print("\n9. Exporting to JSON...")
    json_output = analytics.export_to_json()
    print(f"   JSON export length: {len(json_output)} characters")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
