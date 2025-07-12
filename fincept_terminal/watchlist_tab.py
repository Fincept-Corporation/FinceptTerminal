import dearpygui.dearpygui as dpg
from base_tab import BaseTab
import json
import os
from datetime import datetime
import yfinance as yf
import threading
import time

# Import settings path utility - adjust import as needed
try:
    from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import get_settings_path

    SETTINGS_FILE = get_settings_path()
except ImportError:
    # Fallback if module not available
    SETTINGS_FILE = "watchlist_settings.json"


class WatchlistTab(BaseTab):
    """Stock Watchlist tab with real-time price tracking"""

    def __init__(self, app):
        super().__init__(app)
        self.watchlist = {}
        self.settings = {}
        self.selected_ticker = None
        self.refresh_thread = None
        self.refresh_running = False
        self.load_settings()

    def get_label(self):
        return "Watchlist"

    def create_content(self):
        """Create watchlist content with input fields, actions, and data table"""
        # Create main container
        with dpg.child_window(
                tag="watchlist_main_container",
                width=-1,
                height=-1,
                horizontal_scrollbar=False,
                border=True
        ):
            self.add_section_header("Stock Watchlist")

            # Header row - input fields
            self.create_input_section()
            dpg.add_spacer(height=15)

            # Actions row - buttons
            self.create_actions_section()
            dpg.add_spacer(height=15)

            # Watchlist table
            self.create_watchlist_table()

            # Status and controls
            dpg.add_spacer(height=10)
            self.create_status_section()

        # Initialize watchlist data and start refresh
        self.initialize_watchlist()

    def create_input_section(self):
        """Create input fields for adding stocks"""
        dpg.add_text("Add Stock to Watchlist", color=[100, 255, 100])
        dpg.add_spacer(height=5)

        with dpg.group(horizontal=True):
            dpg.add_input_text(
                tag="ticker_input",
                hint="Enter Stock Symbol (e.g., AAPL)",
                width=200,
                uppercase=True,
                callback=self.on_ticker_input_change
            )
            dpg.add_input_text(
                tag="quantity_input",
                hint="Quantity",
                width=100,
                decimal=True
            )
            dpg.add_input_text(
                tag="avg_price_input",
                hint="Avg Price (optional)",
                width=150,
                decimal=True
            )
            dpg.add_input_text(
                tag="alert_input",
                hint="Alert Price (optional)",
                width=150,
                decimal=True
            )

    def create_actions_section(self):
        """Create action buttons"""
        dpg.add_text("Actions", color=[100, 255, 100])
        dpg.add_spacer(height=5)

        with dpg.group(horizontal=True):
            dpg.add_button(
                label="Add to Watchlist",
                callback=self.add_to_watchlist_callback,
                width=130
            )
            dpg.add_button(
                label="Show Chart",
                callback=self.show_chart_callback,
                width=110
            )
            dpg.add_button(
                label="Show Info",
                callback=self.show_info_callback,
                width=110
            )
            dpg.add_button(
                label="Remove Ticker",
                callback=self.remove_ticker_callback,
                width=120
            )
            dpg.add_button(
                label="Refresh All",
                callback=self.refresh_all_callback,
                width=100
            )

    def create_watchlist_table(self):
        """Create the main watchlist table"""
        dpg.add_text("Watchlist Data", color=[100, 255, 100])
        dpg.add_spacer(height=5)

        with dpg.table(
                tag="watchlist_table",
                resizable=True,
                policy=dpg.mvTable_SizingStretchProp,
                borders_innerH=True,
                borders_outerH=True,
                borders_innerV=True,
                borders_outerV=True,
                height=400,
                callback=self.on_table_selection_change
        ):
            # Add columns
            dpg.add_table_column(label="Ticker", width_fixed=True, init_width_or_weight=80)
            dpg.add_table_column(label="Quantity", width_fixed=True, init_width_or_weight=80)
            dpg.add_table_column(label="Avg Price", width_fixed=True, init_width_or_weight=90)
            dpg.add_table_column(label="Current Price", width_fixed=True, init_width_or_weight=110)
            dpg.add_table_column(label="Change", width_fixed=True, init_width_or_weight=80)
            dpg.add_table_column(label="Change (%)", width_fixed=True, init_width_or_weight=90)
            dpg.add_table_column(label="Total Value", width_fixed=True, init_width_or_weight=100)
            dpg.add_table_column(label="Alert", width_fixed=True, init_width_or_weight=80)
            dpg.add_table_column(label="Last Updated", width_fixed=True, init_width_or_weight=100)
            dpg.add_table_column(label="1d Return", width_fixed=True, init_width_or_weight=80)
            dpg.add_table_column(label="7d Return", width_fixed=True, init_width_or_weight=80)
            dpg.add_table_column(label="30d Return", width_fixed=True, init_width_or_weight=90)

    def create_status_section(self):
        """Create status and control section"""
        with dpg.group(horizontal=True):
            dpg.add_text("Auto-refresh:", color=[200, 200, 200])
            dpg.add_checkbox(
                tag="auto_refresh_checkbox",
                default_value=True,
                callback=self.on_auto_refresh_toggle
            )
            dpg.add_spacer(width=20)
            dpg.add_text("Last updated:", color=[200, 200, 200])
            dpg.add_text("Never", tag="last_update_text", color=[150, 150, 150])
            dpg.add_spacer(width=20)
            dpg.add_text("Selected:", color=[200, 200, 200])
            dpg.add_text("None", tag="selected_ticker_text", color=[150, 150, 150])

    # ============================================================================
    # CALLBACK METHODS
    # ============================================================================

    def on_ticker_input_change(self):
        """Handle ticker input changes - convert to uppercase"""
        value = dpg.get_value("ticker_input")
        if value:
            dpg.set_value("ticker_input", value.upper())

    def on_table_selection_change(self):
        """Handle table row selection changes"""
        try:
            table = dpg.get_item_user_data("watchlist_table")
            if hasattr(table, 'cursor_row') and table.cursor_row is not None:
                # Get selected ticker from table data
                # This is a simplified approach - in practice you'd track row data
                pass
        except:
            pass

    def on_auto_refresh_toggle(self):
        """Handle auto-refresh toggle"""
        enabled = dpg.get_value("auto_refresh_checkbox")
        if enabled and not self.refresh_running:
            self.start_refresh_thread()
        elif not enabled and self.refresh_running:
            self.stop_refresh_thread()

    def add_to_watchlist_callback(self):
        """Callback for add to watchlist button"""
        ticker = dpg.get_value("ticker_input").strip().upper()
        quantity_str = dpg.get_value("quantity_input").strip()
        avg_price_str = dpg.get_value("avg_price_input").strip()
        alert_str = dpg.get_value("alert_input").strip()

        if not ticker:
            self.show_message("Please enter a ticker symbol.", "error")
            return

        try:
            quantity = float(quantity_str) if quantity_str else 0
        except ValueError:
            self.show_message("Invalid quantity value.", "error")
            return

        try:
            avg_price = float(avg_price_str) if avg_price_str else 0
        except ValueError:
            self.show_message("Invalid average price value.", "error")
            return

        try:
            alert_price = float(alert_str) if alert_str else None
        except ValueError:
            self.show_message("Invalid alert price value.", "error")
            return

        if ticker in self.watchlist:
            self.show_message(f"Ticker {ticker} already in watchlist.", "warning")
            return

        # Add to watchlist
        self.add_ticker_to_watchlist(ticker, quantity, avg_price, alert_price)

        # Clear input fields
        dpg.set_value("ticker_input", "")
        dpg.set_value("quantity_input", "")
        dpg.set_value("avg_price_input", "")
        dpg.set_value("alert_input", "")

        self.show_message(f"Added {ticker} to watchlist.", "success")

    def show_chart_callback(self):
        """Callback for show chart button"""
        ticker = self.get_selected_ticker()
        if not ticker:
            self.show_message("Please select a ticker from the table.", "warning")
            return

        # Run chart generation in background thread
        threading.Thread(target=self.generate_chart, args=(ticker,), daemon=True).start()

    def show_info_callback(self):
        """Callback for show info button"""
        ticker = self.get_selected_ticker()
        if not ticker:
            self.show_message("Please select a ticker from the table.", "warning")
            return

        # Run info fetching in background thread
        threading.Thread(target=self.fetch_stock_info, args=(ticker,), daemon=True).start()

    def remove_ticker_callback(self):
        """Callback for remove ticker button"""
        ticker = self.get_selected_ticker()
        if not ticker:
            self.show_message("Please select a ticker from the table.", "warning")
            return

        self.remove_ticker_from_watchlist(ticker)

    def refresh_all_callback(self):
        """Callback for refresh all button"""
        self.show_message("Refreshing all prices...", "info")
        threading.Thread(target=self.refresh_all_prices_sync, daemon=True).start()

    # ============================================================================
    # CORE WATCHLIST METHODS
    # ============================================================================

    def initialize_watchlist(self):
        """Initialize watchlist from settings"""
        if "watchlist" not in self.settings.get("portfolios", {}):
            if "portfolios" not in self.settings:
                self.settings["portfolios"] = {}
            self.settings["portfolios"]["watchlist"] = {}
            self.save_settings()

        self.watchlist = {}
        for ticker, data in self.settings["portfolios"]["watchlist"].items():
            self.watchlist[ticker] = {
                "quantity": data.get("quantity", 0),
                "avg_price": data.get("avg_price", 0),
                "alert": None,
                "current_price": "Fetching...",
                "change": "",
                "percent_change": "",
                "total_value": "",
                "last_updated": data.get("last_added", ""),
                "return_1d": "N/A",
                "return_7d": "N/A",
                "return_30d": "N/A",
            }

        self.update_table()

        # Start auto-refresh if enabled
        if dpg.get_value("auto_refresh_checkbox"):
            self.start_refresh_thread()

        # Initial price fetch
        if self.watchlist:
            threading.Thread(target=self.refresh_all_prices_sync, daemon=True).start()

    def add_ticker_to_watchlist(self, ticker, quantity, avg_price, alert_price):
        """Add a ticker to the watchlist"""
        today = datetime.now().strftime("%Y-%m-%d")

        self.watchlist[ticker] = {
            "quantity": quantity,
            "avg_price": avg_price,
            "alert": alert_price,
            "current_price": "Fetching...",
            "change": "",
            "percent_change": "",
            "total_value": "",
            "last_updated": today,
            "return_1d": "N/A",
            "return_7d": "N/A",
            "return_30d": "N/A",
        }

        # Save to settings
        self.settings["portfolios"]["watchlist"][ticker] = {
            "quantity": quantity,
            "avg_price": avg_price,
            "last_added": today
        }

        self.update_table()
        self.save_settings()

        # Fetch price for new ticker
        threading.Thread(target=self.fetch_single_price, args=(ticker,), daemon=True).start()

    def remove_ticker_from_watchlist(self, ticker):
        """Remove a ticker from the watchlist"""
        if ticker in self.watchlist:
            del self.watchlist[ticker]

        if ticker in self.settings.get("portfolios", {}).get("watchlist", {}):
            del self.settings["portfolios"]["watchlist"][ticker]

        self.update_table()
        self.save_settings()
        self.show_message(f"Removed {ticker} from watchlist.", "success")

    def fetch_single_price(self, ticker):
        """Fetch price for a single ticker"""
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

            # 30d return
            return_30d = None
            if len(data) >= 2:
                first_price = data["Close"].iloc[0]
                return_30d = ((last_price - first_price) / first_price * 100) if first_price != 0 else 0

            # 7d return
            return_7d = None
            if len(data) >= 8:
                price_7d = data["Close"].iloc[-8]
                return_7d = ((last_price - price_7d) / price_7d * 100) if price_7d != 0 else 0

            # 1d return
            return_1d = change_pct if len(data) >= 2 else None

            # Update watchlist data
            if ticker in self.watchlist:
                self.watchlist[ticker].update({
                    "current_price": f"${last_price:.2f}",
                    "change": f"${change:+.2f}",
                    "percent_change": f"{change_pct:+.2f}%",
                    "total_value": f"${last_price * self.watchlist[ticker]['quantity']:.2f}",
                    "last_updated": datetime.now().strftime("%H:%M:%S"),
                    "return_1d": f"{return_1d:+.2f}%" if return_1d is not None else "N/A",
                    "return_7d": f"{return_7d:+.2f}%" if return_7d is not None else "N/A",
                    "return_30d": f"{return_30d:+.2f}%" if return_30d is not None else "N/A",
                })

        except Exception as e:
            if ticker in self.watchlist:
                self.watchlist[ticker].update({
                    "current_price": "Error",
                    "change": "",
                    "percent_change": "",
                    "total_value": "",
                    "last_updated": "",
                    "return_1d": "N/A",
                    "return_7d": "N/A",
                    "return_30d": "N/A",
                })
            print(f"Error fetching price for {ticker}: {e}")

        # Update table on main thread
        dpg.set_value("last_update_text", datetime.now().strftime("%H:%M:%S"))
        self.update_table()

    def refresh_all_prices_sync(self):
        """Refresh all prices synchronously (runs in background thread)"""
        if not self.watchlist:
            return

        for ticker in list(self.watchlist.keys()):
            self.fetch_single_price(ticker)
            time.sleep(0.1)  # Small delay between requests

        self.save_watchlist_to_settings()

    def start_refresh_thread(self):
        """Start the auto-refresh thread"""
        if self.refresh_running:
            return

        self.refresh_running = True

        def refresh_loop():
            while self.refresh_running:
                if self.watchlist:
                    self.refresh_all_prices_sync()

                # Wait 10 minutes between refreshes
                for _ in range(600):  # 600 seconds = 10 minutes
                    if not self.refresh_running:
                        break
                    time.sleep(1)

        self.refresh_thread = threading.Thread(target=refresh_loop, daemon=True)
        self.refresh_thread.start()

    def stop_refresh_thread(self):
        """Stop the auto-refresh thread"""
        self.refresh_running = False
        if self.refresh_thread:
            self.refresh_thread = None

    def update_table(self):
        """Update the watchlist table display"""
        # Clear existing table data
        self.clear_table("watchlist_table")

        # Add rows sorted by ticker
        for ticker in sorted(self.watchlist.keys()):
            data = self.watchlist[ticker]

            with dpg.table_row(parent="watchlist_table"):
                dpg.add_text(ticker)
                dpg.add_text(str(data.get("quantity", "")))
                dpg.add_text(f"${data.get('avg_price', 0):.2f}")
                dpg.add_text(data.get("current_price", ""))
                dpg.add_text(data.get("change", ""))
                dpg.add_text(data.get("percent_change", ""))
                dpg.add_text(data.get("total_value", ""))
                dpg.add_text(str(data.get("alert", "")))
                dpg.add_text(data.get("last_updated", ""))
                dpg.add_text(data.get("return_1d", "N/A"))
                dpg.add_text(data.get("return_7d", "N/A"))
                dpg.add_text(data.get("return_30d", "N/A"))

    def get_selected_ticker(self):
        """Get the currently selected ticker from the table"""
        # In a real implementation, you'd track table selection
        # For now, we'll use the first ticker as a fallback
        if self.watchlist:
            return list(self.watchlist.keys())[0]
        return None

    def generate_chart(self, ticker):
        """Generate and display chart for a ticker using DearPyGUI charts"""
        try:
            stock = yf.Ticker(ticker)
            data = stock.history(period="1mo", interval="1d")

            if data.empty:
                self.show_message(f"No chart data available for {ticker}.", "error")
                return

            # Prepare data for DearPyGUI plotting
            dates = list(range(len(data)))
            closes = data["Close"].tolist()
            opens = data["Open"].tolist()
            highs = data["High"].tolist()
            lows = data["Low"].tolist()
            volumes = data["Volume"].tolist()

            # Create chart window
            chart_window_tag = f"chart_window_{ticker}"

            # Delete existing chart window if it exists
            if dpg.does_item_exist(chart_window_tag):
                dpg.delete_item(chart_window_tag)

            with dpg.window(
                    label=f"{ticker} Stock Chart",
                    tag=chart_window_tag,
                    width=800,
                    height=600,
                    modal=False,
                    show=True,
                    pos=[100, 100]
            ):
                dpg.add_text(f"📈 {ticker} - 1 Month Price Chart", color=[100, 255, 100])
                dpg.add_spacer(height=10)

                # Price chart
                with dpg.plot(
                        label="Price Chart",
                        height=350,
                        width=-1,
                        tag=f"price_plot_{ticker}"
                ):
                    dpg.add_plot_legend()
                    dpg.add_plot_axis(dpg.mvXAxis, label="Days", tag=f"x_axis_{ticker}")
                    dpg.add_plot_axis(dpg.mvYAxis, label="Price ($)", tag=f"y_axis_{ticker}")

                    # Add candlestick chart if available, otherwise line chart
                    try:
                        dpg.add_candle_series(
                            dates, opens, closes, lows, highs,
                            label="OHLC",
                            parent=f"y_axis_{ticker}",
                            tag=f"candle_series_{ticker}"
                        )
                    except:
                        # Fallback to line chart if candlestick fails
                        dpg.add_line_series(
                            dates, closes,
                            label="Close Price",
                            parent=f"y_axis_{ticker}",
                            tag=f"line_series_{ticker}"
                        )

                dpg.add_spacer(height=20)
                dpg.add_text("📊 Volume Chart", color=[100, 255, 100])

                # Volume chart
                with dpg.plot(
                        label="Volume Chart",
                        height=150,
                        width=-1,
                        tag=f"volume_plot_{ticker}"
                ):
                    dpg.add_plot_axis(dpg.mvXAxis, label="Days", tag=f"vol_x_axis_{ticker}")
                    dpg.add_plot_axis(dpg.mvYAxis, label="Volume", tag=f"vol_y_axis_{ticker}")

                    dpg.add_bar_series(
                        dates, volumes,
                        label="Volume",
                        parent=f"vol_y_axis_{ticker}",
                        tag=f"volume_series_{ticker}"
                    )

                dpg.add_spacer(height=20)

                # Chart controls
                with dpg.group(horizontal=True):
                    dpg.add_button(
                        label="Close Chart",
                        callback=lambda: dpg.delete_item(chart_window_tag),
                        width=100
                    )
                    dpg.add_button(
                        label="Refresh Data",
                        callback=lambda: self.refresh_chart_data(ticker),
                        width=100
                    )

                # Add current price info
                dpg.add_spacer(height=10)
                dpg.add_separator()
                dpg.add_spacer(height=5)

                current_price = closes[-1]
                prev_price = closes[-2] if len(closes) > 1 else current_price
                change = current_price - prev_price
                change_pct = (change / prev_price * 100) if prev_price != 0 else 0

                dpg.add_text(f"💰 Current Price: ${current_price:.2f}", color=[255, 255, 100])

                color = [100, 255, 100] if change >= 0 else [255, 100, 100]
                dpg.add_text(f"📈 Daily Change: ${change:+.2f} ({change_pct:+.2f}%)", color=color)

                dpg.add_text(f"📊 Volume: {volumes[-1]:,}", color=[200, 200, 200])
                dpg.add_text(f"📅 Data Period: 1 Month", color=[200, 200, 200])

            self.show_message(f"Chart for {ticker} displayed successfully.", "success")

        except Exception as e:
            self.show_message(f"Error generating chart for {ticker}: {e}", "error")

    def refresh_chart_data(self, ticker):
        """Refresh chart data for a specific ticker"""
        try:
            # Re-generate the chart with fresh data
            threading.Thread(target=self.generate_chart, args=(ticker,), daemon=True).start()
            self.show_message(f"Refreshing chart data for {ticker}...", "info")
        except Exception as e:
            self.show_message(f"Error refreshing chart for {ticker}: {e}", "error")

    def fetch_stock_info(self, ticker):
        """Fetch and display stock information in a popup window"""
        try:
            stock = yf.Ticker(ticker)
            info = stock.info

            # Create info window
            info_window_tag = f"info_window_{ticker}"

            # Delete existing info window if it exists
            if dpg.does_item_exist(info_window_tag):
                dpg.delete_item(info_window_tag)

            with dpg.window(
                    label=f"{ticker} Stock Information",
                    tag=info_window_tag,
                    width=500,
                    height=400,
                    modal=False,
                    show=True,
                    pos=[150, 150]
            ):
                dpg.add_text(f"📊 {ticker} Stock Information", color=[100, 255, 100])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                # Create info table
                with dpg.table(
                        resizable=True,
                        policy=dpg.mvTable_SizingStretchProp,
                        borders_innerH=True,
                        borders_outerH=True,
                        borders_innerV=True,
                        borders_outerV=True,
                        height=280
                ):
                    dpg.add_table_column(label="Metric", width_fixed=True, init_width_or_weight=200)
                    dpg.add_table_column(label="Value", width_fixed=True, init_width_or_weight=250)

                    # Add stock information rows
                    info_items = [
                        ("Company Name", info.get('shortName', 'N/A')),
                        ("Sector", info.get('sector', 'N/A')),
                        ("Industry", info.get('industry', 'N/A')),
                        ("Market Cap", f"{info.get('marketCap', 'N/A'):,}" if isinstance(info.get('marketCap'),
                                                                                         (int, float)) else 'N/A'),
                        ("P/E Ratio", f"{info.get('trailingPE', 'N/A'):.2f}" if isinstance(info.get('trailingPE'),
                                                                                           (int, float)) else 'N/A'),
                        ("Volume",
                         f"{info.get('volume', 'N/A'):,}" if isinstance(info.get('volume'), (int, float)) else 'N/A'),
                        ("Previous Close",
                         f"${info.get('previousClose', 'N/A'):.2f}" if isinstance(info.get('previousClose'),
                                                                                  (int, float)) else 'N/A'),
                        ("52 Week High",
                         f"${info.get('fiftyTwoWeekHigh', 'N/A'):.2f}" if isinstance(info.get('fiftyTwoWeekHigh'),
                                                                                     (int, float)) else 'N/A'),
                        ("52 Week Low",
                         f"${info.get('fiftyTwoWeekLow', 'N/A'):.2f}" if isinstance(info.get('fiftyTwoWeekLow'),
                                                                                    (int, float)) else 'N/A'),
                        ("Dividend Yield",
                         f"{info.get('dividendYield', 'N/A'):.2%}" if isinstance(info.get('dividendYield'),
                                                                                 (int, float)) else 'N/A'),
                        ("Beta",
                         f"{info.get('beta', 'N/A'):.2f}" if isinstance(info.get('beta'), (int, float)) else 'N/A'),
                        ("Book Value", f"${info.get('bookValue', 'N/A'):.2f}" if isinstance(info.get('bookValue'),
                                                                                            (int, float)) else 'N/A'),
                        ("Price to Book", f"{info.get('priceToBook', 'N/A'):.2f}" if isinstance(info.get('priceToBook'),
                                                                                                (int,
                                                                                                 float)) else 'N/A'),
                        ("Forward P/E", f"{info.get('forwardPE', 'N/A'):.2f}" if isinstance(info.get('forwardPE'),
                                                                                            (int, float)) else 'N/A'),
                        ("EPS", f"${info.get('trailingEps', 'N/A'):.2f}" if isinstance(info.get('trailingEps'),
                                                                                       (int, float)) else 'N/A'),
                    ]

                    for metric, value in info_items:
                        with dpg.table_row():
                            dpg.add_text(metric)
                            dpg.add_text(str(value))

                dpg.add_spacer(height=20)

                # Control buttons
                with dpg.group(horizontal=True):
                    dpg.add_button(
                        label="Close",
                        callback=lambda: dpg.delete_item(info_window_tag),
                        width=100
                    )
                    dpg.add_button(
                        label="Show Chart",
                        callback=lambda: threading.Thread(target=self.generate_chart, args=(ticker,),
                                                          daemon=True).start(),
                        width=100
                    )
                    dpg.add_button(
                        label="Refresh Info",
                        callback=lambda: threading.Thread(target=self.fetch_stock_info, args=(ticker,),
                                                          daemon=True).start(),
                        width=100
                    )

            self.show_message(f"Stock info for {ticker} displayed successfully.", "success")

        except Exception as e:
            self.show_message(f"Error fetching info for {ticker}: {e}", "error")

    # ============================================================================
    # UTILITY METHODS
    # ============================================================================

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

    def save_watchlist_to_settings(self):
        """Save current watchlist state to settings"""
        for ticker, data in self.watchlist.items():
            if ticker in self.settings.get("portfolios", {}).get("watchlist", {}):
                self.settings["portfolios"]["watchlist"][ticker]["quantity"] = data.get("quantity", 0)
                self.settings["portfolios"]["watchlist"][ticker]["avg_price"] = data.get("avg_price", 0)
                self.settings["portfolios"]["watchlist"][ticker]["last_added"] = data.get("last_updated", "")

        self.save_settings()

    def clear_table(self, table_tag):
        """Clear all rows from a table while preserving columns"""
        if dpg.does_item_exist(table_tag):
            # Get all children (rows) of the table
            children = dpg.get_item_children(table_tag, slot=1)  # slot=1 for table rows
            if children:
                for child in children:
                    dpg.delete_item(child)

    def show_message(self, message, message_type="info"):
        """Show message to user - integrates with app's notification system"""
        if hasattr(self.app, 'show_message'):
            # Use app's message system if available
            self.app.show_message(message, message_type)
        else:
            # Fallback to console output
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
            # Update main container size if it exists
            if dpg.does_item_exist("watchlist_main_container"):
                # The child window will automatically resize with parent
                pass
        except Exception as e:
            print(f"Error resizing watchlist components: {e}")

    def cleanup(self):
        """Clean up watchlist tab resources"""
        try:
            # Stop refresh thread
            self.stop_refresh_thread()

            # Save current state
            self.save_watchlist_to_settings()

            # Clean up resources
            self.watchlist = {}
            self.settings = {}
            self.selected_ticker = None

            print("✅ Watchlist tab cleaned up")
        except Exception as e:
            print(f"❌ Watchlist tab cleanup error: {e}")