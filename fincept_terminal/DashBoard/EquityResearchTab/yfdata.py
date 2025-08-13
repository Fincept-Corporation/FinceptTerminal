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
from pathlib import Path
import csv
import os

from fincept_terminal.Utils.Logging.logger import (
    info as log_info, debug, warning, error, operation, monitor_performance
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
        self.BLOOMBERG_ORANGE = (255, 165, 0)
        self.BLOOMBERG_WHITE = (255, 255, 255)
        self.BLOOMBERG_RED = (255, 0, 0)
        self.BLOOMBERG_GREEN = (0, 200, 0)
        self.BLOOMBERG_YELLOW = (255, 255, 0)
        self.BLOOMBERG_GRAY = (120, 120, 120)
        self.BLOOMBERG_BLUE = (0, 128, 255)

        # Performance optimization caches
        self._data_cache = {}
        self._chart_cache = {}
        self._search_cache = {}  # Add search cache for faster lookups
        self._last_update_time = None

        self.dropdown_items = []
        self.dropdown_created = False
        self.search_timer = None
        self.equities_data = None
        self.equities_df = None  # Store as DataFrame for fast searching
        self.finance_db_available = False

        self.initialize_equities_database()

        # Initialize theme manager
        try:
            self.theme_manager = AutomaticThemeManager()
            debug("Theme manager initialized successfully")
        except Exception as e:
            warning("Theme manager initialization failed", context={'error': str(e)})
            self.theme_manager = None

        log_info("YFinanceDataTab initialized")

    def get_label(self):
        return "Equity Research"

    def get_config_directory(self) -> Path:
        """Get platform-specific config directory - uses .fincept folder"""
        # Use .fincept folder at home directory for all platforms
        config_dir = Path.home() / '.fincept' / 'cache'
        # Ensure directory exists
        config_dir.mkdir(parents=True, exist_ok=True)
        return config_dir

    def initialize_equities_database(self):
        """Initialize the global equities database with proper DataFrame handling"""
        try:
            try:
                import financedatabase as fd
                log_info("Loading complete equities database...")
                equities = fd.Equities()

                # Get the data and ensure it's a DataFrame
                equity_data = equities.select(exclude_exchanges=False)

                if isinstance(equity_data, dict):
                    # Convert dict to DataFrame if necessary
                    self.equities_df = pd.DataFrame.from_dict(equity_data, orient='index')
                elif isinstance(equity_data, pd.DataFrame):
                    self.equities_df = equity_data
                else:
                    # Fallback - try to convert to DataFrame
                    self.equities_df = pd.DataFrame(equity_data)

                # Store both formats for compatibility
                self.equities_data = equity_data
                self.finance_db_available = True

                # Create search index for faster lookups
                self._create_search_index()

                log_info(
                    f"✅ Loaded {len(self.equities_df)} equities from {self.equities_df['exchange'].nunique() if 'exchange' in self.equities_df.columns else 'multiple'} exchanges")

            except ImportError:
                warning("financedatabase not available. Install with: pip install financedatabase")
                self.finance_db_available = False
                self.equities_data = None
                self.equities_df = None
            except Exception as e:
                error("Error initializing equities database", context={'error': str(e)}, exc_info=True)
                self.finance_db_available = False
                self.equities_data = None
                self.equities_df = None

        except Exception as outer_e:
            error("Outer error in initialize_equities_database", context={'error': str(outer_e)}, exc_info=True)
            self.finance_db_available = False
            self.equities_data = None
            self.equities_df = None

    def _create_search_index(self):
        """Create optimized search index for faster lookups"""
        try:
            if self.equities_df is None or self.equities_df.empty:
                return

            # Create lowercase search columns for faster searching
            search_data = []
            for idx, row in self.equities_df.iterrows():
                symbol = str(idx).upper()
                name = str(row.get('name', '')).lower()
                sector = str(row.get('sector', 'N/A'))
                country = str(row.get('country', 'N/A'))
                exchange = str(row.get('exchange', 'N/A'))

                search_entry = {
                    'symbol': symbol,
                    'symbol_lower': symbol.lower(),
                    'name_lower': name,
                    'display_name': str(row.get('name', 'N/A'))[:35],
                    'sector': sector[:15],
                    'country': country[:10],
                    'exchange': exchange[:10]
                }
                search_data.append(search_entry)

            self._search_index = search_data
            debug(f"Search index created with {len(search_data)} entries")

        except Exception as e:
            error("Error creating search index", context={'error': str(e)}, exc_info=True)
            self._search_index = []

    def load_ticker_from_external(self, ticker: str):
        """Load ticker data when called from external source (like watchlist) - MINIMAL IMPLEMENTATION"""
        try:
            with operation("load_ticker_from_external", context={'ticker': ticker}):
                log_info("Loading ticker from external source", context={'ticker': ticker})

                # Update the search input if it exists
                if dpg.does_item_exist("ticker_search_input"):
                    dpg.set_value("ticker_search_input", ticker)

                # Update status to show external source
                self.safe_set_value("search_status_text", f"🔄 Loading {ticker} from Watchlist...")

                # Set current ticker and trigger data loading
                self.current_ticker = ticker.upper()

                # Load data in background thread
                threading.Thread(
                    target=self._load_external_ticker_data,
                    args=(ticker.upper(),),
                    daemon=True
                ).start()

        except Exception as e:
            error("Error loading ticker from external source",
                  context={'ticker': ticker, 'error': str(e)}, exc_info=True)

    def _load_external_ticker_data(self, ticker: str):
        """Helper method to load ticker data from external source"""
        try:
            # Set loading state
            self.is_loading = True
            self.safe_configure_item("search_button", enabled=False)

            # Load the complete stock data
            self.load_complete_stock_data(ticker)

            # Update status to show it came from external source
            current_status = dpg.get_value("search_status_text") if dpg.does_item_exist("search_status_text") else ""
            if "✅" in current_status and "loaded successfully" in current_status:
                self.safe_set_value("search_status_text", current_status + " (from Watchlist)")

        except Exception as e:
            error("Error in external ticker data loading",
                  context={'ticker': ticker, 'error': str(e)}, exc_info=True)
        finally:
            self.is_loading = False
            self.safe_configure_item("search_button", enabled=True)

    def safe_add_text(self, text, **kwargs):
        """Safely add text with encoding error handling and performance optimization"""
        try:
            # Ensure text is a string and handle encoding efficiently
            if text is None:
                text = "N/A"
            elif not isinstance(text, str):
                text = str(text)

            # Clean the text more efficiently
            text = text.encode('ascii', 'ignore').decode('ascii')

            # Replace common problematic characters
            replacements = {
                '…': '...',
                '–': '-',
                '—': '-',
                '"': '"',
                '"': '"',
                ''': "'",
                ''': "'",
                '€': 'EUR',
                '£': 'GBP',
                '¥': 'JPY'
            }

            for old, new in replacements.items():
                text = text.replace(old, new)

            if not text.strip():
                text = "N/A"

            # FIX: Ensure color parameter is properly formatted
            if 'color' in kwargs:
                color = kwargs['color']
                # Convert list to tuple
                if isinstance(color, list):
                    if len(color) >= 3:
                        kwargs['color'] = (int(color[0]), int(color[1]), int(color[2]))
                    else:
                        kwargs['color'] = (255, 255, 255)  # Default white
                elif isinstance(color, tuple):
                    if len(color) >= 3:
                        kwargs['color'] = (int(color[0]), int(color[1]), int(color[2]))
                    else:
                        kwargs['color'] = (255, 255, 255)  # Default white
                elif isinstance(color, (int, float)):
                    # Handle single values that should be tuples
                    val = int(color)
                    kwargs['color'] = (val, val, val)
                else:
                    # Fallback to white if color format is unknown
                    kwargs['color'] = (255, 255, 255)

            # FIX: Handle tag conflicts by checking if item exists first
            if 'tag' in kwargs:
                tag = kwargs['tag']
                # Generate unique tag if it already exists
                if dpg.does_item_exist(tag):
                    import time
                    kwargs['tag'] = f"{tag}_{int(time.time() * 1000000) % 1000000}"

            return dpg.add_text(text, **kwargs)

        except Exception as e:
            error("Text add error", context={'error': str(e), 'text_preview': str(text)[:50] if text else "None"},
                  exc_info=True)
            try:
                # Fallback with minimal text and safe color
                fallback_kwargs = {k: v for k, v in kwargs.items() if k not in ['color']}
                return dpg.add_text("Data Error", color=(255, 0, 0), **fallback_kwargs)
            except Exception as fallback_error:
                error("Fallback text add failed", context={'error': str(fallback_error)})
                return None

    def safe_configure_item(self, tag, **kwargs):
        """Safely configure item with error handling"""
        try:
            if dpg.does_item_exist(tag):
                # Fix color parameters if present
                if 'color' in kwargs:
                    color = kwargs['color']
                    if isinstance(color, list):
                        if len(color) >= 3:
                            kwargs['color'] = (int(color[0]), int(color[1]), int(color[2]))
                        else:
                            kwargs['color'] = (255, 255, 255)
                    elif isinstance(color, tuple):
                        if len(color) >= 3:
                            kwargs['color'] = (int(color[0]), int(color[1]), int(color[2]))
                        else:
                            kwargs['color'] = (255, 255, 255)

                dpg.configure_item(tag, **kwargs)
            else:
                debug(f"Item {tag} does not exist for configuration")
        except Exception as e:
            debug(f"Error configuring item {tag}", context={'error': str(e)})

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
            else:
                debug(f"Tag {tag} does not exist for value setting")
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

                log_info("YFinance content created successfully")

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
        """Create header with enhanced search functionality"""
        try:
            import time
            unique_suffix = int(time.time() * 1000) % 1000

            with dpg.group(horizontal=True):
                self.safe_add_text("FINCEPT", color=(255, 165, 0), tag=f"fincept_label_{unique_suffix}")
                self.safe_add_text("MARKET DATA", color=(255, 255, 255), tag=f"market_data_label_{unique_suffix}")
                self.safe_add_text(" | ", color=(120, 120, 120))

                self.safe_add_text("TICKER:", color=(255, 255, 0), tag=f"ticker_label_{unique_suffix}")

                # Enhanced search input with real-time search
                dpg.add_input_text(
                    width=200,
                    hint="Search global equities...",
                    tag="ticker_search_input",
                    callback=self.search_callback,
                    on_enter=True,  # Allow enter key to trigger search
                    default_value="AAPL"
                )

                # Add the search button back to fix the error
                dpg.add_button(
                    label="SEARCH",
                    callback=self.handle_search,
                    width=70,
                    height=25,
                    tag="search_button"
                )

                dpg.add_button(
                    label="CLEAR",
                    callback=self.clear_search,
                    width=60,
                    height=25,
                    tag=f"clear_button_{unique_suffix}"
                )

                self.safe_add_text(" | ", color=(120, 120, 120))

                # Database status indicator
                if self.finance_db_available and self.equities_df is not None:
                    self.safe_add_text("🌍", color=(0, 200, 0), tag=f"db_icon_{unique_suffix}")
                    self.safe_add_text(f"{len(self.equities_df):,} symbols", color=(0, 200, 0),
                                       tag=f"db_count_{unique_suffix}")
                else:
                    self.safe_add_text("❌", color=(255, 0, 0), tag=f"db_error_icon_{unique_suffix}")
                    self.safe_add_text("Local DB only", color=(255, 0, 0), tag=f"db_error_text_{unique_suffix}")

                self.safe_add_text(" | ", color=(120, 120, 120))
                self.safe_add_text("TIME:", color=(255, 255, 0), tag=f"time_label_{unique_suffix}")
                self.safe_add_text(datetime.now().strftime('%H:%M:%S'), tag="header_time", color=(255, 255, 255))

            dpg.add_separator()

            # Search status
            with dpg.group(horizontal=True):
                self.safe_add_text("", tag="search_status_text", color=(255, 255, 0))

            # Current stock display
            with dpg.group(tag="current_stock_display", show=False):
                with dpg.group(horizontal=True):
                    self.safe_add_text("CURRENT:", color=(255, 255, 0), tag=f"current_label_{unique_suffix}")
                    self.safe_add_text("", tag="current_stock_name", color=(255, 255, 255))
                    self.safe_add_text(" | ", color=(120, 120, 120))

                    self.safe_add_text("PRICE:", color=(255, 255, 0), tag=f"price_label_{unique_suffix}")
                    self.safe_add_text("", tag="current_stock_price", color=(0, 200, 0))

                    self.safe_add_text("", tag="current_stock_change", color=(0, 200, 0))

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
        """Create status bar with fixed color parameters"""
        try:
            dpg.add_separator()
            with dpg.group(horizontal=True):
                # FIX: Use consistent color format and unique tags
                import time
                unique_suffix = int(time.time() * 1000) % 1000

                # Status items with properly formatted colors
                self.safe_add_text("●", color=(0, 200, 0), tag=f"status_dot_{unique_suffix}")
                self.safe_add_text("DATA SERVICE ONLINE", color=(0, 200, 0), tag=f"data_service_{unique_suffix}")
                self.safe_add_text(" | ", color=(120, 120, 120))

                self.safe_add_text("STATUS:", color=(255, 255, 0), tag=f"status_label_{unique_suffix}")
                self.safe_add_text("INITIALIZING", color=(255, 255, 0), tag="main_status_text")
                self.safe_add_text(" | ", color=(120, 120, 120))

                self.safe_add_text("LAST UPDATE:", color=(255, 255, 0), tag=f"update_label_{unique_suffix}")
                self.safe_add_text("LOADING", color=(255, 255, 255), tag="last_update_time")

        except Exception as e:
            error("Error creating status bar", context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def handle_search(self, sender=None, app_data=None):
        """Handle search functionality with enhanced global search"""
        with operation("handle_search"):
            try:
                ticker = dpg.get_value("ticker_search_input").strip().upper()

                if not ticker:
                    self.safe_set_value("search_status_text", "❌ Enter ticker symbol")
                    return

                if self.is_loading:
                    self.safe_set_value("search_status_text", "⏳ Loading in progress...")
                    return

                # Hide dropdown when manual search is triggered
                self.hide_dropdown()

                # Check cache first for performance
                cache_key = f"{ticker}_basic_data"
                if cache_key in self._data_cache:
                    cached_time = self._data_cache[cache_key].get('timestamp', 0)
                    if time.time() - cached_time < 300:  # 5 minute cache
                        debug("Using cached data", context={'ticker': ticker})
                        self.stock_data = self._data_cache[cache_key]['data']
                        self.update_all_data_sections(self.stock_data)
                        self.safe_set_value("search_status_text", f"✅ {ticker} (cached)")
                        return

                self.is_loading = True
                self.current_ticker = ticker

                self.safe_set_value("search_status_text", f"📈 Loading {ticker}...")
                # Use safe configuration method
                self.safe_configure_item("search_button", enabled=False)

                log_info("Direct stock search initiated", context={'ticker': ticker})

                threading.Thread(target=self.load_complete_stock_data, args=(ticker,), daemon=True).start()

            except Exception as e:
                error("Error handling search", context={'error': str(e)}, exc_info=True)
                self.safe_set_value("search_status_text", "❌ Search error")

    def search_equities(self, query):
        """Fast search through global equities database using optimized index"""
        if not self.finance_db_available or not hasattr(self, '_search_index') or not self._search_index:
            return [f"{query.upper()} | Search in progress... | N/A (Local)"]

        query = query.lower().strip()
        if len(query) < 1:
            return []

        # Check cache first
        cache_key = f"search_{query}"
        if cache_key in self._search_cache:
            cached_time = self._search_cache[cache_key].get('timestamp', 0)
            if time.time() - cached_time < 60:  # 1 minute cache for searches
                return self._search_cache[cache_key]['results']

        results = []
        count = 0

        try:
            # Use the optimized search index
            for entry in self._search_index:
                if count >= 8:  # Show more results
                    break

                # Fast string matching on pre-processed lowercase strings
                if (query in entry['symbol_lower'] or
                        query in entry['name_lower']):
                    result_line = f"{entry['symbol']} | {entry['display_name']} | {entry['sector']} ({entry['country']}-{entry['exchange']})"
                    results.append(result_line)
                    count += 1

            # Always include the direct query as first option if it's a valid format
            if len(query) >= 1 and query.replace('.', '').replace('-', '').isalnum():
                direct_option = f"{query.upper()} | Direct Search | Manual Entry (Direct)"
                if direct_option not in results:
                    results.insert(0, direct_option)

            # Cache the results
            self._search_cache[cache_key] = {
                'results': results,
                'timestamp': time.time()
            }

            return results if results else [f"{query.upper()} | No matches found | Try direct search"]

        except Exception as e:
            error("Error searching equities", context={'error': str(e)}, exc_info=True)
            return [f"{query.upper()} | Search error | Try direct entry"]

    def delayed_search(self):
        """Perform search with delay to avoid excessive API calls"""
        try:
            search_text = dpg.get_value("ticker_search_input").strip()

            if len(search_text) < 1:
                self.hide_dropdown()
                self.safe_set_value("search_status_text", "")
                return

            self.safe_set_value("search_status_text", "🔍 Searching global database...")
            matches = self.search_equities(search_text)
            self.dropdown_items = matches
            self.show_dropdown()

            status_msg = f"✅ {len(matches)} results found" if matches else "❌ No results"
            self.safe_set_value("search_status_text", status_msg)

        except Exception as e:
            error("Error in delayed search", context={'error': str(e)}, exc_info=True)
            self.safe_set_value("search_status_text", "❌ Search error")

    def search_callback(self, sender=None, app_data=None):
        """Handle search input with debouncing"""
        try:
            # Cancel previous timer
            if self.search_timer and self.search_timer.is_alive():
                self.search_timer.cancel()

            # Start new search with delay
            self.search_timer = threading.Timer(0.2, self.delayed_search)  # Reduced delay for better UX
            self.search_timer.start()

        except Exception as e:
            error("Error in search callback", context={'error': str(e)}, exc_info=True)

    def show_dropdown(self):
        """Show search results dropdown"""
        try:
            if not self.dropdown_created:
                with dpg.window(
                        label="##search_dropdown",
                        tag="search_dropdown_window",
                        no_title_bar=True,
                        no_resize=True,
                        no_move=True,
                        no_collapse=True,
                        no_close=True,
                        show=False,
                        modal=False,
                        popup=True
                ):
                    pass
                self.dropdown_created = True

            # Clear existing items
            dpg.delete_item("search_dropdown_window", children_only=True)

            # Add search results
            for i, item in enumerate(self.dropdown_items[:8]):  # Limit to 8 items
                dpg.add_button(
                    label=item,
                    parent="search_dropdown_window",
                    callback=self.select_search_item,
                    user_data=item,
                    width=650,
                    height=30
                )

            # Position dropdown below search input
            if dpg.does_item_exist("ticker_search_input"):
                input_pos = dpg.get_item_pos("ticker_search_input")
                dpg.set_item_pos("search_dropdown_window", [input_pos[0], input_pos[1] + 35])
                dpg.show_item("search_dropdown_window")

        except Exception as e:
            error("Error showing dropdown", context={'error': str(e)}, exc_info=True)

    def hide_dropdown(self):
        """Hide search dropdown"""
        try:
            if dpg.does_item_exist("search_dropdown_window"):
                dpg.hide_item("search_dropdown_window")
        except Exception as e:
            debug("Error hiding dropdown", context={'error': str(e)})

    def select_search_item(self, sender, app_data, user_data):
        """Handle selection from dropdown"""
        try:
            # Extract symbol from the selected item
            symbol = user_data.split(" | ")[0].strip()

            # Update search input
            dpg.set_value("ticker_search_input", symbol)

            # Hide dropdown
            self.hide_dropdown()

            # Clear search status
            self.safe_set_value("search_status_text", f"Selected: {symbol}")

            # Trigger data loading for selected symbol
            self.current_ticker = symbol
            log_info("Symbol selected from search", context={'symbol': symbol})

            # Load data for selected symbol
            threading.Thread(target=self.load_complete_stock_data, args=(symbol,), daemon=True).start()

        except Exception as e:
            error("Error selecting search item", context={'error': str(e)}, exc_info=True)

    def clear_search(self):
        """Clear search input and hide dropdown"""
        try:
            dpg.set_value("ticker_search_input", "")
            self.safe_set_value("search_status_text", "")
            self.hide_dropdown()
        except Exception as e:
            error("Error clearing search", context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def load_complete_stock_data(self, ticker):
        """Load complete stock data including financials with performance optimizations"""
        with operation("load_complete_stock_data", context={'ticker': ticker}):
            try:
                stock = yf.Ticker(ticker)

                # Get basic info
                self.safe_set_value("search_status_text", f"📊 Loading {ticker} info...")
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
                self.safe_set_value("search_status_text", f"📈 Loading {ticker} price history...")
                history_start_time = time.time()
                history = stock.history(period="2y")
                history_load_time = time.time() - history_start_time

                if history.empty:
                    raise Exception("No historical data available")

                self.history_data = history

                # Update UI with basic data
                self.update_all_data_sections(info)

                # Load financial statements
                self.safe_set_value("search_status_text", f"💰 Loading {ticker} financials...")
                financials_start_time = time.time()
                self.load_financial_data(stock)
                financials_load_time = time.time() - financials_start_time

                self.safe_set_value("search_status_text", f"✅ {ticker} loaded successfully")
                self.safe_set_value("last_update_time", datetime.now().strftime('%H:%M:%S'))
                self._last_update_time = time.time()

                from fincept_terminal.Utils.Logging.logger import info as log_info
                log_info("Stock data loaded successfully",
                         context={
                             'ticker': ticker,
                             'info_load_time_ms': f"{info_load_time * 1000:.1f}",
                             'history_load_time_ms': f"{history_load_time * 1000:.1f}",
                             'financials_load_time_ms': f"{financials_load_time * 1000:.1f}"
                         })

            except Exception as e:
                error_msg = str(e)[:50] + "..." if len(str(e)) > 50 else str(e)
                self.safe_set_value("search_status_text", f"❌ Error: {error_msg}")
                error("Error loading stock data",
                      context={'ticker': ticker, 'error': str(e)}, exc_info=True)

            finally:
                self.is_loading = False
                # Use safe configuration method instead
                self.safe_configure_item("search_button", enabled=True)

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
                # Cancel any running search timer
                if self.search_timer and self.search_timer.is_alive():
                    self.search_timer.cancel()

                # Hide dropdown
                self.hide_dropdown()

                # Clear caches
                self._data_cache.clear()
                self._chart_cache.clear()
                self._search_cache.clear()

                # Clear search index
                if hasattr(self, '_search_index'):
                    del self._search_index

                # Clear data structures
                self.financials_data.clear()
                if hasattr(self, 'history_data') and self.history_data is not None:
                    del self.history_data
                if hasattr(self, 'stock_data') and self.stock_data is not None:
                    del self.stock_data
                if hasattr(self, 'equities_data') and self.equities_data is not None:
                    del self.equities_data
                if hasattr(self, 'equities_df') and self.equities_df is not None:
                    del self.equities_df

                log_info("YFinance Data Tab cleanup completed")
            except Exception as e:
                error("Error during cleanup", context={'error': str(e)}, exc_info=True)