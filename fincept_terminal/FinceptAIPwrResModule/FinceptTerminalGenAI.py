from textual.containers import VerticalScroll, Horizontal, Container, Vertical
from textual.widgets import Static, OptionList, Button, Input
import json
import os
import uuid

from fincept_terminal.FinceptSettingModule.FinceptTerminalSettingUtils import get_settings_path
SETTINGS_FILE = get_settings_path()

class GenAITab(VerticalScroll):
    """Generative AI Chatbot with Multi-Session Management"""

    def __init__(self):
        super().__init__()
        self.chat_sessions = {}  # Stores chat histories by session ID
        self.active_chat_id = None  # ID of the current active chat
        self.load_chat_sessions()

    def compose(self):
        """Define the chat interface layout with proper grid structure."""
        # Title
        with Container(classes="genai_container"):
            # ✅ First Column (Spans 2 Rows) - Chat Session Controls
            with Vertical(classes="genai_chat_vertical"):
                with Horizontal(classes="chat_session_controls"):
                    yield Button("New Chat", id="new_chat")
                    yield Button("Delete Chat", id="delete_chat")

                # Chat session selector
                yield OptionList(id="chat_selector")

            # ✅ First Row, Second Column - Chat Message Display
            with Container(classes="genai_message_container"):
                yield VerticalScroll(id="chat_display", classes="genai_message_vertical")

            # ✅ Second Row, Second Column - Chat Input Section
            with Container(classes="genai_input_container"):
                with Horizontal(classes="chat_input_container"):
                    yield Input(id="chat_input", placeholder="Type your message...",classes="chat_input")
                    yield Button("Send", id="send_chat", classes="send_button")

    async def on_mount(self):
        """Initialize UI elements on mount."""
        self.populate_chat_sessions()

    def load_chat_sessions(self):
        """Load saved chat sessions from file."""
        if os.path.exists("chat_sessions.json"):
            with open("chat_sessions.json", "r") as file:
                self.chat_sessions = json.load(file)

    def save_chat_sessions(self):
        """Save chat sessions to a file for persistence."""
        with open("chat_sessions.json", "w") as file:
            json.dump(self.chat_sessions, file, indent=4)

    def populate_chat_sessions(self):
        """Populate the session selector with available chat sessions."""
        chat_selector = self.query_one("#chat_selector", OptionList)
        chat_selector.clear_options()

        if not self.chat_sessions:
            chat_selector.add_option("No active chats")
        else:
            for chat_id, chat_data in self.chat_sessions.items():
                # Use the chat name as the prompt
                chat_selector.add_option(chat_data['title'])  # Only display the title (no ID)

    async def on_option_list_option_selected(self, event: OptionList.OptionSelected):
        """Handle selection of a chat session."""

        # Get the selected option
        selected_option = event.option_list.get_option_at_index(event.option_index)

        if selected_option is None:
            self.app.notify("⚠ No chat session selected!", severity="warning")
            return

        # Retrieve the chat name from the prompt
        chat_name = selected_option.prompt

        # Find the chat ID by matching the name
        chat_id = next(
            (cid for cid, cdata in self.chat_sessions.items() if cdata["title"] == chat_name),
            None
        )

        if chat_id is None:
            self.app.notify("⚠ Invalid chat session selected!", severity="error")
            return

        self.active_chat_id = chat_id  # Set the active chat session
        await self.display_chat_messages()

    async def display_chat_messages(self):
        """Display messages for the active chat session with sent/received formatting."""
        chat_display = self.query_one("#chat_display", VerticalScroll)

        # Clear the chat display
        await chat_display.remove_children()

        # Fetch messages from the active chat
        messages = self.chat_sessions[self.active_chat_id].get("messages", [])

        for message in messages:
            role = message.get("role", "Unknown")
            content = message.get("content", "")

            # Format message based on role
            if role == "user":
                await chat_display.mount(
                    Static(f"[bold cyan]You:[/bold cyan] {content}", classes="user-message")
                )
            elif role == "ai":
                await chat_display.mount(
                    Static(f"[bold magenta]GenAI:[/bold magenta] {content}", classes="ai-message")
                )
            else:
                await chat_display.mount(
                    Static(f"[bold red]Unknown:[/bold red] {content}", classes="unknown-message")
                )

        self.app.refresh()  # Refresh the UI to update the display

    async def on_button_pressed(self, event):
        """Handle button clicks for chat actions."""
        if event.button.id == "send_chat":
            await self.process_chat_message()
        elif event.button.id == "new_chat":
            self.create_new_chat()
        elif event.button.id == "delete_chat":
            self.delete_active_chat()

    def create_new_chat(self):
        """Create a new chat session."""
        new_chat_id = str(uuid.uuid4())
        self.chat_sessions[new_chat_id] = {"title": f"Chat {len(self.chat_sessions) + 1}", "messages": []}
        self.active_chat_id = new_chat_id
        self.populate_chat_sessions()
        self.save_chat_sessions()
        self.display_chat_messages()

    def delete_active_chat(self):
        """Delete the currently active chat session."""
        if not self.active_chat_id:
            self.app.notify("⚠ No active chat session to delete!", severity="warning")
            return

        # Remove the active chat session
        del self.chat_sessions[self.active_chat_id]
        self.active_chat_id = None

        # Update UI
        self.populate_chat_sessions()
        self.save_chat_sessions()

        # ✅ Properly clear chat display by removing all children
        chat_display = self.query_one("#chat_display", VerticalScroll)
        chat_display.remove_children()  # ✅ Instead of `clear()`, use `remove_children()`

        self.app.notify("✅ Chat session deleted successfully!")

    async def process_chat_message(self):
        chat_input = self.query_one("#chat_input")
        chat_display = self.query_one("#chat_display", VerticalScroll)

        user_message = chat_input.value.strip()
        chat_input.value = ""

        if not user_message:
            self.app.notify("Please enter a message!", severity="warning")
            return

        # Check if an active chat session exists
        if not self.active_chat_id:
            self.app.notify("No active chat session. Please create or select a chat first.", severity="warning")
            return

        # Store & display the user's message
        self.chat_sessions[self.active_chat_id]["messages"].append({"role": "user", "content": user_message})
        await self.display_chat_messages()

        # Display loading indicator
        loading_message = Static("[italic magenta]Generating response...[/italic magenta]")
        await chat_display.mount(loading_message)

        # Fetch AI response asynchronously
        response = await self.query_genai(user_message)

        # Remove loading indicator
        await chat_display.remove_children()

        # Save & display the AI response
        self.chat_sessions[self.active_chat_id]["messages"].append({"role": "ai", "content": response})
        self.save_chat_sessions()

        await self.display_chat_messages()

    async def query_genai(self, user_input):
        """Query the AI model based on user settings asynchronously."""
        settings = self.load_settings()
        source = settings.get("data_sources", {}).get("genai_model", {}).get("source_name", "gemini_api")
        api_key = settings.get("data_sources", {}).get("genai_model", {}).get("apikey")

        if not api_key:
            return "❌ Error: API key is missing!"

        if source == "gemini_api":
            return await self.query_gemini_direct(user_input, api_key)  # ✅ Await async function
        elif source == "openai_api":
            return await self.query_openai(user_input, api_key)  # ✅ Await async function
        else:
            return "❌ Error: Unsupported AI model."

    def load_settings(self):
        """
        Load user settings from settings.json.

        Returns:
            dict: A dictionary containing the settings data.
        """
        if not os.path.exists(SETTINGS_FILE):
            self.app.notify("⚠ settings.json not found! Using default settings.", severity="warning")
            return {}  # Return an empty dictionary if the file doesn't exist

        try:
            with open(SETTINGS_FILE, "r") as file:
                settings = json.load(file)
                self.app.notify("✅ Successfully loaded settings.")
                return settings
        except json.JSONDecodeError as e:
            self.app.notify(f"❌ Error parsing settings.json: {e}", severity="error")
            return {}

    async def query_gemini_direct(self, user_input, api_key):
        """Query Gemini AI."""
        try:
            import google.generativeai as genai
            genai.configure(api_key=api_key)
            model = genai.GenerativeModel("gemini-1.5-flash")
            response = model.generate_content(user_input)
            return response.text
        except Exception as e:
            return f"Error querying Gemini: {str(e)}"

    async def query_openai(self, user_input, api_key):
        """Query OpenAI API."""
        try:
            from openai import OpenAI
            client = OpenAI(api_key=api_key)
            completion = client.chat.completions.create(
                model="gpt-4o-mini",
                messages=[{"role": "system", "content": "You are a helpful assistant."}, {"role": "user", "content": user_input}]
            )
            return completion.choices[0].message.content
        except Exception as e:
            return f"Error querying OpenAI: {str(e)}"
