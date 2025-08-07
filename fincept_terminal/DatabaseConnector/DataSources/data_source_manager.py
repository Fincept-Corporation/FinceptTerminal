# -*- coding: utf-8 -*-
# data_source_manager.py

import json
import requests
import yfinance as yf
import pandas as pd
import csv
import io
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, Any, List, Optional
import threading
import time


class DataSourceManager:
    """
    Universal Data Source Manager - The backbone of all data in the terminal
    All tabs query this manager instead of directly calling APIs
    """

    def __init__(self, app):
        self.app = app
        self.config_file = Path.home() / ".fincept" / "data_sources.json"
        self.ensure_config_dir()

        # Cache for performance
        self.cache = {}
        self.cache_expiry = {}
        self.cache_duration = 300  # 5 minutes default

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

        # Load configuration AFTER default_sources is defined
        self.config = self.load_configuration()

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
            "alpha_vantage": {
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

        print("🔄 Data Source Manager initialized")
        print(f"📊 Default sources: {self.default_sources}")

    def ensure_config_dir(self):
        """Ensure configuration directory exists"""
        self.config_file.parent.mkdir(exist_ok=True)

    def load_configuration(self) -> Dict[str, Any]:
        """Load data source configuration"""
        try:
            if self.config_file.exists():
                with open(self.config_file, 'r') as f:
                    config = json.load(f)
                print(" Data source configuration loaded")
                return config
            else:
                print(" No configuration found, using defaults")
                return {"data_mappings": self.default_sources.copy(), "source_configs": {}}
        except Exception as e:
            print(f" Error loading configuration: {e}")
            return {"data_mappings": self.default_sources.copy(), "source_configs": {}}

    def save_configuration(self):
        """Save current configuration"""
        try:
            with open(self.config_file, 'w') as f:
                json.dump(self.config, f, indent=2)
            print("✅ Configuration saved")
            return True
        except Exception as e:
            print(f"❌ Error saving configuration: {e}")
            return False

    def set_data_source(self, data_type: str, source_name: str, source_config: Dict[str, Any] = None):
        """Set data source for a specific data type"""
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
        print(f"🔄 Set {data_type} data source to {source_name}")

    def get_data_source(self, data_type: str) -> str:
        """Get configured data source for a data type"""
        return self.config["data_mappings"].get(data_type, self.default_sources.get(data_type, "fincept_api"))

    def is_cache_valid(self, cache_key: str) -> bool:
        """Check if cached data is still valid"""
        if cache_key not in self.cache:
            return False
        if cache_key not in self.cache_expiry:
            return False
        return datetime.now() < self.cache_expiry[cache_key]

    def set_cache(self, cache_key: str, data: Any, duration: int = None):
        """Set data in cache"""
        duration = duration or self.cache_duration
        self.cache[cache_key] = data
        self.cache_expiry[cache_key] = datetime.now() + timedelta(seconds=duration)

    def get_cache(self, cache_key: str) -> Any:
        """Get data from cache if valid"""
        if self.is_cache_valid(cache_key):
            return self.cache[cache_key]
        return None

    # ===========================================
    # UNIVERSAL DATA RETRIEVAL METHODS
    # These are called by ALL tabs in the terminal
    # ===========================================

    def get_stock_data(self, symbol: str, period: str = "1d", interval: str = "1m") -> Dict[str, Any]:
        """Universal stock data retrieval"""
        cache_key = f"stock_{symbol}_{period}_{interval}"

        # Check cache first
        cached_data = self.get_cache(cache_key)
        if cached_data:
            return cached_data

        source = self.get_data_source("stocks")

        try:
            if source == "yfinance":
                data = self._get_yfinance_stock_data(symbol, period, interval)
            elif source == "fincept_api":
                data = self._get_fincept_stock_data(symbol, period, interval)
            elif source == "alpha_vantage":
                data = self._get_alpha_vantage_stock_data(symbol, period, interval)
            else:
                data = self._get_fallback_stock_data(symbol, period, interval)

            # Cache successful results
            if data.get("success"):
                self.set_cache(cache_key, data, 60)  # Cache for 1 minute

            return data

        except Exception as e:
            return {
                "success": False,
                "error": f"Error fetching stock data: {str(e)}",
                "source": source,
                "symbol": symbol
            }

    def get_forex_data(self, pair: str, period: str = "1d") -> Dict[str, Any]:
        """Universal forex data retrieval"""
        cache_key = f"forex_{pair}_{period}"

        cached_data = self.get_cache(cache_key)
        if cached_data:
            return cached_data

        source = self.get_data_source("forex")

        try:
            if source == "yfinance":
                data = self._get_yfinance_forex_data(pair, period)
            elif source == "fincept_api":
                data = self._get_fincept_forex_data(pair, period)
            else:
                data = self._get_fallback_forex_data(pair, period)

            if data.get("success"):
                self.set_cache(cache_key, data, 300)  # Cache for 5 minutes

            return data

        except Exception as e:
            return {
                "success": False,
                "error": f"Error fetching forex data: {str(e)}",
                "source": source,
                "pair": pair
            }

    def get_news_data(self, category: str = "financial", limit: int = 20) -> Dict[str, Any]:
        """Universal news data retrieval"""
        cache_key = f"news_{category}_{limit}"

        cached_data = self.get_cache(cache_key)
        if cached_data:
            return cached_data

        source = self.get_data_source("news")

        try:
            if source == "fincept_api":
                data = self._get_fincept_news_data(category, limit)
            elif source == "dummy_news":
                data = self._get_dummy_news_data(category, limit)
            else:
                data = self._get_fallback_news_data(category, limit)

            if data.get("success"):
                self.set_cache(cache_key, data, 600)  # Cache for 10 minutes

            return data

        except Exception as e:
            return {
                "success": False,
                "error": f"Error fetching news data: {str(e)}",
                "source": source,
                "category": category
            }

    def get_crypto_data(self, symbol: str, period: str = "1d") -> Dict[str, Any]:
        """Universal crypto data retrieval"""
        cache_key = f"crypto_{symbol}_{period}"

        cached_data = self.get_cache(cache_key)
        if cached_data:
            return cached_data

        source = self.get_data_source("crypto")

        try:
            if source == "fincept_api":
                data = self._get_fincept_crypto_data(symbol, period)
            else:
                data = self._get_fallback_crypto_data(symbol, period)

            if data.get("success"):
                self.set_cache(cache_key, data, 120)  # Cache for 2 minutes

            return data

        except Exception as e:
            return {
                "success": False,
                "error": f"Error fetching crypto data: {str(e)}",
                "source": source,
                "symbol": symbol
            }

    def get_economic_data(self, indicator: str, country: str = "US") -> Dict[str, Any]:
        """Universal economic data retrieval"""
        cache_key = f"economic_{indicator}_{country}"

        cached_data = self.get_cache(cache_key)
        if cached_data:
            return cached_data

        source = self.get_data_source("economic")

        try:
            if source == "fincept_api":
                data = self._get_fincept_economic_data(indicator, country)
            else:
                data = self._get_fallback_economic_data(indicator, country)

            if data.get("success"):
                self.set_cache(cache_key, data, 3600)  # Cache for 1 hour

            return data

        except Exception as e:
            return {
                "success": False,
                "error": f"Error fetching economic data: {str(e)}",
                "source": source,
                "indicator": indicator
            }

    # ===========================================
    # DATA SOURCE IMPLEMENTATIONS
    # ===========================================

    def _get_yfinance_stock_data(self, symbol: str, period: str, interval: str) -> Dict[str, Any]:
        """Get stock data from Yahoo Finance"""
        try:
            ticker = yf.Ticker(symbol)
            hist = ticker.history(period=period, interval=interval)

            if hist.empty:
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

            return data

        except Exception as e:
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
            ticker = yf.Ticker(yahoo_pair)
            hist = ticker.history(period=period)

            if hist.empty:
                return {
                    "success": False,
                    "error": f"No forex data found for pair {pair}",
                    "source": "yfinance"
                }

            return {
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

        except Exception as e:
            return {
                "success": False,
                "error": f"YFinance forex error: {str(e)}",
                "source": "yfinance",
                "pair": pair
            }

    def _get_fincept_stock_data(self, symbol: str, period: str, interval: str) -> Dict[str, Any]:
        """Get stock data from Fincept API (dummy implementation)"""
        # This would use your actual Fincept API
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

    def _get_alpha_vantage_stock_data(self, symbol: str, period: str, interval: str) -> Dict[str, Any]:
        """Get stock data from Alpha Vantage (dummy implementation)"""
        # Would use actual Alpha Vantage API
        return {
            "success": True,
            "source": "alpha_vantage",
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
            "note": "This is dummy Alpha Vantage data"
        }

    def _get_fincept_crypto_data(self, symbol: str, period: str) -> Dict[str, Any]:
        """Get crypto data from Fincept API (dummy implementation)"""
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
        return self._get_yfinance_stock_data(symbol, period, interval)

    def _get_fallback_forex_data(self, pair: str, period: str) -> Dict[str, Any]:
        """Fallback forex data"""
        return {
            "success": False,
            "error": "No fallback available for forex data",
            "source": "fallback"
        }

    def _get_fallback_news_data(self, category: str, limit: int) -> Dict[str, Any]:
        """Fallback to dummy news"""
        return self._get_dummy_news_data(category, limit)

    def _get_fallback_crypto_data(self, symbol: str, period: str) -> Dict[str, Any]:
        """Fallback crypto data"""
        return {
            "success": False,
            "error": "No fallback available for crypto data",
            "source": "fallback"
        }

    def _get_fallback_economic_data(self, indicator: str, country: str) -> Dict[str, Any]:
        """Fallback economic data"""
        return {
            "success": False,
            "error": "No fallback available for economic data",
            "source": "fallback"
        }

    # ===========================================
    # DATA SOURCE MANAGEMENT
    # ===========================================

    def test_data_source(self, source_name: str, config: Dict[str, Any] = None) -> Dict[str, Any]:
        """Test if a data source is working"""
        try:
            if source_name == "yfinance":
                # Test with a common stock
                result = self._get_yfinance_stock_data("AAPL", "1d", "1h")
                return {
                    "success": result.get("success", False),
                    "message": "YFinance connection successful" if result.get("success") else result.get("error"),
                    "response_time": "< 1s"
                }
            elif source_name == "fincept_api":
                # Test Fincept API connection
                return {
                    "success": True,
                    "message": "Fincept API connection successful (dummy)",
                    "response_time": "< 1s"
                }
            else:
                return {
                    "success": True,
                    "message": f"{source_name} test successful (dummy)",
                    "response_time": "< 1s"
                }

        except Exception as e:
            return {
                "success": False,
                "message": f"Test failed: {str(e)}",
                "response_time": "timeout"
            }

    def get_available_sources(self) -> Dict[str, Any]:
        """Get all available data sources"""
        return self.available_sources

    def get_source_config(self, source_name: str) -> Dict[str, Any]:
        """Get configuration for a specific source"""
        return self.config.get("source_configs", {}).get(source_name, {})

    def get_current_mappings(self) -> Dict[str, str]:
        """Get current data type to source mappings"""
        return self.config.get("data_mappings", {})

    def import_csv_data(self, file_path: str, data_type: str, column_mapping: Dict[str, str]) -> Dict[str, Any]:
        """Import data from CSV file"""
        try:
            df = pd.read_csv(file_path)

            # Apply column mapping
            mapped_data = {}
            for standard_col, csv_col in column_mapping.items():
                if csv_col in df.columns:
                    mapped_data[standard_col] = df[csv_col].tolist()

            return {
                "success": True,
                "source": "csv_import",
                "data_type": data_type,
                "data": mapped_data,
                "row_count": len(df),
                "imported_at": datetime.now().isoformat()
            }

        except Exception as e:
            return {
                "success": False,
                "error": f"CSV import error: {str(e)}",
                "source": "csv_import"
            }

    def clear_cache(self, data_type: str = None):
        """Clear cache for specific data type or all"""
        if data_type:
            keys_to_remove = [k for k in self.cache.keys() if k.startswith(data_type)]
            for key in keys_to_remove:
                del self.cache[key]
                if key in self.cache_expiry:
                    del self.cache_expiry[key]
            print(f"🗑️ Cleared cache for {data_type}")
        else:
            self.cache.clear()
            self.cache_expiry.clear()
            print("🗑️ Cleared all cache")

    def get_cache_stats(self) -> Dict[str, Any]:
        """Get cache statistics"""
        total_items = len(self.cache)
        expired_items = sum(1 for k in self.cache.keys() if not self.is_cache_valid(k))

        return {
            "total_items": total_items,
            "valid_items": total_items - expired_items,
            "expired_items": expired_items,
            "cache_hit_rate": "Not tracked",  # Could implement hit rate tracking
            "memory_usage": f"{len(str(self.cache))} bytes (estimated)"
        }

    def reset_to_defaults(self):
        """Reset configuration to defaults"""
        self.config = {"data_mappings": self.default_sources.copy(), "source_configs": {}}
        self.save_configuration()
        self.clear_cache()
        print("🔄 Reset to default configuration")


# Global instance - will be imported by tabs
data_source_manager = None


def get_data_source_manager(app=None):
    """Get global data source manager instance"""
    global data_source_manager
    if data_source_manager is None and app:
        data_source_manager = DataSourceManager(app)
    return data_source_manager