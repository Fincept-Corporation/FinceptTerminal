from textual.screen import Screen
from textual.app import ComposeResult
from textual.containers import Vertical, Center
from textual.widgets import Button


class ExitScreen(Screen):
    """Screen with an Exit button to close the terminal."""
    def compose(self) -> ComposeResult:
        with Center():
            with Vertical():
                yield Button("Exit", id="exit-button", variant="default")