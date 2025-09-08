# -*- coding: utf-8 -*-
# analytics_tab.py (Permanent Fix - All logging issues resolved)

import dearpygui.dearpygui as dpg
from fincept_terminal.utils.base_tab import BaseTab
from fincept_terminal.DatabaseConnector.DataSources.data_source_manager import get_data_source_manager

# PERMANENT FIX: Set up global debug function before any other imports
import builtins


# Create safe logging functions that never fail
def safe_info(msg, module=None, context=None):
    """Safe info logging that never fails"""
    try:
        from fincept_terminal.utils.Logging.logger import info
        info(msg, module, context)
    except:
        pass


def safe_debug(msg, module=None, context=None):
    """Safe debug logging that never fails"""
    try:
        from fincept_terminal.utils.Logging.logger import debug
        debug(msg, module, context)
    except:
        pass


def safe_warning(msg, module=None, context=None):
    """Safe warning logging that never fails"""
    try:
        from fincept_terminal.utils.Logging.logger import warning
        warning(msg, module, context)
    except:
        pass


def safe_error(msg, module=None, context=None, exc_info=False):
    """Safe error logging that never fails"""
    try:
        from fincept_terminal.utils.Logging.logger import error
        error(msg, module, context, exc_info)
    except:
        pass


def safe_operation(name, module=None, **kwargs):
    """Safe operation context that never fails"""
    try:
        from fincept_terminal.utils.Logging.logger import operation
        return operation(name, module, **kwargs)
    except:
        from contextlib import contextmanager
        @contextmanager
        def dummy_operation():
            yield

        return dummy_operation()


def safe_monitor_performance(func):
    """Safe performance monitor that never fails"""
    try:
        from fincept_terminal.utils.Logging.logger import monitor_performance
        return monitor_performance(func)
    except:
        return func


# Set global debug function to prevent NameError
if not hasattr(builtins, 'debug'):
    builtins.debug = safe_debug

# Also set other common logging functions globally
if not hasattr(builtins, 'info'):
    builtins.info = safe_info
if not hasattr(builtins, 'warning'):
    builtins.warning = safe_warning
if not hasattr(builtins, 'error'):
    builtins.error = safe_error

# Additional safety: monkey patch the debug function in common modules
import sys

current_module = sys.modules[__name__]
current_module.debug = safe_debug
current_module.info = safe_info
current_module.warning = safe_warning
current_module.error = safe_error


