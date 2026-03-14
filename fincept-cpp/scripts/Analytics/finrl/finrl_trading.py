"""
FinRL Paper & Live Trading
Wrapper around finrl.trade and finrl.meta.env_stock_trading.env_stock_papertrading.

Covers:
- Alpaca paper trading with trained DRL agents
- Live trading deployment
- Trade mode backtesting (via finrl.trade)
- Crypto trading with CCXT
- Trading session management

Usage (CLI):
    python finrl_trading.py --action list_modes
    python finrl_trading.py --action paper_trade --algorithm ppo --tickers AAPL MSFT GOOGL --api_key YOUR_KEY --api_secret YOUR_SECRET
    python finrl_trading.py --action trade_backtest --algorithm ppo --tickers AAPL MSFT --trade_start 2023-01-01 --trade_end 2023-12-31
    python finrl_trading.py --action crypto_config --pairs BTC/USDT ETH/USDT
"""

import json
import sys
import os
import argparse

from finrl.trade import trade as finrl_trade
from finrl.config import (
    INDICATORS,
    ALPACA_API_KEY, ALPACA_API_SECRET, ALPACA_API_BASE_URL,
    TRADE_START_DATE, TRADE_END_DATE,
    TRAINED_MODEL_DIR,
)


# ============================================================================
# Trade Mode Registry
# ============================================================================

TRADE_MODES = {
    "paper_alpaca": {
        "description": "Paper trading on Alpaca Markets",
        "requirements": ["Alpaca API key & secret", "Trained model"],
        "supported_assets": "US equities",
        "market_hours": "9:30 AM - 4:00 PM ET",
        "features": [
            "Real-time market data via Alpaca API",
            "Automatic order submission",
            "Position tracking",
            "Turbulence-based risk management",
            "Latency testing",
        ],
    },
    "backtesting": {
        "description": "Trade-mode backtesting with historical data",
        "requirements": ["Trained model", "Data source"],
        "supported_assets": "Any supported data source",
        "features": [
            "Replay historical data",
            "Evaluate trained models",
            "Multiple data sources",
        ],
    },
    "crypto_ccxt": {
        "description": "Cryptocurrency trading via CCXT",
        "requirements": ["Exchange API credentials", "CCXT library"],
        "supported_assets": "100+ crypto exchanges",
        "features": [
            "24/7 trading",
            "Multiple exchanges",
            "Sub-minute data",
        ],
    },
}


# ============================================================================
# Paper Trading
# ============================================================================

def setup_paper_trading(
    algorithm: str = "ppo",
    tickers: list = None,
    time_interval: str = "1Min",
    tech_indicator_list: list = None,
    turbulence_threshold: float = 30.0,
    max_stock: float = 100.0,
    api_key: str = None,
    api_secret: str = None,
    api_base_url: str = None,
    model_dir: str = None,
    net_dim: int = 512,
    drl_lib: str = "stable_baselines3",
) -> dict:
    """
    Configure Alpaca paper trading session.
    Returns configuration dict (does not start trading).
    """
    if tickers is None:
        tickers = ["AAPL", "MSFT", "GOOGL"]
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)
    if api_key is None:
        api_key = ALPACA_API_KEY
    if api_secret is None:
        api_secret = ALPACA_API_SECRET
    if api_base_url is None:
        api_base_url = ALPACA_API_BASE_URL
    if model_dir is None:
        model_dir = TRAINED_MODEL_DIR

    stock_dim = len(tickers)
    state_dim = 1 + 2 * stock_dim + len(tech_indicator_list) * stock_dim
    action_dim = stock_dim

    config = {
        "algorithm": algorithm,
        "drl_lib": drl_lib,
        "tickers": tickers,
        "stock_dim": stock_dim,
        "state_dim": state_dim,
        "action_dim": action_dim,
        "time_interval": time_interval,
        "tech_indicators": tech_indicator_list,
        "turbulence_threshold": turbulence_threshold,
        "max_stock": max_stock,
        "net_dim": net_dim,
        "model_dir": model_dir,
        "api_base_url": api_base_url,
        "api_key_configured": bool(api_key and api_key != "xxx"),
        "api_secret_configured": bool(api_secret and api_secret != "xxx"),
    }

    return {"status": "configured", "config": config}


