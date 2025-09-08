# -*- coding: utf-8 -*-
# market_tab.py

import dearpygui.dearpygui as dpg
from fincept_terminal.utils.base_tab import BaseTab
import threading
import time
import datetime
import random
from typing import Dict, Any, List, Optional, Tuple

# Import logging system (reduced usage)
from fincept_terminal.utils.Logging.logger import (
    info, error, warning
)

# Try to import yfinance with proper error handling
try:
    import yfinance as yf

    YFINANCE_AVAILABLE = True
    info("yfinance library loaded successfully", module="MarketTab")
except ImportError as e:
    YFINANCE_AVAILABLE = False
    warning("yfinance not available, using simulated data", module="MarketTab")

# Market data configuration
MARKET_ASSETS = {
    "Stock Indices": {
        "S&P 500": 5232.35, "Nasdaq 100": 16412.87, "Dow Jones": 38546.12,
        "Russell 2000": 2087.45, "VIX": 18.23, "FTSE 100": 7623.98,
        "DAX 40": 18245.67, "CAC 40": 7890.12, "Nikkei 225": 35897.45,
        "Hang Seng": 18654.23
    },
    "Forex": {
        "EUR/USD": 1.0840, "GBP/USD": 1.2567, "USD/JPY": 151.45,
        "USD/CHF": 0.8923, "USD/CAD": 1.3578, "AUD/USD": 0.6634,
        "NZD/USD": 0.6089, "EUR/GBP": 0.8623, "EUR/JPY": 164.23,
        "GBP/JPY": 190.45
    },
    "Commodities": {
        "Gold": 2312.80, "Silver": 28.45, "Crude Oil (WTI)": 78.35,
        "Brent Crude": 82.10, "Natural Gas": 3.45, "Copper": 4.23,
        "Platinum": 987.50, "Palladium": 1045.20, "Wheat": 567.80,
        "Corn": 478.90
    },
    "Bonds": {
        "US 10Y Treasury": 4.35, "US 30Y Treasury": 4.52, "US 2Y Treasury": 4.89,
        "Germany 10Y": 2.45, "UK 10Y": 4.12, "Japan 10Y": 0.78,
        "France 10Y": 2.98, "Italy 10Y": 4.23, "Spain 10Y": 3.67,
        "Canada 10Y": 3.89
    },
    "ETFs": {
        "SPY (S&P 500)": 523.45, "QQQ (Nasdaq)": 412.67, "DIA (Dow)": 385.23,
        "EEM (Emerging)": 41.78, "GLD (Gold)": 231.45, "XLK (Tech)": 198.90,
        "XLE (Energy)": 89.34, "XLF (Finance)": 42.56, "XLV (Health)": 156.78,
        "VNQ (REIT)": 78.90
    },
    "Cryptocurrencies": {
        "Bitcoin": 67890.45, "Ethereum": 3789.23, "Binance Coin": 567.89,
        "Solana": 234.56, "Dogecoin": 0.1234, "Polygon": 1.23,
        "Chainlink": 23.45, "Cardano": 0.67, "Polkadot": 12.34,
        "Avalanche": 45.67
    }
}

