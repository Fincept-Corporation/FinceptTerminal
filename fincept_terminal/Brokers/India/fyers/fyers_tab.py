# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
import json
import requests
import pyotp
import hashlib
from urllib import parse
import threading
import datetime
import os
import csv
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
from concurrent.futures import ThreadPoolExecutor
import time
import platform
from fincept_terminal.utils.base_tab import BaseTab

# Import new logger module
from fincept_terminal.utils.Logging.logger import (
    info, debug, warning, error, operation, monitor_performance
)

# Safe import of Fyers WebSocket
try:
    from fyers_apiv3.FyersWebsocket import data_ws
    FYERS_AVAILABLE = True
    info("Fyers API successfully imported")
except ImportError as e:
    warning(f"Fyers API not found: {e}")
    info("Install with: pip install fyers-apiv3")
    FYERS_AVAILABLE = False


class FyersTab(BaseTab):
    """Optimized Fyers Trading Tab for stock data streaming and API integration"""

    def __init__(self, app):
        super().__init__(app)

        # Generate unique tag prefix for this instance
        self.tag_prefix = f"fyers_{id(self)}_"

        # Initialize config directory
        self.config_dir = self._get_config_directory()
        self.config_dir.mkdir(parents=True, exist_ok=True)

        # Fyers credentials (will be loaded from UI or config)
        self.credentials = {
            'client_id': "",
            'pin': "",
            'app_id': "",
            'app_type': "",
            'app_secret': "",
            'totp_secret_key': "",
            'redirect_uri': "https://trade.fyers.in/api-login/redirect-uri/index.html"
        }

        # API endpoints
        self.BASE_URL = "https://api-t2.fyers.in/vagator/v2"
        self.BASE_URL_2 = "https://api-t1.fyers.in/api/v3"

        # State management with thread safety
        self._lock = threading.RLock()
        self.access_token = None
        self.is_connected = False
        self.websocket_client = None
        self.streaming_data = []
        self.max_streaming_rows = 1000
        self.previous_prices = {}
        self.is_paused = False
        self.session_start_time = None
        self.message_count = 0
        self.last_message_time = None

        # WebSocket settings
        self.current_symbols = ['NSE:SBIN-EQ', 'NSE:ADANIENT-EQ']
        self.current_data_type = "DepthUpdate"

        # UI update throttling
        self._last_table_update = None
        self._last_stats_update = None
        self.update_throttle_interval = 0.5  # seconds

        # Load saved token on startup
        self.load_access_token_on_startup()

        info("FyersTab initialized", context={'config_dir': str(self.config_dir)})

    def _get_config_directory(self) -> Path:
        """Get platform-specific config directory - uses .fincept folder"""
        # Use .fincept folder at home directory for all platforms
        config_dir = Path.home() / '.fincept' / 'fyers'
        return config_dir

    def get_label(self):
        return " Fyers Trading"

    def get_tag(self, tag_name: str) -> str:
        """Generate unique tag with prefix"""
        return f"{self.tag_prefix}{tag_name}"

    def safe_add_item(self, add_func, *args, **kwargs):
        """Safely add DearPyGUI item with tag checking"""
        if 'tag' in kwargs:
            tag = kwargs['tag']
            if dpg.does_item_exist(tag):
                try:
                    dpg.delete_item(tag)
                except:
                    pass
        return add_func(*args, **kwargs)

    def safe_set_value(self, tag: str, value: Any):
        """Safely set value with existence check"""
        try:
            if dpg.does_item_exist(tag):
                dpg.set_value(tag, value)
        except Exception as e:
            warning(f"Error setting value for {tag}: {e}")

    def safe_configure_item(self, tag: str, **kwargs):
        """Safely configure item with existence check"""
        try:
            if dpg.does_item_exist(tag):
                dpg.configure_item(tag, **kwargs)
        except Exception as e:
            warning(f"Error configuring item {tag}: {e}")

    @monitor_performance
    def create_content(self):
        """Create the Fyers trading interface with error handling"""
        with operation("create_fyers_content"):
            try:
                self.cleanup_existing_items()
                self.add_section_header(" Fyers Trading Platform")

                if not FYERS_AVAILABLE:
                    dpg.add_text(" Fyers API not available!")
                    dpg.add_text("Install with: pip install fyers-apiv3")
                    dpg.add_text("Command: pip install fyers-apiv3")
                    return

                # Create main interface
                self.create_auth_panel()
                dpg.add_spacer(height=10)
                self.create_streaming_panel()
                dpg.add_spacer(height=10)
                self.create_data_viewer()

                info("Fyers tab content created successfully")

            except Exception as e:
                error("Error creating Fyers tab content", context={'error': str(e)}, exc_info=True)
                # Fallback error display
                try:
                    dpg.add_text(f" Error creating interface: {str(e)}")
                    dpg.add_text("Please restart the application or check logs.")
                except:
                    pass

    def cleanup_existing_items(self):
        """Clean up any existing items with our tag prefix"""
        try:
            # Get all items and delete those with our prefix
            all_items = dpg.get_all_items()
            for item in all_items:
                try:
                    alias = dpg.get_item_alias(item)
                    if alias and alias.startswith(self.tag_prefix):
                        dpg.delete_item(item)
                except:
                    continue
        except Exception as e:
            warning(f"Warning during cleanup: {e}")

    def create_auth_panel(self):
        """Create authentication and token management panel"""
        with dpg.collapsing_header(label=" Authentication & Token Management", default_open=True):
            with dpg.group(horizontal=True):
                # Credentials panel
                with self.create_child_window("credentials_panel", width=400, height=320):
                    dpg.add_text("Fyers API Credentials")
                    dpg.add_separator()

                    self.safe_add_item(dpg.add_input_text,
                                       label="Client ID",
                                       default_value=self.credentials['client_id'],
                                       tag=self.get_tag("fyers_client_id"),
                                       width=200)

                    self.safe_add_item(dpg.add_input_text,
                                       label="PIN",
                                       default_value=self.credentials['pin'],
                                       tag=self.get_tag("fyers_pin"),
                                       password=True,
                                       width=200)

                    self.safe_add_item(dpg.add_input_text,
                                       label="App ID",
                                       default_value=self.credentials['app_id'],
                                       tag=self.get_tag("fyers_app_id"),
                                       width=200)

                    self.safe_add_item(dpg.add_input_text,
                                       label="App Type",
                                       default_value=self.credentials['app_type'],
                                       tag=self.get_tag("fyers_app_type"),
                                       width=200)

                    self.safe_add_item(dpg.add_input_text,
                                       label="App Secret",
                                       default_value=self.credentials['app_secret'],
                                       tag=self.get_tag("fyers_app_secret"),
                                       password=True,
                                       width=200)

                    self.safe_add_item(dpg.add_input_text,
                                       label="TOTP Secret",
                                       default_value=self.credentials['totp_secret_key'],
                                       tag=self.get_tag("fyers_totp_secret"),
                                       password=True,
                                       width=200)

                    dpg.add_spacer(height=10)
                    with dpg.group(horizontal=True):
                        dpg.add_button(label=" Generate Token",
                                       callback=self.generate_access_token, width=120)
                        dpg.add_button(label=" Load Token",
                                       callback=self.load_access_token, width=100)
                        dpg.add_button(label=" Save Config",
                                       callback=self.save_credentials, width=100)

                # Status and token info panel
                with self.create_child_window("auth_status_panel", width=390, height=320):
                    dpg.add_text("Authentication Status")
                    dpg.add_separator()

                    self.safe_add_item(dpg.add_text,
                                       "Status: Not Authenticated",
                                       tag=self.get_tag("auth_status_text"),
                                       color=(255, 100, 100))

                    self.safe_add_item(dpg.add_text,
                                       "Token: None",
                                       tag=self.get_tag("token_status"))

                    self.safe_add_item(dpg.add_text,
                                       "Generated: Never",
                                       tag=self.get_tag("token_generated_time"))

                    self.safe_add_item(dpg.add_text,
                                       "Valid Until: Unknown",
                                       tag=self.get_tag("token_validity"))

                    dpg.add_spacer(height=10)
                    dpg.add_text("Token File Status:")
                    self.safe_add_item(dpg.add_text,
                                       "access_token.log: Not Found",
                                       tag=self.get_tag("token_file_status"))

                    dpg.add_spacer(height=10)
                    with dpg.child_window(height=120, tag=self.get_tag("auth_log")):
                        self.safe_add_item(dpg.add_text,
                                           "Ready for authentication...",
                                           tag=self.get_tag("auth_log_text"),
                                           wrap=370)

    def create_streaming_panel(self):
        """Create WebSocket streaming control panel"""
        with dpg.collapsing_header(label=" Real-time Data Streaming", default_open=True):
            with dpg.group(horizontal=True):
                # Connection controls
                with self.create_child_window("connection_controls", width=300, height=280):
                    dpg.add_text("WebSocket Connection")
                    dpg.add_separator()

                    self.safe_add_item(dpg.add_text,
                                       "Status: Disconnected",
                                       tag=self.get_tag("ws_status_text"),
                                       color=(255, 100, 100))

                    self.safe_add_item(dpg.add_text,
                                       "Data Type: None",
                                       tag=self.get_tag("ws_data_type"))

                    self.safe_add_item(dpg.add_text,
                                       "Symbols: None",
                                       tag=self.get_tag("ws_symbols"))

                    self.safe_add_item(dpg.add_text,
                                       "Messages Received: 0",
                                       tag=self.get_tag("ws_message_count"))

                    dpg.add_spacer(height=10)
                    with dpg.group(horizontal=True):
                        dpg.add_button(label=" Connect", callback=self.connect_websocket, width=90)
                        dpg.add_button(label=" Disconnect", callback=self.disconnect_websocket, width=90)

                    dpg.add_spacer(height=10)
                    dpg.add_text("Connection Health:")
                    self.safe_add_item(dpg.add_text,
                                       "Ping: Unknown",
                                       tag=self.get_tag("ws_ping"))

                    self.safe_add_item(dpg.add_text,
                                       "Reconnects: 0",
                                       tag=self.get_tag("ws_reconnects"))

                # Streaming settings
                with self.create_child_window("streaming_settings", width=250, height=280):
                    dpg.add_text("Streaming Settings")
                    dpg.add_separator()

                    dpg.add_text("Data Type:")
                    self.safe_add_item(dpg.add_combo,
                                       ["SymbolUpdate", "DepthUpdate"],
                                       default_value=self.current_data_type,
                                       tag=self.get_tag("stream_data_type"),
                                       width=-1)

                    dpg.add_spacer(height=10)
                    dpg.add_text("Stock Symbols:")
                    self.safe_add_item(dpg.add_input_text,
                                       hint="NSE:SBIN-EQ,NSE:ADANIENT-EQ",
                                       default_value=",".join(self.current_symbols),
                                       tag=self.get_tag("stream_symbols"),
                                       width=-1,
                                       multiline=True,
                                       height=80)

                    dpg.add_spacer(height=10)
                    dpg.add_button(label=" Update Subscription",
                                   callback=self.update_subscription, width=-1)

                    dpg.add_spacer(height=10)
                    dpg.add_text("Quick Symbols:")
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="NIFTY50", callback=lambda: self.set_quick_symbols("nifty50"), width=60)
                        dpg.add_button(label="BANKNIFTY", callback=lambda: self.set_quick_symbols("banknifty"),
                                       width=80)

                # Streaming stats
                with self.create_child_window("streaming_stats", width=240, height=280):
                    dpg.add_text("Streaming Statistics")
                    dpg.add_separator()

                    self.safe_add_item(dpg.add_text,
                                       "Session Time: 00:00:00",
                                       tag=self.get_tag("session_time"))

                    self.safe_add_item(dpg.add_text,
                                       "Data Points: 0",
                                       tag=self.get_tag("data_points_count"))

                    self.safe_add_item(dpg.add_text,
                                       "Last Update: Never",
                                       tag=self.get_tag("last_update_time"))

                    self.safe_add_item(dpg.add_text,
                                       "Data Rate: 0 msg/sec",
                                       tag=self.get_tag("data_rate"))

                    dpg.add_spacer(height=10)
                    dpg.add_text("Max Display Rows:")
                    self.safe_add_item(dpg.add_combo,
                                       [100, 500, 1000, 2000],
                                       default_value=1000,
                                       tag=self.get_tag("max_display_rows"),
                                       callback=self.on_max_rows_changed,
                                       width=-1)

                    dpg.add_spacer(height=10)
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="Clear", callback=self.clear_streaming_data, width=60)
                        dpg.add_button(label="Stats", callback=self.show_detailed_stats, width=60)

    def create_data_viewer(self):
        """Create real-time data viewer"""
        with dpg.collapsing_header(label="Live Data Feed", default_open=True):
            with dpg.group(horizontal=True):
                # Controls
                with dpg.group():
                    with dpg.group(horizontal=True):
                        self.safe_add_item(dpg.add_button,
                                           label="Pause",
                                           tag=self.get_tag("pause_button"),
                                           callback=self.toggle_pause,
                                           width=80)

                        dpg.add_button(label="Export", callback=self.export_data, width=80)
                        dpg.add_button(label="Refresh", callback=self.force_refresh_table, width=80)
                        dpg.add_text("Auto-scroll:")
                        self.safe_add_item(dpg.add_checkbox,
                                           tag=self.get_tag("auto_scroll"),
                                           default_value=True)

                    with dpg.group(horizontal=True):
                        dpg.add_text("Filter Symbol:")
                        self.safe_add_item(dpg.add_input_text,
                                           tag=self.get_tag("symbol_filter"),
                                           width=120,
                                           callback=self.on_symbol_filter_changed)

                        dpg.add_text("Update Rate:")
                        self.safe_add_item(dpg.add_combo,
                                           ["Real-time", "1 sec", "2 sec", "5 sec"],
                                           default_value="Real-time",
                                           tag=self.get_tag("update_rate"),
                                           callback=self.on_update_rate_changed,
                                           width=100)

            dpg.add_spacer(height=5)

            # Data table container
            with self.create_child_window("live_data_viewer", width=-1, height=450):
                self.safe_add_item(dpg.add_text,
                                   "Connect to WebSocket to see live data...",
                                   tag=self.get_tag("data_viewer_status"))

                # Dynamic table will be created here
                with dpg.group(tag=self.get_tag("live_data_table_container")):
                    pass

    # Authentication methods with enhanced error handling
    def load_access_token_on_startup(self):
        """Load access token on startup if available"""
        with operation("load_token_on_startup"):
            try:
                token_path = self.config_dir / "access_token.log"
                if token_path.exists():
                    with open(token_path, "r", encoding='utf-8') as f:
                        tokens = [line.strip() for line in f if line.strip()]
                    if tokens:
                        self.access_token = tokens[-1]
                        info("Access token loaded on startup", context={'token_file': str(token_path)})
            except Exception as e:
                warning("Could not load token on startup", context={'error': str(e)})

    @monitor_performance
    def save_credentials(self):
        """Save credentials to config file"""
        with operation("save_credentials"):
            try:
                config = {
                    'client_id': dpg.get_value(self.get_tag("fyers_client_id")),
                    'app_id': dpg.get_value(self.get_tag("fyers_app_id")),
                    'app_type': dpg.get_value(self.get_tag("fyers_app_type")),
                    'redirect_uri': self.credentials['redirect_uri']
                }

                config_path = self.config_dir / "fyers_config.json"
                with open(config_path, "w", encoding='utf-8') as f:
                    json.dump(config, f, indent=2)

                self.update_auth_log("Credentials saved to fyers_config.json")
                info("Fyers credentials saved", context={'config_path': str(config_path)})

            except Exception as e:
                error_msg = f"Error saving credentials: {str(e)}"
                self.update_auth_log(error_msg)
                error("Failed to save credentials", context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def generate_access_token(self):
        """Generate new access token using Fyers authentication flow"""

        def auth_thread():
            with operation("generate_access_token"):
                try:
                    with self._lock:
                        self.update_auth_log(" Starting authentication process...")

                        # Get credentials from UI
                        credentials = {
                            'client_id': dpg.get_value(self.get_tag("fyers_client_id")),
                            'pin': dpg.get_value(self.get_tag("fyers_pin")),
                            'app_id': dpg.get_value(self.get_tag("fyers_app_id")),
                            'app_type': dpg.get_value(self.get_tag("fyers_app_type")),
                            'app_secret': dpg.get_value(self.get_tag("fyers_app_secret")),
                            'totp_secret': dpg.get_value(self.get_tag("fyers_totp_secret"))
                        }

                        # Validate inputs
                        missing_fields = [k for k, v in credentials.items() if not v.strip()]
                        if missing_fields:
                            self.update_auth_log(f" Missing fields: {', '.join(missing_fields)}")
                            return

                        # Authentication flow
                        request_key = None
                        totp = None

                        # Step 1: Verify client ID
                        status, request_key = self.verify_client_id(credentials['client_id'])
                        if status != 1:
                            self.update_auth_log(f"Client ID verification failed: {request_key}")
                            return
                        self.update_auth_log(" Client ID verified")

                        # Step 2: Generate TOTP
                        status, totp = self.generate_totp(credentials['totp_secret'])
                        if status != 1:
                            self.update_auth_log(f"TOTP generation failed: {totp}")
                            return
                        self.update_auth_log(" TOTP generated")

                        # Step 3: Verify TOTP
                        status, request_key = self.verify_totp(request_key, totp)
                        if status != 1:
                            self.update_auth_log(f"TOTP verification failed: {request_key}")
                            return
                        self.update_auth_log("TOTP verified")

                        # Step 4: Verify PIN
                        status, fyers_access_token = self.verify_pin(request_key, credentials['pin'])
                        if status != 1:
                            self.update_auth_log(f"PIN verification failed: {fyers_access_token}")
                            return
                        self.update_auth_log("PIN verified")

                        # Step 5: Get auth code
                        status, auth_code = self.get_token(
                            credentials['client_id'], credentials['app_id'],
                            self.credentials['redirect_uri'], credentials['app_type'],
                            fyers_access_token
                        )
                        if status != 1:
                            self.update_auth_log(f"Token generation failed: {auth_code}")
                            return
                        self.update_auth_log("Auth code received")

                        # Step 6: Validate auth code
                        status, v3_access = self.validate_authcode(
                            auth_code, credentials['app_id'],
                            credentials['app_type'], credentials['app_secret']
                        )
                        if status != 1:
                            self.update_auth_log(f"Auth code validation failed: {v3_access}")
                            return
                        self.update_auth_log("Access token validated")

                        # Build final token
                        self.access_token = f"{credentials['app_id']}-{credentials['app_type']}:{v3_access}"

                        # Save token to file
                        self.save_access_token()

                        # Update UI
                        self.update_auth_status()
                        self.update_auth_log(" Authentication completed successfully!")
                        info("Fyers authentication completed successfully")

                except Exception as e:
                    error_msg = f"Authentication error: {str(e)}"
                    self.update_auth_log(error_msg)
                    error("Authentication failed", context={'error': str(e)}, exc_info=True)

        threading.Thread(target=auth_thread, daemon=True).start()

    def load_access_token(self):
        """Load access token from file"""
        with operation("load_access_token"):
            try:
                token_path = self.config_dir / "access_token.log"
                if not token_path.exists():
                    self.update_auth_log("access_token.log file not found")
                    return

                with open(token_path, "r", encoding='utf-8') as f:
                    tokens = [line.strip() for line in f if line.strip()]

                if not tokens:
                    self.update_auth_log("No tokens found in access_token.log")
                    return

                with self._lock:
                    self.access_token = tokens[-1]

                self.update_auth_status()
                self.update_auth_log("Access token loaded from file")
                info("Access token loaded from file", context={'token_file': str(token_path)})

            except Exception as e:
                error_msg = f" Error loading token: {str(e)}"
                self.update_auth_log(error_msg)
                error("Failed to load token", context={'error': str(e)}, exc_info=True)

    def save_access_token(self):
        """Save access token to file"""
        try:
            token_path = self.config_dir / "access_token.log"
            with open(token_path, "a", encoding='utf-8') as f:
                f.write(f"{self.access_token}\n")
            self.update_auth_log(" Token saved to access_token.log")
            info("Token saved", context={'token_file': str(token_path)})
        except Exception as e:
            error_msg = f" Error saving token: {str(e)}"
            self.update_auth_log(error_msg)
            error("Failed to save token", context={'error': str(e)}, exc_info=True)

    # Fyers API methods with enhanced error handling
    def verify_client_id(self, client_id: str) -> Tuple[int, str]:
        """Verify client ID with Fyers API"""
        try:
            payload = {"fy_id": client_id, "app_id": "2"}
            resp = requests.post(url=f"{self.BASE_URL}/send_login_otp",
                                 json=payload, timeout=30)
            if resp.status_code != 200:
                return [-1, f"HTTP {resp.status_code}: {resp.text}"]
            data = resp.json()
            debug("Client ID verified successfully", context={'client_id': client_id})
            return [1, data["request_key"]]
        except requests.exceptions.Timeout:
            return [-1, "Request timeout"]
        except requests.exceptions.RequestException as e:
            return [-1, f"Network error: {str(e)}"]
        except Exception as e:
            return [-1, str(e)]

    def generate_totp(self, secret: str) -> Tuple[int, str]:
        """Generate TOTP code"""
        try:
            if not secret.strip():
                return [-1, "TOTP secret is empty"]
            totp = pyotp.TOTP(secret).now()
            debug("TOTP generated successfully")
            return [1, totp]
        except Exception as e:
            return [-1, f"TOTP generation error: {str(e)}"]

    def verify_totp(self, request_key: str, totp: str) -> Tuple[int, str]:
        """Verify TOTP with Fyers API"""
        try:
            payload = {"request_key": request_key, "otp": totp}
            resp = requests.post(url=f"{self.BASE_URL}/verify_otp",
                                 json=payload, timeout=30)
            if resp.status_code != 200:
                return [-1, f"HTTP {resp.status_code}: {resp.text}"]
            data = resp.json()
            debug("TOTP verified successfully")
            return [1, data["request_key"]]
        except requests.exceptions.Timeout:
            return [-1, "Request timeout"]
        except requests.exceptions.RequestException as e:
            return [-1, f"Network error: {str(e)}"]
        except Exception as e:
            return [-1, str(e)]

    def verify_pin(self, request_key: str, pin: str) -> Tuple[int, str]:
        """Verify PIN with Fyers API"""
        try:
            payload = {
                "request_key": request_key,
                "identity_type": "pin",
                "identifier": pin
            }
            resp = requests.post(url=f"{self.BASE_URL}/verify_pin",
                                 json=payload, timeout=30)
            if resp.status_code != 200:
                return [-1, f"HTTP {resp.status_code}: {resp.text}"]
            data = resp.json()
            debug("PIN verified successfully")
            return [1, data["data"]["access_token"]]
        except requests.exceptions.Timeout:
            return [-1, "Request timeout"]
        except requests.exceptions.RequestException as e:
            return [-1, f"Network error: {str(e)}"]
        except Exception as e:
            return [-1, str(e)]

    def get_token(self, client_id: str, app_id: str, redirect_uri: str,
                  app_type: str, access_token: str) -> Tuple[int, str]:
        """Get authorization token"""
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
            resp = requests.post(url=f"{self.BASE_URL_2}/token",
                                 json=payload, headers=headers, timeout=30)
            if resp.status_code != 308:
                return [-1, f"HTTP {resp.status_code}: {resp.text}"]
            data = resp.json()
            url = data["Url"]
            auth_code = parse.parse_qs(parse.urlparse(url).query)['auth_code'][0]
            debug("Authorization token received successfully")
            return [1, auth_code]
        except requests.exceptions.Timeout:
            return [-1, "Request timeout"]
        except requests.exceptions.RequestException as e:
            return [-1, f"Network error: {str(e)}"]
        except Exception as e:
            return [-1, str(e)]

    def validate_authcode(self, auth_code: str, app_id: str, app_type: str,
                          app_secret: str) -> Tuple[int, str]:
        """Validate authorization code"""
        try:
            app_id_hash = hashlib.sha256(f"{app_id}-{app_type}:{app_secret}".encode()).hexdigest()
            payload = {
                "grant_type": "authorization_code",
                "appIdHash": app_id_hash,
                "code": auth_code,
            }
            resp = requests.post(url=f"{self.BASE_URL_2}/validate-authcode",
                                 json=payload, timeout=30)
            if resp.status_code != 200:
                return [-1, f"HTTP {resp.status_code}: {resp.text}"]
            data = resp.json()
            debug("Authorization code validated successfully")
            return [1, data["access_token"]]
        except requests.exceptions.Timeout:
            return [-1, "Request timeout"]
        except requests.exceptions.RequestException as e:
            return [-1, f"Network error: {str(e)}"]
        except Exception as e:
            return [-1, str(e)]

    # WebSocket methods with enhanced error handling
    @monitor_performance
    def connect_websocket(self):
        """Connect to Fyers WebSocket with enhanced error handling"""
        if not self.access_token:
            self.update_auth_log(" No access token available. Generate token first.")
            return

        def connect_thread():
            with operation("connect_websocket"):
                try:
                    with self._lock:
                        if self.is_connected:
                            self.update_auth_log(" Already connected to WebSocket")
                            return

                    self.update_auth_log(" Connecting to WebSocket...")

                    # Create WebSocket client with enhanced configuration
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

                    # Set session start time
                    with self._lock:
                        self.session_start_time = datetime.datetime.now()
                        self.message_count = 0

                    # Connect with timeout handling
                    self.websocket_client.connect()
                    info("WebSocket connection initiated")

                except Exception as e:
                    error_msg = f" WebSocket connection failed: {str(e)}"
                    self.update_auth_log(error_msg)
                    error("WebSocket connection failed", context={'error': str(e)}, exc_info=True)

        threading.Thread(target=connect_thread, daemon=True).start()

    def disconnect_websocket(self):
        """Enhanced WebSocket disconnection"""
        with operation("disconnect_websocket"):
            try:
                self.update_auth_log(" Disconnecting WebSocket...")

                with self._lock:
                    # Set flags immediately
                    self.is_connected = False
                    self.is_paused = True

                # Try multiple disconnect approaches
                if self.websocket_client:
                    disconnect_methods = [
                        ('disconnect', lambda: self.websocket_client.disconnect()),
                        ('close', lambda: self.websocket_client.close()),
                        ('stop', lambda: self.websocket_client.stop()),
                    ]

                    for method_name, method_func in disconnect_methods:
                        try:
                            if hasattr(self.websocket_client, method_name):
                                method_func()
                                self.update_auth_log(f" Called {method_name}() method")
                        except Exception as e:
                            warning(f"Warning calling {method_name}: {e}")

                    # Force clear the client reference
                    self.websocket_client = None
                    self.update_auth_log(" WebSocket client reference cleared")

                # Update UI immediately
                self.safe_set_value(self.get_tag("ws_status_text"), "Status: Disconnected")
                self.safe_configure_item(self.get_tag("ws_status_text"), color=(255, 100, 100))
                self.safe_set_value(self.get_tag("ws_data_type"), "Data Type: None")
                self.safe_set_value(self.get_tag("ws_symbols"), "Symbols: None")
                self.safe_set_value(self.get_tag("ws_message_count"), "Messages Received: 0")
                self.safe_set_value(self.get_tag("pause_button"), " Paused")

                self.update_auth_log(" WebSocket disconnected successfully")
                info("WebSocket disconnected")

            except Exception as e:
                error_msg = f" Disconnect error: {str(e)}"
                self.update_auth_log(error_msg)
                error("WebSocket disconnect error", context={'error': str(e)}, exc_info=True)

                # Force update UI even if there's an error
                with self._lock:
                    self.is_connected = False
                    self.is_paused = True
                self.safe_set_value(self.get_tag("ws_status_text"), "Status: Force Disconnected")
                self.safe_configure_item(self.get_tag("ws_status_text"), color=(255, 100, 100))

    # WebSocket callbacks
    def on_websocket_open(self):
        """WebSocket open callback with enhanced setup"""
        try:
            with self._lock:
                self.is_connected = True
                self.is_paused = False
                self.session_start_time = datetime.datetime.now()

            self.safe_set_value(self.get_tag("ws_status_text"), "Status: Connected")
            self.safe_configure_item(self.get_tag("ws_status_text"), color=(100, 255, 100))
            self.safe_set_value(self.get_tag("pause_button"), " Pause")

            # Subscribe to symbols
            data_type = dpg.get_value(self.get_tag("stream_data_type"))
            symbols_text = dpg.get_value(self.get_tag("stream_symbols"))
            symbols = [s.strip() for s in symbols_text.split(",") if s.strip()]

            if symbols and self.websocket_client:
                self.websocket_client.subscribe(symbols=symbols, data_type=data_type)
                self.safe_set_value(self.get_tag("ws_data_type"), f"Data Type: {data_type}")
                self.safe_set_value(self.get_tag("ws_symbols"), f"Symbols: {', '.join(symbols)}")

                # Update current settings
                with self._lock:
                    self.current_symbols = symbols
                    self.current_data_type = data_type

                self.websocket_client.keep_running()

            self.update_auth_log(" WebSocket connected and subscribed")
            info("WebSocket connected successfully",
                 context={'symbols': len(symbols), 'data_type': data_type})

        except Exception as e:
            error_msg = f" WebSocket open callback error: {str(e)}"
            self.update_auth_log(error_msg)
            error("WebSocket open callback error", context={'error': str(e)}, exc_info=True)

    def on_websocket_close(self, code):
        """WebSocket close callback"""
        try:
            with self._lock:
                self.is_connected = False
                self.is_paused = True

            self.safe_set_value(self.get_tag("ws_status_text"), "Status: Disconnected")
            self.safe_configure_item(self.get_tag("ws_status_text"), color=(255, 100, 100))
            self.safe_set_value(self.get_tag("pause_button"), " Paused")

            error_msg = f" WebSocket closed (code: {code})"
            self.update_auth_log(error_msg)
            warning("WebSocket closed", context={'code': code})

        except Exception as e:
            error("Error in WebSocket close callback", context={'error': str(e)}, exc_info=True)

    def on_websocket_error(self, error):
        """WebSocket error callback"""
        error_msg = f" WebSocket error: {str(error)}"
        self.update_auth_log(error_msg)
        error("WebSocket error", context={'error': str(error)})

    def on_websocket_message(self, message):
        """Enhanced WebSocket message callback with filtering and throttling"""
        try:
            # Skip processing if paused or disconnected
            with self._lock:
                if self.is_paused or not self.is_connected:
                    return

                self.message_count += 1
                self.last_message_time = datetime.datetime.now()

            # Apply symbol filter if set
            symbol_filter = dpg.get_value(self.get_tag("symbol_filter"))
            if symbol_filter and symbol_filter.strip():
                message_symbol = message.get('symbol', '').upper()
                if symbol_filter.upper() not in message_symbol:
                    return

            timestamp = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]

            # Create data row with safe encoding handling
            data_row = {
                'timestamp': timestamp,
                'symbol': self.safe_encode_text(message.get('symbol', 'Unknown')),
                'type': self.safe_encode_text(message.get('type', 'Unknown')),
                'data': message
            }

            with self._lock:
                self.streaming_data.append(data_row)

                # Limit data size with safe conversion
                try:
                    max_rows = dpg.get_value(self.get_tag("max_display_rows"))
                    if isinstance(max_rows, str):
                        max_rows = int(max_rows)
                    elif max_rows is None:
                        max_rows = 1000
                except (ValueError, TypeError):
                    max_rows = 1000

                if len(self.streaming_data) > max_rows:
                    self.streaming_data = self.streaming_data[-max_rows:]

            # Update UI based on update rate setting
            should_update = self.should_update_ui()
            if should_update and dpg.get_value(self.get_tag("auto_scroll")) and not self.is_paused:
                self.update_streaming_stats()
                self.update_data_table()

        except Exception as e:
            error_msg = f" Message processing error: {str(e)}"
            self.update_auth_log(error_msg)
            error("Message processing error", context={'error': str(e)}, exc_info=True)

    def safe_encode_text(self, text: Any) -> str:
        """Safely encode text with proper handling"""
        try:
            if isinstance(text, bytes):
                return text.decode('utf-8', errors='ignore')
            elif isinstance(text, (int, float)):
                return str(text)
            elif text is None:
                return ""
            else:
                return str(text).encode('ascii', errors='ignore').decode('ascii')
        except Exception:
            return "N/A"

    def should_update_ui(self) -> bool:
        """Determine if UI should be updated based on throttling settings"""
        current_time = datetime.datetime.now()

        # Get update rate setting
        try:
            update_rate = dpg.get_value(self.get_tag("update_rate"))
            if update_rate == "Real-time":
                throttle_seconds = 0.1
            elif update_rate == "1 sec":
                throttle_seconds = 1.0
            elif update_rate == "2 sec":
                throttle_seconds = 2.0
            elif update_rate == "5 sec":
                throttle_seconds = 5.0
            else:
                throttle_seconds = 0.5
        except:
            throttle_seconds = 0.5

        if self._last_table_update is None:
            self._last_table_update = current_time
            return True

        time_diff = (current_time - self._last_table_update).total_seconds()
        if time_diff >= throttle_seconds:
            self._last_table_update = current_time
            return True

        return False

    # UI update methods with enhanced error handling
    def update_auth_status(self):
        """Update authentication status in UI with safe operations"""
        try:
            if self.access_token:
                self.safe_set_value(self.get_tag("auth_status_text"), "Status: Authenticated")
                self.safe_configure_item(self.get_tag("auth_status_text"), color=(100, 255, 100))

                token_display = f"Token: {self.access_token[:20]}..." if len(
                    self.access_token) > 20 else f"Token: {self.access_token}"
                self.safe_set_value(self.get_tag("token_status"), token_display)

                self.safe_set_value(self.get_tag("token_generated_time"),
                                    f"Generated: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
                self.safe_set_value(self.get_tag("token_validity"), "Valid Until: End of trading day")

            # Check token file
            token_file_path = self.config_dir / "access_token.log"
            token_file_status = f"access_token.log: Found" if token_file_path.exists() else f"access_token.log: Not Found"
            self.safe_set_value(self.get_tag("token_file_status"), token_file_status)

        except Exception as e:
            error("Error updating auth status", context={'error': str(e)}, exc_info=True)

    def update_streaming_stats(self):
        """Update streaming statistics with enhanced calculations"""
        try:
            with self._lock:
                data_count = len(self.streaming_data)
                current_time = datetime.datetime.now()

                # Update data points count
                self.safe_set_value(self.get_tag("data_points_count"), f"Data Points: {data_count}")

                # Update last update time
                self.safe_set_value(self.get_tag("last_update_time"),
                                    f"Last Update: {current_time.strftime('%H:%M:%S')}")

                # Calculate session time
                if self.session_start_time:
                    session_duration = current_time - self.session_start_time
                    hours, remainder = divmod(session_duration.total_seconds(), 3600)
                    minutes, seconds = divmod(remainder, 60)
                    session_time_str = f"Session Time: {int(hours):02d}:{int(minutes):02d}:{int(seconds):02d}"
                    self.safe_set_value(self.get_tag("session_time"), session_time_str)

                # Calculate data rate
                if self.session_start_time and self.message_count > 0:
                    elapsed_seconds = (current_time - self.session_start_time).total_seconds()
                    if elapsed_seconds > 0:
                        rate = self.message_count / elapsed_seconds
                        self.safe_set_value(self.get_tag("data_rate"), f"Data Rate: {rate:.1f} msg/sec")

                # Update message count
                self.safe_set_value(self.get_tag("ws_message_count"), f"Messages Received: {self.message_count}")

        except Exception as e:
            error("Error updating streaming stats", context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def update_data_table(self):
        """Enhanced data table update with better performance and error handling"""
        with operation("update_data_table"):
            try:
                # Only update if conditions are met
                if not dpg.get_value(self.get_tag("auto_scroll")) or self.is_paused:
                    return

                with self._lock:
                    if not self.streaming_data:
                        return

                    # Get recent data for performance (last 100 rows)
                    recent_data = self.streaming_data[-100:] if len(self.streaming_data) > 100 else self.streaming_data

                if not recent_data:
                    return

                # Clear existing table
                container_tag = self.get_tag("live_data_table_container")
                if dpg.does_item_exist(container_tag):
                    dpg.delete_item(container_tag, children_only=True)

                    # Collect all unique keys from messages for dynamic columns
                    all_keys = set()
                    for row in recent_data[-10:]:  # Sample last 10 for keys
                        if isinstance(row['data'], dict):
                            all_keys.update(row['data'].keys())

                    # Sort keys for consistent column order
                    sorted_keys = sorted(list(all_keys))

                    # Create new table with dynamic columns
                    table_tag = self.get_tag("live_data_table")
                    if dpg.does_item_exist(table_tag):
                        dpg.delete_item(table_tag)

                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                                   borders_innerV=True, borders_outerV=True,
                                   parent=container_tag, scrollY=True, scrollX=True, height=400,
                                   tag=table_tag):

                        # Add fixed columns
                        dpg.add_table_column(label="Time", width_fixed=True, init_width_or_weight=80)
                        dpg.add_table_column(label="Symbol", width_fixed=True, init_width_or_weight=120)
                        dpg.add_table_column(label="Type", width_fixed=True, init_width_or_weight=60)

                        # Add dynamic columns for important fields first
                        priority_fields = ['ltp', 'volume', 'bid_price1', 'ask_price1', 'high_price', 'low_price']
                        added_fields = set()

                        for field in priority_fields:
                            if field in sorted_keys:
                                dpg.add_table_column(label=field, width_fixed=True, init_width_or_weight=100)
                                added_fields.add(field)

                        # Add remaining fields
                        for key in sorted_keys:
                            if key not in ['symbol', 'type'] and key not in added_fields:
                                dpg.add_table_column(label=key, width_fixed=True, init_width_or_weight=100)
                                added_fields.add(key)

                        # Add data rows (show newest first, limit to 50 for performance)
                        display_data = list(reversed(recent_data[-50:]))

                        for row in display_data:
                            with dpg.table_row():
                                # Fixed columns
                                dpg.add_text(row['timestamp'])
                                dpg.add_text(row['symbol'])
                                dpg.add_text(row['type'])

                                # Dynamic columns
                                if isinstance(row['data'], dict):
                                    # Priority fields first
                                    for field in priority_fields:
                                        if field in added_fields:
                                            self.add_table_cell(row, field)

                                    # Remaining fields
                                    for key in sorted_keys:
                                        if key not in ['symbol', 'type'] and key not in priority_fields:
                                            if key in added_fields:
                                                self.add_table_cell(row, key)
                                else:
                                    # Fill with empty values if data is not a dict
                                    for _ in added_fields:
                                        dpg.add_text("")

            except Exception as e:
                error("Error updating data table", context={'error': str(e)}, exc_info=True)
                # Try to show error in table
                try:
                    container_tag = self.get_tag("live_data_table_container")
                    if dpg.does_item_exist(container_tag):
                        dpg.delete_item(container_tag, children_only=True)
                        dpg.add_text(f"Table update error: {str(e)}", parent=container_tag)
                except:
                    pass

    def add_table_cell(self, row: Dict[str, Any], field: str):
        """Add a table cell with proper formatting and color coding"""
        try:
            value = row['data'].get(field, '')

            if value is None:
                dpg.add_text('NULL')
            else:
                try:
                    # Format numbers and apply colors
                    if isinstance(value, float):
                        display_value = f"{value:.2f}"
                        color = self.get_price_color(row['symbol'], field, value)
                    elif isinstance(value, int):
                        display_value = f"{value:,}"
                        color = self.get_price_color(row['symbol'], field, value)
                    else:
                        display_value = str(value)
                        color = None

                    # Add text with color if applicable
                    if color:
                        dpg.add_text(display_value, color=color)
                    else:
                        dpg.add_text(display_value)

                except Exception:
                    dpg.add_text(str(value))
        except Exception:
            dpg.add_text("")

    def update_auth_log(self, message: str):
        """Update authentication log with safe operations"""
        try:
            log_tag = self.get_tag("auth_log_text")
            if dpg.does_item_exist(log_tag):
                current_log = dpg.get_value(log_tag)
                timestamp = datetime.datetime.now().strftime('%H:%M:%S')
                new_message = f"[{timestamp}] {message}"
                new_log = f"{new_message}\n{current_log}"

                # Keep only last 15 lines for better performance
                lines = new_log.split('\n')[:15]
                dpg.set_value(log_tag, '\n'.join(lines))
        except Exception as e:
            warning(f"Error updating auth log: {e}")

    def get_price_color(self, symbol: str, field: str, current_value: Any) -> Optional[Tuple[int, int, int]]:
        """Get color for price fields based on movement with enhanced field detection"""
        # Expanded list of price-related fields
        price_fields = {
            'ltp', 'ask_price1', 'ask_price2', 'ask_price3', 'ask_price4', 'ask_price5',
            'bid_price1', 'bid_price2', 'bid_price3', 'bid_price4', 'bid_price5',
            'high_price', 'low_price', 'open_price', 'prev_close_price', 'avg_trade_price',
            'last_traded_price', 'close_price', 'price'
        }

        if field not in price_fields or not isinstance(current_value, (int, float)):
            return None

        # Create unique key for symbol+field combination
        key = f"{symbol}_{field}"

        # Get previous value
        with self._lock:
            previous_value = self.previous_prices.get(key)
            # Update stored value
            self.previous_prices[key] = current_value

        # If no previous value, return default color
        if previous_value is None:
            return None

        # Compare and return color
        try:
            if current_value > previous_value:
                return (100, 255, 100)  # Green for price increase
            elif current_value < previous_value:
                return (255, 100, 100)  # Red for price decrease
            else:
                return None  # Default color for no change
        except:
            return None

    # Control callbacks with enhanced functionality
    def update_subscription(self):
        """Enhanced WebSocket subscription update"""
        if not self.is_connected or not self.websocket_client:
            self.update_auth_log(" Not connected to WebSocket")
            return

        with operation("update_subscription"):
            try:
                # Get new settings from UI
                data_type = dpg.get_value(self.get_tag("stream_data_type"))
                symbols_text = dpg.get_value(self.get_tag("stream_symbols"))
                symbols = [s.strip().upper() for s in symbols_text.split(",") if s.strip()]

                if not symbols:
                    self.update_auth_log(" No symbols provided")
                    return

                self.update_auth_log(f" Updating subscription to {len(symbols)} symbols...")

                # Unsubscribe from current symbols first
                with self._lock:
                    if hasattr(self.websocket_client, 'unsubscribe') and self.current_symbols:
                        try:
                            self.websocket_client.unsubscribe(symbols=self.current_symbols,
                                                              data_type=self.current_data_type)
                            self.update_auth_log(" Unsubscribed from previous symbols")
                        except Exception as e:
                            self.update_auth_log(f" Unsubscribe warning: {str(e)}")

                    # Subscribe to new symbols
                    self.websocket_client.subscribe(symbols=symbols, data_type=data_type)

                    # Update stored settings
                    self.current_symbols = symbols
                    self.current_data_type = data_type

                # Update UI
                self.safe_set_value(self.get_tag("ws_data_type"), f"Data Type: {data_type}")
                self.safe_set_value(self.get_tag("ws_symbols"), f"Symbols: {', '.join(symbols)}")

                self.update_auth_log(f" Subscription updated: {data_type} for {len(symbols)} symbols")
                info("Subscription updated",
                     context={'symbols_count': len(symbols), 'data_type': data_type})

            except Exception as e:
                error_msg = f" Subscription update failed: {str(e)}"
                self.update_auth_log(error_msg)
                error("Subscription update failed", context={'error': str(e)}, exc_info=True)

    def set_quick_symbols(self, symbol_set: str):
        """Set predefined symbol sets for quick access"""
        try:
            if symbol_set == "nifty50":
                symbols = [
                    "NSE:RELIANCE-EQ", "NSE:TCS-EQ", "NSE:HDFCBANK-EQ", "NSE:INFY-EQ", "NSE:HINDUNILVR-EQ",
                    "NSE:ICICIBANK-EQ", "NSE:KOTAKBANK-EQ", "NSE:SBIN-EQ", "NSE:BHARTIARTL-EQ", "NSE:ITC-EQ"
                ]
            elif symbol_set == "banknifty":
                symbols = [
                    "NSE:HDFCBANK-EQ", "NSE:ICICIBANK-EQ", "NSE:KOTAKBANK-EQ", "NSE:SBIN-EQ",
                    "NSE:AXISBANK-EQ", "NSE:INDUSINDBK-EQ", "NSE:PNB-EQ", "NSE:BANKBARODA-EQ"
                ]
            else:
                symbols = self.current_symbols

            symbols_text = ",".join(symbols)
            self.safe_set_value(self.get_tag("stream_symbols"), symbols_text)
            self.update_auth_log(f" Set {symbol_set.upper()} symbols")
            info("Quick symbols set", context={'symbol_set': symbol_set, 'count': len(symbols)})

        except Exception as e:
            error_msg = f" Error setting quick symbols: {str(e)}"
            self.update_auth_log(error_msg)
            error("Failed to set quick symbols", context={'error': str(e)}, exc_info=True)

    def on_max_rows_changed(self, sender, app_data):
        """Handle max rows change with safe conversion"""
        try:
            if isinstance(app_data, str):
                max_rows = int(app_data)
            else:
                max_rows = app_data

            with self._lock:
                self.max_streaming_rows = max_rows

                # Trim data if needed
                if len(self.streaming_data) > max_rows:
                    self.streaming_data = self.streaming_data[-max_rows:]

            self.update_data_table()
            info("Max rows changed", context={'max_rows': max_rows})

        except (ValueError, TypeError):
            self.max_streaming_rows = 1000
            warning("Invalid max rows value, using default 1000")

    def on_symbol_filter_changed(self, sender, app_data):
        """Handle symbol filter changes"""
        try:
            filter_value = app_data.strip().upper() if app_data else ""
            if filter_value:
                self.update_auth_log(f" Symbol filter set to: {filter_value}")
            else:
                self.update_auth_log(" Symbol filter cleared")

            # Force table refresh if auto-scroll is enabled
            if dpg.get_value(self.get_tag("auto_scroll")):
                self.update_data_table()

            debug("Symbol filter changed", context={'filter': filter_value})

        except Exception as e:
            error("Error in symbol filter change", context={'error': str(e)}, exc_info=True)

    def on_update_rate_changed(self, sender, app_data):
        """Handle update rate changes"""
        try:
            self.update_auth_log(f" Update rate changed to: {app_data}")
            info("Update rate changed", context={'rate': app_data})
        except Exception as e:
            error("Error in update rate change", context={'error': str(e)}, exc_info=True)

    def clear_streaming_data(self):
        """Clear all streaming data with enhanced cleanup"""
        with operation("clear_streaming_data"):
            try:
                with self._lock:
                    self.streaming_data.clear()
                    self.previous_prices.clear()
                    self.message_count = 0

                # Clear table
                container_tag = self.get_tag("live_data_table_container")
                if dpg.does_item_exist(container_tag):
                    dpg.delete_item(container_tag, children_only=True)
                    dpg.add_text("Data cleared. Waiting for new messages...", parent=container_tag)

                self.update_auth_log(" All streaming data cleared")
                info("Streaming data cleared")

            except Exception as e:
                error_msg = f" Error clearing data: {str(e)}"
                self.update_auth_log(error_msg)
                error("Failed to clear streaming data", context={'error': str(e)}, exc_info=True)

    def toggle_pause(self):
        """Enhanced pause/resume functionality"""
        try:
            with self._lock:
                self.is_paused = not self.is_paused

            if self.is_paused:
                self.safe_set_value(self.get_tag("pause_button"), " Resume")
                self.update_auth_log(" Data streaming paused")
            else:
                self.safe_set_value(self.get_tag("pause_button"), " Pause")
                self.update_auth_log(" Data streaming resumed")
                # Force table update when resuming
                if self.streaming_data:
                    self.update_data_table()

            info(f"Streaming {'paused' if self.is_paused else 'resumed'}")

        except Exception as e:
            error_msg = f" Error toggling pause: {str(e)}"
            self.update_auth_log(error_msg)
            error("Failed to toggle pause", context={'error': str(e)}, exc_info=True)

    def force_refresh_table(self):
        """Force refresh the data table"""
        try:
            self.update_data_table()
            self.update_auth_log(" Table refreshed manually")
            info("Table manually refreshed")
        except Exception as e:
            error_msg = f" Error refreshing table: {str(e)}"
            self.update_auth_log(error_msg)
            error("Failed to refresh table", context={'error': str(e)}, exc_info=True)

    def show_detailed_stats(self):
        """Show detailed streaming statistics"""
        try:
            with self._lock:
                total_messages = self.message_count
                data_count = len(self.streaming_data)
                unique_symbols = len(set(row['symbol'] for row in self.streaming_data))

            stats_message = f""" Detailed Statistics:
• Total Messages: {total_messages:,}
• Data Points Stored: {data_count:,}
• Unique Symbols: {unique_symbols}
• Memory Usage: ~{len(str(self.streaming_data)) / 1024:.1f} KB"""

            self.update_auth_log(stats_message)
            info("Detailed stats shown",
                 context={'messages': total_messages, 'data_points': data_count, 'symbols': unique_symbols})

        except Exception as e:
            error_msg = f" Error showing stats: {str(e)}"
            self.update_auth_log(error_msg)
            error("Failed to show stats", context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def export_data(self):
        """Enhanced export functionality with multiple formats"""
        with operation("export_data"):
            try:
                with self._lock:
                    if not self.streaming_data:
                        self.update_auth_log(" No data to export")
                        return

                    data_to_export = self.streaming_data.copy()

                timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')

                # Export to CSV
                csv_filename = f"fyers_stream_{timestamp}.csv"
                csv_path = self.config_dir / csv_filename
                self.export_to_csv(data_to_export, csv_path)

                # Export to JSON for detailed analysis
                json_filename = f"fyers_stream_{timestamp}.json"
                json_path = self.config_dir / json_filename
                self.export_to_json(data_to_export, json_path)

                export_msg = f" Exported {len(data_to_export)} records to {csv_filename} and {json_filename}"
                self.update_auth_log(export_msg)
                info("Data exported successfully",
                     context={'records': len(data_to_export), 'csv_file': str(csv_path), 'json_file': str(json_path)})

            except Exception as e:
                error_msg = f" Export failed: {str(e)}"
                self.update_auth_log(error_msg)
                error("Data export failed", context={'error': str(e)}, exc_info=True)

    def export_to_csv(self, data: List[Dict], filepath: Path):
        """Export data to CSV format"""
        try:
            # Collect all unique keys from all messages
            all_keys = set(['timestamp', 'symbol', 'type'])
            for row in data:
                if isinstance(row['data'], dict):
                    all_keys.update(row['data'].keys())

            sorted_keys = sorted(list(all_keys))

            with open(filepath, 'w', newline='', encoding='utf-8') as csvfile:
                writer = csv.DictWriter(csvfile, fieldnames=sorted_keys)
                writer.writeheader()

                for row in data:
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

            debug("CSV export completed", context={'filepath': str(filepath), 'rows': len(data)})

        except Exception as e:
            raise Exception(f"CSV export error: {str(e)}")

    def export_to_json(self, data: List[Dict], filepath: Path):
        """Export data to JSON format"""
        try:
            export_data = {
                'metadata': {
                    'export_time': datetime.datetime.now().isoformat(),
                    'total_records': len(data),
                    'symbols': list(set(row['symbol'] for row in data)),
                    'data_types': list(set(row['type'] for row in data)),
                    'time_range': {
                        'start': data[0]['timestamp'] if data else None,
                        'end': data[-1]['timestamp'] if data else None
                    }
                },
                'data': data
            }

            with open(filepath, 'w', encoding='utf-8') as jsonfile:
                json.dump(export_data, jsonfile, indent=2, ensure_ascii=False)

            debug("JSON export completed", context={'filepath': str(filepath), 'records': len(data)})

        except Exception as e:
            raise Exception(f"JSON export error: {str(e)}")

    def get_connection_health(self) -> Dict[str, Any]:
        """Get current connection health status"""
        try:
            with self._lock:
                current_time = datetime.datetime.now()

                health_status = {
                    'is_connected': self.is_connected,
                    'is_paused': self.is_paused,
                    'has_token': bool(self.access_token),
                    'session_duration': None,
                    'message_count': self.message_count,
                    'data_points': len(self.streaming_data),
                    'last_message': None,
                    'symbols_count': len(self.current_symbols),
                    'websocket_client': self.websocket_client is not None
                }

                if self.session_start_time:
                    duration = current_time - self.session_start_time
                    health_status['session_duration'] = duration.total_seconds()

                if self.last_message_time:
                    time_since_last = current_time - self.last_message_time
                    health_status['last_message'] = time_since_last.total_seconds()

                return health_status

        except Exception as e:
            error("Error getting connection health", context={'error': str(e)}, exc_info=True)
            return {'error': str(e)}

    def reconnect_websocket(self):
        """Reconnect WebSocket with enhanced logic"""
        with operation("reconnect_websocket"):
            try:
                self.update_auth_log(" Attempting to reconnect WebSocket...")

                # Disconnect first if connected
                if self.is_connected:
                    self.disconnect_websocket()
                    time.sleep(2)  # Wait before reconnecting

                # Reconnect
                self.connect_websocket()
                info("WebSocket reconnection attempted")

            except Exception as e:
                error_msg = f" Reconnection failed: {str(e)}"
                self.update_auth_log(error_msg)
                error("WebSocket reconnection failed", context={'error': str(e)}, exc_info=True)

    def validate_symbols(self, symbols: List[str]) -> Tuple[List[str], List[str]]:
        """Validate symbol format and return valid/invalid lists"""
        valid_symbols = []
        invalid_symbols = []

        for symbol in symbols:
            symbol = symbol.strip().upper()
            if not symbol:
                continue

            # Basic validation for Indian market symbols
            if ':' in symbol and '-EQ' in symbol:
                parts = symbol.split(':')
                if len(parts) == 2 and parts[0] in ['NSE', 'BSE']:
                    valid_symbols.append(symbol)
                else:
                    invalid_symbols.append(symbol)
            else:
                invalid_symbols.append(symbol)

        return valid_symbols, invalid_symbols

    def auto_save_config(self):
        """Auto-save current configuration"""
        try:
            config = {
                'last_symbols': self.current_symbols,
                'last_data_type': self.current_data_type,
                'max_rows': self.max_streaming_rows,
                'auto_scroll': dpg.get_value(self.get_tag("auto_scroll")) if dpg.does_item_exist(
                    self.get_tag("auto_scroll")) else True,
                'update_rate': dpg.get_value(self.get_tag("update_rate")) if dpg.does_item_exist(
                    self.get_tag("update_rate")) else "Real-time"
            }

            config_path = self.config_dir / "fyers_session_config.json"
            with open(config_path, "w", encoding='utf-8') as f:
                json.dump(config, f, indent=2)

            info("Session configuration auto-saved", context={'config_path': str(config_path)})

        except Exception as e:
            warning("Could not auto-save config", context={'error': str(e)})

    def load_session_config(self):
        """Load saved session configuration"""
        try:
            config_path = self.config_dir / "fyers_session_config.json"
            if config_path.exists():
                with open(config_path, "r", encoding='utf-8') as f:
                    config = json.load(f)

                # Apply saved settings
                if 'last_symbols' in config:
                    self.current_symbols = config['last_symbols']

                if 'last_data_type' in config:
                    self.current_data_type = config['last_data_type']

                if 'max_rows' in config:
                    self.max_streaming_rows = config['max_rows']

                info("Session configuration loaded", context={'config_path': str(config_path)})
                return True

        except Exception as e:
            warning("Could not load session config", context={'error': str(e)})

        return False

    @monitor_performance
    def cleanup(self):
        """Enhanced cleanup with auto-save"""
        with operation("fyers_tab_cleanup"):
            try:
                # Auto-save configuration before cleanup
                self.auto_save_config()

                # Disconnect WebSocket
                if self.websocket_client:
                    try:
                        self.disconnect_websocket()
                    except Exception as e:
                        warning("Warning during WebSocket cleanup", context={'error': str(e)})

                # Clear data structures
                with self._lock:
                    self.streaming_data.clear()
                    self.previous_prices.clear()

                # Clean up UI items
                self.cleanup_existing_items()

                info("Fyers tab cleaned up successfully")

            except Exception as e:
                error("Error during cleanup", context={'error': str(e)}, exc_info=True)

    def get_performance_metrics(self) -> Dict[str, Any]:
        """Get performance metrics for monitoring"""
        try:
            with self._lock:
                current_time = datetime.datetime.now()

                metrics = {
                    'memory_usage_kb': len(str(self.streaming_data)) / 1024,
                    'total_messages': self.message_count,
                    'stored_data_points': len(self.streaming_data),
                    'unique_symbols': len(set(row['symbol'] for row in self.streaming_data)),
                    'avg_message_size': len(str(self.streaming_data)) / max(len(self.streaming_data), 1),
                    'is_healthy': self.is_connected and not self.is_paused,
                    'uptime_seconds': None
                }

                if self.session_start_time:
                    uptime = current_time - self.session_start_time
                    metrics['uptime_seconds'] = uptime.total_seconds()

                    if uptime.total_seconds() > 0:
                        metrics['messages_per_second'] = self.message_count / uptime.total_seconds()

                return metrics

        except Exception as e:
            error("Error getting performance metrics", context={'error': str(e)}, exc_info=True)
            return {'error': str(e)}

    # Additional utility methods
    def format_number(self, value: Any) -> str:
        """Format numbers for display"""
        try:
            if isinstance(value, float):
                if value >= 1_000_000:
                    return f"{value / 1_000_000:.2f}M"
                elif value >= 1_000:
                    return f"{value / 1_000:.2f}K"
                else:
                    return f"{value:.2f}"
            elif isinstance(value, int):
                if value >= 1_000_000:
                    return f"{value / 1_000_000:.1f}M"
                elif value >= 1_000:
                    return f"{value / 1_000:.1f}K"
                else:
                    return f"{value:,}"
            else:
                return str(value)
        except:
            return str(value)

    def get_symbol_stats(self, symbol: str) -> Dict[str, Any]:
        """Get statistics for a specific symbol"""
        try:
            with self._lock:
                symbol_data = [row for row in self.streaming_data if row['symbol'] == symbol]

            if not symbol_data:
                return {'error': 'No data found for symbol'}

            stats = {
                'total_updates': len(symbol_data),
                'first_seen': symbol_data[0]['timestamp'],
                'last_seen': symbol_data[-1]['timestamp'],
                'data_types': list(set(row['type'] for row in symbol_data))
            }

            # Calculate price statistics if available
            prices = []
            for row in symbol_data:
                if isinstance(row['data'], dict) and 'ltp' in row['data']:
                    try:
                        prices.append(float(row['data']['ltp']))
                    except:
                        continue

            if prices:
                stats.update({
                    'price_high': max(prices),
                    'price_low': min(prices),
                    'price_avg': sum(prices) / len(prices),
                    'price_current': prices[-1],
                    'price_change': prices[-1] - prices[0] if len(prices) > 1 else 0
                })

            return stats

        except Exception as e:
            return {'error': str(e)}

    def emergency_stop(self):
        """Emergency stop all operations"""
        try:
            warning("Emergency stop initiated")

            with self._lock:
                self.is_connected = False
                self.is_paused = True

            # Force disconnect
            if self.websocket_client:
                self.websocket_client = None

            # Update UI
            self.safe_set_value(self.get_tag("ws_status_text"), "Status: Emergency Stopped")
            self.safe_configure_item(self.get_tag("ws_status_text"), color=(255, 0, 0))

            self.update_auth_log("Emergency stop - All operations halted")
            error("Emergency stop executed")

        except Exception as e:
            error("Error during emergency stop", context={'error': str(e)}, exc_info=True)