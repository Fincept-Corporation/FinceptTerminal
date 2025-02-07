from textual.containers import Horizontal,Container
from textual.widget import Widget
from textual.app import ComposeResult
from textual.widgets import Button, Static
import webbrowser
import os
from fincept_terminal.FinceptUtilsModule.ChartingUtils.ChartDataProcessing import DataProcessing
from fincept_terminal.FinceptUtilsModule.ChartingUtils.ChartingMain import ChartRenderer


class ChartWidget(Widget):
    """A widget that allows developers to specify a chart type and raw data for display in the TUI."""

    def __init__(self, chart_type: str, raw_data: any, title: str, output_dir="charts", **kwargs):
        """
        Initialize the ChartWidget.

        Args:
            chart_type (str): The type of chart to generate (e.g., "line", "bar", etc.).
            raw_data (any): The unstructured data to be processed.
            title (str): The title of the chart.
            output_dir (str): Directory to save the generated chart HTML file.
        """
        super().__init__(**kwargs)
        self.chart_type = chart_type.lower().strip()  # Convert to lowercase for consistency
        self.raw_data = raw_data
        self.title = title
        self.output_dir = output_dir

        # ✅ Ensure output directory exists
        os.makedirs(self.output_dir, exist_ok=True)

        self.chart_renderer = ChartRenderer(self.output_dir)
        self.chart_path = None  # Store path to generated chart

    def compose(self) -> ComposeResult:
        """Create UI elements."""
        with Container(classes="graph_buttons"):
            yield Static(f"📊 {self.title} 📊", classes="title")
            with Horizontal():
                yield Button("Open Chart", id="open_chart")
                yield Button("Close", id="close_button")

    def generate_chart(self):
        """Process raw data and generate only the requested chart type."""
        try:
            # ✅ Ensure valid data before processing
            chart_data = DataProcessing.process(self.chart_type, self.raw_data)

            # ✅ Define valid chart types
            chart_mapping = {
                "line": lambda: self.chart_renderer.generate_line_chart("line_chart.html", chart_data["x"],
                                                                        chart_data["y"]),
                "bar": lambda: self.chart_renderer.generate_bar_chart("bar_chart.html", chart_data["labels"],
                                                                      chart_data["values"]),
                "scatter": lambda: self.chart_renderer.generate_scatter_plot("scatter_chart.html", chart_data["x"],
                                                                             chart_data["y"]),
                "candlestick": lambda: self.chart_renderer.generate_candlestick_chart("candlestick_chart.html",
                                                                                      chart_data["ohlc"]),
                "pie": lambda: self.chart_renderer.generate_pie_chart("pie_chart.html", chart_data["labels"],
                                                                      chart_data["values"]),
                "histogram": lambda: self.chart_renderer.generate_histogram("histogram.html", chart_data["values"]),
                "bubble": lambda: self.chart_renderer.generate_bubble_chart("bubble_chart.html", chart_data["x"],
                                                                            chart_data["y"], chart_data["sizes"]),
                "multi_bar": lambda: self.chart_renderer.generate_multi_bar_chart(
                    "multi_bar_chart.html",
                    chart_data["categories"],
                    chart_data["series_data"]
                )
            }

            # ✅ Generate only the requested chart type
            if self.chart_type in chart_mapping:
                print(f"📊 Generating chart: {self.chart_type}")
                self.chart_path = chart_mapping[self.chart_type]()  # Generate chart and store its path
                print(f"✅ Chart saved at: {self.chart_path}")
            else:
                print(f"❌ Error: Unsupported chart type '{self.chart_type}'")

        except ValueError as e:
            print(f"⚠ Data Processing Error: {e}")

    def on_mount(self) -> None:
        """Generate the chart when the widget is mounted."""
        self.generate_chart()

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button presses to open or close the chart."""
        if event.button.id == "open_chart":
            print(f"🔍 Debug: Chart path -> {self.chart_path}")  # Debugging print

            if self.chart_path and os.path.exists(self.chart_path):
                print(f"✅ Opening chart: {self.chart_path}")
                webbrowser.open(f"file://{os.path.abspath(self.chart_path)}")
            else:
                print(f"❌ Error: Chart file {self.chart_path} not found!")
        elif event.button.id == "close_button":
            self.remove()
