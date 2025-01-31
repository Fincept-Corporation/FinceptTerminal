import random
import feedparser
import asyncio
from textual.app import ComposeResult, App
from textual.screen import Screen
from textual.validation import Validator, ValidationResult, Function
from textual.containers import Container, Vertical, Horizontal, VerticalScroll, Center
from textual.widgets import Static, Link, Button, ContentSwitcher, DataTable, Header, Input, TabbedContent, TabPane, Footer, Markdown
import plotly.graph_objects as go
import plotly.offline as pyo
import datetime
import yfinance as yf
from textual.reactive import reactive
import json
from collections import defaultdict
from plotly.subplots import make_subplots
import google.generativeai as genai
import logging
import sys

# # Configure logging to both console and log file
# logging.basicConfig(
#     level=logging.DEBUG,
#     format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
#     handlers=[
#         logging.StreamHandler(sys.stdout),  # Log to terminal
#         logging.FileHandler("fincept_terminal.log", mode="a", encoding="utf-8"),  # Log to file
#     ]
# )
#
# # Example usage
# logging.info("🚀 Logging initialized. Saving logs to fincept_terminal.log")

import os
BASE_DIR = os.path.dirname(os.path.abspath(__file__))  # Current file's directory
SETTINGS_FILE = os.path.join(BASE_DIR, "..", "settings", "settings.json")


ASSETS = {
    "Stock Indices": {
        "S&P 500": "^GSPC",
        "Nasdaq 100": "^NDX",
        "Dow Jones": "^DJI",
        "Russell 2000": "^RUT",
        "VIX (Volatility Index)": "^VIX",
        "FTSE 100 (UK)": "^FTSE",
        "DAX 40 (Germany)": "^GDAXI",
        "CAC 40 (France)": "^FCHI",
        "Nikkei 225 (Japan)": "^N225",
        "Hang Seng (Hong Kong)": "^HSI",
        "Shanghai Composite (China)": "000001.SS",
        "Sensex 30 (India)": "^BSESN",
        "Nifty 50 (India)": "^NSEI",
        "ASX 200 (Australia)": "^AXJO",
        "Euro Stoxx 50 (EU)": "^STOXX50E",
        "MSCI Emerging Markets": "EEM",
        "Brazil Bovespa": "^BVSP",
        "Mexico IPC": "^MXX",
        "South Korea KOSPI": "^KS11",
        "Taiwan TAIEX": "^TWII",
        "Singapore STI": "^STI",
    },
    "Forex": {
        "EUR/USD": "EURUSD=X",
        "GBP/USD": "GBPUSD=X",
        "USD/JPY": "USDJPY=X",
        "USD/CHF": "USDCHF=X",
        "USD/CAD": "USDCAD=X",
        "AUD/USD": "AUDUSD=X",
        "NZD/USD": "NZDUSD=X",
        "USD/CNY": "USDCNY=X",
        "USD/INR": "USDINR=X",
        "USD/SGD": "USDSGD=X",
        "USD/KRW": "USDKRW=X",
    },
    "Commodities": {
        "Gold": "GC=F",
        "Silver": "SI=F",
        "Crude Oil (WTI)": "CL=F",
        "Brent Crude Oil": "BZ=F",
        "Natural Gas": "NG=F",
        "Copper": "HG=F",
        "Platinum": "PL=F",
        "Palladium": "PA=F",
        "Wheat": "ZW=F",
        "Corn": "ZC=F",
        "Soybeans": "ZS=F",
        "Coffee": "KC=F",
        "Cocoa": "CC=F",
        "Sugar": "SB=F",
        "Cotton": "CT=F",
        "Lumber": "LB=F",
        "Aluminum": "AL=F",
        "Nickel": "NI=F",
        "Uranium": "UX=F",
    },
    "Bonds": {
        "US 10Y Treasury": "^TNX",
        "US 30Y Treasury": "^TYX",
        "Germany 10Y Bund": "DE10YB=R",
        "UK 10Y Gilt": "GB10YB=R",
        "Japan 10Y JGB": "JP10YB=R",
        "India 10Y Bond": "IN10YB=R",
        "US Corporate Bonds (LQD)": "LQD",
        "US High Yield Bonds (HYG)": "HYG",
        "US Fed Funds Rate": "^IRX",
    },
    "ETFs": {
        "S&P 500 ETF (SPY)": "SPY",
        "Nasdaq 100 ETF (QQQ)": "QQQ",
        "Dow Jones ETF (DIA)": "DIA",
        "Emerging Markets ETF (EEM)": "EEM",
        "Gold ETF (GLD)": "GLD",
        "Technology ETF (XLK)": "XLK",
        "Energy ETF (XLE)": "XLE",
        "Financials ETF (XLF)": "XLF",
        "Healthcare ETF (XLV)": "XLV",
        "Real Estate ETF (VNQ)": "VNQ",
        "AI & Robotics ETF (BOTZ)": "BOTZ",
        "Clean Energy ETF (ICLN)": "ICLN",
        "EV & Battery ETF (LIT)": "LIT",
        "Cybersecurity ETF (HACK)": "HACK",
    },
    "Cryptocurrencies": {
        "Bitcoin": "BTC-USD",
        "Ethereum": "ETH-USD",
        "Binance Coin": "BNB-USD",
        "Solana": "SOL-USD",
        "XRP": "XRP-USD",
        "Dogecoin": "DOGE-USD",
        "Polygon (MATIC)": "MATIC-USD",
        "Chainlink": "LINK-USD",
    },
}

# Set a custom User-Agent so the RSS feeds don't block feedparser
feedparser.USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"


class ExitScreen(Screen):
    """Screen with an Exit button to close the terminal."""
    def compose(self) -> ComposeResult:
        with Center():
            with Vertical():
                yield Button("Exit", id="exit-button", variant="default")


