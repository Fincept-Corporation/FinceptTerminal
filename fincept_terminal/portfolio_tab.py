import dearpygui.dearpygui as dpg
from base_tab import BaseTab
import yfinance as yf
import json
import logging
from datetime import datetime, timedelta
import sys
import os
import platform
from collections import defaultdict
import threading
import time
import uuid


def get_settings_path():
    """Returns a consistent settings file path for both Python library and EXE."""
    # For simplicity, save in current directory as requested
    return "settings.json"


SETTINGS_FILE = get_settings_path()


class PortfolioTab(BaseTab):
    """Simplified Portfolio Management tab with comprehensive functionality"""

    def __init__(self, app):
        super().__init__(app)
        self.portfolios = self.load_portfolios()
        self.current_portfolio = None
        self.current_view = "overview"  # overview, create, manage, analyze
        self.price_cache = {}  # Cache for current prices
        self.last_price_update = {}  # Track when prices were last updated
        self.refresh_thread = None
        self.refresh_running = False

    def get_label(self):
        return "Portfolio"

    def create_content(self):
        """Create simplified portfolio management content"""
        # Create main container
        with dpg.child_window(
                tag="portfolio_main_container",
                width=-1,
                height=-1,
                horizontal_scrollbar=False,
                border=True
        ):
            # Create navigation
            self.create_navigation()
            dpg.add_spacer(height=10)

            # Content areas for each view (only one visible at a time)
            self.create_overview_content()
            self.create_create_content()
            self.create_manage_content()
            self.create_analyze_content()

            # Start price refresh thread
            self.start_price_refresh_thread()

    def create_navigation(self):
        """Create simplified navigation buttons"""
        with dpg.group(horizontal=True, tag="portfolio_nav"):
            dpg.add_button(
                label="📊 Overview",
                callback=lambda: self.switch_view("overview"),
                tag="overview_nav_btn",
                width=120
            )
            dpg.add_button(
                label="➕ Create Portfolio",
                callback=lambda: self.switch_view("create"),
                tag="create_nav_btn",
                width=140
            )
            dpg.add_button(
                label="📝 Manage Stocks",
                callback=lambda: self.switch_view("manage"),
                tag="manage_nav_btn",
                width=130
            )
            dpg.add_button(
                label="📈 Analytics",
                callback=lambda: self.switch_view("analyze"),
                tag="analyze_nav_btn",
                width=120
            )

        dpg.add_separator()

        # Initialize button states
        self.update_nav_button_states()

    def switch_view(self, view_name):
        """Switch between different views"""
        self.current_view = view_name

        # Hide all content areas
        for view in ["overview", "create", "manage", "analyze"]:
            if dpg.does_item_exist(f"portfolio_{view}_content"):
                dpg.hide_item(f"portfolio_{view}_content")

        # Show selected content area
        if dpg.does_item_exist(f"portfolio_{view_name}_content"):
            dpg.show_item(f"portfolio_{view_name}_content")

        # Update navigation button states
        self.update_nav_button_states()

        # Refresh content based on view
        if view_name == "overview":
            self.refresh_overview()
        elif view_name == "manage":
            self.refresh_manage_selectors()
        elif view_name == "analyze":
            self.refresh_analyze_selectors()

    def update_nav_button_states(self):
        """Update navigation button appearances"""
        buttons = {
            "overview": "overview_nav_btn",
            "create": "create_nav_btn",
            "manage": "manage_nav_btn",
            "analyze": "analyze_nav_btn"
        }

        for view, btn_tag in buttons.items():
            if dpg.does_item_exist(btn_tag):
                if view == self.current_view:
                    dpg.bind_item_theme(btn_tag, self.get_active_button_theme())
                else:
                    dpg.bind_item_theme(btn_tag, 0)

    def get_active_button_theme(self):
        """Create and return theme for active button"""
        if not dpg.does_item_exist("active_button_theme"):
            with dpg.theme(tag="active_button_theme"):
                with dpg.theme_component(dpg.mvButton):
                    dpg.add_theme_color(dpg.mvThemeCol_Button, [70, 130, 180, 255])
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [100, 150, 200, 255])
        return "active_button_theme"

    # ============================================================================
    # OVERVIEW CONTENT
    # ============================================================================

    def create_overview_content(self):
        """Create portfolio overview content"""
        with dpg.group(tag="portfolio_overview_content"):
            self.add_section_header("Portfolio Overview")

            # Quick stats
            with dpg.group(horizontal=True, tag="portfolio_stats"):
                with dpg.child_window(width=250, height=140, border=True):
                    dpg.add_text("📊 Portfolio Summary", color=[100, 255, 100])
                    dpg.add_spacer(height=5)
                    dpg.add_text("Total Portfolios: 0", tag="total_portfolios_text")
                    dpg.add_text("Total Investment: $0.00", tag="total_investment_text")
                    dpg.add_text("Current Value: $0.00", tag="total_current_value_text")
                    dpg.add_text("Total Gain/Loss: $0.00 (0.00%)", tag="total_gain_loss_text")
                    dpg.add_text("Last Updated: Never", tag="last_price_update_text")

                dpg.add_spacer(width=20)

                with dpg.child_window(width=-1, height=140, border=True):
                    dpg.add_text("🎯 Quick Actions", color=[100, 255, 100])
                    dpg.add_spacer(height=5)
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="Create New Portfolio", callback=lambda: self.switch_view("create"),
                                       width=150)
                        dpg.add_button(label="Refresh All Prices", callback=self.refresh_all_prices_now, width=130)
                    dpg.add_spacer(height=5)
                    with dpg.group(horizontal=True):
                        dpg.add_text("Auto-refresh:")
                        dpg.add_checkbox(tag="auto_refresh_checkbox", default_value=True,
                                         callback=self.toggle_auto_refresh)
                        dpg.add_text("(Every 1 hour)")

            dpg.add_spacer(height=20)

            # Portfolio list
            dpg.add_text("📁 Your Portfolios", color=[100, 255, 100])

            with dpg.table(
                    tag="overview_portfolios_table",
                    resizable=True,
                    policy=dpg.mvTable_SizingStretchProp,
                    borders_innerH=True,
                    borders_outerH=True,
                    borders_innerV=True,
                    borders_outerV=True,
                    height=350
            ):
                dpg.add_table_column(label="Portfolio Name", width_fixed=True, init_width_or_weight=150)
                dpg.add_table_column(label="Stocks", width_fixed=True, init_width_or_weight=70)
                dpg.add_table_column(label="Investment", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Current Value", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Today's Change", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Total Gain/Loss", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Actions", width_fixed=True, init_width_or_weight=200)

    # ============================================================================
    # CREATE PORTFOLIO CONTENT
    # ============================================================================

    def create_create_content(self):
        """Create simplified portfolio creation content"""
        with dpg.group(tag="portfolio_create_content", show=False):
            self.add_section_header("Create New Portfolio")

            dpg.add_text("📝 Step 1: Portfolio Details", color=[100, 255, 100])
            dpg.add_spacer(height=10)

            dpg.add_text("Portfolio Name:")
            dpg.add_input_text(tag="new_portfolio_name", hint="Enter a unique name for your portfolio", width=300)
            dpg.add_spacer(height=20)

            dpg.add_text("📈 Step 2: Add Your First Stock (Optional)", color=[100, 255, 100])
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                with dpg.group():
                    dpg.add_text("Stock Symbol:")
                    dpg.add_input_text(tag="create_stock_symbol", hint="e.g., RELIANCE.NS", width=150, uppercase=True)

                dpg.add_spacer(width=20)

                with dpg.group():
                    dpg.add_text("Quantity:")
                    dpg.add_input_text(tag="create_stock_quantity", hint="Number of shares", width=150, decimal=True)

                dpg.add_spacer(width=20)

                with dpg.group():
                    dpg.add_text("Purchase Price:")
                    dpg.add_input_text(tag="create_stock_price", hint="Price per share", width=150, decimal=True)

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_button(label="Create Portfolio", callback=self.create_portfolio_simple, width=150)
                dpg.add_button(label="Create & Add Stock", callback=self.create_portfolio_with_stock, width=150)
                dpg.add_button(label="Cancel", callback=lambda: self.switch_view("overview"), width=100)

    # ============================================================================
    # MANAGE STOCKS CONTENT
    # ============================================================================

    def create_manage_content(self):
        """Create simplified stock management content"""
        with dpg.group(tag="portfolio_manage_content", show=False):
            self.add_section_header("Manage Portfolio Stocks")

            # Portfolio selector
            dpg.add_text("📁 Select Portfolio:", color=[100, 255, 100])
            dpg.add_combo([], tag="portfolio_selector", callback=self.on_portfolio_select, width=300)
            dpg.add_spacer(height=20)

            # Current holdings
            dpg.add_text("📊 Current Holdings", color=[100, 255, 100])

            with dpg.table(
                    tag="manage_holdings_table",
                    resizable=True,
                    policy=dpg.mvTable_SizingStretchProp,
                    borders_innerH=True,
                    borders_outerH=True,
                    borders_innerV=True,
                    borders_outerV=True,
                    height=250
            ):
                dpg.add_table_column(label="Stock", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Shares", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Avg Cost", width_fixed=True, init_width_or_weight=100)
                dpg.add_table_column(label="Current Price", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Market Value", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Gain/Loss", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Actions", width_fixed=True, init_width_or_weight=100)

            dpg.add_spacer(height=20)

            # Add new stock
            dpg.add_text("➕ Add New Stock", color=[100, 255, 100])

            with dpg.group(horizontal=True):
                dpg.add_input_text(tag="manage_stock_symbol", hint="Stock Symbol", width=120, uppercase=True)
                dpg.add_input_text(tag="manage_stock_quantity", hint="Quantity", width=100, decimal=True)
                dpg.add_input_text(tag="manage_stock_price", hint="Purchase Price", width=120, decimal=True)
                dpg.add_button(label="Add Stock", callback=self.add_stock_simple, width=100)

    # ============================================================================
    # ANALYTICS CONTENT
    # ============================================================================

    def create_analyze_content(self):
        """Create portfolio analytics content with multiple charts"""
        with dpg.group(tag="portfolio_analyze_content", show=False):
            self.add_section_header("Portfolio Analytics")

            # Portfolio selector for analysis
            dpg.add_text("📁 Select Portfolio for Analysis:", color=[100, 255, 100])
            dpg.add_combo([], tag="analyze_portfolio_selector", callback=self.on_analyze_portfolio_select, width=300)
            dpg.add_spacer(height=20)

            # Charts container
            dpg.add_group(tag="analytics_charts_container")
            dpg.add_text("📊 Select a portfolio to view analytics", color=[200, 200, 200],
                         parent="analytics_charts_container")

    def create_analytics_charts(self, portfolio_name):
        """Create multiple analytics charts for portfolio with professional styling"""
        # Clear existing charts
        if dpg.does_item_exist("analytics_charts_container"):
            children = dpg.get_item_children("analytics_charts_container", slot=1)
            if children:
                for child in children:
                    dpg.delete_item(child)

        # Create unique tags to avoid conflicts
        chart_id = str(uuid.uuid4())[:8]

        dpg.add_text(f"📈 Portfolio Analytics: {portfolio_name}", parent="analytics_charts_container",
                     color=[100, 255, 100])
        dpg.add_spacer(height=15, parent="analytics_charts_container")

        # First row: Allocation charts
        with dpg.group(horizontal=True, parent="analytics_charts_container"):
            # Stock allocation pie chart
            with dpg.child_window(width=420, height=350, border=True):
                dpg.add_text("🥧 Portfolio Allocation (Value)", color=[255, 200, 100])
                dpg.add_separator()
                dpg.add_spacer(height=5)

                # Create pie chart using scatter plot with different sizes and colors
                with dpg.plot(height=280, width=-1, tag=f"stock_pie_plot_{chart_id}", no_title=True):
                    dpg.add_plot_legend()
                    dpg.add_plot_axis(dpg.mvXAxis, label="", tag=f"pie_x_axis_{chart_id}",
                                      no_tick_labels=True, no_gridlines=True)
                    dpg.add_plot_axis(dpg.mvYAxis, label="", tag=f"pie_y_axis_{chart_id}",
                                      no_tick_labels=True, no_gridlines=True)

                    self.create_portfolio_pie_chart(portfolio_name, chart_id)

            dpg.add_spacer(width=15)

            # Sector allocation pie chart
            with dpg.child_window(width=420, height=350, border=True):
                dpg.add_text("🏢 Sector Distribution", color=[255, 200, 100])
                dpg.add_separator()
                dpg.add_spacer(height=5)

                with dpg.plot(height=280, width=-1, tag=f"sector_pie_plot_{chart_id}", no_title=True):
                    dpg.add_plot_legend()
                    dpg.add_plot_axis(dpg.mvXAxis, label="", tag=f"sector_pie_x_{chart_id}",
                                      no_tick_labels=True, no_gridlines=True)
                    dpg.add_plot_axis(dpg.mvYAxis, label="", tag=f"sector_pie_y_{chart_id}",
                                      no_tick_labels=True, no_gridlines=True)

                    self.create_sector_pie_chart(portfolio_name, chart_id)

        dpg.add_spacer(height=20, parent="analytics_charts_container")

        # Second row: Performance and Returns
        with dpg.group(horizontal=True, parent="analytics_charts_container"):
            # Portfolio performance line chart
            with dpg.child_window(width=550, height=350, border=True):
                dpg.add_text("📊 Performance Tracking", color=[255, 200, 100])
                dpg.add_separator()
                dpg.add_spacer(height=5)

                with dpg.plot(height=280, width=-1, tag=f"performance_line_{chart_id}"):
                    dpg.add_plot_legend()
                    dpg.add_plot_axis(dpg.mvXAxis, label="Time Period", tag=f"perf_line_x_{chart_id}")
                    dpg.add_plot_axis(dpg.mvYAxis, label="Value (₹)", tag=f"perf_line_y_{chart_id}")

                    self.create_performance_line_chart(portfolio_name, chart_id)

            dpg.add_spacer(width=15)

            # Stock returns histogram
            with dpg.child_window(width=285, height=350, border=True):
                dpg.add_text("📈 Returns Distribution", color=[255, 200, 100])
                dpg.add_separator()
                dpg.add_spacer(height=5)

                with dpg.plot(height=280, width=-1, tag=f"returns_hist_{chart_id}"):
                    dpg.add_plot_legend()
                    dpg.add_plot_axis(dpg.mvXAxis, label="Return %", tag=f"hist_x_{chart_id}")
                    dpg.add_plot_axis(dpg.mvYAxis, label="Frequency", tag=f"hist_y_{chart_id}")

                    self.create_returns_histogram(portfolio_name, chart_id)

        dpg.add_spacer(height=20, parent="analytics_charts_container")

        # Third row: Stock prices and heatmap
        with dpg.group(horizontal=True, parent="analytics_charts_container"):
            # Individual stock prices candlestick
            with dpg.child_window(width=550, height=350, border=True):
                dpg.add_text("📊 Stock Price Charts", color=[255, 200, 100])
                dpg.add_separator()
                dpg.add_spacer(height=5)

                # Stock selector for price chart
                stock_list = list(self.portfolios.get(portfolio_name, {}).keys())
                if stock_list:
                    dpg.add_combo(stock_list, label="Select Stock:", tag=f"stock_selector_{chart_id}",
                                  default_value=stock_list[0],
                                  callback=lambda: self.update_stock_chart(portfolio_name, chart_id), width=150)
                    dpg.add_spacer(height=5)

                    with dpg.plot(height=220, width=-1, tag=f"stock_price_plot_{chart_id}"):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Days", tag=f"stock_x_{chart_id}")
                        dpg.add_plot_axis(dpg.mvYAxis, label="Price (₹)", tag=f"stock_y_{chart_id}")

                        self.create_stock_price_chart(portfolio_name, chart_id, stock_list[0])
                else:
                    dpg.add_text("No stocks in portfolio", color=[200, 200, 200])

            dpg.add_spacer(width=15)

            # Performance heatmap (using scatter plot)
            with dpg.child_window(width=285, height=350, border=True):
                dpg.add_text("🔥 Performance Heatmap", color=[255, 200, 100])
                dpg.add_separator()
                dpg.add_spacer(height=5)

                with dpg.plot(height=280, width=-1, tag=f"heatmap_plot_{chart_id}"):
                    dpg.add_plot_legend()
                    dpg.add_plot_axis(dpg.mvXAxis, label="Risk", tag=f"heat_x_{chart_id}")
                    dpg.add_plot_axis(dpg.mvYAxis, label="Return", tag=f"heat_y_{chart_id}")

                    self.create_performance_heatmap(portfolio_name, chart_id)

    def create_portfolio_pie_chart(self, portfolio_name, chart_id):
        """Create portfolio allocation pie chart using scatter plot with circles"""
        try:
            portfolio = self.portfolios.get(portfolio_name, {})
            if not portfolio:
                return

            # Calculate allocations with current prices
            stock_values = {}
            total_value = 0

            for symbol, data in portfolio.items():
                if isinstance(data, dict):
                    quantity = data.get('quantity', 0)
                    current_price = self.get_current_price(symbol)
                    if current_price > 0:
                        value = quantity * current_price
                        stock_values[symbol] = value
                        total_value += value

            if not stock_values or total_value == 0:
                return

            # Create pie chart using scatter plot with circles
            import math
            center_x, center_y = 0, 0
            radius = 1

            # Sort by value and take top 8 stocks
            sorted_stocks = sorted(stock_values.items(), key=lambda x: x[1], reverse=True)[:8]
            colors = [
                [255, 99, 132], [54, 162, 235], [255, 205, 86], [75, 192, 192],
                [153, 102, 255], [255, 159, 64], [199, 199, 199], [83, 102, 147]
            ]

            current_angle = 0
            for i, (symbol, value) in enumerate(sorted_stocks):
                percentage = (value / total_value) * 100
                angle_span = (value / total_value) * 2 * math.pi

                # Create arc points for the pie slice
                num_points = max(int(angle_span * 20), 3)  # More points for larger slices
                x_points = []
                y_points = []

                # Add center point
                x_points.append(center_x)
                y_points.append(center_y)

                # Add arc points
                for j in range(num_points + 1):
                    angle = current_angle + (j / num_points) * angle_span
                    x = center_x + radius * math.cos(angle)
                    y = center_y + radius * math.sin(angle)
                    x_points.append(x)
                    y_points.append(y)

                # Add the slice as a filled area
                dpg.add_shade_series(
                    x_points, y_points,
                    label=f"{symbol} ({percentage:.1f}%)",
                    parent=f"pie_y_axis_{chart_id}",
                    tag=f"pie_slice_{chart_id}_{i}"
                )

                current_angle += angle_span

        except Exception as e:
            print(f"Error creating portfolio pie chart: {e}")

    def create_sector_pie_chart(self, portfolio_name, chart_id):
        """Create sector allocation pie chart"""
        try:
            portfolio = self.portfolios.get(portfolio_name, {})
            if not portfolio:
                return

            sector_data = defaultdict(float)
            total_value = 0

            for symbol, data in portfolio.items():
                if isinstance(data, dict):
                    try:
                        stock = yf.Ticker(symbol)
                        info = stock.info
                        sector = info.get('sector', 'Unknown')

                        quantity = data.get('quantity', 0)
                        current_price = self.get_current_price(symbol)

                        if current_price > 0:
                            value = quantity * current_price
                            sector_data[sector] += value
                            total_value += value
                    except Exception:
                        continue

            if not sector_data or total_value == 0:
                return

            # Create sector pie chart
            import math
            center_x, center_y = 0, 0
            radius = 1

            colors = [
                [255, 99, 132], [54, 162, 235], [255, 205, 86], [75, 192, 192],
                [153, 102, 255], [255, 159, 64], [199, 199, 199], [83, 102, 147]
            ]

            current_angle = 0
            for i, (sector, value) in enumerate(sector_data.items()):
                percentage = (value / total_value) * 100
                angle_span = (value / total_value) * 2 * math.pi

                # Create arc points
                num_points = max(int(angle_span * 20), 3)
                x_points = [center_x]
                y_points = [center_y]

                for j in range(num_points + 1):
                    angle = current_angle + (j / num_points) * angle_span
                    x = center_x + radius * math.cos(angle)
                    y = center_y + radius * math.sin(angle)
                    x_points.append(x)
                    y_points.append(y)

                dpg.add_shade_series(
                    x_points, y_points,
                    label=f"{sector} ({percentage:.1f}%)",
                    parent=f"sector_pie_y_{chart_id}",
                    tag=f"sector_slice_{chart_id}_{i}"
                )

                current_angle += angle_span

        except Exception as e:
            print(f"Error creating sector pie chart: {e}")

    def create_performance_line_chart(self, portfolio_name, chart_id):
        """Create portfolio performance line chart over time"""
        try:
            portfolio = self.portfolios.get(portfolio_name, {})
            if not portfolio:
                return

            # Simulate portfolio performance over 30 days
            # In real implementation, you'd track historical portfolio values
            import random
            random.seed(hash(portfolio_name) % 2147483647)  # Consistent seed for same portfolio

            days = list(range(30))

            # Calculate current portfolio value
            current_value = 0
            investment_value = 0

            for symbol, data in portfolio.items():
                if isinstance(data, dict):
                    quantity = data.get('quantity', 0)
                    avg_price = data.get('avg_price', 0)
                    current_price = self.get_current_price(symbol)

                    investment_value += quantity * avg_price
                    current_value += quantity * (current_price if current_price > 0 else avg_price)

            # Generate simulated historical performance
            investment_line = [investment_value] * 30

            portfolio_values = []
            base_value = investment_value

            for i in range(30):
                if i == 29:  # Last day is current value
                    portfolio_values.append(current_value)
                else:
                    # Simulate daily changes
                    daily_change = random.uniform(-0.03, 0.04)  # -3% to +4%
                    base_value *= (1 + daily_change)
                    portfolio_values.append(base_value)

            # Add investment baseline
            dpg.add_line_series(
                days, investment_line,
                label="Investment Amount",
                parent=f"perf_line_y_{chart_id}",
                tag=f"investment_line_{chart_id}"
            )

            # Add portfolio value line
            dpg.add_line_series(
                days, portfolio_values,
                label="Portfolio Value",
                parent=f"perf_line_y_{chart_id}",
                tag=f"portfolio_line_{chart_id}"
            )

        except Exception as e:
            print(f"Error creating performance line chart: {e}")

    def create_returns_histogram(self, portfolio_name, chart_id):
        """Create returns distribution histogram"""
        try:
            portfolio = self.portfolios.get(portfolio_name, {})
            if not portfolio:
                return

            returns = []

            for symbol, data in portfolio.items():
                if isinstance(data, dict):
                    try:
                        current_price = self.get_current_price(symbol)
                        avg_price = data.get('avg_price', 0)

                        if avg_price > 0 and current_price > 0:
                            return_pct = ((current_price - avg_price) / avg_price) * 100
                            returns.append(return_pct)
                    except Exception:
                        continue

            if not returns:
                return

            # Create histogram bins
            min_return = min(returns)
            max_return = max(returns)

            # Create 7 bins
            bin_count = min(7, len(returns))
            bin_width = (max_return - min_return) / bin_count if bin_count > 1 else 1

            bins = []
            counts = []

            for i in range(bin_count):
                bin_start = min_return + i * bin_width
                bin_end = bin_start + bin_width

                count = sum(1 for r in returns if bin_start <= r < bin_end)
                if i == bin_count - 1:  # Include max value in last bin
                    count = sum(1 for r in returns if bin_start <= r <= bin_end)

                bins.append(bin_start)
                counts.append(count)

            # Add histogram
            dpg.add_bar_series(
                list(range(len(bins))), counts,
                label="Return Distribution",
                parent=f"hist_y_{chart_id}",
                tag=f"histogram_{chart_id}"
            )

        except Exception as e:
            print(f"Error creating returns histogram: {e}")

    def create_stock_price_chart(self, portfolio_name, chart_id, selected_stock):
        """Create candlestick chart for individual stock"""
        try:
            if not selected_stock:
                return

            # Fetch stock data
            stock = yf.Ticker(selected_stock)
            data = stock.history(period="1mo", interval="1d")

            if data.empty:
                return

            # Prepare data for candlestick chart
            dates = list(range(len(data)))
            opens = data["Open"].tolist()
            highs = data["High"].tolist()
            lows = data["Low"].tolist()
            closes = data["Close"].tolist()

            # Add candlestick series
            dpg.add_candle_series(
                dates, opens, closes, lows, highs,
                label=f"{selected_stock} OHLC",
                parent=f"stock_y_{chart_id}",
                tag=f"candle_{chart_id}"
            )

        except Exception as e:
            print(f"Error creating stock price chart: {e}")

    def create_performance_heatmap(self, portfolio_name, chart_id):
        """Create performance heatmap using scatter plot"""
        try:
            portfolio = self.portfolios.get(portfolio_name, {})
            if not portfolio:
                return

            x_risk = []  # Risk metric (volatility)
            y_return = []  # Return metric
            sizes = []  # Bubble size based on investment amount

            for symbol, data in portfolio.items():
                if isinstance(data, dict):
                    try:
                        # Calculate return
                        current_price = self.get_current_price(symbol)
                        avg_price = data.get('avg_price', 0)
                        quantity = data.get('quantity', 0)

                        if avg_price > 0 and current_price > 0:
                            return_pct = ((current_price - avg_price) / avg_price) * 100

                            # Simple risk metric (you could use actual volatility)
                            # For now, use price variation as proxy
                            stock = yf.Ticker(symbol)
                            hist_data = stock.history(period="1mo")
                            if not hist_data.empty:
                                volatility = hist_data["Close"].std() / hist_data["Close"].mean() * 100
                            else:
                                volatility = abs(return_pct) * 0.5  # Fallback

                            investment_amount = quantity * avg_price

                            x_risk.append(volatility)
                            y_return.append(return_pct)
                            sizes.append(max(investment_amount / 1000, 10))  # Scale size

                    except Exception:
                        continue

            if x_risk and y_return:
                # Create scatter plot heatmap
                dpg.add_scatter_series(
                    x_risk, y_return,
                    label="Risk vs Return",
                    parent=f"heat_y_{chart_id}",
                    tag=f"heatmap_{chart_id}"
                )

        except Exception as e:
            print(f"Error creating performance heatmap: {e}")

    def update_stock_chart(self, portfolio_name, chart_id):
        """Update stock price chart when selection changes"""
        try:
            selected_stock = dpg.get_value(f"stock_selector_{chart_id}")

            # Clear existing candlestick data
            if dpg.does_item_exist(f"candle_{chart_id}"):
                dpg.delete_item(f"candle_{chart_id}")

            # Create new chart
            self.create_stock_price_chart(portfolio_name, chart_id, selected_stock)

        except Exception as e:
            print(f"Error updating stock chart: {e}")
            f"Error calculating return for : {e}"



    # ============================================================================
    # PRICE MANAGEMENT METHODS
    # ============================================================================

    def get_current_price(self, symbol):
        """Get current price from cache or fetch if needed"""
        try:
            # Check if we have cached price that's recent (less than 1 hour old)
            if symbol in self.price_cache and symbol in self.last_price_update:
                last_update = self.last_price_update[symbol]
                if (datetime.now() - last_update).total_seconds() < 3600:  # 1 hour
                    return self.price_cache[symbol]

            # Fetch new price
            stock = yf.Ticker(symbol)
            data = stock.history(period="1d", interval="1m")

            if not data.empty:
                current_price = data["Close"].iloc[-1]
                self.price_cache[symbol] = current_price
                self.last_price_update[symbol] = datetime.now()
                return current_price

            return 0
        except Exception as e:
            print(f"Error fetching price for {symbol}: {e}")
            return self.price_cache.get(symbol, 0)

    def calculate_today_change(self, symbol):
        """Calculate today's change for a symbol"""
        try:
            stock = yf.Ticker(symbol)
            data = stock.history(period="2d", interval="1d")

            if len(data) >= 2:
                current_price = data["Close"].iloc[-1]
                prev_close = data["Close"].iloc[-2]
                change = current_price - prev_close
                change_pct = (change / prev_close) * 100 if prev_close != 0 else 0
                return change, change_pct

            return 0, 0
        except Exception as e:
            print(f"Error calculating today's change for {symbol}: {e}")
            return 0, 0

    def start_price_refresh_thread(self):
        """Start the auto price refresh thread"""
        if not self.refresh_running:
            self.refresh_running = True
            self.refresh_thread = threading.Thread(target=self._price_refresh_loop, daemon=True)
            self.refresh_thread.start()

    def stop_price_refresh_thread(self):
        """Stop the auto price refresh thread"""
        self.refresh_running = False

    def _price_refresh_loop(self):
        """Background thread for price refresh"""
        while self.refresh_running:
            try:
                # Check if auto-refresh is enabled
                if dpg.does_item_exist("auto_refresh_checkbox") and dpg.get_value("auto_refresh_checkbox"):
                    self.refresh_all_prices_background()

                # Wait 1 hour between updates
                for _ in range(3600):  # 3600 seconds = 1 hour
                    if not self.refresh_running:
                        break
                    time.sleep(1)

            except Exception as e:
                print(f"Error in price refresh loop: {e}")
                time.sleep(60)  # Wait 1 minute before retry

    def refresh_all_prices_background(self):
        """Refresh all prices in background"""
        try:
            all_symbols = set()
            for portfolio in self.portfolios.values():
                if isinstance(portfolio, dict):
                    all_symbols.update(portfolio.keys())

            # Fetch prices for all symbols
            for symbol in all_symbols:
                self.get_current_price(symbol)
                time.sleep(0.1)  # Small delay between requests

            # Update displays
            if self.current_view == "overview":
                self.refresh_overview()
            elif self.current_view == "manage" and self.current_portfolio:
                self.refresh_manage_view()

            # Update last refresh time
            if dpg.does_item_exist("last_price_update_text"):
                dpg.set_value("last_price_update_text", f"Last Updated: {datetime.now().strftime('%H:%M:%S')}")

        except Exception as e:
            print(f"Error refreshing prices: {e}")

    def refresh_all_prices_now(self):
        """Refresh all prices immediately"""
        self.show_message("Refreshing all prices...", "info")
        threading.Thread(target=self.refresh_all_prices_background, daemon=True).start()

    def toggle_auto_refresh(self):
        """Toggle auto refresh on/off"""
        enabled = dpg.get_value("auto_refresh_checkbox")
        if enabled:
            self.show_message("Auto-refresh enabled (every 1 hour)", "info")
        else:
            self.show_message("Auto-refresh disabled", "info")

    # ============================================================================
    # CALLBACK METHODS
    # ============================================================================

    def create_portfolio_simple(self):
        """Create a simple portfolio"""
        name = dpg.get_value("new_portfolio_name").strip()
        if not name:
            self.show_message("Please enter a portfolio name.", "error")
            return

        if name in self.portfolios:
            self.show_message("Portfolio name already exists.", "error")
            return

        self.portfolios[name] = {}
        self.save_portfolios()
        self.refresh_overview()

        # Clear input and switch to overview
        dpg.set_value("new_portfolio_name", "")
        self.switch_view("overview")

        self.show_message(f"Portfolio '{name}' created successfully!", "success")

    def create_portfolio_with_stock(self):
        """Create portfolio and add first stock"""
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
                        "last_added": datetime.now().strftime("%Y-%m-%d")
                    }
                }
            except ValueError:
                self.show_message("Invalid quantity or price values.", "error")
                return
        else:
            # Create empty portfolio
            self.portfolios[name] = {}

        self.save_portfolios()
        self.refresh_overview()

        # Clear inputs and switch to overview
        dpg.set_value("new_portfolio_name", "")
        dpg.set_value("create_stock_symbol", "")
        dpg.set_value("create_stock_quantity", "")
        dpg.set_value("create_stock_price", "")
        self.switch_view("overview")

        success_msg = f"Portfolio '{name}' created successfully!"
        if symbol:
            success_msg += f" Added {quantity} shares of {symbol} at ₹{price:.2f}."

        self.show_message(success_msg, "success")

    def on_portfolio_select(self):
        """Handle portfolio selection in manage view"""
        selected = dpg.get_value("portfolio_selector")
        if selected:
            self.current_portfolio = selected
            self.refresh_manage_view()

    def on_analyze_portfolio_select(self):
        """Handle portfolio selection in analyze view"""
        selected = dpg.get_value("analyze_portfolio_selector")
        if selected:
            self.current_portfolio = selected
            threading.Thread(target=self.refresh_analytics, daemon=True).start()

    def add_stock_simple(self):
        """Add stock to selected portfolio"""
        if not self.current_portfolio:
            self.show_message("Please select a portfolio first.", "error")
            return

        symbol = dpg.get_value("manage_stock_symbol").strip().upper()
        quantity_str = dpg.get_value("manage_stock_quantity").strip()
        price_str = dpg.get_value("manage_stock_price").strip()

        if not symbol or not quantity_str or not price_str:
            self.show_message("Please fill in all fields.", "error")
            return

        try:
            quantity = float(quantity_str)
            price = float(price_str)
        except ValueError:
            self.show_message("Invalid quantity or price values.", "error")
            return

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
                "last_added": datetime.now().strftime("%Y-%m-%d")
            }
        else:
            # Add new stock
            self.portfolios[self.current_portfolio][symbol] = {
                "quantity": quantity,
                "avg_price": price,
                "last_added": datetime.now().strftime("%Y-%m-%d")
            }

        self.save_portfolios()
        self.refresh_manage_view()

        # Clear inputs
        dpg.set_value("manage_stock_symbol", "")
        dpg.set_value("manage_stock_quantity", "")
        dpg.set_value("manage_stock_price", "")

        self.show_message(f"Added {quantity} shares of {symbol} to {self.current_portfolio}.", "success")

    def refresh_overview(self):
        """Refresh the overview content with real calculations"""
        # Calculate portfolio summary
        total_portfolios = len(self.portfolios)
        total_investment = 0
        total_current_value = 0

        # Clear table first
        self.clear_table("overview_portfolios_table")

        for portfolio_name, stocks in self.portfolios.items():
            if not isinstance(stocks, dict):
                continue

            portfolio_investment = 0
            portfolio_current_value = 0
            portfolio_today_change = 0

            for symbol, data in stocks.items():
                if isinstance(data, dict):
                    quantity = data.get('quantity', 0)
                    avg_price = data.get('avg_price', 0)
                    current_price = self.get_current_price(symbol)

                    investment = quantity * avg_price
                    current_value = quantity * current_price if current_price > 0 else investment

                    # Calculate today's change
                    today_change, _ = self.calculate_today_change(symbol)
                    today_change_value = quantity * today_change

                    portfolio_investment += investment
                    portfolio_current_value += current_value
                    portfolio_today_change += today_change_value

            total_investment += portfolio_investment
            total_current_value += portfolio_current_value

            # Calculate gain/loss
            gain_loss = portfolio_current_value - portfolio_investment
            gain_loss_pct = (gain_loss / portfolio_investment * 100) if portfolio_investment > 0 else 0

            # Add row to table
            with dpg.table_row(parent="overview_portfolios_table"):
                dpg.add_text(portfolio_name)
                dpg.add_text(str(len(stocks)))
                dpg.add_text(f"₹{portfolio_investment:.2f}")
                dpg.add_text(f"₹{portfolio_current_value:.2f}")

                # Today's change with color coding
                change_color = [100, 255, 100] if portfolio_today_change >= 0 else [255, 100, 100]
                dpg.add_text(f"₹{portfolio_today_change:+.2f}", color=change_color)

                # Total gain/loss with color coding
                gain_color = [100, 255, 100] if gain_loss >= 0 else [255, 100, 100]
                dpg.add_text(f"₹{gain_loss:+.2f} ({gain_loss_pct:+.1f}%)", color=gain_color)

                # Action buttons
                with dpg.group(horizontal=True):
                    dpg.add_button(
                        label="Manage",
                        callback=lambda p=portfolio_name: self.select_and_manage_portfolio(p),
                        width=60
                    )
                    dpg.add_button(
                        label="Analyze",
                        callback=lambda p=portfolio_name: self.select_and_analyze_portfolio(p),
                        width=60
                    )
                    dpg.add_button(
                        label="Delete",
                        callback=lambda p=portfolio_name: self.delete_portfolio_confirm(p),
                        width=60
                    )

        # Update summary stats
        total_gain_loss = total_current_value - total_investment
        total_gain_loss_pct = (total_gain_loss / total_investment * 100) if total_investment > 0 else 0

        dpg.set_value("total_portfolios_text", f"Total Portfolios: {total_portfolios}")
        dpg.set_value("total_investment_text", f"Total Investment: ₹{total_investment:.2f}")
        dpg.set_value("total_current_value_text", f"Current Value: ₹{total_current_value:.2f}")

        gain_text = f"Total Gain/Loss: ₹{total_gain_loss:+.2f} ({total_gain_loss_pct:+.1f}%)"
        dpg.set_value("total_gain_loss_text", gain_text)

    def refresh_manage_view(self):
        """Refresh the manage view content with real data"""
        if not self.current_portfolio:
            return

        # Update holdings table with real prices
        self.clear_table("manage_holdings_table")

        portfolio = self.portfolios[self.current_portfolio]
        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get("quantity", 0)
                avg_price = data.get("avg_price", 0)
                current_price = self.get_current_price(symbol)

                market_value = quantity * current_price if current_price > 0 else quantity * avg_price
                investment = quantity * avg_price
                gain_loss = market_value - investment
                gain_loss_pct = (gain_loss / investment * 100) if investment > 0 else 0

                with dpg.table_row(parent="manage_holdings_table"):
                    dpg.add_text(symbol)
                    dpg.add_text(f"{quantity:.2f}")
                    dpg.add_text(f"₹{avg_price:.2f}")
                    dpg.add_text(f"₹{current_price:.2f}" if current_price > 0 else "Fetching...")
                    dpg.add_text(f"₹{market_value:.2f}")

                    # Gain/loss with color coding
                    gain_color = [100, 255, 100] if gain_loss >= 0 else [255, 100, 100]
                    dpg.add_text(f"₹{gain_loss:+.2f} ({gain_loss_pct:+.1f}%)", color=gain_color)

                    dpg.add_button(
                        label="Remove",
                        callback=lambda s=symbol: self.remove_stock_from_portfolio(s),
                        width=80
                    )

    def refresh_manage_selectors(self):
        """Refresh portfolio selectors in manage view"""
        portfolio_names = list(self.portfolios.keys())
        if dpg.does_item_exist("portfolio_selector"):
            dpg.configure_item("portfolio_selector", items=portfolio_names)

    def refresh_analyze_selectors(self):
        """Refresh portfolio selectors in analyze view"""
        portfolio_names = list(self.portfolios.keys())
        if dpg.does_item_exist("analyze_portfolio_selector"):
            dpg.configure_item("analyze_portfolio_selector", items=portfolio_names)

    def refresh_analytics(self):
        """Refresh analytics view"""
        if not self.current_portfolio:
            return

        # Update portfolio selector
        portfolio_names = list(self.portfolios.keys())
        dpg.configure_item("analyze_portfolio_selector", items=portfolio_names, default_value=self.current_portfolio)

        # Create analytics charts
        self.create_analytics_charts(self.current_portfolio)

    def select_and_manage_portfolio(self, portfolio_name):
        """Select portfolio and switch to manage view"""
        self.current_portfolio = portfolio_name
        self.switch_view("manage")
        # Update selector and refresh view
        dpg.set_value("portfolio_selector", portfolio_name)
        self.refresh_manage_view()

    def select_and_analyze_portfolio(self, portfolio_name):
        """Select portfolio and switch to analyze view"""
        self.current_portfolio = portfolio_name
        self.switch_view("analyze")
        # Update selector and refresh analytics
        dpg.set_value("analyze_portfolio_selector", portfolio_name)
        threading.Thread(target=self.refresh_analytics, daemon=True).start()

    def delete_portfolio_confirm(self, portfolio_name):
        """Confirm and delete portfolio"""
        if portfolio_name in self.portfolios:
            del self.portfolios[portfolio_name]
            self.save_portfolios()
            self.refresh_overview()

            if self.current_portfolio == portfolio_name:
                self.current_portfolio = None

            self.show_message(f"Portfolio '{portfolio_name}' deleted successfully.", "success")

    def remove_stock_from_portfolio(self, symbol):
        """Remove stock from current portfolio"""
        if not self.current_portfolio or symbol not in self.portfolios[self.current_portfolio]:
            return

        del self.portfolios[self.current_portfolio][symbol]
        self.save_portfolios()
        self.refresh_manage_view()

        self.show_message(f"Removed {symbol} from {self.current_portfolio}.", "success")

    # ============================================================================
    # UTILITY METHODS
    # ============================================================================

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
                logging.error("Error loading portfolios: Corrupted settings.json file.")
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

            logging.info("Portfolios saved successfully.")

        except Exception as e:
            logging.error(f"Error saving portfolios: {e}")
            self.show_message(f"Error saving portfolios: {e}", "error")

    def clear_table(self, table_tag):
        """Clear all rows from a table while preserving columns"""
        if dpg.does_item_exist(table_tag):
            children = dpg.get_item_children(table_tag, slot=1)  # slot=1 for table rows
            if children:
                for child in children:
                    dpg.delete_item(child)

    def show_message(self, message, message_type="info"):
        """Show message to user"""
        if hasattr(self.app, 'show_message'):
            self.app.show_message(message, message_type)
        else:
            type_symbols = {
                "success": "✅",
                "error": "❌",
                "warning": "⚠️",
                "info": "ℹ️"
            }
            symbol = type_symbols.get(message_type, "ℹ️")
            print(f"{symbol} {message}")

    def resize_components(self, left_width, center_width, right_width,
                          top_height, bottom_height, cell_height):
        """Handle component resizing"""
        try:
            if dpg.does_item_exist("portfolio_main_container"):
                # The child window will automatically resize with parent
                pass
        except Exception as e:
            print(f"Error resizing portfolio components: {e}")

    def cleanup(self):
        """Clean up portfolio tab resources"""
        try:
            # Stop price refresh thread
            self.stop_price_refresh_thread()

            # Save any unsaved changes
            if hasattr(self, 'portfolios'):
                self.save_portfolios()

            # Clean up resources
            self.portfolios = {}
            self.current_portfolio = None
            self.current_view = "overview"
            self.price_cache = {}
            self.last_price_update = {}

            print("✅ Portfolio tab cleaned up")
        except Exception as e:
            print(f"❌ Portfolio tab cleanup error: {e}")