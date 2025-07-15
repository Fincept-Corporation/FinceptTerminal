import dearpygui.dearpygui as dpg
import math
from fincept_terminal.Utils.base_tab import BaseTab


class AnalyticsTab(BaseTab):
    """Analytics tab with comprehensive charts"""

    def __init__(self, app):
        super().__init__(app)
        self.chart_data = self._generate_chart_data()

    def get_label(self):
        return "Analytics"

    def _generate_chart_data(self):
        """Pre-generate chart data for performance"""
        return {
            'x_data': [i for i in range(10)],
            'y_data1': [i ** 0.5 for i in range(10)],
            'y_data2': [math.sin(i) * 10 for i in range(10)],
            'y_data3': [math.cos(i) * 8 for i in range(10)],
            'hist_data': [1, 2, 2, 3, 3, 3, 4, 4, 5],
            'dates': [1, 2, 3, 4, 5],
            'opens': [10, 12, 11, 13, 12],
            'highs': [15, 16, 14, 17, 16],
            'lows': [8, 9, 8, 10, 9],
            'closes': [12, 11, 13, 12, 14],
            'bar_x': [1, 2, 3, 4, 5],
            'bar_y': [10, 15, 8, 12, 18]
        }

    def create_content(self):
        """Create analytics dashboard content"""
        self.add_section_header("Analytics Dashboard - All Chart Types")

        # Control panel
        self.create_control_panel()
        dpg.add_spacer(height=15)

        # Charts grid
        self.create_charts_grid()

    def create_control_panel(self):
        """Create analytics control panel"""
        with dpg.group(horizontal=True):
            dpg.add_button(label="Refresh Data", callback=self.refresh_data)
            dpg.add_button(label="Export Charts", callback=self.export_charts)
            dpg.add_combo(["Last 7 Days", "Last 30 Days", "Last 90 Days"],
                          default_value="Last 30 Days", tag="time_range")
            dpg.add_checkbox(label="Real-time Updates", tag="realtime_updates")

    def create_charts_grid(self):
        """Create the main charts grid"""
        data = self.chart_data

        # First row of charts
        with dpg.group(horizontal=True):
            self.create_line_chart(data)
            self.create_scatter_chart(data)
            self.create_bar_chart(data)

        # Second row of charts
        with dpg.group(horizontal=True):
            self.create_histogram_chart(data)
            self.create_stem_chart(data)
            self.create_candlestick_chart(data)

        # Third row of charts
        with dpg.group(horizontal=True):
            self.create_area_chart(data)
            self.create_error_chart(data)
            self.create_progress_gauges()

    def create_line_chart(self, data):
        """Create line chart"""
        with self.create_child_window(tag="line_chart_window", width=380, height=250):
            dpg.add_text("Line Plot")
            with dpg.plot(label="Line Plot", height=200, width=-1, tag="line_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="X Axis")
                dpg.add_plot_axis(dpg.mvYAxis, label="Y Axis", tag="line_y_axis")
                dpg.add_line_series(data['x_data'], data['y_data1'],
                                    label="Series 1", parent="line_y_axis", tag="line_series1")
                dpg.add_line_series(data['x_data'], data['y_data2'],
                                    label="Series 2", parent="line_y_axis", tag="line_series2")

    def create_scatter_chart(self, data):
        """Create scatter chart"""
        with self.create_child_window(tag="scatter_chart_window", width=380, height=250):
            dpg.add_text("Scatter Plot")
            with dpg.plot(label="Scatter Plot", height=200, width=-1, tag="scatter_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="X Axis")
                dpg.add_plot_axis(dpg.mvYAxis, label="Y Axis", tag="scatter_y_axis")
                dpg.add_scatter_series(data['x_data'], data['y_data3'],
                                       label="Scatter Data", parent="scatter_y_axis", tag="scatter_series")

    def create_bar_chart(self, data):
        """Create bar chart"""
        with self.create_child_window(tag="bar_chart_window", width=380, height=250):
            dpg.add_text("Bar Chart")
            with dpg.plot(label="Bar Chart", height=200, width=-1, tag="bar_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Categories")
                dpg.add_plot_axis(dpg.mvYAxis, label="Values", tag="bar_y_axis")
                dpg.add_bar_series(data['bar_x'], data['bar_y'],
                                   label="Bar Data", parent="bar_y_axis", tag="bar_series")

    def create_histogram_chart(self, data):
        """Create histogram chart"""
        with self.create_child_window(tag="hist_chart_window", width=380, height=250):
            dpg.add_text("Histogram")
            with dpg.plot(label="Histogram", height=200, width=-1, tag="hist_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Value")
                dpg.add_plot_axis(dpg.mvYAxis, label="Frequency", tag="hist_y_axis")
                dpg.add_histogram_series(data['hist_data'],
                                         label="Histogram", parent="hist_y_axis", tag="hist_series")

    def create_stem_chart(self, data):
        """Create stem chart"""
        with self.create_child_window(tag="stem_chart_window", width=380, height=250):
            dpg.add_text("Stem Plot")
            with dpg.plot(label="Stem Plot", height=200, width=-1, tag="stem_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="X Axis")
                dpg.add_plot_axis(dpg.mvYAxis, label="Y Axis", tag="stem_y_axis")
                dpg.add_stem_series(data['x_data'][:6], data['y_data1'][:6],
                                    label="Stem Data", parent="stem_y_axis", tag="stem_series")

    def create_candlestick_chart(self, data):
        """Create candlestick chart"""
        with self.create_child_window(tag="candle_chart_window", width=380, height=250):
            dpg.add_text("Candlestick Chart")
            with dpg.plot(label="Candlestick", height=200, width=-1, tag="candle_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                dpg.add_plot_axis(dpg.mvYAxis, label="Price", tag="candle_y_axis")
                dpg.add_candle_series(data['dates'], data['opens'], data['closes'],
                                      data['lows'], data['highs'],
                                      label="OHLC", parent="candle_y_axis", tag="candle_series")

    def create_area_chart(self, data):
        """Create area chart"""
        with self.create_child_window(tag="area_chart_window", width=380, height=250):
            dpg.add_text("Area Plot")
            with dpg.plot(label="Area Plot", height=200, width=-1, tag="area_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="X Axis")
                dpg.add_plot_axis(dpg.mvYAxis, label="Y Axis", tag="area_y_axis")
                dpg.add_shade_series(data['x_data'], data['y_data1'],
                                     label="Area", parent="area_y_axis", tag="area_series")

    def create_error_chart(self, data):
        """Create error bars chart"""
        with self.create_child_window(tag="error_chart_window", width=380, height=250):
            dpg.add_text("Error Bars")
            with dpg.plot(label="Error Bars", height=200, width=-1, tag="error_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="X Axis")
                dpg.add_plot_axis(dpg.mvYAxis, label="Y Axis", tag="error_y_axis")
                dpg.add_line_series(data['x_data'][:5], data['y_data1'][:5],
                                    label="Data", parent="error_y_axis", tag="error_line")
                dpg.add_error_series(data['x_data'][:5], data['y_data1'][:5], [1] * 5, [1] * 5,
                                     label="Error", parent="error_y_axis", tag="error_bars")

    def create_progress_gauges(self):
        """Create progress gauges panel"""
        with self.create_child_window(tag="gauges_window", width=380, height=250):
            dpg.add_text("Progress & Gauges")
            dpg.add_spacer(height=10)
            dpg.add_text("CPU Usage")
            dpg.add_progress_bar(default_value=0.75, width=-1, tag="cpu_gauge")
            dpg.add_spacer(height=10)
            dpg.add_text("Memory Usage")
            dpg.add_progress_bar(default_value=0.45, width=-1, tag="memory_gauge")
            dpg.add_spacer(height=10)
            dpg.add_text("Disk Usage")
            dpg.add_progress_bar(default_value=0.60, width=-1, tag="disk_gauge")
            dpg.add_spacer(height=10)
            dpg.add_text("Network Speed")
            dpg.add_progress_bar(default_value=0.30, width=-1, tag="network_gauge")

    # Callback methods
    def refresh_data(self):
        """Refresh all chart data"""
        print("Refreshing analytics data...")
        # Here you would typically fetch new data and update the charts
        # Example: Update line chart
        new_data = self._generate_chart_data()
        if dpg.does_item_exist("line_series1"):
            dpg.set_value("line_series1", [new_data['x_data'], new_data['y_data1']])
        if dpg.does_item_exist("line_series2"):
            dpg.set_value("line_series2", [new_data['x_data'], new_data['y_data2']])

    def export_charts(self):
        """Export charts functionality"""
        print("Exporting charts...")
        # Implementation for exporting charts

    def cleanup(self):
        """Clean up analytics resources"""
        self.chart_data = None