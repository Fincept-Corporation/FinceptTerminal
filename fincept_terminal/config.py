# config.py - Centralized Configuration for Fincept API
"""
Single source of truth for all API configuration
All modules should import from this file
"""

import os
from typing import Optional


class APIConfig:
    """Centralized API configuration"""

    # MAIN API CONFIGURATION - CHANGE ONLY HERE
    API_BASE_URL = os.getenv("FINCEPT_API_URL", "https://finceptbackend.share.zrok.io")  # Your actual API

    # Alternative URLs for fallback (if needed)
    FALLBACK_URLS = [
        "http://localhost:4500",
        "https://api.fincept.in"
    ]

    # API Configuration
    API_VERSION = "2.1.0"
    REQUEST_TIMEOUT = 10
    CONNECTION_TIMEOUT = 5

    # Authentication
    REQUIRE_API_CONNECTION = True  # Set False to allow offline mode
    ALLOW_GUEST_FALLBACK = False  # Set False to require API for guests

    # Local Storage
    CONFIG_DIR_NAME = ".fincept"
    CREDENTIALS_FILE = "credentials.json"

    # Application Settings
    APP_NAME = "Fincept Financial Terminal"
    APP_VERSION = "2.1.0"

    @classmethod
    def get_api_url(cls) -> str:
        """Get the primary API URL"""
        return cls.API_BASE_URL.rstrip('/')

    @classmethod
    def get_full_url(cls, endpoint: str) -> str:
        """Get full URL for an endpoint"""
        return f"{cls.get_api_url()}{endpoint}"

    @classmethod
    def validate_configuration(cls) -> dict:
        """Validate current configuration"""
        return {
            "api_url": cls.get_api_url(),
            "require_connection": cls.REQUIRE_API_CONNECTION,
            "allow_guest_fallback": cls.ALLOW_GUEST_FALLBACK,
            "timeout": cls.REQUEST_TIMEOUT
        }

    @classmethod
    def set_api_url(cls, new_url: str):
        """Update API URL at runtime"""
        cls.API_BASE_URL = new_url.rstrip('/')
        print(f"🔄 API URL updated to: {cls.API_BASE_URL}")


# Global configuration instance
config = APIConfig()


# Helper functions for backward compatibility
def get_api_base() -> str:
    """Get API base URL"""
    return config.get_api_url()


def get_api_endpoint(endpoint: str) -> str:
    """Get full API endpoint URL"""
    return config.get_full_url(endpoint)


def is_strict_mode() -> bool:
    """Check if strict API mode is enabled"""
    return config.REQUIRE_API_CONNECTION


def allow_offline_fallback() -> bool:
    """Check if offline fallback is allowed"""
    return not config.REQUIRE_API_CONNECTION or config.ALLOW_GUEST_FALLBACK


# Configuration validation on import
# if __name__ == "__main__":
#     print("🔧 Fincept API Configuration:")
#     validation = config.validate_configuration()
#     for key, value in validation.items():
#         print(f"  {key}: {value}")
# else:
#     # Print config when imported
#     print(f"📡 Fincept API: {config.get_api_url()}")
#     if config.REQUIRE_API_CONNECTION:
#         print("🔒 Strict Mode: API connection required")
#     else:
#         print("🔓 Fallback Mode: Offline operation allowed")