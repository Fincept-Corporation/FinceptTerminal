from textual.screen import Screen
from textual.app import ComposeResult
from textual.containers import Center, Vertical
from textual.widgets import Button

class ExitScreen(Screen):
    """Screen with an Exit button to close the terminal."""

    def compose(self) -> ComposeResult:
        """Create UI components."""
        with Center():
            with Vertical():
                yield Button("Exit", id="exit-button", variant="default")

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle exit button press."""
        if event.button.id == "exit-button":
            self.app.exit()  # Correct way to close the application in Textual
