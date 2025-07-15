# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import random
import datetime


class DashboardTab(BaseTab):
    """Bloomberg Terminal style Dashboard tab - Simplified and Compatible"""

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

        # Initialize data without threading
        self.initialize_data()

    def get_label(self):
        return "📊 Fincept Terminal"

    def initialize_data(self):
        """Initialize stock and market data"""
        # Stock tickers
        self.tickers = ["AAPL", "MSFT", "AMZN", "GOOGL", "META", "TSLA", "NVDA", "JPM", "V", "JNJ",
                        "BAC", "PG", "MA", "UNH", "HD", "INTC", "VZ", "DIS", "PYPL", "NFLX"]

        # Initialize stock data
        self.stock_data = {}
        for ticker in self.tickers:
            price = round(random.uniform(50, 500), 2)
            change_pct = round(random.uniform(-3, 3), 2)
            change_val = round(price * change_pct / 100, 2)
            volume = random.randint(500000, 10000000)

            self.stock_data[ticker] = {
                "price": price,
                "change_pct": change_pct,
                "change_val": change_val,
                "volume": volume,
                "high": round(price * 1.05, 2),
                "low": round(price * 0.95, 2),
                "open": round(price * 0.98, 2)
            }

        # Market indices
        self.indices = {
            "S&P 500": {"value": 5232.35, "change": 0.78},
            "DOW JONES": {"value": 38546.12, "change": 0.42},
            "NASDAQ": {"value": 16412.87, "change": 1.25},
            "FTSE 100": {"value": 7623.98, "change": -0.31},
            "DAX": {"value": 18245.67, "change": 0.29},
            "NIKKEI 225": {"value": 35897.45, "change": 0.85}
        }

        # Economic indicators
        self.economic_indicators = {
            "US 10Y Treasury": {"value": 4.35, "change": 0.05},
            "US GDP Growth": {"value": 2.8, "change": 0.1},
            "US Unemployment": {"value": 3.6, "change": -0.1},
            "EUR/USD": {"value": 1.084, "change": -0.002},
            "Gold": {"value": 2312.80, "change": 15.60},
            "WTI Crude": {"value": 78.35, "change": -1.25}
        }

        # News headlines
        self.news_headlines = [
            "Fed Signals Potential Rate Cuts Amid Economic Slowdown",
            "Tech Stocks Surge on Strong Earnings Reports",
            "Oil Prices Stabilize After OPEC+ Meeting",
            "Treasury Yields Hit New Heights as Inflation Concerns Rise",
            "Economic Data Points to Resilient Consumer Spending",
            "Global Markets React to Central Bank Policy Decisions"
        ]

    def create_content(self):
        """Create the Bloomberg Terminal layout - Simplified version"""
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

                        # Sample data
                        x_data = list(range(30))
                        y_data = [self.stock_data['AAPL']['price'] + random.uniform(-5, 5) for _ in range(30)]
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
            ticker_text = "BREAKING: Fed maintains rates • S&P 500 hits new high • Tech stocks rally • Oil prices stabilize"
            dpg.add_text(ticker_text)

        dpg.add_separator()

        # Status bar
        with dpg.group(horizontal=True):
            dpg.add_text("●", color=self.BLOOMBERG_GREEN)
            dpg.add_text("CONNECTED", color=self.BLOOMBERG_GREEN)
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
            dpg.add_text(f"LAST UPDATE: {datetime.datetime.now().strftime('%H:%M:%S')}", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LATENCY: 12ms", color=self.BLOOMBERG_GREEN)

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Resize components - simplified"""
        pass

    def cleanup(self):
        """Clean up resources"""
        print("Bloomberg Terminal tab cleanup completed")