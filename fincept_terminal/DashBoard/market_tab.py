# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import threading
import time
import datetime
import random

# Simplified assets for faster loading and testing
MARKET_ASSETS = {
    "Stock Indices": {
        "S&P 500": 5232.35,
        "Nasdaq 100": 16412.87,
        "Dow Jones": 38546.12,
        "Russell 2000": 2087.45,
        "VIX": 18.23,
        "FTSE 100": 7623.98,
        "DAX 40": 18245.67,
        "CAC 40": 7890.12,
        "Nikkei 225": 35897.45,
        "Hang Seng": 18654.23
    },
    "Forex": {
        "EUR/USD": 1.0840,
        "GBP/USD": 1.2567,
        "USD/JPY": 151.45,
        "USD/CHF": 0.8923,
        "USD/CAD": 1.3578,
        "AUD/USD": 0.6634,
        "NZD/USD": 0.6089,
        "EUR/GBP": 0.8623,
        "EUR/JPY": 164.23,
        "GBP/JPY": 190.45
    },
    "Commodities": {
        "Gold": 2312.80,
        "Silver": 28.45,
        "Crude Oil (WTI)": 78.35,
        "Brent Crude": 82.10,
        "Natural Gas": 3.45,
        "Copper": 4.23,
        "Platinum": 987.50,
        "Palladium": 1045.20,
        "Wheat": 567.80,
        "Corn": 478.90
    },
    "Bonds": {
        "US 10Y Treasury": 4.35,
        "US 30Y Treasury": 4.52,
        "US 2Y Treasury": 4.89,
        "Germany 10Y": 2.45,
        "UK 10Y": 4.12,
        "Japan 10Y": 0.78,
        "France 10Y": 2.98,
        "Italy 10Y": 4.23,
        "Spain 10Y": 3.67,
        "Canada 10Y": 3.89
    },
    "ETFs": {
        "SPY (S&P 500)": 523.45,
        "QQQ (Nasdaq)": 412.67,
        "DIA (Dow)": 385.23,
        "EEM (Emerging)": 41.78,
        "GLD (Gold)": 231.45,
        "XLK (Tech)": 198.90,
        "XLE (Energy)": 89.34,
        "XLF (Finance)": 42.56,
        "XLV (Health)": 156.78,
        "VNQ (REIT)": 78.90
    },
    "Cryptocurrencies": {
        "Bitcoin": 67890.45,
        "Ethereum": 3789.23,
        "Binance Coin": 567.89,
        "Solana": 234.56,
        "Dogecoin": 0.1234,
        "Polygon": 1.23,
        "Chainlink": 23.45,
        "Cardano": 0.67,
        "Polkadot": 12.34,
        "Avalanche": 45.67
    }
}


