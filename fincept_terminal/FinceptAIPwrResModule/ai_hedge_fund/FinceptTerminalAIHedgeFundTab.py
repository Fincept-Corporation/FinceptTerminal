import logging
from textual.containers import VerticalScroll, Container
from textual.app import ComposeResult
from textual.widgets import Static, Input, Button, DataTable
import json, os

from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.utils.config import API_KEY

BASE_DIR = os.path.dirname(os.path.abspath(__file__))  # Current file's directory
SETTINGS_FILE = os.path.join(BASE_DIR, "settings", "settings.json")

class AIAgentsTab(VerticalScroll):
    """AI Agents Tab: Dynamically displays AI agent results in DataTables."""

    def __init__(self):
        super().__init__()
        self.logger = logging.getLogger("AIAgentsTab")
        self.analysis_results = None

    def compose(self) -> ComposeResult:
        """Define the AI agents interface layout."""
        with Container(classes="ai_agents_container"):
            # Input Section for Stock Symbol
            with Container(classes="ai_agents_input"):
                yield Input(id="stock_symbol", placeholder="Enter Stock Symbol...", classes="input_field")
                yield Button("Run AI Agents", id="run_agents", classes="run_button")

            # Results Display Section
            with VerticalScroll(id="ai_agents_display", classes="ai_agents_display"):
                yield Static("Results will appear here after running AI Agents.", id="results_placeholder")

    async def on_button_pressed(self, event):
        """Handle button press for running AI agents."""
        if event.button.id == "run_agents":
            await self.run_ai_agents()

    def fetch_genai_api_key(self):
        """Fetch the GenAI API key from settings.json."""
        if not os.path.exists(SETTINGS_FILE):
            self.logger.error("⚠ settings.json not found! Using default API key.")
            return API_KEY  # Fallback to the predefined API_KEY

        try:
            with open(SETTINGS_FILE, "r") as file:
                settings = json.load(file)
                return settings.get("data_sources", {}).get("genai_model", {}).get("apikey", API_KEY)
        except json.JSONDecodeError as e:
            self.logger.error(f"❌ Error parsing settings.json: {e}")
            return API_KEY  # Use fallback API_KEY

    async def run_ai_agents(self):
        """Executes all AI agents for the given stock symbol."""
        input_field = self.query_one("#stock_symbol", Input)
        stock_symbol = input_field.value.strip().upper()

        if not stock_symbol:
            self.app.notify("⚠ Please enter a valid stock symbol.", severity="warning")
            return

        # Display Loading Indicator
        display_area = self.query_one("#ai_agents_display", VerticalScroll)
        await display_area.remove_children()
        loading_message = Static("[italic magenta]Running AI Agents... Please wait.[/italic magenta]")
        await display_area.mount(loading_message)

        # Run AI analysis
        self.analysis_results = await self.execute_agents(stock_symbol)

        # Remove loading indicator and display results
        await display_area.remove_children()
        await self.display_results()

    async def execute_agents(self, ticker):
        """Runs all AI agents for a stock symbol and returns analysis results."""
        start_date, end_date = "2024-01-01", "2024-12-31"
        from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.state import AgentState
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
                    "cost_basis": {ticker: 200 * 150.0}
                }
            },
            metadata={"show_reasoning": True}
        )
        # Use the fetched API Key
        api_key = self.fetch_genai_api_key()
        self.logger.info(f"Using API Key: {api_key}")

        # Run Sentiment Analysis
        from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.agents.sentiment_agent import SentimentAgent
        sentiment_agent = SentimentAgent(api_key=api_key)
        sentiment_result = sentiment_agent.analyze_sentiment("Company posted robust quarterly earnings.")

        state["data"]["analyst_signals"]["sentiment_agent"] = {
            ticker: {"signal": "bullish" if "Positive" in sentiment_result["sentiment"] else "neutral",
                     "confidence": 70.0,
                     "rationale": sentiment_result["rationale"]}
        }

        # Fetch Historical Data
        from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.data_acquisition import \
            get_historical_data
        hist_df = get_historical_data(ticker, start_date, end_date)
        if hist_df is None or hist_df.empty:
            self.logger.error("No historical data found for %s", ticker)
            return None

        from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.data_preprocessing import preprocess_data
        clean_df = preprocess_data(hist_df)

        # Run AI Agents
        from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.agents.fundamentals_agent import \
            fundamentals_agent
        state = fundamentals_agent(state)
        from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.agents.valuation_agent import valuation_agent
        state = valuation_agent(state)
        from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.agents.technical_analyst import \
            technical_analyst_agent
        state = technical_analyst_agent(state)
        from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.agents.risk_manager import \
            risk_management_agent
        state = risk_management_agent(state)

        # Run Portfolio Management
        from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.agents.portfolio_manager import \
            PortfolioManagementAgent
        portfolio_manager = PortfolioManagementAgent(api_key=api_key)
        final_state = portfolio_manager.make_decision(state)

        return final_state["data"]["analyst_signals"]


    async def display_results(self):
        """Displays the results from AI agents in dynamically created DataTables."""
        if not self.analysis_results:
            self.app.notify("⚠ No results found. Please check stock symbol.", severity="warning")
            return

        display_area = self.query_one("#ai_agents_display", VerticalScroll)

        # Clear any placeholder
        await display_area.remove_children()

        for agent_name, agent_data in self.analysis_results.items():
            # Sanitize agent_name to create valid IDs
            sanitized_agent_name = agent_name.replace(" ", "_").lower()

            # Add a title for the agent
            title = Static(f"[bold cyan]{agent_name}[/bold cyan]", id=f"{sanitized_agent_name}_title")
            await display_area.mount(title)

            # Create a new DataTable for this agent
            data_table = DataTable(id=f"{sanitized_agent_name}_table")
            data_table.add_columns("Key", "Value")

            # Populate the table with results
            for stock_symbol, details in agent_data.items():
                for key, value in details.items():
                    if isinstance(value, dict):  # Handle nested dictionaries
                        for sub_key, sub_value in value.items():
                            data_table.add_row(f"{key} → {sub_key}", str(sub_value))
                    else:
                        data_table.add_row(key, str(value))

            # Mount the table to the display area
            await display_area.mount(data_table)

        # Refresh the app to update the UI
        self.app.refresh()
        self.app.notify("Results have been successfully displayed in the DataTables.", severity="information")
