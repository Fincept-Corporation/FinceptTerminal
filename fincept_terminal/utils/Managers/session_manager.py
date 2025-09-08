"""
Session Manager module for Fincept Terminal
"""

import json
import os
import requests
from datetime import datetime, timedelta
from pathlib import Path
from typing import Optional, Dict, Any

# Import centralized config
from fincept_terminal.utils.config import config, get_api_endpoint, is_strict_mode

# Import new logger module
from fincept_terminal.utils.Logging.logger import (
    info, debug, warning, error, operation, monitor_performance
)


class SessionManager:
    """Session manager with first-time user detection and centralized config"""

    def __init__(self):
        self.config_dir = Path.home() / config.CONFIG_DIR_NAME
        self.credentials_file = self.config_dir / config.CREDENTIALS_FILE
        self.ensure_config_dir()

        # Log configuration on init
        info("Session Manager initialized", module="session")
        info("API URL configured", module="session",
             context={'api_url': config.get_api_url()})
        info("Strict Mode status", module="session",
             context={'strict_mode': is_strict_mode()})
        info("Credentials file location", module="session",
             context={'credentials_file': str(self.credentials_file)})

    @monitor_performance
    def ensure_config_dir(self):
        """Ensure config directory exists"""
        with operation("ensure_config_directory", module="session"):
            try:
                self.config_dir.mkdir(exist_ok=True)
                debug("Config directory ensured", module="session",
                      context={'config_dir': str(self.config_dir)})
            except Exception as e:
                error("Could not create config directory", module="session",
                      context={'error': str(e), 'config_dir': str(self.config_dir)},
                      exc_info=True)

    def is_first_time_user(self) -> bool:
        """Check if this is a first-time user (no credentials file exists)"""
        exists = self.credentials_file.exists()
        info(f"First-time user check: {'No' if exists else 'Yes'}", module="session",
             context={'credentials_file_exists': exists})
        return not exists

    @monitor_performance
    def save_credentials_only(self, credentials_data: Dict[str, Any]) -> bool:
        """Save only essential credentials (API key, user type) - not full session"""
        with operation("save_credentials", module="session"):
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

                with open(self.credentials_file, 'w', encoding='utf-8') as f:
                    json.dump(essential_data, f, indent=2)

                user_type = credentials_data.get('user_type', 'unknown')
                info("Credentials saved successfully", module="session",
                     context={'user_type': user_type, 'is_first_time': self.is_first_time_user()})
                return True

            except Exception as e:
                error("Failed to save credentials", module="session",
                      context={'error': str(e)}, exc_info=True)
                return False

    @monitor_performance
    def load_credentials(self) -> Optional[Dict[str, Any]]:
        """Load only stored credentials"""
        with operation("load_credentials", module="session"):
            try:
                if not self.credentials_file.exists():
                    debug("No credentials file found - first-time user", module="session")
                    return None

                with open(self.credentials_file, 'r', encoding='utf-8') as f:
                    credentials = json.load(f)

                # Check if credentials were saved for the same API URL
                saved_api_url = credentials.get("api_url")
                current_api_url = config.get_api_url()

                if saved_api_url and saved_api_url != current_api_url:
                    warning("API URL mismatch detected", module="session",
                            context={'saved_url': saved_api_url, 'current_url': current_api_url})
                    info("Clearing credentials due to API URL change", module="session")
                    self.clear_credentials()
                    return None

                user_type = credentials.get('user_type', 'unknown')
                info("Credentials loaded successfully", module="session",
                     context={'user_type': user_type})
                return credentials

            except json.JSONDecodeError as e:
                error("Invalid JSON in credentials file", module="session",
                      context={'error': str(e)}, exc_info=True)
                return None
            except Exception as e:
                error("Failed to load credentials", module="session",
                      context={'error': str(e)}, exc_info=True)
                return None

    def clear_credentials(self) -> bool:
        """Clear saved credentials"""
        with operation("clear_credentials", module="session"):
            try:
                if self.credentials_file.exists():
                    self.credentials_file.unlink()
                    info("Credentials cleared successfully", module="session")
                    return True
                else:
                    debug("No credentials file to clear", module="session")
                    return True
            except Exception as e:
                error("Failed to clear credentials", module="session",
                      context={'error': str(e)}, exc_info=True)
                return False

    @monitor_performance
    def check_api_connectivity(self) -> bool:
        """Check if API server is available"""
        with operation("check_api_connectivity", module="session"):
            debug("Checking API connectivity", module="session")

            try:
                response = requests.get(
                    get_api_endpoint("/health"),
                    timeout=config.CONNECTION_TIMEOUT
                )

                if response.status_code == 200:
                    info("API server is available", module="session",
                         context={'api_url': config.get_api_url()})
                    return True
                else:
                    warning("API server returned non-200 status", module="session",
                            context={'status_code': response.status_code, 'api_url': config.get_api_url()})
                    return False

            except requests.exceptions.ConnectionError:
                warning("Cannot connect to API server", module="session",
                        context={'api_url': config.get_api_url()})
                return False
            except requests.exceptions.Timeout:
                warning("API server timeout", module="session",
                        context={'api_url': config.get_api_url(), 'timeout': config.CONNECTION_TIMEOUT})
                return False
            except Exception as e:
                error("API connectivity error", module="session",
                      context={'error': str(e), 'api_url': config.get_api_url()}, exc_info=True)
                return False

    @monitor_performance
    def fetch_session_from_api(self, api_key: Optional[str] = None, user_type: Optional[str] = None) -> Optional[Dict[str, Any]]:
        """Always fetch fresh session data from API"""
        with operation("fetch_session_from_api", module="session"):
            debug("Fetching fresh session data from API", module="session")

            try:
                headers = {"Content-Type": "application/json"}

                if api_key:
                    headers["X-API-Key"] = api_key

                # For guest users, also try the guest status endpoint if auth/status fails
                if user_type == "guest" and not api_key:
                    # Try guest status endpoint for device-based authentication
                    credentials = self.load_credentials()
                    if credentials and credentials.get("device_id"):
                        headers["X-Device-ID"] = credentials.get("device_id")

                        guest_response = requests.get(
                            get_api_endpoint("/guest/status"),
                            headers=headers,
                            timeout=config.REQUEST_TIMEOUT
                        )

                        if guest_response.status_code == 200:
                            guest_data = guest_response.json()
                            if guest_data.get("success"):
                                guest_info = guest_data.get("data", {})

                                session_data = {
                                    "authenticated": True,
                                    "api_key": guest_info.get("api_key"),
                                    "user_type": "guest",
                                    "device_id": guest_info.get("device_id"),
                                    "expires_at": guest_info.get("expires_at"),
                                    "daily_limit": guest_info.get("daily_limit", 50),
                                    "requests_today": guest_info.get("requests_today", 0),
                                    "fetched_at": datetime.now().isoformat(),
                                    "api_version": config.API_VERSION,
                                    "api_url": config.get_api_url()
                                }

                                info("Guest session data fetched successfully", module="session")
                                return session_data

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

                            session_user_type = session_data.get('user_type', 'unknown')
                            info("Fresh session data fetched successfully", module="session",
                                 context={'user_type': session_user_type})
                            return session_data
                        else:
                            warning("API reports not authenticated", module="session")
                            return None
                    else:
                        error("API request failed", module="session",
                              context={'response_data': data})
                        return None
                else:
                    error("API validation failed", module="session",
                          context={'status_code': response.status_code})
                    return None

            except requests.exceptions.ConnectionError:
                warning("Cannot connect to API server", module="session",
                        context={'api_url': config.get_api_url()})
                return None
            except requests.exceptions.Timeout:
                warning("API request timeout", module="session",
                        context={'timeout': config.REQUEST_TIMEOUT})
                return None
            except Exception as e:
                error("API fetch error", module="session",
                      context={'error': str(e)}, exc_info=True)
                return None

    @monitor_performance
    def get_fresh_session(self) -> Optional[Dict[str, Any]]:
        """Get fresh session - either from saved credentials or force new auth"""
        with operation("get_fresh_session", module="session"):
            debug("Getting fresh session data", module="session")

            # In strict mode, always check API connectivity first
            if is_strict_mode() and not self.check_api_connectivity():
                warning("API not available in strict mode", module="session")
                return None

            # Try to load saved credentials
            credentials = self.load_credentials()

            if credentials and credentials.get("api_key"):
                api_key = credentials.get("api_key")
                user_type = credentials.get("user_type")

                debug("Found saved credentials", module="session",
                      context={'user_type': user_type})

                # Always fetch fresh session data from API
                session_data = self.fetch_session_from_api(api_key, user_type)

                if session_data:
                    # Add device_id from credentials if not in API response
                    if not session_data.get("device_id") and credentials.get("device_id"):
                        session_data["device_id"] = credentials.get("device_id")

                    return session_data
                else:
                    warning("Saved credentials are invalid, clearing", module="session")
                    self.clear_credentials()
                    return None
            else:
                debug("No saved credentials found", module="session")
                return None

    def save_session_credentials(self, session_data: Dict[str, Any]) -> bool:
        """Save session credentials after successful authentication"""
        if session_data and session_data.get("authenticated"):
            return self.save_credentials_only(session_data)
        else:
            warning("Cannot save credentials - session not authenticated", module="session")
            return False

    def is_api_available(self) -> bool:
        """Check if API server is available"""
        return self.check_api_connectivity()

    @monitor_performance
    def get_session_info(self) -> Dict[str, Any]:
        """Get current session info (always fresh from API)"""
        with operation("get_session_info", module="session"):
            credentials = self.load_credentials()

            if not credentials:
                return {
                    "status": "No credentials found",
                    "first_time_user": self.is_first_time_user()
                }

            # Check API availability first
            if not self.check_api_connectivity():
                return {
                    "status": "API not available",
                    "api_url": config.get_api_url(),
                    "strict_mode": is_strict_mode(),
                    "first_time_user": False
                }

            session_data = self.fetch_session_from_api(
                credentials.get("api_key"),
                credentials.get("user_type")
            )

            if not session_data:
                return {
                    "status": "Credentials invalid or API error",
                    "api_url": config.get_api_url(),
                    "strict_mode": is_strict_mode(),
                    "first_time_user": False
                }

            info_data = {
                "user_type": session_data.get("user_type", "unknown"),
                "authenticated": session_data.get("authenticated", False),
                "has_api_key": bool(session_data.get("api_key")),
                "fetched_at": session_data.get("fetched_at", "unknown"),
                "api_version": session_data.get("api_version", "unknown"),
                "api_url": config.get_api_url(),
                "strict_mode": is_strict_mode(),
                "api_available": True,
                "first_time_user": False
            }

            if session_data.get("user_type") == "guest":
                info_data.update({
                    "expires_at": session_data.get("expires_at", "unknown"),
                    "daily_limit": session_data.get("daily_limit", "unknown"),
                    "requests_today": session_data.get("requests_today", 0)
                })

            if session_data.get("user_type") == "registered":
                user_info = session_data.get("user_info", {})
                info_data.update({
                    "username": user_info.get("username", "unknown"),
                    "credit_balance": user_info.get("credit_balance", 0)
                })

            debug("Session info compiled", module="session",
                  context={'user_type': info_data.get('user_type'), 'authenticated': info_data.get('authenticated')})
            return info_data

    def clear_session(self) -> bool:
        """Clear all session data (credentials)"""
        with operation("clear_session", module="session"):
            result = self.clear_credentials()
            info("Session cleared", module="session", context={'success': result})
            return result

    def get_api_headers(self, session_data: Optional[Dict[str, Any]]) -> Dict[str, str]:
        """Get API headers for authenticated requests"""
        headers = {"Content-Type": "application/json"}

        if session_data and session_data.get("api_key"):
            headers["X-API-Key"] = session_data["api_key"]

        debug("Generated API headers", module="session",
              context={'has_api_key': bool(session_data and session_data.get("api_key"))})
        return headers

    @monitor_performance
    def make_authenticated_request(self, session_data: Dict[str, Any], method: str, endpoint: str, **kwargs) -> Optional[requests.Response]:
        """Make authenticated API request"""
        with operation(f"authenticated_request_{method.lower()}", module="session"):
            try:
                headers = kwargs.get("headers", {})
                headers.update(self.get_api_headers(session_data))
                kwargs["headers"] = headers

                url = get_api_endpoint(endpoint)
                response = getattr(requests, method.lower())(url, **kwargs)

                debug("Authenticated request completed", module="session",
                      context={'method': method, 'endpoint': endpoint, 'status_code': response.status_code})

                return response

            except Exception as e:
                error("Authenticated request error", module="session",
                      context={'method': method, 'endpoint': endpoint, 'error': str(e)},
                      exc_info=True)
                return None


# Global session manager instance
session_manager = SessionManager()