"""Debug GUI for Alpha Vantage Wrapper using DearPyGUI"""

import dearpygui.dearpygui as dpg
from wrapper import AlphaVantageWrapper, WrapperFactory, ParameterValidator
from datetime import datetime, timedelta
import pandas as pd

class WrapperGUI:

    def __init__(self):
        self.wrapper = None
        self.current_data = None

    def setup_gui(self):
        dpg.create_context()

        with dpg.window(label="Alpha Vantage Debug Test", tag="main_window"):

            # API Key Section
            with dpg.group(horizontal=True):
                dpg.add_text("API Key:")
                dpg.add_input_text(tag="api_key", default_value="5TNCFKELJ0Q26GFA", width=200)
                dpg.add_button(label="Connect", callback=self.connect_api)
                dpg.add_button(label="Simple Test", callback=self.simple_test)

            dpg.add_text("", tag="connection_status")
            dpg.add_separator()

            # Query Parameters
            with dpg.group(horizontal=True):
                dpg.add_text("Symbol:")
                dpg.add_input_text(tag="symbol", default_value="AAPL", width=100)

                dpg.add_text("Data Type:")
                dpg.add_combo(["equity", "eps", "etf"], tag="data_type", default_value="equity", width=100)

            with dpg.group(horizontal=True):
                dpg.add_text("Start Date:")
                dpg.add_input_text(tag="start_date", default_value="2024-01-01", width=120)

                dpg.add_text("End Date:")
                dpg.add_input_text(tag="end_date", default_value="2024-01-10", width=120)

            with dpg.group(horizontal=True):
                dpg.add_text("Interval:")
                dpg.add_combo(["1d", "1W", "1M"], tag="interval", default_value="1d", width=100)

                dpg.add_text("Adjustment:")
                dpg.add_combo(["splits_only", "unadjusted"], tag="adjustment", default_value="unadjusted", width=150)

            dpg.add_separator()

            # Action Buttons
            with dpg.group(horizontal=True):
                dpg.add_button(label="Get Data", callback=self.get_data)
                dpg.add_button(label="Test Basic Query", callback=self.test_basic)
                dpg.add_button(label="Clear Debug", callback=self.clear_debug)

            dpg.add_separator()

            # Results Section
            dpg.add_text("Status:")
            dpg.add_text("", tag="result_status")

            # Debug Output
            dpg.add_text("Debug Output:")
            dpg.add_input_text(tag="debug_output", multiline=True, height=200, width=750, readonly=True)

            # Data Table
            dpg.add_text("Data Preview:")
            with dpg.table(header_row=True, tag="data_table", height=200):
                pass

        dpg.create_viewport(title="Alpha Vantage Debug", width=800, height=800)
        dpg.setup_dearpygui()
        dpg.set_primary_window("main_window", True)
        dpg.show_viewport()

    def log_debug(self, message):
        """Add debug message to output"""
        current = dpg.get_value("debug_output")
        timestamp = datetime.now().strftime("%H:%M:%S")
        new_message = f"[{timestamp}] {message}\n"
        dpg.set_value("debug_output", current + new_message)
        print(f"GUI DEBUG: {message}")

    def clear_debug(self):
        dpg.set_value("debug_output", "")

    def connect_api(self):
        api_key = dpg.get_value("api_key").strip()

        self.log_debug(f"Attempting connection with API key: {api_key}")

        if not api_key:
            dpg.set_value("connection_status", "Enter API Key")
            return

        dpg.set_value("connection_status", "Testing...")

        try:
            self.wrapper = WrapperFactory.create_alpha_vantage(api_key)
            self.log_debug("Wrapper created successfully")

            result = self.wrapper.test_connection()
            self.log_debug(f"Connection test result: Success={result.success}")

            if result.success:
                dpg.set_value("connection_status", "✓ Connected")
                self.log_debug(f"Connection successful: {result.message}")
                if result.debug_info:
                    self.log_debug(f"Debug info: {result.debug_info}")
            else:
                dpg.set_value("connection_status", f"✗ {result.error}")
                self.log_debug(f"Connection failed: {result.error}")
                if result.debug_info:
                    self.log_debug(f"Error debug info: {result.debug_info}")

        except Exception as e:
            error_msg = f"Exception during connection: {str(e)}"
            dpg.set_value("connection_status", f"✗ Error")
            self.log_debug(error_msg)

    def simple_test(self):
        """Run simple API test without OpenBB wrapper"""
        if not self.wrapper:
            self.log_debug("No wrapper - creating one for simple test")
            api_key = dpg.get_value("api_key").strip()
            self.wrapper = WrapperFactory.create_alpha_vantage(api_key)

        self.log_debug("Running simple API test...")
        result = self.wrapper.simple_api_test()

        if result.success:
            self.log_debug(f"Simple test PASSED: {result.message}")
            dpg.set_value("connection_status", "✓ Simple test passed")
        else:
            self.log_debug(f"Simple test FAILED: {result.error}")
            dpg.set_value("connection_status", f"✗ Simple test failed")

        if result.debug_info:
            self.log_debug(f"Simple test debug: {result.debug_info}")

    def test_basic(self):
        """Test with minimal parameters"""
        if not self.wrapper:
            self.log_debug("No wrapper available")
            return

        self.log_debug("Testing basic equity query...")

        try:
            result = self.wrapper.get_equity_historical(symbol="AAPL")

            if result.success:
                self.log_debug(f"Basic test SUCCESS: {result.message}")
                dpg.set_value("result_status", f"✓ {result.message}")

                if result.debug_info:
                    self.log_debug(f"Basic test debug: {result.debug_info}")

                if result.data is not None:
                    self.current_data = result.data
                    self._display_data(result.data)
                    self.log_debug(f"Data shape: {result.data.shape}")
                    self.log_debug(f"Data columns: {result.data.columns.tolist()}")
            else:
                self.log_debug(f"Basic test FAILED: {result.error}")
                dpg.set_value("result_status", f"✗ {result.error}")

                if result.debug_info:
                    self.log_debug(f"Failure debug: {result.debug_info}")

        except Exception as e:
            error_msg = f"Basic test exception: {str(e)}"
            self.log_debug(error_msg)
            dpg.set_value("result_status", f"✗ Exception: {str(e)}")

    def get_data(self):
        if not self.wrapper:
            self.log_debug("No wrapper - connect first")
            dpg.set_value("result_status", "Connect to API first")
            return

        try:
            params = self._get_query_params()
            data_type = dpg.get_value("data_type")

            self.log_debug(f"Getting data - Type: {data_type}, Params: {params}")

            result = self.wrapper.execute_query(data_type, **params)

            if result.success:
                self.current_data = result.data
                self._display_data(result.data)
                dpg.set_value("result_status", f"✓ {result.message}")
                self.log_debug(f"Data fetch SUCCESS: {result.message}")

                if hasattr(result, 'debug_info') and result.debug_info:
                    self.log_debug(f"Success debug: {result.debug_info}")
            else:
                dpg.set_value("result_status", f"✗ {result.error}")
                self.log_debug(f"Data fetch FAILED: {result.error}")

                if hasattr(result, 'debug_info') and result.debug_info:
                    self.log_debug(f"Failure debug: {result.debug_info}")

        except Exception as e:
            error_msg = f"Get data exception: {str(e)}"
            self.log_debug(error_msg)
            dpg.set_value("result_status", f"✗ Exception: {str(e)}")

    def _get_query_params(self):
        params = {
            "symbol": dpg.get_value("symbol"),
        }

        data_type = dpg.get_value("data_type")

        if data_type in ["equity", "etf"]:
            params.update({
                "start_date": dpg.get_value("start_date"),
                "end_date": dpg.get_value("end_date"),
                "interval": dpg.get_value("interval"),
                "adjustment": dpg.get_value("adjustment")
            })
        elif data_type == "eps":
            params.update({
                "period": "quarter",
                "limit": 5
            })

        return params

    def _display_data(self, df: pd.DataFrame):
        # Clear existing table
        dpg.delete_item("data_table", children_only=True)

        if df is None or df.empty:
            self.log_debug("No data to display")
            return

        self.log_debug(f"Displaying data with shape: {df.shape}")

        # Add columns
        columns = list(df.columns)
        for col in columns:
            dpg.add_table_column(label=col, parent="data_table")

        # Add rows (limit to first 10 for display)
        display_df = df.head(10)

        for idx, row in display_df.iterrows():
            with dpg.table_row(parent="data_table"):
                for col in columns:
                    value = str(row[col])
                    if len(value) > 15:
                        value = value[:12] + "..."
                    dpg.add_text(value)

    def run(self):
        self.setup_gui()
        dpg.start_dearpygui()
        dpg.destroy_context()


if __name__ == "__main__":
    print("Starting Alpha Vantage Debug GUI...")
    print("Your API Key: 5TNCFKELJ0Q26GFA")
    app = WrapperGUI()
    app.run()