# chat_tab.py - Fixed Chat Tab with Proper UI Management
import dearpygui.dearpygui as dpg
import time
import threading
from datetime import datetime
import uuid
import re
from fincept_terminal.Utils.base_tab import BaseTab


class ChatTab(BaseTab):
    """Chat Tab integrated with Fincept API - Bloomberg Terminal Style"""

    def __init__(self, app):
        super().__init__(app)
        self.app = app
        self.current_session_uuid = None
        self.is_typing = False
        self.message_widgets = []
        self.chat_counter = 1
        self.sessions = []
        self.ui_tags = set()  # Track UI tags to prevent conflicts

        # Get API client from session data
        self.api_client = self.create_api_client()

        # Bloomberg Terminal Colors
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLACK = [0, 0, 0]

    def get_label(self):
        return "💬 AI Chat"

    def create_api_client(self):
        """Create API client from session data"""
        try:
            from fincept_terminal.Utils.APIClient.api_client import create_api_client
            return create_api_client(self.app.get_session_data())
        except Exception as e:
            print(f"Error deleting session: {e}")
            self.safe_set_value("system_status", f"ERROR: {str(e)}")

    def refresh_sessions(self):
        """Refresh sessions list"""
        try:
            self.load_chat_sessions()
        except Exception as e:
            print(f"Error refreshing sessions: {e}")

    def delete_current_session(self):
        """Delete current session"""
        try:
            if self.current_session_uuid:
                self.delete_session_callback(self.current_session_uuid)
        except Exception as e:
            print(f"Error deleting current session: {e}")

    def filter_sessions_callback(self, sender, app_data):
        """Filter sessions based on search"""
        try:
            search_term = app_data.lower()

            # Clear and reload filtered
            self.safe_delete_item("session_list_area", children_only=True)

            for session in self.sessions:
                if search_term in session["title"].lower():
                    self.create_session_item(session)
        except Exception as e:
            print(f"Error filtering sessions: {e}")

    def clear_current_chat(self):
        """Clear current chat messages"""
        try:
            if self.current_session_uuid:
                self.create_welcome_screen()
                self.safe_set_value("system_status", "STATUS: CHAT CLEARED")
        except Exception as e:
            print(f"Error clearing current chat: {e}")

    # Function key callbacks
    def show_help(self):
        """Show help information"""
        try:
            self.send_quick_message("help")
        except Exception as e:
            print(f"Error showing help: {e}")

    def focus_sessions(self):
        """Focus session search"""
        try:
            if dpg.does_item_exist("session_search"):
                dpg.focus_item("session_search")
        except Exception as e:
            print(f"Error focusing sessions: {e}")

    def focus_search(self):
        """Focus search field"""
        try:
            if dpg.does_item_exist("session_search"):
                dpg.focus_item("session_search")
        except Exception as e:
            print(f"Error focusing search: {e}")

    def show_stats(self):
        """Show chat statistics"""
        try:
            if self.api_client:
                result = self.api_client.get_chat_stats()
                if result["success"]:
                    stats = result["stats"]
                    stats_msg = f"Sessions: {stats['total_sessions']}, Messages: {stats['total_messages']}"
                    self.safe_set_value("system_status", f"STATS: {stats_msg}")
        except Exception as e:
            print(f"Error getting stats: {e}")

    def execute_quick_command(self, command):
        """Execute quick command"""
        try:
            if command == "NEW_SESSION":
                self.new_chat_callback()
            elif command == "GET_HELP":
                self.send_quick_message("help")
            elif command == "MARKET_ANALYSIS":
                self.send_quick_message("market analysis")
            elif command == "PORTFOLIO_HELP":
                self.send_quick_message("portfolio help")
            elif command == "SYSTEM_STATUS":
                self.show_stats()
        except Exception as e:
            print(f"Error executing quick command: {e}")

    def send_quick_message(self, message_type):
        """Send a quick predefined message"""
        try:
            messages = {
                "help": "I need help with using this AI assistant",
                "market analysis": "Can you help me with market analysis?",
                "portfolio help": "I need advice on portfolio management"
            }

            message = messages.get(message_type, message_type)

            # Auto-create session if needed
            if not self.current_session_uuid:
                if not self.create_new_session(f"Quick {message_type.title()}"):
                    return

            # Clear welcome screen
            self.safe_delete_item("welcome_screen")

            # Send message
            self.send_message_to_api(message)
        except Exception as e:
            print(f"Error sending quick message: {e}")

    # Helper Methods
    def generate_smart_title(self, message):
        """Generate smart session title"""
        try:
            # Clean message
            clean_msg = re.sub(r'[^\w\s]', '', message).strip()
            words = clean_msg.split()

            if len(words) <= 2:
                return clean_msg[:20] if clean_msg else "New Chat"
            else:
                return ' '.join(words[:2]) + "..."
        except Exception as e:
            print(f"Error generating title: {e}")
            return "Chat Session"

    def cleanup(self):
        """Clean up chat resources"""
        try:
            self.sessions = []
            self.current_session_uuid = None
            self.ui_tags.clear()
        except Exception as e:
            print(f"Error during cleanup: {e}")
            return None

    def safe_add_text(self, text, color=None, tag=None, parent=None):
        """Safe text addition with proper error handling"""
        try:
            kwargs = {}
            if color:
                kwargs["color"] = color
            if tag:
                kwargs["tag"] = tag
                self.ui_tags.add(tag)
            if parent:
                kwargs["parent"] = parent

            return dpg.add_text(text, **kwargs)
        except Exception as e:
            print(f"Error adding text '{text}': {e}")
            return None

    def safe_delete_item(self, tag, children_only=False):
        """Safe item deletion"""
        try:
            if dpg.does_item_exist(tag):
                dpg.delete_item(tag, children_only=children_only)
                if tag in self.ui_tags:
                    self.ui_tags.remove(tag)
                return True
        except Exception as e:
            print(f"Error deleting item {tag}: {e}")
        return False

    def safe_set_value(self, tag, value):
        """Safe value setting"""
        try:
            if dpg.does_item_exist(tag):
                dpg.set_value(tag, value)
                return True
        except Exception as e:
            print(f"Error setting value for {tag}: {e}")
        return False

    def safe_get_value(self, tag, default=""):
        """Safe value getting"""
        try:
            if dpg.does_item_exist(tag):
                return dpg.get_value(tag)
        except Exception as e:
            print(f"Error getting value for {tag}: {e}")
        return default

    def create_content(self):
        """Create chat interface content"""
        try:
            if not self.api_client:
                self.create_error_content("API client not available")
                return

            # Check authentication
            if not self.api_client.is_authenticated():
                self.create_error_content("Authentication required")
                return

            self.create_chat_interface()
            self.load_chat_sessions()

        except Exception as e:
            print(f"Error creating chat content: {e}")
            self.create_error_content(f"Failed to initialize chat: {str(e)}")

    def create_error_content(self, error_message):
        """Create error content when API is not available"""
        self.safe_add_text("🚨 Chat Error", color=self.BLOOMBERG_RED)
        dpg.add_separator()
        self.safe_add_text(error_message, color=self.BLOOMBERG_WHITE)
        dpg.add_spacer(height=20)
        dpg.add_button(label="Refresh", callback=self.refresh_content)

    def refresh_content(self):
        """Refresh chat content"""
        try:
            # This will be handled by the main app
            print("Chat content refresh requested")
        except Exception as e:
            print(f"Error refreshing content: {e}")

    def create_chat_interface(self):
        """Create the main chat interface"""
        try:
            # Header
            self.create_terminal_header()
            dpg.add_separator()

            # Function keys
            self.create_function_keys()
            dpg.add_separator()

            # Main layout
            with dpg.group(horizontal=True):
                # Left panel - Chat Sessions
                self.create_chat_sessions_panel()

                # Center panel - Chat Interface
                self.create_chat_interface_panel()

                # Right panel - Command Panel
                self.create_command_panel()

            # Status bar
            dpg.add_separator()
            self.create_status_bar()

        except Exception as e:
            print(f"Error creating chat interface: {e}")

    def create_terminal_header(self):
        """Create terminal header"""
        try:
            with dpg.group(horizontal=True):
                self.safe_add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                self.safe_add_text("AI ASSISTANT", color=self.BLOOMBERG_WHITE)
                self.safe_add_text(" | ", color=self.BLOOMBERG_GRAY)

                user_type = self.app.get_user_type()
                if user_type == "guest":
                    self.safe_add_text("👤 Guest Mode", color=self.BLOOMBERG_YELLOW)
                else:
                    user_info = self.app.get_session_data().get('user_info', {})
                    username = user_info.get('username', 'User')
                    self.safe_add_text(f"🔑 {username}", color=self.BLOOMBERG_GREEN)

                self.safe_add_text(" | ", color=self.BLOOMBERG_GRAY)
                self.safe_add_text(datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                                   tag="chat_header_time")
        except Exception as e:
            print(f"Error creating header: {e}")

    def create_function_keys(self):
        """Create function keys bar"""
        try:
            with dpg.group(horizontal=True):
                function_keys = [
                    ("F1:HELP", self.show_help),
                    ("F2:SESSIONS", self.focus_sessions),
                    ("F3:NEW", self.new_chat_callback),
                    ("F4:SEARCH", self.focus_search),
                    ("F5:CLEAR", self.clear_current_chat),
                    ("F6:STATS", self.show_stats)
                ]
                for key_label, callback in function_keys:
                    dpg.add_button(label=key_label, width=100, height=25, callback=callback)
        except Exception as e:
            print(f"Error creating function keys: {e}")

    def create_chat_sessions_panel(self):
        """Create left panel for chat sessions"""
        try:
            with dpg.child_window(width=350, height=600, border=True, tag="chat_sessions_panel"):
                # Header
                self.safe_add_text("CHAT SESSIONS", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                # Controls
                with dpg.group(horizontal=True):
                    dpg.add_button(label="NEW", callback=self.new_chat_callback, width=60, height=25)
                    dpg.add_button(label="REFRESH", callback=self.refresh_sessions, width=80, height=25)
                    dpg.add_button(label="DELETE", callback=self.delete_current_session, width=70, height=25)

                # Search
                dpg.add_input_text(hint="Search sessions...", width=-1, tag="session_search",
                                   callback=self.filter_sessions_callback)
                dpg.add_separator()

                # Stats
                self.safe_add_text("SESSION STATISTICS", color=self.BLOOMBERG_YELLOW)
                self.safe_add_text("Total Sessions: 0", tag="total_sessions")
                self.safe_add_text("Total Messages: 0", tag="total_messages")
                self.safe_add_text("Active Session: None", tag="active_session_info")
                dpg.add_separator()

                # Session list
                self.safe_add_text("ACTIVE SESSIONS", color=self.BLOOMBERG_YELLOW)
                dpg.add_child_window(height=-1, border=False, tag="session_list_area")

        except Exception as e:
            print(f"Error creating sessions panel: {e}")

    def create_chat_interface_panel(self):
        """Create optimized chat interface panel"""
        try:
            with dpg.child_window(width=850, height=600, border=True, tag="chat_interface_panel"):
                # Compact header
                with dpg.group(horizontal=True):
                    self.safe_add_text("AI CHAT", color=self.BLOOMBERG_ORANGE)
                    self.safe_add_text(" | ", color=self.BLOOMBERG_GRAY)
                    self.safe_add_text("No Active Session", color=self.BLOOMBERG_WHITE, tag="current_session_name")

                dpg.add_separator()

                # Optimized messages area
                dpg.add_child_window(
                    height=470,  # Increased height for better space utilization
                    border=True,
                    tag="messages_display_area"
                )

                dpg.add_separator()

                # Compact input area
                self.safe_add_text("INPUT", color=self.BLOOMBERG_YELLOW)
                with dpg.group(horizontal=True):
                    dpg.add_input_text(
                        hint="Type message...",
                        width=600,  # Increased width
                        height=40,  # Reduced height
                        multiline=True,
                        tag="message_input_field",
                        callback=self.message_input_callback,
                        on_enter=True
                    )
                    with dpg.group():
                        dpg.add_button(
                            label="SEND",
                            callback=self.send_message_callback,
                            width=80,
                            height=20
                        )
                        dpg.add_button(
                            label="CLEAR",
                            callback=self.clear_input,
                            width=80,
                            height=18
                        )

                # Compact status
                self.safe_add_text("Ready", tag="input_status", color=self.BLOOMBERG_GRAY)

            # Create welcome screen
            self.create_welcome_screen()

        except Exception as e:
            print(f"Error creating interface panel: {e}")

    def create_command_panel(self):
        """Create right panel for commands"""
        try:
            with dpg.child_window(width=300, height=600, border=True, tag="command_panel"):
                self.safe_add_text("COMMAND CENTER", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                # API Status
                self.safe_add_text("API STATUS", color=self.BLOOMBERG_YELLOW)
                if self.api_client and self.api_client.is_authenticated():
                    self.safe_add_text("● Connected", color=self.BLOOMBERG_GREEN)
                    user_type = self.api_client.user_type.title()
                    self.safe_add_text(f"● {user_type} User", color=self.BLOOMBERG_WHITE)
                    self.safe_add_text(f"● Requests: {self.api_client.get_request_count()}",
                                       color=self.BLOOMBERG_WHITE)
                else:
                    self.safe_add_text("● Disconnected", color=self.BLOOMBERG_RED)

                dpg.add_separator()

                # Quick commands
                self.safe_add_text("QUICK COMMANDS", color=self.BLOOMBERG_YELLOW)
                commands = [
                    ("NEW_SESSION", "Create new chat session"),
                    ("GET_HELP", "Get AI assistance"),
                    ("MARKET_ANALYSIS", "Market insights"),
                    ("PORTFOLIO_HELP", "Portfolio advice"),
                    ("SYSTEM_STATUS", "Check system status")
                ]

                for cmd, desc in commands:
                    dpg.add_button(label=cmd, width=-1, height=25,
                                   callback=lambda s, a, command=cmd: self.execute_quick_command(command))
                    self.safe_add_text(f"  {desc}", color=self.BLOOMBERG_GRAY)

                dpg.add_separator()

                # System info
                self.safe_add_text("SYSTEM INFO", color=self.BLOOMBERG_YELLOW)
                self.safe_add_text("Chat API: READY", color=self.BLOOMBERG_GREEN)
                self.safe_add_text("Response Time: <100ms", color=self.BLOOMBERG_GREEN)
                self.safe_add_text("Last Update:", color=self.BLOOMBERG_GRAY)
                self.safe_add_text(datetime.now().strftime('%H:%M:%S'), tag="last_update_time")

        except Exception as e:
            print(f"Error creating command panel: {e}")

    def create_status_bar(self):
        """Create bottom status bar"""
        try:
            with dpg.group(horizontal=True):
                self.safe_add_text("●", color=self.BLOOMBERG_GREEN)
                self.safe_add_text("CONNECTED", color=self.BLOOMBERG_GREEN)
                self.safe_add_text(" | ", color=self.BLOOMBERG_GRAY)
                self.safe_add_text("AI CHAT", color=self.BLOOMBERG_ORANGE)
                self.safe_add_text(" | ", color=self.BLOOMBERG_GRAY)
                self.safe_add_text("STATUS: READY", color=self.BLOOMBERG_WHITE, tag="system_status")
                self.safe_add_text(" | ", color=self.BLOOMBERG_GRAY)
                user_type = self.app.get_user_type().upper()
                self.safe_add_text(f"USER: {user_type}", color=self.BLOOMBERG_WHITE)
        except Exception as e:
            print(f"Error creating status bar: {e}")

    def create_welcome_screen(self):
        """Create compact welcome screen"""
        try:
            self.safe_delete_item("messages_display_area", children_only=True)

            with dpg.group(parent="messages_display_area", tag="welcome_screen"):
                dpg.add_spacer(height=20)

                # Centered content
                with dpg.group(horizontal=True):
                    dpg.add_spacer(width=150)
                    with dpg.group():
                        self.safe_add_text("FINCEPT AI ASSISTANT", color=self.BLOOMBERG_ORANGE)
                        self.safe_add_text("Financial Intelligence System", color=self.BLOOMBERG_WHITE)

                        dpg.add_spacer(height=15)

                        # Compact status
                        with dpg.group(horizontal=True):
                            self.safe_add_text("●", color=self.BLOOMBERG_GREEN)
                            self.safe_add_text("AI Ready", color=self.BLOOMBERG_GREEN)
                            dpg.add_spacer(width=20)
                            self.safe_add_text("●", color=self.BLOOMBERG_GREEN)
                            self.safe_add_text("API Connected", color=self.BLOOMBERG_GREEN)

                        dpg.add_spacer(height=15)

                        user_type = self.app.get_user_type()
                        if user_type == "guest":
                            self.safe_add_text("Mode: Guest (Limited)", color=self.BLOOMBERG_YELLOW)
                        else:
                            self.safe_add_text("Mode: Registered (Full Access)", color=self.BLOOMBERG_GREEN)

                        dpg.add_spacer(height=20)

                        # Compact instructions
                        self.safe_add_text("Quick Start:", color=self.BLOOMBERG_YELLOW)
                        self.safe_add_text("• Type message below and press Enter")
                        self.safe_add_text("• Use function keys for quick actions")
                        self.safe_add_text("• Browse sessions in left panel")

                        dpg.add_spacer(height=20)

                        dpg.add_button(
                            label="START CHAT",
                            callback=self.new_chat_callback,
                            width=120,
                            height=30
                        )

                    dpg.add_spacer(width=150)

        except Exception as e:
            print(f"Error creating welcome screen: {e}")

    # API Integration Methods
    def load_chat_sessions(self):
        """Load chat sessions from API"""
        if not self.api_client:
            return

        try:
            result = self.api_client.get_chat_sessions()

            if result["success"]:
                self.sessions = result["sessions"]
                self.refresh_sessions_display()
                self.update_stats()
            else:
                print(f"Failed to load sessions: {result.get('error', 'Unknown error')}")

        except Exception as e:
            print(f"Error loading sessions: {e}")

    def create_new_session(self, title="New Conversation"):
        """Create new chat session via API"""
        if not self.api_client:
            return False

        try:
            result = self.api_client.create_chat_session(title)

            if result["success"]:
                session_data = result["session"]
                self.current_session_uuid = session_data["session_uuid"]

                # Update UI
                self.safe_set_value("current_session_name", session_data["title"])
                self.safe_set_value("active_session_info", f"Active: {session_data['title']}")

                # Clear welcome screen
                self.safe_delete_item("welcome_screen")

                # Refresh sessions list
                self.load_chat_sessions()
                return True
            else:
                error_msg = result.get("error", "Failed to create session")
                print(f"Session creation failed: {error_msg}")
                self.safe_set_value("system_status", f"ERROR: {error_msg}")
                return False

        except Exception as e:
            print(f"Error creating session: {e}")
            self.safe_set_value("system_status", f"ERROR: {str(e)}")
            return False

    def send_message_to_api(self, content):
        """Send message to API and get response"""
        if not self.api_client or not self.current_session_uuid:
            return False

        try:
            self.safe_set_value("system_status", "STATUS: SENDING MESSAGE...")

            result = self.api_client.send_chat_message(self.current_session_uuid, content)

            if result["success"]:
                user_msg = result["user_message"]
                ai_msg = result["ai_message"]

                # Add messages to UI
                self.create_message_bubble("user", user_msg["content"])
                self.create_message_bubble("assistant", ai_msg["content"])

                # Update session title if changed
                if result.get("new_title"):
                    self.safe_set_value("current_session_name", result["new_title"])
                    self.safe_set_value("active_session_info", f"Active: {result['new_title']}")

                # Refresh sessions to update message count
                self.load_chat_sessions()

                self.safe_set_value("system_status", "STATUS: READY")
                return True
            else:
                error_msg = result.get("error", "Failed to send message")
                print(f"Message send failed: {error_msg}")
                self.safe_set_value("system_status", f"ERROR: {error_msg}")
                return False

        except Exception as e:
            print(f"Error sending message: {e}")
            self.safe_set_value("system_status", f"ERROR: {str(e)}")
            return False

    def load_session_messages(self, session_uuid):
        """Load messages for a specific session"""
        if not self.api_client:
            return

        try:
            result = self.api_client.get_chat_session(session_uuid)

            if result["success"]:
                messages = result["messages"]

                # Clear existing messages
                self.safe_delete_item("messages_display_area", children_only=True)

                # Add messages
                for msg in messages:
                    self.create_message_bubble(msg["role"], msg["content"])

                self.scroll_to_bottom()
            else:
                print(f"Failed to load messages: {result.get('error', 'Unknown error')}")

        except Exception as e:
            print(f"Error loading messages: {e}")

    # UI Helper Methods
    def create_message_bubble(self, role, content):
        """Create properly aligned and sized message bubbles"""
        try:
            time_str = datetime.now().strftime("%H:%M:%S")
            is_user = role == "user"

            # Get optimal bubble dimensions
            bubble_width, estimated_lines = self.calculate_optimal_bubble_size(content)

            # Wrap text to fit bubble width
            wrapped_content = self.wrap_text_for_bubble(content, bubble_width)

            with dpg.group(parent="messages_display_area"):
                # Minimal spacing between messages
                dpg.add_spacer(height=4)

                if is_user:
                    # USER MESSAGE - Right aligned
                    with dpg.group(horizontal=True):
                        # Dynamic left spacer for right alignment
                        chat_area_width = 845  # Approximate chat area width
                        left_spacer = max(20, chat_area_width - bubble_width - 50)
                        dpg.add_spacer(width=left_spacer)

                        with dpg.group():
                            # Compact header
                            with dpg.group(horizontal=True):
                                self.safe_add_text("YOU", color=self.BLOOMBERG_YELLOW)
                                dpg.add_spacer(width=8)
                                self.safe_add_text(f"{time_str}", color=self.BLOOMBERG_GRAY)

                            # Tight message bubble
                            bubble_height = max(30, estimated_lines * 18 + 16)
                            with dpg.child_window(
                                    width=bubble_width,
                                    height=bubble_height,
                                    border=True,
                                    no_scrollbar=True
                            ):
                                dpg.add_spacer(height=4)
                                self.safe_add_text(wrapped_content, color=self.BLOOMBERG_WHITE)
                else:
                    # AI MESSAGE - Left aligned
                    with dpg.group(horizontal=True):
                        with dpg.group():
                            # Compact header
                            with dpg.group(horizontal=True):
                                self.safe_add_text("AI", color=self.BLOOMBERG_ORANGE)
                                dpg.add_spacer(width=8)
                                self.safe_add_text(f"{time_str}", color=self.BLOOMBERG_GRAY)

                            # Tight message bubble
                            bubble_height = max(30, estimated_lines * 18 + 16)
                            with dpg.child_window(
                                    width=bubble_width,
                                    height=bubble_height,
                                    border=True,
                                    no_scrollbar=True
                            ):
                                dpg.add_spacer(height=4)
                                self.safe_add_text(wrapped_content, color=self.BLOOMBERG_WHITE)

                        # Right spacer to prevent expansion
                        dpg.add_spacer(width=50)

            self.scroll_to_bottom()

        except Exception as e:
            print(f"Error creating message bubble: {e}")

    def wrap_text_for_bubble(self, text, bubble_width):
        """Wrap text to fit within bubble width"""
        try:
            if not text:
                return ""

            # Calculate characters per line based on bubble width
            chars_per_line = max(20, int((bubble_width - 20) / 7))  # 7px per char, 20px padding

            # Split text into lines
            lines = text.split('\n')
            wrapped_lines = []

            for line in lines:
                if len(line) <= chars_per_line:
                    wrapped_lines.append(line)
                else:
                    # Wrap long lines
                    words = line.split(' ')
                    current_line = ""

                    for word in words:
                        if len(current_line + word) <= chars_per_line:
                            current_line += word + " "
                        else:
                            if current_line:
                                wrapped_lines.append(current_line.strip())
                            current_line = word + " "

                    if current_line:
                        wrapped_lines.append(current_line.strip())

            return '\n'.join(wrapped_lines)

        except Exception as e:
            print(f"Error wrapping text: {e}")
            return text

    def calculate_optimal_bubble_size(self, content):
        """Calculate optimal bubble width and estimated lines"""
        try:
            # Clean the content
            clean_content = content.strip()

            # Constants
            MIN_WIDTH = 120
            MAX_WIDTH = 450
            CHAR_WIDTH = 7

            # Calculate metrics
            content_length = len(clean_content)
            explicit_lines = clean_content.count('\n') + 1
            word_count = len(clean_content.split())

            # Determine width based on content characteristics
            if content_length <= 20:
                # Very short messages - minimal width
                width = min(MAX_WIDTH, max(MIN_WIDTH, content_length * CHAR_WIDTH + 20))
            elif content_length <= 80:
                # Short messages - compact width
                width = min(MAX_WIDTH, max(MIN_WIDTH, content_length * CHAR_WIDTH * 0.8 + 30))
            elif content_length <= 200:
                # Medium messages - moderate width
                width = min(MAX_WIDTH, max(250, content_length * CHAR_WIDTH * 0.6 + 40))
            else:
                # Long messages - use larger width for readability
                width = MAX_WIDTH

            # Calculate estimated lines after wrapping
            chars_per_line = max(20, int((width - 20) / CHAR_WIDTH))
            estimated_lines = 1

            for line in clean_content.split('\n'):
                if len(line) <= chars_per_line:
                    estimated_lines += 1
                else:
                    estimated_lines += max(1, len(line) // chars_per_line + 1)

            # Adjust for explicit line breaks
            if explicit_lines > 1:
                estimated_lines = max(estimated_lines, explicit_lines)

            # Single word adjustment
            if word_count == 1 and content_length < 30:
                width = min(width, content_length * CHAR_WIDTH + 25)

            # Ensure reasonable bounds
            width = max(MIN_WIDTH, min(MAX_WIDTH, int(width)))
            estimated_lines = max(1, min(15, estimated_lines))

            return width, estimated_lines

        except Exception as e:
            print(f"Error calculating bubble size: {e}")
            return 200, 1

        except Exception as e:
            print(f"Error calculating bubble size: {e}")
            return 200, 1

    def create_session_item(self, session_data):
        """Create session item in the list"""
        try:
            session_uuid = session_data["session_uuid"]
            title = session_data["title"]
            message_count = session_data["message_count"]
            is_active = session_data["is_active"]

            try:
                updated_at = datetime.fromisoformat(session_data["updated_at"].replace('Z', '+00:00'))
                time_str = updated_at.strftime("%m/%d %H:%M")
            except:
                time_str = "Recent"

            group_tag = f"session_item_{session_uuid}"

            with dpg.group(parent="session_list_area", tag=group_tag):
                with dpg.child_window(width=-1, height=70, border=True):
                    dpg.add_button(
                        label=title,
                        callback=lambda: self.select_session_callback(session_uuid, title),
                        width=-1,
                        height=30
                    )

                    with dpg.group(horizontal=True):
                        self.safe_add_text(f"Msgs: {message_count}", color=self.BLOOMBERG_GRAY)
                        dpg.add_spacer(width=20)
                        self.safe_add_text(f"{time_str}", color=self.BLOOMBERG_GRAY)
                        dpg.add_spacer(width=20)
                        dpg.add_button(
                            label="DEL",
                            callback=lambda: self.delete_session_callback(session_uuid),
                            width=35,
                            height=20
                        )

                dpg.add_spacer(height=5)

        except Exception as e:
            print(f"Error creating session item: {e}")

    def refresh_sessions_display(self):
        """Refresh the sessions display"""
        try:
            # Clear existing
            self.safe_delete_item("session_list_area", children_only=True)

            # Add sessions
            for session in self.sessions:
                self.create_session_item(session)
        except Exception as e:
            print(f"Error refreshing sessions display: {e}")

    def update_stats(self):
        """Update statistics display"""
        if not self.api_client:
            return

        try:
            result = self.api_client.get_chat_stats()

            if result["success"]:
                stats = result["stats"]
                self.safe_set_value("total_sessions", f"Total Sessions: {stats['total_sessions']}")
                self.safe_set_value("total_messages", f"Total Messages: {stats['total_messages']}")
        except Exception as e:
            print(f"Error updating stats: {e}")

    def scroll_to_bottom(self):
        """Optimized scroll to bottom"""

        def scroll():
            try:
                if dpg.does_item_exist("messages_display_area"):
                    max_scroll = dpg.get_y_scroll_max("messages_display_area")
                    if max_scroll > 0:
                        dpg.set_y_scroll("messages_display_area", max_scroll)
            except:
                pass

        # Quick scroll timing
        threading.Timer(0.1, scroll).start()

    def format_message_content(self, content):
        """Format message content for optimal display"""
        try:
            if not isinstance(content, str):
                content = str(content)

            # Clean up content
            content = content.strip()

            # Remove excessive whitespace while preserving structure
            lines = content.split('\n')
            cleaned_lines = []

            for line in lines:
                cleaned_line = line.strip()
                if cleaned_line or len(cleaned_lines) == 0:  # Keep first line even if empty
                    cleaned_lines.append(cleaned_line)

            # Join and limit excessive newlines
            result = '\n'.join(cleaned_lines)
            while '\n\n\n' in result:
                result = result.replace('\n\n\n', '\n\n')

            return result

        except Exception as e:
            print(f"Error formatting message: {e}")
            return str(content)

    def update_input_status(self, message=""):
        """Update input status with character count"""
        try:
            char_count = len(message)
            if char_count == 0:
                status = "Ready"
            elif char_count < 500:
                status = f"{char_count} chars"
            else:
                status = f"{char_count} chars (long)"

            self.safe_set_value("input_status", status)
        except Exception as e:
            print(f"Error updating input status: {e}")

    # Callback Methods
    def message_input_callback(self, sender, app_data):
        """Enhanced message input callback"""
        try:
            text = self.safe_get_value("message_input_field", "")
            self.update_input_status(text)

            # Handle Enter key for sending
            if app_data == '\n' and text.strip():
                self.send_message_callback()
        except Exception as e:
            print(f"Error in message input callback: {e}")

    def send_message_callback(self):
        """Optimized send message callback"""
        try:
            message = self.safe_get_value("message_input_field", "").strip()

            if not message:
                return

            # Auto-create session if needed
            if not self.current_session_uuid:
                title = self.generate_smart_title(message)
                if not self.create_new_session(title):
                    return

            # Clear input and update status
            self.safe_set_value("message_input_field", "")
            self.update_input_status()

            # Remove welcome screen
            self.safe_delete_item("welcome_screen")

            # Send message
            self.send_message_to_api(message)

        except Exception as e:
            print(f"Error sending message: {e}")

    def clear_input(self):
        """Clear input field"""
        try:
            self.safe_set_value("message_input_field", "")
            self.update_input_status()
        except Exception as e:
            print(f"Error clearing input: {e}")

    def new_chat_callback(self):
        """Create new chat"""
        try:
            current_time = datetime.now().strftime("%H%M")
            title = f"Session-{self.chat_counter:03d}-{current_time}"
            self.chat_counter += 1

            if self.create_new_session(title):
                # Clear messages and show welcome
                self.create_welcome_screen()

                if dpg.does_item_exist("message_input_field"):
                    dpg.focus_item("message_input_field")
        except Exception as e:
            print(f"Error in new chat callback: {e}")

    def select_session_callback(self, session_uuid, title):
        """Select and load session"""
        try:
            self.current_session_uuid = session_uuid

            # Update UI
            self.safe_set_value("current_session_name", title)
            self.safe_set_value("active_session_info", f"Active: {title}")
            self.safe_set_value("system_status", "STATUS: LOADING SESSION...")

            # Load messages
            self.load_session_messages(session_uuid)

            # Activate session via API
            try:
                self.api_client.activate_chat_session(session_uuid)
            except Exception as e:
                print(f"Error activating session: {e}")

            self.safe_set_value("system_status", "STATUS: READY")
        except Exception as e:
            print(f"Error selecting session: {e}")

    def delete_session_callback(self, session_uuid):
        """Delete session"""
        if not self.api_client:
            return

        try:
            result = self.api_client.delete_chat_session(session_uuid)

            if result["success"]:
                # If this was current session, clear it
                if self.current_session_uuid == session_uuid:
                    self.current_session_uuid = None
                    self.safe_set_value("current_session_name", "No Active Session")
                    self.safe_set_value("active_session_info", "Active: None")

                    # Clear messages and show welcome
                    self.create_welcome_screen()

                # Refresh sessions list
                self.load_chat_sessions()
            else:
                error_msg = result.get("error", "Failed to delete session")
                self.safe_set_value("system_status", f"ERROR: {error_msg}")

        except Exception as e:
            print(f"Error deleting session: {e}")
            self.safe_set_value("system_status", f"ERROR: {str(e)}")