# -*- coding: utf-8 -*-
# main.py

import sys
import gc
import threading
import time
import warnings
from pathlib import Path
from datetime import datetime
from typing import Dict, Any, Optional, Set
import asyncio
from concurrent.futures import ThreadPoolExecutor

import dearpygui.dearpygui as dpg
import requests

import fincept_terminal.DashBoard.WatchListTab.watchlist_tab
from fincept_terminal.menu_toolbar import MenuToolbarManager

# Suppress warnings for cleaner output
warnings.filterwarnings("ignore", category=UserWarning)
warnings.filterwarnings("ignore", category=DeprecationWarning)
warnings.filterwarnings("ignore", category=FutureWarning)

# PERFORMANCE: Import only essential logging functions, avoid heavy decorators during startup
from fincept_terminal.Utils.Logging.logger import (
    info, error, debug, warning, critical, set_debug_mode, setup_for_gui,
    monitor_performance, operation, get_stats, health_check
)

# Import centralized config
from fincept_terminal.Utils.config import config, get_api_endpoint, is_strict_mode

# Import session manager
from fincept_terminal.Utils.Managers.session_manager import session_manager


class PerformantTabImporter:
    """OPTIMIZED: Lightweight tab importer with minimal overhead"""

    def __init__(self):
        self.available_tabs = {}
        self.failed_imports = {}
        self.import_stats = {'successful': 0, 'failed': 0, 'total_time': 0}
        self._module_cache = {}  # Cache imported modules

    def safe_import_tab(self, tab_name: str, module_name: str, class_name: str):
        """OPTIMIZED: Fast import with caching and minimal logging"""
        try:
            # Check cache first
            cache_key = f"{module_name}.{class_name}"
            if cache_key in self._module_cache:
                return self._module_cache[cache_key]

            module = __import__(f"{module_name}", fromlist=[class_name])
            tab_class = getattr(module, class_name)

            # Cache the result
            self._module_cache[cache_key] = tab_class
            self.import_stats['successful'] += 1
            return tab_class

        except ImportError as e:
            self.failed_imports[tab_name] = {'error': str(e), 'type': 'ImportError'}
            self.import_stats['failed'] += 1
            return None
        except AttributeError as e:
            self.failed_imports[tab_name] = {'error': f"Class {class_name} not found", 'type': 'AttributeError'}
            self.import_stats['failed'] += 1
            return None
        except Exception as e:
            error(f"Tab import failed: {tab_name} - {str(e)}", module='main')
            self.failed_imports[tab_name] = {'error': f"Unexpected error: {str(e)}", 'type': type(e).__name__}
            self.import_stats['failed'] += 1
            return None

    @monitor_performance
    def load_all_tabs_parallel(self) -> Dict[str, Any]:
        """OPTIMIZED: Parallel tab loading with controlled concurrency"""
        TAB_IMPORTS = [
            ("Dashboard", "fincept_terminal.DashBoard.DashboardTab.dashboard_tab", "DashboardTab"),
            ("Markets", "fincept_terminal.DashBoard.MarketTab.market_tab", "MarketTab"),
            ("News", "fincept_terminal.DashBoard.NewsAnalysisTab.news_analysis_tab", "NewsAnalysisTab"),
            ("Analytics", "fincept_terminal.DashBoard.AnalyticsTab.data_viewer_tab", "DataViewerTab"),
            ("Watchlist", "fincept_terminal.DashBoard.WatchListTab.watchlist_tab", "WatchlistTab"),
            ("oecd", "fincept_terminal.DashBoard.AnalyticsTab.oecd_data_tab", "OECDDataTab"),
            ("NSE India", "fincept_terminal.DashBoard.InfoTab.rss_tab", "RssTab"),
            ("Forum", "fincept_terminal.DashBoard.ForumTab.forum_tab", "ForumTab"),
            ("Chat", "fincept_terminal.DashBoard.ChatTab.chat_tab", "ChatTab"),
            ("Maps", "fincept_terminal.DashBoard.MarineTrackerTab.maps_tab", "MaritimeMapTab"),
            ("Portfolio", "fincept_terminal.DashBoard.PortfolioTab.portfolio_tab", "PortfolioTab"),
            ("Database", "fincept_terminal.DashBoard.DatabaseTab.database_tab", "DatabaseTab"),
            ("World Trade", "fincept_terminal.DashBoard.WorldTradeTab.world_trade_analysis", "WorldTradeAnalysisTab"),
            ("Economy", "fincept_terminal.DashBoard.DBnomicsTab.dbnomics_tab", "DBnomicsTab"),
            ("Wiki", "fincept_terminal.DashBoard.InfoTab.wiki_tab", "WikipediaTab"),
            ("Equity Research", "fincept_terminal.DashBoard.EquityResearchTab.yfdata", "YFinanceDataTab"),
            ("Fyers", "fincept_terminal.Brokers.India.fyers.fyers_tab", "FyersTab"),
            ("Geopolitical", "fincept_terminal.DashBoard.GeoPoliticsTab.geo", "GeopoliticalAnalysisTab"),
            ("Technicals", "fincept_terminal.DashBoard.TechnicalsTab.technical_tab", "AdvancedNodeEditorTab"),
            ("India Data", "fincept_terminal.DatabaseConnector.DataGovGlobal.GovDataIN.indiagov_tab", "DataGovIndiaTab"),
            ("Robo Advisor", "fincept_terminal.DashBoard.RoboAdvisorTab.robo_advisor_tab", "RoboAdvisorTab"),
            ("Consumer", "fincept_terminal.DashBoard.EquityResearchTab.consumer_behaviour_tab", "ConsumerBehaviorTab"),
            ("Profile", "fincept_terminal.DashBoard.ProfileTab.profile_tab", "ProfileTab"),
            ("Settings", "fincept_terminal.DashBoard.SettingsTab.settings_tab", "SettingsTab"),
            ("Help", "fincept_terminal.Utils.HelpTab.help_tab", "HelpTab"),
        ]

        available_tabs = {}

        # PERFORMANCE: Use ThreadPoolExecutor for parallel imports
        max_workers = min(6, len(TAB_IMPORTS))  # Limit concurrent imports

        with ThreadPoolExecutor(max_workers=max_workers, thread_name_prefix="TabImport") as executor:
            # Submit all import tasks
            future_to_tab = {
                executor.submit(self.safe_import_tab, tab_id, module_name, class_name): tab_id
                for tab_id, module_name, class_name in TAB_IMPORTS
            }

            # Collect results as they complete
            for future in future_to_tab:
                tab_id = future_to_tab[future]
                try:
                    tab_class = future.result(timeout=10)  # 10 second timeout per tab
                    if tab_class is not None:
                        available_tabs[tab_id] = tab_class
                except Exception as e:
                    error(f"Tab loading timeout/error: {tab_id} - {str(e)}", module='main')

        # Update statistics
        success_count = len(available_tabs)
        failed_count = len(TAB_IMPORTS) - success_count

        self.import_stats.update({
            'successful': success_count,
            'failed': failed_count
        })

        # MINIMAL logging - only log summary
        info(f"Tab loading: {success_count}/{len(TAB_IMPORTS)} tabs loaded", module='main')

        if failed_count > 0:
            warning(f"{failed_count} tabs failed to load", module='main')

        return available_tabs