class MarketTab(BaseTab):
    """Bloomberg Terminal style Market tab - Simplified and Working"""

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

        # Initialize data with simulated changes
        self.initialize_market_data()
        self.update_counter = 0
        self.auto_update = True

    def get_label(self):
        return "📈 Markets"

    def initialize_market_data(self):
        """Initialize market data with simulated price changes"""
        self.market_data = {}

        for category, assets in MARKET_ASSETS.items():
            self.market_data[category] = {}
            for asset_name, base_price in assets.items():
                # Generate realistic market data
                current_price = base_price * (1 + random.uniform(-0.05, 0.05))
                change_1d = current_price * random.uniform(-0.03, 0.03)
                change_percent_1d = (change_1d / current_price) * 100
                change_percent_7d = random.uniform(-5, 5)
                change_percent_30d = random.uniform(-15, 15)

                self.market_data[category][asset_name] = {
                    "price": round(current_price, 2),
                    "change_1d": round(change_1d, 2),
                    "change_percent_1d": round(change_percent_1d, 2),
                    "change_percent_7d": round(change_percent_7d, 2),
                    "change_percent_30d": round(change_percent_30d, 2)
                }

    def create_content(self):
        """Create Bloomberg-style market terminal layout"""
        try:
            # Top bar with branding
            with dpg.group(horizontal=True):
                dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("MARKET TERMINAL", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_input_text(label="", default_value="Search Symbol", width=200)
                dpg.add_button(label="SEARCH", width=80, callback=self.search_callback)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), tag="market_time_display")

            dpg.add_separator()

            # Control panel
            with dpg.group(horizontal=True):
                dpg.add_button(label="REFRESH", callback=self.refresh_callback, width=80)
                dpg.add_button(label="AUTO ON", callback=self.toggle_auto_update,
                               tag="auto_toggle_btn", width=80)
                dpg.add_combo(["1 min", "5 min", "10 min", "30 min"],
                              default_value="5 min", width=80, tag="refresh_interval")
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("LAST UPDATE:", color=self.BLOOMBERG_GRAY)
                dpg.add_text(datetime.datetime.now().strftime('%H:%M:%S'),
                             tag="last_update_time", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("●", color=self.BLOOMBERG_GREEN)
                dpg.add_text("LIVE", color=self.BLOOMBERG_GREEN)

            dpg.add_separator()

            # Market grid - 3x2 layout
            self.create_market_grid()

            # Status bar
            dpg.add_separator()
            self.create_status_bar()

            # Start auto-update if enabled
            if self.auto_update:
                self.start_auto_update()

        except Exception as e:
            print(f"Error creating market content: {e}")
            # Fallback content
            dpg.add_text("MARKET TERMINAL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Error loading market data. Please refresh.")
            dpg.add_button(label="Retry", callback=self.retry_callback)

    def create_market_grid(self):
        """Create 3x2 market grid"""
        categories = list(self.market_data.keys())

        # First row - 3 categories
        with dpg.group(horizontal=True):
            for i in range(3):
                if i < len(categories):
                    self.create_market_panel(categories[i], 500, 300)

        dpg.add_spacer(height=10)

        # Second row - 3 categories
        with dpg.group(horizontal=True):
            for i in range(3, 6):
                if i < len(categories):
                    self.create_market_panel(categories[i], 500, 300)

    def create_market_panel(self, category, width, height):
        """Create individual market panel"""
        try:
            tag_suffix = category.replace(' ', '_').replace('(', '').replace(')', '').lower()

            with dpg.child_window(width=width, height=height, border=True):
                # Panel header
                dpg.add_text(f"{category.upper()}", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                # Data table
                with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                               scrollY=True, scrollX=True, height=height - 60):
                    dpg.add_table_column(label="Asset", width_fixed=True, init_width_or_weight=180)
                    dpg.add_table_column(label="Last", width_fixed=True, init_width_or_weight=80)
                    dpg.add_table_column(label="Chg", width_fixed=True, init_width_or_weight=60)
                    dpg.add_table_column(label="1D%", width_fixed=True, init_width_or_weight=60)
                    dpg.add_table_column(label="7D%", width_fixed=True, init_width_or_weight=60)
                    dpg.add_table_column(label="30D%", width_fixed=True, init_width_or_weight=60)

                    # Add data rows
                    assets = self.market_data.get(category, {})
                    for asset_name, data in list(assets.items())[:10]:  # Show top 10
                        with dpg.table_row():
                            # Asset name
                            asset_display = asset_name[:25] + "..." if len(asset_name) > 25 else asset_name
                            dpg.add_text(asset_display, color=self.BLOOMBERG_WHITE)

                            # Price
                            if data["price"] < 1:
                                price_str = f"{data['price']:.4f}"
                            elif data["price"] < 100:
                                price_str = f"{data['price']:.2f}"
                            else:
                                price_str = f"{data['price']:,.0f}"
                            dpg.add_text(price_str, color=self.BLOOMBERG_WHITE)

                            # 1D Change
                            change_color = self.BLOOMBERG_GREEN if data["change_1d"] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_1d']:+.2f}", color=change_color)

                            # 1D %
                            percent_color = self.BLOOMBERG_GREEN if data[
                                                                        "change_percent_1d"] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_percent_1d']:+.2f}%", color=percent_color)

                            # 7D %
                            percent_7d_color = self.BLOOMBERG_GREEN if data[
                                                                           "change_percent_7d"] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_percent_7d']:+.2f}%", color=percent_7d_color)

                            # 30D %
                            percent_30d_color = self.BLOOMBERG_GREEN if data[
                                                                            "change_percent_30d"] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_percent_30d']:+.2f}%", color=percent_30d_color)

        except Exception as e:
            print(f"Error creating market panel {category}: {e}")

    def create_status_bar(self):
        """Create status bar"""
        with dpg.group(horizontal=True):
            dpg.add_text("MARKET STATUS:", color=self.BLOOMBERG_GRAY)

            # Market hours
            current_hour = datetime.datetime.now().hour
            if 9 <= current_hour < 16:
                dpg.add_text("OPEN", color=self.BLOOMBERG_GREEN)
            else:
                dpg.add_text("CLOSED", color=self.BLOOMBERG_RED)

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("DATA FEED:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("SIMULATED", color=self.BLOOMBERG_ORANGE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("ASSETS:", color=self.BLOOMBERG_GRAY)

            total_assets = sum(len(assets) for assets in self.market_data.values())
            dpg.add_text(f"{total_assets}", color=self.BLOOMBERG_YELLOW)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("AUTO-UPDATE:", color=self.BLOOMBERG_GRAY)
            status_text = "ON" if self.auto_update else "OFF"
            status_color = self.BLOOMBERG_GREEN if self.auto_update else self.BLOOMBERG_RED
            dpg.add_text(status_text, color=status_color, tag="auto_status_text")

    def update_market_data(self):
        """Update market data with simulated changes"""
        try:
            self.update_counter += 1

            # Update each asset with small random changes
            for category in self.market_data:
                for asset_name in self.market_data[category]:
                    data = self.market_data[category][asset_name]

                    # Small price movement
                    change_factor = 1 + random.uniform(-0.01, 0.01)  # ±1% max change
                    new_price = data["price"] * change_factor
                    new_change_1d = new_price - data["price"]
                    new_change_percent_1d = (new_change_1d / data["price"]) * 100

                    # Update data
                    self.market_data[category][asset_name].update({
                        "price": round(new_price, 2 if new_price >= 1 else 4),
                        "change_1d": round(new_change_1d, 2),
                        "change_percent_1d": round(new_change_percent_1d, 2),
                        "change_percent_7d": round(data["change_percent_7d"] + random.uniform(-0.5, 0.5), 2),
                        "change_percent_30d": round(data["change_percent_30d"] + random.uniform(-0.2, 0.2), 2)
                    })

            # Update timestamp
            current_time = datetime.datetime.now().strftime('%H:%M:%S')
            if dpg.does_item_exist("last_update_time"):
                dpg.set_value("last_update_time", current_time)
            if dpg.does_item_exist("market_time_display"):
                dpg.set_value("market_time_display", datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

            print(f"✅ Market data updated at {current_time}")

        except Exception as e:
            print(f"Error updating market data: {e}")

    def start_auto_update(self):
        """Start auto-update thread"""

        def update_loop():
            while self.auto_update:
                try:
                    time.sleep(5)  # Update every 5 seconds for demo
                    if self.auto_update:
                        self.update_market_data()
                except Exception as e:
                    print(f"Error in auto-update loop: {e}")
                    break

        if self.auto_update:
            update_thread = threading.Thread(target=update_loop, daemon=True)
            update_thread.start()

    # Callback methods
    def search_callback(self):
        """Search callback"""
        print("Search functionality - feature coming soon")

    def refresh_callback(self):
        """Manual refresh callback"""
        try:
            print("🔄 Manual refresh triggered")
            self.initialize_market_data()  # Reinitialize with new random data
            self.update_market_data()
        except Exception as e:
            print(f"Error in refresh: {e}")

    def toggle_auto_update(self):
        """Toggle auto-update"""
        try:
            self.auto_update = not self.auto_update

            # Update button text
            button_text = "AUTO ON" if self.auto_update else "AUTO OFF"
            dpg.set_item_label("auto_toggle_btn", button_text)

            # Update status text
            status_text = "ON" if self.auto_update else "OFF"
            status_color = self.BLOOMBERG_GREEN if self.auto_update else self.BLOOMBERG_RED
            if dpg.does_item_exist("auto_status_text"):
                dpg.set_value("auto_status_text", status_text)

            # Start/stop auto-update
            if self.auto_update:
                self.start_auto_update()

            print(f"Auto-update {'enabled' if self.auto_update else 'disabled'}")

        except Exception as e:
            print(f"Error toggling auto-update: {e}")

    def retry_callback(self):
        """Retry loading data"""
        try:
            self.initialize_market_data()
            print("Market data reinitialized")
        except Exception as e:
            print(f"Error in retry: {e}")

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle resize events"""
        # Market panels have fixed sizes for stability
        pass

    def cleanup(self):
        """Clean up resources"""
        try:
            print("🧹 Cleaning up market tab...")
            self.auto_update = False
            self.market_data = {}
            print("✅ Market tab cleanup complete")
        except Exception as e:
            print(f"Error in cleanup: {e}")