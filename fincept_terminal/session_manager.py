# session_manager.py - Updated with Centralized Config (Strict Mode)
import json
import os
import requests
from datetime import datetime, timedelta
from pathlib import Path

# Import centralized config
from config import config, get_api_endpoint, is_strict_mode, allow_offline_fallback


class SessionManager:
    """Session manager with centralized config and strict API validation"""

    def __init__(self):
        self.config_dir = Path.home() / config.CONFIG_DIR_NAME
        self.credentials_file = self.config_dir / config.CREDENTIALS_FILE
        self.ensure_config_dir()

        # Log configuration on init
        print(f"📁 Session Manager initialized")
        print(f"📡 API URL: {config.get_api_url()}")
        print(f"🔒 Strict Mode: {is_strict_mode()}")
        print(f"💾 Credentials File: {self.credentials_file}")

    def ensure_config_dir(self):
        """Ensure config directory exists"""
        try:
            self.config_dir.mkdir(exist_ok=True)
        except Exception as e:
            print(f"⚠️ Could not create config directory: {e}")

    def save_credentials_only(self, credentials_data):
        """Save only essential credentials (API key, user type) - not full session"""
        try:
            # Only save minimal credentials needed for API calls
            essential_data = {
                "api_key": credentials_data.get("api_key"),
                "user_type": credentials_data.get("user_type"),
                "device_id": credentials_data.get("device_id"),
                "saved_at": datetime.now().isoformat(),
                "api_version": config.API_VERSION,
                "api_url": config.get_api_url()  # Store which API URL was used
            }

            with open(self.credentials_file, 'w') as f:
                json.dump(essential_data, f, indent=2)

            print(f"✅ Credentials saved for {credentials_data.get('user_type', 'unknown')} user")
            return True

        except Exception as e:
            print(f"❌ Failed to save credentials: {e}")
            return False

    def load_credentials(self):
        """Load only stored credentials"""
        try:
            if not self.credentials_file.exists():
                print("📝 No credentials file found")
                return None

            with open(self.credentials_file, 'r') as f:
                credentials = json.load(f)

            # Check if credentials were saved for the same API URL
            saved_api_url = credentials.get("api_url")
            current_api_url = config.get_api_url()

            if saved_api_url and saved_api_url != current_api_url:
                print(f"⚠️ Credentials saved for different API URL:")
                print(f"  Saved: {saved_api_url}")
                print(f"  Current: {current_api_url}")
                print("🗑️ Clearing credentials due to API URL change")
                self.clear_credentials()
                return None

            print(f"📂 Credentials loaded: {credentials.get('user_type', 'unknown')} user")
            return credentials

        except Exception as e:
            print(f"❌ Failed to load credentials: {e}")
            return None

    def clear_credentials(self):
        """Clear saved credentials"""
        try:
            if self.credentials_file.exists():
                self.credentials_file.unlink()
                print("🗑️ Credentials cleared")
                return True
        except Exception as e:
            print(f"❌ Failed to clear credentials: {e}")
            return False

    def check_api_connectivity(self):
        """Check if API server is available"""
        print("🔍 Checking API connectivity...")

        try:
            response = requests.get(
                get_api_endpoint("/health"),
                timeout=config.CONNECTION_TIMEOUT
            )

            if response.status_code == 200:
                print(f"✅ API server is available at {config.get_api_url()}")
                return True
            else:
                print(f"❌ API server returned status {response.status_code}")
                return False

        except requests.exceptions.ConnectionError:
            print(f"❌ Cannot connect to API server at {config.get_api_url()}")
            return False
        except requests.exceptions.Timeout:
            print(f"❌ API server timeout at {config.get_api_url()}")
            return False
        except Exception as e:
            print(f"❌ API connectivity error: {e}")
            return False

    def fetch_session_from_api(self, api_key=None, user_type=None):
        """Always fetch fresh session data from API"""
        print("🔄 Fetching fresh session data from API...")

        try:
            headers = {"Content-Type": "application/json"}

            if api_key:
                headers["X-API-Key"] = api_key

            # Get fresh auth status from API
            response = requests.get(
                get_api_endpoint("/auth/status"),
                headers=headers,
                timeout=config.REQUEST_TIMEOUT
            )

            if response.status_code == 200:
                data = response.json()
                if data.get("success"):
                    auth_data = data.get("data", {})

                    if auth_data.get("authenticated"):
                        # Build fresh session data from API response
                        session_data = {
                            "authenticated": True,
                            "api_key": api_key,
                            "user_type": auth_data.get("user_type"),
                            "fetched_at": datetime.now().isoformat(),
                            "api_version": config.API_VERSION,
                            "api_url": config.get_api_url()
                        }

                        # Add user-specific data
                        if auth_data.get("user_type") == "registered":
                            session_data["user_info"] = auth_data.get("user", {})
                        elif auth_data.get("user_type") == "guest":
                            guest_info = auth_data.get("guest", {})
                            session_data.update({
                                "device_id": guest_info.get("device_id"),
                                "expires_at": guest_info.get("expires_at"),
                                "daily_limit": guest_info.get("daily_limit", 50),
                                "requests_today": guest_info.get("requests_today", 0)
                            })

                        print(f"✅ Fresh session data fetched: {session_data.get('user_type')} user")
                        return session_data
                    else:
                        print("❌ API reports not authenticated")
                        return None
                else:
                    print("❌ API request failed")
                    return None
            else:
                print(f"❌ API validation failed: {response.status_code}")
                return None

        except requests.exceptions.ConnectionError:
            print(f"❌ Cannot connect to API server at {config.get_api_url()}")
            return None
        except requests.exceptions.Timeout:
            print("❌ API request timeout")
            return None
        except Exception as e:
            print(f"❌ API fetch error: {e}")
            return None

    def get_fresh_session(self):
        """Get fresh session - either from saved credentials or force new auth"""
        print("🔍 Getting fresh session data...")

        # In strict mode, always check API connectivity first
        if is_strict_mode() and not self.check_api_connectivity():
            print("❌ API not available in strict mode")
            return None

        # Try to load saved credentials
        credentials = self.load_credentials()

        if credentials and credentials.get("api_key"):
            api_key = credentials.get("api_key")
            user_type = credentials.get("user_type")

            print(f"🔑 Found saved credentials for {user_type} user")

            # Always fetch fresh session data from API
            session_data = self.fetch_session_from_api(api_key, user_type)

            if session_data:
                # Add device_id from credentials if not in API response
                if not session_data.get("device_id") and credentials.get("device_id"):
                    session_data["device_id"] = credentials.get("device_id")

                return session_data
            else:
                print("❌ Saved credentials are invalid, clearing...")
                self.clear_credentials()
                return None
        else:
            print("📝 No saved credentials found")
            return None

    def save_session_credentials(self, session_data):
        """Save session credentials after successful authentication"""
        if session_data and session_data.get("authenticated"):
            self.save_credentials_only(session_data)

    def is_api_available(self):
        """Check if API server is available"""
        return self.check_api_connectivity()

    def get_session_info(self):
        """Get current session info (always fresh from API)"""
        credentials = self.load_credentials()

        if not credentials:
            return {"status": "No credentials found"}

        # Check API availability first
        if not self.check_api_connectivity():
            return {
                "status": "API not available",
                "api_url": config.get_api_url(),
                "strict_mode": is_strict_mode()
            }

        session_data = self.fetch_session_from_api(
            credentials.get("api_key"),
            credentials.get("user_type")
        )

        if not session_data:
            return {
                "status": "Credentials invalid or API error",
                "api_url": config.get_api_url(),
                "strict_mode": is_strict_mode()
            }

        info = {
            "user_type": session_data.get("user_type", "unknown"),
            "authenticated": session_data.get("authenticated", False),
            "has_api_key": bool(session_data.get("api_key")),
            "fetched_at": session_data.get("fetched_at", "unknown"),
            "api_version": session_data.get("api_version", "unknown"),
            "api_url": config.get_api_url(),
            "strict_mode": is_strict_mode(),
            "api_available": True
        }

        if session_data.get("user_type") == "guest":
            info["expires_at"] = session_data.get("expires_at", "unknown")
            info["daily_limit"] = session_data.get("daily_limit", "unknown")
            info["requests_today"] = session_data.get("requests_today", 0)

        if session_data.get("user_type") == "registered":
            user_info = session_data.get("user_info", {})
            info["username"] = user_info.get("username", "unknown")
            info["credit_balance"] = user_info.get("credit_balance", 0)

        return info

    def clear_session(self):
        """Clear all session data (credentials)"""
        return self.clear_credentials()

    # Legacy methods for compatibility
    def save_session(self, session_data):
        """Legacy method - now just saves credentials"""
        return self.save_credentials_only(session_data)

    def load_session(self):
        """Legacy method - now returns fresh session from API"""
        return self.get_fresh_session()

    def is_session_valid(self, session_data):
        """Legacy method - in strict mode, always validate via API"""
        if not session_data:
            return False

        # In strict mode, always check API
        if is_strict_mode():
            return self.check_api_connectivity()

        return session_data.get("authenticated", False)

    def update_user_info(self, user_info):
        """Update user info - not needed since we always fetch fresh"""
        print("ℹ️ User info update not needed - always fetching fresh from API")
        return True

    def refresh_session(self, session_data):
        """Refresh session - always returns fresh data from API"""
        if not session_data or not session_data.get("api_key"):
            return session_data

        return self.fetch_session_from_api(
            session_data.get("api_key"),
            session_data.get("user_type")
        )

    def get_api_headers(self, session_data):
        """Get API headers for authenticated requests"""
        headers = {"Content-Type": "application/json"}

        if session_data and session_data.get("api_key"):
            headers["X-API-Key"] = session_data["api_key"]

        return headers

    def make_authenticated_request(self, session_data, method, endpoint, **kwargs):
        """Make authenticated API request"""
        try:
            headers = kwargs.get("headers", {})
            headers.update(self.get_api_headers(session_data))
            kwargs["headers"] = headers

            url = get_api_endpoint(endpoint)
            response = getattr(requests, method.lower())(url, **kwargs)

            return response

        except Exception as e:
            print(f"❌ Authenticated request error: {e}")
            return None


# Global session manager instance
session_manager = SessionManager()