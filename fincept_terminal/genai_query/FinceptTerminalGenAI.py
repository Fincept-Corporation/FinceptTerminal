import asyncio
import os
import sqlite3
import fitz  # PyMuPDF for PDF processing
import google.generativeai as genai
from textual.app import App, ComposeResult
from textual.widgets import Footer, Input, Button, Static, ListView, ListItem
from textual.containers import Horizontal, VerticalScroll, Container
from textual.reactive import reactive

# ✅ SQLite Database Setup
DB_PATH = "chat_history.db"

def init_db():
    """Initialize SQLite database and create table if it doesn't exist."""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS chats (
            session_name TEXT PRIMARY KEY,
            chat_history TEXT
        )
    """)
    conn.commit()
    conn.close()

def save_chat(session_name, chat_history):
    """Save chat history to the database."""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute(
        "INSERT OR REPLACE INTO chats (session_name, chat_history) VALUES (?, ?)",
        (session_name, "\n".join(chat_history)),
    )
    conn.commit()
    conn.close()

def load_chats():
    """Load all chat sessions from the database."""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("SELECT session_name FROM chats")
    chats = [row[0] for row in cursor.fetchall()]
    conn.close()
    return chats

def load_chat_history(session_name):
    """Retrieve chat history for a specific session."""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("SELECT chat_history FROM chats WHERE session_name = ?", (session_name,))
    result = cursor.fetchone()
    conn.close()
    return result[0].split("\n") if result else []

class ChatApp(App):
    """A finance-focused terminal-based chat UI with Gemini AI, allowing document-based Q&A."""

    CSS = """
    Screen {
        align: center middle;
        padding: 1 1; /* Padding uses integers */
        background: #1e1e2e;
    }

    #sidebar {
        width: 25; /* Width in percentage */
        height: 100; /* Full height */
        background: #11111b;
        border-right: solid #cdd6f4;
        padding: 1 1; /* Padding with two integers (vertical horizontal) */
        color: #cdd6f4;
    }

    #chat-container {
        width: 100; /* Remaining width for chat container */
        height: 85; /* Height to leave space for inputs */
        border: solid #cdd6f4;
        padding: 1 1;
        background: #11111b;
        color: #cdd6f4;
        overflow: auto; /* Enables scrolling */
    }

    #document-input {
        width: 100; /* Full width for document input */
        background: #313244;
        color: #cdd6f4;
        margin-bottom: 1; /* Valid margin property */
    }

    .message-user {
        color: #89b4fa;
        background: #1e66f5;
        padding: 1 1;
        border: solid #1e66f5;
        max-width: 70;
        align: right middle; /* Fixed alignment */
        margin-bottom: 1;
    }

    .message-finbot {
        color: #000000;
        background: #f9e2af;
        padding: 1 1;
        border: solid #f9e2af;
        max-width: 70;
        align: left middle; /* Fixed alignment */
        margin-bottom: 1;
    }

    #input-container {
        width: 100;
        height: 15; /* Fixed height for input fields */
        background: #11111b;
        padding: 1 1; /* Valid padding */
    }

    #input-box {
        width: 75; /* Chat input takes most of the row */
        background: #313244;
        color: #cdd6f4;
        margin-right: 1; /* Valid margin */
    }

    #send-btn {
        width: 20; /* Fixed width for the button */
        background: #1e66f5;
        color: white;
    }
    """

    chat_sessions = reactive({})
    current_session = reactive("Chat 1")
    pdf_content = reactive("")  # Store extracted PDF text

    def compose(self) -> ComposeResult:
        """Compose UI with docked input fields and expanded chat history."""
        self.sidebar = ListView(id="sidebar")
        self.chat_display = VerticalScroll(id="chat-container")
        self.document_input = Input(placeholder="Enter document name (e.g., financial_report.pdf)", id="document-input")
        self.input_box = Input(placeholder="Ask about stocks, trends, or finance...", id="input-box")
        self.send_button = Button("Send", id="send-btn")

        # Sidebar and chat container
        yield Horizontal(self.sidebar, Container(self.chat_display))

        # Docked input fields at the bottom
        yield Container(
            Horizontal(self.input_box, self.send_button, id="input-container"),
            self.document_input
        )
        yield Footer()

    def on_mount(self) -> None:
        """Called after the UI is fully initialized."""
        init_db()
        self.chat_sessions = {session: load_chat_history(session) for session in load_chats()}
        if not self.chat_sessions:
            self.start_new_chat()
        self.update_sidebar()

    def start_new_chat(self):
        """Creates a new chat session."""
        new_chat_name = f"Chat {len(self.chat_sessions) + 1}"  # Increment session count
        self.chat_sessions[new_chat_name] = []  # Initialize new chat session
        self.current_session = new_chat_name  # Set the new session as active
        self.update_sidebar()  # Refresh the sidebar with the new session
        self.update_chat_display()  # Clear the chat display for the new session

    def update_sidebar(self):
        """Refresh sidebar with available chat sessions."""
        self.sidebar.clear()
        self.sidebar.append(ListItem(Button("New Chat", id="new-chat-btn")))  # Add New Chat Button

        for session_name in self.chat_sessions.keys():
            self.sidebar.append(ListItem(Static(session_name)))  # List all chat sessions

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handles button clicks."""
        if event.button.id == "new-chat-btn":
            self.start_new_chat()
        elif event.button.id == "send-btn":
            asyncio.create_task(self.send_message())

    async def send_message(self):
        """Handles user input and queries Gemini AI asynchronously, considering document context if provided."""
        user_message = self.input_box.value.strip()
        document_name = self.document_input.value.strip()

        if not user_message:
            return

        self.chat_sessions.setdefault(self.current_session, []).append(f"User: {user_message}")
        self.input_box.value = ""
        self.update_chat_display()

        # ✅ If document name is provided, extract text and use as context
        if document_name:
            document_content = self.load_document_content(document_name)
            if document_content:
                ai_response = await asyncio.to_thread(self.query_gemini_with_context, user_message, document_content)
            else:
                ai_response = "⚠️ Document not found. Please check the filename and try again."
        else:
            ai_response = await asyncio.to_thread(self.query_gemini_normal, user_message)

        self.chat_sessions[self.current_session].append(f"FinBot: {ai_response}")
        self.update_chat_display()

        save_chat(self.current_session, self.chat_sessions[self.current_session])

    def load_document_content(self, document_name):
        """Loads text from a PDF file if it exists."""
        file_path = os.path.join(os.getcwd(), document_name)
        if os.path.exists(file_path) and file_path.endswith(".pdf"):
            return self.extract_text_from_pdf(file_path)
        return None

    def extract_text_from_pdf(self, file_path):
        """Extracts text from a PDF using PyMuPDF (fitz)."""
        doc = fitz.open(file_path)
        text = "\n".join([page.get_text() for page in doc])
        doc.close()
        return text

    def update_chat_display(self):
        """Updates the chat display with the latest messages from the selected session."""
        self.chat_display.remove_children()
        chat_history = self.chat_sessions.get(self.current_session, [])
        for message in chat_history[-50:]:
            sender = "user" if "User:" in message else "finbot"
            message_widget = Static(message, classes=f"message-{sender}")
            self.chat_display.mount(message_widget)
        self.chat_display.scroll_end(animate=False)

    def query_gemini_with_context(self, user_message, document_content):
        """Queries Gemini AI with document content as additional context."""
        api_key = "AIzaSyDP_scMwlOiquieXazTxtWargw_7WD1PiM"
        if not api_key:
            return "❌ API key missing."

        try:
            genai.configure(api_key=api_key)
            model = genai.GenerativeModel("models/gemini-1.5-flash")
            prompt = f"Document Context:\n{document_content[:5000]}\n\nUser Question: {user_message}"
            response = model.generate_content(prompt)
            return response.text if response.text else "⚠️ No response from Gemini AI."
        except Exception as e:
            return f"⚠️ Error: {e}"

    def query_gemini_with_context(self, user_message, document_content):
        """Queries Gemini AI with document content as additional context."""
        api_key = "AIzaSyDP_scMwlOiquieXazTxtWargw_7WD1PiM" # Fetch API key from environment

        if not api_key or api_key.strip() == "":
            return "❌ API key missing. Please set GEMINI_API_KEY in your environment variables."

        try:
            genai.configure(api_key=api_key)
            model = genai.GenerativeModel("models/gemini-1.5-flash")
            prompt = f"Document Context:\n{document_content[:5000]}\n\nUser Question: {user_message}"

            response = model.generate_content(prompt)
            return response.text if response.text else "⚠️ No response from Gemini AI."
        except Exception as e:
            return f"⚠️ Error: {e}"


if __name__ == "__main__":
    app = ChatApp()
    app.run()
