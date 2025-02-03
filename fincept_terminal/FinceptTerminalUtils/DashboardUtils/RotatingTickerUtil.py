from textual.widgets import Static
import asyncio
import yfinance as yf

class AsyncRotatingStockTicker(Static):
    """A Textual widget to display rotating stock headlines asynchronously."""

    STOCKS = [
        "RELIANCE.NS", "BAJAJFINSV.NS", "SBIN.NS", "POWERGRID.NS",
        "ICICIBANK.NS", "HINDUNILVR.NS", "HCLTECH.NS", "TCS.NS",
        "INFY.NS", "WIPRO.NS", "ONGC.NS", "ADANIPORTS.NS",
        "MARUTI.NS", "ASIANPAINT.NS", "AXISBANK.NS", "COALINDIA.NS",
        "RITES.NS", "RVNL.NS", "PEL.NS", "IRFC.NS", "ZOMATO.NS", "SWIGGY.NS"
    ]

    REFRESH_INTERVAL = 60  # Fetch new stock data every 60 seconds
    ROTATION_INTERVAL = 5  # Rotate stocks every 5 seconds
    GROUP_SIZE = 6  # Number of stocks displayed at a time

    def __init__(self):
        super().__init__("Loading stock data...", id="ticker", markup=True)
        self.stock_data = []
        self.current_group_index = 0

    async def on_mount(self):
        """Start background tasks when the widget is mounted."""
        self.set_interval(self.ROTATION_INTERVAL, self.rotate_groups)  # Runs on Textual's event loop
        asyncio.create_task(self.refresh_stock_data_periodically())  # Runs in async background

    async def fetch_stock_data(self):
        """Fetch stock data asynchronously using Yahoo Finance in batches."""
        try:
            tickers_str = " ".join(self.STOCKS)  # Batch query for performance
            tickers = await asyncio.to_thread(lambda: yf.Tickers(tickers_str))  # Run in thread

            self.stock_data.clear()  # Clear old data
            for symbol in self.STOCKS:
                try:
                    ticker = tickers.tickers[symbol]
                    data = await asyncio.to_thread(lambda: ticker.history(period="5d"))  # Fetch history
                    if len(data) >= 2:
                        today_close = data["Close"].iloc[-1]
                        yesterday_close = data["Close"].iloc[-2]
                        change = today_close - yesterday_close
                        percent_change = (change / yesterday_close) * 100
                        self.stock_data.append({
                            "symbol": symbol.split(".")[0],  # Remove ".NS"
                            "price": today_close,
                            "change": change,
                            "percent_change": percent_change,
                        })
                except Exception as e:
                    print(f"⚠️ Error fetching {symbol}: {e}")

        except Exception as e:
            print(f"⚠️ Error fetching stock data: {e}")

    async def refresh_stock_data_periodically(self):
        """Fetch stock data periodically without blocking the UI."""
        while True:
            await self.fetch_stock_data()
            await asyncio.sleep(self.REFRESH_INTERVAL)

    def rotate_groups(self):
        """Rotate through the stock data in groups asynchronously."""
        if not self.stock_data:
            self.update("[yellow]Waiting for stock data...[/]")
            return

        start = self.current_group_index * self.GROUP_SIZE
        end = start + self.GROUP_SIZE
        group = self.stock_data[start:end]

        if not group:
            self.current_group_index = 0  # Reset rotation
            return

        group_text = "   ".join(self.format_stock(stock) for stock in group)
        self.update(group_text)  # Update the UI

        self.current_group_index += 1  # Move to the next set

    def format_stock(self, stock):
        """Format a stock entry for display with colors."""
        symbol = stock["symbol"]
        price = f"{stock['price']:.2f}"
        change = f"{stock['change']:+.2f}"
        percent_change = f"({stock['percent_change']:+.2f}%)"
        color = "green" if stock["change"] > 0 else "red"
        return f"[bold {color}]{symbol} {price} {change} {percent_change}[/]"
