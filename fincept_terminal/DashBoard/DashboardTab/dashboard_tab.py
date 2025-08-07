# -*- coding: utf-8 -*-


import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import datetime
import threading
import time
import requests
import json
import logging
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Dict, List, Optional, Any
from fincept_terminal.Utils.Logging.logger import logger, log_operation

# Configure logging to handle encoding issues
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Try to import yfinance, fallback if not available
try:
    import yfinance as yf

    YFINANCE_AVAILABLE = True
except ImportError:
    YFINANCE_AVAILABLE = False
    logger.warning("yfinance not available - using fallback data")

# Try to import feedparser, fallback if not available
try:
    import feedparser

    FEEDPARSER_AVAILABLE = True
except ImportError:
    FEEDPARSER_AVAILABLE = False
    logger.warning("feedparser not available - using fallback news")


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
        self._lock = threading.Lock()  # Thread safety

        # Initialize data
        self.initialize_data()

        # Start immediate background fetch (non-blocking)
        self.start_background_updates()

    def get_label(self):
        return "Fincept Terminal"

    def safe_float_conversion(self, value: Any, default: float = 0.0) -> float:
        """Safely convert value to float with encoding handling"""
        try:
            if value is None:
                return default
            # Handle potential encoding issues
            if isinstance(value, bytes):
                value = value.decode('utf-8', errors='ignore')
            elif isinstance(value, str):
                # Remove any non-numeric characters except decimal point and minus
                value = ''.join(c for c in value if c.isdigit() or c in '.-')
            return float(value) if value else default
        except (ValueError, TypeError, UnicodeDecodeError) as e:
            logger.warning(f"Error converting value to float: {value}, error: {e}")
            return default

    def safe_int_conversion(self, value: Any, default: int = 0) -> int:
        """Safely convert value to int with encoding handling"""
        try:
            if value is None:
                return default
            if isinstance(value, bytes):
                value = value.decode('utf-8', errors='ignore')
            elif isinstance(value, str):
                # Remove any non-numeric characters
                value = ''.join(c for c in value if c.isdigit())
            return int(float(value)) if value else default
        except (ValueError, TypeError, UnicodeDecodeError) as e:
            logger.warning(f"Error converting value to int: {value}, error: {e}")
            return default

    def get_stock_data_optimized(self, symbols: List[str], timeout: int = 10) -> Dict[str, Dict[str, Any]]:
        """Optimized stock data fetch using yfinance history method and concurrent processing"""
        if not YFINANCE_AVAILABLE:
            return self.get_fallback_stock_data(symbols)

        result = {}

        def fetch_single_stock(symbol: str) -> tuple[str, Dict[str, Any]]:
            """Fetch data for a single stock symbol"""
            try:
                # Use Ticker.history() instead of yf.download() for better performance
                ticker = yf.Ticker(symbol)

                # Get 5 days of data to ensure we have enough for comparison
                hist = ticker.history(period="5d", interval="1d", timeout=timeout)

                if hist.empty or len(hist) < 2:
                    logger.warning(f"Insufficient data for {symbol}, using fallback")
                    return symbol, self.get_fallback_stock_data([symbol])[symbol]

                # Get the most recent data
                current_data = hist.iloc[-1]
                prev_data = hist.iloc[-2]

                current_price = self.safe_float_conversion(current_data['Close'])
                prev_price = self.safe_float_conversion(prev_data['Close'])
                volume = self.safe_int_conversion(current_data['Volume'])
                high = self.safe_float_conversion(current_data['High'])
                low = self.safe_float_conversion(current_data['Low'])
                open_price = self.safe_float_conversion(current_data['Open'])

                change_val = current_price - prev_price
                change_pct = (change_val / prev_price) * 100 if prev_price != 0 else 0

                stock_data = {
                    "price": round(current_price, 2),
                    "change_pct": round(change_pct, 2),
                    "change_val": round(change_val, 2),
                    "volume": volume,
                    "high": round(high, 2),
                    "low": round(low, 2),
                    "open": round(open_price, 2)
                }

                return symbol, stock_data

            except Exception as e:
                logger.error(f"Error fetching data for {symbol}: {e}")
                return symbol, self.get_fallback_stock_data([symbol])[symbol]

        # Use ThreadPoolExecutor for concurrent fetching
        try:
            with ThreadPoolExecutor(max_workers=min(10, len(symbols))) as executor:
                # Submit all tasks
                future_to_symbol = {executor.submit(fetch_single_stock, symbol): symbol
                                    for symbol in symbols}

                # Collect results as they complete
                for future in as_completed(future_to_symbol, timeout=timeout + 5):
                    try:
                        symbol, data = future.result(timeout=5)
                        result[symbol] = data
                    except Exception as e:
                        symbol = future_to_symbol[future]
                        logger.error(f"Failed to get data for {symbol}: {e}")
                        result[symbol] = self.get_fallback_stock_data([symbol])[symbol]

        except Exception as e:
            logger.error(f"Error in concurrent stock data fetch: {e}")
            return self.get_fallback_stock_data(symbols)

        # Ensure all symbols have data
        for symbol in symbols:
            if symbol not in result:
                result[symbol] = self.get_fallback_stock_data([symbol])[symbol]

        return result

    def get_fallback_stock_data(self, symbols: List[str]) -> Dict[str, Dict[str, Any]]:
        """Generate realistic fallback stock data with proper encoding handling"""
        import random
        result = {}
        base_prices = {
            "AAPL": 175, "MSFT": 420, "AMZN": 155, "GOOGL": 140, "META": 485,
            "TSLA": 250, "NVDA": 875, "JPM": 155, "V": 265, "JNJ": 165,
            "BAC": 35, "PG": 160, "MA": 465, "UNH": 525, "HD": 385,
            "INTC": 45, "VZ": 40, "DIS": 110, "PYPL": 65, "NFLX": 485
        }

        for symbol in symbols:
            try:
                # Ensure symbol is properly encoded string
                if isinstance(symbol, bytes):
                    symbol = symbol.decode('utf-8', errors='ignore')

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
            except Exception as e:
                logger.error(f"Error generating fallback data for {symbol}: {e}")
                result[symbol] = {
                    "price": 100.0, "change_pct": 0.0, "change_val": 0.0,
                    "volume": 1000000, "high": 103.0, "low": 97.0, "open": 99.5
                }

        return result

    def get_indices_data_optimized(self, timeout: int = 10) -> Dict[str, Dict[str, float]]:
        """Optimized index data fetch with better error handling"""
        if not YFINANCE_AVAILABLE:
            return self.get_fallback_indices_data()

        symbols = ["^GSPC", "^DJI", "^IXIC", "^FTSE", "^GDAXI", "^N225"]
        names = ["S&P 500", "DOW JONES", "NASDAQ", "FTSE 100", "DAX", "NIKKEI 225"]
        result = {}

        def fetch_single_index(symbol: str, name: str) -> tuple[str, Dict[str, float]]:
            """Fetch data for a single index"""
            try:
                ticker = yf.Ticker(symbol)
                hist = ticker.history(period="5d", interval="1d", timeout=timeout)

                if hist.empty or len(hist) < 2:
                    fallback = self.get_fallback_indices_data()
                    return name, fallback[name]

                current_value = self.safe_float_conversion(hist['Close'].iloc[-1])
                prev_value = self.safe_float_conversion(hist['Close'].iloc[-2])
                change_pct = ((current_value - prev_value) / prev_value) * 100 if prev_value != 0 else 0

                return name, {
                    "value": round(current_value, 2),
                    "change": round(change_pct, 2)
                }

            except Exception as e:
                logger.error(f"Error fetching index {name} ({symbol}): {e}")
                fallback = self.get_fallback_indices_data()
                return name, fallback[name]

        try:
            with ThreadPoolExecutor(max_workers=6) as executor:
                future_to_name = {executor.submit(fetch_single_index, symbol, name): name
                                  for symbol, name in zip(symbols, names)}

                for future in as_completed(future_to_name, timeout=timeout + 5):
                    try:
                        name, data = future.result(timeout=5)
                        result[name] = data
                    except Exception as e:
                        name = future_to_name[future]
                        logger.error(f"Failed to get index data for {name}: {e}")
                        fallback = self.get_fallback_indices_data()
                        result[name] = fallback[name]

        except Exception as e:
            logger.error(f"Error in concurrent index data fetch: {e}")
            return self.get_fallback_indices_data()

        return result

    def get_fallback_indices_data(self) -> Dict[str, Dict[str, float]]:
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

    def get_news_optimized(self, timeout: int = 15) -> List[str]:
        """Optimized news fetch with better encoding handling"""
        if not FEEDPARSER_AVAILABLE:
            return self.get_fallback_news()

        try:
            # Use more reliable news sources
            feeds = [
                "https://feeds.finance.yahoo.com/rss/2.0/headline",
                "https://www.cnbc.com/id/100003114/device/rss/rss.html",
                "https://feeds.marketwatch.com/marketwatch/topstories/"
            ]

            news_headlines = []

            def fetch_feed(feed_url: str) -> List[str]:
                """Fetch headlines from a single feed"""
                try:
                    feed = feedparser.parse(feed_url)
                    headlines = []

                    if feed.entries:
                        for entry in feed.entries[:3]:  # Get max 3 from each feed
                            try:
                                title = entry.title.strip()
                                # Handle encoding issues
                                if isinstance(title, bytes):
                                    title = title.decode('utf-8', errors='ignore')

                                # Clean up title
                                title = title.encode('ascii', errors='ignore').decode('ascii')

                                if len(title) > 80:
                                    title = title[:77] + "..."

                                if title:  # Only add non-empty titles
                                    headlines.append(title)
                            except Exception as e:
                                logger.warning(f"Error processing news entry: {e}")
                                continue

                    return headlines
                except Exception as e:
                    logger.error(f"Error parsing feed {feed_url}: {e}")
                    return []

            # Try feeds in order, stop when we get enough news
            for feed_url in feeds:
                try:
                    headlines = fetch_feed(feed_url)
                    news_headlines.extend(headlines)
                    if len(news_headlines) >= 6:
                        break
                except Exception as e:
                    logger.error(f"Error fetching from {feed_url}: {e}")
                    continue

            if not news_headlines:
                return self.get_fallback_news()

            return news_headlines[:6]

        except Exception as e:
            logger.error(f"Error fetching news: {e}")
            return self.get_fallback_news()

    def get_fallback_news(self) -> List[str]:
        """Fallback news headlines"""
        return [
            "Market Update: Fed maintains current interest rates amid economic stability",
            "Tech stocks show strong performance during Q4 earnings season",
            "Oil prices stabilize following recent OPEC+ production decisions",
            "Treasury yields remain elevated on persistent inflation concerns",
            "Consumer spending data indicates continued economic resilience",
            "Global markets react positively to central bank policy updates"
        ]

    def initialize_data(self):
        """Initialize with fallback data for immediate display"""
        # Stock tickers - optimized list
        self.tickers = ["AAPL", "MSFT", "AMZN", "GOOGL", "META", "TSLA", "NVDA", "JPM", "V", "JNJ",
                        "BAC", "PG", "MA", "UNH", "HD", "INTC", "VZ", "DIS", "PYPL", "NFLX"]

        # Initialize with fallback data for immediate display
        with self._lock:
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

    def should_update_data(self) -> bool:
        """Check if data should be updated (every hour)"""
        if self.last_update is None:
            return True

        time_since_update = time.time() - self.last_update
        return time_since_update >= self.update_interval

    def update_data_background(self):
        """Update data in background thread with optimizations"""
        if self.data_loading:
            return  # Already updating

        def fetch_all_data():
            try:
                with self._lock:
                    self.data_loading = True

                logger.info("Starting optimized background data update...")

                # Fetch stock data using optimized method
                stock_data = self.get_stock_data_optimized(self.tickers, timeout=15)

                # Fetch indices data using optimized method
                indices_data = self.get_indices_data_optimized(timeout=15)

                # Fetch news using optimized method
                news_data = self.get_news_optimized(timeout=15)

                # Update data atomically
                with self._lock:
                    self.stock_data.update(stock_data)
                    self.indices = indices_data
                    self.news_headlines = news_data
                    self.last_update = time.time()

                logger.info("Optimized background data update completed successfully!")

            except Exception as e:
                logger.error(f"Error in background data update: {e}")
            finally:
                with self._lock:
                    self.data_loading = False

        # Start background thread
        thread = threading.Thread(target=fetch_all_data, daemon=True)
        thread.start()

    def start_background_updates(self):
        """Start the background update system with better scheduling"""

        def update_loop():
            # Initial update after a short delay
            time.sleep(2)
            self.update_data_background()

            # Periodic updates every hour
            while True:
                time.sleep(300)  # Check every 5 minutes
                if self.should_update_data() and not self.data_loading:
                    logger.info("Starting scheduled hourly data update...")
                    self.update_data_background()

        # Start the update loop in a daemon thread
        update_thread = threading.Thread(target=update_loop, daemon=True)
        update_thread.start()

    def safe_text_display(self, text: Any) -> str:
        """Safely display text with encoding handling"""
        try:
            if isinstance(text, bytes):
                return text.decode('utf-8', errors='ignore')
            elif isinstance(text, (int, float)):
                return str(text)
            elif text is None:
                return ""
            else:
                return str(text).encode('ascii', errors='ignore').decode('ascii')
        except Exception as e:
            logger.warning(f"Error displaying text: {e}")
            return "N/A"

    def create_content(self):
        """Create the Bloomberg Terminal layout with error handling"""
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
            logger.error(f"Error in create_content: {e}")
            # Fallback content
            dpg.add_text("Bloomberg Terminal", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Terminal is loading... Please wait.")

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

                with self._lock:
                    for index, data in self.indices.items():
                        with dpg.table_row():
                            dpg.add_text(self.safe_text_display(index))
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
                        dpg.add_text(self.safe_text_display(indicator))
                        dpg.add_text(f"{data['value']:.2f}")
                        change_color = self.BLOOMBERG_GREEN if data['change'] > 0 else self.BLOOMBERG_RED
                        dpg.add_text(f"{data['change']:+.2f}", color=change_color)

            dpg.add_separator()

            # Latest news
            dpg.add_text("LATEST NEWS", color=self.BLOOMBERG_YELLOW)
            with self._lock:
                for headline in self.news_headlines[:4]:
                    time_str = datetime.datetime.now().strftime('%H:%M')
                    safe_headline = self.safe_text_display(headline)
                    if len(safe_headline) > 50:
                        safe_headline = safe_headline[:47] + "..."
                    dpg.add_text(f"{time_str} - {safe_headline}", wrap=340)

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

                        with self._lock:
                            for ticker in self.tickers:
                                data = self.stock_data.get(ticker, {})
                                with dpg.table_row():
                                    dpg.add_text(self.safe_text_display(ticker))
                                    dpg.add_text(f"{data.get('price', 0):.2f}")

                                    change_color = self.BLOOMBERG_GREEN if data.get('change_pct',
                                                                                    0) > 0 else self.BLOOMBERG_RED
                                    dpg.add_text(f"{data.get('change_val', 0):+.2f}", color=change_color)
                                    dpg.add_text(f"{data.get('change_pct', 0):+.2f}%", color=change_color)

                                    dpg.add_text(f"{data.get('volume', 0):,}")
                                    dpg.add_text(f"{data.get('high', 0):.2f}")
                                    dpg.add_text(f"{data.get('low', 0):.2f}")

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

                            with self._lock:
                                aapl_data = self.stock_data.get('AAPL', {})
                                dpg.add_text(f"Last Price: {aapl_data.get('price', 0):.2f}")
                                change_color = self.BLOOMBERG_GREEN if aapl_data.get('change_pct',
                                                                                     0) > 0 else self.BLOOMBERG_RED
                                dpg.add_text(
                                    f"Change: {aapl_data.get('change_val', 0):+.2f} ({aapl_data.get('change_pct', 0):+.2f}%)",
                                    color=change_color)
                                dpg.add_text(f"Volume: {aapl_data.get('volume', 0):,}")

                        with dpg.group():
                            with self._lock:
                                aapl_data = self.stock_data.get('AAPL', {})
                                dpg.add_text(f"High: {aapl_data.get('high', 0):.2f}")
                                dpg.add_text(f"Low: {aapl_data.get('low', 0):.2f}")
                                dpg.add_text(f"Open: {aapl_data.get('open', 0):.2f}")
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
                        with self._lock:
                            base_price = self.stock_data.get('AAPL', {}).get('price', 175)
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

                    with self._lock:
                        for i, headline in enumerate(self.news_headlines):
                            timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
                            safe_headline = self.safe_text_display(headline)
                            dpg.add_text(safe_headline, color=self.BLOOMBERG_ORANGE)
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
            with self._lock:
                ticker_text = " • ".join(self.news_headlines[:3]) if self.news_headlines else "Loading live news..."
                safe_ticker_text = self.safe_text_display(ticker_text)
            dpg.add_text(safe_ticker_text)

        dpg.add_separator()

        # Status bar
        with dpg.group(horizontal=True):
            with self._lock:
                status_color = self.BLOOMBERG_ORANGE if self.data_loading else self.BLOOMBERG_GREEN
                status_text = "UPDATING" if self.data_loading else "CONNECTED"

            dpg.add_text("", color=status_color)
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

            with self._lock:
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
        logger.info("Bloomberg Terminal tab cleanup completed")

    def force_refresh(self):
        """Force refresh all data - useful for manual updates"""
        if not self.data_loading:
            logger.info("Force refreshing data...")
            self.update_data_background()
        else:
            logger.info("Data update already in progress...")

    def get_market_status(self) -> Dict[str, Any]:
        """Get current market status information"""
        current_time = datetime.datetime.now()
        current_hour = current_time.hour

        # Simple market hours check (can be enhanced with timezone and holiday logic)
        is_market_open = 9 <= current_hour < 16

        with self._lock:
            status = {
                "is_open": is_market_open,
                "last_update": self.last_update,
                "data_loading": self.data_loading,
                "stocks_count": len(self.stock_data),
                "indices_count": len(self.indices),
                "news_count": len(self.news_headlines)
            }

        return status

    def get_stock_by_symbol(self, symbol: str) -> Optional[Dict[str, Any]]:
        """Get stock data for a specific symbol"""
        with self._lock:
            return self.stock_data.get(symbol.upper())

    def add_custom_ticker(self, symbol: str):
        """Add a custom ticker to the watch list"""
        symbol = symbol.upper()
        if symbol not in self.tickers:
            self.tickers.append(symbol)
            # Fetch data for the new symbol
            if not self.data_loading:
                try:
                    new_data = self.get_stock_data_optimized([symbol], timeout=10)
                    with self._lock:
                        self.stock_data.update(new_data)
                    logger.info(f"Added ticker {symbol} to watch list")
                except Exception as e:
                    logger.error(f"Error adding ticker {symbol}: {e}")

    def remove_ticker(self, symbol: str):
        """Remove a ticker from the watch list"""
        symbol = symbol.upper()
        if symbol in self.tickers:
            self.tickers.remove(symbol)
            with self._lock:
                if symbol in self.stock_data:
                    del self.stock_data[symbol]
            logger.info(f"Removed ticker {symbol} from watch list")

    def export_data_to_json(self) -> str:
        """Export current data to JSON format"""
        try:
            with self._lock:
                export_data = {
                    "timestamp": datetime.datetime.now().isoformat(),
                    "stock_data": self.stock_data,
                    "indices": self.indices,
                    "news_headlines": self.news_headlines,
                    "economic_indicators": self.economic_indicators
                }

            return json.dumps(export_data, indent=2)
        except Exception as e:
            logger.error(f"Error exporting data: {e}")
            return "{}"

    def get_performance_stats(self) -> Dict[str, Any]:
        """Get performance statistics"""
        with self._lock:
            gainers = [ticker for ticker, data in self.stock_data.items()
                       if data.get('change_pct', 0) > 0]
            losers = [ticker for ticker, data in self.stock_data.items()
                      if data.get('change_pct', 0) < 0]

            total_volume = sum(data.get('volume', 0) for data in self.stock_data.values())

            stats = {
                "total_stocks": len(self.stock_data),
                "gainers": len(gainers),
                "losers": len(losers),
                "unchanged": len(self.stock_data) - len(gainers) - len(losers),
                "total_volume": total_volume,
                "top_gainer": max(self.stock_data.items(),
                                  key=lambda x: x[1].get('change_pct', 0))[0] if self.stock_data else None,
                "top_loser": min(self.stock_data.items(),
                                 key=lambda x: x[1].get('change_pct', 0))[0] if self.stock_data else None
            }

        return stats