class OptimizedThemeManager:
    """OPTIMIZED: Lightweight theme manager with lazy loading"""

    def __init__(self):
        self.theme_manager = None
        self.themes_available = False
        self.current_theme = "default"
        self._initialization_attempted = False

    def _lazy_initialize(self):
        """PERFORMANCE: Only initialize when first used"""
        if self._initialization_attempted:
            return

        self._initialization_attempted = True
        try:
            from fincept_terminal.Utils.Managers.theme_manager import AutomaticThemeManager
            self.theme_manager = AutomaticThemeManager()
            self.themes_available = True
        except Exception:
            self.themes_available = False

    def apply_theme(self, theme_name: str) -> bool:
        """OPTIMIZED: Apply theme with lazy initialization"""
        self._lazy_initialize()

        if not self.themes_available or not self.theme_manager:
            return False

        try:
            self.theme_manager.apply_theme_globally(theme_name)
            self.current_theme = theme_name
            return True
        except Exception as e:
            error(f"Theme application failed: {theme_name} - {str(e)}", module='theme')
            return False

    def get_current_theme(self) -> str:
        return self.current_theme

    def get_available_themes(self) -> list:
        self._lazy_initialize()
        if not self.themes_available:
            return []
        return ["finance_terminal", "green_terminal", "dark_gold", "default"]

    def cleanup(self):
        """OPTIMIZED: Lightweight cleanup"""
        try:
            if self.theme_manager and hasattr(self.theme_manager, 'cleanup'):
                self.theme_manager.cleanup()
        except Exception:
            pass  # Silent cleanup


