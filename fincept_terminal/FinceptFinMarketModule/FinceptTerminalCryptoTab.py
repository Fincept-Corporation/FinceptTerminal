from textual.containers import VerticalScroll, Horizontal
from textual.app import ComposeResult
from textual.widgets import Static, Collapsible, OptionList, Button, DataTable
import financedatabase as fd
import asyncio
import yfinance as yf


class CryptoMarketTab(VerticalScroll):
    """Custom tab for managing and displaying cryptocurrency market data."""

    ITEMS_PER_PAGE = 10  # Pagination limit

    def __init__(self):
        super().__init__()
        self.all_currencies = []
        self.all_cryptos = []
        self.current_currency_page = 0
        self.current_crypto_page = 0
        self.is_loaded = False  # Prevents redundant loading
        self.loading = False  # Prevents concurrent fetches

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the Cryptocurrency Market tab."""
        yield Static("🔗 Cryptocurrency Market", id="crypto-market-title")

        with Collapsible(title="Select Base Currency", collapsed_symbol=">"):
            yield OptionList(id="currency_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_currency_page", variant="default", disabled=True)
                yield Button("Next", id="next_currency_page", variant="default", disabled=True)

        with Collapsible(title="Select Cryptocurrency", collapsed_symbol=">"):
            yield OptionList(id="crypto_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_crypto_page", variant="default", disabled=True)
                yield Button("Next", id="next_crypto_page", variant="default", disabled=True)

        yield DataTable(name="Crypto Details", id="crypto_market_data_table")

    async def on_show(self):
        """Triggered when the tab becomes visible. Loads data only once."""
        if not self.is_loaded and not self.loading:
            self.loading = True
            self.app.notify("📡 Loading available currencies...")
            asyncio.create_task(self.populate_currency_list())  # Run in background
            self.is_loaded = True

    async def populate_currency_list(self):
        """Fetch and populate the list of available base currencies asynchronously in a separate thread."""
        try:
            self.all_currencies = await asyncio.to_thread(self.fetch_available_currencies)
            currency_list = self.query_one("#currency_selector", OptionList)
            currency_list.clear_options()

            if not self.all_currencies:
                self.app.notify("❌ No base currencies available.", severity="error")
                return

            currency_list.add_options(self.all_currencies)
            self.display_currencies()

            self.app.notify("✅ Currencies loaded successfully!")

        except Exception as e:
            self.app.notify(f"❌ Error loading currencies: {e}", severity="error")
        finally:
            self.loading = False

    def display_currencies(self):
        """Display the current page of base currencies in the OptionList."""
        start, end = self._get_pagination_range(self.current_currency_page, self.all_currencies)
        currency_list = self.query_one("#currency_selector", OptionList)
        currency_list.clear_options()

        for currency in self.all_currencies[start:end]:
            currency_list.add_option(currency)

        self._update_pagination_controls("previous_currency_page", "next_currency_page",
                                         self.current_currency_page, len(self.all_currencies))

    async def populate_crypto_list(self, currency):
        """Fetch and populate cryptocurrencies for a selected base currency asynchronously."""
        if self.loading:
            self.app.notify("⚠ Already loading cryptos, please wait...", severity="warning")
            return

        self.loading = True
        self.app.notify(f"🔄 Fetching cryptos for {currency}...")
        asyncio.create_task(self._populate_crypto_list_task(currency))

    async def _populate_crypto_list_task(self, currency):
        """Background task to fetch and display cryptocurrencies."""
        try:
            self.all_cryptos = await asyncio.to_thread(self.fetch_cryptos_by_currency, currency)
            if not self.all_cryptos:
                self.app.notify(f"⚠ No cryptos found for {currency}.", severity="warning")
                return

            self.current_crypto_page = 0
            self.display_cryptos()

        except Exception as e:
            self.app.notify(f"❌ Error loading cryptos: {e}", severity="error")
        finally:
            self.loading = False

    def display_cryptos(self):
        """Display the current page of cryptocurrencies in the OptionList."""
        start, end = self._get_pagination_range(self.current_crypto_page, self.all_cryptos)
        crypto_list = self.query_one("#crypto_selector", OptionList)
        crypto_list.clear_options()

        for crypto in self.all_cryptos[start:end]:
            crypto_list.add_option(f"{crypto.get('symbol', 'N/A')} - {crypto.get('name', 'Unknown')}")

        self._update_pagination_controls("previous_crypto_page", "next_crypto_page",
                                         self.current_crypto_page, len(self.all_cryptos))

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

    def fetch_available_currencies(self):
        """Fetch available base currencies synchronously (executed in a separate thread)."""
        try:
            cryptos = fd.Cryptos()
            data = cryptos.select()
            return sorted(data["currency"].dropna().unique().tolist()) if not data.empty else []
        except Exception:
            return []

    def fetch_cryptos_by_currency(self, currency):
        """Fetch cryptocurrencies for a specific base currency synchronously (executed in a separate thread)."""
        try:
            cryptos = fd.Cryptos()
            data = cryptos.select(currency=currency)
            return data.reset_index().to_dict("records") if not data.empty else []
        except Exception:
            return []

    async def fetch_crypto_details(self, symbol):
        """Fetch crypto details asynchronously using `asyncio.to_thread`."""
        return await asyncio.to_thread(self._fetch_crypto_details, symbol)

    def _fetch_crypto_details(self, symbol):
        """Fetch crypto details synchronously using yfinance."""
        try:
            ticker = yf.Ticker(symbol)
            return {k: v for k, v in ticker.info.items() if k not in ["messageBoardId", "gmtOffSetMilliseconds", "maxAge"]}
        except Exception:
            return {}

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle pagination buttons for navigating currencies and cryptos."""
        if event.button.id == "next_currency_page":
            self.current_currency_page += 1
            self.display_currencies()
        elif event.button.id == "previous_currency_page":
            self.current_currency_page -= 1
            self.display_currencies()
        elif event.button.id == "next_crypto_page":
            self.current_crypto_page += 1
            self.display_cryptos()
        elif event.button.id == "previous_crypto_page":
            self.current_crypto_page -= 1
            self.display_cryptos()

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for base currencies and cryptocurrencies."""
        if event.option_list.id == "currency_selector":
            selected_currency = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Currency: {selected_currency}")
            await self.populate_crypto_list(selected_currency)

        elif event.option_list.id == "crypto_selector":
            selected_crypto = event.option_list.get_option_at_index(event.option_index).prompt
            symbol = selected_crypto.split(" - ")[0]  # Extract the symbol
            self.app.notify(f"✅ Fetching details for {symbol}...")
            data = await self.fetch_crypto_details(symbol)

            data_table = self.query_one("#crypto_market_data_table", DataTable)
            data_table.clear()
            if not data_table.columns:
                data_table.add_column("Attribute")
                data_table.add_column("Value")
            for key, value in data.items():
                data_table.add_row(key, str(value))

