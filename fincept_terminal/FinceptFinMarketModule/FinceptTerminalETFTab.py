from textual.containers import VerticalScroll, Horizontal
from textual.app import ComposeResult
from textual.widgets import Static, Collapsible, OptionList, Button, DataTable
import financedatabase as fd
import asyncio
import yfinance as yf


class ETFMarketTab(VerticalScroll):
    """Custom tab for managing and displaying Exchange Traded Funds (ETFs)."""

    ITEMS_PER_PAGE = 10  # Pagination limit

    def __init__(self):
        super().__init__()
        self.all_families = []
        self.all_etfs = []
        self.current_family_page = 0
        self.current_etf_page = 0
        self.is_loaded = False  # Prevents redundant loading
        self.loading = False  # Prevents concurrent fetches

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the ETF Market tab."""
        yield Static("📈 Exchange Traded Funds (ETF) Market", id="etf-market-title")

        with Collapsible(title="Select ETF Family", collapsed_symbol=">"):
            yield OptionList(id="family_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_family_page", variant="default", disabled=True)
                yield Button("Next", id="next_family_page", variant="default", disabled=True)

        with Collapsible(title="Select ETF", collapsed_symbol=">"):
            yield OptionList(id="etf_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_etf_page", variant="default", disabled=True)
                yield Button("Next", id="next_etf_page", variant="default", disabled=True)

        yield DataTable(name="ETF Details", id="etf_market_data_table")

    async def on_show(self):
        """Triggered when the tab becomes visible. Loads data only once."""
        if not self.is_loaded and not self.loading:
            self.loading = True
            self.app.notify("📡 Loading available ETF families...")
            asyncio.create_task(self.populate_family_list())  # Run in background
            self.is_loaded = True

    async def populate_family_list(self):
        """Fetch and populate the list of ETF families asynchronously in a separate thread."""
        try:
            self.all_families = await asyncio.to_thread(self.fetch_etf_families)
            family_list = self.query_one("#family_selector", OptionList)
            family_list.clear_options()

            if not self.all_families:
                self.app.notify("❌ No ETF families available.", severity="error")
                return

            family_list.add_options(self.all_families)
            self.display_families()

            self.app.notify("✅ ETF families loaded successfully!")

        except Exception as e:
            self.app.notify(f"❌ Error loading ETF families: {e}", severity="error")
        finally:
            self.loading = False

    def display_families(self):
        """Display the current page of ETF families in the OptionList."""
        start, end = self._get_pagination_range(self.current_family_page, self.all_families)
        family_list = self.query_one("#family_selector", OptionList)
        family_list.clear_options()

        for family in self.all_families[start:end]:
            family_list.add_option(family)

        self._update_pagination_controls("previous_family_page", "next_family_page",
                                         self.current_family_page, len(self.all_families))

    async def populate_etf_list(self, family):
        """Fetch and populate ETFs for a selected ETF family asynchronously."""
        if self.loading:
            self.app.notify("⚠ Already loading ETFs, please wait...", severity="warning")
            return

        self.loading = True
        self.app.notify(f"🔄 Fetching ETFs for {family}...")
        asyncio.create_task(self._populate_etf_list_task(family))

    async def _populate_etf_list_task(self, family):
        """Background task to fetch and display ETFs."""
        try:
            self.all_etfs = await asyncio.to_thread(self.fetch_etfs_by_family, family)
            if not self.all_etfs:
                self.app.notify(f"⚠ No ETFs found for {family}.", severity="warning")
                return

            self.current_etf_page = 0
            self.display_etfs()

        except Exception as e:
            self.app.notify(f"❌ Error loading ETFs: {e}", severity="error")
        finally:
            self.loading = False

    def display_etfs(self):
        """Display the current page of ETFs in the OptionList."""
        start, end = self._get_pagination_range(self.current_etf_page, self.all_etfs)
        etf_list = self.query_one("#etf_selector", OptionList)
        etf_list.clear_options()

        for etf in self.all_etfs[start:end]:
            etf_list.add_option(f"{etf.get('symbol', 'N/A')} - {etf.get('name', 'Unknown')}")

        self._update_pagination_controls("previous_etf_page", "next_etf_page",
                                         self.current_etf_page, len(self.all_etfs))

    def fetch_etf_families(self):
        """Fetch available ETF families synchronously (executed in a separate thread)."""
        try:
            etfs = fd.ETFs()
            data = etfs.select()
            return sorted(data["family"].dropna().unique().tolist()) if not data.empty else []
        except Exception:
            return []

    def fetch_etfs_by_family(self, family):
        """Fetch ETFs for a specific ETF family synchronously (executed in a separate thread)."""
        try:
            etfs = fd.ETFs()
            data = etfs.select(family=family)
            return data.reset_index().to_dict("records") if not data.empty else []
        except Exception:
            return []

    async def fetch_etf_details(self, symbol):
        """Fetch ETF details asynchronously using `asyncio.to_thread`."""
        return await asyncio.to_thread(self._fetch_etf_details, symbol)

    def _fetch_etf_details(self, symbol):
        """Fetch ETF details synchronously using yfinance."""
        try:
            ticker = yf.Ticker(symbol)
            return {k: v for k, v in ticker.info.items() if k not in ["messageBoardId", "gmtOffSetMilliseconds", "maxAge"]}
        except Exception:
            return {}

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle pagination buttons for navigating ETF families and ETFs."""
        if event.button.id == "next_family_page":
            self.current_family_page += 1
            self.display_families()
        elif event.button.id == "previous_family_page":
            self.current_family_page -= 1
            self.display_families()
        elif event.button.id == "next_etf_page":
            self.current_etf_page += 1
            self.display_etfs()
        elif event.button.id == "previous_etf_page":
            self.current_etf_page -= 1
            self.display_etfs()

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for ETF families and ETFs."""
        if event.option_list.id == "family_selector":
            selected_family = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected ETF Family: {selected_family}")
            await self.populate_etf_list(selected_family)

        elif event.option_list.id == "etf_selector":
            selected_etf = event.option_list.get_option_at_index(event.option_index).prompt
            symbol = selected_etf.split(" - ")[0]  # Extract the symbol
            self.app.notify(f"✅ Fetching details for {symbol}...")
            data = await self.fetch_etf_details(symbol)

            data_table = self.query_one("#etf_market_data_table", DataTable)
            data_table.clear()
            if not data_table.columns:
                data_table.add_column("Attribute")
                data_table.add_column("Value")
            for key, value in data.items():
                data_table.add_row(key, str(value))

