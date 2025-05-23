class DataProcessing:
    """Class to process and structure unstructured data for different chart types."""

    @staticmethod
    def process(chart_type: str, raw_data: any) -> dict:
        """
        Convert unstructured data into a structured format required for the given chart type.

        Args:
            chart_type (str): Type of the chart (e.g., "line", "bar", "scatter", "candlestick", etc.).
            raw_data (any): Unstructured data (could be lists, dicts, DataFrames, etc.).

        Returns:
            dict: Structured data required for the chart.
        """

        if chart_type == "line" or chart_type == "scatter":
            # Expecting a list of tuples or dict {"x": [], "y": []}
            if isinstance(raw_data, list) and all(isinstance(i, tuple) and len(i) == 2 for i in raw_data):
                x, y = zip(*raw_data)
                return {"x": list(x), "y": list(y)}
            elif isinstance(raw_data, dict) and "x" in raw_data and "y" in raw_data:
                return raw_data
            else:
                raise ValueError("Invalid data format for line/scatter chart. Expected list of (x, y) tuples or {'x': [], 'y': []}.")

        elif chart_type == "bar":
            # Expecting a dictionary {"labels": [], "values": []}
            if isinstance(raw_data, dict) and "labels" in raw_data and "values" in raw_data:
                return raw_data
            elif isinstance(raw_data, list) and all(isinstance(i, tuple) and len(i) == 2 for i in raw_data):
                labels, values = zip(*raw_data)
                return {"labels": list(labels), "values": list(values)}
            else:
                raise ValueError("Invalid data format for bar chart. Expected {'labels': [], 'values': []} or list of (label, value) tuples.")

        elif chart_type == "candlestick":
            # Expecting a list of tuples [(date, open, high, low, close)]
            if isinstance(raw_data, list) and all(isinstance(i, tuple) and len(i) == 5 for i in raw_data):
                return {"ohlc": raw_data}
            else:
                raise ValueError("Invalid data format for candlestick chart. Expected list of (date, open, high, low, close) tuples.")

        if chart_type == "pie":

            # Expecting a dictionary {"labels": [], "values": []}

            if isinstance(raw_data, dict) and "labels" in raw_data and "values" in raw_data:

                if not isinstance(raw_data["labels"], list) or not isinstance(raw_data["values"], list):
                    raise ValueError("Pie chart labels and values must be lists.")

                if len(raw_data["labels"]) != len(raw_data["values"]):
                    raise ValueError("Pie chart labels and values must have the same length.")

                return raw_data

            elif isinstance(raw_data, list) and all(isinstance(i, tuple) and len(i) == 2 for i in raw_data):

                labels, values = zip(*raw_data)

                return {"labels": list(labels), "values": list(values)}

            else:

                raise ValueError(
                    "Invalid data format for pie chart. Expected {'labels': [], 'values': []} or list of (label, value) tuples.")

        elif chart_type == "histogram":
            # Expecting a list of values
            if isinstance(raw_data, list) and all(isinstance(i, (int, float)) for i in raw_data):
                return {"values": raw_data}
            else:
                raise ValueError("Invalid data format for histogram. Expected a list of numerical values.")

        elif chart_type == "bubble":
            # Expecting a dictionary {"x": [], "y": [], "sizes": []}
            if isinstance(raw_data, dict) and "x" in raw_data and "y" in raw_data and "sizes" in raw_data:
                return raw_data
            elif isinstance(raw_data, list) and all(isinstance(i, tuple) and len(i) == 3 for i in raw_data):
                x, y, sizes = zip(*raw_data)
                return {"x": list(x), "y": list(y), "sizes": list(sizes)}
            else:
                raise ValueError("Invalid data format for bubble chart. Expected {'x': [], 'y': [], 'sizes': []} or list of (x, y, size) tuples.")

        elif chart_type == "multi_bar":
            # Expecting a dictionary {"categories": [], "series_data": {}}
            if isinstance(raw_data, dict) and "categories" in raw_data and "series_data" in raw_data:
                if not isinstance(raw_data["categories"], list) or not isinstance(raw_data["series_data"], dict):
                    raise ValueError("Multi-bar chart data must be structured as {'categories': [], 'series_data': {}}.")

                # Ensure series_data contains lists of equal length
                for key, values in raw_data["series_data"].items():
                    if not isinstance(values, list):
                        raise ValueError(f"Multi-bar chart series '{key}' must contain a list of values.")
                    if len(values) != len(raw_data["categories"]):
                        raise ValueError(f"Mismatch in length between 'categories' and series '{key}' values.")

                return raw_data

            elif isinstance(raw_data, list) and all(isinstance(i, tuple) and len(i) >= 2 for i in raw_data):
                # If input is a list of tuples, assume (category, series_name, value)
                categories = sorted(set(i[0] for i in raw_data))  # Unique sorted categories
                series_data = {}

                for cat, series, val in raw_data:
                    if series not in series_data:
                        series_data[series] = [None] * len(categories)  # Initialize with None
                    series_data[series][categories.index(cat)] = val

                # Convert None to 0 for missing values
                for key in series_data.keys():
                    series_data[key] = [0 if v is None else v for v in series_data[key]]

                return {"categories": categories, "series_data": series_data}

            else:
                raise ValueError(
                    "Invalid data format for multi-bar chart. Expected {'categories': [], 'series_data': {}} or list of (category, series, value) tuples.")

        elif chart_type == "mixed_bar_line":
            # Expecting a dictionary {"categories": [], "bar_series_data": {}, "line_series_data": {}}
            if isinstance(raw_data,
                          dict) and "categories" in raw_data and "bar_series_data" in raw_data and "line_series_data" in raw_data:
                if not isinstance(raw_data["categories"], list) or not isinstance(raw_data["bar_series_data"],
                                                                                  dict) or not isinstance(
                        raw_data["line_series_data"], dict):
                    raise ValueError(
                        "Mixed bar-line chart data must be structured as {'categories': [], 'bar_series_data': {}, 'line_series_data': {}}.")
                return raw_data
            else:
                raise ValueError(
                    "Invalid data format for mixed bar-line chart. Expected {'categories': [], 'bar_series_data': {}, 'line_series_data': {}}.")
        else:
            raise ValueError(f"Unknown chart type: {chart_type}")