import dearpygui.dearpygui as dpg
import json
import requests
import pyotp
import hashlib
from urllib import parse
import sys
import threading
import datetime
import os
from pathlib import Path
from base_tab import BaseTab

# Safe import of Fyers WebSocket
try:
    from fyers_apiv3.FyersWebsocket import data_ws

    FYERS_AVAILABLE = True
except ImportError as e:
    print(f"⚠️ Fyers API not found: {e}")
    print("💡 Install with: pip install fyers-apiv3")
    FYERS_AVAILABLE = False


class FyersTab(BaseTab):
    """Fyers Trading Tab for stock data streaming and API integration"""

    def __init__(self, app):
        super().__init__(app)

        # Fyers credentials (will be loaded from UI or config)
        self.client_id = ""
        self.pin = ""
        self.app_id = ""
        self.app_type = ""
        self.app_secret = ""
        self.totp_secret_key = ""
        self.redirect_uri = "https://trade.fyers.in/api-login/redirect-uri/index.html"

        # API endpoints
        self.BASE_URL = "https://api-t2.fyers.in/vagator/v2"
        self.BASE_URL_2 = "https://api-t1.fyers.in/api/v3"

        # State management
        self.access_token = None
        self.is_connected = False
        self.websocket_client = None
        self.streaming_data = []
        self.max_streaming_rows = 1000
        self.previous_prices = {}  # Store previous prices for color coding
        self.is_paused = False  # Add pause functionality
        self.scroll_position = {'x': 0, 'y': 0}  # Track scroll position

        # WebSocket settings
        self.current_symbols = ['NSE:SBIN-EQ', 'NSE:ADANIENT-EQ']
        self.current_data_type = "DepthUpdate"

        # Authentication status
        self.auth_status = "Not Authenticated"

    def get_label(self):
        return "📈 Fyers Trading"

    def create_content(self):
        """Create the Fyers trading interface"""
        self.add_section_header("📈 Fyers Trading Platform")

        if not FYERS_AVAILABLE:
            dpg.add_text("❌ Fyers API not available!")
            dpg.add_text("Install with: pip install fyers-apiv3")
            return

        # Authentication panel
        self.create_auth_panel()
        dpg.add_spacer(height=10)

        # Connection and streaming panel
        self.create_streaming_panel()
        dpg.add_spacer(height=10)

        # Data viewer
        self.create_data_viewer()

    def create_auth_panel(self):
        """Create authentication and token management panel"""
        with dpg.collapsing_header(label="🔐 Authentication & Token Management", default_open=True):
            with dpg.group(horizontal=True):
                # Credentials panel
                with self.create_child_window("credentials_panel", width=400, height=300):
                    dpg.add_text("Fyers API Credentials")
                    dpg.add_separator()

                    dpg.add_input_text(label="Client ID", default_value=self.client_id,
                                       tag="fyers_client_id", width=200)
                    dpg.add_input_text(label="PIN", default_value=self.pin,
                                       tag="fyers_pin", password=True, width=200)
                    dpg.add_input_text(label="App ID", default_value=self.app_id,
                                       tag="fyers_app_id", width=200)
                    dpg.add_input_text(label="App Type", default_value=self.app_type,
                                       tag="fyers_app_type", width=200)
                    dpg.add_input_text(label="App Secret", default_value=self.app_secret,
                                       tag="fyers_app_secret", password=True, width=200)
                    dpg.add_input_text(label="TOTP Secret", default_value=self.totp_secret_key,
                                       tag="fyers_totp_secret", password=True, width=200)

                    dpg.add_spacer(height=10)
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="🔑 Generate Token",
                                       callback=self.generate_access_token, width=120)
                        dpg.add_button(label="📁 Load Token",
                                       callback=self.load_access_token, width=100)

                # Status and token info panel
                with self.create_child_window("auth_status_panel", width=390, height=300):
                    dpg.add_text("Authentication Status")
                    dpg.add_separator()

                    dpg.add_text("Status: Not Authenticated", tag="auth_status_text",
                                 color=(255, 100, 100))
                    dpg.add_text("Token: None", tag="token_status")
                    dpg.add_text("Generated: Never", tag="token_generated_time")
                    dpg.add_text("Valid Until: Unknown", tag="token_validity")

                    dpg.add_spacer(height=10)
                    dpg.add_text("Token File Status:")
                    dpg.add_text("access_token.log: Not Found", tag="token_file_status")

                    dpg.add_spacer(height=10)
                    with dpg.child_window(height=100, tag="auth_log"):
                        dpg.add_text("Ready for authentication...", tag="auth_log_text")

    def create_streaming_panel(self):
        """Create WebSocket streaming control panel"""
        with dpg.collapsing_header(label="📡 Real-time Data Streaming", default_open=True):
            with dpg.group(horizontal=True):
                # Connection controls
                with self.create_child_window("connection_controls", width=300, height=250):
                    dpg.add_text("WebSocket Connection")
                    dpg.add_separator()

                    dpg.add_text("Status: Disconnected", tag="ws_status_text",
                                 color=(255, 100, 100))
                    dpg.add_text("Data Type: None", tag="ws_data_type")
                    dpg.add_text("Symbols: None", tag="ws_symbols")
                    dpg.add_text("Messages Received: 0", tag="ws_message_count")

                    dpg.add_spacer(height=10)
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="🔗 Connect", callback=self.connect_websocket, width=90)
                        dpg.add_button(label="❌ Disconnect", callback=self.disconnect_websocket, width=90)

                # Streaming settings
                with self.create_child_window("streaming_settings", width=250, height=250):
                    dpg.add_text("Streaming Settings")
                    dpg.add_separator()

                    dpg.add_text("Data Type:")
                    dpg.add_combo(["SymbolUpdate", "DepthUpdate"],
                                  default_value=self.current_data_type,
                                  tag="stream_data_type", width=-1)

                    dpg.add_spacer(height=10)
                    dpg.add_text("Stock Symbols:")
                    dpg.add_input_text(hint="NSE:SBIN-EQ,NSE:ADANIENT-EQ",
                                       default_value=",".join(self.current_symbols),
                                       tag="stream_symbols", width=-1, multiline=True, height=60)

                    dpg.add_spacer(height=10)
                    dpg.add_button(label="🔄 Update Subscription",
                                   callback=self.update_subscription, width=-1)

                # Streaming stats
                with self.create_child_window("streaming_stats", width=240, height=250):
                    dpg.add_text("Streaming Statistics")
                    dpg.add_separator()

                    dpg.add_text("Session Time: 00:00:00", tag="session_time")
                    dpg.add_text("Data Points: 0", tag="data_points_count")
                    dpg.add_text("Last Update: Never", tag="last_update_time")
                    dpg.add_text("Data Rate: 0 msg/sec", tag="data_rate")

                    dpg.add_spacer(height=10)
                    dpg.add_text("Max Display Rows:")
                    dpg.add_combo([100, 500, 1000, 2000], default_value=1000,
                                  tag="max_display_rows", callback=self.on_max_rows_changed, width=-1)

                    dpg.add_spacer(height=10)
                    dpg.add_button(label="🧹 Clear Data", callback=self.clear_streaming_data, width=-1)

    def create_data_viewer(self):
        """Create real-time data viewer"""
        with dpg.collapsing_header(label="📊 Live Data Feed", default_open=True):
            with dpg.group(horizontal=True):
                # Controls
                with dpg.group():
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="⏸️ Pause", tag="pause_button",
                                       callback=self.toggle_pause, width=80)
                        dpg.add_button(label="💾 Export", callback=self.export_data, width=80)
                        dpg.add_text("Auto-scroll:")
                        dpg.add_checkbox(tag="auto_scroll", default_value=True)

            dpg.add_spacer(height=5)

            # Data table
            with self.create_child_window("live_data_viewer", width=-1, height=400):
                dpg.add_text("Connect to WebSocket to see live data...", tag="data_viewer_status")

                # Dynamic table will be created here
                with dpg.group(tag="live_data_table_container"):
                    pass

    # Authentication methods
    def generate_access_token(self):
        """Generate new access token using Fyers authentication flow"""

        def auth_thread():
            try:
                self.update_auth_log("🔄 Starting authentication process...")

                # Get credentials from UI
                client_id = dpg.get_value("fyers_client_id")
                pin = dpg.get_value("fyers_pin")
                app_id = dpg.get_value("fyers_app_id")
                app_type = dpg.get_value("fyers_app_type")
                app_secret = dpg.get_value("fyers_app_secret")
                totp_secret = dpg.get_value("fyers_totp_secret")

                # Step 1: Verify client ID
                status, request_key = self.verify_client_id(client_id)
                if status != 1:
                    self.update_auth_log(f"❌ Client ID verification failed: {request_key}")
                    return
                self.update_auth_log("✅ Client ID verified")

                # Step 2: Generate TOTP
                status, totp = self.generate_totp(totp_secret)
                if status != 1:
                    self.update_auth_log(f"❌ TOTP generation failed: {totp}")
                    return
                self.update_auth_log("✅ TOTP generated")

                # Step 3: Verify TOTP
                status, request_key = self.verify_totp(request_key, totp)
                if status != 1:
                    self.update_auth_log(f"❌ TOTP verification failed: {request_key}")
                    return
                self.update_auth_log("✅ TOTP verified")

                # Step 4: Verify PIN
                status, fyers_access_token = self.verify_pin(request_key, pin)
                if status != 1:
                    self.update_auth_log(f"❌ PIN verification failed: {fyers_access_token}")
                    return
                self.update_auth_log("✅ PIN verified")

                # Step 5: Get auth code
                status, auth_code = self.get_token(client_id, app_id, self.redirect_uri,
                                                   app_type, fyers_access_token)
                if status != 1:
                    self.update_auth_log(f"❌ Token generation failed: {auth_code}")
                    return
                self.update_auth_log("✅ Auth code received")

                # Step 6: Validate auth code
                status, v3_access = self.validate_authcode(auth_code, app_id, app_type, app_secret)
                if status != 1:
                    self.update_auth_log(f"❌ Auth code validation failed: {v3_access}")
                    return
                self.update_auth_log("✅ Access token validated")

                # Build final token
                self.access_token = f"{app_id}-{app_type}:{v3_access}"

                # Save token to file
                self.save_access_token()

                # Update UI
                self.update_auth_status()
                self.update_auth_log("🎉 Authentication completed successfully!")

            except Exception as e:
                self.update_auth_log(f"❌ Authentication error: {str(e)}")

        threading.Thread(target=auth_thread, daemon=True).start()

    def load_access_token(self):
        """Load access token from file"""
        try:
            token_path = "access_token.log"
            if not os.path.exists(token_path):
                self.update_auth_log("❌ access_token.log file not found")
                return

            with open(token_path, "r") as f:
                tokens = [line.strip() for line in f if line.strip()]

            if not tokens:
                self.update_auth_log("❌ No tokens found in access_token.log")
                return

            self.access_token = tokens[-1]
            self.update_auth_status()
            self.update_auth_log("✅ Access token loaded from file")

        except Exception as e:
            self.update_auth_log(f"❌ Error loading token: {str(e)}")

    def save_access_token(self):
        """Save access token to file"""
        try:
            with open("access_token.log", "a") as f:
                f.write(f"{self.access_token}\n")
            self.update_auth_log("✅ Token saved to access_token.log")
        except Exception as e:
            self.update_auth_log(f"❌ Error saving token: {str(e)}")

    # Fyers API methods (from your original script)
    def verify_client_id(self, client_id):
        try:
            payload = {"fy_id": client_id, "app_id": "2"}
            resp = requests.post(url=f"{self.BASE_URL}/send_login_otp", json=payload)
            if resp.status_code != 200:
                return [-1, resp.text]
            data = resp.json()
            return [1, data["request_key"]]
        except Exception as e:
            return [-1, str(e)]

    def generate_totp(self, secret):
        try:
            return [1, pyotp.TOTP(secret).now()]
        except Exception as e:
            return [-1, str(e)]

    def verify_totp(self, request_key, totp):
        try:
            payload = {"request_key": request_key, "otp": totp}
            resp = requests.post(url=f"{self.BASE_URL}/verify_otp", json=payload)
            if resp.status_code != 200:
                return [-1, resp.text]
            data = resp.json()
            return [1, data["request_key"]]
        except Exception as e:
            return [-1, str(e)]

    def verify_pin(self, request_key, pin):
        try:
            payload = {
                "request_key": request_key,
                "identity_type": "pin",
                "identifier": pin
            }
            resp = requests.post(url=f"{self.BASE_URL}/verify_pin", json=payload)
            if resp.status_code != 200:
                return [-1, resp.text]
            data = resp.json()
            return [1, data["data"]["access_token"]]
        except Exception as e:
            return [-1, str(e)]

    def get_token(self, client_id, app_id, redirect_uri, app_type, access_token):
        try:
            payload = {
                "fyers_id": client_id,
                "app_id": app_id,
                "redirect_uri": redirect_uri,
                "appType": app_type,
                "code_challenge": "",
                "state": "sample_state",
                "scope": "",
                "nonce": "",
                "response_type": "code",
                "create_cookie": True
            }
            headers = {'Authorization': f'Bearer {access_token}'}
            resp = requests.post(url=f"{self.BASE_URL_2}/token", json=payload, headers=headers)
            if resp.status_code != 308:
                return [-1, resp.text]
            data = resp.json()
            url = data["Url"]
            auth_code = parse.parse_qs(parse.urlparse(url).query)['auth_code'][0]
            return [1, auth_code]
        except Exception as e:
            return [-1, str(e)]

    def validate_authcode(self, auth_code, app_id, app_type, app_secret):
        try:
            app_id_hash = hashlib.sha256(f"{app_id}-{app_type}:{app_secret}".encode()).hexdigest()
            payload = {
                "grant_type": "authorization_code",
                "appIdHash": app_id_hash,
                "code": auth_code,
            }
            resp = requests.post(url=f"{self.BASE_URL_2}/validate-authcode", json=payload)
            if resp.status_code != 200:
                return [-1, resp.text]
            data = resp.json()
            return [1, data["access_token"]]
        except Exception as e:
            return [-1, str(e)]

    # WebSocket methods
    def connect_websocket(self):
        """Connect to Fyers WebSocket"""
        if not self.access_token:
            self.update_auth_log("❌ No access token available. Generate token first.")
            return

        def connect_thread():
            try:
                self.update_auth_log("🔄 Connecting to WebSocket...")

                # Create WebSocket client
                self.websocket_client = data_ws.FyersDataSocket(
                    access_token=self.access_token,
                    log_path="",
                    litemode=False,
                    write_to_file=False,
                    reconnect=True,
                    on_connect=self.on_websocket_open,
                    on_close=self.on_websocket_close,
                    on_error=self.on_websocket_error,
                    on_message=self.on_websocket_message
                )

                # Connect
                self.websocket_client.connect()

            except Exception as e:
                error_msg = f"❌ WebSocket connection failed: {str(e)}"
                self.update_auth_log(error_msg)

        threading.Thread(target=connect_thread, daemon=True).start()

    def disconnect_websocket(self):
        """Disconnect from WebSocket - ENHANCED VERSION"""
        try:
            self.update_auth_log("🔄 Disconnecting WebSocket...")

            # Set flags immediately
            self.is_connected = False
            self.is_paused = True

            # Try multiple disconnect approaches
            if self.websocket_client:
                try:
                    # Method 1: Try standard disconnect
                    if hasattr(self.websocket_client, 'disconnect'):
                        self.websocket_client.disconnect()
                        self.update_auth_log("✅ Called disconnect() method")

                    # Method 2: Try close connection
                    if hasattr(self.websocket_client, 'close'):
                        self.websocket_client.close()
                        self.update_auth_log("✅ Called close() method")

                    # Method 3: Try stop method
                    if hasattr(self.websocket_client, 'stop'):
                        self.websocket_client.stop()
                        self.update_auth_log("✅ Called stop() method")

                    # Method 4: Check for ws attribute and close it
                    if hasattr(self.websocket_client, 'ws') and self.websocket_client.ws:
                        try:
                            self.websocket_client.ws.close()
                            self.update_auth_log("✅ Called ws.close() method")
                        except:
                            pass

                    # Method 5: Try to set running flag to False if it exists
                    if hasattr(self.websocket_client, 'running'):
                        self.websocket_client.running = False
                        self.update_auth_log("✅ Set running flag to False")

                    # Method 6: Try to set _running flag to False if it exists
                    if hasattr(self.websocket_client, '_running'):
                        self.websocket_client._running = False
                        self.update_auth_log("✅ Set _running flag to False")

                except Exception as e:
                    self.update_auth_log(f"⚠️ Error during disconnect methods: {str(e)}")

                # Force clear the client reference
                self.websocket_client = None
                self.update_auth_log("✅ WebSocket client reference cleared")

            # Update UI immediately
            dpg.set_value("ws_status_text", "Status: Disconnected")
            dpg.configure_item("ws_status_text", color=(255, 100, 100))
            dpg.set_value("ws_data_type", "Data Type: None")
            dpg.set_value("ws_symbols", "Symbols: None")
            dpg.set_value("ws_message_count", "Messages Received: 0")

            # Update pause button
            dpg.set_item_label("pause_button", "⏸️ Paused")

            self.update_auth_log("✅ WebSocket disconnected and UI updated")

        except Exception as e:
            self.update_auth_log(f"❌ Disconnect error: {str(e)}")
            # Force update UI even if there's an error
            self.is_connected = False
            self.is_paused = True
            dpg.set_value("ws_status_text", "Status: Force Disconnected")
            dpg.configure_item("ws_status_text", color=(255, 100, 100))

    # WebSocket callbacks
    def on_websocket_open(self):
        """WebSocket open callback"""
        self.is_connected = True
        self.is_paused = False  # Ensure not paused when connecting
        dpg.set_value("ws_status_text", "Status: Connected")
        dpg.configure_item("ws_status_text", color=(100, 255, 100))
        dpg.set_item_label("pause_button", "⏸️ Pause")

        # Subscribe to symbols
        data_type = dpg.get_value("stream_data_type")
        symbols_text = dpg.get_value("stream_symbols")
        symbols = [s.strip() for s in symbols_text.split(",") if s.strip()]

        if symbols:
            self.websocket_client.subscribe(symbols=symbols, data_type=data_type)
            dpg.set_value("ws_data_type", f"Data Type: {data_type}")
            dpg.set_value("ws_symbols", f"Symbols: {', '.join(symbols)}")
            self.websocket_client.keep_running()

        self.update_auth_log("✅ WebSocket connected and subscribed")

    def on_websocket_close(self, code):
        """WebSocket close callback"""
        self.is_connected = False
        self.is_paused = True
        dpg.set_value("ws_status_text", "Status: Disconnected")
        dpg.configure_item("ws_status_text", color=(255, 100, 100))
        dpg.set_item_label("pause_button", "⏸️ Paused")
        self.update_auth_log(f"❌ WebSocket closed (code: {code})")

    def on_websocket_error(self, error):
        """WebSocket error callback"""
        self.update_auth_log(f"❌ WebSocket error: {str(error)}")

    def on_websocket_message(self, message):
        """WebSocket message callback - ENHANCED WITH PAUSE SUPPORT"""
        try:
            # Skip processing if paused or disconnected
            if self.is_paused or not self.is_connected:
                return

            timestamp = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]

            # Add to streaming data with properly formatted JSON
            data_row = {
                'timestamp': timestamp,
                'symbol': message.get('symbol', 'Unknown'),
                'type': message.get('type', 'Unknown'),
                'data': message  # Store the full message dict
            }
            self.streaming_data.append(data_row)

            # Limit data size - Handle string/int conversion safely
            try:
                max_rows = dpg.get_value("max_display_rows")
                if isinstance(max_rows, str):
                    max_rows = int(max_rows)
                elif max_rows is None:
                    max_rows = 1000  # Default fallback
            except (ValueError, TypeError):
                max_rows = 1000  # Default fallback

            if len(self.streaming_data) > max_rows:
                self.streaming_data = self.streaming_data[-max_rows:]

            # Update UI only if auto-scroll is enabled AND not paused
            if dpg.get_value("auto_scroll") and not self.is_paused:
                self.update_streaming_stats()
                self.update_data_table()

        except Exception as e:
            error_msg = f"❌ Message processing error: {str(e)}"
            self.update_auth_log(error_msg)

    # UI update methods
    def update_auth_status(self):
        """Update authentication status in UI"""
        if self.access_token:
            dpg.set_value("auth_status_text", "Status: Authenticated")
            dpg.configure_item("auth_status_text", color=(100, 255, 100))
            dpg.set_value("token_status", f"Token: {self.access_token[:20]}...")
            dpg.set_value("token_generated_time",
                          f"Generated: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
            dpg.set_value("token_validity", "Valid Until: End of trading day")

        # Check token file
        if os.path.exists("access_token.log"):
            dpg.set_value("token_file_status", "access_token.log: Found")
        else:
            dpg.set_value("token_file_status", "access_token.log: Not Found")

    def update_streaming_stats(self):
        """Update streaming statistics"""
        data_count = len(self.streaming_data)
        dpg.set_value("data_points_count", f"Data Points: {data_count}")
        dpg.set_value("last_update_time",
                      f"Last Update: {datetime.datetime.now().strftime('%H:%M:%S')}")

        # Update message count - Handle string/int conversion safely
        try:
            current_text = dpg.get_value("ws_message_count")
            if isinstance(current_text, str) and ": " in current_text:
                current_count = int(current_text.split(": ")[1])
            else:
                current_count = 0
            dpg.set_value("ws_message_count", f"Messages Received: {current_count + 1}")
        except (ValueError, IndexError, TypeError):
            dpg.set_value("ws_message_count", "Messages Received: 1")

    def update_data_table(self):
        """Update the live data table with individual JSON fields as columns - ENHANCED VERSION"""
        # Only update if auto-scroll is enabled and not paused
        if not dpg.get_value("auto_scroll") or not self.streaming_data or self.is_paused:
            return

        # Don't rebuild table too frequently - add simple rate limiting
        current_time = datetime.datetime.now()
        if hasattr(self, '_last_table_update'):
            time_diff = (current_time - self._last_table_update).total_seconds()
            if time_diff < 0.5:  # Limit updates to max 2 per second
                return

        self._last_table_update = current_time

        # Clear existing table
        dpg.delete_item("live_data_table_container", children_only=True)

        if not self.streaming_data:
            dpg.add_text("No data to display", parent="live_data_table_container")
            return

        # Get recent data (last 50 rows for performance)
        recent_data = self.streaming_data[-50:] if len(self.streaming_data) > 50 else self.streaming_data

        if not recent_data:
            return

        # Collect all unique keys from all messages to create dynamic columns
        all_keys = set()
        for row in recent_data:
            if isinstance(row['data'], dict):
                all_keys.update(row['data'].keys())

        # Sort keys for consistent column order
        sorted_keys = sorted(list(all_keys))

        # Create new table with dynamic columns
        with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                       borders_innerV=True, borders_outerV=True,
                       parent="live_data_table_container", scrollY=True, scrollX=True, height=350,
                       tag="live_data_table"):

            # Add fixed columns first
            dpg.add_table_column(label="Time", width_fixed=True, init_width_or_weight=80)
            dpg.add_table_column(label="Symbol", width_fixed=True, init_width_or_weight=120)
            dpg.add_table_column(label="Type", width_fixed=True, init_width_or_weight=60)

            # Add dynamic columns for each JSON field
            for key in sorted_keys:
                if key not in ['symbol', 'type']:  # Skip already displayed fields
                    dpg.add_table_column(label=key, width_fixed=True, init_width_or_weight=100)

            # Add data rows
            for row in reversed(recent_data):  # Show newest first
                with dpg.table_row():
                    # Fixed columns
                    dpg.add_text(row['timestamp'])
                    dpg.add_text(row['symbol'])
                    dpg.add_text(row['type'])

                    # Dynamic columns from JSON data
                    if isinstance(row['data'], dict):
                        for key in sorted_keys:
                            if key not in ['symbol', 'type']:  # Skip already displayed fields
                                value = row['data'].get(key, '')
                                if value is None:
                                    display_value = 'NULL'
                                    color = None
                                else:
                                    try:
                                        # Format numbers nicely and determine color
                                        if isinstance(value, float):
                                            display_value = f"{value:.2f}"
                                            color = self.get_price_color(row['symbol'], key, value)
                                        else:
                                            display_value = str(value)
                                            color = None
                                    except:
                                        display_value = str(value)
                                        color = None

                                # Add text with color if it's a price field
                                if color:
                                    dpg.add_text(display_value, color=color)
                                else:
                                    dpg.add_text(display_value)
                    else:
                        # If data is not a dict, fill with empty values
                        for key in sorted_keys:
                            if key not in ['symbol', 'type']:
                                dpg.add_text("")

    def update_auth_log(self, message):
        """Update authentication log"""
        try:
            if dpg.does_item_exist("auth_log_text"):
                current_log = dpg.get_value("auth_log_text")
                new_log = f"{message}\n{current_log}"
                # Keep only last 10 lines
                lines = new_log.split('\n')[:10]
                dpg.set_value("auth_log_text", '\n'.join(lines))
        except:
            pass

    def get_price_color(self, symbol, field, current_value):
        """Get color for price fields based on movement (green=up, red=down)"""
        # Only apply colors to price-related fields
        price_fields = ['ltp', 'ask_price1', 'ask_price2', 'ask_price3', 'ask_price4', 'ask_price5',
                        'bid_price1', 'bid_price2', 'bid_price3', 'bid_price4', 'bid_price5',
                        'high_price', 'low_price', 'open_price', 'prev_close_price', 'avg_trade_price']

        if field not in price_fields or not isinstance(current_value, (int, float)):
            return None

        # Create unique key for symbol+field combination
        key = f"{symbol}_{field}"

        # Get previous value
        previous_value = self.previous_prices.get(key)

        # Update stored value
        self.previous_prices[key] = current_value

        # If no previous value, return default color
        if previous_value is None:
            return None

        # Compare and return color
        if current_value > previous_value:
            return (100, 255, 100)  # Green for price increase
        elif current_value < previous_value:
            return (255, 100, 100)  # Red for price decrease
        else:
            return None  # Default color for no change

    # Control callbacks
    def update_subscription(self):
        """Update WebSocket subscription"""
        if not self.is_connected or not self.websocket_client:
            self.update_auth_log("❌ Not connected to WebSocket")
            return

        try:
            # Get new settings from UI
            data_type = dpg.get_value("stream_data_type")
            symbols_text = dpg.get_value("stream_symbols")
            symbols = [s.strip() for s in symbols_text.split(",") if s.strip()]

            if not symbols:
                self.update_auth_log("❌ No symbols provided")
                return

            self.update_auth_log(f"🔄 Updating subscription to {len(symbols)} symbols...")

            # Try to unsubscribe from current symbols first (if method exists)
            if hasattr(self.websocket_client, 'unsubscribe') and self.current_symbols:
                try:
                    self.websocket_client.unsubscribe(symbols=self.current_symbols, data_type=self.current_data_type)
                    self.update_auth_log("✅ Unsubscribed from previous symbols")
                except Exception as e:
                    self.update_auth_log(f"⚠️ Unsubscribe warning: {str(e)}")

            # Subscribe to new symbols
            self.websocket_client.subscribe(symbols=symbols, data_type=data_type)

            # Update stored settings
            self.current_symbols = symbols
            self.current_data_type = data_type

            # Update UI
            dpg.set_value("ws_data_type", f"Data Type: {data_type}")
            dpg.set_value("ws_symbols", f"Symbols: {', '.join(symbols)}")

            self.update_auth_log(f"✅ Subscription updated: {data_type} for {len(symbols)} symbols")
            self.update_auth_log(f"📊 Symbols: {', '.join(symbols)}")

        except Exception as e:
            self.update_auth_log(f"❌ Subscription update failed: {str(e)}")

    def on_max_rows_changed(self, sender, app_data):
        """Handle max rows change"""
        try:
            if isinstance(app_data, str):
                self.max_streaming_rows = int(app_data)
            else:
                self.max_streaming_rows = app_data

            # Trim data if needed
            if len(self.streaming_data) > self.max_streaming_rows:
                self.streaming_data = self.streaming_data[-self.max_streaming_rows:]

            self.update_data_table()
        except (ValueError, TypeError):
            self.max_streaming_rows = 1000  # Default fallback

    def clear_streaming_data(self):
        """Clear all streaming data"""
        self.streaming_data = []
        dpg.delete_item("live_data_table_container", children_only=True)
        dpg.add_text("Data cleared. Waiting for new messages...",
                     parent="live_data_table_container")
        self.update_auth_log("🧹 Streaming data cleared")

    def toggle_pause(self):
        """Toggle pause/resume streaming"""
        self.is_paused = not self.is_paused

        if self.is_paused:
            dpg.set_item_label("pause_button", "▶️ Resume")
            self.update_auth_log("⏸️ Data streaming paused")
        else:
            dpg.set_item_label("pause_button", "⏸️ Pause")
            self.update_auth_log("▶️ Data streaming resumed")
            # Update table when resuming
            if self.streaming_data:
                self.update_data_table()

    def export_data(self):
        """Export streaming data to CSV with individual columns"""
        if not self.streaming_data:
            self.update_auth_log("❌ No data to export")
            return

        try:
            import csv
            filename = f"fyers_stream_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

            # Collect all unique keys from all messages
            all_keys = set(['timestamp', 'symbol', 'type'])
            for row in self.streaming_data:
                if isinstance(row['data'], dict):
                    all_keys.update(row['data'].keys())

            sorted_keys = sorted(list(all_keys))

            with open(filename, 'w', newline='', encoding='utf-8') as csvfile:
                writer = csv.DictWriter(csvfile, fieldnames=sorted_keys)
                writer.writeheader()

                for row in self.streaming_data:
                    export_row = {
                        'timestamp': row['timestamp'],
                        'symbol': row['symbol'],
                        'type': row['type']
                    }

                    # Add all JSON fields as separate columns
                    if isinstance(row['data'], dict):
                        for key, value in row['data'].items():
                            if key not in ['symbol', 'type']:  # Avoid duplicates
                                export_row[key] = value

                    writer.writerow(export_row)

            self.update_auth_log(f"✅ Exported {len(self.streaming_data)} records to {filename}")

        except Exception as e:
            self.update_auth_log(f"❌ Export failed: {str(e)}")

    def cleanup(self):
        """Clean up resources"""
        try:
            if self.websocket_client:
                self.websocket_client.disconnect()
            print("🧹 Fyers tab cleaned up")
        except:
            pass