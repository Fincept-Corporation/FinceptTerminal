"""
Volatility Prediction Agent for FinceptTerminal
GARCH-based volatility forecasting with risk analysis
"""

import sys
import json
import pandas as pd
import numpy as np
from typing import Dict, List, Any, TypedDict
from datetime import datetime, timedelta

sys.path.append('/home/user/FinceptTerminal/fincept-terminal-desktop/src-tauri/resources/scripts')

from Analytics.prediction.volatility_forecaster import VolatilityForecaster


class AgentState(TypedDict):
    """State for agent execution"""
    messages: List[str]
    data: Dict[str, Any]
    tickers: List[str]
    volatility_forecasts: Dict[str, Any]
    risk_metrics: Dict[str, Any]
    reasoning: List[str]


def fetch_price_data(ticker: str, days: int = 365) -> pd.Series:
    """Fetch price data for volatility analysis"""
    try:
        import yfinance as yf

        end_date = datetime.now()
        start_date = end_date - timedelta(days=days)

        stock = yf.Ticker(ticker)
        hist = stock.history(start=start_date, end=end_date)

        return hist['Close']

    except:
        # Synthetic data
        dates = pd.date_range(end=datetime.now(), periods=days, freq='D')
        np.random.seed(hash(ticker) % 2**32)
        prices = 100 * (1 + np.random.normal(0.001, 0.02, days)).cumprod()
        return pd.Series(prices, index=dates)


def volatility_prediction_agent(state: AgentState) -> AgentState:
    """
    Volatility Prediction Agent

    Forecasts future volatility using GARCH models
    Provides VaR and risk metrics

    Args:
        state: Current agent state

    Returns:
        Updated state with volatility forecasts
    """
    messages = state.get("messages", [])
    data = state.get("data", {})
    tickers = state.get("tickers", data.get("tickers", ["SPY"]))

    volatility_forecasts = {}
    risk_metrics = {}
    reasoning = []

    for ticker in tickers:
        try:
            # Fetch price data
            prices = fetch_price_data(ticker, days=365)

            if len(prices) < 100:
                reasoning.append(f"⚠️ {ticker}: Insufficient data for volatility analysis")
                continue

            # Initialize forecaster
            forecaster = VolatilityForecaster(model_type='garch')

            # Calculate returns
            returns = forecaster.calculate_returns(prices, method='log')

            messages.append(f"📊 Fitting GARCH model for {ticker}...")

            # Fit GARCH model
            fit_result = forecaster.fit_garch(returns, p=1, q=1, vol='GARCH')

            if 'error' in fit_result:
                reasoning.append(f"❌ {ticker}: GARCH fitting failed - {fit_result['error']}")
                continue

            # Generate volatility forecast
            vol_forecast = forecaster.predict_volatility(horizon=30)

            if 'error' in vol_forecast:
                reasoning.append(f"❌ {ticker}: Volatility forecast failed")
                continue

            # Calculate VaR
            var_95 = forecaster.calculate_var(
                confidence_level=0.95,
                horizon=1,
                portfolio_value=100000
            )

            var_99 = forecaster.calculate_var(
                confidence_level=0.99,
                horizon=1,
                portfolio_value=100000
            )

            # Detect volatility regime
            regime = forecaster.detect_volatility_regime(prices)

            # Calculate historical volatility
            hist_vol = forecaster.calculate_historical_volatility(prices, window=20)

            # Store results
            volatility_forecasts[ticker] = vol_forecast
            risk_metrics[ticker] = {
                'var_95': var_95,
                'var_99': var_99,
                'regime': regime,
                'historical': hist_vol
            }

            # Generate reasoning
            current_vol = vol_forecast['current_volatility']
            mean_forecast_vol = vol_forecast['mean_forecast_vol']
            max_forecast_vol = vol_forecast['max_forecast_vol']

            vol_change = (mean_forecast_vol - current_vol) / current_vol * 100

            reasoning.append(f"""
🌊 **{ticker} Volatility Forecast**

📊 **Current Volatility**: {current_vol:.2f}% (annualized)

**30-Day Forecast:**
- Mean Volatility: {mean_forecast_vol:.2f}% ({vol_change:+.1f}%)
- Max Volatility: {max_forecast_vol:.2f}%
- Min Volatility: {vol_forecast['min_forecast_vol']:.2f}%

**Volatility Regime**: {regime['regime']} 🎯
- {regime['description']}

**Risk Metrics (Portfolio: $100,000):**
- VaR (95%, 1-day): ${var_95['var_dollar']:,.0f} ({var_95['var_percent']:.2f}%)
- VaR (99%, 1-day): ${var_99['var_dollar']:,.0f} ({var_99['var_percent']:.2f}%)

**GARCH Model:**
- AIC: {fit_result['aic']:.2f}
- BIC: {fit_result['bic']:.2f}

**Interpretation:**
{generate_volatility_interpretation(current_vol, mean_forecast_vol, regime['regime'])}
""")

        except Exception as e:
            reasoning.append(f"❌ {ticker}: Error - {str(e)}")

    # Update state
    state["volatility_forecasts"] = volatility_forecasts
    state["risk_metrics"] = risk_metrics
    state["reasoning"] = reasoning
    state["messages"] = messages

    return state


