# -*- coding: utf-8 -*-
# menu_toolbar.py

import dearpygui.dearpygui as dpg
from datetime import datetime
from fincept_terminal.Utils.config import is_strict_mode
from fincept_terminal.Utils.Logging.logger import info, warning, error, debug


class MenuToolbarManager:
    """Manages menu bar and toolbar functionality for the main application"""

    def __init__(self, app_instance):
        """Initialize with reference to main application instance"""
        self.app = app_instance

    def create_menu_bar(self):
        """Create enhanced menu bar with tab navigation"""
        with dpg.menu_bar():
            # Your existing menus (keep them all)
            with dpg.menu(label="File"):
                dpg.add_menu_item(label="New Session", callback=self.app.new_session)
                dpg.add_menu_item(label="Save Configuration", callback=self.app.save_configuration)
                dpg.add_menu_item(label="Load Configuration", callback=self.app.load_configuration)
                dpg.add_separator()
                dpg.add_menu_item(label="Toggle Full Screen (F11)", callback=self.app.toggle_fullscreen)
                dpg.add_menu_item(label="Exit", callback=self.app.safe_exit)

            # Tab Navigation menu
            with dpg.menu(label="Tabs"):
                dpg.add_menu_item(label="<< Previous Tabs", callback=lambda: self.app.scroll_tab_view(-1))
                dpg.add_menu_item(label="Next Tabs >>", callback=lambda: self.app.scroll_tab_view(1))
                dpg.add_separator()
                dpg.add_text("Quick Jump:")
                # Add quick jump to each tab
                for tab_name in self.app.tabs.keys():
                    dpg.add_menu_item(label=tab_name,
                                      callback=lambda s, a, u, tn=tab_name: self.app.jump_to_specific_tab(tn))

            # Session menu
            with dpg.menu(label="Session"):
                self._create_session_menu()

            # API Configuration menu
            with dpg.menu(label="API"):
                dpg.add_menu_item(label="Connection Status", callback=self.app.show_api_status)
                dpg.add_menu_item(label="Configuration", callback=self.app.show_api_config)
                dpg.add_menu_item(label="Test Connection", callback=self.app.test_api_connection)
                if not is_strict_mode():
                    dpg.add_menu_item(label="Enable Strict Mode", callback=self.app.enable_strict_mode)

            # Database menu
            if 'Database' in self.app.tabs:
                with dpg.menu(label="Database"):
                    dpg.add_menu_item(label="Browse Databases", callback=self.app.goto_database_tab)
                    dpg.add_menu_item(label="Data Sources", callback=self.app.goto_data_sources_tab)

            # View menu with enhanced theme support
            with dpg.menu(label="View"):
                self._create_view_menu()

            # Tools menu
            with dpg.menu(label="Tools"):
                dpg.add_menu_item(label="System Diagnostics", callback=self.app.show_diagnostics)
                dpg.add_menu_item(label="Performance Monitor", callback=self.app.show_performance_monitor)
                dpg.add_menu_item(label="Log Viewer", callback=self.app.show_log_viewer)

            # Help menu
            with dpg.menu(label="Help"):
                dpg.add_menu_item(label="Documentation", callback=self.app.show_documentation)
                dpg.add_menu_item(label="Keyboard Shortcuts", callback=self.app.show_shortcuts)
                dpg.add_menu_item(label="Support", callback=self.app.show_support)
                dpg.add_separator()
                dpg.add_menu_item(label="About", callback=self.app.show_about)

            # Enhanced user status indicator
            self._create_status_indicator()

    def _create_session_menu(self):
        """Create session menu items based on user type"""
        if self.app.user_type == "guest":
            dpg.add_menu_item(label="Guest Mode", callback=self.app.show_profile_info)
            dpg.add_menu_item(label="Create Account", callback=self.app.show_upgrade_info)
            dpg.add_separator()
            dpg.add_menu_item(label="Change User", callback=self.app.clear_session_and_restart)
        elif self.app.user_type == "registered":
            user_info = self.app.session_data.get('user_info', {})
            username = user_info.get('username', 'User')
            dpg.add_menu_item(label=f"User: {username}", callback=self.app.show_profile_info)
            dpg.add_menu_item(label="Regenerate API Key", callback=self.app.regenerate_api_key)
            dpg.add_separator()
            dpg.add_menu_item(label="Change User", callback=self.app.clear_session_and_restart)
            dpg.add_menu_item(label="Logout", callback=self.app.logout_and_restart)

        dpg.add_separator()
        dpg.add_menu_item(label="Session Information", callback=self.app.show_session_info)
        dpg.add_menu_item(label="Refresh Session", callback=self.app.refresh_session_data)

    def _create_view_menu(self):
        """Create view menu with theme options"""
        if self.app.theme_manager.themes_available:
            with dpg.menu(label="Themes"):
                available_themes = self.app.theme_manager.get_available_themes()
                for theme in available_themes:
                    theme_label = theme.replace('_', ' ').title()
                    dpg.add_menu_item(
                        label=theme_label,
                        callback=lambda s, a, u, t=theme: self.app.apply_theme_safe(t)
                    )

        dpg.add_separator()
        dpg.add_menu_item(label="Fullscreen", callback=self.app.toggle_fullscreen)
        dpg.add_menu_item(label="Reset Layout", callback=self.app.reset_layout)

    def _create_status_indicator(self):
        """Create enhanced status indicator"""
        with dpg.group(horizontal=True):
            dpg.add_spacer(width=20)

            # User status
            if self.app.user_type == "guest":
                expires_at = self.app.session_data.get("expires_at")
                if expires_at:
                    try:
                        expiry = datetime.fromisoformat(expires_at.replace('Z', '+00:00'))
                        hours_left = max(0, int((expiry.replace(tzinfo=None) - datetime.now()).total_seconds() / 3600))
                        auth_text = f"Guest ({hours_left}h remaining)"
                    except Exception:
                        auth_text = "Guest (Temp Access)"
                else:
                    auth_text = "Guest Mode"
                dpg.add_text(auth_text, color=[255, 255, 100])
            elif self.app.user_type == "registered":
                user_info = self.app.session_data.get('user_info', {})
                username = user_info.get('username', 'User')
                credit_balance = user_info.get('credit_balance', 0)
                auth_text = f"{username} ({credit_balance} credits)"
                dpg.add_text(auth_text, color=[100, 255, 100])

            # API status indicator
            dpg.add_spacer(width=10)
            api_type = self.app.get_api_key_type()
            if api_type == "Offline":
                dpg.add_text("●", color=[255, 100, 100])  # Red for offline
            elif api_type == "Guest":
                dpg.add_text("●", color=[255, 255, 100])  # Yellow for guest
            else:
                dpg.add_text("●", color=[100, 255, 100])  # Green for authenticated