# chat_tab.py

import dearpygui.dearpygui as dpg
import time
import threading
from datetime import datetime
import uuid
import re
from fincept_terminal.Utils.base_tab import BaseTab

# PERFORMANCE: Import only essential logging functions
from fincept_terminal.Utils.Logging.logger import info, error, debug, warning


class ChatTab(BaseTab):
    """High Performance Chat Tab - Bloomberg Terminal Style"""

    def __init__(self, app):
        super().__init__(app)
        self.app = app
        self.current_session_uuid = None
        self.is_typing = False
        self.message_widgets = []
        self.chat_counter = 1
        self.sessions = []
        self.ui_tags = set()

        # PERFORMANCE: Lazy API client initialization
        self._api_client = None
        self._api_client_initialized = False
        self._auth_cached = None
        self._auth_cache_time = 0
        self._auth_cache_ttl = 300  # 5 minutes

        # Bloomberg Terminal Colors (cached for performance)
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLACK = [0, 0, 0]

        # PERFORMANCE: Pre-calculate common UI dimensions
        self._ui_dimensions = {
            'bubble_min_width': 120,
            'bubble_max_width': 450,
            'char_width': 7,
            'line_height': 18,
            'padding': 20
        }

        debug("[CHAT_TAB] Chat tab initialized")

    def get_label(self):
        return "AI Chat"

    @property
    def api_client(self):
        """PERFORMANCE: Lazy API client initialization"""
        if not self._api_client_initialized:
            self._initialize_api_client()
        return self._api_client

    def _initialize_api_client(self):
        """PERFORMANCE: Initialize API client only when needed"""
        try:
            from fincept_terminal.Utils.APIClient.api_client import create_api_client
            self._api_client = create_api_client(self.app.get_session_data())
            self._api_client_initialized = True
            if self._api_client:
                debug("[CHAT_TAB] API client initialized successfully")
            else:
                warning("[CHAT_TAB] API client creation failed")
        except Exception as e:
            error(f"[CHAT_TAB] API client initialization failed: {str(e)}")
            self._api_client = None
            self._api_client_initialized = True

    def _is_authenticated_cached(self) -> bool:
        """PERFORMANCE: Cached authentication check"""
        current_time = time.time()

        if (self._auth_cached is not None and
                current_time - self._auth_cache_time < self._auth_cache_ttl):
            return self._auth_cached

        # Check authentication
        is_auth = False
        if self.api_client:
            try:
                is_auth = self.api_client.is_authenticated()
            except Exception:
                is_auth = False

        # Cache result
        self._auth_cached = is_auth
        self._auth_cache_time = current_time
        return is_auth

    def refresh_sessions(self):
        """OPTIMIZED: Fast session refresh"""
        try:
            self.load_chat_sessions()
        except Exception as e:
            error(f"[CHAT_TAB] Session refresh failed: {str(e)}")

    def delete_current_session(self):
        """Delete current session"""
        try:
            if self.current_session_uuid:
                self.delete_session_callback(self.current_session_uuid)
        except Exception as e:
            error(f"[CHAT_TAB] Current session deletion failed: {str(e)}")

    def filter_sessions_callback(self, sender, app_data):
        """OPTIMIZED: Fast session filtering"""
        try:
            search_term = app_data.lower()
            self.safe_delete_item("session_list_area", children_only=True)

            # PERFORMANCE: Filter and create in single pass
            for session in self.sessions:
                if search_term in session["title"].lower():
                    self.create_session_item(session)
        except Exception as e:
            error(f"[CHAT_TAB] Session filtering failed: {str(e)}")

    def clear_current_chat(self):
        """Clear current chat messages"""
        try:
            if self.current_session_uuid:
                self.create_welcome_screen()
                self.safe_set_value("system_status", "STATUS: CHAT CLEARED")
        except Exception as e:
            error(f"[CHAT_TAB] Chat clear failed: {str(e)}")

    # Function key callbacks - OPTIMIZED
    def show_help(self):
        """Show help information"""
        self.send_quick_message("help")

    def focus_sessions(self):
        """Focus session search"""
        try:
            if dpg.does_item_exist("session_search"):
                dpg.focus_item("session_search")
        except Exception as e:
            debug(f"[CHAT_TAB] Focus sessions failed: {str(e)}")

    def focus_search(self):
        """Focus search field"""
        self.focus_sessions()

    def show_stats(self):
        """OPTIMIZED: Show chat statistics"""
        try:
            if self.api_client:
                def get_stats_async():
                    try:
                        result = self.api_client.get_chat_stats()
                        if result["success"]:
                            stats = result["stats"]
                            stats_msg = f"Sessions: {stats['total_sessions']}, Messages: {stats['total_messages']}"
                            self.safe_set_value("system_status", f"STATS: {stats_msg}")
                    except Exception as e:
                        error(f"[CHAT_TAB] Stats retrieval failed: {str(e)}")

                # Run in background to avoid blocking UI
                threading.Thread(target=get_stats_async, daemon=True, name="ChatStats").start()
        except Exception as e:
            error(f"[CHAT_TAB] Stats display failed: {str(e)}")

    def execute_quick_command(self, command):
        """OPTIMIZED: Execute quick command with error handling"""
        try:
            command_map = {
                "NEW_SESSION": self.new_chat_callback,
                "GET_HELP": lambda: self.send_quick_message("help"),
                "MARKET_ANALYSIS": lambda: self.send_quick_message("market analysis"),
                "PORTFOLIO_HELP": lambda: self.send_quick_message("portfolio help"),
                "SYSTEM_STATUS": self.show_stats
            }

            handler = command_map.get(command)
            if handler:
                handler()
            else:
                warning(f"[CHAT_TAB] Unknown command: {command}")
        except Exception as e:
            error(f"[CHAT_TAB] Quick command failed: {command} - {str(e)}")

    def send_quick_message(self, message_type):
        """OPTIMIZED: Send predefined message with minimal overhead"""
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

            # Send message asynchronously
            def send_async():
                self.send_message_to_api(message)

            threading.Thread(target=send_async, daemon=True, name="QuickMessage").start()
        except Exception as e:
            error(f"[CHAT_TAB] Quick message failed: {message_type} - {str(e)}")

    # Helper Methods - OPTIMIZED
    def generate_smart_title(self, message):
        """PERFORMANCE: Fast title generation"""
        try:
            clean_msg = re.sub(r'[^\w\s]', '', message).strip()
            words = clean_msg.split()

            if len(words) <= 2:
                return clean_msg[:20] if clean_msg else "New Chat"
            else:
                return ' '.join(words[:2]) + "..."
        except Exception:
            return "Chat Session"

    def cleanup(self):
        """OPTIMIZED: Fast cleanup"""
        try:
            self.sessions.clear()
            self.current_session_uuid = None
            self.ui_tags.clear()
            # Clear caches
            self._auth_cached = None
            self._auth_cache_time = 0
            debug("[CHAT_TAB] Cleanup completed")
        except Exception as e:
            error(f"[CHAT_TAB] Cleanup failed: {str(e)}")

    # Safe UI operations - OPTIMIZED
    def safe_add_text(self, text, color=None, tag=None, parent=None):
        """PERFORMANCE: Optimized text addition"""
        try:
            kwargs = {"color": color} if color else {}
            if tag:
                kwargs["tag"] = tag
                self.ui_tags.add(tag)
            if parent:
                kwargs["parent"] = parent

            return dpg.add_text(text, **kwargs)
        except Exception as e:
            debug(f"[CHAT_TAB] Text addition failed: {str(e)}")
            return None

    def safe_delete_item(self, tag, children_only=False):
        """PERFORMANCE: Safe item deletion with cache cleanup"""
        try:
            if dpg.does_item_exist(tag):
                dpg.delete_item(tag, children_only=children_only)
                if tag in self.ui_tags:
                    self.ui_tags.remove(tag)
                return True
        except Exception as e:
            debug(f"[CHAT_TAB] Item deletion failed: {tag} - {str(e)}")
        return False

    def safe_set_value(self, tag, value):
        """PERFORMANCE: Safe value setting"""
        try:
            if dpg.does_item_exist(tag):
                dpg.set_value(tag, value)
                return True
        except Exception:
            return False

    def safe_get_value(self, tag, default=""):
        """PERFORMANCE: Safe value getting"""
        try:
            if dpg.does_item_exist(tag):
                return dpg.get_value(tag)
        except Exception:
            return default

    def create_content(self):
        """OPTIMIZED: Create chat interface with minimal API calls"""
        try:
            # Quick authentication check
            if not self._is_authenticated_cached():
                self.create_error_content("Authentication required")
                return

            self.create_chat_interface()

            # Load sessions asynchronously to avoid blocking UI
            def load_sessions_async():
                try:
                    self.load_chat_sessions()
                except Exception as e:
                    error(f"[CHAT_TAB] Async session loading failed: {str(e)}")

            threading.Thread(target=load_sessions_async, daemon=True, name="ChatSessionLoader").start()

        except Exception as e:
            error(f"[CHAT_TAB] Content creation failed: {str(e)}")
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
        debug("[CHAT_TAB] Content refresh requested")

    def create_chat_interface(self):
        """OPTIMIZED: Create main chat interface with minimal overhead"""
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
            error(f"[CHAT_TAB] Interface creation failed: {str(e)}")

    def create_terminal_header(self):
        """OPTIMIZED: Create terminal header with cached user info"""
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
            error(f"[CHAT_TAB] Header creation failed: {str(e)}")

    def create_function_keys(self):
        """OPTIMIZED: Create function keys bar"""
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
            error(f"[CHAT_TAB] Function keys creation failed: {str(e)}")

    def create_chat_sessions_panel(self):
        """OPTIMIZED: Create left panel for chat sessions"""
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
            error(f"[CHAT_TAB] Sessions panel creation failed: {str(e)}")

    def create_chat_interface_panel(self):
        """OPTIMIZED: Create chat interface panel"""
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
                    height=470,
                    border=True,
                    tag="messages_display_area"
                )

                dpg.add_separator()

                # Compact input area
                self.safe_add_text("INPUT", color=self.BLOOMBERG_YELLOW)
                with dpg.group(horizontal=True):
                    dpg.add_input_text(
                        hint="Type message...",
                        width=600,
                        height=40,
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
            error(f"[CHAT_TAB] Interface panel creation failed: {str(e)}")

    def create_command_panel(self):
        """OPTIMIZED: Create right panel for commands"""
        try:
            with dpg.child_window(width=300, height=600, border=True, tag="command_panel"):
                self.safe_add_text("COMMAND CENTER", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                # API Status (cached check)
                self.safe_add_text("API STATUS", color=self.BLOOMBERG_YELLOW)
                if self._is_authenticated_cached():
                    self.safe_add_text("● Connected", color=self.BLOOMBERG_GREEN)
                    if self.api_client:
                        user_type = getattr(self.api_client, 'user_type', 'Unknown')
                        self.safe_add_text(f"● {user_type.title()} User", color=self.BLOOMBERG_WHITE)
                        try:
                            req_count = self.api_client.get_request_count()
                            self.safe_add_text(f"● Requests: {req_count}", color=self.BLOOMBERG_WHITE)
                        except:
                            self.safe_add_text("● Requests: N/A", color=self.BLOOMBERG_WHITE)
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
            error(f"[CHAT_TAB] Command panel creation failed: {str(e)}")

    def create_status_bar(self):
        """OPTIMIZED: Create bottom status bar"""
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
            error(f"[CHAT_TAB] Status bar creation failed: {str(e)}")

    def create_welcome_screen(self):
        """OPTIMIZED: Create compact welcome screen"""
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
            error(f"[CHAT_TAB] Welcome screen creation failed: {str(e)}")

    # API Integration Methods - OPTIMIZED
    def load_chat_sessions(self):
        """OPTIMIZED: Load chat sessions from API with caching"""
        if not self.api_client:
            return

        try:
            result = self.api_client.get_chat_sessions()

            if result["success"]:
                self.sessions = result["sessions"]
                self.refresh_sessions_display()
                self.update_stats()
                debug(f"[CHAT_TAB] Loaded {len(self.sessions)} sessions")
            else:
                warning(f"[CHAT_TAB] Session loading failed: {result.get('error', 'Unknown error')}")

        except Exception as e:
            error(f"[CHAT_TAB] Session loading error: {str(e)}")

    def create_new_session(self, title="New Conversation"):
        """OPTIMIZED: Create new chat session via API"""
        if not self.api_client:
            return False

        try:
            result = self.api_client.create_chat_session(title)

            if result["success"]:
                session_data = result["session"]
                self.current_session_uuid = session_data["session_uuid"]

                # Update UI efficiently
                self.safe_set_value("current_session_name", session_data["title"])
                self.safe_set_value("active_session_info", f"Active: {session_data['title']}")

                # Clear welcome screen
                self.safe_delete_item("welcome_screen")

                # Refresh sessions in background
                def refresh_async():
                    self.load_chat_sessions()

                threading.Thread(target=refresh_async, daemon=True, name="SessionRefresh").start()

                info(f"[CHAT_TAB] New session created: {session_data['title']}")
                return True
            else:
                error_msg = result.get("error", "Failed to create session")
                error(f"[CHAT_TAB] Session creation failed: {error_msg}")
                self.safe_set_value("system_status", f"ERROR: {error_msg}")
                return False

        except Exception as e:
            error(f"[CHAT_TAB] Session creation error: {str(e)}")
            self.safe_set_value("system_status", f"ERROR: {str(e)}")
            return False

    def send_message_to_api(self, content):
        """OPTIMIZED: Send message to API with background processing"""
        if not self.api_client or not self.current_session_uuid:
            return False

        try:
            self.safe_set_value("system_status", "STATUS: SENDING MESSAGE...")

            result = self.api_client.send_chat_message(self.current_session_uuid, content)

            if result["success"]:
                user_msg = result["user_message"]
                ai_msg = result["ai_message"]

                # Add messages to UI efficiently
                self.create_message_bubble("user", user_msg["content"])
                self.create_message_bubble("assistant", ai_msg["content"])

                # Update session title if changed
                if result.get("new_title"):
                    self.safe_set_value("current_session_name", result["new_title"])
                    self.safe_set_value("active_session_info", f"Active: {result['new_title']}")

                # Refresh sessions in background
                def refresh_async():
                    try:
                        self.load_chat_sessions()
                    except Exception as e:
                        debug(f"[CHAT_TAB] Background refresh failed: {str(e)}")

                threading.Thread(target=refresh_async, daemon=True, name="MessageRefresh").start()

                self.safe_set_value("system_status", "STATUS: READY")
                return True
            else:
                error_msg = result.get("error", "Failed to send message")
                error(f"[CHAT_TAB] Message send failed: {error_msg}")
                self.safe_set_value("system_status", f"ERROR: {error_msg}")
                return False

        except Exception as e:
            error(f"[CHAT_TAB] Message send error: {str(e)}")
            self.safe_set_value("system_status", f"ERROR: {str(e)}")
            return False

    def load_session_messages(self, session_uuid):
        """OPTIMIZED: Load messages for a specific session"""
        if not self.api_client:
            return

        try:
            result = self.api_client.get_chat_session(session_uuid)

            if result["success"]:
                messages = result["messages"]

                # Clear existing messages
                self.safe_delete_item("messages_display_area", children_only=True)

                # Add messages efficiently
                for msg in messages:
                    self.create_message_bubble(msg["role"], msg["content"])

                self.scroll_to_bottom()
                debug(f"[CHAT_TAB] Loaded {len(messages)} messages for session")
            else:
                warning(f"[CHAT_TAB] Message loading failed: {result.get('error', 'Unknown error')}")

        except Exception as e:
            error(f"[CHAT_TAB] Message loading error: {str(e)}")

    # UI Helper Methods - OPTIMIZED
    def create_message_bubble(self, role, content):
        """OPTIMIZED: Create message bubbles with pre-calculated dimensions"""
        try:
            time_str = datetime.now().strftime("%H:%M:%S")
            is_user = role == "user"

            # PERFORMANCE: Use cached calculations
            bubble_width, estimated_lines = self._calculate_bubble_size_cached(content)
            wrapped_content = self._wrap_text_cached(content, bubble_width)

            with dpg.group(parent="messages_display_area"):
                # Minimal spacing between messages
                dpg.add_spacer(height=4)

                if is_user:
                    # USER MESSAGE - Right aligned
                    with dpg.group(horizontal=True):
                        chat_area_width = 845
                        left_spacer = max(20, chat_area_width - bubble_width - 50)
                        dpg.add_spacer(width=left_spacer)

                        with dpg.group():
                            # Compact header
                            with dpg.group(horizontal=True):
                                self.safe_add_text("YOU", color=self.BLOOMBERG_YELLOW)
                                dpg.add_spacer(width=8)
                                self.safe_add_text(f"{time_str}", color=self.BLOOMBERG_GRAY)

                            # Tight message bubble
                            bubble_height = max(30, estimated_lines * self._ui_dimensions['line_height'] + 16)
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
                            bubble_height = max(30, estimated_lines * self._ui_dimensions['line_height'] + 16)
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

            # PERFORMANCE: Delayed scroll to avoid blocking UI
            def delayed_scroll():
                time.sleep(0.05)  # Reduced delay
                self.scroll_to_bottom()

            threading.Thread(target=delayed_scroll, daemon=True).start()

        except Exception as e:
            error(f"[CHAT_TAB] Message bubble creation failed: {str(e)}")

    def _calculate_bubble_size_cached(self, content):
        """PERFORMANCE: Optimized bubble size calculation with caching"""
        try:
            clean_content = content.strip()
            content_length = len(clean_content)

            # Use cached dimensions
            MIN_WIDTH = self._ui_dimensions['bubble_min_width']
            MAX_WIDTH = self._ui_dimensions['bubble_max_width']
            CHAR_WIDTH = self._ui_dimensions['char_width']

            # Fast width calculation
            if content_length <= 20:
                width = min(MAX_WIDTH, max(MIN_WIDTH, content_length * CHAR_WIDTH + 20))
            elif content_length <= 80:
                width = min(MAX_WIDTH, max(MIN_WIDTH, int(content_length * CHAR_WIDTH * 0.8) + 30))
            elif content_length <= 200:
                width = min(MAX_WIDTH, max(250, int(content_length * CHAR_WIDTH * 0.6) + 40))
            else:
                width = MAX_WIDTH

            # Fast line estimation
            chars_per_line = max(20, int((width - self._ui_dimensions['padding']) / CHAR_WIDTH))
            estimated_lines = max(1, min(15, content_length // chars_per_line + clean_content.count('\n') + 1))

            return int(width), estimated_lines

        except Exception:
            return 200, 1

    def _wrap_text_cached(self, text, bubble_width):
        """PERFORMANCE: Fast text wrapping with minimal processing"""
        try:
            if not text:
                return ""

            chars_per_line = max(20, int((bubble_width - self._ui_dimensions['padding']) / self._ui_dimensions[
                'char_width']))

            if len(text) <= chars_per_line:
                return text

            # Simple line wrapping
            lines = []
            current_line = ""

            for word in text.split():
                if len(current_line + word) <= chars_per_line:
                    current_line += word + " "
                else:
                    if current_line:
                        lines.append(current_line.strip())
                    current_line = word + " "

            if current_line:
                lines.append(current_line.strip())

            return '\n'.join(lines)

        except Exception:
            return text

    def create_session_item(self, session_data):
        """OPTIMIZED: Create session item in the list"""
        try:
            session_uuid = session_data["session_uuid"]
            title = session_data["title"]
            message_count = session_data["message_count"]

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
            error(f"[CHAT_TAB] Session item creation failed: {str(e)}")

    def refresh_sessions_display(self):
        """OPTIMIZED: Fast session display refresh"""
        try:
            self.safe_delete_item("session_list_area", children_only=True)

            # Batch create session items
            for session in self.sessions:
                self.create_session_item(session)

        except Exception as e:
            error(f"[CHAT_TAB] Session display refresh failed: {str(e)}")

    def update_stats(self):
        """OPTIMIZED: Update statistics display asynchronously"""
        if not self.api_client:
            return

        def update_stats_async():
            try:
                result = self.api_client.get_chat_stats()
                if result["success"]:
                    stats = result["stats"]
                    self.safe_set_value("total_sessions", f"Total Sessions: {stats['total_sessions']}")
                    self.safe_set_value("total_messages", f"Total Messages: {stats['total_messages']}")
            except Exception as e:
                debug(f"[CHAT_TAB] Stats update failed: {str(e)}")

        threading.Thread(target=update_stats_async, daemon=True, name="StatsUpdate").start()

    def scroll_to_bottom(self):
        """OPTIMIZED: Fast scroll to bottom"""

        def scroll():
            try:
                if dpg.does_item_exist("messages_display_area"):
                    max_scroll = dpg.get_y_scroll_max("messages_display_area")
                    if max_scroll > 0:
                        dpg.set_y_scroll("messages_display_area", max_scroll)
            except:
                pass

        # Reduced timing for better performance
        threading.Timer(0.05, scroll).start()

    # Callback Methods - OPTIMIZED
    def message_input_callback(self, sender, app_data):
        """OPTIMIZED: Message input callback"""
        try:
            text = self.safe_get_value("message_input_field", "")

            # Simple character count without formatting
            char_count = len(text)
            if char_count == 0:
                status = "Ready"
            elif char_count < 500:
                status = f"{char_count} chars"
            else:
                status = f"{char_count} chars (long)"

            self.safe_set_value("input_status", status)

            # Handle Enter key for sending
            if app_data == '\n' and text.strip():
                self.send_message_callback()
        except Exception as e:
            debug(f"[CHAT_TAB] Input callback failed: {str(e)}")

    def send_message_callback(self):
        """OPTIMIZED: Fast send message callback"""
        try:
            message = self.safe_get_value("message_input_field", "").strip()

            if not message:
                return

            # Auto-create session if needed
            if not self.current_session_uuid:
                title = self.generate_smart_title(message)
                if not self.create_new_session(title):
                    return

            # Clear input immediately
            self.safe_set_value("message_input_field", "")
            self.safe_set_value("input_status", "Ready")

            # Remove welcome screen
            self.safe_delete_item("welcome_screen")

            # Send message in background
            def send_async():
                self.send_message_to_api(message)

            threading.Thread(target=send_async, daemon=True, name="MessageSend").start()

        except Exception as e:
            error(f"[CHAT_TAB] Send message failed: {str(e)}")

    def clear_input(self):
        """Clear input field"""
        self.safe_set_value("message_input_field", "")
        self.safe_set_value("input_status", "Ready")

    def new_chat_callback(self):
        """OPTIMIZED: Create new chat"""
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
            error(f"[CHAT_TAB] New chat creation failed: {str(e)}")

    def select_session_callback(self, session_uuid, title):
        """OPTIMIZED: Select and load session asynchronously"""
        try:
            self.current_session_uuid = session_uuid

            # Update UI immediately
            self.safe_set_value("current_session_name", title)
            self.safe_set_value("active_session_info", f"Active: {title}")
            self.safe_set_value("system_status", "STATUS: LOADING SESSION...")

            # Load messages in background
            def load_session_async():
                try:
                    self.load_session_messages(session_uuid)

                    # Activate session via API
                    if self.api_client:
                        self.api_client.activate_chat_session(session_uuid)

                    self.safe_set_value("system_status", "STATUS: READY")
                except Exception as e:
                    error(f"[CHAT_TAB] Session loading failed: {str(e)}")
                    self.safe_set_value("system_status", f"ERROR: {str(e)}")

            threading.Thread(target=load_session_async, daemon=True, name="SessionLoad").start()

        except Exception as e:
            error(f"[CHAT_TAB] Session selection failed: {str(e)}")

    def delete_session_callback(self, session_uuid):
        """OPTIMIZED: Delete session with background processing"""
        if not self.api_client:
            return

        try:
            def delete_session_async():
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
                        info(f"[CHAT_TAB] Session deleted: {session_uuid[:8]}")
                    else:
                        error_msg = result.get("error", "Failed to delete session")
                        self.safe_set_value("system_status", f"ERROR: {error_msg}")
                        error(f"[CHAT_TAB] Session deletion failed: {error_msg}")

                except Exception as e:
                    error(f"[CHAT_TAB] Session deletion error: {str(e)}")
                    self.safe_set_value("system_status", f"ERROR: {str(e)}")

            threading.Thread(target=delete_session_async, daemon=True, name="SessionDelete").start()

        except Exception as e:
            error(f"[CHAT_TAB] Delete session callback failed: {str(e)}")


# PERFORMANCE: Alias the optimized class to maintain compatibility
ChatTab = ChatTab