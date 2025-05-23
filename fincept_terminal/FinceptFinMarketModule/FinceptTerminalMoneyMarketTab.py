from textual.containers import VerticalScroll, Horizontal
from textual.app import ComposeResult
from textual.widgets import Static, Collapsible, OptionList, Button, DataTable
import financedatabase as fd
import asyncio
import yfinance as yf


class MoneyMarketTab(VerticalScroll):
    """Custom tab for managing and displaying money market data."""

    ITEMS_PER_PAGE = 10  # Pagination limit

    def __init__(self):
        super().__init__()
        self.all_exchanges = []
        self.all_symbols = []
        self.current_exchange_page = 0
        self.current_symbol_page = 0
        self.is_loaded = False  # Prevents redundant loading
        self.loading = False  # Prevents concurrent fetches

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the Money Market tab."""
        yield Static("💵 Money Market Data", id="money-market-title")

        with Collapsible(title="Select Exchange", collapsed_symbol=">"):
            yield OptionList(id="exchange_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_exchange_page", variant="default", disabled=True)
                yield Button("Next", id="next_exchange_page", variant="default", disabled=True)

        with Collapsible(title="Select Symbol", collapsed_symbol=">"):
            yield OptionList(id="symbol_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_symbol_page", variant="default", disabled=True)
                yield Button("Next", id="next_symbol_page", variant="default", disabled=True)

        yield DataTable(name="Symbol Data", id="money_market_data_table")

    async def on_show(self):
        """Triggered when the tab becomes visible. Loads data only once."""
        if not self.is_loaded and not self.loading:
            self.loading = True
            self.app.notify("📡 Loading available money market exchanges...")
            asyncio.create_task(self.populate_exchange_list())  # Run in background
            self.is_loaded = True

    async def populate_exchange_list(self):
        """Fetch and populate the list of money market exchanges asynchronously in a separate thread."""
        try:
            self.all_exchanges = await asyncio.to_thread(self.fetch_money_market_exchanges)
            exchange_list = self.query_one("#exchange_selector", OptionList)
            exchange_list.clear_options()

            if not self.all_exchanges:
                self.app.notify("❌ No exchanges available.", severity="error")
                return

            exchange_list.add_options(self.all_exchanges)
            self.display_exchanges()

            self.app.notify("✅ Money market exchanges loaded successfully!")

        except Exception as e:
            self.app.notify(f"❌ Error loading exchanges: {e}", severity="error")
        finally:
            self.loading = False

    def display_exchanges(self):
        """Display the current page of exchanges in the OptionList."""
        start, end = self._get_pagination_range(self.current_exchange_page, self.all_exchanges)
        exchange_list = self.query_one("#exchange_selector", OptionList)
        exchange_list.clear_options()

        for exchange in self.all_exchanges[start:end]:
            exchange_list.add_option(exchange)

        self._update_pagination_controls("previous_exchange_page", "next_exchange_page",
                                         self.current_exchange_page, len(self.all_exchanges))

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

    async def populate_symbol_list(self, exchange):
        """Fetch and populate symbols for a selected exchange asynchronously."""
        if self.loading:
            self.app.notify("⚠ Already loading symbols, please wait...", severity="warning")
            return

        self.loading = True
        self.app.notify(f"🔄 Fetching symbols for {exchange}...")
        asyncio.create_task(self._populate_symbol_list_task(exchange))

    async def _populate_symbol_list_task(self, exchange):
        """Background task to fetch and display symbols."""
        try:
            self.all_symbols = await asyncio.to_thread(self.fetch_money_market_symbols_by_exchange, exchange)
            if not self.all_symbols:
                self.app.notify(f"⚠ No symbols found for {exchange}.", severity="warning")
                return

            self.current_symbol_page = 0
            self.display_symbols()

        except Exception as e:
            self.app.notify(f"❌ Error loading symbols: {e}", severity="error")
        finally:
            self.loading = False

    def display_symbols(self):
        """Display the current page of symbols in the OptionList."""
        start, end = self._get_pagination_range(self.current_symbol_page, self.all_symbols)
        symbol_list = self.query_one("#symbol_selector", OptionList)
        symbol_list.clear_options()

        for symbol in self.all_symbols[start:end]:
            symbol_list.add_option(f"{symbol.get('symbol', 'N/A')} - {symbol.get('name', 'Unknown')}")

        self._update_pagination_controls("previous_symbol_page", "next_symbol_page",
                                         self.current_symbol_page, len(self.all_symbols))

    def fetch_money_market_exchanges(self):
        """Fetch available money market exchanges synchronously (executed in a separate thread)."""
        try:
            moneymarkets = fd.Moneymarkets()
            data = moneymarkets.select()
            return sorted(data["exchange"].dropna().unique().tolist()) if not data.empty else []
        except Exception:
            return []

    def fetch_money_market_symbols_by_exchange(self, exchange):
        """Fetch symbols for a specific exchange synchronously (executed in a separate thread)."""
        try:
            moneymarkets = fd.Moneymarkets()
            data = moneymarkets.select()
            symbols = data[data["exchange"] == exchange].reset_index()

            return symbols.to_dict("records") if not symbols.empty else []
        except Exception:
            return []

    async def fetch_money_market_data_by_symbol(self, symbol):
        """Fetch detailed money market symbol data asynchronously using `asyncio.to_thread`."""
        return await asyncio.to_thread(self._fetch_money_market_data_by_symbol, symbol)

    def _fetch_money_market_data_by_symbol(self, symbol):
        """Fetch detailed money market symbol data synchronously using yfinance."""
        try:
            ticker = yf.Ticker(symbol)
            return {k: v for k, v in ticker.info.items() if k not in ["messageBoardId", "gmtOffSetMilliseconds", "maxAge"]}
        except Exception:
            return {}

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle pagination buttons for navigating exchanges and symbols."""
        if event.button.id == "next_exchange_page":
            self.current_exchange_page += 1
            self.display_exchanges()
        elif event.button.id == "previous_exchange_page":
            self.current_exchange_page -= 1
            self.display_exchanges()
        elif event.button.id == "next_symbol_page":
            self.current_symbol_page += 1
            self.display_symbols()
        elif event.button.id == "previous_symbol_page":
            self.current_symbol_page -= 1
            self.display_symbols()

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for exchanges and symbols."""
        if event.option_list.id == "exchange_selector":
            selected_exchange = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Exchange: {selected_exchange}")
            await self.populate_symbol_list(selected_exchange)

        elif event.option_list.id == "symbol_selector":
            selected_symbol = event.option_list.get_option_at_index(event.option_index).prompt
            symbol_key = selected_symbol.split(" - ")[0]  # Extract the symbol
            self.app.notify(f"✅ Fetching details for {symbol_key}...")
            data = await self.fetch_money_market_data_by_symbol(symbol_key)

            data_table = self.query_one("#money_market_data_table", DataTable)
            data_table.clear()
            if not data_table.columns:
                data_table.add_column("Attribute")
                data_table.add_column("Value")
            for key, value in data.items():
                data_table.add_row(key, str(value))

