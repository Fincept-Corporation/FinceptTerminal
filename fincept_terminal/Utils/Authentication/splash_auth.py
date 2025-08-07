# -*- coding: utf-8 -*-

import json
import hashlib
import platform
import uuid
import time
from datetime import datetime, timedelta
from typing import Dict, Any, Optional, Tuple
from functools import lru_cache
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading
from dataclasses import dataclass

# Lazy imports - only imported when needed
_dpg = None
_requests = None

# Import centralized config
from fincept_terminal.Utils.config import config, get_api_endpoint, is_strict_mode
from fincept_terminal.Utils.Logging.logger import logger, log_operation


@dataclass
class UICache:
    """Cache for pre-built UI components"""
    components: Dict[str, Any] = None
    last_updated: datetime = None

    def __post_init__(self):
        if self.components is None:
            self.components = {}
        if self.last_updated is None:
            self.last_updated = datetime.now()


class LazyImporter:
    """Lazy import manager for heavy dependencies"""

    def __init__(self):
        self._dpg = None
        self._requests = None
        self._import_lock = threading.Lock()

    def get_dpg(self):
        if self._dpg is None:
            with self._import_lock:
                if self._dpg is None:
                    import dearpygui.dearpygui as dpg
                    self._dpg = dpg
        return self._dpg

    def get_requests(self):
        if self._requests is None:
            with self._import_lock:
                if self._requests is None:
                    import requests
                    self._requests = requests
        return self._requests


# Global lazy importer instance
_lazy_imports = LazyImporter()


class ConnectionPool:
    """Simple connection pool for HTTP requests"""

    def __init__(self, max_connections=5):
        self.max_connections = max_connections
        self._session = None
        self._lock = threading.Lock()

    def get_session(self):
        if self._session is None:
            with self._lock:
                if self._session is None:
                    requests = _lazy_imports.get_requests()
                    self._session = requests.Session()
                    # Configure session for performance
                    adapter = requests.adapters.HTTPAdapter(
                        pool_connections=self.max_connections,
                        pool_maxsize=self.max_connections,
                        max_retries=0  # We handle retries manually
                    )
                    self._session.mount('http://', adapter)
                    self._session.mount('https://', adapter)
        return self._session

    def close(self):
        if self._session:
            self._session.close()
            self._session = None