class HighPerformanceMainApplication:
    """OPTIMIZED: High-performance main application with minimal overhead"""

    def __init__(self, session_data: Dict[str, Any]):
        # PERFORMANCE: Minimal logging during initialization

        # Application state
        self.is_running = True
        self.resize_lock = False
        self.api_request_count = 0
        self.last_resize_time = 0
        self.menu_toolbar_manager = MenuToolbarManager(self)


        # Store session data
        self.session_data = session_data
        self.user_type = session_data.get("user_type", "guest")
        self.authenticated = session_data.get("authenticated", False)
        self.api_key = session_data.get("api_key")

        # PERFORMANCE: Cache auth headers to avoid regeneration
        self._cached_auth_headers = None
        self._auth_headers_timestamp = 0
        self._auth_cache_ttl = 300  # 5 minutes

        # Layout configuration
        self.DEFAULT_WIDTH = 1200
        self.DEFAULT_HEIGHT = 600
        self.MARGIN_WIDTH = 20
        self.MARGIN_HEIGHT = 120
        self.MIN_WIDTH = 800
        self.MIN_HEIGHT = 600

        # PERFORMANCE: Initialize managers with lazy loading
        self.theme_manager = OptimizedThemeManager()
        self.tab_importer = PerformantTabImporter()

        # Calculate responsive sizes
        self.calculate_sizes()

        # Initialize tabs
        self.tabs = {}
        self.tabs_initialized: Set[str] = set()

        # PERFORMANCE: Don't initialize tabs here, do it later
        info(f"Application initialized - User: {self.user_type}", module='main')

    def _initialize_tabs_optimized(self):
        """OPTIMIZED: Fast tab initialization with parallel loading"""
        with operation("tab_initialization", module='main'):
            # Load tabs in parallel
            available_tabs = self.tab_importer.load_all_tabs_parallel()

            if not available_tabs:
                critical("No tabs available - application cannot continue", module='main')
                self.safe_exit()
                return

            # Initialize tab instances with batch processing
            failed_tabs = []
            for tab_id, tab_class in available_tabs.items():
                try:
                    self.tabs[tab_id] = tab_class(self)
                    self.tabs_initialized.add(tab_id)
                except Exception as e:
                    failed_tabs.append(tab_id)
                    error(f"Tab init failed: {tab_id} - {str(e)}", module='main')

            success_count = len(self.tabs_initialized)
            info(f"Tab initialization: {success_count} tabs ready", module='main')

            if failed_tabs:
                warning(f"Failed tab inits: {failed_tabs}", module='main')

    def calculate_sizes(self):
        """OPTIMIZED: Lightweight size calculation"""
        # Ensure minimum sizes
        width = max(self.DEFAULT_WIDTH, self.MIN_WIDTH)
        height = max(self.DEFAULT_HEIGHT, self.MIN_HEIGHT)

        self.usable_width = width - self.MARGIN_WIDTH
        self.usable_height = height - self.MARGIN_HEIGHT

        # Calculate proportional widths with minimum constraints
        min_panel_width = 200
        self.left_width = max(int(self.usable_width * 0.25) - 3, min_panel_width)
        self.center_width = max(int(self.usable_width * 0.50) - 3, min_panel_width)
        self.right_width = max(int(self.usable_width * 0.25) - 3, min_panel_width)

        # Calculate heights
        min_panel_height = 150
        self.top_height = max(int(self.usable_height * 0.66) - 3, min_panel_height)
        self.bottom_height = max(int(self.usable_height * 0.34) - 3, min_panel_height)
        self.cell_height = max(int(self.top_height / 2) - 2, 100)

    def get_api_key_type(self) -> str:
        """OPTIMIZED: Fast API key type detection"""
        if not self.api_key:
            return "Offline"

        # Use string startswith for faster checking
        if self.api_key.startswith("fk_guest_"):
            return "Guest"
        elif self.api_key.startswith("fk_user_"):
            return "User"
        elif self.api_key.startswith("fk_dev_"):
            return "Developer"
        elif self.api_key.startswith("fk_admin_"):
            return "Admin"
        else:
            return "Legacy"

    def resize_callback(self, sender=None, app_data=None):
        """OPTIMIZED: Lightweight resize with debouncing"""
        if self.resize_lock or not self.is_running:
            return

        current_time = time.time()

        # PERFORMANCE: Increased debounce time to reduce callback frequency
        if current_time - self.last_resize_time < 0.2:  # 200ms debounce
            return

        try:
            self.resize_lock = True
            self.last_resize_time = current_time

            viewport_width = dpg.get_viewport_width()
            viewport_height = dpg.get_viewport_height()

            # Update layout sizes based on new viewport dimensions
            if viewport_width > self.MIN_WIDTH and viewport_height > self.MIN_HEIGHT:
                self.DEFAULT_WIDTH = viewport_width
                self.DEFAULT_HEIGHT = viewport_height
                self.calculate_sizes()

        except Exception as e:
            error(f"Resize failed: {str(e)}", module='main')
        finally:
            self.resize_lock = False

    def safe_exit(self):
        """OPTIMIZED: Fast exit"""
        info("Application exit requested", module='main')
        self.is_running = False

        try:
            if dpg.is_dearpygui_running():
                dpg.stop_dearpygui()
        except Exception as e:
            error(f"Exit error: {str(e)}", module='main')

    def switch_to_tab_with_ticker(self, tab_label: str, ticker: str):
        """Switch to a specific tab and load ticker data - MINIMAL IMPLEMENTATION"""
        try:
            # Find the tab by label
            target_tab = None
            target_tab_key = None

            for tab_name, tab_instance in self.tabs.items():
                # Check if tab has the matching label
                if hasattr(tab_instance, 'get_label'):
                    if tab_instance.get_label() == tab_label:
                        target_tab = tab_instance
                        target_tab_key = tab_name
                        break
                elif tab_name == tab_label:  # Fallback to tab name
                    target_tab = tab_instance
                    target_tab_key = tab_name
                    break

            if not target_tab:
                warning(f"Tab '{tab_label}' not found", module='main')
                return False

            # Switch to the tab in the tab bar
            tab_id = f"tab_{target_tab_key}"
            if dpg.does_item_exist("main_tab_bar") and dpg.does_item_exist(tab_id):
                dpg.set_value("main_tab_bar", tab_id)

            # Load ticker data in the target tab
            if hasattr(target_tab, 'load_ticker_from_external'):
                target_tab.load_ticker_from_external(ticker)
                info(f"Switched to {tab_label} with ticker {ticker}", module='main')
                return True
            else:
                warning(f"Tab {tab_label} doesn't support external ticker loading", module='main')
                return False

        except Exception as e:
            error(f"Tab switch failed: {tab_label} with {ticker} - {str(e)}", module='main')
            return False

    def get_tab_by_label(self, label: str):
        """Get tab instance by label - UTILITY METHOD"""
        for tab_name, tab_instance in self.tabs.items():
            if hasattr(tab_instance, 'get_label') and tab_instance.get_label() == label:
                return tab_name, tab_instance
            elif tab_name == label:  # Fallback
                return tab_name, tab_instance
        return None, None

    def create_menu_bar(self):
        """Create enhanced menu bar with tab navigation - delegated to MenuToolbarManager"""
        self.menu_toolbar_manager.create_menu_bar()

    # Tab navigation methods
    def scroll_tab_view(self, direction):
        """Navigate through tabs using menu"""
        try:
            total_tabs = len(self.tab_names_list)

            if direction > 0:  # Next tabs
                if self.current_visible_tab_start + self.tabs_per_view < total_tabs:
                    self.current_visible_tab_start += self.tabs_per_view
            else:  # Previous tabs
                if self.current_visible_tab_start > 0:
                    self.current_visible_tab_start = max(0, self.current_visible_tab_start - self.tabs_per_view)

            self.update_tab_visibility()

            # Show a status message
            start = self.current_visible_tab_start + 1
            end = min(self.current_visible_tab_start + self.tabs_per_view, total_tabs)
            info(f"Showing tabs {start}-{end} of {total_tabs}", module='main')

        except Exception as e:
            error(f"Tab scroll failed: {str(e)}", module='main')

    def update_tab_visibility(self):
        """Show/hide tabs based on current view"""
        try:
            for i, tab_name in enumerate(self.tab_names_list):
                tab_id = f"tab_{tab_name}"
                if dpg.does_item_exist(tab_id):
                    if self.current_visible_tab_start <= i < self.current_visible_tab_start + self.tabs_per_view:
                        dpg.show_item(tab_id)
                    else:
                        dpg.hide_item(tab_id)
        except Exception as e:
            error(f"Tab visibility update failed: {str(e)}", module='main')

    def jump_to_specific_tab(self, tab_name):
        """Jump directly to a specific tab"""
        try:
            if tab_name in self.tab_names_list:
                tab_index = self.tab_names_list.index(tab_name)
                # Calculate which page this tab is on
                target_page = tab_index // self.tabs_per_view
                self.current_visible_tab_start = target_page * self.tabs_per_view

                # Update visibility
                self.update_tab_visibility()

                # Switch to the tab
                tab_id = f"tab_{tab_name}"
                if dpg.does_item_exist(tab_id):
                    dpg.set_value("main_tab_bar", tab_id)

        except Exception as e:
            error(f"Tab jump failed: {tab_name} - {str(e)}", module='main')

    @monitor_performance
    def create_tabs(self):
        """OPTIMIZED: Fast tab creation with minimal overhead"""
        try:
            # Create tab bar
            dpg.add_tab_bar(tag="main_tab_bar", reorderable=True)

            successful_tabs = 0
            failed_tabs = 0

            # Create tabs efficiently
            for tab_name, tab_instance in self.tabs.items():
                try:
                    tab_id = f"tab_{tab_name}"

                    # Get label from tab instance with fallback
                    try:
                        label = tab_instance.get_label() if hasattr(tab_instance, 'get_label') else tab_name.title()
                    except Exception:
                        label = tab_name.title()

                    dpg.add_tab(label=label, tag=tab_id, parent="main_tab_bar")

                    # Add content with error protection
                    try:
                        dpg.push_container_stack(tab_id)
                        tab_instance.create_content()
                        dpg.pop_container_stack()
                        successful_tabs += 1

                    except Exception as content_error:
                        # Ensure container stack is clean
                        try:
                            dpg.pop_container_stack()
                        except Exception:
                            pass

                        # Create error display
                        dpg.push_container_stack(tab_id)
                        dpg.add_text(f"Error loading {tab_name} tab", color=[255, 100, 100])
                        dpg.add_text(f"Error: {str(content_error)[:100]}...")

                        if tab_name.lower() == "profile":
                            dpg.add_text("This may be due to missing session data.")
                            dpg.add_button(label="Clear Session & Restart",
                                           callback=self.clear_session_and_restart)

                        dpg.add_button(label="Retry Tab Loading",
                                       callback=lambda s, a, u, tn=tab_name: self.retry_tab_loading(tn))
                        dpg.pop_container_stack()

                        error(f"Tab content creation failed: {tab_name} - {str(content_error)}", module='main')
                        failed_tabs += 1

                except Exception as tab_error:
                    error(f"Tab creation failed: {tab_name} - {str(tab_error)}", module='main')
                    failed_tabs += 1

            # Initialize tab navigation
            self.current_visible_tab_start = 0
            self.tabs_per_view = 30
            self.tab_names_list = list(self.tabs.keys())

            # Only hide tabs if we have more than can fit
            if len(self.tabs) > self.tabs_per_view:
                self.update_tab_visibility()

            info(f"Tab creation completed: {successful_tabs} tabs", module='main')

        except Exception as e:
            critical(f"Critical tab creation error: {str(e)}", module='main')
            # Create minimal emergency interface
            dpg.add_text("Tab System Error", color=[255, 100, 100])
            dpg.add_text("The tab system failed to initialize properly.")
            dpg.add_button(label="Restart Application", callback=self.safe_exit)
            dpg.add_button(label="Show Diagnostics", callback=self.show_diagnostics)

    # OPTIMIZED: Cached auth headers to avoid repeated generation
    def get_auth_headers(self) -> Dict[str, str]:
        """PERFORMANCE: Cached authentication headers"""
        current_time = time.time()

        # Check cache validity
        if (self._cached_auth_headers and
                current_time - self._auth_headers_timestamp < self._auth_cache_ttl):
            return self._cached_auth_headers

        try:
            headers = config.get_request_headers(self.api_key)
            self._cached_auth_headers = headers
            self._auth_headers_timestamp = current_time
            return headers
        except Exception as e:
            error(f"Auth headers generation failed: {str(e)}", module='api')
            return {}

    def make_api_request(self, method: str, endpoint: str, **kwargs) -> Optional[requests.Response]:
        """OPTIMIZED: Fast API request with caching"""
        try:
            headers = kwargs.get("headers", {})
            auth_headers = self.get_auth_headers()
            headers.update(auth_headers)
            kwargs["headers"] = headers

            url = get_api_endpoint(endpoint)
            timeout_config = config.get_timeout_config()
            kwargs.setdefault("timeout", timeout_config['total'])

            response = getattr(requests, method.lower())(url, **kwargs)
            self.api_request_count += 1

            return response

        except requests.exceptions.Timeout:
            warning(f"API timeout: {method} {endpoint}", module='api')
            return None
        except requests.exceptions.ConnectionError:
            warning(f"API connection error: {method} {endpoint}", module='api')
            return None
        except Exception as e:
            error(f"API request failed: {method} {endpoint} - {str(e)}", module='api')
            return None

    # PERFORMANCE: Lightweight placeholder methods for callbacks
    def retry_tab_loading(self, tab_name: str):
        """Retry loading a specific tab"""
        info(f"Retrying tab: {tab_name}", module='main')

    def new_session(self):
        """Create new session"""
        info("New session requested", module='session')
        self.clear_session_and_restart()

    def save_configuration(self):
        """Save current configuration"""
        debug("Configuration save requested", module='main')

    def load_configuration(self):
        """Load configuration"""
        debug("Configuration load requested", module='main')

    def test_api_connection(self):
        """Test API connection"""
        try:
            available = session_manager.is_api_available()
            if available:
                info("API connection test successful", module='api')
            else:
                warning("API connection test failed", module='api')
        except Exception as e:
            error(f"API connection test error: {str(e)}", module='api')

    def show_diagnostics(self):
        """Show system diagnostics"""
        try:
            log_stats = get_stats()
            health = health_check()
            info(f"System diagnostics - Health: {health['status']}", module='main')
        except Exception as e:
            error(f"Diagnostics failed: {str(e)}", module='main')

    def show_performance_monitor(self):
        """Show performance monitoring dashboard"""
        try:
            log_stats = get_stats()
            performance_stats = log_stats.get('performance_stats', {})
            info(f"Performance monitor - Operations tracked: {len(performance_stats)}", module='main')
        except Exception as e:
            error(f"Performance monitor failed: {str(e)}", module='main')

    def show_log_viewer(self):
        """Show log viewer interface"""
        info("Log viewer requested", module='main')

    def show_shortcuts(self):
        """Show keyboard shortcuts"""
        info("Keyboard shortcuts requested", module='main')

    def reset_layout(self):
        """Reset layout to default"""
        try:
            info("Layout reset requested", module='main')
            self.calculate_sizes()
            self.resize_callback()
        except Exception as e:
            error(f"Layout reset failed: {str(e)}", module='main')

    def goto_data_sources_tab(self):
        """Navigate to data sources tab"""
        try:
            if "Data Sources" in self.tabs:
                dpg.set_value("main_tab_bar", "tab_Data Sources")
                debug("Navigated to data sources tab", module='main')
            else:
                warning("Data sources tab not available", module='main')
        except Exception as e:
            error(f"Data sources navigation failed: {str(e)}", module='main')

    def toggle_fullscreen(self, sender=None, app_data=None, user_data=None):
        """Toggle fullscreen mode"""
        try:
            dpg.toggle_viewport_fullscreen()
            debug("Fullscreen mode toggled", module='main')
        except Exception as e:
            error(f"Fullscreen toggle failed: {str(e)}", module='main')

    def apply_theme_safe(self, theme_name: str):
        """Safely apply theme"""
        try:
            success = self.theme_manager.apply_theme(theme_name)
            if success:
                info(f"Theme applied: {theme_name}", module='theme')
            else:
                warning(f"Theme application failed: {theme_name}", module='theme')
        except Exception as e:
            error(f"Theme error: {theme_name} - {str(e)}", module='theme')

    # Session management methods
    def show_session_info(self):
        """Show session information"""
        try:
            info_data = session_manager.get_session_info()
            info(f"Session info - User: {self.user_type}", module='session')
        except Exception as e:
            error(f"Session info failed: {str(e)}", module='session')

    def show_api_status(self):
        """Show API status information"""
        try:
            connectivity = session_manager.check_api_connectivity()
            info(f"API status - Connected: {connectivity}", module='api')
        except Exception as e:
            error(f"API status check failed: {str(e)}", module='api')

    def show_api_config(self):
        """Show API configuration"""
        try:
            config.validate_configuration()
            debug("API configuration requested", module='api')
        except Exception as e:
            error(f"API config validation failed: {str(e)}", module='api')

    def enable_strict_mode(self):
        """Enable strict mode"""
        try:
            config.REQUIRE_API_CONNECTION = True
            config.ALLOW_GUEST_FALLBACK = False
            info("Strict mode enabled", module='main')
        except Exception as e:
            error(f"Strict mode enable failed: {str(e)}", module='main')

    def refresh_session_data(self):
        """OPTIMIZED: Fast session refresh with caching"""
        try:
            fresh_session = session_manager.get_fresh_session()

            if fresh_session:
                self.session_data = fresh_session
                self.user_type = fresh_session.get("user_type", "guest")
                self.authenticated = fresh_session.get("authenticated", False)
                self.api_key = fresh_session.get("api_key")

                # PERFORMANCE: Clear auth cache when session changes
                self._cached_auth_headers = None
                self._auth_headers_timestamp = 0

                info(f"Session refreshed - User: {self.user_type}", module='session')
            else:
                warning("Session refresh failed", module='session')

        except Exception as e:
            error(f"Session refresh error: {str(e)}", module='session')

    def clear_session_and_restart(self):
        """Clear session and restart"""
        try:
            info("Clearing session and restarting", module='session')
            session_manager.clear_session()
            self.safe_exit()
        except Exception as e:
            error(f"Session clear failed: {str(e)}", module='session')
            self.safe_exit()

    def logout_and_restart(self):
        """Logout and restart"""
        try:
            info(f"Logging out - User: {self.user_type}", module='session')
            session_manager.clear_session()
            self.safe_exit()
        except Exception as e:
            error(f"Logout failed: {str(e)}", module='session')
            self.safe_exit()

    def save_current_session(self):
        """Save current session credentials"""
        try:
            session_manager.save_session_credentials(self.session_data)
            debug("Session credentials saved", module='session')
        except Exception as e:
            error(f"Session save failed: {str(e)}", module='session')

    # Utility methods
    def is_user_authenticated(self) -> bool:
        """Check if user is authenticated"""
        return self.authenticated

    def get_user_type(self) -> str:
        """Get current user type"""
        return self.user_type

    def get_session_data(self) -> Dict[str, Any]:
        """Get session data safely"""
        return self.session_data.copy()

    def refresh_user_profile(self) -> bool:
        """OPTIMIZED: Fast user profile refresh"""
        try:
            if self.user_type != "registered":
                debug("Profile refresh skipped - user not registered", module='session')
                return False

            fresh_session = session_manager.get_fresh_session()
            if fresh_session:
                self.session_data = fresh_session
                # Clear auth cache
                self._cached_auth_headers = None
                info("User profile refreshed", module='session')
                return True
            else:
                warning("Profile refresh failed", module='session')
                return False

        except Exception as e:
            error(f"Profile refresh error: {str(e)}", module='session')
            return False

    # Navigation methods
    def goto_database_tab(self):
        """Navigate to database tab"""
        try:
            if "Database" in self.tabs:
                dpg.set_value("main_tab_bar", "tab_Database")
                debug("Navigated to database tab", module='main')
            else:
                warning("Database tab not available", module='main')
        except Exception as e:
            error(f"Database navigation failed: {str(e)}", module='main')

    def show_profile_info(self):
        """Show profile information"""
        try:
            if "Profile" in self.tabs:
                dpg.set_value("main_tab_bar", "tab_Profile")
                debug("Navigated to profile tab", module='main')
            else:
                warning("Profile tab not available", module='main')
        except Exception as e:
            error(f"Profile navigation failed: {str(e)}", module='main')

    def show_upgrade_info(self):
        """Show upgrade information"""
        info(f"Upgrade info requested - Current: {self.user_type}", module='main')

    @monitor_performance
    def regenerate_api_key(self):
        """OPTIMIZED: Fast API key regeneration"""
        if self.user_type != "registered":
            warning(f"API key regeneration denied - User: {self.user_type}", module='api')
            return

        try:
            response = self.make_api_request("POST", "/user/regenerate-api-key")

            if response and response.status_code == 200:
                data = response.json()
                if data.get("success"):
                    new_api_key = data["data"]["api_key"]
                    old_key_type = self.get_api_key_type()

                    self.api_key = new_api_key
                    self.session_data["api_key"] = new_api_key

                    # PERFORMANCE: Clear auth cache immediately
                    self._cached_auth_headers = None
                    self._auth_headers_timestamp = 0

                    session_manager.save_session_credentials(self.session_data)

                    info(f"API key regenerated: {old_key_type} -> {self.get_api_key_type()}", module='api')
                else:
                    warning(f"API key regeneration failed: {data}", module='api')
            else:
                error(
                    f"API key regeneration request failed: {response.status_code if response else 'No response'}", module='api')

        except Exception as e:
            error(f"API key regeneration error: {str(e)}", module='api')

    # Help methods
    def show_documentation(self):
        """Show documentation"""
        info("Documentation requested", module='main')

    def show_support(self):
        """Show support information"""
        info("Support requested", module='main')

    def show_about(self):
        """Show about information"""
        info("About dialog requested", module='main')

    @monitor_performance
    def run(self):
        """OPTIMIZED: High-performance application runner"""
        try:
            info("Starting Fincept Terminal application", module='main')

            # Create DearPyGUI context
            dpg.create_context()

            # Create primary window
            dpg.add_window(tag="Primary Window", label="Fincept Terminal")
            dpg.push_container_stack("Primary Window")

            try:
                # PERFORMANCE: Initialize tabs first (parallel loading)
                self._initialize_tabs_optimized()

                # Then create UI components
                self.create_menu_bar()
                self.create_tabs()
            finally:
                dpg.pop_container_stack()

            # Enhanced terminal title
            api_key_type = self.get_api_key_type()
            strict_indicator = "[Strict]" if is_strict_mode() else "[Fallback]"
            version_info = getattr(config, 'APP_VERSION', 'v1.0.0')
            terminal_title = f"Fincept Terminal {version_info} - {self.user_type.title()} ({api_key_type}) {strict_indicator}"

            # Create viewport with optimized settings
            dpg.create_viewport(
                title=terminal_title,
                width=self.DEFAULT_WIDTH,
                height=self.DEFAULT_HEIGHT,
                min_width=self.MIN_WIDTH,
                min_height=self.MIN_HEIGHT,
                resizable=True,
                vsync=True
            )

            dpg.setup_dearpygui()
            dpg.set_primary_window("Primary Window", True)

            # PERFORMANCE: Apply theme after main UI is ready (lazy loading)
            self.apply_theme_safe("finance_terminal")

            # Set up callbacks
            dpg.set_viewport_resize_callback(self.resize_callback)

            # Save credentials
            self.save_current_session()

            info(f"Startup completed - {len(self.tabs)} tabs loaded", module='main')

            # Show viewport and start main loop
            dpg.show_viewport()
            dpg.start_dearpygui()

        except Exception as e:
            critical(f"Application startup failed: {str(e)}", module='main')
            raise
        finally:
            self.cleanup()

    def cleanup(self):
        """OPTIMIZED: Fast cleanup with minimal overhead"""
        try:
            info("Starting application cleanup", module='main')
            self.is_running = False

            # Save session quickly
            try:
                self.save_current_session()
            except Exception:
                pass  # Silent save

            # PERFORMANCE: Cleanup tabs in background
            def cleanup_tabs_background():
                if hasattr(self, 'tabs') and self.tabs:
                    cleanup_count = 0
                    for tab_name, tab in self.tabs.items():
                        if hasattr(tab, 'cleanup'):
                            try:
                                tab.cleanup()
                                cleanup_count += 1
                            except Exception:
                                pass  # Silent cleanup
                    debug(f"Background cleanup: {cleanup_count} tabs cleaned", module='main')

            # Start background cleanup
            cleanup_thread = threading.Thread(target=cleanup_tabs_background, daemon=True)
            cleanup_thread.start()

            # Cleanup theme manager
            try:
                self.theme_manager.cleanup()
            except Exception:
                pass

            # Force garbage collection in background
            def background_gc():
                try:
                    collected = gc.collect()
                    debug(f"GC collected: {collected} objects", module='main')
                except Exception:
                    pass

            gc_thread = threading.Thread(target=background_gc, daemon=True)
            gc_thread.start()

            # Cleanup DearPyGUI context
            try:
                if dpg.is_dearpygui_running():
                    dpg.stop_dearpygui()
                dpg.destroy_context()
                debug("DearPyGUI context destroyed", module='main')
            except Exception as e:
                error(f"DearPyGUI cleanup failed: {str(e)}", module='main')

            info("Application cleanup completed", module='main')

        except Exception as e:
            error(f"Critical cleanup error: {str(e)}", module='main')


