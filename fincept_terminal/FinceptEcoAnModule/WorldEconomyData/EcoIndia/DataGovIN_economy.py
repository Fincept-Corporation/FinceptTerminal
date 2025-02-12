# EconomicAnalysisScreen.py
from textual.widgets import Static, Button, Collapsible, DataTable, OptionList
import asyncio, os, json
from textual.containers import VerticalScroll, Container, Horizontal
from textual.app import ComposeResult
from datagovindia import DataGovIndia, check_api_key
import sqlite3

DB_PATH = os.path.expanduser("~/datagovindia.db")

from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import get_settings_path, get_documents_folder
SETTINGS_FILE = get_settings_path()
DOCUMENTS_FOLDER = get_documents_folder()

class DataGovINtab(VerticalScroll):
    """
    A Textual Tab to interact with DataGovIndia economic data.

    This tab validates the API key, initializes and syncs metadata if needed,
    displays a paginated list of available resources, and lets the user view
    resource information, sample data, and download the resource data.
    """

    def __init__(self):
        super().__init__()
        self.api_key = None
        self.resources = []  # List of resources (each a dict with title and resource_id)
        self.current_resource_page = 0
        self.items_per_page = 7
        self.current_resource_id = None  # Tracks the resource currently selected

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the DataGovIndia tab."""
        yield Static("DataGovIndia Economic Data", id="header")

        with Container(id="datagovin_container"):

        # Section for selecting a resource
            with Collapsible(title="Select Resource", collapsed_symbol=">",classes="resource_collapsible"):
                with VerticalScroll():
                    yield OptionList(id="resource_selector")
                    with Horizontal(classes="horizontal_next_previous"):
                        yield Button("Previous Resources", id="previous_resource_page", variant="default")
                        yield Button("Next Resources", id="next_resource_page", variant="default")

        # Section for displaying general resource information
            with Container(id="Resource_Information", classes="Resource_Information"):
                yield Static("Resource Info", classes="header")
                yield DataTable(id="resource_info_table", name="Resource Info")

        # Section for displaying sample resource data
            with VerticalScroll(id="Resource_Data", classes="Resource_Data"):
                yield Static("Resource Data", classes="header")
                yield DataTable(id="resource_data_table", name="Resource Data")
                yield Button("Download Data", id="download_data", variant="primary")

    async def on_mount(self):
        """Called when the component is mounted; initialize the metadata."""
        self.app.notify("Initializing DataGovIndia Tab...")
        self.loading = True
        asyncio.create_task(self.initialize_metadata())

    async def initialize_metadata(self):
        """
        Validate the API key, initialize metadata, and populate the resource list.
        """
        # Validate API key (runs file I/O in background)
        self.api_key = await self.validate_api_key()
        if not self.api_key:
            self.app.notify("API key validation failed. Please check your settings.", severity="error")
            return

        try:
            datagovin = DataGovIndia(api_key=self.api_key, db_path=DB_PATH)

            if not os.path.exists(DB_PATH):
                self.app.notify("Database file not found. Creating a new database...", severity="warning")

            try:
                update_info = await asyncio.to_thread(datagovin.get_update_info)
                if update_info:
                    self.app.notify(f"Metadata last updated: {update_info}")
                else:
                    self.app.notify("No update info available. Initializing metadata...", severity="information")
            except Exception as e:
                self.app.notify(f"Metadata check failed: {e}. Initializing metadata...", severity="warning")

            # Retrieve available resources from the database (using background thread)
            resources = await self.get_all_resources()
            if resources:
                self.app.notify(f"Found {len(resources)} resources in the database.")
            else:
                self.app.notify("No resources found in the database. Syncing metadata...", severity="warning")
                await asyncio.to_thread(datagovin.sync_metadata)
                resources = await self.get_all_resources()

            self.resources = resources
            self.current_resource_page = 0
            asyncio.create_task(self.display_resources())

        except Exception as e:
            self.app.notify(f"Error initializing metadata: {e}", severity="error")
        finally:
            self.loading = False

    async def validate_api_key(self) -> str:
        """
        Validate the API key from the settings file.
        Instead of exiting on error, we notify the user.
        """
        if not os.path.exists(SETTINGS_FILE):
            self.app.notify(
                "Settings file not found. Please ensure 'settings.json' exists in the settings folder.",
                severity="error"
            )
            return None

        # Wrap file reading in a thread since it is blocking.
        try:
            settings = await asyncio.to_thread(self._read_settings)
            data_source = settings.get("data_sources", {}).get("world_economy_tracker", {})
            api_key = data_source.get("apikey")
        except Exception:
            self.app.notify("Error: 'settings.json' file is corrupted or invalid.", severity="error")
            return None

        if not api_key:
            self.app.notify(
                "API key not found for 'World Economy Tracker' in 'settings.json'. Please add it.",
                severity="error"
            )
            return None

        if not await asyncio.to_thread(check_api_key, api_key):
            self.app.notify(
                "Invalid API key for 'World Economy Tracker'. Please set a valid key in 'settings.json'.",
                severity="error"
            )
            return None

        self.app.notify("API key for 'World Economy Tracker' validated successfully!", severity="information")
        return api_key

    def _read_settings(self):
        """Helper function to read settings from SETTINGS_FILE."""
        with open(SETTINGS_FILE, "r") as file:
            return json.load(file)

    async def get_all_resources(self, db_path=DB_PATH) -> list:
        """
        Retrieve all resources (title and resource_id) directly from the SQLite database.
        This call is offloaded to a background thread.
        """
        return await asyncio.to_thread(self._get_all_resources_sync, db_path)

    def _get_all_resources_sync(self, db_path):
        """Synchronous helper for fetching resources from SQLite."""
        try:
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            query = "SELECT title, resource_id FROM resources"
            cursor.execute(query)
            resources = [{"title": row[0], "resource_id": row[1]} for row in cursor.fetchall()]
            conn.close()
            return resources
        except sqlite3.Error as e:
            self.app.notify(f"Error fetching resources from the database: {e}", severity="error")
            return []

    async def display_resources(self):
        """
        Display the current page of resources in the OptionList with separators.
        """
        start_index = self.current_resource_page * self.items_per_page
        end_index = start_index + self.items_per_page
        resource_list = self.query_one("#resource_selector", OptionList)
        resource_list.clear_options()

        # Get the resources for the current page
        page_resources = self.resources[start_index:end_index]

        for i, resource in enumerate(page_resources):
            # Format the option as "Resource Title - Resource_ID"
            option_text = f"{resource['title']} - {resource['resource_id']}"
            resource_list.add_option(option_text)

        self.app.notify(
            f"Showing resources {start_index + 1} to {min(end_index, len(self.resources))} of {len(self.resources)}"
        )

    async def on_button_pressed(self, event: Button.Pressed):
        """
        Handle pagination and download button actions.
        """
        button_id = event.button.id

        if button_id == "next_resource_page":
            if (self.current_resource_page + 1) * self.items_per_page < len(self.resources):
                self.current_resource_page += 1
                asyncio.create_task(self.display_resources())
            else:
                self.app.notify("No more resources to display.", severity="warning")

        elif button_id == "previous_resource_page":
            if self.current_resource_page > 0:
                self.current_resource_page -= 1
                asyncio.create_task(self.display_resources())
            else:
                self.app.notify("Already at the first page of resources.", severity="warning")

        elif button_id == "download_data":
            if self.current_resource_id:
                # For simplicity, use a default file name based on the resource id.
                file_name = f"resource_{self.current_resource_id}.csv"
                success = await asyncio.create_task(self.download_resource_data(self.current_resource_id, file_name))
                if success:
                    self.app.notify(f"Data successfully downloaded as {file_name}!", severity="information")
                else:
                    self.app.notify("Failed to download the data.", severity="error")
            else:
                self.app.notify("No resource selected for download.", severity="warning")

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """
        When the user selects a resource, fetch and display its information and sample data.
        """
        if event.option_list.id == "resource_selector":
            selected_text = event.option_list.get_option_at_index(event.option_index).prompt
            # Expecting the format: "Resource Title - Resource_ID"
            parts = selected_text.rsplit(" - ", 1)
            if len(parts) != 2:
                self.app.notify("Invalid resource format.", severity="error")
                return

            resource_id = parts[1]
            self.current_resource_id = resource_id
            self.app.notify(f"Selected Resource ID: {resource_id}", severity="information")
            self.loading = True
            await asyncio.create_task(self.fetch_and_display_resource_info(resource_id))
            await asyncio.create_task(self.fetch_and_display_resource_data(resource_id))

    async def fetch_and_display_resource_info(self, resource_id: str):
        """
        Fetch resource information and display it in the Resource Information table.
        """
        try:
            datagovin = DataGovIndia(api_key=self.api_key, db_path=DB_PATH)
            self.app.notify(f"Fetching info for Resource ID: {resource_id}...", severity="information")
            resource_info = await asyncio.to_thread(datagovin.get_resource_info, resource_id)
            if not resource_info:
                self.app.notify(f"No information found for Resource ID: {resource_id}", severity="warning")
                return

            data_table = self.query_one("#resource_info_table", DataTable)
            data_table.clear()
            if not data_table.columns:
                data_table.add_column("Attribute")
                data_table.add_column("Value")

            # Display general resource info (skip the "field" details)
            for key, value in resource_info.items():
                if key == "field":
                    continue
                if isinstance(value, list):
                    value = ", ".join(map(str, value))
                elif isinstance(value, bool):
                    value = "True" if value else "False"
                elif value is None:
                    value = "N/A"
                data_table.add_row(str(key), str(value))

            self.app.notify(f"Resource info fetched successfully for Resource ID: {resource_id}", severity="information")
        except Exception as e:
            self.app.notify(f"Error fetching resource info: {e}", severity="error")

    async def fetch_and_display_resource_data(self, resource_id: str):
        """
        Fetch resource data (sample rows) and display it in the Resource Data table.
        """
        try:
            datagovin = DataGovIndia(api_key=self.api_key, db_path=DB_PATH)
            self.app.notify(f"Fetching data for Resource ID: {resource_id}...", severity="information")
            # Limit to first 10 rows for display
            data = datagovin.get_data(resource_id, limit=10)
            if data is None or data.empty:
                self.app.notify(f"No data available for Resource ID: {resource_id}", severity="warning")
                return

            data_table = self.query_one("#resource_data_table", DataTable)
            data_table.clear()
            # Create columns based on the DataFrame
            for col in data.columns:
                data_table.add_column(str(col))

            # Add rows (only the first 5 rows)
            for _, row in data.head(10).iterrows():
                row_values = [str(item) for item in row.tolist()]
                data_table.add_row(*row_values)

            self.app.notify(f"Resource data fetched successfully for Resource ID: {resource_id}", severity="information")
        except Exception as e:
            self.app.notify(f"Error fetching resource data: {e}", severity="error")
        finally:
            self.loading = False

    async def download_resource_data(self, resource_id: str, output_file: str) -> bool:
        """
        Download the complete data for a resource and save it to the specified file inside the Documents folder.
        Supported file formats are .csv, .json, and .xlsx.
        """
        try:
            datagovin = DataGovIndia(api_key=self.api_key)
            self.app.notify(f"Downloading data for Resource ID: {resource_id}...", severity="information")

            # Fetch data asynchronously
            data = await asyncio.to_thread(datagovin.get_data, resource_id)
            if data is None or data.empty:
                self.app.notify("No data available to download.", severity="warning")
                return False

            # Ensure the file is stored in the Documents/FinceptTerminal folder
            output_file_path = os.path.join(DOCUMENTS_FOLDER, output_file)

            # Save the file based on format
            if output_file.endswith(".csv"):
                await asyncio.to_thread(data.to_csv, output_file_path, index=False)
            elif output_file.endswith(".json"):
                await asyncio.to_thread(data.to_json, output_file_path, orient="records", indent=4)
            elif output_file.endswith(".xlsx"):
                await asyncio.to_thread(data.to_excel, output_file_path, index=False, engine="openpyxl")
            else:
                self.app.notify("Invalid file format. Supported formats: .csv, .json, .xlsx", severity="error")
                return False

            self.app.notify(f"✅ File saved at: {output_file_path}", severity="success")
            return True

        except Exception as e:
            self.app.notify(f"Error downloading resource data: {e}", severity="error")
            return False

