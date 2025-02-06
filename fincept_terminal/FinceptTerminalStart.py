import sys
import os
import json
from textual.app import App
from textual.containers import Container
from textual.binding import Binding
from textual.widgets import Header, Footer

# 🔹 Define a persistent settings directory in the user's home folder
SETTINGS_DIR = os.path.join(os.path.expanduser("~"), ".fincept")
SETTINGS_FILE = os.path.join(SETTINGS_DIR, "FinceptSettingModule.json")

# ✅ Default settings (ensures new settings file is created if missing)
DEFAULT_SETTINGS = {
    "theme": "dark",
    "notifications": True,
    "display_rows": 10,
    "auto_update": False,
    "data_sources": {}
}

BINDINGS = [
    Binding(key="q", action="quit", description="Quit the app"),
    Binding(
        key="question_mark",
        action="help",
        description="Show help screen",
        key_display="?",
    ),
    Binding(key="j", action="down", description="Scroll down", show=False),
]

def ensure_settings_file():
    """Creates `FinceptSettingModule.json` in the user's home directory if it doesn't exist."""

    # ✅ Ensure the settings directory exists
    if not os.path.exists(SETTINGS_DIR):
        os.makedirs(SETTINGS_DIR, exist_ok=True)
        print(f"📁 Created settings directory: {SETTINGS_DIR}")

    # ✅ Create settings file if missing
    if not os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
            json.dump(DEFAULT_SETTINGS, f, indent=4)
        print(f"✅ Settings file created: {SETTINGS_FILE}")
    else:
        print(f"⚡ Settings file already exists: {SETTINGS_FILE}")


class FinceptTerminal(App):
    """Main entry point for Fincept Terminal."""

    def __init__(self):
        super().__init__()

        # ✅ Ensure settings file is created in a persistent location
        ensure_settings_file()


        from fincept_terminal.FinceptAuthModule.WelcomeScreen import WelcomeScreen
        from fincept_terminal.FinceptDashboardModule.FinceptTerminalDashboard import FinceptTerminalDashboard
        from fincept_terminal.FinceptAuthModule.RegistrationScreen import RegistrationScreen

        self.install_screen(WelcomeScreen(), "welcome")
        self.install_screen(FinceptTerminalDashboard(), "dashboard")
        self.install_screen(RegistrationScreen(), "registration")


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
