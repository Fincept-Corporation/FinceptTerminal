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
        self.loading = False  # Prevents concurrent fetches

    def compose(self) -> ComposeResult:
        """Compose the UI layout for the Global Funds tab."""
        yield Static("🌍 Select Fund Exchange and Fund:", id="funds-title")

        with Collapsible(title="Select Exchange", collapsed_symbol=">"):
            yield OptionList(id="exchange_selector")

        with Collapsible(title="Select Fund", collapsed_symbol=">"):
            yield OptionList(id="fund_selector")
            with Horizontal(classes="horizontal_next_previous"):
                yield Button("Previous", id="previous_fund_page", variant="default", disabled=True)
                yield Button("Next", id="next_fund_page", variant="default", disabled=True)

        yield Button("Show Fund Info", id="show_fund_info")
        yield DataTable(name="Fund Details", id="fund_details_table")

    async def on_show(self):
        """Triggered when the tab becomes visible. Loads data only once."""
        if not self.is_loaded and not self.loading:
            self.loading = True
            self.app.notify("📡 Loading available fund exchanges...")
            asyncio.create_task(self.populate_exchange_list())  # Run in background
            self.is_loaded = True

    async def populate_exchange_list(self):
        """Fetch and populate the list of available fund exchanges asynchronously in a separate thread."""
        try:
            self.all_exchanges = await asyncio.to_thread(self.fetch_fund_exchanges)
            if not self.all_exchanges:
                self.app.notify("❌ No exchanges available.", severity="error")
                return

            exchange_list = self.query_one("#exchange_selector", OptionList)
            exchange_list.clear_options()
            exchange_list.add_options(self.all_exchanges)

            self.app.notify("✅ Fund exchanges loaded successfully!")
        except Exception as e:
            self.app.notify(f"❌ Error loading fund exchanges: {e}", severity="error")
        finally:
            self.loading = False  # Unlock for next request

    def display_funds(self):
        """Display the current page of funds asynchronously."""
        start, end = self._get_pagination_range(self.current_page, self.all_funds)
        fund_list = self.query_one("#fund_selector", OptionList)
        fund_list.clear_options()
        fund_list.add_options([fund.get("name", "Unknown Fund") for fund in self.all_funds[start:end]])

        self._update_pagination_controls(
            "previous_fund_page", "next_fund_page",
            self.current_page, len(self.all_funds)
        )

    def _get_pagination_range(self, current_page: int, items: list):
        """
        Calculate the start and end indices for paginated items.

        Args:
            current_page (int): The current page number.
            items (list): The full list of items to paginate.

        Returns:
            tuple: (start_index, end_index) for slicing the list.
        """
        start_index = current_page * self.FUNDS_PER_PAGE
        end_index = min(start_index + self.FUNDS_PER_PAGE, len(items))
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
        next_button.disabled = (current_page + 1) * self.FUNDS_PER_PAGE >= total_items  # Disable if on the last page

    async def populate_fund_list(self, exchange):
        """Fetch and populate funds for the selected exchange asynchronously in a separate thread."""
        if self.loading:
            self.app.notify("⚠ Already loading funds, please wait...", severity="warning")
            return

        self.loading = True
        self.app.notify(f"🔄 Fetching funds for {exchange}...")
        asyncio.create_task(self._populate_fund_list_task(exchange))

    async def _populate_fund_list_task(self, exchange):
        """Background task to fetch and display funds."""
        try:
            self.all_funds = await asyncio.to_thread(self.fetch_funds_by_exchange, exchange)
            if not self.all_funds:
                self.app.notify(f"⚠ No funds available for {exchange}.", severity="warning")
                return

            self.current_page = 0
            self.display_funds()
        except Exception as e:
            self.app.notify(f"❌ Error loading funds: {e}", severity="error")
        finally:
            self.loading = False  # Unlock for next request

    def fetch_fund_exchanges(self):
        """Fetch available fund exchanges synchronously (executed in a separate thread)."""
        try:
            funds = fd.Funds()
            data = funds.select()
            return sorted(data["exchange"].dropna().unique().tolist()) if not data.empty else []
        except Exception as e:
            return []

    def fetch_funds_by_exchange(self, exchange):
        """Fetch funds filtered by the selected exchange synchronously (executed in a separate thread)."""
        try:
            funds = fd.Funds()
            data = funds.select()
            filtered_funds = data[data["exchange"] == exchange]
            return filtered_funds.reset_index().to_dict("records") if not filtered_funds.empty else []
        except Exception as e:
            return []

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection events for exchanges and funds."""
        if event.option_list.id == "exchange_selector":
            selected_exchange = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Exchange: {selected_exchange}")
            await self.populate_fund_list(selected_exchange)  # Run asynchronously

        elif event.option_list.id == "fund_selector":
            selected_fund = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Fund: {selected_fund}")
            asyncio.create_task(self.show_fund_info(selected_fund))  # Run in background

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle pagination and fund info display."""
        if event.button.id == "show_fund_info":
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
        """Display information about the selected fund asynchronously."""
        self.app.notify(f"🔄 Fetching details for {fund_name}...")
        fund_details = await asyncio.to_thread(self.fetch_fund_details, fund_name)

        if not fund_details:
            self.app.notify("⚠ No details available for the selected fund.", severity="warning")
            return

        fund_table = self.query_one("#fund_details_table", DataTable)
        fund_table.clear()

        if not fund_table.columns:
            fund_table.add_column("Attribute")
            fund_table.add_column("Value")

        fund_table.add_rows([(str(key), str(value)) for key, value in fund_details.items()])
        self.app.notify(f"✅ Fund details for {fund_name} displayed.")

    def fetch_fund_details(self, fund_name):
        """Fetch details for a specific fund synchronously (executed in a separate thread)."""
        try:
            funds = fd.Funds()
            data = funds.select()
            fund_row = data[data["name"] == fund_name]
            return fund_row.iloc[0].to_dict() if not fund_row.empty else {}
        except Exception as e:
            return {}