class SplashAuth:
    """Splash screen with optimized performance and preserved security"""

    def __init__(self, is_first_time_user=False):
        self.current_screen = "welcome"
        self.is_first_time_user = is_first_time_user
        self.session_data = {
            "user_type": None,
            "api_key": None,
            "device_id": None,
            "user_info": {},
            "authenticated": False,
            "expires_at": None
        }

        # Performance optimizations
        self.context_created = False
        self.pending_email = None
        self._connection_pool = ConnectionPool()
        self._ui_cache = UICache()
        self._device_id_cache = None
        self._hardware_info_cache = None
        self._api_status_cache = {"status": None, "expires": None}
        self._executor = ThreadPoolExecutor(max_workers=3)
        self._auth_lock = threading.Lock()

        # Pre-compute expensive operations
        self._precompute_device_info()

        logger.info(f"Splash Auth initialized with optimizations", module="SplashAuth",
                    context={
                        'api_url': config.get_api_url(),
                        'strict_mode': is_strict_mode(),
                        'first_time_user': is_first_time_user
                    })

    def _precompute_device_info(self):
        """Pre-compute device information in background"""

        def _compute():
            try:
                self._device_id_cache = self._generate_device_id_internal()
                self._hardware_info_cache = self._get_hardware_info_internal()
            except Exception as e:
                logger.error("Failed to precompute device info", module="SplashAuth",
                             context={'error': str(e)}, exc_info=True)

        self._executor.submit(_compute)

    @lru_cache(maxsize=1)
    def _generate_device_id_internal(self) -> str:
        """Generate unique device ID - cached"""
        try:
            mac_address = ':'.join(
                ['{:02x}'.format((uuid.getnode() >> elements) & 0xff)
                 for elements in range(0, 2 * 6, 2)][::-1])
            system_info = f"{platform.system()}-{platform.node()}-{mac_address}"
            device_hash = hashlib.sha256(system_info.encode()).hexdigest()
            return f"desktop_{device_hash[:16]}"
        except Exception:
            # Fallback to simple UUID
            return f"desktop_{uuid.uuid4().hex[:16]}"

    @lru_cache(maxsize=1)
    def _get_hardware_info_internal(self) -> Dict[str, Any]:
        """Get hardware fingerprint - cached"""
        return {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "node": platform.node(),
            "timestamp": datetime.now().isoformat()
        }

    def generate_device_id(self) -> str:
        """Get cached device ID"""
        if self._device_id_cache is None:
            self._device_id_cache = self._generate_device_id_internal()
        return self._device_id_cache

    def get_hardware_info(self) -> Dict[str, Any]:
        """Get cached hardware info"""
        if self._hardware_info_cache is None:
            self._hardware_info_cache = self._get_hardware_info_internal()
        return self._hardware_info_cache

    def _is_api_cache_valid(self) -> bool:
        """Check if API status cache is still valid"""
        if self._api_status_cache["expires"] is None:
            return False
        return datetime.now() < self._api_status_cache["expires"]

    def check_api_connectivity(self) -> bool:
        """Check API connectivity with caching"""
        # Return cached result if valid
        if self._is_api_cache_valid():
            return self._api_status_cache["status"]

        logger.info("Checking API connectivity...", module="SplashAuth")

        try:
            session = self._connection_pool.get_session()
            response = session.get(
                get_api_endpoint("/health"),
                timeout=config.CONNECTION_TIMEOUT
            )

            status = response.status_code == 200

            # Cache result for 30 seconds
            self._api_status_cache = {
                "status": status,
                "expires": datetime.now() + timedelta(seconds=30)
            }

            if status:
                logger.info("API server is available", module="SplashAuth",
                            context={'api_url': config.get_api_url()})
            else:
                logger.info(f"API server returned status {response.status_code}", module="SplashAuth")

            return status

        except Exception as e:
            logger.error("API connectivity error", module="SplashAuth",
                         context={'error': str(e)}, exc_info=True)

            # Cache negative result for shorter time
            self._api_status_cache = {
                "status": False,
                "expires": datetime.now() + timedelta(seconds=10)
            }
            return False

    def _get_dpg(self):
        """Get DearPyGui with lazy loading"""
        return _lazy_imports.get_dpg()

    def _create_ui_component(self, component_type: str, **kwargs) -> Any:
        """Create UI component with caching"""
        cache_key = f"{component_type}_{hash(str(sorted(kwargs.items())))}"

        if cache_key in self._ui_cache.components:
            return self._ui_cache.components[cache_key]

        dpg = self._get_dpg()
        component = getattr(dpg, component_type)(**kwargs)
        self._ui_cache.components[cache_key] = component
        return component

    def show_api_error_screen(self):
        """Show API connection error screen"""
        self.clear_content()
        dpg = self._get_dpg()
        parent = "content_container"

        self.safe_add_spacer(30, parent)

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=100)
            dpg.add_text("🚫 API Connection Error", color=[255, 100, 100])

        self.safe_add_spacer(30, parent)

        with dpg.child_window(width=460, height=350, border=True, parent=parent):
            dpg.add_spacer(height=30)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Cannot connect to Fincept API server", color=[255, 150, 150])

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text(f"API URL: {config.get_api_url()}", color=[200, 200, 200])

            dpg.add_spacer(height=15)

            error_messages = [
                "• Check if the API server is running",
                "• Verify the API URL is correct",
                "• Check your internet connection",
                "• Ensure firewall is not blocking the connection"
            ]

            for msg in error_messages:
                with dpg.group(horizontal=True):
                    dpg.add_spacer(width=50)
                    dpg.add_text(msg, color=[200, 200, 200])
                dpg.add_spacer(height=5)

            dpg.add_spacer(height=30)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_button(label="🔄 Retry Connection", width=150, callback=self.retry_api_connection)
                dpg.add_spacer(width=20)
                dpg.add_button(label="❌ Exit", width=100, callback=self.close_splash_error)

    def retry_api_connection(self, *args, **kwargs):
        """Retry API connection with cache invalidation"""
        # Invalidate cache to force fresh check
        self._api_status_cache = {"status": None, "expires": None}

        if self.check_api_connectivity():
            self.create_welcome_screen()
        else:
            self.show_api_error_screen()

    def close_splash_error(self, *args, **kwargs):
        """Close splash with error"""
        try:
            dpg = self._get_dpg()
            dpg.stop_dearpygui()
        except:
            pass

    def show_splash(self) -> Dict[str, Any]:
        """Show splash screen with performance optimizations"""
        try:
            logger.info("Creating splash screen with optimizations", module="SplashAuth",
                        context={'first_time_user': self.is_first_time_user})

            dpg = self._get_dpg()

            if not self.context_created:
                dpg.create_context()
                self.context_created = True

            # Start API connectivity check in background while creating UI
            api_future = self._executor.submit(self.check_api_connectivity) if is_strict_mode() else None

            with dpg.window(tag="splash_window", label="Fincept Authentication",
                            width=500, height=600, no_resize=True, no_move=True,
                            no_collapse=True, no_close=True):

                with dpg.group(tag="content_container"):
                    # Check API result if in strict mode
                    if is_strict_mode() and api_future:
                        api_available = api_future.result(timeout=config.CONNECTION_TIMEOUT)
                        if not api_available:
                            self.show_api_error_screen()
                        else:
                            self.create_welcome_screen()
                    else:
                        self.create_welcome_screen()

            title = "Fincept Terminal - Welcome!" if self.is_first_time_user else "Fincept Terminal - Authentication"
            dpg.create_viewport(title=title, width=520, height=640, resizable=False)
            dpg.setup_dearpygui()
            dpg.set_primary_window("splash_window", True)

            logger.info("Splash screen created successfully", module="SplashAuth")

            dpg.show_viewport()
            dpg.start_dearpygui()

        except Exception as e:
            logger.error("Splash screen error", module="SplashAuth",
                         context={'error': str(e)}, exc_info=True)

            # Secure fallback - only in non-strict mode
            if is_strict_mode():
                return {
                    "authenticated": False,
                    "error": f"Splash initialization failed: {str(e)}"
                }
            else:
                return {
                    "user_type": "guest",
                    "authenticated": True,
                    "device_id": self.generate_device_id(),
                    "api_key": None,
                    "user_info": {},
                    "expires_at": None
                }

        return self.session_data

    def clear_content(self):
        """Safely clear content with batching"""
        try:
            dpg = self._get_dpg()
            if dpg.does_item_exist("content_container"):
                children = dpg.get_item_children("content_container", 1)
                # Batch delete operations
                for child in children:
                    if dpg.does_item_exist(child):
                        dpg.delete_item(child)
        except Exception as e:
            logger.error("Error clearing content", module="SplashAuth",
                         context={'error': str(e)}, exc_info=True)

    def safe_add_spacer(self, height=10, parent="content_container"):
        """Safely add spacer"""
        try:
            dpg = self._get_dpg()
            if dpg.does_item_exist(parent):
                dpg.add_spacer(height=height, parent=parent)
        except Exception as e:
            logger.warning("Could not add spacer", module="SplashAuth",
                           context={'error': str(e)}, exc_info=True)

    def create_welcome_screen(self):
        """Create welcome screen with optimized rendering"""
        self.clear_content()
        dpg = self._get_dpg()
        parent = "content_container"

        # Batch UI creation for better performance
        ui_elements = [
            ("spacer", {"height": 20}),
            ("logo_group", {}),
            ("title_group", {}),
            ("spacer", {"height": 10}),
        ]

        # Create logo/title section
        self.safe_add_spacer(20, parent)

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=80)
            dpg.add_text("🚀 FINCEPT", color=[255, 215, 0])

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=120)
            dpg.add_text("FINANCIAL TERMINAL", color=[200, 200, 200])

        self.safe_add_spacer(10, parent)

        # Conditional welcome message
        if self.is_first_time_user:
            with dpg.group(horizontal=True, parent=parent):
                dpg.add_spacer(width=140)
                dpg.add_text("👋 Welcome to Fincept!", color=[100, 255, 100])
        else:
            with dpg.group(horizontal=True, parent=parent):
                dpg.add_spacer(width=120)
                dpg.add_text("🔄 Session Expired - Please Sign In", color=[255, 255, 100])

        self.safe_add_spacer(20, parent)

        # API Status
        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=60)
            dpg.add_text(f"🌐 API: {config.get_api_url()}", color=[100, 255, 100])

        self.safe_add_spacer(30, parent)

        # Authentication cards
        self.create_auth_cards(parent)

        self.safe_add_spacer(30, parent)

        # Footer
        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=150)
            mode_text = "First Time" if self.is_first_time_user else "Returning User"
            dpg.add_text(f"API v{config.API_VERSION} - {mode_text}", color=[100, 100, 100])

    def create_auth_cards(self, parent):
        """Create authentication cards with optimized layout"""
        if self.is_first_time_user:
            self.create_guest_card(parent, emphasized=True)
            self.safe_add_spacer(15, parent)
            self.create_signin_card(parent, emphasized=False)
            self.safe_add_spacer(15, parent)
            self.create_signup_card(parent, emphasized=False)
        else:
            self.create_signin_card(parent, emphasized=True)
            self.safe_add_spacer(15, parent)
            self.create_guest_card(parent, emphasized=False)
            self.safe_add_spacer(15, parent)
            self.create_signup_card(parent, emphasized=False)

    def create_signin_card(self, parent, emphasized=False):
        """Create sign in card"""
        dpg = self._get_dpg()
        with dpg.child_window(width=460, height=100, border=True, parent=parent):
            if emphasized:
                dpg.add_spacer(height=5)
                with dpg.group(horizontal=True):
                    dpg.add_spacer(width=15)
                    dpg.add_text("🔑 RECOMMENDED", color=[100, 255, 100])

            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                dpg.add_text("🔐 Sign In", color=[100, 255, 100])
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                text = "Welcome back! Access your account" if not self.is_first_time_user else "Access your account with permanent API key"
                dpg.add_text(text, color=[200, 200, 200])
            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=350)
                dpg.add_button(label="Sign In", width=100, callback=self.go_to_login)

    def create_guest_card(self, parent, emphasized=False):
        """Create guest card"""
        dpg = self._get_dpg()
        with dpg.child_window(width=460, height=100, border=True, parent=parent):
            if emphasized:
                dpg.add_spacer(height=5)
                with dpg.group(horizontal=True):
                    dpg.add_spacer(width=15)
                    dpg.add_text("⭐ QUICK START", color=[255, 255, 100])

            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                dpg.add_text("🎯 Guest Access", color=[255, 255, 100])
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                text = "🚀 Try Fincept instantly! No signup required" if self.is_first_time_user else "50 requests/day with temporary API key"
                dpg.add_text(text, color=[200, 200, 200])
            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=280)
                button_text = "🚀 Try Now!" if self.is_first_time_user else "Continue as Guest"
                button_width = 170 if self.is_first_time_user else 150
                dpg.add_button(label=button_text, width=button_width, callback=self.setup_guest_access)

    def create_signup_card(self, parent, emphasized=False):
        """Create signup card"""
        dpg = self._get_dpg()
        with dpg.child_window(width=460, height=100, border=True, parent=parent):
            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                dpg.add_text("✨ Create Account", color=[100, 150, 255])
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                text = "🎁 Join Fincept for unlimited access"
                dpg.add_text(text, color=[200, 200, 200])
            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=340)
                dpg.add_button(label="Sign Up", width=110, callback=self.go_to_signup)

    def create_login_screen(self):
        """Create login screen with optimized layout"""
        self.clear_content()
        dpg = self._get_dpg()
        parent = "content_container"

        self.safe_add_spacer(30, parent)

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=180)
            dpg.add_text("🔐 Sign In", color=[100, 255, 100])

        self.safe_add_spacer(30, parent)

        with dpg.child_window(width=460, height=350, border=True, parent=parent):
            dpg.add_spacer(height=30)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Email Address:")
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_input_text(tag="login_email", width=400, hint="Enter your email")

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Password:")
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_input_text(tag="login_password", width=400, password=True, hint="Enter password")

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("", tag="login_status", color=[255, 100, 100])

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_button(label="🔐 Sign In", width=120, callback=self.attempt_login)
                dpg.add_spacer(width=20)
                dpg.add_button(label="⬅️ Back", width=120, callback=self.go_to_welcome)

    def create_signup_screen(self):
        """Create signup screen"""
        self.clear_content()
        dpg = self._get_dpg()
        parent = "content_container"

        self.safe_add_spacer(20, parent)

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=170)
            dpg.add_text("✨ Create Account", color=[100, 150, 255])

        self.safe_add_spacer(20, parent)

        with dpg.child_window(width=460, height=450, border=True, parent=parent):
            dpg.add_spacer(height=20)

            # Username
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Username:")
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_input_text(tag="signup_username", width=400, hint="Choose username")

            dpg.add_spacer(height=15)

            # Email
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Email Address:")
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_input_text(tag="signup_email", width=400, hint="Enter email")

            dpg.add_spacer(height=15)

            # Password
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Password:")
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_input_text(tag="signup_password", width=400, password=True, hint="Create password")

            dpg.add_spacer(height=15)

            # Confirm Password
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Confirm Password:")
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_input_text(tag="signup_confirm_password", width=400, password=True, hint="Confirm password")

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("", tag="signup_status", color=[255, 100, 100])

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_button(label="✨ Create Account", width=140, callback=self.attempt_signup)
                dpg.add_spacer(width=20)
                dpg.add_button(label="⬅️ Back", width=120, callback=self.go_to_welcome)

    def create_otp_screen(self):
        """Create OTP verification screen"""
        self.clear_content()
        dpg = self._get_dpg()
        parent = "content_container"

        self.safe_add_spacer(50, parent)

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=160)
            dpg.add_text("📧 Email Verification", color=[255, 255, 100])

        self.safe_add_spacer(30, parent)

        with dpg.child_window(width=460, height=300, border=True, parent=parent):
            dpg.add_spacer(height=30)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Enter the 6-digit code sent to your email:", color=[200, 200, 200])

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Verification Code:")
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=130)
                dpg.add_input_text(tag="otp_code", width=200, hint="6-digit code")

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("", tag="otp_status", color=[255, 100, 100])

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=80)
                dpg.add_button(label="✅ Verify Code", width=120, callback=self.verify_otp)
                dpg.add_spacer(width=20)
                dpg.add_button(label="⬅️ Back", width=120, callback=self.go_to_signup)

    def create_guest_setup_screen(self):
        """Create guest setup screen"""
        self.clear_content()
        dpg = self._get_dpg()
        parent = "content_container"

        self.safe_add_spacer(40, parent)

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=130)
            dpg.add_text("🎯 Setting up Guest Access", color=[255, 255, 100])

        self.safe_add_spacer(30, parent)

        with dpg.child_window(width=460, height=350, border=True, parent=parent):
            dpg.add_spacer(height=30)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Guest Features:", color=[100, 255, 100])

            dpg.add_spacer(height=15)

            features = [
                "📈 Financial market data access",
                "💹 Real-time stock prices & forex",
                "🔢 50 API requests per day",
                "⏰ 24-hour access period",
                "🔑 Temporary API key authentication"
            ]

            for feature in features:
                with dpg.group(horizontal=True):
                    dpg.add_spacer(width=50)
                    dpg.add_text(feature, color=[200, 255, 200])
                dpg.add_spacer(height=5)

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Status: Creating guest API key...", tag="guest_status", color=[255, 255, 100])

            dpg.add_spacer(height=20)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=130)
                dpg.add_button(label="🚀 Continue to Terminal", width=200,
                               callback=self.complete_guest_setup, show=False, tag="guest_continue_btn")

        # Start guest session creation in background
        self._executor.submit(self.create_guest_session)

    # Navigation methods
    def go_to_welcome(self, *args, **kwargs):
        self.current_screen = "welcome"
        self.create_welcome_screen()

    def go_to_login(self, *args, **kwargs):
        self.current_screen = "login"
        self.create_login_screen()

    def go_to_signup(self, *args, **kwargs):
        self.current_screen = "signup"
        self.create_signup_screen()

    # Authentication methods - STRICT API ONLY with optimized requests
    def _make_api_request(self, method: str, endpoint: str, data: Optional[Dict] = None,
                          headers: Optional[Dict] = None, timeout: Optional[int] = None) -> Tuple[bool, Dict]:
        """Optimized API request with connection pooling"""
        try:
            session = self._connection_pool.get_session()
            timeout = timeout or config.REQUEST_TIMEOUT

            if method.upper() == 'GET':
                response = session.get(
                    get_api_endpoint(endpoint),
                    headers=headers,
                    timeout=timeout
                )
            elif method.upper() == 'POST':
                response = session.post(
                    get_api_endpoint(endpoint),
                    json=data,
                    headers=headers,
                    timeout=timeout
                )
            else:
                return False, {"error": "Unsupported HTTP method"}

            if response.status_code == 200:
                return True, response.json()
            else:
                return False, {"error": f"HTTP {response.status_code}", "status_code": response.status_code}

        except Exception as e:
            return False, {"error": str(e)}

    def attempt_login(self, *args, **kwargs):
        """Attempt user login with optimized API calls"""
        with self._auth_lock:  # Prevent concurrent auth attempts
            try:
                dpg = self._get_dpg()
                email = dpg.get_value("login_email") if dpg.does_item_exist("login_email") else ""
                password = dpg.get_value("login_password") if dpg.does_item_exist("login_password") else ""

                if not email or not password:
                    self.update_status("login_status", "Please fill in all fields")
                    return

                self.update_status("login_status", "🔐 Signing in...")

                success, response_data = self._make_api_request(
                    "POST", "/auth/login",
                    {"email": email, "password": password}
                )

                if success and response_data.get("success"):
                    data = response_data.get("data", {})

                    self.session_data.update({
                        "user_type": "registered",
                        "api_key": data.get("api_key"),
                        "authenticated": True,
                        "device_id": self.generate_device_id()
                    })

                    # Fetch user profile in background
                    self._executor.submit(self.fetch_user_profile)
                    self.update_status("login_status", "✅ Login successful! Opening terminal...")

                    # Delayed close for user feedback
                    threading.Timer(1.0, self.close_splash_success).start()
                else:
                    error_msg = response_data.get("message", "Login failed")
                    self.update_status("login_status", f"❌ {error_msg}")

            except Exception as e:
                self.update_status("login_status", f"❌ Error: {str(e)}")
                logger.error("Login error", module="SplashAuth",
                             context={'error': str(e)}, exc_info=True)

    def attempt_signup(self, *args, **kwargs):
        """Attempt user registration with optimized validation"""
        with self._auth_lock:
            try:
                dpg = self._get_dpg()
                username = dpg.get_value("signup_username") if dpg.does_item_exist("signup_username") else ""
                email = dpg.get_value("signup_email") if dpg.does_item_exist("signup_email") else ""
                password = dpg.get_value("signup_password") if dpg.does_item_exist("signup_password") else ""
                confirm_password = dpg.get_value("signup_confirm_password") if dpg.does_item_exist(
                    "signup_confirm_password") else ""

                # Client-side validation first (faster feedback)
                if not all([username, email, password, confirm_password]):
                    self.update_status("signup_status", "Please fill in all fields")
                    return

                if password != confirm_password:
                    self.update_status("signup_status", "Passwords do not match")
                    return

                if len(password) < 6:  # Basic validation
                    self.update_status("signup_status", "Password must be at least 6 characters")
                    return

                self.update_status("signup_status", "✨ Creating account...")

                success, response_data = self._make_api_request(
                    "POST", "/auth/register",
                    {"username": username, "email": email, "password": password}
                )

                if success and response_data.get("success"):
                    self.pending_email = email
                    self.current_screen = "otp_verify"
                    self.create_otp_screen()
                else:
                    error_msg = response_data.get("message", "Registration failed")
                    self.update_status("signup_status", f"❌ {error_msg}")

            except Exception as e:
                self.update_status("signup_status", f"❌ Error: {str(e)}")
                logger.error("Signup error", module="SplashAuth",
                             context={'error': str(e)}, exc_info=True)

    def verify_otp(self, *args, **kwargs):
        """Verify OTP code with optimized validation"""
        with self._auth_lock:
            try:
                dpg = self._get_dpg()
                otp_code = dpg.get_value("otp_code") if dpg.does_item_exist("otp_code") else ""

                if not otp_code or len(otp_code) != 6 or not otp_code.isdigit():
                    self.update_status("otp_status", "Please enter valid 6-digit code")
                    return

                self.update_status("otp_status", "📧 Verifying...")

                success, response_data = self._make_api_request(
                    "POST", "/auth/verify-otp",
                    {"email": self.pending_email, "otp": otp_code}
                )

                if success and response_data.get("success"):
                    data = response_data.get("data", {})
                    self.session_data.update({
                        "user_type": "registered",
                        "api_key": data.get("api_key"),
                        "authenticated": True,
                        "device_id": self.generate_device_id()
                    })

                    # Fetch user profile in background
                    self._executor.submit(self.fetch_user_profile)
                    self.update_status("otp_status", "✅ Success! Opening terminal...")
                    threading.Timer(1.0, self.close_splash_success).start()
                else:
                    error_msg = response_data.get("message", "Verification failed")
                    self.update_status("otp_status", f"❌ {error_msg}")

            except Exception as e:
                self.update_status("otp_status", f"❌ Error: {str(e)}")
                logger.error("OTP verification error", module="SplashAuth",
                             context={'error': str(e)}, exc_info=True)

    def setup_guest_access(self, *args, **kwargs):
        """Setup guest access with background processing"""
        try:
            self.current_screen = "guest_setup"
            self.create_guest_setup_screen()
        except Exception as e:
            logger.error("Error setting up guest access", module="SplashAuth",
                         context={'error': str(e)}, exc_info=True)
            if is_strict_mode():
                self.update_status("guest_status", f"❌ Guest setup failed: {str(e)}")
            else:
                # Secure fallback only if not in strict mode
                self.session_data.update({
                    "user_type": "guest",
                    "device_id": self.generate_device_id(),
                    "authenticated": True,
                    "api_key": None
                })
                self.close_splash_success()

    def create_guest_session(self):
        """Create guest session with optimized API integration"""
        try:
            device_id = self.generate_device_id()
            hardware_info = self.get_hardware_info()

            # Update UI from background thread
            def update_ui_safe(message):
                try:
                    self.update_status("guest_status", message)
                except:
                    pass  # UI might be destroyed

            update_ui_safe("🌐 Checking for existing session...")

            # Import API client only when needed
            from fincept_terminal.Utils.APIClient.api_client import FinceptAPIClient

            temp_session = {"user_type": "guest", "device_id": device_id}
            api_client = FinceptAPIClient(temp_session)

            # Use optimized session creation
            result = api_client.get_or_create_guest_session(
                device_id=device_id,
                device_name=f"Fincept Terminal - {platform.system()}",
                platform="desktop",
                hardware_info=hardware_info
            )

            if result["success"]:
                guest_data = result.get("data", {})
                message = result.get("message", "Session ready")

                # Update session data atomically
                with self._auth_lock:
                    self.session_data.update({
                        "user_type": "guest",
                        "device_id": device_id,
                        "api_key": guest_data.get("api_key") or guest_data.get("temp_api_key"),
                        "authenticated": True,
                        "expires_at": guest_data.get("expires_at"),
                        "daily_limit": guest_data.get("daily_limit", 50),
                        "requests_today": guest_data.get("requests_today", 0)
                    })

                update_ui_safe(f"✅ {message}!")

                # Enable continue button
                try:
                    dpg = self._get_dpg()
                    if dpg.does_item_exist("guest_continue_btn"):
                        dpg.show_item("guest_continue_btn")
                except:
                    pass
            else:
                error_msg = result.get("error", "Unknown error")
                update_ui_safe(f"❌ Session setup failed: {error_msg}")
                logger.error("Guest session setup failed", module="SplashAuth",
                             context={'error': error_msg, 'device_id': device_id})

        except Exception as e:
            try:
                self.update_status("guest_status", f"❌ Guest creation failed: {str(e)}")
            except:
                pass
            logger.error("Guest session creation error", module="SplashAuth",
                         context={'error': str(e)}, exc_info=True)

    def complete_guest_setup(self, *args, **kwargs):
        """Complete guest setup"""
        self.close_splash_success()

    def fetch_user_profile(self):
        """Fetch user profile with optimized API call"""
        try:
            if not self.session_data.get("api_key"):
                return

            success, response_data = self._make_api_request(
                "GET", "/user/profile",
                headers={"X-API-Key": self.session_data["api_key"]}
            )

            if success and response_data.get("success"):
                with self._auth_lock:
                    self.session_data["user_info"] = response_data.get("data", {})
                logger.info("User profile fetched from API", module="SplashAuth")

        except Exception as e:
            logger.error("Failed to fetch profile from API", module="SplashAuth",
                         context={'error': str(e)}, exc_info=True)

    def update_status(self, tag: str, message: str):
        """Thread-safe status update"""
        try:
            dpg = self._get_dpg()
            if dpg.does_item_exist(tag):
                dpg.set_value(tag, message)
        except Exception as e:
            logger.debug("Could not update status", module="SplashAuth",
                         context={'tag': tag, 'error': str(e)})

    def close_splash_success(self):
        """Close splash successfully with cleanup"""
        try:
            dpg = self._get_dpg()
            dpg.stop_dearpygui()
        except Exception as e:
            logger.error("Error closing splash", module="SplashAuth",
                         context={'error': str(e)}, exc_info=True)

    def cleanup(self):
        """Enhanced cleanup with resource management"""
        try:
            # Shutdown thread pool
            if hasattr(self, '_executor'):
                self._executor.shutdown(wait=False)

            # Close connection pool
            if hasattr(self, '_connection_pool'):
                self._connection_pool.close()

            # Clear caches
            if hasattr(self, '_ui_cache'):
                self._ui_cache.components.clear()

            # Destroy DPG context
            if self.context_created:
                dpg = self._get_dpg()
                dpg.destroy_context()
                self.context_created = False

        except Exception as e:
            logger.error("Cleanup error", module="SplashAuth",
                         context={'error': str(e)}, exc_info=True)

    def __del__(self):
        """Destructor with resource cleanup"""
        self.cleanup()


def show_authentication_splash(is_first_time_user=False) -> Dict[str, Any]:
    """Show splash with optimized performance and preserved security"""
    splash = SplashAuth(is_first_time_user=is_first_time_user)
    try:
        result = splash.show_splash()

        # SECURITY: In strict mode, don't allow fallback
        if is_strict_mode() and not result.get("authenticated"):
            logger.error("Authentication failed in strict mode", module="SplashAuth")
            return {
                "authenticated": False,
                "error": "API authentication required but failed"
            }

        return result
    except Exception as e:
        logger.error("Splash error", module="SplashAuth",
                     context={'error': str(e)}, exc_info=True)

        # SECURITY: In strict mode, return error
        if is_strict_mode():
            return {
                "authenticated": False,
                "error": f"Splash failed: {str(e)}"
            }
        else:
            # SECURITY: Only secure fallback if not in strict mode
            return {
                "user_type": "guest",
                "authenticated": True,
                "device_id": splash.generate_device_id(),
                "api_key": None,
                "user_info": {},
                "expires_at": None
            }
    finally:
        splash.cleanup()