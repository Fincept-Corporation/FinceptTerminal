from textual.screen import Screen
from textual.widgets import Input, Button, Static, Header, Footer
from textual.containers import Center, Vertical, Container, Horizontal
from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import save_user_data
import asyncio

import requests

# Set your API base URL (adjust if needed)
API_BASE_URL = "https://finceptapi.share.zrok.io"

def login_user(email: str, password: str):
    """
    Call the API /login endpoint with the provided email and password.
    Returns a dictionary with the JSON response.
    """
    url = f"{API_BASE_URL}/login"
    payload = {"email": email, "password": password}
    try:
        response = requests.post(url, json=payload)
        # Raise an HTTPError for bad status codes (4xx, 5xx)
        response.raise_for_status()
        return response.json()
    except requests.RequestException as e:
        # In case of any error, return a dict with a 'detail' key
        return {"detail": str(e)}


class LoginScreen(Screen):
    """Screen for user login."""
    CSS = """
        .super_container{
            background: #282c34;
            align: center middle;
        }
        #base_container{
            padding: 1;
            align: center top;
            height: 40%;
            width: 30%;
            border: round #ffa500;
        }
        .header {
            text-align: center;
            background: #1c1c1c; /* Charcoal black for header */
            color: #ffa500; /* Orange text for header title */
            border: heavy #333333; /* Slightly lighter border for the header */
            width: 100%;
        }
        .submit_horizontal{
            margin: 1;
            width: 100%;
            align: center top;
        }
        .button{
            width: 50%;
            border: round #ffa500;
        }
        .input-field{
            border: round #ffa500
        }
    """

    def compose(self):
        """Compose the login screen layout."""
        yield Header()
        with Container(classes="super_container"):
            with Vertical(id="base_container"):
                yield Static("User Login", classes="header")
                yield Input(placeholder="Enter your email", id="email", classes="input-field")
                yield Input(placeholder="Enter your password", id="password", password=True, classes="input-field")
                with Horizontal(classes="submit_horizontal"):
                    yield Button("Login", id="login", classes="button")
                    yield Button("Back", id="back", classes="button")
        yield Footer()

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle button presses."""
        button = event.button
        if button.id == "login":
            await self.handle_login()
        elif button.id == "back":
            await self.app.pop_screen()

    async def handle_login(self):
        """Handle the login button logic asynchronously."""
        email = self.query_one("#email", Input).value.strip()
        password = self.query_one("#password", Input).value.strip()

        # Validate inputs
        if not email or not password:
            self.app.notify("Error: Both email and password are required.", severity="error")
            return

        self.app.notify("Info: Logging in, please wait...", severity="information")
        try:
            # Call the login function in a thread to avoid blocking the UI
            login_response = await asyncio.to_thread(login_user, email, password)
        except Exception as e:
            self.app.notify(f"Error: Login failed: {e}", severity="error")
            return

        # Check if the login response indicates a successful login
        if login_response.get("message") != "Login successful":
            error_detail = login_response.get("detail", "Unknown error occurred during login.")
            self.app.notify(f"Error: Login failed: {error_detail}", severity="error")
            return

        # Save user data (you may choose to save additional details if available)
        save_user_data({
            "email": email,
        })

        self.app.notify("Success: Login successful!", severity="information")
        await self.app.push_screen("dashboard")
