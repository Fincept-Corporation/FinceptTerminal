from textual.widgets import DataTable, Static
from textual.app import ComposeResult
from textual.containers import Container
import asyncio
import yfinance as yf

class MarketTab(Container):
    """MarketTab for displaying world markets in a 3x2 grid layout asynchronously."""

    REFRESH_INTERVAL = 60  # Update every 60 seconds

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.market_tables = {}  # Store references to each DataTable
        self.last_prices = {}  # Store previous prices for flashing effect

    def compose(self) -> ComposeResult:
        """Create a 3x2 grid layout with 6 sections for market data."""
        with Container(id="market-grid"):
            from fincept_terminal.FinceptUtilsModule.const import ASSETS
            categories = list(ASSETS.keys())
            for i, category in enumerate(categories):
                table = DataTable(id=f"table-{category.replace(' ', '-').lower()}", classes="market-table")
                self.market_tables[category] = table

                # Add section headers with tables in grid cells
                yield Container(
                    Static(f"{category}", classes="section-header"),
                    table,
                    id=f"grid-cell-{i}",
                    classes="grid-cell"
                )

    async def on_mount(self) -> None:
        """Initialize tables and start data updates asynchronously."""
        self.setup_tables()
        asyncio.create_task(self.refresh_data_periodically())  # Run background updates

    def setup_tables(self) -> None:
        """Set up column headers for all market tables."""
        for table in self.market_tables.values():
            table.clear()
            if not table.columns:
                table.add_column("Asset", width=18)
                table.add_column("Symbol", width=5)
                table.add_column("Price", width=10)
                table.add_column("Change (1D)", width=12)
                table.add_column("Change% (1D)", width=12)
                table.add_column("Change% (7D)", width=12)
                table.add_column("Change% (30D)", width=12)

    async def fetch_single_asset(self, category: str, asset_name: str, ticker: str):
        """Fetch price & percentage changes asynchronously with threading for non-blocking execution."""
        try:
            hist = await asyncio.to_thread(lambda: yf.Ticker(ticker).history(period="3mo"))

            if hist.empty:
                return category, asset_name, ticker, None, None, None, None, None

            latest_price = hist["Close"].iloc[-1]  # Last closing price
            prev_1d = hist["Close"].iloc[-2] if len(hist) > 1 else latest_price
            prev_7d = hist["Close"].iloc[-7] if len(hist) > 7 else latest_price
            prev_30d = hist["Close"].iloc[-30] if len(hist) > 30 else latest_price

            return category, asset_name, ticker, latest_price, latest_price - prev_1d, \
                   (latest_price - prev_1d) / prev_1d * 100 if prev_1d else 0, \
                   (latest_price - prev_7d) / prev_7d * 100 if prev_7d else 0, \
                   (latest_price - prev_30d) / prev_30d * 100 if prev_30d else 0

        except Exception as e:
            print(f"⚠️ Error fetching {ticker}: {e}")
            return category, asset_name, ticker, None, None, None, None, None

    async def fetch_asset_data(self):
        """Fetch market data for all assets concurrently."""
        from fincept_terminal.FinceptUtilsModule.const import ASSETS
        tasks = [self.fetch_single_asset(cat, name, ticker) for cat, items in ASSETS.items() for name, ticker in items.items()]
        return await asyncio.gather(*tasks)

    async def refresh_data_periodically(self):
        """Continuously refresh data without freezing the UI."""
        while True:
            await self.refresh_data()
            await asyncio.sleep(self.REFRESH_INTERVAL)  # Prevents UI blocking

    async def refresh_data(self) -> None:
        """Fetch new data and update tables with all assets asynchronously."""
        rows = await self.fetch_asset_data()

        for category, table in self.market_tables.items():
            table.clear(columns=False)  # Clear rows but retain columns

            for row in rows:
                category_name, asset_name, ticker, price, change, change_percent_1d, change_percent_7d, change_percent_30d = row

                if category_name != category:
                    continue

                # Format values for display
                price_str = f"{price:,.2f}" if price is not None else "-"
                change_str = f"{change:+,.2f}" if change is not None else "-"
                change_percent_1d_str = f"{change_percent_1d:+,.2f}%" if change_percent_1d is not None else "-"
                change_percent_7d_str = f"{change_percent_7d:+,.2f}%" if change_percent_7d is not None else "-"
                change_percent_30d_str = f"{change_percent_30d:+,.2f}%" if change_percent_30d is not None else "-"

                color_change = "green" if change and change > 0 else "red"
                color_percent_1d = "green" if change_percent_1d and change_percent_1d > 0 else "red"
                color_percent_7d = "green" if change_percent_7d and change_percent_7d > 0 else "red"
                color_percent_30d = "green" if change_percent_30d and change_percent_30d > 0 else "red"

                # Add row with color formatting
                table.add_row(
                    asset_name,
                    ticker,
                    price_str,
                    f"[{color_change}]{change_str}[/]",
                    f"[{color_percent_1d}]{change_percent_1d_str}[/]",
                    f"[{color_percent_7d}]{change_percent_7d_str}[/]",
                    f"[{color_percent_30d}]{change_percent_30d_str}[/]",
                )

    def apply_flash_effect(self, table, row_key):
        """Temporarily highlight price change with a flash effect."""
        if row_key in table.styles:
            table.styles[row_key] = "row_flash"
            self.set_timer(2, lambda: self.remove_flash_style(table, row_key))

    def remove_flash_style(self, table, row_key):
        """Reset row styling back to default after flashing."""
        if row_key in table.styles:
            del table.styles[row_key]
        self.refresh()
