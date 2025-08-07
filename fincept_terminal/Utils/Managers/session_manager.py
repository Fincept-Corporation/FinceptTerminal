"""
Session Manager module for Fincept Terminal
Enhanced with first-time user detection and cleaned up code
"""

import json
import os
import requests
from datetime import datetime, timedelta
from pathlib import Path
from typing import Optional, Dict, Any

# Import centralized config
from fincept_terminal.Utils.config import config, get_api_endpoint, is_strict_mode

# Import centralized logging
from fincept_terminal.Utils.Logging.logger import logger, log_operation


class SessionManager:
    """Session manager with first-time user detection and centralized config"""

    def __init__(self):
        self.config_dir = Path.home() / config.CONFIG_DIR_NAME
        self.credentials_file = self.config_dir / config.CREDENTIALS_FILE
        self.ensure_config_dir()

        # Log configuration on init
        logger.info("Session Manager initialized", module="SessionManager")
        logger.info("API URL configured", module="SessionManager",
                   context={'api_url': config.get_api_url()})
        logger.info("Strict Mode status", module="SessionManager",
                   context={'strict_mode': is_strict_mode()})
        logger.info("Credentials file location", module="SessionManager",
                   context={'credentials_file': str(self.credentials_file)})

    def ensure_config_dir(self):
        """Ensure config directory exists"""
        try:
            self.config_dir.mkdir(exist_ok=True)
            logger.debug("Config directory ensured", module="SessionManager",
                        context={'config_dir': str(self.config_dir)})
        except Exception as e:
            logger.error("Could not create config directory", module="SessionManager",
                        context={'error': str(e), 'config_dir': str(self.config_dir)}, exc_info=True)

    def is_first_time_user(self) -> bool:
        """Check if this is a first-time user (no credentials file exists)"""
        exists = self.credentials_file.exists()
        logger.info(f"First-time user check: {'No' if exists else 'Yes'}", module="SessionManager",
                   context={'credentials_file_exists': exists})
        return not exists

    def save_credentials_only(self, credentials_data: Dict[str, Any]) -> bool:
        """Save only essential credentials (API key, user type) - not full session"""
        try:
            with log_operation("save_credentials", module="SessionManager"):
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
                logger.info("Credentials saved successfully", module="SessionManager",
                           context={'user_type': user_type, 'first_time_save': self.is_first_time_user()})
                return True

        except Exception as e:
            logger.error("Failed to save credentials", module="SessionManager",
                        context={'error': str(e)}, exc_info=True)
            return False

    def load_credentials(self) -> Optional[Dict[str, Any]]:
        """Load only stored credentials"""
        try:
            if not self.credentials_file.exists():
                logger.debug("No credentials file found - first-time user", module="SessionManager")
                return None

            with open(self.credentials_file, 'r', encoding='utf-8') as f:
                credentials = json.load(f)

            # Check if credentials were saved for the same API URL
            saved_api_url = credentials.get("api_url")
            current_api_url = config.get_api_url()

            if saved_api_url and saved_api_url != current_api_url:
                logger.warning("API URL mismatch detected", module="SessionManager",
                              context={'saved_url': saved_api_url, 'current_url': current_api_url})
                logger.info("Clearing credentials due to API URL change", module="SessionManager")
                self.clear_credentials()
                return None

            user_type = credentials.get('user_type', 'unknown')
            logger.info("Credentials loaded successfully", module="SessionManager",
                       context={'user_type': user_type})
            return credentials

        except json.JSONDecodeError as e:
            logger.error("Invalid JSON in credentials file", module="SessionManager",
                        context={'error': str(e)}, exc_info=True)
            return None
        except Exception as e:
            logger.error("Failed to load credentials", module="SessionManager",
                        context={'error': str(e)}, exc_info=True)
            return None

    def clear_credentials(self) -> bool:
        """Clear saved credentials"""
        try:
            if self.credentials_file.exists():
                self.credentials_file.unlink()
                logger.info("Credentials cleared successfully", module="SessionManager")
                return True
            else:
                logger.debug("No credentials file to clear", module="SessionManager")
                return True
        except Exception as e:
            logger.error("Failed to clear credentials", module="SessionManager",
                        context={'error': str(e)}, exc_info=True)
            return False

    def check_api_connectivity(self) -> bool:
        """Check if API server is available"""
        logger.debug("Checking API connectivity", module="SessionManager")

        try:
            response = requests.get(
                get_api_endpoint("/health"),
                timeout=config.CONNECTION_TIMEOUT
            )

            if response.status_code == 200:
                logger.info("API server is available", module="SessionManager",
                           context={'api_url': config.get_api_url()})
                return True
            else:
                logger.warning("API server returned non-200 status", module="SessionManager",
                              context={'status_code': response.status_code, 'api_url': config.get_api_url()})
                return False

        except requests.exceptions.ConnectionError:
            logger.warning("Cannot connect to API server", module="SessionManager",
                          context={'api_url': config.get_api_url()})
            return False
        except requests.exceptions.Timeout:
            logger.warning("API server timeout", module="SessionManager",
                          context={'api_url': config.get_api_url(), 'timeout': config.CONNECTION_TIMEOUT})
            return False
        except Exception as e:
            logger.error("API connectivity error", module="SessionManager",
                        context={'error': str(e), 'api_url': config.get_api_url()}, exc_info=True)
            return False


    def fetch_session_from_api(self, api_key: Optional[str] = None, user_type: Optional[str] = None) -> Optional[
        Dict[str, Any]]:
        """Always fetch fresh session data from API"""
        logger.debug("Fetching fresh session data from API", module="SessionManager")

        try:
            with log_operation("fetch_session_from_api", module="SessionManager"):
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

                                logger.info("Guest session data fetched successfully", module="SessionManager")
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
                            logger.info("Fresh session data fetched successfully", module="SessionManager",
                                        context={'user_type': session_user_type})
                            return session_data
                        else:
                            logger.warning("API reports not authenticated", module="SessionManager")
                            return None
                    else:
                        logger.error("API request failed", module="SessionManager",
                                     context={'response_data': data})
                        return None
                else:
                    logger.error("API validation failed", module="SessionManager",
                                 context={'status_code': response.status_code})
                    return None

        except requests.exceptions.ConnectionError:
            logger.warning("Cannot connect to API server", module="SessionManager",
                           context={'api_url': config.get_api_url()})
            return None
        except requests.exceptions.Timeout:
            logger.warning("API request timeout", module="SessionManager",
                           context={'timeout': config.REQUEST_TIMEOUT})
            return None
        except Exception as e:
            logger.error("API fetch error", module="SessionManager",
                         context={'error': str(e)}, exc_info=True)
            return None


    def get_fresh_session(self) -> Optional[Dict[str, Any]]:
        """Get fresh session - either from saved credentials or force new auth"""
        logger.debug("Getting fresh session data", module="SessionManager")

        # In strict mode, always check API connectivity first
        if is_strict_mode() and not self.check_api_connectivity():
            logger.warning("API not available in strict mode", module="SessionManager")
            return None

        # Try to load saved credentials
        credentials = self.load_credentials()

        if credentials and credentials.get("api_key"):
            api_key = credentials.get("api_key")
            user_type = credentials.get("user_type")

            logger.debug("Found saved credentials", module="SessionManager",
                        context={'user_type': user_type})

            # Always fetch fresh session data from API
            session_data = self.fetch_session_from_api(api_key, user_type)

            if session_data:
                # Add device_id from credentials if not in API response
                if not session_data.get("device_id") and credentials.get("device_id"):
                    session_data["device_id"] = credentials.get("device_id")

                return session_data
            else:
                logger.warning("Saved credentials are invalid, clearing", module="SessionManager")
                self.clear_credentials()
                return None
        else:
            logger.debug("No saved credentials found", module="SessionManager")
            return None

    def save_session_credentials(self, session_data: Dict[str, Any]) -> bool:
        """Save session credentials after successful authentication"""
        if session_data and session_data.get("authenticated"):
            return self.save_credentials_only(session_data)
        else:
            logger.warning("Cannot save credentials - session not authenticated", module="SessionManager")
            return False

    def is_api_available(self) -> bool:
        """Check if API server is available"""
        return self.check_api_connectivity()

    def get_session_info(self) -> Dict[str, Any]:
        """Get current session info (always fresh from API)"""
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

        info = {
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
            info.update({
                "expires_at": session_data.get("expires_at", "unknown"),
                "daily_limit": session_data.get("daily_limit", "unknown"),
                "requests_today": session_data.get("requests_today", 0)
            })

        if session_data.get("user_type") == "registered":
            user_info = session_data.get("user_info", {})
            info.update({
                "username": user_info.get("username", "unknown"),
                "credit_balance": user_info.get("credit_balance", 0)
            })

        return info

    def clear_session(self) -> bool:
        """Clear all session data (credentials)"""
        return self.clear_credentials()

    def get_api_headers(self, session_data: Optional[Dict[str, Any]]) -> Dict[str, str]:
        """Get API headers for authenticated requests"""
        headers = {"Content-Type": "application/json"}

        if session_data and session_data.get("api_key"):
            headers["X-API-Key"] = session_data["api_key"]

        return headers

    def make_authenticated_request(self, session_data: Dict[str, Any], method: str, endpoint: str, **kwargs) -> Optional[requests.Response]:
        """Make authenticated API request"""
        try:
            headers = kwargs.get("headers", {})
            headers.update(self.get_api_headers(session_data))
            kwargs["headers"] = headers

            url = get_api_endpoint(endpoint)
            response = getattr(requests, method.lower())(url, **kwargs)

            logger.debug("Authenticated request completed", module="SessionManager",
                        context={'method': method, 'endpoint': endpoint, 'status_code': response.status_code})

            return response

        except Exception as e:
            logger.error("Authenticated request error", module="SessionManager",
                        context={'method': method, 'endpoint': endpoint, 'error': str(e)}, exc_info=True)
            return None


# Global session manager instance
session_manager = SessionManager()