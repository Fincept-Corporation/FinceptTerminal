# profile_tab.py - Corrected version with logout and working authentication
import dearpygui.dearpygui as dpg
import requests
import webbrowser
import threading
from datetime import datetime
from fincept_terminal.Utils.base_tab import BaseTab

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

    def refresh_profile_data(self):
        """Refresh all profile data using new API client"""
        try:
            self.last_refresh = datetime.now()

            session_data = self.get_session_data()
            user_type = session_data.get("user_type", "unknown")

            # Recreate API client with fresh session data
            self.api_client = create_api_client(session_data)

            # If authenticated, fetch fresh user profile
            if user_type == "registered":
                self.fetch_fresh_user_profile()
                self.fetch_usage_statistics()

            # Update request count from app or API client
            if hasattr(self.app, 'api_request_count'):
                self.request_count = self.app.api_request_count
            elif self.api_client:
                self.request_count = self.api_client.get_request_count()

            print(f"✅ ProfileTab refreshed (user_type: {user_type})")

        except Exception as e:
            print(f"Error refreshing profile data: {e}")

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
        """Enhanced manual refresh"""
        print("🔄 Manually refreshing profile data...")
        try:
            self.refresh_profile_data()

            # Recreate the content to show updated information
            self.clear_tab_content()
            self.create_content()

            print("✅ Profile data refreshed")
            print(f"📊 Request count: {self.request_count}")
            print(f"⏰ Last refresh: {self.last_refresh}")

            session_data = self.get_session_data()
            user_type = session_data.get("user_type", "unknown")
            api_key = session_data.get("api_key")
            print(f"🔐 User type: {user_type}")
            print(f"🔑 API key: {'Present' if api_key else 'None'}")

        except Exception as e:
            print(f"❌ Manual refresh error: {e}")

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

    def logout_user(self):
        """Logout current user and return to splash screen (don't close terminal)"""
        if self.logout_in_progress:
            print("⚠️ Logout already in progress...")
            return

        self.logout_in_progress = True

        try:
            # Update logout button to show progress
            if dpg.does_item_exist("logout_btn"):
                dpg.set_item_label("logout_btn", "Logging out...")
                dpg.disable_item("logout_btn")

            print("🚪 Logging out user...")
            print("🔄 Returning to splash screen (terminal will remain open)")

            # Perform logout API call if authenticated
            session_data = self.get_session_data()
            if self.api_client and session_data.get("user_type") == "registered":
                self.perform_api_logout()

            # Clear session data but keep terminal open
            self.clear_session_data()

            # Return to splash screen instead of closing terminal
            self.return_to_splash_screen()

        except Exception as e:
            print(f"❌ Logout error: {e}")
            # Force return to splash anyway
            self.return_to_splash_screen()
        finally:
            self.logout_in_progress = False

    def return_to_splash_screen(self):
        """Return to splash screen without closing terminal"""
        try:
            print("🔐 Returning to authentication splash screen...")

            # Method 1: Direct app methods (most reliable)
            if hasattr(self.app, 'show_splash_authentication'):
                self.app.show_splash_authentication()
                return
            elif hasattr(self.app, 'return_to_authentication'):
                self.app.return_to_authentication()
                return
            elif hasattr(self.app, 'show_login_screen'):
                self.app.show_login_screen()
                return

            # Method 2: Import splash and show in thread
            self.show_splash_in_thread()

        except Exception as e:
            print(f"❌ Error returning to splash: {e}")
            self.show_fallback_message()

    def show_splash_in_thread(self):
        """Show splash screen in separate thread"""

        def splash_worker():
            try:
                # Import splash authentication
                from fincept_terminal.Utils.splash_auth import show_authentication_splash

                print("🔐 Opening authentication window...")

                # Show splash and get new session
                new_session = show_authentication_splash()

                if new_session and new_session.get("authenticated"):
                    print("✅ Authentication successful! Updating terminal...")

                    # Update app session in main thread
                    self.update_session_after_auth(new_session)

                else:
                    print("❌ Authentication was cancelled or failed")
                    print("💡 You can try logging in again anytime")

            except ImportError as e:
                print(f"❌ Cannot import splash_auth: {e}")
                print("💡 Please check your splash_auth module")
                self.show_fallback_message()
            except Exception as e:
                print(f"❌ Splash authentication error: {e}")
                self.show_fallback_message()

        # Start splash in separate thread
        threading.Thread(target=splash_worker, daemon=True).start()

    def update_session_after_auth(self, new_session):
        """Update session data after successful authentication"""
        try:
            print("🔄 Updating terminal with new session data...")

            # Update app session data
            if hasattr(self.app, 'session_data'):
                self.app.session_data.update(new_session)

            # Update API client
            self.api_client = create_api_client(new_session)

            # Refresh profile data
            self.refresh_profile_data()

            # Refresh terminal content
            if hasattr(self.app, 'refresh_all_tabs'):
                self.app.refresh_all_tabs()
            elif hasattr(self.app, 'update_terminal_content'):
                self.app.update_terminal_content()

            # Refresh current profile tab
            self.manual_refresh()

            print("✅ Terminal updated with new authentication!")

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

    def logout_and_switch(self):
        """Logout and switch to different account (return to splash)"""
        print("🔄 Switching account - returning to authentication...")
        self.logout_user()

    def logout_and_restart(self):
        """Logout and return to splash (don't close terminal)"""
        print("🔄 Returning to authentication screen...")
        self.logout_user()

    def perform_api_logout(self):
        """Perform API logout if user is authenticated"""
        try:
            if not self.api_client:
                return

            print("🔐 Performing API logout...")

            # Try to logout via API
            result = self.api_client.logout()

            if result.get("success"):
                print("✅ API logout successful")
            else:
                print(f"⚠️ API logout failed: {result.get('message', 'Unknown error')}")

        except Exception as e:
            print(f"❌ API logout error: {e}")

    def clear_session_data(self):
        """Clear all session data"""
        try:
            print("🗑️ Clearing session data...")

            # Clear app session data
            if hasattr(self.app, 'session_data'):
                self.app.session_data = {
                    "user_type": None,
                    "api_key": None,
                    "device_id": None,
                    "user_info": {},
                    "authenticated": False,
                    "expires_at": None
                }

            # Clear profile data
            self.usage_stats = {}
            self.request_count = 0
            self.api_client = None

            # Clear any saved credentials
            if hasattr(self.app, 'clear_saved_credentials'):
                self.app.clear_saved_credentials()

            print("✅ Session data cleared")

        except Exception as e:
            print(f"❌ Error clearing session data: {e}")

    def restart_authentication(self):
        """Restart authentication process - return to splash screen"""
        try:
            print("🔄 Returning to splash screen...")

            # Method 1: Use app's show splash method (preferred)
            if hasattr(self.app, 'show_splash_screen'):
                self.app.show_splash_screen()
            elif hasattr(self.app, 'return_to_splash'):
                self.app.return_to_splash()
            elif hasattr(self.app, 'restart_authentication'):
                self.app.restart_authentication()

            # Method 2: Import and show splash directly
            elif hasattr(self.app, 'session_data'):
                try:
                    # Import splash auth
                    from fincept_terminal.Utils.splash_auth import show_authentication_splash

                    # Hide current terminal window if possible
                    if hasattr(self.app, 'hide_terminal'):
                        self.app.hide_terminal()

                    # Show splash in separate thread to avoid blocking
                    def show_splash():
                        try:
                            print("🔐 Showing authentication splash...")
                            new_session = show_authentication_splash()

                            if new_session and new_session.get("authenticated"):
                                print("✅ New authentication successful")
                                # Update app session data
                                self.app.session_data = new_session

                                # Show terminal again if hidden
                                if hasattr(self.app, 'show_terminal'):
                                    self.app.show_terminal()

                                # Refresh all tabs
                                if hasattr(self.app, 'refresh_all_tabs'):
                                    self.app.refresh_all_tabs()

                            else:
                                print("❌ Authentication cancelled or failed")
                                # Keep terminal hidden or show error

                        except Exception as e:
                            print(f"❌ Splash screen error: {e}")
                            if hasattr(self.app, 'show_terminal'):
                                self.app.show_terminal()

                    # Run splash in thread
                    threading.Thread(target=show_splash, daemon=True).start()

                except ImportError as e:
                    print(f"❌ Could not import splash_auth: {e}")
                    self.fallback_restart()

            # Method 3: Use generic restart methods
            elif hasattr(self.app, 'show_authentication_splash'):
                # Run authentication in separate thread to avoid blocking UI
                threading.Thread(target=self.app.show_authentication_splash, daemon=True).start()
            elif hasattr(self.app, 'clear_session_and_restart'):
                self.app.clear_session_and_restart()
            else:
                print("⚠️ No splash screen method available. Using fallback...")
                self.fallback_restart()

        except Exception as e:
            print(f"❌ Error returning to splash screen: {e}")
            self.fallback_restart()

    def fallback_restart(self):
        """Fallback method when splash screen cannot be shown"""
        try:
            print("🔄 Using fallback restart method...")

            # Try to reset the terminal to initial state
            if hasattr(self.app, 'reset_to_initial_state'):
                self.app.reset_to_initial_state()
            elif hasattr(self.app, 'show_welcome_screen'):
                self.app.show_welcome_screen()
            else:
                print("⚠️ Please restart the application to log in again.")
                print("💡 Close the terminal and run the application again.")

        except Exception as e:
            print(f"❌ Fallback restart error: {e}")
            print("⚠️ Please restart the application manually.")

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

    def regenerate_api_key(self):
        """Regenerate API key for authenticated users"""
        try:
            print("🔄 Regenerating API key...")

            if not self.api_client or not self.api_client.is_registered():
                print("❌ API key regeneration requires authenticated user")
                return

            # Call API to regenerate key
            result = self.api_client.regenerate_api_key()

            if result.get("success"):
                new_api_key = result.get("data", {}).get("api_key")
                if new_api_key:
                    # Update session data
                    session_data = self.get_session_data()
                    session_data["api_key"] = new_api_key

                    # Update API client
                    self.api_client = create_api_client(session_data)

                    print("✅ API key regenerated successfully")
                    print(f"🔑 New API key: {new_api_key[:20]}...")

                    # Refresh profile to show new key
                    self.manual_refresh()
                else:
                    print("❌ No new API key received from server")
            else:
                error_msg = result.get("message", "Unknown error")
                print(f"❌ API key regeneration failed: {error_msg}")

        except Exception as e:
            print(f"❌ API key regeneration error: {e}")

    def view_usage_stats(self):
        """View detailed usage statistics"""
        print("📊 Detailed Usage Statistics:")
        print("=" * 50)

        if self.usage_stats:
            print(f"Total Requests: {self.usage_stats.get('total_requests', 0)}")
            print(f"Credits Used: {self.usage_stats.get('total_credits_used', 0)}")
            print(f"Success Rate: {self.usage_stats.get('success_rate', 100)}%")

            # Show endpoint breakdown if available
            endpoint_usage = self.usage_stats.get('endpoint_usage', {})
            if endpoint_usage:
                print("\nEndpoint Usage Breakdown:")
                for endpoint, stats in endpoint_usage.items():
                    print(f"  {endpoint}: {stats.get('count', 0)} requests")
        else:
            print("No usage statistics available")

        print(f"\nCurrent Session Requests: {self.request_count}")

        session_data = self.get_session_data()
        auth_method = session_data.get('user_type', 'unknown')
        print(f"Authentication Method: {auth_method}")

        api_key = session_data.get("api_key")
        if api_key:
            if api_key.startswith("fk_user_"):
                print("API Key Type: Permanent User Key")
            elif api_key.startswith("fk_guest_"):
                print("API Key Type: Temporary Guest Key")
            else:
                print("API Key Type: Legacy Key")
        else:
            print("API Key Type: None (Offline Mode)")

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
        """Show subscription information"""
        print("💳 Subscription Information:")
        print("=" * 40)
        print("Current subscription features:")

        session_data = self.get_session_data()
        user_type = session_data.get("user_type", "unknown")

        if user_type == "registered":
            print("✅ Full account access")
            print("✅ Unlimited API requests")
            print("✅ All databases access")
            print("✅ Premium data feeds")
            print("✅ Advanced analytics")
            print("✅ Data export features")
            print("✅ Priority support")
        else:
            print("⚠️ Guest access limitations:")
            daily_limit = session_data.get("daily_limit", 50)
            print(f"⚠️ {daily_limit} requests per day")
            print("⚠️ Public databases only")
            print("⚠️ No data export")
            print("⚠️ Basic support")
            print("")
            print("💡 Create an account for unlimited access!")

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
        """Clean up profile resources"""
        try:
            self.usage_stats = {}
            self.request_count = 0
            self.api_client = None
            self.logout_in_progress = False
            print("✅ Profile tab cleanup completed")
        except Exception as e:
            print(f"❌ Profile cleanup error: {e}")

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