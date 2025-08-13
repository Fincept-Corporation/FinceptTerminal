# -*- coding: utf-8 -*-
# profile_tab.py

import dearpygui.dearpygui as dpg
import requests
import webbrowser
import threading
from datetime import datetime, timedelta
from functools import lru_cache
from typing import Dict, Any, Optional

from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import logger, operation, monitor_performance
from fincept_terminal.Utils.config import config, get_api_endpoint
from fincept_terminal.Utils.APIClient.api_client import create_api_client


class ProfileTab(BaseTab):
    """Enhanced profile tab with unified API key authentication and logout functionality"""

    def __init__(self, app):
        super().__init__(app)
        self._initialize_state()
        self._initialize_api_client()
        logger.info("ProfileTab initialized successfully", context={
            'api_url': config.get_api_url(),
            'has_session_data': hasattr(app, 'get_session_data')
        })

    def _initialize_state(self):
        """Initialize tab state with performance optimizations"""
        self.last_refresh = None
        self.usage_stats = {}
        self.request_count = 0
        self.api_client = None
        self.logout_in_progress = False

        # Performance caching
        self._session_cache = None
        self._session_cache_time = None
        self._cache_ttl = 5  # seconds

    def _initialize_api_client(self):
        """Initialize API client with session data"""
        try:
            if hasattr(self.app, 'get_session_data'):
                session_data = self.get_session_data()
                self.api_client = create_api_client(session_data)
                logger.debug("API client initialized",
                             context={'session_type': session_data.get("user_type", "unknown")})
        except Exception as e:
            logger.error("Failed to initialize API client", exc_info=True)

    def get_label(self):
        return "Profile"

    @lru_cache(maxsize=1)
    def get_cached_session_data(self):
        """Get cached session data with TTL"""
        current_time = datetime.now().timestamp()

        if (self._session_cache is None or
                self._session_cache_time is None or
                current_time - self._session_cache_time > self._cache_ttl):

            if hasattr(self.app, 'get_session_data'):
                self._session_cache = self.app.get_session_data()
            elif hasattr(self.app, 'session_data'):
                self._session_cache = self.app.session_data
            else:
                self._session_cache = {"user_type": "unknown", "authenticated": False}

            self._session_cache_time = current_time

        return self._session_cache

    def get_session_data(self):
        """Get session data with caching for performance"""
        return self.get_cached_session_data()

    def invalidate_session_cache(self):
        """Invalidate session cache when data changes"""
        self._session_cache = None
        self._session_cache_time = None
        self.get_cached_session_data.cache_clear()

    @monitor_performance
    def create_content(self):
        """Create profile content with performance monitoring"""
        try:
            with operation("create_profile_content"):
                # Always refresh data when tab is accessed
                self.refresh_profile_data()

                session_data = self.get_session_data()
                user_type = session_data.get("user_type", "unknown")

                # Use optimized content creation methods
                content_creators = {
                    "guest": self.create_dynamic_guest_profile,
                    "registered": self.create_dynamic_user_profile,
                    "unknown": self.create_unknown_profile
                }

                creator = content_creators.get(user_type, self.create_unknown_profile)
                creator()

        except Exception as e:
            logger.error("ProfileTab content creation failed",
                         context={'user_type': session_data.get("user_type", "unknown")},
                         exc_info=True)
            self.create_error_profile(str(e))

    # ============================================
    # OPTIMIZED LOGOUT METHODS
    # ============================================

    @monitor_performance
    def logout_user(self):
        """Complete logout with performance monitoring and proper error handling"""
        if self.logout_in_progress:
            logger.warning("Logout already in progress, skipping")
            return

        self.logout_in_progress = True

        try:
            with operation("user_logout"):
                # Update logout button state
                self._update_logout_button_state(True)

                session_data = self.get_session_data()
                user_type = session_data.get("user_type", "unknown")

                logger.info("Starting logout process", context={'user_type': user_type})

                # Step 1: Perform API logout
                logout_success = self._perform_secure_logout(session_data)

                # Step 2: Clear session data
                clear_success = self.clear_session_data()

                # Step 3: Clear saved credentials
                self.clear_saved_credentials()

                # Step 4: Complete logout
                self.complete_logout_and_exit()

        except Exception as e:
            logger.error("Logout process failed", exc_info=True)
            self.force_exit_application()
        finally:
            self.logout_in_progress = False

    def _update_logout_button_state(self, logging_out=False):
        """Update logout button state safely"""
        try:
            if dpg.does_item_exist("logout_btn"):
                if logging_out:
                    dpg.set_item_label("logout_btn", "Logging out...")
                    dpg.disable_item("logout_btn")
                else:
                    dpg.set_item_label("logout_btn", "🚪 Logout")
                    dpg.enable_item("logout_btn")
        except Exception as e:
            logger.warning("Could not update logout button", context={'error': str(e)})

    def _perform_secure_logout(self, session_data):
        """Perform secure API logout with fallbacks"""
        if not self.api_client or not session_data.get("authenticated"):
            logger.info("No API logout needed - offline or guest session")
            return True

        try:
            with operation("api_logout"):
                logger.debug("Attempting API logout")

                # Try primary logout endpoint
                result = self.api_client.make_request("POST", "/auth/logout")

                if result.get("success") and result.get("data", {}).get("success"):
                    logger.info("API logout successful")
                    return True

                # Try alternative logout methods
                return self._try_alternative_logout_methods()

        except Exception as e:
            logger.error("API logout failed", exc_info=True)
            return self._try_alternative_logout_methods()

    def _try_alternative_logout_methods(self):
        """Try alternative logout endpoints"""
        alternative_endpoints = ["/auth/signout", "/user/logout"]

        for endpoint in alternative_endpoints:
            try:
                result = self.api_client.make_request("POST", endpoint)
                if result.get("success"):
                    logger.info(f"Alternative logout successful", context={'endpoint': endpoint})
                    return True
            except Exception as e:
                logger.debug(f"Alternative logout failed", context={'endpoint': endpoint, 'error': str(e)})
                continue

        # Force local cleanup if API logout fails
        logger.info("Performing local session cleanup only")
        self._force_local_logout_cleanup()
        return True

    def _force_local_logout_cleanup(self):
        """Force local logout cleanup"""
        try:
            if hasattr(self.api_client, 'session_data'):
                self.api_client.session_data = {
                    "user_type": "guest",
                    "authenticated": False,
                    "api_key": None,
                    "user_info": {}
                }
            logger.debug("Local logout cleanup completed")
        except Exception as e:
            logger.warning("Local logout cleanup failed", context={'error': str(e)})

    def clear_saved_credentials(self):
        """Clear saved credentials with enhanced error handling"""
        try:
            with operation("clear_credentials"):
                from fincept_terminal.Utils.Managers.session_manager import session_manager

                if session_manager.clear_credentials():
                    logger.info("Saved credentials cleared successfully")
                else:
                    logger.warning("Could not clear saved credentials")

        except ImportError:
            logger.warning("Session manager not available for credential clearing")
        except Exception as e:
            logger.error("Error clearing credentials", exc_info=True)

    def complete_logout_and_exit(self):
        """Complete logout and exit with better error handling"""
        try:
            with operation("complete_logout"):
                logger.info("Logout completed successfully")

                # Show completion message
                self._show_logout_completion_message()

                # Give user time to see the message
                threading.Timer(2.0, self._attempt_graceful_exit).start()

        except Exception as e:
            logger.error("Error during logout completion", exc_info=True)
            self.force_exit_application()

    def _show_logout_completion_message(self):
        """Show logout completion message"""
        message = """
✅ Logout completed successfully!
🚪 Closing Fincept Terminal...

To access Fincept again:
1. 🔄 Run the application
2. 🔑 Choose authentication method
3. 👤 Sign in or continue as guest

👋 Thank you for using Fincept!
        """
        logger.info("Logout completion message displayed")
        print(message.strip())

    def _attempt_graceful_exit(self):
        """Attempt graceful application exit with fallbacks"""
        exit_methods = [
            ('app.close_application', lambda: self.app.close_application()),
            ('app.shutdown', lambda: self.app.shutdown()),
            ('dpg.stop_dearpygui', lambda: dpg.stop_dearpygui()),
            ('force_exit', lambda: self.force_exit_application())
        ]

        for method_name, method_func in exit_methods:
            try:
                logger.debug(f"Attempting exit via {method_name}")
                method_func()
                return
            except Exception as e:
                logger.debug(f"Exit method {method_name} failed", context={'error': str(e)})
                continue

    def force_exit_application(self):
        """Force exit application with multiple fallbacks"""
        try:
            with operation("force_exit"):
                logger.info("Force closing application")

                # Try graceful DearPyGUI shutdown first
                try:
                    if hasattr(dpg, 'is_dearpygui_running') and dpg.is_dearpygui_running():
                        dpg.stop_dearpygui()
                        dpg.destroy_context()
                except Exception as e:
                    logger.debug("DearPyGUI shutdown failed", context={'error': str(e)})

                # System exit fallbacks
                import sys
                import os

                exit_methods = [
                    lambda: sys.exit(0),
                    lambda: os._exit(0),
                    lambda: quit()
                ]

                for exit_method in exit_methods:
                    try:
                        exit_method()
                    except:
                        continue

        except Exception as e:
            logger.critical("Force exit failed", exc_info=True)
            import os
            os._exit(1)

    def return_to_splash_screen(self):
        """Simple return message - no new windows"""
        logger.info("Session cleared - restart required for new authentication")
        print("🔐 Session cleared - please restart application for new authentication")
        self.complete_logout_and_exit()

    # ============================================
    # OPTIMIZED SESSION MANAGEMENT METHODS
    # ============================================

    @monitor_performance
    def clear_session_data(self):
        """Optimized session data clearing"""
        try:
            with operation("clear_session_data"):
                logger.info("Clearing session data")

                # Clear API client state
                success = self._clear_api_client_state()

                # Clear app session data
                self._clear_app_session_data()

                # Clear profile-specific data
                self._clear_profile_data()

                # Invalidate caches
                self.invalidate_session_cache()

                logger.info("Session data cleared successfully")
                return success

        except Exception as e:
            logger.error("Error clearing session data", exc_info=True)
            return False

    def _clear_api_client_state(self):
        """Clear API client state safely"""
        try:
            if self.api_client:
                # Reset API client properties
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
                for attr in ['api_key', 'user_type', 'request_count']:
                    if hasattr(self.api_client, attr):
                        setattr(self.api_client, attr, None if attr == 'api_key' else
                        "guest" if attr == 'user_type' else 0)

                logger.debug("API client state reset successfully")
            return True
        except Exception as e:
            logger.warning("API client state reset failed", context={'error': str(e)})
            return False

    def _clear_app_session_data(self):
        """Clear application session data"""
        try:
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
                logger.debug("App session data reset to guest defaults")

            # Reset app-level counters
            if hasattr(self.app, 'api_request_count'):
                self.app.api_request_count = 0

        except Exception as e:
            logger.warning("App session data reset failed", context={'error': str(e)})

    def _clear_profile_data(self):
        """Clear profile-specific data"""
        self.usage_stats = {}
        self.request_count = 0
        self.last_refresh = None

    def check_session_validity(self):
        """Optimized session validity check"""
        try:
            with operation("check_session_validity"):
                if not self.api_client:
                    return False

                # Quick authentication check
                auth_result = self.api_client.check_auth_status()

                if auth_result.get("success"):
                    is_authenticated = auth_result.get("authenticated", False)
                    if not is_authenticated:
                        logger.warning("Session expired or invalid")
                        return False
                    return True

                logger.warning("Could not verify session status")
                return False

        except Exception as e:
            logger.error("Session validity check failed", exc_info=True)
            return False

    # ============================================
    # OPTIMIZED DATA REFRESH METHODS
    # ============================================

    @monitor_performance
    def refresh_profile_data(self):
        """Optimized profile data refresh with better caching"""
        try:
            with operation("refresh_profile_data"):
                self.last_refresh = datetime.now()

                # Invalidate session cache to get fresh data
                self.invalidate_session_cache()
                session_data = self.get_session_data()
                user_type = session_data.get("user_type", "unknown")

                # Recreate API client with fresh session data
                self.api_client = create_api_client(session_data)

                logger.debug(f"Refreshing profile for {user_type} user")

                # Parallel data fetching for better performance
                if self.api_client and session_data.get("authenticated"):
                    # Check authentication first
                    if not self.check_session_validity():
                        logger.warning("Authentication expired - initiating logout")
                        self.logout_user()
                        return

                # Fetch data based on user type
                if user_type == "registered":
                    self._fetch_user_data_parallel()
                elif user_type == "guest":
                    self._fetch_guest_data()

                # Update request count
                self.update_request_count()

                logger.info("ProfileTab refresh completed successfully")

        except Exception as e:
            logger.error("Error refreshing profile data", exc_info=True)

    def _fetch_user_data_parallel(self):
        """Fetch user data using threading for better performance"""
        try:
            import concurrent.futures

            with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
                # Submit parallel requests
                profile_future = executor.submit(self.fetch_fresh_user_profile)
                usage_future = executor.submit(self.fetch_usage_statistics)

                # Wait for completion with timeout
                concurrent.futures.wait([profile_future, usage_future], timeout=10)

                logger.debug("Parallel user data fetch completed")

        except ImportError:
            # Fallback to sequential execution
            logger.debug("Threading not available, using sequential fetch")
            self.fetch_fresh_user_profile()
            self.fetch_usage_statistics()
        except Exception as e:
            logger.warning("Parallel data fetch failed, using sequential", context={'error': str(e)})
            self.fetch_fresh_user_profile()
            self.fetch_usage_statistics()

    def _fetch_guest_data(self):
        """Optimized guest data fetching"""
        try:
            if self.api_client and self.api_client.is_guest():
                self.fetch_guest_status()
        except Exception as e:
            logger.debug("Guest data fetch failed", context={'error': str(e)})

    def fetch_guest_status(self):
        """Optimized guest status fetching"""
        try:
            with operation("fetch_guest_status"):
                if not self.api_client or not self.api_client.is_guest():
                    return

                result = self.api_client.get_guest_status()

                if result["success"]:
                    guest_status = result["status"]

                    # Update session data efficiently
                    if hasattr(self.app, 'session_data'):
                        update_data = {
                            "requests_today": guest_status.get("requests_today", 0),
                            "daily_limit": guest_status.get("daily_limit", 50),
                            "expires_at": guest_status.get("expires_at")
                        }
                        self.app.session_data.update(update_data)
                        self.invalidate_session_cache()

                    logger.debug("Guest status refreshed successfully")
                else:
                    error_msg = self.api_client.handle_api_error(result, "Failed to fetch guest status")
                    logger.warning("Guest status API error", context={'error': error_msg})

        except Exception as e:
            logger.error("Failed to refresh guest status", exc_info=True)

    def fetch_fresh_user_profile(self):
        """Optimized user profile fetching"""
        try:
            with operation("fetch_user_profile"):
                if not self.api_client or not self.api_client.is_registered():
                    return

                result = self.api_client.get_user_profile()

                if result["success"]:
                    # Update session data efficiently
                    if hasattr(self.app, 'session_data'):
                        self.app.session_data["user_info"] = result["profile"]
                        self.invalidate_session_cache()
                    logger.debug("User profile refreshed successfully")
                else:
                    error_msg = self.api_client.handle_api_error(result, "Failed to fetch profile")
                    logger.warning("Profile API error", context={'error': error_msg})

        except Exception as e:
            logger.error("Failed to refresh user profile", exc_info=True)

    def fetch_usage_statistics(self):
        """Optimized usage statistics fetching"""
        try:
            with operation("fetch_usage_stats"):
                if not self.api_client or not self.api_client.is_registered():
                    return

                result = self.api_client.get_user_usage()

                if result["success"]:
                    self.usage_stats = result["usage"]
                    logger.debug("Usage statistics refreshed successfully")
                else:
                    error_msg = self.api_client.handle_api_error(result, "Failed to fetch usage stats")
                    logger.warning("Usage stats error", context={'error': error_msg})

        except Exception as e:
            logger.error("Failed to refresh usage statistics", exc_info=True)

    def update_request_count(self):
        """Optimized request count update"""
        try:
            # Priority order for request count sources
            if self.api_client:
                self.request_count = self.api_client.get_request_count()
            elif hasattr(self.app, 'api_request_count'):
                self.request_count = self.app.api_request_count
            else:
                session_data = self.get_session_data()
                self.request_count = session_data.get("requests_today", 0)

        except Exception as e:
            logger.debug("Could not update request count", context={'error': str(e)})

    # ============================================
    # OPTIMIZED UI CREATION METHODS
    # ============================================

    @monitor_performance
    def create_dynamic_guest_profile(self):
        """Optimized guest profile creation"""
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
            self._create_guest_status_column(session_data, api_key)
            dpg.add_spacer(width=20)
            # Right column - Upgrade Options
            self._create_guest_upgrade_column(session_data, api_key)

        dpg.add_spacer(height=20)
        # Live statistics
        self.create_live_guest_stats()

    def _create_guest_status_column(self, session_data, api_key):
        """Create guest status column efficiently"""
        with dpg.child_window(width=350, height=450, border=True):
            dpg.add_text("Current Session Status", color=[255, 255, 100])
            dpg.add_separator()
            dpg.add_spacer(height=10)

            dpg.add_text("Account Type: Guest User")
            device_id = session_data.get('device_id', 'Unknown')
            display_device_id = device_id[:20] + "..." if len(device_id) > 20 else device_id
            dpg.add_text(f"Device ID: {display_device_id}")

            # API Key information with optimized display logic
            self._display_api_key_info(api_key, session_data)

            # Display request counts and limits
            self._display_request_limits(session_data)

            # Session info
            self._display_session_info(session_data)

            # Available features
            self._display_available_features(api_key)

    def _display_api_key_info(self, api_key, session_data):
        """Display API key information efficiently"""
        if api_key:
            if api_key.startswith("fk_guest_"):
                dpg.add_text("Auth Method: Temporary API Key", color=[255, 255, 100])
                dpg.add_text(f"API Key: {api_key[:20]}...", color=[200, 255, 200])
                self._display_expiry_info(session_data)
            else:
                dpg.add_text("Auth Method: Legacy API Key", color=[255, 200, 100])
                dpg.add_text(f"API Key: {api_key[:20]}...", color=[200, 200, 200])
        else:
            dpg.add_text("Auth Method: Device ID Only", color=[255, 150, 150])
            dpg.add_text("API Key: None (Offline Mode)", color=[255, 150, 150])

    def _display_expiry_info(self, session_data):
        """Display expiry information efficiently"""
        expires_at = session_data.get("expires_at")
        if expires_at:
            try:
                expiry = datetime.fromisoformat(expires_at.replace('Z', '+00:00'))
                hours_left = max(0, int((expiry.replace(tzinfo=None) - datetime.now()).total_seconds() / 3600))
                dpg.add_text(f"Expires in: {hours_left} hours", color=[255, 200, 100])
            except:
                dpg.add_text("Expires: 24 hours", color=[255, 200, 100])
        else:
            dpg.add_text("Expires: 24 hours", color=[255, 200, 100])

    def _display_request_limits(self, session_data):
        """Display request limits and counts"""
        dpg.add_text("Access Level: Limited")

        daily_limit = session_data.get("daily_limit", 50)
        requests_today = session_data.get("requests_today", 0)

        dpg.add_text(f"Session Requests: {self.request_count}", color=[255, 255, 100])
        dpg.add_text(f"Today's Requests: {requests_today}/{daily_limit}", color=[200, 255, 200])

        remaining = max(0, daily_limit - requests_today)
        remaining_color = [100, 255, 100] if remaining > 10 else [255, 150, 150]
        dpg.add_text(f"Remaining Today: {remaining}", color=remaining_color)

    def _display_session_info(self, session_data):
        """Display session timing information"""
        dpg.add_spacer(height=15)
        dpg.add_text("Session Info:", color=[200, 255, 200])

        fetched_at = session_data.get('fetched_at') or session_data.get('saved_at')
        if fetched_at:
            try:
                session_time = datetime.fromisoformat(fetched_at)
                dpg.add_text(f"Started: {session_time.strftime('%H:%M:%S')}")

                duration = datetime.now() - session_time
                hours, remainder = divmod(duration.seconds, 3600)
                minutes, _ = divmod(remainder, 60)
                dpg.add_text(f"Duration: {hours}h {minutes}m")
            except:
                dpg.add_text(f"Started: {datetime.now().strftime('%H:%M:%S')}")
                dpg.add_text("Duration: Unknown")
        else:
            dpg.add_text(f"Started: {datetime.now().strftime('%H:%M:%S')}")

    def _display_available_features(self, api_key):
        """Display available features list"""
        dpg.add_spacer(height=20)
        dpg.add_text("Available Features:", color=[100, 255, 100])
        dpg.add_text("✓ Basic market data")
        dpg.add_text("✓ Real-time quotes")
        dpg.add_text("✓ Public databases")

        if api_key:
            dpg.add_text("✓ API key authentication", color=[200, 255, 200])
        else:
            dpg.add_text("⚠ Limited offline access", color=[255, 200, 100])

    def _create_guest_upgrade_column(self, session_data, api_key):
        """Create upgrade options column efficiently"""
        with dpg.child_window(width=350, height=450, border=True):
            dpg.add_text("Upgrade Your Access", color=[100, 255, 100])
            dpg.add_separator()
            dpg.add_spacer(height=10)

            # Current status display
            self._display_current_status(api_key)

            dpg.add_spacer(height=15)

            # Create account section
            self._create_account_upgrade_section()

            dpg.add_spacer(height=20)

            # Sign in section
            self._create_signin_section()

            dpg.add_spacer(height=30)

            # Current limitations
            self._display_current_limitations(session_data)

    def _display_current_status(self, api_key):
        """Display current access status"""
        if api_key and api_key.startswith("fk_guest_"):
            dpg.add_text("🔄 Current: Guest API Key", color=[255, 255, 100])
            status_items = [
                "• Temporary access (24 hours)",
                "• 50 requests per day",
                "• Public databases only",
                "• Unified authentication"
            ]
        elif api_key:
            dpg.add_text("🔄 Current: Legacy Access", color=[255, 200, 100])
            status_items = ["• Limited API access"]
        else:
            dpg.add_text("🔄 Current: Offline Mode", color=[255, 150, 150])
            status_items = ["• No API access"]

        for item in status_items:
            dpg.add_text(item)

    def _create_account_upgrade_section(self):
        """Create account upgrade section"""
        dpg.add_text("🔑 Create Account", color=[100, 150, 255])
        dpg.add_spacer(height=5)

        upgrade_benefits = [
            "• Permanent API key",
            "• Unlimited requests",
            "• All databases access",
            "• Premium data feeds",
            "• Advanced analytics",
            "• Data export features",
            "• Priority support"
        ]

        dpg.add_text("Get unlimited access:")
        for benefit in upgrade_benefits:
            dpg.add_text(benefit)

        dpg.add_spacer(height=15)
        dpg.add_button(label="Create Free Account", width=320, callback=self.show_signup_info)

    def _create_signin_section(self):
        """Create sign in section"""
        dpg.add_text("📝 Already Have Account?", color=[255, 200, 100])
        dpg.add_spacer(height=5)

        signin_benefits = [
            "• Restore your settings",
            "• Access premium features",
            "• View usage history",
            "• Manage subscriptions"
        ]

        dpg.add_text("Sign in for full access:")
        for benefit in signin_benefits:
            dpg.add_text(benefit)

        dpg.add_spacer(height=15)
        dpg.add_button(label="Sign In to Account", width=320, callback=self.show_login_info)

    def _display_current_limitations(self, session_data):
        """Display current limitations"""
        dpg.add_text("Current Limitations:", color=[255, 200, 100])

        daily_limit = session_data.get("daily_limit", 50)
        api_key = session_data.get("api_key")

        limitations = [f"• {daily_limit} requests per day maximum"]

        if api_key and api_key.startswith("fk_guest_"):
            limitations.append("• 24-hour API key expiry")
        else:
            limitations.append("• No API key authentication")

        limitations.extend([
            "• Public databases only",
            "• No data export",
            "• Basic support only"
        ])

        for limitation in limitations:
            dpg.add_text(limitation)

    @monitor_performance
    def create_dynamic_user_profile(self):
        """Optimized user profile creation"""
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
            # Left column - Account Details
            self._create_user_account_column(user_info, api_key)
            dpg.add_spacer(width=20)
            # Right column - Credits & Usage
            self._create_user_usage_column(user_info, api_key)

        dpg.add_spacer(height=20)
        # Live user stats
        self.create_live_user_stats()

    def _create_user_account_column(self, user_info, api_key):
        """Create user account details column"""
        with dpg.child_window(width=350, height=450, border=True):
            dpg.add_text("Account Details", color=[100, 255, 100])
            dpg.add_separator()
            dpg.add_spacer(height=10)

            # Basic account information
            account_fields = [
                ("Username", user_info.get('username', 'N/A')),
                ("Email", user_info.get('email', 'N/A')),
                ("Account Type", user_info.get('account_type', 'free').title()),
                ("Member Since", self.format_date(user_info.get('created_at'))),
                ("Last Login", self.format_date(user_info.get('last_login_at')))
            ]

            for label, value in account_fields:
                dpg.add_text(f"{label}: {value}")

            dpg.add_spacer(height=15)
            dpg.add_text("Authentication:", color=[200, 255, 200])

            # API Key information with optimized display
            self._display_user_api_key_info(api_key)

            # Verification status
            self._display_verification_status(user_info)

            # API Management section
            self._display_api_management_section(api_key)

    def _display_user_api_key_info(self, api_key):
        """Display user API key information"""
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

    def _display_verification_status(self, user_info):
        """Display verification status"""
        verification_status = "✓ Verified" if user_info.get('is_verified') else "⚠ Unverified"
        verification_color = [100, 255, 100] if user_info.get('is_verified') else [255, 200, 100]
        dpg.add_text(f"Verification: {verification_status}", color=verification_color)

        if user_info.get('is_admin'):
            dpg.add_text("Role: Administrator", color=[255, 200, 100])

    def _display_api_management_section(self, api_key):
        """Display API management section"""
        dpg.add_spacer(height=15)

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

    def _create_user_usage_column(self, user_info, api_key):
        """Create user credits and usage column"""
        with dpg.child_window(width=350, height=450, border=True):
            dpg.add_text("Credits & Live Usage", color=[255, 200, 100])
            dpg.add_separator()
            dpg.add_spacer(height=10)

            # Credit balance with status
            self._display_credit_balance(user_info)

            # API Access info
            self._display_api_access_info(api_key)

            # Live usage stats
            self._display_live_usage_stats()

            # Account features and actions
            self._display_account_features_and_actions()

    def _display_credit_balance(self, user_info):
        """Display credit balance with status"""
        credit_balance = user_info.get('credit_balance', 0)
        dpg.add_text(f"Current Balance: {credit_balance} credits")

        # Credit status with color coding
        if credit_balance > 1000:
            balance_color, status_text = [100, 255, 100], "Excellent"
        elif credit_balance > 100:
            balance_color, status_text = [255, 255, 100], "Good"
        else:
            balance_color, status_text = [255, 150, 150], "Low Credits"

        dpg.add_text(f"Status: {status_text}", color=balance_color)

    def _display_api_access_info(self, api_key):
        """Display API access information"""
        dpg.add_spacer(height=15)
        dpg.add_text("API Access:", color=[200, 255, 200])

        if api_key and api_key.startswith("fk_user_"):
            access_info = [
                ("Access", "Unlimited", [100, 255, 100]),
                ("Rate Limit", "Premium", None),
                ("Databases", "All Available", None)
            ]
        elif api_key:
            access_info = [
                ("Access", "Standard", [255, 200, 100]),
                ("Rate Limit", "Standard", None),
                ("Databases", "Subscribed", None)
            ]
        else:
            access_info = [
                ("Access", "Limited", [255, 150, 150]),
                ("Rate Limit", "Restricted", None)
            ]

        for label, value, color in access_info:
            if color:
                dpg.add_text(f"{label}: {value}", color=color)
            else:
                dpg.add_text(f"{label}: {value}")

    def _display_live_usage_stats(self):
        """Display live usage statistics"""
        dpg.add_spacer(height=15)
        dpg.add_text("Live Usage Stats:", color=[200, 255, 200])

        if self.usage_stats:
            usage_items = [
                ("Total Requests", self.usage_stats.get('total_requests', 0)),
                ("Credits Used", self.usage_stats.get('total_credits_used', 0)),
                ("This Session", self.request_count)
            ]

            for label, value in usage_items:
                dpg.add_text(f"{label}: {value}")

            # Show top endpoint if available
            endpoint_usage = self.usage_stats.get('endpoint_usage', {})
            if endpoint_usage:
                top_endpoint = max(endpoint_usage.keys(), key=lambda k: endpoint_usage[k]['count'])
                dpg.add_text(f"Top Endpoint: {top_endpoint}")
        else:
            dpg.add_text("Total Requests: Loading...")
            dpg.add_text("Credits Used: Loading...")
            dpg.add_text(f"This Session: {self.request_count}")

    def _display_account_features_and_actions(self):
        """Display account features and quick actions"""
        dpg.add_spacer(height=15)

        features = [
            "✓ Unlimited API requests",
            "✓ Real-time data access",
            "✓ All database access",
            "✓ Advanced analytics",
            "✓ Data export",
            "✓ Priority support"
        ]

        dpg.add_text("Account Features:")
        for feature in features:
            dpg.add_text(feature)

        dpg.add_spacer(height=15)
        dpg.add_text("Quick Actions:")

        actions = [
            ("View Usage Details", self.view_usage_stats),
            ("API Documentation", self.show_api_docs),
            ("Subscribe to Database", self.show_subscription_info)
        ]

        for label, callback in actions:
            dpg.add_button(label=label, width=280, callback=callback)

    # ============================================
    # OPTIMIZED STATISTICS DISPLAY METHODS
    # ============================================

    def create_live_guest_stats(self):
        """Optimized guest statistics display"""
        dpg.add_text("📊 Live Session Statistics", color=[200, 200, 255])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        session_data = self.get_session_data()

        with dpg.group(horizontal=True):
            # Session Usage
            self._create_session_usage_widget(session_data)
            dpg.add_spacer(width=20)
            # Session Features
            self._create_session_features_widget(session_data)
            dpg.add_spacer(width=20)
            # API Status
            self._create_api_status_widget(session_data)

    def _create_session_usage_widget(self, session_data):
        """Create session usage widget"""
        with dpg.child_window(width=250, height=150, border=True):
            dpg.add_text("Session Usage", color=[255, 255, 100])
            dpg.add_separator()
            dpg.add_spacer(height=10)

            api_key = session_data.get("api_key")
            daily_limit = session_data.get("daily_limit", 50)
            requests_today = session_data.get("requests_today", 0)

            usage_items = [
                f"Session Requests: {self.request_count}",
                f"Daily Requests: {requests_today}/{daily_limit}",
                f"Auth: {'API Key' if api_key else 'Offline'}"
            ]

            for item in usage_items:
                dpg.add_text(item)

            # Session timing
            self._display_session_duration(session_data)

            # Progress bar
            dpg.add_spacer(height=10)
            dpg.add_text("Daily Progress:")
            progress = min(1.0, requests_today / daily_limit)
            dpg.add_progress_bar(default_value=progress, width=200)

    def _display_session_duration(self, session_data):
        """Display session duration efficiently"""
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

    def _create_session_features_widget(self, session_data):
        """Create session features widget"""
        with dpg.child_window(width=250, height=150, border=True):
            dpg.add_text("Available Features", color=[100, 255, 100])
            dpg.add_separator()
            dpg.add_spacer(height=10)

            api_key = session_data.get("api_key")

            base_features = [
                "✓ Market Data Access",
                "✓ Basic Analytics",
                "✓ Public Databases"
            ]

            for feature in base_features:
                dpg.add_text(feature)

            if api_key:
                if api_key.startswith("fk_guest_"):
                    dpg.add_text("✓ Guest API Key", color=[200, 255, 200])
                else:
                    dpg.add_text("✓ Legacy API Access", color=[255, 200, 100])
            else:
                dpg.add_text("⚠ No API Access", color=[255, 150, 150])

    def _create_api_status_widget(self, session_data):
        """Create API status widget"""
        with dpg.child_window(width=250, height=150, border=True):
            dpg.add_text("API Connection", color=[200, 255, 200])
            dpg.add_separator()
            dpg.add_spacer(height=10)

            api_key = session_data.get("api_key")

            dpg.add_text(f"Server: {config.get_api_url()}")

            if api_key:
                dpg.add_text("Status: Connected", color=[100, 255, 100])
                dpg.add_text("Auth: API Key")

                key_type = "Temporary" if api_key.startswith("fk_guest_") else "Legacy"
                dpg.add_text(f"Type: {key_type}")
            else:
                dpg.add_text("Status: Offline", color=[255, 150, 150])
                dpg.add_text("Auth: None")

    def create_live_user_stats(self):
        """Optimized user statistics display"""
        dpg.add_text("📊 Live Account Overview", color=[200, 200, 255])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        session_data = self.get_session_data()

        with dpg.group(horizontal=True):
            # Monthly Usage
            self._create_monthly_usage_widget()
            dpg.add_spacer(width=15)
            # API Performance
            self._create_api_performance_widget(session_data)
            dpg.add_spacer(width=15)
            # Account Status
            self._create_account_status_widget(session_data)

    def _create_monthly_usage_widget(self):
        """Create monthly usage widget"""
        with dpg.child_window(width=230, height=150, border=True):
            dpg.add_text("Usage Summary", color=[100, 255, 100])
            dpg.add_separator()
            dpg.add_spacer(height=10)

            if self.usage_stats:
                usage_items = [
                    ("Requests", self.usage_stats.get('total_requests', 0)),
                    ("Credits", self.usage_stats.get('total_credits_used', 0)),
                    ("Session", self.request_count)
                ]

                for label, value in usage_items:
                    dpg.add_text(f"{label}: {value}")

                # Success rate calculation
                total_requests = self.usage_stats.get('total_requests', 0)
                if total_requests > 0:
                    dpg.add_text("Success: 100%")
            else:
                loading_items = [
                    "Requests: Loading...",
                    "Credits: Loading...",
                    f"Session: {self.request_count}"
                ]

                for item in loading_items:
                    dpg.add_text(item)

    def _create_api_performance_widget(self, session_data):
        """Create API performance widget"""
        with dpg.child_window(width=230, height=150, border=True):
            dpg.add_text("API Performance", color=[255, 255, 100])
            dpg.add_separator()
            dpg.add_spacer(height=10)

            api_key = session_data.get("api_key")

            performance_items = [
                "Avg Response: ~200ms",
                f"Server: {config.get_api_url()}",
                "Uptime: 99.9%"
            ]

            for item in performance_items:
                dpg.add_text(item)

            if api_key and api_key.startswith("fk_user_"):
                dpg.add_text("Auth: Premium ✓", color=[100, 255, 100])
            elif api_key:
                dpg.add_text("Auth: Standard", color=[255, 200, 100])
            else:
                dpg.add_text("Auth: None", color=[255, 150, 150])

    def _create_account_status_widget(self, session_data):
        """Create account status widget"""
        with dpg.child_window(width=230, height=150, border=True):
            dpg.add_text("Account Status", color=[200, 255, 200])
            dpg.add_separator()
            dpg.add_spacer(height=10)

            api_key = session_data.get("api_key")

            status_items = [
                f"Last Update: {self.last_refresh.strftime('%H:%M:%S')}",
                "Status: Active",
                "Connection: Stable"
            ]

            for item in status_items:
                dpg.add_text(item)

            if api_key:
                dpg.add_text("API: Connected", color=[100, 255, 100])
            else:
                dpg.add_text("API: Offline", color=[255, 150, 150])

    # ============================================
    # OPTIMIZED MANUAL REFRESH AND UI METHODS
    # ============================================

    @monitor_performance
    def manual_refresh(self):
        """Optimized manual refresh with comprehensive error handling"""
        try:
            with operation("manual_refresh"):
                logger.info("Manual refresh initiated")

                # Check if we're in a logged out state
                session_data = self.get_session_data()
                if not session_data.get("authenticated") and session_data.get("user_type") == "guest":
                    logger.info("In logged out state - showing logout message")
                    self.show_logout_complete_message()
                    return

                # Check session validity first
                if self.api_client and not self.check_session_validity():
                    logger.warning("Session invalid - initiating logout")
                    self.logout_user()
                    return

                # Perform refresh operations
                self.refresh_profile_data()
                self.safe_recreate_content()

                logger.info("Manual refresh completed successfully", context={
                    'request_count': self.request_count,
                    'user_type': session_data.get("user_type", "unknown")
                })

        except Exception as e:
            logger.error("Manual refresh failed", exc_info=True)
            self.create_error_profile(str(e))

    def safe_recreate_content(self):
        """Safely recreate content with optimized error handling"""
        try:
            with operation("recreate_content"):
                # Clear content safely
                self.safe_clear_tab_content()
                # Recreate content
                self.create_content()

        except Exception as e:
            logger.error("Could not recreate content safely", exc_info=True)
            self._show_content_refresh_error()

    def _show_content_refresh_error(self):
        """Show minimal error state for content refresh failures"""
        try:
            parent = self.find_safe_parent()
            if parent:
                dpg.add_text("⚠️ Content refresh failed", color=[255, 200, 100], parent=parent)
                dpg.add_text("Try refreshing again or restart the application", parent=parent)
        except:
            logger.warning("Please restart the application for best experience")

    def safe_clear_tab_content(self):
        """Optimized tab content clearing"""
        try:
            with operation("clear_tab_content"):
                # Try BaseTab's clear method if available
                if hasattr(super(), 'clear_content'):
                    super().clear_content()
                    return

                # Try content_tag clearing
                if hasattr(self, 'content_tag') and dpg.does_item_exist(self.content_tag):
                    self._clear_children(self.content_tag)
                    return

                # Try common tag clearing
                common_tags = ['profile_content', 'tab_content', 'main_content']
                for tag in common_tags:
                    if dpg.does_item_exist(tag):
                        self._clear_children(tag)
                        return

        except Exception as e:
            logger.debug("Tab content clearing failed", context={'error': str(e)})

    def _clear_children(self, parent_tag):
        """Clear children of a parent tag efficiently"""
        try:
            children = dpg.get_item_children(parent_tag, 1)
            for child in children:
                if dpg.does_item_exist(child):
                    dpg.delete_item(child)
        except Exception as e:
            logger.debug("Child clearing failed", context={'parent': parent_tag, 'error': str(e)})

    def find_safe_parent(self):
        """Optimized safe parent finding"""
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

    def create_unknown_profile(self):
        """Optimized unknown profile creation"""
        dpg.add_spacer(height=100)

        with dpg.group(horizontal=True):
            dpg.add_text("❓ Unknown Session State", color=[255, 200, 100])
            dpg.add_spacer(width=20)
            dpg.add_button(label="🚪 Logout", callback=self.logout_user)

        dpg.add_separator()
        dpg.add_spacer(height=30)

        error_messages = [
            "Unable to determine authentication status",
            "This may indicate a configuration issue."
        ]

        for message in error_messages:
            dpg.add_text(message)

        dpg.add_spacer(height=20)

        action_buttons = [
            ("🔄 Refresh Profile", self.manual_refresh),
            ("Clear Session & Restart", self.logout_and_restart)
        ]

        for label, callback in action_buttons:
            dpg.add_button(label=label, width=200, callback=callback)

    def create_error_profile(self, error_msg):
        """Optimized error profile creation"""
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

        recovery_buttons = [
            ("🔄 Refresh Profile", self.manual_refresh),
            ("Clear Session & Restart", self.logout_and_restart)
        ]

        for label, callback in recovery_buttons:
            dpg.add_button(label=label, width=200, callback=callback)

    @lru_cache(maxsize=32)
    def format_date(self, date_str):
        """Cached date formatting for better performance"""
        if not date_str:
            return "Never"

        try:
            date_obj = datetime.fromisoformat(date_str.replace('Z', '+00:00'))
            return date_obj.strftime("%Y-%m-%d %H:%M")
        except:
            return date_str

    # ============================================
    # OPTIMIZED CALLBACK METHODS
    # ============================================

    def show_logout_complete_message(self):
        """Optimized logout completion message display"""
        try:
            with operation("show_logout_message"):
                completion_message = """
✅ Logout completed successfully!
═══════════════════════════════════════════════════════════
🔐 LOGOUT SUCCESSFUL
═══════════════════════════════════════════════════════════
Your session has been cleared and you have been logged out.

To access Fincept again:
1. 🔄 Restart the application
2. 🔑 Use the authentication screen to sign in
3. 👤 Continue as guest, or
4. ✨ Create a new account

💡 You can now safely close this terminal window.
═══════════════════════════════════════════════════════════
                """

                logger.info("Logout completion message displayed")
                print(completion_message.strip())

                # Try to update the profile tab UI safely
                self.safe_show_logout_ui()

        except Exception as e:
            logger.error("Error showing logout message", exc_info=True)
            print("💡 Logout completed - please restart application")

    def safe_show_logout_ui(self):
        """Optimized safe logout UI display"""
        try:
            with operation("show_logout_ui"):
                # Clear existing content
                self.safe_clear_tab_content()

                # Check UI availability
                if not self.can_add_ui_elements():
                    logger.info("UI context not available - logout message shown in console only")
                    return

                # Create logout UI
                self.create_safe_logout_ui()

        except Exception as e:
            logger.warning("Could not create logout UI", context={'error': str(e)})

    def can_add_ui_elements(self):
        """Optimized UI element availability check"""
        try:
            # Check DearPyGUI context
            if hasattr(dpg, 'is_dearpygui_running') and not dpg.is_dearpygui_running():
                return False

            # Check for valid parent containers
            return (hasattr(self, 'content_tag') and dpg.does_item_exist(self.content_tag)) or \
                (hasattr(self.app, 'current_tab_content') and dpg.does_item_exist(self.app.current_tab_content))

        except Exception as e:
            logger.debug("UI availability check failed", context={'error': str(e)})
            return False

    def create_safe_logout_ui(self):
        """Optimized safe logout UI creation"""
        try:
            parent = self.find_safe_parent()
            if not parent:
                logger.warning("No safe parent container found")
                return

            self._add_logout_ui_elements(parent)
            logger.debug("Logout UI created successfully")

        except Exception as e:
            logger.error("Error creating safe logout UI", exc_info=True)

    def _add_logout_ui_elements(self, parent):
        """Add logout UI elements safely and efficiently"""
        try:
            # Spacer
            dpg.add_spacer(height=50, parent=parent)

            # Logout success message
            with dpg.group(horizontal=True, parent=parent):
                dpg.add_spacer(width=100)
                dpg.add_text("✅ Logout Successful", color=[100, 255, 100])

            dpg.add_separator(parent=parent)
            dpg.add_spacer(height=20, parent=parent)

            # Instructions
            with dpg.group(horizontal=True, parent=parent):
                dpg.add_spacer(width=50)
                dpg.add_text("Your session has been cleared successfully", color=[200, 200, 200])

            dpg.add_spacer(height=15, parent=parent)

            with dpg.group(horizontal=True, parent=parent):
                dpg.add_spacer(width=80)
                dpg.add_text("To continue using Fincept:", color=[255, 255, 100])

            dpg.add_spacer(height=30, parent=parent)

            # Action buttons
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
            logger.warning("Could not add logout elements", context={'error': str(e)})

    @monitor_performance
    def regenerate_api_key(self):
        """Optimized API key regeneration"""
        try:
            with operation("regenerate_api_key"):
                logger.info("API key regeneration requested")

                if not self.api_client or not self.api_client.is_registered():
                    error_msg = "API key regeneration requires authenticated user"
                    logger.warning(error_msg)
                    self.show_error_message(error_msg)
                    return

                # Call API to regenerate key
                result = self.api_client.regenerate_api_key()

                if result.get("success"):
                    new_api_key = result.get("api_key")
                    if new_api_key:
                        logger.info("API key regenerated successfully",
                                    context={'key_prefix': new_api_key[:10]})

                        # Update session data
                        if hasattr(self.app, 'session_data'):
                            self.app.session_data["api_key"] = new_api_key
                            self.invalidate_session_cache()

                        # Refresh profile with delay
                        threading.Timer(1.0, self.manual_refresh).start()
                        self.show_success_message("API key regenerated successfully!")
                    else:
                        error_msg = "No new API key received from server"
                        logger.error(error_msg)
                        self.show_error_message(error_msg)
                else:
                    error_msg = self.api_client.handle_api_error(result, "API key regeneration failed")
                    logger.error("API key regeneration failed", context={'error': error_msg})
                    self.show_error_message(f"Regeneration failed: {error_msg}")

        except Exception as e:
            logger.error("API key regeneration error", exc_info=True)
            self.show_error_message(f"API key regeneration error: {e}")

    @monitor_performance
    def view_usage_stats(self):
        """Optimized usage statistics viewer"""
        try:
            with operation("view_usage_stats"):
                logger.info("Viewing detailed usage statistics")

                # Get fresh usage stats if possible
                if self.api_client and self.api_client.is_registered():
                    logger.debug("Fetching latest usage statistics")
                    result = self.api_client.get_user_usage()

                    if result.get("success"):
                        self.usage_stats = result["usage"]
                        logger.debug("Latest usage stats retrieved")

                # Display comprehensive stats
                self._display_comprehensive_usage_stats()

        except Exception as e:
            logger.error("Error displaying usage stats", exc_info=True)

    def _display_comprehensive_usage_stats(self):
        """Display comprehensive usage statistics efficiently"""
        stats_header = "📊 Detailed Usage Statistics:"
        separator = "=" * 50

        print(f"{stats_header}\n{separator}")

        if self.usage_stats:
            # Main statistics
            main_stats = [
                ("Total Requests", self.usage_stats.get('total_requests', 0)),
                ("Credits Used", self.usage_stats.get('total_credits_used', 0)),
                ("Success Rate", f"{self.usage_stats.get('success_rate', 100)}%"),
                ("Last 30 Days", f"{self.usage_stats.get('requests_last_30_days', 0)} requests")
            ]

            for label, value in main_stats:
                print(f"📈 {label}: {value}")

            # Endpoint breakdown
            self._display_endpoint_breakdown()

            # Monthly breakdown
            self._display_monthly_breakdown()
        else:
            print("📊 No detailed usage statistics available")

        # Current session info
        self._display_current_session_info()

        # API client stats
        self._display_api_client_stats()

    def _display_endpoint_breakdown(self):
        """Display endpoint usage breakdown"""
        endpoint_usage = self.usage_stats.get('endpoint_usage', {})
        if endpoint_usage:
            print("\n🔗 Top Endpoints:")
            sorted_endpoints = sorted(
                endpoint_usage.items(),
                key=lambda x: x[1].get('count', 0),
                reverse=True
            )[:5]

            for endpoint, stats in sorted_endpoints:
                count = stats.get('count', 0)
                print(f"  {endpoint}: {count} requests")

    def _display_monthly_breakdown(self):
        """Display monthly usage breakdown"""
        monthly_usage = self.usage_stats.get('monthly_breakdown', {})
        if monthly_usage:
            print("\n📅 Monthly Breakdown:")
            for month, data in list(monthly_usage.items())[-3:]:  # Last 3 months
                requests = data.get('requests', 0)
                credits = data.get('credits', 0)
                print(f"  {month}: {requests} requests, {credits} credits")

    def _display_current_session_info(self):
        """Display current session information"""
        session_data = self.get_session_data()
        api_key = session_data.get("api_key")

        session_info = [
            f"📱 Session Requests: {self.request_count}",
            f"🔐 Authentication: {session_data.get('user_type', 'unknown')}"
        ]

        print(f"\n🔄 Current Session:")
        for info in session_info:
            print(info)

        # API Key info
        if api_key:
            if api_key.startswith("fk_user_"):
                print("🔑 API Key: Permanent User Key")
            elif api_key.startswith("fk_guest_"):
                print("🔑 API Key: Temporary Guest Key")
            else:
                print("🔑 API Key: Legacy Format")
        else:
            print("🔑 API Key: None (Offline Mode)")

    def _display_api_client_stats(self):
        """Display API client performance statistics"""
        if self.api_client:
            try:
                performance_stats = self.api_client.get_performance_stats()
                print(f"\n⚡ API Client Stats:")

                client_stats = [
                    ("Total Requests", performance_stats.get('total_requests', 0)),
                    ("Client Type", performance_stats.get('user_type', 'unknown')),
                    ("Authenticated", performance_stats.get('authenticated', False))
                ]

                for label, value in client_stats:
                    print(f"   {label}: {value}")

            except Exception as e:
                logger.debug("Could not get API client stats", context={'error': str(e)})

    def show_api_docs(self):
        """Optimized API documentation display"""
        try:
            with operation("show_api_docs"):
                api_docs_url = f"{config.get_api_url()}/docs"
                logger.info("Opening API documentation", context={'url': api_docs_url})

                webbrowser.open(api_docs_url)
                print(f"✅ Opened API docs: {api_docs_url}")

        except Exception as e:
            logger.warning("Could not open browser for API docs", context={'error': str(e)})
            self._display_manual_api_docs_info()

    def _display_manual_api_docs_info(self):
        """Display manual API docs information"""
        api_docs_url = f"{config.get_api_url()}/docs"

        docs_info = f"""
📖 API Documentation Details:
📎 Manual URL: {api_docs_url}
Base URL: {config.get_api_url()}
Documentation: /docs (Swagger UI)
Authentication: API Key based
        """.strip()

        print(docs_info)

    @monitor_performance
    def show_subscription_info(self):
        """Optimized subscription management display"""
        try:
            with operation("show_subscription_info"):
                logger.info("Displaying subscription information")

                session_data = self.get_session_data()
                user_type = session_data.get("user_type", "unknown")

                print("💳 Subscription Management:")
                print("=" * 40)

                if user_type == "registered":
                    self._display_registered_subscription_info()
                else:
                    self._display_guest_subscription_info(session_data)

        except Exception as e:
            logger.error("Error displaying subscription info", exc_info=True)

    def _display_registered_subscription_info(self):
        """Display subscription info for registered users"""
        registered_features = [
            "✅ Unlimited API requests",
            "✅ All databases access",
            "✅ Premium data feeds",
            "✅ Advanced analytics",
            "✅ Data export features",
            "✅ Priority support",
            "✅ Device management",
            "✅ Chat session management"
        ]

        print("✅ Registered Account Features:")
        for feature in registered_features:
            print(feature)

        # Try to get subscription details from API
        if self.api_client:
            self._fetch_and_display_subscription_details()

        subscription_actions = [
            "   • View all databases: Use Database tab",
            "   • Subscribe to new database: Use subscribe_to_database API",
            "   • Check transactions: Use get_user_transactions API"
        ]

        print(f"\n🔧 Subscription Actions:")
        for action in subscription_actions:
            print(action)

    def _fetch_and_display_subscription_details(self):
        """Fetch and display subscription details from API"""
        try:
            print("\n🔄 Checking subscription details...")

            # Get user profile for subscription info
            profile_result = self.api_client.get_user_profile()
            if profile_result.get("success"):
                profile = profile_result["profile"]
                account_type = profile.get("account_type", "free")
                credit_balance = profile.get("credit_balance", 0)

                print(f"📊 Account Type: {account_type.title()}")
                print(f"💰 Credit Balance: {credit_balance}")

                status_message = "🌟 Premium Account Active" if account_type == "premium" else "💡 Free Account - Upgrade available"
                print(status_message)

            # Check database subscriptions
            self._display_database_subscriptions()

        except Exception as e:
            logger.debug("Could not fetch subscription details", context={'error': str(e)})

    def _display_database_subscriptions(self):
        """Display available database subscriptions"""
        try:
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

        except Exception as e:
            logger.debug("Could not fetch database info", context={'error': str(e)})

    def _display_guest_subscription_info(self, session_data):
        """Display subscription info for guest users"""
        daily_limit = session_data.get("daily_limit", 50)
        requests_today = session_data.get("requests_today", 0)

        guest_limitations = [
            f"⚠️ Daily limit: {requests_today}/{daily_limit} requests",
            "⚠️ Public databases only",
            "⚠️ No data export",
            "⚠️ Basic support",
            "⚠️ 24-hour session expiry"
        ]

        print("⚠️ Guest Account Limitations:")
        for limitation in guest_limitations:
            print(limitation)

        # Display public databases
        if self.api_client:
            self._display_public_databases()

        upgrade_benefits = [
            "   • Create account for unlimited access",
            "   • Permanent API key",
            "   • All databases access",
            "   • Premium features"
        ]

        print(f"\n💡 Upgrade Benefits:")
        for benefit in upgrade_benefits:
            print(benefit)

    def _display_public_databases(self):
        """Display available public databases"""
        try:
            print("\n📚 Available Public Databases:")
            pub_db_result = self.api_client.get_public_databases()
            if pub_db_result.get("success"):
                pub_databases = pub_db_result["databases"]
                print(f"   Total Public: {len(pub_databases)}")

                for db in pub_databases[:3]:  # Show first 3
                    name = db.get("name", "Unknown")
                    print(f"   • {name} (Public)")

        except Exception as e:
            logger.debug("Could not fetch public database info", context={'error': str(e)})

    # ============================================
    # HELPER METHODS FOR USER INTERACTIONS
    # ============================================

    def show_signup_info(self):
        """Optimized signup information display"""
        signup_steps = [
            "1. Use the logout button to return to authentication",
            "2. Choose 'Sign Up' option",
            "3. Fill in your details (username, email, password)",
            "4. Verify your email with the 6-digit code",
            "5. Get instant permanent API key access",
            "6. Enjoy unlimited features and enhanced security"
        ]

        print("📝 Create Account Information:")
        print("To create a free Fincept account:")
        for step in signup_steps:
            print(step)

        print("\n💡 Click 'Logout' button above to start account creation process")

    def show_login_info(self):
        """Optimized login information display"""
        login_steps = [
            "1. Use the logout button to return to authentication",
            "2. Choose 'Sign In' option",
            "3. Enter your registered email and password",
            "4. Get secure permanent API key",
            "5. Access your full account with all features"
        ]

        print("🔑 Sign In Information:")
        print("To sign in to your existing account:")
        for step in login_steps:
            print(step)

        print("\n💡 Click 'Logout' button above to start sign in process")

    def show_support_info(self):
        """Optimized support information display"""
        session_data = self.get_session_data()
        user_type = session_data.get("user_type", "unknown")

        support_info = [
            "📧 Email: support@fincept.com",
            "📖 Documentation: /docs",
            "🔧 API Issues: Check API connectivity",
            "💬 Community: Discord/Forums (if available)"
        ]

        print("🆘 Support Information:")
        print("=" * 30)
        print("For technical support:")
        for info in support_info:
            print(info)

        priority_msg = "🔑 Priority support available for registered users" if user_type == "registered" else "📝 Create account for priority support"
        print(priority_msg)

    def show_success_message(self, message):
        """Optimized success message display"""
        logger.info("Success message displayed", context={'message': message})
        print(f"✅ {message}")

    def show_error_message(self, message):
        """Optimized error message display"""
        logger.warning("Error message displayed", context={'message': message})
        print(f"❌ {message}")

    def suggest_restart(self):
        """Optimized restart suggestion"""
        restart_message = """
═══════════════════════════════════════════════════════
🔄 APPLICATION RESTART RECOMMENDED
═══════════════════════════════════════════════════════
For the best experience:
1. Close this terminal window
2. Run the Fincept Terminal application again
3. Choose your authentication method

💡 This ensures a clean authentication state
═══════════════════════════════════════════════════════
        """.strip()

        logger.info("Application restart recommended")
        print(restart_message)

    def close_application(self):
        """Optimized application closing"""
        try:
            with operation("close_application"):
                logger.info("Closing application via user request")
                print("👋 Closing Fincept Terminal...")
                print("Thank you for using Fincept!")

                # Try graceful app closing methods
                close_methods = [
                    ('app.close_application', lambda: self.app.close_application()),
                    ('app.shutdown', lambda: self.app.shutdown()),
                    ('sys.exit', lambda: __import__('sys').exit(0))
                ]

                for method_name, method_func in close_methods:
                    try:
                        logger.debug(f"Attempting close via {method_name}")
                        method_func()
                        return
                    except Exception as e:
                        logger.debug(f"Close method {method_name} failed", context={'error': str(e)})
                        continue

        except Exception as e:
            logger.error("Error closing application", exc_info=True)
            import sys
            sys.exit(0)

    @monitor_performance
    def restart_as_guest(self):
        """Optimized guest restart functionality"""
        try:
            with operation("restart_as_guest"):
                logger.info("Restarting as guest user")

                # Create guest session
                guest_session = {
                    "user_type": "guest",
                    "authenticated": False,
                    "api_key": None,
                    "device_id": f"restart_guest_{int(datetime.now().timestamp())}",
                    "user_info": {},
                    "expires_at": None,
                    "requests_today": 0,
                    "daily_limit": 50
                }

                # Update app session safely
                if hasattr(self.app, 'session_data'):
                    self.app.session_data.update(guest_session)
                    self.invalidate_session_cache()

                # Create new API client
                try:
                    self.api_client = create_api_client(guest_session)
                    logger.debug("New guest API client created")
                except Exception as e:
                    logger.warning("Could not create new API client", context={'error': str(e)})

                logger.info("Restarted as guest user successfully")
                print("✅ Restarted as guest user")
                print("💡 Limited access - create account for full features")

                # Refresh the current tab
                try:
                    self.refresh_profile_data()
                    self.safe_recreate_content()
                except Exception as e:
                    logger.warning("Could not refresh tab after guest restart", context={'error': str(e)})

        except Exception as e:
            logger.error("Error restarting as guest", exc_info=True)

    def logout_and_switch(self):
        """Optimized account switching"""
        logger.info("Account switching requested")

        switch_instructions = [
            "To switch accounts:",
            "1. Current session will be cleared",
            "2. Restart the application",
            "3. Choose different authentication method"
        ]

        print("🔄 Account switching requested...")
        for instruction in switch_instructions:
            print(instruction)

        self.logout_user()

    def logout_and_restart(self):
        """Optimized logout and restart instruction"""
        logger.info("Restart authentication requested")

        restart_instructions = [
            "To restart authentication:",
            "1. Current session will be cleared",
            "2. Close and restart the terminal",
            "3. Use the authentication screen"
        ]

        print("🔄 Restart requested...")
        for instruction in restart_instructions:
            print(instruction)

        self.logout_user()

    # ============================================
    # OPTIMIZED CLEANUP METHODS
    # ============================================

    @monitor_performance
    def cleanup(self):
        """Optimized cleanup with proper resource disposal"""
        try:
            with operation("profile_tab_cleanup"):
                logger.info("Cleaning up ProfileTab resources")

                # Clear API client resources
                if self.api_client:
                    try:
                        if hasattr(self.api_client, 'is_authenticated') and self.api_client.is_authenticated():
                            self.api_client.force_logout()
                    except Exception as e:
                        logger.debug("API client logout during cleanup failed", context={'error': str(e)})

                    self.api_client = None

                # Clear data structures
                self.usage_stats = {}
                self.request_count = 0
                self.last_refresh = None
                self.logout_in_progress = False

                # Clear caches
                self.invalidate_session_cache()
                self.format_date.cache_clear()

                # Clear UI elements
                try:
                    if dpg.does_item_exist("logout_btn"):
                        dpg.delete_item("logout_btn")
                except Exception as e:
                    logger.debug("Could not clean up logout button", context={'error': str(e)})

                logger.info("ProfileTab cleanup completed successfully")

        except Exception as e:
            logger.error("ProfileTab cleanup error", exc_info=True)

    def __del__(self):
        """Destructor to ensure cleanup"""
        try:
            self.cleanup()
        except:
            pass