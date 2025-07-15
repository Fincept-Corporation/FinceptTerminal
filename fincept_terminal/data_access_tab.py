import dearpygui.dearpygui as dpg
import requests
import json
from datetime import datetime
from fincept_terminal.Utils.base_tab import BaseTab


class DataAccessTab(BaseTab):
    """Minimal, bulletproof data access tab"""

    def __init__(self, app):
        super().__init__(app)
        self.api_base = "https://finceptbackend.share.zrok.io"
        self.databases = []
        self.loading = False

    def get_label(self):
        return "📊 Data Access"

    def create_content(self):
        """Create minimal, safe content"""
        try:
            # Just add basic elements without complex containers
            if not self.is_user_authenticated():
                self.create_simple_auth_screen()
            else:
                self.create_simple_data_screen()

        except Exception as e:
            print(f"Data access error: {e}")
            # Absolute fallback
            try:
                dpg.add_text("📊 Data Access (Minimal Mode)")
                dpg.add_text("Data access interface is in minimal mode")
                dpg.add_button(label="Test API Connection", callback=self.test_connection)
            except:
                pass

    def create_simple_auth_screen(self):
        """Simple authentication required screen"""
        dpg.add_text("🔐 Authentication Required")
        dpg.add_spacer(height=20)
        dpg.add_text("Please authenticate to access financial data")
        dpg.add_spacer(height=10)
        dpg.add_text("Available options:")
        dpg.add_text("• Login/Signup for unlimited access")
        dpg.add_text("• Guest access for 50 requests/day")
        dpg.add_spacer(height=20)
        dpg.add_button(label="Restart & Authenticate", callback=self.restart_auth)

    def create_simple_data_screen(self):
        """Simple data access screen"""
        # Header
        dpg.add_text("📊 Financial Data Access")
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
            dpg.add_text(f"🔑 {username} - {credit_balance} credits", color=[100, 255, 100])

        dpg.add_spacer(height=20)

        # Simple controls
        dpg.add_button(label="🔄 Load Databases", callback=self.load_databases_simple)
        dpg.add_spacer(height=10)

        # Status
        dpg.add_text("Ready", tag="simple_status", color=[100, 255, 100])
        dpg.add_spacer(height=20)

        # Simple database list
        dpg.add_text("Available Databases:")
        dpg.add_listbox(items=["Click 'Load Databases' to fetch"],
                        tag="simple_db_list", width=400, num_items=5,
                        callback=self.on_simple_db_select)

        dpg.add_spacer(height=20)

        # Simple table list
        dpg.add_text("Available Tables:")
        dpg.add_listbox(items=["Select a database first"],
                        tag="simple_table_list", width=400, num_items=5,
                        callback=self.on_simple_table_select)

        dpg.add_spacer(height=20)

        # Load data button
        dpg.add_button(label="📥 Load Sample Data", callback=self.load_sample_data)

        dpg.add_spacer(height=20)

        # Simple output area
        dpg.add_text("Data Output:")
        dpg.add_input_text(tag="simple_output", multiline=True, height=200, width=600, readonly=True)

        # Initialize with sample message
        dpg.set_value("simple_output", "Welcome to Data Access!\n\nClick 'Load Databases' to start.")

    def load_databases_simple(self):
        """Load databases with minimal complexity"""
        if self.loading:
            return

        self.loading = True
        self.update_simple_status("Loading databases...")

        try:
            headers = self.get_auth_headers()
            response = requests.get(f"{self.api_base}/databases", headers=headers, timeout=10)

            if response.status_code == 200:
                data = response.json()
                if data.get('success'):
                    self.databases = data.get('data', {}).get('databases', [])
                    db_names = [db['display_name'] for db in self.databases]

                    # Update listbox safely
                    if dpg.does_item_exist("simple_db_list"):
                        dpg.configure_item("simple_db_list", items=db_names)

                    self.update_simple_status(f"✅ Loaded {len(self.databases)} databases")
                    self.update_output(f"Found {len(self.databases)} databases:\n" + "\n".join(db_names))
                else:
                    error_msg = data.get('message', 'Failed to load databases')
                    self.update_simple_status(f"❌ {error_msg}")
                    self.update_output(f"Error: {error_msg}")
            else:
                self.update_simple_status(f"❌ HTTP {response.status_code}")
                self.update_output(f"HTTP Error: {response.status_code}")

        except requests.exceptions.Timeout:
            self.update_simple_status("❌ Request timeout")
            self.update_output("Error: Request timeout - check your connection")
        except Exception as e:
            self.update_simple_status(f"❌ Error: {str(e)}")
            self.update_output(f"Error: {str(e)}")
        finally:
            self.loading = False

    def on_simple_db_select(self, sender, selection):
        """Handle simple database selection"""
        if not selection:
            return

        # Find selected database
        selected_db = None
        for db in self.databases:
            if db['display_name'] == selection:
                selected_db = db
                break

        if selected_db:
            self.update_simple_status(f"📊 Selected: {selection}")
            self.load_tables_simple(selected_db)

    def load_tables_simple(self, database):
        """Load tables for selected database"""
        if self.loading:
            return

        self.loading = True
        db_name = database['database_name']
        self.update_simple_status(f"Loading tables for {db_name}...")

        try:
            headers = self.get_auth_headers()
            response = requests.get(f"{self.api_base}/database/{db_name}/tables",
                                    headers=headers, timeout=10)

            if response.status_code == 200:
                data = response.json()
                if data.get('success'):
                    tables = data.get('data', {}).get('tables', [])

                    # Update table listbox
                    if dpg.does_item_exist("simple_table_list"):
                        dpg.configure_item("simple_table_list", items=tables)

                    self.update_simple_status(f"✅ Found {len(tables)} tables")
                    self.update_output(f"Tables in {database['display_name']}:\n" + "\n".join(tables))
                else:
                    error_msg = data.get('message', 'Failed to load tables')
                    self.update_simple_status(f"❌ {error_msg}")
                    self.update_output(f"Table loading error: {error_msg}")
            else:
                self.update_simple_status(f"❌ HTTP {response.status_code}")
                self.update_output(f"Table loading HTTP error: {response.status_code}")

        except Exception as e:
            self.update_simple_status(f"❌ Error: {str(e)}")
            self.update_output(f"Table loading error: {str(e)}")
        finally:
            self.loading = False

    def on_simple_table_select(self, sender, selection):
        """Handle simple table selection"""
        if selection:
            self.update_simple_status(f"📋 Selected table: {selection}")
            self.update_output(f"Selected table: {selection}\nClick 'Load Sample Data' to fetch data.")

    def load_sample_data(self):
        """Load sample data"""
        self.update_simple_status("📊 Loading sample data...")

        # Generate sample financial data
        sample_data = """Sample Financial Data:

AAPL (Apple Inc.)
- Price: $150.25
- Volume: 45,123,456
- Change: +2.15%

GOOGL (Alphabet Inc.)
- Price: $2,750.80
- Volume: 12,345,678
- Change: -0.85%

MSFT (Microsoft Corp.)
- Price: $305.67
- Volume: 28,901,234
- Change: +1.23%

Data refreshed: """ + datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        self.update_output(sample_data)
        self.update_simple_status("✅ Sample data loaded")

    def test_connection(self):
        """Test API connection"""
        self.update_simple_status("Testing connection...")

        try:
            response = requests.get(f"{self.api_base}/health", timeout=5)
            if response.status_code == 200:
                self.update_simple_status("✅ Connection successful")
                self.update_output("API connection test: SUCCESS\nAPI is reachable and responding.")
            else:
                self.update_simple_status(f"⚠️ HTTP {response.status_code}")
                self.update_output(f"API connection test: HTTP {response.status_code}")
        except Exception as e:
            self.update_simple_status("❌ Connection failed")
            self.update_output(f"API connection test: FAILED\nError: {str(e)}")

    def update_simple_status(self, message):
        """Update status safely"""
        try:
            if dpg.does_item_exist("simple_status"):
                dpg.set_value("simple_status", message)
            print(f"Data Access Status: {message}")
        except:
            print(f"Data Access Status: {message}")

    def update_output(self, message):
        """Update output area safely"""
        try:
            if dpg.does_item_exist("simple_output"):
                dpg.set_value("simple_output", message)
        except:
            print(f"Data Output: {message}")

    def restart_auth(self):
        """Restart authentication"""
        if hasattr(self.app, 'clear_session_and_restart'):
            self.app.clear_session_and_restart()
        else:
            print("🔄 Please restart the application to authenticate")

    def is_user_authenticated(self):
        """Check authentication from session"""
        try:
            session_data = self.get_session_data()
            return session_data.get("authenticated", False)
        except:
            return False

    def get_auth_headers(self):
        """Get auth headers from session"""
        try:
            session_data = self.get_session_data()
            headers = {}

            if session_data.get("api_key"):
                headers["X-API-Key"] = session_data["api_key"]

            if session_data.get("device_id"):
                headers["X-Device-ID"] = session_data["device_id"]

            return headers
        except:
            return {}

    def cleanup(self):
        """Clean up resources"""
        try:
            self.databases = []
            self.loading = False
        except:
            pass