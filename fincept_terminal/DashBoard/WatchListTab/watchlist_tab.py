# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import datetime
import threading
import time
import random
import json
import os

# Try to import yfinance for real data, fallback to simulated data
try:
    import yfinance as yf

    HAS_YFINANCE = True
except ImportError:
    HAS_YFINANCE = False
    print("yfinance not available, using simulated data")

# Settings file path
SETTINGS_FILE = "watchlist_settings.json"


class WatchlistTab(BaseTab):
    """Bloomberg Terminal style Watchlist tab - Clean and Working"""

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

        # Watchlist data
        self.watchlist = {}
        self.settings = {}
        self.auto_update = True
        self.refresh_running = False
        self.selected_ticker = None

        # Unique identifier for this instance
        self.instance_id = str(id(self))

        # Load settings and initialize
        self.load_settings()
        self.initialize_watchlist()

    def get_label(self):
        return "Watchlist"

    def load_settings(self):
        """Load settings from file"""
        if os.path.exists(SETTINGS_FILE):
            try:
                with open(SETTINGS_FILE, "r") as f:
                    self.settings = json.load(f)
            except (json.JSONDecodeError, IOError):
                self.settings = {}
        else:
            self.settings = {}

    def save_settings(self):
        """Save settings to file"""
        try:
            with open(SETTINGS_FILE, "w") as f:
                json.dump(self.settings, f, indent=4)
        except IOError as e:
            print(f"Error saving settings: {e}")

    def initialize_watchlist(self):
        """Initialize watchlist from settings or with sample data"""
        # Load from settings if available
        if "portfolios" in self.settings and "watchlist" in self.settings["portfolios"]:
            self.watchlist = {}
            for ticker, data in self.settings["portfolios"]["watchlist"].items():
                self.watchlist[ticker] = {
                    "quantity": data.get("quantity", 0),
                    "avg_price": data.get("avg_price", 0),
                    "alert_price": data.get("alert_price"),
                    "current_price": 0.0,
                    "change_1d": 0.0,
                    "change_pct_1d": 0.0,
                    "change_pct_7d": 0.0,
                    "change_pct_30d": 0.0,
                    "last_updated": "",
                    "total_value": 0.0,
                    "unrealized_pnl": 0.0
                }
        else:
            # Initialize with sample data if no settings
            self.initialize_sample_watchlist()

    def initialize_sample_watchlist(self):
        """Initialize with sample watchlist data"""
        sample_stocks = {
            "AAPL": {"quantity": 100, "avg_price": 150.25},
            "MSFT": {"quantity": 50, "avg_price": 305.80},
            "GOOGL": {"quantity": 25, "avg_price": 2650.40},
            "TSLA": {"quantity": 75, "avg_price": 220.15},
            "AMZN": {"quantity": 30, "avg_price": 3280.75},
            "NVDA": {"quantity": 40, "avg_price": 450.60},
            "META": {"quantity": 60, "avg_price": 315.90},
            "NFLX": {"quantity": 20, "avg_price": 485.30}
        }

        for ticker, data in sample_stocks.items():
            # Generate realistic current data
            avg_price = data["avg_price"]
            current_price = avg_price * (1 + random.uniform(-0.15, 0.15))
            change_1d = current_price * random.uniform(-0.03, 0.03)
            change_pct_1d = (change_1d / current_price) * 100

            self.watchlist[ticker] = {
                "quantity": data["quantity"],
                "avg_price": avg_price,
                "alert_price": None,
                "current_price": round(current_price, 2),
                "change_1d": round(change_1d, 2),
                "change_pct_1d": round(change_pct_1d, 2),
                "change_pct_7d": round(random.uniform(-8, 8), 2),
                "change_pct_30d": round(random.uniform(-20, 20), 2),
                "last_updated": datetime.datetime.now().strftime("%H:%M:%S"),
                "total_value": round(current_price * data["quantity"], 2),
                "unrealized_pnl": round((current_price - avg_price) * data["quantity"], 2)
            }

        # Save to settings
        self.save_watchlist_to_settings()

    def get_tag(self, base_name):
        """Generate unique tag for this instance"""
        return f"{base_name}_{self.instance_id}"

    def create_content(self):
        """Create Bloomberg-style watchlist terminal layout"""
        try:
            # Delete any existing items with potential duplicate tags
            self.cleanup_existing_items()

            # Top bar with branding
            with dpg.group(horizontal=True):
                dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("WATCHLIST TERMINAL", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_input_text(default_value="", hint="Add Symbol", width=150,
                                   tag=self.get_tag("add_symbol_input"), uppercase=True)
                dpg.add_input_text(default_value="", hint="Qty", width=80,
                                   tag=self.get_tag("add_qty_input"), decimal=True)
                dpg.add_input_text(default_value="", hint="Avg Price", width=100,
                                   tag=self.get_tag("add_price_input"), decimal=True)
                dpg.add_input_text(default_value="", hint="Alert Price", width=100,
                                   tag=self.get_tag("add_alert_input"), decimal=True)
                dpg.add_button(label="ADD", width=60, callback=self.add_to_watchlist_callback)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                             tag=self.get_tag("watchlist_time"))

            dpg.add_separator()

            # Control panel
            with dpg.group(horizontal=True):
                dpg.add_button(label="REFRESH", width=80, callback=self.refresh_all_callback)
                dpg.add_button(label="REMOVE", width=80, callback=self.remove_ticker_callback)
                dpg.add_button(label="AUTO ON", tag=self.get_tag("auto_toggle_btn"), width=80,
                               callback=self.toggle_auto_update)
                dpg.add_button(label="CHART", width=80, callback=self.show_chart_callback)
                dpg.add_button(label="INFO", width=80, callback=self.show_info_callback)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("LAST UPDATE:", color=self.BLOOMBERG_GRAY)
                dpg.add_text(datetime.datetime.now().strftime('%H:%M:%S'),
                             tag=self.get_tag("last_update_time"), color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("LIVE", color=self.BLOOMBERG_GREEN)

            dpg.add_separator()

            # Main area
            with dpg.group(horizontal=True):
                # Left panel - Watchlist table
                self.create_watchlist_panel()

                # Right panel - Portfolio summary
                self.create_summary_panel()

            # Bottom status bar
            dpg.add_separator()
            self.create_status_bar()

            # Start auto-update
            if self.auto_update:
                self.start_auto_update()

        except Exception as e:
            print(f"Error creating watchlist content: {e}")
            import traceback
            traceback.print_exc()
            # Fallback minimal content
            dpg.add_text("WATCHLIST TERMINAL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text(f"Error loading watchlist: {e}")

    def cleanup_existing_items(self):
        """Clean up any existing items that might cause tag conflicts"""
        tags_to_cleanup = [
            "add_symbol_input", "add_qty_input", "add_price_input", "add_alert_input",
            "watchlist_time", "auto_toggle_btn", "last_update_time", "watchlist_table",
            "total_value_text", "total_pnl_text", "day_pnl_text", "positions_count",
            "top_holdings_group", "status_positions", "auto_status", "selected_ticker_text"
        ]

        for tag in tags_to_cleanup:
            unique_tag = self.get_tag(tag)
            if dpg.does_item_exist(unique_tag):
                try:
                    dpg.delete_item(unique_tag)
                except:
                    pass

    def create_watchlist_panel(self):
        """Create watchlist table panel"""
        with dpg.child_window(width=1000, height=550, border=True):
            dpg.add_text("PORTFOLIO WATCHLIST", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Watchlist table
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           borders_innerV=True, borders_outerV=True,
                           scrollY=True, height=480, tag=self.get_tag("watchlist_table")):
                dpg.add_table_column(label="Symbol", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Qty", width_fixed=True, init_width_or_weight=70)
                dpg.add_table_column(label="Avg Price", width_fixed=True, init_width_or_weight=90)
                dpg.add_table_column(label="Current", width_fixed=True, init_width_or_weight=90)
                dpg.add_table_column(label="Chg", width_fixed=True, init_width_or_weight=70)
                dpg.add_table_column(label="Chg%", width_fixed=True, init_width_or_weight=70)
                dpg.add_table_column(label="7D%", width_fixed=True, init_width_or_weight=70)
                dpg.add_table_column(label="30D%", width_fixed=True, init_width_or_weight=70)
                dpg.add_table_column(label="Total Value", width_fixed=True, init_width_or_weight=110)
                dpg.add_table_column(label="P&L", width_fixed=True, init_width_or_weight=90)
                dpg.add_table_column(label="Alert", width_fixed=True, init_width_or_weight=80)

                # Populate table
                self.populate_watchlist_table()

    def create_summary_panel(self):
        """Create portfolio summary panel"""
        with dpg.child_window(width=450, height=550, border=True):
            dpg.add_text("PORTFOLIO SUMMARY", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Portfolio metrics
            total_value, total_pnl, day_pnl = self.calculate_portfolio_metrics()

            dpg.add_text(f"TOTAL VALUE: ${total_value:,.2f}", color=self.BLOOMBERG_WHITE,
                         tag=self.get_tag("total_value_text"))

            pnl_color = self.BLOOMBERG_GREEN if total_pnl >= 0 else self.BLOOMBERG_RED
            dpg.add_text(f"UNREALIZED P&L: ${total_pnl:+,.2f}", color=pnl_color,
                         tag=self.get_tag("total_pnl_text"))

            day_color = self.BLOOMBERG_GREEN if day_pnl >= 0 else self.BLOOMBERG_RED
            dpg.add_text(f"DAY P&L: ${day_pnl:+,.2f}", color=day_color,
                         tag=self.get_tag("day_pnl_text"))

            dpg.add_text(f"POSITIONS: {len(self.watchlist)}", color=self.BLOOMBERG_YELLOW,
                         tag=self.get_tag("positions_count"))

            dpg.add_separator()

            # Top holdings
            dpg.add_text("TOP HOLDINGS", color=self.BLOOMBERG_YELLOW)

            with dpg.group(tag=self.get_tag("top_holdings_group")):
                self.update_top_holdings(total_value)

            dpg.add_separator()

            # Market alerts
            dpg.add_text("MARKET ALERTS", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("System: Ready", color=self.BLOOMBERG_GREEN)
            if HAS_YFINANCE:
                dpg.add_text("Data: Live Feed", color=self.BLOOMBERG_GREEN)
            else:
                dpg.add_text("Data: Simulated", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Status: Active", color=self.BLOOMBERG_WHITE)

            dpg.add_separator()

            # Quick actions
            dpg.add_text("QUICK ACTIONS", color=self.BLOOMBERG_YELLOW)
            dpg.add_button(label="EXPORT PORTFOLIO", width=-1, callback=self.export_portfolio)
            dpg.add_spacer(height=5)
            dpg.add_button(label="RESET WATCHLIST", width=-1, callback=self.reset_watchlist)
            dpg.add_spacer(height=5)
            dpg.add_button(label="PORTFOLIO REPORT", width=-1, callback=self.portfolio_report)

    def update_top_holdings(self, total_value):
        """Update top holdings display"""
        holdings_tag = self.get_tag("top_holdings_group")
        if dpg.does_item_exist(holdings_tag):
            # Clear existing holdings
            children = dpg.get_item_children(holdings_tag, slot=1)
            if children:
                for child in children:
                    dpg.delete_item(child)

            # Add new holdings
            sorted_holdings = sorted(self.watchlist.items(),
                                     key=lambda x: x[1]["total_value"], reverse=True)

            for ticker, data in sorted_holdings[:6]:
                percentage = (data['total_value'] / total_value * 100) if total_value > 0 else 0
                dpg.add_text(f"{ticker}: ${data['total_value']:,.0f} ({percentage:.1f}%)",
                             color=self.BLOOMBERG_WHITE, parent=holdings_tag)

    def create_status_bar(self):
        """Create status bar"""
        with dpg.group(horizontal=True):
            dpg.add_text("WATCHLIST STATUS:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("ACTIVE", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("POSITIONS:", color=self.BLOOMBERG_GRAY)
            dpg.add_text(f"{len(self.watchlist)}", color=self.BLOOMBERG_YELLOW,
                         tag=self.get_tag("status_positions"))
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("AUTO-UPDATE:", color=self.BLOOMBERG_GRAY)
            status_text = "ON" if self.auto_update else "OFF"
            status_color = self.BLOOMBERG_GREEN if self.auto_update else self.BLOOMBERG_RED
            dpg.add_text(status_text, color=status_color, tag=self.get_tag("auto_status"))
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("SELECTED:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("None", color=self.BLOOMBERG_GRAY, tag=self.get_tag("selected_ticker_text"))

    def populate_watchlist_table(self):
        """Populate the watchlist table with current data"""
        try:
            table_tag = self.get_tag("watchlist_table")
            # Clear existing table data
            if dpg.does_item_exist(table_tag):
                children = dpg.get_item_children(table_tag, slot=1)
                if children:
                    for child in children:
                        dpg.delete_item(child)

            # Add rows sorted by ticker
            for ticker in sorted(self.watchlist.keys()):
                data = self.watchlist[ticker]

                with dpg.table_row(parent=table_tag):
                    # Symbol - make it selectable
                    symbol_selectable = dpg.add_selectable(label=ticker, span_columns=False)
                    dpg.set_item_callback(symbol_selectable, lambda s, a, u=ticker: self.select_ticker(u))

                    # Quantity
                    dpg.add_text(f"{data['quantity']:,}", color=self.BLOOMBERG_WHITE)

                    # Average price
                    dpg.add_text(f"${data['avg_price']:.2f}", color=self.BLOOMBERG_GRAY)

                    # Current price
                    dpg.add_text(f"${data['current_price']:.2f}", color=self.BLOOMBERG_WHITE)

                    # 1D Change
                    change_color = self.BLOOMBERG_GREEN if data['change_1d'] >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"${data['change_1d']:+.2f}", color=change_color)

                    # 1D Change %
                    pct_color = self.BLOOMBERG_GREEN if data['change_pct_1d'] >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"{data['change_pct_1d']:+.2f}%", color=pct_color)

                    # 7D Change %
                    pct_7d_color = self.BLOOMBERG_GREEN if data['change_pct_7d'] >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"{data['change_pct_7d']:+.2f}%", color=pct_7d_color)

                    # 30D Change %
                    pct_30d_color = self.BLOOMBERG_GREEN if data['change_pct_30d'] >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"{data['change_pct_30d']:+.2f}%", color=pct_30d_color)

                    # Total value
                    dpg.add_text(f"${data['total_value']:,.2f}", color=self.BLOOMBERG_WHITE)

                    # P&L
                    pnl_color = self.BLOOMBERG_GREEN if data['unrealized_pnl'] >= 0 else self.BLOOMBERG_RED
                    dpg.add_text(f"${data['unrealized_pnl']:+,.2f}", color=pnl_color)

                    # Alert
                    alert_text = f"${data['alert_price']:.2f}" if data['alert_price'] else ""
                    dpg.add_text(alert_text, color=self.BLOOMBERG_YELLOW)

        except Exception as e:
            print(f"Error populating watchlist table: {e}")

    def calculate_portfolio_metrics(self):
        """Calculate portfolio summary metrics"""
        try:
            total_value = sum(data['total_value'] for data in self.watchlist.values())
            total_pnl = sum(data['unrealized_pnl'] for data in self.watchlist.values())
            day_pnl = sum(data['change_1d'] * data['quantity'] for data in self.watchlist.values())

            return total_value, total_pnl, day_pnl
        except:
            return 0.0, 0.0, 0.0

    # ============================================================================
    # CALLBACK METHODS
    # ============================================================================

    def select_ticker(self, ticker):
        """Select a ticker from the table"""
        self.selected_ticker = ticker
        selected_tag = self.get_tag("selected_ticker_text")
        if dpg.does_item_exist(selected_tag):
            dpg.set_value(selected_tag, ticker)
        print(f"Selected ticker: {ticker}")

    def add_to_watchlist_callback(self, sender, app_data):
        """Callback for add to watchlist button"""
        try:
            ticker = dpg.get_value(self.get_tag("add_symbol_input")).strip().upper()
            quantity_str = dpg.get_value(self.get_tag("add_qty_input")).strip()
            avg_price_str = dpg.get_value(self.get_tag("add_price_input")).strip()
            alert_str = dpg.get_value(self.get_tag("add_alert_input")).strip()

            if not ticker:
                print("Please enter a ticker symbol.")
                return

            if ticker in self.watchlist:
                print(f"Ticker {ticker} already in watchlist.")
                return

            try:
                quantity = float(quantity_str) if quantity_str else 1
                avg_price = float(avg_price_str) if avg_price_str else 100.0
                alert_price = float(alert_str) if alert_str else None
            except ValueError:
                print("Invalid numeric values.")
                return

            # Add to watchlist
            self.add_ticker_to_watchlist(ticker, quantity, avg_price, alert_price)

            # Clear input fields
            dpg.set_value(self.get_tag("add_symbol_input"), "")
            dpg.set_value(self.get_tag("add_qty_input"), "")
            dpg.set_value(self.get_tag("add_price_input"), "")
            dpg.set_value(self.get_tag("add_alert_input"), "")

            print(f"Added {ticker} to watchlist.")

        except Exception as e:
            print(f"Error adding to watchlist: {e}")

    def remove_ticker_callback(self, sender, app_data):
        """Callback for remove ticker button"""
        if not self.selected_ticker:
            print("Please select a ticker from the table.")
            return

        self.remove_ticker_from_watchlist(self.selected_ticker)

    def refresh_all_callback(self, sender, app_data):
        """Callback for refresh all button"""
        print("Refreshing all prices...")
        if HAS_YFINANCE:
            threading.Thread(target=self.refresh_all_prices_sync, daemon=True).start()
        else:
            self.update_watchlist_data()
            self.update_display()

    def toggle_auto_update(self, sender, app_data):
        """Toggle auto-update on/off"""
        self.auto_update = not self.auto_update

        auto_btn_tag = self.get_tag("auto_toggle_btn")
        if dpg.does_item_exist(auto_btn_tag):
            dpg.set_item_label(auto_btn_tag, "AUTO ON" if self.auto_update else "AUTO OFF")

        auto_status_tag = self.get_tag("auto_status")
        if dpg.does_item_exist(auto_status_tag):
            status_text = "ON" if self.auto_update else "OFF"
            dpg.set_value(auto_status_tag, status_text)

        if self.auto_update and not self.refresh_running:
            self.start_auto_update()
        elif not self.auto_update:
            self.refresh_running = False

        print(f"Auto-update: {'ON' if self.auto_update else 'OFF'}")

    def show_chart_callback(self, sender, app_data):
        """Callback for show chart button"""
        if not self.selected_ticker:
            print("Please select a ticker from the table.")
            return

        print(f"Showing chart for {self.selected_ticker}")
        # Chart functionality would be implemented here using yfinance

    def show_info_callback(self, sender, app_data):
        """Callback for show info button"""
        if not self.selected_ticker:
            print("Please select a ticker from the table.")
            return

        print(f"Showing info for {self.selected_ticker}")
        # Info functionality would be implemented here using yfinance

    def export_portfolio(self, sender, app_data):
        """Export portfolio to file"""
        print("Exporting portfolio...")
        try:
            export_data = {
                "timestamp": datetime.datetime.now().isoformat(),
                "watchlist": self.watchlist,
                "portfolio_metrics": {
                    "total_value": sum(data['total_value'] for data in self.watchlist.values()),
                    "total_pnl": sum(data['unrealized_pnl'] for data in self.watchlist.values()),
                    "positions_count": len(self.watchlist)
                }
            }

            filename = f"portfolio_export_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
            with open(filename, 'w') as f:
                json.dump(export_data, f, indent=4)

            print(f"Portfolio exported to {filename}")
        except Exception as e:
            print(f"Error exporting portfolio: {e}")

    def reset_watchlist(self, sender, app_data):
        """Reset watchlist to default"""
        self.watchlist = {}
        self.settings = {}
        self.initialize_sample_watchlist()
        self.update_display()
        print("Watchlist reset to default.")

    def portfolio_report(self, sender, app_data):
        """Generate portfolio report"""
        print("Generating portfolio report...")
        total_value, total_pnl, day_pnl = self.calculate_portfolio_metrics()

        print(f"\n=== PORTFOLIO REPORT ===")
        print(f"Generated: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"Total Positions: {len(self.watchlist)}")
        print(f"Total Value: ${total_value:,.2f}")
        print(f"Unrealized P&L: ${total_pnl:+,.2f}")
        print(f"Day P&L: ${day_pnl:+,.2f}")
        print(f"========================\n")

    # ============================================================================
    # CORE WATCHLIST METHODS
    # ============================================================================

    def add_ticker_to_watchlist(self, ticker, quantity, avg_price, alert_price):
        """Add a ticker to the watchlist"""
        current_price = avg_price if avg_price > 0 else 100.0

        self.watchlist[ticker] = {
            "quantity": quantity,
            "avg_price": avg_price,
            "alert_price": alert_price,
            "current_price": current_price,
            "change_1d": random.uniform(-5, 5),
            "change_pct_1d": random.uniform(-3, 3),
            "change_pct_7d": random.uniform(-8, 8),
            "change_pct_30d": random.uniform(-20, 20),
            "last_updated": datetime.datetime.now().strftime("%H:%M:%S"),
            "total_value": current_price * quantity,
            "unrealized_pnl": (current_price - avg_price) * quantity if avg_price > 0 else 0
        }

        self.save_watchlist_to_settings()
        self.update_display()

        # Fetch real price if yfinance is available
        if HAS_YFINANCE:
            threading.Thread(target=self.fetch_single_price, args=(ticker,), daemon=True).start()

    def remove_ticker_from_watchlist(self, ticker):
        """Remove a ticker from the watchlist"""
        if ticker in self.watchlist:
            del self.watchlist[ticker]

        if ticker in self.settings.get("portfolios", {}).get("watchlist", {}):
            del self.settings["portfolios"]["watchlist"][ticker]

        self.save_settings()
        self.update_display()
        self.selected_ticker = None

        selected_tag = self.get_tag("selected_ticker_text")
        if dpg.does_item_exist(selected_tag):
            dpg.set_value(selected_tag, "None")

        print(f"Removed {ticker} from watchlist.")

    def fetch_single_price(self, ticker):
        """Fetch price for a single ticker"""
        if not HAS_YFINANCE:
            return

        try:
            stock = yf.Ticker(ticker)
            data = stock.history(period="30d", interval="1d")

            if data.empty:
                raise ValueError("No data available")

            last_price = data["Close"].iloc[-1]

            # Calculate changes and returns
            if len(data) >= 2:
                prev_price = data["Close"].iloc[-2]
                change = last_price - prev_price
                change_pct = (change / prev_price * 100) if prev_price != 0 else 0
            else:
                change = 0
                change_pct = 0

            # Update watchlist data
            if ticker in self.watchlist:
                avg_price = self.watchlist[ticker]["avg_price"]
                quantity = self.watchlist[ticker]["quantity"]

                self.watchlist[ticker].update({
                    "current_price": round(last_price, 2),
                    "change_1d": round(change, 2),
                    "change_pct_1d": round(change_pct, 2),
                    "last_updated": datetime.datetime.now().strftime("%H:%M:%S"),
                    "total_value": round(last_price * quantity, 2),
                    "unrealized_pnl": round((last_price - avg_price) * quantity, 2) if avg_price > 0 else 0
                })

                self.update_display()

        except Exception as e:
            print(f"Error fetching price for {ticker}: {e}")

    def refresh_all_prices_sync(self):
        """Refresh all prices synchronously (runs in background thread)"""
        if not self.watchlist:
            return

        for ticker in list(self.watchlist.keys()):
            self.fetch_single_price(ticker)
            time.sleep(0.1)  # Small delay between requests

        self.save_watchlist_to_settings()

    def update_watchlist_data(self):
        """Update watchlist data with simulated changes"""
        try:
            for ticker in self.watchlist:
                data = self.watchlist[ticker]

                # Small price movement
                change_factor = 1 + random.uniform(-0.015, 0.015)  # ±1.5% max
                new_price = data['current_price'] * change_factor
                new_change_1d = new_price - data['current_price']
                new_change_pct_1d = (new_change_1d / data['current_price']) * 100

                # Update data
                self.watchlist[ticker].update({
                    'current_price': round(new_price, 2),
                    'change_1d': round(new_change_1d, 2),
                    'change_pct_1d': round(new_change_pct_1d, 2),
                    'change_pct_7d': round(data['change_pct_7d'] + random.uniform(-0.5, 0.5), 2),
                    'change_pct_30d': round(data['change_pct_30d'] + random.uniform(-0.2, 0.2), 2),
                    'last_updated': datetime.datetime.now().strftime("%H:%M:%S"),
                    'total_value': round(new_price * data['quantity'], 2),
                    'unrealized_pnl': round((new_price - data['avg_price']) * data['quantity'], 2)
                })

            # Update timestamps
            current_time = datetime.datetime.now().strftime('%H:%M:%S')
            last_update_tag = self.get_tag("last_update_time")
            if dpg.does_item_exist(last_update_tag):
                dpg.set_value(last_update_tag, current_time)

            watchlist_time_tag = self.get_tag("watchlist_time")
            if dpg.does_item_exist(watchlist_time_tag):
                dpg.set_value(watchlist_time_tag, datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

            print(f"Watchlist updated at {current_time}")

        except Exception as e:
            print(f"Error updating watchlist data: {e}")

    def update_display(self):
        """Update all display elements"""
        try:
            # Update table
            self.populate_watchlist_table()

            # Update portfolio metrics
            total_value, total_pnl, day_pnl = self.calculate_portfolio_metrics()

            total_value_tag = self.get_tag("total_value_text")
            if dpg.does_item_exist(total_value_tag):
                dpg.set_value(total_value_tag, f"TOTAL VALUE: ${total_value:,.2f}")

            total_pnl_tag = self.get_tag("total_pnl_text")
            if dpg.does_item_exist(total_pnl_tag):
                pnl_color = self.BLOOMBERG_GREEN if total_pnl >= 0 else self.BLOOMBERG_RED
                dpg.configure_item(total_pnl_tag, color=pnl_color)
                dpg.set_value(total_pnl_tag, f"UNREALIZED P&L: ${total_pnl:+,.2f}")

            day_pnl_tag = self.get_tag("day_pnl_text")
            if dpg.does_item_exist(day_pnl_tag):
                day_color = self.BLOOMBERG_GREEN if day_pnl >= 0 else self.BLOOMBERG_RED
                dpg.configure_item(day_pnl_tag, color=day_color)
                dpg.set_value(day_pnl_tag, f"DAY P&L: ${day_pnl:+,.2f}")

            positions_tag = self.get_tag("positions_count")
            if dpg.does_item_exist(positions_tag):
                dpg.set_value(positions_tag, f"POSITIONS: {len(self.watchlist)}")

            status_positions_tag = self.get_tag("status_positions")
            if dpg.does_item_exist(status_positions_tag):
                dpg.set_value(status_positions_tag, f"{len(self.watchlist)}")

            # Update top holdings
            self.update_top_holdings(total_value)

        except Exception as e:
            print(f"Error updating display: {e}")

    def start_auto_update(self):
        """Start auto-update thread"""

        def update_loop():
            while self.auto_update and self.refresh_running:
                try:
                    time.sleep(4)  # Update every 4 seconds
                    if self.auto_update and self.refresh_running:
                        if HAS_YFINANCE:
                            self.refresh_all_prices_sync()
                        else:
                            self.update_watchlist_data()
                        self.update_display()
                except Exception as e:
                    print(f"Error in auto-update loop: {e}")
                    break

        if self.auto_update and not self.refresh_running:
            self.refresh_running = True
            update_thread = threading.Thread(target=update_loop, daemon=True)
            update_thread.start()

    def save_watchlist_to_settings(self):
        """Save current watchlist state to settings"""
        if "portfolios" not in self.settings:
            self.settings["portfolios"] = {}

        if "watchlist" not in self.settings["portfolios"]:
            self.settings["portfolios"]["watchlist"] = {}

        for ticker, data in self.watchlist.items():
            self.settings["portfolios"]["watchlist"][ticker] = {
                "quantity": data.get("quantity", 0),
                "avg_price": data.get("avg_price", 0),
                "alert_price": data.get("alert_price"),
                "last_added": datetime.datetime.now().strftime("%Y-%m-%d")
            }

        self.save_settings()

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle resize events"""
        # Fixed layout for stability
        pass

    def cleanup(self):
        """Clean up resources"""
        try:
            print("Cleaning up watchlist tab...")
            self.auto_update = False
            self.refresh_running = False

            # Save current state before cleanup
            self.save_watchlist_to_settings()

            # Clean up UI elements with unique tags
            self.cleanup_existing_items()

            # Clear data
            self.watchlist = {}
            self.settings = {}
            self.selected_ticker = None

            print("Watchlist tab cleanup complete")
        except Exception as e:
            print(f"Error in cleanup: {e}")