# -*- coding: utf-8 -*-
# data_config_tab.py

import dearpygui.dearpygui as dpg
import json
import os
from pathlib import Path
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.DatabaseConnector.DataSources.data_source_manager import get_data_source_manager


class DataConfigurationTab(BaseTab):
    """
    Universal Data Configuration Tab
    The MASTER CONTROL CENTER for all data sources in the terminal
    """

    def __init__(self, app):
        super().__init__(app)
        self.data_manager = get_data_source_manager(app)
        if not self.data_manager:
            # Initialize if not already done
            from fincept_terminal.DatabaseConnector.DataSources.data_source_manager import DataSourceManager
            self.data_manager = DataSourceManager(app)

        self.selected_source = None
        self.test_results = {}
        self.csv_column_mapping = {}

    def get_label(self):
        return "Data Sources"

    def create_content(self):
        """Create the data configuration interface"""
        # Add header
        dpg.add_text("Universal Data Source Configuration", color=[100, 255, 100])
        dpg.add_text("Configure data sources for the entire terminal. All tabs will use your settings.",
                     color=[200, 200, 200])
        dpg.add_spacer(height=20)

        # Create horizontal layout
        dpg.add_text("📊 Current Data Source Mappings", color=[255, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        # Get data for mappings
        current_mappings = self.data_manager.get_current_mappings()
        available_sources = self.data_manager.get_available_sources()

        # Create mapping controls for each data type
        data_types = ["stocks", "forex", "crypto", "news", "economic", "portfolio", "options", "indices"]

        for data_type in data_types:
            # Create horizontal group for each data type
            dpg.add_text(f"{data_type.capitalize()} Source:", color=[255, 255, 255])

            # Get sources that support this data type
            supported_sources = []
            for source_name, source_info in available_sources.items():
                if data_type in source_info["supports"]:
                    supported_sources.append(source_name)

            if supported_sources:
                current_source = current_mappings.get(data_type, "fincept_api")
                dpg.add_combo(
                    supported_sources,
                    default_value=current_source if current_source in supported_sources else supported_sources[0],
                    width=300,
                    tag=f"combo_{data_type}",
                    callback=lambda sender, app_data, user_data: self.on_source_changed(user_data, app_data),
                    user_data=data_type
                )

                dpg.add_button(
                    label=f"Test {data_type}",
                    tag=f"test_{data_type}",
                    callback=lambda sender, app_data, user_data: self.test_data_type(user_data),
                    user_data=data_type
                )
            else:
                dpg.add_text("No sources available", color=[255, 100, 100])

            dpg.add_spacer(height=10)

        dpg.add_spacer(height=20)

        # Control buttons
        dpg.add_button(label="💾 Save Configuration", callback=self.save_configuration)
        dpg.add_button(label="🔄 Reset to Defaults", callback=self.reset_to_defaults)
        dpg.add_button(label="🧪 Test All Sources", callback=self.test_all_sources)

        dpg.add_spacer(height=20)

        # Source configuration section
        dpg.add_text("⚙️ Data Source Configuration", color=[255, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        # Source selection dropdown
        available_source_names = list(available_sources.keys())
        dpg.add_text("Select Source to Configure:")
        dpg.add_combo(
            available_source_names,
            default_value=available_source_names[0] if available_source_names else "",
            width=300,
            tag="source_selector",
            callback=self.on_source_selected
        )

        dpg.add_spacer(height=15)

        # Configuration area
        dpg.add_group(tag="config_area")

        # Create initial config UI
        if available_source_names:
            self.create_source_config_ui(available_source_names[0])

        dpg.add_spacer(height=20)

        # Cache management
        dpg.add_text("Cache Management:", color=[255, 255, 100])
        dpg.add_button(label="Clear All Cache", callback=self.clear_all_cache)
        dpg.add_button(label="Show Cache Stats", callback=self.show_cache_stats)

    def create_source_config_ui(self, source_name: str):
        """Create configuration UI for selected source"""
        # Clear existing config UI
        if dpg.does_item_exist("config_content"):
            dpg.delete_item("config_content")

        # Create new config content
        dpg.add_group(tag="config_content", parent="config_area")

        if not source_name:
            dpg.add_text("No source selected", color=[255, 100, 100], parent="config_content")
            return

        available_sources = self.data_manager.get_available_sources()
        source_info = available_sources.get(source_name, {})

        # Source information
        dpg.add_text(f"📋 {source_info.get('name', source_name)}", color=[255, 255, 100], parent="config_content")
        dpg.add_text(f"Type: {source_info.get('type', 'unknown')}", color=[200, 200, 200], parent="config_content")
        dpg.add_text(f"Supports: {', '.join(source_info.get('supports', []))}", color=[200, 200, 200],
                     parent="config_content")
        dpg.add_text(f"Requires Auth: {'Yes' if source_info.get('requires_auth') else 'No'}", color=[200, 200, 200],
                     parent="config_content")
        dpg.add_text(f"Real-time: {'Yes' if source_info.get('real_time') else 'No'}", color=[200, 200, 200],
                     parent="config_content")

        dpg.add_spacer(height=15, parent="config_content")

        # Configuration based on source type
        if source_name == "yfinance":
            self.create_yfinance_config()
        elif source_name == "fincept_api":
            self.create_fincept_config()
        elif source_name == "alpha_vantage":
            self.create_alpha_vantage_config()
        elif source_name == "csv_import":
            self.create_csv_import_config()
        elif source_name == "websocket_feed":
            self.create_websocket_config()
        else:
            dpg.add_text("No additional configuration required", color=[100, 255, 100], parent="config_content")

        dpg.add_spacer(height=15, parent="config_content")

        # Test this source
        dpg.add_button(
            label=f"🧪 Test {source_name}",
            callback=lambda: self.test_specific_source(source_name),
            parent="config_content"
        )

        # Show test result
        test_result = self.test_results.get(source_name, {})
        if test_result:
            color = [100, 255, 100] if test_result.get("success") else [255, 100, 100]
            status = "✅ Working" if test_result.get("success") else "❌ Failed"
            dpg.add_text(status, color=color, parent="config_content")

        # Show test details if available
        if source_name in self.test_results:
            result = self.test_results[source_name]
            dpg.add_spacer(height=10, parent="config_content")
            dpg.add_text("Test Result:", color=[255, 255, 100], parent="config_content")
            dpg.add_text(f"Message: {result.get('message', 'No message')}", color=[200, 200, 200],
                         parent="config_content")
            dpg.add_text(f"Response Time: {result.get('response_time', 'Unknown')}", color=[200, 200, 200],
                         parent="config_content")

    def create_yfinance_config(self):
        """Create Yahoo Finance configuration"""
        dpg.add_text("📈 Yahoo Finance Configuration", color=[255, 255, 100], parent="config_content")
        dpg.add_text("✅ No API key required", color=[100, 255, 100], parent="config_content")
        dpg.add_text("✅ Free access to stocks, forex, indices", color=[100, 255, 100], parent="config_content")
        dpg.add_text("⚠️ Rate limited (2000 requests/hour)", color=[255, 255, 100], parent="config_content")

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_checkbox(label="Enable extended hours data", tag="yfinance_extended_hours", parent="config_content")
        dpg.add_checkbox(label="Include dividends and splits", tag="yfinance_include_events", parent="config_content")

    def create_fincept_config(self):
        """Create Fincept API configuration"""
        dpg.add_text("🏦 Fincept Premium API Configuration", color=[255, 255, 100], parent="config_content")

        # Get current config
        current_config = self.data_manager.get_source_config("fincept_api")

        dpg.add_text("API Endpoint:", parent="config_content")
        dpg.add_input_text(
            tag="fincept_endpoint",
            default_value=current_config.get("endpoint", "https://api.fincept.com"),
            width=300,
            parent="config_content"
        )

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_text("API Key:", parent="config_content")
        dpg.add_input_text(
            tag="fincept_api_key",
            default_value=current_config.get("api_key", ""),
            width=300,
            password=True,
            parent="config_content"
        )

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_checkbox(
            label="Enable real-time data",
            tag="fincept_realtime",
            default_value=current_config.get("realtime", True),
            parent="config_content"
        )

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_button(label="💾 Save Fincept Config", callback=self.save_fincept_config, parent="config_content")
        dpg.add_button(label="🔑 Test API Key", callback=self.test_fincept_api_key, parent="config_content")

    def create_alpha_vantage_config(self):
        """Create Alpha Vantage configuration"""
        dpg.add_text("📊 Alpha Vantage Configuration", color=[255, 255, 100], parent="config_content")

        current_config = self.data_manager.get_source_config("alpha_vantage")

        dpg.add_text("API Key:", parent="config_content")
        dpg.add_input_text(
            tag="alpha_vantage_api_key",
            default_value=current_config.get("api_key", ""),
            width=300,
            password=True,
            parent="config_content"
        )

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_text("Rate Limit (requests/minute):", parent="config_content")
        dpg.add_input_int(
            tag="alpha_vantage_rate_limit",
            default_value=current_config.get("rate_limit", 5),
            width=100,
            parent="config_content"
        )

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_button(label="💾 Save Alpha Vantage Config", callback=self.save_alpha_vantage_config,
                       parent="config_content")
        dpg.add_button(label="🔑 Test API Key", callback=self.test_alpha_vantage_api_key, parent="config_content")

    def create_csv_import_config(self):
        """Create CSV import configuration"""
        dpg.add_text("📁 CSV Import Configuration", color=[255, 255, 100], parent="config_content")

        dpg.add_text("Select CSV file:", parent="config_content")
        dpg.add_input_text(tag="csv_file_path", width=250, readonly=True, parent="config_content")
        dpg.add_button(label="Browse", callback=self.browse_csv_file, parent="config_content")

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_text("Data Type:", parent="config_content")
        dpg.add_combo(
            ["stocks", "forex", "crypto", "portfolio", "custom"],
            tag="csv_data_type",
            width=200,
            parent="config_content"
        )

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_text("Column Mapping:", color=[255, 255, 100], parent="config_content")
        dpg.add_text("Date Column:", parent="config_content")
        dpg.add_input_text(tag="csv_date_column", hint="e.g., Date", width=200, parent="config_content")
        dpg.add_text("Price/Close Column:", parent="config_content")
        dpg.add_input_text(tag="csv_price_column", hint="e.g., Close", width=200, parent="config_content")
        dpg.add_text("Volume Column (optional):", parent="config_content")
        dpg.add_input_text(tag="csv_volume_column", hint="e.g., Volume", width=200, parent="config_content")

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_button(label="📥 Import CSV", callback=self.import_csv_data, parent="config_content")
        dpg.add_button(label="👁️ Preview", callback=self.preview_csv_data, parent="config_content")

    def create_websocket_config(self):
        """Create WebSocket configuration"""
        dpg.add_text("🔌 WebSocket Feed Configuration", color=[255, 255, 100], parent="config_content")

        current_config = self.data_manager.get_source_config("websocket_feed")

        dpg.add_text("WebSocket URL:", parent="config_content")
        dpg.add_input_text(
            tag="websocket_url",
            default_value=current_config.get("url", "wss://api.example.com/ws"),
            width=300,
            parent="config_content"
        )

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_text("Authentication Token:", parent="config_content")
        dpg.add_input_text(
            tag="websocket_token",
            default_value=current_config.get("token", ""),
            width=300,
            password=True,
            parent="config_content"
        )

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_checkbox(
            label="Auto-reconnect on disconnect",
            tag="websocket_auto_reconnect",
            default_value=current_config.get("auto_reconnect", True),
            parent="config_content"
        )

        dpg.add_spacer(height=10, parent="config_content")
        dpg.add_button(label="💾 Save WebSocket Config", callback=self.save_websocket_config, parent="config_content")
        dpg.add_button(label="🔌 Test Connection", callback=self.test_websocket_connection, parent="config_content")

    # Callback methods
    def on_source_changed(self, data_type: str, source_name: str):
        """Handle data type source change"""
        try:
            self.data_manager.set_data_source(data_type, source_name)
            print(f"✅ Changed {data_type} source to {source_name}")
        except Exception as e:
            print(f"❌ Error changing {data_type} source: {e}")

    def on_source_selected(self, sender, app_data):
        """Handle source selection for configuration"""
        self.selected_source = app_data
        self.create_source_config_ui(app_data)

    def test_data_type(self, data_type: str):
        """Test specific data type with current source"""
        try:
            # Test based on data type
            if data_type == "stocks":
                result = self.data_manager.get_stock_data("AAPL", "1d", "1h")
            elif data_type == "forex":
                result = self.data_manager.get_forex_data("EURUSD", "1d")
            elif data_type == "crypto":
                result = self.data_manager.get_crypto_data("BTC", "1d")
            elif data_type == "news":
                result = self.data_manager.get_news_data("financial", 5)
            elif data_type == "economic":
                result = self.data_manager.get_economic_data("GDP", "US")
            else:
                result = {"success": False, "error": "Test not implemented for this data type"}

            print(f"Test {data_type}: {'✅ Success' if result.get('success') else '❌ Failed'}")
            if not result.get("success"):
                print(f"Error: {result.get('error')}")

        except Exception as e:
            print(f"❌ Test error for {data_type}: {e}")

    def test_specific_source(self, source_name: str):
        """Test a specific data source"""
        result = self.data_manager.test_data_source(source_name)
        self.test_results[source_name] = result

        # Refresh the UI to show test result
        self.create_source_config_ui(source_name)

    def test_all_sources(self):
        """Test all configured sources"""
        available_sources = self.data_manager.get_available_sources()

        for source_name in available_sources.keys():
            self.test_specific_source(source_name)

        print("🧪 Tested all available sources")

    def save_configuration(self):
        """Save current configuration"""
        success = self.data_manager.save_configuration()
        if success:
            print("✅ Configuration saved successfully")
        else:
            print("❌ Failed to save configuration")

    def reset_to_defaults(self):
        """Reset to default configuration"""
        self.data_manager.reset_to_defaults()
        print("🔄 Reset to default configuration")

    def clear_all_cache(self):
        """Clear all cached data"""
        self.data_manager.clear_cache()
        print("🗑️ All cache cleared")

    def show_cache_stats(self):
        """Show cache statistics"""
        stats = self.data_manager.get_cache_stats()
        print("📊 Cache Statistics:")
        for key, value in stats.items():
            print(f"  {key}: {value}")

    def save_fincept_config(self):
        """Save Fincept API configuration"""
        config = {}
        if dpg.does_item_exist("fincept_endpoint"):
            config["endpoint"] = dpg.get_value("fincept_endpoint")
        if dpg.does_item_exist("fincept_api_key"):
            config["api_key"] = dpg.get_value("fincept_api_key")
        if dpg.does_item_exist("fincept_realtime"):
            config["realtime"] = dpg.get_value("fincept_realtime")

        self.data_manager.set_data_source("stocks", "fincept_api", config)
        print("✅ Fincept API configuration saved")

    def test_fincept_api_key(self):
        """Test Fincept API key"""
        print("🔑 Testing Fincept API key...")

    def save_alpha_vantage_config(self):
        """Save Alpha Vantage configuration"""
        config = {}
        if dpg.does_item_exist("alpha_vantage_api_key"):
            config["api_key"] = dpg.get_value("alpha_vantage_api_key")
        if dpg.does_item_exist("alpha_vantage_rate_limit"):
            config["rate_limit"] = dpg.get_value("alpha_vantage_rate_limit")

        # Apply to supported data types
        for data_type in ["stocks", "forex", "crypto"]:
            try:
                self.data_manager.set_data_source(data_type, "alpha_vantage", config)
            except:
                pass  # Skip if not supported

        print("✅ Alpha Vantage configuration saved")

    def test_alpha_vantage_api_key(self):
        """Test Alpha Vantage API key"""
        print("🔑 Testing Alpha Vantage API key...")

    def browse_csv_file(self):
        """Browse for CSV file"""
        print("📁 File browser would open here")

    def import_csv_data(self):
        """Import data from CSV"""
        if not dpg.does_item_exist("csv_file_path"):
            return

        file_path = dpg.get_value("csv_file_path")
        data_type = dpg.get_value("csv_data_type") if dpg.does_item_exist("csv_data_type") else "stocks"

        # Build column mapping
        column_mapping = {}
        if dpg.does_item_exist("csv_date_column"):
            date_col = dpg.get_value("csv_date_column")
            if date_col:
                column_mapping["timestamp"] = date_col

        if dpg.does_item_exist("csv_price_column"):
            price_col = dpg.get_value("csv_price_column")
            if price_col:
                column_mapping["price"] = price_col

        if dpg.does_item_exist("csv_volume_column"):
            volume_col = dpg.get_value("csv_volume_column")
            if volume_col:
                column_mapping["volume"] = volume_col

        if file_path and column_mapping:
            result = self.data_manager.import_csv_data(file_path, data_type, column_mapping)
            if result.get("success"):
                print(f"✅ Successfully imported {result.get('row_count', 0)} rows")
            else:
                print(f"❌ Import failed: {result.get('error')}")

    def preview_csv_data(self):
        """Preview CSV data"""
        print("👁️ CSV preview would show here")

    def save_websocket_config(self):
        """Save WebSocket configuration"""
        config = {}
        if dpg.does_item_exist("websocket_url"):
            config["url"] = dpg.get_value("websocket_url")
        if dpg.does_item_exist("websocket_token"):
            config["token"] = dpg.get_value("websocket_token")
        if dpg.does_item_exist("websocket_auto_reconnect"):
            config["auto_reconnect"] = dpg.get_value("websocket_auto_reconnect")

        # Apply to supported data types
        for data_type in ["stocks", "crypto", "forex"]:
            try:
                self.data_manager.set_data_source(data_type, "websocket_feed", config)
            except:
                pass

        print("✅ WebSocket configuration saved")

    def test_websocket_connection(self):
        """Test WebSocket connection"""
        print("🔌 Testing WebSocket connection...")

    def cleanup(self):
        """Clean up resources"""
        self.test_results.clear()
        self.csv_column_mapping.clear()