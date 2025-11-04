"""
Volatility Forecasting Module for FinceptTerminal
Implements GARCH, EGARCH, and other volatility models
"""

import numpy as np
import pandas as pd
from typing import Dict, List, Tuple, Optional
import json
import sys
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

# Volatility models
try:
    from arch import arch_model
    from arch.univariate import GARCH, EGARCH, ConstantMean, ZeroMean
except ImportError:
    arch_model = None


class VolatilityForecaster:
    """
    Volatility forecasting using GARCH family models
    Supports GARCH, EGARCH, GJR-GARCH
    """

    def __init__(self, model_type: str = 'garch'):
        """
        Initialize volatility forecaster

        Args:
            model_type: Type of model ('garch', 'egarch', 'gjr-garch')
        """
        self.model_type = model_type.lower()
        self.model = None
        self.fitted_model = None
        self.returns = None
        self.is_fitted = False

    def calculate_returns(self, prices: pd.Series, method: str = 'log') -> pd.Series:
        """
        Calculate returns from prices

        Args:
            prices: Price series
            method: 'simple' or 'log' returns

        Returns:
            Returns series
        """
        if method == 'log':
            returns = np.log(prices / prices.shift(1))
        else:
            returns = prices.pct_change()

        return returns.dropna() * 100  # Convert to percentage

    def fit_garch(self, returns: pd.Series, p: int = 1, q: int = 1,
                  mean_model: str = 'Constant', vol: str = 'GARCH') -> Dict:
        """
        Fit GARCH model

        Args:
            returns: Return series
            p: GARCH lag order
            q: ARCH lag order
            mean_model: Mean model ('Constant', 'Zero', 'AR')
            vol: Volatility model ('GARCH', 'EGARCH', 'GJR-GARCH')

        Returns:
            Fitting results
        """
        if arch_model is None:
            return {"error": "arch library not installed. Install with: pip install arch"}

        self.returns = returns

        try:
            # Create and fit model
            self.model = arch_model(
                returns,
                mean=mean_model,
                vol=vol,
                p=p,
                q=q,
                rescale=False
            )

            self.fitted_model = self.model.fit(disp='off', show_warning=False)
            self.is_fitted = True

            return {
                'success': True,
                'model_type': vol,
                'p': p,
                'q': q,
                'aic': float(self.fitted_model.aic),
                'bic': float(self.fitted_model.bic),
                'log_likelihood': float(self.fitted_model.loglikelihood),
                'params': {k: float(v) for k, v in self.fitted_model.params.items()},
                'summary': str(self.fitted_model.summary())
            }

        except Exception as e:
            return {'error': str(e)}

    def fit_egarch(self, returns: pd.Series, p: int = 1, q: int = 1) -> Dict:
        """
        Fit EGARCH model (captures asymmetric volatility)

        Args:
            returns: Return series
            p: EGARCH lag order
            q: ARCH lag order

        Returns:
            Fitting results
        """
        return self.fit_garch(returns, p=p, q=q, vol='EGARCH')

    def fit_gjr_garch(self, returns: pd.Series, p: int = 1, o: int = 1, q: int = 1) -> Dict:
        """
        Fit GJR-GARCH model (threshold GARCH for asymmetry)

        Args:
            returns: Return series
            p: GARCH lag order
            o: Asymmetric term order
            q: ARCH lag order

        Returns:
            Fitting results
        """
        if arch_model is None:
            return {"error": "arch library not installed"}

        self.returns = returns

        try:
            self.model = arch_model(
                returns,
                mean='Constant',
                p=p,
                o=o,
                q=q,
                rescale=False
            )

            self.fitted_model = self.model.fit(disp='off', show_warning=False)
            self.is_fitted = True

            return {
                'success': True,
                'model_type': 'GJR-GARCH',
                'p': p,
                'o': o,
                'q': q,
                'aic': float(self.fitted_model.aic),
                'bic': float(self.fitted_model.bic)
            }

        except Exception as e:
            return {'error': str(e)}

    def predict_volatility(self, horizon: int = 30) -> Dict:
        """
        Forecast future volatility

        Args:
            horizon: Forecast horizon in days

        Returns:
            Volatility predictions
        """
        if not self.is_fitted:
            return {'error': 'Model not fitted. Call fit method first.'}

        try:
            # Generate forecast
            forecasts = self.fitted_model.forecast(horizon=horizon, reindex=False)

            # Extract variance forecasts
            variance_forecast = forecasts.variance.values[-1, :]

            # Convert variance to volatility (annualized)
            volatility_forecast = np.sqrt(variance_forecast) * np.sqrt(252)

            # Current volatility
            current_vol = float(np.sqrt(self.fitted_model.conditional_volatility.iloc[-1]**2) * np.sqrt(252))

            # Create forecast dates
            if isinstance(self.returns.index, pd.DatetimeIndex):
                last_date = self.returns.index[-1]
                forecast_dates = pd.date_range(
                    start=last_date + timedelta(days=1),
                    periods=horizon,
                    freq='D'
                )
                forecast_dates = [str(d.date()) for d in forecast_dates]
            else:
                forecast_dates = [f"T+{i+1}" for i in range(horizon)]

            return {
                'success': True,
                'model_type': self.model_type,
                'current_volatility': current_vol,
                'forecast_horizon': horizon,
                'volatility_forecast': volatility_forecast.tolist(),
                'forecast_dates': forecast_dates,
                'mean_forecast_vol': float(np.mean(volatility_forecast)),
                'max_forecast_vol': float(np.max(volatility_forecast)),
                'min_forecast_vol': float(np.min(volatility_forecast))
            }

        except Exception as e:
            return {'error': str(e)}

    def calculate_var(self, confidence_level: float = 0.95, horizon: int = 1,
                      portfolio_value: float = 1000000) -> Dict:
        """
        Calculate Value at Risk using volatility forecast

        Args:
            confidence_level: Confidence level (0.95 or 0.99)
            horizon: Time horizon in days
            portfolio_value: Portfolio value

        Returns:
            VaR estimate
        """
        if not self.is_fitted:
            return {'error': 'Model not fitted'}

        try:
            # Get volatility forecast
            vol_forecast = self.predict_volatility(horizon=horizon)

            if 'error' in vol_forecast:
                return vol_forecast

            # Calculate VaR
            volatility = vol_forecast['volatility_forecast'][0] / 100  # Daily vol

            # Z-score for confidence level
            if confidence_level == 0.95:
                z_score = 1.645
            elif confidence_level == 0.99:
                z_score = 2.326
            else:
                z_score = 1.645

            var_dollar = portfolio_value * z_score * volatility * np.sqrt(horizon)
            var_percent = z_score * volatility * np.sqrt(horizon) * 100

            return {
                'success': True,
                'var_dollar': float(var_dollar),
                'var_percent': float(var_percent),
                'confidence_level': confidence_level,
                'horizon_days': horizon,
                'portfolio_value': portfolio_value,
                'forecast_volatility': float(volatility * 100)
            }

        except Exception as e:
            return {'error': str(e)}

    def calculate_historical_volatility(self, prices: pd.Series, window: int = 20) -> Dict:
        """
        Calculate rolling historical volatility

        Args:
            prices: Price series
            window: Rolling window size

        Returns:
            Historical volatility metrics
        """
        returns = self.calculate_returns(prices, method='log')

        # Rolling volatility (annualized)
        rolling_vol = returns.rolling(window=window).std() * np.sqrt(252)

        # Parkinson's volatility (using high-low)
        # Note: Requires OHLC data, simplified here

        return {
            'success': True,
            'current_volatility': float(rolling_vol.iloc[-1]),
            'mean_volatility': float(rolling_vol.mean()),
            'max_volatility': float(rolling_vol.max()),
            'min_volatility': float(rolling_vol.min()),
            'rolling_volatility': rolling_vol.tolist(),
            'window': window
        }

    def detect_volatility_regime(self, prices: pd.Series,
                                  low_threshold: float = 15,
                                  high_threshold: float = 30) -> Dict:
        """
        Detect volatility regime (low, normal, high)

        Args:
            prices: Price series
            low_threshold: Low volatility threshold (%)
            high_threshold: High volatility threshold (%)

        Returns:
            Volatility regime classification
        """
        hist_vol = self.calculate_historical_volatility(prices, window=20)
        current_vol = hist_vol['current_volatility']

        if current_vol < low_threshold:
            regime = 'LOW'
            description = 'Low volatility - potential breakout ahead'
        elif current_vol > high_threshold:
            regime = 'HIGH'
            description = 'High volatility - increased risk'
        else:
            regime = 'NORMAL'
            description = 'Normal volatility - stable conditions'

        return {
            'success': True,
            'regime': regime,
            'current_volatility': float(current_vol),
            'description': description,
            'thresholds': {
                'low': low_threshold,
                'high': high_threshold
            }
        }

    def calculate_realized_volatility(self, returns: pd.Series, frequency: str = 'daily') -> Dict:
        """
        Calculate realized volatility

        Args:
            returns: Return series
            frequency: Data frequency ('daily', 'intraday')

        Returns:
            Realized volatility
        """
        # Annualization factor
        if frequency == 'daily':
            annualization = np.sqrt(252)
        elif frequency == 'intraday':
            annualization = np.sqrt(252 * 390)  # 390 minutes in trading day
        else:
            annualization = np.sqrt(252)

        realized_vol = returns.std() * annualization

        return {
            'success': True,
            'realized_volatility': float(realized_vol),
            'frequency': frequency,
            'annualization_factor': float(annualization),
            'sample_size': len(returns)
        }


