# -*- coding: utf-8 -*-
# config.py - Production Configuration for AI Hedge Fund System

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - ticker symbols (array)
#   - end_date (string)
#   - FINANCIAL_DATASETS_API_KEY
#
# OUTPUT:
#   - Financial metrics: ROE, D/E, Current Ratio
#   - Financial line items: Revenue, Net Income, FCF, Debt, Equity, Retained Earnings, Operating Margin, R&D, Shares
#   - Market Cap
#
# PARAMETERS:
#   - period="annual"
#   - limit=10 years
"""

import os
from dataclasses import dataclass
from typing import Dict, List, Optional
import logging
from pathlib import Path


@dataclass
class APIConfig:
    """API Configuration for external services"""
    openai_api_key: str = os.getenv("OPENAI_API_KEY", "")
    anthropic_api_key: str = os.getenv("ANTHROPIC_API_KEY", "")
    finnhub_api_key: str = os.getenv("FINNHUB_API_KEY", "")
    alpha_vantage_key: str = os.getenv("ALPHA_VANTAGE_API_KEY", "")
    fred_api_key: str = os.getenv("FRED_API_KEY", "")
    newsapi_key: str = os.getenv("NEWS_API_KEY", "")
    twitter_bearer_token: str = os.getenv("TWITTER_BEARER_TOKEN", "")
    reddit_client_id: str = os.getenv("REDDIT_CLIENT_ID", "")
    reddit_client_secret: str = os.getenv("REDDIT_CLIENT_SECRET", "")
    polygon_api_key: str = os.getenv("POLYGON_API_KEY", "")


@dataclass
class LLMConfig:
    """LLM Configuration for different thinking modes"""
    deep_think_model: str = "gpt-4o"
    quick_think_model: str = "gpt-4o-mini"
    temperature: float = 0.1
    max_tokens: int = 4000
    timeout: int = 30
    max_retries: int = 3


@dataclass
class AgentConfig:
    """Individual agent configuration"""
    confidence_threshold: float = 0.7
    update_frequency: int = 300  # seconds
    data_lookback_days: int = 30
    max_api_calls_per_hour: int = 100
    enable_caching: bool = True
    cache_ttl: int = 1800  # 30 minutes


@dataclass
class RiskConfig:
    """Risk management parameters"""
    max_position_size: float = 0.05  # 5% max per position
    max_sector_exposure: float = 0.25  # 25% max per sector
    max_country_exposure: float = 0.40  # 40% max per country
    stop_loss_threshold: float = 0.02  # 2% stop loss
    var_limit: float = 0.03  # 3% daily VaR limit
    correlation_limit: float = 0.8  # Max correlation between positions


@dataclass
class TradingConfig:
    """Trading execution parameters"""
    min_trade_size: float = 1000.0  # Minimum trade size USD
    max_trade_size: float = 1000000.0  # Maximum trade size USD
    slippage_assumption: float = 0.001  # 10 bps slippage
    commission_rate: float = 0.0005  # 5 bps commission
    market_impact_threshold: float = 0.01  # 1% of daily volume


class Config:
    """Main configuration class"""

    def __init__(self):
        self.api = APIConfig()
        self.llm = LLMConfig()
        self.agent = AgentConfig()
        self.risk = RiskConfig()
        self.trading = TradingConfig()

        # Logging configuration
        self.log_level = logging.INFO
        self.log_format = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"

        # Data storage paths
        self.data_dir = Path("data")
        self.cache_dir = Path("cache")
        self.logs_dir = Path("logs")

        # Create directories if they don't exist
        for directory in [self.data_dir, self.cache_dir, self.logs_dir]:
            directory.mkdir(exist_ok=True)

        # Agent weights for decision aggregation
        self.agent_weights = {
            "macro_cycle": 0.15,
            "central_bank": 0.20,
            "geopolitical": 0.18,
            "regulatory": 0.12,
            "sentiment": 0.10,
            "institutional_flow": 0.15,
            "supply_chain": 0.05,
            "innovation": 0.03,
            "currency": 0.02,
            "behavioral": 0.00
        }

        # Market hours and trading sessions
        self.trading_sessions = {
            "US": {"open": "09:30", "close": "16:00", "timezone": "America/New_York"},
            "EU": {"open": "08:00", "close": "16:30", "timezone": "Europe/London"},
            "ASIA": {"open": "09:00", "close": "15:00", "timezone": "Asia/Tokyo"}
        }

        # Asset class mappings
        self.asset_classes = {
            "equities": ["SPY", "QQQ", "IWM", "EFA", "EEM"],
            "bonds": ["TLT", "IEF", "SHY", "LQD", "HYG"],
            "commodities": ["GLD", "SLV", "USO", "UNG", "DBA"],
            "currencies": ["UUP", "FXE", "FXY", "FXB", "EWZ"],
            "volatility": ["VIX", "UVXY", "SVXY"]
        }

        # Economic indicators to monitor
        self.economic_indicators = [
            "GDP", "CPI", "PPI", "UNRATE", "FEDFUNDS", "DGS10", "DGS2",
            "DEXUSEU", "DEXCHUS", "DEXJPUS", "DEXUSUK", "CPIAUCSL",
            "PAYEMS", "INDPRO", "HOUST", "RSAFS", "UMCSENT"
        ]

        # News sources and their reliability scores
        self.news_sources = {
            "reuters": 0.95,
            "bloomberg": 0.95,
            "wsj": 0.90,
            "ft": 0.90,
            "cnbc": 0.75,
            "marketwatch": 0.70,
            "yahoo_finance": 0.65
        }

    def validate_config(self) -> bool:
        """Validate configuration settings"""
        required_keys = [
            self.api.openai_api_key,
            self.api.finnhub_api_key,
            self.api.fred_api_key
        ]

        if not all(required_keys):
            raise ValueError("Missing required API keys. Check environment variables.")

        if sum(self.agent_weights.values()) != 1.0:
            raise ValueError("Agent weights must sum to 1.0")

        return True

    def get_market_hours(self, market: str) -> Dict[str, str]:
        """Get trading hours for specific market"""
        return self.trading_sessions.get(market.upper(), {})

    def get_asset_universe(self, asset_class: str) -> List[str]:
        """Get tradeable assets for asset class"""
        return self.asset_classes.get(asset_class.lower(), [])


# Global configuration instance
CONFIG = Config()