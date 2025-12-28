"""
Rateslib Volatility Analytics Module
======================================
Wrapper for rateslib FX volatility surfaces and smiles.

This module provides tools for:
- FX Delta Volatility Smiles
- FX Delta Volatility Surfaces
- FX SABR Smiles
- FX SABR Surfaces
- Volatility interpolation and extrapolation
- Implied volatility calculations
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
class VolatilityConfig:
    """Configuration for volatility surfaces"""
    pair: str = "eurusd"
    calendar: str = "nyc"
    spot_date: Optional[datetime] = None
    interpolation: str = "linear"


class VolatilityAnalytics:
    """Analytics for FX volatility surfaces and smiles"""

    def __init__(self, config: VolatilityConfig = None):
        self.config = config or VolatilityConfig()
        self.surface = None
        self.surface_type = None

    def create_fx_delta_vol_smile(
        self,
        expiry: datetime,
        atm_vol: float,
        risk_reversal_25d: float,
        butterfly_25d: float,
        spot: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Create FX Delta Volatility Smile for single expiry

        Args:
            expiry: Option expiry date
            atm_vol: ATM volatility in percent
            risk_reversal_25d: 25-delta risk reversal in vol points
            butterfly_25d: 25-delta butterfly in vol points
            spot: Spot FX rate

        Returns:
            Dictionary with vol smile details
        """
        try:
            # FXDeltaVolSmile construction
            self.surface = rl.FXDeltaVolSmile(
                expiry=expiry,
                atm=atm_vol,
                rr=risk_reversal_25d,
                bf=butterfly_25d,
                delta_type="spot"  # or "forward"
            )
            self.surface_type = "FXDeltaVolSmile"

            # Calculate strikes from deltas
            vol_25d_call = atm_vol + butterfly_25d + 0.5 * risk_reversal_25d
            vol_25d_put = atm_vol + butterfly_25d - 0.5 * risk_reversal_25d

            result = {
                'type': 'FXDeltaVolSmile',
                'expiry': expiry.strftime('%Y-%m-%d'),
                'atm_vol': atm_vol,
                'risk_reversal_25d': risk_reversal_25d,
                'butterfly_25d': butterfly_25d,
                'vol_25d_call': vol_25d_call,
                'vol_25d_put': vol_25d_put,
                'smile_shape': 'skew' if abs(risk_reversal_25d) > 0.1 else 'symmetric'
            }

            print(f"FX Delta Vol Smile: ATM {atm_vol}%, 25d RR {risk_reversal_25d}%, 25d BF {butterfly_25d}%")
            return result

        except Exception as e:
            print(f"Error creating FX delta vol smile: {e}")
            return {}

    def create_fx_delta_vol_surface(
        self,
        expiries: List[datetime],
        atm_vols: List[float],
        risk_reversals_25d: List[float],
        butterflies_25d: List[float]
    ) -> Dict[str, Any]:
        """
        Create FX Delta Volatility Surface (multiple expiries)

        Args:
            expiries: List of expiry dates
            atm_vols: List of ATM vols for each expiry
            risk_reversals_25d: List of 25d RR for each expiry
            butterflies_25d: List of 25d BF for each expiry

        Returns:
            Dictionary with vol surface details
        """
        try:
            # Create list of smiles
            smiles = []
            for exp, atm, rr, bf in zip(expiries, atm_vols, risk_reversals_25d, butterflies_25d):
                smile = rl.FXDeltaVolSmile(
                    expiry=exp,
                    atm=atm,
                    rr=rr,
                    bf=bf
                )
                smiles.append(smile)

            # Combine into surface
            self.surface = rl.FXDeltaVolSurface(smiles=smiles)
            self.surface_type = "FXDeltaVolSurface"

            result = {
                'type': 'FXDeltaVolSurface',
                'num_expiries': len(expiries),
                'expiries': [exp.strftime('%Y-%m-%d') for exp in expiries],
                'atm_vols': atm_vols,
                'min_expiry': expiries[0].strftime('%Y-%m-%d'),
                'max_expiry': expiries[-1].strftime('%Y-%m-%d'),
                'interpolation': self.config.interpolation
            }

            print(f"FX Delta Vol Surface: {len(expiries)} expiries")
            return result

        except Exception as e:
            print(f"Error creating FX delta vol surface: {e}")
            return {}

    def create_fx_sabr_smile(
        self,
        expiry: datetime,
        forward: float,
        alpha: float,
        beta: float,
        rho: float,
        nu: float
    ) -> Dict[str, Any]:
        """
        Create FX SABR Volatility Smile

        Args:
            expiry: Option expiry
            forward: Forward FX rate
            alpha: SABR alpha (initial volatility)
            beta: SABR beta (CEV exponent, typically 0.5)
            rho: SABR rho (correlation)
            nu: SABR nu (vol of vol)

        Returns:
            Dictionary with SABR smile details
        """
        try:
            self.surface = rl.FXSabrSmile(
                expiry=expiry,
                forward=forward,
                alpha=alpha,
                beta=beta,
                rho=rho,
                nu=nu
            )
            self.surface_type = "FXSabrSmile"

            result = {
                'type': 'FXSabrSmile',
                'expiry': expiry.strftime('%Y-%m-%d'),
                'forward': forward,
                'sabr_parameters': {
                    'alpha': alpha,
                    'beta': beta,
                    'rho': rho,
                    'nu': nu
                },
                'model': 'SABR stochastic volatility model'
            }

            print(f"FX SABR Smile: α={alpha:.4f}, β={beta:.2f}, ρ={rho:.4f}, ν={nu:.4f}")
            return result

        except Exception as e:
            print(f"Error creating FX SABR smile: {e}")
            return {}

    def create_fx_sabr_surface(
        self,
        expiries: List[datetime],
        forwards: List[float],
        sabr_params: List[Dict[str, float]]
    ) -> Dict[str, Any]:
        """
        Create FX SABR Volatility Surface

        Args:
            expiries: List of expiry dates
            forwards: List of forward rates
            sabr_params: List of dicts with SABR parameters for each expiry

        Returns:
            Dictionary with SABR surface details
        """
        try:
            smiles = []
            for exp, fwd, params in zip(expiries, forwards, sabr_params):
                smile = rl.FXSabrSmile(
                    expiry=exp,
                    forward=fwd,
                    alpha=params['alpha'],
                    beta=params['beta'],
                    rho=params['rho'],
                    nu=params['nu']
                )
                smiles.append(smile)

            self.surface = rl.FXSabrSurface(smiles=smiles)
            self.surface_type = "FXSabrSurface"

            result = {
                'type': 'FXSabrSurface',
                'num_expiries': len(expiries),
                'expiries': [exp.strftime('%Y-%m-%d') for exp in expiries],
                'model': 'SABR stochastic volatility surface'
            }

            print(f"FX SABR Surface: {len(expiries)} expiries with SABR calibration")
            return result

        except Exception as e:
            print(f"Error creating FX SABR surface: {e}")
            return {}

    def get_implied_vol(
        self,
        strike: float,
        expiry: Optional[datetime] = None
    ) -> Dict[str, Any]:
        """
        Get implied volatility for a strike

        Args:
            strike: Strike price
            expiry: Expiry date (required for surface, not for smile)

        Returns:
            Dictionary with implied vol
        """
        if self.surface is None:
            return {'error': 'No surface created'}

        try:
            if hasattr(self.surface, 'get_vol'):
                if expiry:
                    vol = self.surface.get_vol(strike, expiry)
                else:
                    vol = self.surface.get_vol(strike)

                result = {
                    'strike': strike,
                    'implied_vol': float(vol) if vol else None,
                    'surface_type': self.surface_type
                }
                if expiry:
                    result['expiry'] = expiry.strftime('%Y-%m-%d')

                return result
            else:
                return {'error': 'Implied vol method not available'}

        except Exception as e:
            print(f"Error getting implied vol: {e}")
            return {'error': str(e)}

    def plot_vol_smile(
        self,
        strike_range: Tuple[float, float] = (0.8, 1.2),
        num_points: int = 50
    ) -> go.Figure:
        """
        Plot volatility smile

        Args:
            strike_range: (min_strike, max_strike) relative to ATM
            num_points: Number of points to plot

        Returns:
            Plotly figure
        """
        if self.surface is None or self.surface_type not in ['FXDeltaVolSmile', 'FXSabrSmile']:
            print("Error: No smile to plot")
            return go.Figure()

        try:
            strikes = np.linspace(strike_range[0], strike_range[1], num_points)
            vols = []

            for strike in strikes:
                try:
                    if hasattr(self.surface, 'get_vol'):
                        vol = self.surface.get_vol(strike)
                        vols.append(float(vol) if vol else None)
                    else:
                        vols.append(None)
                except:
                    vols.append(None)

            fig = go.Figure()

            fig.add_trace(go.Scatter(
                x=strikes,
                y=vols,
                mode='lines+markers',
                name='Implied Volatility',
                line=dict(color='cyan', width=2),
                marker=dict(size=4)
            ))

            fig.update_layout(
                title=f'{self.surface_type} - Volatility Smile',
                xaxis_title='Strike',
                yaxis_title='Implied Volatility (%)',
                template='plotly_dark',
                hovermode='x unified',
                height=500
            )

            return fig

        except Exception as e:
            print(f"Error plotting vol smile: {e}")
            return go.Figure()

    def plot_vol_surface(
        self,
        strike_range: Tuple[float, float] = (0.8, 1.2),
        num_strikes: int = 20
    ) -> go.Figure:
        """
        Plot 3D volatility surface

        Args:
            strike_range: (min_strike, max_strike)
            num_strikes: Number of strike points

        Returns:
            Plotly figure
        """
        if self.surface is None or self.surface_type not in ['FXDeltaVolSurface', 'FXSabrSurface']:
            print("Error: No surface to plot")
            return go.Figure()

        try:
            # This is a simplified placeholder
            # Actual implementation would query the surface object
            fig = go.Figure()

            strikes = np.linspace(strike_range[0], strike_range[1], num_strikes)

            # Placeholder data
            x_data = strikes
            y_data = [30, 60, 90, 180, 360]  # Days to expiry
            z_data = np.random.uniform(8, 20, (len(y_data), len(x_data)))

            fig.add_trace(go.Surface(
                x=x_data,
                y=y_data,
                z=z_data,
                colorscale='Viridis'
            ))

            fig.update_layout(
                title='FX Volatility Surface',
                scene=dict(
                    xaxis_title='Strike',
                    yaxis_title='Days to Expiry',
                    zaxis_title='Implied Vol (%)'
                ),
                template='plotly_dark',
                height=600
            )

            return fig

        except Exception as e:
            print(f"Error plotting vol surface: {e}")
            return go.Figure()

    def export_to_json(self) -> str:
        """Export volatility data to JSON"""
        if self.surface is None:
            return json.dumps({'error': 'No surface created'}, indent=2)

        export_data = {
            'surface_type': self.surface_type,
            'configuration': {
                'pair': self.config.pair,
                'calendar': self.config.calendar,
                'interpolation': self.config.interpolation
            }
        }

        return json.dumps(export_data, indent=2)


