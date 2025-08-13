# -*- coding: utf-8 -*-
# oecd_data_tab.py

import dearpygui.dearpygui as dpg
import threading
import traceback
import uuid
import asyncio
import concurrent.futures
import multiprocessing
from datetime import datetime
from typing import Dict, Any, Optional, List
import json

from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import info, debug, warning, error, operation, monitor_performance


def _fetch_data_in_process(indicator_config, params):
    """Function to run in separate process for complete isolation"""
    try:
        import asyncio
        from fincept_terminal.DatabaseConnector.DataSources.oced_data.oced_provider import OECDProvider

        # Create new event loop
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)

        try:
            # Create fresh provider instance
            provider = OECDProvider()
            method = getattr(provider, indicator_config["method"])

            # Execute the async method
            result = loop.run_until_complete(method(**params))

            # Clean up
            if hasattr(provider, 'close'):
                loop.run_until_complete(provider.close())

            return result

        finally:
            loop.close()

    except Exception as e:
        return {"success": False, "error": str(e)}


class OECDDataTab(BaseTab):
    """OECD Economic Data tab for displaying economic indicators from OECD"""

    def __init__(self, app):
        super().__init__(app)
        self.tab_id = str(uuid.uuid4())[:8]

        # Thread pool for async operations
        self.executor = concurrent.futures.ThreadPoolExecutor(max_workers=2)

        # Process pool for complete isolation (recommended)
        self.process_executor = concurrent.futures.ProcessPoolExecutor(max_workers=2)

        # Initialize OECD provider
        try:
            from fincept_terminal.DatabaseConnector.DataSources.oced_data.oced_provider import OECDProvider
            self.oecd_provider = OECDProvider()
            print("✅ OECD Provider initialized successfully")
        except ImportError as e:
            error(f"Failed to import OECD provider: {e}", module="OECDDataTab")
            self.oecd_provider = None
            print(f"❌ OECD Provider import failed: {e}")

        # Import constants for country lists
        try:
            from fincept_terminal.DatabaseConnector.DataSources.oced_data.constants import (
                COUNTRY_TO_CODE_GDP, COUNTRY_TO_CODE_CPI, COUNTRY_TO_CODE_UNEMPLOYMENT,
                COUNTRY_TO_CODE_IR, COUNTRY_TO_CODE_CLI, COUNTRY_TO_CODE_SHARES,
                COUNTRY_TO_CODE_RGDP, COUNTRY_TO_CODE_GDP_FORECAST
            )
            self.constants = {
                "gdp": list(COUNTRY_TO_CODE_GDP.keys())[:20],
                "cpi": list(COUNTRY_TO_CODE_CPI.keys())[:20],
                "unemployment": list(COUNTRY_TO_CODE_UNEMPLOYMENT.keys())[:20],
                "interest_rates": list(COUNTRY_TO_CODE_IR.keys())[:20],
                "cli": list(COUNTRY_TO_CODE_CLI.keys())[:20],
                "shares": list(COUNTRY_TO_CODE_SHARES.keys())[:20],
                "housing": list(COUNTRY_TO_CODE_RGDP.keys())[:20],
                "forecast": list(COUNTRY_TO_CODE_GDP_FORECAST.keys())[:20]
            }
            print("✅ Constants imported successfully")
        except ImportError as e:
            # Fallback country list
            self.constants = {
                "gdp": ["united_states", "germany", "japan", "united_kingdom", "france", "italy",
                        "canada", "australia", "spain", "netherlands", "g7", "g20", "oecd", "all"]
            }
            print(f"⚠️ Using fallback constants: {e}")

        # Data storage
        self.current_data = {}
        self.last_refresh = None

        # OECD endpoints configuration with enhanced metadata
        self.indicators = {
            "GDP Nominal": {
                "method": "get_gdp_nominal",
                "params": ["countries", "frequency", "units", "price_base"],
                "countries_key": "gdp",
                "description": "Gross Domestic Product at market prices",
                "y_label": "GDP Value",
                "units_info": {
                    "level": "USD (Millions)",
                    "index": "Index (2015=100)",
                    "capita": "USD per Capita",
                    "growth": "Growth Rate (%)"
                }
            },
            "GDP Real": {
                "method": "get_gdp_real",
                "params": ["countries", "frequency"],
                "countries_key": "gdp",
                "description": "Real GDP (PPP-adjusted, constant prices)",
                "y_label": "Real GDP (USD PPP)",
                "units_info": {"default": "USD PPP (Millions)"}
            },
            "Consumer Price Index": {
                "method": "get_cpi",
                "params": ["countries", "frequency", "transform", "harmonized", "expenditure"],
                "countries_key": "cpi",
                "description": "Consumer Price Index - Inflation measure",
                "y_label": "CPI Value",
                "units_info": {
                    "index": "Index (2015=100)",
                    "yoy": "Year-over-Year (%)",
                    "mom": "Month-over-Month (%)"
                }
            },
            "Unemployment Rate": {
                "method": "get_unemployment",
                "params": ["countries", "frequency", "sex", "age", "seasonal_adjustment"],
                "countries_key": "unemployment",
                "description": "Unemployment rate as % of labor force",
                "y_label": "Unemployment Rate (%)",
                "units_info": {"default": "Percentage (%)"}
            },
            "Interest Rates": {
                "method": "get_interest_rates",
                "params": ["countries", "duration", "frequency"],
                "countries_key": "interest_rates",
                "description": "Interest rates by duration",
                "y_label": "Interest Rate (%)",
                "units_info": {"default": "Percentage (%)"}
            }
        }

        # Enhanced parameter options
        self.param_options = {
            "frequency": ["monthly", "quarter", "annual"],
            "units": ["level", "index", "capita", "volume", "current_prices", "growth", "deflator"],
            "price_base": ["current_prices", "volume"],
            "transform": ["index", "yoy", "mom", "period"],
            "expenditure": ["total", "food_non_alcoholic_beverages", "housing_water_electricity_gas",
                            "transport", "energy"],
            "sex": ["total", "male", "female"],
            "age": ["total", "15-24", "25+"],
            "duration": ["immediate", "short", "long"],
            "adjustment": ["amplitude", "normalized"]
        }

    def get_label(self):
        return "OECD"

    def get_countries_for_indicator(self, indicator: str) -> List[str]:
        """Get available countries for a specific indicator"""
        if indicator in self.indicators:
            countries_key = self.indicators[indicator].get("countries_key", "gdp")
            return self.constants.get(countries_key, self.constants.get("gdp", []))
        return self.constants.get("gdp", [])

    def format_number(self, value: float, unit_type: str = "default") -> str:
        """Format numbers for better readability"""
        try:
            if value is None or str(value).lower() == "nan":
                return "N/A"

            num_value = float(value)

            # Handle percentage values
            if "%" in unit_type or "percentage" in unit_type.lower():
                return f"{num_value:.2f}%"

            # Handle different scales
            abs_value = abs(num_value)
            if abs_value >= 1_000_000_000_000:  # Trillions
                return f"{num_value / 1_000_000_000_000:.2f}T"
            elif abs_value >= 1_000_000_000:  # Billions
                return f"{num_value / 1_000_000_000:.2f}B"
            elif abs_value >= 1_000_000:  # Millions
                return f"{num_value / 1_000_000:.2f}M"
            elif abs_value >= 1000:
                return f"{num_value:,.0f}"
            elif abs_value >= 1:
                return f"{num_value:.2f}"
            else:
                return f"{num_value:.4f}"

        except (ValueError, TypeError):
            return str(value) if value is not None else "N/A"

    def get_y_axis_label(self, indicator: str, params: Dict[str, Any]) -> str:
        """Generate appropriate Y-axis label based on indicator and parameters"""
        if indicator not in self.indicators:
            return "Value"

        config = self.indicators[indicator]
        base_label = config.get("y_label", "Value")
        units_info = config.get("units_info", {})

        # Get unit information from parameters
        unit_key = params.get("units", params.get("transform", "default"))
        unit_desc = units_info.get(unit_key, units_info.get("default", ""))

        if unit_desc:
            return f"{base_label} ({unit_desc})"
        return base_label

    def create_content(self):
        """Create the enhanced OECD data interface"""
        try:
            print("🔧 Creating enhanced OECD content...")

            # Enhanced header with description
            with dpg.group():
                dpg.add_text("🌍 OECD Economic Indicators", color=[100, 200, 255])
                dpg.add_text("Access comprehensive economic data from OECD countries with enhanced visualizations",
                             color=[180, 180, 180])

                # Add info about last refresh
                with dpg.group(horizontal=True):
                    dpg.add_text("Last Updated:", color=[150, 150, 150])
                    dpg.add_text("Not yet loaded", tag=f"last_update_{self.tab_id}", color=[120, 120, 120])

            dpg.add_spacer(height=15)

            # Check provider availability
            if not self.oecd_provider:
                dpg.add_text("❌ OECD Provider not available. Check import paths.", color=[255, 100, 100])
                return

            # Create main content area with better organization
            with dpg.child_window(height=750, border=True):

                # Enhanced controls section
                with dpg.collapsing_header(label="📊 Data Selection & Parameters", default_open=True):
                    dpg.add_spacer(height=5)

                    # Row 1: Indicator selection with description
                    with dpg.group():
                        with dpg.group(horizontal=True):
                            dpg.add_text("Economic Indicator:", color=[200, 200, 100])
                            dpg.add_combo(
                                list(self.indicators.keys()),
                                tag=f"indicator_{self.tab_id}",
                                default_value="GDP Nominal",
                                width=220,
                                callback=self.on_indicator_change
                            )

                        # Show indicator description
                        dpg.add_text("", tag=f"indicator_desc_{self.tab_id}",
                                     color=[160, 160, 160], wrap=600)

                    dpg.add_spacer(height=10)

                    # Row 2: Country and date selection
                    with dpg.group(horizontal=True):
                        dpg.add_text("Country/Region:")
                        initial_countries = self.get_countries_for_indicator("GDP Nominal")
                        dpg.add_combo(
                            initial_countries,
                            tag=f"countries_{self.tab_id}",
                            default_value=initial_countries[0] if initial_countries else "united_states",
                            width=150
                        )

                        dpg.add_spacer(width=30)
                        dpg.add_text("Start Date:")
                        dpg.add_input_text(
                            tag=f"start_date_{self.tab_id}",
                            default_value="2020-01-01",
                            width=110,
                            hint="YYYY-MM-DD"
                        )

                        dpg.add_spacer(width=15)
                        dpg.add_text("End Date:")
                        dpg.add_input_text(
                            tag=f"end_date_{self.tab_id}",
                            default_value="2024-12-31",
                            width=110,
                            hint="YYYY-MM-DD"
                        )

                    dpg.add_spacer(height=10)

                    # Dynamic parameters area
                    with dpg.group(tag=f"dynamic_params_{self.tab_id}"):
                        self.create_parameter_controls("GDP Nominal")

                    dpg.add_spacer(height=15)

                    # Enhanced action buttons
                    with dpg.group(horizontal=True):
                        dpg.add_button(
                            label="📈 Fetch Data",
                            callback=self.fetch_data,
                            width=130,
                            height=35
                        )
                        dpg.add_button(
                            label="🔄 Refresh",
                            callback=self.refresh_data,
                            width=100,
                            height=35
                        )
                        dpg.add_button(
                            label="🧹 Clear",
                            callback=self.clear_data,
                            width=90,
                            height=35
                        )
                        dpg.add_button(
                            label="📊 Export CSV",
                            callback=self.export_data,
                            width=130,
                            height=35
                        )

                    # Enhanced status section
                    dpg.add_spacer(height=10)
                    with dpg.group():
                        with dpg.group(horizontal=True):
                            dpg.add_text("Status:", color=[150, 150, 150])
                            dpg.add_text("Ready", tag=f"status_{self.tab_id}", color=[100, 255, 100])

                        # Data summary
                        dpg.add_text("", tag=f"data_summary_{self.tab_id}", color=[140, 140, 140])

                dpg.add_separator()
                dpg.add_spacer(height=5)

                # Enhanced display area with tabs
                with dpg.tab_bar():
                    # Enhanced Chart tab
                    with dpg.tab(label="📈 Interactive Chart"):
                        dpg.add_spacer(height=8)

                        # Chart controls with better layout
                        with dpg.group():
                            with dpg.group(horizontal=True):
                                dpg.add_text("Chart Type:")
                                dpg.add_combo(
                                    ["Line", "Bar", "Scatter"],
                                    tag=f"chart_type_{self.tab_id}",
                                    default_value="Line",
                                    width=100,
                                    callback=self.on_chart_type_change
                                )

                                dpg.add_spacer(width=20)
                                dpg.add_checkbox(
                                    label="Show Grid",
                                    tag=f"show_grid_{self.tab_id}",
                                    default_value=True,
                                    callback=self.update_chart
                                )

                                dpg.add_spacer(width=20)
                                dpg.add_checkbox(
                                    label="Auto-scale Y",
                                    tag=f"auto_scale_{self.tab_id}",
                                    default_value=True
                                )

                                dpg.add_spacer(width=30)
                                dpg.add_button(
                                    label="🔄 Refresh Chart",
                                    callback=self.update_chart,
                                    width=140
                                )

                        dpg.add_spacer(height=10)

                        # Enhanced chart plot
                        with dpg.plot(
                                tag=f"chart_plot_{self.tab_id}",
                                label="Economic Data Visualization",
                                height=400,
                                width=-1
                        ):
                            dpg.add_plot_legend()
                            dpg.add_plot_axis(dpg.mvXAxis, label="Time Period", tag=f"x_axis_{self.tab_id}")
                            dpg.add_plot_axis(dpg.mvYAxis, label="Value", tag=f"y_axis_{self.tab_id}")

                        # Chart information panel
                        with dpg.group():
                            dpg.add_text("Chart Information:", color=[150, 150, 200])
                            dpg.add_text("", tag=f"chart_info_{self.tab_id}", color=[130, 130, 130], wrap=800)

                    # Enhanced Table tab
                    with dpg.tab(label="📊 Data Table"):
                        dpg.add_spacer(height=8)

                        # Table controls
                        with dpg.group(horizontal=True):
                            dpg.add_text("Show rows:")
                            dpg.add_combo(
                                ["25", "50", "100", "All"],
                                tag=f"table_limit_{self.tab_id}",
                                default_value="50",
                                width=80,
                                callback=self.update_table
                            )

                            dpg.add_spacer(width=20)
                            dpg.add_text("Search:")
                            dpg.add_input_text(
                                tag=f"table_search_{self.tab_id}",
                                width=150,
                                hint="Filter data...",
                                callback=self.update_table
                            )

                        dpg.add_spacer(height=10)

                        # Enhanced table
                        with dpg.table(
                                tag=f"data_table_{self.tab_id}",
                                header_row=True,
                                resizable=True,
                                borders_innerH=True,
                                borders_innerV=True,
                                scrollY=True,
                                height=380,
                                sortable=True
                        ):
                            dpg.add_table_column(label="Date", width_fixed=True, init_width_or_weight=120)
                            dpg.add_table_column(label="Country", width_fixed=True, init_width_or_weight=140)
                            dpg.add_table_column(label="Value", width_fixed=True, init_width_or_weight=180)
                            dpg.add_table_column(label="Frequency", width_fixed=True, init_width_or_weight=100)
                            dpg.add_table_column(label="Indicator", width_fixed=True, init_width_or_weight=160)

                    # Enhanced Statistics tab
                    with dpg.tab(label="📈 Statistics"):
                        dpg.add_spacer(height=10)

                        with dpg.group():
                            dpg.add_text("Data Statistics & Analysis", color=[200, 200, 100])
                            dpg.add_spacer(height=10)

                            # Statistics display
                            with dpg.group(tag=f"stats_display_{self.tab_id}"):
                                dpg.add_text("No data loaded for analysis...", color=[140, 140, 140])

                    # Enhanced Raw data tab
                    with dpg.tab(label="🔍 Raw Data"):
                        dpg.add_spacer(height=8)

                        with dpg.group(horizontal=True):
                            dpg.add_button(
                                label="📋 Copy JSON",
                                callback=self.copy_raw_data,
                                width=130
                            )
                            dpg.add_button(
                                label="💾 Save JSON",
                                callback=self.save_raw_data,
                                width=130
                            )

                        dpg.add_spacer(height=10)

                        dpg.add_input_text(
                            tag=f"raw_display_{self.tab_id}",
                            multiline=True,
                            height=380,
                            width=-1,
                            readonly=True,
                            default_value="No data loaded..."
                        )

            print("✅ Enhanced OECD content created successfully")

        except Exception as e:
            error(f"Error creating OECD tab content: {str(e)}", module="OECDDataTab")
            print(f"❌ Error creating content: {str(e)}")
            print(f"❌ Traceback: {traceback.format_exc()}")
            dpg.add_text(f"Error: {str(e)}", color=[255, 100, 100])

    def create_parameter_controls(self, indicator: str):
        """Create dynamic parameter controls with better layout and error handling"""
        try:
            print(f"🔧 Creating enhanced parameters for {indicator}")

            # Ensure the parent container exists
            if not dpg.does_item_exist(f"dynamic_params_{self.tab_id}"):
                print(f"❌ Parent container dynamic_params_{self.tab_id} does not exist")
                return

            # Clear existing parameter controls
            children = dpg.get_item_children(f"dynamic_params_{self.tab_id}", 1)
            if children:
                for child in children:
                    try:
                        if dpg.does_item_exist(child):
                            dpg.delete_item(child)
                    except Exception as e:
                        print(f"⚠️ Error deleting child item {child}: {e}")

            if indicator not in self.indicators:
                print(f"❌ Indicator {indicator} not found in indicators")
                return

            # Update indicator description
            if dpg.does_item_exist(f"indicator_desc_{self.tab_id}"):
                desc = self.indicators[indicator].get("description", "Economic data indicator")
                dpg.set_value(f"indicator_desc_{self.tab_id}", f"📝 {desc}")

            params = self.indicators[indicator]["params"]

            # Filter out 'countries' param as it's handled in main controls
            filtered_params = [param for param in params if param != "countries"]

            if not filtered_params:
                # Add a message if no additional parameters
                dpg.add_text("No additional parameters required for this indicator",
                             parent=f"dynamic_params_{self.tab_id}",
                             color=[140, 140, 140])
                return

            # Create parameter controls in a more organized layout
            param_count = 0
            current_row = None

            for param in filtered_params:
                try:
                    # Create new row every 3 parameters
                    if param_count % 3 == 0:
                        current_row = dpg.add_group(horizontal=True, parent=f"dynamic_params_{self.tab_id}")
                        if not dpg.does_item_exist(current_row):
                            print(f"❌ Failed to create row group")
                            continue

                    # Create parameter group
                    param_group = dpg.add_group(horizontal=True, parent=current_row)
                    if not dpg.does_item_exist(param_group):
                        print(f"❌ Failed to create parameter group for {param}")
                        continue

                    # Add parameter label
                    label_text = f"{param.replace('_', ' ').title()}:"
                    dpg.add_text(label_text, parent=param_group)

                    # Add parameter control based on type
                    param_tag = f"{param}_{self.tab_id}"

                    if param in self.param_options:
                        # Dropdown for predefined options
                        dpg.add_combo(
                            self.param_options[param],
                            tag=param_tag,
                            default_value=self.param_options[param][0],
                            width=130,
                            parent=param_group
                        )
                    elif param in ["harmonized", "seasonal_adjustment", "growth_rate"]:
                        # Checkbox for boolean parameters
                        dpg.add_checkbox(
                            tag=param_tag,
                            default_value=False,
                            parent=param_group
                        )
                    else:
                        # Text input for other parameters
                        dpg.add_input_text(
                            tag=param_tag,
                            width=130,
                            hint=f"Enter {param}",
                            parent=param_group
                        )

                    # Add spacer between parameters in the same row (except for last item)
                    if param_count % 3 < 2 and param_count < len(filtered_params) - 1:
                        dpg.add_spacer(width=20, parent=current_row)

                    param_count += 1

                except Exception as param_error:
                    print(f"❌ Error creating control for parameter {param}: {param_error}")
                    continue

            print(f"✅ Enhanced parameters created for {indicator} ({param_count} parameters)")

        except Exception as e:
            error(f"Error creating parameter controls: {str(e)}", module="OECDDataTab")
            print(f"❌ Error creating parameters: {str(e)}")
            print(f"❌ Traceback: {traceback.format_exc()}")

            # Add fallback message
            try:
                if dpg.does_item_exist(f"dynamic_params_{self.tab_id}"):
                    dpg.add_text(f"Error loading parameters for {indicator}",
                                 parent=f"dynamic_params_{self.tab_id}",
                                 color=[255, 100, 100])
            except Exception as fallback_error:
                print(f"❌ Even fallback failed: {fallback_error}")

    def on_indicator_change(self, sender, app_data):
        """Handle indicator selection change with enhanced feedback"""
        try:
            print(f"🔧 Indicator changed to: {app_data}")

            # Update countries dropdown
            countries = self.get_countries_for_indicator(app_data)
            if dpg.does_item_exist(f"countries_{self.tab_id}"):
                dpg.configure_item(f"countries_{self.tab_id}", items=countries)
                if countries:
                    dpg.set_value(f"countries_{self.tab_id}", countries[0])

            # Update parameter controls
            self.create_parameter_controls(app_data)

            # Clear previous data and update status
            self.update_status("Indicator changed - ready for new data", [200, 200, 100])

        except Exception as e:
            error(f"Error in indicator change: {str(e)}", module="OECDDataTab")
            print(f"❌ Error in indicator change: {str(e)}")

    def on_chart_type_change(self, sender, app_data):
        """Handle chart type change"""
        try:
            if self.current_data:
                self.update_chart()
        except Exception as e:
            print(f"❌ Error in chart type change: {str(e)}")

    def fetch_data(self):
        """Fetch OECD data with enhanced error handling"""
        try:
            print("🔧 Starting enhanced data fetch...")

            if not self.oecd_provider:
                self.update_status("❌ OECD Provider not available", [255, 100, 100])
                return

            indicator = dpg.get_value(f"indicator_{self.tab_id}")
            if indicator not in self.indicators:
                self.update_status("❌ Invalid indicator selected", [255, 100, 100])
                return

            print(f"🔧 Fetching {indicator} data...")
            self.update_status("🔄 Fetching data from OECD...", [255, 255, 100])

            # Use process pool for complete isolation
            params = self.prepare_parameters(indicator)
            future = self.process_executor.submit(
                _fetch_data_in_process,
                self.indicators[indicator],
                params
            )

            # Monitor completion
            monitor_thread = threading.Thread(
                target=self._monitor_fetch_completion,
                args=(future, indicator),
                daemon=True
            )
            monitor_thread.start()

        except Exception as e:
            error(f"Error fetching data: {str(e)}", module="OECDDataTab")
            print(f"❌ Error fetching data: {str(e)}")
            self.update_status(f"❌ Error: {str(e)}", [255, 100, 100])

    def refresh_data(self):
        """Refresh current data"""
        try:
            if self.current_data:
                self.fetch_data()
            else:
                self.update_status("⚠️ No data to refresh - fetch data first", [255, 200, 100])
        except Exception as e:
            print(f"❌ Error refreshing data: {str(e)}")

    def _monitor_fetch_completion(self, future: concurrent.futures.Future, indicator: str):
        """Enhanced monitoring with better error handling"""
        try:
            # Wait for completion with timeout
            data = future.result(timeout=120)

            # Schedule UI update
            def update_ui():
                try:
                    if data.get("success"):
                        self.current_data = data
                        self.update_displays(data, indicator)
                        self.update_statistics(data)

                        data_count = len(data.get("data", []))
                        country = data.get("countries", "Unknown")
                        freq = data.get("frequency", "Unknown")

                        self.update_status(f"✅ Loaded {data_count} data points", [100, 255, 100])
                        self.update_data_summary(f"📊 {data_count} records • {country} • {freq} frequency")
                        self.last_refresh = datetime.now()

                        if dpg.does_item_exist(f"last_update_{self.tab_id}"):
                            dpg.set_value(f"last_update_{self.tab_id}", self.last_refresh.strftime("%Y-%m-%d %H:%M:%S"))
                            dpg.configure_item(f"last_update_{self.tab_id}", color=[100, 255, 100])
                    else:
                        error_msg = data.get("error", "Unknown error occurred")
                        print(f"❌ Data fetch failed: {error_msg}")
                        self.update_status(f"❌ {error_msg}", [255, 100, 100])
                        self.update_data_summary("No data available")

                except Exception as ui_error:
                    print(f"❌ Error updating UI: {ui_error}")
                    self.update_status(f"❌ UI Error: {str(ui_error)}", [255, 100, 100])

            threading.Timer(0.1, update_ui).start()

        except concurrent.futures.TimeoutError:
            print("❌ Fetch operation timed out")

            def timeout_update():
                self.update_status("❌ Request timed out (120s)", [255, 100, 100])

            threading.Timer(0.1, timeout_update).start()

        except Exception as e:
            print(f"❌ Error in fetch monitoring: {str(e)}")

            def error_update():
                self.update_status(f"❌ Monitoring error: {str(e)}", [255, 100, 100])

            threading.Timer(0.1, error_update).start()

    def prepare_parameters(self, indicator: str) -> Dict[str, Any]:
        """Prepare API parameters with validation"""
        try:
            params = {}

            # Get countries
            params["countries"] = dpg.get_value(f"countries_{self.tab_id}")

            # Get and validate date range
            start_date = dpg.get_value(f"start_date_{self.tab_id}")
            end_date = dpg.get_value(f"end_date_{self.tab_id}")

            if start_date and start_date.strip():
                params["start_date"] = start_date.strip()
            if end_date and end_date.strip():
                params["end_date"] = end_date.strip()

            # Get indicator-specific parameters
            for param in self.indicators[indicator]["params"]:
                if param == "countries":
                    continue

                param_tag = f"{param}_{self.tab_id}"
                if dpg.does_item_exist(param_tag):
                    value = dpg.get_value(param_tag)
                    if value is not None and (value != "" or isinstance(value, bool)):
                        params[param] = value

            return params

        except Exception as e:
            error(f"Error preparing parameters: {str(e)}", module="OECDDataTab")
            print(f"❌ Error preparing parameters: {str(e)}")
            return {"countries": "united_states"}

    def update_displays(self, data: Dict[str, Any], indicator: str):
        """Update all display components with enhanced formatting"""
        try:
            print("🔧 Updating enhanced displays...")
            self.update_chart(data=data, indicator=indicator)
            self.update_table(data=data)
            self.update_raw_display(data)
            self.update_chart_info(data, indicator)
            print("✅ Enhanced displays updated")

        except Exception as e:
            error(f"Error updating displays: {str(e)}", module="OECDDataTab")
            print(f"❌ Error updating displays: {str(e)}")

    def update_chart(self, sender=None, app_data=None, data=None, indicator=None):
        """Update chart with enhanced formatting and proper axis labels"""
        try:
            if data is None:
                data = self.current_data
            if not data or "data" not in data:
                print("❌ No data for chart")
                return

            chart_data = data["data"]
            if not isinstance(chart_data, list) or not chart_data:
                print("❌ Invalid chart data")
                return

            print(f"🔧 Updating enhanced chart with {len(chart_data)} data points")

            # Clear existing plots
            if dpg.does_item_exist(f"y_axis_{self.tab_id}"):
                children = dpg.get_item_children(f"y_axis_{self.tab_id}", 1)
                if children:
                    for child in children:
                        if dpg.does_item_exist(child):
                            dpg.delete_item(child)

            # Process and sort data by date
            plot_data = []
            date_labels = []

            for item in chart_data:
                value = item.get("value")
                date_str = item.get("date", "")

                if value is not None and str(value).lower() != "nan":
                    try:
                        float_value = float(value)
                        plot_data.append(float_value)
                        date_labels.append(date_str)
                    except (ValueError, TypeError):
                        continue

            if not plot_data:
                print("❌ No valid plot data")
                return

            # Create x-axis indices
            x_values = list(range(len(plot_data)))
            y_values = plot_data

            # Get chart configuration
            chart_type = dpg.get_value(f"chart_type_{self.tab_id}") if dpg.does_item_exist(
                f"chart_type_{self.tab_id}") else "Line"
            show_grid = dpg.get_value(f"show_grid_{self.tab_id}") if dpg.does_item_exist(
                f"show_grid_{self.tab_id}") else True

            # Prepare labels
            country = data.get("countries", "Data").replace("_", " ").title()
            indicator_name = data.get("indicator", indicator or "Economic Data").replace("_", " ").title()
            frequency = data.get("frequency", "").title()

            series_label = f"{country} - {indicator_name}"
            if frequency:
                series_label += f" ({frequency})"

            # Update axis labels
            if dpg.does_item_exist(f"x_axis_{self.tab_id}"):
                dpg.configure_item(f"x_axis_{self.tab_id}", label="Time Period")

            if dpg.does_item_exist(f"y_axis_{self.tab_id}"):
                # Get appropriate Y-axis label based on indicator and parameters
                current_indicator = dpg.get_value(f"indicator_{self.tab_id}") if dpg.does_item_exist(
                    f"indicator_{self.tab_id}") else indicator
                params = self.prepare_parameters(current_indicator) if current_indicator else {}
                y_label = self.get_y_axis_label(current_indicator, params)
                dpg.configure_item(f"y_axis_{self.tab_id}", label=y_label)

            # Plot data based on chart type
            if chart_type == "Line":
                dpg.add_line_series(
                    x_values,
                    y_values,
                    label=series_label,
                    parent=f"y_axis_{self.tab_id}"
                )
            elif chart_type == "Bar":
                dpg.add_bar_series(
                    x_values,
                    y_values,
                    label=series_label,
                    parent=f"y_axis_{self.tab_id}"
                )
            elif chart_type == "Scatter":
                dpg.add_scatter_series(
                    x_values,
                    y_values,
                    label=series_label,
                    parent=f"y_axis_{self.tab_id}"
                )

            # Configure grid
            if dpg.does_item_exist(f"chart_plot_{self.tab_id}"):
                if show_grid:
                    # Add grid (DearPyGui doesn't have direct grid, but we can configure axis ticks)
                    pass

            # Fit axes
            auto_scale = dpg.get_value(f"auto_scale_{self.tab_id}") if dpg.does_item_exist(
                f"auto_scale_{self.tab_id}") else True

            if auto_scale:
                if dpg.does_item_exist(f"x_axis_{self.tab_id}"):
                    dpg.fit_axis_data(f"x_axis_{self.tab_id}")
                if dpg.does_item_exist(f"y_axis_{self.tab_id}"):
                    dpg.fit_axis_data(f"y_axis_{self.tab_id}")

            print("✅ Enhanced chart updated successfully")

        except Exception as e:
            error(f"Error updating chart: {str(e)}", module="OECDDataTab")
            print(f"❌ Error updating chart: {str(e)}")

    def update_chart_info(self, data: Dict[str, Any], indicator: str):
        """Update chart information panel"""
        try:
            if not dpg.does_item_exist(f"chart_info_{self.tab_id}"):
                return

            chart_data = data.get("data", [])
            if not chart_data:
                dpg.set_value(f"chart_info_{self.tab_id}", "No data to display")
                return

            # Calculate basic statistics
            values = []
            for item in chart_data:
                try:
                    val = float(item.get("value"))
                    if str(val).lower() != "nan":
                        values.append(val)
                except:
                    continue

            if values:
                info_text = (f"Data Points: {len(values)} | "
                             f"Min: {self.format_number(min(values))} | "
                             f"Max: {self.format_number(max(values))} | "
                             f"Avg: {self.format_number(sum(values) / len(values))}")

                if len(chart_data) > 1:
                    date_range = f" | Period: {chart_data[0].get('date', 'N/A')} to {chart_data[-1].get('date', 'N/A')}"
                    info_text += date_range

                dpg.set_value(f"chart_info_{self.tab_id}", info_text)
            else:
                dpg.set_value(f"chart_info_{self.tab_id}", "No valid numerical data found")

        except Exception as e:
            print(f"❌ Error updating chart info: {str(e)}")

    def update_table(self, sender=None, app_data=None, data=None):
        """Update data table with enhanced formatting and search"""
        try:
            if data is None:
                data = self.current_data
            if not data or "data" not in data:
                return

            table_data = data["data"]
            if not isinstance(table_data, list):
                return

            print(f"🔧 Updating enhanced table with {len(table_data)} rows")

            # Clear existing rows
            if dpg.does_item_exist(f"data_table_{self.tab_id}"):
                children = dpg.get_item_children(f"data_table_{self.tab_id}", 1)
                if children:
                    for child in children:
                        if dpg.does_item_exist(child):
                            dpg.delete_item(child)

            # Get search filter
            search_term = ""
            if dpg.does_item_exist(f"table_search_{self.tab_id}"):
                search_term = dpg.get_value(f"table_search_{self.tab_id}").lower()

            # Filter data based on search
            filtered_data = table_data
            if search_term:
                filtered_data = []
                for item in table_data:
                    searchable_text = f"{item.get('country', '')} {item.get('date', '')} {item.get('value', '')}".lower()
                    if search_term in searchable_text:
                        filtered_data.append(item)

            # Get limit
            limit_str = dpg.get_value(f"table_limit_{self.tab_id}") if dpg.does_item_exist(
                f"table_limit_{self.tab_id}") else "50"
            limit = len(filtered_data) if limit_str == "All" else min(int(limit_str), len(filtered_data))

            # Get unit type for formatting
            current_indicator = dpg.get_value(f"indicator_{self.tab_id}") if dpg.does_item_exist(
                f"indicator_{self.tab_id}") else ""
            params = self.prepare_parameters(current_indicator) if current_indicator else {}
            unit_type = params.get("units", params.get("transform", "default"))

            # Add rows with enhanced formatting
            for i in range(min(limit, len(filtered_data))):
                item = filtered_data[i]

                with dpg.table_row(parent=f"data_table_{self.tab_id}"):
                    # Date
                    date_str = str(item.get("date", "N/A"))
                    dpg.add_text(date_str)

                    # Country
                    country_str = str(item.get("country", "N/A")).replace("_", " ").title()
                    dpg.add_text(country_str)

                    # Value with proper formatting
                    value = item.get("value")
                    formatted_value = self.format_number(value, unit_type)
                    dpg.add_text(formatted_value)

                    # Frequency
                    freq_str = str(item.get("FREQ", data.get("frequency", "N/A"))).title()
                    dpg.add_text(freq_str)

                    # Indicator
                    indicator_str = str(data.get("indicator", "N/A")).replace("_", " ").title()
                    dpg.add_text(indicator_str)

            print("✅ Enhanced table updated successfully")

        except Exception as e:
            error(f"Error updating table: {str(e)}", module="OECDDataTab")
            print(f"❌ Error updating table: {str(e)}")

    def update_statistics(self, data: Dict[str, Any]):
        """Update statistics panel with comprehensive analysis"""
        try:
            if not dpg.does_item_exist(f"stats_display_{self.tab_id}"):
                return

            # Clear existing statistics
            children = dpg.get_item_children(f"stats_display_{self.tab_id}", 1)
            if children:
                for child in children:
                    if dpg.does_item_exist(child):
                        dpg.delete_item(child)

            chart_data = data.get("data", [])
            if not chart_data:
                dpg.add_text("No data available for statistical analysis",
                             color=[140, 140, 140], parent=f"stats_display_{self.tab_id}")
                return

            # Extract numerical values
            values = []
            for item in chart_data:
                try:
                    val = float(item.get("value"))
                    if str(val).lower() != "nan":
                        values.append(val)
                except:
                    continue

            if not values:
                dpg.add_text("No valid numerical data for analysis",
                             color=[140, 140, 140], parent=f"stats_display_{self.tab_id}")
                return

            # Calculate statistics
            n = len(values)
            mean_val = sum(values) / n
            sorted_values = sorted(values)
            median_val = sorted_values[n // 2] if n % 2 == 1 else (sorted_values[n // 2 - 1] + sorted_values[
                n // 2]) / 2
            min_val = min(values)
            max_val = max(values)
            range_val = max_val - min_val

            # Standard deviation
            variance = sum((x - mean_val) ** 2 for x in values) / n
            std_dev = variance ** 0.5

            # Get unit type for formatting
            current_indicator = dpg.get_value(f"indicator_{self.tab_id}") if dpg.does_item_exist(
                f"indicator_{self.tab_id}") else ""
            params = self.prepare_parameters(current_indicator) if current_indicator else {}
            unit_type = params.get("units", params.get("transform", "default"))

            # Display statistics with proper formatting
            with dpg.group(parent=f"stats_display_{self.tab_id}"):
                dpg.add_text("📊 Descriptive Statistics", color=[200, 200, 100])
                dpg.add_spacer(height=5)

                with dpg.table(header_row=True, borders_innerH=True, borders_innerV=True):
                    dpg.add_table_column(label="Statistic", width_fixed=True, init_width_or_weight=150)
                    dpg.add_table_column(label="Value", width_fixed=True, init_width_or_weight=200)

                    stats = [
                        ("Count", str(n)),
                        ("Mean", self.format_number(mean_val, unit_type)),
                        ("Median", self.format_number(median_val, unit_type)),
                        ("Minimum", self.format_number(min_val, unit_type)),
                        ("Maximum", self.format_number(max_val, unit_type)),
                        ("Range", self.format_number(range_val, unit_type)),
                        ("Std Deviation", self.format_number(std_dev, unit_type))
                    ]

                    for stat_name, stat_value in stats:
                        with dpg.table_row():
                            dpg.add_text(stat_name)
                            dpg.add_text(stat_value)

                dpg.add_spacer(height=10)

                # Additional analysis
                dpg.add_text("📈 Trend Analysis", color=[200, 200, 100])
                dpg.add_spacer(height=5)

                if n > 1:
                    # Simple trend analysis
                    first_val = values[0]
                    last_val = values[-1]
                    change = last_val - first_val
                    change_pct = (change / first_val * 100) if first_val != 0 else 0

                    trend_text = "Increasing" if change > 0 else "Decreasing" if change < 0 else "Stable"
                    trend_color = [100, 255, 100] if change > 0 else [255, 100, 100] if change < 0 else [200, 200, 200]

                    dpg.add_text(f"Trend: {trend_text}", color=trend_color)
                    dpg.add_text(f"Total Change: {self.format_number(change, unit_type)}")
                    dpg.add_text(f"Percentage Change: {change_pct:.2f}%")

        except Exception as e:
            error(f"Error updating statistics: {str(e)}", module="OECDDataTab")
            print(f"❌ Error updating statistics: {str(e)}")

    def update_raw_display(self, data: Dict[str, Any]):
        """Update raw JSON display with better formatting"""
        try:
            formatted_json = json.dumps(data, indent=2, default=str)
            if dpg.does_item_exist(f"raw_display_{self.tab_id}"):
                dpg.set_value(f"raw_display_{self.tab_id}", formatted_json)

        except Exception as e:
            error(f"Error updating raw display: {str(e)}", module="OECDDataTab")

    def copy_raw_data(self):
        """Copy raw data to clipboard"""
        try:
            if self.current_data:
                formatted_json = json.dumps(self.current_data, indent=2, default=str)
                dpg.set_clipboard_text(formatted_json)
                self.update_status("📋 Data copied to clipboard", [100, 255, 100])
            else:
                self.update_status("⚠️ No data to copy", [255, 200, 100])

        except Exception as e:
            error(f"Error copying data: {str(e)}", module="OECDDataTab")

    def save_raw_data(self):
        """Save raw data to file"""
        try:
            if not self.current_data:
                self.update_status("⚠️ No data to save", [255, 200, 100])
                return

            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            indicator = self.current_data.get("indicator", "data")
            country = self.current_data.get("countries", "unknown")
            filename = f"oecd_{indicator}_{country}_{timestamp}.json"

            with open(filename, 'w') as f:
                json.dump(self.current_data, f, indent=2, default=str)

            self.update_status(f"💾 Data saved to {filename}", [100, 255, 100])

        except Exception as e:
            error(f"Error saving data: {str(e)}", module="OECDDataTab")
            self.update_status(f"❌ Save error: {str(e)}", [255, 100, 100])

    def export_data(self):
        """Export data to CSV with enhanced formatting"""
        try:
            if not self.current_data or "data" not in self.current_data:
                self.update_status("❌ No data to export", [255, 100, 100])
                return

            import csv
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            indicator = self.current_data.get("indicator", "data")
            country = self.current_data.get("countries", "unknown")
            filename = f"oecd_{indicator}_{country}_{timestamp}.csv"

            with open(filename, 'w', newline='', encoding='utf-8') as csvfile:
                writer = csv.writer(csvfile)

                # Enhanced headers
                writer.writerow([
                    "Date", "Country", "Value", "Indicator", "Frequency",
                    "Units", "Source", "Fetched_At"
                ])

                # Get metadata
                units = self.current_data.get("units", "N/A")
                frequency = self.current_data.get("frequency", "N/A")
                indicator_name = self.current_data.get("indicator", "N/A")
                fetched_at = self.current_data.get("fetched_at", "N/A")

                for item in self.current_data["data"]:
                    writer.writerow([
                        item.get("date", ""),
                        item.get("country", ""),
                        item.get("value", ""),
                        indicator_name,
                        item.get("FREQ", frequency),
                        units,
                        "OECD",
                        fetched_at
                    ])

            self.update_status(f"📊 Exported to {filename}", [100, 255, 100])

        except Exception as e:
            error(f"Error exporting data: {str(e)}", module="OECDDataTab")
            self.update_status(f"❌ Export error: {str(e)}", [255, 100, 100])

    def clear_data(self):
        """Clear all data and displays"""
        try:
            print("🔧 Clearing all data...")
            self.current_data = {}

            # Clear chart
            if dpg.does_item_exist(f"y_axis_{self.tab_id}"):
                children = dpg.get_item_children(f"y_axis_{self.tab_id}", 1)
                if children:
                    for child in children:
                        if dpg.does_item_exist(child):
                            dpg.delete_item(child)

            # Clear table
            if dpg.does_item_exist(f"data_table_{self.tab_id}"):
                children = dpg.get_item_children(f"data_table_{self.tab_id}", 1)
                if children:
                    for child in children:
                        if dpg.does_item_exist(child):
                            dpg.delete_item(child)

            # Clear statistics
            if dpg.does_item_exist(f"stats_display_{self.tab_id}"):
                children = dpg.get_item_children(f"stats_display_{self.tab_id}", 1)
                if children:
                    for child in children:
                        if dpg.does_item_exist(child):
                            dpg.delete_item(child)
                dpg.add_text("No data loaded for analysis...",
                             color=[140, 140, 140], parent=f"stats_display_{self.tab_id}")

            # Clear displays
            if dpg.does_item_exist(f"raw_display_{self.tab_id}"):
                dpg.set_value(f"raw_display_{self.tab_id}", "No data loaded...")

            if dpg.does_item_exist(f"chart_info_{self.tab_id}"):
                dpg.set_value(f"chart_info_{self.tab_id}", "No chart data available")

            # Reset status and summary
            self.update_status("🧹 Data cleared", [200, 200, 200])
            self.update_data_summary("Ready for new data")

            if dpg.does_item_exist(f"last_update_{self.tab_id}"):
                dpg.set_value(f"last_update_{self.tab_id}", "Not yet loaded")
                dpg.configure_item(f"last_update_{self.tab_id}", color=[120, 120, 120])

            print("✅ All data cleared successfully")

        except Exception as e:
            error(f"Error clearing data: {str(e)}", module="OECDDataTab")
            print(f"❌ Error clearing data: {str(e)}")

    def update_status(self, message: str, color: List[int] = None):
        """Update status message"""
        try:
            if color is None:
                color = [200, 200, 200]
            if dpg.does_item_exist(f"status_{self.tab_id}"):
                dpg.set_value(f"status_{self.tab_id}", message)
                dpg.configure_item(f"status_{self.tab_id}", color=color)
                print(f"📊 Status: {message}")

        except Exception as e:
            error(f"Error updating status: {str(e)}", module="OECDDataTab")
            print(f"❌ Error updating status: {str(e)}")

    def update_data_summary(self, summary: str):
        """Update data summary information"""
        try:
            if dpg.does_item_exist(f"data_summary_{self.tab_id}"):
                dpg.set_value(f"data_summary_{self.tab_id}", summary)

        except Exception as e:
            print(f"❌ Error updating data summary: {str(e)}")

    async def cleanup(self):
        """Clean up resources"""
        try:
            # Shutdown the thread pool
            if hasattr(self, 'executor'):
                self.executor.shutdown(wait=True)
                print("✅ Thread pool shutdown completed")

            # Shutdown the process pool
            if hasattr(self, 'process_executor'):
                self.process_executor.shutdown(wait=True)
                print("✅ Process pool shutdown completed")

            # Clean up OECD provider
            if hasattr(self, 'oecd_provider') and self.oecd_provider:
                await self.oecd_provider.close()
                print("✅ OECD provider cleanup completed")

            self.current_data = {}
            print("✅ OECD cleanup completed")

        except Exception as e:
            error(f"Error during cleanup: {str(e)}", module="OECDDataTab")
            print(f"❌ Error during cleanup: {str(e)}")

    def __del__(self):
        """Destructor to ensure cleanup"""
        try:
            if hasattr(self, 'executor'):
                self.executor.shutdown(wait=False)
            if hasattr(self, 'process_executor'):
                self.process_executor.shutdown(wait=False)
        except Exception:
            pass  # Ignore errors during destruction