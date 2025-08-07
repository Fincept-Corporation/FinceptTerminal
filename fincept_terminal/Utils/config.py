# config.py - Centralized Configuration for Fincept API
"""
Single source of truth for all API configuration
All modules should import from this file
"""

import os
from typing import Optional, List, Dict, Any
from pathlib import Path

# Import the new logger module
from fincept_terminal.Utils.Logging.logger import (
    logger, info, debug, error, warning,
    set_debug_mode, operation, monitor_performance
)


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
        info("API configuration initialized", module="config",
             context={'api_url': self.get_api_url(), 'version': self.API_VERSION})

    @monitor_performance
    def _setup_directories(self):
        """Setup configuration directories"""
        with operation("setup_config_directories", module="config"):
            try:
                home_dir = Path.home()
                self._config_dir = home_dir / self.CONFIG_DIR_NAME
                self._config_dir.mkdir(exist_ok=True)

                self._credentials_path = self._config_dir / self.CREDENTIALS_FILE
                self._cache_path = self._config_dir / self.CACHE_FILE

                debug("Configuration directories setup completed", module="config",
                      context={'config_dir': str(self._config_dir)})

            except Exception as e:
                error("Failed to setup configuration directories", module="config",
                      context={'error': str(e)}, exc_info=True)
                raise

    @classmethod
    def get_api_url(cls) -> str:
        """Get the primary API URL"""
        url = cls.API_BASE_URL.rstrip('/')
        debug("Retrieved API URL", module="config", context={'url': url})
        return url

    @classmethod
    def get_full_url(cls, endpoint: str) -> str:
        """Get full URL for an endpoint"""
        if not endpoint.startswith('/'):
            endpoint = '/' + endpoint

        full_url = f"{cls.get_api_url()}{endpoint}"
        debug("Generated full URL", module="config",
              context={'endpoint': endpoint, 'full_url': full_url})
        return full_url

    @classmethod
    def get_fallback_urls(cls) -> List[str]:
        """Get list of fallback URLs"""
        urls = [url.rstrip('/') for url in cls.FALLBACK_URLS]
        debug("Retrieved fallback URLs", module="config",
              context={'count': len(urls)})
        return urls

    @classmethod
    def validate_configuration(cls) -> Dict[str, Any]:
        """Validate current configuration"""
        with operation("validate_configuration", module="config"):
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

            info("Configuration validated", module="config", context=config_data)
            return config_data

    @classmethod
    def set_api_url(cls, new_url: str):
        """Update API URL at runtime"""
        old_url = cls.API_BASE_URL
        cls.API_BASE_URL = new_url.rstrip('/')

        info("API URL updated", module="config",
             context={'old_url': old_url, 'new_url': cls.API_BASE_URL})

    @classmethod
    def set_debug_mode(cls, debug_enabled: bool):
        """Set debug mode"""
        old_debug = cls.DEBUG_MODE
        cls.DEBUG_MODE = debug_enabled

        # Update logger debug mode using the new logger's function
        set_debug_mode(debug_enabled)

        info("Debug mode changed", module="config",
             context={'old_debug': old_debug, 'new_debug': debug_enabled})

    @classmethod
    def set_strict_mode(cls, strict: bool):
        """Set strict API connection mode"""
        old_strict = cls.REQUIRE_API_CONNECTION
        cls.REQUIRE_API_CONNECTION = strict
        cls.ALLOW_GUEST_FALLBACK = not strict

        info("Strict mode changed", module="config",
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

        debug("Generated request headers", module="config",
              context={'has_api_key': bool(api_key)})
        return headers

    @classmethod
    def get_timeout_config(cls) -> Dict[str, float]:
        """Get timeout configuration"""
        timeout_config = {
            'connect': cls.CONNECTION_TIMEOUT,
            'read': cls.REQUEST_TIMEOUT,
            'total': cls.REQUEST_TIMEOUT + cls.CONNECTION_TIMEOUT
        }
        debug("Generated timeout configuration", module="config", context=timeout_config)
        return timeout_config

    def cleanup(self):
        """Cleanup configuration resources"""
        with operation("config_cleanup", module="config"):
            info("Configuration cleanup completed", module="config")


# Global configuration instance
config = APIConfig()


# Helper functions for backward compatibility
def get_api_base() -> str:
    """Get API base URL"""
    url = config.get_api_url()
    debug("Retrieved API base URL", module="config", context={'url': url})
    return url


def get_api_endpoint(endpoint: str) -> str:
    """Get full API endpoint URL"""
    full_url = config.get_full_url(endpoint)
    debug("Retrieved API endpoint URL", module="config",
          context={'endpoint': endpoint, 'full_url': full_url})
    return full_url


def is_strict_mode() -> bool:
    """Check if strict API mode is enabled"""
    strict = config.REQUIRE_API_CONNECTION
    debug("Checked strict mode", module="config", context={'strict_mode': strict})
    return strict


def allow_offline_fallback() -> bool:
    """Check if offline fallback is allowed"""
    fallback_allowed = not config.REQUIRE_API_CONNECTION or config.ALLOW_GUEST_FALLBACK
    debug("Checked offline fallback", module="config",
          context={'fallback_allowed': fallback_allowed})
    return fallback_allowed


def get_config_directory() -> Path:
    """Get configuration directory"""
    config_dir = config.get_config_dir()
    debug("Retrieved configuration directory", module="config",
          context={'config_dir': str(config_dir)})
    return config_dir


def get_credentials_file() -> Path:
    """Get credentials file path"""
    creds_path = config.get_credentials_path()
    debug("Retrieved credentials file path", module="config",
          context={'credentials_path': str(creds_path)})
    return creds_path


@monitor_performance
def validate_config() -> Dict[str, Any]:
    """Validate configuration"""
    with operation("validate_config_helper", module="config"):
        config_data = config.validate_configuration()
        debug("Configuration validation completed", module="config",
              context={'validation_items': len(config_data)})
        return config_data


def set_debug_mode_global(debug_enabled: bool):
    """Set debug mode globally"""
    with operation("set_debug_mode_global", module="config"):
        config.set_debug_mode(debug_enabled)
        info("Global debug mode updated", module="config",
             context={'debug_enabled': debug_enabled})


def set_strict_mode_global(strict: bool):
    """Set strict mode globally"""
    with operation("set_strict_mode_global", module="config"):
        config.set_strict_mode(strict)
        info("Global strict mode updated", module="config",
             context={'strict_mode': strict})


def cleanup_config():
    """Cleanup configuration"""
    with operation("cleanup_config_global", module="config"):
        config.cleanup()
        info("Global configuration cleanup completed", module="config")