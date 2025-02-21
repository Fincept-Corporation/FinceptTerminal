from textual.app import ComposeResult
from textual.containers import Container, VerticalScroll, Horizontal
from textual.widgets import Input, Button, Static, TabPane, TabbedContent
import asyncio
import json
import requests

# ✅ Server API Configuration
BASE_URL = "https://finceptapi.share.zrok.io"
API_KEY = "33f51a75-c36b-4003-ac71-8a75351b35f6"
HEADERS = {
    "X-API-Key": API_KEY,
    "Content-Type": "application/json"
}


class BaseForumTab(Container):
    """Base class for forum chat tabs, handling message retrieval and sending."""

    def __init__(self, forum_name: str, channel: str):
        """Initialize forum tab with unique properties."""
        self.forum_name = forum_name
        self.channel = channel
        self.display_id = f"forum_chat_display_{channel}"
        self.input_id = f"forum_chat_input_{channel}"
        self.button_id = f"send_forum_message_{channel}"
        super().__init__()

    def compose(self) -> ComposeResult:
        """Compose the forum chat UI."""
        yield Static(self.forum_name, classes="forum_header")

        with Container(classes="forum_container"):
            # ✅ Chat Message Display
            with Container(classes="forum_message_container"):
                yield VerticalScroll(id=self.display_id, classes="forum_message_vertical")

            # ✅ Chat Input Section
            with Container(classes="forum_input_container"):
                with Horizontal(classes="chat_input_container"):
                    yield Input(id=self.input_id, placeholder="Type your message...", classes="chat_input")
                    yield Button("Send", id=self.button_id, classes="send_button")

    async def _on_show(self):
        """Fetch and display forum messages when the tab is opened."""
        self.app.notify("Fetching Forum Messages")
        await asyncio.create_task(self.fetch_forum_messages())

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle send button press."""
        if event.button.id == self.button_id:
            self.app.notify("Sending Message to forum...")
            await asyncio.create_task(self.send_message())

    async def send_message(self):
        """Send the user's message to the Fincept server and immediately update chat display."""
        chat_input = self.query_one(f"#{self.input_id}", Input)
        message = chat_input.value.strip()

        if not message:
            self.app.notify("⚠ Message cannot be empty!", severity="warning")
            return

        # ✅ Prepare payload
        payload = {"message": message}

        try:
            # ✅ Using `asyncio.to_thread()` to execute `requests.post()` in a separate thread
            response = await asyncio.to_thread(
                requests.post,
                f"{BASE_URL}/chat/{self.channel}/message",
                headers=HEADERS,
                data=json.dumps(payload)
            )

            # ✅ Ensure response can be parsed safely
            try:
                response_data = response.json()
            except Exception as e:
                response_data = {"error": str(e)}

            if response.status_code == 200:
                self.app.notify("✅ Message sent successfully!", severity="information")

                # ✅ Clear input after sending
                chat_input.value = ""

                forum_display = self.query_one(f"#{self.display_id}", VerticalScroll)
                # ✅ Scroll to the latest message smoothly
                forum_display.scroll_end(animate=True)

                # ✅ Fetch latest messages from server (to show messages from others)
                await asyncio.create_task(self.fetch_forum_messages())

            else:
                error_msg = response_data.get("error", "Unknown Error")
                self.app.notify(f"⚠ Failed to send message: {error_msg}", severity="error")

        except requests.RequestException as e:
            self.app.notify(f"❌ Network error while sending message: {e}", severity="error")
        except Exception as e:
            self.app.notify(f"❌ Unexpected error: {e}", severity="error")

    async def fetch_forum_messages(self):
        """Fetch all forum messages from the Fincept server and update chat display."""
        forum_display = self.query_one(f"#{self.display_id}", VerticalScroll)

        try:
            # ✅ Fetch messages from the specific forum channel
            url = f"{BASE_URL}/chat/{self.channel}/messages"
            response = await asyncio.to_thread(requests.get, url, headers=HEADERS)

            print(f"GET /chat/{self.channel}/messages")
            print("Status Code:", response.status_code)

            if response.status_code == 200:
                messages = response.json()

                # ✅ Ensure 'chat_messages' exists in response
                chat_messages = messages.get("chat_messages", [])

                if not chat_messages:
                    self.app.notify("⚠ No messages found.", severity="warning")
                    return

                # ✅ Clear previous messages
                await forum_display.remove_children()

                # ✅ Display each message
                for msg in chat_messages:
                    chat_data = msg.get("chat_data", {})
                    username = chat_data.get("username", "Unknown User")
                    text = chat_data.get("message", "No message")

                    chat_bubble = Static(f"[bold cyan]{username}:[/bold cyan] {text}", classes="forum_message")
                    await forum_display.mount(chat_bubble)

                # ✅ Auto-scroll to the latest message
                forum_display.scroll_end(animate=True)

            else:
                self.app.notify(f"⚠ Failed to fetch messages. Server response: {response.text}", severity="warning")

        except requests.RequestException as e:
            self.app.notify(f"❌ Error fetching messages: {e}", severity="error")


# ✅ Individual Forum Tabs (Global & 7 Continents)
class GlobalForumTab(BaseForumTab):
    def __init__(self): super().__init__("🌍 Global Forum Chat", "global")


class AsiaForumTab(BaseForumTab):
    def __init__(self): super().__init__("🌏 Asia Forum Chat", "asia")


class AfricaForumTab(BaseForumTab):
    def __init__(self): super().__init__("🌍 Africa Forum Chat", "africa")


class NorthAmericaForumTab(BaseForumTab):
    def __init__(self): super().__init__("🌎 North America Forum Chat", "north_america")


class AntarcticaForumTab(BaseForumTab):
    def __init__(self): super().__init__("🌍 Antarctica Forum Chat", "antarctica")


class OceaniaForumTab(BaseForumTab):
    def __init__(self): super().__init__("🌏 Oceania Forum Chat", "oceania")


class SouthAmericaForumTab(BaseForumTab):
    def __init__(self): super().__init__("🌎 South America Forum Chat", "south_america")


# ✅ Main ForumTab with all Sub-Tabs
class ForumTab(Container):
    """The main container holding all forum sub-tabs."""

    def compose(self) -> ComposeResult:
        """Compose the forum chat UI with sub-tabs for global and continent-specific chats."""

        with TabbedContent():
            with TabPane("Global Forum", id="global_forum_tab"): yield GlobalForumTab()
            with TabPane("Asia Forum", id="asia_forum_tab"): yield AsiaForumTab()
            with TabPane("Africa Forum", id="africa_forum_tab"): yield AfricaForumTab()
            with TabPane("North America Forum", id="north_america_forum_tab"): yield NorthAmericaForumTab()
            with TabPane("Antarctica Forum", id="antarctica_forum_tab"): yield AntarcticaForumTab()
            with TabPane("Oceania Forum", id="oceania_forum_tab"): yield OceaniaForumTab()
            with TabPane("South America Forum", id="south_america_forum_tab"): yield SouthAmericaForumTab()