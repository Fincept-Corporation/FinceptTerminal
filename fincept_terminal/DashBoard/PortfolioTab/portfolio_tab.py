# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import json
import os
import datetime
import threading
import time
import random
from collections import defaultdict


def get_settings_path():
    """Returns a consistent settings file path"""
    return "settings.json"


SETTINGS_FILE = get_settings_path()


class PortfolioTab(BaseTab):
    """Bloomberg Terminal style Portfolio Management tab"""

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
        self.BLOOMBERG_BLUE = [100, 150, 250]
        self.BLOOMBERG_BLACK = [0, 0, 0]

        # Portfolio data
        self.portfolios = self.load_portfolios()
        self.current_portfolio = None
        self.current_view = "overview"
        self.price_cache = {}
        self.last_price_update = {}
        self.refresh_thread = None
        self.refresh_running = False

        # Sample market data for demonstration
        self.initialize_sample_data()

    def get_label(self):
        return "💼 Portfolio"

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

        # Initialize price cache with sample data
        sample_prices = {
            "AAPL": 175.50, "MSFT": 310.25, "GOOGL": 140.75, "NVDA": 520.80,
            "JNJ": 165.30, "PG": 152.40, "KO": 61.20, "TSLA": 245.60,
            "AMZN": 135.90, "META": 380.45
        }

        for symbol, price in sample_prices.items():
            self.price_cache[symbol] = price * (1 + random.uniform(-0.05, 0.05))  # Add some variation
            self.last_price_update[symbol] = datetime.datetime.now()

    def create_content(self):
        """Create Bloomberg-style portfolio terminal layout"""
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
                portfolio_functions = ["F1:OVERVIEW", "F2:CREATE", "F3:MANAGE", "F4:ANALYZE", "F5:REPORTS",
                                       "F6:SETTINGS"]
                for i, key in enumerate(portfolio_functions):
                    callback_map = {
                        0: lambda: self.switch_view("overview"),
                        1: lambda: self.switch_view("create"),
                        2: lambda: self.switch_view("manage"),
                        3: lambda: self.switch_view("analyze"),
                        4: lambda: self.show_reports(),
                        5: lambda: self.show_settings()
                    }
                    dpg.add_button(label=key, width=120, height=25, callback=callback_map.get(i, lambda: None))

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
            print(f"Error creating portfolio content: {e}")
            # Fallback content
            dpg.add_text("PORTFOLIO TERMINAL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Error loading portfolio content. Please try again.")

    def create_left_portfolio_panel(self):
        """Create left portfolio navigation panel"""
        with dpg.child_window(width=350, height=650, border=True):
            dpg.add_text("PORTFOLIO NAVIGATOR", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Portfolio summary stats
            dpg.add_text("PORTFOLIO SUMMARY", color=self.BLOOMBERG_YELLOW)
            with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                dpg.add_table_column()
                dpg.add_table_column()

                with dpg.table_row():
                    dpg.add_text("Total Portfolios:")
                    dpg.add_text(f"{len(self.portfolios)}", color=self.BLOOMBERG_WHITE, tag="nav_total_portfolios")
                with dpg.table_row():
                    dpg.add_text("Total Investment:")
                    dpg.add_text("$0.00", color=self.BLOOMBERG_WHITE, tag="nav_total_investment")
                with dpg.table_row():
                    dpg.add_text("Current Value:")
                    dpg.add_text("$0.00", color=self.BLOOMBERG_WHITE, tag="nav_current_value")
                with dpg.table_row():
                    dpg.add_text("Total P&L:")
                    dpg.add_text("$0.00 (0.00%)", color=self.BLOOMBERG_WHITE, tag="nav_total_pnl")
                with dpg.table_row():
                    dpg.add_text("Today's Change:")
                    dpg.add_text("$0.00 (0.00%)", color=self.BLOOMBERG_WHITE, tag="nav_today_change")

            dpg.add_separator()

            # Portfolio list
            dpg.add_text("PORTFOLIO LIST", color=self.BLOOMBERG_YELLOW)
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           scrollY=True, height=200):
                dpg.add_table_column(label="Name", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Stocks", width_fixed=True, init_width_or_weight=60)
                dpg.add_table_column(label="Value", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="P&L%", width_fixed=True, init_width_or_weight=70)

                # Populate portfolio list
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

            dpg.add_separator()

            # Quick actions
            dpg.add_text("QUICK ACTIONS", color=self.BLOOMBERG_YELLOW)
            quick_actions = [
                ("📊 View Overview", lambda: self.switch_view("overview")),
                ("➕ Create Portfolio", lambda: self.switch_view("create")),
                ("📝 Manage Holdings", lambda: self.switch_view("manage")),
                ("📈 View Analytics", lambda: self.switch_view("analyze")),
                ("🔄 Refresh Prices", self.refresh_all_prices_now),
                ("💾 Export Data", self.export_portfolio_data)
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
        """Create right portfolio analytics panel"""
        with dpg.child_window(width=350, height=650, border=True):
            dpg.add_text("PORTFOLIO ANALYTICS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Market indicators
            dpg.add_text("MARKET INDICATORS", color=self.BLOOMBERG_YELLOW)
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

            dpg.add_separator()

            # Top performers
            dpg.add_text("TOP PERFORMERS TODAY", color=self.BLOOMBERG_YELLOW)
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

            dpg.add_separator()

            # Risk metrics
            dpg.add_text("RISK METRICS", color=self.BLOOMBERG_YELLOW)
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

            dpg.add_separator()

            # Recent transactions
            dpg.add_text("RECENT TRANSACTIONS", color=self.BLOOMBERG_YELLOW)
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
            dpg.add_text("PORTFOLIO STATUS:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("ACTIVE", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("REAL-TIME PRICING:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("ENABLED", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LAST UPDATE:", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.datetime.now().strftime('%H:%M:%S'),
                         tag="portfolio_last_update", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("AUTO-REFRESH:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("ON", color=self.BLOOMBERG_GREEN, tag="portfolio_auto_refresh_status")

    def switch_view(self, view_name):
        """Switch between different portfolio views"""
        self.current_view = view_name

        # Clear center panel
        if dpg.does_item_exist("portfolio_center_panel"):
            children = dpg.get_item_children("portfolio_center_panel", slot=1)
            for child in children:
                try:
                    dpg.delete_item(child)
                except:
                    pass

        # Load appropriate content
        if view_name == "overview":
            self.create_overview_content()
        elif view_name == "create":
            self.create_create_content()
        elif view_name == "manage":
            self.create_manage_content()
        elif view_name == "analyze":
            self.create_analyze_content()

        # Update navigation summary
        self.update_navigation_summary()

    def create_overview_content(self):
        """Create portfolio overview content"""
        with dpg.group(parent="portfolio_center_panel"):
            dpg.add_text("PORTFOLIO OVERVIEW", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Summary cards
            with dpg.group(horizontal=True):
                # Total portfolio value card
                with dpg.child_window(width=280, height=120, border=True):
                    dpg.add_text("💰 TOTAL PORTFOLIO VALUE", color=self.BLOOMBERG_YELLOW)
                    dpg.add_spacer(height=5)
                    total_value = sum(self.calculate_portfolio_value(name) for name in self.portfolios.keys())
                    dpg.add_text(f"${total_value:,.2f}", color=self.BLOOMBERG_WHITE)
                    dpg.add_text("Real-time valuation", color=self.BLOOMBERG_GRAY)

                dpg.add_spacer(width=15)

                # Today's change card
                with dpg.child_window(width=280, height=120, border=True):
                    dpg.add_text("📈 TODAY'S CHANGE", color=self.BLOOMBERG_YELLOW)
                    dpg.add_spacer(height=5)
                    today_change = random.uniform(-1000, 2000)  # Simulated
                    change_pct = (today_change / total_value * 100) if total_value > 0 else 0
                    change_color = self.BLOOMBERG_GREEN if today_change >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"${today_change:+,.2f}", color=change_color)
                    dpg.add_text(f"({change_pct:+.2f}%)", color=change_color)

                dpg.add_spacer(width=15)

                # Total P&L card
                with dpg.child_window(width=280, height=120, border=True):
                    dpg.add_text("💎 TOTAL GAIN/LOSS", color=self.BLOOMBERG_YELLOW)
                    dpg.add_spacer(height=5)
                    total_investment = sum(self.calculate_portfolio_investment(name) for name in self.portfolios.keys())
                    total_pnl = total_value - total_investment
                    pnl_pct = (total_pnl / total_investment * 100) if total_investment > 0 else 0
                    pnl_color = self.BLOOMBERG_GREEN if total_pnl >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"${total_pnl:+,.2f}", color=pnl_color)
                    dpg.add_text(f"({pnl_pct:+.2f}%)", color=pnl_color)

            dpg.add_spacer(height=20)

            # Detailed portfolio table
            dpg.add_text("PORTFOLIO BREAKDOWN", color=self.BLOOMBERG_ORANGE)
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           scrollY=True, scrollX=True, height=400):
                dpg.add_table_column(label="Portfolio", width_fixed=True, init_width_or_weight=150)
                dpg.add_table_column(label="Stocks", width_fixed=True, init_width_or_weight=70)
                dpg.add_table_column(label="Investment", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Current Value", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Today's Change", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Total P&L", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Allocation %", width_fixed=True, init_width_or_weight=100)
                dpg.add_table_column(label="Actions", width_fixed=True, init_width_or_weight=200)

                for portfolio_name, stocks in self.portfolios.items():
                    portfolio_investment = self.calculate_portfolio_investment(portfolio_name)
                    portfolio_value = self.calculate_portfolio_value(portfolio_name)
                    portfolio_pnl = portfolio_value - portfolio_investment
                    portfolio_pnl_pct = (portfolio_pnl / portfolio_investment * 100) if portfolio_investment > 0 else 0
                    allocation_pct = (portfolio_value / total_value * 100) if total_value > 0 else 0
                    today_change = random.uniform(-200, 300)  # Simulated daily change

                    with dpg.table_row():
                        dpg.add_text(portfolio_name, color=self.BLOOMBERG_WHITE)
                        dpg.add_text(str(len(stocks)), color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${portfolio_investment:,.2f}", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${portfolio_value:,.2f}", color=self.BLOOMBERG_WHITE)

                        # Today's change
                        change_color = self.BLOOMBERG_GREEN if today_change >= 0 else self.BLOOMBERG_RED
                        change_pct = (today_change / portfolio_value * 100) if portfolio_value > 0 else 0
                        dpg.add_text(f"${today_change:+,.2f} ({change_pct:+.1f}%)", color=change_color)

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
                dpg.add_table_column(label="Stock", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Shares", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Avg Cost", width_fixed=True, init_width_or_weight=100)
                dpg.add_table_column(label="Current Price", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Market Value", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Gain/Loss", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Weight %", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Actions", width_fixed=True, init_width_or_weight=100)

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

    def create_analytics_charts(self, portfolio_name):
        """Create analytics charts for selected portfolio"""
        # Clear existing content
        if dpg.does_item_exist("analytics_content"):
            children = dpg.get_item_children("analytics_content", slot=1)
            for child in children:
                try:
                    dpg.delete_item(child)
                except:
                    pass

        # Create analytics content
        with dpg.group(parent="analytics_content"):
            dpg.add_text(f"ANALYTICS FOR: {portfolio_name.upper()}", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Performance metrics
            with dpg.group(horizontal=True):
                # Portfolio composition chart
                with dpg.child_window(width=420, height=300, border=True):
                    dpg.add_text("📊 PORTFOLIO COMPOSITION", color=self.BLOOMBERG_YELLOW)
                    dpg.add_separator()

                    # Create simple bar chart for portfolio composition
                    with dpg.plot(height=220, width=-1):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Stocks")
                        y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Value ($)")

                        # Get portfolio data
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

                dpg.add_spacer(width=15)

                # Performance tracking chart
                with dpg.child_window(width=420, height=300, border=True):
                    dpg.add_text("📈 PERFORMANCE TRACKING", color=self.BLOOMBERG_YELLOW)
                    dpg.add_separator()

                    with dpg.plot(height=220, width=-1):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Days")
                        y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Portfolio Value ($)")

                        # Generate sample performance data
                        days = list(range(30))
                        current_value = self.calculate_portfolio_value(portfolio_name)

                        # Simulate portfolio performance over 30 days
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

            dpg.add_spacer(height=20)

            # Risk and return metrics
            with dpg.group(horizontal=True):
                # Risk metrics table
                with dpg.child_window(width=280, height=250, border=True):
                    dpg.add_text("⚠️ RISK METRICS", color=self.BLOOMBERG_YELLOW)
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

                dpg.add_spacer(width=15)

                # Return metrics table
                with dpg.child_window(width=280, height=250, border=True):
                    dpg.add_text("💰 RETURN METRICS", color=self.BLOOMBERG_YELLOW)
                    dpg.add_separator()

                    with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                        dpg.add_table_column()
                        dpg.add_table_column()

                        # Calculate actual returns
                        portfolio_value = self.calculate_portfolio_value(portfolio_name)
                        portfolio_investment = self.calculate_portfolio_investment(portfolio_name)
                        total_return = portfolio_value - portfolio_investment
                        total_return_pct = (
                                    total_return / portfolio_investment * 100) if portfolio_investment > 0 else 0

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

                dpg.add_spacer(width=15)

                # Sector allocation
                with dpg.child_window(width=280, height=250, border=True):
                    dpg.add_text("🏢 SECTOR ALLOCATION", color=self.BLOOMBERG_YELLOW)
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

    # Calculation methods
    def calculate_portfolio_value(self, portfolio_name):
        """Calculate current portfolio value"""
        portfolio = self.portfolios.get(portfolio_name, {})
        total_value = 0

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get('quantity', 0)
                current_price = self.get_current_price(symbol)
                total_value += quantity * current_price

        return total_value

    def calculate_portfolio_investment(self, portfolio_name):
        """Calculate total portfolio investment"""
        portfolio = self.portfolios.get(portfolio_name, {})
        total_investment = 0

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get('quantity', 0)
                avg_price = data.get('avg_price', 0)
                total_investment += quantity * avg_price

        return total_investment

    def get_current_price(self, symbol):
        """Get current price from cache or return simulated price"""
        if symbol in self.price_cache:
            # Add small random variation for live effect
            base_price = self.price_cache[symbol]
            return base_price * (1 + random.uniform(-0.01, 0.01))

        # Return simulated price if not in cache
        return random.uniform(50, 500)

    def update_navigation_summary(self):
        """Update navigation panel summary"""
        try:
            total_portfolios = len(self.portfolios)
            total_investment = sum(self.calculate_portfolio_investment(name) for name in self.portfolios.keys())
            total_value = sum(self.calculate_portfolio_value(name) for name in self.portfolios.keys())
            total_pnl = total_value - total_investment
            total_pnl_pct = (total_pnl / total_investment * 100) if total_investment > 0 else 0

            # Simulate today's change
            today_change = random.uniform(-1000, 2000)
            today_change_pct = (today_change / total_value * 100) if total_value > 0 else 0

            if dpg.does_item_exist("nav_total_portfolios"):
                dpg.set_value("nav_total_portfolios", str(total_portfolios))
            if dpg.does_item_exist("nav_total_investment"):
                dpg.set_value("nav_total_investment", f"${total_investment:,.2f}")
            if dpg.does_item_exist("nav_current_value"):
                dpg.set_value("nav_current_value", f"${total_value:,.2f}")
            if dpg.does_item_exist("nav_total_pnl"):
                pnl_text = f"${total_pnl:+,.2f} ({total_pnl_pct:+.1f}%)"
                dpg.set_value("nav_total_pnl", pnl_text)
            if dpg.does_item_exist("nav_today_change"):
                change_text = f"${today_change:+,.2f} ({today_change_pct:+.1f}%)"
                dpg.set_value("nav_today_change", change_text)

        except Exception as e:
            print(f"Error updating navigation summary: {e}")

    # Callback methods
    def create_portfolio_simple(self):
        """Create a simple portfolio"""
        try:
            name = dpg.get_value("new_portfolio_name").strip()
            if not name:
                self.show_message("Please enter a portfolio name.", "error")
                return

            if name in self.portfolios:
                self.show_message("Portfolio name already exists.", "error")
                return

            self.portfolios[name] = {}
            self.save_portfolios()

            # Clear input and switch to overview
            dpg.set_value("new_portfolio_name", "")
            dpg.set_value("new_portfolio_description", "")
            self.switch_view("overview")

            self.show_message(f"Portfolio '{name}' created successfully!", "success")

        except Exception as e:
            print(f"Error creating portfolio: {e}")
            self.show_message("Error creating portfolio", "error")

    def create_portfolio_with_stock(self):
        """Create portfolio and add first stock"""
        try:
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

                    # Update price cache
                    self.price_cache[symbol] = price * (1 + random.uniform(-0.05, 0.05))

                except ValueError:
                    self.show_message("Invalid quantity or price values.", "error")
                    return
            else:
                # Create empty portfolio
                self.portfolios[name] = {}

            self.save_portfolios()

            # Clear inputs and switch to overview
            dpg.set_value("new_portfolio_name", "")
            dpg.set_value("new_portfolio_description", "")
            dpg.set_value("create_stock_symbol", "")
            dpg.set_value("create_stock_quantity", "")
            dpg.set_value("create_stock_price", "")
            self.switch_view("overview")

            success_msg = f"Portfolio '{name}' created successfully!"
            if symbol:
                success_msg += f" Added {quantity} shares of {symbol} at ${price:.2f}."

            self.show_message(success_msg, "success")

        except Exception as e:
            print(f"Error creating portfolio with stock: {e}")
            self.show_message("Error creating portfolio", "error")

    def on_portfolio_select(self):
        """Handle portfolio selection in manage view"""
        try:
            selected = dpg.get_value("portfolio_selector")
            if selected:
                self.current_portfolio = selected
                self.refresh_manage_view()
        except Exception as e:
            print(f"Error in portfolio select: {e}")

    def on_analyze_portfolio_select(self):
        """Handle portfolio selection in analyze view"""
        try:
            selected = dpg.get_value("analyze_portfolio_selector")
            if selected:
                self.current_portfolio = selected
                threading.Thread(target=lambda: self.create_analytics_charts(selected), daemon=True).start()
        except Exception as e:
            print(f"Error in analyze portfolio select: {e}")

    def add_stock_simple(self):
        """Add stock to selected portfolio"""
        try:
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
            self.refresh_manage_view()

            # Clear inputs
            dpg.set_value("manage_stock_symbol", "")
            dpg.set_value("manage_stock_quantity", "")
            dpg.set_value("manage_stock_price", "")

            self.show_message(f"Added {quantity} shares of {symbol} to {self.current_portfolio}.", "success")

        except ValueError:
            self.show_message("Invalid quantity or price values.", "error")
        except Exception as e:
            print(f"Error adding stock: {e}")
            self.show_message("Error adding stock", "error")

    def refresh_manage_view(self):
        """Refresh the manage view content"""
        try:
            if not self.current_portfolio:
                return

            # Clear existing table
            if dpg.does_item_exist("manage_holdings_table"):
                children = dpg.get_item_children("manage_holdings_table", slot=1)
                for child in children:
                    try:
                        dpg.delete_item(child)
                    except:
                        pass

            # Update holdings table
            portfolio = self.portfolios[self.current_portfolio]
            portfolio_value = self.calculate_portfolio_value(self.current_portfolio)

            for symbol, data in portfolio.items():
                if isinstance(data, dict):
                    quantity = data.get("quantity", 0)
                    avg_price = data.get("avg_price", 0)
                    current_price = self.get_current_price(symbol)

                    market_value = quantity * current_price
                    investment = quantity * avg_price
                    gain_loss = market_value - investment
                    gain_loss_pct = (gain_loss / investment * 100) if investment > 0 else 0
                    weight_pct = (market_value / portfolio_value * 100) if portfolio_value > 0 else 0

                    with dpg.table_row(parent="manage_holdings_table"):
                        dpg.add_text(symbol, color=self.BLOOMBERG_WHITE)
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
            print(f"Error refreshing manage view: {e}")

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
            self.refresh_manage_view()

            self.show_message(f"Removed {symbol} from {self.current_portfolio}.", "success")
        except Exception as e:
            print(f"Error removing stock: {e}")

    # Threading and price management
    def start_price_refresh_thread(self):
        """Start the auto price refresh thread"""
        if not self.refresh_running:
            self.refresh_running = True
            self.refresh_thread = threading.Thread(target=self._price_refresh_loop, daemon=True)
            self.refresh_thread.start()

    def _price_refresh_loop(self):
        """Background thread for price refresh"""
        while self.refresh_running:
            try:
                # Update prices every 30 seconds for demo
                time.sleep(30)
                if self.refresh_running:
                    self.refresh_all_prices_background()
            except Exception as e:
                print(f"Error in price refresh loop: {e}")
                time.sleep(60)

    def refresh_all_prices_background(self):
        """Refresh all prices in background"""
        try:
            # Simulate price updates
            for symbol in self.price_cache:
                current_price = self.price_cache[symbol]
                change = random.uniform(-0.02, 0.02)  # ±2% change
                new_price = current_price * (1 + change)
                self.price_cache[symbol] = max(new_price, 1.0)  # Minimum $1
                self.last_price_update[symbol] = datetime.datetime.now()

            # Update timestamp
            if dpg.does_item_exist("portfolio_last_update"):
                dpg.set_value("portfolio_last_update", datetime.datetime.now().strftime('%H:%M:%S'))

            # Update navigation summary
            self.update_navigation_summary()

        except Exception as e:
            print(f"Error refreshing prices: {e}")

    def refresh_all_prices_now(self):
        """Refresh all prices immediately"""
        self.show_message("Refreshing all prices...", "info")
        threading.Thread(target=self.refresh_all_prices_background, daemon=True).start()

    # File operations
    def load_portfolios(self):
        """Load portfolios from settings file"""
        if os.path.exists(SETTINGS_FILE):
            try:
                with open(SETTINGS_FILE, "r") as file:
                    settings = json.load(file)
                    portfolios = settings.get("portfolios", {})

                    # Filter out 'watchlist' which is handled by watchlist tab
                    if "watchlist" in portfolios:
                        del portfolios["watchlist"]

                    return portfolios
            except (json.JSONDecodeError, IOError):
                print("Error loading portfolios: Corrupted settings.json file.")
                return {}
        return {}

    def save_portfolios(self):
        """Save portfolios to settings file"""
        try:
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

            # Save back to file
            with open(SETTINGS_FILE, "w") as file:
                json.dump(settings, file, indent=4)

            print("Portfolios saved successfully.")

        except Exception as e:
            print(f"Error saving portfolios: {e}")
            self.show_message(f"Error saving portfolios: {e}", "error")

    # Additional callback methods
    def search_portfolio(self):
        """Search portfolio functionality"""
        print("🔍 Portfolio search functionality")

    def show_reports(self):
        """Show portfolio reports"""
        print("📊 Portfolio reports functionality")

    def show_settings(self):
        """Show portfolio settings"""
        print("⚙️ Portfolio settings functionality")

    def export_portfolio_data(self):
        """Export portfolio data"""
        print("💾 Export portfolio data functionality")
        self.show_message("Portfolio data export feature coming soon!", "info")

    def show_message(self, message, message_type="info"):
        """Show message to user"""
        type_symbols = {
            "success": "✅",
            "error": "❌",
            "warning": "⚠️",
            "info": "ℹ️"
        }
        symbol = type_symbols.get(message_type, "ℹ️")
        print(f"{symbol} {message}")

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle component resizing"""
        # Bloomberg terminal has fixed layout
        pass

    def cleanup(self):
        """Clean up portfolio tab resources"""
        try:
            print("🧹 Cleaning up portfolio tab...")
            self.refresh_running = False

            if hasattr(self, 'portfolios'):
                self.save_portfolios()

            self.portfolios = {}
            self.current_portfolio = None
            self.current_view = "overview"
            self.price_cache = {}
            self.last_price_update = {}

            print("✅ Portfolio tab cleanup complete")
        except Exception as e:
            print(f"Error in portfolio cleanup: {e}")