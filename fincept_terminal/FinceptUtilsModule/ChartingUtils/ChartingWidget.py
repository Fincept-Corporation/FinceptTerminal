from textual.containers import Horizontal, Container
from textual.widget import Widget
from textual.app import ComposeResult
from textual.widgets import Button, Static
import webbrowser
import os
import tempfile
from fincept_terminal.FinceptUtilsModule.ChartingUtils.ChartDataProcessing import DataProcessing
from fincept_terminal.FinceptUtilsModule.ChartingUtils.ChartingMain import ChartRenderer

class ChartWidget(Widget):
    """Custom reusable widget for dynamic charting."""
    CSS = """
    .chart_buttons{
        height: auto;
        border: red;
    }
    .chart_container{
        height: auto;
        border: cyan;
    }
    """
    def __init__(self, chart_type: str, title: str, widget_id: str, output_dir="charts", raw_data: any = None, **kwargs):
        """
        Initialize the ChartWidget.

        Args:
            chart_type (str): Type of chart to generate (e.g., "line", "bar", etc.).
            raw_data (any): The data to be visualized.
            title (str): Title of the chart.
            widget_id (str): Unique ID for the widget (e.g., "gdp_growth").
            output_dir (str): Directory for saving chart files.
        """
        super().__init__(id=widget_id,**kwargs)
        self.chart_type = chart_type.lower().strip()
        self.raw_data = raw_data
        self.title = title
        self.widget_id = widget_id
        self.output_dir = output_dir
        os.makedirs(self.output_dir, exist_ok=True)

        self.chart_renderer = ChartRenderer(self.output_dir)
        self.temp_chart_path = None  # Path to the generated chart

    def compose(self) -> ComposeResult:
        """Compose the widget UI."""
        yield Button(f"📊 Visualize {self.title} 📊", id=f"{self.widget_id}_open_chart")



    def generate_chart(self):
        """Generate the chart based on the provided data and chart type."""
        try:
            if not self.raw_data:
                self.app.notify("no raw data found")
            # Process the data for the chart
            chart_data = DataProcessing.process(self.chart_type, self.raw_data)
            # Chart type mapping
            chart_mapping = {
                "line": lambda: self.chart_renderer.generate_line_chart(
                    f"{self.title}_line_chart.html", chart_data["x"], chart_data["y"]
                ),
                "bar": lambda: self.chart_renderer.generate_bar_chart(
                    f"{self.title}_bar_chart.html", chart_data["labels"], chart_data["values"]
                ),
                "scatter": lambda: self.chart_renderer.generate_scatter_plot(
                    f"{self.title}_scatter_chart.html", chart_data["x"], chart_data["y"]
                ),
                "candlestick": lambda: self.chart_renderer.generate_candlestick_chart(
                    f"{self.title}_candlestick_chart.html", chart_data["ohlc"]
                ),
                "pie": lambda: self.chart_renderer.generate_pie_chart(
                    f"{self.title}_pie_chart.html", chart_data["labels"], chart_data["values"]
                ),
                "histogram": lambda: self.chart_renderer.generate_histogram(
                    f"{self.title}_histogram_chart.html", chart_data["values"]
                ),
                "bubble": lambda: self.chart_renderer.generate_bubble_chart(
                    f"{self.title}_bubble_chart.html", chart_data["x"], chart_data["y"], chart_data["sizes"]
                ),
                "multi_bar": lambda: self.chart_renderer.generate_multi_bar_chart(
                    f"{self.title}_multi_bar_chart.html", chart_data["categories"], chart_data["series_data"]
                ),
                "mixed_bar_line": lambda: self.chart_renderer.generate_mixed_bar_line_chart(
                    f"{self.title}_mixed_bar_line_chart.html",
                    chart_data["categories"],
                    chart_data["bar_series_data"],
                    chart_data["line_series_data"],
                    y1_title="Values (Mn. USD)",  # Customize y-axis title for bar
                    y2_title="Percentage Change",  # Customize y-axis title for line
                    chart_title=self.title  # Use the provided title
                ),
            }

            # ✅ Generate the appropriate chart
            if self.chart_type in chart_mapping:
                fig = chart_mapping[self.chart_type]()
            else:
                raise ValueError(f"❌ Unsupported chart type: {self.chart_type}")

            fig_html = f"""
                <html>
                <head>
                    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
                    <style>
                        body {{
                            margin: 0;
                            padding: 0;
                            display: flex;
                            justify-content: center;
                            align-items: center;
                            height: 100vh;
                            background-color: #f8f9fa; /* Light background */
                        }}
                        #chart-container {{
                            width: 95vw;
                            height: 90vh;
                        }}
                    </style>
                </head>
                <body>
                    <div id="chart-container"></div>
                    <script>
                        var graph = {fig};
                        Plotly.newPlot('chart-container', graph.data, graph.layout, {{ responsive: true }});
                    </script>
                </body>
                </html>
                """

            # ✅ Save chart temporarily
            with tempfile.NamedTemporaryFile(delete=False, suffix=".html") as tmpfile:
                tmpfile.write(fig_html.encode("utf-8"))  # Write HTML content
                self.temp_chart_path = tmpfile.name  # Store temp file path

        except Exception as e:
            print(f"Error generating chart: {e}")

    def update_chart(self, new_data: any, new_title: str):
        """Update the chart with new data dynamically."""
        if not new_data:
            self.app.notify("No Data found")
            return

        self.raw_data = new_data
        self.title = new_title
        self.generate_chart()

    def on_mount(self) -> None:
        """Generate the chart when the widget is mounted."""
        self.generate_chart()

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button presses for opening the chart."""
        if event.button.id == f"{self.widget_id}_open_chart":
            if self.temp_chart_path and os.path.exists(self.temp_chart_path):
                webbrowser.open(f"file://{self.temp_chart_path}")
            else:
                self.app.notify(f"⚠ Chart file not found: {self.temp_chart_path}", severity="error")

