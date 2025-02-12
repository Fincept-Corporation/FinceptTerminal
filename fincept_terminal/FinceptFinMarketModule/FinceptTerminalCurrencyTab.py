from textual.containers import VerticalScroll, Horizontal
from textual.app import ComposeResult
from textual.widgets import Static, Collapsible, OptionList, Button, DataTable
import financedatabase as fd
import asyncio
import yfinance as yf

class CurrencyForexTab(VerticalScroll):
    """Custom tab for managing and displaying Currency Forex data."""

    ITEMS_PER_PAGE = 10  # Pagination limit

    def __init__(self):
        super().__init__()
        self.all_base_currencies = []
        self.all_currency_pairs = []
        self.current_base_page = 0
        self.current_pair_page = 0
        self.is_loaded = False  # Prevents redundant loading
        self.loading = False  # Prevents concurrent fetches

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the Currency Forex tab."""
        yield Static("💱 Currency Forex Data", id="currency-forex-title")

        with Collapsible(title="Select Base Currency", collapsed_symbol=">"):
            yield OptionList(id="base_currency_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_base_page", variant="default", disabled=True)
                yield Button("Next", id="next_base_page", variant="default", disabled=True)

        with Collapsible(title="Select Currency Pair", collapsed_symbol=">"):
            yield OptionList(id="currency_pair_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_pair_page", variant="default", disabled=True)
                yield Button("Next", id="next_pair_page", variant="default", disabled=True)

        yield DataTable(name="Currency Pair Details", id="currency_forex_data_table")

    async def on_show(self):
        """Triggered when the tab becomes visible. Loads data only once."""
        if not self.is_loaded and not self.loading:
            self.loading = True
            self.app.notify("📡 Loading available base currencies...")
            asyncio.create_task(self.populate_base_currency_list())  # Run in background
            self.is_loaded = True

    async def populate_base_currency_list(self):
        """Fetch and populate the list of base currencies asynchronously in a separate thread."""
        try:
            self.all_base_currencies = await asyncio.to_thread(self.fetch_base_currencies)
            currency_list = self.query_one("#base_currency_selector", OptionList)
            currency_list.clear_options()

            if not self.all_base_currencies:
                self.app.notify("❌ No base currencies available.", severity="error")
                return

            currency_list.add_options(self.all_base_currencies)
            self.display_base_currencies()

            self.app.notify("✅ Base currencies loaded successfully!")

        except Exception as e:
            self.app.notify(f"❌ Error loading base currencies: {e}", severity="error")
        finally:
            self.loading = False

    def display_base_currencies(self):
        """Display the current page of base currencies in the OptionList."""
        start, end = self._get_pagination_range(self.current_base_page, self.all_base_currencies)
        currency_list = self.query_one("#base_currency_selector", OptionList)
        currency_list.clear_options()

        for currency in self.all_base_currencies[start:end]:
            currency_list.add_option(currency)

        self._update_pagination_controls("previous_base_page", "next_base_page",
                                         self.current_base_page, len(self.all_base_currencies))

    async def populate_currency_pair_list(self, base_currency):
        """Fetch and populate currency pairs for a selected base currency asynchronously."""
        if self.loading:
            self.app.notify("⚠ Already loading currency pairs, please wait...", severity="warning")
            return

        self.loading = True
        self.app.notify(f"🔄 Fetching currency pairs for {base_currency}...")
        asyncio.create_task(self._populate_currency_pair_list_task(base_currency))

    async def _populate_currency_pair_list_task(self, base_currency):
        """Background task to fetch and display currency pairs."""
        try:
            self.all_currency_pairs = await asyncio.to_thread(self.fetch_currency_pairs, base_currency)
            if not self.all_currency_pairs:
                self.app.notify(f"⚠ No currency pairs found for {base_currency}.", severity="warning")
                return

            self.current_pair_page = 0
            self.display_currency_pairs()

        except Exception as e:
            self.app.notify(f"❌ Error loading currency pairs: {e}", severity="error")
        finally:
            self.loading = False

    def display_currency_pairs(self):
        """Display the current page of currency pairs in the OptionList."""
        start, end = self._get_pagination_range(self.current_pair_page, self.all_currency_pairs)
        pair_list = self.query_one("#currency_pair_selector", OptionList)
        pair_list.clear_options()

        for pair in self.all_currency_pairs[start:end]:
            pair_list.add_option(f"{pair.get('symbol', 'N/A')} - {pair.get('name', 'Unknown')}")

        self._update_pagination_controls("previous_pair_page", "next_pair_page",
                                         self.current_pair_page, len(self.all_currency_pairs))

    def _get_pagination_range(self, current_page: int, items: list):
        """
        Calculate the start and end indices for paginated items.

        Args:
            current_page (int): The current page number.
            items (list): The full list of items to paginate.

        Returns:
            tuple: (start_index, end_index) for slicing the list.
        """
        start_index = current_page * self.ITEMS_PER_PAGE
        end_index = min(start_index + self.ITEMS_PER_PAGE, len(items))
        return start_index, end_index

    def _update_pagination_controls(self, prev_button_id: str, next_button_id: str, current_page: int,
                                    total_items: int):
        """
        Enable or disable pagination buttons based on the current page.

        Args:
            prev_button_id (str): ID of the previous page button.
            next_button_id (str): ID of the next page button.
            current_page (int): The current page number.
            total_items (int): The total number of items in the list.
        """
        prev_button = self.query_one(f"#{prev_button_id}", Button)
        next_button = self.query_one(f"#{next_button_id}", Button)

        prev_button.disabled = current_page == 0  # Disable if on the first page
        next_button.disabled = (current_page + 1) * self.ITEMS_PER_PAGE >= total_items  # Disable if on the last page

    def fetch_base_currencies(self):
        """Fetch available base currencies synchronously (executed in a separate thread)."""
        try:
            currencies = fd.Currencies()
            data = currencies.select()
            return sorted(data["base_currency"].dropna().unique().tolist()) if not data.empty else []
        except Exception:
            return []

    def fetch_currency_pairs(self, base_currency):
        """Fetch currency pairs for a specific base currency synchronously (executed in a separate thread)."""
        try:
            currencies = fd.Currencies()
            data = currencies.select(base_currency=base_currency)
            return data.reset_index().to_dict("records") if not data.empty else []
        except Exception:
            return []

    async def fetch_currency_pair_details(self, symbol):
        """Fetch currency pair details asynchronously using `asyncio.to_thread`."""
        return await asyncio.to_thread(self._fetch_currency_pair_details, symbol)

    def _fetch_currency_pair_details(self, symbol):
        """Fetch currency pair details synchronously using yfinance."""
        try:
            ticker = yf.Ticker(symbol)
            return {k: v for k, v in ticker.info.items() if k not in ["messageBoardId", "gmtOffSetMilliseconds", "maxAge"]}
        except Exception:
            return {}

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle pagination buttons for navigating base currencies and currency pairs."""
        if event.button.id == "next_base_page":
            self.current_base_page += 1
            self.display_base_currencies()
        elif event.button.id == "previous_base_page":
            self.current_base_page -= 1
            self.display_base_currencies()
        elif event.button.id == "next_pair_page":
            self.current_pair_page += 1
            self.display_currency_pairs()
        elif event.button.id == "previous_pair_page":
            self.current_pair_page -= 1
            self.display_currency_pairs()

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for base currencies and currency pairs."""
        if event.option_list.id == "base_currency_selector":
            selected_base_currency = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Base Currency: {selected_base_currency}")
            await self.populate_currency_pair_list(selected_base_currency)

        elif event.option_list.id == "currency_pair_selector":
            selected_pair = event.option_list.get_option_at_index(event.option_index).prompt
            symbol = selected_pair.split(" - ")[0]  # Extract the symbol
            self.app.notify(f"✅ Fetching details for {symbol}...")
            data = await self.fetch_currency_pair_details(symbol)

            data_table = self.query_one("#currency_forex_data_table", DataTable)
            data_table.clear()
            if not data_table.columns:
                data_table.add_column("Attribute")
                data_table.add_column("Value")
            for key, value in data.items():
                data_table.add_row(key, str(value))

