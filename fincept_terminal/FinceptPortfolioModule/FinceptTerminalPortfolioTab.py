from textual.widgets import Input, Static, Button, TabPane, TabbedContent, DataTable
from textual.containers import VerticalScroll, Horizontal
from textual.validation import Function
import yfinance as yf
import json
import logging
from datetime import datetime
import sys
from collections import defaultdict
from plotly.subplots import make_subplots
import plotly.graph_objects as go
import plotly.offline as pyo


import os
BASE_DIR = os.path.dirname(os.path.abspath(__file__))  # Current file's directory
SETTINGS_FILE = os.path.join(BASE_DIR, "..", "FinceptSettingModule", "FinceptSettingModule.json")

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
        """Load portfolios from the FinceptSettingModule file with notifications."""
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
                logging.error("Error loading portfolios: Corrupted FinceptSettingModule.json file.")
                self.notify("Error: Corrupted FinceptSettingModule file. Starting with an empty portfolio.")
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
