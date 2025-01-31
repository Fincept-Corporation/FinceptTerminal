# EconomicAnalysisScreen.py
from textual.app import App
from textual.screen import Screen
from textual.widgets import Static, Button
from textual.containers import Center, Vertical

class EconomicAnalysisScreen(Screen):
    """Screen for Economic Analysis."""

    def compose(self):
        """Compose the layout of the Economic Analysis screen."""
        with Center():
            with Vertical():
                yield Static("Welcome to Economic Analysis!", classes="title")
                yield Static("Here is the data for Economic Analysis.", classes="content")
                yield Button("Back to Dashboard", id="back-button", variant="default")

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle button presses."""
        button = event.button
        if button.id == "back-button":
            # Pop the current screen and return to the previous screen (Dashboard)
            await self.app.pop_screen()  # This is asynchronous and prevents blocking
