# main.py - Optimized with Reduced Logging and Faster Loading
import dearpygui.dearpygui as dpg
import gc
import sys
import requests
from pathlib import Path
from datetime import datetime

# Import centralized config
from fincept_terminal.Utils.config import config, get_api_endpoint, is_strict_mode

import warnings
warnings.filterwarnings("ignore",category=UserWarning)
warnings.filterwarnings("ignore",category=DeprecationWarning)

# Import session manager
from fincept_terminal.Utils.Managers.session_manager import session_manager

# Safe import of theme manager
try:
    from fincept_terminal.Utils.Managers.theme_manager import AutomaticThemeManager

    THEMES_AVAILABLE = True
except ImportError:
    THEMES_AVAILABLE = False



# Safe import of tab modules
AVAILABLE_TABS = {}


def safe_import_tab(tab_name, module_name, class_name):
    """Safely import a tab module and class"""
    try:
        module = __import__(f"{module_name}", fromlist=[class_name])
        tab_class = getattr(module, class_name)
        return tab_class
    except ImportError:
        return None
    except Exception:
        return None



# Import tabs safely
TAB_IMPORTS = [
    ("Dashboard", "fincept_terminal.DashBoard.dashboard_tab", "DashboardTab"),
    ("Market Tab", "fincept_terminal.DashBoard.market_tab", "MarketTab"),
    ("Rss Tab", "rss_tab", "RssTab"),
    ("analytics", "analytics_tab", "AnalyticsTab"),
    ("Portfolio", "portfolio_tab", "PortfolioTab"),
    ("Watchlist", "watchlist_tab", "WatchlistTab"),
    ("database", "database_tab", "DatabaseTab"),
    ("fyers", "fyers_tab", "FyersTab"),
    #("Technicals", "technical_tab", "TechnicalAnalysisTab"),
    ("Technicals", "technicals", "TechnicalAnalysisTab"),
    ("Data Access", "data_access_tab", "DataAccessTab"),
    ("India Data", "indiagov_tab", "DataGovIndiaTab"),
    ("Equity Research", "stock_research_tab", "StockResearchTab"),
    ("Robo Advisor", "robo_advisor_tab", "RoboAdvisorTab"),
    ("Consumer", "consumer_behaviour_tab", "ConsumerBehaviorTab"),
    ("Dbnomics", "dbnomics_tab", "DBnomicsTab"),
    ("profile", "fincept_terminal.DashBoard.ProfileTab.profile_tab", "ProfileTab"),
    ("Help", "help_tab", "HelpTab"),
]

# Parallel tab loading for faster startup
import threading

loading_results = {}
loading_lock = threading.Lock()


def load_tab_worker(tab_id, module_name, class_name):
    """Worker function for parallel tab loading"""
    tab_class = safe_import_tab(tab_id, module_name, class_name)
    with loading_lock:
        loading_results[tab_id] = tab_class


# Start parallel loading
threads = []
for tab_id, module_name, class_name in TAB_IMPORTS:
    thread = threading.Thread(target=load_tab_worker, args=(tab_id, module_name, class_name))
    thread.start()
    threads.append(thread)

# Wait for all tabs to load
for thread in threads:
    thread.join()

# Collect results
for tab_id, module_name, class_name in TAB_IMPORTS:
    if tab_id in loading_results and loading_results[tab_id]:
        AVAILABLE_TABS[tab_id] = loading_results[tab_id]

if not AVAILABLE_TABS:
    sys.exit(1)


