import dearpygui.dearpygui as dpg
import requests
import pandas as pd
import asyncio
import threading
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta
import json
from base_tab import BaseTab


class DBnomicsTab(BaseTab):
    """DBnomics Economic Data Explorer - Query and visualize economic data from DBnomics API"""

    def __init__(self, app):
        super().__init__(app)
        self.base_url = "https://api.db.nomics.world/v22"
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0',
            'Accept': 'application/json'
        })

        # Cache and state management
        self.providers_cache = {}
        self.datasets_cache = {}
        self.series_cache = {}
        self.search_cache = {}
        self.current_data = None
        self.selected_series = []

        # Threading
        self.executor = ThreadPoolExecutor(max_workers=4)
        self._loading = False

        # Pagination
        self.current_page = 1
        self.items_per_page = 50
        self.total_items = 0

    def get_label(self):
        return "Economic Data"

    def create_content(self):
        """Create the main interface for DBnomics data explorer"""
        self.add_section_header("DBnomics Economic Data Explorer")

        # Create main tab bar
        dpg.add_tab_bar(tag="dbnomics_main_tabs", callback=self._on_main_tab_change)

        # Search & Browse Tab
        with dpg.tab(label="Search & Browse", tag="tab_search", parent="dbnomics_main_tabs"):
            self._create_search_interface()

        # Data Viewer Tab
        with dpg.tab(label="Data Viewer", tag="tab_viewer", parent="dbnomics_main_tabs"):
            self._create_data_viewer()

        # Charts Tab
        with dpg.tab(label="Charts", tag="tab_charts", parent="dbnomics_main_tabs"):
            self._create_charts_interface()

        # Export Tab
        with dpg.tab(label="Export", tag="tab_export", parent="dbnomics_main_tabs"):
            self._create_export_interface()

    def _create_search_interface(self):
        """Create the search and browse interface"""
        # Search controls
        with dpg.group(horizontal=True):
            dpg.add_input_text(
                label="Search Terms",
                tag="search_input",
                width=300,
                callback=self._on_search_input
            )
            dpg.add_button(label="Search", callback=self._on_search_click)
            dpg.add_button(label="Clear", callback=self._on_clear_search)

        dpg.add_text("Examples: GDP, inflation, unemployment, interest rates", color=[150, 150, 150])

        # Filters
        with dpg.collapsing_header(label="Filters", default_open=True):
            with dpg.group(horizontal=True):
                dpg.add_combo(
                    label="Provider",
                    tag="provider_filter",
                    items=["All Providers"],
                    default_value="All Providers",
                    width=200,
                    callback=self._on_provider_change
                )
                dpg.add_combo(
                    label="Frequency",
                    tag="frequency_filter",
                    items=["All", "annual", "quarterly", "monthly", "daily"],
                    default_value="All",
                    width=150
                )
                dpg.add_combo(
                    label="Geo",
                    tag="geo_filter",
                    items=["All Countries"],
                    default_value="All Countries",
                    width=200
                )

        # Status and pagination
        with dpg.group(horizontal=True):
            dpg.add_text("Ready", tag="search_status")
            dpg.add_spacer(width=50)
            dpg.add_button(label="◀ Prev", tag="prev_page", callback=self._on_prev_page, enabled=False)
            dpg.add_text("Page 1", tag="page_info")
            dpg.add_button(label="Next ▶", tag="next_page", callback=self._on_next_page, enabled=False)

        dpg.add_separator()

        # Results section
        dpg.add_text("Search Results", color=[200, 200, 255])

        # Results table
        with dpg.table(
                tag="search_results_table",
                header_row=True,
                row_background=True,
                borders_innerH=True,
                borders_outerH=True,
                borders_innerV=True,
                borders_outerV=True,
                resizable=True,
                policy=dpg.mvTable_SizingFixedFit,
                height=400,
                width=-1
        ):
            dpg.add_table_column(label="Select", width_fixed=True, init_width_or_weight=60)
            dpg.add_table_column(label="Provider", width_fixed=True, init_width_or_weight=100)
            dpg.add_table_column(label="Dataset", width_fixed=True, init_width_or_weight=150)
            dpg.add_table_column(label="Series Name", width_fixed=True, init_width_or_weight=300)
            dpg.add_table_column(label="Frequency", width_fixed=True, init_width_or_weight=100)
            dpg.add_table_column(label="Start", width_fixed=True, init_width_or_weight=100)
            dpg.add_table_column(label="End", width_fixed=True, init_width_or_weight=100)
            dpg.add_table_column(label="Last Update", width_fixed=True, init_width_or_weight=120)

        # Action buttons
        with dpg.group(horizontal=True):
            dpg.add_button(label="Load Selected Series", callback=self._on_load_selected)
            dpg.add_button(label="View Details", callback=self._on_view_details)
            dpg.add_button(label="Add to Watchlist", callback=self._on_add_watchlist)

    def _create_data_viewer(self):
        """Create the data viewing interface"""
        dpg.add_text("Loaded Series Data", color=[200, 200, 255])

        # Series selection
        with dpg.group(horizontal=True):
            dpg.add_combo(
                label="Selected Series",
                tag="loaded_series_combo",
                items=["No series loaded"],
                width=400,
                callback=self._on_series_selection_change
            )
            dpg.add_button(label="Refresh", callback=self._on_refresh_series)
            dpg.add_button(label="Remove", callback=self._on_remove_series)

        # Data transformation options
        with dpg.collapsing_header(label="Data Transformations", default_open=False):
            with dpg.group(horizontal=True):
                dpg.add_combo(
                    label="Transform",
                    tag="transform_combo",
                    items=["None", "Percentage Change", "Log", "Difference", "Moving Average"],
                    default_value="None",
                    callback=self._on_transform_change
                )
                dpg.add_input_int(
                    label="Window Size",
                    tag="window_size",
                    default_value=12,
                    min_value=1,
                    max_value=100,
                    width=100
                )

        # Date range selection
        with dpg.group(horizontal=True):
            dpg.add_date_picker(
                label="Start Date",
                tag="start_date_picker",
                default_value={'month_day': 1, 'year': 2020, 'month': 1}
            )
            dpg.add_date_picker(
                label="End Date",
                tag="end_date_picker",
                default_value={'month_day': 31, 'year': 2024, 'month': 12}
            )
            dpg.add_button(label="Apply Date Filter", callback=self._on_apply_date_filter)

        dpg.add_separator()

        # Data statistics
        with dpg.collapsing_header(label="Statistics", default_open=True):
            with dpg.table(
                    tag="stats_table",
                    header_row=True,
                    row_background=True,
                    borders_innerH=True,
                    borders_outerH=True,
                    height=150,
                    width=-1
            ):
                dpg.add_table_column(label="Statistic")
                dpg.add_table_column(label="Value")

        # Data table
        dpg.add_text("Data Table", color=[200, 200, 255])
        with dpg.table(
                tag="data_table",
                header_row=True,
                row_background=True,
                borders_innerH=True,
                borders_outerH=True,
                borders_innerV=True,
                borders_outerV=True,
                resizable=True,
                sortable=True,
                height=300,
                width=-1
        ):
            dpg.add_table_column(label="Date", width_fixed=True, init_width_or_weight=120)
            dpg.add_table_column(label="Value", width_fixed=True, init_width_or_weight=150)

    def _create_charts_interface(self):
        """Create the charting interface"""
        dpg.add_text("Data Visualization", color=[200, 200, 255])

        # Chart controls
        with dpg.group(horizontal=True):
            dpg.add_combo(
                label="Chart Type",
                tag="chart_type",
                items=["Line Chart", "Bar Chart", "Scatter Plot"],
                default_value="Line Chart",
                callback=self._on_chart_type_change
            )
            dpg.add_button(label="Update Chart", callback=self._on_update_chart)
            dpg.add_button(label="Export Chart", callback=self._on_export_chart)

        # Multi-series selection for comparison
        with dpg.group(horizontal=True):
            dpg.add_listbox(
                tag="chart_series_list",
                items=[],
                num_items=6,
                width=300
            )
            with dpg.group():
                dpg.add_button(label="Add Series", callback=self._on_add_chart_series)
                dpg.add_button(label="Remove Series", callback=self._on_remove_chart_series)
                dpg.add_button(label="Clear All", callback=self._on_clear_chart_series)

        dpg.add_separator()

        # Chart area
        with dpg.plot(
                label="Economic Data Chart",
                height=400,
                width=-1,
                tag="economic_chart"
        ):
            dpg.add_plot_legend()
            dpg.add_plot_axis(dpg.mvXAxis, label="Time", tag="chart_x_axis", time_unit=dpg.mvTimeUnit_Day)
            dpg.add_plot_axis(dpg.mvYAxis, label="Value", tag="chart_y_axis")

    def _create_export_interface(self):
        """Create the export interface"""
        dpg.add_text("Export Data", color=[200, 200, 255])

        # Export options
        with dpg.group(horizontal=True):
            dpg.add_combo(
                label="Format",
                tag="export_format",
                items=["CSV", "Excel", "JSON"],
                default_value="CSV",
                width=150
            )
            dpg.add_checkbox(label="Include Metadata", tag="include_metadata", default_value=True)
            dpg.add_checkbox(label="Include Statistics", tag="include_stats", default_value=False)

        # File path selection
        with dpg.group(horizontal=True):
            dpg.add_input_text(
                label="File Path",
                tag="export_path",
                default_value="economic_data",
                width=400
            )
            dpg.add_button(label="Browse", callback=self._on_browse_export_path)

        # Export actions
        with dpg.group(horizontal=True):
            dpg.add_button(label="Export Current Series", callback=self._on_export_current)
            dpg.add_button(label="Export All Loaded", callback=self._on_export_all)
            dpg.add_button(label="Export Search Results", callback=self._on_export_search)

        dpg.add_separator()

        # Export log
        dpg.add_text("Export Log", color=[200, 200, 255])
        dpg.add_input_text(
            tag="export_log",
            multiline=True,
            readonly=True,
            height=200,
            width=-1,
            default_value="Ready to export data..."
        )

    # Event handlers
    def _on_main_tab_change(self, sender, app_data):
        """Handle main tab changes"""
        if app_data == "tab_search" and not self.providers_cache:
            self._load_providers()

    def _on_search_input(self, sender, app_data):
        """Handle search input with debouncing"""
        pass  # Implement debouncing if needed

    def _on_search_click(self):
        """Handle search button click"""
        query = dpg.get_value("search_input").strip()
        if not query:
            dpg.set_value("search_status", "Please enter search terms")
            return

        self.current_page = 1
        dpg.set_value("search_status", "Searching...")
        asyncio.create_task(self._search_series(query))

    def _on_clear_search(self):
        """Clear search results and reset interface"""
        dpg.set_value("search_input", "")
        dpg.delete_item("search_results_table", children_only=True)
        self._recreate_search_table_headers()
        dpg.set_value("search_status", "Ready")
        dpg.configure_item("prev_page", enabled=False)
        dpg.configure_item("next_page", enabled=False)
        dpg.set_value("page_info", "Page 1")

    def _on_provider_change(self):
        """Handle provider filter change"""
        provider = dpg.get_value("provider_filter")
        if provider != "All Providers":
            self._load_datasets_for_provider(provider)

    def _on_prev_page(self):
        """Handle previous page navigation"""
        if self.current_page > 1:
            self.current_page -= 1
            query = dpg.get_value("search_input")
            asyncio.create_task(self._search_series(query))

    def _on_next_page(self):
        """Handle next page navigation"""
        if self.current_page * self.items_per_page < self.total_items:
            self.current_page += 1
            query = dpg.get_value("search_input")
            asyncio.create_task(self._search_series(query))

    def _on_load_selected(self):
        """Load selected series data"""
        selected_rows = self._get_selected_table_rows()
        if not selected_rows:
            dpg.set_value("search_status", "No series selected")
            return

        dpg.set_value("search_status", f"Loading {len(selected_rows)} series...")
        asyncio.create_task(self._load_series_data(selected_rows))

    def _on_view_details(self):
        """View details of selected series"""
        pass  # Implement details view

    def _on_add_watchlist(self):
        """Add selected series to watchlist"""
        pass  # Implement watchlist functionality

    # API methods
    async def _search_series(self, query):
        """Search for series using DBnomics API"""
        try:
            params = {
                'q': query,
                'limit': self.items_per_page,
                'offset': (self.current_page - 1) * self.items_per_page,
                'format': 'json'
            }

            # Add filters
            provider = dpg.get_value("provider_filter")
            if provider != "All Providers":
                params['provider_code'] = provider

            frequency = dpg.get_value("frequency_filter")
            if frequency != "All":
                params['dimensions.freq'] = frequency

            response = await asyncio.to_thread(
                self.session.get,
                f"{self.base_url}/series",
                params=params,
                timeout=30
            )

            if response.status_code == 200:
                data = response.json()
                self.total_items = data.get('series', {}).get('num_found', 0)
                series_list = data.get('series', {}).get('docs', [])

                dpg.run_async_callback(self._update_search_results, series_list)
            else:
                dpg.run_async_callback(
                    lambda: dpg.set_value("search_status", f"Search failed: {response.status_code}")
                )

        except Exception as e:
            dpg.run_async_callback(
                lambda: dpg.set_value("search_status", f"Search error: {str(e)}")
            )

    async def _load_series_data(self, series_info_list):
        """Load actual time series data for selected series"""
        try:
            loaded_count = 0
            for series_info in series_info_list:
                series_code = series_info.get('code')
                provider_code = series_info.get('provider_code')
                dataset_code = series_info.get('dataset_code')

                if not all([series_code, provider_code, dataset_code]):
                    continue

                # Construct series URL
                series_url = f"{self.base_url}/series/{provider_code}/{dataset_code}/{series_code}"

                response = await asyncio.to_thread(
                    self.session.get,
                    series_url,
                    params={'format': 'json'},
                    timeout=30
                )

                if response.status_code == 200:
                    data = response.json()
                    series_data = data.get('series', {}).get('docs', [])

                    if series_data:
                        # Store the series data
                        series_key = f"{provider_code}/{dataset_code}/{series_code}"
                        self.series_cache[series_key] = {
                            'info': series_info,
                            'data': series_data[0],
                            'observations': series_data[0].get('observations', [])
                        }
                        loaded_count += 1

            dpg.run_async_callback(self._update_loaded_series_ui, loaded_count)

        except Exception as e:
            dpg.run_async_callback(
                lambda: dpg.set_value("search_status", f"Loading error: {str(e)}")
            )

    def _load_providers(self):
        """Load available data providers"""

        def load_providers_async():
            try:
                response = self.session.get(f"{self.base_url}/providers", timeout=10)
                if response.status_code == 200:
                    data = response.json()
                    providers = data.get('providers', {}).get('docs', [])
                    provider_names = ["All Providers"] + [p['code'] for p in providers]
                    self.providers_cache = {p['code']: p for p in providers}

                    dpg.run_async_callback(
                        lambda: dpg.configure_item("provider_filter", items=provider_names)
                    )
            except Exception as e:
                print(f"Failed to load providers: {e}")

        self.executor.submit(load_providers_async)

    # UI update methods
    def _update_search_results(self, series_list):
        """Update search results table"""
        # Clear existing results
        dpg.delete_item("search_results_table", children_only=True)
        self._recreate_search_table_headers()

        # Add new results
        for i, series in enumerate(series_list):
            with dpg.table_row(parent="search_results_table"):
                # Checkbox for selection
                dpg.add_checkbox(tag=f"select_{i}", user_data=series)

                # Series information
                dpg.add_text(series.get('provider_code', 'N/A'))
                dpg.add_text(series.get('dataset_code', 'N/A'))
                dpg.add_text(
                    series.get('name', 'N/A')[:50] + "..." if len(series.get('name', '')) > 50 else series.get('name',
                                                                                                               'N/A'))

                # Frequency
                freq = series.get('dimensions', {}).get('freq', ['N/A'])
                dpg.add_text(freq[0] if isinstance(freq, list) else str(freq))

                # Start and end dates
                period_start = series.get('period_start_day', 'N/A')
                period_end = series.get('period_end_day', 'N/A')
                dpg.add_text(period_start)
                dpg.add_text(period_end)

                # Last update
                indexed_at = series.get('indexed_at', 'N/A')
                if indexed_at != 'N/A':
                    try:
                        dt = datetime.fromisoformat(indexed_at.replace('Z', '+00:00'))
                        indexed_at = dt.strftime('%Y-%m-%d')
                    except:
                        pass
                dpg.add_text(indexed_at)

        # Update pagination
        total_pages = (self.total_items + self.items_per_page - 1) // self.items_per_page
        dpg.set_value("page_info", f"Page {self.current_page} of {total_pages}")
        dpg.configure_item("prev_page", enabled=self.current_page > 1)
        dpg.configure_item("next_page", enabled=self.current_page < total_pages)
        dpg.set_value("search_status", f"Found {self.total_items} series")

    def _recreate_search_table_headers(self):
        """Recreate table headers after clearing"""
        dpg.add_table_column(label="Select", width_fixed=True, init_width_or_weight=60, parent="search_results_table")
        dpg.add_table_column(label="Provider", width_fixed=True, init_width_or_weight=100,
                             parent="search_results_table")
        dpg.add_table_column(label="Dataset", width_fixed=True, init_width_or_weight=150, parent="search_results_table")
        dpg.add_table_column(label="Series Name", width_fixed=True, init_width_or_weight=300,
                             parent="search_results_table")
        dpg.add_table_column(label="Frequency", width_fixed=True, init_width_or_weight=100,
                             parent="search_results_table")
        dpg.add_table_column(label="Start", width_fixed=True, init_width_or_weight=100, parent="search_results_table")
        dpg.add_table_column(label="End", width_fixed=True, init_width_or_weight=100, parent="search_results_table")
        dpg.add_table_column(label="Last Update", width_fixed=True, init_width_or_weight=120,
                             parent="search_results_table")

    def _update_loaded_series_ui(self, loaded_count):
        """Update UI after loading series data"""
        if loaded_count > 0:
            series_names = list(self.series_cache.keys())
            dpg.configure_item("loaded_series_combo", items=series_names)
            if series_names:
                dpg.set_value("loaded_series_combo", series_names[0])
                self._display_series_data(series_names[0])

            dpg.set_value("search_status", f"Loaded {loaded_count} series successfully")
        else:
            dpg.set_value("search_status", "No series data loaded")

    def _display_series_data(self, series_key):
        """Display data for a specific series"""
        if series_key not in self.series_cache:
            return

        series_data = self.series_cache[series_key]
        observations = series_data['observations']

        if not observations:
            return

        # Convert to DataFrame for easier manipulation
        df = pd.DataFrame(observations)
        if 'period' in df.columns and 'value' in df.columns:
            df['period'] = pd.to_datetime(df['period'])
            df = df.sort_values('period')
            df = df.dropna(subset=['value'])

            # Update data table
            self._update_data_table(df)

            # Update statistics
            self._update_statistics(df)

            # Update chart
            self._update_chart_data(series_key, df)

    def _update_data_table(self, df):
        """Update the data table with series observations"""
        dpg.delete_item("data_table", children_only=True)

        # Recreate headers
        dpg.add_table_column(label="Date", width_fixed=True, init_width_or_weight=120, parent="data_table")
        dpg.add_table_column(label="Value", width_fixed=True, init_width_or_weight=150, parent="data_table")

        # Add data rows (limit to recent 100 observations for performance)
        recent_data = df.tail(100)
        for _, row in recent_data.iterrows():
            with dpg.table_row(parent="data_table"):
                dpg.add_text(row['period'].strftime('%Y-%m-%d'))
                dpg.add_text(f"{float(row['value']):.4f}")

    def _update_statistics(self, df):
        """Update statistics table"""
        dpg.delete_item("stats_table", children_only=True)

        # Recreate headers
        dpg.add_table_column(label="Statistic", parent="stats_table")
        dpg.add_table_column(label="Value", parent="stats_table")

        # Calculate statistics
        values = df['value'].astype(float)
        stats = {
            'Count': len(values),
            'Mean': values.mean(),
            'Median': values.median(),
            'Std Dev': values.std(),
            'Min': values.min(),
            'Max': values.max(),
            'Latest': values.iloc[-1] if len(values) > 0 else None,
            'Latest Date': df['period'].iloc[-1].strftime('%Y-%m-%d') if len(df) > 0 else None
        }

        for stat_name, stat_value in stats.items():
            with dpg.table_row(parent="stats_table"):
                dpg.add_text(stat_name)
                if isinstance(stat_value, (int, float)):
                    dpg.add_text(f"{stat_value:.4f}")
                else:
                    dpg.add_text(str(stat_value))

    def _update_chart_data(self, series_key, df):
        """Update chart with series data"""
        # Clear existing data
        dpg.delete_item("economic_chart", children_only=True)

        # Recreate chart elements
        dpg.add_plot_legend(parent="economic_chart")
        dpg.add_plot_axis(dpg.mvXAxis, label="Time", tag="chart_x_axis_new", time_unit=dpg.mvTimeUnit_Day,
                          parent="economic_chart")
        dpg.add_plot_axis(dpg.mvYAxis, label="Value", tag="chart_y_axis_new", parent="economic_chart")

        # Add data series
        if len(df) > 0:
            timestamps = [dt.timestamp() for dt in df['period']]
            values = df['value'].astype(float).tolist()

            dpg.add_line_series(
                timestamps,
                values,
                label=series_key.split('/')[-1],
                parent="chart_y_axis_new"
            )

    def _get_selected_table_rows(self):
        """Get selected rows from search results table"""
        selected = []
        i = 0
        while dpg.does_item_exist(f"select_{i}"):
            if dpg.get_value(f"select_{i}"):
                user_data = dpg.get_item_user_data(f"select_{i}")
                if user_data:
                    selected.append(user_data)
            i += 1
        return selected

    # Data viewer event handlers
    def _on_series_selection_change(self):
        """Handle series selection change in data viewer"""
        selected_series = dpg.get_value("loaded_series_combo")
        if selected_series and selected_series in self.series_cache:
            self._display_series_data(selected_series)

    def _on_refresh_series(self):
        """Refresh current series data"""
        pass  # Implement refresh functionality

    def _on_remove_series(self):
        """Remove current series from cache"""
        selected_series = dpg.get_value("loaded_series_combo")
        if selected_series and selected_series in self.series_cache:
            del self.series_cache[selected_series]
            series_names = list(self.series_cache.keys())
            dpg.configure_item("loaded_series_combo", items=series_names or ["No series loaded"])

    def _on_transform_change(self):
        """Handle data transformation change"""
        pass  # Implement data transformations

    def _on_apply_date_filter(self):
        """Apply date range filter to current series"""
        pass  # Implement date filtering

    # Chart event handlers
    def _on_chart_type_change(self):
        """Handle chart type change"""
        pass  # Implement different chart types

    def _on_update_chart(self):
        """Update chart with current settings"""
        pass  # Implement chart updates

    def _on_export_chart(self):
        """Export current chart"""
        pass  # Implement chart export

    def _on_add_chart_series(self):
        """Add series to chart comparison"""
        pass  # Implement multi-series charts

    def _on_remove_chart_series(self):
        """Remove series from chart comparison"""
        pass  # Implement series removal

    def _on_clear_chart_series(self):
        """Clear all series from chart"""
        pass  # Implement chart clearing

    # Export event handlers
    def _on_browse_export_path(self):
        """Browse for export file path"""

        def file_dialog_callback(sender, app_data):
            if app_data['file_path_name']:
                dpg.set_value("export_path", app_data['file_path_name'])

        with dpg.file_dialog(
                directory_selector=False,
                show=True,
                callback=file_dialog_callback,
                tag="export_file_dialog",
                width=700,
                height=400,
                default_filename="economic_data"
        ):
            dpg.add_file_extension(".*")
            dpg.add_file_extension(".csv", color=(0, 255, 0, 255))
            dpg.add_file_extension(".xlsx", color=(255, 255, 0, 255))
            dpg.add_file_extension(".json", color=(255, 0, 255, 255))

    def _on_export_current(self):
        """Export currently selected series"""
        selected_series = dpg.get_value("loaded_series_combo")
        if not selected_series or selected_series not in self.series_cache:
            self._log_export("No series selected for export")
            return

        asyncio.create_task(self._export_series_data([selected_series]))

    def _on_export_all(self):
        """Export all loaded series"""
        if not self.series_cache:
            self._log_export("No series loaded for export")
            return

        series_list = list(self.series_cache.keys())
        asyncio.create_task(self._export_series_data(series_list))

    def _on_export_search(self):
        """Export current search results metadata"""
        selected_rows = self._get_selected_table_rows()
        if not selected_rows:
            self._log_export("No search results selected for export")
            return

        asyncio.create_task(self._export_search_results(selected_rows))

    async def _export_series_data(self, series_keys):
        """Export series data to file"""
        try:
            export_format = dpg.get_value("export_format")
            file_path = dpg.get_value("export_path")
            include_metadata = dpg.get_value("include_metadata")
            include_stats = dpg.get_value("include_stats")

            if not file_path:
                self._log_export("Please specify export file path")
                return

            # Collect all data
            all_data = {}
            metadata = {}

            for series_key in series_keys:
                if series_key in self.series_cache:
                    series_data = self.series_cache[series_key]
                    observations = series_data['observations']

                    if observations:
                        df = pd.DataFrame(observations)
                        if 'period' in df.columns and 'value' in df.columns:
                            df['period'] = pd.to_datetime(df['period'])
                            df = df.sort_values('period')
                            df = df.dropna(subset=['value'])
                            df['value'] = df['value'].astype(float)

                            all_data[series_key] = df

                            if include_metadata:
                                metadata[series_key] = series_data['info']

            if not all_data:
                self._log_export("No valid data to export")
                return

            # Export based on format
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

            if export_format == "CSV":
                await self._export_to_csv(all_data, file_path, timestamp, include_metadata, include_stats)
            elif export_format == "Excel":
                await self._export_to_excel(all_data, metadata, file_path, timestamp, include_metadata, include_stats)
            elif export_format == "JSON":
                await self._export_to_json(all_data, metadata, file_path, timestamp, include_metadata, include_stats)

            self._log_export(f"Successfully exported {len(all_data)} series to {export_format}")

        except Exception as e:
            self._log_export(f"Export failed: {str(e)}")

    async def _export_to_csv(self, all_data, file_path, timestamp, include_metadata, include_stats):
        """Export data to CSV format"""

        def export_csv():
            if len(all_data) == 1:
                # Single series - simple CSV
                series_key = list(all_data.keys())[0]
                df = all_data[series_key]
                output_path = f"{file_path}_{timestamp}.csv"
                df.to_csv(output_path, index=False)
            else:
                # Multiple series - combined CSV with series identifier
                combined_data = []
                for series_key, df in all_data.items():
                    df_copy = df.copy()
                    df_copy['series'] = series_key
                    combined_data.append(df_copy)

                combined_df = pd.concat(combined_data, ignore_index=True)
                output_path = f"{file_path}_{timestamp}.csv"
                combined_df.to_csv(output_path, index=False)

            return output_path

        output_path = await asyncio.to_thread(export_csv)
        return output_path

    async def _export_to_excel(self, all_data, metadata, file_path, timestamp, include_metadata, include_stats):
        """Export data to Excel format with multiple sheets"""

        def export_excel():
            output_path = f"{file_path}_{timestamp}.xlsx"

            with pd.ExcelWriter(output_path, engine='xlsxwriter') as writer:
                # Data sheets
                for series_key, df in all_data.items():
                    sheet_name = series_key.replace('/', '_')[:31]  # Excel sheet name limit
                    df.to_excel(writer, sheet_name=sheet_name, index=False)

                    # Add statistics if requested
                    if include_stats:
                        values = df['value'].astype(float)
                        stats_data = {
                            'Statistic': ['Count', 'Mean', 'Median', 'Std Dev', 'Min', 'Max'],
                            'Value': [len(values), values.mean(), values.median(),
                                      values.std(), values.min(), values.max()]
                        }
                        stats_df = pd.DataFrame(stats_data)
                        stats_df.to_excel(writer, sheet_name=f"{sheet_name}_stats", index=False)

                # Metadata sheet
                if include_metadata and metadata:
                    metadata_rows = []
                    for series_key, meta in metadata.items():
                        metadata_rows.append({
                            'Series': series_key,
                            'Name': meta.get('name', ''),
                            'Provider': meta.get('provider_code', ''),
                            'Dataset': meta.get('dataset_code', ''),
                            'Start Date': meta.get('period_start_day', ''),
                            'End Date': meta.get('period_end_day', ''),
                            'Frequency': str(meta.get('dimensions', {}).get('freq', '')),
                            'Unit': meta.get('unit', ''),
                            'Description': meta.get('description', '')
                        })

                    metadata_df = pd.DataFrame(metadata_rows)
                    metadata_df.to_excel(writer, sheet_name='Metadata', index=False)

            return output_path

        output_path = await asyncio.to_thread(export_excel)
        return output_path

    async def _export_to_json(self, all_data, metadata, file_path, timestamp, include_metadata, include_stats):
        """Export data to JSON format"""

        def export_json():
            export_data = {
                'export_info': {
                    'timestamp': timestamp,
                    'exported_by': 'Fincept Terminal',
                    'series_count': len(all_data)
                },
                'series_data': {}
            }

            for series_key, df in all_data.items():
                series_export = {
                    'observations': df.to_dict('records')
                }

                if include_stats:
                    values = df['value'].astype(float)
                    series_export['statistics'] = {
                        'count': len(values),
                        'mean': values.mean(),
                        'median': values.median(),
                        'std_dev': values.std(),
                        'min': values.min(),
                        'max': values.max()
                    }

                if include_metadata and series_key in metadata:
                    series_export['metadata'] = metadata[series_key]

                export_data['series_data'][series_key] = series_export

            output_path = f"{file_path}_{timestamp}.json"
            with open(output_path, 'w', encoding='utf-8') as f:
                json.dump(export_data, f, indent=2, default=str)

            return output_path

        output_path = await asyncio.to_thread(export_json)
        return output_path

    async def _export_search_results(self, search_results):
        """Export search results metadata"""
        try:
            file_path = dpg.get_value("export_path")
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

            # Convert search results to DataFrame
            results_data = []
            for result in search_results:
                results_data.append({
                    'Provider': result.get('provider_code', ''),
                    'Dataset': result.get('dataset_code', ''),
                    'Series Code': result.get('code', ''),
                    'Name': result.get('name', ''),
                    'Frequency': str(result.get('dimensions', {}).get('freq', '')),
                    'Start Date': result.get('period_start_day', ''),
                    'End Date': result.get('period_end_day', ''),
                    'Last Update': result.get('indexed_at', ''),
                    'Unit': result.get('unit', ''),
                    'Description': result.get('description', '')
                })

            df = pd.DataFrame(results_data)
            output_path = f"{file_path}_search_results_{timestamp}.csv"

            def save_search_results():
                df.to_csv(output_path, index=False)
                return output_path

            saved_path = await asyncio.to_thread(save_search_results)
            self._log_export(f"Search results exported to {saved_path}")

        except Exception as e:
            self._log_export(f"Search results export failed: {str(e)}")

    def _log_export(self, message):
        """Log export messages"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        current_log = dpg.get_value("export_log")
        new_log = f"[{timestamp}] {message}\n{current_log}"
        dpg.set_value("export_log", new_log)

    # Additional utility methods
    def _get_series_metadata(self, series_key):
        """Get metadata for a series"""
        if series_key in self.series_cache:
            return self.series_cache[series_key]['info']
        return None

    def _format_number(self, value, decimals=2):
        """Format numbers for display"""
        try:
            if isinstance(value, (int, float)):
                if abs(value) >= 1e9:
                    return f"{value / 1e9:.{decimals}f}B"
                elif abs(value) >= 1e6:
                    return f"{value / 1e6:.{decimals}f}M"
                elif abs(value) >= 1e3:
                    return f"{value / 1e3:.{decimals}f}K"
                else:
                    return f"{value:.{decimals}f}"
            return str(value)
        except:
            return str(value)

    def _validate_api_response(self, response):
        """Validate API response"""
        if response.status_code != 200:
            return False, f"API returned status {response.status_code}"

        try:
            data = response.json()
            return True, data
        except json.JSONDecodeError:
            return False, "Invalid JSON response"

    def _get_api_status(self):
        """Check DBnomics API status"""
        try:
            response = self.session.get(f"{self.base_url}/providers", timeout=5)
            return response.status_code == 200
        except:
            return False

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle window resize"""
        # Update table heights
        new_table_height = max(300, top_height - 200)
        try:
            dpg.configure_item("search_results_table", height=new_table_height)
            dpg.configure_item("data_table", height=min(300, new_table_height))
            dpg.configure_item("economic_chart", height=max(350, new_table_height))
        except:
            pass

    def cleanup(self):
        """Clean up resources"""
        if hasattr(self, 'executor'):
            self.executor.shutdown(wait=False)

        # Clear caches
        self.providers_cache.clear()
        self.datasets_cache.clear()
        self.series_cache.clear()
        self.search_cache.clear()

        # Close session
        if hasattr(self, 'session'):
            self.session.close()

    # Advanced features
    def _apply_data_transformation(self, df, transform_type, window_size=12):
        """Apply data transformations"""
        if transform_type == "None" or df.empty:
            return df

        df = df.copy()

        try:
            if transform_type == "Percentage Change":
                df['value'] = df['value'].pct_change() * 100
            elif transform_type == "Log":
                df['value'] = pd.np.log(df['value'])
            elif transform_type == "Difference":
                df['value'] = df['value'].diff()
            elif transform_type == "Moving Average":
                df['value'] = df['value'].rolling(window=window_size).mean()

            # Remove NaN values created by transformations
            df = df.dropna()

        except Exception as e:
            print(f"Transformation error: {e}")

        return df

    def _filter_by_date_range(self, df, start_date, end_date):
        """Filter DataFrame by date range"""
        try:
            mask = (df['period'] >= start_date) & (df['period'] <= end_date)
            return df.loc[mask]
        except:
            return df

    def _get_popular_series(self):
        """Get list of popular economic series"""
        return [
            "OECD/MEI/FRA.CPALTT01.IXOB.M",  # France CPI
            "OECD/MEI/USA.CPALTT01.IXOB.M",  # USA CPI
            "OECD/QNA/USA.B1_GE.GYSA.Q",  # USA GDP
            "ECB/IRS/M.EUR.L.L40.CI.0000.EUR.N.Z",  # EUR Interest Rates
            "FRED/GDP",  # US GDP
            "FRED/UNRATE",  # US Unemployment Rate
            "FRED/FEDFUNDS",  # Federal Funds Rate
        ]

    def _load_popular_series(self):
        """Load popular economic series for quick access"""
        popular_series = self._get_popular_series()
        # This could be implemented to pre-load popular series
        pass

    def _create_comparison_chart(self, series_keys):
        """Create a comparison chart with multiple series"""
        # Clear existing chart
        dpg.delete_item("economic_chart", children_only=True)

        # Recreate chart elements
        dpg.add_plot_legend(parent="economic_chart")
        dpg.add_plot_axis(dpg.mvXAxis, label="Time", tag="chart_x_comp", time_unit=dpg.mvTimeUnit_Day,
                          parent="economic_chart")
        dpg.add_plot_axis(dpg.mvYAxis, label="Value", tag="chart_y_comp", parent="economic_chart")

        colors = [
            [255, 0, 0, 255],  # Red
            [0, 255, 0, 255],  # Green
            [0, 0, 255, 255],  # Blue
            [255, 255, 0, 255],  # Yellow
            [255, 0, 255, 255],  # Magenta
            [0, 255, 255, 255],  # Cyan
        ]

        for i, series_key in enumerate(series_keys):
            if series_key in self.series_cache:
                series_data = self.series_cache[series_key]
                observations = series_data['observations']

                if observations:
                    df = pd.DataFrame(observations)
                    if 'period' in df.columns and 'value' in df.columns:
                        df['period'] = pd.to_datetime(df['period'])
                        df = df.sort_values('period')
                        df = df.dropna(subset=['value'])

                        timestamps = [dt.timestamp() for dt in df['period']]
                        values = df['value'].astype(float).tolist()

                        color = colors[i % len(colors)]
                        label = series_key.split('/')[-1]

                        dpg.add_line_series(
                            timestamps,
                            values,
                            label=label,
                            parent="chart_y_comp"
                        )