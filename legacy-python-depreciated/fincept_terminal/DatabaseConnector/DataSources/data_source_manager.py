# -*- coding: utf-8 -*-
# data_source_manager.py (Modified to include Alpha Vantage integration)

import json
import requests
import yfinance as yf
import pandas as pd
import csv
import io
import asyncio
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, Any, List, Optional
import threading
import time
from functools import lru_cache

# Import our new logger
from fincept_terminal.utils.Logging.logger import info, debug, warning, error, operation, monitor_performance, logger


class DataSourceManager:
    """
    Universal Data Source Manager - The backbone of all data in the terminal
    All tabs query this manager instead of directly calling APIs
    """

    def __init__(self, app):
        logger.info("Initializing DataSourceManager")

        try:
            with operation("DataSourceManager initialization"):
                self.app = app
                self.config_file = Path.home() / ".fincept" / "data_sources.json"
                self.ensure_config_dir()

                # Cache for performance with thread safety
                self.cache = {}
                self.cache_expiry = {}
                self.cache_duration = 300  # 5 minutes default
                self._cache_lock = threading.RLock()

                # Provider instances cache
                self._provider_cache = {}

                # Statistics tracking
                self._cache_hits = 0
                self._cache_misses = 0
                self._api_calls = 0
                self._errors = 0

                # Default data source mappings - MUST be defined before loading config
                self.default_sources = {
                    "stocks": "yfinance",
                    "forex": "fincept_api",
                    "crypto": "fincept_api",
                    "news": "dummy_news",
                    "economic": "fincept_api",
                    "portfolio": "local_storage",
                    "options": "yfinance",
                    "indices": "yfinance"
                }

                # Available data sources
                self.available_sources = {
                    "yfinance": {
                        "name": "Yahoo Finance",
                        "type": "api",
                        "supports": ["stocks", "indices", "options", "forex"],
                        "requires_auth": False,
                        "real_time": False
                    },
                    "fincept_api": {
                        "name": "Fincept Premium API",
                        "type": "api",
                        "supports": ["stocks", "forex", "crypto", "economic", "news"],
                        "requires_auth": True,
                        "real_time": True
                    },
                    "alpha_vantage_data": {
                        "name": "Alpha Vantage",
                        "type": "api",
                        "supports": ["stocks", "forex", "crypto"],
                        "requires_auth": True,
                        "real_time": False
                    },
                    "dummy_news": {
                        "name": "Sample News Feed",
                        "type": "dummy",
                        "supports": ["news"],
                        "requires_auth": False,
                        "real_time": False
                    },
                    "csv_import": {
                        "name": "CSV File Import",
                        "type": "file",
                        "supports": ["stocks", "portfolio", "custom"],
                        "requires_auth": False,
                        "real_time": False
                    },
                    "websocket_feed": {
                        "name": "WebSocket Data Feed",
                        "type": "websocket",
                        "supports": ["stocks", "crypto", "forex"],
                        "requires_auth": True,
                        "real_time": True
                    }
                }

                # Load configuration AFTER default_sources is defined
                self.config = self.load_configuration()

                logger.info("DataSourceManager initialized successfully",
                            context={"default_sources": list(self.default_sources.keys()),
                                     "available_sources": len(self.available_sources)})

        except Exception as e:
            logger.error("DataSourceManager initialization failed",
                         context={"error": str(e)}, exc_info=True)
            raise

    def get_settings_manager(self):
        """Get settings manager from the settings tab"""
        try:
            # Look for Settings tab (case-sensitive)
            if hasattr(self.app, 'tabs'):
                for tab_key in self.app.tabs.keys():
                    if 'settings' in tab_key.lower() or 'Settings' in tab_key:
                        settings_tab = self.app.tabs[tab_key]
                        if hasattr(settings_tab, 'settings_manager'):
                            debug(f"Found settings manager in tab: {tab_key}", module="DataSourceManager")
                            return settings_tab.settings_manager

            # Alternative: try different tab names
            possible_names = ['Settings', '⚙️ Settings', 'settings', 'SETTINGS']
            for name in possible_names:
                if hasattr(self.app, 'tabs') and name in self.app.tabs:
                    settings_tab = self.app.tabs[name]
                    if hasattr(settings_tab, 'settings_manager'):
                        debug(f"Found settings manager in tab: {name}", module="DataSourceManager")
                        return settings_tab.settings_manager

            debug("Settings manager not found", module="DataSourceManager")
            return None

        except Exception as e:
            debug(f"Error getting settings manager: {str(e)}", module="DataSourceManager")
            return None

    def _get_provider_instance(self, provider_name: str, credentials: Dict[str, str] = None):
        """Get or create provider instance"""
        cache_key = f"{provider_name}_{hash(str(credentials))}"

        if cache_key in self._provider_cache:
            return self._provider_cache[cache_key]

        if provider_name == "alpha_vantage_data":
            api_key = ""

            # Try credentials first
            if credentials and "alpha_vantage_api_key" in credentials:
                api_key = credentials["alpha_vantage_api_key"]

            # Try settings manager
            if not api_key:
                settings_manager = self.get_settings_manager()
                if settings_manager:
                    api_key = settings_manager.get_api_key("alpha_vantage_data")
                    debug(f"Got API key from settings: {len(api_key)} chars", module="DataSourceManager")

            if api_key and len(api_key) > 5:  # Basic validation
                try:
                    from fincept_terminal.DatabaseConnector.DataSources.alpha_vantage_data.alpha_vantage_provider import \
                        AlphaVantageProvider
                    provider = AlphaVantageProvider(api_key)
                    self._provider_cache[cache_key] = provider
                    info(f"Alpha Vantage provider created successfully", module="DataSourceManager")
                    return provider
                except ImportError as e:
                    error(f"Alpha Vantage provider import failed: {str(e)}", module="DataSourceManager")
                    return None
            else:
                warning(f"No valid Alpha Vantage API key found (length: {len(api_key)})", module="DataSourceManager")
                return None

        return None

    def ensure_config_dir(self):
        """Ensure configuration directory exists"""
        try:
            self.config_file.parent.mkdir(exist_ok=True, parents=True)
            logger.debug("Configuration directory ensured",
                         context={"config_dir": str(self.config_file.parent)})
        except Exception as e:
            logger.error("Failed to create config directory",
                         context={"error": str(e)}, exc_info=True)

    def load_configuration(self) -> Dict[str, Any]:
        """Load data source configuration"""
        try:
            if self.config_file.exists():
                with open(self.config_file, 'r') as f:
                    config = json.load(f)
                logger.info("Data source configuration loaded",
                            context={"config_file": str(self.config_file)})
                return config
            else:
                logger.info("No configuration found, using defaults")
                return {"data_mappings": self.default_sources.copy(), "source_configs": {}}

        except Exception as e:
            logger.error("Error loading configuration",
                         context={"error": str(e), "config_file": str(self.config_file)},
                         exc_info=True)
            return {"data_mappings": self.default_sources.copy(), "source_configs": {}}

    def save_configuration(self):
        """Save current configuration"""
        try:
            with open(self.config_file, 'w') as f:
                json.dump(self.config, f, indent=2)
            logger.info("Configuration saved successfully",
                        context={"config_file": str(self.config_file)})
            return True
        except Exception as e:
            logger.error("Error saving configuration",
                         context={"error": str(e)}, exc_info=True)
            return False

    def set_data_source(self, data_type: str, source_name: str, source_config: Dict[str, Any] = None):
        """Set data source for a specific data type"""
        try:
            if source_name not in self.available_sources:
                raise ValueError(f"Unknown data source: {source_name}")

            if data_type not in self.available_sources[source_name]["supports"]:
                raise ValueError(f"Source {source_name} doesn't support {data_type}")

            self.config["data_mappings"][data_type] = source_name

            if source_config:
                if "source_configs" not in self.config:
                    self.config["source_configs"] = {}
                self.config["source_configs"][source_name] = source_config

            self.save_configuration()
            logger.info("Data source updated",
                        context={"data_type": data_type, "source": source_name})

        except Exception as e:
            logger.error("Failed to set data source",
                         context={"data_type": data_type, "source_name": source_name, "error": str(e)},
                         exc_info=True)
            raise

    def get_data_source(self, data_type: str) -> str:
        """Get configured data source for a data type"""
        # Try settings manager first
        settings_manager = self.get_settings_manager()
        if settings_manager:
            try:
                preferences = settings_manager.settings.get("preferences", {})
                default_provider = preferences.get("default_provider", "yfinance")

                # Check if the default provider supports this data type and is enabled
                if (default_provider in self.available_sources and
                        data_type in self.available_sources[default_provider]["supports"] and
                        settings_manager.is_provider_enabled(default_provider)):
                    debug(f"Using provider from settings: {default_provider} for {data_type}",
                          module="DataSourceManager")
                    return default_provider

                # If Alpha Vantage is enabled and supports the data type, prioritize it
                if (settings_manager.is_provider_enabled("alpha_vantage_data") and
                        data_type in self.available_sources["alpha_vantage_data"]["supports"] and
                        settings_manager.get_api_key("alpha_vantage_data")):
                    debug(f"Using Alpha Vantage for {data_type} (enabled with API key)", module="DataSourceManager")
                    return "alpha_vantage_data"

            except Exception as e:
                debug(f"Error checking settings for data source: {str(e)}", module="DataSourceManager")

        # Fallback to config file or defaults
        source = self.config["data_mappings"].get(data_type, self.default_sources.get(data_type, "yfinance"))
        debug(f"Using fallback source: {source} for {data_type}", module="DataSourceManager")
        return source

    def is_cache_valid(self, cache_key: str) -> bool:
        """Check if cached data is still valid"""
        with self._cache_lock:
            if cache_key not in self.cache:
                return False
            if cache_key not in self.cache_expiry:
                return False
            return datetime.now() < self.cache_expiry[cache_key]

    def set_cache(self, cache_key: str, data: Any, duration: int = None):
        """Set data in cache"""
        # Check if caching is enabled in settings
        settings_manager = self.get_settings_manager()
        if settings_manager:
            preferences = settings_manager.settings.get("preferences", {})
            if not preferences.get("cache_enabled", True):
                return
            duration = duration or preferences.get("cache_duration", self.cache_duration)

        duration = duration or self.cache_duration
        with self._cache_lock:
            self.cache[cache_key] = data
            self.cache_expiry[cache_key] = datetime.now() + timedelta(seconds=duration)
        logger.debug("Data cached", context={"cache_key": cache_key, "duration": duration})

    def get_cache(self, cache_key: str) -> Any:
        """Get data from cache if valid"""
        with self._cache_lock:
            if self.is_cache_valid(cache_key):
                self._cache_hits += 1
                logger.debug("Cache hit", context={"cache_key": cache_key})
                return self.cache[cache_key]
            else:
                self._cache_misses += 1
                logger.debug("Cache miss", context={"cache_key": cache_key})
                return None

    # ===========================================
    # UNIVERSAL DATA RETRIEVAL METHODS
    # These are called by ALL tabs in the terminal
    # ===========================================

    @monitor_performance
    def get_stock_data(self, symbol: str, period: str = "1d", interval: str = "1m") -> Dict[str, Any]:
        """Universal stock data retrieval"""
        try:
            with operation(f"Get stock data for {symbol}"):
                cache_key = f"stock_{symbol}_{period}_{interval}"

                # Check cache first
                cached_data = self.get_cache(cache_key)
                if cached_data:
                    return cached_data

                source = self.get_data_source("stocks")
                self._api_calls += 1

                logger.debug("Fetching stock data",
                             context={"symbol": symbol, "period": period,
                                      "interval": interval, "source": source})

                if source == "yfinance":
                    data = self._get_yfinance_stock_data(symbol, period, interval)
                elif source == "fincept_api":
                    data = self._get_fincept_stock_data(symbol, period, interval)
                elif source == "alpha_vantage_data":
                    data = asyncio.run(self._get_alpha_vantage_stock_data(symbol, period, interval))
                else:
                    data = self._get_fallback_stock_data(symbol, period, interval)

                # Cache successful results
                if data.get("success"):
                    self.set_cache(cache_key, data, 60)  # Cache for 1 minute
                    logger.info("Stock data retrieved successfully",
                                context={"symbol": symbol, "source": source})
                else:
                    self._errors += 1
                    logger.warning("Stock data retrieval failed",
                                   context={"symbol": symbol, "error": data.get("error")})

                return data

        except Exception as e:
            self._errors += 1
            logger.error("Stock data retrieval error",
                         context={"symbol": symbol, "error": str(e)}, exc_info=True)
            return {
                "success": False,
                "error": f"Error fetching stock data: {str(e)}",
                "source": "error",
                "symbol": symbol
            }

    async def _get_alpha_vantage_stock_data(self, symbol: str, period: str, interval: str) -> Dict[str, Any]:
        """Get stock data from Alpha Vantage provider"""
        try:
            provider = self._get_provider_instance("alpha_vantage_data")
            if not provider:
                return {"success": False, "error": "Alpha Vantage provider not configured", "source": "alpha_vantage_data"}

            return await provider.get_stock_data(symbol, period, interval)

        except Exception as e:
            logger.error("Alpha Vantage stock data error",
                         context={"symbol": symbol, "error": str(e)}, exc_info=True)
            return {"success": False, "error": str(e), "source": "alpha_vantage_data"}

    # STOCK DATA METHODS
    async def get_weekly_data(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get weekly stock data from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_stock_data(symbol, interval="W")

    async def get_monthly_data(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get monthly stock data from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_stock_data(symbol, interval="M")

    async def get_daily_adjusted(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get daily adjusted stock data from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_daily_adjusted(symbol)

    async def get_weekly_adjusted(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get weekly adjusted stock data from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_weekly_adjusted(symbol)

    async def get_monthly_adjusted(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get monthly adjusted stock data from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_monthly_adjusted(symbol)

    async def get_global_quote(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get global quote from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_global_quote(symbol)

    async def search_symbols(self, keywords: str, **kwargs) -> Dict[str, Any]:
        """Search symbols from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.search_symbols(keywords)

    # FUNDAMENTAL DATA METHODS
    async def get_company_overview(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get company overview from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_company_overview(symbol)

    async def get_income_statement(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get income statement from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_income_statement(symbol)

    async def get_balance_sheet(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get balance sheet from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_balance_sheet(symbol)

    async def get_cash_flow(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get cash flow from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_cash_flow(symbol)

    async def get_earnings(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get earnings from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_earnings(symbol)

    async def get_earnings_estimates(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get earnings estimates from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_earnings_estimates(symbol)

    async def get_dividends(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get dividends from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_dividends(symbol)

    async def get_splits(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get splits from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_splits(symbol)

    # TECHNICAL INDICATORS METHODS
    async def get_sma(self, symbol: str, interval: str = "daily", time_period: int = 14, series_type: str = "close",
                      **kwargs) -> Dict[str, Any]:
        """Get Simple Moving Average from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_sma(symbol, interval, time_period, series_type)

    async def get_ema(self, symbol: str, interval: str = "daily", time_period: int = 14, series_type: str = "close",
                      **kwargs) -> Dict[str, Any]:
        """Get Exponential Moving Average from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_ema(symbol, interval, time_period, series_type)

    async def get_rsi(self, symbol: str, interval: str = "daily", time_period: int = 14, series_type: str = "close",
                      **kwargs) -> Dict[str, Any]:
        """Get RSI from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_rsi(symbol, interval, time_period, series_type)

    async def get_macd(self, symbol: str, interval: str = "daily", series_type: str = "close", **kwargs) -> Dict[
        str, Any]:
        """Get MACD from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_macd(symbol, interval, series_type)

    async def get_bbands(self, symbol: str, interval: str = "daily", time_period: int = 20, series_type: str = "close",
                         **kwargs) -> Dict[str, Any]:
        """Get Bollinger Bands from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_bbands(symbol, interval, time_period, series_type)

    async def get_stoch(self, symbol: str, interval: str = "daily", **kwargs) -> Dict[str, Any]:
        """Get Stochastic from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_stoch(symbol, interval)

    async def get_adx(self, symbol: str, interval: str = "daily", time_period: int = 14, **kwargs) -> Dict[str, Any]:
        """Get ADX from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_adx(symbol, interval, time_period)

    async def get_vwap(self, symbol: str, interval: str = "15min", **kwargs) -> Dict[str, Any]:
        """Get VWAP from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_vwap(symbol, interval)

    # FOREX METHODS
    async def get_currency_exchange_rate(self, from_currency: str = "USD", to_currency: str = "EUR", **kwargs) -> Dict[
        str, Any]:
        """Get currency exchange rate from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_currency_exchange_rate(from_currency, to_currency)

    async def get_fx_intraday(self, from_symbol: str = "USD", to_symbol: str = "EUR", interval: str = "5min",
                              **kwargs) -> Dict[str, Any]:
        """Get FX intraday from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_fx_intraday(from_symbol, to_symbol, interval)

    async def get_fx_weekly(self, from_symbol: str = "USD", to_symbol: str = "EUR", **kwargs) -> Dict[str, Any]:
        """Get FX weekly from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_fx_weekly(from_symbol, to_symbol)

    async def get_fx_monthly(self, from_symbol: str = "USD", to_symbol: str = "EUR", **kwargs) -> Dict[str, Any]:
        """Get FX monthly from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_fx_monthly(from_symbol, to_symbol)

    # CRYPTOCURRENCY METHODS
    async def get_crypto_intraday(self, symbol: str, market: str = "USD", interval: str = "5min", **kwargs) -> Dict[
        str, Any]:
        """Get crypto intraday from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_crypto_intraday(symbol, market, interval)

    async def get_digital_currency_weekly(self, symbol: str, market: str = "USD", **kwargs) -> Dict[str, Any]:
        """Get digital currency weekly from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_digital_currency_weekly(symbol, market)

    async def get_digital_currency_monthly(self, symbol: str, market: str = "USD", **kwargs) -> Dict[str, Any]:
        """Get digital currency monthly from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_digital_currency_monthly(symbol, market)

    # COMMODITIES METHODS
    async def get_wti_oil(self, interval: str = "monthly", **kwargs) -> Dict[str, Any]:
        """Get WTI oil from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_wti_oil(interval)

    async def get_brent_oil(self, interval: str = "monthly", **kwargs) -> Dict[str, Any]:
        """Get Brent oil from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_brent_oil(interval)

    async def get_natural_gas(self, interval: str = "monthly", **kwargs) -> Dict[str, Any]:
        """Get Natural gas from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_natural_gas(interval)

    async def get_copper(self, interval: str = "monthly", **kwargs) -> Dict[str, Any]:
        """Get Copper from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_copper(interval)

    async def get_aluminum(self, interval: str = "monthly", **kwargs) -> Dict[str, Any]:
        """Get Aluminum from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_aluminum(interval)

    # ECONOMIC INDICATORS METHODS
    async def get_real_gdp(self, interval: str = "annual", **kwargs) -> Dict[str, Any]:
        """Get Real GDP from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_real_gdp(interval)

    async def get_unemployment(self, **kwargs) -> Dict[str, Any]:
        """Get Unemployment from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_unemployment()

    async def get_cpi(self, interval: str = "monthly", **kwargs) -> Dict[str, Any]:
        """Get CPI from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_cpi(interval)

    async def get_treasury_yield(self, interval: str = "monthly", maturity: str = "10year", **kwargs) -> Dict[str, Any]:
        """Get Treasury yield from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_treasury_yield(interval, maturity)

    async def get_federal_funds_rate(self, interval: str = "monthly", **kwargs) -> Dict[str, Any]:
        """Get Federal funds rate from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_federal_funds_rate(interval)

    # MARKET INTELLIGENCE METHODS
    async def get_news_sentiment(self, tickers: str = None, topics: str = None, **kwargs) -> Dict[str, Any]:
        """Get news sentiment from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_news_sentiment(tickers, topics)

    async def get_top_gainers_losers(self, **kwargs) -> Dict[str, Any]:
        """Get top gainers/losers from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_top_gainers_losers()

    async def get_insider_transactions(self, symbol: str, **kwargs) -> Dict[str, Any]:
        """Get insider transactions from Alpha Vantage"""
        provider = self._get_provider_instance("alpha_vantage_data")
        if not provider:
            return {"success": False, "error": "Alpha Vantage provider not configured"}
        return await provider.get_insider_transactions(symbol)

    @monitor_performance
    def get_forex_data(self, pair: str, period: str = "1d") -> Dict[str, Any]:
        """Universal forex data retrieval"""
        try:
            with operation(f"Get forex data for {pair}"):
                cache_key = f"forex_{pair}_{period}"

                cached_data = self.get_cache(cache_key)
                if cached_data:
                    return cached_data

                source = self.get_data_source("forex")
                self._api_calls += 1

                logger.debug("Fetching forex data",
                             context={"pair": pair, "period": period, "source": source})

                if source == "yfinance":
                    data = self._get_yfinance_forex_data(pair, period)
                elif source == "fincept_api":
                    data = self._get_fincept_forex_data(pair, period)
                elif source == "alpha_vantage_data":
                    data = asyncio.run(self._get_alpha_vantage_forex_data(pair, period))
                else:
                    data = self._get_fallback_forex_data(pair, period)

                if data.get("success"):
                    self.set_cache(cache_key, data, 300)  # Cache for 5 minutes
                    logger.info("Forex data retrieved successfully",
                                context={"pair": pair, "source": source})
                else:
                    self._errors += 1
                    logger.warning("Forex data retrieval failed",
                                   context={"pair": pair, "error": data.get("error")})

                return data

        except Exception as e:
            self._errors += 1
            logger.error("Forex data retrieval error",
                         context={"pair": pair, "error": str(e)}, exc_info=True)
            return {
                "success": False,
                "error": f"Error fetching forex data: {str(e)}",
                "source": "error",
                "pair": pair
            }

    async def _get_alpha_vantage_forex_data(self, pair: str, period: str) -> Dict[str, Any]:
        """Get forex data from Alpha Vantage provider"""
        try:
            provider = self._get_provider_instance("alpha_vantage_data")
            if not provider:
                return {"success": False, "error": "Alpha Vantage provider not configured", "source": "alpha_vantage_data"}

            return await provider.get_forex_data(pair, period)

        except Exception as e:
            logger.error("Alpha Vantage forex data error",
                         context={"pair": pair, "error": str(e)}, exc_info=True)
            return {"success": False, "error": str(e), "source": "alpha_vantage_data"}

    @monitor_performance
    def get_news_data(self, category: str = "financial", limit: int = 20) -> Dict[str, Any]:
        """Universal news data retrieval"""
        try:
            with operation(f"Get news data for {category}"):
                cache_key = f"news_{category}_{limit}"

                cached_data = self.get_cache(cache_key)
                if cached_data:
                    return cached_data

                source = self.get_data_source("news")
                self._api_calls += 1

                logger.debug("Fetching news data",
                             context={"category": category, "limit": limit, "source": source})

                if source == "fincept_api":
                    data = self._get_fincept_news_data(category, limit)
                elif source == "dummy_news":
                    data = self._get_dummy_news_data(category, limit)
                else:
                    data = self._get_fallback_news_data(category, limit)

                if data.get("success"):
                    self.set_cache(cache_key, data, 600)  # Cache for 10 minutes
                    logger.info("News data retrieved successfully",
                                context={"category": category, "articles": len(data.get("articles", []))})
                else:
                    self._errors += 1
                    logger.warning("News data retrieval failed",
                                   context={"category": category, "error": data.get("error")})

                return data

        except Exception as e:
            self._errors += 1
            logger.error("News data retrieval error",
                         context={"category": category, "error": str(e)}, exc_info=True)
            return {
                "success": False,
                "error": f"Error fetching news data: {str(e)}",
                "source": "error",
                "category": category
            }

    @monitor_performance
    def get_crypto_data(self, symbol: str, period: str = "1d") -> Dict[str, Any]:
        """Universal crypto data retrieval"""
        try:
            with operation(f"Get crypto data for {symbol}"):
                cache_key = f"crypto_{symbol}_{period}"

                cached_data = self.get_cache(cache_key)
                if cached_data:
                    return cached_data

                source = self.get_data_source("crypto")
                self._api_calls += 1

                logger.debug("Fetching crypto data",
                             context={"symbol": symbol, "period": period, "source": source})

                if source == "fincept_api":
                    data = self._get_fincept_crypto_data(symbol, period)
                elif source == "alpha_vantage_data":
                    data = asyncio.run(self._get_alpha_vantage_crypto_data(symbol, period))
                else:
                    data = self._get_fallback_crypto_data(symbol, period)

                if data.get("success"):
                    self.set_cache(cache_key, data, 120)  # Cache for 2 minutes
                    logger.info("Crypto data retrieved successfully",
                                context={"symbol": symbol, "source": source})
                else:
                    self._errors += 1
                    logger.warning("Crypto data retrieval failed",
                                   context={"symbol": symbol, "error": data.get("error")})

                return data

        except Exception as e:
            self._errors += 1
            logger.error("Crypto data retrieval error",
                         context={"symbol": symbol, "error": str(e)}, exc_info=True)
            return {
                "success": False,
                "error": f"Error fetching crypto data: {str(e)}",
                "source": "error",
                "symbol": symbol
            }

    async def _get_alpha_vantage_crypto_data(self, symbol: str, period: str) -> Dict[str, Any]:
        """Get crypto data from Alpha Vantage provider"""
        try:
            provider = self._get_provider_instance("alpha_vantage_data")
            if not provider:
                return {"success": False, "error": "Alpha Vantage provider not configured", "source": "alpha_vantage_data"}

            return await provider.get_crypto_data(symbol, period)

        except Exception as e:
            logger.error("Alpha Vantage crypto data error",
                         context={"symbol": symbol, "error": str(e)}, exc_info=True)
            return {"success": False, "error": str(e), "source": "alpha_vantage_data"}

    @monitor_performance
    def get_economic_data(self, indicator: str, country: str = "US") -> Dict[str, Any]:
        """Universal economic data retrieval"""
        try:
            with operation(f"Get economic data for {indicator}"):
                cache_key = f"economic_{indicator}_{country}"

                cached_data = self.get_cache(cache_key)
                if cached_data:
                    return cached_data

                source = self.get_data_source("economic")
                self._api_calls += 1

                logger.debug("Fetching economic data",
                             context={"indicator": indicator, "country": country, "source": source})

                if source == "fincept_api":
                    data = self._get_fincept_economic_data(indicator, country)
                else:
                    data = self._get_fallback_economic_data(indicator, country)

                if data.get("success"):
                    self.set_cache(cache_key, data, 3600)  # Cache for 1 hour
                    logger.info("Economic data retrieved successfully",
                                context={"indicator": indicator, "country": country})
                else:
                    self._errors += 1
                    logger.warning("Economic data retrieval failed",
                                   context={"indicator": indicator, "error": data.get("error")})

                return data

        except Exception as e:
            self._errors += 1
            logger.error("Economic data retrieval error",
                         context={"indicator": indicator, "error": str(e)}, exc_info=True)
            return {
                "success": False,
                "error": f"Error fetching economic data: {str(e)}",
                "source": "error",
                "indicator": indicator
            }

    # ===========================================
    # DATA SOURCE IMPLEMENTATIONS
    # ===========================================

    def _get_yfinance_stock_data(self, symbol: str, period: str, interval: str) -> Dict[str, Any]:
        """Get stock data from Yahoo Finance"""
        try:
            logger.debug("Calling yfinance API", context={"symbol": symbol})
            ticker = yf.Ticker(symbol)
            hist = ticker.history(period=period, interval=interval)

            if hist.empty:
                logger.warning("No data found in yfinance", context={"symbol": symbol})
                return {
                    "success": False,
                    "error": f"No data found for symbol {symbol}",
                    "source": "yfinance"
                }

            # Convert to standard format
            data = {
                "success": True,
                "source": "yfinance",
                "symbol": symbol,
                "data": {
                    "timestamps": [t.isoformat() for t in hist.index],
                    "open": hist['Open'].tolist(),
                    "high": hist['High'].tolist(),
                    "low": hist['Low'].tolist(),
                    "close": hist['Close'].tolist(),
                    "volume": hist['Volume'].tolist()
                },
                "current_price": float(hist['Close'][-1]) if len(hist) > 0 else None,
                "fetched_at": datetime.now().isoformat()
            }

            logger.debug("yfinance data parsed successfully",
                         context={"symbol": symbol, "data_points": len(hist)})
            return data

        except Exception as e:
            logger.error("yfinance API error",
                         context={"symbol": symbol, "error": str(e)}, exc_info=True)
            return {
                "success": False,
                "error": f"YFinance error: {str(e)}",
                "source": "yfinance",
                "symbol": symbol
            }

    def _get_yfinance_forex_data(self, pair: str, period: str) -> Dict[str, Any]:
        """Get forex data from Yahoo Finance"""
        try:
            # Convert pair format (EURUSD -> EURUSD=X)
            yahoo_pair = f"{pair}=X" if not pair.endswith("=X") else pair
            logger.debug("Calling yfinance forex API", context={"pair": pair, "yahoo_pair": yahoo_pair})

            ticker = yf.Ticker(yahoo_pair)
            hist = ticker.history(period=period)

            if hist.empty:
                logger.warning("No forex data found in yfinance", context={"pair": pair})
                return {
                    "success": False,
                    "error": f"No forex data found for pair {pair}",
                    "source": "yfinance"
                }

            data = {
                "success": True,
                "source": "yfinance",
                "pair": pair,
                "data": {
                    "timestamps": [t.isoformat() for t in hist.index],
                    "rates": hist['Close'].tolist()
                },
                "current_rate": float(hist['Close'][-1]) if len(hist) > 0 else None,
                "fetched_at": datetime.now().isoformat()
            }

            logger.debug("yfinance forex data parsed successfully",
                         context={"pair": pair, "data_points": len(hist)})
            return data

        except Exception as e:
            logger.error("yfinance forex API error",
                         context={"pair": pair, "error": str(e)}, exc_info=True)
            return {
                "success": False,
                "error": f"YFinance forex error: {str(e)}",
                "source": "yfinance",
                "pair": pair
            }

    def _get_fincept_stock_data(self, symbol: str, period: str, interval: str) -> Dict[str, Any]:
        """Get stock data from Fincept API (dummy implementation)"""
        logger.debug("Using dummy Fincept API for stock data", context={"symbol": symbol})
        return {
            "success": True,
            "source": "fincept_api",
            "symbol": symbol,
            "data": {
                "timestamps": [datetime.now().isoformat()],
                "open": [100.0],
                "high": [105.0],
                "low": [98.0],
                "close": [102.0],
                "volume": [1000000]
            },
            "current_price": 102.0,
            "fetched_at": datetime.now().isoformat(),
            "note": "This is dummy Fincept API data"
        }

    def _get_fincept_forex_data(self, pair: str, period: str) -> Dict[str, Any]:
        """Get forex data from Fincept API (dummy implementation)"""
        logger.debug("Using dummy Fincept API for forex data", context={"pair": pair})
        return {
            "success": True,
            "source": "fincept_api",
            "pair": pair,
            "data": {
                "timestamps": [datetime.now().isoformat()],
                "rates": [1.2345]
            },
            "current_rate": 1.2345,
            "fetched_at": datetime.now().isoformat(),
            "note": "This is dummy Fincept forex data"
        }

    def _get_fincept_news_data(self, category: str, limit: int) -> Dict[str, Any]:
        """Get news data from Fincept API (dummy implementation)"""
        logger.debug("Using dummy Fincept API for news data", context={"category": category, "limit": limit})
        return {
            "success": True,
            "source": "fincept_api",
            "category": category,
            "articles": [
                {
                    "title": "Market Update: Tech Stocks Rally",
                    "summary": "Technology stocks gained momentum in today's trading session.",
                    "url": "https://example.com/news/1",
                    "published_at": datetime.now().isoformat(),
                    "source": "Fincept News"
                }
            ],
            "total": limit,
            "fetched_at": datetime.now().isoformat(),
            "note": "This is dummy Fincept news data"
        }

    def _get_dummy_news_data(self, category: str, limit: int) -> Dict[str, Any]:
        """Get dummy news data"""
        logger.debug("Generating dummy news data", context={"category": category, "limit": limit})

        dummy_articles = []
        for i in range(min(limit, 5)):
            dummy_articles.append({
                "title": f"Sample Financial News Article {i + 1}",
                "summary": f"This is a sample news summary for {category} category.",
                "url": f"https://example.com/news/{i + 1}",
                "published_at": (datetime.now() - timedelta(hours=i)).isoformat(),
                "source": "Sample News"
            })

        return {
            "success": True,
            "source": "dummy_news",
            "category": category,
            "articles": dummy_articles,
            "total": len(dummy_articles),
            "fetched_at": datetime.now().isoformat()
        }

    def _get_fincept_crypto_data(self, symbol: str, period: str) -> Dict[str, Any]:
        """Get crypto data from Fincept API (dummy implementation)"""
        logger.debug("Using dummy Fincept API for crypto data", context={"symbol": symbol})
        return {
            "success": True,
            "source": "fincept_api",
            "symbol": symbol,
            "data": {
                "timestamps": [datetime.now().isoformat()],
                "prices": [50000.0]
            },
            "current_price": 50000.0,
            "fetched_at": datetime.now().isoformat(),
            "note": "This is dummy Fincept crypto data"
        }

    def _get_fincept_economic_data(self, indicator: str, country: str) -> Dict[str, Any]:
        """Get economic data from Fincept API (dummy implementation)"""
        logger.debug("Using dummy Fincept API for economic data",
                     context={"indicator": indicator, "country": country})
        return {
            "success": True,
            "source": "fincept_api",
            "indicator": indicator,
            "country": country,
            "data": {
                "timestamps": [datetime.now().isoformat()],
                "values": [2.5]
            },
            "current_value": 2.5,
            "fetched_at": datetime.now().isoformat(),
            "note": "This is dummy Fincept economic data"
        }

    # Fallback methods (when primary source fails)
    def _get_fallback_stock_data(self, symbol: str, period: str, interval: str) -> Dict[str, Any]:
        """Fallback to YFinance for stock data"""
        logger.info("Using fallback stock data source", context={"symbol": symbol})
        return self._get_yfinance_stock_data(symbol, period, interval)

    def _get_fallback_forex_data(self, pair: str, period: str) -> Dict[str, Any]:
        """Fallback to YFinance for forex data"""
        logger.info("Using fallback forex data source", context={"pair": pair})
        return self._get_yfinance_forex_data(pair, period)

    def _get_fallback_news_data(self, category: str, limit: int) -> Dict[str, Any]:
        """Fallback to dummy news"""
        logger.info("Using fallback news data source", context={"category": category})
        return self._get_dummy_news_data(category, limit)

    def _get_fallback_crypto_data(self, symbol: str, period: str) -> Dict[str, Any]:
        """Fallback crypto data"""
        logger.warning("No fallback available for crypto data", context={"symbol": symbol})
        return {
            "success": False,
            "error": "No fallback available for crypto data",
            "source": "fallback"
        }

    def _get_fallback_economic_data(self, indicator: str, country: str) -> Dict[str, Any]:
        """Fallback economic data"""
        logger.warning("No fallback available for economic data",
                       context={"indicator": indicator, "country": country})
        return {
            "success": False,
            "error": "No fallback available for economic data",
            "source": "fallback"
        }

    # ===========================================
    # DATA SOURCE MANAGEMENT
    # ===========================================

    @monitor_performance
    def test_data_source(self, source_name: str, config: Dict[str, Any] = None) -> Dict[str, Any]:
        """Test if a data source is working"""
        try:
            with operation(f"Test data source {source_name}"):
                logger.info("Testing data source", context={"source": source_name})

                if source_name == "yfinance":
                    # Test with a common stock
                    result = self._get_yfinance_stock_data("AAPL", "1d", "1d")
                    success = result.get("success", False)
                    logger.info("yfinance test completed", context={"success": success})
                    return {
                        "success": success,
                        "message": "YFinance connection successful" if success else result.get("error"),
                        "response_time": "< 1s"
                    }
                elif source_name == "alpha_vantage_data":
                    # Test Alpha Vantage
                    provider = self._get_provider_instance("alpha_vantage_data")
                    if provider:
                        result = asyncio.run(provider.verify_api_key())
                        logger.info("alpha_vantage_data test completed", context={"success": result.get("valid", False)})
                        return {
                            "success": result.get("valid", False),
                            "message": result.get("message", result.get("error", "Unknown")),
                            "response_time": "< 2s"
                        }
                    else:
                        return {
                            "success": False,
                            "message": "Alpha Vantage provider not configured",
                            "response_time": "immediate"
                        }
                elif source_name == "fincept_api":
                    logger.info("fincept_api test completed", context={"success": True})
                    return {
                        "success": True,
                        "message": "Fincept API connection successful (dummy)",
                        "response_time": "< 1s"
                    }
                else:
                    logger.info("Generic source test completed", context={"source": source_name})
                    return {
                        "success": True,
                        "message": f"{source_name} test successful (dummy)",
                        "response_time": "< 1s"
                    }

        except Exception as e:
            logger.error("Data source test failed",
                         context={"source": source_name, "error": str(e)}, exc_info=True)
            return {
                "success": False,
                "message": f"Test failed: {str(e)}",
                "response_time": "timeout"
            }

    def get_available_sources(self) -> Dict[str, Any]:
        """Get all available data sources"""
        logger.debug("Retrieved available sources",
                     context={"count": len(self.available_sources)})
        return self.available_sources

    def get_source_config(self, source_name: str) -> Dict[str, Any]:
        """Get configuration for a specific source"""
        config = self.config.get("source_configs", {}).get(source_name, {})
        logger.debug("Retrieved source config",
                     context={"source": source_name, "has_config": bool(config)})
        return config

    def get_current_mappings(self) -> Dict[str, str]:
        """Get current data type to source mappings"""
        mappings = {}
        for data_type in ["stocks", "forex", "crypto", "news", "economic"]:
            mappings[data_type] = self.get_data_source(data_type)

        logger.debug("Retrieved current mappings",
                     context={"mappings_count": len(mappings)})
        return mappings

    @monitor_performance
    def import_csv_data(self, file_path: str, data_type: str, column_mapping: Dict[str, str]) -> Dict[str, Any]:
        """Import data from CSV file"""
        try:
            with operation(f"Import CSV data from {file_path}"):
                logger.info("Starting CSV import",
                            context={"file_path": file_path, "data_type": data_type})

                df = pd.read_csv(file_path)

                # Apply column mapping
                mapped_data = {}
                for standard_col, csv_col in column_mapping.items():
                    if csv_col in df.columns:
                        mapped_data[standard_col] = df[csv_col].tolist()
                    else:
                        logger.warning("Column not found in CSV",
                                       context={"expected_column": csv_col, "available_columns": list(df.columns)})

                result = {
                    "success": True,
                    "source": "csv_import",
                    "data_type": data_type,
                    "data": mapped_data,
                    "row_count": len(df),
                    "imported_at": datetime.now().isoformat()
                }

                logger.info("CSV import completed successfully",
                            context={"rows_imported": len(df), "columns_mapped": len(mapped_data)})
                return result

        except Exception as e:
            logger.error("CSV import failed",
                         context={"file_path": file_path, "error": str(e)}, exc_info=True)
            return {
                "success": False,
                "error": f"CSV import error: {str(e)}",
                "source": "csv_import"
            }

    def clear_cache(self, data_type: str = None):
        """Clear cache for specific data type or all"""
        try:
            with self._cache_lock:
                if data_type:
                    keys_to_remove = [k for k in self.cache.keys() if k.startswith(data_type)]
                    for key in keys_to_remove:
                        del self.cache[key]
                        if key in self.cache_expiry:
                            del self.cache_expiry[key]
                    logger.info("Cache cleared for data type",
                                context={"data_type": data_type, "keys_removed": len(keys_to_remove)})
                else:
                    cache_size = len(self.cache)
                    self.cache.clear()
                    self.cache_expiry.clear()
                    logger.info("All cache cleared", context={"items_removed": cache_size})
        except Exception as e:
            logger.error("Cache clear failed",
                         context={"data_type": data_type, "error": str(e)}, exc_info=True)

    def get_cache_stats(self) -> Dict[str, Any]:
        """Get cache statistics"""
        try:
            with self._cache_lock:
                total_items = len(self.cache)
                expired_items = sum(1 for k in self.cache.keys() if not self.is_cache_valid(k))
                valid_items = total_items - expired_items

                # Calculate hit rate
                total_requests = self._cache_hits + self._cache_misses
                hit_rate = (self._cache_hits / total_requests * 100) if total_requests > 0 else 0

                stats = {
                    "total_items": total_items,
                    "valid_items": valid_items,
                    "expired_items": expired_items,
                    "cache_hits": self._cache_hits,
                    "cache_misses": self._cache_misses,
                    "hit_rate_percent": round(hit_rate, 2),
                    "api_calls": self._api_calls,
                    "error_count": self._errors,
                    "memory_usage_estimate": f"{len(str(self.cache))} bytes"
                }

                logger.debug("Cache statistics calculated", context=stats)
                return stats

        except Exception as e:
            logger.error("Failed to calculate cache stats",
                         context={"error": str(e)}, exc_info=True)
            return {"error": str(e)}

    def reset_to_defaults(self):
        """Reset configuration to defaults"""
        try:
            with operation("Reset to defaults"):
                logger.info("Resetting configuration to defaults")

                self.config = {"data_mappings": self.default_sources.copy(), "source_configs": {}}
                self.save_configuration()
                self.clear_cache()

                # Reset statistics
                self._cache_hits = 0
                self._cache_misses = 0
                self._api_calls = 0
                self._errors = 0

                logger.info("Configuration reset completed")

        except Exception as e:
            logger.error("Failed to reset configuration",
                         context={"error": str(e)}, exc_info=True)

    def get_performance_stats(self) -> Dict[str, Any]:
        """Get performance statistics"""
        try:
            cache_stats = self.get_cache_stats()

            stats = {
                "data_source_manager": {
                    "total_api_calls": self._api_calls,
                    "total_errors": self._errors,
                    "error_rate_percent": (self._errors / self._api_calls * 100) if self._api_calls > 0 else 0,
                    "uptime_seconds": time.time() - getattr(self, '_start_time', time.time()),
                },
                "cache_performance": cache_stats,
                "active_sources": list(self.config.get("data_mappings", {}).values()),
                "available_sources": len(self.available_sources)
            }

            logger.debug("Performance statistics generated",
                         context={"total_api_calls": self._api_calls,
                                  "error_rate": stats["data_source_manager"]["error_rate_percent"]})
            return stats

        except Exception as e:
            logger.error("Failed to generate performance stats",
                         context={"error": str(e)}, exc_info=True)
            return {"error": str(e)}

    @lru_cache(maxsize=100)
    def get_supported_data_types(self, source_name: str) -> List[str]:
        """Get supported data types for a source - cached"""
        supported = self.available_sources.get(source_name, {}).get("supports", [])
        logger.debug("Retrieved supported data types",
                     context={"source": source_name, "types": supported})
        return supported

    def validate_configuration(self) -> Dict[str, Any]:
        """Validate current configuration"""
        try:
            with operation("Validate configuration"):
                logger.info("Starting configuration validation")

                issues = []
                warnings = []

                # Check if all mapped sources exist
                for data_type, source_name in self.config.get("data_mappings", {}).items():
                    if source_name not in self.available_sources:
                        issues.append(f"Unknown source '{source_name}' mapped to '{data_type}'")
                    elif data_type not in self.available_sources[source_name]["supports"]:
                        issues.append(f"Source '{source_name}' doesn't support data type '{data_type}'")

                # Check for missing essential data types
                essential_types = ["stocks", "forex", "news"]
                for data_type in essential_types:
                    if data_type not in self.config.get("data_mappings", {}):
                        warnings.append(f"No source configured for essential data type '{data_type}'")

                # Check source configurations
                for source_name, config in self.config.get("source_configs", {}).items():
                    if source_name not in self.available_sources:
                        warnings.append(f"Configuration exists for unknown source '{source_name}'")
                    elif self.available_sources[source_name]["requires_auth"] and not config:
                        warnings.append(f"Source '{source_name}' requires authentication but no config provided")

                validation_result = {
                    "valid": len(issues) == 0,
                    "issues": issues,
                    "warnings": warnings,
                    "total_issues": len(issues),
                    "total_warnings": len(warnings),
                    "validated_at": datetime.now().isoformat()
                }

                if issues:
                    logger.warning("Configuration validation found issues",
                                   context={"issues": len(issues), "warnings": len(warnings)})
                else:
                    logger.info("Configuration validation passed",
                                context={"warnings": len(warnings)})

                return validation_result

        except Exception as e:
            logger.error("Configuration validation failed",
                         context={"error": str(e)}, exc_info=True)
            return {
                "valid": False,
                "error": str(e),
                "validated_at": datetime.now().isoformat()
            }

    def health_check(self) -> Dict[str, Any]:
        """Perform health check on data source manager"""
        try:
            with operation("Health check"):
                logger.debug("Starting health check")

                health_status = {
                    "status": "healthy",
                    "timestamp": datetime.now().isoformat(),
                    "cache_functional": False,
                    "configuration_valid": False,
                    "primary_sources_available": [],
                    "issues": []
                }

                # Test cache functionality
                try:
                    test_key = "health_check_test"
                    test_data = {"test": True}
                    self.set_cache(test_key, test_data, 1)
                    retrieved = self.get_cache(test_key)
                    health_status["cache_functional"] = retrieved == test_data
                except Exception as cache_error:
                    health_status["issues"].append(f"Cache test failed: {str(cache_error)}")

                # Validate configuration
                validation = self.validate_configuration()
                health_status["configuration_valid"] = validation["valid"]
                if not validation["valid"]:
                    health_status["issues"].extend(validation["issues"])

                # Test primary sources
                primary_sources = ["yfinance", "alpha_vantage_data"]
                for source in primary_sources:
                    try:
                        test_result = self.test_data_source(source)
                        if test_result["success"]:
                            health_status["primary_sources_available"].append(source)
                    except Exception as source_error:
                        health_status["issues"].append(f"Source {source} test failed: {str(source_error)}")

                # Overall health determination
                if health_status["issues"]:
                    health_status["status"] = "degraded" if health_status["cache_functional"] else "unhealthy"

                logger.info("Health check completed",
                            context={"status": health_status["status"], "issues": len(health_status["issues"])})
                return health_status

        except Exception as e:
            logger.error("Health check failed",
                         context={"error": str(e)}, exc_info=True)
            return {
                "status": "error",
                "error": str(e),
                "timestamp": datetime.now().isoformat()
            }

    def __repr__(self) -> str:
        """String representation for debugging"""
        return (f"DataSourceManager(sources={len(self.available_sources)}, "
                f"cache_items={len(self.cache)}, "
                f"api_calls={self._api_calls}, "
                f"errors={self._errors})")

    def cleanup(self):
        """Clean up resources"""
        try:
            with operation("DataSourceManager cleanup"):
                logger.info("Starting DataSourceManager cleanup")

                # Clear cache
                self.clear_cache()

                # Close provider connections
                for provider in self._provider_cache.values():
                    if hasattr(provider, 'close'):
                        try:
                            if asyncio.iscoroutinefunction(provider.close):
                                asyncio.run(provider.close())
                            else:
                                provider.close()
                        except Exception as e:
                            logger.debug("Error closing provider", context={"error": str(e)})

                self._provider_cache.clear()

                # Clear LRU cache
                self.get_supported_data_types.cache_clear()

                # Save final configuration
                self.save_configuration()

                logger.info("DataSourceManager cleanup completed",
                            context={"final_api_calls": self._api_calls, "final_errors": self._errors})

        except Exception as e:
            logger.error("DataSourceManager cleanup failed",
                         context={"error": str(e)}, exc_info=True)


# Global instance - will be imported by tabs
data_source_manager = None


def get_provider_status(self, provider_name: str) -> Dict[str, Any]:
    """Get comprehensive status of a provider"""
    try:
        settings_manager = self.get_settings_manager()
        status = {
            "name": provider_name,
            "enabled": False,
            "configured": False,
            "tested": False,
            "supports": [],
            "last_used": None,
            "error": None
        }

        # Check if provider exists
        if provider_name not in self.available_sources:
            status["error"] = f"Provider {provider_name} not found"
            return status

        provider_info = self.available_sources[provider_name]
        status["supports"] = provider_info.get("supports", [])

        # Check if enabled in settings
        if settings_manager:
            status["enabled"] = settings_manager.is_provider_enabled(provider_name)

            # Check if properly configured
            if provider_info.get("requires_auth", False):
                api_key = settings_manager.get_api_key(provider_name)
                status["configured"] = bool(api_key and len(api_key) > 5)
            else:
                status["configured"] = True

        return status

    except Exception as e:
        error(f"Error getting provider status: {str(e)}", module="DataSourceManager")
        return {"name": provider_name, "error": str(e)}

def get_data_source_manager(app=None):
    """Get global data source manager instance"""
    global data_source_manager
    try:
        if data_source_manager is None and app:
            logger.info("Creating global DataSourceManager instance")
            data_source_manager = DataSourceManager(app)
            # Store start time for uptime calculation
            data_source_manager._start_time = time.time()
        return data_source_manager
    except Exception as e:
        logger.error("Failed to create DataSourceManager instance",
                     context={"error": str(e)}, exc_info=True)
        return None


# Convenience functions for tabs to use
def get_stock_data(symbol: str, period: str = "1d", interval: str = "1m") -> Dict[str, Any]:
    """Convenience function for getting stock data"""
    dsm = get_data_source_manager()
    if dsm:
        return dsm.get_stock_data(symbol, period, interval)
    else:
        logger.error("DataSourceManager not available")
        return {"success": False, "error": "DataSourceManager not available"}


def get_forex_data(pair: str, period: str = "1d") -> Dict[str, Any]:
    """Convenience function for getting forex data"""
    dsm = get_data_source_manager()
    if dsm:
        return dsm.get_forex_data(pair, period)
    else:
        logger.error("DataSourceManager not available")
        return {"success": False, "error": "DataSourceManager not available"}


def get_news_data(category: str = "financial", limit: int = 20) -> Dict[str, Any]:
    """Convenience function for getting news data"""
    dsm = get_data_source_manager()
    if dsm:
        return dsm.get_news_data(category, limit)
    else:
        logger.error("DataSourceManager not available")
        return {"success": False, "error": "DataSourceManager not available"}


# Export main components
__all__ = [
    'DataSourceManager',
    'get_data_source_manager',
    'get_stock_data',
    'get_forex_data',
    'get_news_data'
]