# chat_ui.py - Bloomberg Terminal Chat Interface with Letter-by-Letter Animation
# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
import sqlite3
import json
import time
import threading
from datetime import datetime
import uuid
import re
from fincept_terminal.Utils.Managers.theme_manager import AutomaticThemeManager


class ChatDatabase:
    """SQLite database manager for chat history"""

    def __init__(self, db_path="chat_history.db"):
        self.db_path = db_path
        self.init_database()

    def init_database(self):
        """Initialize the chat database with proper schema"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        # Create chats table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS chats (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                is_active INTEGER DEFAULT 0,
                message_count INTEGER DEFAULT 0
            )
        ''')

        # Create messages table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                chat_id INTEGER,
                role TEXT NOT NULL,
                content TEXT NOT NULL,
                timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (chat_id) REFERENCES chats (id) ON DELETE CASCADE
            )
        ''')

        conn.commit()
        conn.close()

    def create_new_chat(self, title="New Conversation"):
        """Create a new chat session"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        # Deactivate all other chats
        cursor.execute("UPDATE chats SET is_active = 0")

        # Create new chat
        cursor.execute(
            "INSERT INTO chats (title, is_active, message_count) VALUES (?, 1, 0)",
            (title,)
        )
        chat_id = cursor.lastrowid

        conn.commit()
        conn.close()
        return chat_id

    def get_all_chats(self):
        """Get all chat sessions ordered by updated_at"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute('''
            SELECT id, title, created_at, updated_at, is_active, message_count 
            FROM chats 
            ORDER BY updated_at DESC
        ''')

        chats = cursor.fetchall()
        conn.close()
        return chats

    def get_chat_messages(self, chat_id):
        """Get all messages for a specific chat"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute('''
            SELECT role, content, timestamp 
            FROM messages 
            WHERE chat_id = ? 
            ORDER BY timestamp ASC
        ''', (chat_id,))

        messages = cursor.fetchall()
        conn.close()
        return messages

    def add_message(self, chat_id, role, content):
        """Add a new message to a chat"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute(
            "INSERT INTO messages (chat_id, role, content) VALUES (?, ?, ?)",
            (chat_id, role, content)
        )

        # Update chat's updated_at timestamp and message count
        cursor.execute(
            "UPDATE chats SET updated_at = CURRENT_TIMESTAMP, message_count = message_count + 1 WHERE id = ?",
            (chat_id,)
        )

        conn.commit()
        conn.close()

    def delete_chat(self, chat_id):
        """Delete a chat and all its messages"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute("DELETE FROM messages WHERE chat_id = ?", (chat_id,))
        cursor.execute("DELETE FROM chats WHERE id = ?", (chat_id,))

        conn.commit()
        conn.close()

    def set_active_chat(self, chat_id):
        """Set a chat as active"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute("UPDATE chats SET is_active = 0")
        cursor.execute("UPDATE chats SET is_active = 1 WHERE id = ?", (chat_id,))

        conn.commit()
        conn.close()

    def get_active_chat(self):
        """Get the currently active chat"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute("SELECT id, title FROM chats WHERE is_active = 1 LIMIT 1")
        result = cursor.fetchone()

        conn.close()
        return result

    def update_chat_title(self, chat_id, new_title):
        """Update chat title"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute(
            "UPDATE chats SET title = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
            (new_title, chat_id)
        )

        conn.commit()
        conn.close()

    def get_chat_stats(self):
        """Get database statistics"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute("SELECT COUNT(*) FROM chats")
        total_chats = cursor.fetchone()[0]

        cursor.execute("SELECT COUNT(*) FROM messages")
        total_messages = cursor.fetchone()[0]

        conn.close()
        return {"total_chats": total_chats, "total_messages": total_messages}


class BloombergTerminalChatUI:
    """Bloomberg Terminal Chat Interface - Enhanced with Dashboard Styling"""

    def __init__(self):
        self.db = ChatDatabase()
        self.theme_manager = AutomaticThemeManager()
        self.current_chat_id = None
        self.is_typing = False
        self.message_widgets = []
        self.chat_counter = 1

        # Bloomberg Terminal Colors (matching dashboard)
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLACK = [0, 0, 0]

        # Initialize DearPyGUI
        dpg.create_context()
        self.create_bloomberg_terminal_ui()
        self.load_initial_data()

    def generate_smart_title(self, first_message):
        """Generate smart chat title from first message"""
        clean_msg = re.sub(r'[^\w\s]', '', first_message).strip()
        words = clean_msg.split()

        if len(words) <= 3:
            return clean_msg[:25] if len(clean_msg) <= 25 else clean_msg[:22] + "..."
        else:
            return ' '.join(words[:3]) + "..."

    def create_bloomberg_terminal_ui(self):
        """Create Bloomberg Terminal style UI matching dashboard design"""

        with dpg.window(tag="main_window", label="FINCEPT TERMINAL - AI ASSISTANT"):
            # Top branding bar (matching dashboard style)
            self.create_terminal_header()

            # Function keys bar (matching dashboard)
            self.create_function_keys()

            # Main terminal layout
            with dpg.group(horizontal=True):
                # Left panel - Chat Sessions (Bloomberg style)
                self.create_chat_sessions_panel()

                # Center panel - Chat Interface
                self.create_chat_interface_panel()

                # Right panel - Command Line (Bloomberg style)
                self.create_command_panel()

            # Bottom status bar (matching dashboard)
            self.create_status_bar()

        dpg.set_primary_window("main_window", True)

    def create_terminal_header(self):
        """Create Bloomberg Terminal header matching dashboard"""
        with dpg.group(horizontal=True):
            dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("AI ASSISTANT", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_input_text(label="", default_value="Enter AI Command", width=300, tag="command_input")
            dpg.add_button(label="Execute", width=80, callback=self.execute_command)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.now().strftime('%Y-%m-%d %H:%M:%S'), tag="header_time")

        dpg.add_separator()

    def create_function_keys(self):
        """Create function keys bar matching dashboard"""
        with dpg.group(horizontal=True):
            function_keys = [
                ("F1:HELP", self.show_help),
                ("F2:CHATS", self.focus_chats),
                ("F3:NEW", self.new_chat_callback),
                ("F4:SEARCH", self.focus_search),
                ("F5:CLEAR", self.clear_current_chat),
                ("F6:SETTINGS", self.show_settings)
            ]
            for key_label, callback in function_keys:
                dpg.add_button(label=key_label, width=100, height=25, callback=callback)

        dpg.add_separator()

    def create_chat_sessions_panel(self):
        """Create left panel for chat sessions (Bloomberg style)"""
        with dpg.child_window(width=350, height=650, border=True, tag="chat_sessions_panel"):
            # Header
            dpg.add_text("CHAT SESSIONS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Controls
            with dpg.group(horizontal=True):
                dpg.add_button(label="NEW", callback=self.new_chat_callback, width=60, height=25)
                dpg.add_button(label="REFRESH", callback=self.refresh_chat_list, width=80, height=25)
                dpg.add_button(label="DELETE", callback=self.delete_current_chat, width=70, height=25)

            # Search
            dpg.add_input_text(hint="Search chats...", width=-1, tag="chat_search",
                               callback=self.filter_chats_callback)

            dpg.add_separator()

            # Stats
            dpg.add_text("SESSION STATISTICS", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("Total Sessions: 0", tag="total_sessions")
            dpg.add_text("Total Messages: 0", tag="total_messages")
            dpg.add_text("Active Session: None", tag="active_session_info")

            dpg.add_separator()

            # Chat list
            dpg.add_text("ACTIVE SESSIONS", color=self.BLOOMBERG_YELLOW)
            dpg.add_child_window(height=-1, border=False, tag="chat_list_area")

    def create_chat_interface_panel(self):
        """Create center panel for chat interface (Bloomberg style) - Responsive"""
        with dpg.child_window(width=750, height=650, border=True, tag="chat_interface_panel"):
            # Chat header
            with dpg.group(horizontal=True):
                dpg.add_text("AI CHAT TERMINAL", color=self.BLOOMBERG_ORANGE, tag="chat_header_title")
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("No Active Session", color=self.BLOOMBERG_WHITE, tag="chat_session_name")

            dpg.add_separator()

            # Messages area - optimized height
            with dpg.child_window(height=480, border=True, tag="messages_display_area"):
                self.create_welcome_screen()

            dpg.add_separator()

            # Input area - more compact
            dpg.add_text("MESSAGE INPUT", color=self.BLOOMBERG_YELLOW)
            with dpg.group(horizontal=True):
                dpg.add_input_text(
                    hint="Type your message and press Enter...",
                    width=580,
                    height=50,
                    multiline=True,
                    tag="message_input_field",
                    callback=self.message_input_callback,
                    on_enter=True
                )
                with dpg.group():
                    dpg.add_button(
                        label="SEND",
                        callback=self.send_message_callback,
                        width=100,
                        height=25,
                        tag="send_button"
                    )
                    dpg.add_button(
                        label="CLEAR",
                        callback=self.clear_input,
                        width=100,
                        height=20
                    )

            # Input status - more compact
            dpg.add_text("Characters: 0 | Ready to send", tag="input_status", color=self.BLOOMBERG_GRAY)

    def create_command_panel(self):
        """Create right panel for commands (Bloomberg style)"""
        with dpg.child_window(width=300, height=650, border=True, tag="command_panel"):
            dpg.add_text("COMMAND CENTER", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Command history
            dpg.add_text("COMMAND HISTORY", color=self.BLOOMBERG_YELLOW)
            with dpg.child_window(height=200, border=True, tag="command_history"):
                dpg.add_text("> NEW_CHAT", color=self.BLOOMBERG_WHITE)
                dpg.add_text("  Session created successfully", color=self.BLOOMBERG_GREEN)
                dpg.add_text("> AI_QUERY: Market Analysis", color=self.BLOOMBERG_WHITE)
                dpg.add_text("  Processing query...", color=self.BLOOMBERG_GRAY)

            dpg.add_separator()

            # Quick commands
            dpg.add_text("QUICK COMMANDS", color=self.BLOOMBERG_YELLOW)
            commands = [
                ("NEW_CHAT", "Create new chat session"),
                ("CLEAR_HISTORY", "Clear chat history"),
                ("EXPORT_CHAT", "Export current chat"),
                ("AI_SUMMARY", "Get conversation summary"),
                ("MARKET_HELP", "Financial assistance"),
                ("SYSTEM_STATUS", "Check system status")
            ]

            for cmd, desc in commands:
                dpg.add_button(label=cmd, width=-1, height=25,
                               callback=lambda s, a, command=cmd: self.execute_quick_command(command))
                dpg.add_text(f"  {desc}", color=self.BLOOMBERG_GRAY)

            dpg.add_separator()

            # System info
            dpg.add_text("SYSTEM STATUS", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("AI Engine: ONLINE", color=self.BLOOMBERG_GREEN)
            dpg.add_text("Database: CONNECTED", color=self.BLOOMBERG_GREEN)
            dpg.add_text("Response Time: <50ms", color=self.BLOOMBERG_GREEN)
            dpg.add_text("Last Update:", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.now().strftime('%H:%M:%S'), tag="last_update_time")

    def create_welcome_screen(self):
        """Create welcome screen matching Bloomberg Terminal style"""
        with dpg.group(tag="welcome_screen"):
            dpg.add_spacer(height=50)
            dpg.add_text("FINCEPT AI ASSISTANT TERMINAL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Advanced Financial Intelligence System", color=self.BLOOMBERG_WHITE)

            dpg.add_spacer(height=30)

            dpg.add_text("SYSTEM STATUS:", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("● AI Engine: READY", color=self.BLOOMBERG_GREEN)
            dpg.add_text("● Database: CONNECTED", color=self.BLOOMBERG_GREEN)
            dpg.add_text("● Real-time Data: ACTIVE", color=self.BLOOMBERG_GREEN)

            dpg.add_spacer(height=30)

            dpg.add_text("GETTING STARTED:", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("• Type a message below to start chatting")
            dpg.add_text("• Press Enter to send messages quickly")
            dpg.add_text("• Use function keys for quick commands")
            dpg.add_text("• Browse chat history in the left panel")

            dpg.add_spacer(height=30)

            with dpg.group(horizontal=True):
                dpg.add_button(label="START NEW CHAT", callback=self.new_chat_callback,
                               width=150, height=40)
                dpg.add_spacer(width=20)
                dpg.add_button(label="VIEW HELP", callback=self.show_help,
                               width=150, height=40)

    def create_status_bar(self):
        """Create bottom status bar matching dashboard style"""
        dpg.add_separator()
        with dpg.group(horizontal=True):
            dpg.add_text("●", color=self.BLOOMBERG_GREEN)
            dpg.add_text("CONNECTED", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("AI ASSISTANT", color=self.BLOOMBERG_ORANGE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("STATUS: READY", color=self.BLOOMBERG_WHITE, tag="system_status")
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("SERVER: AI-01", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("USER: TRADER001", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text(f"TIME: {datetime.now().strftime('%H:%M:%S')}",
                         color=self.BLOOMBERG_WHITE, tag="status_time")
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LATENCY: <20ms", color=self.BLOOMBERG_GREEN)

    def create_chat_item(self, chat_id, title, message_count, updated_at, is_active=False):
        """Create chat item with Bloomberg Terminal styling"""
        try:
            dt = datetime.fromisoformat(updated_at.replace('Z', '+00:00'))
            time_str = dt.strftime("%m/%d %H:%M")
        except:
            time_str = "Recent"

        group_tag = f"chat_item_{chat_id}"

        with dpg.group(parent="chat_list_area", tag=group_tag):
            # Chat container
            with dpg.child_window(width=-1, height=70, border=True):
                # Main button
                color = self.BLOOMBERG_ORANGE if is_active else self.BLOOMBERG_WHITE
                dpg.add_button(
                    label=f"{title}",
                    callback=lambda: self.select_chat_callback(chat_id),
                    width=-1,
                    height=30
                )

                # Info row
                with dpg.group(horizontal=True):
                    dpg.add_text(f"Msgs: {message_count}", color=self.BLOOMBERG_GRAY)
                    dpg.add_spacer(width=20)
                    dpg.add_text(f"{time_str}", color=self.BLOOMBERG_GRAY)
                    dpg.add_spacer(width=20)
                    dpg.add_button(
                        label="DEL",
                        callback=lambda: self.delete_chat_callback(chat_id),
                        width=35,
                        height=20
                    )

            dpg.add_spacer(height=5)

    def create_message_bubble(self, role, content, timestamp=None):
        """Create message bubble with Bloomberg Terminal styling - Responsive layout"""
        time_str = datetime.now().strftime("%H:%M:%S")
        if timestamp:
            try:
                dt = datetime.fromisoformat(timestamp.replace('Z', '+00:00'))
                time_str = dt.strftime("%H:%M:%S")
            except:
                pass

        is_user = role == "user"

        with dpg.group(parent="messages_display_area"):
            dpg.add_spacer(height=8)

            if is_user:
                # User message (right aligned, compact)
                with dpg.group(horizontal=True):
                    dpg.add_spacer(width=200)  # Increased left margin for right alignment

                    with dpg.group():
                        # Header - more compact
                        with dpg.group(horizontal=True):
                            dpg.add_text("USER", color=self.BLOOMBERG_YELLOW)
                            dpg.add_spacer(width=15)
                            dpg.add_text(f"[{time_str}]", color=self.BLOOMBERG_GRAY)

                        # Message - smaller, responsive width
                        with dpg.child_window(width=350, height=0, border=True, autosize_y=True):
                            dpg.add_text(content, wrap=330, color=self.BLOOMBERG_WHITE)
            else:
                # AI message (left aligned, larger but responsive)
                with dpg.group(horizontal=True):
                    with dpg.group():
                        # Header
                        with dpg.group(horizontal=True):
                            dpg.add_text("AI ASSISTANT", color=self.BLOOMBERG_ORANGE)
                            dpg.add_spacer(width=15)
                            dpg.add_text(f"[{time_str}]", color=self.BLOOMBERG_GRAY)

                        # Message - responsive width based on content
                        message_width = min(450, max(300, len(content) * 8))  # Dynamic width
                        with dpg.child_window(width=message_width, height=0, border=True, autosize_y=True):
                            dpg.add_text(content, wrap=message_width - 20, color=self.BLOOMBERG_WHITE)

                    dpg.add_spacer(width=100)  # Right margin for AI messages

    def create_typing_indicator(self):
        """Create typing indicator - responsive"""
        with dpg.group(parent="messages_display_area", tag="typing_indicator"):
            dpg.add_spacer(height=8)

            with dpg.group(horizontal=True):
                with dpg.group():
                    with dpg.group(horizontal=True):
                        dpg.add_text("AI ASSISTANT", color=self.BLOOMBERG_ORANGE)
                        dpg.add_spacer(width=15)
                        dpg.add_text("[PROCESSING...]", color=self.BLOOMBERG_YELLOW)

                    with dpg.child_window(width=300, height=40, border=True):
                        dpg.add_text("", tag="typing_text", color=self.BLOOMBERG_WHITE)

                dpg.add_spacer(width=200)

    def animate_text_letter_by_letter(self, full_text, text_tag, speed=0.03):
        """Animate text letter by letter with good speed"""

        def animate():
            if not dpg.does_item_exist(text_tag):
                return

            current_text = ""
            for char in full_text:
                if not dpg.does_item_exist(text_tag):
                    break

                current_text += char
                try:
                    dpg.set_value(text_tag, current_text)
                    time.sleep(speed)
                except:
                    break

        threading.Thread(target=animate, daemon=True).start()

    def simulate_ai_response(self, user_message):
        """Simulate AI response with letter-by-letter animation"""
        if self.is_typing:
            return

        def response_process():
            self.is_typing = True

            # Show typing indicator
            self.create_typing_indicator()
            self.scroll_to_bottom()

            # Typing animation for indicator
            typing_messages = [
                "Analyzing your query...",
                "Processing request...",
                "Generating response...",
                "Finalizing output..."
            ]

            for msg in typing_messages:
                if dpg.does_item_exist("typing_text"):
                    self.animate_text_letter_by_letter(msg, "typing_text", 0.05)
                    time.sleep(len(msg) * 0.05 + 0.5)

            # Remove typing indicator
            if dpg.does_item_exist("typing_indicator"):
                dpg.delete_item("typing_indicator")

            # Generate response
            response = self.get_bloomberg_ai_response(user_message)

            # Create message bubble and animate the response
            time_str = datetime.now().strftime("%H:%M:%S")

            with dpg.group(parent="messages_display_area"):
                dpg.add_spacer(height=8)

                with dpg.group(horizontal=True):
                    with dpg.group():
                        # Header
                        with dpg.group(horizontal=True):
                            dpg.add_text("AI ASSISTANT", color=self.BLOOMBERG_ORANGE)
                            dpg.add_spacer(width=15)
                            dpg.add_text(f"[{time_str}]", color=self.BLOOMBERG_GRAY)

                        # Message container - responsive width
                        response_width = min(450, max(300, len(response) * 6))
                        with dpg.child_window(width=response_width, height=0, border=True, autosize_y=True):
                            response_text_tag = f"response_text_{int(time.time())}"
                            dpg.add_text("", wrap=response_width - 20, color=self.BLOOMBERG_WHITE,
                                         tag=response_text_tag)

                    dpg.add_spacer(width=100)

            # Animate the response letter by letter
            self.animate_text_letter_by_letter(response, response_text_tag, 0.02)

            # Save to database
            self.db.add_message(self.current_chat_id, "assistant", response)

            # Update status
            dpg.set_value("system_status", "STATUS: READY")

            self.scroll_to_bottom()
            self.is_typing = False
            self.refresh_chat_list()

        threading.Thread(target=response_process, daemon=True).start()

    def get_bloomberg_ai_response(self, user_message):
        """Generate Bloomberg Terminal style AI responses"""
        message_lower = user_message.lower()

        if any(word in message_lower for word in ["hello", "hi", "hey"]):
            return "FINCEPT AI ASSISTANT ONLINE\n\nWelcome to the Fincept Terminal AI Assistant. I'm your advanced financial intelligence system, ready to help with market analysis, trading insights, portfolio optimization, and financial queries.\n\nHow may I assist you with your financial needs today?"

        elif any(word in message_lower for word in ["market", "stock", "trading", "equity"]):
            return "MARKET ANALYSIS MODULE ACTIVATED\n\nI can provide comprehensive market assistance including:\n\n• Real-time equity analysis and price monitoring\n• Technical indicator analysis (RSI, MACD, Moving Averages)\n• Sector performance and comparative analysis\n• Trading strategy recommendations\n• Risk assessment and portfolio optimization\n• Market trend identification and forecasting\n\nPlease specify which market segment, ticker symbol, or analysis type you're interested in."

        elif any(word in message_lower for word in ["portfolio", "investment", "asset"]):
            return "PORTFOLIO MANAGEMENT SYSTEM\n\nAccessing investment advisory capabilities:\n\n• Portfolio composition analysis\n• Asset allocation optimization\n• Risk-return profiling\n• Diversification strategies\n• Performance benchmarking\n• Rebalancing recommendations\n\nTo provide personalized investment advice, please share your investment objectives, risk tolerance, and time horizon."

        elif any(word in message_lower for word in ["news", "earnings", "financial", "report"]):
            return "FINANCIAL NEWS & ANALYTICS ENGINE\n\nI can help you with:\n\n• Real-time market news analysis\n• Earnings report interpretation\n• Economic indicator impact assessment\n• Corporate announcement analysis\n• Regulatory filing insights\n• Market sentiment analysis\n\nWhat specific financial information or news analysis do you need?"

        elif any(word in message_lower for word in ["help", "commands", "functions"]):
            return "FINCEPT TERMINAL HELP SYSTEM\n\nAvailable AI Assistant Functions:\n\n• MARKET ANALYSIS - Real-time stock and market data\n• PORTFOLIO ADVISORY - Investment strategy and optimization\n• FINANCIAL NEWS - Market news and economic updates\n• TECHNICAL ANALYSIS - Chart patterns and indicators\n• RISK MANAGEMENT - Portfolio risk assessment\n• ECONOMIC DATA - Macro-economic indicators\n\nFunction Keys:\nF1: Help | F2: Chats | F3: New Chat | F4: Search | F5: Clear | F6: Settings\n\nType any financial query to get started!"

        elif any(word in message_lower for word in ["risk", "volatility", "hedge"]):
            return "RISK MANAGEMENT MODULE\n\nRisk analysis capabilities available:\n\n• Value at Risk (VaR) calculations\n• Portfolio volatility assessment\n• Correlation analysis\n• Hedging strategy recommendations\n• Stress testing scenarios\n• Black swan event preparation\n\nWhat specific risk analysis do you require for your portfolio or trading position?"

        else:
            return f"QUERY PROCESSING: {user_message[:40]}...\n\nI'm analyzing your financial query using advanced AI algorithms. As your Bloomberg Terminal AI Assistant, I specialize in:\n\n• Market data analysis and interpretation\n• Financial modeling and forecasting\n• Investment research and recommendations\n• Risk assessment and management\n• Real-time market monitoring\n\nCould you provide more specific details about your financial requirements or specify which market instruments you're interested in?"

    def scroll_to_bottom(self):
        """Scroll messages to bottom"""

        def scroll():
            try:
                max_scroll = dpg.get_y_scroll_max("messages_display_area")
                dpg.set_y_scroll("messages_display_area", max_scroll)
            except:
                pass

        threading.Timer(0.1, scroll).start()

    def message_input_callback(self, sender, app_data):
        """Handle message input changes and Enter key"""
        text = dpg.get_value("message_input_field").strip()
        char_count = len(text)

        # Update status
        status = f"Characters: {char_count} | "
        if char_count > 0:
            status += "Ready to send"
        else:
            status += "Type a message"

        dpg.set_value("input_status", status)

        # Handle Enter key press (auto-send)
        if app_data == '\n' and text:
            # Remove the newline character
            dpg.set_value("message_input_field", text)
            self.send_message_callback()

    def send_message_callback(self):
        """Enhanced send message with auto chat creation"""
        message = dpg.get_value("message_input_field").strip()

        if not message:
            return

        # Auto-create chat if none selected
        if not self.current_chat_id:
            smart_title = self.generate_smart_title(message)
            self.current_chat_id = self.db.create_new_chat(smart_title)
            dpg.set_value("chat_session_name", smart_title)
            dpg.set_value("active_session_info", f"Active Session: {smart_title}")

            # Clear welcome screen
            if dpg.does_item_exist("welcome_screen"):
                dpg.delete_item("welcome_screen")

        # Check if this is first message in existing "New Conversation" chat
        messages = self.db.get_chat_messages(self.current_chat_id)
        if len(messages) == 0:
            # Get current chat title
            chats = self.db.get_all_chats()
            for chat_id, title, _, _, _, _ in chats:
                if chat_id == self.current_chat_id and "Session-" in title:
                    smart_title = self.generate_smart_title(message)
                    self.db.update_chat_title(self.current_chat_id, smart_title)
                    dpg.set_value("chat_session_name", smart_title)
                    dpg.set_value("active_session_info", f"Active Session: {smart_title}")
                    break

        # Clear welcome screen if present
        if dpg.does_item_exist("welcome_screen"):
            dpg.delete_item("welcome_screen")

        # Add user message
        self.create_message_bubble("user", message)
        self.db.add_message(self.current_chat_id, "user", message)

        # Clear input
        dpg.set_value("message_input_field", "")
        dpg.set_value("input_status", "Characters: 0 | Message sent")

        # Update status
        dpg.set_value("system_status", "STATUS: PROCESSING")

        # Add to command history
        with dpg.group(parent="command_history"):
            dpg.add_text(f"> AI_QUERY: {message[:30]}...", color=self.BLOOMBERG_WHITE)
            dpg.add_text("  Processing with AI engine...", color=self.BLOOMBERG_YELLOW)

        # Scroll command history
        try:
            max_scroll = dpg.get_y_scroll_max("command_history")
            dpg.set_y_scroll("command_history", max_scroll)
        except:
            pass

        # Generate AI response
        self.simulate_ai_response(message)

        # Refresh and scroll
        self.scroll_to_bottom()
        self.refresh_chat_list()

    def clear_input(self):
        """Clear input field"""
        dpg.set_value("message_input_field", "")
        dpg.set_value("input_status", "Characters: 0 | Ready to type")

    def new_chat_callback(self):
        """Create new chat with Bloomberg styling"""
        current_time = datetime.now().strftime("%H%M")
        chat_title = f"Session-{self.chat_counter:03d}-{current_time}"
        self.chat_counter += 1

        self.current_chat_id = self.db.create_new_chat(chat_title)

        # Update UI
        dpg.set_value("chat_session_name", chat_title)
        dpg.set_value("active_session_info", f"Active Session: {chat_title}")
        dpg.set_value("system_status", "STATUS: NEW CHAT CREATED")

        # Clear messages and show welcome
        dpg.delete_item("messages_display_area", children_only=True)
        self.create_welcome_screen()

        # Add to command history
        with dpg.group(parent="command_history"):
            dpg.add_text("> NEW_CHAT", color=self.BLOOMBERG_WHITE)
            dpg.add_text(f"  Created: {chat_title}", color=self.BLOOMBERG_GREEN)

        # Refresh and focus
        self.refresh_chat_list()
        dpg.focus_item("message_input_field")

    def select_chat_callback(self, chat_id):
        """Select and load chat"""
        self.current_chat_id = chat_id
        self.db.set_active_chat(chat_id)

        # Get chat info
        chats = self.db.get_all_chats()
        chat_title = "Unknown"
        for cid, title, _, _, _, _ in chats:
            if cid == chat_id:
                chat_title = title
                break

        # Update UI
        dpg.set_value("chat_session_name", chat_title)
        dpg.set_value("active_session_info", f"Active Session: {chat_title}")
        dpg.set_value("system_status", "STATUS: CHAT LOADED")

        # Load messages
        self.load_chat_messages(chat_id)

        # Add to command history
        with dpg.group(parent="command_history"):
            dpg.add_text(f"> LOAD_CHAT: {chat_title[:20]}", color=self.BLOOMBERG_WHITE)
            dpg.add_text("  Chat loaded successfully", color=self.BLOOMBERG_GREEN)

        self.refresh_chat_list()

    def load_chat_messages(self, chat_id):
        """Load messages for specific chat"""
        # Clear welcome and existing messages
        if dpg.does_item_exist("welcome_screen"):
            dpg.delete_item("welcome_screen")

        dpg.delete_item("messages_display_area", children_only=True)

        # Load messages
        messages = self.db.get_chat_messages(chat_id)
        for role, content, timestamp in messages:
            self.create_message_bubble(role, content, timestamp)

        self.scroll_to_bottom()

    def delete_chat_callback(self, chat_id):
        """Delete specific chat"""
        self.db.delete_chat(chat_id)

        if self.current_chat_id == chat_id:
            self.current_chat_id = None
            dpg.set_value("chat_session_name", "No Active Session")
            dpg.set_value("active_session_info", "Active Session: None")
            dpg.set_value("system_status", "STATUS: CHAT DELETED")

            dpg.delete_item("messages_display_area", children_only=True)
            self.create_welcome_screen()

        self.refresh_chat_list()

    def delete_current_chat(self):
        """Delete current active chat"""
        if self.current_chat_id:
            self.delete_chat_callback(self.current_chat_id)

    def refresh_chat_list(self):
        """Refresh chat list"""
        # Clear existing
        dpg.delete_item("chat_list_area", children_only=True)

        # Add all chats
        chats = self.db.get_all_chats()
        for chat_id, title, created_at, updated_at, is_active, message_count in chats:
            self.create_chat_item(chat_id, title, message_count, updated_at, is_active)

        # Update stats
        stats = self.db.get_chat_stats()
        dpg.set_value("total_sessions", f"Total Sessions: {stats['total_chats']}")
        dpg.set_value("total_messages", f"Total Messages: {stats['total_messages']}")

    def filter_chats_callback(self, sender, app_data):
        """Filter chats based on search"""
        search_term = app_data.lower()

        # Clear and reload filtered
        dpg.delete_item("chat_list_area", children_only=True)

        chats = self.db.get_all_chats()
        for chat_id, title, created_at, updated_at, is_active, message_count in chats:
            if search_term in title.lower():
                self.create_chat_item(chat_id, title, message_count, updated_at, is_active)

    def clear_current_chat(self):
        """Clear current chat messages"""
        if self.current_chat_id:
            dpg.delete_item("messages_display_area", children_only=True)
            self.create_welcome_screen()
            dpg.set_value("system_status", "STATUS: CHAT CLEARED")

    # Function key callbacks
    def show_help(self):
        """Show help information"""
        if not dpg.does_item_exist("help_window"):
            with dpg.window(label="FINCEPT TERMINAL - HELP", tag="help_window",
                            width=600, height=500, pos=[300, 150]):
                dpg.add_text("FINCEPT AI ASSISTANT HELP", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                dpg.add_text("FUNCTION KEYS:", color=self.BLOOMBERG_YELLOW)
                dpg.add_text("F1: Help - Show this help screen")
                dpg.add_text("F2: Chats - Focus chat list")
                dpg.add_text("F3: New - Create new chat session")
                dpg.add_text("F4: Search - Focus search field")
                dpg.add_text("F5: Clear - Clear current chat")
                dpg.add_text("F6: Settings - Show settings")

                dpg.add_separator()

                dpg.add_text("USAGE:", color=self.BLOOMBERG_YELLOW)
                dpg.add_text("• Type message and press Enter to send")
                dpg.add_text("• Chat auto-created when sending first message")
                dpg.add_text("• Browse chat history in left panel")
                dpg.add_text("• Use search to filter chats")

                dpg.add_spacer(height=20)
                dpg.add_button(label="CLOSE", callback=lambda: dpg.delete_item("help_window"))
        else:
            dpg.show_item("help_window")

    def focus_chats(self):
        """Focus chat search"""
        dpg.focus_item("chat_search")

    def focus_search(self):
        """Focus search field"""
        dpg.focus_item("chat_search")

    def show_settings(self):
        """Show settings window"""
        if not dpg.does_item_exist("settings_window"):
            with dpg.window(label="SYSTEM SETTINGS", tag="settings_window",
                            width=500, height=400, pos=[350, 200]):
                dpg.add_text("FINCEPT TERMINAL SETTINGS", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                stats = self.db.get_chat_stats()
                dpg.add_text(f"Database: {self.db.db_path}")
                dpg.add_text(f"Total Chats: {stats['total_chats']}")
                dpg.add_text(f"Total Messages: {stats['total_messages']}")

                dpg.add_separator()

                dpg.add_button(label="CLEAR ALL DATA", callback=self.confirm_clear_all)

                dpg.add_spacer(height=20)
                dpg.add_button(label="CLOSE", callback=lambda: dpg.delete_item("settings_window"))
        else:
            dpg.show_item("settings_window")

    def confirm_clear_all(self):
        """Confirm clear all data"""
        if not dpg.does_item_exist("confirm_window"):
            with dpg.window(label="CONFIRM DELETE", tag="confirm_window",
                            width=400, height=200, pos=[400, 300]):
                dpg.add_text("WARNING: DELETE ALL DATA?", color=self.BLOOMBERG_RED)
                dpg.add_text("This will permanently delete all chats!")

                dpg.add_spacer(height=30)

                with dpg.group(horizontal=True):
                    dpg.add_button(label="CANCEL",
                                   callback=lambda: dpg.delete_item("confirm_window"))
                    dpg.add_spacer(width=50)
                    dpg.add_button(label="DELETE ALL", callback=self.execute_clear_all)
        else:
            dpg.show_item("confirm_window")

    def execute_clear_all(self):
        """Execute clear all data"""
        # Clear database
        conn = sqlite3.connect(self.db.db_path)
        cursor = conn.cursor()
        cursor.execute("DELETE FROM messages")
        cursor.execute("DELETE FROM chats")
        conn.commit()
        conn.close()

        # Reset UI
        self.current_chat_id = None
        self.chat_counter = 1
        dpg.set_value("chat_session_name", "No Active Session")
        dpg.set_value("active_session_info", "Active Session: None")
        dpg.set_value("system_status", "STATUS: ALL DATA CLEARED")

        # Clear displays
        dpg.delete_item("messages_display_area", children_only=True)
        self.create_welcome_screen()
        self.refresh_chat_list()

        # Close dialogs
        dpg.delete_item("confirm_window")
        dpg.delete_item("settings_window")

    def execute_command(self):
        """Execute command from header input"""
        command = dpg.get_value("command_input").strip().upper()

        if command == "NEW_CHAT":
            self.new_chat_callback()
        elif command == "CLEAR":
            self.clear_current_chat()
        elif command == "HELP":
            self.show_help()
        elif command == "SETTINGS":
            self.show_settings()
        else:
            dpg.set_value("system_status", f"STATUS: UNKNOWN COMMAND - {command}")

        dpg.set_value("command_input", "Enter AI Command")

    def execute_quick_command(self, command):
        """Execute quick command"""
        if command == "NEW_CHAT":
            self.new_chat_callback()
        elif command == "CLEAR_HISTORY":
            self.clear_current_chat()
        elif command == "EXPORT_CHAT":
            dpg.set_value("system_status", "STATUS: EXPORT FEATURE COMING SOON")
        elif command == "AI_SUMMARY":
            if self.current_chat_id:
                # Auto-send summary request
                dpg.set_value("message_input_field", "Please provide a summary of our conversation")
                self.send_message_callback()
        elif command == "MARKET_HELP":
            dpg.set_value("message_input_field", "I need help with market analysis and trading")
            self.send_message_callback()
        elif command == "SYSTEM_STATUS":
            stats = self.db.get_chat_stats()
            status_msg = f"System Status: Online | Chats: {stats['total_chats']} | Messages: {stats['total_messages']}"
            dpg.set_value("system_status", status_msg)

    def update_time_displays(self):
        """Update time displays"""
        current_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        time_only = datetime.now().strftime('%H:%M:%S')

        try:
            dpg.set_value("header_time", current_time)
            dpg.set_value("status_time", f"TIME: {time_only}")
            dpg.set_value("last_update_time", time_only)
        except:
            pass

    def start_time_updater(self):
        """Start time updater thread"""

        def time_loop():
            while True:
                self.update_time_displays()
                time.sleep(1)

        timer_thread = threading.Thread(target=time_loop, daemon=True)
        timer_thread.start()

    def load_initial_data(self):
        """Load initial data and setup"""
        # Apply theme
        self.theme_manager.apply_theme_globally("bloomberg_terminal")

        # Load chats
        self.refresh_chat_list()

        # Check for active chat
        active_chat = self.db.get_active_chat()
        if active_chat:
            self.current_chat_id = active_chat[0]
            self.load_chat_messages(self.current_chat_id)
            dpg.set_value("chat_session_name", active_chat[1])
            dpg.set_value("active_session_info", f"Active Session: {active_chat[1]}")

        # Start time updater
        self.start_time_updater()

    def run(self):
        """Run the Bloomberg Terminal Chat Application"""
        try:
            # Create viewport
            dpg.create_viewport(
                title="FINCEPT TERMINAL - AI Assistant Professional Interface",
                width=1600,
                height=1000,
                min_width=1200,
                min_height=800,
                resizable=True,
                vsync=True
            )

            # Setup and show
            dpg.setup_dearpygui()
            dpg.show_viewport()

            # Apply theme
            self.theme_manager.apply_theme_globally("bloomberg_terminal")

            print("=" * 80)
            print("FINCEPT TERMINAL AI ASSISTANT - PROFESSIONAL EDITION")
            print("=" * 80)
            print("STATUS: ONLINE")
            print("THEME: Bloomberg Terminal (Authentic)")
            print("DATABASE: SQLite Connected")
            print("AI ENGINE: Ready")
            print("FEATURES: Enhanced UI with Letter-by-Letter Animation")
            print("=" * 80)

            # Main loop
            while dpg.is_dearpygui_running():
                dpg.render_dearpygui_frame()

        except Exception as e:
            print(f"CRITICAL ERROR: {e}")
            import traceback
            traceback.print_exc()
        finally:
            self.cleanup()

    def cleanup(self):
        """Cleanup resources"""
        try:
            print("SHUTTING DOWN FINCEPT TERMINAL AI ASSISTANT...")

            if hasattr(self, 'theme_manager'):
                self.theme_manager.cleanup()

            if dpg.is_dearpygui_running():
                dpg.destroy_context()

            print("SHUTDOWN COMPLETE")

        except Exception as e:
            print(f"CLEANUP WARNING: {e}")


def main():
    """Main entry point"""
    try:
        print("INITIALIZING FINCEPT TERMINAL AI ASSISTANT...")
        print("Loading Bloomberg Terminal interface components...")

        # Create and run application
        chat_app = BloombergTerminalChatUI()
        chat_app.run()

    except KeyboardInterrupt:
        print("\nSHUTDOWN INITIATED BY USER")
    except Exception as e:
        print(f"FATAL SYSTEM ERROR: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()