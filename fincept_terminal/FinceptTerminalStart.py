import sys
import os
import json
from textual.app import App
from textual.containers import Container

from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import ensure_settings_file


class FinceptTerminal(App):
    """Main entry point for Fincept Terminal."""

    def __init__(self):
        super().__init__()

        # ✅ Ensure settings file is created in a persistent location
        ensure_settings_file()

        from fincept_terminal.FinceptAuthModule.WelcomeScreen import WelcomeScreen
        from fincept_terminal.FinceptDashboardModule.FinceptTerminalDashboard import FinceptTerminalDashboard
        from fincept_terminal.FinceptAuthModule.RegistrationScreen import RegistrationScreen
        from fincept_terminal.FinceptAuthModule.LoginScreen import LoginScreen

        self.install_screen(WelcomeScreen(), "welcome")
        self.install_screen(FinceptTerminalDashboard(), "dashboard")
        self.install_screen(RegistrationScreen(), "registration")
        self.install_screen(LoginScreen(), "login")

    def compose(self):
        yield Container(id="fincept-app")

    async def on_mount(self):
        await self.push_screen("welcome")


def start_terminal():
    """Start Fincept Terminal application."""
    ensure_settings_file()  # ✅ Ensure settings file is created before starting UI
    FinceptTerminal().run()


if __name__ == "__main__":
    start_terminal()
