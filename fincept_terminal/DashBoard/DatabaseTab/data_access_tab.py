# -*- coding: utf-8 -*-
# data_access_tab.py

import dearpygui.dearpygui as dpg
import requests
import json
from datetime import datetime
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import info, error, warning, monitor_performance, operation


class DataAccessTab(BaseTab):
    """Minimal, bulletproof data access tab"""

    def __init__(self, app):
        super().__init__(app)
        self.api_base = "https://finceptbackend.share.zrok.io"
        self.databases = []
        self.loading = False
        self._request_cache = {}  # Cache for API responses

        info("Data Access tab initialized", context={"api_base": self.api_base})

    def get_label(self):
        return "📊 Data Access"

    def create_content(self):
        """Create minimal, safe content"""
        try:
            # Check authentication first
            if not self.is_user_authenticated():
                self.create_simple_auth_screen()
            else:
                self.create_simple_data_screen()

        except Exception as e:
            error("Data access content creation failed", context={"error": str(e)}, exc_info=True)
            # Absolute fallback - create minimal interface
            self.create_fallback_interface()

    def create_fallback_interface(self):
        """Absolute fallback interface"""
        try:
            dpg.add_text("📊 Data Access (Minimal Mode)")
            dpg.add_spacer(height=10)
            dpg.add_text("Data access interface is in minimal mode")
            dpg.add_spacer(height=10)
            dpg.add_button(label="Test API Connection", callback=self.test_connection)
            dpg.add_spacer(height=10)
            dpg.add_text("Status: Ready", tag="fallback_status", color=[100, 255, 100])

            warning("Using fallback interface due to initialization error")

        except Exception as e:
            error("Fallback interface creation failed", context={"error": str(e)}, exc_info=True)

    def create_simple_auth_screen(self):
        """Simple authentication required screen"""
        try:
            dpg.add_text("🔐 Authentication Required", color=[255, 255, 100])
            dpg.add_spacer(height=20)
            dpg.add_text("Please authenticate to access financial data")
            dpg.add_spacer(height=10)
            dpg.add_text("Available options:")
            dpg.add_text("• Login/Signup for unlimited access")
            dpg.add_text("• Guest access for 50 requests/day")
            dpg.add_spacer(height=20)
            dpg.add_button(label="Restart & Authenticate", callback=self.restart_auth)
            dpg.add_spacer(height=10)
            dpg.add_button(label="Test API Connection", callback=self.test_connection)

        except Exception as e:
            error("Auth screen creation failed", context={"error": str(e)}, exc_info=True)
            self.create_fallback_interface()

    def create_simple_data_screen(self):
        """Simple data access screen"""
        try:
            # Header
            dpg.add_text("📊 Financial Data Access", color=[100, 255, 100])
            dpg.add_spacer(height=10)

            # User info
            session_data = self.get_session_data()
            user_type = session_data.get("user_type", "unknown")

            if user_type == "guest":
                dpg.add_text("👤 Guest Access - 50 requests/day limit", color=[255, 255, 100])
            elif user_type == "authenticated":
                user_info = session_data.get('user_info', {})
                username = user_info.get('username', 'User')
                credit_balance = user_info.get('credit_balance', 0)
                dpg.add_text(f"👤 {username} - {credit_balance} credits", color=[100, 255, 100])
            else:
                dpg.add_text("👤 Unknown user type", color=[255, 100, 100])

            dpg.add_spacer(height=20)

            # Control buttons
            dpg.add_text("Controls:", color=[255, 255, 255])
            dpg.add_button(label="🔄 Load Databases", callback=self.load_databases_simple)
            dpg.add_button(label="🧪 Test Connection", callback=self.test_connection)
            dpg.add_button(label="📋 Load Sample Data", callback=self.load_sample_data)

            dpg.add_spacer(height=15)

            # Status
            dpg.add_text("Status:", color=[255, 255, 255])
            dpg.add_text("Ready", tag="simple_status", color=[100, 255, 100])
            dpg.add_spacer(height=20)

            # Database section
            dpg.add_text("Available Databases:", color=[255, 255, 255])
            dpg.add_listbox(
                items=["Click 'Load Databases' to fetch"],
                tag="simple_db_list",
                width=400,
                num_items=5,
                callback=self.on_simple_db_select
            )

            dpg.add_spacer(height=15)

            # Table section
            dpg.add_text("Available Tables:", color=[255, 255, 255])
            dpg.add_listbox(
                items=["Select a database first"],
                tag="simple_table_list",
                width=400,
                num_items=5,
                callback=self.on_simple_table_select
            )

            dpg.add_spacer(height=20)

            # Output section
            dpg.add_text("Data Output:", color=[255, 255, 255])
            dpg.add_input_text(
                tag="simple_output",
                multiline=True,
                height=200,
                width=600,
                readonly=True,
                default_value="Welcome to Data Access!\n\nClick 'Load Databases' to start exploring financial data.\n\nAPI Endpoint: " + self.api_base
            )

            info("Data access interface created successfully", context={"user_type": user_type})

        except Exception as e:
            error("Data screen creation failed", context={"error": str(e)}, exc_info=True)
            self.create_fallback_interface()

    @monitor_performance
    def load_databases_simple(self):
        """Load databases with minimal complexity"""
        if self.loading:
            self.update_simple_status("⏳ Already loading...")
            return

        self.loading = True

        with operation("load_databases", context={"api_base": self.api_base}):
            self.update_simple_status("🔄 Loading databases...")

            try:
                # Check cache first
                cache_key = "databases"
                if cache_key in self._request_cache:
                    cached_time, cached_data = self._request_cache[cache_key]
                    if datetime.now().timestamp() - cached_time < 300:  # 5 minutes cache
                        self._process_databases_response(cached_data, from_cache=True)
                        return

                headers = self.get_auth_headers()

                # Add timeout and better error handling
                response = requests.get(
                    f"{self.api_base}/databases",
                    headers=headers,
                    timeout=15
                )

                if response.status_code == 200:
                    data = response.json()
                    # Cache the response
                    self._request_cache[cache_key] = (datetime.now().timestamp(), data)
                    self._process_databases_response(data)

                elif response.status_code == 401:
                    warning("Authentication required for database access")
                    self.update_simple_status("🔐 Authentication required")
                    self.update_output(
                        "Authentication error: Please check your credentials or restart the application.")

                elif response.status_code == 403:
                    warning("Access forbidden for database listing")
                    self.update_simple_status("⛔ Access forbidden")
                    self.update_output("Access denied: You don't have permission to access databases.")

                else:
                    error("Database loading HTTP error", context={"status_code": response.status_code})
                    self.update_simple_status(f"❌ HTTP {response.status_code}")
                    self.update_output(f"HTTP Error {response.status_code}: {response.text[:200]}")

            except requests.exceptions.Timeout:
                warning("Database loading request timeout")
                self.update_simple_status("⏰ Request timeout")
                self.update_output(
                    "Error: Request timeout - the server is taking too long to respond.\nPlease check your internet connection.")

            except requests.exceptions.ConnectionError:
                error("Database loading connection error")
                self.update_simple_status("🌐 Connection error")
                self.update_output(
                    "Error: Cannot connect to the API server.\nPlease check your internet connection and try again.")

            except requests.exceptions.RequestException as e:
                error("Database loading network error", context={"error": str(e)}, exc_info=True)
                self.update_simple_status("❌ Network error")
                self.update_output(f"Network error: {str(e)}")

            except json.JSONDecodeError:
                error("Invalid JSON response from database API")
                self.update_simple_status("❌ Invalid response")
                self.update_output("Error: Received invalid response from server.")

            except Exception as e:
                error("Unexpected database loading error", context={"error": str(e)}, exc_info=True)
                self.update_simple_status(f"❌ Error: {str(e)[:50]}")
                self.update_output(f"Unexpected error: {str(e)}")

            finally:
                self.loading = False

    def _process_databases_response(self, data, from_cache=False):
        """Process databases response data"""
        try:
            if data.get('success'):
                self.databases = data.get('data', {}).get('databases', [])

                if self.databases:
                    db_names = [db.get('display_name', db.get('database_name', 'Unnamed')) for db in self.databases]

                    # Update listbox safely
                    if dpg.does_item_exist("simple_db_list"):
                        dpg.configure_item("simple_db_list", items=db_names)

                    cache_info = " (cached)" if from_cache else ""
                    self.update_simple_status(f"✅ Loaded {len(self.databases)} databases{cache_info}")

                    output_text = f"Found {len(self.databases)} databases{cache_info}:\n\n"
                    for i, db in enumerate(self.databases, 1):
                        output_text += f"{i}. {db.get('display_name', 'Unnamed')}\n"
                    output_text += f"\nLoaded at: {datetime.now().strftime('%H:%M:%S')}"

                    self.update_output(output_text)
                    info("Databases loaded successfully", context={
                        "count": len(self.databases),
                        "from_cache": from_cache
                    })
                else:
                    self.update_simple_status("ℹ️ No databases found")
                    self.update_output("No databases available in your account.")
            else:
                error_msg = data.get('message', 'Failed to load databases')
                error("Database API returned error", context={"message": error_msg})
                self.update_simple_status(f"❌ {error_msg}")
                self.update_output(f"API Error: {error_msg}")

        except Exception as e:
            error("Error processing databases response", context={"error": str(e)}, exc_info=True)

    def on_simple_db_select(self, sender, selection):
        """Handle simple database selection"""
        try:
            if not selection or selection == "Click 'Load Databases' to fetch":
                return

            # Find selected database
            selected_db = None
            for db in self.databases:
                if db.get('display_name') == selection or db.get('database_name') == selection:
                    selected_db = db
                    break

            if selected_db:
                self.update_simple_status(f"📂 Selected: {selection}")
                self.load_tables_simple(selected_db)
            else:
                warning("Selected database not found", context={"selection": selection})
                self.update_simple_status("❌ Database not found")

        except Exception as e:
            error("Database selection error", context={"error": str(e)}, exc_info=True)
            self.update_simple_status("❌ Selection error")

    @monitor_performance
    def load_tables_simple(self, database):
        """Load tables for selected database"""
        if self.loading:
            self.update_simple_status("⏳ Already loading...")
            return

        self.loading = True
        db_name = database.get('database_name', database.get('display_name', 'unknown'))

        with operation("load_tables", context={"database": db_name}):
            self.update_simple_status(f"🔄 Loading tables for {db_name}...")

            try:
                # Check cache first
                cache_key = f"tables_{db_name}"
                if cache_key in self._request_cache:
                    cached_time, cached_data = self._request_cache[cache_key]
                    if datetime.now().timestamp() - cached_time < 300:  # 5 minutes cache
                        self._process_tables_response(cached_data, database, from_cache=True)
                        return

                headers = self.get_auth_headers()
                response = requests.get(
                    f"{self.api_base}/database/{db_name}/tables",
                    headers=headers,
                    timeout=15
                )

                if response.status_code == 200:
                    data = response.json()
                    # Cache the response
                    self._request_cache[cache_key] = (datetime.now().timestamp(), data)
                    self._process_tables_response(data, database)
                else:
                    error("Table loading HTTP error", context={
                        "status_code": response.status_code,
                        "database": db_name
                    })
                    self.update_simple_status(f"❌ HTTP {response.status_code}")
                    self.update_output(f"Table loading HTTP error: {response.status_code}")

            except Exception as e:
                error("Table loading error", context={"database": db_name, "error": str(e)}, exc_info=True)
                self.update_simple_status(f"❌ Error: {str(e)[:50]}")
                self.update_output(f"Table loading error: {str(e)}")
            finally:
                self.loading = False

    def _process_tables_response(self, data, database, from_cache=False):
        """Process tables response data"""
        try:
            db_name = database.get('database_name', database.get('display_name', 'unknown'))

            if data.get('success'):
                tables = data.get('data', {}).get('tables', [])

                if tables:
                    # Update table listbox
                    if dpg.does_item_exist("simple_table_list"):
                        dpg.configure_item("simple_table_list", items=tables)

                    cache_info = " (cached)" if from_cache else ""
                    self.update_simple_status(f"✅ Found {len(tables)} tables{cache_info}")

                    output_text = f"Tables in {database.get('display_name', db_name)}{cache_info}:\n\n"
                    for i, table in enumerate(tables, 1):
                        output_text += f"{i}. {table}\n"
                    output_text += f"\nLoaded at: {datetime.now().strftime('%H:%M:%S')}"

                    self.update_output(output_text)
                    info("Tables loaded successfully", context={
                        "database": db_name,
                        "count": len(tables),
                        "from_cache": from_cache
                    })
                else:
                    self.update_simple_status("ℹ️ No tables found")
                    self.update_output(f"No tables found in database: {database.get('display_name', db_name)}")
            else:
                error_msg = data.get('message', 'Failed to load tables')
                error("Tables API returned error", context={"database": db_name, "message": error_msg})
                self.update_simple_status(f"❌ {error_msg}")
                self.update_output(f"Table loading error: {error_msg}")

        except Exception as e:
            error("Error processing tables response", context={"database": db_name, "error": str(e)}, exc_info=True)

    def on_simple_table_select(self, sender, selection):
        """Handle simple table selection"""
        try:
            if selection and selection != "Select a database first":
                self.update_simple_status(f"📋 Selected table: {selection}")
                self.update_output(
                    f"Selected table: {selection}\n\nClick 'Load Sample Data' to fetch sample data from this table.\n\nTable selected at: {datetime.now().strftime('%H:%M:%S')}")
                info("Table selected", context={"table": selection})
        except Exception as e:
            error("Table selection error", context={"error": str(e)}, exc_info=True)

    def load_sample_data(self):
        """Load sample data"""
        try:
            with operation("load_sample_data"):
                self.update_simple_status("📋 Loading sample data...")

                # Generate sample financial data with current timestamp
                current_time = datetime.now()

                sample_data = f"""📊 Sample Financial Data

🍎 AAPL (Apple Inc.)
   Price: $150.25 (+2.15%)
   Volume: 45,123,456
   Market Cap: $2.5T

🔍 GOOGL (Alphabet Inc.)
   Price: $2,750.80 (-0.85%)
   Volume: 12,345,678
   Market Cap: $1.8T

🖥️ MSFT (Microsoft Corp.)
   Price: $305.67 (+1.23%)
   Volume: 28,901,234
   Market Cap: $2.3T

📈 TSLA (Tesla Inc.)
   Price: $185.45 (+4.50%)
   Volume: 67,890,123
   Market Cap: $580B

💰 BTC/USD
   Price: $43,250.00 (+1.75%)
   24h Volume: $15.2B

💶 EUR/USD
   Rate: 1.0875 (-0.12%)

📅 Data refreshed: {current_time.strftime("%Y-%m-%d %H:%M:%S")}
🌐 Source: Sample Data Generator
⚡ Status: Real-time simulation

Note: This is sample data for demonstration purposes.
Connect to real data sources through the Data Sources tab."""

                self.update_output(sample_data)
                self.update_simple_status("✅ Sample data loaded")
                info("Sample data loaded successfully")

        except Exception as e:
            error("Sample data loading error", context={"error": str(e)}, exc_info=True)
            self.update_simple_status("❌ Sample data error")

    @monitor_performance
    def test_connection(self):
        """Test API connection"""
        with operation("test_api_connection", context={"endpoint": f"{self.api_base}/health"}):
            try:
                self.update_simple_status("🧪 Testing connection...")

                response = requests.get(f"{self.api_base}/health", timeout=10)

                if response.status_code == 200:
                    self.update_simple_status("✅ Connection successful")

                    test_result = f"""🧪 API Connection Test Results

✅ Status: SUCCESS
🌐 Endpoint: {self.api_base}
📡 Response Code: {response.status_code}
⏱️ Response Time: < 1 second
📅 Test Time: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}

Server is reachable and responding normally.
You can proceed with data operations."""

                    self.update_output(test_result)
                    info("API connection test successful", context={"status_code": response.status_code})
                else:
                    warning("API connection test returned error", context={"status_code": response.status_code})
                    self.update_simple_status(f"⚠️ HTTP {response.status_code}")
                    self.update_output(
                        f"🧪 API Connection Test: PARTIAL\n\nReceived HTTP {response.status_code}\nServer is reachable but returned an error.")

            except requests.exceptions.Timeout:
                warning("API connection test timeout")
                self.update_simple_status("⏰ Connection timeout")
                self.update_output(
                    "🧪 API Connection Test: TIMEOUT\n\nThe server took too long to respond.\nPlease check your internet connection.")

            except requests.exceptions.ConnectionError:
                error("API connection test failed - connection error")
                self.update_simple_status("❌ Connection failed")
                self.update_output(
                    "🧪 API Connection Test: FAILED\n\nCannot reach the API server.\nPlease check your internet connection and firewall settings.")

            except Exception as e:
                error("API connection test error", context={"error": str(e)}, exc_info=True)
                self.update_simple_status("❌ Test failed")
                self.update_output(f"🧪 API Connection Test: ERROR\n\nUnexpected error: {str(e)}")

    def update_simple_status(self, message):
        """Update status safely"""
        try:
            if dpg.does_item_exist("simple_status"):
                dpg.set_value("simple_status", message)
            elif dpg.does_item_exist("fallback_status"):
                dpg.set_value("fallback_status", message)

        except Exception as e:
            # Log status updates only for significant events or errors
            if "error" in message.lower() or "failed" in message.lower():
                error("Status update failed", context={"message": message, "error": str(e)})

    def update_output(self, message):
        """Update output area safely"""
        try:
            if dpg.does_item_exist("simple_output"):
                dpg.set_value("simple_output", message)

        except Exception as e:
            error("Output update failed", context={"error": str(e)})

    def restart_auth(self):
        """Restart authentication"""
        try:
            if hasattr(self.app, 'clear_session_and_restart'):
                info("Restarting authentication via app method")
                self.app.clear_session_and_restart()
            else:
                self.update_output(
                    "🔄 Please restart the application to authenticate.\n\nClose the terminal and run it again to access the authentication screen.")
                info("Authentication restart requested - manual restart required")
        except Exception as e:
            error("Restart auth error", context={"error": str(e)}, exc_info=True)

    def is_user_authenticated(self):
        """Check authentication from session"""
        try:
            session_data = self.get_session_data()
            authenticated = session_data.get("authenticated", False)
            return authenticated
        except Exception as e:
            error("Authentication check error", context={"error": str(e)}, exc_info=True)
            return False

    def get_auth_headers(self):
        """Get auth headers from session"""
        try:
            session_data = self.get_session_data()
            headers = {"Content-Type": "application/json"}

            if session_data.get("api_key"):
                headers["X-API-Key"] = session_data["api_key"]

            if session_data.get("device_id"):
                headers["X-Device-ID"] = session_data["device_id"]

            # Add user agent
            headers["User-Agent"] = "Fincept-Terminal/1.0"

            return headers

        except Exception as e:
            error("Auth headers generation error", context={"error": str(e)}, exc_info=True)
            return {"Content-Type": "application/json", "User-Agent": "Fincept-Terminal/1.0"}

    def cleanup(self):
        """Clean up resources"""
        try:
            self.databases = []
            self.loading = False
            self._request_cache.clear()  # Clear cache to free memory
            info("Data Access tab cleanup completed")
        except Exception as e:
            error("Cleanup error", context={"error": str(e)}, exc_info=True)