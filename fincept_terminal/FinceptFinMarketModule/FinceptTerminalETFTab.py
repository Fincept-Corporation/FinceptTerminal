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

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the ETF Market tab."""
        yield Static("📈 Exchange Traded Funds (ETF) Market", id="etf-market-title")

        # Collapsible section for selecting ETF family
        with Collapsible(title="Select ETF Family", collapsed_symbol=">"):
            yield OptionList(id="family_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_family_page", variant="default", disabled=True)
                yield Button("Next", id="next_family_page", variant="default", disabled=True)

        # Collapsible section for selecting ETF
        with Collapsible(title="Select ETF", collapsed_symbol=">"):
            yield OptionList(id="etf_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_etf_page", variant="default", disabled=True)
                yield Button("Next", id="next_etf_page", variant="default", disabled=True)

        # DataTable to display ETF details
        yield DataTable(name="ETF Details", id="etf_market_data_table")

    async def on_show(self):
        """Triggered when the tab becomes visible. Loads data only once."""
        if not self.is_loaded:
            self.app.notify("📡 Loading available ETF families...")
            await self.populate_family_list()
            self.is_loaded = True  # Prevents duplicate loading

    async def populate_family_list(self):
        """Fetch and populate the list of ETF families."""
        try:
            self.all_families = await self.fetch_etf_families()
            if not self.all_families:
                self.app.notify("❌ No ETF families available.", severity="error")
                return

            self.current_family_page = 0
            self.display_families()
        except Exception as e:
            self.app.notify(f"❌ Error loading ETF families: {e}", severity="error")

    def display_families(self):
        """Display the current page of ETF families in the OptionList."""
        start, end = self._get_pagination_range(self.current_family_page, self.all_families)
        family_list = self.query_one("#family_selector", OptionList)
        family_list.clear_options()

        for family in self.all_families[start:end]:
            family_list.add_option(family)

        self._update_pagination_controls(
            "previous_family_page", "next_family_page",
            self.current_family_page, len(self.all_families)
        )

    async def populate_etf_list(self, family):
        """Fetch and populate ETFs for a selected ETF family."""
        try:
            self.all_etfs = await self.fetch_etfs_by_family(family)
            if not self.all_etfs:
                self.app.notify(f"⚠ No ETFs found for {family}.", severity="warning")
                return

            self.current_etf_page = 0
            self.display_etfs()
        except Exception as e:
            self.app.notify(f"❌ Error loading ETFs: {e}", severity="error")

    def display_etfs(self):
        """Display the current page of ETFs in the OptionList."""
        start, end = self._get_pagination_range(self.current_etf_page, self.all_etfs)
        etf_list = self.query_one("#etf_selector", OptionList)
        etf_list.clear_options()

        for etf in self.all_etfs[start:end]:
            etf_list.add_option(f"{etf.get('symbol', 'N/A')} - {etf.get('name', 'Unknown')}")

        self._update_pagination_controls(
            "previous_etf_page", "next_etf_page",
            self.current_etf_page, len(self.all_etfs)
        )

    async def fetch_etf_families(self):
        """Fetch available ETF families from financedatabase."""
        try:
            etfs = fd.ETFs()
            data = etfs.select()
            return sorted(data["family"].dropna().unique().tolist()) if not data.empty else []
        except Exception as e:
            self.app.notify(f"❌ Failed to fetch ETF families: {e}", severity="error")
            return []

    async def fetch_etfs_by_family(self, family):
        """Fetch ETFs for a specific ETF family."""
        try:
            etfs = fd.ETFs()
            data = etfs.select(family=family)
            return data.reset_index().to_dict("records") if not data.empty else []
        except Exception as e:
            self.app.notify(f"❌ Failed to fetch ETFs: {e}", severity="error")
            return []

    async def fetch_etf_details(self, symbol):
        """Fetch ETF details from yfinance."""
        try:
            ticker = yf.Ticker(symbol)
            return {k: v for k, v in ticker.info.items() if k not in ["messageBoardId", "gmtOffSetMilliseconds", "maxAge"]}
        except Exception as e:
            self.app.notify(f"❌ Failed to fetch {symbol} details: {e}", severity="error")
            return {}

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle button presses for pagination."""
        if event.button.id in ["next_family_page", "previous_family_page"]:
            self.current_family_page += 1 if "next" in event.button.id else -1
            self.display_families()
        elif event.button.id in ["next_etf_page", "previous_etf_page"]:
            self.current_etf_page += 1 if "next" in event.button.id else -1
            self.display_etfs()

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for ETF families and ETFs."""
        if event.option_list.id == "family_selector":
            selected_family = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected ETF Family: {selected_family}")
            await self.populate_etf_list(selected_family)

        elif event.option_list.id == "etf_selector":
            selected_etf = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected ETF: {selected_etf}")

            symbol = selected_etf.split(" - ")[0]  # Extract the symbol
            data = await self.fetch_etf_details(symbol)

            # Display data in DataTable
            data_table = self.query_one("#etf_market_data_table", DataTable)
            data_table.clear()
            if not data_table.columns:
                data_table.add_column("Attribute")
                data_table.add_column("Value")
            for key, value in data.items():
                data_table.add_row(key, str(value))

    def _get_pagination_range(self, current_page, data_list):
        """Calculate start and end indices for pagination."""
        start = current_page * self.ITEMS_PER_PAGE
        end = start + self.ITEMS_PER_PAGE
        return start, min(end, len(data_list))

    def _update_pagination_controls(self, prev_button_id, next_button_id, current_page, total_items):
        """Enable/Disable pagination buttons based on available pages."""
        self.query_one(f"#{prev_button_id}", Button).disabled = current_page == 0
        self.query_one(f"#{next_button_id}", Button).disabled = (current_page + 1) * self.ITEMS_PER_PAGE >= total_items
