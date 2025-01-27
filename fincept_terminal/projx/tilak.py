import asyncio
from textual.app import App, ComposeResult
from textual.widgets import Header, SelectionList
from textual.app import App, ComposeResult
from textual.widgets import (
    Header, Footer, Label, Markdown, TabbedContent, TabPane, DataTable, Input, Button, OptionList, Tree
)
from textual.widgets.selection_list import Selection
from textual.app import Screen
from textual.worker import Worker
from textual.message import Message
from textual.containers import Horizontal, VerticalScroll, Container
from textual.widgets.option_list import Option, Separator
import requests
import logging
from fincept_terminal.ai_hedge_fund.agents.sentiment_agent import SentimentAgent
from fincept_terminal.ai_hedge_fund.agents.valuation_agent import valuation_agent
from fincept_terminal.ai_hedge_fund.agents.fundamentals_agent import fundamentals_agent
from fincept_terminal.ai_hedge_fund.agents.technical_analyst import technical_analyst_agent
from fincept_terminal.ai_hedge_fund.agents.risk_manager import risk_management_agent
from fincept_terminal.ai_hedge_fund.agents.portfolio_manager import portfolio_management_agent
from fincept_terminal.ai_hedge_fund.data.data_acquisition import get_historical_data
from fincept_terminal.ai_hedge_fund.data.data_preprocessing import preprocess_data
from fincept_terminal.ai_hedge_fund.utils.config import API_KEY
from fincept_terminal.ai_hedge_fund.data.state import AgentState


ROWS = [
    ("lane", "swimmer", "country", "time"),
    (4, "Joseph Schooling", "Singapore", 50.39),
    (2, "Michael Phelps", "United States", 51.14),
    (5, "Chad le Clos", "South Africa", 51.14),
    (6, "László Cseh", "Hungary", 51.14),
    (3, "Li Zhuhao", "China", 51.26),
    (8, "Mehdy Metella", "France", 51.58),
    (7, "Tom Shields", "United States", 51.73),
    (1, "Aleksandr Sadovnikov", "Russia", 51.84),
    (10, "Darren Burns", "Scotland", 51.84),
]



def fetch_news(api_key: str = "109b48c2a3754f18abb73e54f5ff7057", query: str = "finance") -> list | str:
    """Fetch news articles related to a specific query."""
    url = f"https://newsapi.org/v2/everything?q={query}&apiKey={api_key}"
    try:
        response = requests.get(url)
        if response.status_code == 200:
            data = response.json()
            if "articles" in data:
                return data["articles"]
            else:
                return "[yellow]No news articles found[/yellow]"
        else:
            return f"[red]Error fetching news: {response.status_code}[/red]"
    except Exception as e:
        return f"[red]Error: {str(e)}[/red]"

class NewsTab(VerticalScroll):
    """Custom tab to display news content."""

    def compose(self) -> ComposeResult:
        yield Label("News Query:", id="news-query-label")
        yield Input(placeholder="Enter query...", id="news-query")
        yield Button("Fetch News", id="fetch-news")
        yield DataTable(id="news-table")
        #yield Markdown(id="news-message")

    def on_mount(self):
        table = self.query_one(DataTable)
        table.add_columns("Title", "Source", "Published At")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "fetch-news":
            query = self.query_one("#news-query", Input).value.strip()
            news = fetch_news(query=query)

            if isinstance(news, str):
                self.query_one("#news-message", Markdown).update(news)
                self.query_one(DataTable).clear()
            else:
                self.query_one("#news-message", Markdown).update("")
                table = self.query_one(DataTable)
                table.clear()
                for article in news[:5]:  # Limit to 10 articles
                    table.add_row(article["title"], article["source"]["name"], article["publishedAt"])


