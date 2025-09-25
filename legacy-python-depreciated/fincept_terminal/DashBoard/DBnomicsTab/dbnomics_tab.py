# -*- coding: utf-8 -*-
# dbnomics_tab.py

import dearpygui.dearpygui as dpg
import requests
import pandas as pd
import threading
from datetime import datetime
import json
import traceback
from fincept_terminal.utils.base_tab import BaseTab
from fincept_terminal.utils.Logging.logger import info, error, warning, debug, monitor_performance, operation


class DBnomicsTab(BaseTab):
    """DBnomics Data Explorer with Bloomberg Terminal styling"""

    def __init__(self, main_app=None, *args, **kwargs):
        super().__init__(main_app, *args, **kwargs)
        self.main_app = main_app

        info("Initializing DBnomics tab")

        # Bloomberg Terminal Colors
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 80, 80]
        self.BLOOMBERG_GREEN = [0, 255, 100]
        self.BLOOMBERG_YELLOW = [255, 255, 100]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [100, 180, 255]

        # API Setup with connection pooling
        self.base_url = "https://api.db.nomics.world/v22"
        self.session = requests.Session()
        self.session.headers.update({'User-Agent': 'Fincept-Terminal/1.0'})

        # Data storage with caching
        self.providers = []
        self.datasets = []
        self.series_list = []
        self.current_data = None
        self._cache = {}  # Response cache for performance

        # Current selections
        self.current_provider = None
        self.current_dataset = None
        self.current_series = None

        # View state
        self.view_mode = "table"

        # 6 data points for comparison
        self.data_points = [None, None, None, None, None, None]
        self.data_point_names = ["Empty", "Empty", "Empty", "Empty", "Empty", "Empty"]

        info("DBnomics tab initialization complete")

    def get_label(self, *args, **kwargs):
        return "DBnomics"

    @monitor_performance
    def create_content(self, *args, **kwargs):
        """Create content with proper error handling"""
        try:
            with operation("create_content"):
                # Header section
                with dpg.group(horizontal=True):
                    dpg.add_text("DBNOMICS", color=self.BLOOMBERG_ORANGE)
                    dpg.add_text("GLOBAL DATA TERMINAL", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(" | STATUS: READY", color=self.BLOOMBERG_GREEN)
                    dpg.add_text(" | PROVIDERS: 0", color=self.BLOOMBERG_YELLOW, tag="provider_count")

                dpg.add_separator()

                # Function buttons
                with dpg.group(horizontal=True):
                    dpg.add_button(label="LOAD PROVIDERS", width=120, callback=self.btn_load_providers)
                    dpg.add_button(label="REFRESH", width=80, callback=self.btn_refresh)
                    dpg.add_button(label="TABLE VIEW", width=100, callback=self.btn_table_view)
                    dpg.add_button(label="CHART VIEW", width=100, callback=self.btn_chart_view)
                    dpg.add_button(label="EXPORT", width=80, callback=self.btn_export)

                dpg.add_separator()

                # Main layout
                with dpg.group(horizontal=True):
                    # Left navigation panel
                    with dpg.child_window(width=400, height=900, border=True):
                        self.create_navigation_panel()

                    # Right data panel
                    with dpg.child_window(width=-1, height=900, border=True):
                        self.create_data_panel()

                info("DBnomics content creation completed successfully")

        except Exception as e:
            error("Error in create_content", context={"error": str(e)}, exc_info=True)
            # Create minimal error display
            dpg.add_text(f"ERROR: {str(e)}", color=self.BLOOMBERG_RED)

    def create_navigation_panel(self, *args, **kwargs):
        """Create navigation panel"""
        try:
            dpg.add_text("DATA NAVIGATION", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Providers section
            dpg.add_text("1. SELECT PROVIDER", color=self.BLOOMBERG_YELLOW)
            dpg.add_listbox([], tag="providers_listbox", width=380, num_items=6,
                            callback=self.on_provider_selected)
            dpg.add_separator()

            # Datasets section
            dpg.add_text("2. SELECT DATASET", color=self.BLOOMBERG_YELLOW)
            dpg.add_listbox([], tag="datasets_listbox", width=380, num_items=5,
                            callback=self.on_dataset_selected)
            dpg.add_separator()

            # Series section
            dpg.add_text("3. SELECT SERIES", color=self.BLOOMBERG_YELLOW)
            dpg.add_listbox([], tag="series_listbox", width=380, num_items=5,
                            callback=self.on_series_selected)
            dpg.add_separator()

            # Comparison slots
            dpg.add_text("4. COMPARISON SLOTS (6 TOTAL)", color=self.BLOOMBERG_YELLOW)

            # Slot buttons
            with dpg.group(horizontal=True):
                dpg.add_button(label="Add->1", width=60, callback=self.add_to_slot_0)
                dpg.add_button(label="Add->2", width=60, callback=self.add_to_slot_1)
                dpg.add_button(label="Add->3", width=60, callback=self.add_to_slot_2)

            with dpg.group(horizontal=True):
                dpg.add_button(label="Add->4", width=60, callback=self.add_to_slot_3)
                dpg.add_button(label="Add->5", width=60, callback=self.add_to_slot_4)
                dpg.add_button(label="Add->6", width=60, callback=self.add_to_slot_5)

            with dpg.group(horizontal=True):
                dpg.add_button(label="Clear All", width=120, callback=self.clear_all_slots)

            dpg.add_separator()

            # Slot status
            dpg.add_text("SLOT STATUS:", color=self.BLOOMBERG_GRAY)
            for i in range(6):
                with dpg.group(horizontal=True):
                    dpg.add_text(f"Slot {i + 1}:", color=self.BLOOMBERG_WHITE)
                    dpg.add_text("Empty", tag=f"slot_status_{i}", color=self.BLOOMBERG_GRAY)

            dpg.add_separator()
            dpg.add_text("STATUS:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("Ready", tag="main_status", color=self.BLOOMBERG_GREEN)

            debug("Navigation panel created successfully")

        except Exception as e:
            error("Error in create_navigation_panel", context={"error": str(e)}, exc_info=True)

    def create_data_panel(self, *args, **kwargs):
        """Create data panel"""
        try:
            # Header
            with dpg.group(horizontal=True):
                dpg.add_text("DATA DISPLAY", color=self.BLOOMBERG_ORANGE)
                dpg.add_spacer(width=200)
                dpg.add_text("Mode:", color=self.BLOOMBERG_GRAY)
                dpg.add_text("TABLE", tag="view_mode_display", color=self.BLOOMBERG_YELLOW)

            dpg.add_separator()

            # Tabs
            with dpg.tab_bar(tag="main_tabs"):
                # Single series tab
                with dpg.tab(label="SINGLE SERIES", tag="single_tab"):
                    with dpg.child_window(height=520, tag="single_data_area", horizontal_scrollbar=True):
                        dpg.add_text("Select a data series to view", color=self.BLOOMBERG_GRAY)

                # Comparison tab
                with dpg.tab(label="COMPARISON (6 SLOTS)", tag="comparison_tab"):
                    with dpg.child_window(height=520, tag="comparison_data_area", horizontal_scrollbar=True):
                        self.create_comparison_layout()

            debug("Data panel created successfully")

        except Exception as e:
            error("Error in create_data_panel", context={"error": str(e)}, exc_info=True)

    def create_comparison_layout(self, *args, **kwargs):
        """Create comparison layout"""
        try:
            dpg.add_text("COMPARISON DASHBOARD - 6 DATA SLOTS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Top row - Slots 1, 2, 3
            with dpg.group(horizontal=True):
                with dpg.child_window(width=450, height=240, border=True, tag="slot_area_0"):
                    dpg.add_text("SLOT 1: Empty", color=self.BLOOMBERG_GRAY)

                with dpg.child_window(width=450, height=240, border=True, tag="slot_area_1"):
                    dpg.add_text("SLOT 2: Empty", color=self.BLOOMBERG_GRAY)

                with dpg.child_window(width=450, height=240, border=True, tag="slot_area_2"):
                    dpg.add_text("SLOT 3: Empty", color=self.BLOOMBERG_GRAY)

            dpg.add_spacer(height=10)

            # Bottom row - Slots 4, 5, 6
            with dpg.group(horizontal=True):
                with dpg.child_window(width=450, height=240, border=True, tag="slot_area_3"):
                    dpg.add_text("SLOT 4: Empty", color=self.BLOOMBERG_GRAY)

                with dpg.child_window(width=450, height=240, border=True, tag="slot_area_4"):
                    dpg.add_text("SLOT 5: Empty", color=self.BLOOMBERG_GRAY)

                with dpg.child_window(width=450, height=240, border=True, tag="slot_area_5"):
                    dpg.add_text("SLOT 6: Empty", color=self.BLOOMBERG_GRAY)

            debug("Comparison layout created successfully")

        except Exception as e:
            error("Error in create_comparison_layout", context={"error": str(e)}, exc_info=True)

    # Individual slot callback methods
    def add_to_slot_0(self, *args, **kwargs):
        self.add_data_to_slot(0)

    def add_to_slot_1(self, *args, **kwargs):
        self.add_data_to_slot(1)

    def add_to_slot_2(self, *args, **kwargs):
        self.add_data_to_slot(2)

    def add_to_slot_3(self, *args, **kwargs):
        self.add_data_to_slot(3)

    def add_to_slot_4(self, *args, **kwargs):
        self.add_data_to_slot(4)

    def add_to_slot_5(self, *args, **kwargs):
        self.add_data_to_slot(5)

    # Button callbacks
    def btn_load_providers(self, *args, **kwargs):
        info("Load providers button clicked")
        self.update_status("Loading providers...")
        thread = threading.Thread(target=self.load_providers_thread, daemon=True)
        thread.start()

    def btn_refresh(self, *args, **kwargs):
        if self.current_series:
            info("Refresh button clicked", context={"current_series": self.current_series})
            self.update_status("Refreshing...")
            thread = threading.Thread(target=self.load_current_series_data, daemon=True)
            thread.start()
        else:
            warning("Refresh clicked but no series selected")
            self.update_status("No series selected")

    def btn_table_view(self, *args, **kwargs):
        debug("Table view selected")
        self.view_mode = "table"
        if dpg.does_item_exist("view_mode_display"):
            dpg.set_value("view_mode_display", "TABLE")
        self.refresh_displays()

    def btn_chart_view(self, *args, **kwargs):
        debug("Chart view selected")
        self.view_mode = "chart"
        if dpg.does_item_exist("view_mode_display"):
            dpg.set_value("view_mode_display", "CHART")
        self.refresh_displays()

    def btn_export(self, *args, **kwargs):
        info("Export button clicked")
        self.export_data()

    # API Methods with caching
    @monitor_performance
    def load_providers_thread(self, *args, **kwargs):
        try:
            with operation("load_providers"):
                # Check cache first
                cache_key = "providers"
                if cache_key in self._cache:
                    cached_time, cached_data = self._cache[cache_key]
                    if datetime.now().timestamp() - cached_time < 3600:  # 1 hour cache
                        self._process_providers_response(cached_data, from_cache=True)
                        return

                url = f"{self.base_url}/providers"
                debug(f"Requesting providers from: {url}")

                response = self.session.get(url, timeout=15)

                if response.status_code == 200:
                    data = response.json()
                    # Cache the response
                    self._cache[cache_key] = (datetime.now().timestamp(), data)
                    self._process_providers_response(data)
                else:
                    error("Failed to load providers", context={"status_code": response.status_code})
                    dpg.set_frame_callback(
                        dpg.get_frame_count() + 1,
                        lambda: self.update_status(f"Failed to load providers: {response.status_code}")
                    )

        except Exception as e:
            error("Error in load_providers_thread", context={"error": str(e)}, exc_info=True)
            dpg.set_frame_callback(
                dpg.get_frame_count() + 1,
                lambda: self.update_status(f"Provider error: {str(e)}")
            )

    def _process_providers_response(self, data, from_cache=False):
        """Process providers response data"""
        try:
            providers_data = data.get('providers', {}).get('docs', [])
            info(f"Found {len(providers_data)} providers", context={"from_cache": from_cache})

            self.providers = []
            provider_items = []

            for provider in providers_data:
                code = provider.get('code', '')
                name = provider.get('name', code)
                self.providers.append({'code': code, 'name': name})
                display_item = f"{code} - {name[:35]}"
                provider_items.append(display_item)

            cache_info = " (cached)" if from_cache else ""
            dpg.set_frame_callback(
                dpg.get_frame_count() + 1,
                lambda: self.update_providers_list(provider_items, cache_info)
            )

        except Exception as e:
            error("Error processing providers response", context={"error": str(e)}, exc_info=True)

    def update_providers_list(self, provider_items, cache_info="", *args, **kwargs):
        try:
            if dpg.does_item_exist("providers_listbox"):
                dpg.configure_item("providers_listbox", items=provider_items)
            else:
                error("providers_listbox does not exist")

            if dpg.does_item_exist("provider_count"):
                dpg.set_value("provider_count", f"PROVIDERS: {len(provider_items)}")

            self.update_status(f"Loaded {len(provider_items)} providers{cache_info}")
        except Exception as e:
            error("Error in update_providers_list", context={"error": str(e)}, exc_info=True)

    def on_provider_selected(self, sender, app_data, *args, **kwargs):
        try:
            selected_value = dpg.get_value("providers_listbox")

            if isinstance(selected_value, str):
                provider_code = selected_value.split(" - ")[0]
            elif isinstance(selected_value, int) and 0 <= selected_value < len(self.providers):
                provider_code = self.providers[selected_value]['code']
            else:
                warning(f"Invalid provider selection: {selected_value}")
                self.update_status("Invalid provider selection")
                return

            info(f"Selected provider: {provider_code}")
            self.current_provider = provider_code
            self.update_status(f"Loading datasets for {provider_code}...")

            # Clear downstream
            if dpg.does_item_exist("datasets_listbox"):
                dpg.configure_item("datasets_listbox", items=[])
            if dpg.does_item_exist("series_listbox"):
                dpg.configure_item("series_listbox", items=[])

            # Load datasets
            thread = threading.Thread(target=self.load_datasets_thread, args=(provider_code,), daemon=True)
            thread.start()

        except Exception as e:
            error("Error in on_provider_selected", context={"error": str(e)}, exc_info=True)
            self.update_status(f"Provider selection error: {str(e)}")

    @monitor_performance
    def load_datasets_thread(self, provider_code, *args, **kwargs):
        try:
            with operation(f"load_datasets_{provider_code}"):
                # Check cache first
                cache_key = f"datasets_{provider_code}"
                if cache_key in self._cache:
                    cached_time, cached_data = self._cache[cache_key]
                    if datetime.now().timestamp() - cached_time < 1800:  # 30 minutes cache
                        self._process_datasets_response(cached_data, provider_code, from_cache=True)
                        return

                url = f"{self.base_url}/datasets/{provider_code}"
                debug(f"Requesting datasets from: {url}")

                response = self.session.get(url, timeout=15)

                if response.status_code == 200:
                    data = response.json()
                    # Cache the response
                    self._cache[cache_key] = (datetime.now().timestamp(), data)
                    self._process_datasets_response(data, provider_code)
                else:
                    error("Failed to load datasets", context={
                        "provider_code": provider_code,
                        "status_code": response.status_code
                    })
                    dpg.set_frame_callback(
                        dpg.get_frame_count() + 1,
                        lambda: self.update_status(f"Failed to load datasets: {response.status_code}")
                    )

        except Exception as e:
            error("Error in load_datasets_thread", context={
                "provider_code": provider_code,
                "error": str(e)
            }, exc_info=True)
            dpg.set_frame_callback(
                dpg.get_frame_count() + 1,
                lambda: self.update_status(f"Dataset error: {str(e)}")
            )

    def _process_datasets_response(self, data, provider_code, from_cache=False):
        """Process datasets response data"""
        try:
            datasets_data = data.get('datasets', {}).get('docs', [])
            info(f"Found {len(datasets_data)} datasets for {provider_code}", context={"from_cache": from_cache})

            self.datasets = []
            dataset_items = []

            for dataset in datasets_data:
                code = dataset.get('code', '')
                name = dataset.get('name', code)
                self.datasets.append({'code': code, 'name': name})
                display_item = f"{code} - {name[:40]}"
                dataset_items.append(display_item)

            cache_info = " (cached)" if from_cache else ""
            dpg.set_frame_callback(
                dpg.get_frame_count() + 1,
                lambda: self.update_datasets_list(dataset_items, cache_info)
            )

        except Exception as e:
            error("Error processing datasets response", context={"provider_code": provider_code, "error": str(e)}, exc_info=True)

    def update_datasets_list(self, dataset_items, cache_info="", *args, **kwargs):
        try:
            if dpg.does_item_exist("datasets_listbox"):
                dpg.configure_item("datasets_listbox", items=dataset_items)
                debug("Datasets listbox updated")
            else:
                error("datasets_listbox does not exist")
            self.update_status(f"Loaded {len(dataset_items)} datasets{cache_info}")
        except Exception as e:
            error("Error in update_datasets_list", context={"error": str(e)}, exc_info=True)

    def on_dataset_selected(self, sender, app_data, *args, **kwargs):
        try:
            selected_value = dpg.get_value("datasets_listbox")

            if isinstance(selected_value, str):
                dataset_code = selected_value.split(" - ")[0]
            elif isinstance(selected_value, int) and 0 <= selected_value < len(self.datasets):
                dataset_code = self.datasets[selected_value]['code']
            else:
                warning(f"Invalid dataset selection: {selected_value}")
                self.update_status("Invalid dataset selection")
                return

            info(f"Selected dataset: {dataset_code}")
            self.current_dataset = dataset_code
            self.update_status(f"Loading series for {dataset_code}...")

            # Clear series
            if dpg.does_item_exist("series_listbox"):
                dpg.configure_item("series_listbox", items=[])

            # Load series
            thread = threading.Thread(target=self.load_series_thread,
                                    args=(self.current_provider, dataset_code), daemon=True)
            thread.start()

        except Exception as e:
            error("Error in on_dataset_selected", context={"error": str(e)}, exc_info=True)
            self.update_status(f"Dataset selection error: {str(e)}")

    @monitor_performance
    def load_series_thread(self, provider_code, dataset_code, *args, **kwargs):
        try:
            with operation(f"load_series_{provider_code}_{dataset_code}"):
                # Check cache first
                cache_key = f"series_{provider_code}_{dataset_code}"
                if cache_key in self._cache:
                    cached_time, cached_data = self._cache[cache_key]
                    if datetime.now().timestamp() - cached_time < 1800:  # 30 minutes cache
                        self._process_series_response(cached_data, provider_code, from_cache=True)
                        return

                url = f"{self.base_url}/series/{provider_code}/{dataset_code}"
                params = {'limit': 50, 'observations': 'false'}
                debug(f"Requesting series from: {url}")

                response = self.session.get(url, params=params, timeout=15)

                if response.status_code == 200:
                    data = response.json()
                    # Cache the response
                    self._cache[cache_key] = (datetime.now().timestamp(), data)
                    self._process_series_response(data, provider_code)
                else:
                    error("Failed to load series", context={
                        "provider_code": provider_code,
                        "dataset_code": dataset_code,
                        "status_code": response.status_code
                    })
                    dpg.set_frame_callback(
                        dpg.get_frame_count() + 1,
                        lambda: self.update_status(f"Failed to load series: {response.status_code}")
                    )

        except Exception as e:
            error("Error in load_series_thread", context={
                "provider_code": provider_code,
                "dataset_code": dataset_code,
                "error": str(e)
            }, exc_info=True)
            dpg.set_frame_callback(
                dpg.get_frame_count() + 1,
                lambda: self.update_status(f"Series error: {str(e)}")
            )

    def _process_series_response(self, data, provider_code, from_cache=False):
        """Process series response data"""
        try:
            series_data = data.get('series', {}).get('docs', [])
            info(f"Found {len(series_data)} series", context={"from_cache": from_cache})

            self.series_list = []
            series_items = []

            for series in series_data:
                code = series.get('series_code', '')
                name = series.get('series_name', code)
                full_id = f"{provider_code}/{self.current_dataset}/{code}"

                self.series_list.append({
                    'code': code,
                    'name': name,
                    'full_id': full_id
                })
                display_item = f"{code} - {name[:45]}"
                series_items.append(display_item)

            cache_info = " (cached)" if from_cache else ""
            dpg.set_frame_callback(
                dpg.get_frame_count() + 1,
                lambda: self.update_series_list(series_items, cache_info)
            )

        except Exception as e:
            error("Error processing series response", context={"provider_code": provider_code, "error": str(e)}, exc_info=True)

    def update_series_list(self, series_items, cache_info="", *args, **kwargs):
        try:
            if dpg.does_item_exist("series_listbox"):
                dpg.configure_item("series_listbox", items=series_items)
                debug("Series listbox updated")
            else:
                error("series_listbox does not exist")
            self.update_status(f"Loaded {len(series_items)} series{cache_info}")
        except Exception as e:
            error("Error in update_series_list", context={"error": str(e)}, exc_info=True)

    def on_series_selected(self, sender, app_data, *args, **kwargs):
        try:
            selected_value = dpg.get_value("series_listbox")

            if isinstance(selected_value, str):
                series_code = selected_value.split(" - ")[0]
                selected_series = None
                for series in self.series_list:
                    if series['code'] == series_code:
                        selected_series = series
                        break
                if not selected_series:
                    warning(f"Series not found: {series_code}")
                    self.update_status("Series not found")
                    return
            elif isinstance(selected_value, int) and 0 <= selected_value < len(self.series_list):
                selected_series = self.series_list[selected_value]
            else:
                warning(f"Invalid series selection: {selected_value}")
                self.update_status("Invalid series selection")
                return

            info(f"Selected series: {selected_series['full_id']}")
            self.current_series = selected_series['full_id']
            self.update_status(f"Loading data for {selected_series['name'][:30]}...")

            # Load data
            thread = threading.Thread(target=self.load_series_data_thread,
                                    args=(selected_series['full_id'], selected_series['name']), daemon=True)
            thread.start()

        except Exception as e:
            error("Error in on_series_selected", context={"error": str(e)}, exc_info=True)
            self.update_status(f"Series selection error: {str(e)}")

    @monitor_performance
    def load_series_data_thread(self, full_series_id, series_name, *args, **kwargs):
        try:
            with operation(f"load_series_data_{full_series_id}"):
                provider_code, dataset_code, series_code = full_series_id.split('/')
                url = f"{self.base_url}/series/{provider_code}/{dataset_code}/{series_code}"
                params = {'observations': '1', 'format': 'json'}
                debug(f"Requesting data: {url}")

                response = self.session.get(url, params=params, timeout=20)

                if response.status_code == 200:
                    data = response.json()
                    series_docs = data.get('series', {}).get('docs', [])

                    if series_docs:
                        first_series = series_docs[0]
                        observations = []

                        if 'period' in first_series and 'value' in first_series:
                            periods = first_series['period']
                            values = first_series['value']

                            for i in range(min(len(periods), len(values))):
                                if values[i] is not None:
                                    observations.append({
                                        'period': periods[i],
                                        'value': values[i]
                                    })
                        elif 'observations' in first_series:
                            observations = first_series['observations']

                        if observations:
                            info(f"Loaded {len(observations)} observations for {series_name}")
                            self.current_data = {
                                'series_id': full_series_id,
                                'series_name': series_name,
                                'observations': observations
                            }

                            dpg.set_frame_callback(
                                dpg.get_frame_count() + 1,
                                lambda: self.display_current_data()
                            )
                            return

                warning("No data found for series", context={"series_id": full_series_id})
                dpg.set_frame_callback(
                    dpg.get_frame_count() + 1,
                    lambda: self.update_status("No data found")
                )

        except Exception as e:
            error("Error in load_series_data_thread", context={
                "series_id": full_series_id,
                "error": str(e)
            }, exc_info=True)
            dpg.set_frame_callback(
                dpg.get_frame_count() + 1,
                lambda: self.update_status(f"Data load error: {str(e)}")
            )

    def load_current_series_data(self, *args, **kwargs):
        if self.current_series:
            parts = self.current_series.split('/')
            if len(parts) == 3:
                self.load_series_data_thread(self.current_series, "Current Series")

    def display_current_data(self, *args, **kwargs):
        if not self.current_data:
            warning("No current data to display")
            return

        observations = self.current_data['observations']
        series_name = self.current_data['series_name']
        info(f"Displaying {len(observations)} observations for {series_name}")

        self.update_status(f"Loaded {len(observations)} data points")
        self.show_single_series_data(observations, series_name)

    def show_single_series_data(self, observations, series_name, *args, **kwargs):
        if not dpg.does_item_exist("single_data_area"):
            error("single_data_area does not exist")
            return

        try:
            # Clear area
            dpg.delete_item("single_data_area", children_only=True)

            if self.view_mode == "table":
                self.create_single_table(observations, series_name)
            else:
                self.create_single_chart(observations, series_name)

        except Exception as e:
            error("Error in show_single_series_data", context={"error": str(e)}, exc_info=True)

    def create_single_table(self, observations, series_name, *args, **kwargs):
        try:
            with operation(f"create_table_{len(observations)}_rows"):
                table_id = dpg.add_table(header_row=True, borders_innerH=True, borders_outerH=True,
                                        height=480, scrollY=True, parent="single_data_area")

                dpg.add_table_column(label="Date", width_fixed=True, init_width_or_weight=120, parent=table_id)
                dpg.add_table_column(label="Value", width_fixed=True, init_width_or_weight=150, parent=table_id)
                dpg.add_table_column(label="Series", width_stretch=True, parent=table_id)

                df = pd.DataFrame(observations)

                if not df.empty:
                    df['period'] = pd.to_datetime(df['period'], errors='coerce')
                    df['value'] = pd.to_numeric(df['value'], errors='coerce')
                    df = df.dropna().sort_values('period')

                    row_count = 0
                    for _, row in df.iterrows():
                        with dpg.table_row(parent=table_id):
                            dpg.add_text(row['period'].strftime('%Y-%m-%d'))
                            dpg.add_text(f"{row['value']:.6f}")
                            dpg.add_text(series_name)
                        row_count += 1

                    info(f"Table created with {row_count} rows")
                else:
                    warning("DataFrame is empty")

        except Exception as e:
            error("Error in create_single_table", context={"error": str(e)}, exc_info=True)
            dpg.add_text(f"Table error: {str(e)}", color=self.BLOOMBERG_RED, parent="single_data_area")

    def create_single_chart(self, observations, series_name, *args, **kwargs):
        try:
            with operation(f"create_chart_{len(observations)}_points"):
                df = pd.DataFrame(observations)

                if not df.empty:
                    df['period'] = pd.to_datetime(df['period'], errors='coerce')
                    df['value'] = pd.to_numeric(df['value'], errors='coerce')
                    df = df.dropna().sort_values('period')

                    if len(df) > 0:
                        timestamps = list(range(len(df)))
                        values = df['value'].tolist()

                        with dpg.plot(label=series_name, height=480, width=-1, parent="single_data_area"):
                            dpg.add_plot_legend()
                            dpg.add_plot_axis(dpg.mvXAxis, label="Time Points")
                            y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Value")
                            dpg.add_line_series(timestamps, values, label=series_name[:40], parent=y_axis)

                        info("Chart created successfully")
                    else:
                        warning("No valid data for chart")
                        dpg.add_text("No valid data", color=self.BLOOMBERG_RED, parent="single_data_area")
                else:
                    warning("DataFrame is empty for chart")
                    dpg.add_text("No data available", color=self.BLOOMBERG_RED, parent="single_data_area")

        except Exception as e:
            error("Error in create_single_chart", context={"error": str(e)}, exc_info=True)
            dpg.add_text(f"Chart error: {str(e)}", color=self.BLOOMBERG_RED, parent="single_data_area")

    # Slot Management
    def add_data_to_slot(self, slot_index, *args, **kwargs):
        if not self.current_data:
            warning("No current data to add to slot")
            self.update_status("No data to add")
            return

        if 0 <= slot_index < 6:
            self.data_points[slot_index] = self.current_data.copy()
            short_name = self.current_data['series_name'][:25]
            self.data_point_names[slot_index] = short_name
            info(f"Added data to slot {slot_index}: {short_name}")

            # Update status display
            if dpg.does_item_exist(f"slot_status_{slot_index}"):
                dpg.set_value(f"slot_status_{slot_index}", short_name)
                dpg.configure_item(f"slot_status_{slot_index}", color=self.BLOOMBERG_GREEN)
            else:
                error(f"slot_status_{slot_index} does not exist")

            self.update_comparison_display()
            self.update_status(f"Added to slot {slot_index + 1}")

    def clear_all_slots(self, *args, **kwargs):
        try:
            info("Clearing all slots")
            for i in range(6):
                self.data_points[i] = None
                self.data_point_names[i] = "Empty"

                if dpg.does_item_exist(f"slot_status_{i}"):
                    dpg.set_value(f"slot_status_{i}", "Empty")
                    dpg.configure_item(f"slot_status_{i}", color=self.BLOOMBERG_GRAY)

            self.update_comparison_display()
            self.update_status("All slots cleared")
            info("All slots cleared successfully")
        except Exception as e:
            error("Error in clear_all_slots", context={"error": str(e)}, exc_info=True)

    def update_comparison_display(self, *args, **kwargs):
        try:
            with operation("update_comparison_display"):
                for slot_index in range(6):
                    slot_area_id = f"slot_area_{slot_index}"

                    if dpg.does_item_exist(slot_area_id):
                        dpg.delete_item(slot_area_id, children_only=True)

                        data_point = self.data_points[slot_index]

                        if data_point is None:
                            dpg.add_text(f"SLOT {slot_index + 1}: Empty",
                                        color=self.BLOOMBERG_GRAY, parent=slot_area_id)
                        else:
                            series_name = data_point['series_name']
                            observations = data_point['observations']

                            dpg.add_text(f"SLOT {slot_index + 1}: {series_name[:30]}",
                                        color=self.BLOOMBERG_YELLOW, parent=slot_area_id)
                            dpg.add_separator(parent=slot_area_id)

                            if self.view_mode == "table":
                                self.create_slot_table(observations, slot_area_id)
                            else:
                                self.create_slot_chart(observations, series_name, slot_area_id)
                    else:
                        error(f"slot_area_{slot_index} does not exist")

                debug("Comparison display update completed")
        except Exception as e:
            error("Error in update_comparison_display", context={"error": str(e)}, exc_info=True)

    def create_slot_table(self, observations, parent_id, *args, **kwargs):
        try:
            table_id = dpg.add_table(header_row=True, borders_innerH=True, borders_outerH=True,
                                    height=180, scrollY=True, parent=parent_id)

            dpg.add_table_column(label="Date", width_fixed=True, init_width_or_weight=100, parent=table_id)
            dpg.add_table_column(label="Value", width_stretch=True, parent=table_id)

            df = pd.DataFrame(observations)
            if not df.empty:
                df['period'] = pd.to_datetime(df['period'], errors='coerce')
                df['value'] = pd.to_numeric(df['value'], errors='coerce')
                df = df.dropna().sort_values('period')

                # Show last 10 rows
                display_data = df.tail(10)

                for _, row in display_data.iterrows():
                    with dpg.table_row(parent=table_id):
                        dpg.add_text(row['period'].strftime('%Y-%m-%d'))
                        dpg.add_text(f"{row['value']:.4f}")

        except Exception as e:
            error("Error in create_slot_table", context={"error": str(e)}, exc_info=True)
            dpg.add_text(f"Table error: {str(e)}", color=self.BLOOMBERG_RED, parent=parent_id)

    def create_slot_chart(self, observations, series_name, parent_id, *args, **kwargs):
        try:
            df = pd.DataFrame(observations)
            if not df.empty:
                df['period'] = pd.to_datetime(df['period'], errors='coerce')
                df['value'] = pd.to_numeric(df['value'], errors='coerce')
                df = df.dropna().sort_values('period')

                if len(df) > 0:
                    timestamps = list(range(len(df)))
                    values = df['value'].tolist()

                    with dpg.plot(label=series_name[:20], height=180, width=420, parent=parent_id):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                        y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Value")
                        dpg.add_line_series(timestamps, values, label=series_name[:25], parent=y_axis)
                else:
                    dpg.add_text("No valid data", color=self.BLOOMBERG_RED, parent=parent_id)
            else:
                dpg.add_text("No data available", color=self.BLOOMBERG_RED, parent=parent_id)

        except Exception as e:
            error("Error in create_slot_chart", context={"error": str(e)}, exc_info=True)
            dpg.add_text(f"Chart error: {str(e)}", color=self.BLOOMBERG_RED, parent=parent_id)

    # Utility Methods
    def refresh_displays(self, *args, **kwargs):
        try:
            debug("Refreshing displays")
            if self.current_data:
                self.display_current_data()
            self.update_comparison_display()
            debug("Display refresh completed")
        except Exception as e:
            error("Error in refresh_displays", context={"error": str(e)}, exc_info=True)

    def update_status(self, message, *args, **kwargs):
        try:
            if dpg.does_item_exist("main_status"):
                dpg.set_value("main_status", message)
                if "Loaded" in message or "Added" in message or "cleared" in message:
                    dpg.configure_item("main_status", color=self.BLOOMBERG_GREEN)
                elif "error" in message.lower() or "failed" in message.lower():
                    dpg.configure_item("main_status", color=self.BLOOMBERG_RED)
                else:
                    dpg.configure_item("main_status", color=self.BLOOMBERG_WHITE)
            else:
                error("main_status does not exist")
        except Exception as e:
            error("Error in update_status", context={"error": str(e)}, exc_info=True)

    @monitor_performance
    def export_data(self, *args, **kwargs):
        try:
            if not self.current_data:
                warning("No current data to export")
                self.update_status("No data to export")
                return

            with operation("export_data"):
                df = pd.DataFrame(self.current_data['observations'])
                if not df.empty:
                    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
                    filename = f"dbnomics_export_{timestamp}.csv"
                    df.to_csv(filename, index=False)
                    info(f"Data exported to: {filename}", context={
                        "rows": len(df),
                        "filename": filename
                    })
                    self.update_status(f"Exported: {filename}")
                else:
                    warning("No data to export - DataFrame is empty")
                    self.update_status("No data to export")
        except Exception as e:
            error("Error in export_data", context={"error": str(e)}, exc_info=True)
            self.update_status(f"Export failed: {str(e)}")

    def cleanup(self, *args, **kwargs):
        try:
            info("Starting DBnomics tab cleanup")
            self.session.close()
            self._cache.clear()  # Clear cache to free memory
            info("DBnomics tab cleanup completed")
        except Exception as e:
            error("Error in cleanup", context={"error": str(e)}, exc_info=True)