from textual.app import App
from textual.containers import Container

class FinceptTerminal(App):
    """Main entry point for the Fincept Terminal."""

    def __init__(self):
        super().__init__()
        # Register screens here
        from fincept_terminal.FinceptTerminalAuthModule.WelcomeScreen import WelcomeScreen
        self.install_screen(WelcomeScreen(), "welcome")
        from fincept_terminal.FinceptDashboardModule.FinceptTerminalDashboard import FinceptTerminalDashboard
        self.install_screen(FinceptTerminalDashboard(), "dashboard")
        from fincept_terminal.FinceptTerminalAuthModule.RegistrationScreen import RegistrationScreen
        self.install_screen(RegistrationScreen(), "registration")
        from fincept_terminal.FinceptTerminalEcoAnModule.FinceptTerminalEconomicAnalysisScreen import EconomicAnalysisScreen
        self.install_screen(EconomicAnalysisScreen(), "economic_analysis")

    def compose(self):
        """Define the layout of the app."""
        yield Container(id="fincept-app")  # Updated ID to match TCSS

    async def on_mount(self):
        """Mount the Welcome Screen."""
        await self.push_screen("welcome")

def start():
    """
    Entry point function for running the Fincept Terminal.
    """
    try:
        app = FinceptTerminal()  # Create an instance of the app
        app.run()           # Launch the app instance
    except Exception as e:
        raise


if __name__ == "__main__":
    import sys, os
    sys.stdout = open(os.devnull, 'w')
    start()
