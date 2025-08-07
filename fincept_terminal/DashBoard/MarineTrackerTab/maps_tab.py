"""
Maps Tab module for Fincept Terminal
Updated to use centralized logging system
Fixed positional arguments errors
"""

# maps_tab.py - Fixed version
import dearpygui.dearpygui as dpg
import subprocess
import json
import os
import time
import sys
from fincept_terminal.Utils.base_tab import BaseTab

from fincept_terminal.Utils.Logging.logger import logger, log_operation

class MaritimeMapTab(BaseTab):
    """Maritime Maps tab that controls separate PyQt process"""

    def __init__(self, app):
        super().__init__(app)
        self.map_process = None
        self.markers_file = "maritime_markers.json"
        self.commands_file = "map_commands.json"
        self.status_file = "map_status.json"
        self.markers_data = self.load_markers()

        # Check if PyQt is available
        self.pyqt_available = self.check_pyqt()

    def get_label(self):
        return "Maps"  # Simplified without emoji

    def check_pyqt(self):
        """Check if PyQt5 is available"""
        try:
            import PyQt5
            return True
        except ImportError:
            return False

    def load_markers(self):
        """Load markers from file"""
        try:
            if os.path.exists(self.markers_file):
                with open(self.markers_file, 'r', encoding='utf-8') as f:
                    return json.load(f)
        except Exception as e:
            logger.error(f"Load markers error: {e}", module="Maps_Tab", context={'e': str(e)})
        return []

    def save_markers(self):
        """Save markers to file"""
        try:
            with open(self.markers_file, 'w', encoding='utf-8') as f:
                json.dump(self.markers_data, f, indent=2, ensure_ascii=False)
        except Exception as e:
            logger.error(f"Save error: {e}", module="Maps_Tab", context={'e': str(e)})

    def send_command(self, command):
        """Send command to PyQt process"""
        try:
            commands = {'commands': [command]}
            if os.path.exists(self.commands_file):
                try:
                    with open(self.commands_file, 'r', encoding='utf-8') as f:
                        existing = json.load(f)
                        existing.get('commands', []).append(command)
                        commands = existing
                except:
                    pass

            with open(self.commands_file, 'w', encoding='utf-8') as f:
                json.dump(commands, f, ensure_ascii=False)
        except Exception as e:
            logger.error(f"Command send error: {e}", module="Maps_Tab", context={'e': str(e)})

    def get_map_status(self):
        """Get status from PyQt process"""
        try:
            if os.path.exists(self.status_file):
                with open(self.status_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    return data.get('status', 'unknown')
        except Exception as e:
            logger.error(f"Status read error: {e}", module="Maps_Tab", context={'e': str(e)})
        return 'unknown'

    def safe_add_text(self, text, **kwargs):
        """Safely add text with error handling"""
        try:
            # Ensure text is a string and handle encoding
            if not isinstance(text, str):
                text = str(text)

            # Remove or replace problematic characters
            text = text.encode('ascii', 'replace').decode('ascii')

            return dpg.add_text(text, **kwargs)
        except Exception as e:
            logger.error(f"Text add error: {e}", module="Maps_Tab", context={'e': str(e)})
            # Fallback to simple text
            try:
                return dpg.add_text("Text Error", **kwargs)
            except:
                return None

    def create_content(self):
        """Create maritime maps dashboard content"""
        try:
            self.add_section_header("FINCEPT MARITIME MAPS")

            # Control panel
            with self.create_child_window(tag="map_control", width=-1, height=120):
                self.add_section_header("Map Control")

                with dpg.group(horizontal=True):
                    if self.pyqt_available:
                        dpg.add_button(
                            label="LAUNCH MAP",
                            callback=self.launch_map,
                            width=150,
                            height=35,
                            tag="launch_btn"
                        )
                        dpg.add_button(
                            label="CLOSE MAP",
                            callback=self.close_map,
                            width=120,
                            height=35
                        )
                    else:
                        self.safe_add_text("PyQt5 Required", color=[255, 100, 100])
                        self.safe_add_text("Run: pip install PyQt5 PyQtWebEngine", color=[255, 200, 100])

                dpg.add_spacer(height=5)

                with dpg.group(horizontal=True):
                    self.safe_add_text("Status:", color=[255, 255, 255])
                    status = "Ready" if self.pyqt_available else "PyQt5 Missing"
                    color = [100, 255, 100] if self.pyqt_available else [255, 100, 100]
                    self.safe_add_text(status, color=color, tag="status_text")

                    dpg.add_spacer(width=50)
                    self.safe_add_text("Markers:", color=[255, 255, 255])
                    self.safe_add_text(str(len(self.markers_data)), color=[255, 200, 100], tag="marker_count")

            dpg.add_spacer(height=10)

            # Marker controls
            with self.create_child_window(tag="marker_controls", width=-1, height=150):
                self.add_section_header("Add Markers")

                with dpg.group(horizontal=True):
                    dpg.add_input_text(hint="Marker Title", width=150, tag="marker_title")
                    dpg.add_combo(
                        items=["Ship", "Port", "Industry", "Bank", "Exchange"],
                        default_value="Ship",
                        width=100,
                        tag="marker_type",
                        callback=self.on_marker_type_changed
                    )

                dpg.add_spacer(height=5)

                with dpg.group(horizontal=True):
                    dpg.add_input_float(label="Lat", default_value=19.0760, format="%.4f", width=100, tag="lat_input")
                    dpg.add_input_float(label="Lng", default_value=72.8777, format="%.4f", width=100, tag="lng_input")
                    dpg.add_button(label="ADD", callback=self.add_marker, width=60)

                dpg.add_spacer(height=5)

                # Presets
                presets = ["Mumbai Port", "Shanghai Port", "Singapore Port", "Hong Kong Port"]

                with dpg.group(horizontal=True):
                    dpg.add_combo(items=presets, default_value="Mumbai Port", width=150, tag="preset_combo")
                    dpg.add_button(label="ADD PRESET", callback=self.add_preset, width=100)

            dpg.add_spacer(height=10)

            # Features and markers list
            with dpg.group(horizontal=True):
                # Map features
                with self.create_child_window(tag="features", width=300, height=200):
                    self.add_section_header("Map Features")

                    dpg.add_button(label="Toggle Routes", callback=self.toggle_routes, width=150)
                    dpg.add_spacer(height=5)
                    dpg.add_button(label="Toggle Ships", callback=self.toggle_ships, width=150)
                    dpg.add_spacer(height=5)
                    dpg.add_button(label="Clear All", callback=self.clear_all, width=150)

                    dpg.add_spacer(height=15)
                    self.safe_add_text("Quick Add:")
                    dpg.add_button(label="Indian Ports", callback=self.add_indian_ports, width=150)
                    dpg.add_spacer(height=3)
                    dpg.add_button(label="Financial Centers", callback=self.add_financial, width=150)

                # Markers list
                with self.create_child_window(tag="markers_list", width=460, height=200):
                    self.add_section_header("Current Markers")

                    try:
                        with dpg.table(header_row=True, resizable=True, tag="markers_table"):
                            dpg.add_table_column(label="Title", width_fixed=True, init_width_or_weight=150)
                            dpg.add_table_column(label="Type", width_fixed=True, init_width_or_weight=80)
                            dpg.add_table_column(label="Coordinates", width_fixed=True, init_width_or_weight=120)

                        self.update_markers_table()
                    except Exception as e:
                        logger.error(f"Table creation error: {e}", module="Maps_Tab", context={'e': str(e)})
                        self.safe_add_text("Table Error - Check Console")

        except Exception as e:
            logger.error(f"Content creation error: {e}", module="Maps_Tab", context={'e': str(e)})
            # Create minimal error display
            try:
                self.safe_add_text(f"Error loading Maps tab: {str(e)}", color=[255, 100, 100])
            except:
                pass

    def launch_map(self, *args, **kwargs):
        """Launch PyQt map process - Flexible callback signature"""
        if not self.pyqt_available:
            self.update_status("PyQt5 not available")
            return

        try:
            if self.map_process is None or self.map_process.poll() is not None:
                # Get path to leaflet_map_ui.py
                current_dir = os.path.dirname(os.path.abspath(__file__))
                map_script = os.path.join(current_dir, "leaflet_map_ui.py")

                if not os.path.exists(map_script):
                    self.update_status("Map script not found")
                    return

                # Launch separate process
                self.map_process = subprocess.Popen([
                    sys.executable, map_script
                ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

                self.update_status("Launching map...")

                try:
                    if dpg.does_item_exist("launch_btn"):
                        dpg.set_item_label("launch_btn", "MAP RUNNING")
                except:
                    pass

            else:
                self.update_status("Map already running")

        except Exception as e:
            self.update_status(f"Launch error: {str(e)}")

    def close_map(self, *args, **kwargs):
        """Close PyQt map process - Flexible callback signature"""
        try:
            if self.map_process and self.map_process.poll() is None:
                self.map_process.terminate()
                self.map_process = None
                self.update_status("Map closed")
                try:
                    if dpg.does_item_exist("launch_btn"):
                        dpg.set_item_label("launch_btn", "LAUNCH MAP")
                except:
                    pass
            else:
                self.update_status("Map not running")
        except Exception as e:
            self.update_status(f"Close error: {str(e)}")

    def on_marker_type_changed(self, *args, **kwargs):
        """Handle marker type change in DearPyGUI - Flexible callback signature"""
        try:
            # Extract app_data from args if available
            app_data = args[1] if len(args) > 1 else kwargs.get('app_data', "Ship")
            marker_type = app_data if app_data is not None else "Ship"
            self.send_marker_type(marker_type)
            self.update_status(f"Marker type: {marker_type}")
        except Exception as e:
            logger.error(f"Marker type change error: {e}", module="Maps_Tab", context={'e': str(e)})

    def send_marker_type(self, marker_type):
        """Send marker type change to PyQt process"""
        try:
            command = f"set_marker_type:{marker_type}"
            self.send_command(command)
        except Exception as e:
            logger.error(f"Send marker type error: {e}", module="Maps_Tab", context={'e': str(e)})

    def add_marker(self, *args, **kwargs):
        """Add marker from inputs - Flexible callback signature"""
        try:
            title = dpg.get_value("marker_title") or "New Marker"
            marker_type = dpg.get_value("marker_type") or "Ship"
            lat = dpg.get_value("lat_input") or 0.0
            lng = dpg.get_value("lng_input") or 0.0

            marker_data = {
                "lat": float(lat),
                "lng": float(lng),
                "title": str(title),
                "type": str(marker_type)
            }
            self.markers_data.append(marker_data)
            self.save_markers()

            self.update_markers_table()
            self.update_marker_count()
            self.update_status(f"Added: {title}")

            if dpg.does_item_exist("marker_title"):
                dpg.set_value("marker_title", "")

        except Exception as e:
            self.update_status(f"Add error: {str(e)}")

    def add_preset(self, *args, **kwargs):
        """Add preset location - Flexible callback signature"""
        preset_coords = {
            "Mumbai Port": (19.0760, 72.8777, "Port"),
            "Shanghai Port": (31.2304, 121.4737, "Port"),
            "Singapore Port": (1.3521, 103.8198, "Port"),
            "Hong Kong Port": (22.3193, 114.1694, "Port")
        }

        try:
            selected = dpg.get_value("preset_combo") or "Mumbai Port"
            if selected in preset_coords:
                lat, lng, marker_type = preset_coords[selected]

                marker_data = {
                    "lat": float(lat),
                    "lng": float(lng),
                    "title": str(selected),
                    "type": str(marker_type)
                }
                self.markers_data.append(marker_data)
                self.save_markers()

                self.update_markers_table()
                self.update_marker_count()
                self.update_status(f"Added: {selected}")
        except Exception as e:
            self.update_status(f"Preset error: {str(e)}")

    def add_indian_ports(self, *args, **kwargs):
        """Add Indian ports - Flexible callback signature"""
        ports = [
            (19.0760, 72.8777, "Mumbai Port", "Port"),
            (22.5726, 88.3639, "Kolkata Port", "Port"),
            (13.0827, 80.2707, "Chennai Port", "Port"),
            (9.9312, 76.2673, "Cochin Port", "Port")
        ]
        self.add_multiple_markers(ports, "Indian ports")

    def add_financial(self, *args, **kwargs):
        """Add financial centers - Flexible callback signature"""
        centers = [
            (19.1136, 72.8697, "Mumbai Financial District", "Bank"),
            (28.5355, 77.3910, "Delhi Financial District", "Bank"),
            (1.2797, 103.8565, "Singapore Financial Center", "Bank")
        ]
        self.add_multiple_markers(centers, "Financial centers")

    def add_multiple_markers(self, markers_list, description):
        """Add multiple markers"""
        try:
            added = 0
            for lat, lng, title, marker_type in markers_list:
                marker_data = {
                    "lat": float(lat),
                    "lng": float(lng),
                    "title": str(title),
                    "type": str(marker_type)
                }
                self.markers_data.append(marker_data)
                added += 1

            if added > 0:
                self.save_markers()
                self.update_markers_table()
                self.update_marker_count()
                self.update_status(f"Added {added} {description}")
        except Exception as e:
            self.update_status(f"Bulk add error: {str(e)}")

    def toggle_routes(self, *args, **kwargs):
        """Toggle trade routes - Flexible callback signature"""
        try:
            self.send_command("toggle_routes")
            self.update_status("Routes toggled")
        except Exception as e:
            self.update_status(f"Routes error: {str(e)}")

    def toggle_ships(self, *args, **kwargs):
        """Toggle live ships - Flexible callback signature"""
        try:
            self.send_command("toggle_ships")
            self.update_status("Ships toggled")
        except Exception as e:
            self.update_status(f"Ships error: {str(e)}")

    def clear_all(self, *args, **kwargs):
        """Clear all markers - Flexible callback signature"""
        try:
            self.markers_data = []
            self.save_markers()
            self.send_command("clear_all")
            self.update_markers_table()
            self.update_marker_count()
            self.update_status("Cleared all")
        except Exception as e:
            self.update_status(f"Clear error: {str(e)}")

    def update_markers_table(self):
        """Update markers table"""
        try:
            if not dpg.does_item_exist("markers_table"):
                return

            # Clear existing rows safely
            try:
                children = dpg.get_item_children("markers_table", slot=1)
                if children:
                    for child in children:
                        try:
                            dpg.delete_item(child)
                        except:
                            pass
            except:
                pass

            # Add new rows
            for marker in self.markers_data[-6:]:  # Show last 6
                try:
                    with dpg.table_row(parent="markers_table"):
                        title = str(marker.get('title', 'Unknown'))
                        title = title[:20] + "..." if len(title) > 20 else title

                        self.safe_add_text(title)
                        self.safe_add_text(str(marker.get('type', 'Unknown')))
                        self.safe_add_text(f"{marker.get('lat', 0):.2f}, {marker.get('lng', 0):.2f}")
                except Exception as e:
                    logger.error(f"Row add error: {e}", module="Maps_Tab", context={'e': str(e)})

        except Exception as e:
            logger.error(f"Table update error: {e}", module="Maps_Tab", context={'e': str(e)})

    def update_marker_count(self):
        """Update marker count"""
        try:
            if dpg.does_item_exist("marker_count"):
                dpg.set_value("marker_count", str(len(self.markers_data)))
        except Exception as e:
            logger.error(f"Count update error: {e}", module="Maps_Tab", context={'e': str(e)})

    def update_status(self, message):
        """Update status"""
        try:
            # Ensure message is a clean string
            message = str(message).encode('ascii', 'replace').decode('ascii')

            if dpg.does_item_exist("status_text"):
                dpg.set_value("status_text", message)
            logger.info(f"Maps: {message}", module="Maps_Tab", context={'message': message})
        except Exception as e:
            logger.error(f"Status update error: {e}", module="Maps_Tab", context={'e': str(e)})

    def cleanup(self):
        """Cleanup"""
        try:
            if self.map_process:
                self.map_process.terminate()

            # Clean up files
            for f in [self.commands_file, self.status_file]:
                try:
                    if os.path.exists(f):
                        os.remove(f)
                except:
                    pass
        except Exception as e:
            logger.error(f"Cleanup error: {e}", module="Maps_Tab", context={'e': str(e)})