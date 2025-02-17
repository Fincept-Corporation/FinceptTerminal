import asyncio
import csv
import json
import os
import tempfile
from datetime import datetime

import yfinance as yf
import mplfinance as mpf

from textual.app import ComposeResult
from textual.containers import Horizontal, Vertical, Container
from textual.widgets import Input, Button, DataTable
from textual.reactive import reactive
from textual.events import Key

from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import get_settings_path

SETTINGS_FILE = get_settings_path()


class WatchlistApp(Container):
    CSS = """
    Screen {
        align: center middle;
    }
    #header {
        margin: 1 2;
    }
    #header Input {
        width: 20%;
        margin-right: 1;
    }
    #actions {
        margin: 1 2;
    }
    #watchlist_table {
        height: 60%;
        width: 90%;
        margin: 1 2;
    }
    """
    watchlist: dict = reactive(dict)
    settings: dict = reactive(dict)

    def compose(self) -> ComposeResult:
        yield Vertical(
            # Header row: input fields for stock symbol, quantity, avg price, and alert price.
            Horizontal(
                Input(placeholder="Enter Stock Symbol (e.g., AAPL)", id="ticker_input"),
                Input(placeholder="Quantity", id="quantity_input"),
                Input(placeholder="Avg Price (optional)", id="avg_price_input"),
                Input(placeholder="Alert Price (optional)", id="alert_input"),
                id="header",
            ),
            # Actions row: buttons for various actions.
            Horizontal(
                Button("Show Chart", id="show_chart"),
                Button("Show Info", id="show_info"),
                Button("Remove Ticker", id="remove_ticker"),
                Button("Add to Watchlist", id="add_to_watchlist"),
                id="actions",
            ),
            # Watchlist table with additional return columns.
            DataTable(id="watchlist_table"),
        )

    def load_settings(self) -> None:
        if os.path.exists(SETTINGS_FILE):
            with open(SETTINGS_FILE, "r") as f:
                self.settings = json.load(f)
        else:
            print("")

    def save_settings(self) -> None:
        with open(SETTINGS_FILE, "w") as f:
            json.dump(self.settings, f, indent=4)

    def on_mount(self) -> None:
        self.load_settings()
        if "watchlist" not in self.settings.get("portfolios", {}):
            self.settings.setdefault("portfolios", {})["watchlist"] = {}
            self.save_settings()
        self.watchlist = {}
        for ticker, data in self.settings["portfolios"]["watchlist"].items():
            self.watchlist[ticker] = {
                "quantity": data.get("quantity", 0),
                "avg_price": data.get("avg_price", 0),
                "alert": None,
                "current_price": "Fetching...",
                "change": "",
                "percent_change": "",
                "total_value": "",
                "last_updated": data.get("last_added", ""),
                "return_1d": "N/A",
                "return_7d": "N/A",
                "return_30d": "N/A",
            }
        table = self.query_one(DataTable)
        table.clear()
        table.add_columns(
            "Ticker", "Quantity", "Avg Price", "Current Price", "Change", "Change (%)",
            "Total Value", "Alert", "Last Updated", "1d Return", "7d Return", "30d Return"
        )
        self.query_one("#ticker_input", Input).focus()
        self.set_interval(600, self.refresh_all_prices)
        asyncio.create_task(self.refresh_all_prices())

    async def refresh_all_prices(self) -> None:
        tasks = [self.fetch_price(ticker) for ticker in list(self.watchlist.keys())]
        if tasks:
            await asyncio.gather(*tasks)
            self.update_table()
            self.save_watchlist_to_settings()

    def update_table(self) -> None:
        table = self.query_one(DataTable)
        table.clear()
        for ticker, data in sorted(self.watchlist.items(), key=lambda x: x[0]):
            table.add_row(
                ticker,
                str(data.get("quantity", "")),
                f"${data.get('avg_price', 0):.2f}",
                data.get("current_price", ""),
                data.get("change", ""),
                data.get("percent_change", ""),
                data.get("total_value", ""),
                str(data.get("alert", "")),
                data.get("last_updated", ""),
                data.get("return_1d", "N/A"),
                data.get("return_7d", "N/A"),
                data.get("return_30d", "N/A"),
            )

    async def on_button_pressed(self, event: Button.Pressed) -> None:
        btn_id = event.button.id
        if btn_id == "add_to_watchlist":
            await self.handle_add_ticker()
        elif btn_id == "show_chart":
            await self.handle_show_chart()
        elif btn_id == "show_info":
            await self.handle_show_info()
        elif btn_id == "remove_ticker":
            self.handle_remove_ticker()

    async def handle_add_ticker(self) -> None:
        ticker_input = self.query_one("#ticker_input", Input)
        quantity_input = self.query_one("#quantity_input", Input)
        avg_price_input = self.query_one("#avg_price_input", Input)
        alert_input = self.query_one("#alert_input", Input)
        ticker = ticker_input.value.strip().upper()
        if not ticker:
            return
        try:
            quantity = float(quantity_input.value.strip())
        except ValueError:
            return
        avg_price = 0
        if avg_price_input.value.strip():
            try:
                avg_price = float(avg_price_input.value.strip())
            except ValueError:
                return
        alert_price = None
        if alert_input.value.strip():
            try:
                alert_price = float(alert_input.value.strip())
            except ValueError:
                return
        if ticker in self.watchlist:
            return
        today = datetime.now().strftime("%Y-%m-%d")
        self.watchlist[ticker] = {
            "quantity": quantity,
            "avg_price": avg_price,
            "alert": alert_price,
            "current_price": "Fetching...",
            "change": "",
            "percent_change": "",
            "total_value": "",
            "last_updated": today,
            "return_1d": "N/A",
            "return_7d": "N/A",
            "return_30d": "N/A",
        }
        self.settings["portfolios"]["watchlist"][ticker] = {
            "quantity": quantity,
            "avg_price": avg_price,
            "last_added": today
        }
        self.update_table()
        ticker_input.value = ""
        quantity_input.value = ""
        avg_price_input.value = ""
        alert_input.value = ""
        asyncio.create_task(self.fetch_price(ticker))
        self.save_settings()

    async def fetch_price(self, ticker: str) -> None:
        try:
            stock = yf.Ticker(ticker)
            data = await asyncio.to_thread(stock.history, period="30d", interval="1d")
            if data.empty:
                raise ValueError("No data")
            last_price = data["Close"].iloc[-1]
            if len(data) >= 2:
                prev_price = data["Close"].iloc[-2]
                change = last_price - prev_price
                change_pct = (change / prev_price * 100) if prev_price != 0 else 0
            else:
                change = 0
                change_pct = 0
            if len(data) >= 2:
                first_price = data["Close"].iloc[0]
                return_30d = ((last_price - first_price) / first_price * 100) if first_price != 0 else 0
            else:
                return_30d = None
            if len(data) >= 8:
                price_7d = data["Close"].iloc[-8]
                return_7d = ((last_price - price_7d) / price_7d * 100) if price_7d != 0 else 0
            else:
                return_7d = None
            return_1d = change_pct if len(data) >= 2 else None

            self.watchlist[ticker].update({
                "current_price": f"${last_price:.2f}",
                "change": f"${change:+.2f}",
                "percent_change": f"{change_pct:+.2f}%",
                "total_value": f"${last_price * self.watchlist[ticker]['quantity']:.2f}",
                "last_updated": datetime.now().strftime("%H:%M:%S"),
                "return_1d": f"{return_1d:+.2f}%" if return_1d is not None else "N/A",
                "return_7d": f"{return_7d:+.2f}%" if return_7d is not None else "N/A",
                "return_30d": f"{return_30d:+.2f}%" if return_30d is not None else "N/A",
            })
        except Exception as e:
            self.watchlist[ticker].update({
                "current_price": "Error",
                "change": "",
                "percent_change": "",
                "total_value": "",
                "last_updated": "",
                "return_1d": "N/A",
                "return_7d": "N/A",
                "return_30d": "N/A",
            })
        self.update_table()

    def save_watchlist_to_settings(self) -> None:
        for ticker, data in self.watchlist.items():
            if ticker in self.settings["portfolios"]["watchlist"]:
                self.settings["portfolios"]["watchlist"][ticker]["quantity"] = data.get("quantity", 0)
                self.settings["portfolios"]["watchlist"][ticker]["avg_price"] = data.get("avg_price", 0)
                self.settings["portfolios"]["watchlist"][ticker]["last_added"] = data.get("last_updated", "")
        self.save_settings()

    async def handle_show_chart(self) -> None:
        ticker = self.get_selected_ticker()
        if not ticker:
            return
        try:
            stock = yf.Ticker(ticker)
            data = await asyncio.to_thread(stock.history, period="1mo", interval="1d")
            if data.empty:
                return
            tmp_file = tempfile.NamedTemporaryFile(suffix=".png", delete=False)
            tmp_file.close()
            await asyncio.to_thread(
                mpf.plot,
                data,
                type="candle",
                style="charles",
                title=f"{ticker} Candlestick Chart",
                savefig=tmp_file.name,
            )
            # Instead of a modal, simply print the file path to the console.
            print(f"Chart for {ticker} generated and saved to: {tmp_file.name}")
        except Exception as e:
            pass

    async def handle_show_info(self) -> None:
        ticker = self.get_selected_ticker()
        if not ticker:
            return
        try:
            stock = yf.Ticker(ticker)
            info = await asyncio.to_thread(lambda: stock.info)
            info_text = (
                f"Name: {info.get('shortName', 'N/A')}\n"
                f"Market Cap: {info.get('marketCap', 'N/A')}\n"
                f"P/E Ratio: {info.get('trailingPE', 'N/A')}\n"
                f"Volume: {info.get('volume', 'N/A')}\n"
                f"Previous Close: {info.get('previousClose', 'N/A')}\n"
            )
            print(info_text)
        except Exception as e:
            pass

    def handle_remove_ticker(self) -> None:
        ticker = self.get_selected_ticker()
        if not ticker:
            return
        if ticker in self.watchlist:
            del self.watchlist[ticker]
            if ticker in self.settings["portfolios"]["watchlist"]:
                del self.settings["portfolios"]["watchlist"][ticker]
            self.update_table()
            self.save_settings()

    def get_selected_ticker(self) -> str:
        table = self.query_one(DataTable)
        if table.cursor_row is None:
            return ""
        try:
            row = table.get_row_at(table.cursor_row)
        except Exception:
            return ""
        if row:
            return row[0]
        return ""

    async def on_key(self, event: Key) -> None:
        if event.key in ("d", "delete"):
            self.handle_remove_ticker()
        if event.key == "escape":
            # No modal to close since we removed the green status box.
            pass