class ChatWidget(Static):
    """A finance-focused chat UI using Textual and GenAI."""

    CSS = """
    Screen {
        align: center middle;
        padding: 2;
        background: #1e1e2e;
    }

    #chat-container {
        width: 80%;
        height: 70%;
        border: solid #cdd6f4;
        padding: 1;
        background: #11111b;
        color: #cdd6f4;
    }

    .message-user {
        color: #89b4fa;
        background: #1e66f5;
        padding: 1;
        border: solid #1e66f5;
        max-width: 70%;
        align: right middle;
        margin-bottom: 2;
    }

    .message-finbot {
        color: #000000;
        background: #f9e2af;
        padding: 1;
        border: solid #f9e2af;
        max-width: 70%;
        align: left middle;
        margin-bottom: 2;
    }

    #input-box {
        width: 70%;
        background: #313244;
        color: #cdd6f4;
    }

    #send-btn {
        width: 15%;
        background: #1e66f5;
        color: white;
    }
    """

    chat_log = reactive([])  # Store chat messages

    def compose(self) -> ComposeResult:
        self.chat_display = VerticalScroll(id="chat-container")
        yield self.chat_display
        yield Horizontal(
            Input(placeholder="Ask about stocks, trends, or finance...", id="input-box"),
            Button("Send", id="send-btn")
        )
        yield Footer()

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handles message sending when the button is pressed."""
        asyncio.create_task(self.send_message())

    def on_input_submitted(self, event: Input.Submitted) -> None:
        """Handles message sending when Enter is pressed."""
        asyncio.create_task(self.send_message())

    async def send_message(self):
        """Adds the user message to the chat log and fetches GenAI response asynchronously."""
        input_box = self.query_one("#input-box", Input)
        user_message = input_box.value.strip()
        if user_message:
            self.chat_log.append(("user", user_message))
            self.update_chat_display()
            input_box.value = ""  # Clear input field

            # Run the GenAI query asynchronously
            ai_response = await self.query_gemini_direct(user_message)
            self.chat_log.append(("finbot", ai_response))
            self.update_chat_display()

    def update_chat_display(self):
        """Updates the chat display with the latest messages."""
        self.chat_display.remove_children()

        # Add updated messages
        for sender, message in self.chat_log[-50:]:  # Display the last 50 messages
            message_widget = Static(message, classes=f"message-{sender}")
            self.chat_display.mount(message_widget)

        # Scroll to the latest message
        self.chat_display.scroll_end(animate=False)

    async def query_gemini_direct(self, input_text):
        """Queries Gemini AI directly for a finance-focused response."""
        api_key = "AIzaSyDP_scMwlOiquieXazTxtWargw_7WD1PiM"  # Replace with your valid API key
        try:
            # Configure the Gemini API with the provided API key
            genai.configure(api_key=api_key)

            # Use the Gemini generative model
            model = genai.GenerativeModel("models/gemini-1.5-flash")

            # Generate a response using the model
            response = model.generate_content(f"Finance Chatbot:\n\n{input_text}")

            return response.text if response else "No response received."
        except Exception as e:
            return f"Error: {e}"
class ChatScreen(Screen):
    """Screen for AI-Powered Chat."""
    def compose(self) -> ComposeResult:
        yield ChatWidget()  # ✅ Mount ChatWidget instead of ChatApp
        yield Button("Back", id="back-button", variant="default")

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle button events."""
        if event.button.id == "back-button":
            await self.app.pop_screen()