class PipelineTab(VerticalScroll):
    """Custom tab for running and displaying the pipeline."""

    def compose(self) -> ComposeResult:
        yield Label("Enter Ticker Symbol:", id="ticker-label")
        yield Input(placeholder="E.g., AAPL", id="ticker-input")
        yield Button("Run Pipeline", id="run-pipeline")
        yield DataTable(id="pipeline-output")
        yield Markdown(id="pipeline-messages")
        yield Tree("Pipeline Final Output", id="pipeline-tree")

    def on_mount(self) -> None:
        table = self.query_one("#pipeline-output", DataTable)
        table.add_columns("Step", "Details")

    def show_toast(self, message: str, severity: str = "info", timeout: int = 3):
        """Display a toast notification."""
        self.notify(message, severity=severity, timeout=timeout)

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "run-pipeline":
            ticker_input = self.query_one("#ticker-input", Input).value.strip()
            if not ticker_input:
                self.show_toast("Please enter a valid ticker symbol.", "red")
                return
            self.show_toast("Pipeline started", "yellow")
            self.run_worker(self.run_pipeline(ticker_input), exclusive=True)

    async def run_pipeline(self, ticker_input: str):
        """Runs the pipeline asynchronously with error handling."""
        logger = logging.getLogger("PipelineTab")
        logging.basicConfig(level=logging.INFO)

        messages_md = self.query_one("#pipeline-messages", Markdown)
        tree = self.query_one("#pipeline-tree", Tree)

        # Clear all child nodes from the root
        for child in list(tree.root.children):
            tree.root.remove(child)
        tree.root.expand()

        table = self.query_one("#pipeline-output", DataTable)
        table.clear()

        if not ticker_input:
            self.show_toast("Please enter a valid ticker symbol.", "red")
            return

        try:
            sentiment_agent = SentimentAgent(api_key=API_KEY)
            ticker = ticker_input
            start_date, end_date = "2024-01-01", "2024-12-31"

            # Data Acquisition
            self.show_toast("Fetching historical data...", "yellow")
            table.add_row("Data Acquisition", f"Fetching data for {ticker}...")
            try:
                hist_df = get_historical_data(ticker, start_date, end_date)
                table.add_row("Data Acquisition", "Data fetched successfully.")
            except Exception as e:
                logger.error(f"Error fetching historical data: {e}")
                self.show_toast("Error fetching data.", "red")
                messages_md.update(f"[red]Error fetching historical data: {e}[/red]")
                return

            # Data Preprocessing
            try:
                clean_df = preprocess_data(hist_df)
                table.add_row("Data Preprocessing", "Data preprocessed successfully.")
            except Exception as e:
                logger.error(f"Error preprocessing data: {e}")
                self.show_toast("Error preprocessing data.", "red")
                messages_md.update(f"[red]Error preprocessing data: {e}[/red]")
                return

            # Initialize state
            state = AgentState(
                messages=[],
                data={
                    "start_date": start_date,
                    "end_date": end_date,
                    "tickers": [ticker],
                    "analyst_signals": {},
                    "portfolio": {
                        "cash": 100000.0,
                        "positions": {ticker: {"cash": 0.0, "shares": 200, "ticker": ticker}},
                        "cost_basis": {ticker: 200 * 150.0},
                    },
                },
                metadata={"show_reasoning": True},
            )

            # Sentiment Analysis
            self.show_toast("Running sentiment analysis...", "yellow")
            table.add_row("Sentiment Analysis", "Running SentimentAgent...")
            try:
                sentiment_result = sentiment_agent.analyze_sentiment(
                    "Company overcame supply chain disruptions and posted robust quarterly earnings."
                )
                state["data"]["analyst_signals"]["sentiment_agent"] = {
                    ticker: {
                        "signal": "bullish" if "Positive" in sentiment_result["sentiment"] else "neutral",
                        "confidence": 70.0,
                        "rationale": sentiment_result["rationale"],
                    }
                }
                table.add_row("Sentiment Analysis", str(sentiment_result))
            except Exception as e:
                logger.error(f"Error in sentiment analysis: {e}")
                self.show_toast("Error in sentiment analysis.", "red")
                messages_md.update(f"[red]Error in sentiment analysis: {e}[/red]")
                return

            # Run Other Agents
            agents = [
                ("Fundamentals Analysis", fundamentals_agent),
                ("Valuation Analysis", valuation_agent),
                ("Technical Analysis", technical_analyst_agent),
                ("Risk Management", risk_management_agent),
                ("Portfolio Management", portfolio_management_agent),
            ]

            for agent_name, agent_func in agents:
                self.show_toast(f"Running {agent_name}...", "yellow")
                table.add_row(agent_name, f"Running {agent_name}...")
                await asyncio.sleep(1)  # Simulate async delay
                try:
                    state = agent_func(state)
                    table.add_row(agent_name, "Completed successfully.")
                    # Add agent results to the Tree
                    if agent_name == "Portfolio Management":
                        portfolio_branch = tree.root.add("Portfolio", expand=True)
                        portfolio = state["data"]["portfolio"]
                        portfolio_branch.add_leaf(f"Cash: {portfolio['cash']}")
                        for ticker, position in portfolio["positions"].items():
                            position_branch = portfolio_branch.add(f"Ticker: {ticker}", expand=True)
                            for key, value in position.items():
                                position_branch.add_leaf(f"{key}: {value}")
                    else:
                        branch = tree.root.add(agent_name, expand=True)
                        for key, value in state["data"]["analyst_signals"].get(agent_name.lower().replace(" ", "_"),
                                                                               {}).items():
                            branch.add_leaf(f"{key}: {value}")
                except Exception as e:
                    logger.error(f"Error in {agent_name}: {e}")
                    self.show_toast(f"Error in {agent_name}.", "red")
                    messages_md.update(f"[red]Error in {agent_name}: {e}[/red]")
                    return

            # Pipeline Completion
            self.show_toast("Pipeline finished successfully!", "green")

        except Exception as e:
            logger.exception("Pipeline execution failed")
            messages_md.update(f"[red]Error: {str(e)}[/red]")
            self.show_toast("Pipeline failed.", "red")