# OPTIMIZED utility functions
def check_api_availability() -> bool:
    """PERFORMANCE: Fast API availability check with caching"""
    try:
        # Simple cache for API availability (30 second TTL)
        cache_key = "_api_availability_cache"
        cache_time_key = "_api_availability_time"

        if hasattr(check_api_availability, cache_key):
            cached_time = getattr(check_api_availability, cache_time_key, 0)
            if time.time() - cached_time < 30:  # 30 second cache
                return getattr(check_api_availability, cache_key)

        available = session_manager.is_api_available()

        # Cache the result
        setattr(check_api_availability, cache_key, available)
        setattr(check_api_availability, cache_time_key, time.time())

        info(f"API availability: {available}", module='main')
        return available
    except Exception as e:
        error(f"API availability check failed: {str(e)}", module='main')
        return False


def check_saved_credentials() -> Optional[Dict[str, Any]]:
    """OPTIMIZED: Fast credential check with minimal API calls"""
    try:
        # In strict mode, always check API first
        if is_strict_mode():
            if not check_api_availability():
                warning("API unavailable in strict mode", module='main')
                return None

        # Get fresh session from API
        fresh_session = session_manager.get_fresh_session()

        if fresh_session:
            user_type = fresh_session.get('user_type', 'unknown')
            authenticated = fresh_session.get('authenticated', False)
            info(f"Fresh session obtained - User: {user_type}, Auth: {authenticated}", module='main')
        else:
            warning("No fresh session available", module='main')

        return fresh_session

    except Exception as e:
        error(f"Credential check failed: {str(e)}", module='main')
        return None


