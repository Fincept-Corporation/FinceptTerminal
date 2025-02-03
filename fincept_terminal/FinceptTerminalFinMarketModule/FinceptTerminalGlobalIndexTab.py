from textual.containers import VerticalScroll, Horizontal
from textual.app import ComposeResult
from textual.widgets import Static, Collapsible, OptionList, Button, DataTable
import asyncio
import financedatabase as fd
import pandas as pd
import yfinance as yf

class GlobalIndicesTab(VerticalScroll):
    """Custom tab for managing and displaying global indices with pagination."""

    def __init__(self):
        super().__init__()
        self.all_indices = []  # Stores all fetched indices
        self.current_page = 0  # Current page index
        self.indices_per_page = 10  # Number of indices per page
        self.all_exchanges = []  # Stores all available exchanges
        self.is_loaded = False

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the Global Indices tab."""

        yield Static("🌍 Select Index Exchange and Index:", id="indices-title")

        # Collapsible section for selecting exchange
        with Collapsible(title="Select Exchange", collapsed_symbol=">"):
            yield OptionList(id="exchange_selector")

        # Collapsible section for selecting index (updates dynamically)
        with Collapsible(title="Select Index", collapsed_symbol=">"):
            yield OptionList(id="index_selector")

            # Pagination Controls
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_index_page", variant="default")
                yield Button("Next", id="next_index_page", variant="default")

        # Button to load index details
        yield Button("Show Index Info", id="show_index_info")

        # Static section for displaying index details
        yield DataTable(name="Index Details", id="index_details_table")

    async def on_mount(self):
        """Triggered when the tab is mounted. Does not fetch exchanges immediately."""
        self.is_loaded = False  # Track whether data has been loaded

    async def on_show(self):
        """Triggered when the tab becomes visible. Loads data if not already loaded."""
        if not self.is_loaded:
            self.app.notify("📡 Loading index exchanges...")
            await self.populate_exchange_list()
            self.is_loaded = True  # Prevent multiple loads

    async def populate_exchange_list(self):
        """Fetch and populate the OptionList with available index exchanges."""
        exchange_list = self.query_one("#exchange_selector", OptionList)
        exchange_list.clear_options()

        # ✅ Add a temporary placeholder without 'disabled'
        loading_placeholder = "Loading..."
        exchange_list.add_option(loading_placeholder)

        try:
            self.all_exchanges = await self.fetch_index_exchanges()
            exchange_list.clear_options()  # Remove the placeholder

            if not self.all_exchanges:
                self.app.notify("❌ No exchanges found!", severity="error")
                return

            for exchange in self.all_exchanges:
                exchange_list.add_option(exchange)

            self.app.notify("✅ Index exchanges loaded successfully!")

        except Exception as e:
            self.app.notify(f"❌ Error loading index exchanges: {e}", severity="error")

    def display_indices(self):
        """Display the current page of indices in the OptionList."""
        start_index = self.current_page * self.indices_per_page
        end_index = start_index + self.indices_per_page

        index_list = self.query_one("#index_selector", OptionList)
        index_list.clear_options()

        # Display indices for the current page
        for index in self.all_indices[start_index:end_index]:
            index_name = index.get("name", "Unknown Index")
            index_list.add_option(index_name)

        # Update UI with the current page status
        self.app.notify(
            f"Showing indices {start_index + 1} to {min(end_index, len(self.all_indices))} of {len(self.all_indices)}"
        )

    async def populate_index_list(self, exchange):
        """Fetch and populate indices for the selected exchange with pagination."""
        try:
            self.all_indices = await self.fetch_indices_by_exchange(exchange)
            if not self.all_indices:
                self.app.notify(f"⚠ No indices available for {exchange}.", severity="warning")
                return

            self.current_page = 0  # Reset pagination
            self.display_indices()  # Show first page
        except Exception as e:
            self.app.notify(f"❌ Error loading indices: {e}", severity="error")

    async def fetch_index_exchanges(self):
        """Fetch a list of unique index exchanges using financedatabase."""
        try:
            indices = fd.Indices()
            indices_data = indices.select()
            return sorted(indices_data["exchange"].dropna().unique().tolist())
        except Exception as e:
            return []

    async def fetch_indices_by_exchange(self, exchange):
        """Fetch indices filtered by the selected exchange using financedatabase."""
        try:
            indices = fd.Indices()
            indices_data = indices.select()
            filtered_indices = indices_data[indices_data["exchange"] == exchange]

            if filtered_indices.empty:
                return []

            return filtered_indices.reset_index().to_dict("records")
        except Exception as e:
            return []

    async def fetch_index_details(self, index_name):
        """
        Fetch general information and the last 7 days of historical data for the given index symbol.

        Parameters:
            index_name (str): The symbol of the index or financial instrument.

        Returns:
            tuple: (General info dictionary, Historical data DataFrame)
        """
        try:
            ticker = yf.Ticker(index_name)

            # ✅ Fetch general info
            info = ticker.info
            filtered_info = {k: v for k, v in info.items() if
                             k not in ['messageBoardId', 'gmtOffSetMilliseconds', 'uuid', 'maxAge']}

            # ✅ Fetch last 7 days of historical data
            history_data = ticker.history(period='7d')

            # If no recent data, try fetching last 1-month data
            if history_data.empty:
                self.app.notify(f"⚠ No recent data for {index_name}. Trying 1-month history...", severity="warning")
                history_data = ticker.history(period='1mo')

            # If still empty, return empty DataFrame
            if history_data.empty:
                self.app.notify(f"❌ No historical data available for {index_name}.", severity="error")
                return filtered_info, pd.DataFrame(columns=['Date', 'Open', 'High', 'Low', 'Close', 'Volume'])

            # ✅ Extract and limit to required columns
            required_columns = ['Open', 'High', 'Low', 'Close', 'Volume']
            available_columns = [col for col in required_columns if col in history_data.columns]
            history_table = history_data[available_columns].tail(7)  # Keep only the last 7 days

            return filtered_info, history_table

        except Exception as e:
            self.app.notify(f"❌ Error fetching data for {index_name}: {e}", severity="error")
            return {}, pd.DataFrame(columns=['Date', 'Open', 'High', 'Low', 'Close', 'Volume'])

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for exchanges and indices."""
        if event.option_list.id == "exchange_selector":
            selected_exchange = event.option_list.get_option_at_index(event.option_index).prompt
            self.selected_exchange = selected_exchange
            self.app.notify(f"✅ Selected Exchange: {self.selected_exchange}")
            await self.populate_index_list(self.selected_exchange)

        elif event.option_list.id == "index_selector":
            selected_index = event.option_list.get_option_at_index(event.option_index).prompt
            self.selected_index = selected_index
            self.app.notify(f"✅ Selected Index: {self.selected_index}")

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle pagination and index info display."""
        if event.button.id == "show_index_info":
            await self.show_index_info()

        elif event.button.id == "next_index_page":
            if (self.current_page + 1) * self.indices_per_page < len(self.all_indices):
                self.current_page += 1
                self.display_indices()
            else:
                self.app.notify("⚠ No more indices to display.", severity="warning")

        elif event.button.id == "previous_index_page":
            if self.current_page > 0:
                self.current_page -= 1
                self.display_indices()
            else:
                self.app.notify("⚠ Already at the first page.", severity="warning")

    async def show_index_info(self):
        """Display general information and historical data for the selected index."""
        if not hasattr(self, "selected_index"):
            self.app.notify("⚠ No index selected!", severity="warning")
            return

        # ✅ Fetch details and history
        index_details, history_data = await self.fetch_index_details(self.selected_index)

        if not index_details:
            self.app.notify("⚠ No details available for the selected index.", severity="warning")
            return

        # ✅ Reference to the DataTable
        index_table = self.query_one("#index_details_table", DataTable)
        index_table.clear()

        # ✅ Add columns if not set
        if not index_table.columns:
            index_table.add_column("Attribute")
            index_table.add_column("Value")

        # ✅ Add general info
        for attribute, value in index_details.items():
            index_table.add_row(str(attribute), str(value))

        # ✅ Add historical data (if available)
        if not history_data.empty:
            index_table.add_row("Last 7 Days Price History", "")
            for date, row in history_data.iterrows():
                index_table.add_row(str(date.date()),
                                    f"Open: {row['Open']}, Close: {row['Close']}, High: {row['High']}, Low: {row['Low']}")

        self.app.notify(f"✅ Index details for {self.selected_index} displayed.")