def start_paper_trading(
    algorithm: str = "ppo",
    tickers: list = None,
    time_interval: str = "1Min",
    tech_indicator_list: list = None,
    turbulence_threshold: float = 30.0,
    max_stock: float = 100.0,
    api_key: str = None,
    api_secret: str = None,
    api_base_url: str = None,
    model_dir: str = None,
    net_dim: int = 512,
    drl_lib: str = "stable_baselines3",
) -> dict:
    """
    Start Alpaca paper trading with a trained DRL agent.
    WARNING: This starts a live trading loop.
    """
    if tickers is None:
        tickers = ["AAPL", "MSFT", "GOOGL"]
    if tech_indicator_list is None:
        tech_indicator_list = list(INDICATORS)
    if api_key is None or api_key == "xxx":
        return {"error": "Valid Alpaca API key required. Set --api_key"}
    if api_secret is None or api_secret == "xxx":
        return {"error": "Valid Alpaca API secret required. Set --api_secret"}
    if api_base_url is None:
        api_base_url = ALPACA_API_BASE_URL
    if model_dir is None:
        model_dir = TRAINED_MODEL_DIR

    stock_dim = len(tickers)
    state_dim = 1 + 2 * stock_dim + len(tech_indicator_list) * stock_dim
    action_dim = stock_dim

    try:
        from finrl.meta.env_stock_trading.env_stock_papertrading import AlpacaPaperTrading

        paper_trader = AlpacaPaperTrading(
            ticker_list=tickers,
            time_interval=time_interval,
            drl_lib=drl_lib,
            agent=algorithm,
            cwd=model_dir,
            net_dim=net_dim,
            state_dim=state_dim,
            action_dim=action_dim,
            API_KEY=api_key,
            API_SECRET=api_secret,
            API_BASE_URL=api_base_url,
            tech_indicator_list=tech_indicator_list,
            turbulence_thresh=turbulence_threshold,
            max_stock=max_stock,
        )

        paper_trader.run()

        return {"status": "trading_session_ended"}

    except Exception as e:
        return {"error": str(e), "status": "failed"}


# ============================================================================
# Trade-mode Backtesting
# ============================================================================

def trade_backtest(
    algorithm: str = "ppo",
    tickers: list = None,
    trade_start: str = None,
    trade_end: str = None,
    data_source: str = "yahoofinance",
    time_interval: str = "1D",
    indicators: list = None,
    if_vix: bool = True,
    api_key: str = "",
    api_secret: str = "",
    api_base_url: str = "",
    **kwargs,
) -> dict:
    """Run trade-mode backtesting via finrl.trade."""
    if tickers is None:
        tickers = ["AAPL", "MSFT", "GOOGL"]
    if indicators is None:
        indicators = list(INDICATORS)
    if trade_start is None:
        trade_start = TRADE_START_DATE
    if trade_end is None:
        trade_end = TRADE_END_DATE

    from finrl.meta.env_stock_trading.env_stocktrading import StockTradingEnv

    try:
        result = finrl_trade(
            start_date=trade_start,
            end_date=trade_end,
            ticker_list=tickers,
            data_source=data_source,
            time_interval=time_interval,
            technical_indicator_list=indicators,
            drl_lib="stable_baselines3",
            env=StockTradingEnv,
            model_name=algorithm.lower(),
            API_KEY=api_key,
            API_SECRET=api_secret,
            API_BASE_URL=api_base_url,
            trade_mode="backtesting",
            if_vix=if_vix,
            **kwargs,
        )

        return {
            "status": "success",
            "algorithm": algorithm,
            "trade_period": f"{trade_start} to {trade_end}",
            "tickers": tickers,
            "data_source": data_source,
        }

    except Exception as e:
        return {"error": str(e), "status": "failed"}


