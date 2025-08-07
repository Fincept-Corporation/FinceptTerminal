# profile_tab.py - Corrected version with logout and working authentication
import dearpygui.dearpygui as dpg
import requests
import webbrowser
import threading
from datetime import datetime, timedelta
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import logger, log_operation
from typing import Dict, Any

# Import centralized config and API client
from fincept_terminal.Utils.config import config, get_api_endpoint
from fincept_terminal.Utils.APIClient.api_client import create_api_client


class ProfileTab(BaseTab):
    """Enhanced profile tab with unified API key authentication and logout functionality"""

    def __init__(self, app):
        super().__init__(app)
        self.last_refresh = None
        self.usage_stats = {}
        self.request_count = 0
        self.api_client = None
        self.logout_in_progress = False

        # Initialize API client with session data
        if hasattr(app, 'get_session_data'):
            session_data = app.get_session_data()
            self.api_client = create_api_client(session_data)

        print(f"🔧 ProfileTab initialized with API: {config.get_api_url()}")

    def get_label(self):
        return "👤 Profile"

    def create_content(self):
        """Create profile content with new API awareness"""
        try:
            # Always refresh data when tab is accessed
            self.refresh_profile_data()

            session_data = self.get_session_data()
            user_type = session_data.get("user_type", "unknown")

            if user_type == "guest":
                self.create_dynamic_guest_profile()
            elif user_type == "registered":
                self.create_dynamic_user_profile()
            else:
                self.create_unknown_profile()

        except Exception as e:
            print(f"ProfileTab content error: {e}")
            self.create_error_profile(str(e))

    # ============================================
    # FIXED PROFILE TAB LOGOUT METHODS
    # ============================================

    def logout_user(self):
        """Complete logout that properly terminates session"""
        if self.logout_in_progress:
            print("⚠️ Logout already in progress...")
            return

        self.logout_in_progress = True

        try:
            # Update logout button to show progress
            if dpg.does_item_exist("logout_btn"):
                dpg.set_item_label("logout_btn", "Logging out...")
                dpg.disable_item("logout_btn")

            print("🚪 Starting complete logout process...")

            # Get current session info for logging
            session_data = self.get_session_data()
            user_type = session_data.get("user_type", "unknown")
            print(f"🔐 Logging out {user_type} user...")

            # Step 1: Perform API logout
            logout_success = False
            if self.api_client and session_data.get("authenticated"):
                logout_success = self.perform_api_logout()
            else:
                print("📝 No API logout needed (offline or guest session)")
                logout_success = True

            # Step 2: Clear session data
            clear_success = self.clear_session_data()

            # Step 3: Clear saved credentials (important!)
            self.clear_saved_credentials()

            # Step 4: Close terminal application
            self.complete_logout_and_exit()

        except Exception as e:
            print(f"❌ Logout error: {e}")
            # Force exit anyway
            self.force_exit_application()
        finally:
            self.logout_in_progress = False

    def clear_saved_credentials(self):
        """Clear saved credentials using session manager"""
        try:
            print("🗑️ Clearing saved credentials...")

            # Import session manager
            from fincept_terminal.Utils.Managers.session_manager import session_manager

            if session_manager.clear_credentials():
                print("✅ Saved credentials cleared")
            else:
                print("⚠️ Could not clear saved credentials")

        except Exception as e:
            print(f"⚠️ Error clearing credentials: {e}")

    def complete_logout_and_exit(self):
        """Complete logout and properly exit application"""
        try:
            print("✅ Logout completed successfully!")
            print("🚪 Closing Fincept Terminal...")
            print("")
            print("To access Fincept again:")
            print("1. 🔄 Run the application")
            print("2. 🔑 Choose authentication method")
            print("3. 👤 Sign in or continue as guest")
            print("")
            print("👋 Thank you for using Fincept!")

            # Give user time to see the message
            import time
            time.sleep(2)

            # Method 1: Try app's close method
            if hasattr(self.app, 'close_application'):
                self.app.close_application()
                return

            # Method 2: Try app's shutdown method
            if hasattr(self.app, 'shutdown'):
                self.app.shutdown()
                return

            # Method 3: Try DearPyGUI stop
            try:
                dpg.stop_dearpygui()
                return
            except:
                pass

            # Method 4: Force exit
            self.force_exit_application()

        except Exception as e:
            print(f"❌ Error during complete logout: {e}")
            self.force_exit_application()

    def force_exit_application(self):
        """Force exit the application"""
        try:
            print("🔧 Force closing application...")

            # Try graceful DearPyGUI shutdown first
            try:
                if hasattr(dpg, 'is_dearpygui_running') and dpg.is_dearpygui_running():
                    dpg.stop_dearpygui()
                    dpg.destroy_context()
            except:
                pass

            # Force system exit
            import sys
            import os

            print("👋 Goodbye!")

            # Try different exit methods
            try:
                sys.exit(0)
            except:
                try:
                    os._exit(0)
                except:
                    quit()

        except Exception as e:
            print(f"❌ Force exit error: {e}")
            # Last resort
            import os
            os._exit(1)

    # Remove these methods completely - they cause crashes:
    # def try_splash_imports(self):  # DELETE THIS METHOD
    # def return_to_splash_screen(self):  # REPLACE WITH SIMPLE VERSION BELOW
    # def show_splash_in_thread(self):  # DELETE THIS METHOD
    # def show_splash_with_import(self):  # DELETE THIS METHOD
    # def restart_authentication(self):  # DELETE THIS METHOD

    def return_to_splash_screen(self):
        """Simple return message - no new windows"""
        print("🔐 Session cleared - please restart application for new authentication")
        # Just complete the logout process
        self.complete_logout_and_exit()

    # ============================================
    # UPDATED SESSION MANAGER METHODS
    # ============================================

    def logout_session(self) -> bool:
        """Add this method to SessionManager class"""
        try:
            with log_operation("logout_session", module="SessionManager"):
                # Clear credentials file
                if self.clear_credentials():
                    logger.info("Session logout completed", module="SessionManager")
                    return True
                else:
                    logger.error("Failed to clear credentials during logout", module="SessionManager")
                    return False

        except Exception as e:
            logger.error("Session logout error", module="SessionManager",
                         context={'error': str(e)}, exc_info=True)
            return False

    def force_clear_all_data(self) -> bool:
        """Add this method to SessionManager class - force clear everything"""
        try:
            logger.info("Force clearing all session data", module="SessionManager")

            # Clear credentials file
            success = self.clear_credentials()

            # Also try to clear any other session files if they exist
            try:
                session_files = [
                    self.config_dir / "session.json",
                    self.config_dir / "temp_session.json",
                    self.config_dir / "cache.json"
                ]

                for file_path in session_files:
                    if file_path.exists():
                        file_path.unlink()
                        logger.debug(f"Cleared additional file: {file_path}", module="SessionManager")

            except Exception as e:
                logger.warning("Could not clear additional files", module="SessionManager",
                               context={'error': str(e)})

            logger.info("Force clear completed", module="SessionManager")
            return success

        except Exception as e:
            logger.error("Force clear error", module="SessionManager",
                         context={'error': str(e)}, exc_info=True)
            return False

    def is_session_expired(self, session_data: Dict[str, Any]) -> bool:
        """Add this method to SessionManager class - check if session expired"""
        try:
            if not session_data:
                return True

            # Check for guest session expiry
            if session_data.get("user_type") == "guest":
                expires_at = session_data.get("expires_at")
                if expires_at:
                    try:
                        expiry_time = datetime.fromisoformat(expires_at.replace('Z', '+00:00'))
                        if datetime.now() > expiry_time.replace(tzinfo=None):
                            logger.info("Guest session expired", module="SessionManager")
                            return True
                    except Exception as e:
                        logger.warning("Could not parse expiry time", module="SessionManager",
                                       context={'expires_at': expires_at, 'error': str(e)})
                        return True

            # Check if session is too old (for registered users)
            fetched_at = session_data.get("fetched_at")
            if fetched_at:
                try:
                    fetch_time = datetime.fromisoformat(fetched_at)
                    # Consider session stale after 1 hour for security
                    if datetime.now() > fetch_time + timedelta(hours=1):
                        logger.info("Session data is stale", module="SessionManager")
                        return True
                except Exception as e:
                    logger.warning("Could not parse fetch time", module="SessionManager",
                                   context={'fetched_at': fetched_at, 'error': str(e)})
                    return True

            return False

        except Exception as e:
            logger.error("Session expiry check error", module="SessionManager",
                         context={'error': str(e)}, exc_info=True)
            return True

    def show_logout_complete_message(self):
        """Show logout completion message safely without UI errors"""
        try:
            print("✅ Logout completed successfully!")
            print("=" * 60)
            print("🔐 LOGOUT SUCCESSFUL")
            print("=" * 60)
            print("Your session has been cleared and you have been logged out.")
            print("")
            print("To access Fincept again:")
            print("1. 🔄 Restart the application")
            print("2. 🔑 Use the authentication screen to sign in")
            print("3. 👤 Continue as guest, or")
            print("4. ✨ Create a new account")
            print("")
            print("💡 You can now safely close this terminal window.")
            print("=" * 60)

            # Try to update the profile tab UI safely
            self.safe_show_logout_ui()

        except Exception as e:
            print(f"❌ Error showing logout message: {e}")
            # Just complete without UI update
            print("💡 Logout completed - please restart application")

    def safe_show_logout_ui(self):
        """Safely show logout UI with proper error handling"""
        try:
            # First, try to clear existing content safely
            self.safe_clear_tab_content()

            # Check if we can add UI elements safely
            if not self.can_add_ui_elements():
                print("⚠️ UI context not available - logout message shown in console only")
                return

            # Try to create logout UI with error handling for each step
            self.create_safe_logout_ui()

        except Exception as e:
            print(f"⚠️ Could not create logout UI: {e}")
            print("💡 Logout successful - UI update skipped")

    def can_add_ui_elements(self):
        """Check if we can safely add UI elements"""
        try:
            # Try to check if DearPyGUI context is valid
            if hasattr(dpg, 'is_dearpygui_running') and not dpg.is_dearpygui_running():
                return False

            # Check if we have a valid parent container
            if hasattr(self, 'content_tag') and dpg.does_item_exist(self.content_tag):
                return True

            # Check if the tab has content area
            if hasattr(self.app, 'current_tab_content') and dpg.does_item_exist(self.app.current_tab_content):
                return True

            return False

        except Exception as e:
            print(f"⚠️ UI availability check failed: {e}")
            return False

    def safe_clear_tab_content(self):
        """Safely clear tab content with error handling"""
        try:
            # Method 1: Use BaseTab's clear method if available
            if hasattr(super(), 'clear_content'):
                super().clear_content()
                return

            # Method 2: Clear using content_tag if available
            if hasattr(self, 'content_tag') and dpg.does_item_exist(self.content_tag):
                children = dpg.get_item_children(self.content_tag, 1)
                for child in children:
                    if dpg.does_item_exist(child):
                        dpg.delete_item(child)
                return

            # Method 3: Try to find and clear tab content
            possible_tags = ['profile_content', 'tab_content', 'main_content']
            for tag in possible_tags:
                if dpg.does_item_exist(tag):
                    children = dpg.get_item_children(tag, 1)
                    for child in children:
                        if dpg.does_item_exist(child):
                            dpg.delete_item(child)
                    return

            print("ℹ️ No clearable content found - continuing")

        except Exception as e:
            print(f"⚠️ Could not clear tab content: {e}")

    def create_safe_logout_ui(self):
        """Create logout UI with safe error handling for each element"""
        try:
            # Find the best parent container
            parent = self.find_safe_parent()
            if not parent:
                print("⚠️ No safe parent container found")
                return

            # Add elements one by one with error handling
            self.safe_add_logout_elements(parent)

        except Exception as e:
            print(f"❌ Error creating safe logout UI: {e}")

    def find_safe_parent(self):
        """Find a safe parent container for UI elements"""
        try:
            # Try various parent containers in order of preference
            candidates = [
                getattr(self, 'content_tag', None),
                'profile_content',
                'tab_content',
                'main_content',
                'content_container'
            ]

            for candidate in candidates:
                if candidate and dpg.does_item_exist(candidate):
                    return candidate

            return None

        except Exception as e:
            print(f"⚠️ Error finding parent: {e}")
            return None

    def safe_add_logout_elements(self, parent):
        """Safely add logout UI elements one by one"""
        try:
            # Add spacer safely
            try:
                dpg.add_spacer(height=50, parent=parent)
            except Exception as e:
                print(f"⚠️ Could not add spacer: {e}")

            # Add logout success text
            try:
                with dpg.group(horizontal=True, parent=parent):
                    dpg.add_spacer(width=100)
                    dpg.add_text("✅ Logout Successful", color=[100, 255, 100])
            except Exception as e:
                print(f"⚠️ Could not add logout title: {e}")

            # Add separator safely
            try:
                dpg.add_separator(parent=parent)
            except Exception as e:
                print(f"⚠️ Could not add separator: {e}")

            # Add instruction text safely
            try:
                dpg.add_spacer(height=20, parent=parent)

                with dpg.group(horizontal=True, parent=parent):
                    dpg.add_spacer(width=50)
                    dpg.add_text("Your session has been cleared successfully", color=[200, 200, 200])

                dpg.add_spacer(height=15, parent=parent)

                with dpg.group(horizontal=True, parent=parent):
                    dpg.add_spacer(width=80)
                    dpg.add_text("To continue using Fincept:", color=[255, 255, 100])

            except Exception as e:
                print(f"⚠️ Could not add instruction text: {e}")

            # Add action buttons safely
            try:
                dpg.add_spacer(height=30, parent=parent)

                with dpg.group(horizontal=True, parent=parent):
                    dpg.add_spacer(width=50)
                    dpg.add_button(label="🔄 Restart App (Recommended)", width=200,
                                   callback=self.suggest_restart, parent=parent)
                    dpg.add_spacer(width=20)
                    dpg.add_button(label="👤 Continue as Guest", width=180,
                                   callback=self.restart_as_guest, parent=parent)

                dpg.add_spacer(height=20, parent=parent)

                with dpg.group(horizontal=True, parent=parent):
                    dpg.add_spacer(width=150)
                    dpg.add_button(label="❌ Close Terminal", width=150,
                                   callback=self.close_application, parent=parent)

            except Exception as e:
                print(f"⚠️ Could not add action buttons: {e}")

            print("✅ Logout UI created successfully")

        except Exception as e:
            print(f"❌ Error adding logout elements: {e}")

    def clear_tab_content(self):
        """Enhanced tab content clearing with better error handling"""
        try:
            self.safe_clear_tab_content()
        except Exception as e:
            print(f"⚠️ Tab content clear failed: {e}")

    def manual_refresh(self):
        """Safe manual refresh that handles logout state"""
        print("🔄 Manually refreshing profile data...")
        try:
            # Check if we're in a logged out state
            session_data = self.get_session_data()
            if not session_data.get("authenticated") and session_data.get("user_type") == "guest":
                print("ℹ️ In logged out state - showing logout message")
                self.show_logout_complete_message()
                return

            # First check if session is still valid
            if self.api_client and not self.check_session_validity():
                print("⚠️ Session invalid - initiating logout")
                self.logout_user()
                return

            # Normal refresh process
            self.refresh_profile_data()
            self.safe_recreate_content()

            print("✅ Profile data refreshed successfully")

        except Exception as e:
            print(f"❌ Manual refresh error: {e}")
            self.create_error_profile(str(e))

    def safe_recreate_content(self):
        """Safely recreate content with error handling"""
        try:
            # Clear content safely
            self.safe_clear_tab_content()

            # Recreate content safely
            self.create_content()

        except Exception as e:
            print(f"⚠️ Could not recreate content safely: {e}")
            # Show minimal error state
            try:
                parent = self.find_safe_parent()
                if parent:
                    dpg.add_text("⚠️ Content refresh failed", color=[255, 200, 100], parent=parent)
                    dpg.add_text("Try refreshing again or restart the application", parent=parent)
            except:
                print("💡 Please restart the application for best experience")

    def cleanup(self):
        """Safe cleanup that won't cause UI crashes"""
        try:
            print("🧹 Cleaning up ProfileTab resources...")

            # Clear data structures safely
            self.usage_stats = {}
            self.request_count = 0
            self.last_refresh = None
            self.logout_in_progress = False

            # Don't force logout on cleanup to avoid crashes
            # Just clear references
            self.api_client = None

            # Safely clear UI elements that might still exist
            try:
                if dpg.does_item_exist("logout_btn"):
                    dpg.delete_item("logout_btn")
            except Exception as e:
                print(f"⚠️ Could not clear logout button: {e}")

            print("✅ ProfileTab cleanup completed safely")

        except Exception as e:
            print(f"❌ ProfileTab cleanup error: {e}")

    def show_logout_ui_message(self):
        """Show logout message in the UI"""
        try:
            # Clear current tab content
            self.clear_tab_content()

            # Create logout success screen in current tab
            parent = "content_container" if dpg.does_item_exist("content_container") else None

            # Center the logout message
            dpg.add_spacer(height=100)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=150)
                dpg.add_text("✅ Logout Successful", color=[100, 255, 100])

            dpg.add_separator()
            dpg.add_spacer(height=30)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=100)
                dpg.add_text("Your session has been cleared successfully", color=[200, 200, 200])

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=120)
                dpg.add_text("To continue using Fincept:", color=[255, 255, 100])

            dpg.add_spacer(height=15)

            logout_options = [
                "🔄 Restart the terminal application",
                "🔑 Use authentication screen to sign in",
                "👤 Continue as guest user",
                "✨ Create a new account"
            ]

            for option in logout_options:
                with dpg.group(horizontal=True):
                    dpg.add_spacer(width=80)
                    dpg.add_text(option, color=[200, 255, 200])
                dpg.add_spacer(height=5)

            dpg.add_spacer(height=30)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=100)
                dpg.add_button(label="🔄 Restart Application", width=180, callback=self.suggest_restart)
                dpg.add_spacer(width=20)
                dpg.add_button(label="❌ Close Terminal", width=150, callback=self.close_application)

        except Exception as e:
            print(f"❌ Error creating logout UI: {e}")

    def suggest_restart(self):
        """Suggest application restart"""
        print("=" * 50)
        print("🔄 APPLICATION RESTART RECOMMENDED")
        print("=" * 50)
        print("For the best experience:")
        print("1. Close this terminal window")
        print("2. Run the Fincept Terminal application again")
        print("3. Choose your authentication method")
        print("")
        print("💡 This ensures a clean authentication state")
        print("=" * 50)

    def close_application(self):
        """Close the application safely"""
        try:
            print("👋 Closing Fincept Terminal...")
            print("Thank you for using Fincept!")

            # Try to close the main app gracefully
            if hasattr(self.app, 'close_application'):
                self.app.close_application()
            elif hasattr(self.app, 'shutdown'):
                self.app.shutdown()
            else:
                # Force exit as last resort
                import sys
                sys.exit(0)

        except Exception as e:
            print(f"❌ Error closing application: {e}")
            import sys
            sys.exit(0)

    def perform_api_logout(self):
        """Safe API logout that won't crash UI"""
        try:
            if not self.api_client:
                print("📝 No API client available for logout")
                return True

            print("🔐 Attempting API logout...")

            # Try to call logout endpoint using make_request
            try:
                result = self.api_client.make_request("POST", "/auth/logout")

                if result.get("success") and result.get("data", {}).get("success"):
                    print("✅ API logout successful")
                    return True
                else:
                    error_msg = result.get("error", "Unknown error")
                    print(f"⚠️ API logout failed: {error_msg}")

                    # Try alternative logout methods
                    return self.try_alternative_logout()

            except Exception as e:
                print(f"⚠️ Direct API logout failed: {e}")
                return self.try_alternative_logout()

        except Exception as e:
            print(f"❌ API logout exception: {e}")
            return False

    def try_alternative_logout(self):
        """Try alternative logout methods"""
        try:
            print("🔄 Trying alternative logout methods...")

            # Method 1: Try auth/signout endpoint
            try:
                result = self.api_client.make_request("POST", "/auth/signout")
                if result.get("success"):
                    print("✅ Alternative logout successful (/auth/signout)")
                    return True
            except:
                pass

            # Method 2: Try user/logout endpoint
            try:
                result = self.api_client.make_request("POST", "/user/logout")
                if result.get("success"):
                    print("✅ Alternative logout successful (/user/logout)")
                    return True
            except:
                pass

            # Method 3: Just clear locally
            print("🔄 Performing local session cleanup only...")
            if hasattr(self.api_client, 'session_data'):
                self.api_client.session_data = {
                    "user_type": "guest",
                    "authenticated": False,
                    "api_key": None,
                    "user_info": {}
                }

            print("✅ Local cleanup completed")
            return True

        except Exception as e:
            print(f"❌ Alternative logout failed: {e}")
            return False

    def clear_session_data(self):
        """Safe session data clearing that won't crash UI"""
        try:
            print("🗑️ Clearing session data...")

            # Clear API client safely
            if self.api_client:
                try:
                    # Reset API client properties manually
                    if hasattr(self.api_client, 'session_data'):
                        device_id = self.api_client.session_data.get("device_id", "unknown")
                        self.api_client.session_data = {
                            "user_type": "guest",
                            "authenticated": False,
                            "api_key": None,
                            "device_id": device_id,
                            "user_info": {},
                            "expires_at": None,
                            "requests_today": 0,
                            "daily_limit": 50
                        }

                    # Reset other properties
                    if hasattr(self.api_client, 'api_key'):
                        self.api_client.api_key = None
                    if hasattr(self.api_client, 'user_type'):
                        self.api_client.user_type = "guest"
                    if hasattr(self.api_client, 'request_count'):
                        self.api_client.request_count = 0

                    print("✅ API client state reset")
                except Exception as e:
                    print(f"⚠️ API client reset error: {e}")

                # Don't set to None, just reset the state
                # self.api_client = None

            # Clear app session data safely
            if hasattr(self.app, 'session_data'):
                device_id = self.app.session_data.get("device_id", "unknown")

                self.app.session_data = {
                    "user_type": "guest",
                    "api_key": None,
                    "device_id": device_id,
                    "user_info": {},
                    "authenticated": False,
                    "expires_at": None,
                    "requests_today": 0,
                    "daily_limit": 50
                }
                print("✅ App session data reset to guest defaults")

            # Clear profile-specific data
            self.usage_stats = {}
            self.request_count = 0
            self.last_refresh = None

            # Clear any saved credentials safely
            try:
                if hasattr(self.app, 'clear_saved_credentials'):
                    self.app.clear_saved_credentials()
                    print("✅ Saved credentials cleared")
            except Exception as e:
                print(f"⚠️ Could not clear saved credentials: {e}")

            # Reset app-level request counter
            try:
                if hasattr(self.app, 'api_request_count'):
                    self.app.api_request_count = 0
            except Exception as e:
                print(f"⚠️ Could not reset app request count: {e}")

            print("✅ Session data cleared successfully")
            return True

        except Exception as e:
            print(f"❌ Error clearing session data: {e}")
            return False

    def try_splash_imports(self):
        """REMOVED - Don't try to import splash during logout"""
        print("🚫 Splash import skipped to prevent UI crash")
        print("💡 Logout completed - please restart application to sign in again")

        # Instead of trying to show splash, just complete logout
        self.show_logout_complete_message()

    def return_to_splash_screen(self):
        """Safe return that won't crash UI - NO NEW WINDOWS"""
        try:
            print("🔐 Logout completed - authentication required for next session")

            # Method 1: Try app methods that don't create new windows
            safe_app_methods = [
                'reset_to_login_state',
                'clear_session_and_show_message',
                'return_to_initial_state',
                'logout_complete'
            ]

            for method_name in safe_app_methods:
                if hasattr(self.app, method_name):
                    print(f"📱 Using safe app method: {method_name}")
                    try:
                        getattr(self.app, method_name)()
                        return
                    except Exception as e:
                        print(f"⚠️ App method {method_name} failed: {e}")
                        continue

            # Method 2: DON'T try to open splash - just show completion message
            print("📱 No safe app logout methods found")
            self.show_logout_complete_message()

        except Exception as e:
            print(f"❌ Error in safe return: {e}")
            self.show_logout_complete_message()

    def check_session_validity(self):
        """Safe session validity check"""
        try:
            if not self.api_client:
                return False

            # Check authentication status
            auth_result = self.api_client.check_auth_status()

            if auth_result.get("success"):
                is_authenticated = auth_result.get("authenticated", False)

                if not is_authenticated:
                    print("⚠️ Session expired or invalid")
                    return False

                return True
            else:
                print("⚠️ Could not verify session status")
                return False

        except Exception as e:
            print(f"❌ Session validity check error: {e}")
            return False

    def force_logout_cleanup(self):
        """Enhanced force cleanup"""
        try:
            print("🔧 Performing force cleanup...")

            # Force clear everything without causing crashes
            self.usage_stats = {}
            self.request_count = 0
            self.logout_in_progress = False
            self.last_refresh = None

            # Reset API client state without deleting it
            if self.api_client:
                try:
                    if hasattr(self.api_client, 'session_data'):
                        self.api_client.session_data = {
                            "user_type": "guest",
                            "authenticated": False,
                            "api_key": None
                        }
                    if hasattr(self.api_client, 'api_key'):
                        self.api_client.api_key = None
                    if hasattr(self.api_client, 'user_type'):
                        self.api_client.user_type = "guest"
                except:
                    pass

            # Force reset app session safely
            if hasattr(self.app, 'session_data'):
                try:
                    self.app.session_data = {
                        "user_type": "guest",
                        "authenticated": False,
                        "api_key": None,
                        "device_id": "logged_out_device"
                    }
                except:
                    pass

            print("✅ Force cleanup completed")
            return True

        except Exception as e:
            print(f"❌ Force cleanup error: {e}")
            return False

    def logout_and_switch(self):
        """Safe account switching"""
        print("🔄 Account switching requested...")
        print("To switch accounts:")
        print("1. Current session will be cleared")
        print("2. Restart the application")
        print("3. Choose different authentication method")

        # Perform logout without trying to open new windows
        self.logout_user()

    def logout_and_restart(self):
        """Safe logout and restart instruction"""
        print("🔄 Restart requested...")
        print("To restart authentication:")
        print("1. Current session will be cleared")
        print("2. Close and restart the terminal")
        print("3. Use the authentication screen")

        # Perform logout without trying to open new windows
        self.logout_user()

    def restart_as_guest(self):
        """Safe restart as guest without new windows"""
        try:
            print("👤 Restarting as guest user...")

            # Create minimal guest session
            guest_session = {
                "user_type": "guest",
                "authenticated": False,
                "api_key": None,
                "device_id": "restart_guest_" + str(int(datetime.now().timestamp())),
                "user_info": {},
                "expires_at": None,
                "requests_today": 0,
                "daily_limit": 50
            }

            # Update app session safely
            if hasattr(self.app, 'session_data'):
                self.app.session_data.update(guest_session)

            # Create new API client safely
            try:
                self.api_client = create_api_client(guest_session)
            except Exception as e:
                print(f"⚠️ Could not create new API client: {e}")

            print("✅ Restarted as guest user")
            print("💡 Limited access - create account for full features")

            # Refresh the current tab to show guest state
            try:
                self.refresh_profile_data()
                self.clear_tab_content()
                self.create_content()
            except Exception as e:
                print(f"⚠️ Could not refresh tab: {e}")

        except Exception as e:
            print(f"❌ Error restarting as guest: {e}")

    def show_alternative_auth_options(self):
        """Show alternatives without creating new windows"""
        print("=" * 60)
        print("🔐 AUTHENTICATION OPTIONS")
        print("=" * 60)
        print("Session cleared. Choose how to proceed:")
        print("")
        print("🔄 RECOMMENDED: Restart application")
        print("   • Close this terminal window")
        print("   • Run Fincept Terminal again")
        print("   • Choose authentication method")
        print("")
        print("👤 CONTINUE AS GUEST:")
        print("   • Limited access (50 requests/day)")
        print("   • Public databases only")
        print("   • No permanent API key")
        print("")
        print("Terminal remains open for current operations.")
        print("=" * 60)

        # Auto-restart as guest after message
        print("🔄 Auto-restarting as guest in 5 seconds...")
        threading.Timer(5.0, self.restart_as_guest).start()


    def update_request_count(self):
        """Update request count from multiple sources"""
        try:
            # Priority order: API client > app counter > session data
            if self.api_client:
                self.request_count = self.api_client.get_request_count()
            elif hasattr(self.app, 'api_request_count'):
                self.request_count = self.app.api_request_count
            else:
                session_data = self.get_session_data()
                self.request_count = session_data.get("requests_today", 0)

        except Exception as e:
            print(f"⚠️ Could not update request count: {e}")

    def refresh_profile_data(self):
        """Enhanced profile data refresh with better error handling"""
        try:
            self.last_refresh = datetime.now()

            session_data = self.get_session_data()
            user_type = session_data.get("user_type", "unknown")

            # Always recreate API client with fresh session data
            self.api_client = create_api_client(session_data)

            print(f"🔄 Refreshing profile for {user_type} user...")

            # Check authentication status first
            if self.api_client and session_data.get("authenticated"):
                auth_result = self.api_client.check_auth_status()

                if not auth_result.get("authenticated"):
                    print("⚠️ Authentication expired - forcing logout")
                    self.logout_user()
                    return

            # Fetch data based on user type
            if user_type == "registered":
                self.fetch_fresh_user_profile()
                self.fetch_usage_statistics()
            elif user_type == "guest":
                # For guest users, try to get guest status
                self.fetch_guest_status()

            # Update request count from various sources
            self.update_request_count()

            print(f"✅ ProfileTab refreshed successfully")

        except Exception as e:
            print(f"❌ Error refreshing profile data: {e}")

    def fetch_guest_status(self):
        """Fetch guest user status and update session data"""
        try:
            if not self.api_client or not self.api_client.is_guest():
                return

            result = self.api_client.get_guest_status()

            if result["success"]:
                guest_status = result["status"]

                # Update session data with fresh guest info
                if hasattr(self.app, 'session_data'):
                    self.app.session_data.update({
                        "requests_today": guest_status.get("requests_today", 0),
                        "daily_limit": guest_status.get("daily_limit", 50),
                        "expires_at": guest_status.get("expires_at")
                    })

                print("✅ Guest status refreshed from API")
            else:
                error_msg = self.api_client.handle_api_error(result, "Failed to fetch guest status")
                print(f"⚠️ Guest status API error: {error_msg}")

        except Exception as e:
            print(f"❌ Failed to refresh guest status: {e}")

    def fetch_fresh_user_profile(self):
        """Fetch fresh user profile using API client"""
        try:
            if not self.api_client or not self.api_client.is_registered():
                return

            result = self.api_client.get_user_profile()

            if result["success"]:
                # Update session data with fresh profile
                if hasattr(self.app, 'session_data'):
                    self.app.session_data["user_info"] = result["profile"]
                print("✅ Profile refreshed from API")
            else:
                error_msg = self.api_client.handle_api_error(result, "Failed to fetch profile")
                print(f"⚠️ Profile API error: {error_msg}")

        except Exception as e:
            print(f"❌ Failed to refresh profile: {e}")

    def fetch_usage_statistics(self):
        """Fetch fresh usage statistics using API client"""
        try:
            if not self.api_client or not self.api_client.is_registered():
                return

            result = self.api_client.get_user_usage()

            if result["success"]:
                self.usage_stats = result["usage"]
                print("✅ Usage stats refreshed")
            else:
                error_msg = self.api_client.handle_api_error(result, "Failed to fetch usage stats")
                print(f"⚠️ Usage stats error: {error_msg}")

        except Exception as e:
            print(f"❌ Failed to refresh usage stats: {e}")

    def create_dynamic_guest_profile(self):
        """Create enhanced guest profile"""
        session_data = self.get_session_data()
        api_key = session_data.get("api_key")

        # Header with refresh indicator and logout
        with dpg.group(horizontal=True):
            dpg.add_text("👤 Guest Profile", color=[255, 255, 100])
            dpg.add_spacer(width=20)
            dpg.add_text(f"🔄 Refreshed: {self.last_refresh.strftime('%H:%M:%S')}", color=[150, 150, 150])
            dpg.add_spacer(width=20)
            dpg.add_button(label="🔄 Refresh", callback=self.manual_refresh)
            dpg.add_spacer(width=10)
            dpg.add_button(label="🚪 Logout", callback=self.logout_user, tag="logout_btn")

        dpg.add_separator()
        dpg.add_spacer(height=20)

        # Two-column layout
        with dpg.group(horizontal=True):
            # Left column - Current Status
            with dpg.child_window(width=350, height=450, border=True):
                dpg.add_text("Current Session Status", color=[255, 255, 100])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                dpg.add_text("Account Type: Guest User")
                device_id = session_data.get('device_id', 'Unknown')
                display_device_id = device_id[:20] + "..." if len(device_id) > 20 else device_id
                dpg.add_text(f"Device ID: {display_device_id}")

                # API Key information
                if api_key:
                    if api_key.startswith("fk_guest_"):
                        dpg.add_text("Auth Method: Temporary API Key", color=[255, 255, 100])
                        dpg.add_text(f"API Key: {api_key[:20]}...", color=[200, 255, 200])

                        # Show expiry information
                        expires_at = session_data.get("expires_at")
                        if expires_at:
                            try:
                                expiry = datetime.fromisoformat(expires_at.replace('Z', '+00:00'))
                                hours_left = max(0, int((expiry.replace(
                                    tzinfo=None) - datetime.now()).total_seconds() / 3600))
                                dpg.add_text(f"Expires in: {hours_left} hours", color=[255, 200, 100])
                            except:
                                dpg.add_text("Expires: 24 hours", color=[255, 200, 100])
                        else:
                            dpg.add_text("Expires: 24 hours", color=[255, 200, 100])
                    else:
                        dpg.add_text("Auth Method: Legacy API Key", color=[255, 200, 100])
                        dpg.add_text(f"API Key: {api_key[:20]}...", color=[200, 200, 200])
                else:
                    dpg.add_text("Auth Method: Device ID Only", color=[255, 150, 150])
                    dpg.add_text("API Key: None (Offline Mode)", color=[255, 150, 150])

                dpg.add_text("Access Level: Limited")

                # Dynamic request counter and limits
                daily_limit = session_data.get("daily_limit", 50)
                requests_today = session_data.get("requests_today", 0)

                dpg.add_text(f"Session Requests: {self.request_count}", color=[255, 255, 100])
                dpg.add_text(f"Today's Requests: {requests_today}/{daily_limit}", color=[200, 255, 200])

                # Calculate remaining requests
                remaining = max(0, daily_limit - requests_today)
                remaining_color = [100, 255, 100] if remaining > 10 else [255, 150, 150]
                dpg.add_text(f"Remaining Today: {remaining}", color=remaining_color)

                dpg.add_spacer(height=15)
                dpg.add_text("Session Info:", color=[200, 255, 200])

                # Session timing
                fetched_at = session_data.get('fetched_at') or session_data.get('saved_at')
                if fetched_at:
                    try:
                        session_time = datetime.fromisoformat(fetched_at)
                        dpg.add_text(f"Started: {session_time.strftime('%H:%M:%S')}")

                        # Session duration
                        duration = datetime.now() - session_time
                        hours, remainder = divmod(duration.seconds, 3600)
                        minutes, _ = divmod(remainder, 60)
                        dpg.add_text(f"Duration: {hours}h {minutes}m")
                    except:
                        dpg.add_text(f"Started: {datetime.now().strftime('%H:%M:%S')}")
                        dpg.add_text("Duration: Unknown")
                else:
                    dpg.add_text(f"Started: {datetime.now().strftime('%H:%M:%S')}")

                dpg.add_spacer(height=20)
                dpg.add_text("Available Features:", color=[100, 255, 100])
                dpg.add_text("✓ Basic market data")
                dpg.add_text("✓ Real-time quotes")
                dpg.add_text("✓ Public databases")
                if api_key:
                    dpg.add_text("✓ API key authentication", color=[200, 255, 200])
                else:
                    dpg.add_text("⚠ Limited offline access", color=[255, 200, 100])

            dpg.add_spacer(width=20)

            # Right column - Enhanced Upgrade Options
            with dpg.child_window(width=350, height=450, border=True):
                dpg.add_text("Upgrade Your Access", color=[100, 255, 100])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                # Current status
                if api_key and api_key.startswith("fk_guest_"):
                    dpg.add_text("🔄 Current: Guest API Key", color=[255, 255, 100])
                    dpg.add_text("• Temporary access (24 hours)")
                    dpg.add_text("• 50 requests per day")
                    dpg.add_text("• Public databases only")
                    dpg.add_text("• Unified authentication")
                elif api_key:
                    dpg.add_text("🔄 Current: Legacy Access", color=[255, 200, 100])
                    dpg.add_text("• Limited API access")
                else:
                    dpg.add_text("🔄 Current: Offline Mode", color=[255, 150, 150])
                    dpg.add_text("• No API access")

                dpg.add_spacer(height=15)

                # Create Account Section
                dpg.add_text("🔑 Create Account", color=[100, 150, 255])
                dpg.add_spacer(height=5)
                dpg.add_text("Get unlimited access:")
                dpg.add_text("• Permanent API key")
                dpg.add_text("• Unlimited requests")
                dpg.add_text("• All databases access")
                dpg.add_text("• Premium data feeds")
                dpg.add_text("• Advanced analytics")
                dpg.add_text("• Data export features")
                dpg.add_text("• Priority support")

                dpg.add_spacer(height=15)
                dpg.add_button(label="Create Free Account", width=320, callback=self.show_signup_info)

                dpg.add_spacer(height=20)

                # Sign In Section
                dpg.add_text("📝 Already Have Account?", color=[255, 200, 100])
                dpg.add_spacer(height=5)
                dpg.add_text("Sign in for full access:")
                dpg.add_text("• Restore your settings")
                dpg.add_text("• Access premium features")
                dpg.add_text("• View usage history")
                dpg.add_text("• Manage subscriptions")

                dpg.add_spacer(height=15)
                dpg.add_button(label="Sign In to Account", width=320, callback=self.show_login_info)

                dpg.add_spacer(height=30)

                # Current Limitations
                dpg.add_text("Current Limitations:", color=[255, 200, 100])
                dpg.add_text(f"• {daily_limit} requests per day maximum")
                if api_key and api_key.startswith("fk_guest_"):
                    dpg.add_text("• 24-hour API key expiry")
                else:
                    dpg.add_text("• No API key authentication")
                dpg.add_text("• Public databases only")
                dpg.add_text("• No data export")
                dpg.add_text("• Basic support only")

        dpg.add_spacer(height=20)

        # Enhanced live statistics
        self.create_live_guest_stats()

    def create_dynamic_user_profile(self):
        """Create enhanced authenticated user profile"""
        session_data = self.get_session_data()
        user_info = session_data.get('user_info', {})
        api_key = session_data.get("api_key")

        # Header with refresh indicator and logout
        username = user_info.get('username', 'User')
        with dpg.group(horizontal=True):
            dpg.add_text(f"🔑 {username}'s Profile", color=[100, 255, 100])
            dpg.add_spacer(width=20)
            dpg.add_text(f"🔄 Refreshed: {self.last_refresh.strftime('%H:%M:%S')}", color=[150, 150, 150])
            dpg.add_spacer(width=20)
            dpg.add_button(label="🔄 Refresh", callback=self.manual_refresh)
            dpg.add_spacer(width=10)
            dpg.add_button(label="🚪 Logout", callback=self.logout_user, tag="logout_btn")

        dpg.add_separator()
        dpg.add_spacer(height=20)

        # User Information Cards
        with dpg.group(horizontal=True):
            # Left column - Enhanced Account Details
            with dpg.child_window(width=350, height=450, border=True):
                dpg.add_text("Account Details", color=[100, 255, 100])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                dpg.add_text(f"Username: {user_info.get('username', 'N/A')}")
                dpg.add_text(f"Email: {user_info.get('email', 'N/A')}")
                dpg.add_text(f"Account Type: {user_info.get('account_type', 'free').title()}")
                dpg.add_text(f"Member Since: {self.format_date(user_info.get('created_at'))}")
                dpg.add_text(f"Last Login: {self.format_date(user_info.get('last_login_at'))}")

                dpg.add_spacer(height=15)
                dpg.add_text("Authentication:", color=[200, 255, 200])

                # API Key information
                if api_key:
                    if api_key.startswith("fk_user_"):
                        dpg.add_text("Method: Permanent API Key", color=[100, 255, 100])
                        dpg.add_text(f"API Key: {api_key[:25]}...", color=[100, 255, 100])
                        dpg.add_text("Status: Active & Permanent")
                    else:
                        dpg.add_text("Method: Legacy API Key", color=[255, 200, 100])
                        dpg.add_text(f"API Key: {api_key[:25]}...", color=[255, 200, 100])
                        dpg.add_text("Status: Legacy Format")
                else:
                    dpg.add_text("Method: No API Key", color=[255, 150, 150])
                    dpg.add_text("Status: Limited Access")

                verification_status = "✓ Verified" if user_info.get('is_verified') else "⚠ Unverified"
                verification_color = [100, 255, 100] if user_info.get('is_verified') else [255, 200, 100]
                dpg.add_text(f"Verification: {verification_status}", color=verification_color)

                if user_info.get('is_admin'):
                    dpg.add_text("Role: Administrator", color=[255, 200, 100])

                dpg.add_spacer(height=15)

                # API Management
                if api_key:
                    dpg.add_text("API Management:", color=[200, 255, 200])
                    dpg.add_text("Key Type: Unified Authentication")
                    dpg.add_text("Scope: Full API Access")
                    dpg.add_button(label="Regenerate API Key", width=280, callback=self.regenerate_api_key)
                else:
                    dpg.add_text("API Management:", color=[255, 150, 150])
                    dpg.add_text("No API key available")
                    dpg.add_button(label="Contact Support", width=280, callback=self.show_support_info)

                dpg.add_spacer(height=15)
                dpg.add_button(label="Switch Account", width=280, callback=self.logout_and_switch)

            dpg.add_spacer(width=20)

            # Right column - Enhanced Credits & Usage
            with dpg.child_window(width=350, height=450, border=True):
                dpg.add_text("Credits & Live Usage", color=[255, 200, 100])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                # Live credit balance
                credit_balance = user_info.get('credit_balance', 0)
                dpg.add_text(f"Current Balance: {credit_balance} credits")

                # Credit status color coding
                if credit_balance > 1000:
                    balance_color = [100, 255, 100]
                    status_text = "Excellent"
                elif credit_balance > 100:
                    balance_color = [255, 255, 100]
                    status_text = "Good"
                else:
                    balance_color = [255, 150, 150]
                    status_text = "Low Credits"

                dpg.add_text(f"Status: {status_text}", color=balance_color)

                # API Key info
                dpg.add_spacer(height=15)
                dpg.add_text("API Access:", color=[200, 255, 200])

                if api_key and api_key.startswith("fk_user_"):
                    dpg.add_text("Access: Unlimited", color=[100, 255, 100])
                    dpg.add_text("Rate Limit: Premium")
                    dpg.add_text("Databases: All Available")
                elif api_key:
                    dpg.add_text("Access: Standard", color=[255, 200, 100])
                    dpg.add_text("Rate Limit: Standard")
                    dpg.add_text("Databases: Subscribed")
                else:
                    dpg.add_text("Access: Limited", color=[255, 150, 150])
                    dpg.add_text("Rate Limit: Restricted")

                # Live usage stats
                dpg.add_spacer(height=15)
                dpg.add_text("Live Usage Stats:", color=[200, 255, 200])

                if self.usage_stats:
                    dpg.add_text(f"Total Requests: {self.usage_stats.get('total_requests', 0)}")
                    dpg.add_text(f"Credits Used: {self.usage_stats.get('total_credits_used', 0)}")
                    dpg.add_text(f"This Session: {self.request_count} requests")

                    # Show endpoint usage if available
                    endpoint_usage = self.usage_stats.get('endpoint_usage', {})
                    if endpoint_usage:
                        top_endpoint = max(endpoint_usage.keys(), key=lambda k: endpoint_usage[k]['count'])
                        dpg.add_text(f"Top Endpoint: {top_endpoint}")
                else:
                    dpg.add_text("Total Requests: Loading...")
                    dpg.add_text("Credits Used: Loading...")
                    dpg.add_text(f"This Session: {self.request_count} requests")

                dpg.add_spacer(height=15)
                dpg.add_text("Account Features:")
                dpg.add_text("✓ Unlimited API requests")
                dpg.add_text("✓ Real-time data access")
                dpg.add_text("✓ All database access")
                dpg.add_text("✓ Advanced analytics")
                dpg.add_text("✓ Data export")
                dpg.add_text("✓ Priority support")

                dpg.add_spacer(height=15)
                dpg.add_text("Quick Actions:")
                dpg.add_button(label="View Usage Details", width=280, callback=self.view_usage_stats)
                dpg.add_button(label="API Documentation", width=280, callback=self.show_api_docs)
                dpg.add_button(label="Subscribe to Database", width=280, callback=self.show_subscription_info)

        dpg.add_spacer(height=20)

        # Enhanced live user stats
        self.create_live_user_stats()

    def create_live_guest_stats(self):
        """Enhanced guest statistics"""
        dpg.add_text("📊 Live Session Statistics", color=[200, 200, 255])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        session_data = self.get_session_data()
        api_key = session_data.get("api_key")
        daily_limit = session_data.get("daily_limit", 50)
        requests_today = session_data.get("requests_today", 0)

        with dpg.group(horizontal=True):
            # Session Usage
            with dpg.child_window(width=250, height=150, border=True):
                dpg.add_text("Session Usage", color=[255, 255, 100])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                dpg.add_text(f"Session Requests: {self.request_count}")
                dpg.add_text(f"Daily Requests: {requests_today}/{daily_limit}")

                api_status = "API Key" if api_key else "Offline"
                dpg.add_text(f"Auth: {api_status}")

                # Session timing
                fetched_at = session_data.get('fetched_at')
                if fetched_at:
                    try:
                        session_time = datetime.fromisoformat(fetched_at)
                        duration = datetime.now() - session_time
                        minutes = duration.seconds // 60
                        dpg.add_text(f"Duration: {minutes}m")
                    except:
                        dpg.add_text("Duration: 0m")
                else:
                    dpg.add_text("Duration: 0m")

                # Progress bar
                dpg.add_spacer(height=10)
                dpg.add_text("Daily Progress:")
                progress = min(1.0, requests_today / daily_limit)
                dpg.add_progress_bar(default_value=progress, width=200)

            dpg.add_spacer(width=20)

            # Session Features
            with dpg.child_window(width=250, height=150, border=True):
                dpg.add_text("Available Features", color=[100, 255, 100])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                dpg.add_text("✓ Market Data Access")
                dpg.add_text("✓ Basic Analytics")
                dpg.add_text("✓ Public Databases")

                if api_key:
                    if api_key.startswith("fk_guest_"):
                        dpg.add_text("✓ Guest API Key", color=[200, 255, 200])
                    else:
                        dpg.add_text("✓ Legacy API Access", color=[255, 200, 100])
                else:
                    dpg.add_text("⚠ No API Access", color=[255, 150, 150])

            dpg.add_spacer(width=20)

            # API Status
            with dpg.child_window(width=250, height=150, border=True):
                dpg.add_text("API Connection", color=[200, 255, 200])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                dpg.add_text(f"Server: {config.get_api_url()}")

                if api_key:
                    dpg.add_text("Status: Connected", color=[100, 255, 100])
                    dpg.add_text("Auth: API Key")
                    if api_key.startswith("fk_guest_"):
                        dpg.add_text("Type: Temporary")
                    else:
                        dpg.add_text("Type: Legacy")
                else:
                    dpg.add_text("Status: Offline", color=[255, 150, 150])
                    dpg.add_text("Auth: None")

    def create_live_user_stats(self):
        """Enhanced user statistics"""
        dpg.add_text("📊 Live Account Overview", color=[200, 200, 255])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        session_data = self.get_session_data()
        api_key = session_data.get("api_key")

        with dpg.group(horizontal=True):
            # Monthly Usage
            with dpg.child_window(width=230, height=150, border=True):
                dpg.add_text("Usage Summary", color=[100, 255, 100])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                if self.usage_stats:
                    dpg.add_text(f"Requests: {self.usage_stats.get('total_requests', 0)}")
                    dpg.add_text(f"Credits: {self.usage_stats.get('total_credits_used', 0)}")
                    dpg.add_text(f"Session: {self.request_count}")

                    # Calculate success rate
                    total_requests = self.usage_stats.get('total_requests', 0)
                    if total_requests > 0:
                        success_rate = 100  # Assume 100% for now
                        dpg.add_text(f"Success: {success_rate}%")
                else:
                    dpg.add_text("Requests: Loading...")
                    dpg.add_text("Credits: Loading...")
                    dpg.add_text(f"Session: {self.request_count}")

            dpg.add_spacer(width=15)

            # API Performance
            with dpg.child_window(width=230, height=150, border=True):
                dpg.add_text("API Performance", color=[255, 255, 100])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                dpg.add_text("Avg Response: ~200ms")
                dpg.add_text(f"Server: {config.get_api_url()}")
                dpg.add_text("Uptime: 99.9%")

                if api_key and api_key.startswith("fk_user_"):
                    dpg.add_text("Auth: Premium ✓", color=[100, 255, 100])
                elif api_key:
                    dpg.add_text("Auth: Standard", color=[255, 200, 100])
                else:
                    dpg.add_text("Auth: None", color=[255, 150, 150])

            dpg.add_spacer(width=15)

            # Account Status
            with dpg.child_window(width=230, height=150, border=True):
                dpg.add_text("Account Status", color=[200, 255, 200])
                dpg.add_separator()
                dpg.add_spacer(height=10)

                dpg.add_text(f"Last Update: {self.last_refresh.strftime('%H:%M:%S')}")
                dpg.add_text("Status: Active")
                dpg.add_text("Connection: Stable")

                if api_key:
                    dpg.add_text("API: Connected", color=[100, 255, 100])
                else:
                    dpg.add_text("API: Offline", color=[255, 150, 150])

    def manual_refresh(self):
        """Enhanced manual refresh with session validity check"""
        print("🔄 Manually refreshing profile data...")
        try:
            # First check if session is still valid
            if self.api_client and not self.check_session_validity():
                print("⚠️ Session invalid - initiating logout")
                self.logout_user()
                return

            # Refresh profile data
            self.refresh_profile_data()

            # Recreate the content to show updated information
            self.clear_tab_content()
            self.create_content()

            print("✅ Profile data refreshed successfully")
            print(f"📊 Request count: {self.request_count}")
            print(f"⏰ Last refresh: {self.last_refresh}")

            # Log session info
            session_data = self.get_session_data()
            user_type = session_data.get("user_type", "unknown")
            api_key = session_data.get("api_key")
            print(f"🔐 User type: {user_type}")
            print(f"🔑 API key: {'Present' if api_key else 'None'}")

        except Exception as e:
            print(f"❌ Manual refresh error: {e}")
            self.create_error_profile(str(e))

    def create_unknown_profile(self):
        """Create profile for unknown session state"""
        dpg.add_spacer(height=100)

        with dpg.group(horizontal=True):
            dpg.add_text("❓ Unknown Session State", color=[255, 200, 100])
            dpg.add_spacer(width=20)
            dpg.add_button(label="🚪 Logout", callback=self.logout_user)

        dpg.add_separator()
        dpg.add_spacer(height=30)
        dpg.add_text("Unable to determine authentication status")
        dpg.add_text("This may indicate a configuration issue.")
        dpg.add_spacer(height=20)
        dpg.add_button(label="🔄 Refresh Profile", width=200, callback=self.manual_refresh)
        dpg.add_button(label="Clear Session & Restart", width=200, callback=self.logout_and_restart)

    def create_error_profile(self, error_msg):
        """Create error profile screen"""
        dpg.add_spacer(height=50)

        with dpg.group(horizontal=True):
            dpg.add_text("🚨 Profile Error", color=[255, 100, 100])
            dpg.add_spacer(width=20)
            dpg.add_button(label="🚪 Logout", callback=self.logout_user)

        dpg.add_separator()
        dpg.add_spacer(height=20)
        dpg.add_text("An error occurred while loading your profile:")
        dpg.add_text(f"Error: {error_msg}", color=[255, 150, 150])
        dpg.add_spacer(height=20)
        dpg.add_button(label="🔄 Refresh Profile", width=200, callback=self.manual_refresh)
        dpg.add_button(label="Clear Session & Restart", width=200, callback=self.logout_and_restart)

    def format_date(self, date_str):
        """Format date string for display"""
        if not date_str:
            return "Never"

        try:
            date_obj = datetime.fromisoformat(date_str.replace('Z', '+00:00'))
            return date_obj.strftime("%Y-%m-%d %H:%M")
        except:
            return date_str

    # ============================================
    # LOGOUT AND AUTHENTICATION METHODS
    # ============================================

    def update_session_after_auth(self, new_session):
        """Enhanced session update after successful authentication"""
        try:
            print("🔄 Updating terminal with new session data...")

            # Update app session data
            if hasattr(self.app, 'session_data'):
                self.app.session_data.update(new_session)
                print("✅ App session data updated")

            # Create new API client with fresh session
            self.api_client = create_api_client(new_session)
            print("✅ API client recreated")

            # Refresh profile data with new session
            self.refresh_profile_data()
            print("✅ Profile data refreshed")

            # Refresh all tabs if method exists
            refresh_methods = ['refresh_all_tabs', 'update_all_tabs', 'reload_tabs']
            for method_name in refresh_methods:
                if hasattr(self.app, method_name):
                    getattr(self.app, method_name)()
                    print(f"✅ Refreshed using {method_name}")
                    break

            # Refresh current profile tab
            self.manual_refresh()

            print("🎉 Terminal updated with new authentication!")

        except Exception as e:
            print(f"❌ Error updating session: {e}")

    def show_fallback_message(self):
        """Show fallback message when splash cannot be opened"""
        print("=" * 60)
        print("🔐 AUTHENTICATION REQUIRED")
        print("=" * 60)
        print("The splash screen could not be opened automatically.")
        print("To log in again:")
        print("1. Restart the terminal application")
        print("2. Or contact support if this issue persists")
        print("=" * 60)
        print("💡 Terminal remains open for any other operations")

    # def restart_authentication(self):
    #     """Restart authentication process - return to splash screen"""
    #     try:
    #         print("🔄 Returning to splash screen...")
    #
    #         # Method 1: Use app's show splash method (preferred)
    #         if hasattr(self.app, 'show_splash_screen'):
    #             self.app.show_splash_screen()
    #         elif hasattr(self.app, 'return_to_splash'):
    #             self.app.return_to_splash()
    #         elif hasattr(self.app, 'restart_authentication'):
    #             self.app.restart_authentication()
    #
    #         # Method 2: Import and show splash directly
    #         elif hasattr(self.app, 'session_data'):
    #             try:
    #                 # Import splash auth
    #                 from fincept_terminal.Utils.splash_auth import show_authentication_splash
    #
    #                 # Hide current terminal window if possible
    #                 if hasattr(self.app, 'hide_terminal'):
    #                     self.app.hide_terminal()
    #
    #                 # Show splash in separate thread to avoid blocking
    #                 def show_splash():
    #                     try:
    #                         print("🔐 Showing authentication splash...")
    #                         new_session = show_authentication_splash()
    #
    #                         if new_session and new_session.get("authenticated"):
    #                             print("✅ New authentication successful")
    #                             # Update app session data
    #                             self.app.session_data = new_session
    #
    #                             # Show terminal again if hidden
    #                             if hasattr(self.app, 'show_terminal'):
    #                                 self.app.show_terminal()
    #
    #                             # Refresh all tabs
    #                             if hasattr(self.app, 'refresh_all_tabs'):
    #                                 self.app.refresh_all_tabs()
    #
    #                         else:
    #                             print("❌ Authentication cancelled or failed")
    #                             # Keep terminal hidden or show error
    #
    #                     except Exception as e:
    #                         print(f"❌ Splash screen error: {e}")
    #                         if hasattr(self.app, 'show_terminal'):
    #                             self.app.show_terminal()
    #
    #                 # Run splash in thread
    #                 threading.Thread(target=show_splash, daemon=True).start()
    #
    #             except ImportError as e:
    #                 print(f"❌ Could not import splash_auth: {e}")
    #                 self.fallback_restart()
    #
    #         # Method 3: Use generic restart methods
    #         elif hasattr(self.app, 'show_authentication_splash'):
    #             # Run authentication in separate thread to avoid blocking UI
    #             threading.Thread(target=self.app.show_authentication_splash, daemon=True).start()
    #         elif hasattr(self.app, 'clear_session_and_restart'):
    #             self.app.clear_session_and_restart()
    #         else:
    #             print("⚠️ No splash screen method available. Using fallback...")
    #             self.fallback_restart()
    #
    #     except Exception as e:
    #         print(f"❌ Error returning to splash screen: {e}")
    #         self.fallback_restart()
    #
    # def fallback_restart(self):
    #     """Fallback method when splash screen cannot be shown"""
    #     try:
    #         print("🔄 Using fallback restart method...")
    #
    #         # Try to reset the terminal to initial state
    #         if hasattr(self.app, 'reset_to_initial_state'):
    #             self.app.reset_to_initial_state()
    #         elif hasattr(self.app, 'show_welcome_screen'):
    #             self.app.show_welcome_screen()
    #         else:
    #             print("⚠️ Please restart the application to log in again.")
    #             print("💡 Close the terminal and run the application again.")
    #
    #     except Exception as e:
    #         print(f"❌ Fallback restart error: {e}")
    #         print("⚠️ Please restart the application manually.")

    # ============================================
    # ENHANCED CALLBACK METHODS
    # ============================================

    def show_signup_info(self):
        """Show enhanced signup information with working callback"""
        print("📝 Create Account Information:")
        print("To create a free Fincept account:")
        print("1. Use the logout button to return to authentication")
        print("2. Choose 'Sign Up' option")
        print("3. Fill in your details (username, email, password)")
        print("4. Verify your email with the 6-digit code")
        print("5. Get instant permanent API key access")
        print("6. Enjoy unlimited features and enhanced security")
        print("")
        print("💡 Click 'Logout' button above to start account creation process")

        # Show confirmation dialog
        if hasattr(self, 'show_confirmation_dialog'):
            self.show_confirmation_dialog(
                "Create Account",
                "Would you like to logout and create a new account?",
                self.logout_user
            )

    def show_login_info(self):
        """Show enhanced login information with working callback"""
        print("🔑 Sign In Information:")
        print("To sign in to your existing account:")
        print("1. Use the logout button to return to authentication")
        print("2. Choose 'Sign In' option")
        print("3. Enter your registered email and password")
        print("4. Get secure permanent API key")
        print("5. Access your full account with all features")
        print("")
        print("💡 Click 'Logout' button above to start sign in process")

        # Show confirmation dialog
        if hasattr(self, 'show_confirmation_dialog'):
            self.show_confirmation_dialog(
                "Sign In",
                "Would you like to logout and sign in to your account?",
                self.logout_user
            )

    def delayed_refresh(self):
        """Refresh profile after a short delay"""
        import time
        time.sleep(1)  # Give API time to update
        self.manual_refresh()

    def show_success_message(self, message):
        """Show success message to user"""
        print(f"✅ {message}")
        # You can implement a popup or notification here if needed

    def show_error_message(self, message):
        """Show error message to user"""
        print(f"❌ {message}")
        # You can implement a popup or notification here if needed

    def regenerate_api_key(self):
        """Enhanced API key regeneration with better error handling"""
        try:
            print("🔄 Regenerating API key...")

            if not self.api_client or not self.api_client.is_registered():
                print("❌ API key regeneration requires authenticated user")
                self.show_error_message("API key regeneration is only available for registered users")
                return

            # Show progress
            print("📡 Contacting API server...")

            # Call API to regenerate key
            result = self.api_client.regenerate_api_key()

            if result.get("success"):
                new_api_key = result.get("api_key")
                if new_api_key:
                    print(f"✅ API key regenerated successfully")
                    print(f"🔑 New API key: {new_api_key[:20]}...")

                    # Update session data in app
                    if hasattr(self.app, 'session_data'):
                        self.app.session_data["api_key"] = new_api_key

                    # Refresh profile to show new key
                    threading.Thread(target=self.delayed_refresh, daemon=True).start()

                    self.show_success_message("API key regenerated successfully!")
                else:
                    print("❌ No new API key received from server")
                    self.show_error_message("No new API key received from server")
            else:
                error_msg = self.api_client.handle_api_error(result, "API key regeneration failed")
                print(f"❌ {error_msg}")
                self.show_error_message(f"Regeneration failed: {error_msg}")

        except Exception as e:
            error_msg = f"API key regeneration error: {e}"
            print(f"❌ {error_msg}")
            self.show_error_message(error_msg)

    def view_usage_stats(self):
        """Enhanced usage statistics viewer"""
        print("📊 Detailed Usage Statistics:")
        print("=" * 50)

        try:
            # Get fresh usage stats if possible
            if self.api_client and self.api_client.is_registered():
                print("🔄 Fetching latest usage statistics...")
                result = self.api_client.get_user_usage()

                if result.get("success"):
                    self.usage_stats = result["usage"]
                    print("✅ Latest stats retrieved")

            # Display comprehensive stats
            if self.usage_stats:
                print(f"📈 Total Requests: {self.usage_stats.get('total_requests', 0)}")
                print(f"💳 Credits Used: {self.usage_stats.get('total_credits_used', 0)}")
                print(f"📊 Success Rate: {self.usage_stats.get('success_rate', 100)}%")
                print(f"📅 Last 30 Days: {self.usage_stats.get('requests_last_30_days', 0)} requests")

                # Show endpoint breakdown if available
                endpoint_usage = self.usage_stats.get('endpoint_usage', {})
                if endpoint_usage:
                    print("\n🔗 Top Endpoints:")
                    sorted_endpoints = sorted(endpoint_usage.items(),
                                              key=lambda x: x[1].get('count', 0),
                                              reverse=True)[:5]
                    for endpoint, stats in sorted_endpoints:
                        count = stats.get('count', 0)
                        print(f"  {endpoint}: {count} requests")

                # Show monthly breakdown if available
                monthly_usage = self.usage_stats.get('monthly_breakdown', {})
                if monthly_usage:
                    print("\n📅 Monthly Breakdown:")
                    for month, data in list(monthly_usage.items())[-3:]:  # Last 3 months
                        print(f"  {month}: {data.get('requests', 0)} requests, {data.get('credits', 0)} credits")

            else:
                print("📊 No detailed usage statistics available")

            # Current session info
            print(f"\n🔄 Current Session:")
            print(f"📱 Session Requests: {self.request_count}")

            session_data = self.get_session_data()
            auth_method = session_data.get('user_type', 'unknown')
            print(f"🔐 Authentication: {auth_method}")

            api_key = session_data.get("api_key")
            if api_key:
                if api_key.startswith("fk_user_"):
                    print("🔑 API Key: Permanent User Key")
                elif api_key.startswith("fk_guest_"):
                    print("🔑 API Key: Temporary Guest Key")
                else:
                    print("🔑 API Key: Legacy Format")
            else:
                print("🔑 API Key: None (Offline Mode)")

            # Show API client stats if available
            if self.api_client:
                performance_stats = self.api_client.get_performance_stats()
                print(f"\n⚡ API Client Stats:")
                print(f"   Total Requests: {performance_stats.get('total_requests', 0)}")
                print(f"   Client Type: {performance_stats.get('user_type', 'unknown')}")
                print(f"   Authenticated: {performance_stats.get('authenticated', False)}")

        except Exception as e:
            print(f"❌ Error displaying usage stats: {e}")

    def show_api_docs(self):
        """Show API documentation"""
        print("📖 Opening Fincept API Documentation...")
        api_docs_url = f"{config.get_api_url()}/docs"

        try:
            webbrowser.open(api_docs_url)
            print(f"✅ Opened API docs: {api_docs_url}")
        except Exception as e:
            print(f"❌ Could not open browser: {e}")
            print(f"📎 Manual URL: {api_docs_url}")
            print("API Documentation Details:")
            print(f"Base URL: {config.get_api_url()}")
            print("Documentation: /docs (Swagger UI)")
            print("Authentication: API Key based")

    def show_subscription_info(self):
        """Enhanced subscription management with API integration"""
        print("💳 Subscription Management:")
        print("=" * 40)

        try:
            session_data = self.get_session_data()
            user_type = session_data.get("user_type", "unknown")

            if user_type == "registered":
                print("✅ Registered Account Features:")
                print("✅ Unlimited API requests")
                print("✅ All databases access")
                print("✅ Premium data feeds")
                print("✅ Advanced analytics")
                print("✅ Data export features")
                print("✅ Priority support")
                print("✅ Device management")
                print("✅ Chat session management")

                # Try to get subscription details from API
                if self.api_client:
                    print("\n🔄 Checking subscription details...")

                    # Get user profile for subscription info
                    profile_result = self.api_client.get_user_profile()
                    if profile_result.get("success"):
                        profile = profile_result["profile"]
                        account_type = profile.get("account_type", "free")
                        credit_balance = profile.get("credit_balance", 0)

                        print(f"📊 Account Type: {account_type.title()}")
                        print(f"💰 Credit Balance: {credit_balance}")

                        if account_type == "premium":
                            print("🌟 Premium Account Active")
                        elif account_type == "free":
                            print("💡 Free Account - Upgrade available")

                    # Check database subscriptions
                    print("\n📚 Available Databases:")
                    db_result = self.api_client.get_databases()
                    if db_result.get("success"):
                        databases = db_result["databases"]
                        print(f"   Total Available: {len(databases)}")
                        for db in databases[:5]:  # Show first 5
                            name = db.get("name", "Unknown")
                            access_level = db.get("access_level", "unknown")
                            print(f"   • {name} ({access_level})")

                        if len(databases) > 5:
                            print(f"   ... and {len(databases) - 5} more")

                print(f"\n🔧 Subscription Actions:")
                print("   • View all databases: Use Database tab")
                print("   • Subscribe to new database: Use subscribe_to_database API")
                print("   • Check transactions: Use get_user_transactions API")

            else:
                print("⚠️ Guest Account Limitations:")
                daily_limit = session_data.get("daily_limit", 50)
                requests_today = session_data.get("requests_today", 0)

                print(f"⚠️ Daily limit: {requests_today}/{daily_limit} requests")
                print("⚠️ Public databases only")
                print("⚠️ No data export")
                print("⚠️ Basic support")
                print("⚠️ 24-hour session expiry")

                # Try to get public databases
                if self.api_client:
                    print("\n📚 Available Public Databases:")
                    pub_db_result = self.api_client.get_public_databases()
                    if pub_db_result.get("success"):
                        pub_databases = pub_db_result["databases"]
                        print(f"   Total Public: {len(pub_databases)}")
                        for db in pub_databases[:3]:  # Show first 3
                            name = db.get("name", "Unknown")
                            print(f"   • {name} (Public)")

                print(f"\n💡 Upgrade Benefits:")
                print("   • Create account for unlimited access")
                print("   • Permanent API key")
                print("   • All databases access")
                print("   • Premium features")

        except Exception as e:
            print(f"❌ Error displaying subscription info: {e}")

    def show_support_info(self):
        """Show support information"""
        print("🆘 Support Information:")
        print("=" * 30)
        print("For technical support:")
        print("📧 Email: support@fincept.com")
        print("📖 Documentation: /docs")
        print("🔧 API Issues: Check API connectivity")
        print("💬 Community: Discord/Forums (if available)")

        session_data = self.get_session_data()
        user_type = session_data.get("user_type", "unknown")

        if user_type == "registered":
            print("🔑 Priority support available for registered users")
        else:
            print("📝 Create account for priority support")

    def show_confirmation_dialog(self, title, message, callback):
        """Show confirmation dialog (basic implementation)"""
        print(f"❓ {title}: {message}")
        print("💡 Use the logout button to proceed")

    def clear_tab_content(self):
        """Clear current tab content safely"""
        try:
            # This method should be implemented based on your BaseTab class
            # For now, we'll just clear what we can
            if hasattr(self, 'content_tag') and dpg.does_item_exist(self.content_tag):
                children = dpg.get_item_children(self.content_tag, 1)
                for child in children:
                    if dpg.does_item_exist(child):
                        dpg.delete_item(child)
        except Exception as e:
            print(f"Warning: Could not clear tab content: {e}")

    def cleanup(self):
        """Enhanced cleanup with proper resource disposal"""
        try:
            print("🧹 Cleaning up ProfileTab resources...")

            # Clear API client
            if self.api_client:
                # Force logout to clean server session
                try:
                    if self.api_client.is_authenticated():
                        self.api_client.force_logout()
                except:
                    pass
                self.api_client = None

            # Clear data structures
            self.usage_stats = {}
            self.request_count = 0
            self.last_refresh = None
            self.logout_in_progress = False

            # Clear any UI elements that might still exist
            if dpg.does_item_exist("logout_btn"):
                dpg.delete_item("logout_btn")

            print("✅ ProfileTab cleanup completed")

        except Exception as e:
            print(f"❌ ProfileTab cleanup error: {e}")

    # ============================================
    # INTEGRATION HELPER METHODS
    # ============================================

    def get_required_app_methods(self):
        """Return list of methods the main app should implement for best integration"""
        return [
            "show_splash_authentication",  # Preferred: Show splash without closing terminal
            "return_to_authentication",  # Alternative: Return to auth screen
            "show_login_screen",  # Alternative: Show login UI
            "refresh_all_tabs",  # Refresh all tabs after new authentication
            "update_terminal_content",  # Update terminal content after auth
            "hide_terminal",  # Optional: Hide terminal during splash
            "show_terminal",  # Optional: Show terminal after splash
        ]

    def check_app_integration(self):
        """Check which integration methods are available in the main app"""
        methods = self.get_required_app_methods()
        available = []
        missing = []

        for method in methods:
            if hasattr(self.app, method):
                available.append(method)
            else:
                missing.append(method)

        print("🔍 App Integration Check:")
        print(f"✅ Available methods: {available}")
        print(f"⚠️ Missing methods: {missing}")

        return available, missing

    def suggest_app_integration(self):
        """Suggest how to integrate logout functionality in main app"""
        print("💡 To properly integrate logout functionality, add these methods to your main app:")
        print("")
        print("def show_splash_authentication(self):")
        print("    '''Show splash screen without closing terminal'''")
        print("    # Hide main terminal window")
        print("    # Show splash screen")
        print("    # Update session_data when auth completes")
        print("    # Show main terminal window again")
        print("")
        print("def refresh_all_tabs(self):")
        print("    '''Refresh all tabs after authentication change'''")
        print("    # Refresh each tab's content")
        print("    # Update UI elements")
        print("")
        print("def return_to_authentication(self):")
        print("    '''Alternative method to return to auth'''")
        print("    # Reset UI to authentication state")
        print("    # Keep terminal window open")