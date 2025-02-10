from textual.widgets import Button, TabPane, TabbedContent, Static, Collapsible, Input, Switch, Label
from textual.containers import Container, Vertical, Horizontal
import os, json, requests, asyncio, time
import pyperclip

# Path for storing user settings
from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import get_settings_path
SETTINGS_FILE = get_settings_path()

FINCEPT_API_URL = "https://finceptapi.share.zrok.io"

class SettingsScreen(Container):

    data_menus = {
        "World Economy Tracker": ["FRED_API", "DataGovIn_API", "DataGovUS_API"],
        "Global News & Sentiment": ["News_API", "GNews", "RSS_Feed"],
        "Currency Markets": ["Forex_API", "Crypto_Exchange_Data"],
        "Portfolio Management": ["Manual_Input", "Broker_API"],
        "GenAI Model": ["Gemini_API", "OpenAI_API", "Ollama_API", "Claude_API"]
    }

    def __init__(self):
        super().__init__()
        self.settings = self.load_settings()
        self.last_sync_time = 0
        self.cooldown_active = False

    async def upload_to_cloud(self):
        """ Enable Cloud Sync: Upload settings when the switch is turned ON """
        if not self.settings.get("api_key") or not self.settings.get("username"):
            self.notify("Error: API Key & Username required for Cloud Sync.", severity="error")
            return False  # Indicate failure

        payload = {"username": self.settings["username"], "settings": self.settings}
        headers = {"X-API-Key": self.settings["api_key"]}

        try:
            response = await asyncio.to_thread(requests.post, f"{FINCEPT_API_URL}/cloud-sync/upload", json=payload,
                                               headers=headers)
            if response.status_code == 200:
                self.query_one("#cloud-sync-status", Label).update("Cloud Sync Status: Enabled")
                self.notify("Success: Cloud Sync Enabled! Settings uploaded.", severity="information")
                return True  # Indicate success
            else:
                self.notify(f"Error: {response.json().get('detail', 'Failed to sync')}", severity="error")
                return False  # Indicate failure
        except Exception as e:
            self.notify(f"Error: {str(e)}", severity="error")
            return False  # Indicate failure

    async def delete_cloud_settings(self):
        """ Disable Cloud Sync: Delete settings when the switch is turned OFF """
        if not self.settings.get("api_key") or not self.settings.get("username"):
            self.notify("Error: API Key & Username required for Cloud Sync.", severity="error")
            return False  # Indicate failure

        payload = {"username": self.settings["username"]}
        headers = {"X-API-Key": self.settings["api_key"]}

        try:
            response = await asyncio.to_thread(requests.post, f"{FINCEPT_API_URL}/cloud-sync/delete", json=payload,
                                               headers=headers)
            if response.status_code == 200:
                self.query_one("#cloud-sync-status", Label).update("Cloud Sync Status: ❌ Disabled")
                self.notify("Success: Cloud Sync Disabled! Settings deleted.", severity="information")
                return True  # Indicate success
            else:
                self.notify(f"Error: {response.json().get('detail', 'Failed to delete')}", severity="error")
                return False  # Indicate failure
        except Exception as e:
            self.notify(f"Error: {str(e)}", severity="error")
            return False  # Indicate failure

    async def download_from_cloud(self):
        """ Download settings from Fincept Cloud """
        if not self.settings.get("api_key") or not self.settings.get("username"):
            self.notify("Error: API Key & Username required for Cloud Sync.", severity="error")
            return

        payload = {"username": self.settings["username"]}
        headers = {"X-API-Key": self.settings["api_key"]}

        try:
            response = await asyncio.to_thread(requests.post, f"{FINCEPT_API_URL}/cloud-sync/download", json=payload,
                                               headers=headers)
            if response.status_code == 200:
                self.settings.update(response.json().get("settings", {}))
                self.save_settings()
                self.query_one("#cloud-sync-status", Label).update("Cloud Sync Status: Downloaded")
                self.notify("Success: Settings downloaded from cloud!", severity="information")
            else:
                self.notify(f"Error: {response.json().get('detail', 'No settings found')}", severity="error")
        except Exception as e:
            self.notify(f"Error: {str(e)}", severity="error")

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
                                        with Horizontal(classes="settings_save_paste"):
                                            yield Button("Save", id=f"save-{source_id}")
                                            yield Button("Paste", id=f"paste-{source_id}", variant="default")
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

                        yield Static("Sync Settings to Fincept Cloud", classes="header")
                        yield Horizontal(
                            Switch(
                                value=self.settings.get("cloud_sync_enabled", False),  # Load state from JSON
                                id="cloud-sync-toggle"
                            ),
                            classes="container",
                        )
                        yield Label("Cloud Sync Status: Disabled", id="cloud-sync-status")
                        yield Button("Download Settings", id="download-cloud", variant="primary")

                # **New Tab for User Information (API Key)**
                with TabPane("User Info", id="user-info-tab"):
                    with Vertical(id="user-info-container"):
                        yield Static("Fincept User Information", classes="header")
                        yield Label(f"Email: {self.settings.get('email', 'Not Available')}")
                        yield Label(f"Username: {self.settings.get('username', 'Not Available')}")
                        yield Label(f"API Key: {self.settings.get('api_key', 'Not Available')}", id="fincept-api-key")

            # Back Button
            yield Button("Back to Dashboard", id="back-to-dashboard", variant="default")

    async def on_switch_changed(self, event: Switch.Changed) -> None:
        """ Handle enabling/disabling cloud sync with a 30-min cooldown & save state to JSON """

        if event.switch.id == "cloud-sync-toggle":
            current_time = time.time()
            cooldown_period = 30 * 60  # 30 minutes in seconds

            if current_time - self.last_sync_time < cooldown_period:
                if not self.cooldown_active:
                    self.notify("Cloud sync update allowed only every 30 minutes.", severity="warning")
                    self.cooldown_active = True  # Prevent multiple notifications
                event.switch.value = self.settings.get("cloud_sync_enabled", False)  # Revert to last saved state
                return

            self.last_sync_time = current_time
            self.cooldown_active = False

            # Update settings JSON
            self.settings["cloud_sync_enabled"] = event.value
            self.save_settings()  # Save immediately

            # Perform the operation based on switch value
            if event.value:  # Cloud Sync Enabled
                success = await self.run_async_task(self.upload_to_cloud)
                if not success:  # If upload fails, reset switch to OFF
                    self.query_one("#cloud-sync-toggle", Switch).value = False
                    self.settings["cloud_sync_enabled"] = False
                    self.save_settings()
            else:  # Cloud Sync Disabled
                success = await self.run_async_task(self.delete_cloud_settings)
                if not success:  # If deletion fails, reset switch to ON
                    self.query_one("#cloud-sync-toggle", Switch).value = True
                    self.settings["cloud_sync_enabled"] = True
                    self.save_settings()

    async def run_async_task(self, task):
        """ Runs a task in the background without freezing UI """
        await asyncio.create_task(task())

    async def on_button_pressed(self, event: Button.Pressed) -> None:
        button_id = event.button.id

        # Data Source Tab: Save Data Source
        if button_id.startswith("save-"):
            source_id = button_id.replace("save-", "")
            input_widget_id = f"apikey-{source_id}"
            api_key = self.query_one(f"#{input_widget_id}", Input).value.strip()

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

        elif button_id.startswith("paste-"):
            # Extract source_id from the button id, e.g. "paste-gemini_api" -> "gemini_api"
            source_id = button_id.replace("paste-", "")
            try:
                clipboard_text = pyperclip.paste()
            except Exception as e:
                self.notify(f"Error accessing clipboard: {e}", severity="error")
                clipboard_text = ""
            # Find the associated input widget using its id (e.g. "apikey-gemini_api")
            input_widget = self.query_one(f"#apikey-{source_id}", Input)
            input_widget.value = clipboard_text
            self.notify(f"Pasted clipboard text for {source_id}.", severity="information")

        # Theme Tab: Switch Theme
        elif button_id == "dark-theme":
            self.settings["theme"] = "dark"
            self.save_settings()
            self.notify("Switched to Dark Theme.")
        elif button_id == "light-theme":
            self.settings["theme"] = "light"
            self.save_settings()
            self.notify("Switched to Light Theme.")

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
            self.notify("Notifications disabled.")

        # Auto-Update Tab: Toggle Auto-Update
        elif button_id == "enable-auto-update":
            self.settings["auto_update"] = True
            self.save_settings()
            self.notify("Auto-update enabled.")
        elif button_id == "disable-auto-update":
            self.settings["auto_update"] = False
            self.save_settings()
            self.notify("Auto-update disabled.")

        elif button_id == "download-cloud":
            await self.download_from_cloud()

        # Back Button
        elif button_id == "back-to-dashboard":
            await self.app.pop_screen()

    # Load FinceptSettingModule from the JSON file
    def load_settings(self):
        """Load settings from JSON file, including Cloud Sync state."""
        default_settings = {
            "theme": "dark",
            "notifications": True,
            "display_rows": 10,
            "auto_update": False,
            "data_sources": {},
            "api_key": "Not Available",
            "email": "Not Available",
            "username": "Not Available",
            "cloud_sync_enabled": False  # ✅ Default to False
        }

        if os.path.exists(SETTINGS_FILE):
            with open(SETTINGS_FILE, "r", encoding="utf-8") as f:
                try:
                    user_settings = json.load(f)
                    return {**default_settings, **user_settings}  # ✅ Merge with defaults
                except json.JSONDecodeError:
                    print("⚠️ Error: Settings file corrupted. Resetting to defaults.")
                    return default_settings

        return default_settings

    # Save FinceptSettingModule to the JSON file
    def save_settings(self):
        """Save updated settings to JSON file, ensuring cloud sync state is persisted."""
        if os.path.exists(SETTINGS_FILE):
            with open(SETTINGS_FILE, "r", encoding="utf-8") as f:
                try:
                    existing_settings = json.load(f)
                except json.JSONDecodeError:
                    print("⚠️ Error: FinceptSettingModule.json is corrupted. Resetting to defaults.")
                    existing_settings = {}
        else:
            existing_settings = {}

        existing_settings.update(self.settings)  # ✅ Merge new settings

        try:
            with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
                json.dump(existing_settings, f, indent=4)
            print("✅ Settings updated successfully.")
        except Exception as e:
            print(f"❌ Error writing FinceptSettingModule.json: {e}")