def generate_volatility_interpretation(current_vol: float, forecast_vol: float, regime: str) -> str:
    """Generate volatility interpretation"""
    vol_change = forecast_vol - current_vol

    if vol_change > 2:
        trend = "📈 **Increasing Volatility Expected**"
        advice = "Consider protective strategies (options hedging, stop losses)"
    elif vol_change < -2:
        trend = "📉 **Decreasing Volatility Expected**"
        advice = "Stable conditions ahead - good for directional strategies"
    else:
        trend = "➡️ **Stable Volatility Expected**"
        advice = "Current conditions likely to persist"

    if regime == 'HIGH':
        risk_level = "⚠️ **High Risk**: Exercise caution, reduce position sizes"
    elif regime == 'LOW':
        risk_level = "✅ **Low Risk**: Favorable conditions for new positions"
    else:
        risk_level = "⚡ **Moderate Risk**: Normal market conditions"

    return f"{trend}\n{advice}\n\n{risk_level}"


def portfolio_volatility_agent(state: AgentState) -> AgentState:
    """
    Portfolio-level volatility analysis
    Analyzes correlations and portfolio-wide risk
    """
    # First run individual volatility predictions
    state = volatility_prediction_agent(state)

    volatility_forecasts = state.get("volatility_forecasts", {})
    reasoning = state.get("reasoning", [])

    if len(volatility_forecasts) > 1:
        reasoning.append("\n📊 **Portfolio Volatility Analysis**\n")

        # Extract volatility data
        vol_data = []
        for ticker, forecast in volatility_forecasts.items():
            vol_data.append({
                'ticker': ticker,
                'current_vol': forecast['current_volatility'],
                'forecast_vol': forecast['mean_forecast_vol']
            })

        # Sort by volatility
        vol_data.sort(key=lambda x: x['current_vol'], reverse=True)

        reasoning.append("**Stock Volatility Ranking (Highest to Lowest):**")
        for i, vol_info in enumerate(vol_data, 1):
            emoji = '⚠️' if vol_info['current_vol'] > 30 else '⚡' if vol_info['current_vol'] > 20 else '✅'
            reasoning.append(
                f"{emoji} {vol_info['ticker']}: {vol_info['current_vol']:.2f}% "
                f"→ {vol_info['forecast_vol']:.2f}% (forecast)"
            )

        # Portfolio recommendations
        high_vol_stocks = [v['ticker'] for v in vol_data if v['current_vol'] > 30]
        low_vol_stocks = [v['ticker'] for v in vol_data if v['current_vol'] < 20]

        reasoning.append("\n**Portfolio Recommendations:**")
        if high_vol_stocks:
            reasoning.append(f"⚠️ High volatility stocks (hedge recommended): {', '.join(high_vol_stocks)}")
        if low_vol_stocks:
            reasoning.append(f"✅ Low volatility stocks (stable core holdings): {', '.join(low_vol_stocks)}")

        # Calculate portfolio-level metrics
        avg_vol = np.mean([v['current_vol'] for v in vol_data])
        reasoning.append(f"\n📊 Portfolio Average Volatility: {avg_vol:.2f}%")

    state["reasoning"] = reasoning
    return state


def main():
    """
    Command-line interface
    Input JSON:
    {
        "tickers": ["AAPL", "GOOGL"],
        "mode": "single" | "portfolio"
    }
    """
    try:
        input_data = json.loads(sys.stdin.read())

        state: AgentState = {
            "messages": [],
            "data": input_data,
            "tickers": input_data.get("tickers", ["SPY"]),
            "volatility_forecasts": {},
            "risk_metrics": {},
            "reasoning": []
        }

        mode = input_data.get("mode", "portfolio")

        if mode == "portfolio":
            result_state = portfolio_volatility_agent(state)
        else:
            result_state = volatility_prediction_agent(state)

        output = {
            "success": True,
            "tickers": result_state["tickers"],
            "volatility_forecasts": result_state["volatility_forecasts"],
            "risk_metrics": result_state["risk_metrics"],
            "reasoning": "\n".join(result_state["reasoning"]),
            "timestamp": str(datetime.now())
        }

        print(json.dumps(output))

    except Exception as e:
        print(json.dumps({
            "success": False,
            "error": str(e)
        }))


if __name__ == "__main__":
    main()
