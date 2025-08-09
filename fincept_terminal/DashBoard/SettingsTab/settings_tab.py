# -*- coding: utf-8 -*-
# settings_tab.py (Fixed - No Async Issues)

import dearpygui.dearpygui as dpg
import json
import os
import platform
from pathlib import Path
from typing import Dict, Any, Optional, List
from datetime import datetime
import threading
import time

from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import info, debug, warning, error, operation, monitor_performance


class SettingsManager:
    """Settings Manager for API keys and provider configurations"""

    def __init__(self):
        self.config_dir = self._get_config_directory()
        self.config_dir.mkdir(parents=True, exist_ok=True)
        self.settings_file = self.config_dir / "provider_settings.json"
        self.load_settings()

    def _get_config_directory(self) -> Path:
        """Get config directory - uses .fincept folder"""
        config_dir = Path.home() / '.fincept' / 'settings'
        return config_dir

    def load_settings(self) -> Dict[str, Any]:
        """Load settings from file"""
        try:
            if self.settings_file.exists():
                with open(self.settings_file, 'r', encoding='utf-8') as f:
                    self.settings = json.load(f)
                info(f"Settings loaded from: {self.settings_file}", module="SettingsManager")
            else:
                self.settings = self._get_default_settings()
                self.save_settings()
                info(f"Default settings created at: {self.settings_file}", module="SettingsManager")
        except Exception as e:
            error(f"Error loading settings: {str(e)}", module="SettingsManager")
            self.settings = self._get_default_settings()

        return self.settings

    def _get_default_settings(self) -> Dict[str, Any]:
        """Get default settings structure"""
        return {
            "providers": {
                "alpha_vantage": {
                    "api_key": "",
                    "rate_limit": 5,
                    "enabled": False,
                    "last_verified": None
                },
                "yahoo_finance": {
                    "enabled": True,
                    "extended_hours": False,
                    "include_events": True,
                    "last_verified": None
                },
                "fincept_api": {
                    "api_key": "",
                    "endpoint": "https://api.fincept.com",
                    "real_time": True,
                    "enabled": False,
                    "last_verified": None
                }
            },
            "preferences": {
                "default_provider": "yfinance",
                "cache_enabled": True,
                "cache_duration": 300,
                "auto_verify": True
            },
            "created_at": datetime.now().isoformat(),
            "last_modified": datetime.now().isoformat()
        }

    def save_settings(self) -> bool:
        """Save settings to file"""
        try:
            self.config_dir.mkdir(parents=True, exist_ok=True)
            self.settings["last_modified"] = datetime.now().isoformat()
            with open(self.settings_file, 'w', encoding='utf-8') as f:
                json.dump(self.settings, f, indent=2)
            info(f"Settings saved to: {self.settings_file}", module="SettingsManager")
            return True
        except Exception as e:
            error(f"Error saving settings: {str(e)}", module="SettingsManager")
            return False

    def get_provider_config(self, provider_name: str) -> Dict[str, Any]:
        """Get configuration for a specific provider"""
        return self.settings.get("providers", {}).get(provider_name, {})

    def update_provider_config(self, provider_name: str, config: Dict[str, Any]) -> bool:
        """Update configuration for a specific provider"""
        try:
            if "providers" not in self.settings:
                self.settings["providers"] = {}

            if provider_name not in self.settings["providers"]:
                self.settings["providers"][provider_name] = {}

            self.settings["providers"][provider_name].update(config)
            return self.save_settings()
        except Exception as e:
            error(f"Error updating provider config: {str(e)}", module="SettingsManager")
            return False

    def get_api_key(self, provider_name: str) -> str:
        """Get API key for a provider"""
        return self.get_provider_config(provider_name).get("api_key", "")

    def set_api_key(self, provider_name: str, api_key: str) -> bool:
        """Set API key for a provider"""
        return self.update_provider_config(provider_name, {"api_key": api_key})

    def is_provider_enabled(self, provider_name: str) -> bool:
        """Check if a provider is enabled"""
        return self.get_provider_config(provider_name).get("enabled", False)

    def enable_provider(self, provider_name: str, enabled: bool = True) -> bool:
        """Enable or disable a provider"""
        return self.update_provider_config(provider_name, {"enabled": enabled})


