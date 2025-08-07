#indiagov_tab.py

import dearpygui.dearpygui as dpg
import threading
import os
import sqlite3
from datagovindia import DataGovIndia, check_api_key
from fincept_terminal.Utils.Logging.logger import info, error, warning, monitor_performance, operation

# Store DB and downloads in current working directory
DB_FILENAME = "datagovindia.db"
DB_PATH = os.path.abspath(DB_FILENAME)


class DataGovIndiaTab:
    """DearPyGUI tab for DataGovIndia economic data."""

    def __init__(self, app=None):
        self.app = app
        self.api_key = None
        self.resources = []
        self.current_page = 0
        self.items_per_page = 5
        self.current_resource_id = None
        self._resource_cache = {}  # Cache for resource info

        info("DataGovIndia tab initialized", context={"db_path": DB_PATH})

    def get_label(self):
        return "Economic Data"

    def create_content(self):
        dpg.add_text("DataGovIndia Economic Data", tag="dg_header")
        with dpg.group(horizontal=True):
            dpg.add_input_text(label="API Key", tag="dg_api_key_input", width=400,
                               hint="Enter your API key here")
            dpg.add_button(label="Validate Key", callback=self._on_validate)
        dpg.add_separator()
        dpg.add_button(label="Initialize/Sync Metadata", callback=self._on_sync)
        dpg.add_separator()
        with dpg.collapsing_header(label="Select Resource", default_open=True):
            dpg.add_combo(items=[], tag="resource_selector", width=450,
                          callback=self._on_select)
            with dpg.group(horizontal=True):
                dpg.add_button(label="Previous", callback=lambda s,a: self._on_page(-1))
                dpg.add_button(label="Next", callback=lambda s,a: self._on_page(1))
        dpg.add_separator()
        dpg.add_text("Resource Info")
        with dpg.table(tag="resource_info_table", header_row=True,
                       row_background=True, height=150, width=-1):
            dpg.add_table_column(label="Attribute")
            dpg.add_table_column(label="Value")
        dpg.add_text("Resource Data")
        with dpg.table(tag="resource_data_table", header_row=True,
                       row_background=True, height=150, width=-1):
            dpg.add_table_column(label="Message")
        dpg.add_separator()
        dpg.add_button(label="Download Selected Data", callback=self._on_download)
        dpg.add_text("", tag="dg_status")

    def _on_validate(self, sender, app_data):
        key = dpg.get_value("dg_api_key_input").strip()
        threading.Thread(target=self._validate_sync, args=(key,), daemon=True).start()

    def _validate_sync(self, key):
        if not key:
            dpg.set_value("dg_status", " API key is empty.")
            return

        with operation("api_key_validation"):
            try:
                valid = check_api_key(key)
                if valid:
                    self.api_key = key
                    info("API key validated successfully")
                    dpg.set_value("dg_status", " API key validated.")
                else:
                    warning("API key validation failed - invalid key")
                    dpg.set_value("dg_status", " Invalid API key.")
            except Exception as e:
                error("API key validation error", context={"error": str(e)}, exc_info=True)
                dpg.set_value("dg_status", " Invalid API key.")

    def _on_sync(self, sender, app_data):
        if not self.api_key:
            dpg.set_value("dg_status", " Please validate API key first.")
            warning("Metadata sync attempted without API key")
            return
        threading.Thread(target=self._sync_metadata, daemon=True).start()

    @monitor_performance
    def _sync_metadata(self):
        dpg.set_value("dg_status", " Syncing metadata... This may take a while.")

        with operation("metadata_sync", context={"db_path": DB_PATH}):
            datagovin = DataGovIndia(api_key=self.api_key, db_path=DB_PATH)
            try:
                datagovin.sync_metadata()
                info("Metadata sync completed", context={"db_path": DB_PATH})
                dpg.set_value("dg_status", f" Metadata synced to {DB_PATH}.")
            except Exception as e:
                error("Metadata sync failed", context={"error": str(e)}, exc_info=True)
                dpg.set_value("dg_status", f" Sync error: {e}")
                return

        # Load resources from SQLite with caching
        self._load_resources_from_db()

    def _load_resources_from_db(self):
        """Load resources from database with error handling and caching"""
        try:
            with operation("load_resources_from_db"):
                conn = sqlite3.connect(DB_PATH)
                cursor = conn.cursor()
                cursor.execute("SELECT title, resource_id FROM resources")
                rows = cursor.fetchall()
                conn.close()

                self.resources = [{"title": r[0], "resource_id": r[1]} for r in rows]
                info("Resources loaded from database", context={"count": len(self.resources)})

        except Exception as e:
            error("Failed to load resources from database", context={"error": str(e)}, exc_info=True)
            dpg.set_value("dg_status", f" DB load error: {e}")
            self.resources = []

        self._update_resources()

    def _update_resources(self):
        """Update resource selector with pagination"""
        start = self.current_page * self.items_per_page
        end = start + self.items_per_page
        items = [f"{r['title']} - {r['resource_id']}" for r in self.resources[start:end]]

        dpg.configure_item("resource_selector", items=items)
        if items:
            dpg.set_value("resource_selector", items[0])
            self._on_select(None, items[0])

        total = len(self.resources)
        dpg.set_value("dg_status", f"Showing {start+1}-{min(end,total)} of {total}")

    def _on_page(self, delta):
        """Handle pagination with bounds checking"""
        max_page = max((len(self.resources)-1)//self.items_per_page, 0)
        new_page = self.current_page + delta
        self.current_page = min(max(new_page, 0), max_page)
        self._update_resources()

    def _on_select(self, sender, value):
        """Handle resource selection with caching"""
        if not value:
            return

        self.current_resource_id = value.rsplit(" - ", 1)[-1]

        # Launch both info and data fetching in parallel for better performance
        threading.Thread(target=self._fetch_info_sync, args=(self.current_resource_id,), daemon=True).start()
        threading.Thread(target=self._fetch_data_sync, args=(self.current_resource_id,), daemon=True).start()

    def _fetch_info_sync(self, res_id):
        """Fetch resource info with caching"""
        table = "resource_info_table"
        dpg.delete_item(table, children_only=True)

        # Check cache first for performance
        if res_id in self._resource_cache:
            info_data = self._resource_cache[res_id]
        else:
            try:
                with operation("fetch_resource_info", context={"resource_id": res_id}):
                    info_data = DataGovIndia(api_key=self.api_key, db_path=DB_PATH).get_resource_info(res_id)
                    self._resource_cache[res_id] = info_data  # Cache the result

            except Exception as e:
                error("Failed to fetch resource info", context={"resource_id": res_id, "error": str(e)}, exc_info=True)
                info_data = {"Error": str(e)}

        # Rebuild table structure
        dpg.add_table_column(label="Attribute", parent=table)
        dpg.add_table_column(label="Value", parent=table)

        for k, v in info_data.items():
            if k == "field":
                continue
            val = ", ".join(v) if isinstance(v, list) else str(v or "N/A")
            dpg.add_table_row(parent=table, children=[dpg.add_text(k), dpg.add_text(val)])

    def _fetch_data_sync(self, res_id):
        """Fetch resource data with error handling"""
        table = "resource_data_table"
        dpg.clear_table(table)

        try:
            with operation("fetch_resource_data", context={"resource_id": res_id, "limit": 10}):
                df = DataGovIndia(api_key=self.api_key, db_path=DB_PATH).get_data(res_id, 10)

        except Exception as e:
            error("Failed to fetch resource data", context={"resource_id": res_id, "error": str(e)}, exc_info=True)
            df = None
            err = str(e)

        if df is None or df.empty:
            dpg.add_table_column(label="Message", parent=table)
            error_msg = err if df is None else "No data available."
            dpg.add_table_row(parent=table, children=[dpg.add_text(error_msg)])
            return

        # Build table with data
        for col in df.columns:
            dpg.add_table_column(label=str(col), parent=table)

        for _, row in df.head(10).iterrows():
            cells = [dpg.add_text(str(x)) for x in row]
            dpg.add_table_row(parent=table, children=cells)

    def _on_download(self, sender, app_data):
        """Initiate data download"""
        if not self.current_resource_id:
            dpg.set_value("dg_status", " Select a resource.")
            warning("Download attempted without resource selection")
            return

        filename = f"resource_{self.current_resource_id}.csv"
        threading.Thread(target=self._download_sync, args=(self.current_resource_id, filename), daemon=True).start()

    @monitor_performance
    def _download_sync(self, res_id, filename):
        """Download resource data to CSV file"""
        try:
            with operation("download_resource_data", context={"resource_id": res_id, "filename": filename}):
                df = DataGovIndia(api_key=self.api_key).get_data(res_id)

                if df is None or df.empty:
                    warning("No data available for download", context={"resource_id": res_id})
                    dpg.set_value("dg_status", " No data to download.")
                    return

                path = os.path.abspath(filename)
                df.to_csv(path, index=False)

                info("Data downloaded successfully", context={
                    "resource_id": res_id,
                    "filename": filename,
                    "path": path,
                    "rows": len(df)
                })
                dpg.set_value("dg_status", f" Downloaded: {path}")

        except Exception as e:
            error("Data download failed", context={
                "resource_id": res_id,
                "filename": filename,
                "error": str(e)
            }, exc_info=True)
            dpg.set_value("dg_status", f" Download error: {e}")

    def cleanup(self):
        """Cleanup resources when tab is closed"""
        try:
            # Clear cache to free memory
            self._resource_cache.clear()
            info("DataGovIndia tab cleanup completed")
        except Exception as e:
            error("Error during tab cleanup", context={"error": str(e)}, exc_info=True)