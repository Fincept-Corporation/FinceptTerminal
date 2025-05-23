import plotly.graph_objects as go
import os

class ChartRenderer:
    """Class to generate various types of charts using Plotly."""

    def __init__(self, output_dir="charts"):
        """Initialize with an output directory."""
        self.output_dir = output_dir
        os.makedirs(self.output_dir, exist_ok=True)

    def save_chart(self, fig, filename):
        """Save chart as an interactive HTML file and open it."""
        filepath = os.path.join(self.output_dir, filename)
        fig.write_html(filepath, auto_open=False)
        return filepath

    def generate_line_chart(self, filename, x_vals, y_vals, title="Line Chart"):
        fig = go.Figure()
        fig.add_trace(go.Scatter(x=x_vals, y=y_vals, mode="lines", name="Line"))
        fig.update_layout(title=title, xaxis_title="X-Axis", yaxis_title="Y-Axis")
        return self.save_chart(fig, filename)

    def generate_bar_chart(self, filename, labels, values, title="Bar Chart"):
        fig = go.Figure()
        fig.add_trace(go.Bar(x=labels, y=values, name="Bars"))
        fig.update_layout(title=title, xaxis_title="Categories", yaxis_title="Values")
        return fig.to_json()

    def generate_multi_bar_chart(self, filename, categories, series_data, title="Multi-Bar Chart"):
        fig = go.Figure()

        # Add multiple bar series
        for series_name, values in series_data.items():
            fig.add_trace(go.Bar(x=categories, y=values, name=series_name))

        fig.update_layout(
            title=title,
            xaxis_title="Categories",
            yaxis_title="Values",
            barmode="overlay",  # Ensures bars are grouped, not stacked
        )

        return fig.to_json()

    def generate_scatter_plot(self, filename, x_vals, y_vals, title="Scatter Plot"):
        fig = go.Figure()
        fig.add_trace(go.Scatter(x=x_vals, y=y_vals, mode="markers", name="Scatter"))
        fig.update_layout(title=title, xaxis_title="X-Axis", yaxis_title="Y-Axis")
        return self.save_chart(fig, filename)

    def generate_candlestick_chart(self, filename, ohlc_data, title="Candlestick Chart"):
        dates, opens, highs, lows, closes = zip(*ohlc_data)
        fig = go.Figure()
        fig.add_trace(go.Candlestick(
            x=dates, open=opens, high=highs, low=lows, close=closes,
            increasing=dict(line=dict(color='green')),
            decreasing=dict(line=dict(color='red'))
        ))
        fig.update_layout(title=title, xaxis_title="Date", yaxis_title="Price")
        return self.save_chart(fig, filename)

    def generate_pie_chart(self, filename, labels, values, title="Pie Chart"):
        fig = go.Figure(data=[go.Pie(labels=labels, values=values)])
        fig.update_layout(title=title)
        return self.save_chart(fig, filename)

    def generate_histogram(self, filename, values, title="Histogram"):
        fig = go.Figure(data=[go.Histogram(x=values)])
        fig.update_layout(title=title, xaxis_title="Value", yaxis_title="Frequency")
        return self.save_chart(fig, filename)

    def generate_bubble_chart(self, filename, x_vals, y_vals, sizes, title="Bubble Chart"):
        fig = go.Figure()
        fig.add_trace(go.Scatter(x=x_vals, y=y_vals, mode="markers",
                                 marker=dict(size=sizes)))
        fig.update_layout(title=title, xaxis_title="X-Axis", yaxis_title="Y-Axis")
        return self.save_chart(fig, filename)

    def generate_mixed_bar_line_chart(self, filename, categories, bar_series_data, line_series_data,
                                      bar_title="Bar Data", line_title="Line Data",
                                      y1_title="Primary Y-Axis", y2_title="Secondary Y-Axis",
                                      chart_title="Mixed Chart"):
        """
        Generate and save a chart combining bar and line data with dual y-axes.

        Args:
            filename (str): The output HTML filename.
            categories (list): X-axis categories (e.g., months, years).
            bar_series_data (dict): Bar data, keys are labels, values are lists of values.
            line_series_data (dict): Line data, keys are labels, values are lists of values.
            bar_title (str): Title for bar chart y-axis.
            line_title (str): Title for line chart y-axis.
            y1_title (str): Title for the primary y-axis.
            y2_title (str): Title for the secondary y-axis.
            chart_title (str): Title for the chart.
        """
        fig = go.Figure()

        # Add bar traces
        for series_name, values in bar_series_data.items():
            fig.add_trace(go.Bar(x=categories, y=values, name=series_name, yaxis="y1"))

        # Add line traces
        for series_name, values in line_series_data.items():
            fig.add_trace(go.Scatter(x=categories, y=values, mode="lines+markers",
                                     name=series_name, yaxis="y2"))

        # Update layout for dual y-axes
        fig.update_layout(
            title=chart_title,
            xaxis=dict(title="Categories"),
            yaxis=dict(title=y1_title, titlefont=dict(color="blue"), tickfont=dict(color="blue")),
            yaxis2=dict(title=y2_title, titlefont=dict(color="orange"), tickfont=dict(color="orange"),
                        overlaying="y", side="right"),
            legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1),
            barmode="group",  # Group bars side by side
        )

        return fig.to_json()