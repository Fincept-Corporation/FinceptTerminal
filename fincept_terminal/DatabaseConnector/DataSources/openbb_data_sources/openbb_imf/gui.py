"""Debug IMF Wrapper GUI for Finance Terminal"""

import dearpygui.dearpygui as dpg
from datetime import datetime, timedelta
import pandas as pd
import asyncio
import aiohttp
from dataclasses import dataclass
from typing import Optional, Union, Dict, List, Any
import warnings
import json
import traceback

warnings.filterwarnings("ignore")


@dataclass
class WrapperResponse:
    success: bool
    data: Optional[Union[pd.DataFrame, Dict, List]] = None
    error: Optional[str] = None
    message: Optional[str] = None
    debug_info: Optional[str] = None


class DebugIMFWrapper:
    """Debug IMF wrapper with extensive logging"""

    def __init__(self):
        self.base_url = "http://dataservices.imf.org/REST/SDMX_JSON.svc/"
        self.debug_log = []

    def log_debug(self, message: str):
        """Add debug message with timestamp"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        debug_msg = f"[{timestamp}] {message}"
        self.debug_log.append(debug_msg)
        print(debug_msg)

    def get_debug_log(self) -> str:
        """Get all debug messages"""
        return "\n".join(self.debug_log[-20:])  # Last 20 messages

    def _run_async(self, coro):
        try:
            loop = asyncio.get_event_loop()
        except RuntimeError:
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
        return loop.run_until_complete(coro)

    async def _make_request(self, url: str) -> Dict:
        """Make async HTTP request with debug logging"""
        self.log_debug(f"Making request to: {url}")

        try:
            timeout = aiohttp.ClientTimeout(total=30)
            async with aiohttp.ClientSession(timeout=timeout) as session:
                self.log_debug("Session created, sending request...")

                async with session.get(url) as response:
                    self.log_debug(f"Response status: {response.status}")
                    self.log_debug(f"Response headers: {dict(response.headers)}")

                    if response.status == 200:
                        content_type = response.headers.get('content-type', '')
                        self.log_debug(f"Content type: {content_type}")

                        if 'json' in content_type:
                            data = await response.json()
                            self.log_debug(f"JSON response keys: {list(data.keys()) if isinstance(data, dict) else 'Not a dict'}")
                            return data
                        else:
                            text_data = await response.text()
                            self.log_debug(f"Text response (first 200 chars): {text_data[:200]}")
                            raise Exception(f"Expected JSON but got {content_type}")
                    else:
                        error_text = await response.text()
                        self.log_debug(f"Error response: {error_text[:200]}")
                        raise Exception(f"HTTP {response.status}: {response.reason}")

        except asyncio.TimeoutError:
            self.log_debug("Request timed out after 30 seconds")
            raise Exception("Request timeout")
        except aiohttp.ClientError as e:
            self.log_debug(f"Client error: {str(e)}")
            raise Exception(f"Network error: {str(e)}")
        except Exception as e:
            self.log_debug(f"Unexpected error: {str(e)}")
            raise

    def test_simple_connection(self) -> WrapperResponse:
        """Test basic connection to IMF API"""
        try:
            self.log_debug("Starting simple connection test...")

            # Test multiple endpoints
            test_urls = [
                # Simple dataflow query
                f"{self.base_url}Dataflow",
                # Basic DOT query
                f"{self.base_url}CompactData/DOT/A.US.TXG_FOB_USD.CN?startPeriod=2022&endPeriod=2023",
                # Alternative format
                "https://dataservices.imf.org/REST/SDMX_JSON.svc/CompactData/DOT/A.US.TXG_FOB_USD.CN?startPeriod=2022&endPeriod=2023"
            ]

            for i, url in enumerate(test_urls):
                try:
                    self.log_debug(f"Testing URL {i+1}: {url}")
                    response = self._run_async(self._make_request(url))

                    if response:
                        self.log_debug(f"URL {i+1} SUCCESS - got response")
                        return WrapperResponse(
                            success=True,
                            message=f"Connection successful via URL {i+1}",
                            debug_info=self.get_debug_log()
                        )
                except Exception as e:
                    self.log_debug(f"URL {i+1} FAILED: {str(e)}")
                    continue

            return WrapperResponse(
                success=False,
                error="All test URLs failed",
                debug_info=self.get_debug_log()
            )

        except Exception as e:
            self.log_debug(f"Connection test exception: {str(e)}")
            return WrapperResponse(
                success=False,
                error=str(e),
                debug_info=self.get_debug_log()
            )

    def get_direction_of_trade(self, **params) -> WrapperResponse:
        """Get bilateral trade data with multiple endpoint fallbacks"""
        try:
            self.log_debug("Starting direction of trade request...")

            # Default parameters
            defaults = {
                "country": "US",
                "counterpart": "CN",
                "direction": "exports",
                "frequency": "A",
                "start_date": "2022",
                "end_date": "2023"
            }

            query_params = {**defaults, **params}
            self.log_debug(f"Query parameters: {query_params}")

            # Map direction to indicator
            direction_map = {
                "exports": "TXG_FOB_USD",
                "imports": "TMG_CIF_USD",
                "balance": "TBG_USD"
            }

            indicator = direction_map.get(query_params["direction"], "TXG_FOB_USD")
            country = query_params["country"]
            counterpart = query_params["counterpart"]
            frequency = query_params["frequency"]
            start_date = query_params["start_date"]
            end_date = query_params["end_date"]

            self.log_debug(f"Mapped indicator: {indicator}")

            # Try different URL formats
            url_templates = [
                "https://dataservices.imf.org/REST/SDMX_JSON.svc/CompactData/DOT/{freq}.{country}.{indicator}.{counterpart}?startPeriod={start}&endPeriod={end}",
                "http://dataservices.imf.org/REST/SDMX_JSON.svc/CompactData/DOT/{freq}.{country}.{indicator}.{counterpart}?startPeriod={start}&endPeriod={end}",
                "https://data.imf.org/api/data/DOT/{freq}.{country}.{indicator}.{counterpart}?startPeriod={start}&endPeriod={end}"
            ]

            for i, template in enumerate(url_templates):
                try:
                    url = template.format(
                        freq=frequency,
                        country=country,
                        indicator=indicator,
                        counterpart=counterpart,
                        start=start_date,
                        end=end_date
                    )

                    self.log_debug(f"Trying URL template {i+1}: {url}")

                    # Make request
                    response = self._run_async(self._make_request(url))
                    self.log_debug(f"Got response from template {i+1}, analyzing...")

                    # Try to parse the response
                    result = self._parse_trade_response(response, query_params, indicator)
                    if result.success:
                        return result
                    else:
                        self.log_debug(f"Template {i+1} parsing failed: {result.error}")

                except Exception as e:
                    self.log_debug(f"Template {i+1} failed: {str(e)}")
                    continue

            # If all templates fail, try mock data for testing
            self.log_debug("All URL templates failed, generating mock data for testing...")
            return self._generate_mock_trade_data(query_params)

        except Exception as e:
            self.log_debug(f"Exception in get_direction_of_trade: {str(e)}")
            return WrapperResponse(
                success=False,
                error=str(e),
                debug_info=self.get_debug_log()
            )

    def _parse_trade_response(self, response, query_params, indicator) -> WrapperResponse:
        """Parse trade response with detailed logging"""
        try:
            # Debug response structure
            if isinstance(response, dict):
                self.log_debug(f"Response top-level keys: {list(response.keys())}")

                # Check for different response formats
                if "CompactData" in response:
                    return self._parse_sdmx_response(response, query_params, indicator)
                elif "data" in response:
                    return self._parse_simple_response(response, query_params, indicator)
                elif "ErrorDetails" in response:
                    error_details = response["ErrorDetails"]
                    self.log_debug(f"API Error: {error_details}")
                    return WrapperResponse(success=False, error=f"IMF API Error: {error_details}")
                else:
                    self.log_debug("Unknown response format")
                    return WrapperResponse(success=False, error="Unknown response format")
            else:
                return WrapperResponse(success=False, error="Response is not a dictionary")

        except Exception as e:
            self.log_debug(f"Response parsing error: {str(e)}")
            return WrapperResponse(success=False, error=f"Parsing error: {str(e)}")

    def _parse_sdmx_response(self, response, query_params, indicator) -> WrapperResponse:
        """Parse SDMX format response"""
        try:
            compact_data = response["CompactData"]
            self.log_debug(f"CompactData keys: {list(compact_data.keys()) if isinstance(compact_data, dict) else 'Not a dict'}")

            dataset = compact_data.get("DataSet", {})
            series = dataset.get("Series", [])

            if not series:
                self.log_debug("No series data found in SDMX response")
                return WrapperResponse(success=False, error="No series data in response")

            return self._extract_trade_records(series, query_params, indicator)

        except Exception as e:
            return WrapperResponse(success=False, error=f"SDMX parsing error: {str(e)}")

    def _parse_simple_response(self, response, query_params, indicator) -> WrapperResponse:
        """Parse simple JSON response format"""
        try:
            data = response.get("data", [])
            self.log_debug(f"Simple response data length: {len(data)}")

            if not data:
                return WrapperResponse(success=False, error="No data in simple response")

            # Convert simple format to our standard format
            records = []
            for item in data:
                records.append({
                    "date": item.get("date") or item.get("period"),
                    "country": query_params["country"],
                    "counterpart": query_params["counterpart"],
                    "direction": query_params["direction"],
                    "value": float(item.get("value", 0)),
                    "indicator": indicator
                })

            df = pd.DataFrame(records)
            return WrapperResponse(success=True, data=df, message=f"Retrieved {len(df)} records")

        except Exception as e:
            return WrapperResponse(success=False, error=f"Simple parsing error: {str(e)}")

    def _extract_trade_records(self, series, query_params, indicator) -> WrapperResponse:
        """Extract records from series data"""
        try:
            # Convert to list if single series
            if isinstance(series, dict):
                series = [series]
                self.log_debug("Converted single series dict to list")

            # Extract data
            records = []
            for i, s in enumerate(series):
                self.log_debug(f"Processing series {i}: {list(s.keys()) if isinstance(s, dict) else 'Not a dict'}")

                obs_list = s.get("Obs", [])
                if isinstance(obs_list, dict):
                    obs_list = [obs_list]

                self.log_debug(f"Series {i} has {len(obs_list)} observations")

                for j, obs in enumerate(obs_list):
                    time_period = obs.get("@TIME_PERIOD")
                    obs_value = obs.get("@OBS_VALUE")

                    self.log_debug(f"Obs {j}: TIME_PERIOD={time_period}, OBS_VALUE={obs_value}")

                    records.append({
                        "date": time_period,
                        "country": query_params["country"],
                        "counterpart": query_params["counterpart"],
                        "direction": query_params["direction"],
                        "value": float(obs_value) if obs_value and obs_value != "None" else 0,
                        "indicator": indicator
                    })

            self.log_debug(f"Extracted {len(records)} records")

            if not records:
                return WrapperResponse(success=False, error="No valid records extracted")

            df = pd.DataFrame(records)
            try:
                df["date"] = pd.to_datetime(df["date"])
            except:
                self.log_debug("Date conversion failed, keeping as string")

            return WrapperResponse(success=True, data=df, message=f"Retrieved {len(df)} trade records")

        except Exception as e:
            return WrapperResponse(success=False, error=f"Record extraction error: {str(e)}")

    def _generate_mock_trade_data(self, query_params) -> WrapperResponse:
        """Generate mock trade data when API is unavailable"""
        try:
            self.log_debug("Generating mock trade data for testing...")

            # Create realistic mock data
            years = [query_params["start_date"], query_params["end_date"]]
            if query_params["start_date"] != query_params["end_date"]:
                start_year = int(query_params["start_date"])
                end_year = int(query_params["end_date"])
                years = list(range(start_year, end_year + 1))

            records = []
            base_value = 500000  # 500 billion USD base value

            for year in years:
                # Add some realistic variation
                variation = (hash(f"{year}{query_params['country']}{query_params['counterpart']}") % 200 - 100) / 100
                value = base_value * (1 + variation * 0.1)

                records.append({
                    "date": f"{year}-01-01",
                    "country": query_params["country"],
                    "counterpart": query_params["counterpart"],
                    "direction": query_params["direction"],
                    "value": value,
                    "indicator": "MOCK_DATA"
                })

            df = pd.DataFrame(records)
            df["date"] = pd.to_datetime(df["date"])

            return WrapperResponse(
                success=True,
                data=df,
                message=f"Generated {len(df)} mock trade records (API unavailable)",
                debug_info=self.get_debug_log()
            )

        except Exception as e:
            return WrapperResponse(success=False, error=f"Mock data generation failed: {str(e)}")

    def execute_query(self, data_type: str, **params) -> WrapperResponse:
        """Generic query executor"""
        self.log_debug(f"Executing query: {data_type} with params: {params}")

        method_map = {
            "direction_of_trade": self.get_direction_of_trade,
            "test_connection": self.test_simple_connection
        }

        if data_type not in method_map:
            return WrapperResponse(
                success=False,
                error=f"Unknown data type: {data_type}",
                debug_info=self.get_debug_log()
            )

        return method_map[data_type](**params)


class DebugIMFGUI:

    def __init__(self):
        self.imf_wrapper = DebugIMFWrapper()
        self.current_data = None

    def setup_gui(self):
        dpg.create_context()

        with dpg.window(label="Debug IMF Data Terminal", tag="main_window"):

            # Connection Section
            with dpg.group(horizontal=True):
                dpg.add_text("IMF Data Provider (Debug Mode)")
                dpg.add_button(label="Test Connection", callback=self.test_connection)
                dpg.add_button(label="Clear Debug", callback=self.clear_debug)

            dpg.add_text("", tag="connection_status")
            dpg.add_separator()

            # Parameters Section
            dpg.add_text("Direction of Trade Parameters:")
            with dpg.group(horizontal=True):
                dpg.add_text("Country:")
                dpg.add_input_text(tag="country", default_value="US", width=80)
                dpg.add_text("Counterpart:")
                dpg.add_input_text(tag="counterpart", default_value="CN", width=80)

            with dpg.group(horizontal=True):
                dpg.add_text("Direction:")
                dpg.add_combo(["exports", "imports", "balance"],
                             tag="direction", default_value="exports", width=100)
                dpg.add_text("Frequency:")
                dpg.add_combo(["A", "Q", "M"],
                             tag="frequency", default_value="A", width=60)

            with dpg.group(horizontal=True):
                dpg.add_text("Start Year:")
                dpg.add_input_text(tag="start_date", default_value="2022", width=80)
                dpg.add_text("End Year:")
                dpg.add_input_text(tag="end_date", default_value="2023", width=80)

            dpg.add_separator()

            # Action Buttons
            with dpg.group(horizontal=True):
                dpg.add_button(label="Get Trade Data", callback=self.get_trade_data)
                dpg.add_button(label="Export CSV", callback=self.export_csv)
                dpg.add_button(label="Clear Data", callback=self.clear_data)

            dpg.add_separator()

            # Status
            dpg.add_text("Status:")
            dpg.add_text("", tag="status_text")

            dpg.add_separator()

            # Debug Log
            dpg.add_text("Debug Log:")
            dpg.add_input_text(tag="debug_log", multiline=True, height=200, width=750, readonly=True)

            dpg.add_separator()

            # Data Table
            dpg.add_text("Data Preview:")
            with dpg.table(header_row=True, tag="data_table", height=200):
                pass

            # Data Summary
            dpg.add_text("Summary:")
            dpg.add_text("", tag="data_summary")

        dpg.create_viewport(title="Debug IMF Terminal", width=800, height=800)
        dpg.setup_dearpygui()
        dpg.set_primary_window("main_window", True)
        dpg.show_viewport()

    def clear_debug(self):
        self.imf_wrapper.debug_log = []
        dpg.set_value("debug_log", "")

    def update_debug_display(self):
        debug_text = self.imf_wrapper.get_debug_log()
        dpg.set_value("debug_log", debug_text)

    def test_connection(self):
        try:
            dpg.set_value("connection_status", "Testing connection...")
            result = self.imf_wrapper.test_simple_connection()

            if result.success:
                dpg.set_value("connection_status", "✓ Connection successful")
            else:
                dpg.set_value("connection_status", f"✗ Connection failed: {result.error}")

            self.update_debug_display()

        except Exception as e:
            dpg.set_value("connection_status", f"✗ Test error: {str(e)}")
            self.update_debug_display()

    def get_trade_data(self):
        try:
            params = {
                "country": dpg.get_value("country"),
                "counterpart": dpg.get_value("counterpart"),
                "direction": dpg.get_value("direction"),
                "frequency": dpg.get_value("frequency"),
                "start_date": dpg.get_value("start_date"),
                "end_date": dpg.get_value("end_date")
            }

            dpg.set_value("status_text", "Fetching trade data...")
            result = self.imf_wrapper.get_direction_of_trade(**params)

            if result.success:
                self.current_data = result.data
                self._display_data(result.data)
                dpg.set_value("status_text", f"✓ {result.message}")
                self._update_summary(result.data)
            else:
                dpg.set_value("status_text", f"✗ {result.error}")

            self.update_debug_display()

        except Exception as e:
            dpg.set_value("status_text", f"✗ Error: {str(e)}")
            self.update_debug_display()

    def export_csv(self):
        if self.current_data is None:
            dpg.set_value("status_text", "No data to export")
            return

        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"imf_debug_{timestamp}.csv"

            self.current_data.to_csv(filename, index=False)
            dpg.set_value("status_text", f"✓ Exported to {filename}")
        except Exception as e:
            dpg.set_value("status_text", f"✗ Export error: {str(e)}")

    def clear_data(self):
        self.current_data = None
        dpg.delete_item("data_table", children_only=True)
        dpg.set_value("data_summary", "")
        dpg.set_value("status_text", "Data cleared")

    def _display_data(self, df: pd.DataFrame):
        dpg.delete_item("data_table", children_only=True)

        if df is None or df.empty:
            return

        # Add columns
        columns = list(df.columns)
        for col in columns:
            dpg.add_table_column(label=col, parent="data_table")

        # Add rows (limit to 20 for performance)
        display_df = df.head(20)

        for _, row in display_df.iterrows():
            with dpg.table_row(parent="data_table"):
                for col in columns:
                    value = str(row[col])
                    if len(value) > 15:
                        value = value[:12] + "..."
                    dpg.add_text(value)

    def _update_summary(self, df: pd.DataFrame):
        if df is None or df.empty:
            return

        summary = f"Rows: {len(df)}, Columns: {len(df.columns)}\n"
        summary += f"Columns: {', '.join(df.columns)}\n"

        if 'date' in df.columns:
            summary += f"Date range: {df['date'].min()} to {df['date'].max()}\n"

        if 'value' in df.columns:
            summary += f"Value range: {df['value'].min():.2f} to {df['value'].max():.2f}"

        dpg.set_value("data_summary", summary)

    def run(self):
        self.setup_gui()
        dpg.start_dearpygui()
        dpg.destroy_context()


if __name__ == "__main__":
    print("Starting Debug IMF Terminal...")
    print("This version includes extensive debug logging to identify connection issues.")
    app = DebugIMFGUI()
    app.run()