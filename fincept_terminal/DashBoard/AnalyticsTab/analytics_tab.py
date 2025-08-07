# -*- coding: utf-8 -*-
"""
Analytics Tab module for Fincept Terminal
Updated to use centralized logging system
"""

# analytics_tab.py - Updated to use Universal Data Source Manager
import dearpygui.dearpygui as dpg
import math
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.DatabaseConnector.DataSources.data_source_manager import get_data_source_manager

from fincept_terminal.Utils.Logging.logger import logger, log_operation

class AnalyticsTab(BaseTab):
    """
    Analytics tab that now uses the Universal Data Source Manager
    All data automatically comes from user-configured sources
    """

    def __init__(self, app):
        super().__init__(app)

        # Get the universal data source manager
        self.data_manager = get_data_source_manager(app)
        if not self.data_manager:
            from fincept_terminal.DatabaseConnector.DataSources.data_source_manager import DataSourceManager
            self.data_manager = DataSourceManager(app)

        # Chart data will now come from configured sources
        self.chart_data = {}
        self.current_symbol = "AAPL"
        self.current_forex_pair = "EURUSD"
        self.current_crypto = "BTC"

        # Load initial data
        self.load_real_data()

    def get_label(self):
        return " Analytics"

    def load_real_data(self):
        """Load real data from configured sources"""
        logger.info("Loading data from configured sources...", module="Analytics_Tab")

        # Load stock data using configured source
        stock_data = self.data_manager.get_stock_data(self.current_symbol, "5d", "1h")

        # Load forex data using configured source
        forex_data = self.data_manager.get_forex_data(self.current_forex_pair, "5d")

        # Load crypto data using configured source
        crypto_data = self.data_manager.get_crypto_data(self.current_crypto, "5d")

        # Load news data using configured source
        news_data = self.data_manager.get_news_data("financial", 10)

        # Process and store the data
        self.process_data_for_charts(stock_data, forex_data, crypto_data, news_data)

    def process_data_for_charts(self, stock_data, forex_data, crypto_data, news_data):
        """Process real data for chart display"""
        self.chart_data = {}

        # Process stock data
        if stock_data.get("success") and stock_data.get("data"):
            stock_chart_data = stock_data["data"]
            self.chart_data.update({
                'stock_timestamps': list(range(len(stock_chart_data.get("close", [])))),
                'stock_prices': stock_chart_data.get("close", [100, 101, 99, 102, 105]),
                'stock_volumes': stock_chart_data.get("volume", [1000, 1200, 800, 1500, 1800]),
                'stock_opens': stock_chart_data.get("open", [100, 101, 99, 102, 105]),
                'stock_highs': stock_chart_data.get("high", [102, 103, 101, 104, 107]),
                'stock_lows': stock_chart_data.get("low", [98, 99, 97, 100, 103]),
                'stock_source': stock_data.get("source", "unknown")
            })
        else:
            # Fallback data if source fails
            self.chart_data.update({
                'stock_timestamps': [1, 2, 3, 4, 5],
                'stock_prices': [100, 101, 99, 102, 105],
                'stock_volumes': [1000, 1200, 800, 1500, 1800],
                'stock_opens': [100, 101, 99, 102, 105],
                'stock_highs': [102, 103, 101, 104, 107],
                'stock_lows': [98, 99, 97, 100, 103],
                'stock_source': 'fallback',
                'stock_error': stock_data.get("error", "No error")
            })

        # Process forex data
        if forex_data.get("success") and forex_data.get("data"):
            forex_chart_data = forex_data["data"]
            self.chart_data.update({
                'forex_timestamps': list(range(len(forex_chart_data.get("rates", [])))),
                'forex_rates': forex_chart_data.get("rates", [1.2, 1.21, 1.19, 1.22, 1.25]),
                'forex_source': forex_data.get("source", "unknown")
            })
        else:
            self.chart_data.update({
                'forex_timestamps': [1, 2, 3, 4, 5],
                'forex_rates': [1.2, 1.21, 1.19, 1.22, 1.25],
                'forex_source': 'fallback',
                'forex_error': forex_data.get("error", "No error")
            })

        # Process crypto data
        if crypto_data.get("success") and crypto_data.get("data"):
            crypto_chart_data = crypto_data["data"]
            self.chart_data.update({
                'crypto_timestamps': list(range(len(crypto_chart_data.get("prices", [])))),
                'crypto_prices': crypto_chart_data.get("prices", [50000, 51000, 49000, 52000, 55000]),
                'crypto_source': crypto_data.get("source", "unknown")
            })
        else:
            self.chart_data.update({
                'crypto_timestamps': [1, 2, 3, 4, 5],
                'crypto_prices': [50000, 51000, 49000, 52000, 55000],
                'crypto_source': 'fallback',
                'crypto_error': crypto_data.get("error", "No error")
            })

        # Process news data
        if news_data.get("success") and news_data.get("articles"):
            self.chart_data.update({
                'news_count': len(news_data["articles"]),
                'news_articles': news_data["articles"][:5],  # Take first 5
                'news_source': news_data.get("source", "unknown")
            })
        else:
            self.chart_data.update({
                'news_count': 0,
                'news_articles': [],
                'news_source': 'fallback',
                'news_error': news_data.get("error", "No error")
            })

        # Generate some derived analytics data
        self.chart_data.update({
            'x_data': [i for i in range(10)],
            'y_data1': [i ** 0.5 for i in range(10)],
            'y_data2': [math.sin(i) * 10 for i in range(10)],
            'y_data3': [math.cos(i) * 8 for i in range(10)],
            'hist_data': [1, 2, 2, 3, 3, 3, 4, 4, 5],
            'bar_x': [1, 2, 3, 4, 5],
            'bar_y': [10, 15, 8, 12, 18]
        })

    def create_content(self):
        """Create analytics dashboard content with real data"""
        self.add_section_header(" Analytics Dashboard - Live Data from Configured Sources")

        # Show data source status
        self.create_data_source_status()
        dpg.add_spacer(height=10)

        # Control panel
        self.create_control_panel()
        dpg.add_spacer(height=15)

        # Charts grid with real data
        self.create_charts_grid()

    def create_data_source_status(self):
        """Show current data sources being used"""
        with dpg.group():
            dpg.add_text(" Current Data Sources:", color=[255, 255, 100])

            # Get current mappings
            mappings = self.data_manager.get_current_mappings()

            with dpg.group(horizontal=True):
                dpg.add_text(f"Stocks: {mappings.get('stocks', 'Not configured')}", color=[200, 200, 200])
                dpg.add_spacer(width=20)
                dpg.add_text(f"Forex: {mappings.get('forex', 'Not configured')}", color=[200, 200, 200])
                dpg.add_spacer(width=20)
                dpg.add_text(f"Crypto: {mappings.get('crypto', 'Not configured')}", color=[200, 200, 200])
                dpg.add_spacer(width=20)
                dpg.add_text(f"News: {mappings.get('news', 'Not configured')}", color=[200, 200, 200])

    def create_control_panel(self):
        """Create analytics control panel with data source controls"""
        with dpg.group(horizontal=True):
            dpg.add_button(label=" Refresh Data", callback=self.refresh_data)
            dpg.add_button(label=" Export Charts", callback=self.export_charts)
            dpg.add_combo(["Last 1 Day", "Last 5 Days", "Last 30 Days"],
                          default_value="Last 5 Days", tag="time_range", callback=self.on_time_range_changed)
            dpg.add_checkbox(label="Real-time Updates", tag="realtime_updates")

            dpg.add_spacer(width=20)
            dpg.add_button(label=" Configure Sources", callback=self.open_data_config)

        dpg.add_spacer(height=10)

        # Symbol selection
        with dpg.group(horizontal=True):
            dpg.add_text("Stock Symbol:")
            dpg.add_input_text(tag="stock_symbol", default_value=self.current_symbol, width=100)
            dpg.add_button(label="Load", callback=self.load_stock_symbol)

            dpg.add_spacer(width=20)
            dpg.add_text("Forex Pair:")
            dpg.add_input_text(tag="forex_pair", default_value=self.current_forex_pair, width=100)
            dpg.add_button(label="Load", callback=self.load_forex_pair)

            dpg.add_spacer(width=20)
            dpg.add_text("Crypto:")
            dpg.add_input_text(tag="crypto_symbol", default_value=self.current_crypto, width=100)
            dpg.add_button(label="Load", callback=self.load_crypto_symbol)

    def create_charts_grid(self):
        """Create the main charts grid with real data"""
        data = self.chart_data

        # First row - Real financial data
        with dpg.group(horizontal=True):
            self.create_stock_price_chart(data)
            self.create_forex_chart(data)
            self.create_crypto_chart(data)

        # Second row - Analytics and volume
        with dpg.group(horizontal=True):
            self.create_volume_chart(data)
            self.create_candlestick_chart(data)
            self.create_news_summary(data)

        # Third row - Technical analysis
        with dpg.group(horizontal=True):
            self.create_correlation_chart(data)
            self.create_trend_analysis(data)
            self.create_performance_metrics()

    def create_stock_price_chart(self, data):
        """Create stock price chart with real data"""
        with self.create_child_window(tag="stock_chart_window", width=380, height=250):
            source = data.get('stock_source', 'unknown')
            dpg.add_text(f" {self.current_symbol} Stock Price (Source: {source})")

            # Show error if any
            if 'stock_error' in data:
                dpg.add_text(f" {data['stock_error']}", color=[255, 100, 100])

            with dpg.plot(label="Stock Price", height=200, width=-1, tag="stock_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                dpg.add_plot_axis(dpg.mvYAxis, label="Price ($)", tag="stock_y_axis")

                if data.get('stock_timestamps') and data.get('stock_prices'):
                    dpg.add_line_series(data['stock_timestamps'], data['stock_prices'],
                                        label=f"{self.current_symbol} Price", parent="stock_y_axis", tag="stock_series")

    def create_forex_chart(self, data):
        """Create forex chart with real data"""
        with self.create_child_window(tag="forex_chart_window", width=380, height=250):
            source = data.get('forex_source', 'unknown')
            dpg.add_text(f" {self.current_forex_pair} (Source: {source})")

            if 'forex_error' in data:
                dpg.add_text(f" {data['forex_error']}", color=[255, 100, 100])

            with dpg.plot(label="Forex Rate", height=200, width=-1, tag="forex_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                dpg.add_plot_axis(dpg.mvYAxis, label="Exchange Rate", tag="forex_y_axis")

                if data.get('forex_timestamps') and data.get('forex_rates'):
                    dpg.add_line_series(data['forex_timestamps'], data['forex_rates'],
                                        label=f"{self.current_forex_pair}", parent="forex_y_axis", tag="forex_series")

    def create_crypto_chart(self, data):
        """Create crypto chart with real data"""
        with self.create_child_window(tag="crypto_chart_window", width=380, height=250):
            source = data.get('crypto_source', 'unknown')
            dpg.add_text(f"₿ {self.current_crypto} Price (Source: {source})")

            if 'crypto_error' in data:
                dpg.add_text(f" {data['crypto_error']}", color=[255, 100, 100])

            with dpg.plot(label="Crypto Price", height=200, width=-1, tag="crypto_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                dpg.add_plot_axis(dpg.mvYAxis, label="Price ($)", tag="crypto_y_axis")

                if data.get('crypto_timestamps') and data.get('crypto_prices'):
                    dpg.add_line_series(data['crypto_timestamps'], data['crypto_prices'],
                                        label=f"{self.current_crypto}", parent="crypto_y_axis", tag="crypto_series")

    def create_volume_chart(self, data):
        """Create volume chart"""
        with self.create_child_window(tag="volume_chart_window", width=380, height=250):
            dpg.add_text(f" {self.current_symbol} Volume")

            with dpg.plot(label="Volume", height=200, width=-1, tag="volume_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                dpg.add_plot_axis(dpg.mvYAxis, label="Volume", tag="volume_y_axis")

                if data.get('stock_timestamps') and data.get('stock_volumes'):
                    dpg.add_bar_series(data['stock_timestamps'], data['stock_volumes'],
                                       label="Volume", parent="volume_y_axis", tag="volume_series")

    def create_candlestick_chart(self, data):
        """Create candlestick chart with real data"""
        with self.create_child_window(tag="candle_chart_window", width=380, height=250):
            dpg.add_text(f" {self.current_symbol} OHLC")

            with dpg.plot(label="Candlestick", height=200, width=-1, tag="candle_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                dpg.add_plot_axis(dpg.mvYAxis, label="Price", tag="candle_y_axis")

                if all(k in data for k in
                       ['stock_timestamps', 'stock_opens', 'stock_highs', 'stock_lows', 'stock_prices']):
                    dpg.add_candle_series(data['stock_timestamps'], data['stock_opens'],
                                          data['stock_prices'], data['stock_lows'], data['stock_highs'],
                                          label="OHLC", parent="candle_y_axis", tag="candle_series")

    def create_news_summary(self, data):
        """Create news summary panel"""
        with self.create_child_window(tag="news_window", width=380, height=250):
            source = data.get('news_source', 'unknown')
            dpg.add_text(f" Financial News (Source: {source})")

            if 'news_error' in data:
                dpg.add_text(f" {data['news_error']}", color=[255, 100, 100])

            dpg.add_spacer(height=10)

            articles = data.get('news_articles', [])
            if articles:
                for i, article in enumerate(articles[:3]):  # Show top 3
                    dpg.add_text(f"• {article.get('title', 'No title')[:40]}...",
                                 color=[200, 255, 200])
                    dpg.add_spacer(height=5)
            else:
                dpg.add_text("No news articles available", color=[200, 200, 200])

            dpg.add_spacer(height=10)
            dpg.add_text(f"Total Articles: {data.get('news_count', 0)}")

    def create_correlation_chart(self, data):
        """Create correlation analysis chart"""
        with self.create_child_window(tag="correlation_window", width=380, height=250):
            dpg.add_text(" Asset Correlation Analysis")

            with dpg.plot(label="Correlation", height=200, width=-1, tag="correlation_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Stock Price")
                dpg.add_plot_axis(dpg.mvYAxis, label="Forex Rate", tag="correlation_y_axis")

                # Create scatter plot for correlation
                if data.get('stock_prices') and data.get('forex_rates'):
                    # Normalize data for correlation
                    stock_norm = data['stock_prices'][:len(data['forex_rates'])]
                    forex_norm = data['forex_rates'][:len(data['stock_prices'])]

                    if stock_norm and forex_norm:
                        dpg.add_scatter_series(stock_norm, forex_norm,
                                               label="Stock vs Forex", parent="correlation_y_axis")

    def create_trend_analysis(self, data):
        """Create trend analysis"""
        with self.create_child_window(tag="trend_window", width=380, height=250):
            dpg.add_text(" Trend Analysis")

            with dpg.plot(label="Trends", height=200, width=-1, tag="trend_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                dpg.add_plot_axis(dpg.mvYAxis, label="Normalized Price", tag="trend_y_axis")

                # Create normalized trend lines
                if data.get('stock_prices'):
                    # Simple moving average
                    prices = data['stock_prices']
                    if len(prices) >= 3:
                        moving_avg = []
                        for i in range(len(prices)):
                            if i < 2:
                                moving_avg.append(prices[i])
                            else:
                                avg = sum(prices[i - 2:i + 1]) / 3
                                moving_avg.append(avg)

                        timestamps = data.get('stock_timestamps', list(range(len(prices))))
                        dpg.add_line_series(timestamps, moving_avg,
                                            label="3-Period MA", parent="trend_y_axis")

    def create_performance_metrics(self):
        """Create performance metrics panel"""
        with self.create_child_window(tag="metrics_window", width=380, height=250):
            dpg.add_text(" Performance Metrics")
            dpg.add_spacer(height=10)

            # Calculate some basic metrics from real data
            stock_prices = self.chart_data.get('stock_prices', [])
            if len(stock_prices) > 1:
                current_price = stock_prices[-1]
                previous_price = stock_prices[-2]
                change = current_price - previous_price
                change_pct = (change / previous_price) * 100 if previous_price != 0 else 0

                dpg.add_text(f"Current Price: ${current_price:.2f}")
                color = [100, 255, 100] if change >= 0 else [255, 100, 100]
                dpg.add_text(f"Change: ${change:.2f} ({change_pct:.2f}%)", color=color)

                if len(stock_prices) >= 5:
                    volatility = sum(
                        abs(stock_prices[i] - stock_prices[i - 1]) for i in range(1, len(stock_prices))) / len(
                        stock_prices)
                    dpg.add_text(f"Volatility: {volatility:.2f}")

            dpg.add_spacer(height=10)

            # Data source reliability
            dpg.add_text("Data Source Health:", color=[255, 255, 100])
            stock_source = self.chart_data.get('stock_source', 'unknown')
            forex_source = self.chart_data.get('forex_source', 'unknown')
            crypto_source = self.chart_data.get('crypto_source', 'unknown')

            dpg.add_text(f"Stocks: {stock_source}")
            dpg.add_text(f"Forex: {forex_source}")
            dpg.add_text(f"Crypto: {crypto_source}")

    # Callback methods
    def refresh_data(self):
        """Refresh all data from configured sources"""
        logger.info("Refreshing data from all configured sources...", module="Analytics_Tab")
        self.load_real_data()
        self.update_charts()

    def update_charts(self):
        """Update all charts with new data"""
        data = self.chart_data

        # Update stock chart
        if dpg.does_item_exist("stock_series") and data.get('stock_timestamps') and data.get('stock_prices'):
            dpg.set_value("stock_series", [data['stock_timestamps'], data['stock_prices']])

        # Update forex chart
        if dpg.does_item_exist("forex_series") and data.get('forex_timestamps') and data.get('forex_rates'):
            dpg.set_value("forex_series", [data['forex_timestamps'], data['forex_rates']])

        # Update crypto chart
        if dpg.does_item_exist("crypto_series") and data.get('crypto_timestamps') and data.get('crypto_prices'):
            dpg.set_value("crypto_series", [data['crypto_timestamps'], data['crypto_prices']])

        logger.info("Charts updated with fresh data", module="Analytics_Tab")

    def export_charts(self):
        """Export charts functionality"""
        logger.info("Exporting charts...", module="Analytics_Tab")

    def on_time_range_changed(self, sender, app_data):
        """Handle time range change"""
        time_range_map = {
            "Last 1 Day": "1d",
            "Last 5 Days": "5d",
            "Last 30 Days": "1mo"
        }

        period = time_range_map.get(app_data, "5d")
        logger.info(f" Changed time range to {period}", module="Analytics_Tab", context={'period': period})

        # Reload data with new period
        stock_data = self.data_manager.get_stock_data(self.current_symbol, period, "1h")
        forex_data = self.data_manager.get_forex_data(self.current_forex_pair, period)
        crypto_data = self.data_manager.get_crypto_data(self.current_crypto, period)
        news_data = self.data_manager.get_news_data("financial", 10)

        self.process_data_for_charts(stock_data, forex_data, crypto_data, news_data)
        self.update_charts()

    def load_stock_symbol(self):
        """Load new stock symbol"""
        if dpg.does_item_exist("stock_symbol"):
            new_symbol = dpg.get_value("stock_symbol").upper()
            if new_symbol:
                self.current_symbol = new_symbol
                logger.info(f" Loading stock data for {new_symbol}", module="Analytics_Tab", context={'new_symbol': new_symbol})
                self.refresh_data()

    def load_forex_pair(self):
        """Load new forex pair"""
        if dpg.does_item_exist("forex_pair"):
            new_pair = dpg.get_value("forex_pair").upper()
            if new_pair:
                self.current_forex_pair = new_pair
                logger.info(f" Loading forex data for {new_pair}", module="Analytics_Tab", context={'new_pair': new_pair})
                self.refresh_data()

    def load_crypto_symbol(self):
        """Load new crypto symbol"""
        if dpg.does_item_exist("crypto_symbol"):
            new_crypto = dpg.get_value("crypto_symbol").upper()
            if new_crypto:
                self.current_crypto = new_crypto
                logger.info(f"₿ Loading crypto data for {new_crypto}", module="Analytics_Tab", context={'new_crypto': new_crypto})
                self.refresh_data()

    def open_data_config(self):
        """Open data configuration tab"""
        if hasattr(self.app, 'tabs') and 'Data Sources' in self.app.tabs:
            # Switch to data configuration tab
            try:
                dpg.set_value("main_tab_bar", "tab_Data Sources")
            except:
                pass
        logger.info("Opening data source configuration...", module="Analytics_Tab")

    def cleanup(self):
        """Clean up analytics resources"""
        self.chart_data = None