@monitor_performance
def main():
    """OPTIMIZED: High-performance main entry point"""
    try:
        # PERFORMANCE: Configure logging for GUI mode with minimal overhead
        setup_for_gui()
        set_debug_mode(False)  # Disable debug logging for performance

        # Log application startup with tab-specific prefix
        info("Fincept Terminal starting up", module='main')

        # PERFORMANCE: Quick health check without detailed logging
        try:
            health = health_check()
            if health['status'] != 'healthy':
                warning(f"Logging system health: {health['status']}", module='logger')
        except Exception:
            pass  # Silent health check

        # PERFORMANCE: Fast session check
        fresh_session = check_saved_credentials()

        if fresh_session:
            session_data = fresh_session
            info("Using fresh session data", module='main')
        else:
            # Show authentication splash
            try:
                from fincept_terminal.Utils.Authentication.splash_auth import show_authentication_splash
                is_first_time = session_manager.is_first_time_user()
                info(f"Showing authentication splash - First time: {is_first_time}", module='main')
                session_data = show_authentication_splash(is_first_time_user=is_first_time)
            except ImportError as e:
                critical(f"Authentication module unavailable: {str(e)}", module='main')
                return
            except Exception as e:
                error(f"Authentication splash failed: {str(e)}", module='main')
                return

        # Validate authentication result
        if not session_data or not session_data.get("authenticated"):
            warning("Authentication failed or cancelled", module='main')
            return

        # Save credentials for next startup
        try:
            session_manager.save_session_credentials(session_data)
            debug("Session credentials saved", module='session')
        except Exception as e:
            warning(f"Session save failed: {str(e)}", module='session')

        # PERFORMANCE: Launch optimized application
        info("Authentication completed - Launching application", module='main')

        app = HighPerformanceMainApplication(session_data)
        app.run()

    except KeyboardInterrupt:
        info("Application interrupted by user (Ctrl+C)", module='main')
    except Exception as e:
        critical(f"Application failed to start: {str(e)}", module='main')
        raise
    finally:
        # Final cleanup
        try:
            info("Application shutdown initiated", module='main')
        except Exception as e:
            print(f"[CRITICAL] Final cleanup failed: {e}")


def start_terminal():
    """OPTIMIZED: Console script entry point"""
    try:
        main()
    except Exception as e:
        print(f"[CRITICAL] Terminal startup failed: {e}")
        sys.exit(1)


if __name__ == "__main__":
    start_terminal()