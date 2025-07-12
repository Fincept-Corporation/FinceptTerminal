# profile_tab.py - Updated for New Fincept API v2.1.0
import dearpygui.dearpygui as dpg
import requests
from datetime import datetime
from base_tab import BaseTab

# Import centralized config and API client
from config import config, get_api_endpoint
from api_client import create_api_client


class ProfileTab(BaseTab):
    """Enhanced profile tab with unified API key authentication"""

    def __init__(self, app):
        super().__init__(app)
        self.last_refresh = None
        self.usage_stats = {}
        self.request_count = 0
        self.api_client = None

        # Initialize API client with session data
        if hasattr(app, 'get_session_data'):
            session_data = app.get_session_data()
            self.api_client = create_api_client(session_data)

        print(f"🔧 Profile tab initialized with API: {config.get_api_url()}")

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
            print(f"Profile content error: {e}")
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

            print(f"✅ Profile refreshed (user_type: {user_type})")

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

        # Header with refresh indicator
        with dpg.group(horizontal=True):
            dpg.add_text("👤 Guest Profile", color=[255, 255, 100])
            dpg.add_spacer(width=20)
            dpg.add_text(f"🔄 Refreshed: {self.last_refresh.strftime('%H:%M:%S')}", color=[150, 150, 150])
            dpg.add_spacer(width=20)
            dpg.add_button(label="🔄 Refresh Now", callback=self.manual_refresh)

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

                dpg.add_spacer(height=20)
                dpg.add_button(label="Clear Session & Restart", width=320, callback=self.clear_session)

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

        # Header with refresh indicator
        username = user_info.get('username', 'User')
        with dpg.group(horizontal=True):
            dpg.add_text(f"🔑 {username}'s Profile", color=[100, 255, 100])
            dpg.add_spacer(width=20)
            dpg.add_text(f"🔄 Refreshed: {self.last_refresh.strftime('%H:%M:%S')}", color=[150, 150, 150])
            dpg.add_spacer(width=20)
            dpg.add_button(label="🔄 Refresh Now", callback=self.manual_refresh)

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
                dpg.add_button(label="Change User Account", width=280, callback=self.clear_session)

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
                progress_color = [100, 255, 100] if progress < 0.8 else [255, 200, 100] if progress < 0.95 else [255,
                                                                                                                 150,
                                                                                                                 150]
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
        dpg.add_text("❓ Unknown Session State", color=[255, 200, 100])
        dpg.add_separator()
        dpg.add_spacer(height=30)
        dpg.add_text("Unable to determine authentication status")
        dpg.add_text("This may indicate a configuration issue.")
        dpg.add_spacer(height=20)
        dpg.add_button(label="🔄 Refresh Profile", width=200, callback=self.manual_refresh)
        dpg.add_button(label="Clear Session & Restart", width=200, callback=self.clear_session)

    def create_error_profile(self, error_msg):
        """Create error profile screen"""
        dpg.add_spacer(height=50)
        dpg.add_text("🚨 Profile Error", color=[255, 100, 100])
        dpg.add_separator()
        dpg.add_spacer(height=20)
        dpg.add_text("An error occurred while loading your profile:")
        dpg.add_text(f"Error: {error_msg}", color=[255, 150, 150])
        dpg.add_spacer(height=20)
        dpg.add_button(label="🔄 Refresh Profile", width=200, callback=self.manual_refresh)
        dpg.add_button(label="Clear Session & Restart", width=200, callback=self.clear_session)

    def format_date(self, date_str):
        """Format date string for display"""
        if not date_str:
            return "Never"

        try:
            date_obj = datetime.fromisoformat(date_str.replace('Z', '+00:00'))
            return date_obj.strftime("%Y-%m-%d %H:%M")
        except:
            return date_str

    # Enhanced callback methods
    def clear_session(self):
        """Clear session and restart authentication"""
        print("🔄 Clearing session and restarting...")
        if hasattr(self.app, 'clear_session_and_restart'):
            self.app.clear_session_and_restart()
        else:
            print("Please restart the application to change user")

    def logout_all_sessions(self):
        """Logout from all sessions"""
        print("📤 Logging out from all sessions...")
        if hasattr(self.app, 'logout_and_restart'):
            self.app.logout_and_restart()
        else:
            print("Logout functionality not available")

    def extend_session(self):
        """Extend current session"""
        try:
            if hasattr(self.app, 'make_api_request'):
                response = self.app.make_api_request("POST", "/session/extend", json={"extend_hours": 2}, timeout=10)

                if response and response.status_code == 200:
                    data = response.json()
                    if data.get("success"):
                        print("✅ Session extended by 2 hours")
                    else:
                        print(f"❌ Session extension failed: {data.get('message')}")
                else:
                    print("❌ Session extension request failed")
            else:
                print("Session extension not available")
        except Exception as e:
            print(f"❌ Session extension error: {e}")

    def show_signup_info(self):
        """Show enhanced signup information"""
        print("📝 Create Account Information:")
        print("To create a free Fincept account with session tokens:")
        print("1. Restart the application")
        print("2. Choose 'Sign Up' option")
        print("3. Fill in your details")
        print("4. Verify your email")
        print("5. Get instant session token access")
        print("6. Enjoy enhanced security and features")

    def show_login_info(self):
        """Show enhanced login information"""
        print("🔑 Sign In Information:")
        print("To sign in with session token support:")
        print("1. Restart the application")
        print("2. Choose 'Sign In' option")
        print("3. Enter your email and password")
        print("4. Get secure session token")
        print("5. Access your full account with enhanced security")

    def regenerate_api_key(self):
        """Regenerate API key for authenticated users"""
        if hasattr(self.app, 'regenerate_api_key'):
            self.app.regenerate_api_key()
            # Refresh profile after API key regeneration
            self.manual_refresh()
        else:
            print("🔄 API key regeneration requested")

    def view_usage_stats(self):
        """View detailed usage statistics"""
        print("📊 Detailed Usage Statistics:")
        if self.usage_stats:
            print(f"Total Requests: {self.usage_stats.get('total_requests', 0)}")
            print(f"Credits Used: {self.usage_stats.get('total_credits_used', 0)}")
            print(f"Success Rate: {self.usage_stats.get('success_rate', 100)}%")
        else:
            print("No usage statistics available")
        print(f"Current Session Requests: {self.request_count}")

        session_data = self.get_session_data()
        print(f"Auth Method: {session_data.get('auth_method', 'unknown')}")

    def show_api_docs(self):
        """Show API documentation"""
        print("📖 Fincept API Documentation:")
        print("Base URL: https://finceptbackend.share.zrok.io")
        print("Documentation: /docs (Swagger UI)")
        print("Session Auth: Supported for enhanced security")

    def download_data(self):
        """Download data functionality"""
        print("📥 Data Download features:")
        session_data = self.get_session_data()
        auth_method = session_data.get("auth_method", "unknown")

        if auth_method == "session_token":
            print("✅ Full data export available with session tokens")
        else:
            print("⚠️ Limited export with API key authentication")
        print("Coming soon!")

    def cleanup(self):
        """Clean up profile resources"""
        self.usage_stats = {}
        self.request_count = 0