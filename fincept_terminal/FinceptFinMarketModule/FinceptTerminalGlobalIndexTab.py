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
    EXCHANGES_PER_PAGE = 10  # Number of exchanges per page

    def __init__(self):
        super().__init__()
        self.all_indices = []  # Stores all fetched indices
        self.all_exchanges = []  # Stores all available exchanges
        self.current_page = 0  # Current page index
        self.exchange_page = 0  # Current page index for exchanges
        self.is_loaded = False
        self.loading = False  # Flag to prevent concurrent fetches

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the Global Indices tab."""

        yield Static("🌍 Select Index Exchange and Index:", id="indices-title")

        with Collapsible(title="Select Exchange", collapsed_symbol=">"):
            yield OptionList(id="exchange_selector")

            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous Exchange", id="previous_exchange_page", variant="default", disabled=True)
                yield Button("Next Exchange", id="next_exchange_page", variant="default", disabled=True)

        with Collapsible(title="Select Index", collapsed_symbol=">"):
            yield OptionList(id="index_selector")

            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_index_page", variant="default", disabled=True)
                yield Button("Next", id="next_index_page", variant="default", disabled=True)

        yield DataTable(name="Index Details", id="index_details_table")
        yield DataTable(name="Historical Data", id="history_table")

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

            self.exchange_page = 0  # Reset pagination when data loads
            self.display_exchanges()
            self.app.notify("✅ Index exchanges loaded successfully!")

        except Exception as e:
            self.app.notify(f"❌ Error loading index exchanges: {e}", severity="error")
        finally:
            self.loading = False

    def display_exchanges(self):
        """Display the current page of exchanges in the OptionList."""
        start_index = self.exchange_page * self.EXCHANGES_PER_PAGE
        end_index = start_index + self.EXCHANGES_PER_PAGE
        exchange_list = self.query_one("#exchange_selector", OptionList)
        exchange_list.clear_options()

        for exchange in self.all_exchanges[start_index:end_index]:
            exchange_list.add_option(exchange)

        self._update_exchange_pagination_controls()

    def _update_exchange_pagination_controls(self):
        """Enable/Disable the exchange pagination buttons based on the current page."""
        previous_btn = self.query_one("#previous_exchange_page", Button)
        next_btn = self.query_one("#next_exchange_page", Button)

        previous_btn.disabled = self.exchange_page == 0
        next_btn.disabled = (self.exchange_page + 1) * self.EXCHANGES_PER_PAGE >= len(self.all_exchanges)

    def display_indices(self):
        """Display the current page of indices in the OptionList."""
        start_index = self.current_page * self.INDICES_PER_PAGE
        end_index = start_index + self.INDICES_PER_PAGE
        index_list = self.query_one("#index_selector", OptionList)
        index_list.clear_options()

        for index in self.all_indices[start_index:end_index]:
            index_name = index.get("name", "Unknown Index")
            index_symbol = index.get("symbol", "N/A")  # Ensure symbol is stored
            index_list.add_option(f"{index_name} ({index_symbol})")  # Store ticker symbol as value

        self._update_index_pagination_controls()

    def _update_index_pagination_controls(self):
        """Enable/Disable the index pagination buttons based on the current page."""
        previous_btn = self.query_one("#previous_index_page", Button)
        next_btn = self.query_one("#next_index_page", Button)

        previous_btn.disabled = self.current_page == 0
        next_btn.disabled = (self.current_page + 1) * self.INDICES_PER_PAGE >= len(self.all_indices)

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
            self.selected_exchange = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Exchange: {self.selected_exchange}")
            await self.populate_index_list(self.selected_exchange)



        elif event.option_list.id == "index_selector":
            # ✅ Get the full text from the selected option (e.g., "S&P BSE SENSEX (^BSESN)")
            selected_text = event.option_list.get_option_at_index(event.option_index).prompt

            # ✅ Extract ticker symbol (anything inside parentheses)
            if "(" in selected_text and ")" in selected_text:
                self.selected_index_symbol = selected_text.split("(")[-1].strip(")")
            else:
                self.selected_index_symbol = selected_text  # Fallback in case no symbol exists

            self.selected_index_name = selected_text  # Store the display name
            self.app.notify(f"✅ Selected Index: {self.selected_index_name} (Symbol: {self.selected_index_symbol})")
            asyncio.create_task(self.show_index_info())  # Run in background

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle pagination and index info display."""

        if event.button.id == "next_exchange_page":
            if (self.exchange_page + 1) * self.EXCHANGES_PER_PAGE < len(self.all_exchanges):
                self.exchange_page += 1
                self.display_exchanges()

        elif event.button.id == "previous_exchange_page":
            if self.exchange_page > 0:
                self.exchange_page -= 1
                self.display_exchanges()

        elif event.button.id == "next_index_page":
            if (self.current_page + 1) * self.INDICES_PER_PAGE < len(self.all_indices):
                self.current_page += 1
                self.display_indices()

        elif event.button.id == "previous_index_page":
            if self.current_page > 0:
                self.current_page -= 1
                self.display_indices()

    async def show_index_info(self):
        """Display general information and historical data for the selected index."""

        if not hasattr(self, "selected_index_symbol") or not self.selected_index_symbol:
            self.app.notify("⚠ No index selected!", severity="warning")
            return

        # ✅ Fetch details and history using the correct ticker symbol
        index_details, history_data = await self.fetch_index_details(self.selected_index_symbol)

        if not index_details:
            self.app.notify(f"⚠ No details available for {self.selected_index_name}.", severity="warning")
            return

        # ✅ Reference to the DataTables
        index_table = self.query_one("#index_details_table", DataTable)
        history_table = self.query_one("#history_table", DataTable)

        # ✅ Clear previous data
        index_table.clear()
        history_table.clear()

        # ✅ Add columns if not set
        if not index_table.columns:
            index_table.add_column("Attribute")
            index_table.add_column("Value")

        # ✅ Populate general info
        for attribute, value in index_details.items():
            index_table.add_row(str(attribute), str(value))

        # ✅ Populate historical data (if available)
        if not history_data.empty:
            if not history_table.columns:
                history_table.add_column("Date")
                history_table.add_column("Open")
                history_table.add_column("High")
                history_table.add_column("Low")
                history_table.add_column("Close")
                history_table.add_column("Volume")

            for date, row in history_data.iterrows():
                history_table.add_row(
                    str(date.date()),
                    f"{row.get('Open', 'N/A')}",
                    f"{row.get('High', 'N/A')}",
                    f"{row.get('Low', 'N/A')}",
                    f"{row.get('Close', 'N/A')}",
                    f"{row.get('Volume', 'N/A')}"
                )

        self.app.notify(f"✅ Index details for {self.selected_index_name} ({self.selected_index_symbol}) displayed.")

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