class SettingsScreen(Screen):
    """Screen to display settings options."""

    def compose(self) -> ComposeResult:
        yield Header()
        with Horizontal(id="button-container"):
            yield Button("Global Market Sentiment", id="main-dash")
            yield Button("Portfolio Manager", id="btn-2")
            yield Button("World Marine Data", id="btn-3")
            yield Button("Trade Analysis", id="btn-4")
            yield Button("Country Specifc Data", id="btn-5")
            yield Button("Robo Advisor", id="btn-6")
            yield Button("Settings", id="settings-btn")
            yield Button("Exit", id="btn-exit")
        yield SelectionList[int](
            Selection("Option 1: Enable Feature A", 0),
            Selection("Option 2: Enable Feature B", 1, True),
            Selection("Option 3: Adjust Threshold", 2),
            Selection("Option 4: Debug Mode", 3),
            Selection("Option 5: Reset to Defaults", 4),
        )
        yield Footer()

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button press events in SettingsScreen."""
        if event.button.id == "main-dash":  # Navigate back to the main screen
            self.app.pop_screen()
        elif event.button.id == "btn-exit":  # Exit the application
            self.app.exit()


import os
import json
import yfinance as yf
# Define the path for settings.json
SETTINGS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "settings.json")

# Initialize portfolios as an empty dictionary
portfolios = {}


def load_portfolios():
    """Load portfolios from the settings.json file."""
    global portfolios
    if os.path.exists(SETTINGS_FILE):
        try:
            with open(SETTINGS_FILE, "r") as file:
                data = json.load(file)
                portfolios = data.get("portfolios", {})
        except (json.JSONDecodeError, ValueError):
            portfolios = {}
    else:
        portfolios = {}
    return portfolios


def save_portfolios():
    """Save portfolios to the settings.json file."""
    with open(SETTINGS_FILE, "w") as file:
        json.dump({"portfolios": portfolios}, file, indent=4)


class EditPortfolioScreen(Screen):
    """Screen to manage portfolios."""

    def compose(self) -> ComposeResult:
        yield Header()
        with Horizontal(id="button-container"):
            yield Button("Create Portfolio", id="btn-create")
            yield Button("Add Stock to Portfolio", id="btn-add-stock")
            yield Button("View All Portfolios", id="btn-view")
            yield Button("Back", id="btn-back")
            yield Button("Submit Portfolio", id="btn-portfolio-submit")
            yield Button("Submit Stock", id="btn-stock-submit")
        yield Label("Portfolio Management Actions:", id="label-actions")
        yield Input(placeholder="Enter stock ticker...", id="stock-ticker-input")
        yield VerticalScroll(DataTable(id="portfolio-table"))

        # Add a section for entering portfolio name and submitting
        with Horizontal(id="create-portfolio-container"):
            yield Input(placeholder="Enter portfolio name...", id="portfolio-name-input")
            yield Button("Submit Portfolio", id="btn-portfolio-submit")  # Button for portfolio submission

        # Add a section for selecting portfolio and adding stocks
        with Horizontal(id="add-stock-container"):
            yield Input(placeholder="Select portfolio...", id="portfolio-select-input")
            yield Input(placeholder="Enter stock ticker...", id="stock-ticker-input")
            yield Button("Submit Stock", id="btn-stock-submit")  # Button for stock submission

        yield Footer()


    def on_mount(self) -> None:
        """Populate the table with existing portfolios."""
        self.populate_portfolio_table()

    def populate_portfolio_table(self) -> None:
        """Populate the portfolio table with data."""
        table = self.query_one("#portfolio-table", DataTable)
        table.clear()
        table.add_columns("Portfolio Name", "Stocks Count")
        for portfolio_name, stocks in portfolios.items():
            table.add_row(portfolio_name, str(len(stocks)))

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button presses."""
        if event.button.id == "btn-create":
            self.app.notify("Enter portfolio name and click Submit Portfolio.", severity="info")
        elif event.button.id == "btn-add-stock":
            self.app.notify("Enter portfolio name and stock ticker, then click Submit Stock.", severity="info")
        elif event.button.id == "btn-portfolio-submit":
            self.create_portfolio()
        elif event.button.id == "btn-stock-submit":
            self.add_stock_to_portfolio()
        elif event.button.id == "btn-view":
            self.populate_portfolio_table()
            self.app.notify("Portfolios reloaded.", severity="info")
        elif event.button.id == "btn-back":
            self.app.pop_screen()

    def create_portfolio(self) -> None:
        """Create a new portfolio."""
        portfolio_name_input = self.query_one("#portfolio-name-input", Input).value.strip()

        if not portfolio_name_input:
            self.app.notify("Portfolio name cannot be empty.", severity="error")
            return

        if portfolio_name_input in portfolios:
            self.app.notify(f"Portfolio '{portfolio_name_input}' already exists.", severity="error")
            return

        portfolios[portfolio_name_input] = []  # Create an empty portfolio
        save_portfolios()
        self.app.notify(f"Portfolio '{portfolio_name_input}' created successfully.", severity="success")
        self.populate_portfolio_table()  # Refresh the portfolio table

    def add_stock_to_portfolio(self) -> None:
        """Add stocks to an existing portfolio."""
        if not portfolios:
            self.app.notify("No portfolios available. Create one first.", severity="error")
            return

        # Get the user-selected portfolio
        portfolio_name_input = self.query_one("#portfolio-select-input", Input).value.strip()
        if portfolio_name_input not in portfolios:
            self.app.notify("Invalid portfolio name. Ensure it exists.", severity="error")
            return

        # Get the stock ticker from input
        stock_ticker_input = self.query_one("#stock-ticker-input", Input).value.strip().upper()
        if not stock_ticker_input:
            self.app.notify("Stock ticker cannot be empty.", severity="error")
            return

        # Add the stock to the portfolio
        try:
            import yfinance as yf
            stock = yf.Ticker(stock_ticker_input)
            stock_info = stock.info
            if not stock_info or "symbol" not in stock_info:
                self.app.notify("Invalid stock ticker.", severity="error")
            else:
                portfolios[portfolio_name_input].append(stock_ticker_input)
                save_portfolios()
                self.app.notify(f"Added {stock_ticker_input} to {portfolio_name_input}.", severity="success")
                self.populate_portfolio_table()
        except Exception as e:
            self.app.notify(f"Error adding stock: {e}", severity="error")

    def prompt_for_stock(self, portfolio_name: str) -> None:
        """Prompt the user to add stocks to the selected portfolio."""
        stock_input = Input("Enter stock symbol (e.g., AAPL)", placeholder="Stock Ticker")
        self.mount(stock_input)

        def add_stock() -> None:
            ticker = stock_input.value.strip().upper()
            try:
                import yfinance as yf
                stock = yf.Ticker(ticker)
                stock_info = stock.info
                if not stock_info or "symbol" not in stock_info:
                    self.app.notify("Invalid stock ticker.", severity="error")
                else:
                    portfolios[portfolio_name].append(stock)
                    save_portfolios()
                    self.app.notify(f"Added {ticker} to {portfolio_name}.", severity="success")
                    self.populate_portfolio_table()
            except Exception as e:
                self.app.notify(f"Error adding stock: {e}", severity="error")
            stock_input.remove()

        stock_input.on_submit = add_stock


