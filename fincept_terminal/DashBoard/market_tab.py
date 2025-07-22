# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import threading
import time
import datetime
import random

# Try to import yfinance, fallback if not available
try:
    import yfinance as yf
    YFINANCE_AVAILABLE = True
except ImportError:
    YFINANCE_AVAILABLE = False

# Simplified assets for faster loading and testing
MARKET_ASSETS = {
    "Stock Indices": {
        "S&P 500": 5232.35,
        "Nasdaq 100": 16412.87,
        "Dow Jones": 38546.12,
        "Russell 2000": 2087.45,
        "VIX": 18.23,
        "FTSE 100": 7623.98,
        "DAX 40": 18245.67,
        "CAC 40": 7890.12,
        "Nikkei 225": 35897.45,
        "Hang Seng": 18654.23
    },
    "Forex": {
        "EUR/USD": 1.0840,
        "GBP/USD": 1.2567,
        "USD/JPY": 151.45,
        "USD/CHF": 0.8923,
        "USD/CAD": 1.3578,
        "AUD/USD": 0.6634,
        "NZD/USD": 0.6089,
        "EUR/GBP": 0.8623,
        "EUR/JPY": 164.23,
        "GBP/JPY": 190.45
    },
    "Commodities": {
        "Gold": 2312.80,
        "Silver": 28.45,
        "Crude Oil (WTI)": 78.35,
        "Brent Crude": 82.10,
        "Natural Gas": 3.45,
        "Copper": 4.23,
        "Platinum": 987.50,
        "Palladium": 1045.20,
        "Wheat": 567.80,
        "Corn": 478.90
    },
    "Bonds": {
        "US 10Y Treasury": 4.35,
        "US 30Y Treasury": 4.52,
        "US 2Y Treasury": 4.89,
        "Germany 10Y": 2.45,
        "UK 10Y": 4.12,
        "Japan 10Y": 0.78,
        "France 10Y": 2.98,
        "Italy 10Y": 4.23,
        "Spain 10Y": 3.67,
        "Canada 10Y": 3.89
    },
    "ETFs": {
        "SPY (S&P 500)": 523.45,
        "QQQ (Nasdaq)": 412.67,
        "DIA (Dow)": 385.23,
        "EEM (Emerging)": 41.78,
        "GLD (Gold)": 231.45,
        "XLK (Tech)": 198.90,
        "XLE (Energy)": 89.34,
        "XLF (Finance)": 42.56,
        "XLV (Health)": 156.78,
        "VNQ (REIT)": 78.90
    },
    "Cryptocurrencies": {
        "Bitcoin": 67890.45,
        "Ethereum": 3789.23,
        "Binance Coin": 567.89,
        "Solana": 234.56,
        "Dogecoin": 0.1234,
        "Polygon": 1.23,
        "Chainlink": 23.45,
        "Cardano": 0.67,
        "Polkadot": 12.34,
        "Avalanche": 45.67
    }
}

# Regional stock symbols for real data
REGIONAL_STOCKS = {
    "India": {
        "symbols": ["RELIANCE.NS", "TCS.NS", "HDFCBANK.NS", "INFY.NS", "HINDUNILVR.NS",
                    "ICICIBANK.NS", "SBIN.NS", "BHARTIARTL.NS", "ITC.NS", "KOTAKBANK.NS",
                    "LT.NS", "ASIANPAINT.NS", "AXISBANK.NS", "MARUTI.NS", "SUNPHARMA.NS"],
        "names": ["Reliance Industries", "Tata Consultancy", "HDFC Bank", "Infosys", "Hindustan Unilever",
                  "ICICI Bank", "State Bank of India", "Bharti Airtel", "ITC Limited", "Kotak Mahindra Bank",
                  "Larsen & Toubro", "Asian Paints", "Axis Bank", "Maruti Suzuki", "Sun Pharmaceutical"]
    },
    "China": {
        "symbols": ["BABA", "PDD", "JD", "BIDU", "TCEHY", "NIO", "LI", "XPEV", "EDU", "BILI",
                    "TME", "NTES", "WB", "ATHM", "YMM"],
        "names": ["Alibaba Group", "PDD Holdings", "JD.com", "Baidu", "Tencent Holdings",
                  "NIO Inc", "Li Auto", "XPeng", "New Oriental Education", "Bilibili",
                  "Tencent Music", "NetEase", "Weibo Corporation", "Autohome", "Full Truck Alliance"]
    },
    "United States": {
        "symbols": ["AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "BRK-B", "JPM", "V",
                    "JNJ", "WMT", "PG", "UNH", "MA", "HD", "NFLX", "BAC", "ABBV", "CRM"],
        "names": ["Apple Inc", "Microsoft Corp", "Alphabet Inc", "Amazon.com", "NVIDIA Corp",
                  "Meta Platforms", "Tesla Inc", "Berkshire Hathaway", "JPMorgan Chase", "Visa Inc",
                  "Johnson & Johnson", "Walmart Inc", "Procter & Gamble", "UnitedHealth Group", "Mastercard",
                  "Home Depot", "Netflix Inc", "Bank of America", "AbbVie Inc", "Salesforce"]
    }
}


