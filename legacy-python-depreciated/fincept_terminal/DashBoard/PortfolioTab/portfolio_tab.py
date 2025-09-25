# -*- coding: utf-8 -*-
# portfolio_tab.py

import dearpygui.dearpygui as dpg
from fincept_terminal.utils.base_tab import BaseTab
import datetime
import threading
import time
import random
from fincept_terminal.utils.Logging.logger import logger, operation, monitor_performance
from fincept_terminal.DashBoard.PortfolioTab.portfolio_business import PortfolioBusinessLogic


class PortfolioTab(BaseTab):
    """Bloomberg Terminal style Portfolio Management tab with CSV import - UI Layer"""

    def __init__(self, main_app=None):
        super().__init__(main_app)
        self.main_app = main_app

        # Bloomberg color scheme - cached
        self._init_color_scheme()

        # Initialize business logic
        self.business_logic = PortfolioBusinessLogic()

        # UI state
        self.current_view = "overview"

        # Start price refresh thread
        self.business_logic.start_price_refresh_thread()

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

    def get_label(self):
        return " Portfolio"

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
        breakdown = self.business_logic.get_portfolio_breakdown()

        for portfolio_data in breakdown:
            with dpg.table_row():
                name = portfolio_data['name']
                dpg.add_text(name[:15] + ("..." if len(name) > 15 else ""))
                dpg.add_text(str(portfolio_data['stocks_count']))
                dpg.add_text(f"${portfolio_data['value']:,.0f}")
                pnl_color = self.BLOOMBERG_GREEN if portfolio_data['pnl_pct'] >= 0 else self.BLOOMBERG_RED
                dpg.add_text(f"{portfolio_data['pnl_pct']:+.1f}%", color=pnl_color)

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
                dpg.add_combo(list(self.business_logic.country_suffixes.keys()), tag="country_selector",
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
            filename = self.business_logic.select_csv_file()

            if filename:
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
            result = self.business_logic.analyze_csv_file()

            # Show column mapping section
            self.create_column_mapping_ui(result['headers'])
            dpg.show_item("column_mapping_section")

            self.show_message(f"CSV analyzed: {result['row_count']} rows, {result['columns_count']} columns", "success")

        except Exception as e:
            logger.error(f"Error analyzing CSV: {e}", exc_info=True)
            self.show_message(f"Error analyzing CSV: {str(e)}", "error")

    def create_column_mapping_ui(self, headers):
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
                        auto_detected = self.business_logic.auto_detect_column(field_key)
                        dpg.add_combo([""] + headers, tag=f"map_{field_key}",
                                      default_value=auto_detected, width=180)

                        required_color = self.BLOOMBERG_RED if is_required else self.BLOOMBERG_GREEN
                        required_text = "YES" if is_required else "NO"
                        dpg.add_text(required_text, color=required_color)

            dpg.add_spacer(height=10)
            dpg.add_button(label="PREVIEW IMPORT", callback=self.preview_import, width=150, height=30)

    @monitor_performance
    def preview_import(self):
        """Preview the import with current column mapping - optimized"""
        try:
            # Get column mappings from UI
            column_mapping = {}
            required_mappings = ["symbol", "quantity", "avg_price"]
            optional_mappings = ["current_price", "investment", "current_value", "pnl"]

            for field in required_mappings + optional_mappings:
                if dpg.does_item_exist(f"map_{field}"):
                    column_mapping[field] = dpg.get_value(f"map_{field}")

            country_suffix = self.business_logic.country_suffixes.get(dpg.get_value("country_selector"), "")

            result = self.business_logic.preview_import(column_mapping, country_suffix)

            # Show preview
            self.create_import_preview(result)
            dpg.show_item("import_preview_section")
            dpg.show_item("import_portfolio_btn")

            self.show_message(f"Preview ready: {result['valid_rows']} valid rows found", "success")

        except Exception as e:
            logger.error(f"Error creating preview: {e}", exc_info=True)
            self.show_message(f"Error creating preview: {str(e)}", "error")

    def create_import_preview(self, result):
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
            dpg.add_text(f"Preview of {result['valid_rows']} stocks to be imported:", color=self.BLOOMBERG_WHITE)
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

                for item in result['preview_data']:
                    investment = item["quantity"] * item["avg_price"]
                    with dpg.table_row():
                        dpg.add_text(item["original_symbol"], color=self.BLOOMBERG_WHITE)
                        dpg.add_text(item["yfinance_symbol"], color=self.BLOOMBERG_GREEN)
                        dpg.add_text(f"{item['quantity']:.2f}", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${item['avg_price']:.2f}", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${investment:,.2f}", color=self.BLOOMBERG_WHITE)

            dpg.add_spacer(height=10)

            # Summary
            with dpg.group(horizontal=True):
                dpg.add_text("Summary:", color=self.BLOOMBERG_YELLOW)
                dpg.add_text(f"{result['valid_rows']} stocks", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(f"Total Investment: ${result['total_investment']:,.2f}", color=self.BLOOMBERG_WHITE)

    @monitor_performance
    def import_csv_portfolio(self):
        """Import the CSV data as a new portfolio - optimized"""
        try:
            portfolio_name = dpg.get_value("import_portfolio_name").strip()
            result = self.business_logic.import_csv_portfolio(portfolio_name)

            # Switch to overview
            self.switch_view("overview")

            success_msg = f"Portfolio '{result['portfolio_name']}' imported successfully with {result['stocks_imported']} stocks! Fetching real-time prices..."
            self.show_message(success_msg, "success")

        except Exception as e:
            logger.error(f"Error importing portfolio: {e}", exc_info=True)
            self.show_message(f"Error importing portfolio: {str(e)}", "error")

    @monitor_performance
    def create_overview_content(self):
        """Create portfolio overview content - optimized"""
        with dpg.group(parent="portfolio_center_panel"):
            dpg.add_text("PORTFOLIO OVERVIEW", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Get summary data from business logic
            summary = self.business_logic.get_portfolio_summary()

            # Summary cards
            with dpg.group(horizontal=True):
                summary_cards = [
                    (" TOTAL PORTFOLIO VALUE", f"${summary['total_value']:,.2f}", "Real-time valuation",
                     self.BLOOMBERG_WHITE),
                    (" TODAY'S CHANGE", f"${summary['today_change']:+,.2f}",
                     f"({summary['today_change_pct']:+.2f}%)",
                     self.BLOOMBERG_GREEN if summary['today_change'] >= 0 else self.BLOOMBERG_RED),
                    (" TOTAL GAIN/LOSS", f"${summary['total_pnl']:+,.2f}",
                     f"({summary['total_pnl_pct']:+.2f}%)",
                     self.BLOOMBERG_GREEN if summary['total_pnl'] >= 0 else self.BLOOMBERG_RED)
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
            self._create_portfolio_breakdown_table()

    def _create_portfolio_breakdown_table(self):
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

            breakdown = self.business_logic.get_portfolio_breakdown()

            for portfolio_data in breakdown:
                with dpg.table_row():
                    dpg.add_text(portfolio_data['name'], color=self.BLOOMBERG_WHITE)
                    dpg.add_text(str(portfolio_data['stocks_count']), color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"${portfolio_data['investment']:,.2f}", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"${portfolio_data['value']:,.2f}", color=self.BLOOMBERG_WHITE)

                    # Today's change
                    change_color = self.BLOOMBERG_GREEN if portfolio_data['today_change'] >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(
                        f"${portfolio_data['today_change']:+,.2f} ({portfolio_data['today_change_pct']:+.1f}%)",
                        color=change_color)

                    # Total P&L
                    pnl_color = self.BLOOMBERG_GREEN if portfolio_data['pnl'] >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"${portfolio_data['pnl']:+,.2f} ({portfolio_data['pnl_pct']:+.1f}%)", color=pnl_color)

                    dpg.add_text(f"{portfolio_data['allocation_pct']:.1f}%", color=self.BLOOMBERG_WHITE)

                    # Action buttons
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="Manage", width=60,
                                       callback=lambda p=portfolio_data['name']: self.select_and_manage_portfolio(p))
                        dpg.add_button(label="Analyze", width=60,
                                       callback=lambda p=portfolio_data['name']: self.select_and_analyze_portfolio(p))
                        dpg.add_button(label="Delete", width=60,
                                       callback=lambda p=portfolio_data['name']: self.delete_portfolio_confirm(p))

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
            dpg.add_combo(list(self.business_logic.portfolios.keys()), tag="portfolio_selector",
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
            dpg.add_combo(list(self.business_logic.portfolios.keys()), tag="analyze_portfolio_selector",
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
                holdings = self.business_logic.get_portfolio_holdings(portfolio_name)
                stocks = [holding['symbol'] for holding in holdings[:6]]  # Show top 6 stocks
                values = [holding['market_value'] for holding in holdings[:6]]

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
                current_value = self.business_logic.calculate_portfolio_value(portfolio_name)

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

    def _create_return_metrics_panel(self, portfolio_name):
        """Create return metrics panel efficiently"""
        with dpg.child_window(width=280, height=250, border=True):
            dpg.add_text(" RETURN METRICS", color=self.BLOOMBERG_YELLOW)
            dpg.add_separator()

            with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                dpg.add_table_column()
                dpg.add_table_column()

                # Calculate actual returns efficiently
                portfolio_value = self.business_logic.calculate_portfolio_value(portfolio_name)
                portfolio_investment = self.business_logic.calculate_portfolio_investment(portfolio_name)
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

    @monitor_performance
    def update_navigation_summary(self):
        """Update navigation panel summary - optimized"""
        try:
            summary = self.business_logic.get_portfolio_summary()

            # Update UI elements efficiently
            ui_updates = [
                ("nav_total_portfolios", str(summary['total_portfolios'])),
                ("nav_total_investment", f"${summary['total_investment']:,.2f}"),
                ("nav_current_value", f"${summary['total_value']:,.2f}"),
                ("nav_total_pnl", f"${summary['total_pnl']:+,.2f} ({summary['total_pnl_pct']:+.1f}%)"),
                ("nav_today_change", f"${summary['today_change']:+,.2f} ({summary['today_change_pct']:+.1f}%)")
            ]

            for tag, value in ui_updates:
                if dpg.does_item_exist(tag):
                    dpg.set_value(tag, value)

            # Update last update time
            if dpg.does_item_exist("portfolio_last_update"):
                dpg.set_value("portfolio_last_update", datetime.datetime.now().strftime('%H:%M:%S'))

        except Exception as e:
            logger.error(f"Error updating navigation summary: {e}")

    # Callback methods - optimized
    @monitor_performance
    def create_portfolio_simple(self):
        """Create a simple portfolio - optimized"""
        try:
            name = dpg.get_value("new_portfolio_name").strip()
            self.business_logic.create_portfolio(name)

            # Clear input and switch to overview
            dpg.set_value("new_portfolio_name", "")
            dpg.set_value("new_portfolio_description", "")
            self.switch_view("overview")

            self.show_message(f"Portfolio '{name}' created successfully!", "success")

        except Exception as e:
            logger.error(f"Error creating portfolio: {e}", exc_info=True)
            self.show_message(str(e), "error")

    @monitor_performance
    def create_portfolio_with_stock(self):
        """Create portfolio and add first stock - optimized"""
        try:
            name = dpg.get_value("new_portfolio_name").strip()
            symbol = dpg.get_value("create_stock_symbol").strip().upper()
            quantity_str = dpg.get_value("create_stock_quantity").strip()
            price_str = dpg.get_value("create_stock_price").strip()

            quantity = None
            price = None

            if quantity_str:
                quantity = float(quantity_str)
            if price_str:
                price = float(price_str)

            self.business_logic.create_portfolio_with_stock(name, symbol, quantity, price)

            # Clear inputs and switch to overview
            for tag in ["new_portfolio_name", "new_portfolio_description", "create_stock_symbol",
                        "create_stock_quantity", "create_stock_price"]:
                dpg.set_value(tag, "")

            self.switch_view("overview")

            success_msg = f"Portfolio '{name}' created successfully!"
            if symbol and quantity is not None:
                success_msg += f" Added {quantity} shares of {symbol} at ${price:.2f}. Fetching real-time price..."

            self.show_message(success_msg, "success")

        except Exception as e:
            logger.error(f"Error creating portfolio with stock: {e}", exc_info=True)
            self.show_message(str(e), "error")

    def on_portfolio_select(self):
        """Handle portfolio selection in manage view"""
        try:
            selected = dpg.get_value("portfolio_selector")
            if selected:
                self.business_logic.current_portfolio = selected
                self.refresh_manage_view()
        except Exception as e:
            logger.error(f"Error in portfolio select: {e}")

    def on_analyze_portfolio_select(self):
        """Handle portfolio selection in analyze view"""
        try:
            selected = dpg.get_value("analyze_portfolio_selector")
            if selected:
                self.business_logic.current_portfolio = selected
                threading.Thread(target=lambda: self.create_analytics_charts(selected), daemon=True).start()
        except Exception as e:
            logger.error(f"Error in analyze portfolio select: {e}")

    @monitor_performance
    def add_stock_simple(self):
        """Add stock to selected portfolio - optimized"""
        try:
            portfolio_name = self.business_logic.current_portfolio
            symbol = dpg.get_value("manage_stock_symbol").strip().upper()
            quantity_str = dpg.get_value("manage_stock_quantity").strip()
            price_str = dpg.get_value("manage_stock_price").strip()

            self.business_logic.add_stock_to_portfolio(portfolio_name, symbol, quantity_str, price_str)

            self.refresh_manage_view()

            # Clear inputs
            for tag in ["manage_stock_symbol", "manage_stock_quantity", "manage_stock_price"]:
                dpg.set_value(tag, "")

            self.show_message(
                f"Added {quantity_str} shares of {symbol} to {portfolio_name}. Fetching real-time price...",
                "success")

        except Exception as e:
            logger.error(f"Error adding stock: {e}", exc_info=True)
            self.show_message(str(e), "error")

    @monitor_performance
    def refresh_manage_view(self):
        """Refresh the manage view content - optimized"""
        try:
            if not self.business_logic.current_portfolio:
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
            holdings = self.business_logic.get_portfolio_holdings(self.business_logic.current_portfolio)

            for holding in holdings:
                with dpg.table_row(parent="manage_holdings_table"):
                    dpg.add_text(holding['symbol'], color=self.BLOOMBERG_WHITE)  # YFinance symbol
                    dpg.add_text(holding['original_symbol'], color=self.BLOOMBERG_GRAY)  # Original symbol
                    dpg.add_text(f"{holding['quantity']:.2f}", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"${holding['avg_price']:.2f}", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"${holding['current_price']:.2f}", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"${holding['market_value']:,.2f}", color=self.BLOOMBERG_WHITE)

                    # Gain/loss with color coding
                    gain_color = self.BLOOMBERG_GREEN if holding['gain_loss'] >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"${holding['gain_loss']:+,.2f} ({holding['gain_loss_pct']:+.1f}%)", color=gain_color)

                    dpg.add_text(f"{holding['weight_pct']:.1f}%", color=self.BLOOMBERG_WHITE)

                    dpg.add_button(label="Remove", width=80,
                                   callback=lambda s=holding['symbol']: self.remove_stock_from_portfolio(s))

        except Exception as e:
            logger.error(f"Error refreshing manage view: {e}")

    def select_and_manage_portfolio(self, portfolio_name):
        """Select portfolio and switch to manage view"""
        self.business_logic.current_portfolio = portfolio_name
        self.switch_view("manage")
        if dpg.does_item_exist("portfolio_selector"):
            dpg.set_value("portfolio_selector", portfolio_name)
        self.refresh_manage_view()

    def select_and_analyze_portfolio(self, portfolio_name):
        """Select portfolio and switch to analyze view"""
        self.business_logic.current_portfolio = portfolio_name
        self.switch_view("analyze")
        if dpg.does_item_exist("analyze_portfolio_selector"):
            dpg.set_value("analyze_portfolio_selector", portfolio_name)
        threading.Thread(target=lambda: self.create_analytics_charts(portfolio_name), daemon=True).start()

    def remove_stock_from_portfolio(self, symbol):
        """Remove stock from current portfolio"""
        try:
            self.business_logic.remove_stock_from_portfolio(self.business_logic.current_portfolio, symbol)
            self.refresh_manage_view()
            self.show_message(f"Removed {symbol} from {self.business_logic.current_portfolio}.", "success")
        except Exception as e:
            logger.error(f"Error removing stock: {e}")
            self.show_message(str(e), "error")

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
        if not self.business_logic.portfolios:
            self.show_message("No portfolios to delete", "warning")
            return

        try:
            # Create a simple selection dialog
            portfolio_names = list(self.business_logic.portfolios.keys())

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
            self.business_logic.delete_portfolio(portfolio_name)

            # Close any confirmation dialogs
            if dpg.does_item_exist(f"delete_confirm_{portfolio_name}"):
                dpg.delete_item(f"delete_confirm_{portfolio_name}")

            # Refresh UI
            self.switch_view("overview")

            self.show_message(f"Portfolio '{portfolio_name}' deleted successfully", "success")

        except Exception as e:
            logger.error(f"Error deleting portfolio: {e}", exc_info=True)
            self.show_message(str(e), "error")

    def refresh_all_prices_now(self):
        """Refresh all prices immediately"""
        self.show_message("Refreshing all prices...", "info")
        self.business_logic.refresh_all_prices_now()

    # Export methods
    @monitor_performance
    def export_portfolio_data(self):
        """Export portfolio data - optimized"""
        try:
            filename = self.business_logic.export_portfolio_data()
            self.show_message(f"Portfolio data exported to {filename}", "success")

        except Exception as e:
            logger.error(f"Error exporting portfolio data: {e}", exc_info=True)
            self.show_message(str(e), "error")

    # Additional callback methods - optimized
    def search_portfolio(self):
        """Search portfolio functionality"""
        logger.debug("Portfolio search functionality triggered")

    def show_settings(self):
        """Show portfolio settings"""
        logger.debug("Portfolio settings functionality triggered")

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
            logger.info("🧹 Cleaning up portfolio UI...")

            # Clean up business logic
            if hasattr(self, 'business_logic'):
                self.business_logic.cleanup()

            # Clear UI state
            self.current_view = "overview"

            logger.info("Portfolio UI cleanup complete")

        except Exception as e:
            logger.error(f"Error in portfolio UI cleanup: {e}", exc_info=True)