from textual.app import ComposeResult
from textual.containers import Container, VerticalScroll, Horizontal
from textual.widgets import Static, OptionList, DataTable, Collapsible, Button
import asyncio
import financedatabase as fd  # FinanceDatabase for fetching stocks
import yfinance as yf


class StockTrackerTab(Container):
    """Stock Tracker for fetching all stocks by country and displaying them sector-wise."""

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.all_countries = []  # Stores the full list of available countries
        self.current_page = 0  # Pagination control for country selection
        self.page_size = 10  # Number of countries per page
        self.selected_country = None
        self.market_tables = {}  # Stores DataTable references

    def compose(self) -> ComposeResult:
        """Compose the stock tracker UI."""
        yield Static("📈 Stock Tracker", classes="tracker_header")

        # ✅ Country Selection Section with Pagination Controls
        with Collapsible(title="🌍 Select Country", collapsed_symbol=">"):
            yield OptionList(id="country_selector")
            with Horizontal(id="country_controls", classes="horizontal_next_previous"):
                yield Button("⬅ Prev", id="prev_page", classes="page_button")
                yield Button("➡ Next", id="next_page", classes="page_button")

        # ✅ Stocks Display Section (Sector-wise)
        with VerticalScroll(id="stocks_display", classes="stocks_display"):
            yield Static("📊 Stocks Data", classes="stocks_header")

    async def on_show(self):
        """Triggered when the tab becomes visible. Load country data if not already loaded."""
        if not hasattr(self, "countries_loaded"):
            self.app.notify("Loading Countries...")
            await asyncio.create_task(self.load_country_list())
            self.countries_loaded = True

    async def load_country_list(self):
        """Fetch and store the list of available countries asynchronously."""
        try:
            indices = await asyncio.to_thread(fd.Equities)
            self.all_countries = sorted(set(indices.select()["country"].dropna().unique()))

            if not self.all_countries:
                self.app.notify("❌ No countries found!", severity="error")
                return

            # ✅ Load the first page of countries
            await self.update_country_list()
            self.app.notify("✅ Countries loaded successfully!")

        except Exception as e:
            self.app.notify(f"❌ Error loading countries: {e}", severity="error")

    async def update_country_list(self):
        """Update the option list with a paginated subset of countries."""
        country_list = self.query_one("#country_selector", OptionList)
        country_list.clear_options()

        start_index = self.current_page * self.page_size
        end_index = start_index + self.page_size
        paginated_countries = self.all_countries[start_index:end_index]

        for country in paginated_countries:
            country_list.add_option(country)

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection event for countries."""
        if event.option_list.id == "country_selector":
            self.selected_country = event.option_list.get_option_at_index(event.option_index).prompt
            self.app.notify(f"✅ Selected Country: {self.selected_country}")
            await asyncio.create_task(self.fetch_and_display_stocks())

    async def on_button_pressed(self, event):
        """Handle pagination button clicks."""
        if event.button.id == "prev_page" and self.current_page > 0:
            self.current_page -= 1
        elif event.button.id == "next_page" and (self.current_page + 1) * self.page_size < len(self.all_countries):
            self.current_page += 1

        await self.update_country_list()

    async def fetch_and_display_stocks(self):
        """Fetch all stocks for the selected country, get OHLC & volume from yfinance, and display them sector-wise."""
        if not self.selected_country:
            self.app.notify("⚠ Please select a country first!", severity="warning")
            return

        self.app.notify(f"🔄 Fetching stocks for {self.selected_country}...")

        try:
            equities = await asyncio.to_thread(fd.Equities)
            country_query = self.selected_country.strip()

            # ✅ Get all unique sectors
            available_sectors = await asyncio.to_thread(lambda: equities.options("sector"))
            if available_sectors is None:
                self.app.notify(f"⚠ No sectors found for {self.selected_country}.", severity="warning")
                return

            # ✅ Clear previous tables and UI
            stocks_display = self.query_one("#stocks_display", VerticalScroll)
            await stocks_display.remove_children()
            self.market_tables.clear()

            # ✅ Iterate through all sectors and fetch stocks
            for sector in available_sectors:
                stock_data = await asyncio.to_thread(lambda: equities.select(country=country_query, sector=sector))
                if stock_data.empty:
                    continue  # Skip sectors with no stocks

                # ✅ Create a table for each sector
                table = DataTable(id=f"table-{sector.replace(' ', '-').lower()}", classes="stock-table")
                table.add_column("Symbol", width=10)
                table.add_column("Open", width=12)
                table.add_column("High", width=12)
                table.add_column("Low", width=12)
                table.add_column("Close", width=12)
                table.add_column("Volume", width=15)

                # ✅ Fetch OHLC and volume data for each stock symbol using yfinance
                for symbol in stock_data.index:  # Symbols are in the index
                    ohlc_data = await self.fetch_ohlc_data(symbol)

                    table.add_row(
                        symbol,  # Stock symbol
                        f"{ohlc_data.get('Open', 'N/A'):.2f}" if ohlc_data.get("Open") else "N/A",
                        f"{ohlc_data.get('High', 'N/A'):.2f}" if ohlc_data.get("High") else "N/A",
                        f"{ohlc_data.get('Low', 'N/A'):.2f}" if ohlc_data.get("Low") else "N/A",
                        f"{ohlc_data.get('Close', 'N/A'):.2f}" if ohlc_data.get("Close") else "N/A",
                        f"{ohlc_data.get('Volume', 'N/A'):,}" if ohlc_data.get("Volume") else "N/A"
                    )

                # ✅ Store table reference and add to UI dynamically
                self.market_tables[sector] = table
                await stocks_display.mount(
                    Container(
                        Static(f"📌 {sector} Sector", classes="section-header"),
                        table
                    )
                )

            self.app.notify(f"✅ Stocks for {self.selected_country} loaded successfully!", severity="information")

        except ValueError as ve:
            self.app.notify(f"❌ Value Error: {ve}", severity="error")
        except Exception as e:
            self.app.notify(f"❌ Error fetching stocks: {e}", severity="error")

    async def fetch_ohlc_data(self, symbol):
        """Fetch OHLC (Open, High, Low, Close) and Volume data using yfinance."""
        try:
            stock = await asyncio.to_thread(lambda: yf.Ticker(symbol).history(period="1d"))
            if stock.empty:
                return {}

            latest_data = stock.iloc[-1]  # Get the last available row
            return {
                "Open": latest_data["Open"],
                "High": latest_data["High"],
                "Low": latest_data["Low"],
                "Close": latest_data["Close"],
                "Volume": latest_data["Volume"]
            }

        except Exception as e:
            print(f"⚠️ Error fetching OHLC data for {symbol}: {e}")
            return {}
