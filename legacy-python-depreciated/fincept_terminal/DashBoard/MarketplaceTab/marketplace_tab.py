import dearpygui.dearpygui as dpg
import sys
import requests
import json
import threading
from typing import Dict, List, Optional
import time


class LiveMarketplaceInterface:
    """Live Financial Data Marketplace Interface with API Integration"""

    def __init__(self):
        self.SCREEN_WIDTH = 1400
        self.SCREEN_HEIGHT = 900
        self.SIDEBAR_WIDTH = 280
        self.CARD_WIDTH = 350
        self.CARD_HEIGHT = 320

        # API Configuration
        self.API_BASE_URL = "https://finceptbackend.share.zrok.io"
        self.API_KEY = ""  # Will be set after login
        self.current_user = None
        self.datasets = []
        self.categories = []
        self.user_purchases = []
        self.my_datasets = []
        self.selected_dataset = None

        # Filters
        self.current_filters = {
            "category": None,
            "price_tier": None,
            "search": ""
        }

        self.setup_ui()

    def make_api_request(self, endpoint: str, method: str = "GET", data: dict = None, files: dict = None) -> dict:
        """Make API request with error handling"""
        try:
            url = f"{self.API_BASE_URL}{endpoint}"
            headers = {}

            if self.API_KEY:
                headers["X-API-Key"] = self.API_KEY

            if method == "GET":
                response = requests.get(url, headers=headers, timeout=10)
            elif method == "POST":
                if files:
                    response = requests.post(url, headers={k: v for k, v in headers.items() if k != "Content-Type"},
                                             data=data, files=files, timeout=30)
                else:
                    headers["Content-Type"] = "application/json"
                    response = requests.post(url, headers=headers, json=data, timeout=10)
            elif method == "DELETE":
                response = requests.delete(url, headers=headers, timeout=10)

            if response.status_code == 200:
                return response.json()
            else:
                error_msg = f"API Error {response.status_code}: {response.text}"
                print(error_msg)
                return {"success": False, "message": error_msg}

        except requests.RequestException as e:
            error_msg = f"Network Error: {str(e)}"
            print(error_msg)
            return {"success": False, "message": error_msg}

    def login_user(self, email: str, password: str) -> bool:
        """Login user and get API key"""
        try:
            login_data = {"email": email, "password": password}
            response = self.make_api_request("/user/login", "POST", login_data)

            if response.get("success") and "api_key" in response.get("data", {}):
                self.API_KEY = response["data"]["api_key"]
                self.load_user_profile()
                return True
            else:
                return False
        except Exception as e:
            print(f"Login error: {e}")
            return False

    def load_user_profile(self):
        """Load user profile information"""
        try:
            response = self.make_api_request("/user/profile")
            if response.get("success"):
                self.current_user = response.get("data", {})
        except Exception as e:
            print(f"Profile load error: {e}")

    def load_datasets(self):
        """Load marketplace datasets"""
        try:
            params = {}
            if self.current_filters["category"]:
                params["category"] = self.current_filters["category"]
            if self.current_filters["price_tier"]:
                params["price_tier"] = self.current_filters["price_tier"]
            if self.current_filters["search"]:
                params["search"] = self.current_filters["search"]

            # Build query string
            query_string = "&".join([f"{k}={v}" for k, v in params.items()])
            endpoint = "/marketplace/datasets"
            if query_string:
                endpoint += f"?{query_string}"

            response = self.make_api_request(endpoint)
            if response.get("success"):
                self.datasets = response.get("data", {}).get("datasets", [])
                return True
            return False
        except Exception as e:
            print(f"Dataset load error: {e}")
            return False

    def load_categories(self):
        """Load available categories"""
        try:
            response = self.make_api_request("/marketplace/categories")
            if response.get("success"):
                self.categories = response.get("data", {}).get("categories", [])
                return True
            return False
        except Exception as e:
            print(f"Categories load error: {e}")
            return False

    def load_user_purchases(self):
        """Load user's dataset purchases"""
        try:
            response = self.make_api_request("/marketplace/my-purchases")
            if response.get("success"):
                self.user_purchases = response.get("data", {}).get("purchases", [])
                return True
            return False
        except Exception as e:
            print(f"Purchases load error: {e}")
            return False

    def load_my_datasets(self):
        """Load user's uploaded datasets"""
        try:
            response = self.make_api_request("/marketplace/my-datasets")
            if response.get("success"):
                self.my_datasets = response.get("data", {}).get("datasets", [])
                return True
            return False
        except Exception as e:
            print(f"My datasets load error: {e}")
            return False

    def purchase_dataset(self, dataset_id: int, payment_method: str = "subscription_credit"):
        """Purchase a dataset"""
        try:
            purchase_data = {"payment_method": payment_method}
            response = self.make_api_request(f"/marketplace/datasets/{dataset_id}/purchase", "POST", purchase_data)
            return response.get("success", False)
        except Exception as e:
            print(f"Purchase error: {e}")
            return False

    def download_dataset(self, dataset_id: int):
        """Download a dataset"""
        try:
            response = self.make_api_request(f"/marketplace/datasets/{dataset_id}/download", "POST")
            return response.get("success", False)
        except Exception as e:
            print(f"Download error: {e}")
            return False

    def setup_ui(self):
        """Initialize UI"""
        try:
            dpg.create_context()
            self.create_theme()

            dpg.create_viewport(
                title="Fincept Live Marketplace",
                width=self.SCREEN_WIDTH,
                height=self.SCREEN_HEIGHT,
                resizable=True
            )

            dpg.setup_dearpygui()

            # Check if user is logged in
            if not self.API_KEY:
                self.create_login_window()
            else:
                self.create_main_interface()

            dpg.bind_theme("marketplace_theme")
            dpg.show_viewport()
            dpg.start_dearpygui()

        except Exception as e:
            print(f"UI setup error: {e}")
            sys.exit(1)
        finally:
            try:
                dpg.destroy_context()
            except:
                pass

    def create_theme(self):
        """Create marketplace theme"""
        try:
            with dpg.theme(tag="marketplace_theme"):
                with dpg.theme_component(dpg.mvAll):
                    # Dark theme colors
                    DARK_BG = [15, 15, 20, 255]
                    MEDIUM_BG = [25, 30, 35, 255]
                    LIGHT_BG = [40, 45, 50, 255]
                    ACCENT = [64, 156, 255, 255]
                    WHITE = [255, 255, 255, 255]
                    GRAY = [160, 160, 160, 255]

                    dpg.add_theme_color(dpg.mvThemeCol_WindowBg, DARK_BG)
                    dpg.add_theme_color(dpg.mvThemeCol_ChildBg, DARK_BG)
                    dpg.add_theme_color(dpg.mvThemeCol_Text, WHITE)
                    dpg.add_theme_color(dpg.mvThemeCol_TextDisabled, GRAY)
                    dpg.add_theme_color(dpg.mvThemeCol_Button, MEDIUM_BG)
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [64, 156, 255, 120])
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [64, 156, 255, 180])
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBg, LIGHT_BG)
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, [64, 156, 255, 60])
                    dpg.add_theme_color(dpg.mvThemeCol_Header, ACCENT)
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, [64, 156, 255, 180])
                    dpg.add_theme_color(dpg.mvThemeCol_Tab, MEDIUM_BG)
                    dpg.add_theme_color(dpg.mvThemeCol_TabHovered, [64, 156, 255, 120])
                    dpg.add_theme_color(dpg.mvThemeCol_TabActive, ACCENT)

                    dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 5)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 3)
                    dpg.add_theme_style(dpg.mvStyleVar_WindowPadding, 10, 10)
                    dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing, 8, 6)

        except Exception as e:
            print(f"Theme creation error: {e}")

    def create_login_window(self):
        """Create login interface"""
        with dpg.window(label="Login to Fincept Marketplace", tag="login_window",
                        width=400, height=300, no_resize=True, modal=True, pos=[500, 300]):
            dpg.add_text("Welcome to Fincept Marketplace")
            dpg.add_separator()

            dpg.add_text("Email:")
            dpg.add_input_text(tag="login_email", width=350)

            dpg.add_text("Password:")
            dpg.add_input_text(tag="login_password", width=350, password=True)

            dpg.add_separator()

            with dpg.group(horizontal=True):
                dpg.add_button(label="Login", width=100, callback=self.handle_login)
                dpg.add_button(label="Cancel", width=100, callback=lambda: dpg.stop_dearpygui())

            dpg.add_text("", tag="login_status", color=[255, 100, 100, 255])

        dpg.set_primary_window("login_window", True)

    def handle_login(self):
        """Handle login button click"""
        email = dpg.get_value("login_email")
        password = dpg.get_value("login_password")

        if not email or not password:
            dpg.set_value("login_status", "Please enter email and password")
            return

        dpg.set_value("login_status", "Logging in...")

        def login_thread():
            success = self.login_user(email, password)

            if success:
                dpg.delete_item("login_window")
                self.create_main_interface()
            else:
                dpg.set_value("login_status", "Login failed. Check credentials.")

        threading.Thread(target=login_thread, daemon=True).start()

    def create_main_interface(self):
        """Create main marketplace interface"""
        # Load initial data
        self.load_categories()
        self.load_datasets()

        with dpg.window(label="Fincept Marketplace", tag="main_window",
                        width=self.SCREEN_WIDTH, height=self.SCREEN_HEIGHT,
                        no_title_bar=True, no_resize=True, no_move=True, pos=[0, 0]):
            # Header
            self.create_header()

            # Tab system
            with dpg.tab_bar(tag="main_tabs"):
                # Marketplace Tab
                with dpg.tab(label="Browse Marketplace", tag="marketplace_tab"):
                    with dpg.group(horizontal=True):
                        self.create_sidebar()
                        self.create_dataset_grid()

                # My Purchases Tab
                with dpg.tab(label="My Purchases", tag="purchases_tab"):
                    self.create_purchases_content()

                # My Datasets Tab
                with dpg.tab(label="My Datasets", tag="my_datasets_tab"):
                    self.create_my_datasets_content()

                # Upload Tab
                with dpg.tab(label="Upload Dataset", tag="upload_tab"):
                    self.create_upload_content()

                # Dataset Details Tab (hidden initially)
                with dpg.tab(label="Dataset Details", tag="details_tab", show=False):
                    self.create_dataset_details_content()

        dpg.set_primary_window("main_window", True)

    def create_header(self):
        """Create header section"""
        with dpg.child_window(width=-1, height=60, border=True, no_scrollbar=True):
            with dpg.group(horizontal=True):
                dpg.add_text("FINCEPT", color=[64, 156, 255, 255])
                dpg.add_text(" Live Marketplace")

                if self.current_user:
                    dpg.add_text(f" | Welcome, {self.current_user.get('username', 'User')}")

                dpg.add_input_text(hint="Search datasets...", width=250, tag="search_input")
                dpg.add_button(label="Search", callback=self.handle_search)

                dpg.add_button(label="Refresh", callback=self.refresh_data)
                dpg.add_button(label="Logout", callback=self.logout)

    def create_sidebar(self):
        """Create filter sidebar"""
        with dpg.child_window(width=self.SIDEBAR_WIDTH, height=-1, border=True):
            dpg.add_text("FILTERS", color=[64, 156, 255, 255])
            dpg.add_separator()

            # Categories filter
            dpg.add_text("Categories")
            dpg.add_combo(tag="category_filter", items=["All"] + [cat.get("category", "") for cat in self.categories],
                          default_value="All", callback=self.apply_filters, width=240)

            # Price tier filter
            dpg.add_text("Price Tier")
            dpg.add_combo(tag="price_filter", items=["All", "free", "basic", "premium", "enterprise"],
                          default_value="All", callback=self.apply_filters, width=240)

            dpg.add_separator()
            dpg.add_button(label="Clear Filters", width=240, callback=self.clear_filters)

            # Dataset count
            dpg.add_text(f"Showing: {len(self.datasets)} datasets")

    def create_dataset_grid(self):
        """Create dataset grid display"""
        with dpg.child_window(width=-1, height=-1, border=True, tag="dataset_grid"):
            self.update_dataset_grid()

    def update_dataset_grid(self):
        """Update dataset grid content"""
        # Clear existing content
        if dpg.does_item_exist("dataset_grid"):
            dpg.delete_item("dataset_grid", children_only=True)

        if not self.datasets:
            dpg.add_text("No datasets found. Try adjusting your filters.", parent="dataset_grid")
            return

        dpg.add_text(f"Available Datasets ({len(self.datasets)})", parent="dataset_grid")
        dpg.add_separator(parent="dataset_grid")

        # Create rows of 3 cards each
        for i in range(0, len(self.datasets), 3):
            with dpg.group(horizontal=True, parent="dataset_grid"):
                for j in range(3):
                    if i + j < len(self.datasets):
                        self.create_dataset_card(self.datasets[i + j])

    def create_dataset_card(self, dataset: dict):
        """Create individual dataset card"""
        with dpg.child_window(width=self.CARD_WIDTH, height=self.CARD_HEIGHT, border=True):
            # Title and uploader
            title = dataset.get("title", "Unknown Dataset")
            if len(title) > 25:
                title = title[:22] + "..."
            dpg.add_text(title, color=[255, 255, 255, 255])

            uploader = dataset.get("uploader", {}).get("username", "Unknown")
            dpg.add_text(f"by {uploader}", color=[160, 160, 160, 255])

            # Category and pricing
            with dpg.group(horizontal=True):
                category = dataset.get("category", "Unknown")
                dpg.add_button(label=category, width=100, height=20, enabled=False)

                pricing = dataset.get("pricing", {})
                price = pricing.get("price_usd", 0)
                if price == 0:
                    dpg.add_text("FREE", color=[0, 255, 100, 255])
                else:
                    dpg.add_text(f"${price:.2f}", color=[255, 140, 0, 255])

            # Description
            description = dataset.get("description", "No description available")
            if len(description) > 120:
                description = description[:117] + "..."
            dpg.add_text(description, wrap=320, color=[180, 180, 180, 255])

            # Metadata
            metadata = dataset.get("metadata", {})
            dpg.add_text(f"Rows: {metadata.get('total_rows', 0):,} | Cols: {metadata.get('total_columns', 0)}")
            dpg.add_text(f"Size: {metadata.get('file_size_mb', 0):.1f} MB")

            # Statistics
            stats = dataset.get("statistics", {})
            dpg.add_text(f"Downloads: {stats.get('download_count', 0)} | Views: {stats.get('view_count', 0)}")

            # Action buttons
            with dpg.group(horizontal=True):
                dataset_id = dataset.get("id")
                dpg.add_button(label="View Details", width=110, height=30,
                               user_data=dataset_id, callback=self.show_dataset_details)

                access = dataset.get("access", {})
                if access.get("can_access", False):
                    dpg.add_button(label="Download", width=80, height=30,
                                   user_data=dataset_id, callback=self.handle_download)
                elif access.get("requires_purchase", False):
                    dpg.add_button(label="Purchase", width=80, height=30,
                                   user_data=dataset_id, callback=self.handle_purchase)
                else:
                    dpg.add_button(label="Locked", width=80, height=30, enabled=False)

    def create_purchases_content(self):
        """Create purchases tab content"""
        with dpg.child_window(width=-1, height=-1, border=True, tag="purchases_content"):
            self.update_purchases_content()

    def update_purchases_content(self):
        """Update purchases content"""
        if dpg.does_item_exist("purchases_content"):
            dpg.delete_item("purchases_content", children_only=True)

        self.load_user_purchases()

        dpg.add_text("My Dataset Purchases", parent="purchases_content")
        dpg.add_separator(parent="purchases_content")

        if not self.user_purchases:
            dpg.add_text("No purchases yet.", parent="purchases_content")
            return

        for purchase in self.user_purchases:
            with dpg.child_window(width=-1, height=80, border=True, parent="purchases_content"):
                dataset = purchase.get("dataset", {})
                dpg.add_text(f"Dataset: {dataset.get('title', 'Unknown')}")
                dpg.add_text(
                    f"Amount: ${purchase.get('amount_paid', 0):.2f} | Status: {purchase.get('status', 'Unknown')}")
                dpg.add_text(f"Purchased: {purchase.get('purchased_at', 'Unknown')}")

    def create_my_datasets_content(self):
        """Create my datasets tab content"""
        with dpg.child_window(width=-1, height=-1, border=True, tag="my_datasets_content"):
            self.update_my_datasets_content()

    def update_my_datasets_content(self):
        """Update my datasets content"""
        if dpg.does_item_exist("my_datasets_content"):
            dpg.delete_item("my_datasets_content", children_only=True)

        self.load_my_datasets()

        dpg.add_text("My Uploaded Datasets", parent="my_datasets_content")
        dpg.add_separator(parent="my_datasets_content")

        if not self.my_datasets:
            dpg.add_text("No datasets uploaded yet.", parent="my_datasets_content")
            return

        for dataset in self.my_datasets:
            with dpg.child_window(width=-1, height=100, border=True, parent="my_datasets_content"):
                dpg.add_text(f"Title: {dataset.get('title', 'Unknown')}")
                dpg.add_text(
                    f"Category: {dataset.get('category', 'Unknown')} | Status: {dataset.get('status', 'Unknown')}")

                stats = dataset.get("statistics", {})
                dpg.add_text(f"Downloads: {stats.get('download_count', 0)} | Views: {stats.get('view_count', 0)}")

                with dpg.group(horizontal=True):
                    if dataset.get("status") == "rejected":
                        dpg.add_text(f"Rejection reason: {dataset.get('admin_notes', 'No reason provided')}",
                                     color=[255, 100, 100, 255])

    def create_upload_content(self):
        """Create upload tab content"""
        with dpg.child_window(width=-1, height=-1, border=True):
            dpg.add_text("Upload New Dataset")
            dpg.add_separator()

            dpg.add_text("Dataset Title:")
            dpg.add_input_text(tag="upload_title", width=400)

            dpg.add_text("Description:")
            dpg.add_input_text(tag="upload_description", width=400, multiline=True, height=100)

            dpg.add_text("Category:")
            categories = [cat.get("category", "") for cat in self.categories] if self.categories else ["stocks",
                                                                                                       "forex",
                                                                                                       "crypto"]
            dpg.add_combo(tag="upload_category", items=categories, width=200)

            dpg.add_text("Price Tier:")
            dpg.add_combo(tag="upload_price_tier", items=["free", "basic", "premium", "enterprise"],
                          default_value="free", width=200)

            dpg.add_text("Tags (comma-separated):")
            dpg.add_input_text(tag="upload_tags", width=400)

            dpg.add_checkbox(label="Requires Subscription", tag="upload_requires_sub")

            dpg.add_text("CSV File:")
            dpg.add_text("Please select a CSV file to upload", tag="file_status")
            dpg.add_button(label="Select File", callback=self.select_file)

            dpg.add_separator()
            dpg.add_button(label="Upload Dataset", width=200, height=40, callback=self.handle_upload)

    def create_dataset_details_content(self):
        """Create dataset details content"""
        dpg.add_text("Loading dataset details...", tag="details_content")

    def select_file(self):
        """Handle file selection"""
        dpg.set_value("file_status", "File selection not implemented in this demo")

    def handle_search(self):
        """Handle search functionality"""
        search_term = dpg.get_value("search_input")
        self.current_filters["search"] = search_term
        self.load_datasets()
        self.update_dataset_grid()

    def apply_filters(self):
        """Apply selected filters"""
        category = dpg.get_value("category_filter")
        price_tier = dpg.get_value("price_filter")

        self.current_filters["category"] = category if category != "All" else None
        self.current_filters["price_tier"] = price_tier if price_tier != "All" else None

        self.load_datasets()
        self.update_dataset_grid()

    def clear_filters(self):
        """Clear all filters"""
        self.current_filters = {"category": None, "price_tier": None, "search": ""}
        dpg.set_value("category_filter", "All")
        dpg.set_value("price_filter", "All")
        dpg.set_value("search_input", "")
        self.load_datasets()
        self.update_dataset_grid()

    def show_dataset_details(self, sender, app_data, user_data):
        """Show dataset details"""
        dataset_id = user_data

        # Find dataset in current list
        dataset = next((d for d in self.datasets if d.get("id") == dataset_id), None)
        if not dataset:
            return

        self.selected_dataset = dataset

        # Clear and update details tab
        if dpg.does_item_exist("details_tab"):
            dpg.delete_item("details_tab", children_only=True)

        # Back button
        dpg.add_button(label="← Back to Marketplace", callback=self.back_to_marketplace, parent="details_tab")

        # Dataset information
        dpg.add_text(dataset.get("title", "Unknown"), color=[255, 255, 255, 255], parent="details_tab")
        uploader = dataset.get("uploader", {}).get("username", "Unknown")
        dpg.add_text(f"by {uploader}", color=[160, 160, 160, 255], parent="details_tab")
        dpg.add_separator(parent="details_tab")

        # Two column layout
        with dpg.group(horizontal=True, parent="details_tab"):
            # Left column - Details
            with dpg.child_window(width=700, height=500, border=True):
                dpg.add_text("Description", color=[64, 156, 255, 255])
                dpg.add_text(dataset.get("description", "No description"), wrap=680)

                dpg.add_text("Dataset Information", color=[64, 156, 255, 255])
                metadata = dataset.get("metadata", {})
                dpg.add_text(f"Rows: {metadata.get('total_rows', 0):,}")
                dpg.add_text(f"Columns: {metadata.get('total_columns', 0)}")
                dpg.add_text(f"File Size: {metadata.get('file_size_mb', 0):.1f} MB")

                # Show column names if available
                file_info = dataset.get("file_info", {})
                columns = file_info.get("column_names", [])
                if columns:
                    dpg.add_text("Columns:", color=[64, 156, 255, 255])
                    dpg.add_text(", ".join(columns[:10]), wrap=680)
                    if len(columns) > 10:
                        dpg.add_text(f"... and {len(columns) - 10} more columns")

            # Right column - Actions
            with dpg.child_window(width=350, height=500, border=True):
                dpg.add_text("Dataset Access", color=[64, 156, 255, 255])

                pricing = dataset.get("pricing", {})
                price = pricing.get("price_usd", 0)

                if price == 0:
                    dpg.add_text("FREE DATASET", color=[0, 255, 100, 255])
                else:
                    dpg.add_text(f"Price: ${price:.2f}", color=[255, 140, 0, 255])

                access = dataset.get("access", {})
                if access.get("can_access", False):
                    dpg.add_button(label="Download Dataset", width=320, height=40,
                                   user_data=dataset_id, callback=self.handle_download)
                elif access.get("requires_purchase", False):
                    dpg.add_button(label="Purchase Dataset", width=320, height=40,
                                   user_data=dataset_id, callback=self.handle_purchase)
                else:
                    dpg.add_text("Access Requirements Not Met", color=[255, 100, 100, 255])

                # Statistics
                dpg.add_text("Statistics", color=[64, 156, 255, 255])
                stats = dataset.get("statistics", {})
                dpg.add_text(f"Downloads: {stats.get('download_count', 0)}")
                dpg.add_text(f"Views: {stats.get('view_count', 0)}")

        # Show details tab
        dpg.configure_item("details_tab", show=True)
        dpg.set_value("main_tabs", "details_tab")

    def back_to_marketplace(self):
        """Return to marketplace tab"""
        dpg.set_value("main_tabs", "marketplace_tab")

    def handle_purchase(self, sender, app_data, user_data):
        """Handle dataset purchase"""
        dataset_id = user_data

        def purchase_thread():
            success = self.purchase_dataset(dataset_id)
            if success:
                # Refresh data after purchase
                self.load_datasets()
                self.update_dataset_grid()
                print("Purchase successful!")
            else:
                print("Purchase failed!")

        threading.Thread(target=purchase_thread, daemon=True).start()

    def handle_download(self, sender, app_data, user_data):
        """Handle dataset download"""
        dataset_id = user_data

        def download_thread():
            success = self.download_dataset(dataset_id)
            if success:
                print("Download successful!")
            else:
                print("Download failed!")

        threading.Thread(target=download_thread, daemon=True).start()

    def handle_upload(self):
        """Handle dataset upload"""
        title = dpg.get_value("upload_title")
        description = dpg.get_value("upload_description")
        category = dpg.get_value("upload_category")
        price_tier = dpg.get_value("upload_price_tier")
        tags = dpg.get_value("upload_tags").split(",")
        requires_sub = dpg.get_value("upload_requires_sub")

        if not title or not description or not category:
            print("Please fill all required fields")
            return

        # In a real implementation, this would handle file upload
        print("Upload functionality requires file selection implementation")

    def refresh_data(self):
        """Refresh all data"""

        def refresh_thread():
            self.load_categories()
            self.load_datasets()
            self.update_dataset_grid()

            # Update other tabs if they're loaded
            if dpg.get_value("main_tabs") == "purchases_tab":
                self.update_purchases_content()
            elif dpg.get_value("main_tabs") == "my_datasets_tab":
                self.update_my_datasets_content()

        threading.Thread(target=refresh_thread, daemon=True).start()

    def logout(self):
        """Logout user"""
        self.API_KEY = ""
        self.current_user = None
        self.datasets = []
        self.categories = []

        # Close main window and show login
        dpg.delete_item("main_window")
        self.create_login_window()


if __name__ == "__main__":
    try:
        print("Initializing Live Marketplace Interface...")
        marketplace = LiveMarketplaceInterface()
    except Exception as e:
        print(f"Fatal error: {e}")
        sys.exit(1)