class ImpliedVolatilityCalculator:
    """
    Calculate implied volatility from options prices
    (Simplified Black-Scholes based)
    """

    @staticmethod
    def black_scholes_call(S: float, K: float, T: float, r: float, sigma: float) -> float:
        """
        Black-Scholes call option price

        Args:
            S: Spot price
            K: Strike price
            T: Time to maturity (years)
            r: Risk-free rate
            sigma: Volatility

        Returns:
            Call option price
        """
        from scipy.stats import norm

        d1 = (np.log(S / K) + (r + 0.5 * sigma**2) * T) / (sigma * np.sqrt(T))
        d2 = d1 - sigma * np.sqrt(T)

        call_price = S * norm.cdf(d1) - K * np.exp(-r * T) * norm.cdf(d2)
        return call_price

    @staticmethod
    def implied_volatility(option_price: float, S: float, K: float,
                          T: float, r: float, option_type: str = 'call') -> float:
        """
        Calculate implied volatility using Newton-Raphson

        Args:
            option_price: Market option price
            S: Spot price
            K: Strike price
            T: Time to maturity
            r: Risk-free rate
            option_type: 'call' or 'put'

        Returns:
            Implied volatility
        """
        try:
            from scipy.optimize import brentq

            def objective(sigma):
                if option_type == 'call':
                    return ImpliedVolatilityCalculator.black_scholes_call(S, K, T, r, sigma) - option_price
                else:
                    # Put-call parity for put
                    call = ImpliedVolatilityCalculator.black_scholes_call(S, K, T, r, sigma)
                    put = call - S + K * np.exp(-r * T)
                    return put - option_price

            # Find implied vol between 0.01 and 500%
            iv = brentq(objective, 0.01, 5.0)
            return iv

        except:
            return np.nan


