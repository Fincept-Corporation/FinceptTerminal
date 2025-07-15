import dearpygui.dearpygui as dpg
import yfinance as yf
import ta
import pandas as pd
from datetime import datetime, timedelta
from fincept_terminal.Utils.base_tab import BaseTab


class TechnicalAnalysisTab(BaseTab):
    """Technical Analysis tab with yfinance data and TA indicators"""

    def __init__(self, app):
        super().__init__(app)
        self.current_ticker = "AAPL"
        self.current_period = "1mo"
        self.current_indicator = "SMA"
        self.data = None
        self.indicators_data = {}
        self.available_indicators = {
            "SMA": "Simple Moving Average",
            "RSI": "Relative Strength Index",
            "MACD": "MACD",
            "Bollinger": "Bollinger Bands"
        }

    def get_label(self):
        return "Technical Analysis"

    def create_content(self):
        """Create technical analysis content"""
        self.add_section_header("📈 Technical Analysis Dashboard")

        # Control panel
        self.create_control_panel()
        dpg.add_spacer(height=15)

        # Charts area
        self.create_charts_area()

        # Load initial data
        self.fetch_data()

    def create_control_panel(self):
        """Create control panel with ticker input and settings"""
        try:
            with dpg.child_window(height=80, border=False):
                with dpg.group(horizontal=True):
                    # Ticker input
                    dpg.add_text("Ticker:")
                    dpg.add_input_text(
                        default_value=self.current_ticker,
                        width=80,
                        tag="ta_ticker_input",
                        on_enter=True
                    )

                    dpg.add_spacer(width=10)

                    # Period selection
                    dpg.add_text("Period:")
                    dpg.add_combo(
                        items=["5d", "1mo", "3mo", "6mo", "1y", "2y"],
                        default_value=self.current_period,
                        width=80,
                        tag="ta_period_combo"
                    )

                    dpg.add_spacer(width=10)

                    # Indicator selection
                    dpg.add_text("Indicator:")
                    dpg.add_combo(
                        items=list(self.available_indicators.keys()),
                        default_value=self.current_indicator,
                        width=120,
                        tag="ta_indicator_combo"
                    )

                    dpg.add_spacer(width=10)

                    # Action buttons
                    dpg.add_button(label="Fetch Data", callback=self.fetch_data)
                    dpg.add_button(label="Clear Charts", callback=self.clear_charts)
        except Exception as e:
            dpg.add_text(f"Control panel error: {str(e)}", color=[255, 100, 100])

    def create_charts_area(self):
        """Create charts display area"""
        try:
            with dpg.group():
                # Main candlestick chart
                with dpg.child_window(tag="ta_main_chart_window", height=400, border=True):
                    dpg.add_text("Price Chart with Indicators", tag="ta_chart_title")
                    with dpg.plot(label="Price Chart", height=350, width=-1, tag="ta_main_plot"):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Date", tag="ta_main_x_axis")
                        dpg.add_plot_axis(dpg.mvYAxis, label="Price ($)", tag="ta_main_y_axis")

                dpg.add_spacer(height=10)

                # Indicator chart (for oscillators like RSI)
                with dpg.child_window(tag="ta_indicator_chart_window", height=200, border=True):
                    dpg.add_text("Indicator Chart", tag="ta_indicator_title")
                    with dpg.plot(label="Indicator", height=150, width=-1, tag="ta_indicator_plot"):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Date", tag="ta_ind_x_axis")
                        dpg.add_plot_axis(dpg.mvYAxis, label="Value", tag="ta_ind_y_axis")
        except Exception as e:
            dpg.add_text(f"Charts area error: {str(e)}", color=[255, 100, 100])

    def fetch_data(self):
        """Fetch data from yfinance and calculate indicators"""
        try:
            # Update current values - get values directly since callbacks are removed
            if dpg.does_item_exist("ta_ticker_input"):
                self.current_ticker = dpg.get_value("ta_ticker_input").upper()
            if dpg.does_item_exist("ta_period_combo"):
                self.current_period = dpg.get_value("ta_period_combo")
            if dpg.does_item_exist("ta_indicator_combo"):
                self.current_indicator = dpg.get_value("ta_indicator_combo")

            # Update chart title
            if dpg.does_item_exist("ta_chart_title"):
                dpg.set_value("ta_chart_title",
                              f"{self.current_ticker} - {self.current_period} - {self.available_indicators[self.current_indicator]}")

            # Fetch data from yfinance
            ticker = yf.Ticker(self.current_ticker)
            self.data = ticker.history(period=self.current_period)

            if self.data.empty:
                self.show_error("No data found for ticker")
                return

            # Calculate indicators
            self.calculate_indicators()

            # Update charts
            self.update_charts()

        except Exception as e:
            self.show_error(f"Error fetching data: {str(e)}")

    def calculate_indicators(self):
        """Calculate technical indicators using TA library"""
        if self.data is None or self.data.empty:
            return

        self.indicators_data = {}

        try:
            # Simple Moving Average
            self.indicators_data['SMA_20'] = ta.trend.sma_indicator(self.data['Close'], window=20)
            self.indicators_data['SMA_50'] = ta.trend.sma_indicator(self.data['Close'], window=50)

            # RSI
            self.indicators_data['RSI'] = ta.momentum.rsi(self.data['Close'], window=14)

            # MACD
            macd = ta.trend.MACD(self.data['Close'])
            self.indicators_data['MACD'] = macd.macd()
            self.indicators_data['MACD_Signal'] = macd.macd_signal()
            self.indicators_data['MACD_Hist'] = macd.macd_diff()

            # Bollinger Bands
            bollinger = ta.volatility.BollingerBands(self.data['Close'])
            self.indicators_data['BB_Upper'] = bollinger.bollinger_hband()
            self.indicators_data['BB_Lower'] = bollinger.bollinger_lband()
            self.indicators_data['BB_Middle'] = bollinger.bollinger_mavg()

        except Exception as e:
            print(f"Error calculating indicators: {e}")

    def update_charts(self):
        """Update the charts with new data"""
        if self.data is None or self.data.empty:
            return

        # Clear existing series
        self.clear_charts()

        # Prepare data for plotting
        dates = list(range(len(self.data)))  # Use index as x-axis
        opens = self.data['Open'].tolist()
        highs = self.data['High'].tolist()
        lows = self.data['Low'].tolist()
        closes = self.data['Close'].tolist()

        # Add candlestick chart
        if dpg.does_item_exist("ta_main_y_axis"):
            dpg.add_candle_series(
                dates, opens, closes, lows, highs,
                label=f"{self.current_ticker} Price",
                parent="ta_main_y_axis",
                tag="ta_candlestick_series"
            )

            # Add selected indicator to main chart
            if self.current_indicator == "SMA":
                if 'SMA_20' in self.indicators_data:
                    sma_20 = self.indicators_data['SMA_20'].dropna().tolist()
                    sma_dates = dates[-len(sma_20):]
                    dpg.add_line_series(
                        sma_dates, sma_20,
                        label="SMA 20",
                        parent="ta_main_y_axis",
                        tag="ta_sma_20_series"
                    )

                if 'SMA_50' in self.indicators_data:
                    sma_50 = self.indicators_data['SMA_50'].dropna().tolist()
                    sma_dates = dates[-len(sma_50):]
                    dpg.add_line_series(
                        sma_dates, sma_50,
                        label="SMA 50",
                        parent="ta_main_y_axis",
                        tag="ta_sma_50_series"
                    )

            elif self.current_indicator == "Bollinger":
                if all(key in self.indicators_data for key in ['BB_Upper', 'BB_Lower', 'BB_Middle']):
                    bb_upper = self.indicators_data['BB_Upper'].dropna().tolist()
                    bb_lower = self.indicators_data['BB_Lower'].dropna().tolist()
                    bb_middle = self.indicators_data['BB_Middle'].dropna().tolist()
                    bb_dates = dates[-len(bb_upper):]

                    dpg.add_line_series(bb_dates, bb_upper, label="BB Upper", parent="ta_main_y_axis",
                                        tag="ta_bb_upper_series")
                    dpg.add_line_series(bb_dates, bb_lower, label="BB Lower", parent="ta_main_y_axis",
                                        tag="ta_bb_lower_series")
                    dpg.add_line_series(bb_dates, bb_middle, label="BB Middle", parent="ta_main_y_axis",
                                        tag="ta_bb_middle_series")

        # Add indicator to secondary chart
        self.update_indicator_chart(dates)

    def update_indicator_chart(self, dates):
        """Update the indicator chart"""
        if not dpg.does_item_exist("ta_ind_y_axis"):
            return

        if self.current_indicator == "RSI" and 'RSI' in self.indicators_data:
            rsi_data = self.indicators_data['RSI'].dropna().tolist()
            rsi_dates = dates[-len(rsi_data):]
            dpg.add_line_series(rsi_dates, rsi_data, label="RSI", parent="ta_ind_y_axis", tag="ta_rsi_series")

            # Add RSI reference lines
            dpg.add_line_series([rsi_dates[0], rsi_dates[-1]], [70, 70], label="Overbought", parent="ta_ind_y_axis",
                                tag="ta_rsi_70")
            dpg.add_line_series([rsi_dates[0], rsi_dates[-1]], [30, 30], label="Oversold", parent="ta_ind_y_axis",
                                tag="ta_rsi_30")

        elif self.current_indicator == "MACD" and all(key in self.indicators_data for key in ['MACD', 'MACD_Signal']):
            macd_data = self.indicators_data['MACD'].dropna().tolist()
            signal_data = self.indicators_data['MACD_Signal'].dropna().tolist()
            macd_dates = dates[-len(macd_data):]

            dpg.add_line_series(macd_dates, macd_data, label="MACD", parent="ta_ind_y_axis", tag="ta_macd_series")
            dpg.add_line_series(macd_dates, signal_data, label="Signal", parent="ta_ind_y_axis",
                                tag="ta_macd_signal_series")

    def clear_charts(self):
        """Clear all chart series"""
        series_tags = [
            "ta_candlestick_series", "ta_sma_20_series", "ta_sma_50_series",
            "ta_bb_upper_series", "ta_bb_lower_series", "ta_bb_middle_series",
            "ta_rsi_series", "ta_rsi_70", "ta_rsi_30", "ta_macd_series", "ta_macd_signal_series"
        ]

        for tag in series_tags:
            if dpg.does_item_exist(tag):
                dpg.delete_item(tag)

    def show_error(self, message):
        """Show error message"""
        if dpg.does_item_exist("ta_chart_title"):
            dpg.set_value("ta_chart_title", f"Error: {message}")

    # Callback methods - simplified since we removed direct callbacks
    def on_ticker_changed(self, sender, app_data):
        """Handle ticker change"""
        self.current_ticker = app_data.upper()

    def on_period_changed(self, sender, app_data):
        """Handle period change"""
        self.current_period = app_data

    def on_indicator_changed(self, sender, app_data):
        """Handle indicator change"""
        self.current_indicator = app_data
        if self.data is not None:
            self.update_charts()

    def cleanup(self):
        """Clean up resources"""
        self.data = None
        self.indicators_data = {}