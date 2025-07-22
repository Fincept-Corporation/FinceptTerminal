# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import datetime
import threading
import time
import requests
import json

# Try to import yfinance, fallback if not available
try:
    import yfinance as yf

    YFINANCE_AVAILABLE = True
except ImportError:
    YFINANCE_AVAILABLE = False
    print("yfinance not available - using fallback data")

# Try to import feedparser, fallback if not available
try:
    import feedparser

    FEEDPARSER_AVAILABLE = True
except ImportError:
    FEEDPARSER_AVAILABLE = False
    print("feedparser not available - using fallback news")


class DashboardTab(BaseTab):
    """Bloomberg Terminal style Dashboard tab - With Real Data (Fast Loading)"""

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

        # Initialize data
        self.initialize_data()

        # Start immediate background fetch (non-blocking)
        self.start_background_updates()

    def get_label(self):
        return "Fincept Terminal"

    def get_stock_data_fast(self, symbols, timeout=10):
        """Fast stock data fetch with timeout"""
        if not YFINANCE_AVAILABLE:
            return self.get_fallback_stock_data(symbols)

        try:
            # Use yfinance download for multiple symbols at once (faster)
            tickers_str = " ".join(symbols)
            data = yf.download(tickers_str, period="2d", interval="1d",
                               progress=False, show_errors=False, timeout=timeout)

            result = {}

            if data.empty:
                return self.get_fallback_stock_data(symbols)

            # Handle single vs multiple tickers
            if len(symbols) == 1:
                symbol = symbols[0]
                if len(data) >= 2:
                    current_price = float(data['Close'].iloc[-1])
                    prev_price = float(data['Close'].iloc[-2])
                    volume = int(data['Volume'].iloc[-1]) if not data['Volume'].empty else 0
                    high = float(data['High'].iloc[-1])
                    low = float(data['Low'].iloc[-1])
                    open_price = float(data['Open'].iloc[-1])

                    change_val = current_price - prev_price
                    change_pct = (change_val / prev_price) * 100 if prev_price != 0 else 0

                    result[symbol] = {
                        "price": round(current_price, 2),
                        "change_pct": round(change_pct, 2),
                        "change_val": round(change_val, 2),
                        "volume": volume,
                        "high": round(high, 2),
                        "low": round(low, 2),
                        "open": round(open_price, 2)
                    }
            else:
                # Multiple symbols
                for symbol in symbols:
                    try:
                        if symbol in data['Close'].columns and len(data) >= 2:
                            current_price = float(data['Close'][symbol].iloc[-1])
                            prev_price = float(data['Close'][symbol].iloc[-2])
                            volume = int(data['Volume'][symbol].iloc[-1]) if symbol in data['Volume'].columns else 0
                            high = float(data['High'][symbol].iloc[-1])
                            low = float(data['Low'][symbol].iloc[-1])
                            open_price = float(data['Open'][symbol].iloc[-1])

                            change_val = current_price - prev_price
                            change_pct = (change_val / prev_price) * 100 if prev_price != 0 else 0

                            result[symbol] = {
                                "price": round(current_price, 2),
                                "change_pct": round(change_pct, 2),
                                "change_val": round(change_val, 2),
                                "volume": volume,
                                "high": round(high, 2),
                                "low": round(low, 2),
                                "open": round(open_price, 2)
                            }
                        else:
                            result[symbol] = self.get_fallback_stock_data([symbol])[symbol]
                    except Exception as e:
                        print(f"Error processing {symbol}: {e}")
                        result[symbol] = self.get_fallback_stock_data([symbol])[symbol]

            return result

        except Exception as e:
            print(f"Error fetching stock data: {e}")
            return self.get_fallback_stock_data(symbols)

    def get_fallback_stock_data(self, symbols):
        """Generate realistic fallback stock data"""
        import random
        result = {}
        base_prices = {
            "AAPL": 175, "MSFT": 420, "AMZN": 155, "GOOGL": 140, "META": 485,
            "TSLA": 250, "NVDA": 875, "JPM": 155, "V": 265, "JNJ": 165,
            "BAC": 35, "PG": 160, "MA": 465, "UNH": 525, "HD": 385,
            "INTC": 45, "VZ": 40, "DIS": 110, "PYPL": 65, "NFLX": 485
        }

        for symbol in symbols:
            base_price = base_prices.get(symbol, 100)
            change_pct = round(random.uniform(-2.5, 2.5), 2)
            price = round(base_price * (1 + change_pct / 100), 2)
            change_val = round(price * change_pct / 100, 2)

            result[symbol] = {
                "price": price,
                "change_pct": change_pct,
                "change_val": change_val,
                "volume": random.randint(1000000, 50000000),
                "high": round(price * 1.03, 2),
                "low": round(price * 0.97, 2),
                "open": round(price * 0.995, 2)
            }

        return result

    def get_indices_data_fast(self, timeout=10):
        """Fast index data fetch"""
        if not YFINANCE_AVAILABLE:
            return self.get_fallback_indices_data()

        try:
            symbols = ["^GSPC", "^DJI", "^IXIC", "^FTSE", "^GDAXI", "^N225"]
            names = ["S&P 500", "DOW JONES", "NASDAQ", "FTSE 100", "DAX", "NIKKEI 225"]

            data = yf.download(" ".join(symbols), period="2d", interval="1d",
                               progress=False, show_errors=False, timeout=timeout)

            result = {}

            if data.empty:
                return self.get_fallback_indices_data()

            for i, symbol in enumerate(symbols):
                name = names[i]
                try:
                    if len(data) >= 2 and symbol in data['Close'].columns:
                        current_value = float(data['Close'][symbol].iloc[-1])
                        prev_value = float(data['Close'][symbol].iloc[-2])
                        change_pct = ((current_value - prev_value) / prev_value) * 100 if prev_value != 0 else 0

                        result[name] = {
                            "value": round(current_value, 2),
                            "change": round(change_pct, 2)
                        }
                    else:
                        # Fallback for this specific index
                        fallback = self.get_fallback_indices_data()
                        result[name] = fallback[name]
                except Exception as e:
                    print(f"Error processing index {name}: {e}")
                    fallback = self.get_fallback_indices_data()
                    result[name] = fallback[name]

            return result

        except Exception as e:
            print(f"Error fetching indices data: {e}")
            return self.get_fallback_indices_data()

    def get_fallback_indices_data(self):
        """Generate realistic fallback indices data"""
        import random
        return {
            "S&P 500": {"value": round(5200 + random.uniform(-50, 50), 2), "change": round(random.uniform(-1, 1), 2)},
            "DOW JONES": {"value": round(38500 + random.uniform(-200, 200), 2),
                          "change": round(random.uniform(-1, 1), 2)},
            "NASDAQ": {"value": round(16400 + random.uniform(-100, 100), 2),
                       "change": round(random.uniform(-1.5, 1.5), 2)},
            "FTSE 100": {"value": round(7600 + random.uniform(-50, 50), 2), "change": round(random.uniform(-1, 1), 2)},
            "DAX": {"value": round(18200 + random.uniform(-100, 100), 2), "change": round(random.uniform(-1, 1), 2)},
            "NIKKEI 225": {"value": round(35800 + random.uniform(-200, 200), 2),
                           "change": round(random.uniform(-1, 1), 2)}
        }

    def get_news_fast(self, timeout=15):
        """Fast news fetch with timeout"""
        if not FEEDPARSER_AVAILABLE:
            return self.get_fallback_news()

        try:
            # Use fewer, more reliable news sources
            feeds = [
                "https://feeds.finance.yahoo.com/rss/2.0/headline",
                "https://www.cnbc.com/id/100003114/device/rss/rss.html"
            ]

            news_headlines = []

            for feed_url in feeds:
                try:
                    # Set timeout for feed parsing
                    feed = feedparser.parse(feed_url)
                    if feed.entries:
                        for entry in feed.entries[:3]:  # Get max 3 from each feed
                            title = entry.title.strip()
                            if len(title) > 80:
                                title = title[:77] + "..."
                            news_headlines.append(title)
                        break  # If we got news from first feed, don't wait for second
                except Exception as e:
                    print(f"Error parsing feed {feed_url}: {e}")
                    continue

            if not news_headlines:
                return self.get_fallback_news()

            return news_headlines[:6]

        except Exception as e:
            print(f"Error fetching news: {e}")
            return self.get_fallback_news()

    def get_fallback_news(self):
        """Fallback news headlines"""
        return [
            "Market Update: Fed maintains current interest rates",
            "Tech stocks show strong performance amid earnings season",
            "Oil prices stabilize following OPEC+ production decisions",
            "Treasury yields remain elevated on inflation concerns",
            "Consumer spending data shows economic resilience",
            "Global markets react to central bank policy updates"
        ]

    def initialize_data(self):
        """Initialize with fallback data for immediate display"""
        # Stock tickers
        self.tickers = ["AAPL", "MSFT", "AMZN", "GOOGL", "META", "TSLA", "NVDA", "JPM", "V", "JNJ",
                        "BAC", "PG", "MA", "UNH", "HD", "INTC", "VZ", "DIS", "PYPL", "NFLX"]

        # Initialize with fallback data for immediate display
        self.stock_data = self.get_fallback_stock_data(self.tickers)
        self.indices = self.get_fallback_indices_data()
        self.news_headlines = self.get_fallback_news()

        # Economic indicators (keeping static for now as they need specialized APIs)
        self.economic_indicators = {
            "US 10Y Treasury": {"value": 4.35, "change": 0.05},
            "US GDP Growth": {"value": 2.8, "change": 0.1},
            "US Unemployment": {"value": 3.6, "change": -0.1},
            "EUR/USD": {"value": 1.084, "change": -0.002},
            "Gold": {"value": 2312.80, "change": 15.60},
            "WTI Crude": {"value": 78.35, "change": -1.25}
        }

    def should_update_data(self):
        """Check if data should be updated (every hour)"""
        if self.last_update is None:
            return True

        time_since_update = time.time() - self.last_update
        return time_since_update >= self.update_interval

    def update_data_background(self):
        """Update data in background thread"""
        if self.data_loading:
            return  # Already updating

        def fetch_all_data():
            try:
                self.data_loading = True
                print("Starting background data update...")

                # Fetch stock data in batches for speed
                batch_size = 10
                for i in range(0, len(self.tickers), batch_size):
                    batch = self.tickers[i:i + batch_size]
                    batch_data = self.get_stock_data_fast(batch, timeout=15)
                    self.stock_data.update(batch_data)
                    time.sleep(0.5)  # Small delay between batches

                # Fetch indices data
                self.indices = self.get_indices_data_fast(timeout=15)

                # Fetch news
                self.news_headlines = self.get_news_fast(timeout=15)

                self.last_update = time.time()
                print("Background data update completed!")

            except Exception as e:
                print(f"Error in background data update: {e}")
            finally:
                self.data_loading = False

        # Start background thread
        thread = threading.Thread(target=fetch_all_data, daemon=True)
        thread.start()

    def start_background_updates(self):
        """Start the background update system"""

        def update_loop():
            # Initial update
            self.update_data_background()

            # Periodic updates every hour
            while True:
                time.sleep(300)  # Check every 5 minutes
                if self.should_update_data() and not self.data_loading:
                    print("Starting hourly data update...")
                    self.update_data_background()

        # Start the update loop in a daemon thread
        update_thread = threading.Thread(target=update_loop, daemon=True)
        update_thread.start()

    def create_content(self):
        """Create the Bloomberg Terminal layout"""
        try:
            # Top bar with branding and search
            with dpg.group(horizontal=True):
                dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("PROFESSIONAL", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_input_text(label="", default_value="Enter Command", width=300)
                dpg.add_button(label="Search", width=80)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

            dpg.add_separator()

            # Function keys
            with dpg.group(horizontal=True):
                function_keys = ["F1:HELP", "F2:MARKETS", "F3:NEWS", "F4:PORT", "F5:MOVERS", "F6:ECON"]
                for key in function_keys:
                    dpg.add_button(label=key, width=80, height=25)

            dpg.add_separator()

            # Main terminal area
            with dpg.group(horizontal=True):
                # Left panel - Market Monitor
                self.create_left_panel()

                # Center panel - Market Data
                self.create_center_panel()

                # Right panel - Command Line
                self.create_right_panel()

            # Bottom section
            dpg.add_separator()
            self.create_bottom_section()

        except Exception as e:
            print(f"Error in create_content: {e}")
            # Fallback content
            dpg.add_text("Bloomberg Terminal", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Terminal is loading...")

    def create_left_panel(self):
        """Create left panel with market data"""
        with dpg.child_window(width=350, height=600, border=True):
            dpg.add_text("MARKET MONITOR", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Global indices
            dpg.add_text("GLOBAL INDICES", color=self.BLOOMBERG_YELLOW)
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
                dpg.add_table_column(label="Index")
                dpg.add_table_column(label="Value")
                dpg.add_table_column(label="Change %")

                for index, data in self.indices.items():
                    with dpg.table_row():
                        dpg.add_text(index)
                        dpg.add_text(f"{data['value']:.2f}")
                        change_color = self.BLOOMBERG_GREEN if data['change'] > 0 else self.BLOOMBERG_RED
                        dpg.add_text(f"{data['change']:+.2f}%", color=change_color)

            dpg.add_separator()

            # Economic indicators
            dpg.add_text("ECONOMIC INDICATORS", color=self.BLOOMBERG_YELLOW)
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
                dpg.add_table_column(label="Indicator")
                dpg.add_table_column(label="Value")
                dpg.add_table_column(label="Change")

                for indicator, data in self.economic_indicators.items():
                    with dpg.table_row():
                        dpg.add_text(indicator)
                        dpg.add_text(f"{data['value']:.2f}")
                        change_color = self.BLOOMBERG_GREEN if data['change'] > 0 else self.BLOOMBERG_RED
                        dpg.add_text(f"{data['change']:+.2f}", color=change_color)

            dpg.add_separator()

            # Latest news
            dpg.add_text("LATEST NEWS", color=self.BLOOMBERG_YELLOW)
            for headline in self.news_headlines[:4]:
                time_str = datetime.datetime.now().strftime('%H:%M')
                dpg.add_text(f"{time_str} - {headline[:50]}...", wrap=340)

    def create_center_panel(self):
        """Create center panel with stock data"""
        with dpg.child_window(width=800, height=600, border=True):
            with dpg.tab_bar():
                # Market Data Tab
                with dpg.tab(label="Market Data"):
                    dpg.add_text("TOP STOCKS", color=self.BLOOMBERG_ORANGE)

                    # Stock table
                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                                   scrollY=True, height=300):
                        dpg.add_table_column(label="Ticker")
                        dpg.add_table_column(label="Last")
                        dpg.add_table_column(label="Chg")
                        dpg.add_table_column(label="Chg%")
                        dpg.add_table_column(label="Volume")
                        dpg.add_table_column(label="High")
                        dpg.add_table_column(label="Low")

                        for ticker in self.tickers:
                            data = self.stock_data[ticker]
                            with dpg.table_row():
                                dpg.add_text(ticker)
                                dpg.add_text(f"{data['price']:.2f}")

                                change_color = self.BLOOMBERG_GREEN if data['change_pct'] > 0 else self.BLOOMBERG_RED
                                dpg.add_text(f"{data['change_val']:+.2f}", color=change_color)
                                dpg.add_text(f"{data['change_pct']:+.2f}%", color=change_color)

                                dpg.add_text(f"{data['volume']:,}")
                                dpg.add_text(f"{data['high']:.2f}")
                                dpg.add_text(f"{data['low']:.2f}")

                    # Stock details
                    dpg.add_separator()
                    dpg.add_text("STOCK DETAILS", color=self.BLOOMBERG_ORANGE)

                    with dpg.group(horizontal=True):
                        dpg.add_input_text(label="Ticker", default_value="AAPL", width=150)
                        dpg.add_button(label="Load")

                    # AAPL details
                    with dpg.group(horizontal=True):
                        with dpg.group():
                            dpg.add_text("Apple Inc (AAPL US Equity)", color=self.BLOOMBERG_ORANGE)
                            dpg.add_text("Technology - Consumer Electronics")
                            dpg.add_text(f"Last Price: {self.stock_data['AAPL']['price']:.2f}")
                            change_color = self.BLOOMBERG_GREEN if self.stock_data['AAPL'][
                                                                       'change_pct'] > 0 else self.BLOOMBERG_RED
                            dpg.add_text(
                                f"Change: {self.stock_data['AAPL']['change_val']:+.2f} ({self.stock_data['AAPL']['change_pct']:+.2f}%)",
                                color=change_color)
                            dpg.add_text(f"Volume: {self.stock_data['AAPL']['volume']:,}")

                        with dpg.group():
                            dpg.add_text(f"High: {self.stock_data['AAPL']['high']:.2f}")
                            dpg.add_text(f"Low: {self.stock_data['AAPL']['low']:.2f}")
                            dpg.add_text(f"Open: {self.stock_data['AAPL']['open']:.2f}")
                            dpg.add_text("P/E Ratio: 28.5")
                            dpg.add_text("Market Cap: $2.8T")

                # Charts Tab
                with dpg.tab(label="Charts"):
                    dpg.add_text("ADVANCED CHARTS", color=self.BLOOMBERG_ORANGE)

                    with dpg.group(horizontal=True):
                        dpg.add_combo(["AAPL", "MSFT", "GOOGL", "AMZN"], default_value="AAPL", width=150)
                        dpg.add_combo(["1D", "5D", "1M", "3M"], default_value="1M", width=100)
                        dpg.add_button(label="Update Chart")

                    # Simple chart placeholder
                    with dpg.plot(height=300, width=-1):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                        y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Price")

                        # Sample data based on real AAPL price
                        x_data = list(range(30))
                        base_price = self.stock_data['AAPL']['price']
                        y_data = [base_price + (i * 0.5) for i in range(30)]  # Simple trending data
                        dpg.add_line_series(x_data, y_data, label="AAPL", parent=y_axis)

                    # Technical indicators
                    dpg.add_text("TECHNICAL INDICATORS", color=self.BLOOMBERG_ORANGE)
                    with dpg.group(horizontal=True):
                        with dpg.group():
                            dpg.add_text("Moving Averages", color=self.BLOOMBERG_YELLOW)
                            dpg.add_text("MA 20: 175.50 - Buy", color=self.BLOOMBERG_GREEN)
                            dpg.add_text("MA 50: 172.30 - Buy", color=self.BLOOMBERG_GREEN)
                            dpg.add_text("MA 200: 165.80 - Neutral", color=self.BLOOMBERG_WHITE)

                        with dpg.group():
                            dpg.add_text("Oscillators", color=self.BLOOMBERG_YELLOW)
                            dpg.add_text("RSI(14): 65.42 - Neutral", color=self.BLOOMBERG_WHITE)
                            dpg.add_text("MACD: 2.15 - Buy", color=self.BLOOMBERG_GREEN)
                            dpg.add_text("Stochastic: 75.30 - Sell", color=self.BLOOMBERG_RED)

                # News Tab
                with dpg.tab(label="News"):
                    dpg.add_text("FINANCIAL NEWS", color=self.BLOOMBERG_ORANGE)

                    with dpg.group(horizontal=True):
                        dpg.add_input_text(label="Search", width=300)
                        dpg.add_button(label="Go")

                    dpg.add_separator()

                    for i, headline in enumerate(self.news_headlines):
                        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
                        dpg.add_text(headline, color=self.BLOOMBERG_ORANGE)
                        dpg.add_text(timestamp, color=self.BLOOMBERG_GRAY)
                        dpg.add_text("Market analysis and financial news content goes here...")
                        dpg.add_separator()

    def create_right_panel(self):
        """Create right panel with command line"""
        with dpg.child_window(width=350, height=600, border=True):
            dpg.add_text("COMMAND LINE", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Command history
            with dpg.child_window(height=200, border=True):
                dpg.add_text("> AAPL US Equity <GO>", color=self.BLOOMBERG_WHITE)
                dpg.add_text("  Loading AAPL US Equity...", color=self.BLOOMBERG_GRAY)
                dpg.add_text("> TOP <GO>", color=self.BLOOMBERG_WHITE)
                dpg.add_text("  Loading TOP news...", color=self.BLOOMBERG_GRAY)
                dpg.add_text("> WEI <GO>", color=self.BLOOMBERG_WHITE)
                dpg.add_text("  Loading World Equity Indices...", color=self.BLOOMBERG_GRAY)

            # Command input
            dpg.add_input_text(label=">", width=-1)
            dpg.add_text("<HELP> for commands. Press <GO> to execute.", color=self.BLOOMBERG_GRAY)

            dpg.add_separator()

            # Quick commands
            dpg.add_text("COMMON COMMANDS", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("HELP - Show available commands")
            dpg.add_text("DES - Company description")
            dpg.add_text("GP - Price graph")
            dpg.add_text("TOP - Top news headlines")
            dpg.add_text("WEI - World equity indices")
            dpg.add_text("PORT - Portfolio overview")

    def create_bottom_section(self):
        """Create bottom news ticker and status bar"""
        # News ticker
        dpg.add_text("LIVE NEWS TICKER", color=self.BLOOMBERG_ORANGE)
        with dpg.child_window(height=50, border=True):
            ticker_text = " • ".join(self.news_headlines[:3]) if self.news_headlines else "Loading live news..."
            dpg.add_text(ticker_text)

        dpg.add_separator()

        # Status bar
        with dpg.group(horizontal=True):
            status_color = self.BLOOMBERG_ORANGE if self.data_loading else self.BLOOMBERG_GREEN
            status_text = "UPDATING" if self.data_loading else "CONNECTED"
            dpg.add_text("●", color=status_color)
            dpg.add_text(status_text, color=status_color)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LIVE DATA", color=self.BLOOMBERG_ORANGE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)

            current_hour = datetime.datetime.now().hour
            if 9 <= current_hour < 16:
                dpg.add_text("MARKET OPEN", color=self.BLOOMBERG_GREEN)
            else:
                dpg.add_text("MARKET CLOSED", color=self.BLOOMBERG_RED)

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("SERVER: NY-01", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("USER: TRADER001", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)

            if self.last_update:
                last_update_str = datetime.datetime.fromtimestamp(self.last_update).strftime('%H:%M:%S')
                dpg.add_text(f"LAST UPDATE: {last_update_str}", color=self.BLOOMBERG_WHITE)
            else:
                dpg.add_text("LAST UPDATE: --:--:--", color=self.BLOOMBERG_WHITE)

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LATENCY: 12ms", color=self.BLOOMBERG_GREEN)

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Resize components - simplified"""
        pass

    def cleanup(self):
        """Clean up resources"""
        print("Bloomberg Terminal tab cleanup completed")