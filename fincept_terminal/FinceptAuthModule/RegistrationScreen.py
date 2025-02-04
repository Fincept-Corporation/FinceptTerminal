from textual.screen import Screen
from textual.widgets import Input, Button, Static
from textual.containers import Center, Vertical
from fincept_terminal.FinceptAuthModule.authentication import register_user, verify_otp
from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import save_user_data
import asyncio

class RegistrationScreen(Screen):
    """Screen for user registration."""

    def compose(self):
        """Compose the registration screen layout."""
        with Center():
            with Vertical(id="fincept-registration-container"):
                yield Static("Register a New User", classes="title")
                yield Input(placeholder="Enter your username", id="username", classes="input-field")
                yield Input(placeholder="Enter your email", id="email", classes="input-field")
                yield Input(placeholder="Enter your password", id="password", password=True, classes="input-field")
                yield Button("Submit", id="submit", classes="button")
                with Vertical(id="fincept-otp-container", classes="otp-section"):
                    yield Static("", id="otp-message", classes="info-message")
                    otp_input = Input(placeholder="Enter OTP", id="otp-input", classes="input-field")
                    otp_input.styles.display = "none"  # Hide initially
                    yield otp_input
                    verify_button = Button("Verify OTP", id="verify-otp", classes="button")
                    verify_button.styles.display = "none"  # Hide initially
                    yield verify_button

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle button presses."""
        button = event.button

        if button.id == "submit":
            await self.handle_submit()

        elif button.id == "verify-otp":
            await self.handle_verify_otp()

    async def handle_submit(self):
        """Handle the submit button logic asynchronously."""
        username = self.query_one("#username", Input).value.strip()
        email = self.query_one("#email", Input).value.strip()
        password = self.query_one("#password", Input).value.strip()

        # Validate inputs
        if not username or not email or not password:
            self.app.notify("Error: All fields are required.", severity="error")
            return

        # Step 1: Register user (note: registration response now does not return an API key)
        self.app.notify("Info: Registering user, please wait...", severity="information")
        try:
            registration_response = await asyncio.to_thread(register_user, username, email, password)
        except Exception as e:
            self.app.notify(f"Error: Registration failed: {e}", severity="error")
            return

        # Expect a response like {"message": "Registered. OTP sent."}
        if not registration_response or registration_response.get("message") != "Registered. OTP sent.":
            error_detail = registration_response.get("detail", "Unknown error occurred.")
            self.app.notify(f"Error: Registration failed: {error_detail}", severity="error")
            return

        # Save email and username for later use (API key will be received after OTP verification)
        self.username = username
        self.email = email

        # Step 2: Show OTP input
        self.app.notify("Info: A verification code was sent to your email.", severity="information")
        otp_message = self.query_one("#otp-message", Static)
        otp_message.update("A verification code was sent to your email.")
        otp_message.styles.display = "block"

        otp_input = self.query_one("#otp-input", Input)
        otp_input.styles.display = "block"

        verify_button = self.query_one("#verify-otp", Button)
        verify_button.styles.display = "block"

    async def handle_verify_otp(self):
        """Handle OTP verification asynchronously."""
        otp_input_value = self.query_one("#otp-input", Input).value.strip()
        if not otp_input_value:
            self.app.notify("Error: OTP is required for verification.", severity="error")
            return

        self.app.notify("Info: Verifying OTP, please wait...", severity="information")
        try:
            verification_response = await asyncio.to_thread(verify_otp, self.email, otp_input_value)
        except Exception as e:
            self.app.notify(f"Error: OTP verification failed: {e}", severity="error")
            return

        # Expected response: {"message": "Verified", "api_key": <user_api_key>}
        if verification_response.get("message") != "Verified":
            self.app.notify("Error: Invalid OTP. Registration failed.", severity="error")
            return

        # Save the API key from the verification response
        self.api_key = verification_response.get("api_key")

        # Step 3: Save user data locally
        save_user_data({
            "username": self.username,
            "email": self.email,
            "api_key": self.api_key,
        })

        # Step 4: Navigate to the dashboard
        self.app.notify("Success: Registration complete!", severity="information")
        await self.app.push_screen("dashboard")
