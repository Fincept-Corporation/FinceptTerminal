# splash_auth.py - Updated with Strict API Mode (No Fallbacks)
import dearpygui.dearpygui as dpg
import requests
import json
import hashlib
import platform
import uuid
import time
from datetime import datetime

# Import centralized config
from fincept_terminal.Utils.config import config, get_api_endpoint, is_strict_mode


class SplashAuth:
    """Splash screen with strict API validation (no fallbacks)"""

    def __init__(self):
        self.current_screen = "welcome"
        self.session_data = {
            "user_type": None,
            "api_key": None,
            "device_id": None,
            "user_info": {},
            "authenticated": False,
            "expires_at": None
        }
        self.context_created = False
        self.pending_email = None

        # Use centralized config
        print(f"🔧 Splash Auth initialized with API: {config.get_api_url()}")
        print(f"🔒 Strict Mode: {is_strict_mode()}")

    def check_api_connectivity(self):
        """Check if API is available before proceeding"""
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

    def show_api_error_screen(self):
        """Show API connection error screen"""
        self.clear_content()
        parent = "content_container"

        self.safe_add_spacer(30, parent)

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=100)
            dpg.add_text("🚨 API Connection Error", color=[255, 100, 100])

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
                dpg.add_button(label="Retry Connection", width=150, callback=self.retry_api_connection)
                dpg.add_spacer(width=20)
                dpg.add_button(label="Exit", width=100, callback=self.close_splash_error)

    def retry_api_connection(self):
        """Retry API connection"""
        if self.check_api_connectivity():
            self.create_welcome_screen()
        else:
            self.show_api_error_screen()

    def close_splash_error(self):
        """Close splash with error"""
        try:
            dpg.stop_dearpygui()
        except:
            pass

    def generate_device_id(self):
        """Generate unique device ID"""
        if not self.session_data["device_id"]:
            system_info = f"{platform.system()}-{platform.node()}-{platform.processor()}"
            device_hash = hashlib.sha256(system_info.encode()).hexdigest()
            self.session_data["device_id"] = f"desktop_{device_hash[:16]}"
        return self.session_data["device_id"]

    def get_hardware_info(self):
        """Get hardware fingerprint"""
        return {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "node": platform.node(),
            "timestamp": datetime.now().isoformat()
        }

    def show_splash(self):
        """Show splash screen with strict API validation"""
        try:
            print("🔐 Creating splash screen with strict API validation...")

            if not self.context_created:
                dpg.create_context()
                self.context_created = True

            with dpg.window(tag="splash_window", label="Fincept Authentication",
                            width=500, height=600, no_resize=True, no_move=True,
                            no_collapse=True, no_close=True):

                with dpg.group(tag="content_container"):
                    # Check API connectivity first
                    if is_strict_mode() and not self.check_api_connectivity():
                        self.show_api_error_screen()
                    else:
                        self.create_welcome_screen()

            dpg.create_viewport(title="Fincept Terminal - Authentication",
                                width=520, height=640, resizable=False)
            dpg.setup_dearpygui()
            dpg.set_primary_window("splash_window", True)

            print("✅ Splash screen created")

            dpg.show_viewport()
            dpg.start_dearpygui()

        except Exception as e:
            print(f"❌ Splash screen error: {e}")
            # In strict mode, return error instead of fallback
            if is_strict_mode():
                return {
                    "authenticated": False,
                    "error": f"Splash initialization failed: {str(e)}"
                }
            else:
                # Fallback only if not in strict mode
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
        """Safely clear content"""
        try:
            if dpg.does_item_exist("content_container"):
                children = dpg.get_item_children("content_container", 1)
                for child in children:
                    if dpg.does_item_exist(child):
                        dpg.delete_item(child)
        except Exception as e:
            print(f"Warning: Error clearing content: {e}")

    def safe_add_spacer(self, height=10, parent="content_container"):
        """Safely add spacer"""
        try:
            if dpg.does_item_exist(parent):
                dpg.add_spacer(height=height, parent=parent)
        except Exception as e:
            print(f"Warning: Could not add spacer: {e}")

    def create_welcome_screen(self):
        """Create welcome screen"""
        self.clear_content()
        parent = "content_container"

        self.safe_add_spacer(20, parent)

        # Logo/Title
        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=80)
            dpg.add_text("🏦 FINCEPT", color=[255, 215, 0])

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=120)
            dpg.add_text("FINANCIAL TERMINAL", color=[200, 200, 200])

        self.safe_add_spacer(20, parent)

        # API Status
        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=60)
            dpg.add_text(f"📡 API: {config.get_api_url()}", color=[100, 255, 100])

        self.safe_add_spacer(30, parent)

        # Authentication cards
        self.create_auth_cards(parent)

        self.safe_add_spacer(30, parent)

        # Footer
        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=150)
            dpg.add_text(f"API v{config.API_VERSION} - Strict Mode", color=[100, 100, 100])

    def create_auth_cards(self, parent):
        """Create authentication cards"""
        # Sign In Card
        with dpg.child_window(width=460, height=100, border=True, parent=parent):
            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                dpg.add_text("🔑 Sign In", color=[100, 255, 100])
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                dpg.add_text("Access your account with permanent API key", color=[200, 200, 200])
            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=350)
                dpg.add_button(label="Sign In", width=100, callback=self.go_to_login)

        self.safe_add_spacer(15, parent)

        # Sign Up Card
        with dpg.child_window(width=460, height=100, border=True, parent=parent):
            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                dpg.add_text("📝 Create Account", color=[100, 150, 255])
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                dpg.add_text("Join Fincept for unlimited access", color=[200, 200, 200])
            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=340)
                dpg.add_button(label="Sign Up", width=110, callback=self.go_to_signup)

        self.safe_add_spacer(15, parent)

        # Guest Card
        with dpg.child_window(width=460, height=100, border=True, parent=parent):
            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                dpg.add_text("👤 Guest Access", color=[255, 255, 100])
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=15)
                dpg.add_text("50 requests/day with temporary API key", color=[200, 200, 200])
            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_spacer(width=300)
                dpg.add_button(label="Continue as Guest", width=150, callback=self.setup_guest_access)

    def create_login_screen(self):
        """Create login screen"""
        self.clear_content()
        parent = "content_container"

        self.safe_add_spacer(30, parent)

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=180)
            dpg.add_text("🔑 Sign In", color=[100, 255, 100])

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
                dpg.add_button(label="Sign In", width=120, callback=self.attempt_login)
                dpg.add_spacer(width=20)
                dpg.add_button(label="Back", width=120, callback=self.go_to_welcome)

    def create_signup_screen(self):
        """Create signup screen"""
        self.clear_content()
        parent = "content_container"

        self.safe_add_spacer(20, parent)

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=170)
            dpg.add_text("📝 Create Account", color=[100, 150, 255])

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
                dpg.add_button(label="Create Account", width=140, callback=self.attempt_signup)
                dpg.add_spacer(width=20)
                dpg.add_button(label="Back", width=120, callback=self.go_to_welcome)

    def create_otp_screen(self):
        """Create OTP verification screen"""
        self.clear_content()
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
                dpg.add_button(label="Verify Code", width=120, callback=self.verify_otp)
                dpg.add_spacer(width=20)
                dpg.add_button(label="Back", width=120, callback=self.go_to_signup)

    def create_guest_setup_screen(self):
        """Create guest setup screen"""
        self.clear_content()
        parent = "content_container"

        self.safe_add_spacer(40, parent)

        with dpg.group(horizontal=True, parent=parent):
            dpg.add_spacer(width=130)
            dpg.add_text("👤 Setting up Guest Access", color=[255, 255, 100])

        self.safe_add_spacer(30, parent)

        with dpg.child_window(width=460, height=350, border=True, parent=parent):
            dpg.add_spacer(height=30)

            with dpg.group(horizontal=True):
                dpg.add_spacer(width=30)
                dpg.add_text("Guest Features:", color=[100, 255, 100])

            dpg.add_spacer(height=15)

            features = [
                "✓ Financial market data access",
                "✓ Real-time stock prices & forex",
                "✓ 50 API requests per day",
                "✓ 24-hour access period",
                "✓ Temporary API key authentication"
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
                dpg.add_button(label="Continue to Terminal", width=200,
                               callback=self.complete_guest_setup, show=False, tag="guest_continue_btn")

        # Start guest session creation
        self.create_guest_session()

    # Navigation methods
    def go_to_welcome(self):
        self.current_screen = "welcome"
        self.create_welcome_screen()

    def go_to_login(self):
        self.current_screen = "login"
        self.create_login_screen()

    def go_to_signup(self):
        self.current_screen = "signup"
        self.create_signup_screen()

    # Authentication methods - STRICT API ONLY
    def attempt_login(self):
        """Attempt user login - REQUIRES API"""
        try:
            email = dpg.get_value("login_email") if dpg.does_item_exist("login_email") else ""
            password = dpg.get_value("login_password") if dpg.does_item_exist("login_password") else ""

            if not email or not password:
                self.update_status("login_status", "Please fill in all fields")
                return

            self.update_status("login_status", "Signing in...")

            response = requests.post(
                get_api_endpoint("/auth/login"),
                json={"email": email, "password": password},
                timeout=config.REQUEST_TIMEOUT
            )

            if response.status_code == 200:
                data = response.json()
                if data.get("success"):
                    response_data = data.get("data", {})

                    self.session_data.update({
                        "user_type": "registered",
                        "api_key": response_data.get("api_key"),
                        "authenticated": True,
                        "device_id": self.generate_device_id()
                    })

                    # Fetch user profile
                    self.fetch_user_profile()
                    self.update_status("login_status", "Login successful! Opening terminal...")
                    self.close_splash_success()
                else:
                    self.update_status("login_status", data.get("message", "Login failed"))
            else:
                self.update_status("login_status", f"Login failed: {response.status_code}")

        except requests.exceptions.ConnectionError:
            self.update_status("login_status", f"Cannot connect to API server")
        except requests.exceptions.Timeout:
            self.update_status("login_status", "Request timeout - API server not responding")
        except Exception as e:
            self.update_status("login_status", f"Error: {str(e)}")

    def attempt_signup(self):
        """Attempt user registration - REQUIRES API"""
        try:
            username = dpg.get_value("signup_username") if dpg.does_item_exist("signup_username") else ""
            email = dpg.get_value("signup_email") if dpg.does_item_exist("signup_email") else ""
            password = dpg.get_value("signup_password") if dpg.does_item_exist("signup_password") else ""
            confirm_password = dpg.get_value("signup_confirm_password") if dpg.does_item_exist(
                "signup_confirm_password") else ""

            if not all([username, email, password, confirm_password]):
                self.update_status("signup_status", "Please fill in all fields")
                return

            if password != confirm_password:
                self.update_status("signup_status", "Passwords do not match")
                return

            self.update_status("signup_status", "Creating account...")

            response = requests.post(
                get_api_endpoint("/auth/register"),
                json={"username": username, "email": email, "password": password},
                timeout=config.REQUEST_TIMEOUT
            )

            if response.status_code == 200:
                data = response.json()
                if data.get("success"):
                    self.pending_email = email
                    self.current_screen = "otp_verify"
                    self.create_otp_screen()
                else:
                    self.update_status("signup_status", data.get("message", "Registration failed"))
            else:
                self.update_status("signup_status", f"Registration failed: {response.status_code}")

        except requests.exceptions.ConnectionError:
            self.update_status("signup_status", "Cannot connect to API server")
        except requests.exceptions.Timeout:
            self.update_status("signup_status", "Request timeout - API server not responding")
        except Exception as e:
            self.update_status("signup_status", f"Error: {str(e)}")

    def verify_otp(self):
        """Verify OTP code - REQUIRES API"""
        try:
            otp_code = dpg.get_value("otp_code") if dpg.does_item_exist("otp_code") else ""

            if not otp_code or len(otp_code) != 6:
                self.update_status("otp_status", "Please enter valid 6-digit code")
                return

            self.update_status("otp_status", "Verifying...")

            response = requests.post(
                get_api_endpoint("/auth/verify-otp"),
                json={"email": self.pending_email, "otp": otp_code},
                timeout=config.REQUEST_TIMEOUT
            )

            if response.status_code == 200:
                data = response.json()
                if data.get("success"):
                    self.session_data.update({
                        "user_type": "registered",
                        "api_key": data["data"]["api_key"],
                        "authenticated": True,
                        "device_id": self.generate_device_id()
                    })

                    self.fetch_user_profile()
                    self.update_status("otp_status", "Success! Opening terminal...")
                    self.close_splash_success()
                else:
                    self.update_status("otp_status", data.get("message", "Verification failed"))
            else:
                self.update_status("otp_status", f"Verification failed: {response.status_code}")

        except requests.exceptions.ConnectionError:
            self.update_status("otp_status", "Cannot connect to API server")
        except requests.exceptions.Timeout:
            self.update_status("otp_status", "Request timeout - API server not responding")
        except Exception as e:
            self.update_status("otp_status", f"Error: {str(e)}")

    def setup_guest_access(self):
        """Setup guest access - REQUIRES API"""
        try:
            self.current_screen = "guest_setup"
            self.create_guest_setup_screen()
        except Exception as e:
            print(f"Error setting up guest access: {e}")
            if is_strict_mode():
                self.update_status("guest_status", f"Guest setup failed: {str(e)}")
            else:
                # Only fallback if not in strict mode
                self.session_data.update({
                    "user_type": "guest",
                    "device_id": self.generate_device_id(),
                    "authenticated": True,
                    "api_key": None
                })
                self.close_splash_success()

    def create_guest_session(self):
        """Create guest session - REQUIRES API (NO FALLBACK)"""
        try:
            device_id = self.generate_device_id()
            hardware_info = self.get_hardware_info()

            self.update_status("guest_status", "Connecting to API...")

            response = requests.post(
                get_api_endpoint("/device/register"),
                json={
                    "device_id": device_id,
                    "device_name": f"Fincept Terminal - {platform.system()}",
                    "platform": "desktop",
                    "hardware_info": hardware_info
                },
                timeout=config.REQUEST_TIMEOUT
            )

            if response.status_code == 200:
                data = response.json()
                if data.get("success"):
                    response_data = data.get("data", {})

                    self.session_data.update({
                        "user_type": "guest",
                        "device_id": device_id,
                        "api_key": response_data.get("api_key"),
                        "authenticated": True,
                        "expires_at": response_data.get("expires_at"),
                        "daily_limit": response_data.get("daily_limit", 50)
                    })

                    self.update_status("guest_status", "Guest API key created successfully!")
                    if dpg.does_item_exist("guest_continue_btn"):
                        dpg.show_item("guest_continue_btn")
                else:
                    self.update_status("guest_status", f"Registration failed: {data.get('message')}")
            else:
                self.update_status("guest_status", f"Registration failed: HTTP {response.status_code}")

        except requests.exceptions.ConnectionError:
            self.update_status("guest_status", f"Cannot connect to API server at {config.get_api_url()}")
            print(f"❌ Guest session creation failed: Cannot connect to API")
        except requests.exceptions.Timeout:
            self.update_status("guest_status", "Request timeout - API server not responding")
            print(f"❌ Guest session creation failed: Timeout")
        except Exception as e:
            self.update_status("guest_status", f"Guest creation failed: {str(e)}")
            print(f"❌ Guest session creation error: {e}")

    def complete_guest_setup(self):
        """Complete guest setup"""
        self.close_splash_success()

    def fetch_user_profile(self):
        """Fetch user profile - REQUIRES API"""
        try:
            if not self.session_data.get("api_key"):
                return

            headers = {"X-API-Key": self.session_data["api_key"]}
            response = requests.get(
                get_api_endpoint("/user/profile"),
                headers=headers,
                timeout=config.REQUEST_TIMEOUT
            )

            if response.status_code == 200:
                data = response.json()
                if data.get("success"):
                    self.session_data["user_info"] = data["data"]
                    print("✅ User profile fetched from API")

        except Exception as e:
            print(f"Failed to fetch profile from API: {e}")

    def update_status(self, tag, message):
        """Safely update status text"""
        try:
            if dpg.does_item_exist(tag):
                dpg.set_value(tag, message)
        except Exception as e:
            print(f"Could not update status {tag}: {e}")

    def close_splash_success(self):
        """Close splash successfully"""
        try:
            time.sleep(0.5)
            dpg.stop_dearpygui()
        except Exception as e:
            print(f"Error closing splash: {e}")

    def cleanup(self):
        """Clean up safely"""
        try:
            if self.context_created:
                dpg.destroy_context()
                self.context_created = False
        except Exception as e:
            print(f"Cleanup error: {e}")


def show_authentication_splash():
    """Show splash and return session data"""
    splash = SplashAuth()
    try:
        result = splash.show_splash()

        # In strict mode, don't allow fallback
        if is_strict_mode() and not result.get("authenticated"):
            print("❌ Authentication failed in strict mode")
            return {
                "authenticated": False,
                "error": "API authentication required but failed"
            }

        return result
    except Exception as e:
        print(f"❌ Splash error: {e}")

        # In strict mode, return error
        if is_strict_mode():
            return {
                "authenticated": False,
                "error": f"Splash failed: {str(e)}"
            }
        else:
            # Only fallback if not in strict mode
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