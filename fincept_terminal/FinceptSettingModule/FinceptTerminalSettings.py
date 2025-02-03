from textual.widgets import Button, TabPane, TabbedContent, Static, Collapsible, Input, Switch, Label, Rule
from textual.containers import Container, Vertical, Horizontal
import os
import json

# Path for storing user FinceptSettingModule
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SETTINGS_FILE = os.path.join(BASE_DIR, "FinceptSettingModule.json")

class SettingsScreen(Container):
    CSS_PATH = "FinceptTerminalDashboard.tcss"

    data_menus = {
        "World Economy Tracker": ["Fincept", "FRED_API", "DataGovIn_API", "DataGovUS_API"],
        "Global News & Sentiment": ["News_API", "GNews", "RSS_Feed"],
        "Currency Markets": ["Forex_API", "Crypto_Exchange_Data"],
        "Portfolio Management": ["Manual_Input", "Broker_API"],
        "GenAI Model": ["Gemini_API", "OpenAI_API", "Ollama_API", "Claude_API"]
    }

    def __init__(self):
        super().__init__()
        self.settings = self.load_settings()

    def compose(self):
        with Container(id="FinceptSettingModule-container"):
            with TabbedContent():
                # Data Sources Tab
                with TabPane("Data Sources", id="data-source"):
                    with Vertical(id="data-source-container"):
                        yield Static("Configure Data Sources", classes="header")
                        for parent, children in self.data_menus.items():
                            with Collapsible(title=parent, collapsed_symbol=">"):
                                for source in children:
                                    source_id = source.lower().replace(" ", "_").replace("(api)", "")
                                    with Collapsible(title=source, id=f"source-{source_id}"):
                                        yield Input(placeholder="Enter API Key", id=f"apikey-{source_id}")
                                        yield Button("Save", id=f"save-{source_id}")

                # Theme Tab

                with TabPane("Theme", id="theme-tab"):
                    with Vertical(id="theme-container"):
                        yield Static("Choose Theme", classes="header")
                        yield Switch(animate=False)
                        yield Button("Switch to Dark Theme", id="dark-theme", variant="primary")
                        yield Button("Switch to Light Theme", id="light-theme", variant="primary")

                # Display Options Tab

                with TabPane("Display Options", id="display-tab"):
                    with Vertical(id="display-container"):
                        yield Static("Configure Display Options", classes="header")
                        yield Input(placeholder="Enter Rows to Display", id="display-rows")
                        yield Button("Save Display Settings", id="save-display-FinceptSettingModule", variant="primary")

                # Notifications Tab
                with TabPane("Notifications", id="notifications-tab"):
                    with Vertical(id="notifications-container"):
                        yield Static("Manage Notifications", classes="header")
                        yield Button("Enable Notifications", id="enable-notifications", variant="primary")
                        yield Button("Disable Notifications", id="disable-notifications", variant="primary")

                # Auto-Update Tab
                with TabPane("FinHub", id="auto-update-tab"):
                    with Vertical(id="auto-update-container"):
                        yield Static("Manage Auto-Update Settings", classes="header")
                        yield Button("Enable Auto-Update", id="enable-auto-update", variant="primary")
                        yield Button("Disable Auto-Update", id="disable-auto-update", variant="primary")
                        yield Static("Sync Your Data with Fincept Cloud")
                        yield Horizontal(
                            Switch(value=True),
                            classes="container",
                        )

            # Back Button
            yield Button("Back to Dashboard", id="back-to-dashboard", variant="default")

    async def on_button_pressed(self, event: Button.Pressed) -> None:
        button_id = event.button.id

        # Data Source Tab: Save Data Source
        if button_id.startswith("save-"):
            # Updated logic for saving data sources
            source_id = button_id.replace("save-", "")  # Extract the source ID from button ID
            input_widget_id = f"apikey-{source_id}"
            api_key = self.query_one(f"#{input_widget_id}", Input).value.strip()

            # Process API key saving with data_menus
            for parent, children in self.data_menus.items():
                source_titles = [child.lower().replace(" ", "_").replace("(api)", "") for child in children]
                if source_id in source_titles:
                    if "data_sources" not in self.settings:
                        self.settings["data_sources"] = {}

                    formatted_parent = parent.lower().replace(" ", "_")
                    self.settings["data_sources"][formatted_parent] = {
                        "source_name": source_id,
                        "apikey": api_key
                    }
                    self.save_settings()
                    self.notify(f"API key for {source_id} saved under {formatted_parent} successfully!")
                    break
            else:
                self.notify(f"Failed to find the parent collapsible for {source_id}.", severity="error")
        elif button_id == "back":
            await self.app.pop_screen()
            self.notify("Returning back to Dashboard")
        # Theme Tab: Switch Theme
        elif button_id == "dark-theme":
            self.settings["theme"] = "dark"
            self.save_settings()
            self.notify("Switched to Dark Theme." )
        elif button_id == "light-theme":
            self.settings["theme"] = "light"
            self.save_settings()
            self.notify("Switched to Light Theme." )

        # Display Options Tab: Save Rows Setting
        elif button_id == "save-display-FinceptSettingModule":
            rows = self.query_one("#display-rows", Input).value.strip()
            if rows.isdigit():
                self.settings["display_rows"] = int(rows)
                self.save_settings()
                self.notify(f"Display rows set to {rows}.")
            else:
                self.notify("Invalid input. Please enter a number.", severity="error")

        # Notifications Tab: Toggle Notifications
        elif button_id == "enable-notifications":
            self.settings["notifications"] = True
            self.save_settings()
            self.notify("Notifications enabled.")
        elif button_id == "disable-notifications":
            self.settings["notifications"] = False
            self.save_settings()
            self.notify("Notifications disabled." )

        # Auto-Update Tab: Toggle Auto-Update
        elif button_id == "enable-auto-update":
            self.settings["auto_update"] = True
            self.save_settings()
            self.notify("Auto-update enabled." )
        elif button_id == "disable-auto-update":
            self.settings["auto_update"] = False
            self.save_settings()
            self.notify("Auto-update disabled." )

        # Back Button
        elif button_id == "back-to-dashboard":
            await self.app.pop_screen()

    # Load FinceptSettingModule from the JSON file
    def load_settings(self):
        default_settings = {
            # "api_key": None,
            # "email": None,
            # "username": None,
            "theme": "dark",
            "notifications": True,
            "display_rows": 10,
            "auto_update": False,
            "data_sources": {}
        }
        if os.path.exists(SETTINGS_FILE):
            with open(SETTINGS_FILE, "r") as f:
                return {**default_settings, **json.load(f)}
        return default_settings

    # Save FinceptSettingModule to the JSON file
    def save_settings(self):
        """
        Save or update FinceptSettingModule in `FinceptSettingModule.json`.
        - Updates existing data instead of overwriting the whole file.
        - Merges old data with new FinceptSettingModule.
        """
        # ✅ Load current FinceptSettingModule if file exists
        if os.path.exists(SETTINGS_FILE):
            with open(SETTINGS_FILE, "r", encoding="utf-8") as f:
                try:
                    existing_settings = json.load(f)
                except json.JSONDecodeError:
                    print("⚠️ Error: FinceptSettingModule.json is corrupted. Resetting to defaults.")
                    existing_settings = {}
        else:
            existing_settings = {}

        # ✅ Merge new FinceptSettingModule with existing FinceptSettingModule
        existing_settings.update(self.settings)

        # ✅ Write updated FinceptSettingModule back to the file
        try:
            with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
                json.dump(existing_settings, f, indent=4)
            print("✅ Settings updated successfully.")
        except Exception as e:
            print(f"❌ Error writing FinceptSettingModule file: {e}")

        # ✅ Debugging: Print FinceptSettingModule after save
        print("📜 Current Settings:", json.dumps(existing_settings, indent=4))