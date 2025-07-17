# yfinance_data_tab.py - Fixed Version with Default AAPL Loading
# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
import yfinance as yf
import pandas as pd
import numpy as np
from datetime import datetime, timedelta
import threading
import time
from fincept_terminal.Utils.Managers.theme_manager import AutomaticThemeManager


class YFinanceDataTab:
    """Fixed Bloomberg Terminal YFinance Data Tab with Default AAPL Loading"""

    def __init__(self, main_app=None):
        self.main_app = main_app
        self.current_ticker = None
        self.stock_data = None
        self.history_data = None
        self.financials_data = {}
        self.is_loading = False

        # Bloomberg Terminal Colors
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [0, 128, 255]

        # Initialize theme manager
        try:
            self.theme_manager = AutomaticThemeManager()
        except:
            self.theme_manager = None

    def get_label(self):
        return "Equity Research"

    def create_content(self):
        """Create the complete YFinance data interface"""
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

        except Exception as e:
            print(f"Error creating YFinance content: {e}")
            dpg.add_text(f"ERROR: {e}", color=self.BLOOMBERG_RED)

    def load_default_ticker(self):
        """Load AAPL ticker by default"""
        try:
            dpg.set_value("ticker_search_input", "AAPL")
            self.handle_search()
        except Exception as e:
            print(f"Error loading default ticker: {e}")

    def create_header_section(self):
        """Create header with search functionality"""
        with dpg.group(horizontal=True):
            dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("MARKET DATA", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)

            dpg.add_text("TICKER:", color=self.BLOOMBERG_YELLOW)
            dpg.add_input_text(
                width=120,
                hint="Enter ticker",
                tag="ticker_search_input",
                on_enter=True,
                callback=self.handle_search,
                default_value="AAPL"  # Set AAPL as default value
            )
            dpg.add_button(
                label="SEARCH",
                callback=self.handle_search,
                width=70,
                height=25,
                tag="search_button"
            )

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("TIME:", color=self.BLOOMBERG_YELLOW)
            dpg.add_text(datetime.now().strftime('%H:%M:%S'), tag="header_time", color=self.BLOOMBERG_WHITE)

        dpg.add_separator()

        # Current stock display
        with dpg.group(tag="current_stock_display", show=False):
            with dpg.group(horizontal=True):
                dpg.add_text("CURRENT:", color=self.BLOOMBERG_YELLOW)
                dpg.add_text("", tag="current_stock_name", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("PRICE:", color=self.BLOOMBERG_YELLOW)
                dpg.add_text("", tag="current_stock_price", color=self.BLOOMBERG_GREEN)
                dpg.add_text("", tag="current_stock_change", color=self.BLOOMBERG_GREEN)
            dpg.add_separator()

    def create_company_overview_section(self):
        """Create company overview section"""
        with dpg.collapsing_header(label="COMPANY OVERVIEW", default_open=True, tag="company_overview_header"):
            with dpg.group(horizontal=True):
                # Company Information - Light Blue
                with dpg.child_window(width=380, height=220, border=True):
                    dpg.add_text("COMPANY INFORMATION", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    dpg.add_text("Loading AAPL data...",
                                 color=[173, 216, 230], wrap=360, tag="company_info_text")  # Light Blue

                # Business Summary - Light Green
                with dpg.child_window(width=500, height=220, border=True):
                    dpg.add_text("BUSINESS SUMMARY", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    dpg.add_text("Loading AAPL data...",
                                 color=[144, 238, 144], wrap=480, tag="business_summary_text")  # Light Green

                # Key Metrics - Light Coral
                with dpg.child_window(width=320, height=220, border=True):
                    dpg.add_text("KEY METRICS", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    dpg.add_text("Loading AAPL data...",
                                 color=[240, 128, 128], tag="key_metrics_text")  # Light Coral

    def create_market_data_section(self):
        """Create market data section"""
        with dpg.collapsing_header(label="MARKET DATA & PERFORMANCE", default_open=True, tag="market_data_header"):
            with dpg.group(horizontal=True):
                # Price Data - Light Cyan
                with dpg.child_window(width=300, height=200, border=True):
                    dpg.add_text("PRICE DATA", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    dpg.add_text("Loading AAPL data...", color=[224, 255, 255], tag="price_data_text")  # Light Cyan

                # Trading Metrics - Light Yellow
                with dpg.child_window(width=300, height=200, border=True):
                    dpg.add_text("TRADING METRICS", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    dpg.add_text("Loading AAPL data...", color=[255, 255, 224], tag="trading_metrics_text")  # Light Yellow

                # Valuation Metrics - Light Pink
                with dpg.child_window(width=300, height=200, border=True):
                    dpg.add_text("VALUATION METRICS", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    dpg.add_text("Loading AAPL data...", color=[255, 182, 193], tag="valuation_metrics_text")  # Light Pink

                # Executive Team - Keep original color for now
                with dpg.child_window(width=400, height=200, border=True):
                    dpg.add_text("EXECUTIVE TEAM", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    dpg.add_text("Loading AAPL data...", color=self.BLOOMBERG_YELLOW, tag="executives_text")

    def create_price_and_returns_section(self):
        """Create price chart and returns section"""
        with dpg.collapsing_header(label="PRICE CHART & RETURNS", default_open=True, tag="price_returns_header"):
            # Chart controls
            with dpg.group(horizontal=True):
                dpg.add_text("PERIOD:", color=self.BLOOMBERG_YELLOW)
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
                dpg.add_text("CHART TYPE:", color=self.BLOOMBERG_YELLOW)
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
                    dpg.add_text("PRICE CHART", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()

                    # Chart container
                    with dpg.group(tag="chart_container"):
                        dpg.add_text("Loading AAPL chart data...",
                                     color=self.BLOOMBERG_YELLOW, tag="chart_placeholder")

                # Returns Table
                with dpg.child_window(width=400, height=400, border=True):
                    dpg.add_text("HISTORICAL RETURNS", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()

                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                                   borders_innerV=True, tag="returns_performance_table"):
                        dpg.add_table_column(label="Period", width_fixed=True, init_width_or_weight=80)
                        dpg.add_table_column(label="Return", width_fixed=True, init_width_or_weight=100)
                        dpg.add_table_column(label="Status", width_fixed=True, init_width_or_weight=80)

                        periods = ["1D", "7D", "15D", "30D", "60D", "90D", "180D", "365D"]
                        for i, period in enumerate(periods):
                            with dpg.table_row(tag=f"return_row_{i}"):
                                dpg.add_text(period, color=self.BLOOMBERG_YELLOW)
                                dpg.add_text("Loading...", tag=f"return_value_{i}", color=self.BLOOMBERG_YELLOW)
                                dpg.add_text("--", tag=f"return_status_{i}", color=self.BLOOMBERG_GRAY)

    def create_financial_statements_section(self):
        """Create financial statements section"""
        with dpg.collapsing_header(label="FINANCIAL STATEMENTS", default_open=True, tag="financials_header"):
            with dpg.tab_bar(tag="financial_statements_tabs"):
                # Income Statement Tab
                with dpg.tab(label="INCOME STATEMENT"):
                    with dpg.child_window(height=400, border=True, horizontal_scrollbar=True,
                                          tag="income_statement_container"):
                        dpg.add_text("INCOME STATEMENT DATA", color=self.BLOOMBERG_ORANGE)
                        dpg.add_separator()
                        dpg.add_text("Loading AAPL financial data...",
                                     color=self.BLOOMBERG_YELLOW, tag="income_statement_placeholder")

                # Balance Sheet Tab
                with dpg.tab(label="BALANCE SHEET"):
                    with dpg.child_window(height=400, border=True, horizontal_scrollbar=True,
                                          tag="balance_sheet_container"):
                        dpg.add_text("BALANCE SHEET DATA", color=self.BLOOMBERG_ORANGE)
                        dpg.add_separator()
                        dpg.add_text("Loading AAPL financial data...",
                                     color=self.BLOOMBERG_YELLOW, tag="balance_sheet_placeholder")

                # Cash Flow Tab
                with dpg.tab(label="CASH FLOW STATEMENT"):
                    with dpg.child_window(height=400, border=True, horizontal_scrollbar=True,
                                          tag="cash_flow_container"):
                        dpg.add_text("CASH FLOW STATEMENT DATA", color=self.BLOOMBERG_ORANGE)
                        dpg.add_separator()
                        dpg.add_text("Loading AAPL financial data...",
                                     color=self.BLOOMBERG_YELLOW, tag="cash_flow_placeholder")

    def create_financial_charts_section(self):
        """Create financial analysis charts section"""
        with dpg.collapsing_header(label="FINANCIAL ANALYSIS CHARTS", default_open=True,
                                   tag="financial_charts_header"):
            # Chart controls
            with dpg.group(horizontal=True):
                dpg.add_text("CHART CONTROLS:", color=self.BLOOMBERG_ORANGE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("YEARS:", color=self.BLOOMBERG_YELLOW)
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

            with dpg.group(horizontal=True):
                # Revenue & Profitability Chart
                with dpg.child_window(width=600, height=350, border=True):
                    dpg.add_text("REVENUE & PROFITABILITY TRENDS", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    with dpg.group(tag="revenue_chart_container"):
                        dpg.add_text("Loading AAPL financial charts...", color=self.BLOOMBERG_YELLOW)

                # Balance Sheet Trends Chart
                with dpg.child_window(width=600, height=350, border=True):
                    dpg.add_text("BALANCE SHEET TRENDS", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    with dpg.group(tag="balance_chart_container"):
                        dpg.add_text("Loading AAPL financial charts...", color=self.BLOOMBERG_YELLOW)

            dpg.add_spacer(height=15)

            with dpg.group(horizontal=True):
                # Cash Flow Analysis Chart
                with dpg.child_window(width=600, height=350, border=True):
                    dpg.add_text("CASH FLOW ANALYSIS", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    with dpg.group(tag="cashflow_chart_container"):
                        dpg.add_text("Loading AAPL financial charts...", color=self.BLOOMBERG_YELLOW)

                # Financial Ratios Chart
                with dpg.child_window(width=600, height=350, border=True):
                    dpg.add_text("FINANCIAL RATIOS & MARGINS", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()
                    with dpg.group(tag="ratios_chart_container"):
                        dpg.add_text("Loading AAPL financial charts...", color=self.BLOOMBERG_YELLOW)

    def create_status_bar(self):
        """Create status bar"""
        dpg.add_separator()
        with dpg.group(horizontal=True):
            dpg.add_text("●", color=self.BLOOMBERG_GREEN)
            dpg.add_text("DATA SERVICE ONLINE", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("STATUS:", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("INITIALIZING...", color=self.BLOOMBERG_YELLOW, tag="main_status_text")
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LAST UPDATE:", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("Never", color=self.BLOOMBERG_WHITE, tag="last_update_time")

    def handle_search(self, sender=None, app_data=None):
        """Handle search functionality"""
        ticker = dpg.get_value("ticker_search_input").strip().upper()

        if not ticker:
            dpg.set_value("main_status_text", "ERROR: ENTER TICKER")
            return

        if self.is_loading:
            dpg.set_value("main_status_text", "LOADING IN PROGRESS...")
            return

        self.is_loading = True
        self.current_ticker = ticker

        dpg.set_value("main_status_text", f"LOADING {ticker}...")
        dpg.configure_item("search_button", enabled=False)

        threading.Thread(target=self.load_complete_stock_data, args=(ticker,), daemon=True).start()

    def load_complete_stock_data(self, ticker):
        """Load complete stock data including financials"""
        try:
            stock = yf.Ticker(ticker)

            # Get basic info
            dpg.set_value("main_status_text", f"LOADING {ticker} INFO...")
            info = stock.info

            if not info or len(info) < 5:
                raise Exception("Invalid ticker or no data available")

            self.stock_data = info

            # Get historical data
            dpg.set_value("main_status_text", f"LOADING {ticker} PRICE HISTORY...")
            history = stock.history(period="2y")

            if history.empty:
                raise Exception("No historical data available")

            self.history_data = history

            # Update UI with basic data
            self.update_all_data_sections(info)

            # Load financial statements
            dpg.set_value("main_status_text", f"LOADING {ticker} FINANCIALS...")
            self.load_financial_data(stock)

            dpg.set_value("main_status_text", f"{ticker} LOADED")
            dpg.set_value("last_update_time", datetime.now().strftime('%H:%M:%S'))

        except Exception as e:
            error_msg = str(e)[:30] + "..." if len(str(e)) > 30 else str(e)
            dpg.set_value("main_status_text", f"ERROR: {error_msg}")
            print(f"Error loading {ticker}: {e}")

        finally:
            self.is_loading = False
            dpg.configure_item("search_button", enabled=True)

    def update_all_data_sections(self, info):
        """Update all data sections with stock info"""
        try:
            dpg.show_item("current_stock_display")

            # Update header stock info
            company_name = info.get('shortName', info.get('longName', 'Unknown Company'))
            dpg.set_value("current_stock_name", f"{self.current_ticker} - {company_name}")

            # Update current price and change
            current_price = info.get('currentPrice', info.get('regularMarketPrice', 0))
            previous_close = info.get('previousClose', info.get('regularMarketPreviousClose', 0))

            if current_price and previous_close:
                change = current_price - previous_close
                change_pct = (change / previous_close) * 100 if previous_close != 0 else 0

                price_color = self.BLOOMBERG_GREEN if change >= 0 else self.BLOOMBERG_RED

                dpg.set_value("current_stock_price", f"${current_price:.2f}")
                dpg.configure_item("current_stock_price", color=price_color)

                change_text = f"{change:+.2f} ({change_pct:+.2f}%)"
                dpg.set_value("current_stock_change", change_text)
                dpg.configure_item("current_stock_change", color=price_color)

            # Update company information
            self.update_company_information(info)
            # Update market data
            self.update_market_data_info(info)
            # Update price chart and returns
            self.update_price_chart()
            self.update_returns_table()

        except Exception as e:
            print(f"Error updating data sections: {e}")

    def update_company_information(self, info):
        """Update company information section - FIXED VERSION"""
        try:
            # Company Information - Fixed formatting issue
            employees = info.get('fullTimeEmployees', 'N/A')
            if isinstance(employees, int):
                employees_str = f"{employees:,}"
            else:
                employees_str = str(employees)

            company_info = f"""Company: {info.get('longName', 'N/A')}
Sector: {info.get('sector', 'N/A')}
Industry: {info.get('industry', 'N/A')}
Country: {info.get('country', 'N/A')}
Website: {info.get('website', 'N/A')}
Phone: {info.get('phone', 'N/A')}
Employees: {employees_str}
Exchange: {info.get('exchange', 'N/A')}
Currency: {info.get('currency', 'N/A')}
Address: {info.get('address1', 'N/A')}, {info.get('city', 'N/A')}"""

            dpg.set_value("company_info_text", company_info)
            dpg.configure_item("company_info_text", color=[173, 216, 230])  # Light Blue

            # Business Summary
            business_summary = info.get('longBusinessSummary', 'No business summary available.')
            if len(business_summary) > 1200:
                business_summary = business_summary[:1200] + "..."

            dpg.set_value("business_summary_text", business_summary)
            dpg.configure_item("business_summary_text", color=[144, 238, 144])  # Light Green

            # Key Metrics - Fixed formatting
            def safe_format(value, is_percentage=False, is_currency=False):
                try:
                    if value is None or pd.isna(value):
                        return "N/A"
                    if is_percentage:
                        return f"{float(value) * 100:.2f}%" if value != 0 else "0.00%"
                    elif is_currency:
                        return f"${float(value):,.0f}" if value != 0 else "$0"
                    else:
                        return f"{float(value):.2f}" if value != 0 else "0.00"
                except:
                    return "N/A"

            key_metrics = f"""Market Cap: {safe_format(info.get('marketCap'), is_currency=True)}
Enterprise Value: {safe_format(info.get('enterpriseValue'), is_currency=True)}
P/E Ratio: {safe_format(info.get('trailingPE'))}
Forward P/E: {safe_format(info.get('forwardPE'))}
PEG Ratio: {safe_format(info.get('pegRatio'))}
Price/Sales: {safe_format(info.get('priceToSalesTrailing12Months'))}
Price/Book: {safe_format(info.get('priceToBook'))}
Beta: {safe_format(info.get('beta'))}
ROE: {safe_format(info.get('returnOnEquity'), is_percentage=True)}
ROA: {safe_format(info.get('returnOnAssets'), is_percentage=True)}
Debt/Equity: {safe_format(info.get('debtToEquity'))}
Profit Margin: {safe_format(info.get('profitMargins'), is_percentage=True)}"""

            dpg.set_value("key_metrics_text", key_metrics)
            dpg.configure_item("key_metrics_text", color=[240, 128, 128])  # Light Coral

        except Exception as e:
            print(f"Error updating company information: {e}")

    def update_market_data_info(self, info):
        """Update market data information"""
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
            price_data = f"""Current Price: {safe_format_value(info.get('currentPrice'))}
Previous Close: {safe_format_value(info.get('previousClose'))}
Open: {safe_format_value(info.get('open'))}
Day Low: {safe_format_value(info.get('dayLow'))}
Day High: {safe_format_value(info.get('dayHigh'))}
52W Low: {safe_format_value(info.get('fiftyTwoWeekLow'))}
52W High: {safe_format_value(info.get('fiftyTwoWeekHigh'))}
Volume: {safe_format_value(info.get('volume'), False)}
Avg Volume: {safe_format_value(info.get('averageVolume'), False)}"""

            dpg.set_value("price_data_text", price_data)
            dpg.configure_item("price_data_text", color=[224, 255, 255])  # Light Cyan

            # Trading Metrics
            trading_metrics = f"""Market Cap: {safe_format_value(info.get('marketCap'))}
Beta: {safe_format_value(info.get('beta'))}
Dividend Rate: {safe_format_value(info.get('dividendRate'))}
Dividend Yield: {safe_format_value(info.get('dividendYield'), False, True)}
Shares Outstanding: {safe_format_value(info.get('sharesOutstanding'), False)}
Float Shares: {safe_format_value(info.get('floatShares'), False)}"""

            dpg.set_value("trading_metrics_text", trading_metrics)
            dpg.configure_item("trading_metrics_text", color=[255, 255, 224])  # Light Yellow

            # Valuation Metrics
            valuation_metrics = f"""P/E Ratio (TTM): {safe_format_value(info.get('trailingPE'), False)}
Forward P/E: {safe_format_value(info.get('forwardPE'), False)}
PEG Ratio: {safe_format_value(info.get('pegRatio'), False)}
Price/Sales (TTM): {safe_format_value(info.get('priceToSalesTrailing12Months'), False)}
Price/Book: {safe_format_value(info.get('priceToBook'), False)}
Book Value: {safe_format_value(info.get('bookValue'))}
Target Price: {safe_format_value(info.get('targetMeanPrice'))}"""

            dpg.set_value("valuation_metrics_text", valuation_metrics)
            dpg.configure_item("valuation_metrics_text", color=[255, 182, 193])  # Light Pink

            # Executive Team - Keep original Bloomberg Yellow
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

                dpg.set_value("executives_text", exec_text)
                dpg.configure_item("executives_text", color=self.BLOOMBERG_YELLOW)  # Keep Bloomberg Yellow
            else:
                dpg.set_value("executives_text", "No executive information available.")
                dpg.configure_item("executives_text", color=self.BLOOMBERG_YELLOW)

        except Exception as e:
            print(f"Error updating market data: {e}")

    def update_price_chart(self, sender=None, app_data=None):
        """Update price chart with historical data - PROPER TRADINGVIEW-STYLE CANDLESTICK"""
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

                # Filter data based on period
                if period == "1D":
                    data = self.history_data.tail(1)
                elif period == "5D":
                    data = self.history_data.tail(5)
                elif period == "1M":
                    data = self.history_data.tail(22)
                elif period == "3M":
                    data = self.history_data.tail(66)
                elif period == "6M":
                    data = self.history_data.tail(132)
                elif period == "1Y":
                    data = self.history_data.tail(252)
                else:  # 2Y
                    data = self.history_data

                if data.empty:
                    dpg.add_text("No data available for selected period",
                                 color=self.BLOOMBERG_RED, parent="chart_container")
                    return

                # Create new plot
                with dpg.plot(height=350, width=-1, parent="chart_container", tag="main_price_chart"):
                    dpg.add_plot_legend()
                    x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                    y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Price ($)")

                    if chart_type == "Candlestick":
                        # Prepare OHLC data for proper candlestick chart
                        dates = list(range(len(data)))
                        opens = data['Open'].tolist()
                        highs = data['High'].tolist()
                        lows = data['Low'].tolist()
                        closes = data['Close'].tolist()

                        # Create OHLC data arrays for candlestick series
                        # Format: [date, open, high, low, close] for each candle
                        ohlc_data = []
                        for i in range(len(dates)):
                            ohlc_data.append([dates[i], opens[i], highs[i], lows[i], closes[i]])

                        # Add proper candlestick series
                        dpg.add_candle_series(
                            dates, opens, closes, lows, highs,
                            label="OHLC",
                            parent=y_axis,
                            bull_color=[0, 255, 0, 255],  # Green for bullish candles
                            bear_color=[255, 0, 0, 255],  # Red for bearish candles
                            weight=0.75,  # Candle width
                            tooltip=True
                        )

                        # Add volume bars at bottom if available
                        if 'Volume' in data.columns:
                            try:
                                # Create secondary y-axis for volume
                                y_axis_vol = dpg.add_plot_axis(dpg.mvYAxis, label="Volume", no_gridlines=True)
                                dpg.set_axis_limits(y_axis_vol, 0, max(data['Volume']) * 1.2)

                                volumes = data['Volume'].tolist()

                                # Color volume bars based on price movement
                                bull_volumes = []
                                bear_volumes = []
                                bull_dates = []
                                bear_dates = []

                                for i in range(len(dates)):
                                    if closes[i] >= opens[i]:  # Bullish
                                        bull_volumes.append(volumes[i])
                                        bull_dates.append(dates[i])
                                        bear_volumes.append(0)
                                        bear_dates.append(dates[i])
                                    else:  # Bearish
                                        bear_volumes.append(volumes[i])
                                        bear_dates.append(dates[i])
                                        bull_volumes.append(0)
                                        bull_dates.append(dates[i])

                                # Add volume bars
                                if bull_volumes:
                                    dpg.add_bar_series(bull_dates, bull_volumes, label="Bull Volume",
                                                       parent=y_axis_vol, weight=0.8)
                                if bear_volumes:
                                    dpg.add_bar_series(bear_dates, bear_volumes, label="Bear Volume",
                                                       parent=y_axis_vol, weight=0.8)

                            except Exception as vol_error:
                                print(f"Volume chart error (non-critical): {vol_error}")

                    else:
                        # Simple line chart for comparison
                        dates = list(range(len(data)))
                        closes = data['Close'].tolist()

                        # Add moving averages for better analysis
                        if len(closes) >= 20:
                            # 20-day moving average
                            ma20 = []
                            for i in range(len(closes)):
                                if i >= 19:
                                    ma20.append(sum(closes[i - 19:i + 1]) / 20)
                                else:
                                    ma20.append(closes[i])

                            dpg.add_line_series(dates, ma20, label="MA20", parent=y_axis)

                        if len(closes) >= 50:
                            # 50-day moving average
                            ma50 = []
                            for i in range(len(closes)):
                                if i >= 49:
                                    ma50.append(sum(closes[i - 49:i + 1]) / 50)
                                else:
                                    ma50.append(closes[i])

                            dpg.add_line_series(dates, ma50, label="MA50", parent=y_axis)

                        # Main price line
                        dpg.add_line_series(dates, closes, label="Close Price", parent=y_axis)

                    # Set axis limits for better view
                    if data is not None and not data.empty:
                        price_min = min(data['Low'].min(), data['Close'].min()) * 0.98
                        price_max = max(data['High'].max(), data['Close'].max()) * 1.02
                        dpg.set_axis_limits(y_axis, price_min, price_max)

        except Exception as e:
            print(f"Error updating price chart: {e}")
            # Fallback: create simple text message
            if dpg.does_item_exist("chart_container"):
                dpg.delete_item("chart_container", children_only=True)
                dpg.add_text(f"Chart Error: {str(e)[:50]}...",
                             color=self.BLOOMBERG_RED, parent="chart_container")

    def update_returns_table(self):
        """Update historical returns table"""
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

                        dpg.set_value(f"return_value_{i}", f"{return_pct:+.2f}%")
                        dpg.configure_item(f"return_value_{i}", color=color)
                        dpg.set_value(f"return_status_{i}", status)
                        dpg.configure_item(f"return_status_{i}", color=color)
                    else:
                        dpg.set_value(f"return_value_{i}", "N/A")
                        dpg.configure_item(f"return_value_{i}", color=self.BLOOMBERG_GRAY)
                        dpg.set_value(f"return_status_{i}", "--")
                        dpg.configure_item(f"return_status_{i}", color=self.BLOOMBERG_GRAY)
                except Exception as e:
                    dpg.set_value(f"return_value_{i}", "Error")
                    dpg.configure_item(f"return_value_{i}", color=self.BLOOMBERG_RED)

        except Exception as e:
            print(f"Error updating returns table: {e}")

    def load_financial_data(self, stock):
        """Load and display financial statements"""
        try:
            # Get financial statements
            income_stmt = stock.financials
            balance_sheet = stock.balance_sheet
            cash_flow = stock.cashflow

            # Store for future use
            self.financials_data = {
                'income_statement': income_stmt,
                'balance_sheet': balance_sheet,
                'cash_flow': cash_flow
            }

            # Update financial statement tables
            self.create_financial_table(income_stmt, "income_statement_container", "Income Statement")
            self.create_financial_table(balance_sheet, "balance_sheet_container", "Balance Sheet")
            self.create_financial_table(cash_flow, "cash_flow_container", "Cash Flow Statement")

            # Update financial charts
            self.update_financial_charts()

        except Exception as e:
            print(f"Error loading financial data: {e}")
            # Update placeholders with error messages
            dpg.set_value("income_statement_placeholder", f"Error loading income statement: {str(e)[:50]}...")
            dpg.set_value("balance_sheet_placeholder", f"Error loading balance sheet: {str(e)[:50]}...")
            dpg.set_value("cash_flow_placeholder", f"Error loading cash flow: {str(e)[:50]}...")

    def create_financial_table(self, df, container_tag, title):
        """Create financial statement table"""
        try:
            if df is None or df.empty:
                return

            # Clear container
            dpg.delete_item(container_tag, children_only=True)

            # Add title and separator
            dpg.add_text(f"{title.upper()} DATA", color=self.BLOOMBERG_ORANGE, parent=container_tag)
            dpg.add_separator(parent=container_tag)

            # Create table
            with dpg.table(parent=container_tag, header_row=True, borders_innerH=True,
                           borders_outerH=True, borders_innerV=True, borders_outerV=True,
                           scrollY=True, scrollX=True, height=330):

                # Add columns
                dpg.add_table_column(label="Line Item", width_fixed=True, init_width_or_weight=250)

                # Add year columns (limit to 4 most recent years)
                years = df.columns[:4] if len(df.columns) > 4 else df.columns
                for year in years:
                    year_str = year.strftime('%Y') if hasattr(year, 'strftime') else str(year)[:4]
                    dpg.add_table_column(label=year_str, width_fixed=True, init_width_or_weight=130)

                # Add data rows (limit to top 20 items for better performance)
                for idx in df.index[:20]:
                    with dpg.table_row():
                        # Line item name
                        dpg.add_text(str(idx), color=self.BLOOMBERG_YELLOW)

                        # Values for each year
                        for year in years:
                            try:
                                value = df.loc[idx, year]
                                if pd.isna(value):
                                    dpg.add_text("N/A", color=self.BLOOMBERG_GRAY)
                                elif abs(value) >= 1e12:
                                    dpg.add_text(f"${value / 1e12:.2f}T", color=self.BLOOMBERG_WHITE)
                                elif abs(value) >= 1e9:
                                    dpg.add_text(f"${value / 1e9:.2f}B", color=self.BLOOMBERG_WHITE)
                                elif abs(value) >= 1e6:
                                    dpg.add_text(f"${value / 1e6:.2f}M", color=self.BLOOMBERG_WHITE)
                                elif abs(value) >= 1e3:
                                    dpg.add_text(f"${value / 1e3:.2f}K", color=self.BLOOMBERG_WHITE)
                                else:
                                    dpg.add_text(f"${value:.2f}", color=self.BLOOMBERG_WHITE)
                            except Exception as e:
                                dpg.add_text("Error", color=self.BLOOMBERG_RED)

        except Exception as e:
            print(f"Error creating financial table for {title}: {e}")

    def update_financial_charts(self, sender=None, app_data=None):
        """Update all financial analysis charts"""
        try:
            if not self.financials_data:
                return

            # Get years selection
            years_selection = dpg.get_value("chart_years_combo") if dpg.does_item_exist(
                "chart_years_combo") else "Last 4 Years"

            # Update each chart
            self.create_revenue_profitability_chart(years_selection)
            self.create_balance_sheet_trends_chart(years_selection)
            self.create_cash_flow_analysis_chart(years_selection)
            self.create_financial_ratios_chart(years_selection)

        except Exception as e:
            print(f"Error updating financial charts: {e}")

    def create_revenue_profitability_chart(self, years_selection):
        """Create revenue and profitability trends chart"""
        try:
            income_stmt = self.financials_data.get('income_statement')
            if income_stmt is None or income_stmt.empty:
                return

            # Clear existing chart
            dpg.delete_item("revenue_chart_container", children_only=True)

            # Determine number of years
            if years_selection == "Last 2 Years":
                years_cols = income_stmt.columns[:2]
            elif years_selection == "Last 3 Years":
                years_cols = income_stmt.columns[:3]
            elif years_selection == "Last 4 Years":
                years_cols = income_stmt.columns[:4]
            else:  # All Years
                years_cols = income_stmt.columns

            with dpg.plot(height=300, width=-1, parent="revenue_chart_container"):
                dpg.add_plot_legend()
                x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Year")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Amount (Billions)")

                # Try to find revenue and income data
                revenue_items = ['Total Revenue', 'Revenue', 'Net Sales', 'Total Revenues']
                income_items = ['Net Income', 'Net Income Common Stockholders']

                # Revenue data
                for revenue_item in revenue_items:
                    if revenue_item in income_stmt.index:
                        revenue_data = income_stmt.loc[revenue_item]
                        years = []
                        revenues = []

                        for year in years_cols:
                            if year in revenue_data.index:
                                value = revenue_data[year]
                                if pd.notna(value) and value != 0:
                                    year_num = year.year if hasattr(year, 'year') else int(str(year)[:4])
                                    years.append(year_num)
                                    revenues.append(value / 1e9)  # Convert to billions

                        if years and revenues:
                            sorted_data = sorted(zip(years, revenues))
                            years, revenues = zip(*sorted_data)
                            dpg.add_line_series(list(years), list(revenues), label="Revenue (B)", parent=y_axis)
                        break

                # Net Income data
                for income_item in income_items:
                    if income_item in income_stmt.index:
                        income_data = income_stmt.loc[income_item]
                        years = []
                        incomes = []

                        for year in years_cols:
                            if year in income_data.index:
                                value = income_data[year]
                                if pd.notna(value):
                                    year_num = year.year if hasattr(year, 'year') else int(str(year)[:4])
                                    years.append(year_num)
                                    incomes.append(value / 1e9)  # Convert to billions

                        if years and incomes:
                            sorted_data = sorted(zip(years, incomes))
                            years, incomes = zip(*sorted_data)
                            dpg.add_line_series(list(years), list(incomes), label="Net Income (B)", parent=y_axis)
                        break

        except Exception as e:
            print(f"Error creating revenue/profitability chart: {e}")

    def create_balance_sheet_trends_chart(self, years_selection):
        """Create balance sheet trends chart"""
        try:
            balance_sheet = self.financials_data.get('balance_sheet')
            if balance_sheet is None or balance_sheet.empty:
                return

            # Clear existing chart
            dpg.delete_item("balance_chart_container", children_only=True)

            # Determine number of years
            if years_selection == "Last 2 Years":
                years_cols = balance_sheet.columns[:2]
            elif years_selection == "Last 3 Years":
                years_cols = balance_sheet.columns[:3]
            elif years_selection == "Last 4 Years":
                years_cols = balance_sheet.columns[:4]
            else:  # All Years
                years_cols = balance_sheet.columns

            with dpg.plot(height=300, width=-1, parent="balance_chart_container"):
                dpg.add_plot_legend()
                x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Year")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Amount (Billions)")

                # Key balance sheet items to chart
                bs_items = ['Total Assets', 'Total Stockholder Equity', 'Total Debt']
                labels = ['Total Assets (B)', 'Stockholder Equity (B)', 'Total Debt (B)']

                for i, item_name in enumerate(bs_items):
                    if item_name in balance_sheet.index:
                        item_data = balance_sheet.loc[item_name]
                        years = []
                        values = []

                        for year in years_cols:
                            if year in item_data.index:
                                value = item_data[year]
                                if pd.notna(value):
                                    year_num = year.year if hasattr(year, 'year') else int(str(year)[:4])
                                    years.append(year_num)
                                    values.append(value / 1e9)  # Convert to billions

                        if years and values:
                            sorted_data = sorted(zip(years, values))
                            years, values = zip(*sorted_data)
                            dpg.add_line_series(list(years), list(values), label=labels[i], parent=y_axis)

        except Exception as e:
            print(f"Error creating balance sheet chart: {e}")

    def create_cash_flow_analysis_chart(self, years_selection):
        """Create cash flow analysis chart"""
        try:
            cash_flow = self.financials_data.get('cash_flow')
            if cash_flow is None or cash_flow.empty:
                return

            # Clear existing chart
            dpg.delete_item("cashflow_chart_container", children_only=True)

            # Determine number of years
            if years_selection == "Last 2 Years":
                years_cols = cash_flow.columns[:2]
            elif years_selection == "Last 3 Years":
                years_cols = cash_flow.columns[:3]
            elif years_selection == "Last 4 Years":
                years_cols = cash_flow.columns[:4]
            else:  # All Years
                years_cols = cash_flow.columns

            with dpg.plot(height=300, width=-1, parent="cashflow_chart_container"):
                dpg.add_plot_legend()
                x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Year")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Cash Flow (Billions)")

                # Key cash flow items
                cf_items = ['Operating Cash Flow', 'Free Cash Flow', 'Investing Cash Flow', 'Financing Cash Flow']
                labels = ['Operating CF (B)', 'Free CF (B)', 'Investing CF (B)', 'Financing CF (B)']

                for i, item_name in enumerate(cf_items):
                    if item_name in cash_flow.index:
                        item_data = cash_flow.loc[item_name]
                        years = []
                        values = []

                        for year in years_cols:
                            if year in item_data.index:
                                value = item_data[year]
                                if pd.notna(value):
                                    year_num = year.year if hasattr(year, 'year') else int(str(year)[:4])
                                    years.append(year_num)
                                    values.append(value / 1e9)  # Convert to billions

                        if years and values:
                            sorted_data = sorted(zip(years, values))
                            years, values = zip(*sorted_data)
                            dpg.add_line_series(list(years), list(values), label=labels[i], parent=y_axis)

        except Exception as e:
            print(f"Error creating cash flow chart: {e}")

    def create_financial_ratios_chart(self, years_selection):
        """Create financial ratios and margins chart"""
        try:
            income_stmt = self.financials_data.get('income_statement')
            balance_sheet = self.financials_data.get('balance_sheet')

            if income_stmt is None or balance_sheet is None:
                return

            if income_stmt.empty or balance_sheet.empty:
                return

            # Clear existing chart
            dpg.delete_item("ratios_chart_container", children_only=True)

            # Determine number of years
            if years_selection == "Last 2 Years":
                years_cols = min(len(income_stmt.columns), len(balance_sheet.columns), 2)
            elif years_selection == "Last 3 Years":
                years_cols = min(len(income_stmt.columns), len(balance_sheet.columns), 3)
            elif years_selection == "Last 4 Years":
                years_cols = min(len(income_stmt.columns), len(balance_sheet.columns), 4)
            else:  # All Years
                years_cols = min(len(income_stmt.columns), len(balance_sheet.columns))

            income_years = income_stmt.columns[:years_cols]
            balance_years = balance_sheet.columns[:years_cols]

            with dpg.plot(height=300, width=-1, parent="ratios_chart_container"):
                dpg.add_plot_legend()
                x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Year")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Percentage (%)")

                # Calculate ROE (Return on Equity) over time
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
                                    roe = (ni / eq) * 100  # Convert to percentage
                                    year_num = year.year if hasattr(year, 'year') else int(str(year)[:4])
                                    years.append(year_num)
                                    roe_values.append(roe)

                        if years and roe_values:
                            sorted_data = sorted(zip(years, roe_values))
                            years, roe_values = zip(*sorted_data)
                            dpg.add_line_series(list(years), list(roe_values), label="ROE (%)", parent=y_axis)

                except Exception as e:
                    print(f"Error calculating ROE: {e}")

                # Calculate Profit Margin over time
                try:
                    revenue_items = ['Total Revenue', 'Revenue', 'Net Sales']
                    revenue_data = None

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
                                margin = (ni / rev) * 100  # Convert to percentage
                                year_num = year.year if hasattr(year, 'year') else int(str(year)[:4])
                                years.append(year_num)
                                margin_values.append(margin)

                        if years and margin_values:
                            sorted_data = sorted(zip(years, margin_values))
                            years, margin_values = zip(*sorted_data)
                            dpg.add_line_series(list(years), list(margin_values), label="Profit Margin (%)",
                                                parent=y_axis)

                except Exception as e:
                    print(f"Error calculating Profit Margin: {e}")

        except Exception as e:
            print(f"Error creating ratios chart: {e}")

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

    def cleanup(self):
        """Cleanup resources"""
        try:
            print("YFinance Data Tab cleanup completed")
        except:
            pass


# Example usage and testing
def main():
    """Test the fixed YFinance Data Tab with default AAPL loading"""
    try:
        dpg.create_context()

        with dpg.window(label="YFinance Data Tab - AAPL Default", tag="test_main_window"):
            yfinance_tab = YFinanceDataTab()
            yfinance_tab.create_content()

        dpg.create_viewport(
            title="YFinance Data Tab - Bloomberg Terminal with AAPL Default",
            width=1600,
            height=1000,
            min_width=1200,
            min_height=800
        )

        dpg.setup_dearpygui()
        dpg.show_viewport()
        dpg.set_primary_window("test_main_window", True)

        try:
            if yfinance_tab.theme_manager:
                yfinance_tab.theme_manager.apply_theme_globally("bloomberg_terminal")
                print("✅ Bloomberg Terminal theme applied successfully")
        except Exception as e:
            print(f"⚠️ Theme application warning: {e}")

        print("=" * 80)
        print("YFINANCE DATA TAB - WITH DEFAULT AAPL LOADING")
        print("=" * 80)
        print("FEATURES:")
        print("✅ Loads AAPL ticker automatically on startup")
        print("✅ Shows loading messages during data fetch")
        print("✅ Full candlestick charts with volume")
        print("✅ Complete financial statements as tables")
        print("✅ 4 financial analysis charts with trends")
        print("✅ Returns table with color-coded performance")
        print("✅ Executive team information")
        print("✅ Real-time status updates")
        print("=" * 80)
        print("DEFAULT TICKER: AAPL (loads automatically)")
        print("TEST WITH OTHER TICKERS: MSFT, GOOGL, TSLA, NVDA")
        print("=" * 80)

        dpg.start_dearpygui()

    except Exception as e:
        print(f"Error running YFinance Data Tab: {e}")
        import traceback
        traceback.print_exc()

    finally:
        try:
            dpg.destroy_context()
        except:
            pass


if __name__ == "__main__":
    main()