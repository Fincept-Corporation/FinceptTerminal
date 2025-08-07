# -*- coding: utf-8 -*-
# yfinance_data_tab.py

import dearpygui.dearpygui as dpg
import yfinance as yf
import pandas as pd
import numpy as np
from datetime import datetime, timedelta
import threading
import time
from fincept_terminal.Utils.Managers.theme_manager import AutomaticThemeManager

# Import new logger module
from fincept_terminal.Utils.Logging.logger import (
    info, debug, warning, error, operation, monitor_performance
)


class YFinanceDataTab:
    """Bloomberg Terminal YFinance Data Tab with performance optimizations and new logger"""

    def __init__(self, main_app=None):
        self.main_app = main_app
        self.current_ticker = None
        self.stock_data = None
        self.history_data = None
        self.financials_data = {}
        self.is_loading = False

        # Bloomberg Terminal Colors - Pre-computed for performance
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [0, 128, 255]

        # Performance optimization caches
        self._data_cache = {}
        self._chart_cache = {}
        self._last_update_time = None

        # Initialize theme manager
        try:
            self.theme_manager = AutomaticThemeManager()
            debug("Theme manager initialized successfully")
        except Exception as e:
            warning("Theme manager initialization failed", context={'error': str(e)})
            self.theme_manager = None

        info("YFinanceDataTab initialized")

    def get_label(self):
        return "Equity Research"

    def safe_add_text(self, text, **kwargs):
        """Safely add text with encoding error handling and performance optimization"""
        try:
            # Ensure text is a string and handle encoding efficiently
            if not isinstance(text, str):
                text = str(text)

            # More efficient character filtering using translate
            # Remove problematic characters while preserving performance
            text = text.encode('ascii', 'replace').decode('ascii')

            return dpg.add_text(text, **kwargs)
        except Exception as e:
            error("Text add error", context={'error': str(e), 'text_length': len(str(text))}, exc_info=True)
            try:
                return dpg.add_text("Text Error", **kwargs)
            except:
                return None

    def safe_set_value(self, tag, value):
        """Safely set value with encoding handling and performance optimization"""
        try:
            if isinstance(value, str):
                # Efficient character filtering
                value = ''.join(c for c in value if ord(c) < 128)
                # Replace common problematic characters in one pass
                replacements = {'…': '...', '–': '-', '—': '-'}
                for old, new in replacements.items():
                    value = value.replace(old, new)
            elif not isinstance(value, (int, float, bool)):
                value = str(value)
                value = ''.join(c for c in value if ord(c) < 128)

            if dpg.does_item_exist(tag):
                dpg.set_value(tag, value)
        except Exception as e:
            error("Set value error", context={'tag': tag, 'error': str(e)}, exc_info=True)
            # Try with simple fallback
            try:
                if dpg.does_item_exist(tag):
                    dpg.set_value(tag, "ERROR")
            except:
                pass

    @monitor_performance
    def create_content(self):
        """Create the complete YFinance data interface"""
        with operation("create_yfinance_content"):
            try:
                # Header section with search
                self.create_header_section()

                # Main scrollable content area
                with dpg.child_window(height=-30, border=False, tag="main_content_area"):
                    # Company Overview Section
                    self.create_company_overview_section()
                    dpg.add_spacer(height=15)

                    # Market Data Section
                    self.create_market_data_section()
                    dpg.add_spacer(height=15)

                    # Price Chart and Returns Section
                    self.create_price_and_returns_section()
                    dpg.add_spacer(height=15)

                    # Financial Statements Section
                    self.create_financial_statements_section()

                    # Financial Analysis Charts Section
                    self.create_financial_charts_section()

                # Status bar
                self.create_status_bar()
                self.start_time_updater()

                # Load AAPL by default after a short delay to ensure UI is ready
                threading.Timer(1.0, self.load_default_ticker).start()

                info("YFinance content created successfully")

            except Exception as e:
                error("Error creating YFinance content", context={'error': str(e)}, exc_info=True)
                self.safe_add_text(f"ERROR: {e}", color=self.BLOOMBERG_RED)

    def load_default_ticker(self):
        """Load AAPL ticker by default"""
        with operation("load_default_ticker"):
            try:
                dpg.set_value("ticker_search_input", "AAPL")
                self.handle_search()
                debug("Default ticker AAPL loaded")
            except Exception as e:
                error("Error loading default ticker", context={'error': str(e)}, exc_info=True)

    def create_header_section(self):
        """Create header with search functionality"""
        try:
            with dpg.group(horizontal=True):
                self.safe_add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                self.safe_add_text("MARKET DATA", color=self.BLOOMBERG_WHITE)
                self.safe_add_text(" | ", color=self.BLOOMBERG_GRAY)

                self.safe_add_text("TICKER:", color=self.BLOOMBERG_YELLOW)
                dpg.add_input_text(
                    width=120,
                    hint="Enter ticker",
                    tag="ticker_search_input",
                    on_enter=True,
                    callback=self.handle_search,
                    default_value="AAPL"
                )
                dpg.add_button(
                    label="SEARCH",
                    callback=self.handle_search,
                    width=70,
                    height=25,
                    tag="search_button"
                )

                self.safe_add_text(" | ", color=self.BLOOMBERG_GRAY)
                self.safe_add_text("TIME:", color=self.BLOOMBERG_YELLOW)
                self.safe_add_text(datetime.now().strftime('%H:%M:%S'), tag="header_time", color=self.BLOOMBERG_WHITE)

            dpg.add_separator()

            # Current stock display
            with dpg.group(tag="current_stock_display", show=False):
                with dpg.group(horizontal=True):
                    display_items = [
                        ("CURRENT:", "current_stock_name", self.BLOOMBERG_YELLOW, self.BLOOMBERG_WHITE),
                        ("PRICE:", "current_stock_price", self.BLOOMBERG_YELLOW, self.BLOOMBERG_GREEN),
                        ("", "current_stock_change", None, self.BLOOMBERG_GREEN)
                    ]

                    for i, (label, tag, label_color, text_color) in enumerate(display_items):
                        if i > 0:
                            self.safe_add_text(" | ", color=self.BLOOMBERG_GRAY)
                        if label:
                            self.safe_add_text(label, color=label_color)
                        self.safe_add_text("", tag=tag, color=text_color)

                dpg.add_separator()
        except Exception as e:
            error("Error creating header", context={'error': str(e)}, exc_info=True)

    def create_company_overview_section(self):
        """Create company overview section"""
        try:
            with dpg.collapsing_header(label="COMPANY OVERVIEW", default_open=True, tag="company_overview_header"):
                with dpg.group(horizontal=True):
                    # Company sections with optimized creation
                    sections = [
                        ("COMPANY INFORMATION", 380, 220, "company_info_text", [173, 216, 230], 360),
                        ("BUSINESS SUMMARY", 500, 220, "business_summary_text", [144, 238, 144], 480),
                        ("KEY METRICS", 320, 220, "key_metrics_text", [240, 128, 128], None)
                    ]

                    for title, width, height, tag, color, wrap in sections:
                        with dpg.child_window(width=width, height=height, border=True):
                            self.safe_add_text(title, color=self.BLOOMBERG_ORANGE)
                            dpg.add_separator()
                            text_kwargs = {"color": color, "tag": tag}
                            if wrap:
                                text_kwargs["wrap"] = wrap
                            self.safe_add_text("Loading AAPL data...", **text_kwargs)

        except Exception as e:
            error("Error creating company overview", context={'error': str(e)}, exc_info=True)

    def create_market_data_section(self):
        """Create market data section"""
        try:
            with dpg.collapsing_header(label="MARKET DATA & PERFORMANCE", default_open=True, tag="market_data_header"):
                with dpg.group(horizontal=True):
                    # Market data sections
                    sections = [
                        ("PRICE DATA", 300, 200, "price_data_text", [224, 255, 255]),
                        ("TRADING METRICS", 300, 200, "trading_metrics_text", [255, 255, 224]),
                        ("VALUATION METRICS", 300, 200, "valuation_metrics_text", [255, 182, 193]),
                        ("EXECUTIVE TEAM", 400, 200, "executives_text", self.BLOOMBERG_YELLOW)
                    ]

                    for title, width, height, tag, color in sections:
                        with dpg.child_window(width=width, height=height, border=True):
                            self.safe_add_text(title, color=self.BLOOMBERG_ORANGE)
                            dpg.add_separator()
                            self.safe_add_text("Loading AAPL data...", color=color, tag=tag)

        except Exception as e:
            error("Error creating market data", context={'error': str(e)}, exc_info=True)

    def create_price_and_returns_section(self):
        """Create price chart and returns section"""
        try:
            with dpg.collapsing_header(label="PRICE CHART & RETURNS", default_open=True, tag="price_returns_header"):
                # Chart controls
                with dpg.group(horizontal=True):
                    self.safe_add_text("PERIOD:", color=self.BLOOMBERG_YELLOW)
                    dpg.add_combo(
                        ["1D", "5D", "1M", "3M", "6M", "1Y", "2Y"],
                        default_value="3M",
                        width=100,
                        tag="chart_period_combo",
                        callback=self.update_price_chart
                    )
                    dpg.add_button(
                        label="UPDATE CHART",
                        callback=self.update_price_chart,
                        width=100,
                        height=25
                    )
                    self.safe_add_text("CHART TYPE:", color=self.BLOOMBERG_YELLOW)
                    dpg.add_combo(
                        ["Line Chart", "Candlestick"],
                        default_value="Candlestick",
                        width=120,
                        tag="chart_type_combo",
                        callback=self.update_price_chart
                    )

                dpg.add_spacer(height=10)

                with dpg.group(horizontal=True):
                    # Price Chart
                    with dpg.child_window(width=800, height=400, border=True):
                        self.safe_add_text("PRICE CHART", color=self.BLOOMBERG_ORANGE)
                        dpg.add_separator()

                        # Chart container
                        with dpg.group(tag="chart_container"):
                            self.safe_add_text("Loading AAPL chart data...",
                                               color=self.BLOOMBERG_YELLOW, tag="chart_placeholder")

                    # Returns Table
                    with dpg.child_window(width=400, height=400, border=True):
                        self.safe_add_text("HISTORICAL RETURNS", color=self.BLOOMBERG_ORANGE)
                        dpg.add_separator()

                        with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                                       borders_innerV=True, tag="returns_performance_table"):
                            dpg.add_table_column(label="Period", width_fixed=True, init_width_or_weight=80)
                            dpg.add_table_column(label="Return", width_fixed=True, init_width_or_weight=100)
                            dpg.add_table_column(label="Status", width_fixed=True, init_width_or_weight=80)

                            periods = ["1D", "7D", "15D", "30D", "60D", "90D", "180D", "365D"]
                            for i, period in enumerate(periods):
                                with dpg.table_row(tag=f"return_row_{i}"):
                                    self.safe_add_text(period, color=self.BLOOMBERG_YELLOW)
                                    self.safe_add_text("Loading...", tag=f"return_value_{i}",
                                                       color=self.BLOOMBERG_YELLOW)
                                    self.safe_add_text("--", tag=f"return_status_{i}", color=self.BLOOMBERG_GRAY)
        except Exception as e:
            error("Error creating price/returns section", context={'error': str(e)}, exc_info=True)

    def create_financial_statements_section(self):
        """Create financial statements section"""
        try:
            with dpg.collapsing_header(label="FINANCIAL STATEMENTS", default_open=True, tag="financials_header"):
                with dpg.tab_bar(tag="financial_statements_tabs"):
                    # Financial statement tabs
                    tabs = [
                        ("INCOME STATEMENT", "income_statement_container", "income_statement_placeholder"),
                        ("BALANCE SHEET", "balance_sheet_container", "balance_sheet_placeholder"),
                        ("CASH FLOW STATEMENT", "cash_flow_container", "cash_flow_placeholder")
                    ]

                    for tab_label, container_tag, placeholder_tag in tabs:
                        with dpg.tab(label=tab_label):
                            with dpg.child_window(height=400, border=True, horizontal_scrollbar=True,
                                                  tag=container_tag):
                                self.safe_add_text(f"{tab_label} DATA", color=self.BLOOMBERG_ORANGE)
                                dpg.add_separator()
                                self.safe_add_text("Loading AAPL financial data...",
                                                   color=self.BLOOMBERG_YELLOW, tag=placeholder_tag)

        except Exception as e:
            error("Error creating financial statements", context={'error': str(e)}, exc_info=True)

    def create_financial_charts_section(self):
        """Create financial analysis charts section"""
        try:
            with dpg.collapsing_header(label="FINANCIAL ANALYSIS CHARTS", default_open=True,
                                       tag="financial_charts_header"):
                # Chart controls
                with dpg.group(horizontal=True):
                    self.safe_add_text("CHART CONTROLS:", color=self.BLOOMBERG_ORANGE)
                    self.safe_add_text(" | ", color=self.BLOOMBERG_GRAY)
                    self.safe_add_text("YEARS:", color=self.BLOOMBERG_YELLOW)
                    dpg.add_combo(
                        ["Last 2 Years", "Last 3 Years", "Last 4 Years", "All Years"],
                        default_value="Last 4 Years",
                        width=120,
                        tag="chart_years_combo",
                        callback=self.update_financial_charts
                    )
                    dpg.add_button(
                        label="UPDATE CHARTS",
                        callback=self.update_financial_charts,
                        width=120,
                        height=25
                    )

                dpg.add_spacer(height=10)

                # Financial charts with optimized layout
                chart_sections = [
                    [("REVENUE & PROFITABILITY TRENDS", "revenue_chart_container", 600),
                     ("BALANCE SHEET TRENDS", "balance_chart_container", 600)],
                    [("CASH FLOW ANALYSIS", "cashflow_chart_container", 600),
                     ("FINANCIAL RATIOS & MARGINS", "ratios_chart_container", 600)]
                ]

                for row in chart_sections:
                    with dpg.group(horizontal=True):
                        for title, container_tag, width in row:
                            with dpg.child_window(width=width, height=350, border=True):
                                self.safe_add_text(title, color=self.BLOOMBERG_ORANGE)
                                dpg.add_separator()
                                with dpg.group(tag=container_tag):
                                    self.safe_add_text("Loading AAPL financial charts...", color=self.BLOOMBERG_YELLOW)

                    if row == chart_sections[0]:  # Add spacer between rows
                        dpg.add_spacer(height=15)

        except Exception as e:
            error("Error creating financial charts", context={'error': str(e)}, exc_info=True)

    def create_status_bar(self):
        """Create status bar"""
        try:
            dpg.add_separator()
            with dpg.group(horizontal=True):
                status_items = [
                    ("●", "DATA SERVICE ONLINE", self.BLOOMBERG_GREEN, self.BLOOMBERG_GREEN),
                    ("STATUS:", "INITIALIZING", self.BLOOMBERG_YELLOW, self.BLOOMBERG_YELLOW),
                    ("LAST UPDATE:", "LOADING", self.BLOOMBERG_YELLOW, self.BLOOMBERG_WHITE)
                ]

                for i, (label, text, label_color, text_color) in enumerate(status_items):
                    if i > 0:
                        dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                    dpg.add_text(label, color=label_color)

                    tag = None
                    if "STATUS" in text:
                        tag = "main_status_text"
                    elif "LOADING" in text:
                        tag = "last_update_time"

                    dpg.add_text(text, color=text_color, tag=tag)

        except Exception as e:
            error("Error creating status bar", context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def handle_search(self, sender=None, app_data=None):
        """Handle search functionality"""
        with operation("handle_search"):
            try:
                ticker = dpg.get_value("ticker_search_input").strip().upper()

                if not ticker:
                    self.safe_set_value("main_status_text", "ERROR: ENTER TICKER")
                    return

                if self.is_loading:
                    self.safe_set_value("main_status_text", "LOADING IN PROGRESS...")
                    return

                # Check cache first for performance
                cache_key = f"{ticker}_basic_data"
                if cache_key in self._data_cache:
                    cached_time = self._data_cache[cache_key].get('timestamp', 0)
                    # Use cache if data is less than 5 minutes old
                    if time.time() - cached_time < 300:
                        debug("Using cached data", context={'ticker': ticker})
                        self.stock_data = self._data_cache[cache_key]['data']
                        self.update_all_data_sections(self.stock_data)
                        return

                self.is_loading = True
                self.current_ticker = ticker

                self.safe_set_value("main_status_text", f"LOADING {ticker}...")
                dpg.configure_item("search_button", enabled=False)

                info("Stock search initiated", context={'ticker': ticker})
                threading.Thread(target=self.load_complete_stock_data, args=(ticker,), daemon=True).start()

            except Exception as e:
                error("Error handling search", context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def load_complete_stock_data(self, ticker):
        """Load complete stock data including financials with performance optimizations"""
        with operation("load_complete_stock_data", context={'ticker': ticker}):
            try:
                stock = yf.Ticker(ticker)

                # Get basic info
                self.safe_set_value("main_status_text", f"LOADING {ticker} INFO...")
                info_start_time = time.time()
                info = stock.info
                info_load_time = time.time() - info_start_time

                if not info or len(info) < 5:
                    raise Exception("Invalid ticker or no data available")

                self.stock_data = info

                # Cache the data
                cache_key = f"{ticker}_basic_data"
                self._data_cache[cache_key] = {
                    'data': info,
                    'timestamp': time.time()
                }

                # Get historical data
                self.safe_set_value("main_status_text", f"LOADING {ticker} PRICE HISTORY...")
                history_start_time = time.time()
                history = stock.history(period="2y")
                history_load_time = time.time() - history_start_time

                if history.empty:
                    raise Exception("No historical data available")

                self.history_data = history

                # Update UI with basic data
                self.update_all_data_sections(info)

                # Load financial statements
                self.safe_set_value("main_status_text", f"LOADING {ticker} FINANCIALS...")
                financials_start_time = time.time()
                self.load_financial_data(stock)
                financials_load_time = time.time() - financials_start_time

                self.safe_set_value("main_status_text", f"{ticker} LOADED")
                self.safe_set_value("last_update_time", datetime.now().strftime('%H:%M:%S'))
                self._last_update_time = time.time()

                info("Stock data loaded successfully",
                     context={
                         'ticker': ticker,
                         'info_load_time_ms': f"{info_load_time * 1000:.1f}",
                         'history_load_time_ms': f"{history_load_time * 1000:.1f}",
                         'financials_load_time_ms': f"{financials_load_time * 1000:.1f}"
                     })

            except Exception as e:
                error_msg = str(e)[:30] + "..." if len(str(e)) > 30 else str(e)
                self.safe_set_value("main_status_text", f"ERROR: {error_msg}")
                error("Error loading stock data",
                      context={'ticker': ticker, 'error': str(e)}, exc_info=True)

            finally:
                self.is_loading = False
                dpg.configure_item("search_button", enabled=True)

    def update_all_data_sections(self, info):
        """Update all data sections with stock info"""
        with operation("update_all_data_sections"):
            try:
                dpg.show_item("current_stock_display")

                # Update header stock info
                company_name = info.get('shortName', info.get('longName', 'Unknown Company'))
                self.safe_set_value("current_stock_name", f"{self.current_ticker} - {company_name}")

                # Update current price and change
                current_price = info.get('currentPrice', info.get('regularMarketPrice', 0))
                previous_close = info.get('previousClose', info.get('regularMarketPreviousClose', 0))

                if current_price and previous_close:
                    change = current_price - previous_close
                    change_pct = (change / previous_close) * 100 if previous_close != 0 else 0

                    price_color = self.BLOOMBERG_GREEN if change >= 0 else self.BLOOMBERG_RED

                    self.safe_set_value("current_stock_price", f"${current_price:.2f}")
                    dpg.configure_item("current_stock_price", color=price_color)

                    change_text = f"{change:+.2f} ({change_pct:+.2f}%)"
                    self.safe_set_value("current_stock_change", change_text)
                    dpg.configure_item("current_stock_change", color=price_color)

                # Update company information
                self.update_company_information(info)
                # Update market data
                self.update_market_data_info(info)
                # Update price chart and returns
                self.update_price_chart()
                self.update_returns_table()

                debug("All data sections updated successfully")

            except Exception as e:
                error("Error updating data sections", context={'error': str(e)}, exc_info=True)

    def update_company_information(self, info):
        """Update company information section with optimized formatting"""
        try:
            # Helper function for safe formatting
            def safe_format(value, format_type='str'):
                try:
                    if value is None or pd.isna(value):
                        return 'N/A'
                    if format_type == 'int':
                        return f"{int(value):,}"
                    elif format_type == 'currency':
                        return f"${float(value):,.0f}" if value != 0 else "$0"
                    elif format_type == 'percentage':
                        return f"{float(value) * 100:.2f}%" if value != 0 else "0.00%"
                    elif format_type == 'float':
                        return f"{float(value):.2f}" if value != 0 else "0.00"
                    else:
                        return str(value)
                except:
                    return 'N/A'

            # Company Information
            employees = safe_format(info.get('fullTimeEmployees'), 'int')

            company_info_items = [
                ("Company", info.get('longName', 'N/A')),
                ("Sector", info.get('sector', 'N/A')),
                ("Industry", info.get('industry', 'N/A')),
                ("Country", info.get('country', 'N/A')),
                ("Website", info.get('website', 'N/A')),
                ("Phone", info.get('phone', 'N/A')),
                ("Employees", employees),
                ("Exchange", info.get('exchange', 'N/A')),
                ("Currency", info.get('currency', 'N/A')),
                ("Address", f"{info.get('address1', 'N/A')}, {info.get('city', 'N/A')}")
            ]

            company_info = '\n'.join(f"{label}: {value}" for label, value in company_info_items)

            self.safe_set_value("company_info_text", company_info)
            dpg.configure_item("company_info_text", color=[173, 216, 230])

            # Business Summary
            business_summary = info.get('longBusinessSummary', 'No business summary available.')
            if len(business_summary) > 1200:
                business_summary = business_summary[:1200] + "..."

            self.safe_set_value("business_summary_text", business_summary)
            dpg.configure_item("business_summary_text", color=[144, 238, 144])

            # Key Metrics
            key_metrics_items = [
                ("Market Cap", safe_format(info.get('marketCap'), 'currency')),
                ("Enterprise Value", safe_format(info.get('enterpriseValue'), 'currency')),
                ("P/E Ratio", safe_format(info.get('trailingPE'), 'float')),
                ("Forward P/E", safe_format(info.get('forwardPE'), 'float')),
                ("PEG Ratio", safe_format(info.get('pegRatio'), 'float')),
                ("Price/Sales", safe_format(info.get('priceToSalesTrailing12Months'), 'float')),
                ("Price/Book", safe_format(info.get('priceToBook'), 'float')),
                ("Beta", safe_format(info.get('beta'), 'float')),
                ("ROE", safe_format(info.get('returnOnEquity'), 'percentage')),
                ("ROA", safe_format(info.get('returnOnAssets'), 'percentage')),
                ("Debt/Equity", safe_format(info.get('debtToEquity'), 'float')),
                ("Profit Margin", safe_format(info.get('profitMargins'), 'percentage'))
            ]

            key_metrics = '\n'.join(f"{label}: {value}" for label, value in key_metrics_items)

            self.safe_set_value("key_metrics_text", key_metrics)
            dpg.configure_item("key_metrics_text", color=[240, 128, 128])

        except Exception as e:
            error("Error updating company information", context={'error': str(e)}, exc_info=True)

    def update_market_data_info(self, info):
        """Update market data information with optimized formatting"""
        try:
            def safe_format_value(value, is_currency=True, is_percentage=False):
                try:
                    if value is None or pd.isna(value):
                        return "N/A"
                    if is_percentage:
                        return f"{float(value) * 100:.2f}%"
                    elif is_currency:
                        return f"${float(value):,.2f}"
                    else:
                        return f"{float(value):,}"
                except:
                    return "N/A"

            # Price Data
            price_data_items = [
                ("Current Price", safe_format_value(info.get('currentPrice'))),
                ("Previous Close", safe_format_value(info.get('previousClose'))),
                ("Open", safe_format_value(info.get('open'))),
                ("Day Low", safe_format_value(info.get('dayLow'))),
                ("Day High", safe_format_value(info.get('dayHigh'))),
                ("52W Low", safe_format_value(info.get('fiftyTwoWeekLow'))),
                ("52W High", safe_format_value(info.get('fiftyTwoWeekHigh'))),
                ("Volume", safe_format_value(info.get('volume'), False)),
                ("Avg Volume", safe_format_value(info.get('averageVolume'), False))
            ]

            price_data = '\n'.join(f"{label}: {value}" for label, value in price_data_items)
            self.safe_set_value("price_data_text", price_data)
            dpg.configure_item("price_data_text", color=[224, 255, 255])

            # Trading Metrics
            trading_metrics_items = [
                ("Market Cap", safe_format_value(info.get('marketCap'))),
                ("Beta", safe_format_value(info.get('beta'))),
                ("Dividend Rate", safe_format_value(info.get('dividendRate'))),
                ("Dividend Yield", safe_format_value(info.get('dividendYield'), False, True)),
                ("Shares Outstanding", safe_format_value(info.get('sharesOutstanding'), False)),
                ("Float Shares", safe_format_value(info.get('floatShares'), False))
            ]

            trading_metrics = '\n'.join(f"{label}: {value}" for label, value in trading_metrics_items)
            self.safe_set_value("trading_metrics_text", trading_metrics)
            dpg.configure_item("trading_metrics_text", color=[255, 255, 224])

            # Valuation Metrics
            valuation_metrics_items = [
                ("P/E Ratio (TTM)", safe_format_value(info.get('trailingPE'), False)),
                ("Forward P/E", safe_format_value(info.get('forwardPE'), False)),
                ("PEG Ratio", safe_format_value(info.get('pegRatio'), False)),
                ("Price/Sales (TTM)", safe_format_value(info.get('priceToSalesTrailing12Months'), False)),
                ("Price/Book", safe_format_value(info.get('priceToBook'), False)),
                ("Book Value", safe_format_value(info.get('bookValue'))),
                ("Target Price", safe_format_value(info.get('targetMeanPrice')))
            ]

            valuation_metrics = '\n'.join(f"{label}: {value}" for label, value in valuation_metrics_items)
            self.safe_set_value("valuation_metrics_text", valuation_metrics)
            dpg.configure_item("valuation_metrics_text", color=[255, 182, 193])

            # Executive Team
            executives = info.get('companyOfficers', [])
            if executives:
                exec_text = ""
                for i, exec in enumerate(executives[:6]):
                    name = exec.get('name', 'Unknown')
                    title = exec.get('title', 'Unknown Title')
                    age = exec.get('age', '')
                    total_pay = exec.get('totalPay', 0)

                    exec_text += f"• {name}"
                    if age:
                        exec_text += f" (Age: {age})"
                    exec_text += f"\n  {title}"
                    if total_pay and total_pay > 0:
                        exec_text += f"\n  Compensation: ${total_pay:,.0f}"
                    exec_text += "\n\n"

                self.safe_set_value("executives_text", exec_text)
                dpg.configure_item("executives_text", color=self.BLOOMBERG_YELLOW)
            else:
                self.safe_set_value("executives_text", "No executive information available.")
                dpg.configure_item("executives_text", color=self.BLOOMBERG_YELLOW)

        except Exception as e:
            error("Error updating market data", context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def update_price_chart(self, sender=None, app_data=None):
        """Update price chart with historical data and caching"""
        with operation("update_price_chart"):
            try:
                if self.history_data is None or self.history_data.empty:
                    return

                # Clear existing chart container
                if dpg.does_item_exist("chart_container"):
                    dpg.delete_item("chart_container", children_only=True)

                    # Get selected period and chart type
                    period = dpg.get_value("chart_period_combo") if dpg.does_item_exist("chart_period_combo") else "3M"
                    chart_type = dpg.get_value("chart_type_combo") if dpg.does_item_exist(
                        "chart_type_combo") else "Candlestick"

                    # Check cache for chart data
                    cache_key = f"{self.current_ticker}_{period}_{chart_type}_chart"
                    if cache_key in self._chart_cache:
                        cached_time = self._chart_cache[cache_key].get('timestamp', 0)
                        if time.time() - cached_time < 300:  # 5 minute cache
                            debug("Using cached chart data", context={'ticker': self.current_ticker, 'period': period})

                    # Filter data based on period
                    period_mapping = {
                        "1D": 1, "5D": 5, "1M": 22, "3M": 66,
                        "6M": 132, "1Y": 252, "2Y": len(self.history_data)
                    }

                    data = self.history_data.tail(period_mapping.get(period, 66))

                    if data.empty:
                        self.safe_add_text("No data available for selected period",
                                           color=self.BLOOMBERG_RED, parent="chart_container")
                        return

                    # Create new plot
                    with dpg.plot(height=350, width=-1, parent="chart_container", tag="main_price_chart"):
                        dpg.add_plot_legend()
                        x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                        y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Price ($)")

                        if chart_type == "Candlestick":
                            # Prepare data for candlestick chart
                            dates = list(range(len(data)))
                            opens = data['Open'].tolist()
                            highs = data['High'].tolist()
                            lows = data['Low'].tolist()
                            closes = data['Close'].tolist()

                            # Add candlestick series
                            dpg.add_candle_series(
                                dates, opens, closes, lows, highs,
                                label="OHLC",
                                parent=y_axis,
                                bull_color=[0, 255, 0, 255],
                                bear_color=[255, 0, 0, 255],
                                weight=0.75,
                                tooltip=True
                            )

                        else:
                            # Simple line chart with moving averages
                            dates = list(range(len(data)))
                            closes = data['Close'].tolist()

                            # Add moving averages for better analysis
                            if len(closes) >= 20:
                                ma20 = pd.Series(closes).rolling(window=20, min_periods=1).mean().tolist()
                                dpg.add_line_series(dates, ma20, label="MA20", parent=y_axis)

                            if len(closes) >= 50:
                                ma50 = pd.Series(closes).rolling(window=50, min_periods=1).mean().tolist()
                                dpg.add_line_series(dates, ma50, label="MA50", parent=y_axis)

                            # Main price line
                            dpg.add_line_series(dates, closes, label="Close Price", parent=y_axis)

                        # Set axis limits with optimization
                        if not data.empty:
                            price_min = min(data['Low'].min(), data['Close'].min()) * 0.98
                            price_max = max(data['High'].max(), data['Close'].max()) * 1.02
                            dpg.set_axis_limits(y_axis, price_min, price_max)

                        # Cache the chart data
                        self._chart_cache[cache_key] = {
                            'timestamp': time.time(),
                            'data': data.copy()
                        }

                debug("Price chart updated successfully", context={'period': period, 'chart_type': chart_type})

            except Exception as e:
                error("Error updating price chart", context={'error': str(e)}, exc_info=True)
                if dpg.does_item_exist("chart_container"):
                    dpg.delete_item("chart_container", children_only=True)
                    self.safe_add_text(f"Chart Error: {str(e)[:50]}...",
                                       color=self.BLOOMBERG_RED, parent="chart_container")

    def update_returns_table(self):
        """Update historical returns table with performance optimization"""
        try:
            if self.history_data is None or self.history_data.empty:
                return

            current_price = self.history_data['Close'].iloc[-1]
            periods = [1, 7, 15, 30, 60, 90, 180, 365]

            for i, days in enumerate(periods):
                try:
                    if len(self.history_data) > days:
                        past_price = self.history_data['Close'].iloc[-(days + 1)]
                        return_pct = ((current_price - past_price) / past_price) * 100

                        if return_pct > 0:
                            color = self.BLOOMBERG_GREEN
                            status = "▲"
                        elif return_pct < 0:
                            color = self.BLOOMBERG_RED
                            status = "▼"
                        else:
                            color = self.BLOOMBERG_WHITE
                            status = "="

                        self.safe_set_value(f"return_value_{i}", f"{return_pct:+.2f}%")
                        dpg.configure_item(f"return_value_{i}", color=color)
                        self.safe_set_value(f"return_status_{i}", status)
                        dpg.configure_item(f"return_status_{i}", color=color)
                    else:
                        self.safe_set_value(f"return_value_{i}", "N/A")
                        dpg.configure_item(f"return_value_{i}", color=self.BLOOMBERG_GRAY)
                        self.safe_set_value(f"return_status_{i}", "--")
                        dpg.configure_item(f"return_status_{i}", color=self.BLOOMBERG_GRAY)
                except Exception as e:
                    self.safe_set_value(f"return_value_{i}", "Error")
                    dpg.configure_item(f"return_value_{i}", color=self.BLOOMBERG_RED)

        except Exception as e:
            error("Error updating returns table", context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def load_financial_data(self, stock):
        """Load and display financial statements with performance optimization"""
        with operation("load_financial_data"):
            try:
                # Get financial statements with error handling
                financials_mapping = {
                    'income_statement': ('financials', self.create_financial_table, "income_statement_container",
                                         "Income Statement"),
                    'balance_sheet': ('balance_sheet', self.create_financial_table, "balance_sheet_container",
                                      "Balance Sheet"),
                    'cash_flow': ('cashflow', self.create_financial_table, "cash_flow_container", "Cash Flow Statement")
                }

                for key, (attr_name, create_func, container, title) in financials_mapping.items():
                    try:
                        df = getattr(stock, attr_name)
                        self.financials_data[key] = df
                        create_func(df, container, title)
                    except Exception as e:
                        warning(f"Could not load {title}", context={'error': str(e)})
                        placeholder_tag = f"{container.replace('_container', '_placeholder')}"
                        self.safe_set_value(placeholder_tag, f"Error loading {title.lower()}: {str(e)[:50]}...")

                # Update financial charts
                self.update_financial_charts()

                debug("Financial data loaded successfully",
                      context={'statements_loaded': len([k for k, v in self.financials_data.items() if v is not None])})

            except Exception as e:
                error("Error loading financial data", context={'error': str(e)}, exc_info=True)
                # Update placeholders with error messages
                error_placeholders = [
                    "income_statement_placeholder",
                    "balance_sheet_placeholder",
                    "cash_flow_placeholder"
                ]
                for placeholder in error_placeholders:
                    self.safe_set_value(placeholder, f"Error loading financial data: {str(e)[:50]}...")

    def create_financial_table(self, df, container_tag, title):
        """Create financial statement table with performance optimization"""
        try:
            if df is None or df.empty:
                return

            # Clear container
            dpg.delete_item(container_tag, children_only=True)

            # Add title and separator
            self.safe_add_text(f"{title.upper()} DATA", color=self.BLOOMBERG_ORANGE, parent=container_tag)
            dpg.add_separator(parent=container_tag)

            # Create table with optimized structure
            with dpg.table(parent=container_tag, header_row=True, borders_innerH=True,
                           borders_outerH=True, borders_innerV=True, borders_outerV=True,
                           scrollY=True, scrollX=True, height=330):

                # Add columns
                dpg.add_table_column(label="Line Item", width_fixed=True, init_width_or_weight=250)

                # Add year columns (limit to 4 most recent years for performance)
                years = df.columns[:4] if len(df.columns) > 4 else df.columns
                for year in years:
                    year_str = year.strftime('%Y') if hasattr(year, 'strftime') else str(year)[:4]
                    dpg.add_table_column(label=year_str, width_fixed=True, init_width_or_weight=130)

                # Add data rows (limit to top 20 items for performance)
                for idx in df.index[:20]:
                    with dpg.table_row():
                        # Line item name
                        self.safe_add_text(str(idx), color=self.BLOOMBERG_YELLOW)

                        # Values for each year with optimized formatting
                        for year in years:
                            try:
                                value = df.loc[idx, year]
                                if pd.isna(value):
                                    self.safe_add_text("N/A", color=self.BLOOMBERG_GRAY)
                                else:
                                    formatted_value = self._format_financial_value(value)
                                    self.safe_add_text(formatted_value, color=self.BLOOMBERG_WHITE)
                            except Exception:
                                self.safe_add_text("Error", color=self.BLOOMBERG_RED)

        except Exception as e:
            error("Error creating financial table", context={'title': title, 'error': str(e)}, exc_info=True)

    def _format_financial_value(self, value):
        """Efficiently format financial values"""
        try:
            abs_value = abs(value)
            if abs_value >= 1e12:
                return f"${value / 1e12:.2f}T"
            elif abs_value >= 1e9:
                return f"${value / 1e9:.2f}B"
            elif abs_value >= 1e6:
                return f"${value / 1e6:.2f}M"
            elif abs_value >= 1e3:
                return f"${value / 1e3:.2f}K"
            else:
                return f"${value:.2f}"
        except:
            return "N/A"

    @monitor_performance
    def update_financial_charts(self, sender=None, app_data=None):
        """Update all financial analysis charts with performance optimization"""
        with operation("update_financial_charts"):
            try:
                if not self.financials_data:
                    return

                # Get years selection
                years_selection = dpg.get_value("chart_years_combo") if dpg.does_item_exist(
                    "chart_years_combo") else "Last 4 Years"

                # Update each chart with error handling
                chart_updates = [
                    ("revenue_profitability", self.create_revenue_profitability_chart),
                    ("balance_sheet_trends", self.create_balance_sheet_trends_chart),
                    ("cash_flow_analysis", self.create_cash_flow_analysis_chart),
                    ("financial_ratios", self.create_financial_ratios_chart)
                ]

                for chart_name, update_func in chart_updates:
                    try:
                        update_func(years_selection)
                    except Exception as e:
                        warning(f"Could not update {chart_name} chart", context={'error': str(e)})

                debug("Financial charts updated", context={'years_selection': years_selection})

            except Exception as e:
                error("Error updating financial charts", context={'error': str(e)}, exc_info=True)

    def create_revenue_profitability_chart(self, years_selection):
        """Create revenue and profitability trends chart"""
        try:
            income_stmt = self.financials_data.get('income_statement')
            if income_stmt is None or income_stmt.empty:
                return

            # Clear existing chart
            dpg.delete_item("revenue_chart_container", children_only=True)

            # Determine number of years
            years_mapping = {
                "Last 2 Years": 2, "Last 3 Years": 3, "Last 4 Years": 4
            }
            num_years = years_mapping.get(years_selection, len(income_stmt.columns))
            years_cols = income_stmt.columns[:num_years]

            with dpg.plot(height=300, width=-1, parent="revenue_chart_container"):
                dpg.add_plot_legend()
                x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Year")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Amount (Billions)")

                # Revenue and Net Income data with optimization
                data_series = [
                    (['Total Revenue', 'Revenue', 'Net Sales', 'Total Revenues'], "Revenue (B)"),
                    (['Net Income', 'Net Income Common Stockholders'], "Net Income (B)")
                ]

                for items_list, label in data_series:
                    for revenue_item in items_list:
                        if revenue_item in income_stmt.index:
                            self._add_financial_series(income_stmt, revenue_item, years_cols, label, y_axis)
                            break

        except Exception as e:
            error("Error creating revenue chart", context={'error': str(e)}, exc_info=True)

    def _add_financial_series(self, df, item_name, years_cols, label, y_axis):
        """Helper method to add financial data series to chart"""
        try:
            item_data = df.loc[item_name]
            years = []
            values = []

            for year in years_cols:
                if year in item_data.index:
                    value = item_data[year]
                    if pd.notna(value) and value != 0:
                        year_num = year.year if hasattr(year, 'year') else int(str(year)[:4])
                        years.append(year_num)
                        values.append(value / 1e9)  # Convert to billions

            if years and values:
                sorted_data = sorted(zip(years, values))
                years, values = zip(*sorted_data)
                dpg.add_line_series(list(years), list(values), label=label, parent=y_axis)
        except Exception as e:
            debug("Could not add financial series", context={'item': item_name, 'error': str(e)})

    def create_balance_sheet_trends_chart(self, years_selection):
        """Create balance sheet trends chart"""
        try:
            balance_sheet = self.financials_data.get('balance_sheet')
            if balance_sheet is None or balance_sheet.empty:
                return

            dpg.delete_item("balance_chart_container", children_only=True)

            years_mapping = {"Last 2 Years": 2, "Last 3 Years": 3, "Last 4 Years": 4}
            num_years = years_mapping.get(years_selection, len(balance_sheet.columns))
            years_cols = balance_sheet.columns[:num_years]

            with dpg.plot(height=300, width=-1, parent="balance_chart_container"):
                dpg.add_plot_legend()
                x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Year")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Amount (Billions)")

                bs_items = [
                    ('Total Assets', 'Total Assets (B)'),
                    ('Total Stockholder Equity', 'Stockholder Equity (B)'),
                    ('Total Debt', 'Total Debt (B)')
                ]

                for item_name, label in bs_items:
                    if item_name in balance_sheet.index:
                        self._add_financial_series(balance_sheet, item_name, years_cols, label, y_axis)

        except Exception as e:
            error("Error creating balance sheet chart", context={'error': str(e)}, exc_info=True)

    def create_cash_flow_analysis_chart(self, years_selection):
        """Create cash flow analysis chart"""
        try:
            cash_flow = self.financials_data.get('cash_flow')
            if cash_flow is None or cash_flow.empty:
                return

            dpg.delete_item("cashflow_chart_container", children_only=True)

            years_mapping = {"Last 2 Years": 2, "Last 3 Years": 3, "Last 4 Years": 4}
            num_years = years_mapping.get(years_selection, len(cash_flow.columns))
            years_cols = cash_flow.columns[:num_years]

            with dpg.plot(height=300, width=-1, parent="cashflow_chart_container"):
                dpg.add_plot_legend()
                x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Year")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Cash Flow (Billions)")

                cf_items = [
                    ('Operating Cash Flow', 'Operating CF (B)'),
                    ('Free Cash Flow', 'Free CF (B)'),
                    ('Investing Cash Flow', 'Investing CF (B)'),
                    ('Financing Cash Flow', 'Financing CF (B)')
                ]

                for item_name, label in cf_items:
                    if item_name in cash_flow.index:
                        self._add_financial_series(cash_flow, item_name, years_cols, label, y_axis)

        except Exception as e:
            error("Error creating cash flow chart", context={'error': str(e)}, exc_info=True)

    def create_financial_ratios_chart(self, years_selection):
        """Create financial ratios and margins chart"""
        try:
            income_stmt = self.financials_data.get('income_statement')
            balance_sheet = self.financials_data.get('balance_sheet')

            if income_stmt is None or balance_sheet is None or income_stmt.empty or balance_sheet.empty:
                return

            dpg.delete_item("ratios_chart_container", children_only=True)

            years_mapping = {"Last 2 Years": 2, "Last 3 Years": 3, "Last 4 Years": 4}
            num_years = years_mapping.get(years_selection, min(len(income_stmt.columns), len(balance_sheet.columns)))

            income_years = income_stmt.columns[:num_years]
            balance_years = balance_sheet.columns[:num_years]

            with dpg.plot(height=300, width=-1, parent="ratios_chart_container"):
                dpg.add_plot_legend()
                x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Year")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Percentage (%)")

                # Calculate ROE and Profit Margin with optimization
                self._calculate_and_plot_roe(income_stmt, balance_sheet, income_years, balance_years, y_axis)
                self._calculate_and_plot_profit_margin(income_stmt, income_years, y_axis)

        except Exception as e:
            error("Error creating ratios chart", context={'error': str(e)}, exc_info=True)

    def _calculate_and_plot_roe(self, income_stmt, balance_sheet, income_years, balance_years, y_axis):
        """Calculate and plot Return on Equity"""
        try:
            net_income_items = ['Net Income', 'Net Income Common Stockholders']
            equity_items = ['Total Stockholder Equity', 'Stockholder Equity']

            net_income_data = None
            equity_data = None

            for item in net_income_items:
                if item in income_stmt.index:
                    net_income_data = income_stmt.loc[item]
                    break

            for item in equity_items:
                if item in balance_sheet.index:
                    equity_data = balance_sheet.loc[item]
                    break

            if net_income_data is not None and equity_data is not None:
                years = []
                roe_values = []

                for year in income_years:
                    if year in balance_years:
                        ni = net_income_data[year] if year in net_income_data.index else None
                        eq = equity_data[year] if year in equity_data.index else None

                        if pd.notna(ni) and pd.notna(eq) and eq != 0:
                            roe = (ni / eq) * 100
                            year_num = year.year if hasattr(year, 'year') else int(str(year)[:4])
                            years.append(year_num)
                            roe_values.append(roe)

                if years and roe_values:
                    sorted_data = sorted(zip(years, roe_values))
                    years, roe_values = zip(*sorted_data)
                    dpg.add_line_series(list(years), list(roe_values), label="ROE (%)", parent=y_axis)
        except Exception as e:
            debug("Could not calculate ROE", context={'error': str(e)})

    def _calculate_and_plot_profit_margin(self, income_stmt, income_years, y_axis):
        """Calculate and plot Profit Margin"""
        try:
            net_income_items = ['Net Income', 'Net Income Common Stockholders']
            revenue_items = ['Total Revenue', 'Revenue', 'Net Sales']

            net_income_data = None
            revenue_data = None

            for item in net_income_items:
                if item in income_stmt.index:
                    net_income_data = income_stmt.loc[item]
                    break

            for item in revenue_items:
                if item in income_stmt.index:
                    revenue_data = income_stmt.loc[item]
                    break

            if net_income_data is not None and revenue_data is not None:
                years = []
                margin_values = []

                for year in income_years:
                    ni = net_income_data[year] if year in net_income_data.index else None
                    rev = revenue_data[year] if year in revenue_data.index else None

                    if pd.notna(ni) and pd.notna(rev) and rev != 0:
                        margin = (ni / rev) * 100
                        year_num = year.year if hasattr(year, 'year') else int(str(year)[:4])
                        years.append(year_num)
                        margin_values.append(margin)

                if years and margin_values:
                    sorted_data = sorted(zip(years, margin_values))
                    years, margin_values = zip(*sorted_data)
                    dpg.add_line_series(list(years), list(margin_values), label="Profit Margin (%)", parent=y_axis)
        except Exception as e:
            debug("Could not calculate profit margin", context={'error': str(e)})

    def start_time_updater(self):
        """Start background time updater"""

        def time_updater():
            while True:
                try:
                    current_time = datetime.now().strftime('%H:%M:%S')
                    if dpg.does_item_exist("header_time"):
                        dpg.set_value("header_time", current_time)
                    time.sleep(1)
                except:
                    break

        threading.Thread(target=time_updater, daemon=True).start()

    @monitor_performance
    def cleanup(self):
        """Cleanup resources with performance optimization"""
        with operation("yfinance_tab_cleanup"):
            try:
                # Clear caches
                self._data_cache.clear()
                self._chart_cache.clear()

                # Clear data structures
                self.financials_data.clear()
                if hasattr(self, 'history_data') and self.history_data is not None:
                    del self.history_data
                if hasattr(self, 'stock_data') and self.stock_data is not None:
                    del self.stock_data

                info("YFinance Data Tab cleanup completed",
                     context={'caches_cleared': 2, 'data_structures_cleared': 3})
            except Exception as e:
                error("Error during cleanup", context={'error': str(e)}, exc_info=True)