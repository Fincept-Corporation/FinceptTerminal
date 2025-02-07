from rich.console import Console
from rich.text import Text
from typing import List, Tuple
import shutil

class TerminalChart:
    def __init__(self, width: int = None, height: int = 20):
        """Initialize the chart with dynamic terminal width and fixed height."""
        self.console = Console()
        self.width = width if width else shutil.get_terminal_size().columns - 10
        self.height = height

    def render(self, data, chart_type: str, title: str = ""):
        """Render the chart based on chart type."""
        self.console.print(f"\n[bold cyan]{title}[/bold cyan]\n")
        if chart_type == "line":
            self._draw_line_chart(data)
        elif chart_type == "bar":
            self._draw_bar_chart(data)
        elif chart_type == "scatter":
            self._draw_scatter_plot(data)
        elif chart_type == "candlestick":
            self._draw_candlestick_chart(data)
        else:
            raise ValueError("Unsupported chart type")

    def _draw_line_chart(self, data: List[Tuple[int, int]]):
        """Draws a simple line chart with proper index handling."""
        max_x = max(x for x, y in data)
        max_y = max(y for x, y in data)
        min_y = min(y for x, y in data)
        scale_y = (max_y - min_y) / (self.height - 1) if max_y != min_y else 1  # Prevent divide by zero

        # Initialize grid with empty space
        chart_grid = [" " * self.width for _ in range(self.height)]

        for x, y in data:
            x_pos = int((x / max_x) * (self.width - 1))
            y_pos = int(self.height - 1 - ((y - min_y) / scale_y))  # Inverted Y-axis

            # Ensure y_pos is within bounds
            y_pos = max(0, min(self.height - 1, y_pos))

            row = list(chart_grid[y_pos])
            row[x_pos] = "*"
            chart_grid[y_pos] = "".join(row)

        # Print chart
        for line in chart_grid:
            self.console.print(line)
        self.console.print("-" * self.width)

    def _draw_bar_chart(self, data: List[Tuple[str, int]]):
        """Draws a bar chart."""
        max_value = max(y for x, y in data)
        scale = max_value / self.height

        for label, value in data:
            bar_height = int(value / scale)
            bar = "█" * bar_height
            self.console.print(f"{label:<6} {bar}")
        self.console.print("-" * self.width)

    def _draw_scatter_plot(self, data: List[Tuple[int, int]]):
        """Draws a scatter plot with proper index handling."""
        if not data:
            self.console.print("[bold red]No data to plot.[/bold red]")
            return

        max_x = max(x for x, y in data)
        max_y = max(y for x, y in data)
        min_y = min(y for x, y in data)

        scale_x = (self.width - 1) / max_x if max_x > 0 else 1
        scale_y = (self.height - 1) / (max_y - min_y) if max_y != min_y else 1  # Prevent divide by zero

        # Initialize empty grid
        chart_grid = [" " * self.width for _ in range(self.height)]

        for x, y in data:
            x_pos = int(x * scale_x)
            y_pos = int(self.height - 1 - ((y - min_y) * scale_y))  # Inverted Y-axis for correct placement

            # Ensure positions stay within bounds
            x_pos = max(0, min(self.width - 1, x_pos))
            y_pos = max(0, min(self.height - 1, y_pos))

            # Convert row to list and place the scatter point
            row = list(chart_grid[y_pos])
            row[x_pos] = "•"
            chart_grid[y_pos] = "".join(row)

        # Print the scatter plot
        for line in chart_grid:
            self.console.print(line)
        self.console.print("-" * self.width)

    def _draw_candlestick_chart(self, data: List[Tuple[float, float, float, float]], title: str = "Candlestick Chart"):
        """Draws a candlestick chart with visible wicks and color-coded bodies."""
        if not data:
            self.console.print("[bold red]No data to plot.[/bold red]")
            return

        # Determine chart dimensions and scaling
        max_price = max(high for _, high, _, _ in data)
        min_price = min(low for _, _, low, _ in data)
        scale = (max_price - min_price) / (self.height - 1) if max_price != min_price else 1

        # Initialize chart grid
        chart_grid = [[" " for _ in range(len(data) * 4)] for _ in range(self.height)]

        for idx, (open_, high, low, close) in enumerate(data):
            x_pos = idx*2  # Space between candlesticks, with padding for alignment
            high_pos = int(self.height - 1 - (high - min_price) / scale)
            low_pos = int(self.height - 1 - (low - min_price) / scale)
            open_pos = int(self.height - 1 - (open_ - min_price) / scale)
            close_pos = int(self.height - 1 - (close - min_price) / scale)

            # Draw wicks (high to low)
            for y in range(low_pos, high_pos + 1):
                print(chart_grid[y][x_pos])
                if chart_grid[y][x_pos] == " ":
                    chart_grid[y][x_pos] = "│"

            # Draw the body (open to close) with color
            if open_ > close:
                body_symbol = "[red]█[/red]"  # Bearish (red)
            else:
                body_symbol = "[green]█[/green]"  # Bullish (green)

            for y in range(min(open_pos, close_pos), max(open_pos, close_pos) + 1):
                chart_grid[y][x_pos] = body_symbol

        # Render the chart
        self.console.print(f"\n[bold cyan]{title}[/bold cyan]\n")
        for row in chart_grid:
            row_text = "".join(row)
            self.console.print(row_text, markup=True)
        self.console.print("-" * self.width)


# Example usage of the upgraded TerminalChart class
if __name__ == "__main__":
    chart = TerminalChart()

    # Line chart data
    line_data = [(1, 10), (2, 15), (3, 8), (4, 20), (5, 10)]
    chart.render(line_data, "line", title="Upgraded Line Chart")

    # Bar chart data
    bar_data = [("A", 10), ("B", 15), ("C", 7), ("D", 20)]
    chart.render(bar_data, "bar", title="Upgraded Bar Chart")

    # Scatter plot data
    scatter_data = [(1, 10), (2, 15), (3, 8), (4, 20), (5, 10)]
    chart.render(scatter_data, "scatter", title="Upgraded Scatter Plot")

    # Candlestick chart data
    candlestick_data = [
        (100, 110, 90, 105),  # Bullish
        (105, 115, 95, 100),  # Bearish
        (100, 120, 85, 110),  # Bullish
        (110, 125, 105, 120),  # Bullish
        (120, 130, 110, 115),  # Bearish
        (115, 125, 105, 120),  # Bullish
        (120, 140, 110, 135),  # Bullish
        (135, 150, 125, 130),  # Bearish
        (130, 145, 120, 140),  # Bullish
        (140, 160, 130, 150),  # Bullish
        (150, 155, 145, 148),  # Bearish
        (148, 152, 140, 150),  # Bullish
        (150, 160, 140, 145),  # Bearish
        (145, 170, 130, 165),  # Bullish
        (165, 180, 150, 170),  # Bullish
        (170, 175, 160, 162),  # Bearish
        (162, 165, 155, 160),  # Bearish
        (160, 170, 150, 165),  # Bullish
    ]
    chart.render(candlestick_data, "candlestick", title="Upgraded Candlestick Chart")
