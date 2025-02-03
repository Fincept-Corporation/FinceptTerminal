from textual.containers import VerticalScroll, Horizontal
from textual.app import ComposeResult
from textual.widgets import Static, Collapsible, OptionList, Button, DataTable
import financedatabase as fd
import asyncio


class GlobalFundsTab(VerticalScroll):
    """Custom tab for managing and displaying global funds with pagination."""

    FUNDS_PER_PAGE = 10  # Pagination limit

    def __init__(self):
        super().__init__()
        self.all_funds = []
        self.all_exchanges = []
        self.current_page = 0
        self.is_loaded = False  # Prevents redundant loading

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the Global Funds tab."""
        yield Static("🌍 Select Fund Exchange and Fund:", id="funds-title")

        # Collapsible section for selecting exchange
        with Collapsible(title="Select Exchange", collapsed_symbol=">"):
            yield OptionList(id="exchange_selector")

        # Collapsible section for selecting fund
        with Collapsible(title="Select Fund", collapsed_symbol=">"):
            yield OptionList(id="fund_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_fund_page", variant="default", disabled=True)
                yield Button("Next", id="next_fund_page", variant="default", disabled=True)

        # Button to load fund details
        yield Button("Show Fund Info", id="show_fund_info")

        # DataTable to display fund details
        yield DataTable(name="Fund Details", id="fund_details_table")

    async def on_show(self):
        """Triggered when the tab becomes visible. Loads data only once."""
        if not self.is_loaded:
            self.app.notify("📡 Loading available fund exchanges...")
            await self.populate_exchange_list()
            self.is_loaded = True  # Prevents duplicate loading

    async def populate_exchange_list(self):
        """Fetch and populate the list of available fund exchanges."""
        try:
            self.all_exchanges = await self.fetch_fund_exchanges()
            if not self.all_exchanges:
                self.app.notify("❌ No exchanges available.", severity="error")
                return

            exchange_list = self.query_one("#exchange_selector", OptionList)
            exchange_list.clear_options()
            for exchange in self.all_exchanges:
                exchange_list.add_option(exchange)

            self.app.notify("✅ Fund exchanges loaded successfully!")
        except Exception as e:
            self.app.notify(f"❌ Error loading fund exchanges: {e}", severity="error")

    def display_funds(self):
        """Display the current page of funds in the OptionList."""
        start, end = self._get_pagination_range(self.current_page, self.all_funds)
        fund_list = self.query_one("#fund_selector", OptionList)
        fund_list.clear_options()

        for fund in self.all_funds[start:end]:
            fund_list.add_option(fund.get("name", "Unknown Fund"))

        self._update_pagination_controls(
            "previous_fund_page", "next_fund_page",
            self.current_page, len(self.all_funds)
        )

    async def populate_fund_list(self, exchange):
        """Fetch and populate funds for the selected exchange."""
        try:
            self.all_funds = await self.fetch_funds_by_exchange(exchange)
            if not self.all_funds:
                self.app.notify(f"⚠ No funds available for {exchange}.", severity="warning")
                return

            self.current_page = 0
            self.display_funds()
        except Exception as e:
            self.app.notify(f"❌ Error loading funds: {e}", severity="error")

    async def fetch_fund_exchanges(self):
        """Fetch available fund exchanges from financedatabase."""
        try:
            funds = fd.Funds()
            data = funds.select()
            return sorted(data["exchange"].dropna().unique().tolist()) if not data.empty else []
        except Exception as e:
            self.app.notify(f"❌ Failed to fetch exchanges: {e}", severity="error")
            return []

    async def fetch_funds_by_exchange(self, exchange):
        """Fetch funds filtered by the selected exchange."""
        try:
            funds = fd.Funds()
            data = funds.select()
            filtered_funds = data[data["exchange"] == exchange]

            return filtered_funds.reset_index().to_dict("records") if not filtered_funds.empty else []
        except Exception as e:
            self.app.notify(f"❌ Failed to fetch funds: {e}", severity="error")
            return []

    async def fetch_fund_details(self, fund_name):
        """Fetch details for a specific fund."""
        try:
            funds = fd.Funds()
            data = funds.select()
            fund_row = data[data["name"] == fund_name]

            return fund_row.iloc[0].to_dict() if not fund_row.empty else {}
        except Exception as e:
            self.app.notify(f"❌ Failed to fetch details for {fund_name}: {e}", severity="error")
            return {}

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for exchanges and funds."""
        if event.option_list.id == "exchange_selector":
            selected_exchange = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Exchange: {selected_exchange}")
            await self.populate_fund_list(selected_exchange)

        elif event.option_list.id == "fund_selector":
            selected_fund = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Fund: {selected_fund}")

            # Fetch and display fund details
            await self.show_fund_info(selected_fund)

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle pagination and fund info display."""
        if event.button.id == "show_fund_info" and hasattr(self, "selected_fund"):
            await self.show_fund_info(self.selected_fund)

        elif event.button.id == "next_fund_page":
            if (self.current_page + 1) * self.FUNDS_PER_PAGE < len(self.all_funds):
                self.current_page += 1
                self.display_funds()

        elif event.button.id == "previous_fund_page":
            if self.current_page > 0:
                self.current_page -= 1
                self.display_funds()

    async def show_fund_info(self, fund_name):
        """Display information about the selected fund."""
        fund_details = await self.fetch_fund_details(fund_name)

        if not fund_details:
            self.app.notify("⚠ No details available for the selected fund.", severity="warning")
            return

        # Reference to the DataTable
        fund_table = self.query_one("#fund_details_table", DataTable)
        fund_table.clear()

        # Add columns if not set
        if not fund_table.columns:
            fund_table.add_column("Attribute")
            fund_table.add_column("Value")

        # Add each attribute and value
        for key, value in fund_details.items():
            fund_table.add_row(str(key), str(value))

        self.app.notify(f"✅ Fund details for {fund_name} displayed.")

    def _get_pagination_range(self, current_page, data_list):
        """Calculate start and end indices for pagination."""
        start = current_page * self.FUNDS_PER_PAGE
        end = start + self.FUNDS_PER_PAGE
        return start, min(end, len(data_list))

    def _update_pagination_controls(self, prev_button_id, next_button_id, current_page, total_items):
        """Enable/Disable pagination buttons based on available pages."""
        self.query_one(f"#{prev_button_id}", Button).disabled = current_page == 0
        self.query_one(f"#{next_button_id}", Button).disabled = (current_page + 1) * self.FUNDS_PER_PAGE >= total_items
