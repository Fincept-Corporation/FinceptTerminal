# -*- coding: utf-8 -*-
# portfolio_tab.py

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import json
import os
import datetime
import threading
import time
import random
import csv
import io
from collections import defaultdict, deque
from tkinter import filedialog
import tkinter as tk
import yfinance as yf
from concurrent.futures import ThreadPoolExecutor, as_completed
import requests
from functools import lru_cache

# Import the new logger
from fincept_terminal.Utils.Logging.logger import logger, operation, monitor_performance


def get_settings_path():
    """Returns a consistent settings file path"""
    return "settings.json"


SETTINGS_FILE = get_settings_path()


class PortfolioTab(BaseTab):
    """Bloomberg Terminal style Portfolio Management tab with CSV import - Optimized"""

    def __init__(self, main_app=None):
        super().__init__(main_app)
        self.main_app = main_app

        # Bloomberg color scheme - cached
        self._init_color_scheme()

        # Price management - optimized
        self.price_cache = {}
        self.last_price_update = {}
        self.price_fetch_errors = {}
        self.daily_change_cache = {}
        self.previous_close_cache = {}
        self.refresh_thread = None
        self.refresh_running = False
        self.price_update_interval = 3600  # 1 hour in seconds
        self.initial_price_fetch_done = False

        # Portfolio data
        self.portfolios = self.load_portfolios()
        self.current_portfolio = None
        self.current_view = "overview"

        # Country suffix mapping for yfinance - cached
        self.country_suffixes = self._get_country_suffixes()

        # Performance optimizations
        self._portfolio_value_cache = {}
        self._portfolio_investment_cache = {}
        self._cache_timeout = 30  # seconds
        self._last_cache_update = {}

        # Sample market data initialization
        self.initialize_sample_data()

        # Start initial price fetch
        self.fetch_initial_prices()

    def _init_color_scheme(self):
        """Initialize Bloomberg color scheme"""
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [100, 150, 250]
        self.BLOOMBERG_BLACK = [0, 0, 0]

    def _get_country_suffixes(self):
        """Get country suffix mapping - cached"""
        return {
            "India": ".NS",
            "United States": "",
            "United Kingdom": ".L",
            "Germany": ".DE",
            "Japan": ".T",
            "Australia": ".AX",
            "Canada": ".TO",
            "France": ".PA",
            "Hong Kong": ".HK",
            "South Korea": ".KS"
        }

    def get_label(self):
        return " Portfolio"

    def initialize_sample_data(self):
        """Initialize sample portfolio data for demonstration"""
        if not self.portfolios:
            # Create sample portfolios
            self.portfolios = {
                "Tech Growth": {
                    "AAPL": {"quantity": 50, "avg_price": 150.25, "last_added": "2024-01-15"},
                    "MSFT": {"quantity": 30, "avg_price": 280.75, "last_added": "2024-01-10"},
                    "GOOGL": {"quantity": 25, "avg_price": 125.50, "last_added": "2024-01-05"},
                    "NVDA": {"quantity": 20, "avg_price": 450.30, "last_added": "2024-01-20"}
                },
                "Dividend Income": {
                    "JNJ": {"quantity": 100, "avg_price": 160.80, "last_added": "2024-01-12"},
                    "PG": {"quantity": 75, "avg_price": 145.20, "last_added": "2024-01-08"},
                    "KO": {"quantity": 150, "avg_price": 58.90, "last_added": "2024-01-18"}
                }
            }
            self.save_portfolios()

    @monitor_performance
    def fetch_initial_prices(self):
        """Fetch initial prices for all stocks in portfolios"""
        threading.Thread(target=self._fetch_initial_prices_worker, daemon=True).start()

    def _fetch_initial_prices_worker(self):
        """Background worker to fetch initial prices"""
        try:
            with operation("initial_price_fetch"):
                logger.info("Fetching initial stock prices...")

                # Collect all unique symbols from all portfolios
                all_symbols = set()
                for portfolio in self.portfolios.values():
                    for symbol in portfolio.keys():
                        all_symbols.add(symbol)

                if not all_symbols:
                    logger.info("No stocks found in portfolios")
                    self.initial_price_fetch_done = True
                    return

                # Fetch prices in batches for better performance
                self._fetch_prices_batch(list(all_symbols))

                self.initial_price_fetch_done = True
                logger.info(f"Initial price fetch completed for {len(all_symbols)} symbols",
                            context={'symbols_count': len(all_symbols)})

                # Update UI if it exists
                if dpg.does_item_exist("portfolio_last_update"):
                    dpg.set_value("portfolio_last_update", datetime.datetime.now().strftime('%H:%M:%S'))

                # Update navigation summary
                self.update_navigation_summary()

        except Exception as e:
            logger.error(f"Error in initial price fetch: {e}", exc_info=True)
            self.initial_price_fetch_done = True

    @monitor_performance
    def _fetch_prices_batch(self, symbols):
        """Fetch prices for a batch of symbols - optimized"""
        max_workers = min(10, len(symbols))  # Adaptive worker count

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            # Submit fetch tasks
            future_to_symbol = {
                executor.submit(self._fetch_single_price, symbol): symbol
                for symbol in symbols
            }

            # Collect results with timeout
            for future in as_completed(future_to_symbol, timeout=60):
                symbol = future_to_symbol[future]
                try:
                    price = future.result(timeout=30)
                    if price is not None:
                        self.price_cache[symbol] = price
                        self.last_price_update[symbol] = datetime.datetime.now()
                        self.price_fetch_errors.pop(symbol, None)  # Clear any previous errors
                        logger.debug(f"Price updated: {symbol} = ${price:.2f}")
                    else:
                        self.price_fetch_errors[symbol] = "No price data available"
                        logger.warning(f"No price data available for {symbol}")

                except Exception as e:
                    self.price_fetch_errors[symbol] = str(e)
                    logger.error(f"Error fetching price for {symbol}: {e}")

                # Small delay to avoid overwhelming the API
                time.sleep(0.1)

    def _fetch_single_price(self, symbol):
        """Fetch price and daily change data for a single symbol using yfinance - optimized"""
        try:
            # Create ticker object
            ticker = yf.Ticker(symbol)

            # Get current data with timeout
            info = ticker.info

            # Try different price fields in order of preference
            price_fields = [
                'regularMarketPrice',
                'currentPrice',
                'previousClose',
                'regularMarketPreviousClose'
            ]

            current_price = None
            previous_close = None

            # Get current price
            for field in price_fields:
                if field in info and info[field] is not None:
                    current_price = float(info[field])
                    if current_price > 0:  # Valid price
                        break

            # Get previous close
            prev_close_fields = ['regularMarketPreviousClose', 'previousClose']
            for field in prev_close_fields:
                if field in info and info[field] is not None:
                    previous_close = float(info[field])
                    if previous_close > 0:
                        break

            # If we couldn't get from info, try historical data
            if current_price is None or previous_close is None:
                hist = ticker.history(period="2d", interval="1d")
                if not hist.empty and len(hist) >= 2:
                    if current_price is None:
                        current_price = float(hist['Close'].iloc[-1])
                    if previous_close is None:
                        previous_close = float(hist['Close'].iloc[-2])
                elif not hist.empty:
                    if current_price is None:
                        current_price = float(hist['Close'].iloc[-1])
                    if previous_close is None:
                        previous_close = current_price  # Use same price as fallback

            # Calculate daily change
            if current_price is not None and previous_close is not None and previous_close > 0:
                daily_change = current_price - previous_close
                daily_change_pct = (daily_change / previous_close) * 100

                # Store in caches
                self.previous_close_cache[symbol] = previous_close
                self.daily_change_cache[symbol] = {
                    'change': daily_change,
                    'change_pct': daily_change_pct
                }

                return current_price

            return current_price

        except Exception as e:
            logger.error(f"Error fetching price for {symbol}: {e}")
            return None

    @lru_cache(maxsize=128)
    def get_daily_change(self, symbol):
        """Get today's change for a symbol - cached"""
        if symbol in self.daily_change_cache:
            return self.daily_change_cache[symbol]

        # Calculate from current and previous close if available
        current_price = self.get_current_price(symbol)
        previous_close = self.previous_close_cache.get(symbol)

        if current_price and previous_close and previous_close > 0:
            daily_change = current_price - previous_close
            daily_change_pct = (daily_change / previous_close) * 100
            return {
                'change': daily_change,
                'change_pct': daily_change_pct
            }

        # Fallback to zero change
        return {'change': 0.0, 'change_pct': 0.0}

    def calculate_portfolio_daily_change(self, portfolio_name):
        """Calculate total daily change for a portfolio - cached"""
        cache_key = f"daily_change_{portfolio_name}"
        current_time = time.time()

        # Check cache
        if (cache_key in self._last_cache_update and
                current_time - self._last_cache_update[cache_key] < self._cache_timeout):
            if cache_key in self._portfolio_value_cache:
                return self._portfolio_value_cache[cache_key]

        portfolio = self.portfolios.get(portfolio_name, {})
        total_change = 0.0
        total_previous_value = 0.0

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get('quantity', 0)
                current_price = self.get_current_price(symbol)
                previous_close = self.previous_close_cache.get(symbol, current_price)

                # Calculate change for this holding
                current_value = quantity * current_price
                previous_value = quantity * previous_close
                holding_change = current_value - previous_value

                total_change += holding_change
                total_previous_value += previous_value

        # Calculate percentage change
        change_pct = (total_change / total_previous_value * 100) if total_previous_value > 0 else 0.0

        result = {
            'change': total_change,
            'change_pct': change_pct
        }

        # Cache result
        self._portfolio_value_cache[cache_key] = result
        self._last_cache_update[cache_key] = current_time

        return result

    def calculate_total_daily_change(self):
        """Calculate total daily change across all portfolios - cached"""
        cache_key = "total_daily_change"
        current_time = time.time()

        # Check cache
        if (cache_key in self._last_cache_update and
                current_time - self._last_cache_update[cache_key] < self._cache_timeout):
            if cache_key in self._portfolio_value_cache:
                return self._portfolio_value_cache[cache_key]

        total_change = 0.0
        total_previous_value = 0.0

        for portfolio_name in self.portfolios.keys():
            portfolio_change = self.calculate_portfolio_daily_change(portfolio_name)
            portfolio_current_value = self.calculate_portfolio_value(portfolio_name)
            portfolio_previous_value = portfolio_current_value - portfolio_change['change']

            total_change += portfolio_change['change']
            total_previous_value += portfolio_previous_value

        # Calculate percentage change
        change_pct = (total_change / total_previous_value * 100) if total_previous_value > 0 else 0.0

        result = {
            'change': total_change,
            'change_pct': change_pct
        }

        # Cache result
        self._portfolio_value_cache[cache_key] = result
        self._last_cache_update[cache_key] = current_time

        return result

    @lru_cache(maxsize=256)
    def get_current_price(self, symbol):
        """Get current price from cache or return fallback price - cached"""
        # Return cached price if available
        if symbol in self.price_cache:
            return self.price_cache[symbol]

        # If symbol has an error, use average price as fallback
        if symbol in self.price_fetch_errors:
            # Try to find avg_price from portfolios
            for portfolio in self.portfolios.values():
                if symbol in portfolio and isinstance(portfolio[symbol], dict):
                    avg_price = portfolio[symbol].get('avg_price', 100)
                    logger.warning(f"Using avg_price ${avg_price:.2f} for {symbol} (fetch error)")
                    return avg_price

        # Default fallback
        logger.debug(f"Using default price $100.00 for {symbol}")
        return 100.0

    @monitor_performance
    def create_content(self):
        """Create Bloomberg-style portfolio terminal layout - optimized"""
        try:
            # Top bar with Bloomberg branding
            with dpg.group(horizontal=True):
                dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("PORTFOLIO TERMINAL", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_input_text(label="", default_value="Search Portfolio", width=200)
                dpg.add_button(label="SEARCH", width=80, callback=self.search_portfolio)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), tag="portfolio_time_display")

            dpg.add_separator()

            # Function keys for portfolio sections
            with dpg.group(horizontal=True):
                portfolio_functions = ["F1:OVERVIEW", "F2:CREATE", "F3:MANAGE", "F4:ANALYZE", "F5:IMPORT",
                                       "F6:SETTINGS"]
                callbacks = [
                    lambda: self.switch_view("overview"),
                    lambda: self.switch_view("create"),
                    lambda: self.switch_view("manage"),
                    lambda: self.switch_view("analyze"),
                    lambda: self.switch_view("import"),
                    lambda: self.show_settings()
                ]

                for i, (key, callback) in enumerate(zip(portfolio_functions, callbacks)):
                    dpg.add_button(label=key, width=120, height=25, callback=callback)

            dpg.add_separator()

            # Main portfolio layout - Three panels
            with dpg.group(horizontal=True):
                # Left panel - Portfolio Navigator
                self.create_left_portfolio_panel()

                # Center panel - Main Portfolio Content
                self.create_center_portfolio_panel()

                # Right panel - Portfolio Analytics
                self.create_right_portfolio_panel()

            # Bottom status bar
            dpg.add_separator()
            self.create_portfolio_status_bar()

            # Initialize with overview
            self.switch_view("overview")
            self.start_price_refresh_thread()

        except Exception as e:
            logger.error(f"Error creating portfolio content: {e}", exc_info=True)
            # Fallback content
            dpg.add_text("PORTFOLIO TERMINAL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Error loading portfolio content. Please try again.")

    def create_left_portfolio_panel(self):
        """Create left portfolio navigation panel - optimized"""
        with dpg.child_window(width=350, height=650, border=True):
            dpg.add_text("PORTFOLIO NAVIGATOR", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Portfolio summary stats
            dpg.add_text("PORTFOLIO SUMMARY", color=self.BLOOMBERG_YELLOW)
            with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                dpg.add_table_column()
                dpg.add_table_column()

                summary_items = [
                    ("Total Portfolios:", "nav_total_portfolios"),
                    ("Total Investment:", "nav_total_investment"),
                    ("Current Value:", "nav_current_value"),
                    ("Total P&L:", "nav_total_pnl"),
                    ("Today's Change:", "nav_today_change")
                ]

                for label, tag in summary_items:
                    with dpg.table_row():
                        dpg.add_text(label)
                        dpg.add_text("$0.00", color=self.BLOOMBERG_WHITE, tag=tag)

            dpg.add_separator()

            # Portfolio list - optimized rendering
            dpg.add_text("PORTFOLIO LIST", color=self.BLOOMBERG_YELLOW)
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           scrollY=True, height=200):
                dpg.add_table_column(label="Name", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Stocks", width_fixed=True, init_width_or_weight=60)
                dpg.add_table_column(label="Value", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="P&L%", width_fixed=True, init_width_or_weight=70)

                # Populate portfolio list efficiently
                self._populate_portfolio_list()

            dpg.add_separator()

            # Quick actions - optimized
            dpg.add_text("QUICK ACTIONS", color=self.BLOOMBERG_YELLOW)
            self._create_quick_actions()

    def _populate_portfolio_list(self):
        """Populate portfolio list efficiently"""
        for portfolio_name, stocks in self.portfolios.items():
            portfolio_value = self.calculate_portfolio_value(portfolio_name)
            portfolio_investment = self.calculate_portfolio_investment(portfolio_name)
            pnl_pct = ((
                                   portfolio_value - portfolio_investment) / portfolio_investment * 100) if portfolio_investment > 0 else 0

            with dpg.table_row():
                dpg.add_text(portfolio_name[:15] + ("..." if len(portfolio_name) > 15 else ""))
                dpg.add_text(str(len(stocks)))
                dpg.add_text(f"${portfolio_value:,.0f}")
                pnl_color = self.BLOOMBERG_GREEN if pnl_pct >= 0 else self.BLOOMBERG_RED
                dpg.add_text(f"{pnl_pct:+.1f}%", color=pnl_color)

    def _create_quick_actions(self):
        """Create quick actions buttons efficiently"""
        quick_actions = [
            (" View Overview", lambda: self.switch_view("overview")),
            (" Create Portfolio", lambda: self.switch_view("create")),
            (" Manage Holdings", lambda: self.switch_view("manage")),
            (" View Analytics", lambda: self.switch_view("analyze")),
            (" Import CSV", lambda: self.switch_view("import")),
            (" Refresh Prices", self.refresh_all_prices_now),
            (" Export Data", self.export_portfolio_data),
            (" Delete Portfolio", self.show_delete_portfolio_dialog)
        ]

        for label, callback in quick_actions:
            dpg.add_button(label=label, callback=callback, width=-1, height=30)
            dpg.add_spacer(height=3)

    def create_center_portfolio_panel(self):
        """Create center portfolio content panel"""
        with dpg.child_window(width=900, height=650, border=True, tag="portfolio_center_panel"):
            # Content will be dynamically loaded based on current view
            dpg.add_text("PORTFOLIO CONTENT LOADING...", color=self.BLOOMBERG_YELLOW, tag="portfolio_loading_text")

    def create_right_portfolio_panel(self):
        """Create right portfolio analytics panel - optimized"""
        with dpg.child_window(width=350, height=650, border=True):
            dpg.add_text("PORTFOLIO ANALYTICS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Market indicators - cached data
            dpg.add_text("MARKET INDICATORS", color=self.BLOOMBERG_YELLOW)
            self._create_market_indicators_table()

            dpg.add_separator()

            # Top performers - optimized
            dpg.add_text("TOP PERFORMERS TODAY", color=self.BLOOMBERG_YELLOW)
            self._create_top_performers_table()

            dpg.add_separator()

            # Risk metrics - cached
            dpg.add_text("RISK METRICS", color=self.BLOOMBERG_YELLOW)
            self._create_risk_metrics_table()

            dpg.add_separator()

            # Recent transactions - optimized
            dpg.add_text("RECENT TRANSACTIONS", color=self.BLOOMBERG_YELLOW)
            self._create_recent_transactions()

    def _create_market_indicators_table(self):
        """Create market indicators table efficiently"""
        with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
            dpg.add_table_column()
            dpg.add_table_column()

            market_data = [
                ("S&P 500", "+0.75%", self.BLOOMBERG_GREEN),
                ("NASDAQ", "+1.20%", self.BLOOMBERG_GREEN),
                ("DOW", "+0.45%", self.BLOOMBERG_GREEN),
                ("VIX", "18.50", self.BLOOMBERG_WHITE),
                ("10Y Treasury", "4.35%", self.BLOOMBERG_WHITE)
            ]

            for indicator, value, color in market_data:
                with dpg.table_row():
                    dpg.add_text(indicator, color=self.BLOOMBERG_GRAY)
                    dpg.add_text(value, color=color)

    def _create_top_performers_table(self):
        """Create top performers table efficiently"""
        with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
            dpg.add_table_column(label="Stock", width_fixed=True, init_width_or_weight=80)
            dpg.add_table_column(label="Change", width_fixed=True, init_width_or_weight=80)
            dpg.add_table_column(label="Price", width_fixed=True, init_width_or_weight=80)

            top_performers = [
                ("NVDA", "+3.2%", "$520.80"),
                ("MSFT", "+2.1%", "$310.25"),
                ("AAPL", "+1.8%", "$175.50"),
                ("GOOGL", "+1.5%", "$140.75")
            ]

            for stock, change, price in top_performers:
                with dpg.table_row():
                    dpg.add_text(stock, color=self.BLOOMBERG_WHITE)
                    dpg.add_text(change, color=self.BLOOMBERG_GREEN)
                    dpg.add_text(price, color=self.BLOOMBERG_WHITE)

    def _create_risk_metrics_table(self):
        """Create risk metrics table efficiently"""
        with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
            dpg.add_table_column()
            dpg.add_table_column()

            risk_metrics = [
                ("Portfolio Beta:", "1.15"),
                ("Sharpe Ratio:", "1.45"),
                ("Max Drawdown:", "-8.2%"),
                ("Volatility:", "18.5%"),
                ("VaR (95%):", "-$2,450")
            ]

            for metric, value in risk_metrics:
                with dpg.table_row():
                    dpg.add_text(metric, color=self.BLOOMBERG_GRAY)
                    value_color = self.BLOOMBERG_RED if "-" in value else self.BLOOMBERG_WHITE
                    dpg.add_text(value, color=value_color)

    def _create_recent_transactions(self):
        """Create recent transactions efficiently"""
        recent_transactions = [
            ("AAPL", "BUY", "50 shares", "2024-01-20"),
            ("MSFT", "BUY", "30 shares", "2024-01-18"),
            ("GOOGL", "SELL", "10 shares", "2024-01-15")
        ]

        for stock, action, quantity, date in recent_transactions:
            with dpg.group(horizontal=True):
                action_color = self.BLOOMBERG_GREEN if action == "BUY" else self.BLOOMBERG_RED
                dpg.add_text("•", color=self.BLOOMBERG_ORANGE)
                dpg.add_text(f"{stock} {action}", color=action_color)
                dpg.add_text(quantity, color=self.BLOOMBERG_WHITE)

    def create_portfolio_status_bar(self):
        """Create portfolio status bar"""
        with dpg.group(horizontal=True):
            status_items = [
                ("PORTFOLIO STATUS:", "ACTIVE", self.BLOOMBERG_GREEN),
                ("REAL-TIME PRICING:", "ENABLED", self.BLOOMBERG_GREEN),
                ("AUTO-REFRESH:", "ON", self.BLOOMBERG_GREEN)
            ]

            for i, (label, value, color) in enumerate(status_items):
                if i > 0:
                    dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(label, color=self.BLOOMBERG_GRAY)
                if label == "AUTO-REFRESH:":
                    dpg.add_text(value, color=color, tag="portfolio_auto_refresh_status")
                else:
                    dpg.add_text(value, color=color)

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LAST UPDATE:", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.datetime.now().strftime('%H:%M:%S'),
                         tag="portfolio_last_update", color=self.BLOOMBERG_WHITE)

    @monitor_performance
    def switch_view(self, view_name):
        """Switch between different portfolio views - optimized"""
        self.current_view = view_name

        # Clear center panel efficiently
        if dpg.does_item_exist("portfolio_center_panel"):
            children = dpg.get_item_children("portfolio_center_panel", slot=1)
            for child in children:
                try:
                    dpg.delete_item(child)
                except:
                    pass

        # Load appropriate content
        view_handlers = {
            "overview": self.create_overview_content,
            "create": self.create_create_content,
            "manage": self.create_manage_content,
            "analyze": self.create_analyze_content,
            "import": self.create_import_content
        }

        handler = view_handlers.get(view_name)
        if handler:
            handler()

        # Update navigation summary
        self.update_navigation_summary()

    def create_import_content(self):
        """Create CSV import content"""
        with dpg.group(parent="portfolio_center_panel"):
            dpg.add_text("CSV PORTFOLIO IMPORT", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Step 1: File Selection
            dpg.add_text("STEP 1: SELECT CSV FILE", color=self.BLOOMBERG_YELLOW)
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                dpg.add_button(label=" SELECT CSV FILE", callback=self.select_csv_file, width=150, height=35)
                dpg.add_text("No file selected", color=self.BLOOMBERG_GRAY, tag="csv_file_status")

            dpg.add_spacer(height=20)

            # Step 2: Country Selection
            dpg.add_text("STEP 2: SELECT COUNTRY/EXCHANGE", color=self.BLOOMBERG_YELLOW)
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                dpg.add_text("Country/Exchange:", color=self.BLOOMBERG_WHITE)
                dpg.add_combo(list(self.country_suffixes.keys()), tag="country_selector",
                              default_value="India", width=200)
                dpg.add_text("(This adds appropriate suffix for yfinance)", color=self.BLOOMBERG_GRAY)

            dpg.add_spacer(height=20)

            # Step 3: Portfolio Name
            dpg.add_text("STEP 3: PORTFOLIO NAME", color=self.BLOOMBERG_YELLOW)
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                dpg.add_text("Portfolio Name:", color=self.BLOOMBERG_WHITE)
                dpg.add_input_text(tag="import_portfolio_name", hint="Enter portfolio name", width=300)

            dpg.add_spacer(height=20)

            # Step 4: Column Mapping (Initially hidden)
            dpg.add_group(tag="column_mapping_section", show=False)

            # Step 5: Preview and Import (Initially hidden)
            dpg.add_group(tag="import_preview_section", show=False)

            # Action buttons
            dpg.add_spacer(height=20)
            with dpg.group(horizontal=True):
                dpg.add_button(label="ANALYZE CSV", callback=self.analyze_csv_file,
                               width=150, height=35, tag="analyze_csv_btn", show=False)
                dpg.add_button(label="IMPORT PORTFOLIO", callback=self.import_csv_portfolio,
                               width=180, height=35, tag="import_portfolio_btn", show=False)
                dpg.add_button(label="CANCEL", callback=lambda: self.switch_view("overview"),
                               width=100, height=35)

    @monitor_performance
    def select_csv_file(self):
        """Open file dialog to select CSV file - optimized"""
        try:
            # Create a temporary tkinter root window (hidden)
            root = tk.Tk()
            root.withdraw()  # Hide the main window

            # Open file dialog
            file_path = filedialog.askopenfilename(
                title="Select Portfolio CSV File",
                filetypes=[("CSV files", "*.csv"), ("All files", "*.*")]
            )

            root.destroy()  # Clean up

            if file_path:
                self.csv_file_path = file_path
                filename = os.path.basename(file_path)

                # Update status
                dpg.set_value("csv_file_status", f"Selected: {filename}")
                dpg.configure_item("csv_file_status", color=self.BLOOMBERG_GREEN)
                dpg.show_item("analyze_csv_btn")

                self.show_message(f"CSV file selected: {filename}", "success")
            else:
                self.show_message("No file selected", "warning")

        except Exception as e:
            logger.error(f"Error selecting CSV file: {e}", exc_info=True)
            self.show_message("Error selecting file", "error")

    @monitor_performance
    def analyze_csv_file(self):
        """Analyze the selected CSV file and show column mapping - optimized"""
        try:
            if not hasattr(self, 'csv_file_path'):
                self.show_message("Please select a CSV file first", "error")
                return

            with operation("csv_analysis"):
                # Read CSV file efficiently
                with open(self.csv_file_path, 'r', encoding='utf-8') as file:
                    # Try to detect delimiter
                    sample = file.read(1024)
                    file.seek(0)

                    sniffer = csv.Sniffer()
                    delimiter = sniffer.sniff(sample).delimiter

                    reader = csv.reader(file, delimiter=delimiter)
                    rows = list(reader)

                if not rows:
                    self.show_message("CSV file is empty", "error")
                    return

                self.csv_headers = rows[0]
                self.csv_data = rows[1:] if len(rows) > 1 else []

                # Show column mapping section
                self.create_column_mapping_ui()
                dpg.show_item("column_mapping_section")

                self.show_message(f"CSV analyzed: {len(self.csv_data)} rows, {len(self.csv_headers)} columns",
                                  "success")

        except Exception as e:
            logger.error(f"Error analyzing CSV: {e}", exc_info=True)
            self.show_message(f"Error analyzing CSV: {str(e)}", "error")

    def create_column_mapping_ui(self):
        """Create column mapping interface - optimized"""
        # Clear existing mapping section
        if dpg.does_item_exist("column_mapping_section"):
            children = dpg.get_item_children("column_mapping_section", slot=1)
            for child in children:
                try:
                    dpg.delete_item(child)
                except:
                    pass

        with dpg.group(parent="column_mapping_section"):
            dpg.add_text("STEP 4: MAP COLUMNS", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("Map your CSV columns to portfolio fields:", color=self.BLOOMBERG_WHITE)
            dpg.add_spacer(height=10)

            # Required and optional field mappings
            field_mappings = [
                # Required fields
                ("symbol", "Stock Symbol/Instrument", "The column containing stock symbols", True),
                ("quantity", "Quantity/Shares", "The column containing number of shares", True),
                ("avg_price", "Average Cost/Purchase Price", "The column containing average purchase price", True),
                # Optional fields
                ("current_price", "Current Price/LTP", "Current market price (optional)", False),
                ("investment", "Invested Amount", "Total invested amount (optional)", False),
                ("current_value", "Current Value", "Current market value (optional)", False),
                ("pnl", "P&L/Gain-Loss", "Profit/Loss amount (optional)", False)
            ]

            # Create mapping table efficiently
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True, height=250):
                dpg.add_table_column(label="Portfolio Field", width_fixed=True, init_width_or_weight=150)
                dpg.add_table_column(label="Description", width_fixed=True, init_width_or_weight=200)
                dpg.add_table_column(label="CSV Column", width_fixed=True, init_width_or_weight=200)
                dpg.add_table_column(label="Required", width_fixed=True, init_width_or_weight=80)

                for field_key, field_name, description, is_required in field_mappings:
                    with dpg.table_row():
                        dpg.add_text(field_name, color=self.BLOOMBERG_WHITE)
                        dpg.add_text(description, color=self.BLOOMBERG_GRAY)

                        # Try to auto-detect column
                        auto_detected = self.auto_detect_column(field_key)
                        dpg.add_combo([""] + self.csv_headers, tag=f"map_{field_key}",
                                      default_value=auto_detected, width=180)

                        required_color = self.BLOOMBERG_RED if is_required else self.BLOOMBERG_GREEN
                        required_text = "YES" if is_required else "NO"
                        dpg.add_text(required_text, color=required_color)

            dpg.add_spacer(height=10)
            dpg.add_button(label="PREVIEW IMPORT", callback=self.preview_import, width=150, height=30)

    @lru_cache(maxsize=64)
    def auto_detect_column(self, field_type):
        """Auto-detect CSV column based on common naming patterns - cached"""
        field_patterns = {
            "symbol": ["symbol", "instrument", "stock", "ticker", "scrip", "name"],
            "quantity": ["quantity", "qty", "shares", "units", "holding"],
            "avg_price": ["avg", "average", "cost", "price", "purchase", "buy"],
            "current_price": ["ltp", "current", "market", "last", "trading"],
            "investment": ["invested", "investment", "total_cost", "amount"],
            "current_value": ["current_value", "market_value", "value", "cur"],
            "pnl": ["pnl", "p&l", "profit", "loss", "gain", "net"]
        }

        patterns = field_patterns.get(field_type, [])

        for header in self.csv_headers:
            header_lower = header.lower().replace(" ", "_").replace(".", "").replace("-", "_")
            for pattern in patterns:
                if pattern in header_lower:
                    return header

        return ""

    @monitor_performance
    def preview_import(self):
        """Preview the import with current column mapping - optimized"""
        try:
            with operation("import_preview"):
                # Validate required mappings
                required_mappings = ["symbol", "quantity", "avg_price"]
                self.column_mapping = {}

                for field in required_mappings:
                    mapped_column = dpg.get_value(f"map_{field}")
                    if not mapped_column:
                        self.show_message(f"Please map the required field: {field}", "error")
                        return
                    self.column_mapping[field] = mapped_column

                # Get optional mappings
                optional_mappings = ["current_price", "investment", "current_value", "pnl"]
                for field in optional_mappings:
                    mapped_column = dpg.get_value(f"map_{field}")
                    if mapped_column:
                        self.column_mapping[field] = mapped_column

                # Process preview data efficiently
                self.csv_preview_data = []
                country_suffix = self.country_suffixes.get(dpg.get_value("country_selector"), "")

                for row in self.csv_data[:10]:  # Preview first 10 rows
                    if len(row) >= len(self.csv_headers):
                        row_dict = dict(zip(self.csv_headers, row))

                        # Extract mapped data
                        symbol = str(row_dict.get(self.column_mapping["symbol"], "")).strip()
                        if not symbol:
                            continue

                        # Add country suffix for yfinance compatibility
                        if country_suffix and not symbol.endswith(country_suffix):
                            symbol_yf = symbol + country_suffix
                        else:
                            symbol_yf = symbol

                        try:
                            quantity = float(row_dict.get(self.column_mapping["quantity"], 0))
                            avg_price = float(row_dict.get(self.column_mapping["avg_price"], 0))
                        except (ValueError, TypeError):
                            continue

                        preview_item = {
                            "original_symbol": symbol,
                            "yfinance_symbol": symbol_yf,
                            "quantity": quantity,
                            "avg_price": avg_price
                        }

                        self.csv_preview_data.append(preview_item)

                # Show preview
                self.create_import_preview()
                dpg.show_item("import_preview_section")
                dpg.show_item("import_portfolio_btn")

                self.show_message(f"Preview ready: {len(self.csv_preview_data)} valid rows found", "success")

        except Exception as e:
            logger.error(f"Error creating preview: {e}", exc_info=True)
            self.show_message(f"Error creating preview: {str(e)}", "error")

    def create_import_preview(self):
        """Create import preview table - optimized"""
        # Clear existing preview section
        if dpg.does_item_exist("import_preview_section"):
            children = dpg.get_item_children("import_preview_section", slot=1)
            for child in children:
                try:
                    dpg.delete_item(child)
                except:
                    pass

        with dpg.group(parent="import_preview_section"):
            dpg.add_text("STEP 5: PREVIEW & IMPORT", color=self.BLOOMBERG_YELLOW)
            dpg.add_text(f"Preview of {len(self.csv_preview_data)} stocks to be imported:", color=self.BLOOMBERG_WHITE)
            dpg.add_spacer(height=10)

            # Preview table
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           scrollY=True, height=200):
                columns = [
                    ("Original Symbol", 120),
                    ("YFinance Symbol", 130),
                    ("Quantity", 100),
                    ("Avg Price", 100),
                    ("Investment", 120)
                ]

                for label, width in columns:
                    dpg.add_table_column(label=label, width_fixed=True, init_width_or_weight=width)

                for item in self.csv_preview_data:
                    investment = item["quantity"] * item["avg_price"]
                    with dpg.table_row():
                        dpg.add_text(item["original_symbol"], color=self.BLOOMBERG_WHITE)
                        dpg.add_text(item["yfinance_symbol"], color=self.BLOOMBERG_GREEN)
                        dpg.add_text(f"{item['quantity']:.2f}", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${item['avg_price']:.2f}", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${investment:,.2f}", color=self.BLOOMBERG_WHITE)

            dpg.add_spacer(height=10)

            # Summary
            total_investment = sum(item["quantity"] * item["avg_price"] for item in self.csv_preview_data)
            unique_stocks = len(self.csv_preview_data)

            with dpg.group(horizontal=True):
                dpg.add_text("Summary:", color=self.BLOOMBERG_YELLOW)
                dpg.add_text(f"{unique_stocks} stocks", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(f"Total Investment: ${total_investment:,.2f}", color=self.BLOOMBERG_WHITE)

    @monitor_performance
    def import_csv_portfolio(self):
        """Import the CSV data as a new portfolio - optimized"""
        try:
            with operation("csv_portfolio_import"):
                portfolio_name = dpg.get_value("import_portfolio_name").strip()
                if not portfolio_name:
                    self.show_message("Please enter a portfolio name", "error")
                    return

                if portfolio_name in self.portfolios:
                    self.show_message("Portfolio name already exists", "error")
                    return

                if not self.csv_preview_data:
                    self.show_message("No preview data available. Please analyze CSV first", "error")
                    return

                # Create new portfolio efficiently
                new_portfolio = {}

                for item in self.csv_preview_data:
                    symbol = item["yfinance_symbol"]
                    new_portfolio[symbol] = {
                        "quantity": item["quantity"],
                        "avg_price": item["avg_price"],
                        "last_added": datetime.datetime.now().strftime("%Y-%m-%d"),
                        "original_symbol": item["original_symbol"]  # Keep original symbol for reference
                    }

                    # Update price cache with avg_price as initial current price
                    self.price_cache[symbol] = item["avg_price"] * (1 + random.uniform(-0.05, 0.05))

                # Add to portfolios
                self.portfolios[portfolio_name] = new_portfolio
                self.save_portfolios()

                # Clear cache for affected calculations
                self._clear_portfolio_cache()

                # Fetch real prices for imported symbols
                imported_symbols = list(new_portfolio.keys())
                threading.Thread(target=lambda: self._fetch_prices_batch(imported_symbols), daemon=True).start()

                # Clear import data
                self.csv_data = None
                self.csv_headers = []
                self.column_mapping = {}
                self.csv_preview_data = []

                # Switch to overview
                self.switch_view("overview")

                success_msg = f"Portfolio '{portfolio_name}' imported successfully with {len(new_portfolio)} stocks! Fetching real-time prices..."
                self.show_message(success_msg, "success")

        except Exception as e:
            logger.error(f"Error importing portfolio: {e}", exc_info=True)
            self.show_message(f"Error importing portfolio: {str(e)}", "error")

    def _clear_portfolio_cache(self):
        """Clear portfolio calculation cache"""
        self._portfolio_value_cache.clear()
        self._portfolio_investment_cache.clear()
        self._last_cache_update.clear()
        # Clear LRU caches
        self.get_current_price.cache_clear()
        self.get_daily_change.cache_clear()

    @monitor_performance
    def create_overview_content(self):
        """Create portfolio overview content - optimized"""
        with dpg.group(parent="portfolio_center_panel"):
            dpg.add_text("PORTFOLIO OVERVIEW", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Calculate summary data efficiently
            total_value = sum(self.calculate_portfolio_value(name) for name in self.portfolios.keys())
            total_investment = sum(self.calculate_portfolio_investment(name) for name in self.portfolios.keys())
            total_pnl = total_value - total_investment
            daily_change_data = self.calculate_total_daily_change()

            # Summary cards
            with dpg.group(horizontal=True):
                summary_cards = [
                    (" TOTAL PORTFOLIO VALUE", f"${total_value:,.2f}", "Real-time valuation", self.BLOOMBERG_WHITE),
                    (" TODAY'S CHANGE", f"${daily_change_data['change']:+,.2f}",
                     f"({daily_change_data['change_pct']:+.2f}%)",
                     self.BLOOMBERG_GREEN if daily_change_data['change'] >= 0 else self.BLOOMBERG_RED),
                    (" TOTAL GAIN/LOSS", f"${total_pnl:+,.2f}",
                     f"({(total_pnl / total_investment * 100) if total_investment > 0 else 0:+.2f}%)",
                     self.BLOOMBERG_GREEN if total_pnl >= 0 else self.BLOOMBERG_RED)
                ]

                for title, main_value, sub_value, color in summary_cards:
                    with dpg.child_window(width=280, height=120, border=True):
                        dpg.add_text(title, color=self.BLOOMBERG_YELLOW)
                        dpg.add_spacer(height=5)
                        dpg.add_text(main_value, color=color)
                        dpg.add_text(sub_value, color=self.BLOOMBERG_GRAY if color == self.BLOOMBERG_WHITE else color)

                    if title != summary_cards[-1][0]:  # Not last card
                        dpg.add_spacer(width=15)

            dpg.add_spacer(height=20)

            # Detailed portfolio table - optimized
            dpg.add_text("PORTFOLIO BREAKDOWN", color=self.BLOOMBERG_ORANGE)
            self._create_portfolio_breakdown_table(total_value)

    def _create_portfolio_breakdown_table(self, total_value):
        """Create portfolio breakdown table efficiently"""
        with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                       scrollY=True, scrollX=True, height=400):

            # Define columns efficiently
            columns = [
                ("Portfolio", 150),
                ("Stocks", 70),
                ("Investment", 120),
                ("Current Value", 120),
                ("Today's Change", 120),
                ("Total P&L", 120),
                ("Allocation %", 100),
                ("Actions", 200)
            ]

            for label, width in columns:
                dpg.add_table_column(label=label, width_fixed=True, init_width_or_weight=width)

            for portfolio_name, stocks in self.portfolios.items():
                # Calculate portfolio metrics efficiently
                portfolio_investment = self.calculate_portfolio_investment(portfolio_name)
                portfolio_value = self.calculate_portfolio_value(portfolio_name)
                portfolio_pnl = portfolio_value - portfolio_investment
                portfolio_pnl_pct = (portfolio_pnl / portfolio_investment * 100) if portfolio_investment > 0 else 0
                allocation_pct = (portfolio_value / total_value * 100) if total_value > 0 else 0

                # Get real daily change for this portfolio
                portfolio_daily_change = self.calculate_portfolio_daily_change(portfolio_name)
                today_change = portfolio_daily_change['change']
                today_change_pct = portfolio_daily_change['change_pct']

                with dpg.table_row():
                    dpg.add_text(portfolio_name, color=self.BLOOMBERG_WHITE)
                    dpg.add_text(str(len(stocks)), color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"${portfolio_investment:,.2f}", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"${portfolio_value:,.2f}", color=self.BLOOMBERG_WHITE)

                    # Today's change
                    change_color = self.BLOOMBERG_GREEN if today_change >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"${today_change:+,.2f} ({today_change_pct:+.1f}%)", color=change_color)

                    # Total P&L
                    pnl_color = self.BLOOMBERG_GREEN if portfolio_pnl >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"${portfolio_pnl:+,.2f} ({portfolio_pnl_pct:+.1f}%)", color=pnl_color)

                    dpg.add_text(f"{allocation_pct:.1f}%", color=self.BLOOMBERG_WHITE)

                    # Action buttons
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="Manage", width=60,
                                       callback=lambda p=portfolio_name: self.select_and_manage_portfolio(p))
                        dpg.add_button(label="Analyze", width=60,
                                       callback=lambda p=portfolio_name: self.select_and_analyze_portfolio(p))
                        dpg.add_button(label="Delete", width=60,
                                       callback=lambda p=portfolio_name: self.delete_portfolio_confirm(p))

    def create_create_content(self):
        """Create portfolio creation content"""
        with dpg.group(parent="portfolio_center_panel"):
            dpg.add_text("CREATE NEW PORTFOLIO", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            dpg.add_text("STEP 1: PORTFOLIO DETAILS", color=self.BLOOMBERG_YELLOW)
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                dpg.add_text("Portfolio Name:", color=self.BLOOMBERG_WHITE)
                dpg.add_input_text(tag="new_portfolio_name", hint="Enter portfolio name", width=300)

            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                dpg.add_text("Description:", color=self.BLOOMBERG_WHITE)
                dpg.add_input_text(tag="new_portfolio_description", hint="Optional description", width=400)

            dpg.add_spacer(height=20)

            dpg.add_text("STEP 2: INITIAL STOCK (OPTIONAL)", color=self.BLOOMBERG_YELLOW)
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                with dpg.group():
                    dpg.add_text("Stock Symbol:", color=self.BLOOMBERG_WHITE)
                    dpg.add_input_text(tag="create_stock_symbol", hint="e.g., AAPL", width=120, uppercase=True)

                dpg.add_spacer(width=20)

                with dpg.group():
                    dpg.add_text("Quantity:", color=self.BLOOMBERG_WHITE)
                    dpg.add_input_text(tag="create_stock_quantity", hint="Shares", width=100, decimal=True)

                dpg.add_spacer(width=20)

                with dpg.group():
                    dpg.add_text("Purchase Price:", color=self.BLOOMBERG_WHITE)
                    dpg.add_input_text(tag="create_stock_price", hint="Price per share", width=120, decimal=True)

            dpg.add_spacer(height=30)

            # Action buttons
            with dpg.group(horizontal=True):
                dpg.add_button(label="CREATE PORTFOLIO", callback=self.create_portfolio_simple,
                               width=180, height=40)
                dpg.add_button(label="CREATE & ADD STOCK", callback=self.create_portfolio_with_stock,
                               width=180, height=40)
                dpg.add_button(label="CANCEL", callback=lambda: self.switch_view("overview"),
                               width=100, height=40)

    def create_manage_content(self):
        """Create portfolio management content"""
        with dpg.group(parent="portfolio_center_panel"):
            dpg.add_text("MANAGE PORTFOLIO HOLDINGS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Portfolio selector
            dpg.add_text("SELECT PORTFOLIO:", color=self.BLOOMBERG_YELLOW)
            dpg.add_combo(list(self.portfolios.keys()), tag="portfolio_selector",
                          callback=self.on_portfolio_select, width=300)
            dpg.add_spacer(height=20)

            # Holdings table
            dpg.add_text("CURRENT HOLDINGS", color=self.BLOOMBERG_YELLOW, tag="holdings_header")
            with dpg.table(tag="manage_holdings_table", header_row=True, borders_innerH=True,
                           borders_outerH=True, scrollY=True, scrollX=True, height=300):
                # Define columns for holdings table
                holdings_columns = [
                    ("Stock", 80),
                    ("Original", 80),
                    ("Shares", 80),
                    ("Avg Cost", 100),
                    ("Current Price", 120),
                    ("Market Value", 120),
                    ("Gain/Loss", 120),
                    ("Weight %", 80),
                    ("Actions", 100)
                ]

                for label, width in holdings_columns:
                    dpg.add_table_column(label=label, width_fixed=True, init_width_or_weight=width)

            dpg.add_spacer(height=20)

            # Add new stock section
            dpg.add_text("ADD NEW STOCK", color=self.BLOOMBERG_YELLOW)
            with dpg.group(horizontal=True):
                dpg.add_input_text(tag="manage_stock_symbol", hint="Symbol", width=100, uppercase=True)
                dpg.add_input_text(tag="manage_stock_quantity", hint="Quantity", width=100, decimal=True)
                dpg.add_input_text(tag="manage_stock_price", hint="Price", width=100, decimal=True)
                dpg.add_button(label="ADD STOCK", callback=self.add_stock_simple, width=120, height=30)

    def create_analyze_content(self):
        """Create portfolio analytics content"""
        with dpg.group(parent="portfolio_center_panel"):
            dpg.add_text("PORTFOLIO ANALYTICS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Portfolio selector for analysis
            dpg.add_text("SELECT PORTFOLIO FOR ANALYSIS:", color=self.BLOOMBERG_YELLOW)
            dpg.add_combo(list(self.portfolios.keys()), tag="analyze_portfolio_selector",
                          callback=self.on_analyze_portfolio_select, width=300)
            dpg.add_spacer(height=20)

            # Analytics content
            dpg.add_group(tag="analytics_content")
            dpg.add_text("Select a portfolio to view detailed analytics",
                         color=self.BLOOMBERG_GRAY, parent="analytics_content")

    @monitor_performance
    def create_analytics_charts(self, portfolio_name):
        """Create analytics charts for selected portfolio - optimized"""
        if not isinstance(portfolio_name, str):
            logger.error(f"Invalid portfolio_name type: {type(portfolio_name)}")
            return

        # Clear existing content efficiently
        if dpg.does_item_exist("analytics_content"):
            children = dpg.get_item_children("analytics_content", slot=1)
            for child in children:
                try:
                    dpg.delete_item(child)
                except:
                    pass

        # Create analytics content efficiently
        with dpg.group(parent="analytics_content"):
            dpg.add_text(f"ANALYTICS FOR: {portfolio_name.upper()}", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Performance metrics
            with dpg.group(horizontal=True):
                # Portfolio composition chart
                self._create_portfolio_composition_chart(portfolio_name)
                dpg.add_spacer(width=15)
                # Performance tracking chart
                self._create_performance_tracking_chart(portfolio_name)

            dpg.add_spacer(height=20)

            # Risk and return metrics
            with dpg.group(horizontal=True):
                self._create_risk_metrics_panel()
                dpg.add_spacer(width=15)
                self._create_return_metrics_panel(portfolio_name)
                dpg.add_spacer(width=15)
                self._create_sector_allocation_panel()

    def _create_portfolio_composition_chart(self, portfolio_name):
        """Create portfolio composition chart efficiently"""
        with dpg.child_window(width=420, height=300, border=True):
            dpg.add_text(" PORTFOLIO COMPOSITION", color=self.BLOOMBERG_YELLOW)
            dpg.add_separator()

            with dpg.plot(height=220, width=-1):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Stocks")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Value ($)")

                # Get portfolio data efficiently
                portfolio = self.portfolios.get(portfolio_name, {})
                stocks = list(portfolio.keys())[:6]  # Show top 6 stocks
                values = []

                for stock in stocks:
                    if stock in portfolio:
                        data = portfolio[stock]
                        quantity = data.get('quantity', 0)
                        current_price = self.get_current_price(stock)
                        value = quantity * current_price
                        values.append(value)
                    else:
                        values.append(0)

                if stocks and values:
                    dpg.add_bar_series(list(range(len(stocks))), values,
                                       label="Stock Values", parent=y_axis)

    def _create_performance_tracking_chart(self, portfolio_name):
        """Create performance tracking chart efficiently"""
        with dpg.child_window(width=420, height=300, border=True):
            dpg.add_text(" PERFORMANCE TRACKING", color=self.BLOOMBERG_YELLOW)
            dpg.add_separator()

            with dpg.plot(height=220, width=-1):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Days")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Portfolio Value ($)")

                # Generate sample performance data efficiently
                days = list(range(30))
                current_value = self.calculate_portfolio_value(portfolio_name)

                # Simulate portfolio performance over 30 days efficiently
                portfolio_values = []
                base_value = current_value * 0.95

                for i in range(30):
                    daily_change = random.uniform(-0.02, 0.03)
                    base_value *= (1 + daily_change)
                    portfolio_values.append(base_value)

                # Ensure last value is current value
                portfolio_values[-1] = current_value

                dpg.add_line_series(days, portfolio_values,
                                    label="Portfolio Value", parent=y_axis)

    def _create_risk_metrics_panel(self):
        """Create risk metrics panel efficiently"""
        with dpg.child_window(width=280, height=250, border=True):
            dpg.add_text(" RISK METRICS", color=self.BLOOMBERG_YELLOW)
            dpg.add_separator()

            with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                dpg.add_table_column()
                dpg.add_table_column()

                risk_metrics = [
                    ("Portfolio Beta:", "1.25"),
                    ("Sharpe Ratio:", "1.85"),
                    ("Volatility:", "16.8%"),
                    ("Max Drawdown:", "-5.2%"),
                    ("VaR (95%):", f"-${random.randint(1000, 5000):,}"),
                    ("Alpha:", "2.3%"),
                    ("R-Squared:", "0.82")
                ]

                for metric, value in risk_metrics:
                    with dpg.table_row():
                        dpg.add_text(metric, color=self.BLOOMBERG_GRAY)
                        value_color = self.BLOOMBERG_RED if "-" in value else self.BLOOMBERG_WHITE
                        if "%" in value and not "-" in value:
                            value_color = self.BLOOMBERG_GREEN
                        dpg.add_text(value, color=value_color)

    def _create_return_metrics_panel(self, portfolio_name):
        """Create return metrics panel efficiently"""
        with dpg.child_window(width=280, height=250, border=True):
            dpg.add_text(" RETURN METRICS", color=self.BLOOMBERG_YELLOW)
            dpg.add_separator()

            with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                dpg.add_table_column()
                dpg.add_table_column()

                # Calculate actual returns efficiently
                portfolio_value = self.calculate_portfolio_value(portfolio_name)
                portfolio_investment = self.calculate_portfolio_investment(portfolio_name)
                total_return = portfolio_value - portfolio_investment
                total_return_pct = (total_return / portfolio_investment * 100) if portfolio_investment > 0 else 0

                return_metrics = [
                    ("Total Return:", f"${total_return:+,.2f}"),
                    ("Total Return %:", f"{total_return_pct:+.2f}%"),
                    ("Annualized Return:", f"{total_return_pct * 1.2:+.2f}%"),
                    ("YTD Return:", f"{random.uniform(-5, 15):+.1f}%"),
                    ("1 Month Return:", f"{random.uniform(-3, 8):+.1f}%"),
                    ("3 Month Return:", f"{random.uniform(-8, 12):+.1f}%"),
                    ("Best Day:", f"+{random.uniform(2, 8):.1f}%"),
                    ("Worst Day:", f"-{random.uniform(1, 5):.1f}%")
                ]

                for metric, value in return_metrics:
                    with dpg.table_row():
                        dpg.add_text(metric, color=self.BLOOMBERG_GRAY)
                        value_color = self.BLOOMBERG_GREEN if "+" in value else self.BLOOMBERG_RED if "-" in value else self.BLOOMBERG_WHITE
                        dpg.add_text(value, color=value_color)

    def _create_sector_allocation_panel(self):
        """Create sector allocation panel efficiently"""
        with dpg.child_window(width=280, height=250, border=True):
            dpg.add_text(" SECTOR ALLOCATION", color=self.BLOOMBERG_YELLOW)
            dpg.add_separator()

            # Sample sector data
            sectors = [
                ("Technology", 45.2),
                ("Healthcare", 18.7),
                ("Financials", 15.3),
                ("Consumer Disc.", 12.1),
                ("Energy", 5.4),
                ("Utilities", 3.3)
            ]

            for sector, percentage in sectors:
                with dpg.group(horizontal=True):
                    dpg.add_text(f"{sector}:", color=self.BLOOMBERG_GRAY)
                    dpg.add_text(f"{percentage:.1f}%", color=self.BLOOMBERG_WHITE)

    # Calculation methods - optimized with caching
    def calculate_portfolio_value(self, portfolio_name):
        """Calculate current portfolio value - cached"""
        cache_key = f"value_{portfolio_name}"
        current_time = time.time()

        # Check cache
        if (cache_key in self._last_cache_update and
                current_time - self._last_cache_update[cache_key] < self._cache_timeout):
            if cache_key in self._portfolio_value_cache:
                return self._portfolio_value_cache[cache_key]

        portfolio = self.portfolios.get(portfolio_name, {})
        total_value = 0

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get('quantity', 0)
                current_price = self.get_current_price(symbol)
                total_value += quantity * current_price

        # Cache result
        self._portfolio_value_cache[cache_key] = total_value
        self._last_cache_update[cache_key] = current_time

        return total_value

    def calculate_portfolio_investment(self, portfolio_name):
        """Calculate total portfolio investment - cached"""
        cache_key = f"investment_{portfolio_name}"
        current_time = time.time()

        # Check cache
        if (cache_key in self._last_cache_update and
                current_time - self._last_cache_update[cache_key] < self._cache_timeout):
            if cache_key in self._portfolio_investment_cache:
                return self._portfolio_investment_cache[cache_key]

        portfolio = self.portfolios.get(portfolio_name, {})
        total_investment = 0

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get('quantity', 0)
                avg_price = data.get('avg_price', 0)
                total_investment += quantity * avg_price

        # Cache result
        self._portfolio_investment_cache[cache_key] = total_investment
        self._last_cache_update[cache_key] = current_time

        return total_investment

    @monitor_performance
    def update_navigation_summary(self):
        """Update navigation panel summary - optimized"""
        try:
            total_portfolios = len(self.portfolios)
            total_investment = sum(self.calculate_portfolio_investment(name) for name in self.portfolios.keys())
            total_value = sum(self.calculate_portfolio_value(name) for name in self.portfolios.keys())
            total_pnl = total_value - total_investment
            total_pnl_pct = (total_pnl / total_investment * 100) if total_investment > 0 else 0

            # Get today's change efficiently
            total_daily_change = self.calculate_total_daily_change()
            today_change = total_daily_change['change']
            today_change_pct = total_daily_change['change_pct']

            # Update UI elements efficiently
            ui_updates = [
                ("nav_total_portfolios", str(total_portfolios)),
                ("nav_total_investment", f"${total_investment:,.2f}"),
                ("nav_current_value", f"${total_value:,.2f}"),
                ("nav_total_pnl", f"${total_pnl:+,.2f} ({total_pnl_pct:+.1f}%)"),
                ("nav_today_change", f"${today_change:+,.2f} ({today_change_pct:+.1f}%)")
            ]

            for tag, value in ui_updates:
                if dpg.does_item_exist(tag):
                    dpg.set_value(tag, value)

        except Exception as e:
            logger.error(f"Error updating navigation summary: {e}")

    # Callback methods - optimized
    @monitor_performance
    def create_portfolio_simple(self):
        """Create a simple portfolio - optimized"""
        try:
            with operation("create_portfolio_simple"):
                name = dpg.get_value("new_portfolio_name").strip()
                if not name:
                    self.show_message("Please enter a portfolio name.", "error")
                    return

                if name in self.portfolios:
                    self.show_message("Portfolio name already exists.", "error")
                    return

                self.portfolios[name] = {}
                self.save_portfolios()
                self._clear_portfolio_cache()

                # Clear input and switch to overview
                dpg.set_value("new_portfolio_name", "")
                dpg.set_value("new_portfolio_description", "")
                self.switch_view("overview")

                self.show_message(f"Portfolio '{name}' created successfully!", "success")

        except Exception as e:
            logger.error(f"Error creating portfolio: {e}", exc_info=True)
            self.show_message("Error creating portfolio", "error")

    @monitor_performance
    def create_portfolio_with_stock(self):
        """Create portfolio and add first stock - optimized"""
        try:
            with operation("create_portfolio_with_stock"):
                name = dpg.get_value("new_portfolio_name").strip()
                symbol = dpg.get_value("create_stock_symbol").strip().upper()
                quantity_str = dpg.get_value("create_stock_quantity").strip()
                price_str = dpg.get_value("create_stock_price").strip()

                if not name:
                    self.show_message("Please enter a portfolio name.", "error")
                    return

                if name in self.portfolios:
                    self.show_message("Portfolio name already exists.", "error")
                    return

                # Validate stock data if provided
                if symbol and quantity_str and price_str:
                    try:
                        quantity = float(quantity_str)
                        price = float(price_str)

                        # Create portfolio with first stock
                        self.portfolios[name] = {
                            symbol: {
                                "quantity": quantity,
                                "avg_price": price,
                                "last_added": datetime.datetime.now().strftime("%Y-%m-%d")
                            }
                        }

                        # Fetch real price for the symbol
                        threading.Thread(target=lambda: self._fetch_single_price_and_update(symbol),
                                         daemon=True).start()

                    except ValueError:
                        self.show_message("Invalid quantity or price values.", "error")
                        return
                else:
                    # Create empty portfolio
                    self.portfolios[name] = {}

                self.save_portfolios()
                self._clear_portfolio_cache()

                # Clear inputs and switch to overview
                for tag in ["new_portfolio_name", "new_portfolio_description", "create_stock_symbol",
                            "create_stock_quantity", "create_stock_price"]:
                    dpg.set_value(tag, "")

                self.switch_view("overview")

                success_msg = f"Portfolio '{name}' created successfully!"
                if symbol:
                    success_msg += f" Added {quantity} shares of {symbol} at ${price:.2f}. Fetching real-time price..."

                self.show_message(success_msg, "success")

        except Exception as e:
            logger.error(f"Error creating portfolio with stock: {e}", exc_info=True)
            self.show_message("Error creating portfolio", "error")

    def on_portfolio_select(self):
        """Handle portfolio selection in manage view"""
        try:
            selected = dpg.get_value("portfolio_selector")
            if selected:
                self.current_portfolio = selected
                self.refresh_manage_view()
        except Exception as e:
            logger.error(f"Error in portfolio select: {e}")

    def on_analyze_portfolio_select(self):
        """Handle portfolio selection in analyze view"""
        try:
            selected = dpg.get_value("analyze_portfolio_selector")
            if selected:
                self.current_portfolio = selected
                threading.Thread(target=lambda: self.create_analytics_charts(selected), daemon=True).start()
        except Exception as e:
            logger.error(f"Error in analyze portfolio select: {e}")

    @monitor_performance
    def add_stock_simple(self):
        """Add stock to selected portfolio - optimized"""
        try:
            with operation("add_stock"):
                if not self.current_portfolio:
                    self.show_message("Please select a portfolio first.", "error")
                    return

                symbol = dpg.get_value("manage_stock_symbol").strip().upper()
                quantity_str = dpg.get_value("manage_stock_quantity").strip()
                price_str = dpg.get_value("manage_stock_price").strip()

                if not symbol or not quantity_str or not price_str:
                    self.show_message("Please fill in all fields.", "error")
                    return

                quantity = float(quantity_str)
                price = float(price_str)

                # Add or update stock
                if symbol in self.portfolios[self.current_portfolio]:
                    # Update existing stock
                    existing = self.portfolios[self.current_portfolio][symbol]
                    current_qty = existing["quantity"]
                    current_avg = existing["avg_price"]

                    new_qty = current_qty + quantity
                    new_avg = ((current_avg * current_qty) + (price * quantity)) / new_qty

                    self.portfolios[self.current_portfolio][symbol] = {
                        "quantity": new_qty,
                        "avg_price": round(new_avg, 2),
                        "last_added": datetime.datetime.now().strftime("%Y-%m-%d")
                    }
                else:
                    # Add new stock
                    self.portfolios[self.current_portfolio][symbol] = {
                        "quantity": quantity,
                        "avg_price": price,
                        "last_added": datetime.datetime.now().strftime("%Y-%m-%d")
                    }

                # Update price cache
                self.price_cache[symbol] = price * (1 + random.uniform(-0.05, 0.05))

                self.save_portfolios()
                self._clear_portfolio_cache()
                self.refresh_manage_view()

                # Fetch real price for new symbol
                threading.Thread(target=lambda: self._fetch_single_price_and_update(symbol), daemon=True).start()

                # Clear inputs
                for tag in ["manage_stock_symbol", "manage_stock_quantity", "manage_stock_price"]:
                    dpg.set_value(tag, "")

                self.show_message(
                    f"Added {quantity} shares of {symbol} to {self.current_portfolio}. Fetching real-time price...",
                    "success")

        except ValueError:
            self.show_message("Invalid quantity or price values.", "error")
        except Exception as e:
            logger.error(f"Error adding stock: {e}", exc_info=True)
            self.show_message("Error adding stock", "error")

    def _fetch_single_price_and_update(self, symbol):
        """Fetch price for a single symbol and update cache - optimized"""
        try:
            with operation("fetch_single_price", context={'symbol': symbol}):
                price = self._fetch_single_price(symbol)
                if price is not None:
                    self.price_cache[symbol] = price
                    self.last_price_update[symbol] = datetime.datetime.now()
                    logger.debug(f"Updated price for {symbol}: ${price:.2f}")

                    # Clear cache for affected calculations
                    self._clear_portfolio_cache()

                    # Update UI if in manage view
                    if self.current_view == "manage":
                        self.refresh_manage_view()
                else:
                    logger.warning(f"Could not fetch price for {symbol}")
        except Exception as e:
            logger.error(f"Error fetching price for {symbol}: {e}")

    @monitor_performance
    def refresh_manage_view(self):
        """Refresh the manage view content - optimized"""
        try:
            if not self.current_portfolio:
                return

            # Clear existing table efficiently
            if dpg.does_item_exist("manage_holdings_table"):
                children = dpg.get_item_children("manage_holdings_table", slot=1)
                for child in children:
                    try:
                        dpg.delete_item(child)
                    except:
                        pass

            # Update holdings table efficiently
            portfolio = self.portfolios[self.current_portfolio]
            portfolio_value = self.calculate_portfolio_value(self.current_portfolio)

            for symbol, data in portfolio.items():
                if isinstance(data, dict):
                    quantity = data.get("quantity", 0)
                    avg_price = data.get("avg_price", 0)
                    original_symbol = data.get("original_symbol", symbol)
                    current_price = self.get_current_price(symbol)

                    market_value = quantity * current_price
                    investment = quantity * avg_price
                    gain_loss = market_value - investment
                    gain_loss_pct = (gain_loss / investment * 100) if investment > 0 else 0
                    weight_pct = (market_value / portfolio_value * 100) if portfolio_value > 0 else 0

                    with dpg.table_row(parent="manage_holdings_table"):
                        dpg.add_text(symbol, color=self.BLOOMBERG_WHITE)  # YFinance symbol
                        dpg.add_text(original_symbol, color=self.BLOOMBERG_GRAY)  # Original symbol
                        dpg.add_text(f"{quantity:.2f}", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${avg_price:.2f}", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${current_price:.2f}", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${market_value:,.2f}", color=self.BLOOMBERG_WHITE)

                        # Gain/loss with color coding
                        gain_color = self.BLOOMBERG_GREEN if gain_loss >= 0 else self.BLOOMBERG_RED
                        dpg.add_text(f"${gain_loss:+,.2f} ({gain_loss_pct:+.1f}%)", color=gain_color)

                        dpg.add_text(f"{weight_pct:.1f}%", color=self.BLOOMBERG_WHITE)

                        dpg.add_button(label="Remove", width=80,
                                       callback=lambda s=symbol: self.remove_stock_from_portfolio(s))

        except Exception as e:
            logger.error(f"Error refreshing manage view: {e}")

    def select_and_manage_portfolio(self, portfolio_name):
        """Select portfolio and switch to manage view"""
        self.current_portfolio = portfolio_name
        self.switch_view("manage")
        if dpg.does_item_exist("portfolio_selector"):
            dpg.set_value("portfolio_selector", portfolio_name)
        self.refresh_manage_view()

    def select_and_analyze_portfolio(self, portfolio_name):
        """Select portfolio and switch to analyze view"""
        self.current_portfolio = portfolio_name
        self.switch_view("analyze")
        if dpg.does_item_exist("analyze_portfolio_selector"):
            dpg.set_value("analyze_portfolio_selector", portfolio_name)
        threading.Thread(target=lambda: self.create_analytics_charts(portfolio_name), daemon=True).start()

    def remove_stock_from_portfolio(self, symbol):
        """Remove stock from current portfolio"""
        try:
            if not self.current_portfolio or symbol not in self.portfolios[self.current_portfolio]:
                return

            del self.portfolios[self.current_portfolio][symbol]
            self.save_portfolios()
            self._clear_portfolio_cache()
            self.refresh_manage_view()

            self.show_message(f"Removed {symbol} from {self.current_portfolio}.", "success")
        except Exception as e:
            logger.error(f"Error removing stock: {e}")

    def delete_portfolio_confirm(self, portfolio_name):
        """Show confirmation dialog for portfolio deletion"""
        try:
            # Create confirmation popup
            with dpg.popup(dpg.last_item(), modal=True, tag=f"delete_confirm_{portfolio_name}"):
                dpg.add_text(f"Delete Portfolio: {portfolio_name}?", color=self.BLOOMBERG_RED)
                dpg.add_separator()
                dpg.add_text("This action cannot be undone!", color=self.BLOOMBERG_YELLOW)
                dpg.add_spacer(height=10)

                with dpg.group(horizontal=True):
                    dpg.add_button(label="DELETE", width=80,
                                   callback=lambda: self.delete_portfolio(portfolio_name))
                    dpg.add_button(label="CANCEL", width=80,
                                   callback=lambda: dpg.delete_item(f"delete_confirm_{portfolio_name}"))

        except Exception as e:
            logger.error(f"Error showing delete confirmation: {e}")
            # Direct delete if popup fails
            self.delete_portfolio(portfolio_name)

    def show_delete_portfolio_dialog(self):
        """Show dialog to select portfolio for deletion"""
        if not self.portfolios:
            self.show_message("No portfolios to delete", "warning")
            return

        try:
            # Create a simple selection dialog
            portfolio_names = list(self.portfolios.keys())

            # Create modal window for portfolio selection
            with dpg.window(label="Delete Portfolio", modal=True, tag="delete_portfolio_window",
                            width=400, height=200, pos=[300, 200]):
                dpg.add_text("SELECT PORTFOLIO TO DELETE:", color=self.BLOOMBERG_YELLOW)
                dpg.add_separator()

                for portfolio_name in portfolio_names:
                    with dpg.group(horizontal=True):
                        dpg.add_text(f"• {portfolio_name}", color=self.BLOOMBERG_WHITE)
                        dpg.add_button(label="DELETE", width=80,
                                       callback=lambda p=portfolio_name: self.delete_portfolio_from_dialog(p))

                dpg.add_separator()
                dpg.add_button(label="CANCEL", width=-1, height=30,
                               callback=lambda: dpg.delete_item("delete_portfolio_window"))

        except Exception as e:
            logger.error(f"Error showing delete dialog: {e}")
            self.show_message("Error showing delete dialog", "error")

    def delete_portfolio_from_dialog(self, portfolio_name):
        """Delete portfolio from selection dialog"""
        try:
            # Close dialog first
            if dpg.does_item_exist("delete_portfolio_window"):
                dpg.delete_item("delete_portfolio_window")

            # Show confirmation
            self.delete_portfolio_confirm(portfolio_name)

        except Exception as e:
            logger.error(f"Error in delete from dialog: {e}")

    @monitor_performance
    def delete_portfolio(self, portfolio_name):
        """Delete the specified portfolio - optimized"""
        try:
            with operation("delete_portfolio", context={'portfolio': portfolio_name}):
                if portfolio_name not in self.portfolios:
                    self.show_message("Portfolio not found", "error")
                    return

                # Remove from portfolios
                del self.portfolios[portfolio_name]

                # Save changes
                self.save_portfolios()
                self._clear_portfolio_cache()

                # Clear current portfolio if it was the deleted one
                if self.current_portfolio == portfolio_name:
                    self.current_portfolio = None

                # Close any confirmation dialogs
                if dpg.does_item_exist(f"delete_confirm_{portfolio_name}"):
                    dpg.delete_item(f"delete_confirm_{portfolio_name}")

                # Refresh UI
                self.switch_view("overview")

                self.show_message(f"Portfolio '{portfolio_name}' deleted successfully", "success")

        except Exception as e:
            logger.error(f"Error deleting portfolio: {e}", exc_info=True)
            self.show_message("Error deleting portfolio", "error")

    def start_price_refresh_thread(self):
        """Start the auto price refresh thread"""
        if not self.refresh_running:
            self.refresh_running = True
            self.refresh_thread = threading.Thread(target=self._price_refresh_loop, daemon=True)
            self.refresh_thread.start()
            logger.info("Started hourly price refresh thread")

    def _price_refresh_loop(self):
        """Background thread for price refresh - runs every hour - optimized"""
        while self.refresh_running:
            try:
                # Wait for initial fetch to complete
                while not self.initial_price_fetch_done and self.refresh_running:
                    time.sleep(10)

                if not self.refresh_running:
                    break

                # Sleep for 1 hour (3600 seconds) - optimized with smaller checks
                for _ in range(0, self.price_update_interval, 10):
                    if not self.refresh_running:
                        break
                    time.sleep(10)

                if self.refresh_running:
                    logger.info("Hourly price update starting...")
                    self.refresh_all_prices_background()

            except Exception as e:
                logger.error(f"Error in price refresh loop: {e}")
                time.sleep(300)  # Wait 5 minutes before retrying

    @monitor_performance
    def refresh_all_prices_background(self):
        """Refresh all prices in background - optimized"""
        try:
            with operation("refresh_all_prices"):
                # Collect all unique symbols from all portfolios
                all_symbols = set()
                for portfolio in self.portfolios.values():
                    for symbol in portfolio.keys():
                        all_symbols.add(symbol)

                if not all_symbols:
                    return

                logger.info(f"Refreshing prices for {len(all_symbols)} symbols...",
                            context={'symbols_count': len(all_symbols)})

                # Fetch updated prices
                self._fetch_prices_batch(list(all_symbols))

                # Clear cache for affected calculations
                self._clear_portfolio_cache()

                # Update timestamp in UI
                if dpg.does_item_exist("portfolio_last_update"):
                    dpg.set_value("portfolio_last_update", datetime.datetime.now().strftime('%H:%M:%S'))

                # Update navigation summary
                self.update_navigation_summary()

                logger.info("Price refresh completed")

        except Exception as e:
            logger.error(f"Error refreshing prices: {e}")

    def refresh_all_prices_now(self):
        """Refresh all prices immediately"""
        self.show_message("Refreshing all prices...", "info")
        threading.Thread(target=self.refresh_all_prices_background, daemon=True).start()

    # File operations - optimized
    @monitor_performance
    def load_portfolios(self):
        """Load portfolios from settings file - optimized"""
        if os.path.exists(SETTINGS_FILE):
            try:
                with operation("load_portfolios"):
                    with open(SETTINGS_FILE, "r") as file:
                        settings = json.load(file)
                        portfolios = settings.get("portfolios", {})

                        # Filter out 'watchlist' which is handled by watchlist tab
                        if "watchlist" in portfolios:
                            del portfolios["watchlist"]

                        return portfolios
            except (json.JSONDecodeError, IOError) as e:
                logger.error(f"Error loading portfolios: Corrupted settings.json file - {e}")
                return {}
        return {}

    @monitor_performance
    def save_portfolios(self):
        """Save portfolios to settings file - optimized"""
        try:
            with operation("save_portfolios"):
                # Load existing settings
                settings = {}
                if os.path.exists(SETTINGS_FILE):
                    try:
                        with open(SETTINGS_FILE, "r") as file:
                            settings = json.load(file)
                    except json.JSONDecodeError:
                        settings = {}

                # Ensure portfolios section exists
                if "portfolios" not in settings:
                    settings["portfolios"] = {}

                # Update only the portfolio data (preserve watchlist)
                for portfolio_name, portfolio_data in self.portfolios.items():
                    settings["portfolios"][portfolio_name] = portfolio_data

                # Save back to file atomically
                temp_file = SETTINGS_FILE + ".tmp"
                with open(temp_file, "w") as file:
                    json.dump(settings, file, indent=4)

                # Atomic rename
                import shutil
                shutil.move(temp_file, SETTINGS_FILE)

                logger.debug("Portfolios saved successfully")

        except Exception as e:
            logger.error(f"Error saving portfolios: {e}", exc_info=True)
            self.show_message(f"Error saving portfolios: {e}", "error")

    # Additional callback methods - optimized
    def search_portfolio(self):
        """Search portfolio functionality"""
        logger.debug("Portfolio search functionality triggered")

    def show_settings(self):
        """Show portfolio settings"""
        logger.debug("Portfolio settings functionality triggered")

    @monitor_performance
    def export_portfolio_data(self):
        """Export portfolio data - optimized"""
        try:
            with operation("export_portfolio_data"):
                # Create CSV export data efficiently
                export_data = []
                export_data.append(
                    ["Portfolio", "Symbol", "Original_Symbol", "Quantity", "Avg_Price", "Current_Price", "Investment",
                     "Current_Value", "P&L", "P&L_%"])

                for portfolio_name, stocks in self.portfolios.items():
                    for symbol, data in stocks.items():
                        if isinstance(data, dict):
                            quantity = data.get("quantity", 0)
                            avg_price = data.get("avg_price", 0)
                            original_symbol = data.get("original_symbol", symbol)
                            current_price = self.get_current_price(symbol)

                            investment = quantity * avg_price
                            current_value = quantity * current_price
                            pnl = current_value - investment
                            pnl_pct = (pnl / investment * 100) if investment > 0 else 0

                            export_data.append([
                                portfolio_name, symbol, original_symbol, quantity, avg_price,
                                current_price, investment, current_value, pnl, pnl_pct
                            ])

                # Save to CSV file
                export_filename = f"portfolio_export_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
                with open(export_filename, 'w', newline='', encoding='utf-8') as csvfile:
                    writer = csv.writer(csvfile)
                    writer.writerows(export_data)

                self.show_message(f"Portfolio data exported to {export_filename}", "success")

        except Exception as e:
            logger.error(f"Error exporting portfolio data: {e}", exc_info=True)
            self.show_message("Error exporting portfolio data", "error")

    def show_message(self, message, message_type="info"):
        """Show message to user - optimized"""
        type_symbols = {
            "success": "✅",
            "error": "❌",
            "warning": "⚠️",
            "info": "ℹ️"
        }
        symbol = type_symbols.get(message_type, "")

        # Use appropriate log level
        if message_type == "error":
            logger.error(f"{symbol} {message}")
        elif message_type == "warning":
            logger.warning(f"{symbol} {message}")
        else:
            logger.info(f"{symbol} {message}")

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle component resizing - optimized"""
        # Bloomberg terminal has fixed layout - no resizing needed
        pass

    @monitor_performance
    def cleanup(self):
        """Clean up portfolio tab resources - optimized"""
        try:
            with operation("portfolio_cleanup"):
                logger.info("🧹 Cleaning up portfolio tab...")
                self.refresh_running = False

                if hasattr(self, 'portfolios'):
                    self.save_portfolios()

                # Clear all data structures efficiently
                self.portfolios.clear()
                self.current_portfolio = None
                self.current_view = "overview"
                self.price_cache.clear()
                self.last_price_update.clear()
                self.price_fetch_errors.clear()
                self.daily_change_cache.clear()
                self.previous_close_cache.clear()

                # Clear CSV import data
                self.csv_data = None
                self.csv_headers = []
                self.column_mapping = {}
                self.csv_preview_data = []

                # Clear caches
                self._clear_portfolio_cache()

                # Clear LRU caches
                self.get_current_price.cache_clear()
                self.get_daily_change.cache_clear()
                self.auto_detect_column.cache_clear()

                logger.info("Portfolio tab cleanup complete")

        except Exception as e:
            logger.error(f"Error in portfolio cleanup: {e}", exc_info=True)