class MainApplication:
    def __init__(self, session_data):
        self.is_running = True
        self.resize_lock = False
        self.api_request_count = 0

        # Store session data
        self.session_data = session_data
        self.user_type = session_data.get("user_type", "guest")
        self.authenticated = session_data.get("authenticated", False)
        self.api_key = session_data.get("api_key")

        # Default sizes
        self.DEFAULT_WIDTH = 1200
        self.DEFAULT_HEIGHT = 600
        self.MARGIN_WIDTH = 20
        self.MARGIN_HEIGHT = 120

        # Initialize theme manager if available
        if THEMES_AVAILABLE:
            try:
                self.theme_manager = AutomaticThemeManager()
            except Exception:
                self.theme_manager = None
        else:
            self.theme_manager = None

        # Calculate responsive sizes
        self.calculate_sizes()

        # Initialize all available tabs (reverted from lazy loading for stability)
        self.tabs = {}
        for tab_id, tab_class in AVAILABLE_TABS.items():
            try:
                self.tabs[tab_id] = tab_class(self)
            except Exception as e:
                print(f"❌ Error initializing {tab_id} tab: {e}")

    def initialize_tab_on_demand(self, tab_id):
        """Initialize tab only when needed"""
        if tab_id in self.tabs_initialized:
            return True

        if tab_id in AVAILABLE_TABS:
            try:
                # Initialize the tab class
                self.tabs[tab_id] = AVAILABLE_TABS[tab_id](self)
                self.tabs_initialized.add(tab_id)

                # Immediately create the content for this tab
                tab_tag = f"tab_{tab_id}"
                if dpg.does_item_exist(tab_tag):
                    # Remove loading placeholder
                    if dpg.does_item_exist(f"loading_{tab_id}"):
                        dpg.delete_item(f"loading_{tab_id}")

                    # Add the actual tab content
                    dpg.push_container_stack(tab_tag)
                    self.tabs[tab_id].create_content()
                    dpg.pop_container_stack()

                return True
            except Exception as e:
                print(f"Error initializing {tab_id}: {e}")
                # Show error in tab
                tab_tag = f"tab_{tab_id}"
                if dpg.does_item_exist(tab_tag):
                    if dpg.does_item_exist(f"loading_{tab_id}"):
                        dpg.delete_item(f"loading_{tab_id}")

                    dpg.push_container_stack(tab_tag)
                    dpg.add_text(f"Error loading {tab_id} tab", color=[255, 100, 100])
                    dpg.add_text(f"Details: {str(e)}")
                    dpg.pop_container_stack()
                return False
        return False


    def calculate_sizes(self):
        """Calculate responsive layout sizes"""
        self.usable_width = self.DEFAULT_WIDTH - self.MARGIN_WIDTH
        self.usable_height = self.DEFAULT_HEIGHT - self.MARGIN_HEIGHT

        self.left_width = int(self.usable_width * 0.25) - 3
        self.center_width = int(self.usable_width * 0.50) - 3
        self.right_width = int(self.usable_width * 0.25) - 3

        self.top_height = int(self.usable_height * 0.66) - 3
        self.bottom_height = int(self.usable_height * 0.34) - 3
        self.cell_height = int(self.top_height / 2) - 2

    def get_api_key_type(self):
        """Get API key type for display"""
        if not self.api_key:
            return "Offline"
        elif self.api_key.startswith("fk_guest_"):
            return "Guest"
        elif self.api_key.startswith("fk_user_"):
            return "User"
        else:
            return "Legacy"

    def resize_callback(self):
        """Handle window resize with optimization"""
        if self.resize_lock or not self.is_running:
            return

        try:
            self.resize_lock = True

            viewport_width = dpg.get_viewport_width()
            viewport_height = dpg.get_viewport_height()

            if viewport_width < 400 or viewport_height < 300:
                return

            # Update sizes
            new_usable_width = viewport_width - self.MARGIN_WIDTH
            new_usable_height = viewport_height - self.MARGIN_HEIGHT

            new_left_width = int(new_usable_width * 0.25) - 5
            new_center_width = int(new_usable_width * 0.49) - 5
            new_right_width = int(new_usable_width * 0.25) - 5

            new_top_height = int(new_usable_height * 0.64) - 5
            new_bottom_height = int(new_usable_height * 0.34) - 5
            new_cell_height = int(new_top_height / 2) - 2

            # Update stored sizes
            self.usable_width = new_usable_width
            self.usable_height = new_usable_height
            self.left_width = new_left_width
            self.center_width = new_center_width
            self.right_width = new_right_width
            self.top_height = new_top_height
            self.bottom_height = new_bottom_height
            self.cell_height = new_cell_height

            # Notify tabs that support resizing
            for tab_name, tab in self.tabs.items():
                if hasattr(tab, 'resize_components'):
                    try:
                        tab.resize_components(
                            new_left_width, new_center_width, new_right_width,
                            new_top_height, new_bottom_height, new_cell_height
                        )
                    except Exception:
                        pass

        except Exception:
            pass
        finally:
            self.resize_lock = False

    def safe_exit(self):
        """Safely exit the application"""
        self.is_running = False
        try:
            dpg.stop_dearpygui()
        except:
            pass

    def create_menu_bar(self):
        """Create menu bar with enhanced session information"""
        with dpg.menu_bar():
            with dpg.menu(label="File"):
                dpg.add_menu_item(label="New", callback=lambda: None)
                dpg.add_menu_item(label="Open", callback=lambda: None)
                dpg.add_menu_item(label="Save", callback=lambda: None)
                dpg.add_separator()
                # Full‐Screen toggle
                dpg.add_menu_item(
                    label="🖥️ Toggle Full Screen\tF11",
                    callback=self.toggle_fullscreen
                )

                dpg.add_menu_item(label="Exit", callback=self.safe_exit)

            with dpg.menu(label="Session"):
                if self.user_type == "guest":
                    dpg.add_menu_item(label="👤 Guest Mode", callback=self.show_profile_info)
                    dpg.add_menu_item(label="📈 Create Account", callback=self.show_upgrade_info)
                    dpg.add_separator()
                    dpg.add_menu_item(label="🔄 Change User", callback=self.clear_session_and_restart)
                elif self.user_type == "registered":
                    user_info = self.session_data.get('user_info', {})
                    username = user_info.get('username', 'User')
                    dpg.add_menu_item(label=f"🔑 {username}", callback=self.show_profile_info)
                    dpg.add_menu_item(label="🔄 Regenerate API Key", callback=self.regenerate_api_key)
                    dpg.add_separator()
                    dpg.add_menu_item(label="🔄 Change User", callback=self.clear_session_and_restart)
                    dpg.add_menu_item(label="📤 Logout", callback=self.logout_and_restart)

                dpg.add_separator()
                dpg.add_menu_item(label="📊 Session Info", callback=self.show_session_info)
                dpg.add_menu_item(label="🔄 Refresh Session", callback=self.refresh_session_data)

            # API Configuration menu
            with dpg.menu(label="API"):
                dpg.add_menu_item(label="📡 API Status", callback=self.show_api_status)
                dpg.add_menu_item(label="🔧 Configuration", callback=self.show_api_config)
                if not is_strict_mode():
                    dpg.add_menu_item(label="🔒 Enable Strict Mode", callback=self.enable_strict_mode)

            # Database menu
            if 'database' in AVAILABLE_TABS:
                with dpg.menu(label="Database"):
                    dpg.add_menu_item(label="🔗 Browse Databases", callback=self.goto_database_tab)
                    dpg.add_menu_item(label="📊 Data Access", callback=self.goto_database_tab)

            with dpg.menu(label="View"):
                # Theme menu only if themes are available
                if self.theme_manager:
                    with dpg.menu(label="🎨 Themes"):
                        dpg.add_menu_item(label="🖥️ Finance Terminal",
                                          callback=lambda: self.apply_theme_safe("finance_terminal"))
                        dpg.add_menu_item(label="✨ Dark Gold",
                                          callback=lambda: self.apply_theme_safe("dark_gold"))
                        dpg.add_menu_item(label="🔵 Default",
                                          callback=lambda: self.apply_theme_safe("default"))

                dpg.add_separator()
                dpg.add_menu_item(label="Fullscreen", callback=lambda: None)

            with dpg.menu(label="Help"):
                dpg.add_menu_item(label="📖 Documentation", callback=self.show_documentation)
                dpg.add_menu_item(label="🆘 Support", callback=self.show_support)
                dpg.add_menu_item(label="ℹ️ About", callback=self.show_about)

            # Enhanced user status indicator
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=20)
                self.create_user_status_indicator()

    def toggle_fullscreen(self, sender=None, app_data=None, user_data=None):
        import dearpygui.dearpygui as dpg
        dpg.toggle_viewport_fullscreen()


    def create_user_status_indicator(self):
        """Create enhanced user status indicator"""
        if self.user_type == "guest":
            expires_at = self.session_data.get("expires_at")
            if expires_at:
                try:
                    expiry = datetime.fromisoformat(expires_at.replace('Z', '+00:00'))
                    hours_left = max(0, int((expiry.replace(tzinfo=None) - datetime.now()).total_seconds() / 3600))
                    auth_text = f"👤 Guest ({hours_left}h left)"
                except:
                    auth_text = "👤 Guest (Temp Access)"
            else:
                auth_text = "👤 Guest Mode"
            dpg.add_text(auth_text, color=[255, 255, 100])
        elif self.user_type == "registered":
            user_info = self.session_data.get('user_info', {})
            username = user_info.get('username', 'User')
            credit_balance = user_info.get('credit_balance', 0)
            auth_text = f"🔑 {username} ({credit_balance} credits)"
            dpg.add_text(auth_text, color=[100, 255, 100])

    def apply_theme_safe(self, theme_name):
        """Safely apply theme"""
        if self.theme_manager:
            try:
                self.theme_manager.apply_theme_globally(theme_name)
            except Exception:
                pass

    def on_tab_changed(self, sender, app_data):
        """Handle tab change event (simplified - no lazy loading)"""
        # This method is now just for potential future use
        # All tabs are initialized at startup
        pass

    def create_tabs(self):
        """Create tabs with standard initialization"""
        try:
            # Create tab bar (removed callback since we're not using lazy loading)
            dpg.add_tab_bar(tag="main_tab_bar", reorderable=True)

            # Add tabs with content
            for tab_name, tab_instance in self.tabs.items():
                try:
                    # Create tab
                    tab_id = f"tab_{tab_name}"

                    # Get label from tab instance
                    try:
                        label = tab_instance.get_label()
                    except:
                        label = tab_name.title()

                    dpg.add_tab(label=label, tag=tab_id, parent="main_tab_bar")

                    # Add content with error protection
                    try:
                        dpg.push_container_stack(tab_id)
                        tab_instance.create_content()
                        dpg.pop_container_stack()
                    except Exception as content_error:
                        try:
                            dpg.pop_container_stack()
                        except:
                            pass

                        dpg.push_container_stack(tab_id)
                        dpg.add_text(f"Error loading {tab_name} tab", color=[255, 100, 100])
                        dpg.add_text(f"Details: {str(content_error)}")
                        if tab_name == "profile":
                            dpg.add_text("This may be due to missing session data.")
                            dpg.add_button(label="Clear Session & Restart", callback=self.clear_session_and_restart)
                        dpg.pop_container_stack()

                except Exception as tab_error:
                    print(f"❌ Error creating {tab_name} tab: {tab_error}")

        except Exception as e:
            print(f"❌ Critical tab creation error: {e}")
            # Create minimal interface
            dpg.add_text("🚨 Tab System Error", color=[255, 100, 100])
            dpg.add_text("The tab system failed to initialize.")
            dpg.add_button(label="Restart Application", callback=self.safe_exit)

    # Enhanced session management methods
    def show_session_info(self):
        """Show enhanced session information"""
        info = session_manager.get_session_info()

    def show_api_status(self):
        """Show API status information"""
        session_manager.check_api_connectivity()

    def show_api_config(self):
        """Show API configuration"""
        config.validate_configuration()

    def enable_strict_mode(self):
        """Enable strict mode"""
        config.REQUIRE_API_CONNECTION = True
        config.ALLOW_GUEST_FALLBACK = False

    def refresh_session_data(self):
        """Refresh session data from API"""
        fresh_session = session_manager.get_fresh_session()

        if fresh_session:
            self.session_data = fresh_session
            self.user_type = fresh_session.get("user_type", "guest")
            self.authenticated = fresh_session.get("authenticated", False)
            self.api_key = fresh_session.get("api_key")

    def clear_session_and_restart(self):
        """Clear session and restart"""
        session_manager.clear_session()
        self.safe_exit()

    def logout_and_restart(self):
        """Logout from current session and restart"""
        session_manager.clear_session()
        self.safe_exit()

    def save_current_session(self):
        """Save current session credentials (minimal data only)"""
        session_manager.save_session_credentials(self.session_data)

    # Enhanced authentication helper methods
    def get_auth_headers(self):
        """Get authentication headers"""
        headers = {"Content-Type": "application/json"}

        if self.api_key:
            headers["X-API-Key"] = self.api_key

        return headers

    def make_api_request(self, method, endpoint, **kwargs):
        """Make API request with proper authentication and tracking"""
        try:
            headers = kwargs.get("headers", {})
            headers.update(self.get_auth_headers())
            kwargs["headers"] = headers

            url = get_api_endpoint(endpoint)
            response = getattr(requests, method.lower())(url, **kwargs)

            # Track API requests
            self.api_request_count += 1

            return response

        except Exception:
            return None

    def is_user_authenticated(self):
        """Check if user is authenticated"""
        return self.authenticated

    def get_user_type(self):
        """Get current user type"""
        return self.user_type

    def get_session_data(self):
        """Get session data"""
        return self.session_data

    def refresh_user_profile(self):
        """Refresh user profile data from API"""
        try:
            if self.user_type != "registered":
                return False

            # Refresh entire session data from API
            fresh_session = session_manager.get_fresh_session()
            if fresh_session:
                self.session_data = fresh_session
                return True

        except Exception:
            pass

        return False

    # Menu callback methods
    def goto_database_tab(self):
        """Navigate to database tab"""
        try:
            dpg.set_value("main_tab_bar", "tab_database")
        except:
            pass

    def show_profile_info(self):
        """Show profile information"""
        try:
            dpg.set_value("main_tab_bar", "tab_profile")
        except:
            pass

    def show_upgrade_info(self):
        """Show upgrade information"""
        pass

    def regenerate_api_key(self):
        """Regenerate API key for authenticated users"""
        if self.user_type != "registered":
            return

        try:
            response = self.make_api_request("POST", "/user/regenerate-api-key", timeout=config.REQUEST_TIMEOUT)

            if response and response.status_code == 200:
                data = response.json()
                if data.get("success"):
                    new_api_key = data["data"]["api_key"]
                    self.api_key = new_api_key
                    self.session_data["api_key"] = new_api_key
                    # Save new credentials
                    session_manager.save_session_credentials(self.session_data)

        except Exception:
            pass

    def show_documentation(self):
        """Show documentation"""
        pass

    def show_support(self):
        """Show support"""
        pass

    def show_about(self):
        """Show about"""
        pass

    def run(self):
        """Run the application with enhanced session support"""
        try:
            dpg.create_context()

            # Create primary window with explicit content management
            dpg.add_window(tag="Primary Window", label="Fincept Terminal")

            # Push window as container
            dpg.push_container_stack("Primary Window")

            try:
                self.create_menu_bar()
                self.create_tabs()
            finally:
                # Always pop the container
                dpg.pop_container_stack()

            # Enhanced terminal title with API info
            api_key_type = self.get_api_key_type()
            strict_indicator = "[Strict]" if is_strict_mode() else "[Fallback]"
            terminal_title = f"🏦 Fincept Terminal - {self.user_type.title()} ({api_key_type}) {strict_indicator}"

            dpg.create_viewport(title=terminal_title,
                                width=self.DEFAULT_WIDTH, height=self.DEFAULT_HEIGHT)
            dpg.setup_dearpygui()
            dpg.set_primary_window("Primary Window", True)

            # Apply theme if available
            if self.theme_manager:
                try:
                    self.theme_manager.apply_theme_globally("finance_terminal")
                except Exception:
                    pass

            dpg.set_viewport_resize_callback(self.resize_callback)

            # Save credentials on successful launch
            self.save_current_session()

            dpg.show_viewport()
            dpg.start_dearpygui()

        except Exception:
            pass
        finally:
            self.cleanup()

    def cleanup(self):
        """Enhanced cleanup with session handling"""
        try:
            self.is_running = False

            # Save credentials before cleanup (minimal data only)
            self.save_current_session()

            # Cleanup tabs
            if hasattr(self, 'tabs') and self.tabs:
                for tab_name, tab in self.tabs.items():
                    if hasattr(tab, 'cleanup'):
                        try:
                            tab.cleanup()
                        except Exception:
                            pass

            # Cleanup theme
            if hasattr(self, 'theme_manager') and self.theme_manager:
                try:
                    if hasattr(self.theme_manager, 'cleanup'):
                        self.theme_manager.cleanup()
                except Exception:
                    pass

            gc.collect()

            # Cleanup DearPyGUI
            try:
                dpg.destroy_context()
            except Exception:
                pass

        except Exception:
            pass


def check_api_availability():
    """Check if API server is available"""
    if session_manager.is_api_available():
        return True
    else:
        return False


def check_saved_credentials():
    """Check for saved credentials and get fresh session from API"""
    # In strict mode, always check API first
    if is_strict_mode():
        if not check_api_availability():
            return None

    # Always get fresh session from API
    fresh_session = session_manager.get_fresh_session()
    return fresh_session


def main():
    """Main entry point - Always fetch fresh data from API with centralized config"""
    try:
        # Always check for fresh session from API first
        fresh_session = check_saved_credentials()

        if fresh_session:
            # Use fresh session from API
            session_data = fresh_session
        else:
            # Show authentication splash if no valid session
            from fincept_terminal.Utils.Authentication.splash_auth import show_authentication_splash
            session_data = show_authentication_splash()

        # Check if authentication was successful
        if not session_data.get("authenticated"):
            return

        # Save credentials for next startup
        session_manager.save_session_credentials(session_data)

        # Launch main terminal
        app = MainApplication(session_data)
        app.run()

    except KeyboardInterrupt:
        pass
    except Exception:
        pass


def start_terminal():
    """Console‐script entry‐point."""
    main()

if __name__ == "__main__":
    start_terminal()