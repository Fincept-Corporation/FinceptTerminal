from textual.app import App, ComposeResult
from textual.containers import Horizontal, Vertical
from textual.widgets import Header, Footer, Static, Button
import json
import subprocess

class FinceptUI(App):
    """A Textual application to display AI hedge fund data processing results."""

    def __init__(self):
        super().__init__()
        self.state_data = {}

    def compose(self) -> ComposeResult:
        yield Header()
        yield Footer()
        yield Vertical(
            Static("Fincept AI Hedge Fund Pipeline", classes="title"),
            Button("Run Analysis", id="run_button"),
            Horizontal(
                Static("Sentiment Analysis"),
                Static("Waiting for data...", id="sentiment")
            ),
            Horizontal(
                Static("Fundamental Analysis"),
                Static("Waiting for data...", id="fundamental")
            ),
            Horizontal(
                Static("Valuation Analysis"),
                Static("Waiting for data...", id="valuation")
            ),
            Horizontal(
                Static("Technical Analysis"),
                Static("Waiting for data...", id="technical")
            ),
            Horizontal(
                Static("Risk Management"),
                Static("Waiting for data...", id="risk")
            ),
            Horizontal(
                Static("Portfolio Management Decision"),
                Static("Waiting for data...", id="portfolio")
            )
        )

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "run_button":
            self.run_analysis()

    def run_analysis(self):
        result = subprocess.run(["python", "samplerun.py"], capture_output=True, text=True)
        try:
            state = json.loads(result.stdout)
            self.update_ui(state)
        except json.JSONDecodeError:
            self.query_one("#sentiment").update("Error parsing data")
            self.query_one("#fundamental").update("Error parsing data")
            self.query_one("#valuation").update("Error parsing data")
            self.query_one("#technical").update("Error parsing data")
            self.query_one("#risk").update("Error parsing data")
            self.query_one("#portfolio").update("Error parsing data")

    def update_ui(self, state):
        self.query_one("#sentiment").update(json.dumps(state.get("sentiment_agent", "No data"), indent=2))
        self.query_one("#fundamental").update(json.dumps(state.get("fundamentals_agent", "No data"), indent=2))
        self.query_one("#valuation").update(json.dumps(state.get("valuation_agent", "No data"), indent=2))
        self.query_one("#technical").update(json.dumps(state.get("technical_analyst_agent", "No data"), indent=2))
        self.query_one("#risk").update(json.dumps(state.get("risk_management_agent", "No data"), indent=2))
        self.query_one("#portfolio").update(json.dumps(state.get("portfolio_management", "No data"), indent=2))

if __name__ == "__main__":
    app = FinceptUI()
    app.run()