def main():
    """
    Command-line interface
    Input JSON:
    {
        "prices": [...],
        "dates": [...],
        "action": "fit_garch" | "predict" | "var" | "regime",
        "params": {...}
    }
    """
    try:
        input_data = json.loads(sys.stdin.read())

        prices = pd.Series(
            input_data.get('prices', []),
            index=pd.to_datetime(input_data.get('dates', []))
        )

        action = input_data.get('action', 'fit_garch')
        params = input_data.get('params', {})

        forecaster = VolatilityForecaster()

        result = {}

        if action == 'fit_garch':
            returns = forecaster.calculate_returns(prices)
            fit_result = forecaster.fit_garch(
                returns,
                p=params.get('p', 1),
                q=params.get('q', 1),
                vol=params.get('vol', 'GARCH')
            )
            result['fit'] = fit_result

            if 'success' in fit_result:
                pred_result = forecaster.predict_volatility(
                    horizon=params.get('horizon', 30)
                )
                result['predictions'] = pred_result

        elif action == 'predict':
            returns = forecaster.calculate_returns(prices)
            forecaster.fit_garch(returns)
            result = forecaster.predict_volatility(
                horizon=params.get('horizon', 30)
            )

        elif action == 'var':
            returns = forecaster.calculate_returns(prices)
            forecaster.fit_garch(returns)
            result = forecaster.calculate_var(
                confidence_level=params.get('confidence_level', 0.95),
                horizon=params.get('horizon', 1),
                portfolio_value=params.get('portfolio_value', 1000000)
            )

        elif action == 'regime':
            result = forecaster.detect_volatility_regime(
                prices,
                low_threshold=params.get('low_threshold', 15),
                high_threshold=params.get('high_threshold', 30)
            )

        elif action == 'historical':
            result = forecaster.calculate_historical_volatility(
                prices,
                window=params.get('window', 20)
            )

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({'error': str(e)}))


if __name__ == '__main__':
    main()
