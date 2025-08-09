# -*- coding: utf-8 -*-
# data_viewer_tab.py

import dearpygui.dearpygui as dpg
import threading
import time
import traceback
import uuid
import asyncio
from datetime import datetime
from typing import Dict, Any, Optional, List

from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import info, debug, warning, error, operation, monitor_performance
from fincept_terminal.DatabaseConnector.DataSources.data_source_manager import get_data_source_manager


class DataViewerTab(BaseTab):
    """Enhanced Data Viewer tab for displaying comprehensive financial data from Alpha Vantage and other providers"""

    def __init__(self, app):
        print("🔧 DEBUG: DataViewerTab.__init__ called")
        try:
            super().__init__(app)
            print("🔧 DEBUG: BaseTab.__init__ completed")

            # Generate unique IDs to avoid conflicts
            self.tab_id = str(uuid.uuid4())[:8]
            self.data_source_manager = get_data_source_manager(app)
            print(f"🔧 DEBUG: DataSourceManager: {self.data_source_manager}")

            # Data storage
            self.current_data = {}
            self.last_refresh = None
            self.auto_refresh = False
            self.refresh_interval = 30  # seconds
            self.refresh_thread = None
            self._stop_refresh = False

            # Available data types and their corresponding endpoints
            self.data_types = {
                "Stock Data": {
                    "Time Series Daily": "get_stock_data",
                    "Time Series Intraday": "get_stock_data",
                    "Time Series Weekly": "get_weekly_data",
                    "Time Series Monthly": "get_monthly_data",
                    "Daily Adjusted": "get_daily_adjusted",
                    "Weekly Adjusted": "get_weekly_adjusted",
                    "Monthly Adjusted": "get_monthly_adjusted",
                    "Global Quote": "get_global_quote",
                    "Symbol Search": "search_symbols"
                },
                "Fundamental Data": {
                    "Company Overview": "get_company_overview",
                    "Income Statement": "get_income_statement",
                    "Balance Sheet": "get_balance_sheet",
                    "Cash Flow": "get_cash_flow",
                    "Earnings": "get_earnings",
                    "Earnings Estimates": "get_earnings_estimates",
                    "Dividends": "get_dividends",
                    "Splits": "get_splits"
                },
                "Technical Indicators": {
                    "SMA": "get_sma",
                    "EMA": "get_ema",
                    "RSI": "get_rsi",
                    "MACD": "get_macd",
                    "Bollinger Bands": "get_bbands",
                    "Stochastic": "get_stoch",
                    "ADX": "get_adx",
                    "VWAP": "get_vwap"
                },
                "Forex": {
                    "Currency Exchange Rate": "get_currency_exchange_rate",
                    "FX Daily": "get_forex_data",
                    "FX Intraday": "get_fx_intraday",
                    "FX Weekly": "get_fx_weekly",
                    "FX Monthly": "get_fx_monthly"
                },
                "Cryptocurrency": {
                    "Crypto Daily": "get_crypto_data",
                    "Crypto Intraday": "get_crypto_intraday",
                    "Digital Currency Weekly": "get_digital_currency_weekly",
                    "Digital Currency Monthly": "get_digital_currency_monthly"
                },
                "Commodities": {
                    "WTI Oil": "get_wti_oil",
                    "Brent Oil": "get_brent_oil",
                    "Natural Gas": "get_natural_gas",
                    "Gold": "get_copper",
                    "Silver": "get_aluminum"
                },
                "Economic Indicators": {
                    "Real GDP": "get_real_gdp",
                    "Unemployment": "get_unemployment",
                    "CPI": "get_cpi",
                    "Treasury Yield": "get_treasury_yield",
                    "Federal Funds Rate": "get_federal_funds_rate"
                },
                "Market Intelligence": {
                    "News Sentiment": "get_news_sentiment",
                    "Top Gainers/Losers": "get_top_gainers_losers",
                    "Insider Transactions": "get_insider_transactions"
                }
            }

            print("✅ DEBUG: DataViewerTab initialization completed successfully")

        except Exception as e:
            print(f"❌ DEBUG: Error in DataViewerTab.__init__: {str(e)}")
            print(f"❌ DEBUG: Traceback: {traceback.format_exc()}")
            raise

    def get_label(self):
        return "📊 Advanced Data Viewer"

    def safe_add_item(self, add_function, *args, **kwargs):
        """Safely add DPG items with error handling"""
        try:
            return add_function(*args, **kwargs)
        except Exception as e:
            print(f"❌ DEBUG: Error adding item {add_function.__name__}: {str(e)}")
            try:
                if 'tag' in kwargs:
                    kwargs['tag'] = f"{kwargs['tag']}_{self.tab_id}"
                return add_function(*args, **kwargs)
            except:
                print(f"❌ DEBUG: Failed to add {add_function.__name__} even with modified tag")
                return None

    def create_content(self):
        """Create the enhanced data viewer interface"""
        print("🔧 DEBUG: create_content() called")
        try:
            self.add_section_header("📊 Advanced Financial Data Viewer")

            self.safe_add_item(dpg.add_text,
                               "Access comprehensive financial data from Alpha Vantage and other providers",
                               color=[200, 200, 200])
            self.safe_add_item(dpg.add_spacer, height=20)

            # Main interface
            with dpg.child_window(height=800, border=True):
                self.create_control_panel()
                self.safe_add_item(dpg.add_separator)
                self.safe_add_item(dpg.add_spacer, height=10)
                self.create_data_display()

            print("✅ DEBUG: create_content() completed successfully")

        except Exception as e:
            print(f"❌ DEBUG: Error in create_content(): {str(e)}")
            print(f"❌ DEBUG: Traceback: {traceback.format_exc()}")
            try:
                self.safe_add_item(dpg.add_text, f"Error creating content: {str(e)}", color=[255, 100, 100])
            except:
                pass

    def create_control_panel(self):
        """Create enhanced control panel"""
        print("🔧 DEBUG: create_control_panel() called")
        try:
            self.safe_add_item(dpg.add_text, "🎛️ Data Controls", color=[255, 255, 100])
            self.safe_add_item(dpg.add_spacer, height=10)

            # Data category selection
            with dpg.group(horizontal=True):
                self.safe_add_item(dpg.add_text, "Category:", width=100)
                self.safe_add_item(dpg.add_combo,
                                   list(self.data_types.keys()),
                                   tag=f"data_category_{self.tab_id}",
                                   default_value="Stock Data",
                                   width=150,
                                   callback=self.on_category_change)

                self.safe_add_item(dpg.add_text, "Data Type:", width=100)
                self.safe_add_item(dpg.add_combo,
                                   list(self.data_types["Stock Data"].keys()),
                                   tag=f"data_type_{self.tab_id}",
                                   default_value="Time Series Daily",
                                   width=150)

            self.safe_add_item(dpg.add_spacer, height=10)

            # Symbol and parameters
            with dpg.group(horizontal=True):
                self.safe_add_item(dpg.add_text, "Symbol:", width=100)
                self.safe_add_item(dpg.add_input_text,
                                   tag=f"symbol_input_{self.tab_id}",
                                   default_value="AAPL",
                                   width=100,
                                   hint="e.g., AAPL, EURUSD, BTC")

                self.safe_add_item(dpg.add_text, "Period:", width=100)
                self.safe_add_item(dpg.add_combo,
                                   ["1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y", "max"],
                                   tag=f"period_combo_{self.tab_id}",
                                   default_value="1d",
                                   width=80)

                self.safe_add_item(dpg.add_text, "Interval:", width=100)
                self.safe_add_item(dpg.add_combo,
                                   ["1min", "5min", "15min", "30min", "60min", "daily", "weekly", "monthly"],
                                   tag=f"interval_combo_{self.tab_id}",
                                   default_value="daily",
                                   width=80)

            self.safe_add_item(dpg.add_spacer, height=10)

            # Additional parameters for technical indicators
            with dpg.group(horizontal=True, tag=f"tech_params_{self.tab_id}", show=False):
                self.safe_add_item(dpg.add_text, "Time Period:", width=100)
                self.safe_add_item(dpg.add_input_int,
                                   tag=f"time_period_{self.tab_id}",
                                   default_value=14,
                                   width=60,
                                   min_value=1,
                                   max_value=200)

                self.safe_add_item(dpg.add_text, "Series Type:", width=100)
                self.safe_add_item(dpg.add_combo,
                                   ["close", "open", "high", "low"],
                                   tag=f"series_type_{self.tab_id}",
                                   default_value="close",
                                   width=80)

            # Additional parameters for forex
            with dpg.group(horizontal=True, tag=f"forex_params_{self.tab_id}", show=False):
                self.safe_add_item(dpg.add_text, "From Currency:", width=100)
                self.safe_add_item(dpg.add_input_text,
                                   tag=f"from_currency_{self.tab_id}",
                                   default_value="USD",
                                   width=60)

                self.safe_add_item(dpg.add_text, "To Currency:", width=100)
                self.safe_add_item(dpg.add_input_text,
                                   tag=f"to_currency_{self.tab_id}",
                                   default_value="EUR",
                                   width=60)

            self.safe_add_item(dpg.add_spacer, height=15)

            # Action buttons
            with dpg.group(horizontal=True):
                self.safe_add_item(dpg.add_button,
                                   label="🔄 Fetch Data",
                                   tag=f"fetch_btn_{self.tab_id}",
                                   callback=self.fetch_data,
                                   width=120)

                self.safe_add_item(dpg.add_button,
                                   label="🧹 Clear",
                                   tag=f"clear_btn_{self.tab_id}",
                                   callback=self.clear_data,
                                   width=80)

                self.safe_add_item(dpg.add_checkbox,
                                   label="Auto Refresh (30s)",
                                   tag=f"auto_refresh_{self.tab_id}",
                                   callback=self.toggle_auto_refresh)

                self.safe_add_item(dpg.add_button,
                                   label="📋 Export CSV",
                                   tag=f"export_btn_{self.tab_id}",
                                   callback=self.export_data,
                                   width=120)

            # Status display
            self.safe_add_item(dpg.add_spacer, height=10)
            with dpg.group(horizontal=True):
                self.safe_add_item(dpg.add_text, "Status:", color=[150, 150, 150])
                self.safe_add_item(dpg.add_text, "Ready",
                                   tag=f"status_text_{self.tab_id}", color=[100, 255, 100])

                self.safe_add_item(dpg.add_text, " | Last Updated:", color=[150, 150, 150])
                self.safe_add_item(dpg.add_text, "Never",
                                   tag=f"last_update_{self.tab_id}", color=[150, 150, 150])

            print("✅ DEBUG: create_control_panel() completed")

        except Exception as e:
            print(f"❌ DEBUG: Error in create_control_panel(): {str(e)}")
            print(f"❌ DEBUG: Traceback: {traceback.format_exc()}")

    def create_data_display(self):
        """Create comprehensive data display area"""
        print("🔧 DEBUG: create_data_display() called")
        try:
            self.safe_add_item(dpg.add_text, "📈 Data Display", color=[255, 255, 100])
            self.safe_add_item(dpg.add_spacer, height=10)

            with dpg.tab_bar(tag=f"display_tabs_{self.tab_id}"):

                # Overview Tab
                with dpg.tab(label="📊 Overview", tag=f"overview_tab_{self.tab_id}"):
                    self.create_overview_display()

                # Time Series Tab
                with dpg.tab(label="📈 Time Series", tag=f"timeseries_tab_{self.tab_id}"):
                    self.create_timeseries_display()

                # Fundamentals Tab
                with dpg.tab(label="💼 Fundamentals", tag=f"fundamentals_tab_{self.tab_id}"):
                    self.create_fundamentals_display()

                # Technical Analysis Tab
                with dpg.tab(label="📐 Technical", tag=f"technical_tab_{self.tab_id}"):
                    self.create_technical_display()

                # Raw Data Tab
                with dpg.tab(label="🔍 Raw Data", tag=f"raw_tab_{self.tab_id}"):
                    self.create_raw_display()

                # Provider Info Tab
                with dpg.tab(label="🔧 Provider Info", tag=f"provider_tab_{self.tab_id}"):
                    self.create_provider_display()

            print("✅ DEBUG: create_data_display() completed")

        except Exception as e:
            print(f"❌ DEBUG: Error in create_data_display(): {str(e)}")
            print(f"❌ DEBUG: Traceback: {traceback.format_exc()}")

    def create_overview_display(self):
        """Create overview display for key metrics"""
        try:
            self.safe_add_item(dpg.add_spacer, height=10)

            # Key metrics cards
            with dpg.group(horizontal=True):
                # Current Price/Value Card
                with dpg.child_window(width=200, height=120, border=True):
                    self.safe_add_item(dpg.add_text, "Current Price", color=[255, 255, 100])
                    self.safe_add_item(dpg.add_separator)
                    self.safe_add_item(dpg.add_text, "N/A", tag=f"overview_price_{self.tab_id}",
                                       color=[100, 255, 100])
                    self.safe_add_item(dpg.add_text, "Change: N/A", tag=f"overview_change_{self.tab_id}",
                                       color=[200, 200, 200])

                # Volume/Activity Card
                with dpg.child_window(width=200, height=120, border=True):
                    self.safe_add_item(dpg.add_text, "Volume/Activity", color=[255, 255, 100])
                    self.safe_add_item(dpg.add_separator)
                    self.safe_add_item(dpg.add_text, "N/A", tag=f"overview_volume_{self.tab_id}",
                                       color=[200, 200, 200])
                    self.safe_add_item(dpg.add_text, "Avg: N/A", tag=f"overview_avg_volume_{self.tab_id}",
                                       color=[200, 200, 200])

                # Range Card
                with dpg.child_window(width=200, height=120, border=True):
                    self.safe_add_item(dpg.add_text, "Price Range", color=[255, 255, 100])
                    self.safe_add_item(dpg.add_separator)
                    self.safe_add_item(dpg.add_text, "High: N/A", tag=f"overview_high_{self.tab_id}",
                                       color=[100, 255, 100])
                    self.safe_add_item(dpg.add_text, "Low: N/A", tag=f"overview_low_{self.tab_id}",
                                       color=[255, 100, 100])

            self.safe_add_item(dpg.add_spacer, height=20)

            # Data info
            self.safe_add_item(dpg.add_text, "📋 Data Information", color=[255, 255, 100])
            self.safe_add_item(dpg.add_separator)
            self.safe_add_item(dpg.add_spacer, height=5)

            with dpg.table(tag=f"overview_info_table_{self.tab_id}", header_row=True,
                           borders_innerH=True, borders_innerV=True):
                dpg.add_table_column(label="Property")
                dpg.add_table_column(label="Value")

        except Exception as e:
            print(f"❌ DEBUG: Error in create_overview_display(): {str(e)}")

    def create_timeseries_display(self):
        """Create time series data display"""
        try:
            self.safe_add_item(dpg.add_spacer, height=10)

            # Data controls
            with dpg.group(horizontal=True):
                self.safe_add_item(dpg.add_text, "Show Last:")
                self.safe_add_item(dpg.add_combo,
                                   ["10", "25", "50", "100", "All"],
                                   tag=f"timeseries_limit_{self.tab_id}",
                                   default_value="25",
                                   width=80)

                self.safe_add_item(dpg.add_button,
                                   label="🔄 Refresh View",
                                   callback=self.refresh_timeseries_view,
                                   width=120)

            self.safe_add_item(dpg.add_spacer, height=10)

            # Time series table
            with dpg.table(tag=f"timeseries_table_{self.tab_id}", header_row=True, resizable=True,
                           borders_innerH=True, borders_innerV=True, borders_outerH=True, borders_outerV=True,
                           scrollY=True, height=400):
                dpg.add_table_column(label="Date/Time", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Open", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="High", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Low", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Close", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Volume", width_fixed=True, init_width_or_weight=100)

        except Exception as e:
            print(f"❌ DEBUG: Error in create_timeseries_display(): {str(e)}")

    def create_fundamentals_display(self):
        """Create fundamentals data display"""
        try:
            self.safe_add_item(dpg.add_spacer, height=10)

            # Company basics
            self.safe_add_item(dpg.add_text, "🏢 Company Information", color=[255, 255, 100])
            self.safe_add_item(dpg.add_separator)

            with dpg.table(tag=f"company_info_table_{self.tab_id}", header_row=True,
                           borders_innerH=True, borders_innerV=True, scrollY=True, height=150):
                dpg.add_table_column(label="Property")
                dpg.add_table_column(label="Value")

            self.safe_add_item(dpg.add_spacer, height=15)

            # Financial statements
            self.safe_add_item(dpg.add_text, "📊 Financial Statements", color=[255, 255, 100])
            self.safe_add_item(dpg.add_separator)

            with dpg.tab_bar(tag=f"financial_tabs_{self.tab_id}"):
                with dpg.tab(label="Income Statement"):
                    with dpg.table(tag=f"income_table_{self.tab_id}", header_row=True,
                                   borders_innerH=True, borders_innerV=True, scrollY=True, height=200):
                        dpg.add_table_column(label="Item")
                        dpg.add_table_column(label="Value")

                with dpg.tab(label="Balance Sheet"):
                    with dpg.table(tag=f"balance_table_{self.tab_id}", header_row=True,
                                   borders_innerH=True, borders_innerV=True, scrollY=True, height=200):
                        dpg.add_table_column(label="Item")
                        dpg.add_table_column(label="Value")

                with dpg.tab(label="Cash Flow"):
                    with dpg.table(tag=f"cashflow_table_{self.tab_id}", header_row=True,
                                   borders_innerH=True, borders_innerV=True, scrollY=True, height=200):
                        dpg.add_table_column(label="Item")
                        dpg.add_table_column(label="Value")

        except Exception as e:
            print(f"❌ DEBUG: Error in create_fundamentals_display(): {str(e)}")

    def create_technical_display(self):
        """Create technical analysis display"""
        try:
            self.safe_add_item(dpg.add_spacer, height=10)

            # Technical indicators summary
            self.safe_add_item(dpg.add_text, "📐 Technical Indicators", color=[255, 255, 100])
            self.safe_add_item(dpg.add_separator)

            with dpg.table(tag=f"technical_table_{self.tab_id}", header_row=True,
                           borders_innerH=True, borders_innerV=True, scrollY=True, height=300):
                dpg.add_table_column(label="Date/Time")
                dpg.add_table_column(label="Indicator")
                dpg.add_table_column(label="Value")
                dpg.add_table_column(label="Signal")

        except Exception as e:
            print(f"❌ DEBUG: Error in create_technical_display(): {str(e)}")

    def create_raw_display(self):
        """Create raw data display"""
        try:
            self.safe_add_item(dpg.add_spacer, height=10)

            # Raw JSON data
            self.safe_add_item(dpg.add_text, "🔍 Raw API Response", color=[255, 255, 100])
            self.safe_add_item(dpg.add_separator)
            self.safe_add_item(dpg.add_spacer, height=5)

            with dpg.group(horizontal=True):
                self.safe_add_item(dpg.add_button,
                                   label="📋 Copy to Clipboard",
                                   callback=self.copy_raw_data,
                                   width=150)

                self.safe_add_item(dpg.add_button,
                                   label="💾 Save to File",
                                   callback=self.save_raw_data,
                                   width=120)

            self.safe_add_item(dpg.add_spacer, height=10)

            self.safe_add_item(dpg.add_input_text,
                               tag=f"raw_data_display_{self.tab_id}",
                               multiline=True,
                               height=400,
                               width=-1,
                               readonly=True,
                               default_value="No data loaded yet...")

        except Exception as e:
            print(f"❌ DEBUG: Error in create_raw_display(): {str(e)}")

    def create_provider_display(self):
        """Create provider information display"""
        try:
            self.safe_add_item(dpg.add_spacer, height=10)

            # Provider status
            self.safe_add_item(dpg.add_text, "🔧 Provider Information", color=[255, 255, 100])
            self.safe_add_item(dpg.add_separator)

            with dpg.table(tag=f"provider_info_table_{self.tab_id}", header_row=True,
                           borders_innerH=True, borders_innerV=True):
                dpg.add_table_column(label="Property")
                dpg.add_table_column(label="Value")

            self.safe_add_item(dpg.add_spacer, height=15)

            # Available endpoints
            self.safe_add_item(dpg.add_text, "📋 Available Endpoints", color=[255, 255, 100])
            self.safe_add_item(dpg.add_separator)

            with dpg.table(tag=f"endpoints_table_{self.tab_id}", header_row=True,
                           borders_innerH=True, borders_innerV=True, scrollY=True, height=200):
                dpg.add_table_column(label="Category")
                dpg.add_table_column(label="Endpoint")
                dpg.add_table_column(label="Status")

            # Populate endpoints table
            self.populate_endpoints_table()

        except Exception as e:
            print(f"❌ DEBUG: Error in create_provider_display(): {str(e)}")

    def populate_endpoints_table(self):
        """Populate the endpoints table with available data types"""
        try:
            table_tag = f"endpoints_table_{self.tab_id}"
            if not dpg.does_item_exist(table_tag):
                return

            for category, endpoints in self.data_types.items():
                for endpoint_name, method_name in endpoints.items():
                    with dpg.table_row(parent=table_tag):
                        dpg.add_text(category)
                        dpg.add_text(endpoint_name)
                        # Check if method exists in data source manager
                        if hasattr(self.data_source_manager, method_name) if self.data_source_manager else False:
                            dpg.add_text("✅ Available", color=[100, 255, 100])
                        else:
                            dpg.add_text("❌ Not Available", color=[255, 100, 100])

        except Exception as e:
            print(f"❌ DEBUG: Error in populate_endpoints_table(): {str(e)}")

    def on_category_change(self, sender, app_data):
        """Handle category change to update data type options"""
        try:
            category = app_data
            data_type_combo = f"data_type_{self.tab_id}"

            if category in self.data_types:
                dpg.configure_item(data_type_combo, items=list(self.data_types[category].keys()))
                dpg.set_value(data_type_combo, list(self.data_types[category].keys())[0])

                # Show/hide parameter groups based on category
                self.update_parameter_visibility(category)

        except Exception as e:
            print(f"❌ DEBUG: Error in on_category_change(): {str(e)}")

    def update_parameter_visibility(self, category):
        """Update visibility of parameter groups based on selected category"""
        try:
            # Hide all parameter groups first
            dpg.configure_item(f"tech_params_{self.tab_id}", show=False)
            dpg.configure_item(f"forex_params_{self.tab_id}", show=False)

            # Show relevant parameter groups
            if category == "Technical Indicators":
                dpg.configure_item(f"tech_params_{self.tab_id}", show=True)
            elif category == "Forex":
                dpg.configure_item(f"forex_params_{self.tab_id}", show=True)

        except Exception as e:
            print(f"❌ DEBUG: Error in update_parameter_visibility(): {str(e)}")

    def fetch_data(self):
        """Fetch data based on current selections"""
        try:
            if not self.data_source_manager:
                self.update_status("❌ Data Source Manager not available", [255, 100, 100])
                return

            category = dpg.get_value(f"data_category_{self.tab_id}")
            data_type = dpg.get_value(f"data_type_{self.tab_id}")
            symbol = dpg.get_value(f"symbol_input_{self.tab_id}").strip().upper()

            if not symbol and category not in ["Economic Indicators", "Market Intelligence"]:
                self.update_status("❌ Please enter a symbol", [255, 100, 100])
                return

            self.update_status("🔄 Fetching data...", [255, 255, 100])

            # Fetch data in a separate thread
            thread = threading.Thread(target=self._fetch_data_async,
                                      args=(category, data_type, symbol))
            thread.daemon = True
            thread.start()

        except Exception as e:
            print(f"❌ DEBUG: Error in fetch_data(): {str(e)}")
            self.update_status(f"❌ Error: {str(e)}", [255, 100, 100])

    def _fetch_data_async(self, category: str, data_type: str, symbol: str):
        """Fetch data asynchronously"""
        try:
            print(f"🔧 DEBUG: Fetching {category} - {data_type} for {symbol}")

            # Get the method name
            if category not in self.data_types or data_type not in self.data_types[category]:
                self.update_status("❌ Invalid data type selection", [255, 100, 100])
                return

            method_name = self.data_types[category][data_type]

            # Check if method exists
            if not hasattr(self.data_source_manager, method_name):
                self.update_status(f"❌ Method {method_name} not available", [255, 100, 100])
                return

            method = getattr(self.data_source_manager, method_name)

            # Prepare parameters based on category and data type
            params = self._prepare_method_parameters(category, data_type, symbol)

            # Call the method
            if asyncio.iscoroutinefunction(method):
                # Handle async methods
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)
                try:
                    data = loop.run_until_complete(method(**params))
                finally:
                    loop.close()
            else:
                # Handle sync methods
                data = method(**params)

            print(f"🔧 DEBUG: Data fetch result: {data.get('success', False) if isinstance(data, dict) else 'Unknown'}")

            # Update UI with results
            if isinstance(data, dict) and data.get("success"):
                self.current_data = data
                self.update_all_displays(data, category, data_type, symbol)
                self.update_status("✅ Data loaded successfully", [100, 255, 100])
                self.last_refresh = datetime.now()
                dpg.set_value(f"last_update_{self.tab_id}", self.last_refresh.strftime("%H:%M:%S"))
            else:
                error_msg = data.get("error", "Unknown error") if isinstance(data, dict) else str(data)
                self.update_status(f"❌ {error_msg}", [255, 100, 100])

        except Exception as e:
            print(f"❌ DEBUG: Error in _fetch_data_async(): {str(e)}")
            print(f"❌ DEBUG: Traceback: {traceback.format_exc()}")
            self.update_status(f"❌ Error: {str(e)}", [255, 100, 100])

    def _prepare_method_parameters(self, category: str, data_type: str, symbol: str) -> Dict[str, Any]:
        """Prepare parameters for the API method call"""
        params = {}

        try:
            # Common parameters
            if symbol:
                params['symbol'] = symbol

            # Get additional parameters from UI
            if dpg.does_item_exist(f"period_combo_{self.tab_id}"):
                params['period'] = dpg.get_value(f"period_combo_{self.tab_id}")

            if dpg.does_item_exist(f"interval_combo_{self.tab_id}"):
                params['interval'] = dpg.get_value(f"interval_combo_{self.tab_id}")

            # Category-specific parameters
            if category == "Technical Indicators":
                if dpg.does_item_exist(f"time_period_{self.tab_id}"):
                    params['time_period'] = dpg.get_value(f"time_period_{self.tab_id}")
                if dpg.does_item_exist(f"series_type_{self.tab_id}"):
                    params['series_type'] = dpg.get_value(f"series_type_{self.tab_id}")

            elif category == "Forex":
                if data_type == "Currency Exchange Rate":
                    params['from_currency'] = dpg.get_value(f"from_currency_{self.tab_id}") if dpg.does_item_exist(
                        f"from_currency_{self.tab_id}") else "USD"
                    params['to_currency'] = dpg.get_value(f"to_currency_{self.tab_id}") if dpg.does_item_exist(
                        f"to_currency_{self.tab_id}") else "EUR"
                elif data_type in ["FX Daily", "FX Intraday", "FX Weekly", "FX Monthly"]:
                    params['from_symbol'] = dpg.get_value(f"from_currency_{self.tab_id}") if dpg.does_item_exist(
                        f"from_currency_{self.tab_id}") else "USD"
                    params['to_symbol'] = dpg.get_value(f"to_currency_{self.tab_id}") if dpg.does_item_exist(
                        f"to_currency_{self.tab_id}") else "EUR"

            elif category == "Economic Indicators":
                # Economic indicators might need specific parameters
                if data_type == "Treasury Yield":
                    params['maturity'] = "10year"  # Default maturity
                    params['interval'] = "monthly"

            # Remove None values and symbol for methods that don't need it
            if data_type in ["Top Gainers/Losers", "Real GDP", "Unemployment", "CPI"]:
                params.pop('symbol', None)

            print(f"🔧 DEBUG: Prepared parameters: {params}")
            return params

        except Exception as e:
            print(f"❌ DEBUG: Error preparing parameters: {str(e)}")
            return {}

    def update_all_displays(self, data: Dict[str, Any], category: str, data_type: str, symbol: str):
        """Update all display tabs with fetched data"""
        try:
            # Update overview
            self.update_overview_display(data, category, data_type, symbol)

            # Update time series if applicable
            if "data" in data and isinstance(data["data"], dict):
                self.update_timeseries_display(data["data"])

            # Update fundamentals if applicable
            if category == "Fundamental Data":
                self.update_fundamentals_display(data, data_type)

            # Update technical indicators if applicable
            if category == "Technical Indicators":
                self.update_technical_display(data, data_type)

            # Update raw data display
            self.update_raw_display(data)

            # Update provider info
            self.update_provider_info(data)

        except Exception as e:
            print(f"❌ DEBUG: Error in update_all_displays(): {str(e)}")

    def update_overview_display(self, data: Dict[str, Any], category: str, data_type: str, symbol: str):
        """Update overview display with key metrics"""
        try:
            # Update price/value
            price_value = "N/A"
            change_value = "N/A"
            volume_value = "N/A"
            high_value = "N/A"
            low_value = "N/A"

            if category == "Stock Data":
                if data_type == "Global Quote" and "data" in data:
                    quote_data = data["data"]
                    price_value = f"${quote_data.get('price', 0):.2f}"
                    change_value = f"{quote_data.get('change', 0):.2f} ({quote_data.get('change_percent', '0%')})"
                    volume_value = f"{quote_data.get('volume', 0):,}"
                    high_value = f"${quote_data.get('high', 0):.2f}"
                    low_value = f"${quote_data.get('low', 0):.2f}"
                elif "current_price" in data:
                    price_value = f"${data['current_price']:.2f}"
                elif "data" in data and isinstance(data["data"], dict):
                    if "close" in data["data"] and data["data"]["close"]:
                        price_value = f"${data['data']['close'][-1]:.2f}"
                        if "high" in data["data"] and data["data"]["high"]:
                            high_value = f"${max(data['data']['high']):.2f}"
                        if "low" in data["data"] and data["data"]["low"]:
                            low_value = f"${min(data['data']['low']):.2f}"
                        if "volume" in data["data"] and data["data"]["volume"]:
                            avg_volume = sum(data['data']['volume']) // len(data['data']['volume'])
                            volume_value = f"{avg_volume:,}"

            elif category == "Forex":
                if "current_rate" in data:
                    price_value = f"{data['current_rate']:.4f}"
                elif "data" in data and "exchange_rate" in data["data"]:
                    price_value = f"{data['data']['exchange_rate']:.4f}"

            elif category == "Cryptocurrency":
                if "current_price" in data:
                    price_value = f"${data['current_price']:.2f}"

            # Update UI elements
            dpg.set_value(f"overview_price_{self.tab_id}", price_value)
            dpg.set_value(f"overview_change_{self.tab_id}", f"Change: {change_value}")
            dpg.set_value(f"overview_volume_{self.tab_id}", volume_value)
            dpg.set_value(f"overview_high_{self.tab_id}", f"High: {high_value}")
            dpg.set_value(f"overview_low_{self.tab_id}", f"Low: {low_value}")

            # Update info table
            self.update_info_table(data, symbol, category, data_type)

        except Exception as e:
            print(f"❌ DEBUG: Error in update_overview_display(): {str(e)}")

    def update_info_table(self, data: Dict[str, Any], symbol: str, category: str, data_type: str):
        """Update the overview info table"""
        try:
            table_tag = f"overview_info_table_{self.tab_id}"

            # Clear existing rows
            if dpg.does_item_exist(table_tag):
                children = dpg.get_item_children(table_tag, 1)
                for child in children:
                    dpg.delete_item(child)

            # Add basic info rows
            with dpg.table_row(parent=table_tag):
                dpg.add_text("Symbol")
                dpg.add_text(symbol)

            with dpg.table_row(parent=table_tag):
                dpg.add_text("Category")
                dpg.add_text(category)

            with dpg.table_row(parent=table_tag):
                dpg.add_text("Data Type")
                dpg.add_text(data_type)

            with dpg.table_row(parent=table_tag):
                dpg.add_text("Source")
                dpg.add_text(data.get("source", "Unknown"))

            with dpg.table_row(parent=table_tag):
                dpg.add_text("Fetched At")
                dpg.add_text(data.get("fetched_at", "Unknown"))

            # Add data-specific info
            if "data" in data and isinstance(data["data"], dict):
                data_info = data["data"]
                for key, value in data_info.items():
                    if key not in ["timestamps", "open", "high", "low", "close", "volume"] and not isinstance(value,
                                                                                                              list):
                        with dpg.table_row(parent=table_tag):
                            dpg.add_text(key.replace("_", " ").title())
                            dpg.add_text(str(value)[:50] + "..." if len(str(value)) > 50 else str(value))

        except Exception as e:
            print(f"❌ DEBUG: Error in update_info_table(): {str(e)}")

    def update_timeseries_display(self, data: Dict[str, Any]):
        """Update time series data table"""
        try:
            table_tag = f"timeseries_table_{self.tab_id}"

            # Clear existing rows
            if dpg.does_item_exist(table_tag):
                children = dpg.get_item_children(table_tag, 1)
                for child in children:
                    dpg.delete_item(child)

            # Check if we have time series data
            if not all(key in data for key in ["timestamps"]):
                return

            timestamps = data.get("timestamps", [])
            opens = data.get("open", [])
            highs = data.get("high", [])
            lows = data.get("low", [])
            closes = data.get("close", [])
            volumes = data.get("volume", [])

            # Get display limit
            limit_str = dpg.get_value(f"timeseries_limit_{self.tab_id}") if dpg.does_item_exist(
                f"timeseries_limit_{self.tab_id}") else "25"
            limit = len(timestamps) if limit_str == "All" else min(int(limit_str), len(timestamps))

            # Add rows (most recent first)
            for i in range(min(limit, len(timestamps))):
                idx = len(timestamps) - 1 - i  # Reverse order for most recent first

                with dpg.table_row(parent=table_tag):
                    dpg.add_text(timestamps[idx] if idx < len(timestamps) else "N/A")
                    dpg.add_text(f"{opens[idx]:.2f}" if idx < len(opens) and opens[idx] else "N/A")
                    dpg.add_text(f"{highs[idx]:.2f}" if idx < len(highs) and highs[idx] else "N/A")
                    dpg.add_text(f"{lows[idx]:.2f}" if idx < len(lows) and lows[idx] else "N/A")
                    dpg.add_text(f"{closes[idx]:.2f}" if idx < len(closes) and closes[idx] else "N/A")
                    dpg.add_text(f"{volumes[idx]:,}" if idx < len(volumes) and volumes[idx] else "N/A")

        except Exception as e:
            print(f"❌ DEBUG: Error in update_timeseries_display(): {str(e)}")

    def update_fundamentals_display(self, data: Dict[str, Any], data_type: str):
        """Update fundamentals display based on data type"""
        try:
            if data_type == "Company Overview":
                self.update_company_info_table(data.get("data", {}))
            elif data_type == "Income Statement":
                self.update_financial_table("income_table", data.get("data", {}))
            elif data_type == "Balance Sheet":
                self.update_financial_table("balance_table", data.get("data", {}))
            elif data_type == "Cash Flow":
                self.update_financial_table("cashflow_table", data.get("data", {}))

        except Exception as e:
            print(f"❌ DEBUG: Error in update_fundamentals_display(): {str(e)}")

    def update_company_info_table(self, data: Dict[str, Any]):
        """Update company information table"""
        try:
            table_tag = f"company_info_table_{self.tab_id}"

            # Clear existing rows
            if dpg.does_item_exist(table_tag):
                children = dpg.get_item_children(table_tag, 1)
                for child in children:
                    dpg.delete_item(child)

            # Add company data rows
            key_fields = [
                "Name", "Symbol", "Description", "Exchange", "Currency", "Country",
                "Sector", "Industry", "MarketCapitalization", "PERatio", "PEGRatio",
                "BookValue", "DividendPerShare", "DividendYield", "EPS", "RevenuePerShareTTM",
                "ProfitMargin", "OperatingMarginTTM", "ReturnOnAssetsTTM", "ReturnOnEquityTTM"
            ]

            for field in key_fields:
                if field in data:
                    with dpg.table_row(parent=table_tag):
                        dpg.add_text(field.replace("TTM", " (TTM)"))
                        value = data[field]
                        if field == "MarketCapitalization" and value.isdigit():
                            value = f"${int(value):,}"
                        elif field in ["PERatio", "PEGRatio", "BookValue", "DividendPerShare",
                                       "EPS"] and value != "None":
                            try:
                                value = f"{float(value):.2f}"
                            except:
                                pass
                        dpg.add_text(str(value))

        except Exception as e:
            print(f"❌ DEBUG: Error in update_company_info_table(): {str(e)}")

    def update_financial_table(self, table_prefix: str, data: Dict[str, Any]):
        """Update financial statement table"""
        try:
            table_tag = f"{table_prefix}_{self.tab_id}"

            # Clear existing rows
            if dpg.does_item_exist(table_tag):
                children = dpg.get_item_children(table_tag, 1)
                for child in children:
                    dpg.delete_item(child)

            # Add financial data rows
            for key, value in data.items():
                if key not in ["symbol", "fiscalDateEnding", "reportedCurrency"]:
                    with dpg.table_row(parent=table_tag):
                        dpg.add_text(key.replace("TTM", " (TTM)"))
                        # Format large numbers
                        if isinstance(value, str) and value.isdigit():
                            formatted_value = f"${int(value):,}"
                        else:
                            formatted_value = str(value)
                        dpg.add_text(formatted_value)

        except Exception as e:
            print(f"❌ DEBUG: Error in update_financial_table(): {str(e)}")

    def update_technical_display(self, data: Dict[str, Any], data_type: str):
        """Update technical analysis display"""
        try:
            table_tag = f"technical_table_{self.tab_id}"

            # Clear existing rows
            if dpg.does_item_exist(table_tag):
                children = dpg.get_item_children(table_tag, 1)
                for child in children:
                    dpg.delete_item(child)

            if "data" not in data:
                return

            tech_data = data["data"]
            timestamps = tech_data.get("timestamps", [])

            # Process technical indicator data
            for key, values in tech_data.items():
                if key != "timestamps" and isinstance(values, list):
                    # Show last 10 values
                    for i in range(min(10, len(values))):
                        idx = len(values) - 1 - i
                        if idx < len(timestamps):
                            with dpg.table_row(parent=table_tag):
                                dpg.add_text(timestamps[idx])
                                dpg.add_text(data_type + f" ({key})")
                                dpg.add_text(
                                    f"{values[idx]:.4f}" if isinstance(values[idx], (int, float)) else str(values[idx]))
                                # Simple signal logic
                                signal = self.calculate_signal(key, values[idx],
                                                               values[idx - 1] if idx > 0 else values[idx])
                                dpg.add_text(signal)

        except Exception as e:
            print(f"❌ DEBUG: Error in update_technical_display(): {str(e)}")

    def calculate_signal(self, indicator: str, current: float, previous: float) -> str:
        """Calculate basic signal from indicator values"""
        try:
            if not isinstance(current, (int, float)) or not isinstance(previous, (int, float)):
                return "N/A"

            if indicator.upper() in ["RSI"]:
                if current > 70:
                    return "🔴 Overbought"
                elif current < 30:
                    return "🟢 Oversold"
                else:
                    return "🟡 Neutral"
            elif "MACD" in indicator.upper():
                if current > previous:
                    return "🟢 Bullish"
                elif current < previous:
                    return "🔴 Bearish"
                else:
                    return "🟡 Neutral"
            else:
                if current > previous:
                    return "⬆️ Rising"
                elif current < previous:
                    return "⬇️ Falling"
                else:
                    return "➡️ Flat"

        except Exception as e:
            return "N/A"

    def update_raw_display(self, data: Dict[str, Any]):
        """Update raw data display"""
        try:
            import json
            raw_text = json.dumps(data, indent=2, default=str)
            dpg.set_value(f"raw_data_display_{self.tab_id}", raw_text)

        except Exception as e:
            print(f"❌ DEBUG: Error in update_raw_display(): {str(e)}")

    def update_provider_info(self, data: Dict[str, Any]):
        """Update provider information"""
        try:
            table_tag = f"provider_info_table_{self.tab_id}"

            # Clear existing rows
            if dpg.does_item_exist(table_tag):
                children = dpg.get_item_children(table_tag, 1)
                for child in children:
                    dpg.delete_item(child)

            # Add provider info
            provider_info = [
                ("Provider", data.get("source", "Unknown")),
                ("Status", "✅ Active" if data.get("success") else "❌ Error"),
                ("Data Points", str(len(data.get("data", {}).get("timestamps", []))) if "data" in data else "N/A"),
                ("Response Time", "< 1s"),  # Could be measured
                ("Last Updated", data.get("fetched_at", "Unknown"))
            ]

            for key, value in provider_info:
                with dpg.table_row(parent=table_tag):
                    dpg.add_text(key)
                    dpg.add_text(value)

        except Exception as e:
            print(f"❌ DEBUG: Error in update_provider_info(): {str(e)}")

    def refresh_timeseries_view(self):
        """Refresh the time series view with current data"""
        try:
            if self.current_data and "data" in self.current_data:
                self.update_timeseries_display(self.current_data["data"])

        except Exception as e:
            print(f"❌ DEBUG: Error in refresh_timeseries_view(): {str(e)}")

    def copy_raw_data(self):
        """Copy raw data to clipboard"""
        try:
            if self.current_data:
                import json
                raw_text = json.dumps(self.current_data, indent=2, default=str)
                dpg.set_clipboard_text(raw_text)
                self.update_status("📋 Data copied to clipboard", [100, 255, 100])

        except Exception as e:
            print(f"❌ DEBUG: Error in copy_raw_data(): {str(e)}")
            self.update_status("❌ Failed to copy data", [255, 100, 100])

    def save_raw_data(self):
        """Save raw data to file"""
        try:
            if self.current_data:
                import json
                from datetime import datetime

                filename = f"financial_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
                with open(filename, 'w') as f:
                    json.dump(self.current_data, f, indent=2, default=str)

                self.update_status(f"💾 Data saved to {filename}", [100, 255, 100])

        except Exception as e:
            print(f"❌ DEBUG: Error in save_raw_data(): {str(e)}")
            self.update_status("❌ Failed to save data", [255, 100, 100])

    def export_data(self):
        """Export current data to CSV"""
        try:
            if not self.current_data or "data" not in self.current_data:
                self.update_status("❌ No data to export", [255, 100, 100])
                return

            data = self.current_data["data"]
            if "timestamps" not in data:
                self.update_status("❌ No time series data to export", [255, 100, 100])
                return

            import csv
            from datetime import datetime

            filename = f"financial_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

            with open(filename, 'w', newline='') as csvfile:
                writer = csv.writer(csvfile)

                # Write header
                headers = ["timestamp"]
                for key in data.keys():
                    if key != "timestamps":
                        headers.append(key)
                writer.writerow(headers)

                # Write data rows
                timestamps = data["timestamps"]
                for i in range(len(timestamps)):
                    row = [timestamps[i]]
                    for key in data.keys():
                        if key != "timestamps":
                            values = data[key]
                            row.append(values[i] if i < len(values) else "")
                    writer.writerow(row)

            self.update_status(f"📊 Data exported to {filename}", [100, 255, 100])

        except Exception as e:
            print(f"❌ DEBUG: Error in export_data(): {str(e)}")
            self.update_status("❌ Failed to export data", [255, 100, 100])

    def toggle_auto_refresh(self, sender, value):
        """Toggle auto refresh functionality"""
        try:
            self.auto_refresh = value
            if self.auto_refresh:
                self.update_status("🔄 Auto refresh enabled", [100, 255, 100])
                self.start_auto_refresh()
            else:
                self.update_status("⏹️ Auto refresh disabled", [200, 200, 200])
                self.stop_auto_refresh()

        except Exception as e:
            print(f"❌ DEBUG: Error in toggle_auto_refresh(): {str(e)}")

    def start_auto_refresh(self):
        """Start auto refresh thread"""
        try:
            if self.refresh_thread and self.refresh_thread.is_alive():
                return

            self._stop_refresh = False
            self.refresh_thread = threading.Thread(target=self._auto_refresh_worker)
            self.refresh_thread.daemon = True
            self.refresh_thread.start()

        except Exception as e:
            print(f"❌ DEBUG: Error in start_auto_refresh(): {str(e)}")

    def stop_auto_refresh(self):
        """Stop auto refresh thread"""
        try:
            self._stop_refresh = True
            if self.refresh_thread:
                self.refresh_thread.join(timeout=1.0)

        except Exception as e:
            print(f"❌ DEBUG: Error in stop_auto_refresh(): {str(e)}")

    def _auto_refresh_worker(self):
        """Auto refresh worker thread"""
        try:
            while not self._stop_refresh and self.auto_refresh:
                time.sleep(self.refresh_interval)
                if not self._stop_refresh and self.auto_refresh:
                    self.fetch_data()

        except Exception as e:
            print(f"❌ DEBUG: Error in _auto_refresh_worker(): {str(e)}")

    def clear_data(self):
        """Clear all displayed data"""
        try:
            self.current_data = {}

            # Clear overview display
            for tag_suffix in ["price", "change", "volume", "high", "low"]:
                tag = f"overview_{tag_suffix}_{self.tab_id}"
                if dpg.does_item_exist(tag):
                    dpg.set_value(tag, "N/A")

            # Clear tables
            for table_name in ["overview_info_table", "timeseries_table", "company_info_table",
                               "income_table", "balance_table", "cashflow_table", "technical_table"]:
                table_tag = f"{table_name}_{self.tab_id}"
                if dpg.does_item_exist(table_tag):
                    children = dpg.get_item_children(table_tag, 1)
                    for child in children:
                        dpg.delete_item(child)

            # Clear raw display
            dpg.set_value(f"raw_data_display_{self.tab_id}", "No data loaded yet...")

            self.update_status("🧹 Data cleared", [200, 200, 200])
            dpg.set_value(f"last_update_{self.tab_id}", "Never")

        except Exception as e:
            print(f"❌ DEBUG: Error in clear_data(): {str(e)}")

    def update_status(self, message: str, color: List[int] = None):
        """Update status message"""
        try:
            if color is None:
                color = [200, 200, 200]

            status_tag = f"status_text_{self.tab_id}"
            if dpg.does_item_exist(status_tag):
                dpg.set_value(status_tag, message)
                dpg.configure_item(status_tag, color=color)

        except Exception as e:
            print(f"❌ DEBUG: Error in update_status(): {str(e)}")

    def cleanup(self):
        """Clean up resources"""
        try:
            self.stop_auto_refresh()
            self.current_data = {}
            print("✅ DEBUG: Enhanced Data Viewer tab cleanup completed")

        except Exception as e:
            print(f"❌ DEBUG: Error during cleanup: {str(e)}")