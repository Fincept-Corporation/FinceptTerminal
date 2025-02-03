import sys
from textual.app import App
from textual.containers import Container

VERSION = "1.0.0"  # Define the version of Fincept Terminal

class FinceptTerminal(App):
    """Main entry point for the Fincept Terminal."""

    def __init__(self):
        super().__init__()
        # Register screens here
        from fincept_terminal.FinceptAuthModule.WelcomeScreen import WelcomeScreen
        self.install_screen(WelcomeScreen(), "welcome")
        from fincept_terminal.FinceptDashboardModule.FinceptTerminalDashboard import FinceptTerminalDashboard
        self.install_screen(FinceptTerminalDashboard(), "dashboard")
        from fincept_terminal.FinceptAuthModule.RegistrationScreen import RegistrationScreen
        self.install_screen(RegistrationScreen(), "registration")
        from fincept_terminal.FinceptEcoAnModule.FinceptTerminalEconomicAnalysisScreen import EconomicAnalysisScreen
        self.install_screen(EconomicAnalysisScreen(), "economic_analysis")

    def compose(self):
        """Define the layout of the app."""
        yield Container(id="fincept-app")

    async def on_mount(self):
        """Mount the Welcome Screen."""
        await self.push_screen("welcome")

def start_terminal():
    """Launches the Fincept Terminal application."""
    app = FinceptTerminal()
    app.run()

def cli_handler():
    """Handles CLI commands for finceptterminal."""
    if len(sys.argv) > 1:
        if sys.argv[1] == "--version":
            print(f"Fincept Terminal Version: {VERSION}")
        elif sys.argv[1] == "--help":
            print("Fincept Terminal: Under Development")
        else:
            print("Invalid command. Use 'finceptterminal --help' for options.")
    else:
        print("Invalid command. Use 'finceptterminal --help' for options.")