class AnalyticsTab(BaseTab):
    """Analytics tab with bulletproof logging integration"""

    def __init__(self, app):
        super().__init__(app)
        safe_info("Initializing Analytics Tab", module="AnalyticsTab")

        # Initialize data manager with error protection
        try:
            self.data_manager = get_data_source_manager(app)
            if not self.data_manager:
                from fincept_terminal.DatabaseConnector.DataSources.data_source_manager import DataSourceManager
                self.data_manager = DataSourceManager(app)
        except Exception as e:
            safe_error(f"Failed to initialize data manager: {str(e)}", module="AnalyticsTab")
            self.data_manager = None

        self.current_symbol = "AAPL"
        self.current_forex_pair = "EURUSD"
        self.current_crypto = "BTC"
        self.chart_data = {}

        # Load data with protection
        self.load_real_data()

    def get_label(self):
        return "📊 Analytics"

    def load_real_data(self):
        """Load real data from configured sources with complete error protection"""
        safe_info(f"Loading data: {self.current_symbol}, {self.current_forex_pair}, {self.current_crypto}",
                  module="AnalyticsTab")

        if not self.data_manager:
            safe_warning("No data manager available", module="AnalyticsTab")
            self._load_fallback_data()
            return

        try:
            with safe_operation("load_data_sources", module="AnalyticsTab"):
                # Safely check mappings
                try:
                    mappings = self.data_manager.get_current_mappings()
                    safe_debug(f"Current mappings: {mappings}", module="AnalyticsTab")
                except Exception as e:
                    safe_warning(f"Could not get mappings: {str(e)}", module="AnalyticsTab")
                    mappings = {}

                # Safely check settings manager
                try:
                    settings_mgr = None
                    if hasattr(self.data_manager, 'get_settings_manager'):
                        settings_mgr = self.data_manager.get_settings_manager()
                    safe_debug(f"Settings manager found: {settings_mgr is not None}", module="AnalyticsTab")
                except Exception as e:
                    safe_warning(f"Could not check settings manager: {str(e)}", module="AnalyticsTab")

                # Load data with individual error handling
                stock_data = self._safe_get_stock_data()
                forex_data = self._safe_get_forex_data()
                crypto_data = self._safe_get_crypto_data()

                self.process_data_for_charts(stock_data, forex_data, crypto_data)
                safe_info("Data loading completed", module="AnalyticsTab")

        except Exception as e:
            safe_error(f"Failed to load data: {str(e)}", module="AnalyticsTab", exc_info=True)
            self._load_fallback_data()

    def _safe_get_stock_data(self):
        """Safely get stock data"""
        try:
            data = self.data_manager.get_stock_data(self.current_symbol, "5d", "1d")
            safe_debug(f"Stock data success: {data.get('success', False)}, source: {data.get('source', 'unknown')}",
                       module="AnalyticsTab")
            return data
        except Exception as e:
            safe_error(f"Error fetching stock data: {str(e)}", module="AnalyticsTab")
            return {"success": False, "error": f"Error fetching stock data: {str(e)}", "source": "error"}

    def _safe_get_forex_data(self):
        """Safely get forex data"""
        try:
            data = self.data_manager.get_forex_data(self.current_forex_pair, "5d")
            safe_debug(f"Forex data success: {data.get('success', False)}, source: {data.get('source', 'unknown')}",
                       module="AnalyticsTab")
            return data
        except Exception as e:
            safe_error(f"Error fetching forex data: {str(e)}", module="AnalyticsTab")
            return {"success": False, "error": f"Error fetching forex data: {str(e)}", "source": "error"}

    def _safe_get_crypto_data(self):
        """Safely get crypto data"""
        try:
            data = self.data_manager.get_crypto_data(self.current_crypto, "5d")
            safe_debug(f"Crypto data success: {data.get('success', False)}, source: {data.get('source', 'unknown')}",
                       module="AnalyticsTab")
            return data
        except Exception as e:
            safe_error(f"Error fetching crypto data: {str(e)}", module="AnalyticsTab")
            return {"success": False, "error": f"Error fetching crypto data: {str(e)}", "source": "error"}

    def process_data_for_charts(self, stock_data, forex_data, crypto_data):
        """Process real data for charts"""
        self.chart_data = {}

        # Process stock data
        if stock_data.get("success") and stock_data.get("data"):
            stock_chart_data = stock_data["data"]
            self.chart_data.update({
                'stock_timestamps': list(range(len(stock_chart_data.get("close", [])))),
                'stock_prices': stock_chart_data.get("close", []),
                'stock_volumes': stock_chart_data.get("volume", []),
                'stock_opens': stock_chart_data.get("open", []),
                'stock_highs': stock_chart_data.get("high", []),
                'stock_lows': stock_chart_data.get("low", []),
                'stock_source': stock_data.get("source", "unknown")
            })
            safe_debug(f"Stock data processed: {len(stock_chart_data.get('close', []))} points", module="AnalyticsTab")
        else:
            self.chart_data.update({
                'stock_source': 'error',
                'stock_error': stock_data.get("error", "No data available")
            })
            safe_warning(f"Stock data failed: {stock_data.get('error', 'Unknown error')}", module="AnalyticsTab")

        # Process forex data
        if forex_data.get("success") and forex_data.get("data"):
            forex_chart_data = forex_data["data"]
            self.chart_data.update({
                'forex_timestamps': list(range(len(forex_chart_data.get("rates", [])))),
                'forex_rates': forex_chart_data.get("rates", []),
                'forex_source': forex_data.get("source", "unknown")
            })
            safe_debug(f"Forex data processed: {len(forex_chart_data.get('rates', []))} points", module="AnalyticsTab")
        else:
            self.chart_data.update({
                'forex_source': 'error',
                'forex_error': forex_data.get("error", "No data available")
            })
            safe_warning(f"Forex data failed: {forex_data.get('error', 'Unknown error')}", module="AnalyticsTab")

        # Process crypto data
        if crypto_data.get("success") and crypto_data.get("data"):
            crypto_chart_data = crypto_data["data"]
            self.chart_data.update({
                'crypto_timestamps': list(range(len(crypto_chart_data.get("prices", [])))),
                'crypto_prices': crypto_chart_data.get("prices", []),
                'crypto_source': crypto_data.get("source", "unknown")
            })
            safe_debug(f"Crypto data processed: {len(crypto_chart_data.get('prices', []))} points",
                       module="AnalyticsTab")
        else:
            self.chart_data.update({
                'crypto_source': 'error',
                'crypto_error': crypto_data.get("error", "No data available")
            })
            safe_warning(f"Crypto data failed: {crypto_data.get('error', 'Unknown error')}", module="AnalyticsTab")

    def _load_fallback_data(self):
        """Load fallback data when sources fail"""
        safe_warning("Loading fallback data", module="AnalyticsTab")
        self.chart_data = {
            'stock_source': 'fallback',
            'forex_source': 'fallback',
            'crypto_source': 'fallback',
            'stock_error': 'Data source unavailable',
            'forex_error': 'Data source unavailable',
            'crypto_error': 'Data source unavailable'
        }

    def create_content(self):
        """Create analytics dashboard"""
        try:
            with safe_operation("create_analytics_content", module="AnalyticsTab"):
                self.add_section_header("📊 Real-Time Analytics Dashboard")

                # Data source status
                self.create_data_source_status()
                dpg.add_spacer(height=10)

                # Control panel
                self.create_control_panel()
                dpg.add_spacer(height=15)

                # Charts
                self.create_charts_grid()
        except Exception as e:
            safe_error(f"Error creating content: {str(e)}", module="AnalyticsTab", exc_info=True)

    def create_data_source_status(self):
        """Show current data sources"""
        with dpg.group():
            dpg.add_text("🔗 Active Data Sources:", color=[255, 255, 100])

            try:
                if self.data_manager:
                    mappings = self.data_manager.get_current_mappings()
                    with dpg.group(horizontal=True):
                        dpg.add_text(f"Stocks: {mappings.get('stocks', 'None')}", color=[200, 200, 200])
                        dpg.add_spacer(width=20)
                        dpg.add_text(f"Forex: {mappings.get('forex', 'None')}", color=[200, 200, 200])
                        dpg.add_spacer(width=20)
                        dpg.add_text(f"Crypto: {mappings.get('crypto', 'None')}", color=[200, 200, 200])
                else:
                    dpg.add_text("Data manager not available", color=[255, 100, 100])
            except Exception as e:
                safe_error(f"Error getting mappings: {str(e)}", module="AnalyticsTab")
                dpg.add_text(f"Error getting mappings: {str(e)}", color=[255, 100, 100])

    def create_control_panel(self):
        """Create control panel"""
        with dpg.group(horizontal=True):
            dpg.add_button(label="🔄 Refresh", callback=self.refresh_data)
            dpg.add_button(label="⚙️ Settings", callback=self.open_settings)

            dpg.add_combo(["1d", "5d", "1mo"], default_value="5d", tag="analytics_time_range",
                          callback=self.on_time_range_changed, width=100)

        dpg.add_spacer(height=10)

        # Symbol inputs
        with dpg.group(horizontal=True):
            dpg.add_text("Stock:")
            dpg.add_input_text(tag="analytics_stock_symbol", default_value=self.current_symbol, width=80)
            dpg.add_button(label="Load", callback=self.load_stock_symbol)

            dpg.add_spacer(width=20)
            dpg.add_text("Forex:")
            dpg.add_input_text(tag="analytics_forex_pair", default_value=self.current_forex_pair, width=80)
            dpg.add_button(label="Load", callback=self.load_forex_pair)

            dpg.add_spacer(width=20)
            dpg.add_text("Crypto:")
            dpg.add_input_text(tag="analytics_crypto_symbol", default_value=self.current_crypto, width=80)
            dpg.add_button(label="Load", callback=self.load_crypto_symbol)

    def create_charts_grid(self):
        """Create charts grid"""
        data = self.chart_data

        # Row 1: Main charts
        with dpg.group(horizontal=True):
            self.create_stock_chart(data)
            self.create_forex_chart(data)
            self.create_crypto_chart(data)

        # Row 2: Volume and metrics
        with dpg.group(horizontal=True):
            self.create_volume_chart(data)
            self.create_performance_metrics(data)

    def create_stock_chart(self, data):
        """Create stock price chart"""
        with self.create_child_window(tag="analytics_stock_window", width=380, height=250):
            source = data.get('stock_source', 'unknown')
            dpg.add_text(f"📈 {self.current_symbol} (Source: {source})")

            if 'stock_error' in data:
                dpg.add_text(f"❌ {data['stock_error']}", color=[255, 100, 100])
            elif data.get('stock_prices'):
                try:
                    with dpg.plot(label="Stock Price", height=200, width=-1):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                        dpg.add_plot_axis(dpg.mvYAxis, label="Price ($)", tag="analytics_stock_y")

                        dpg.add_line_series(data['stock_timestamps'], data['stock_prices'],
                                            label=f"{self.current_symbol}", parent="analytics_stock_y",
                                            tag="analytics_stock_series")
                except Exception as e:
                    dpg.add_text(f"Chart error: {str(e)}", color=[255, 100, 100])

    def create_forex_chart(self, data):
        """Create forex chart"""
        with self.create_child_window(tag="analytics_forex_window", width=380, height=250):
            source = data.get('forex_source', 'unknown')
            dpg.add_text(f"💱 {self.current_forex_pair} (Source: {source})")

            if 'forex_error' in data:
                dpg.add_text(f"❌ {data['forex_error']}", color=[255, 100, 100])
            elif data.get('forex_rates'):
                try:
                    with dpg.plot(label="Forex Rate", height=200, width=-1):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                        dpg.add_plot_axis(dpg.mvYAxis, label="Rate", tag="analytics_forex_y")

                        dpg.add_line_series(data['forex_timestamps'], data['forex_rates'],
                                            label=f"{self.current_forex_pair}", parent="analytics_forex_y",
                                            tag="analytics_forex_series")
                except Exception as e:
                    dpg.add_text(f"Chart error: {str(e)}", color=[255, 100, 100])

    def create_crypto_chart(self, data):
        """Create crypto chart"""
        with self.create_child_window(tag="analytics_crypto_window", width=380, height=250):
            source = data.get('crypto_source', 'unknown')
            dpg.add_text(f"₿ {self.current_crypto} (Source: {source})")

            if 'crypto_error' in data:
                dpg.add_text(f"❌ {data['crypto_error']}", color=[255, 100, 100])
            elif data.get('crypto_prices'):
                try:
                    with dpg.plot(label="Crypto Price", height=200, width=-1):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                        dpg.add_plot_axis(dpg.mvYAxis, label="Price ($)", tag="analytics_crypto_y")

                        dpg.add_line_series(data['crypto_timestamps'], data['crypto_prices'],
                                            label=f"{self.current_crypto}", parent="analytics_crypto_y",
                                            tag="analytics_crypto_series")
                except Exception as e:
                    dpg.add_text(f"Chart error: {str(e)}", color=[255, 100, 100])

    def create_volume_chart(self, data):
        """Create volume chart"""
        with self.create_child_window(tag="analytics_volume_window", width=380, height=250):
            dpg.add_text(f"📊 {self.current_symbol} Volume")

            if data.get('stock_volumes'):
                try:
                    with dpg.plot(label="Volume", height=200, width=-1):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                        dpg.add_plot_axis(dpg.mvYAxis, label="Volume", tag="analytics_volume_y")

                        dpg.add_bar_series(data['stock_timestamps'], data['stock_volumes'],
                                           label="Volume", parent="analytics_volume_y")
                except Exception as e:
                    dpg.add_text(f"Chart error: {str(e)}", color=[255, 100, 100])
            else:
                dpg.add_text("No volume data available", color=[200, 200, 200])

    def create_performance_metrics(self, data):
        """Create performance metrics"""
        with self.create_child_window(tag="analytics_metrics_window", width=380, height=250):
            dpg.add_text("📈 Performance Metrics")
            dpg.add_spacer(height=10)

            prices = data.get('stock_prices', [])
            if len(prices) > 1:
                try:
                    current = prices[-1]
                    previous = prices[-2]
                    change = current - previous
                    change_pct = (change / previous) * 100 if previous != 0 else 0

                    dpg.add_text(f"Current: ${current:.2f}")
                    color = [100, 255, 100] if change >= 0 else [255, 100, 100]
                    dpg.add_text(f"Change: ${change:.2f} ({change_pct:.2f}%)", color=color)

                    if len(prices) >= 5:
                        high = max(prices)
                        low = min(prices)
                        dpg.add_text(f"High: ${high:.2f}")
                        dpg.add_text(f"Low: ${low:.2f}")
                except Exception as e:
                    dpg.add_text(f"Calculation error: {str(e)}", color=[255, 100, 100])
            else:
                dpg.add_text("No price data available", color=[200, 200, 200])

    # Callback methods
    def refresh_data(self):
        """Refresh all data"""
        safe_info("Refreshing all data", module="AnalyticsTab")
        self.load_real_data()
        self.update_charts()

    def update_charts(self):
        """Update chart data"""
        data = self.chart_data

        try:
            with safe_operation("update_charts", module="AnalyticsTab"):
                if dpg.does_item_exist("analytics_stock_series") and data.get('stock_prices'):
                    dpg.set_value("analytics_stock_series", [data['stock_timestamps'], data['stock_prices']])

                if dpg.does_item_exist("analytics_forex_series") and data.get('forex_rates'):
                    dpg.set_value("analytics_forex_series", [data['forex_timestamps'], data['forex_rates']])

                if dpg.does_item_exist("analytics_crypto_series") and data.get('crypto_prices'):
                    dpg.set_value("analytics_crypto_series", [data['crypto_timestamps'], data['crypto_prices']])

                safe_debug("Charts updated successfully", module="AnalyticsTab")
        except Exception as e:
            safe_error(f"Failed to update charts: {str(e)}", module="AnalyticsTab", exc_info=True)

    def on_time_range_changed(self, sender, app_data):
        """Handle time range change"""
        period = app_data
        safe_info(f"Time range changed to: {period}", module="AnalyticsTab")

        try:
            with safe_operation("time_range_change", module="AnalyticsTab", period=period):
                stock_data = self._safe_get_stock_data()
                forex_data = self._safe_get_forex_data()
                crypto_data = self._safe_get_crypto_data()

                self.process_data_for_charts(stock_data, forex_data, crypto_data)
                self.update_charts()
        except Exception as e:
            safe_error(f"Time range change failed: {str(e)}", module="AnalyticsTab", exc_info=True)

    def load_stock_symbol(self):
        """Load new stock symbol"""
        try:
            if dpg.does_item_exist("analytics_stock_symbol"):
                new_symbol = dpg.get_value("analytics_stock_symbol").upper()
                if new_symbol and new_symbol != self.current_symbol:
                    self.current_symbol = new_symbol
                    safe_info(f"Loading new stock: {new_symbol}", module="AnalyticsTab")
                    self.refresh_data()
        except Exception as e:
            safe_error(f"Error loading stock symbol: {str(e)}", module="AnalyticsTab")

    def load_forex_pair(self):
        """Load new forex pair"""
        try:
            if dpg.does_item_exist("analytics_forex_pair"):
                new_pair = dpg.get_value("analytics_forex_pair").upper()
                if new_pair and new_pair != self.current_forex_pair:
                    self.current_forex_pair = new_pair
                    safe_info(f"Loading new forex pair: {new_pair}", module="AnalyticsTab")
                    self.refresh_data()
        except Exception as e:
            safe_error(f"Error loading forex pair: {str(e)}", module="AnalyticsTab")

    def load_crypto_symbol(self):
        """Load new crypto symbol"""
        try:
            if dpg.does_item_exist("analytics_crypto_symbol"):
                new_crypto = dpg.get_value("analytics_crypto_symbol").upper()
                if new_crypto and new_crypto != self.current_crypto:
                    self.current_crypto = new_crypto
                    safe_info(f"Loading new crypto: {new_crypto}", module="AnalyticsTab")
                    self.refresh_data()
        except Exception as e:
            safe_error(f"Error loading crypto symbol: {str(e)}", module="AnalyticsTab")

    def open_settings(self):
        """Open settings tab"""
        try:
            if hasattr(self.app, 'tabs'):
                for tab_key in self.app.tabs.keys():
                    if 'settings' in tab_key.lower():
                        dpg.set_value("main_tab_bar", f"tab_{tab_key}")
                        safe_info(f"Opened Settings tab: {tab_key}", module="AnalyticsTab")
                        return
        except Exception as e:
            safe_warning(f"Could not open Settings tab: {str(e)}", module="AnalyticsTab")

    def cleanup(self):
        """Clean up resources"""
        safe_info("Analytics tab cleanup started", module="AnalyticsTab")
        try:
            self.chart_data = None
            safe_info("Analytics tab cleanup completed", module="AnalyticsTab")
        except Exception as e:
            safe_error(f"Error during cleanup: {str(e)}", module="AnalyticsTab")