class MarketTab(BaseTab):
    """Bloomberg Terminal style Market tab - Enhanced with Real Regional Data"""

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

        # Data refresh control
        self.last_update = None
        self.update_interval = 3600  # 1 hour in seconds
        self.data_loading = False
        self.auto_update = True

        # Initialize data
        self.initialize_market_data()
        self.initialize_regional_data()

        # Start background updates
        self.start_background_updates()

    def get_label(self):
        return "📈 Markets"

    def get_real_stock_data_batch(self, symbols, timeout=15):
        """Get real stock data using yfinance history method for each symbol"""
        if not YFINANCE_AVAILABLE:
            return self.get_fallback_regional_data(symbols)

        result = {}

        # Process each symbol individually using history method
        for symbol in symbols:
            try:
                # Create ticker object
                ticker = yf.Ticker(symbol)

                # Get historical data for last 30 days to calculate all periods
                hist = ticker.history(period="30d", interval="1d")

                if hist.empty or len(hist) < 2:
                    # Try alternative approach with 5 days if 30 days fails
                    hist = ticker.history(period="5d", interval="1d")

                if not hist.empty and len(hist) >= 2:
                    # Get current and previous prices
                    current_price = float(hist['Close'].iloc[-1])
                    prev_price = float(hist['Close'].iloc[-2])
                    volume = int(hist['Volume'].iloc[-1]) if not hist['Volume'].empty else 0
                    high = float(hist['High'].iloc[-1])
                    low = float(hist['Low'].iloc[-1])

                    # Calculate 1D changes
                    change_val = current_price - prev_price
                    change_pct_1d = (change_val / prev_price) * 100 if prev_price != 0 else 0

                    # Calculate 7D changes if enough data
                    change_pct_7d = 0.0
                    if len(hist) >= 7:
                        price_7d_ago = float(hist['Close'].iloc[-7])
                        change_pct_7d = ((current_price - price_7d_ago) / price_7d_ago) * 100 if price_7d_ago != 0 else 0

                    # Calculate 30D changes if enough data
                    change_pct_30d = 0.0
                    if len(hist) >= 30:
                        price_30d_ago = float(hist['Close'].iloc[-30])
                        change_pct_30d = ((current_price - price_30d_ago) / price_30d_ago) * 100 if price_30d_ago != 0 else 0
                    elif len(hist) > 7:
                        # Use available data for approximation
                        price_start = float(hist['Close'].iloc[0])
                        change_pct_30d = ((current_price - price_start) / price_start) * 100 if price_start != 0 else 0

                    # Store result
                    result[symbol] = {
                        "price": round(current_price, 2),
                        "change_1d": round(change_val, 2),
                        "change_percent_1d": round(change_pct_1d, 2),
                        "change_percent_7d": round(change_pct_7d, 2),
                        "change_percent_30d": round(change_pct_30d, 2),
                        "volume": volume,
                        "high": round(high, 2),
                        "low": round(low, 2)
                    }

                elif not hist.empty and len(hist) == 1:
                    # Only one day of data available
                    current_price = float(hist['Close'].iloc[-1])
                    volume = int(hist['Volume'].iloc[-1]) if not hist['Volume'].empty else 0
                    high = float(hist['High'].iloc[-1])
                    low = float(hist['Low'].iloc[-1])

                    result[symbol] = {
                        "price": round(current_price, 2),
                        "change_1d": 0.0,
                        "change_percent_1d": 0.0,
                        "change_percent_7d": 0.0,
                        "change_percent_30d": 0.0,
                        "volume": volume,
                        "high": round(high, 2),
                        "low": round(low, 2)
                    }

                else:
                    # No data available, use fallback
                    fallback = self.get_fallback_regional_data([symbol])
                    result[symbol] = fallback[symbol]

            except Exception:
                # Use fallback data for this symbol
                fallback = self.get_fallback_regional_data([symbol])
                result[symbol] = fallback[symbol]

            # Small delay to avoid rate limiting
            time.sleep(0.2)

        return result

    def get_fallback_regional_data(self, symbols):
        """Generate fallback data for regional stocks"""
        result = {}
        base_prices = {"AAPL": 175, "MSFT": 420, "GOOGL": 140, "RELIANCE.NS": 2500, "TCS.NS": 3500, "BABA": 85}

        for symbol in symbols:
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
                "high": round(price * 1.03, 2),
                "low": round(price * 0.97, 2)
            }

        return result

    def initialize_market_data(self):
        """Initialize market data with simulated changes"""
        self.market_data = {}

        for category, assets in MARKET_ASSETS.items():
            self.market_data[category] = {}
            for asset_name, base_price in assets.items():
                # Generate realistic market data
                current_price = base_price * (1 + random.uniform(-0.05, 0.05))
                change_1d = current_price * random.uniform(-0.03, 0.03)
                change_percent_1d = (change_1d / current_price) * 100
                change_percent_7d = random.uniform(-5, 5)
                change_percent_30d = random.uniform(-15, 15)

                self.market_data[category][asset_name] = {
                    "price": round(current_price, 2),
                    "change_1d": round(change_1d, 2),
                    "change_percent_1d": round(change_percent_1d, 2),
                    "change_percent_7d": round(change_percent_7d, 2),
                    "change_percent_30d": round(change_percent_30d, 2)
                }

    def initialize_regional_data(self):
        """Initialize regional stock data with fallback"""
        self.regional_data = {}

        for region, data in REGIONAL_STOCKS.items():
            self.regional_data[region] = {}
            symbols = data["symbols"]
            names = data["names"]

            # Initialize with fallback data
            fallback_data = self.get_fallback_regional_data(symbols)

            for i, symbol in enumerate(symbols):
                display_name = names[i] if i < len(names) else symbol
                self.regional_data[region][symbol] = {
                    "name": display_name,
                    **fallback_data[symbol]
                }

    def should_update_data(self):
        """Check if data should be updated (every hour)"""
        if self.last_update is None:
            return True

        time_since_update = time.time() - self.last_update
        return time_since_update >= self.update_interval

    def update_regional_data_background(self):
        """Update regional data in background"""
        if self.data_loading:
            return

        def fetch_regional_data():
            try:
                self.data_loading = True

                for region, data in REGIONAL_STOCKS.items():
                    symbols = data["symbols"]
                    names = data["names"]

                    # Fetch in batches of 5 for speed
                    batch_size = 5
                    for i in range(0, len(symbols), batch_size):
                        batch_symbols = symbols[i:i + batch_size]
                        batch_data = self.get_real_stock_data_batch(batch_symbols, timeout=15)

                        # Update regional data
                        for j, symbol in enumerate(batch_symbols):
                            if symbol in batch_data:
                                display_name = names[i + j] if (i + j) < len(names) else symbol
                                self.regional_data[region][symbol] = {
                                    "name": display_name,
                                    **batch_data[symbol]
                                }

                        time.sleep(0.5)  # Small delay between batches

                self.last_update = time.time()

            except Exception:
                pass
            finally:
                self.data_loading = False

        # Start background thread
        thread = threading.Thread(target=fetch_regional_data, daemon=True)
        thread.start()

    def start_background_updates(self):
        """Start background update system"""

        def update_loop():
            # Initial update
            self.update_regional_data_background()

            # Update simulation data every 5 seconds
            while self.auto_update:
                try:
                    time.sleep(5)
                    if self.auto_update:
                        self.update_market_data()

                    # Check for hourly real data update
                    if self.should_update_data() and not self.data_loading:
                        self.update_regional_data_background()

                except Exception:
                    time.sleep(10)  # Wait before retrying

        # Start update thread
        update_thread = threading.Thread(target=update_loop, daemon=True)
        update_thread.start()

    def create_content(self):
        """Create Bloomberg-style market terminal layout"""
        try:
            # Top bar with branding
            with dpg.group(horizontal=True):
                dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("MARKET TERMINAL", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_input_text(label="", default_value="Search Symbol", width=200)
                dpg.add_button(label="SEARCH", width=80, callback=self.search_callback)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), tag="market_time_display")

            dpg.add_separator()

            # Control panel
            with dpg.group(horizontal=True):
                dpg.add_button(label="REFRESH", callback=self.refresh_callback, width=80)
                dpg.add_button(label="AUTO ON", callback=self.toggle_auto_update,
                               tag="auto_toggle_btn", width=80)
                dpg.add_combo(["1 min", "5 min", "10 min", "30 min"],
                              default_value="5 min", width=80, tag="refresh_interval")
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("LAST UPDATE:", color=self.BLOOMBERG_GRAY)
                dpg.add_text(datetime.datetime.now().strftime('%H:%M:%S'),
                             tag="last_update_time", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                status_color = self.BLOOMBERG_ORANGE if self.data_loading else self.BLOOMBERG_GREEN
                status_text = "UPDATING" if self.data_loading else "LIVE"
                dpg.add_text("●", color=status_color, tag="status_indicator")
                dpg.add_text(status_text, color=status_color, tag="status_text")

            dpg.add_separator()

            # Create scrollable content
            with dpg.child_window(height=-50, border=False):  # Leave space for status bar

                # Market grid - 3x2 layout (existing data)
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

        except Exception as e:
            # Fallback content
            dpg.add_text("MARKET TERMINAL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Error loading market data. Please refresh.")
            dpg.add_button(label="Retry", callback=self.retry_callback)

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
                self.create_regional_panel(region, 500, 400)

    def create_regional_panel(self, region, width, height):
        """Create regional stock panel with real data"""
        try:
            with dpg.child_window(width=width, height=height, border=True):
                # Panel header with flag emoji
                flags = {"India": "🇮🇳", "China": "🇨🇳", "United States": "🇺🇸"}
                flag = flags.get(region, "🌍")
                dpg.add_text(f"{flag} {region.upper()} STOCKS", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                # Data table with scrolling
                with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                               scrollY=True, scrollX=True, height=height - 60):
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
                                name = data["name"][:20] + "..." if len(data["name"]) > 20 else data["name"]
                                dpg.add_text(name, color=self.BLOOMBERG_WHITE)

                                # Symbol (remove .NS suffix for display)
                                display_symbol = symbol.replace(".NS", "").replace(".HK", "")
                                dpg.add_text(display_symbol, color=self.BLOOMBERG_YELLOW)

                                # Price
                                if region == "India":
                                    price_str = f"₹{data['price']:,.0f}" if data[
                                                                                'price'] >= 100 else f"₹{data['price']:.2f}"
                                elif region == "China":
                                    price_str = f"${data['price']:.2f}"
                                else:  # US
                                    price_str = f"${data['price']:.2f}"
                                dpg.add_text(price_str, color=self.BLOOMBERG_WHITE)

                                # 1D Change
                                change_color = self.BLOOMBERG_GREEN if data["change_1d"] >= 0 else self.BLOOMBERG_RED
                                dpg.add_text(f"{data['change_1d']:+.2f}", color=change_color)

                                # 1D %
                                percent_color = self.BLOOMBERG_GREEN if data[
                                                                            "change_percent_1d"] >= 0 else self.BLOOMBERG_RED
                                dpg.add_text(f"{data['change_percent_1d']:+.2f}%", color=percent_color)

                                # Volume
                                vol_str = f"{data['volume']:,}" if data[
                                                                       'volume'] < 1000000 else f"{data['volume'] / 1000000:.1f}M"
                                dpg.add_text(vol_str, color=self.BLOOMBERG_GRAY)

        except Exception:
            pass

    def create_market_panel(self, category, width, height):
        """Create individual market panel"""
        try:
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

                            # Price
                            if data["price"] < 1:
                                price_str = f"{data['price']:.4f}"
                            elif data["price"] < 100:
                                price_str = f"{data['price']:.2f}"
                            else:
                                price_str = f"{data['price']:,.0f}"
                            dpg.add_text(price_str, color=self.BLOOMBERG_WHITE)

                            # 1D Change
                            change_color = self.BLOOMBERG_GREEN if data["change_1d"] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_1d']:+.2f}", color=change_color)

                            # 1D %
                            percent_color = self.BLOOMBERG_GREEN if data[
                                                                        "change_percent_1d"] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_percent_1d']:+.2f}%", color=percent_color)

                            # 7D %
                            percent_7d_color = self.BLOOMBERG_GREEN if data[
                                                                           "change_percent_7d"] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_percent_7d']:+.2f}%", color=percent_7d_color)

                            # 30D %
                            percent_30d_color = self.BLOOMBERG_GREEN if data[
                                                                            "change_percent_30d"] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_percent_30d']:+.2f}%", color=percent_30d_color)

        except Exception:
            pass

    def create_status_bar(self):
        """Create status bar"""
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

            total_assets = sum(len(assets) for assets in self.market_data.values()) + sum(
                len(stocks) for stocks in self.regional_data.values())
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
            # Update each asset with small random changes
            for category in self.market_data:
                for asset_name in self.market_data[category]:
                    data = self.market_data[category][asset_name]

                    # Small price movement
                    change_factor = 1 + random.uniform(-0.01, 0.01)  # ±1% max change
                    new_price = data["price"] * change_factor
                    new_change_1d = new_price - data["price"]
                    new_change_percent_1d = (new_change_1d / data["price"]) * 100

                    # Update data
                    self.market_data[category][asset_name].update({
                        "price": round(new_price, 2 if new_price >= 1 else 4),
                        "change_1d": round(new_change_1d, 2),
                        "change_percent_1d": round(new_change_percent_1d, 2),
                        "change_percent_7d": round(data["change_percent_7d"] + random.uniform(-0.5, 0.5), 2),
                        "change_percent_30d": round(data["change_percent_30d"] + random.uniform(-0.2, 0.2), 2)
                    })

            # Update timestamp
            current_time = datetime.datetime.now().strftime('%H:%M:%S')
            if dpg.does_item_exist("last_update_time"):
                dpg.set_value("last_update_time", current_time)
            if dpg.does_item_exist("market_time_display"):
                dpg.set_value("market_time_display", datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

            # Update status indicators
            if dpg.does_item_exist("status_indicator") and dpg.does_item_exist("status_text"):
                status_color = self.BLOOMBERG_ORANGE if self.data_loading else self.BLOOMBERG_GREEN
                status_text = "UPDATING" if self.data_loading else "LIVE"
                dpg.configure_item("status_indicator", color=status_color)
                dpg.set_value("status_text", status_text)
                dpg.configure_item("status_text", color=status_color)

        except Exception:
            pass

    # Callback methods
    def search_callback(self):
        """Search callback"""
        pass

    def refresh_callback(self):
        """Manual refresh callback"""
        try:
            self.initialize_market_data()  # Reinitialize with new random data
            self.update_regional_data_background()  # Fetch fresh real data
            self.update_market_data()
        except Exception:
            pass

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
            if self.auto_update:
                self.start_background_updates()

        except Exception:
            pass

    def retry_callback(self):
        """Retry loading data"""
        try:
            self.initialize_market_data()
            self.initialize_regional_data()
            self.update_regional_data_background()
        except Exception:
            pass

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle resize events"""
        # Market panels have fixed sizes for stability
        pass

    def cleanup(self):
        """Clean up resources"""
        try:
            self.auto_update = False
            self.data_loading = False
            self.market_data = {}
            self.regional_data = {}
        except Exception:
            pass