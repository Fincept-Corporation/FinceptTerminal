from textual.app import App, ComposeResult
from textual.widgets import Header, Footer
from ChartingWidget import ChartWidget


class TestChartApp(App):
    """Textual App to test various chart widgets."""

    CSS = """
    Screen {
        align: center middle;
    }
    """

    def compose(self) -> ComposeResult:
        yield Header()

        print("\n🚀 Testing: Line Chart Widget")
        chart_data_line = {"x": list(range(10)), "y": [x ** 2 for x in range(10)]}
        yield ChartWidget(chart_type="line", raw_data=chart_data_line, title="📈 Line Chart",)

        print("\n🚀 Testing: Bar Chart Widget")
        chart_data_bar = {"labels": ["A", "B", "C", "D"], "values": [10, 30, 20, 50]}
        yield ChartWidget(chart_type="bar", raw_data=chart_data_bar, title="📊 Bar Chart")

        print("\n🚀 Testing: Scatter Plot Widget")
        chart_data_scatter = {"x": list(range(10)), "y": [x * 3 for x in range(10)]}
        yield ChartWidget(chart_type="scatter", raw_data=chart_data_scatter, title="🎯 Scatter Plot")

        print("\n🚀 Testing: Candlestick Chart Widget")
        chart_data_candlestick = {
            "ohlc": [
                ("2024-02-01", 100, 110, 95, 105),
                ("2024-02-02", 105, 115, 100, 110),
                ("2024-02-03", 110, 120, 105, 115),
                ("2024-02-04", 115, 125, 110, 120),
                ("2024-02-05", 120, 130, 115, 125),
            ]
        }
        yield ChartWidget(chart_type="candlestick", raw_data=chart_data_candlestick, title="💹 Candlestick Chart")

        print("\n🚀 Testing: Pie Chart Widget")
        chart_data_pie = {"labels": ["Apple", "Banana", "Cherry"], "values": [30, 40, 30]}
        yield ChartWidget(chart_type="pie", raw_data=chart_data_pie, title="🥧 Pie Chart")

        print("\n🚀 Testing: Histogram Widget")
        chart_data_histogram = {"values": [5, 10, 15, 10, 5, 20, 30, 10, 5, 30, 25, 15, 10]}
        yield ChartWidget(chart_type="histogram", raw_data=chart_data_histogram, title="📊 Histogram")

        print("\n🚀 Testing: Bubble Chart Widget")
        chart_data_bubble = {
            "x": list(range(10)),
            "y": [x * 2 for x in range(10)],
            "sizes": [10 * x for x in range(10)]
        }
        yield ChartWidget(chart_type="bubble", raw_data=chart_data_bubble, title="🔵 Bubble Chart")

        yield Footer()


if __name__ == "__main__":
    print("🚀 Running Test Chart App - Press 'q' to exit.")
    TestChartApp().run()