class PortfolioTab(VerticalScroll):
    """Custom tab for Portfolio Management."""

    def __init__(self):
        super().__init__()
        self.portfolios = self.load_portfolios()
        self.selected_portfolio = None

    def compose(self):
        """Compose the PortfolioTab layout."""
        with TabbedContent(id="portfolio-tabs", classes="tabs-container"):  # TabView to manage the tabs

            with TabPane("Portfolio Tracker", id="portfolio-tracker"):
                yield Static("Portfolio Tracker", id="portfolio-tracker-title", classes="header")
                yield Static("Enter Portfolio Name:", id="portfolio-name-label", classes="sub-header")
                yield Input(placeholder="Enter Portfolio Name...", id="portfolio-name-input-tracker",
                            classes="action-input")

                yield Button("Track Portfolio", id="track-portfolio", classes="action-button")

                yield Static("Portfolio Tracking Details", id="portfolio-tracker-details-title",
                             classes="header hidden")  # Initially hidden
                yield DataTable(id="portfolio-tracker-table",
                                classes="portfolio-tracker-table hidden")
                yield Button("Visualize", id="visual", classes="visual")

            # First Tab - Portfolio Actions
            with TabPane("Portfolio Actions", id="portfolio-actions"):
                yield Static("Portfolio Management", id="portfolio-management-title", classes="header")
                yield DataTable(id="portfolio-table", classes="portfolio-table")

                yield Static("Portfolio Actions", id="portfolio-actions-title", classes="sub-header")
                yield Input(placeholder="Enter Portfolio Name...", id="portfolio-name-input", classes="action-input")

                # Organize buttons in a horizontal container
                with Horizontal(classes="button-container"):
                    yield Button("Create Portfolio", id="create-portfolio", classes="action-button")
                    yield Button("View All Portfolios", id="view-portfolios", classes="action-button")
                    yield Button("Delete Portfolio", id="delete-portfolio", classes="action-button")

            # Second Tab - Manage Portfolio
            with TabPane("Manage Portfolio", id="manage-portfolio"):
                yield Static("Manage Portfolio Options", id="manage-portfolio-title", classes="header")
                yield Static("PORTFOLIO NAME:", id="manage-portfolio-input-title", classes="sub-header")
                yield Input(placeholder="Enter Portfolio Name...", id="manage-portfolio-input", classes="action-input")

                yield DataTable(id="portfolio-details-table", classes="portfolio-details-table")

                yield Static("STOCK SYMBOL:", id="add-stock-title", classes="sub-header")
                yield Input(placeholder="Enter Stock Symbol...", id="stock-symbol-input", classes="action-input")
                yield Input(placeholder="Enter Quantity...", id="stock-quantity-input", classes="action-input")
                yield Input(placeholder="Enter Date in DD-MM-YYYY...", id="stock-date-input", classes="action-input", validators=[Function(self.validate_date_ddmmyyyy, "The Date Formate is incorrect")])


                # Buttons for Portfolio Management
                with Horizontal(classes="button-container"):
                    yield Button("View Portfolio", id="view-portfolio", classes="action-button")
                    yield Button("Add Stock to Portfolio", id="add-stock", classes="action-button")
                    yield Button("Remove Stock from Portfolio", id="remove-stock", classes="action-button")
                    yield Button("Analyze Portfolio Performance", id="analyze-portfolio", classes="action-button")
                    yield Button("Backtest Portfolio", id="backtest-portfolio", classes="action-button")

                yield Static("Portfolio Analysis", id="portfolio-analysis-title",
                             classes="header hidden")  # Initially hidden
                yield DataTable(id="portfolio-analysis-table",
                                classes="portfolio-analysis-table hidden")  # Initially hidden

    @staticmethod
    def validate_date_ddmmyyyy(date_str: str) -> bool:
        """
        Validate if the input date is in the correct format: DD-MM-YYYY.

        Args:
            date_str (str): The date string entered by the user.

        Returns:
            bool: True if the date is valid, False otherwise.
        """
        try:
            # Try parsing the date in DD-MM-YYYY format
            datetime.datetime.strptime(date_str, "%d-%m-%Y")
            return True
        except ValueError:
            return False

    # Handle button clicks for the Portfolio Actions Tab
    async def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button presses for all actions."""
        portfolio_name = self.query_one("#portfolio-name-input", Input).value.strip()
        manage_portfolio_name = self.query_one("#manage-portfolio-input", Input).value.strip()

        if event.button.id == "create-portfolio":
            self.create_portfolio(portfolio_name)
        elif event.button.id == "view-portfolios":
            self.view_all_portfolios()
        elif event.button.id == "delete-portfolio":
            self.delete_portfolio(portfolio_name)

        elif event.button.id == "view-portfolio":
            self.query_one("#portfolio-analysis-title").add_class("hidden")
            self.query_one("#portfolio-analysis-table").add_class("hidden")
            self.view_current_portfolio(manage_portfolio_name)
        elif event.button.id == "add-stock":
            self.add_stock_to_portfolio(manage_portfolio_name)
        elif event.button.id == "remove-stock":
            self.remove_stock_from_portfolio(manage_portfolio_name)
        elif event.button.id == "analyze-portfolio":
            self.analyze_portfolio_performance(manage_portfolio_name)
        elif event.button.id == "backtest-portfolio":
            self.backtest_portfolio(manage_portfolio_name)

        elif event.button.id == "track-portfolio":
            portfolio = self.query_one("#portfolio-name-input-tracker", Input).value.strip()
            self.track_portfolio(portfolio)
        elif event.button.id == "visual":
            portfolio = self.query_one("#portfolio-name-input-tracker", Input).value.strip()
            await self.visualize_portfolio_allocation(portfolio)



    async def visualize_portfolio_allocation(self, portfolio_name: str):
        """
        Fetch stocks in the portfolio, get their sectors and industries,
        and visualize allocation using a pie chart.

        Args:
            portfolio_name (str): The name of the portfolio to analyze.
        """
        try:
            # ✅ Step 1: Fetch portfolio stocks
            portfolios = self.load_portfolios()

            if portfolio_name not in portfolios:
                self.app.notify(f"Portfolio '{portfolio_name}' not found.", severity="error")
                return

            stocks = portfolios[portfolio_name]

            if not stocks:
                self.app.notify(f"Portfolio '{portfolio_name}' is empty.", severity="warning")
                return

            # ✅ Use defaultdict to store sector & industry allocation
            sector_data = defaultdict(int)
            industry_data = defaultdict(int)

            # ✅ Step 2: Fetch Sector & Industry Data
            for stock_symbol in stocks.keys():
                try:
                    stock_info = yf.Ticker(stock_symbol).info
                    sector = stock_info.get("sector", "Unknown")
                    industry = stock_info.get("industry", "Unknown")

                    # Aggregate quantity
                    sector_data[sector] += stocks[stock_symbol]["quantity"]
                    industry_data[industry] += stocks[stock_symbol]["quantity"]

                except Exception as e:
                    print(f"⚠️ Error fetching data for {stock_symbol}: {e}")

            # ✅ Step 3: Generate and Open Pie Chart
            self.generate_and_open_pie_chart(sector_data, industry_data)

        except KeyboardInterrupt:
            print("Keyboard interrupt detected. Exiting...")
            sys.exit(0)

    def generate_and_open_pie_chart(self, sector_data: dict, industry_data: dict):
        """
        Generate pie charts for sector and industry allocations using Plotly and open in browser.

        Args:
            sector_data (dict): Dictionary with sector allocations.
            industry_data (dict): Dictionary with industry allocations.
        """

        # ✅ Create a single figure with two pie charts using make_subplots
        fig = make_subplots(
            rows=1, cols=2,  # ✅ Arrange charts side by side (1 row, 2 columns)
            subplot_titles=("Sector Allocation", "Industry Allocation"),  # ✅ Add titles for each chart
            specs=[[{"type": "pie"}, {"type": "pie"}]]  # ✅ Specify pie chart type
        )

        # ✅ Add the Sector Allocation Pie Chart
        fig.add_trace(
            go.Pie(
                labels=list(sector_data.keys()),
                values=list(sector_data.values()),
                hole=0.4,
                marker=dict(colors=["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd"]),
                hoverinfo="label+percent+value",
                textinfo="label+percent",
            ),
            row=1, col=1  # ✅ Place in first column
        )

        # ✅ Add the Industry Allocation Pie Chart
        fig.add_trace(
            go.Pie(
                labels=list(industry_data.keys()),
                values=list(industry_data.values()),
                hole=0.4,
                marker=dict(colors=["#17becf", "#bcbd22", "#e377c2", "#8c564b", "#7f7f7f"]),
                hoverinfo="label+percent+value",
                textinfo="label+percent",
            ),
            row=1, col=2  # ✅ Place in second column
        )

        # ✅ Update Layout
        fig.update_layout(
            title_text="Portfolio Allocation Breakdown",
            title_x=0.5,
            paper_bgcolor="#282828",
            plot_bgcolor="#282828",
            font=dict(color="white"),
            margin=dict(t=50, l=25, r=25, b=25),
        )

        # ✅ Step 4: Open the chart in a web browser
        pyo.plot(fig, filename="portfolio_allocation.html", auto_open=True)

    def track_portfolio(self, portfolio_name: str):
        """Track and display details for the given portfolio."""
        if not portfolio_name:
            self.app.notify("Please enter a valid portfolio name.", severity="error")
            return

        portfolios = self.load_portfolios()
        if portfolio_name not in portfolios:
            self.app.notify(f"Portfolio '{portfolio_name}' not found.", severity="error")
            return

        stocks = portfolios[portfolio_name]

        # Ensure stocks have the correct data structure
        if isinstance(stocks, list):
            stocks = {symbol: {"quantity": 1} for symbol in stocks}

        portfolio_tracker_table = self.query_one("#portfolio-tracker-table", DataTable)

        # Clear and setup table
        portfolio_tracker_table.clear()
        if not portfolio_tracker_table.columns:
            portfolio_tracker_table.add_columns(
                "Symbol", "Stock Name", "Sector", "Industry", "Market Cap", "Current Price (₹)", "Volume"
            )

        for stock_symbol in stocks.keys():
            try:
                stock = yf.Ticker(stock_symbol)
                stock_info = stock.info

                stock_name = stock_info.get("shortName", "N/A")
                sector = stock_info.get("sector", "N/A")
                industry = stock_info.get("industry", "N/A")
                market_cap = stock_info.get("marketCap", "N/A")
                current_price = stock_info.get("currentPrice", "N/A")
                volume = stock_info.get("volume", "N/A")

                portfolio_tracker_table.add_row(
                    stock_symbol,
                    stock_name,
                    sector,
                    industry,
                    f"{market_cap:,}" if isinstance(market_cap, int) else "N/A",
                    f"₹{current_price:.2f}" if isinstance(current_price, (int, float)) else "N/A",
                    f"{volume:,}" if isinstance(volume, int) else "N/A"
                )

            except Exception as e:
                self.app.notify(f"Error retrieving data for {stock_symbol}: {e}", severity="error")
                continue

        # Make the table and title visible
        self.query_one("#portfolio-tracker-details-title", Static).remove_class("hidden")
        portfolio_tracker_table.remove_class("hidden")
        self.app.notify(f"Portfolio '{portfolio_name}' tracking details loaded successfully.", severity="information")

    def add_stock_to_portfolio(self, portfolio_name):
        """Ask for stock symbol and quantity, fetch current price, and add stock to the selected portfolio."""
        if portfolio_name not in self.portfolios:
            self.notify(f"Portfolio '{portfolio_name}' not found.", severity="error")
            return

        stock_symbol = self.query_one("#stock-symbol-input", Input).value.strip().upper()
        quantity_input = self.query_one("#stock-quantity-input", Input).value.strip()
        date = self.query_one("#stock-date-input", Input).value.strip()

        if not stock_symbol or not quantity_input.isdigit():
            self.notify("Invalid stock symbol or quantity.", severity="error")
            return

        if not self.validate_date_ddmmyyyy(date):
            self.notify("Invalid date format! Please use DD-MM-YYYY.", severity="error")
            return  # ⛔ STOP execution if the date is invalid

        if not stock_symbol or not quantity_input.isdigit():
            self.notify("Invalid stock symbol or quantity.", severity="error")
            return

        quantity = int(quantity_input)

        # Fetch current price from yfinance
        try:
            stock = yf.Ticker(stock_symbol)
            stock_history = stock.history(period="1d")
            avg_price = stock_history["Close"].iloc[-1] if not stock_history.empty else None

            if avg_price is None:
                self.notify(f"Could not fetch price for {stock_symbol}. Stock not added.", severity="error")
                return

        except Exception as e:
            self.notify(f"Error fetching stock price for {stock_symbol}: {e}", severity="error")
            return

        # Ensure the portfolio is in dictionary format
        if not isinstance(self.portfolios[portfolio_name], dict):
            self.portfolios[portfolio_name] = {}

        if stock_symbol in self.portfolios[portfolio_name]:
            # Update existing stock entry
            current_entry = self.portfolios[portfolio_name][stock_symbol]
            current_quantity = current_entry["quantity"]
            current_avg_price = current_entry["avg_price"]

            # Calculate new average price and update quantity
            new_quantity = current_quantity + quantity
            new_avg_price = ((current_avg_price * current_quantity) + (avg_price * quantity)) / new_quantity

            self.portfolios[portfolio_name][stock_symbol] = {
                "quantity": new_quantity,
                "avg_price": round(new_avg_price, 2),
                "last_added": date
            }
        else:
            # Add a new stock entry
            self.portfolios[portfolio_name][stock_symbol] = {
                "quantity": quantity,
                "avg_price": round(avg_price, 2),
                "last_added": date
            }

        self.save_portfolios()

        self.notify(f"Added {quantity} of {stock_symbol} to {portfolio_name} at ₹{avg_price:.2f}.")
        self.view_current_portfolio(portfolio_name)

    def remove_stock_from_portfolio(self, portfolio_name: str):
        """Remove a specified stock from the selected portfolio."""
        if portfolio_name not in self.portfolios:
            self.app.notify(f"Portfolio '{portfolio_name}' not found.", severity="error")
            return

        stock_symbol = self.query_one("#stock-symbol-input", Input).value.strip().upper()

        if not stock_symbol:
            self.app.notify("Invalid stock symbol. Please enter a valid symbol.", severity="error")
            return

        # Ensure portfolio data is correctly formatted
        portfolio_stocks = self.portfolios[portfolio_name]
        if isinstance(portfolio_stocks, dict):
            if stock_symbol not in portfolio_stocks:
                self.app.notify(f"Stock '{stock_symbol}' not found in portfolio '{portfolio_name}'.", severity="error")
                return

            # Remove the stock
            del portfolio_stocks[stock_symbol]
            self.save_portfolios()

            self.app.notify(f"Stock '{stock_symbol}' removed from portfolio '{portfolio_name}'.")
            self.view_current_portfolio(portfolio_name)
        else:
            self.app.notify(f"Invalid portfolio structure for '{portfolio_name}'.", severity="error")

    def analyze_portfolio_performance(self, portfolio_name):
        """Analyze portfolio performance and display results."""
        portfolios = self.load_portfolios()

        if portfolio_name not in portfolios:
            self.notify(f"Portfolio '{portfolio_name}' not found.")
            return

        portfolio = portfolios[portfolio_name]

        # Convert list to dictionary if stored incorrectly
        if isinstance(portfolio, list):
            portfolio = {symbol: 1 for symbol in portfolio}

        import pandas as pd
        portfolio_returns = pd.DataFrame()

        try:
            for stock_symbol in portfolio.keys():
                stock = yf.Ticker(stock_symbol)
                stock_history = stock.history(period="1y")["Close"]
                stock_returns = stock_history.pct_change().dropna()
                portfolio_returns[stock_symbol] = stock_returns

            mean_returns = portfolio_returns.mean(axis=1)

            # Metrics calculation (using `empyrical` or similar library)
            from empyrical import annual_return, annual_volatility, max_drawdown, sharpe_ratio

            cumulative_returns = (1 + mean_returns).prod() - 1
            ann_return = annual_return(mean_returns)
            ann_volatility = annual_volatility(mean_returns)
            sharpe = sharpe_ratio(mean_returns)
            max_dd = max_drawdown(mean_returns)

            # Results dictionary
            analysis_results = {
                "Cumulative Returns": f"{cumulative_returns:.2%}",
                "Annual Return": f"{ann_return:.2%}",
                "Annual Volatility": f"{ann_volatility:.2%}",
                "Sharpe Ratio": f"{sharpe:.2f}",
                "Max Drawdown": f"{max_dd:.2%}",
            }

            # Update UI to display analysis
            analysis_table = self.query_one("#portfolio-analysis-table", DataTable)
            analysis_table.clear()

            # Make the analysis section visible
            self.query_one("#portfolio-analysis-title").remove_class("hidden")
            analysis_table.remove_class("hidden")

            # Add columns if not already added
            if not analysis_table.columns:
                analysis_table.add_columns("Metric", "Value")

            # Populate the table with analysis results
            for metric, value in analysis_results.items():
                analysis_table.add_row(metric, value)

            self.app.notify(f"Portfolio '{portfolio_name}' analyzed successfully!")

        except Exception as e:
            self.notify(f"Error analyzing portfolio: {e}")

    def load_portfolios(self):
        """Load portfolios from the settings file with notifications."""
        if os.path.exists(SETTINGS_FILE):
            try:
                with open(SETTINGS_FILE, "r") as file:
                    settings = json.load(file)
                    portfolios = settings.get("portfolios", {})
                    if portfolios:
                        self.notify("Portfolios loaded successfully.")
                    else:
                        self.notify("No portfolios found. Create one to get started!")
                    return portfolios
            except json.JSONDecodeError:
                logging.error("Error loading portfolios: Corrupted settings.json file.")
                self.notify("Error: Corrupted settings file. Starting with an empty portfolio.")
                return {}
        self.notify("Settings file not found. Starting fresh.")
        return {}

    def view_current_portfolio(self, portfolio_name):
        """Retrieve and display detailed stock information for a given portfolio, including quantity, average price, and profit/loss."""
        portfolios = self.load_portfolios()

        if portfolio_name not in portfolios:
            self.app.notify(f"Portfolio '{portfolio_name}' not found.", severity="error")
            return

        stocks = portfolios[portfolio_name]

        # Ensure `stocks` is a dictionary with valid data structure
        if isinstance(stocks, list):
            stocks = {symbol: {"quantity": 1, "avg_price": 0.0, "last_added": "N/A"} for symbol in stocks}
        else:
            for symbol, value in stocks.items():
                if not isinstance(value, dict):
                    # Convert non-dict values into the correct structure
                    stocks[symbol] = {"quantity": value, "avg_price": 0.0, "last_added": "N/A"}

        portfolio_details_table = self.query_one("#portfolio-details-table", DataTable)

        # Clear existing table content
        portfolio_details_table.clear()

        # Add columns only if they haven't been added before
        if not portfolio_details_table.columns:
            portfolio_details_table.add_columns(
                "Symbol", "Stock Name", "Quantity", "Avg Price (₹)", "Last Added",
                "Current Price", "Day High", "Day Low", "Volume", "Profit/Loss (%)"
            )

        for stock_symbol, stock_data in stocks.items():
            try:
                stock = yf.Ticker(stock_symbol)
                stock_info = stock.info
                stock_history = stock.history(period="1d")

                stock_name = stock_info.get("shortName", "N/A")
                current_price = (
                    stock_history["Close"].iloc[-1] if not stock_history.empty else "N/A"
                )
                day_high = stock_info.get("dayHigh", "N/A")
                day_low = stock_info.get("dayLow", "N/A")
                volume = stock_info.get("volume", "N/A")

                # Calculate profit/loss percentage
                avg_price = stock_data.get("avg_price", 0.0)
                profit_loss = (
                    ((current_price - avg_price) / avg_price) * 100 if avg_price > 0 and isinstance(current_price, (
                    int, float)) else "N/A"
                )

                # Add row to the table
                portfolio_details_table.add_row(
                    stock_symbol,
                    stock_name,
                    str(stock_data.get("quantity", 0)),  # Display stock quantity
                    f"₹{avg_price:.2f}",  # Display average price
                    stock_data.get("last_added", "N/A"),  # Display the last added time
                    f"₹{current_price:.2f}" if isinstance(current_price, (int, float)) else "N/A",
                    f"₹{day_high:.2f}" if isinstance(day_high, (int, float)) else "N/A",
                    f"₹{day_low:.2f}" if isinstance(day_low, (int, float)) else "N/A",
                    f"{volume:,}" if isinstance(volume, int) else "N/A",
                    f"{profit_loss:.2f}%" if isinstance(profit_loss, (int, float)) else "N/A"
                )

            except Exception as e:
                self.app.notify(f"Error retrieving stock data for {stock_symbol}: {e}", severity="error")
                continue  # Continue loading other stocks even if one fails

        self.app.notify(f"Portfolio '{portfolio_name}' loaded successfully!", severity="information")
        self.query_one("#manage-portfolio-title", Static).update(f"Portfolio: {portfolio_name}")

    def save_portfolios(self):
        """Safely update portfolios without overwriting existing data."""
        try:
            # ✅ Load existing data (if file exists)
            if os.path.exists(SETTINGS_FILE):
                with open(SETTINGS_FILE, "r") as file:
                    try:
                        existing_data = json.load(file)  # Read current data
                    except json.JSONDecodeError:
                        existing_data = {}  # Handle corrupted or empty files
            else:
                existing_data = {}

            # ✅ Update only the "portfolios" section
            existing_data["portfolios"] = self.portfolios

            # ✅ Save the updated data back to the file
            with open(SETTINGS_FILE, "w") as file:
                json.dump(existing_data, file, indent=4)

            logging.info("Portfolios saved successfully.")
            self.notify("Portfolios saved successfully.")

        except Exception as e:
            logging.error(f"Error saving portfolios: {e}")
            self.notify(f"Error saving portfolios: {e}")

    def display_portfolios(self):
        """Display the list of available portfolios with average price, valuation, profit/loss, and last updated time, with notifications."""
        portfolio_table = self.query_one("#portfolio-table", DataTable)
        portfolio_table.clear()
        if not portfolio_table.columns:
            portfolio_table.add_columns(
                "Portfolio Name",
                "Number of Stocks",
                "Total Valuation (₹)",
                "Profit/Loss (%)",
                "Last Updated"
            )

        if not self.portfolios:
            self.query_one("#portfolio-management-title", Static).update("No portfolios available. Create one!")
            self.notify("No portfolios available. Create one to get started!", severity="warning")
            return

        for portfolio_name, stocks in self.portfolios.items():
            if isinstance(stocks, list):  # Handle old formats
                stocks = {symbol: {"quantity": 1, "avg_price": 0.0, "last_added": "N/A"} for symbol in stocks}

            total_quantity = sum(stock_data["quantity"] for stock_data in stocks.values())
            total_value = sum(stock_data["quantity"] * stock_data["avg_price"] for stock_data in stocks.values() if
                              stock_data["avg_price"])

            current_valuation = 0.0  # Total current valuation of the portfolio
            profit_loss = 0.0  # Overall profit/loss percentage

            for stock_symbol, stock_data in stocks.items():
                try:
                    stock = yf.Ticker(stock_symbol)
                    stock_history = stock.history(period="1d")
                    current_price = (
                        stock_history["Close"].iloc[-1] if not stock_history.empty else 0.0
                    )
                    current_valuation += stock_data["quantity"] * current_price

                except Exception as e:
                    self.notify(f"Error fetching data for {stock_symbol}: {e}", severity="error")
                    continue

            # Calculate profit/loss percentage
            profit_loss = (
                ((current_valuation - total_value) / total_value) * 100
                if total_value > 0 else 0.0
            )

            # Get the latest timestamp from the portfolio
            last_updated = max(
                (stock_data["last_added"] for stock_data in stocks.values() if stock_data["last_added"] != "N/A"),
                default="N/A"
            )

            portfolio_table.add_row(
                portfolio_name,
                str(total_quantity),  # Total number of stocks
                f"₹{current_valuation:.2f}" if current_valuation > 0 else "N/A",  # Total valuation
                f"{profit_loss:.2f}%" if total_value > 0 else "N/A",  # Profit/Loss percentage
                last_updated  # Last updated time
            )

        self.notify("Portfolios displayed successfully.", severity="information")

    def create_portfolio(self, portfolio_name):
        """Create a new portfolio with notifications."""
        if not portfolio_name:
            self.notify("Portfolio name cannot be empty!")
            return

        if portfolio_name in self.portfolios:
            self.notify(f"Portfolio '{portfolio_name}' already exists.")
            return

        self.portfolios[portfolio_name] = []
        self.save_portfolios()
        self.display_portfolios()
        self.notify(f"Portfolio '{portfolio_name}' created successfully.")

    def delete_portfolio(self, portfolio_name):
        """Delete an existing portfolio with notifications."""
        if portfolio_name not in self.portfolios:
            self.notify(f"Portfolio '{portfolio_name}' does not exist.")
            return

        del self.portfolios[portfolio_name]
        self.save_portfolios()
        self.display_portfolios()
        self.notify(f"Portfolio '{portfolio_name}' deleted successfully.")

    def view_all_portfolios(self):
        """View all portfolios in a table with notifications."""
        self.display_portfolios()
        self.notify("All portfolios are displayed.")

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
    ROTATION_INTERVAL = 5  # Rotate to the next group every 5 seconds
    GROUP_SIZE = 6  # Number of stocks to show in each row

    def __init__(self):
        super().__init__("Loading stock data...", id="ticker", markup=True)
        self.stock_data = []
        self.current_group_index = 0

    async def on_mount(self):
        """Start stock data refresh and rotation in the background."""
        asyncio.create_task(self.refresh_stock_data_periodically())  # Run in background
        asyncio.create_task(self.rotate_groups())  # Keep UI responsive

    async def fetch_stock_data(self):
        """Fetch stock data asynchronously using Yahoo Finance in batches."""
        try:
            tickers_str = " ".join(self.STOCKS)  # Batch query
            tickers = yf.Tickers(tickers_str)

            self.stock_data = []
            for symbol in self.STOCKS:
                try:
                    ticker = tickers.tickers[symbol]
                    data = await asyncio.to_thread(lambda: ticker.history(period="5d"))
                    if len(data) >= 2:
                        today_close = data["Close"].iloc[-1]
                        yesterday_close = data["Close"].iloc[-2]
                        change = today_close - yesterday_close
                        percent_change = (change / yesterday_close) * 100
                        self.stock_data.append({
                            "symbol": symbol.split(".")[0],
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

    async def rotate_groups(self):
        """Rotate through the stock data in groups of GROUP_SIZE asynchronously."""
        while True:
            if self.stock_data:
                start = self.current_group_index * self.GROUP_SIZE
                end = start + self.GROUP_SIZE
                group = self.stock_data[start:end]

                if not group:
                    self.current_group_index = 0
                    continue

                group_text = "   ".join(self.format_stock(stock) for stock in group)
                self.update(group_text)

                self.current_group_index += 1
            else:
                self.update("[yellow]Waiting for stock data...[/]")

            await asyncio.sleep(self.ROTATION_INTERVAL)

    def format_stock(self, stock):
        """Format a stock entry for display with colors."""
        symbol = stock["symbol"]
        price = f"{stock['price']:.2f}"
        change = f"{stock['change']:+.2f}"
        percent_change = f"({stock['percent_change']:+.2f}%)"
        color = "green" if stock["change"] > 0 else "red"
        return f"[bold {color}]{symbol} {price} {change} {percent_change}[/]"


class NewsManager:
    """Manages fetching and displaying RSS headlines in multiple columns."""
    def __init__(self):
        # We'll fill these columns: (Title, Link, Date, Description)
        self.latest_items = []  # list of tuples (title, link, date, desc)

        # Your chosen RSS feeds
        self.feeds = [
            "https://www.business-standard.com/rss/home_page_top_stories.rss",
            "https://www.business-standard.com/rss/latest.rss",
            "https://www.business-standard.com/rss/markets-106.rss",
        ]

    async def fetch_news(self):
        """Fetch RSS feeds concurrently instead of sequentially."""

        async def fetch_feed(feed_url):
            return await asyncio.to_thread(feedparser.parse, feed_url)

        tasks = [fetch_feed(feed) for feed in self.feeds]
        parsed_feeds = await asyncio.gather(*tasks)

        all_items = []
        for parsed in parsed_feeds:
            for entry in parsed.entries:
                title = entry.get("title", "No Title").strip()
                desc = (entry.get("description") or entry.get("summary") or "").strip()
                date = (entry.get("pubDate") or entry.get("published") or "").strip()
                all_items.append((title, desc, date))

        return random.sample(all_items, 5) if len(all_items) > 5 else all_items

    async def display_in_table(self, screen: Screen, items: list):
        """
        Full clear of #news-table (including columns), then re-add columns
        and populate rows with current items. This approach works
        on older Textual versions that don't have clear(rows=True).
        """
        data_table = screen.query_one("#news-table", DataTable)

        # 1) Clear everything (rows + columns)
        data_table.clear(columns=True)

        # 2) Create columns again
        data_table.add_columns("Title", "Description", "Date")

        # 3) Add rows
        if not items:
            data_table.add_row("No items found", "", "", "")
        else:
            for (title, date, desc) in items:
                data_table.add_row(title, date, desc)

    async def auto_refresh_news(self, screen: Screen):
        """Continuously fetch new items every 10 seconds and update the table."""
        while True:
            # 1) Fetch fresh items
            fetched = await self.fetch_news()
            if fetched:
                self.latest_items = fetched

            # 2) Re-display
            await self.display_in_table(screen, self.latest_items)

            # 3) Wait 10 seconds
            await asyncio.sleep(10)


class MarketTab(Container):
    """MarketTab for displaying world markets in a 3x2 grid layout."""

    REFRESH_INTERVAL = 60  # Update every 60 seconds

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.market_tables = {}  # Store references to each DataTable
        self.last_prices = {}  # Store previous prices for flashing effect

    def compose(self) -> ComposeResult:
        """Create a 3x2 grid layout with 6 sections for market data."""
        with Container(id="market-grid"):
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
        """Initialize tables and start data updates."""
        self.setup_tables()
        await self.refresh_data()
        self.set_interval(self.REFRESH_INTERVAL, self.refresh_data)

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
        """Fetch price & percentage changes (1D, 7D, 30D) for an asset asynchronously."""
        price, change, change_percent_1d, change_percent_7d, change_percent_30d = None, None, None, None, None

        try:
            ticker_obj = yf.Ticker(ticker)
            hist = await asyncio.to_thread(lambda: ticker_obj.history(period="3mo"))

            if not hist.empty:
                latest_price = hist["Close"].iloc[-1]  # Last closing price
                prev_1d = hist["Close"].iloc[-2] if len(hist) > 1 else latest_price
                prev_7d = hist["Close"].iloc[-7] if len(hist) > 7 else latest_price
                prev_30d = hist["Close"].iloc[-30] if len(hist) > 30 else latest_price

                price = latest_price
                change = price - prev_1d
                change_percent_1d = (change / prev_1d) * 100 if prev_1d else 0
                change_percent_7d = ((price - prev_7d) / prev_7d) * 100 if prev_7d else 0
                change_percent_30d = ((price - prev_30d) / prev_30d) * 100 if prev_30d else 0

        except Exception as e:
            print(f"⚠️ Error fetching {ticker}: {e}")

        return category, asset_name, ticker, price, change, change_percent_1d, change_percent_7d, change_percent_30d


    async def fetch_asset_data(self):
        """Fetch market data for all assets concurrently."""
        tasks = []
        for category, items in ASSETS.items():
            for asset_name, ticker in items.items():
                tasks.append(self.fetch_single_asset(category, asset_name, ticker))

        return await asyncio.gather(*tasks)

    async def refresh_data(self) -> None:
        """Fetch new data and update tables with all assets."""
        rows = await self.fetch_asset_data()

        for category, table in self.market_tables.items():
            table.clear(columns=False)  # Clear rows but retain columns

            for category_row in rows:
                category_name, asset_name, ticker, price, change, change_percent_1d, change_percent_7d, change_percent_30d = category_row

                if category_name != category:
                    continue

                # Format values for display
                price_str = f"{price:,.2f}" if price is not None else "-"
                change_str = f"{change:,.2f}" if change is not None else "-"
                change_percent_1d_str = f"{change_percent_1d:,.2f}%" if change_percent_1d is not None else "-"
                change_percent_7d_str = f"{change_percent_7d:,.2f}%" if change_percent_7d is not None else "-"
                change_percent_30d_str = f"{change_percent_30d:,.2f}%" if change_percent_30d is not None else "-"

                color_change = "green" if change is not None and change > 0 else "red"
                color_percent_1d = "green" if change_percent_1d is not None and change_percent_1d > 0 else "red"
                color_percent_7d = "green" if change_percent_7d is not None and change_percent_7d > 0 else "red"
                color_percent_30d = "green" if change_percent_30d is not None and change_percent_30d > 0 else "red"

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


class FinceptTerminalDashboard(Screen):
    """Main Dashboard Screen."""
    CSS_PATH = "FinceptTerminalDashboard.tcss"

    def __init__(self):
        super().__init__()
        self.news_manager = NewsManager()
        self.ticker = AsyncRotatingStockTicker()
        

    def compose(self) -> ComposeResult:
        """Compose the professional dashboard layout."""
        yield Header(show_clock=True)
        yield self.ticker  # Adding the Stock Ticker Below the Header

        with Container(id="dashboard-grid"):
            # Sidebar
            with VerticalScroll(id="sidebar-scroll", classes="sidebar"):
                yield Static("-----------------")
                yield Link("Overview", url="#", tooltip="Go to Overview", id="overview")
                yield Static("-----------------")
                yield Link("Balance Sheet", url="#", tooltip="View Balance Sheet", id="balance_sheet")
                yield Static("-----------------")
                yield Link("Cash Flow", url="#", tooltip="View Cash Flow", id="cash_flow")
                yield Static("-----------------")
                yield Link("Aging Report", url="#", tooltip="View Aging Report", id="aging_report")
                yield Static("-----------------")
                yield Link("Forecasting", url="#", tooltip="View Forecasting", id="forecasting")
                yield Static("-----------------")
                yield Link("Terminal Docs", url="https://docs.fincept.in/",
                           tooltip="View Scenario Analysis", id="scenario")
                yield Static("-----------------")

            # Main Content Inside TabbedContent
            with Container(classes="main-content"):
                with TabbedContent(initial="world-tracker"):
                    with TabPane("World Tracker", id="world-tracker"):
                        # **Nested TabbedContent inside "World Tracker"**
                        with TabbedContent(initial="global_indices"):
                            with TabPane("Global Indices", id="global_indices"):
                                yield MarketTab()

                            with TabPane("Global Funds", id="global_funds"):
                                yield Static("Loading exchanges for funds...", id="funds-title")
                                yield DataTable(id="fund-exchange-table")
                                yield DataTable(id="funds-table")

                            with TabPane("World Sentiment Tracker", id="global_sentiment_tab"):
                                yield Static("Sentiment Data of India From Different Parts", id="sentiment_text")

                                # Grid container inside TabPane
                                with Container(id="sentiment_grid"):
                                    yield Static("Region 1: Positive", id="region_1")
                                    yield Static("Region 2: Neutral", id="region_2")
                                    yield Static("Region 3: Negative", id="region_3")
                                    yield Static("Region 4: Mixed", id="region_4")
                                    yield Static("Region 5: Mixed", id="region_5")
                                    yield Static("Region 6: Mixed", id="region_6")

                            with TabPane("World Market Tracker", id="world_market_tracker"):
                                yield MarketTab()

                            with TabPane("Portfolio Management", id="portfolio_management"):
                                yield PortfolioTab()

                    # Other Main Tabs
                    with TabPane("Economic Analysis", id="economic-analysis"):
                        yield Markdown("# Economic Analysis Content")
                    with TabPane("Financial Markets", id="financial-markets"):
                        yield Markdown("# Financial Markets Content")
                    with TabPane("AI-Powered Research", id="ai-research"):
                        yield Markdown("# AI-Powered Research Content")
                    with TabPane("FinScript", id="finscript"):
                        yield Markdown("# FinScript Content")
                    with TabPane("Portfolio Management", id="portfolio-management"):
                        yield Markdown("# Portfolio Management Content")
                    with TabPane("Edu. & Resources", id="edu-resources"):
                        yield Markdown("# Educational & Resources Content")
                    with TabPane("Settings", id="settings"):
                        yield Markdown("# Settings Content")
                    with TabPane("Help & About", id="help-about"):
                        yield Markdown("# Help & About Content")
                    with TabPane("Exit", id="exit"):
                        yield Markdown("# Exit Content")

            # News Section (Always Visible)
            with Vertical(classes="news-section", id="news-section"):
                yield Static("📰 Latest News", id="news-title", classes="news-title")
                yield DataTable(id="news-table")

        yield Footer()


    async def on_mount(self) -> None:
        """On mount, do an initial fetch & display, then start the auto refresh."""
        # 1) Initial fetch
        initial = await self.news_manager.fetch_news()
        if initial:
            self.news_manager.latest_items = initial

        # 2) Display once
        await self.news_manager.display_in_table(self, self.news_manager.latest_items)

        # 3) Start auto-refresh
        asyncio.create_task(self.news_manager.auto_refresh_news(self))

        asyncio.create_task(self.ticker.refresh_stock_data_periodically())

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle button events."""
        button = event.button
        match button.id:
            case "exit-button":
                self.app.exit()
            case "economic-analysis-button":
                from fincept_terminal.FinceptTerminalEconomicAnalysis.FinceptTerminalEconomicAnalysisScreen import \
                    EconomicAnalysisScreen
                await self.app.push_screen(EconomicAnalysisScreen())
            case "ai-research-button":
                await self.app.push_screen(ChatScreen())


if __name__ == "__main__":
    app = FinceptTerminalDashboard()
    app.run()