class SettingsTab(BaseTab):
    """Settings tab for managing data provider configurations"""

    def __init__(self, app):
        super().__init__(app)
        self.settings_manager = SettingsManager()
        self.verification_results = {}
        self.providers = {
            "alpha_vantage": {
                "name": "Alpha Vantage",
                "description": "Premium financial data provider",
                "requires_api_key": True,
                "supports": ["stocks", "forex", "crypto", "economic"]
            },
            "yahoo_finance": {
                "name": "Yahoo Finance",
                "description": "Free financial data (rate limited)",
                "requires_api_key": False,
                "supports": ["stocks", "indices", "forex"]
            },
            "fincept_api": {
                "name": "Fincept Premium",
                "description": "Fincept's premium data service",
                "requires_api_key": True,
                "supports": ["stocks", "forex", "crypto", "news", "economic"]
            }
        }

        info(f"Settings will be stored at: {self.settings_manager.settings_file}", module="SettingsTab")

    def get_label(self):
        return "⚙️ Settings"

    def create_content(self):
        """Create the settings interface"""
        self.add_section_header("📋 Data Provider Settings")
        dpg.add_text("Configure API keys and settings for data providers", color=[200, 200, 200])
        dpg.add_text(f"Settings stored in: {self.settings_manager.config_dir}", color=[150, 150, 150])
        dpg.add_spacer(height=20)

        # Provider Configuration Section
        self.create_provider_configurations()

        dpg.add_spacer(height=20)

        # Global Preferences Section
        self.create_global_preferences()

        dpg.add_spacer(height=20)

        # Actions Section
        self.create_actions_section()

    def create_provider_configurations(self):
        """Create provider configuration UI"""
        dpg.add_text("🔑 Provider Configurations", color=[255, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        for provider_id, provider_info in self.providers.items():
            self.create_provider_section(provider_id, provider_info)
            dpg.add_spacer(height=15)

    def create_provider_section(self, provider_id: str, provider_info: Dict[str, Any]):
        """Create configuration section for a specific provider"""
        config = self.settings_manager.get_provider_config(provider_id)

        # Provider header with unique tags
        checkbox_tag = f"settings_enable_{provider_id}"
        with dpg.group(horizontal=True):
            enabled = config.get("enabled", False)
            dpg.add_checkbox(
                label=provider_info["name"],
                tag=checkbox_tag,
                default_value=enabled,
                callback=lambda s, v, u: self.on_provider_enabled(u, v),
                user_data=provider_id
            )

            dpg.add_text(f"- {provider_info['description']}", color=[150, 150, 150])

        # Provider details
        with dpg.group(indent=20):
            dpg.add_text(f"Supports: {', '.join(provider_info['supports'])}", color=[200, 200, 200])

            # API Key input (if required) with unique tags
            if provider_info["requires_api_key"]:
                dpg.add_spacer(height=5)
                dpg.add_text("API Key:")

                api_key_tag = f"settings_api_key_{provider_id}"
                save_btn_tag = f"settings_save_{provider_id}"
                verify_btn_tag = f"settings_verify_{provider_id}"

                with dpg.group(horizontal=True):
                    dpg.add_input_text(
                        tag=api_key_tag,
                        default_value=config.get("api_key", ""),
                        width=300,
                        password=True,
                        hint="Enter your API key"
                    )
                    dpg.add_button(
                        label="💾 Save",
                        tag=save_btn_tag,
                        callback=lambda s, a, u: self.save_api_key(u),
                        user_data=provider_id
                    )
                    dpg.add_button(
                        label="🔍 Verify",
                        tag=verify_btn_tag,
                        callback=lambda s, a, u: self.verify_api_key(u),
                        user_data=provider_id
                    )

            # Provider-specific settings
            if provider_id == "alpha_vantage":
                self.create_alpha_vantage_settings(config, provider_id)
            elif provider_id == "yahoo_finance":
                self.create_yahoo_finance_settings(config, provider_id)
            elif provider_id == "fincept_api":
                self.create_fincept_settings(config, provider_id)

            # Verification status
            self.create_verification_status(provider_id, config)

    def create_alpha_vantage_settings(self, config: Dict[str, Any], provider_id: str):
        """Create Alpha Vantage specific settings"""
        dpg.add_spacer(height=5)
        dpg.add_text("Rate Limit (requests/minute):")

        rate_limit_tag = f"settings_{provider_id}_rate_limit"
        dpg.add_input_int(
            tag=rate_limit_tag,
            default_value=config.get("rate_limit", 5),
            width=100,
            min_value=1,
            max_value=75
        )

    def create_yahoo_finance_settings(self, config: Dict[str, Any], provider_id: str):
        """Create Yahoo Finance specific settings"""
        dpg.add_spacer(height=5)

        extended_hours_tag = f"settings_{provider_id}_extended_hours"
        include_events_tag = f"settings_{provider_id}_include_events"

        dpg.add_checkbox(
            label="Include extended hours data",
            tag=extended_hours_tag,
            default_value=config.get("extended_hours", False)
        )
        dpg.add_checkbox(
            label="Include dividends and splits",
            tag=include_events_tag,
            default_value=config.get("include_events", True)
        )

    def create_fincept_settings(self, config: Dict[str, Any], provider_id: str):
        """Create Fincept API specific settings"""
        dpg.add_spacer(height=5)

        endpoint_tag = f"settings_{provider_id}_endpoint"
        real_time_tag = f"settings_{provider_id}_real_time"

        dpg.add_text("API Endpoint:")
        dpg.add_input_text(
            tag=endpoint_tag,
            default_value=config.get("endpoint", "https://api.fincept.com"),
            width=300
        )
        dpg.add_checkbox(
            label="Enable real-time data",
            tag=real_time_tag,
            default_value=config.get("real_time", True)
        )

    def create_verification_status(self, provider_id: str, config: Dict[str, Any]):
        """Create verification status display"""
        dpg.add_spacer(height=5)

        last_verified = config.get("last_verified")
        if last_verified:
            dpg.add_text(f"Last verified: {last_verified}", color=[100, 255, 100])

        # Show verification result if available
        if provider_id in self.verification_results:
            result = self.verification_results[provider_id]
            if result.get("valid"):
                dpg.add_text("✅ API key verified", color=[100, 255, 100])
            else:
                error_msg = result.get("error", "Unknown error")
                dpg.add_text(f"❌ {error_msg}", color=[255, 100, 100])

    def create_global_preferences(self):
        """Create global preferences section"""
        dpg.add_text("🌐 Global Preferences", color=[255, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        preferences = self.settings_manager.settings.get("preferences", {})

        # Default provider - use enabled providers
        dpg.add_text("Default Data Provider:")
        enabled_providers = self.get_enabled_providers()

        if not enabled_providers:
            enabled_providers = ["yfinance"]

        default_provider_tag = "settings_default_provider"
        current_default = preferences.get("default_provider", enabled_providers[0])

        # Ensure current default is in enabled list
        if current_default not in enabled_providers:
            current_default = enabled_providers[0]

        dpg.add_combo(
            enabled_providers,
            tag=default_provider_tag,
            default_value=current_default,
            width=200
        )

        dpg.add_spacer(height=10)

        # Cache settings
        cache_enabled_tag = "settings_cache_enabled"
        cache_duration_tag = "settings_cache_duration"
        auto_verify_tag = "settings_auto_verify"

        dpg.add_checkbox(
            label="Enable data caching",
            tag=cache_enabled_tag,
            default_value=preferences.get("cache_enabled", True)
        )

        dpg.add_text("Cache duration (seconds):")
        dpg.add_input_int(
            tag=cache_duration_tag,
            default_value=preferences.get("cache_duration", 300),
            width=100,
            min_value=60,
            max_value=3600
        )

        dpg.add_checkbox(
            label="Auto-verify API keys on startup",
            tag=auto_verify_tag,
            default_value=preferences.get("auto_verify", True)
        )

        dpg.add_spacer(height=10)

        # Additional preferences
        dpg.add_text("Refresh Settings:")

        auto_refresh_tag = "settings_auto_refresh_enabled"
        refresh_interval_tag = "settings_refresh_interval"

        dpg.add_checkbox(
            label="Enable auto-refresh for data viewer",
            tag=auto_refresh_tag,
            default_value=preferences.get("auto_refresh_enabled", False)
        )

        dpg.add_text("Default refresh interval (seconds):")
        dpg.add_input_int(
            tag=refresh_interval_tag,
            default_value=preferences.get("refresh_interval", 30),
            width=100,
            min_value=5,
            max_value=300
        )

        dpg.add_spacer(height=10)

        # Data format preferences
        dpg.add_text("Display Preferences:")

        currency_format_tag = "settings_currency_format"
        date_format_tag = "settings_date_format"
        decimal_places_tag = "settings_decimal_places"

        dpg.add_text("Currency format:")
        dpg.add_combo(
            ["USD ($)", "EUR (€)", "GBP (£)", "JPY (¥)", "Generic"],
            tag=currency_format_tag,
            default_value=preferences.get("currency_format", "USD ($)"),
            width=150
        )

        dpg.add_text("Date format:")
        dpg.add_combo(
            ["YYYY-MM-DD", "DD/MM/YYYY", "MM/DD/YYYY", "DD-MMM-YYYY"],
            tag=date_format_tag,
            default_value=preferences.get("date_format", "YYYY-MM-DD"),
            width=150
        )

        dpg.add_text("Decimal places for prices:")
        dpg.add_input_int(
            tag=decimal_places_tag,
            default_value=preferences.get("decimal_places", 2),
            width=100,
            min_value=0,
            max_value=6
        )

    def get_enabled_providers(self) -> List[str]:
        """Get list of enabled providers"""
        try:
            enabled = []
            for provider_id in self.providers.keys():
                if self.settings_manager.is_provider_enabled(provider_id):
                    enabled.append(provider_id)
            return enabled if enabled else ["yfinance"]
        except Exception as e:
            error(f"Error getting enabled providers: {str(e)}", module="SettingsTab")
            return ["yfinance"]  # fallback

    def create_actions_section(self):
        """Create actions section with buttons"""
        dpg.add_text("💾 Actions", color=[255, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        # Action buttons with unique tags
        save_all_tag = "settings_save_all_btn"
        reset_tag = "settings_reset_btn"
        verify_all_tag = "settings_verify_all_btn"
        export_tag = "settings_export_btn"

        with dpg.group(horizontal=True):
            dpg.add_button(
                label="💾 Save All Settings",
                tag=save_all_tag,
                callback=self.save_all_settings
            )
            dpg.add_button(
                label="🔄 Reset to Defaults",
                tag=reset_tag,
                callback=self.reset_to_defaults
            )
            dpg.add_button(
                label="🧪 Verify All Keys",
                tag=verify_all_tag,
                callback=self.verify_all_keys
            )
            dpg.add_button(
                label="📤 Export Settings",
                tag=export_tag,
                callback=self.export_settings
            )

        # Show current file location
        dpg.add_spacer(height=10)
        dpg.add_text(f"📁 Settings file: {self.settings_manager.settings_file}", color=[150, 150, 150])

    # Callback methods
    def on_provider_enabled(self, provider_id: str, enabled: bool):
        """Handle provider enable/disable"""
        self.settings_manager.enable_provider(provider_id, enabled)
        info(f"Provider {provider_id} {'enabled' if enabled else 'disabled'}", module="SettingsTab")

    def save_api_key(self, provider_id: str):
        """Save API key for a provider"""
        try:
            api_key_tag = f"settings_api_key_{provider_id}"
            if dpg.does_item_exist(api_key_tag):
                api_key = dpg.get_value(api_key_tag)
                if api_key.strip():
                    success = self.settings_manager.set_api_key(provider_id, api_key.strip())
                    if success:
                        info(f"API key saved for {provider_id}", module="SettingsTab")
                        # Enable provider automatically when API key is saved
                        self.settings_manager.enable_provider(provider_id, True)
                        # Update checkbox
                        checkbox_tag = f"settings_enable_{provider_id}"
                        if dpg.does_item_exist(checkbox_tag):
                            dpg.set_value(checkbox_tag, True)
                        # Auto-verify after saving
                        self.verify_api_key(provider_id)
                    else:
                        error(f"Failed to save API key for {provider_id}", module="SettingsTab")
                else:
                    warning(f"Empty API key for {provider_id}", module="SettingsTab")
        except Exception as e:
            error(f"Error saving API key: {str(e)}", module="SettingsTab")

    def verify_api_key(self, provider_id: str):
        """Verify API key for a provider (no async)"""
        try:
            if provider_id == "alpha_vantage":
                # Use threading for verification instead of async
                thread = threading.Thread(target=self._verify_alpha_vantage_key_sync)
                thread.daemon = True
                thread.start()
            else:
                info(f"Verification not implemented for {provider_id}", module="SettingsTab")
        except Exception as e:
            error(f"Error verifying API key: {str(e)}", module="SettingsTab")

    def _verify_alpha_vantage_key_sync(self):
        """Verify Alpha Vantage API key synchronously"""
        try:
            api_key = self.settings_manager.get_api_key("alpha_vantage")
            if not api_key:
                self.verification_results["alpha_vantage"] = {"valid": False, "error": "No API key provided"}
                return

            # Simple HTTP request for verification
            import requests
            url = "https://www.alphavantage.co/query"
            params = {
                "function": "TIME_SERIES_DAILY",
                "symbol": "AAPL",
                "apikey": api_key,
                "outputsize": "compact"
            }

            response = requests.get(url, params=params, timeout=10)

            if response.status_code == 200:
                data = response.json()

                if "Error Message" in data:
                    result = {"valid": False, "error": data["Error Message"]}
                elif "Note" in data:
                    result = {"valid": False, "error": "API rate limit exceeded"}
                elif "Information" in data:
                    result = {"valid": False, "error": data["Information"]}
                elif "Time Series (Daily)" in data:
                    result = {"valid": True, "message": "API key verified successfully"}
                    # Update last verified timestamp
                    self.settings_manager.update_provider_config("alpha_vantage", {
                        "last_verified": datetime.now().isoformat()
                    })
                else:
                    result = {"valid": False, "error": "Unexpected API response"}
            else:
                result = {"valid": False, "error": f"HTTP {response.status_code}"}

            self.verification_results["alpha_vantage"] = result

            if result["valid"]:
                info("Alpha Vantage API key verified successfully", module="SettingsTab")
            else:
                error(f"Alpha Vantage verification failed: {result['error']}", module="SettingsTab")

        except requests.Timeout:
            self.verification_results["alpha_vantage"] = {"valid": False, "error": "API request timeout"}
        except Exception as e:
            error(f"Alpha Vantage verification error: {str(e)}", module="SettingsTab")
            self.verification_results["alpha_vantage"] = {"valid": False, "error": str(e)}

    def save_all_settings(self):
        """Save all current settings"""
        try:
            # Update preferences
            preferences = {}

            # Core preferences
            if dpg.does_item_exist("settings_default_provider"):
                preferences["default_provider"] = dpg.get_value("settings_default_provider")
            if dpg.does_item_exist("settings_cache_enabled"):
                preferences["cache_enabled"] = dpg.get_value("settings_cache_enabled")
            if dpg.does_item_exist("settings_cache_duration"):
                preferences["cache_duration"] = dpg.get_value("settings_cache_duration")
            if dpg.does_item_exist("settings_auto_verify"):
                preferences["auto_verify"] = dpg.get_value("settings_auto_verify")

            # Refresh settings
            if dpg.does_item_exist("settings_auto_refresh_enabled"):
                preferences["auto_refresh_enabled"] = dpg.get_value("settings_auto_refresh_enabled")
            if dpg.does_item_exist("settings_refresh_interval"):
                preferences["refresh_interval"] = dpg.get_value("settings_refresh_interval")

            # Display preferences
            if dpg.does_item_exist("settings_currency_format"):
                preferences["currency_format"] = dpg.get_value("settings_currency_format")
            if dpg.does_item_exist("settings_date_format"):
                preferences["date_format"] = dpg.get_value("settings_date_format")
            if dpg.does_item_exist("settings_decimal_places"):
                preferences["decimal_places"] = dpg.get_value("settings_decimal_places")

            # Update the settings manager
            self.settings_manager.settings["preferences"].update(preferences)

            # Update provider-specific settings
            for provider_id in self.providers.keys():
                config_updates = {}

                # Alpha Vantage specific
                rate_limit_tag = f"settings_{provider_id}_rate_limit"
                if provider_id == "alpha_vantage" and dpg.does_item_exist(rate_limit_tag):
                    config_updates["rate_limit"] = dpg.get_value(rate_limit_tag)

                # Yahoo Finance specific
                elif provider_id == "yahoo_finance":
                    extended_hours_tag = f"settings_{provider_id}_extended_hours"
                    include_events_tag = f"settings_{provider_id}_include_events"
                    if dpg.does_item_exist(extended_hours_tag):
                        config_updates["extended_hours"] = dpg.get_value(extended_hours_tag)
                    if dpg.does_item_exist(include_events_tag):
                        config_updates["include_events"] = dpg.get_value(include_events_tag)

                # Fincept specific
                elif provider_id == "fincept_api":
                    endpoint_tag = f"settings_{provider_id}_endpoint"
                    real_time_tag = f"settings_{provider_id}_real_time"
                    if dpg.does_item_exist(endpoint_tag):
                        config_updates["endpoint"] = dpg.get_value(endpoint_tag)
                    if dpg.does_item_exist(real_time_tag):
                        config_updates["real_time"] = dpg.get_value(real_time_tag)

                if config_updates:
                    self.settings_manager.update_provider_config(provider_id, config_updates)

            # Save to file
            if self.settings_manager.save_settings():
                info("All settings saved successfully", module="SettingsTab")
            else:
                error("Failed to save settings", module="SettingsTab")

        except Exception as e:
            error(f"Error saving settings: {str(e)}", module="SettingsTab")

    def reset_to_defaults(self):
        """Reset all settings to defaults"""
        try:
            self.settings_manager.settings = self.settings_manager._get_default_settings()
            self.settings_manager.save_settings()
            info("Settings reset to defaults", module="SettingsTab")
        except Exception as e:
            error(f"Error resetting settings: {str(e)}", module="SettingsTab")

    def verify_all_keys(self):
        """Verify all configured API keys"""
        try:
            info("Starting verification of all API keys", module="SettingsTab")

            for provider_id in self.providers.keys():
                if (self.settings_manager.is_provider_enabled(provider_id) and
                        self.providers[provider_id]["requires_api_key"]):
                    self.verify_api_key(provider_id)

        except Exception as e:
            error(f"Error verifying all keys: {str(e)}", module="SettingsTab")

    def export_settings(self):
        """Export settings to file"""
        try:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            export_path = self.settings_manager.config_dir / f"settings_export_{timestamp}.json"

            with open(export_path, 'w', encoding='utf-8') as f:
                json.dump(self.settings_manager.settings, f, indent=2)

            info(f"Settings exported to: {export_path}", module="SettingsTab")

        except Exception as e:
            error(f"Error exporting settings: {str(e)}", module="SettingsTab")

    def get_provider_credentials(self, provider_name: str) -> Dict[str, str]:
        """Get credentials for a provider (for use by data source manager)"""
        config = self.settings_manager.get_provider_config(provider_name)

        if provider_name == "alpha_vantage":
            return {"alpha_vantage_api_key": config.get("api_key", "")}
        elif provider_name == "fincept_api":
            return {"fincept_api_key": config.get("api_key", "")}
        else:
            return {}

    def cleanup(self):
        """Clean up resources"""
        self.verification_results.clear()
        info("Settings tab cleanup completed", module="SettingsTab")