# ============================================================================
# Crypto Trading Config
# ============================================================================

def build_crypto_config(
    pairs: list = None,
    exchange: str = "binance",
    initial_capital: float = 1_000_000,
    lookback: int = 1,
    buy_cost_pct: float = 0.001,
    sell_cost_pct: float = 0.001,
    time_interval: str = "1h",
) -> dict:
    """Build configuration for crypto trading environment."""
    if pairs is None:
        pairs = ["BTC/USDT", "ETH/USDT"]

    return {
        "pairs": pairs,
        "exchange": exchange,
        "num_assets": len(pairs),
        "initial_capital": initial_capital,
        "lookback": lookback,
        "buy_cost_pct": buy_cost_pct,
        "sell_cost_pct": sell_cost_pct,
        "time_interval": time_interval,
        "data_source": "ccxt",
        "note": "Use finrl_data.py to download crypto data, then finrl_agents.py to train",
    }


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="FinRL Trading Manager")
    parser.add_argument("--action", required=True,
                        choices=["list_modes", "setup_paper", "start_paper",
                                 "trade_backtest", "crypto_config"])
    parser.add_argument("--algorithm", default="ppo")
    parser.add_argument("--tickers", nargs="*", default=["AAPL", "MSFT", "GOOGL"])
    parser.add_argument("--pairs", nargs="*", default=["BTC/USDT", "ETH/USDT"])
    parser.add_argument("--time_interval", default="1Min")
    parser.add_argument("--api_key", default=None)
    parser.add_argument("--api_secret", default=None)
    parser.add_argument("--api_base_url", default=ALPACA_API_BASE_URL)
    parser.add_argument("--model_dir", default=TRAINED_MODEL_DIR)
    parser.add_argument("--trade_start", default=TRADE_START_DATE)
    parser.add_argument("--trade_end", default=TRADE_END_DATE)
    parser.add_argument("--source", default="yahoofinance")
    parser.add_argument("--turbulence_threshold", type=float, default=30.0)
    parser.add_argument("--max_stock", type=float, default=100.0)
    parser.add_argument("--exchange", default="binance")
    parser.add_argument("--initial_capital", type=float, default=1_000_000)
    parser.add_argument("--indicators", nargs="*")

    args = parser.parse_args()

    if args.action == "list_modes":
        result = TRADE_MODES

    elif args.action == "setup_paper":
        result = setup_paper_trading(
            algorithm=args.algorithm, tickers=args.tickers,
            time_interval=args.time_interval,
            tech_indicator_list=args.indicators,
            turbulence_threshold=args.turbulence_threshold,
            max_stock=args.max_stock,
            api_key=args.api_key, api_secret=args.api_secret,
            api_base_url=args.api_base_url,
            model_dir=args.model_dir,
        )

    elif args.action == "start_paper":
        result = start_paper_trading(
            algorithm=args.algorithm, tickers=args.tickers,
            time_interval=args.time_interval,
            tech_indicator_list=args.indicators,
            turbulence_threshold=args.turbulence_threshold,
            max_stock=args.max_stock,
            api_key=args.api_key, api_secret=args.api_secret,
            api_base_url=args.api_base_url,
            model_dir=args.model_dir,
        )

    elif args.action == "trade_backtest":
        result = trade_backtest(
            algorithm=args.algorithm, tickers=args.tickers,
            trade_start=args.trade_start, trade_end=args.trade_end,
            data_source=args.source, indicators=args.indicators,
        )

    elif args.action == "crypto_config":
        result = build_crypto_config(
            pairs=args.pairs, exchange=args.exchange,
            initial_capital=args.initial_capital,
        )

    else:
        result = {"error": f"Unknown action: {args.action}"}

    print(json.dumps(result, indent=2, default=str))


if __name__ == "__main__":
    main()
