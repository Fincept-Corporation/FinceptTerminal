from textual.containers import VerticalScroll, Horizontal
from textual.app import ComposeResult
from textual.widgets import Static, Collapsible, OptionList, Button, DataTable
import asyncio
import financedatabase as fd
import pandas as pd
import yfinance as yf


class GlobalIndicesTab(VerticalScroll):
    """Custom tab for managing and displaying global indices with pagination."""

    INDICES_PER_PAGE = 10  # Number of indices per page

    def __init__(self):
        super().__init__()
        self.all_indices = []  # Stores all fetched indices
        self.all_exchanges = []  # Stores all available exchanges
        self.current_page = 0  # Current page index
        self.is_loaded = False
        self.loading = False  # Flag to prevent concurrent fetches

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the Global Indices tab."""

        yield Static("🌍 Select Index Exchange and Index:", id="indices-title")

        with Collapsible(title="Select Exchange", collapsed_symbol=">"):
            yield OptionList(id="exchange_selector")

        with Collapsible(title="Select Index", collapsed_symbol=">"):
            yield OptionList(id="index_selector")

            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_index_page", variant="default", disabled=True)
                yield Button("Next", id="next_index_page", variant="default", disabled=True)

        yield Button("Show Index Info", id="show_index_info")
        yield DataTable(name="Index Details", id="index_details_table")

    async def on_show(self):
        """Triggered when the tab becomes visible. Loads data if not already loaded."""
        if not self.is_loaded and not self.loading:
            self.loading = True
            self.app.notify("📡 Loading index exchanges...")
            asyncio.create_task(self.populate_exchange_list())  # Run in background
            self.is_loaded = True

    async def populate_exchange_list(self):
        """Fetch and populate the list of available index exchanges asynchronously in a separate thread."""
        try:
            self.all_exchanges = await asyncio.to_thread(self.fetch_index_exchanges)
            exchange_list = self.query_one("#exchange_selector", OptionList)
            exchange_list.clear_options()

            if not self.all_exchanges:
                self.app.notify("❌ No exchanges found!", severity="error")
                return

            exchange_list.add_options(self.all_exchanges)
            self.app.notify("✅ Index exchanges loaded successfully!")

        except Exception as e:
            self.app.notify(f"❌ Error loading index exchanges: {e}", severity="error")
        finally:
            self.loading = False

    def display_indices(self):
        """Display the current page of indices in the OptionList."""
        start_index = self.current_page * self.INDICES_PER_PAGE
        end_index = start_index + self.INDICES_PER_PAGE
        index_list = self.query_one("#index_selector", OptionList)
        index_list.clear_options()

        for index in self.all_indices[start_index:end_index]:
            index_list.add_option(index.get("name", "Unknown Index"))

        self._update_pagination_controls()

    async def populate_index_list(self, exchange):
        """Fetch and populate indices for the selected exchange asynchronously in a separate thread."""
        if self.loading:
            self.app.notify("⚠ Already loading indices, please wait...", severity="warning")
            return

        self.loading = True
        self.app.notify(f"🔄 Fetching indices for {exchange}...")
        asyncio.create_task(self._populate_index_list_task(exchange))

    async def _populate_index_list_task(self, exchange):
        """Background task to fetch and display indices."""
        try:
            self.all_indices = await asyncio.to_thread(self.fetch_indices_by_exchange, exchange)
            if not self.all_indices:
                self.app.notify(f"⚠ No indices available for {exchange}.", severity="warning")
                return

            self.current_page = 0
            self.display_indices()

        except Exception as e:
            self.app.notify(f"❌ Error loading indices: {e}", severity="error")
        finally:
            self.loading = False

    def fetch_index_exchanges(self):
        """Fetch available index exchanges synchronously (executed in a separate thread)."""
        try:
            indices = fd.Indices()
            indices_data = indices.select()
            return sorted(indices_data["exchange"].dropna().unique().tolist())
        except Exception:
            return []

    def fetch_indices_by_exchange(self, exchange):
        """Fetch indices filtered by the selected exchange synchronously (executed in a separate thread)."""
        try:
            indices = fd.Indices()
            indices_data = indices.select()
            filtered_indices = indices_data[indices_data["exchange"] == exchange]

            return filtered_indices.reset_index().to_dict("records") if not filtered_indices.empty else []
        except Exception:
            return []

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for exchanges and indices."""
        if event.option_list.id == "exchange_selector":
            selected_exchange = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Exchange: {selected_exchange}")
            await self.populate_index_list(selected_exchange)

        elif event.option_list.id == "index_selector":
            selected_index = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Index: {selected_index}")
            asyncio.create_task(self.show_index_info(selected_index))  # Run in background

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle pagination and index info display."""
        if event.button.id == "show_index_info":
            await self.show_index_info(self.selected_index)

        elif event.button.id == "next_index_page":
            if (self.current_page + 1) * self.INDICES_PER_PAGE < len(self.all_indices):
                self.current_page += 1
                self.display_indices()

        elif event.button.id == "previous_index_page":
            if self.current_page > 0:
                self.current_page -= 1
                self.display_indices()

    async def show_index_info(self, index_name):
        """Display general information and historical data for the selected index asynchronously."""
        self.app.notify(f"🔄 Fetching details for {index_name}...")
        index_details, history_data = await asyncio.to_thread(self.fetch_index_details, index_name)

        if not index_details:
            self.app.notify("⚠ No details available for the selected index.", severity="warning")
            return

        index_table = self.query_one("#index_details_table", DataTable)
        index_table.clear()

        if not index_table.columns:
            index_table.add_column("Attribute")
            index_table.add_column("Value")

        for key, value in index_details.items():
            index_table.add_row(str(key), str(value))

        if not history_data.empty:
            index_table.add_row("Last 7 Days Price History", "")
            for date, row in history_data.iterrows():
                index_table.add_row(str(date.date()),
                                    f"Open: {row['Open']}, Close: {row['Close']}, High: {row['High']}, Low: {row['Low']}")

        self.app.notify(f"✅ Index details for {index_name} displayed.")

    def fetch_index_details(self, index_name):
        """Fetch details and historical data for a given index synchronously (executed in a separate thread)."""
        try:
            ticker = yf.Ticker(index_name)
            info = ticker.info
            filtered_info = {k: v for k, v in info.items() if k not in ['messageBoardId', 'uuid', 'maxAge']}

            history_data = ticker.history(period='7d')

            if history_data.empty:
                history_data = ticker.history(period='1mo')

            if history_data.empty:
                return filtered_info, pd.DataFrame(columns=['Date', 'Open', 'High', 'Low', 'Close', 'Volume'])

            required_columns = ['Open', 'High', 'Low', 'Close', 'Volume']
            history_table = history_data[required_columns].tail(7)

            return filtered_info, history_table

        except Exception:
            return {}, pd.DataFrame(columns=['Date', 'Open', 'High', 'Low', 'Close', 'Volume'])