class PortfolioManagerScreen(Screen):
    """Screen to display portfolio management options."""

    def compose(self) -> ComposeResult:
        yield Header()
        with Horizontal(id="button-container"):
            yield Button("View Portfolio", id="btn-view")
            yield Button("Edit Portfolio", id="btn-edit")
            yield Button("Back", id="btn-back")
            yield Button("Exit", id="btn-exit")
        yield VerticalScroll(DataTable(id="portfolio-view-table"))  # Add table for viewing portfolios
        yield Footer()

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button press events in PortfolioManagerScreen."""
        if event.button.id == "btn-back":
            self.app.pop_screen()  # Navigate back to the previous screen
        elif event.button.id == "btn-exit":
            self.app.exit()  # Exit the application
        elif event.button.id == "btn-view":
            self.display_portfolio_with_prices()  # Display portfolios with stock prices
        elif event.button.id == "btn-edit":
            self.app.push_screen(EditPortfolioScreen())  # Navigate to the EditPortfolioScreen

    def display_portfolio_with_prices(self):
        """Fetch and display portfolio stocks and their latest prices."""
        import yfinance as yf

        table = self.query_one("#portfolio-view-table", DataTable)
        table.clear()
        table.add_columns("Portfolio Name", "Stock Ticker", "Latest Price (USD)")

        if not portfolios:
            self.app.notify("No portfolios available.", severity="error")
            return

        for portfolio_name, stocks in portfolios.items():
            for stock in stocks:
                try:
                    # Fetch the latest stock price using yfinance
                    stock_info = yf.Ticker(stock).info
                    latest_price = stock_info.get("regularMarketPreviousClose", "N/A")
                    table.add_row(portfolio_name, stock, f"${latest_price}")
                except Exception as e:
                    self.app.notify(f"Error fetching data for {stock}: {e}", severity="error")

        self.app.notify("Portfolio data loaded successfully.", severity="success")

class TabbedApp(App):
    """An example of tabbed content."""

    CSS_PATH = "tilak.tcss"

    BINDINGS = [
        ("l", "show_tab('leto')", "Leto"),
        ("j", "show_tab('jessica')", "Jessica"),
        ("p", "show_tab('paul')", "Paul"),
    ]

    def compose(self) -> ComposeResult:
        """Compose app with tabbed content."""
        # Footer to show keys
        yield Header()
        # Add horizontal container for buttons
        with Horizontal(id="button-container"):
            yield Button("Global Market Sentiment", id="main-dash")
            yield Button("Portfolio Manager", id="btn-2")
            yield Button("World Marine Data", id="btn-3")
            yield Button("Trade Analysis", id="btn-4")
            yield Button("Country Specifc Data", id="btn-5")
            yield Button("Robo Advisor", id="btn-6")
            yield Button("Settings", id="settings-btn")
            yield Button("Exit", id="btn-exit")

        yield OptionList(
            Option("Aerilon", id="aer"),
            Option("Aquaria", id="aqu"),
            Separator(),
            Option("Canceron", id="can"),
            Option("Caprica", id="cap", disabled=True),
            Separator(),
            Option("Gemenon", id="gem"),
            Separator(),
            Option("Leonis", id="leo"),
            Option("Libran", id="lib"),
            Separator(),
            Option("Picon", id="pic"),
            Separator(),
            Option("Sagittaron", id="sag"),
            Option("Scorpia", id="sco"),
            Separator(),
            Option("Tauron", id="tau"),
            Separator(),
            Option("Virgon", id="vir"),
        )
        yield DataTable()
        yield Footer()

        # Add the TabbedContent widget
        with TabbedContent(initial="jessica"):
            with TabPane("Global News & Sentiment", id="leto"):  # First tab
                yield NewsTab()
            with TabPane("AI-Hedge Fund", id="jessica"):
                yield PipelineTab()
                # with TabbedContent("Paul", "Alia"):
                #     yield TabPane("Paul", Label("First child"))
                #     yield TabPane("Alia", Label("Second child"))

            with TabPane("GenAI Query", id="paul"):
                yield NewsTab()

    def action_show_tab(self, tab: str) -> None:
        """Switch to a new tab."""
        self.get_child_by_type(TabbedContent).active = tab

    def on_mount(self) -> None:
        table = self.query_one(DataTable)
        table.add_columns(*ROWS[0])
        table.add_rows(ROWS[1:])

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button press events."""
        if event.button.id == "btn-exit":
            self.exit()
        elif event.button.id == "settings-btn":
            self.push_screen(SettingsScreen())  # Navigate to the SettingsScreen
        elif event.button.id == "maina-dash":
            self.app.pop_screen()
        elif event.button.id == "btn-2":
            self.push_screen(PortfolioManagerScreen())

if __name__ == "__main__":
    app = TabbedApp()
    app.run()