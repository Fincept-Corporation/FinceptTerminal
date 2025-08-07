# config.py - Centralized Configuration for Fincept API
"""
Single source of truth for all API configuration
All modules should import from this file
"""

import os
from typing import Optional, List, Dict, Any
from pathlib import Path

# Import logger
from fincept_terminal.Utils.Logging.logger import logger


class APIConfig:
    """Centralized API configuration"""

    # MAIN API CONFIGURATION - CHANGE ONLY HERE
    API_BASE_URL = os.getenv("FINCEPT_API_URL", "https://finceptbackend.share.zrok.io")

    # Alternative URLs for fallback (if needed)
    FALLBACK_URLS = [
        "http://localhost:4500",
        "https://api.fincept.in"
    ]

    # API Configuration
    API_VERSION = "2.1.0"
    REQUEST_TIMEOUT = 10
    CONNECTION_TIMEOUT = 5
    MAX_RETRIES = 3
    RETRY_DELAY = 1.0

    # Authentication
    REQUIRE_API_CONNECTION = True  # Set False to allow offline mode
    ALLOW_GUEST_FALLBACK = False  # Set False to require API for guests

    # Local Storage
    CONFIG_DIR_NAME = ".fincept"
    CREDENTIALS_FILE = "credentials.json"
    CACHE_FILE = "cache.json"

    # Application Settings
    APP_NAME = "Fincept Financial Terminal"
    APP_VERSION = "2.1.0"

    # Logging Configuration
    LOG_LEVEL = os.getenv("FINCEPT_LOG_LEVEL", "INFO").upper()
    DEBUG_MODE = os.getenv("FINCEPT_DEBUG", "false").lower() == "true"

    # File paths
    _config_dir = None
    _credentials_path = None
    _cache_path = None

    def __init__(self):
        """Initialize configuration"""
        self._setup_directories()
        logger.info("API configuration initialized", module="Config",
                    context={'api_url': self.get_api_url(), 'version': self.API_VERSION})

    def _setup_directories(self):
        """Setup configuration directories"""
        try:
            home_dir = Path.home()
            self._config_dir = home_dir / self.CONFIG_DIR_NAME
            self._config_dir.mkdir(exist_ok=True)

            self._credentials_path = self._config_dir / self.CREDENTIALS_FILE
            self._cache_path = self._config_dir / self.CACHE_FILE

            logger.debug("Configuration directories setup completed", module="Config",
                         context={'config_dir': str(self._config_dir)})

        except Exception as e:
            logger.error("Failed to setup configuration directories", module="Config",
                         context={'error': str(e)}, exc_info=True)
            raise

    @classmethod
    def get_api_url(cls) -> str:
        """Get the primary API URL"""
        url = cls.API_BASE_URL.rstrip('/')
        logger.debug("Retrieved API URL", module="Config", context={'url': url})
        return url

    @classmethod
    def get_full_url(cls, endpoint: str) -> str:
        """Get full URL for an endpoint"""
        if not endpoint.startswith('/'):
            endpoint = '/' + endpoint

        full_url = f"{cls.get_api_url()}{endpoint}"
        logger.debug("Generated full URL", module="Config",
                     context={'endpoint': endpoint, 'full_url': full_url})
        return full_url

    @classmethod
    def get_fallback_urls(cls) -> List[str]:
        """Get list of fallback URLs"""
        urls = [url.rstrip('/') for url in cls.FALLBACK_URLS]
        logger.debug("Retrieved fallback URLs", module="Config",
                     context={'count': len(urls)})
        return urls

    @classmethod
    def validate_configuration(cls) -> Dict[str, Any]:
        """Validate current configuration"""
        config_data = {
            "api_url": cls.get_api_url(),
            "api_version": cls.API_VERSION,
            "require_connection": cls.REQUIRE_API_CONNECTION,
            "allow_guest_fallback": cls.ALLOW_GUEST_FALLBACK,
            "request_timeout": cls.REQUEST_TIMEOUT,
            "connection_timeout": cls.CONNECTION_TIMEOUT,
            "max_retries": cls.MAX_RETRIES,
            "debug_mode": cls.DEBUG_MODE,
            "fallback_urls_count": len(cls.FALLBACK_URLS)
        }

        logger.info("Configuration validated", module="Config", context=config_data)
        return config_data

    @classmethod
    def set_api_url(cls, new_url: str):
        """Update API URL at runtime"""
        old_url = cls.API_BASE_URL
        cls.API_BASE_URL = new_url.rstrip('/')

        logger.info("API URL updated", module="Config",
                    context={'old_url': old_url, 'new_url': cls.API_BASE_URL})

    @classmethod
    def set_debug_mode(cls, debug: bool):
        """Set debug mode"""
        old_debug = cls.DEBUG_MODE
        cls.DEBUG_MODE = debug

        # Update logger debug mode
        from fincept_terminal.Utils.Logging.logger import set_debug_mode
        set_debug_mode(debug)

        logger.info("Debug mode changed", module="Config",
                    context={'old_debug': old_debug, 'new_debug': debug})

    @classmethod
    def set_strict_mode(cls, strict: bool):
        """Set strict API connection mode"""
        old_strict = cls.REQUIRE_API_CONNECTION
        cls.REQUIRE_API_CONNECTION = strict
        cls.ALLOW_GUEST_FALLBACK = not strict

        logger.info("Strict mode changed", module="Config",
                    context={'old_strict': old_strict, 'new_strict': strict})

    def get_config_dir(self) -> Path:
        """Get configuration directory path"""
        return self._config_dir

    def get_credentials_path(self) -> Path:
        """Get credentials file path"""
        return self._credentials_path

    def get_cache_path(self) -> Path:
        """Get cache file path"""
        return self._cache_path

    @classmethod
    def get_request_headers(cls, api_key: Optional[str] = None) -> Dict[str, str]:
        """Get standard request headers"""
        headers = {
            "Content-Type": "application/json",
            "User-Agent": f"{cls.APP_NAME}/{cls.APP_VERSION}",
            "Accept": "application/json",
            "X-API-Version": cls.API_VERSION
        }

        if api_key:
            headers["X-API-Key"] = api_key

        logger.debug("Generated request headers", module="Config",
                     context={'has_api_key': bool(api_key)})
        return headers

    @classmethod
    def get_timeout_config(cls) -> Dict[str, float]:
        """Get timeout configuration"""
        return {
            'connect': cls.CONNECTION_TIMEOUT,
            'read': cls.REQUEST_TIMEOUT,
            'total': cls.REQUEST_TIMEOUT + cls.CONNECTION_TIMEOUT
        }

    def cleanup(self):
        """Cleanup configuration resources"""
        logger.info("Configuration cleanup completed", module="Config")


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


def get_config_directory() -> Path:
    """Get configuration directory"""
    return config.get_config_dir()


def get_credentials_file() -> Path:
    """Get credentials file path"""
    return config.get_credentials_path()


def validate_config() -> Dict[str, Any]:
    """Validate configuration"""
    return config.validate_configuration()


def set_debug_mode(debug: bool):
    """Set debug mode globally"""
    config.set_debug_mode(debug)


def set_strict_mode(strict: bool):
    """Set strict mode globally"""
    config.set_strict_mode(strict)


def cleanup_config():
    """Cleanup configuration"""
    config.cleanup()