# Regional stock symbols
REGIONAL_STOCKS = {
    "India": {
        "symbols": ["RELIANCE.NS", "TCS.NS", "HDFCBANK.NS", "INFY.NS", "HINDUNILVR.NS",
                    "ICICIBANK.NS", "SBIN.NS", "BHARTIARTL.NS", "ITC.NS", "KOTAKBANK.NS"],
        "names": ["Reliance Industries", "Tata Consultancy", "HDFC Bank", "Infosys",
                  "Hindustan Unilever", "ICICI Bank", "State Bank of India",
                  "Bharti Airtel", "ITC Limited", "Kotak Mahindra Bank"]
    },
    "China": {
        "symbols": ["BABA", "PDD", "JD", "BIDU", "TCEHY", "NIO", "LI", "XPEV", "EDU", "BILI"],
        "names": ["Alibaba Group", "PDD Holdings", "JD.com", "Baidu", "Tencent Holdings",
                  "NIO Inc", "Li Auto", "XPeng", "New Oriental Education", "Bilibili"]
    },
    "United States": {
        "symbols": ["AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "BRK-B", "JPM", "V"],
        "names": ["Apple Inc", "Microsoft Corp", "Alphabet Inc", "Amazon.com", "NVIDIA Corp",
                  "Meta Platforms", "Tesla Inc", "Berkshire Hathaway", "JPMorgan Chase", "Visa Inc"]
    }
}


class MarketTab(BaseTab):
    """Optimized Bloomberg Terminal style Market tab with efficient data fetching"""

    def __init__(self, main_app=None):
        super().__init__(main_app)

        self.main_app = main_app

        # Bloomberg color scheme
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]

        # Configuration - optimized intervals
        self.last_update = None
        self.update_interval = 600  # 10 minutes for real data
        self.simulated_update_interval = 5  # 5 seconds for simulated data
        self.data_loading = False
        self.auto_update = True
        self.ui_initialized = False
        self.background_thread = None
        self.shutdown_requested = False

        # Thread-safe locks
        self.data_lock = threading.Lock()

        # Initialize data stores
        self.market_data = {}
        self.regional_data = {}

        # Initialize components
        self.initialize_market_data()
        self.initialize_regional_data()

        # Start background updates
        self.start_background_updates()

        info("Market Tab initialized", module="MarketTab")

    def get_label(self) -> str:
        """Get tab label"""
        return "Markets"

    def initialize_market_data(self):
        """Initialize market data with minimal logging"""
        try:
            with self.data_lock:
                self.market_data = {}

                for category, assets in MARKET_ASSETS.items():
                    self.market_data[category] = {}

                    for asset_name, base_price in assets.items():
                        if isinstance(base_price, (int, float)) and base_price > 0:
                            current_price = base_price * (1 + random.uniform(-0.05, 0.05))
                            change_1d = current_price * random.uniform(-0.03, 0.03)
                            change_percent_1d = (change_1d / current_price) * 100 if current_price != 0 else 0

                            self.market_data[category][asset_name] = {
                                "price": round(current_price, 2),
                                "change_1d": round(change_1d, 2),
                                "change_percent_1d": round(change_percent_1d, 2),
                                "change_percent_7d": round(random.uniform(-5, 5), 2),
                                "change_percent_30d": round(random.uniform(-15, 15), 2)
                            }

        except Exception as e:
            error("Failed to initialize market data", module="MarketTab")
            self.market_data = {}

    def initialize_regional_data(self):
        """Initialize regional stock data with fallback"""
        try:
            with self.data_lock:
                self.regional_data = {}

                for region, data in REGIONAL_STOCKS.items():
                    symbols = data.get("symbols", [])
                    names = data.get("names", [])

                    if symbols:
                        self.regional_data[region] = {}
                        fallback_data = self.get_fallback_regional_data(symbols)

                        for i, symbol in enumerate(symbols):
                            if symbol in fallback_data:
                                display_name = names[i] if i < len(names) else symbol
                                self.regional_data[region][symbol] = {
                                    "name": display_name,
                                    **fallback_data[symbol]
                                }

        except Exception as e:
            error("Failed to initialize regional data", module="MarketTab")
            self.regional_data = {}

    def get_fallback_regional_data(self, symbols: List[str]) -> Dict[str, Dict[str, Any]]:
        """Generate fallback data for regional stocks"""
        result = {}
        base_prices = {
            "AAPL": 175, "MSFT": 420, "GOOGL": 140, "RELIANCE.NS": 2500,
            "TCS.NS": 3500, "BABA": 85, "NVDA": 800, "TSLA": 200
        }

        for symbol in symbols:
            # Get base price with regional adjustments
            if symbol.endswith('.NS'):  # Indian stocks
                base_price = base_prices.get(symbol, random.uniform(500, 3000))
            elif symbol in ['BABA', 'PDD', 'JD', 'BIDU']:  # Chinese stocks
                base_price = base_prices.get(symbol, random.uniform(20, 200))
            else:  # US stocks
                base_price = base_prices.get(symbol, random.uniform(50, 500))

            change_pct = round(random.uniform(-3, 3), 2)
            price = round(base_price * (1 + change_pct / 100), 2)
            change_val = round(price * change_pct / 100, 2)

            result[symbol] = {
                "price": price,
                "change_1d": change_val,
                "change_percent_1d": change_pct,
                "change_percent_7d": round(random.uniform(-5, 5), 2),
                "change_percent_30d": round(random.uniform(-15, 15), 2),
                "volume": random.randint(100000, 10000000),
                "high": round(price * random.uniform(1.01, 1.05), 2),
                "low": round(price * random.uniform(0.95, 0.99), 2)
            }

        return result

    def get_real_stock_data_batch(self, symbols: List[str], timeout: int = 10) -> Dict[str, Dict[str, Any]]:
        """Get real stock data using yfinance - OPTIMIZED to reduce API calls"""
        if not YFINANCE_AVAILABLE:
            return self.get_fallback_regional_data(symbols)

        try:
            # Use yfinance batch download for efficiency - ONE API CALL for all symbols
            symbols_str = " ".join(symbols)

            try:
                # Single batch request instead of individual requests
                data = yf.download(symbols_str, period="30d", interval="1d",
                                   group_by='ticker', auto_adjust=True,
                                   prepost=True, threads=True, timeout=timeout)

                result = {}
                successful_fetches = 0

                for symbol in symbols:
                    try:
                        if len(symbols) == 1:
                            symbol_data = data
                        else:
                            symbol_data = data[symbol] if symbol in data.columns.get_level_values(0) else None

                        if symbol_data is None or symbol_data.empty:
                            # Use fallback for this symbol
                            fallback = self.get_fallback_regional_data([symbol])
                            result[symbol] = fallback[symbol]
                            continue

                        # Extract latest price data
                        current_price = float(symbol_data['Close'].iloc[-1])
                        volume = int(symbol_data['Volume'].iloc[-1]) if 'Volume' in symbol_data.columns else 0
                        high = float(symbol_data['High'].iloc[-1])
                        low = float(symbol_data['Low'].iloc[-1])

                        # Calculate changes
                        prev_price = float(symbol_data['Close'].iloc[-2]) if len(symbol_data) >= 2 else current_price
                        change_val = current_price - prev_price
                        change_pct_1d = (change_val / prev_price) * 100 if prev_price != 0 else 0

                        # Calculate 7D and 30D changes
                        change_pct_7d = 0.0
                        if len(symbol_data) >= 7:
                            price_7d_ago = float(symbol_data['Close'].iloc[-7])
                            change_pct_7d = ((
                                                         current_price - price_7d_ago) / price_7d_ago) * 100 if price_7d_ago != 0 else 0

                        change_pct_30d = 0.0
                        if len(symbol_data) >= 30:
                            price_30d_ago = float(symbol_data['Close'].iloc[-30])
                            change_pct_30d = ((
                                                          current_price - price_30d_ago) / price_30d_ago) * 100 if price_30d_ago != 0 else 0
                        elif len(symbol_data) > 1:
                            price_start = float(symbol_data['Close'].iloc[0])
                            change_pct_30d = ((
                                                          current_price - price_start) / price_start) * 100 if price_start != 0 else 0

                        result[symbol] = {
                            "price": round(max(0, current_price), 2),
                            "change_1d": round(change_val, 2),
                            "change_percent_1d": round(change_pct_1d, 2),
                            "change_percent_7d": round(change_pct_7d, 2),
                            "change_percent_30d": round(change_pct_30d, 2),
                            "volume": max(0, volume),
                            "high": round(max(current_price, high), 2),
                            "low": round(min(current_price, low), 2)
                        }
                        successful_fetches += 1

                    except Exception:
                        # Use fallback for failed symbols
                        fallback = self.get_fallback_regional_data([symbol])
                        result[symbol] = fallback[symbol]

                if successful_fetches > 0:
                    info(f"Successfully fetched real data for {successful_fetches}/{len(symbols)} symbols",
                         module="MarketTab")

                return result

            except Exception:
                # If batch download fails, use fallback for all symbols
                return self.get_fallback_regional_data(symbols)

        except Exception as e:
            error("Error in stock data fetch", module="MarketTab")
            return self.get_fallback_regional_data(symbols)

    def should_update_real_data(self) -> bool:
        """Check if real data should be updated (10-minute interval)"""
        if self.last_update is None:
            return True

        time_since_update = time.time() - self.last_update
        return time_since_update >= self.update_interval

    def update_regional_data_background(self):
        """Update regional data in background - SINGLE BATCH CALL per region"""
        if self.data_loading or self.shutdown_requested:
            return

        def fetch_regional_data():
            try:
                self.data_loading = True
                info("Starting regional data update", module="MarketTab")

                for region, data in REGIONAL_STOCKS.items():
                    if self.shutdown_requested:
                        break

                    symbols = data["symbols"]
                    names = data["names"]

                    # Single batch call per region - MUCH more efficient
                    region_data = self.get_real_stock_data_batch(symbols, timeout=15)

                    # Update regional data with thread safety
                    with self.data_lock:
                        for i, symbol in enumerate(symbols):
                            if symbol in region_data:
                                display_name = names[i] if i < len(names) else symbol
                                self.regional_data[region][symbol] = {
                                    "name": display_name,
                                    **region_data[symbol]
                                }

                # Update timestamp
                self.last_update = time.time()
                info("Regional data update completed", module="MarketTab")

            except Exception as e:
                error("Error in regional data update", module="MarketTab")
            finally:
                self.data_loading = False

        # Start background thread
        thread = threading.Thread(target=fetch_regional_data, daemon=True)
        thread.start()

    def start_background_updates(self):
        """Start optimized background update system"""

        def update_loop():
            try:
                # Initial real data fetch
                if not self.shutdown_requested:
                    self.update_regional_data_background()

                last_simulated_update = time.time()

                while self.auto_update and not self.shutdown_requested:
                    try:
                        current_time = time.time()

                        # Update simulated data every 5 seconds
                        if current_time - last_simulated_update >= self.simulated_update_interval:
                            if not self.shutdown_requested:
                                self.update_market_data()
                                last_simulated_update = current_time

                        # Check for real data update every 10 minutes
                        if (self.should_update_real_data() and
                                not self.data_loading and
                                not self.shutdown_requested):
                            self.update_regional_data_background()

                        time.sleep(1)  # Check every second

                    except Exception:
                        time.sleep(5)  # Wait before retrying

            except Exception as e:
                error("Error in background update loop", module="MarketTab")

        self.background_thread = threading.Thread(target=update_loop, daemon=True)
        self.background_thread.start()

    def create_content(self):
        """Create Bloomberg-style market terminal layout"""
        try:
            # Top bar with branding and search
            self.create_header_bar()
            dpg.add_separator()

            # Control panel
            self.create_control_panel()
            dpg.add_separator()

            # Create scrollable content
            with dpg.child_window(height=-50, border=False):
                # Global markets section
                dpg.add_text("GLOBAL MARKETS", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()
                self.create_market_grid()

                dpg.add_spacer(height=20)

                # Regional stocks section
                dpg.add_text("REGIONAL MARKETS - LIVE DATA", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()
                self.create_regional_markets()

            # Status bar
            dpg.add_separator()
            self.create_status_bar()

            self.ui_initialized = True

        except Exception as e:
            error("Failed to create market content", module="MarketTab")
            self.create_error_content(str(e))

    def create_error_content(self, error_message: str):
        """Create error content when main content creation fails"""
        dpg.add_text("MARKET TERMINAL - ERROR", color=self.BLOOMBERG_RED)
        dpg.add_separator()
        dpg.add_text(f"Error loading market data: {error_message}", color=self.BLOOMBERG_WHITE)
        dpg.add_spacer(height=20)
        dpg.add_button(label="Retry", callback=self.retry_callback)

    def create_header_bar(self):
        """Create header bar with search functionality"""
        with dpg.group(horizontal=True):
            dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("MARKET TERMINAL", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_input_text(label="", default_value="Search Symbol", width=200, tag="symbol_search")
            dpg.add_button(label="SEARCH", width=80, callback=self.search_callback)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), tag="market_time_display")

    def create_control_panel(self):
        """Create control panel with buttons and status indicators"""
        with dpg.group(horizontal=True):
            dpg.add_button(label="REFRESH", callback=self.refresh_callback, width=80)
            dpg.add_button(label="AUTO ON", callback=self.toggle_auto_update, tag="auto_toggle_btn", width=80)
            dpg.add_combo(["1 min", "5 min", "10 min", "30 min"], default_value="10 min", width=80,
                          tag="refresh_interval")
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LAST UPDATE:", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.datetime.now().strftime('%H:%M:%S'), tag="last_update_time",
                         color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            status_color = self.BLOOMBERG_ORANGE if self.data_loading else self.BLOOMBERG_GREEN
            status_text = "UPDATING" if self.data_loading else "LIVE"
            dpg.add_text("●", color=status_color, tag="status_indicator")
            dpg.add_text(status_text, color=status_color, tag="status_text")

    def create_market_grid(self):
        """Create 3x2 market grid"""
        categories = list(self.market_data.keys())

        # First row - 3 categories
        with dpg.group(horizontal=True):
            for i in range(3):
                if i < len(categories):
                    self.create_market_panel(categories[i], 500, 300)

        dpg.add_spacer(height=10)

        # Second row - 3 categories
        with dpg.group(horizontal=True):
            for i in range(3, 6):
                if i < len(categories):
                    self.create_market_panel(categories[i], 500, 300)

    def create_regional_markets(self):
        """Create regional markets section with real data"""
        # Three regional tables side by side
        with dpg.group(horizontal=True):
            for region in ["India", "China", "United States"]:
                if region in self.regional_data:
                    self.create_regional_panel(region, 500, 400)

    def create_regional_panel(self, region: str, width: int, height: int):
        """Create regional stock panel with real data"""
        with dpg.child_window(width=width, height=height, border=True):
            # Panel header with flag emoji
            flags = {"India": "🇮🇳", "China": "🇨🇳", "United States": "🇺🇸"}
            flag = flags.get(region, "🌍")
            dpg.add_text(f"{flag} {region.upper()} STOCKS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Data table with scrolling
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           scrollY=True, scrollX=True, height=height - 60):
                # Table columns
                dpg.add_table_column(label="Company", width_fixed=True, init_width_or_weight=200)
                dpg.add_table_column(label="Symbol", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Price", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Chg", width_fixed=True, init_width_or_weight=60)
                dpg.add_table_column(label="1D%", width_fixed=True, init_width_or_weight=60)
                dpg.add_table_column(label="Vol", width_fixed=True, init_width_or_weight=80)

                # Add data rows
                if region in self.regional_data:
                    for symbol, data in self.regional_data[region].items():
                        with dpg.table_row():
                            # Company name (truncated)
                            name = data.get("name", symbol)
                            name_display = name[:20] + "..." if len(name) > 20 else name
                            dpg.add_text(name_display, color=self.BLOOMBERG_WHITE)

                            # Symbol (cleaned)
                            display_symbol = symbol.replace(".NS", "").replace(".HK", "")
                            dpg.add_text(display_symbol, color=self.BLOOMBERG_YELLOW)

                            # Price with currency formatting
                            price = data.get("price", 0)
                            if region == "India":
                                price_str = f"₹{price:,.0f}" if price >= 100 else f"₹{price:.2f}"
                            elif region == "China":
                                price_str = f"${price:.2f}"
                            else:  # US
                                price_str = f"${price:.2f}"
                            dpg.add_text(price_str, color=self.BLOOMBERG_WHITE)

                            # 1D Change
                            change_1d = data.get("change_1d", 0)
                            change_color = self.BLOOMBERG_GREEN if change_1d >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{change_1d:+.2f}", color=change_color)

                            # 1D %
                            change_percent_1d = data.get("change_percent_1d", 0)
                            percent_color = self.BLOOMBERG_GREEN if change_percent_1d >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{change_percent_1d:+.2f}%", color=percent_color)

                            # Volume
                            volume = data.get("volume", 0)
                            if volume >= 1000000:
                                vol_str = f"{volume / 1000000:.1f}M"
                            elif volume >= 1000:
                                vol_str = f"{volume / 1000:.1f}K"
                            else:
                                vol_str = f"{volume:,}"
                            dpg.add_text(vol_str, color=self.BLOOMBERG_GRAY)

    def create_market_panel(self, category: str, width: int, height: int):
        """Create individual market panel"""
        with dpg.child_window(width=width, height=height, border=True):
            # Panel header
            dpg.add_text(f"{category.upper()}", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Data table
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           scrollY=True, scrollX=True, height=height - 60):
                dpg.add_table_column(label="Asset", width_fixed=True, init_width_or_weight=180)
                dpg.add_table_column(label="Last", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Chg", width_fixed=True, init_width_or_weight=60)
                dpg.add_table_column(label="1D%", width_fixed=True, init_width_or_weight=60)
                dpg.add_table_column(label="7D%", width_fixed=True, init_width_or_weight=60)
                dpg.add_table_column(label="30D%", width_fixed=True, init_width_or_weight=60)

                # Add data rows
                assets = self.market_data.get(category, {})
                for asset_name, data in list(assets.items())[:10]:  # Show top 10
                    with dpg.table_row():
                        # Asset name
                        asset_display = asset_name[:25] + "..." if len(asset_name) > 25 else asset_name
                        dpg.add_text(asset_display, color=self.BLOOMBERG_WHITE)

                        # Price formatting
                        price = data.get("price", 0)
                        if price < 1:
                            price_str = f"{price:.4f}"
                        elif price < 100:
                            price_str = f"{price:.2f}"
                        else:
                            price_str = f"{price:,.0f}"
                        dpg.add_text(price_str, color=self.BLOOMBERG_WHITE)

                        # Changes with color coding
                        change_1d = data.get("change_1d", 0)
                        change_color = self.BLOOMBERG_GREEN if change_1d >= 0 else self.BLOOMBERG_RED
                        dpg.add_text(f"{change_1d:+.2f}", color=change_color)

                        for period in ["change_percent_1d", "change_percent_7d", "change_percent_30d"]:
                            percent_change = data.get(period, 0)
                            percent_color = self.BLOOMBERG_GREEN if percent_change >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{percent_change:+.2f}%", color=percent_color)

    def create_status_bar(self):
        """Create status bar with market information"""
        with dpg.group(horizontal=True):
            dpg.add_text("MARKET STATUS:", color=self.BLOOMBERG_GRAY)

            # Market hours
            current_hour = datetime.datetime.now().hour
            if 9 <= current_hour < 16:
                dpg.add_text("OPEN", color=self.BLOOMBERG_GREEN)
            else:
                dpg.add_text("CLOSED", color=self.BLOOMBERG_RED)

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("DATA FEED:", color=self.BLOOMBERG_GRAY)
            data_status = "LIVE" if YFINANCE_AVAILABLE else "SIMULATED"
            data_color = self.BLOOMBERG_GREEN if YFINANCE_AVAILABLE else self.BLOOMBERG_ORANGE
            dpg.add_text(data_status, color=data_color)

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("ASSETS:", color=self.BLOOMBERG_GRAY)

            # Calculate total assets
            market_assets = sum(len(assets) for assets in self.market_data.values())
            regional_assets = sum(len(stocks) for stocks in self.regional_data.values())
            total_assets = market_assets + regional_assets
            dpg.add_text(f"{total_assets}", color=self.BLOOMBERG_YELLOW)

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("AUTO-UPDATE:", color=self.BLOOMBERG_GRAY)
            status_text = "ON" if self.auto_update else "OFF"
            status_color = self.BLOOMBERG_GREEN if self.auto_update else self.BLOOMBERG_RED
            dpg.add_text(status_text, color=status_color, tag="auto_status_text")

            if self.last_update:
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("LAST REAL DATA UPDATE:", color=self.BLOOMBERG_GRAY)
                last_update_str = datetime.datetime.fromtimestamp(self.last_update).strftime('%H:%M')
                dpg.add_text(last_update_str, color=self.BLOOMBERG_WHITE)

    def update_market_data(self):
        """Update market data with simulated changes"""
        try:
            if self.shutdown_requested:
                return

            with self.data_lock:
                # Update each asset with small random changes
                for category in self.market_data:
                    for asset_name in self.market_data[category]:
                        data = self.market_data[category][asset_name]
                        current_price = data.get("price", 0)

                        if current_price > 0:
                            # Small price movement (±1% max change)
                            change_factor = 1 + random.uniform(-0.01, 0.01)
                            new_price = current_price * change_factor
                            new_change_1d = new_price - current_price
                            new_change_percent_1d = (new_change_1d / current_price) * 100

                            # Update data
                            self.market_data[category][asset_name].update({
                                "price": round(max(0.0001, new_price), 2 if new_price >= 1 else 4),
                                "change_1d": round(new_change_1d, 2),
                                "change_percent_1d": round(new_change_percent_1d, 2),
                                "change_percent_7d": round(data.get("change_percent_7d", 0) + random.uniform(-0.5, 0.5),
                                                           2),
                                "change_percent_30d": round(
                                    data.get("change_percent_30d", 0) + random.uniform(-0.2, 0.2), 2)
                            })

            # Update UI timestamps
            self.update_ui_timestamps()

        except Exception as e:
            error("Failed to update market data", module="MarketTab")

    def update_ui_timestamps(self):
        """Update UI timestamps and status indicators"""
        try:
            current_time = datetime.datetime.now().strftime('%H:%M:%S')
            current_datetime = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')

            # Update timestamps
            if dpg.does_item_exist("last_update_time"):
                dpg.set_value("last_update_time", current_time)
            if dpg.does_item_exist("market_time_display"):
                dpg.set_value("market_time_display", current_datetime)

            # Update status indicators
            if dpg.does_item_exist("status_indicator") and dpg.does_item_exist("status_text"):
                status_color = self.BLOOMBERG_ORANGE if self.data_loading else self.BLOOMBERG_GREEN
                status_text = "UPDATING" if self.data_loading else "LIVE"
                dpg.configure_item("status_indicator", color=status_color)
                dpg.set_value("status_text", status_text)
                dpg.configure_item("status_text", color=status_color)

        except Exception:
            pass  # Fail silently for UI updates

    # Callback methods
    def search_callback(self):
        """Search callback"""
        try:
            if dpg.does_item_exist("symbol_search"):
                search_term = dpg.get_value("symbol_search")
                if search_term and search_term != "Search Symbol":
                    info(f"Symbol search: {search_term}", module="MarketTab")
                    # TODO: Implement search functionality
        except Exception:
            pass

    def refresh_callback(self):
        """Manual refresh callback"""
        try:
            info("Manual refresh requested", module="MarketTab")

            # Reinitialize market data
            self.initialize_market_data()

            # Trigger fresh real data fetch
            if not self.data_loading:
                self.update_regional_data_background()

            # Update simulated data immediately
            self.update_market_data()

        except Exception as e:
            error("Manual refresh failed", module="MarketTab")

    def toggle_auto_update(self):
        """Toggle auto-update"""
        try:
            self.auto_update = not self.auto_update

            # Update button text
            button_text = "AUTO ON" if self.auto_update else "AUTO OFF"
            if dpg.does_item_exist("auto_toggle_btn"):
                dpg.set_item_label("auto_toggle_btn", button_text)

            # Update status text
            status_text = "ON" if self.auto_update else "OFF"
            status_color = self.BLOOMBERG_GREEN if self.auto_update else self.BLOOMBERG_RED
            if dpg.does_item_exist("auto_status_text"):
                dpg.set_value("auto_status_text", status_text)
                dpg.configure_item("auto_status_text", color=status_color)

            # Restart background updates if enabled
            if self.auto_update and (not self.background_thread or not self.background_thread.is_alive()):
                self.start_background_updates()

        except Exception as e:
            error("Failed to toggle auto-update", module="MarketTab")

    def retry_callback(self):
        """Retry loading data after error"""
        try:
            info("Retry requested", module="MarketTab")
            self.initialize_market_data()
            self.initialize_regional_data()
            if not self.data_loading:
                self.update_regional_data_background()
        except Exception as e:
            error("Retry failed", module="MarketTab")

    def resize_components(self, left_width: int, center_width: int, right_width: int,
                          top_height: int, bottom_height: int, cell_height: int):
        """Handle resize events"""
        # Market panels have fixed sizes for stability - no resize needed
        pass

    def cleanup(self):
        """Clean up resources"""
        try:
            info("Starting Market Tab cleanup", module="MarketTab")

            # Signal shutdown
            self.shutdown_requested = True
            self.auto_update = False

            # Wait for background thread to finish
            if self.background_thread and self.background_thread.is_alive():
                self.background_thread.join(timeout=5)

            # Clear data
            with self.data_lock:
                self.market_data = {}
                self.regional_data = {}

            # Reset state
            self.data_loading = False
            self.ui_initialized = False
            self.last_update = None

            info("Market Tab cleanup completed", module="MarketTab")

        except Exception as e:
            error("Market Tab cleanup failed", module="MarketTab")

    def get_market_health_status(self) -> Dict[str, Any]:
        """Get current market tab health status for monitoring"""
        try:
            return {
                'ui_initialized': self.ui_initialized,
                'auto_update_enabled': self.auto_update,
                'data_loading': self.data_loading,
                'background_thread_alive': self.background_thread.is_alive() if self.background_thread else False,
                'last_update_timestamp': self.last_update,
                'market_categories_count': len(self.market_data),
                'regional_markets_count': len(self.regional_data),
                'total_assets': sum(len(assets) for assets in self.market_data.values()) +
                                sum(len(stocks) for stocks in self.regional_data.values()),
                'yfinance_available': YFINANCE_AVAILABLE,
                'shutdown_requested': self.shutdown_requested
            }
        except Exception as e:
            error("Failed to get market health status", module="MarketTab")
            return {'error': str(e)}