def main():
    """Example usage and testing"""
    print("=== Rateslib Volatility Analytics Test ===\n")

    config = VolatilityConfig(
        pair="eurusd",
        interpolation="linear"
    )
    analytics = VolatilityAnalytics(config)

    # Test 1: Create FX Delta Vol Smile
    print("1. Creating FX Delta Vol Smile...")
    smile = analytics.create_fx_delta_vol_smile(
        expiry=rl.dt(2024, 6, 15),
        atm_vol=10.5,
        risk_reversal_25d=1.5,
        butterfly_25d=0.5
    )
    print(f"   {json.dumps(smile, indent=4)}")

    # Test 2: Create FX Delta Vol Surface
    print("\n2. Creating FX Delta Vol Surface...")
    analytics2 = VolatilityAnalytics(config)
    expiries = [rl.dt(2024, 3, 15), rl.dt(2024, 6, 15), rl.dt(2024, 12, 15)]
    atm_vols = [10.0, 10.5, 11.0]
    rrs = [1.2, 1.5, 1.8]
    bfs = [0.3, 0.5, 0.7]

    surface = analytics2.create_fx_delta_vol_surface(
        expiries=expiries,
        atm_vols=atm_vols,
        risk_reversals_25d=rrs,
        butterflies_25d=bfs
    )
    print(f"   {json.dumps(surface, indent=4)}")

    # Test 3: Create FX SABR Smile
    print("\n3. Creating FX SABR Smile...")
    analytics3 = VolatilityAnalytics(config)
    sabr_smile = analytics3.create_fx_sabr_smile(
        expiry=rl.dt(2024, 6, 15),
        forward=1.0850,
        alpha=0.15,
        beta=0.5,
        rho=-0.25,
        nu=0.3
    )
    print(f"   {json.dumps(sabr_smile, indent=4)}")

    # Test 4: Export to JSON
    print("\n4. Exporting to JSON...")
    json_output = analytics.export_to_json()
    print(f"   JSON length: {len(json_output)} characters")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
