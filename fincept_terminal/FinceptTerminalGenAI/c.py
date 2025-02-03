from textual.app import App, ComposeResult
from textual.containers import Vertical, Horizontal, VerticalScroll
from textual.widgets import Input, Static, Button
import asyncio


class ChatUI(App):
    CSS = """
    Vertical {
        height: 100%;
        width: 100%;
    }

    VerticalScroll {
        height: 85%;
        width: 100%;
        padding: 1;
        border: round green;
        background: black;  /* Chat background */
    }

    Horizontal#input_box {
        height: 10%;
        width: 100%;
        border: round blue;
        padding: 1;
        background: rgb(20, 20, 20);  /* Input box background */
    }

    Input {
        width: 80%;
        height: 3;
        border: round gray;
        padding: 0 1;
    }

    Button {
        width: 20%;
        height: 3;
        border: round gray;
    }

    .user-message {
        background: rgb(30, 144, 255); /* DodgerBlue */
        color: white;
        padding: 1 2;
        border: round;
        margin: 1 0;
        width: auto;
        text-align: left;
    }

    .bot-message {
        background: rgb(34, 139, 34); /* ForestGreen */
        color: white;
        padding: 1 2;
        border: round;
        margin: 1 0;
        width: auto;
        text-align: right;
        align-self: flex-end;  /* Aligns the bot message to the right */
    }
    """

    def compose(self) -> ComposeResult:
        """Create UI components."""
        with Vertical():
            yield VerticalScroll(id="chat_container")
            with Horizontal(id="input_box"):
                yield Input(placeholder="Type a message...", id="chat_input")
                yield Button("Send", id="send_button")

    def on_input_submitted(self, event: Input.Submitted) -> None:
        """Handles user input when Enter is pressed."""
        self.process_chat_message(event.input.value)
        event.input.value = ""  # Clear input field

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handles user input when Send button is pressed."""
        input_field = self.query_one("#chat_input", Input)
        self.process_chat_message(input_field.value)
        input_field.value = ""  # Clear input field

    def process_chat_message(self, message: str):
        """Handles adding user message and triggering bot response."""
        if not message.strip():
            return  # Ignore empty messages

        chat_container = self.query_one("#chat_container", VerticalScroll)

        # Add user message aligned left
        chat_container.mount(Static(f" You: {message} ", classes="user-message"))

        # Simulate bot reply after a short delay
        self.call_later(self.bot_reply)

    async def bot_reply(self):
        """Bot replies with a short delay."""
        await asyncio.sleep(1)
        chat_container = self.query_one("#chat_container", VerticalScroll)

        # Add bot message aligned right
        chat_container.mount(Static(f" Bot: Hi ", classes="bot-message"))


if __name__ == "__main__":
    ChatUI().run()
