# maps_tab.py - DearPyGUI Maritime Maps Tab (Process Controller)
import dearpygui.dearpygui as dpg
import subprocess
import json
import os
import time
import sys
from fincept_terminal.Utils.base_tab import BaseTab


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
        return "🌍 Maps"

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
                with open(self.markers_file, 'r') as f:
                    return json.load(f)
        except:
            pass
        return []

    def save_markers(self):
        """Save markers to file"""
        try:
            with open(self.markers_file, 'w') as f:
                json.dump(self.markers_data, f, indent=2)
        except Exception as e:
            print(f"Save error: {e}")

    def send_command(self, command):
        """Send command to PyQt process"""
        try:
            commands = {'commands': [command]}
            if os.path.exists(self.commands_file):
                with open(self.commands_file, 'r') as f:
                    existing = json.load(f)
                    existing.get('commands', []).append(command)
                    commands = existing

            with open(self.commands_file, 'w') as f:
                json.dump(commands, f)
        except Exception as e:
            print(f"Command send error: {e}")

    def get_map_status(self):
        """Get status from PyQt process"""
        try:
            if os.path.exists(self.status_file):
                with open(self.status_file, 'r') as f:
                    data = json.load(f)
                    return data.get('status', 'unknown')
        except:
            pass
        return 'unknown'

    def create_content(self):
        """Create maritime maps dashboard content"""
        self.add_section_header("🚢 FINCEPT MARITIME MAPS")

        # Control panel
        with self.create_child_window(tag="map_control", width=-1, height=120):
            self.add_section_header("🗺️ Map Control")

            with dpg.group(horizontal=True):
                if self.pyqt_available:
                    dpg.add_button(
                        label="🚀 LAUNCH MAP",
                        callback=self.launch_map,
                        width=150, height=35,
                        tag="launch_btn"
                    )
                    dpg.add_button(
                        label="❌ CLOSE MAP",
                        callback=self.close_map,
                        width=120, height=35
                    )
                else:
                    dpg.add_text("❌ PyQt5 Required", color=[255, 100, 100])
                    dpg.add_text("Run: pip install PyQt5 PyQtWebEngine", color=[255, 200, 100])

            dpg.add_spacer(height=5)

            with dpg.group(horizontal=True):
                dpg.add_text("Status:", color=[255, 255, 255])
                status = "Ready" if self.pyqt_available else "PyQt5 Missing"
                color = [100, 255, 100] if self.pyqt_available else [255, 100, 100]
                dpg.add_text(status, color=color, tag="status_text")

                dpg.add_spacer(width=50)
                dpg.add_text("Markers:", color=[255, 255, 255])
                dpg.add_text(str(len(self.markers_data)), color=[255, 200, 100], tag="marker_count")

        dpg.add_spacer(height=10)

        # Marker controls
        with self.create_child_window(tag="marker_controls", width=-1, height=150):
            self.add_section_header("📍 Add Markers")

            with dpg.group(horizontal=True):
                dpg.add_input_text(hint="Marker Title", width=150, tag="marker_title")
                dpg.add_combo(
                    items=["Ship", "Port", "Industry", "Bank", "Exchange"],
                    default_value="Ship", width=100, tag="marker_type",
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
            preset_coords = {
                "Mumbai Port": (19.0760, 72.8777, "Port"),
                "Shanghai Port": (31.2304, 121.4737, "Port"),
                "Singapore Port": (1.3521, 103.8198, "Port"),
                "Hong Kong Port": (22.3193, 114.1694, "Port")
            }

            with dpg.group(horizontal=True):
                dpg.add_combo(items=presets, default_value="Mumbai Port", width=150, tag="preset_combo")
                dpg.add_button(label="ADD PRESET", callback=self.add_preset, width=100)

        dpg.add_spacer(height=10)

        # Features and markers list
        with dpg.group(horizontal=True):
            # Map features
            with self.create_child_window(tag="features", width=300, height=200):
                self.add_section_header("🎛️ Map Features")

                dpg.add_button(label="🚢 Toggle Routes", callback=self.toggle_routes, width=150)
                dpg.add_spacer(height=5)
                dpg.add_button(label="⚓ Toggle Ships", callback=self.toggle_ships, width=150)
                dpg.add_spacer(height=5)
                dpg.add_button(label="🗑️ Clear All", callback=self.clear_all, width=150)

                dpg.add_spacer(height=15)
                dpg.add_text("Quick Add:")
                dpg.add_button(label="🇮🇳 Indian Ports", callback=self.add_indian_ports, width=150)
                dpg.add_spacer(height=3)
                dpg.add_button(label="🏦 Financial Centers", callback=self.add_financial, width=150)

            # Markers list
            with self.create_child_window(tag="markers_list", width=460, height=200):
                self.add_section_header("📍 Current Markers")

                with dpg.table(header_row=True, resizable=True, tag="markers_table"):
                    dpg.add_table_column(label="Title", width_fixed=True, init_width_or_weight=150)
                    dpg.add_table_column(label="Type", width_fixed=True, init_width_or_weight=80)
                    dpg.add_table_column(label="Coordinates", width_fixed=True, init_width_or_weight=120)

                self.update_markers_table()

    def launch_map(self):
        """Launch PyQt map process"""
        if not self.pyqt_available:
            self.update_status("❌ PyQt5 not available")
            return

        try:
            if self.map_process is None or self.map_process.poll() is not None:
                # Get path to leaflet_map_ui.py
                current_dir = os.path.dirname(os.path.abspath(__file__))
                map_script = os.path.join(current_dir, "leaflet_map_ui.py")

                if not os.path.exists(map_script):
                    self.update_status("❌ Map script not found")
                    return

                # Launch separate process
                self.map_process = subprocess.Popen([
                    sys.executable, map_script
                ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

                self.update_status("🚀 Launching map...")
                dpg.set_item_label("launch_btn", "🗺️ MAP RUNNING")

                # Check status after delay
                dpg.set_value("launch_btn", lambda: self.check_map_status())

            else:
                self.update_status("✅ Map already running")

        except Exception as e:
            self.update_status(f"❌ Launch error: {str(e)}")

    def close_map(self):
        """Close PyQt map process"""
        try:
            if self.map_process and self.map_process.poll() is None:
                self.map_process.terminate()
                self.map_process = None
                self.update_status("❌ Map closed")
                dpg.set_item_label("launch_btn", "🚀 LAUNCH MAP")
            else:
                self.update_status("⚠️ Map not running")
        except Exception as e:
            self.update_status(f"❌ Close error: {str(e)}")

    def check_map_status(self):
        """Check if map is ready"""
        status = self.get_map_status()
        if status == "ready":
            self.update_status("✅ Map ready")
            # Send initial marker type to map
            self.send_marker_type("Ship")
        elif status == "error":
            self.update_status("❌ Map error")

    def on_marker_type_changed(self, sender, app_data):
        """Handle marker type change in DearPyGUI"""
        try:
            marker_type = app_data
            self.send_marker_type(marker_type)
            self.update_status(f"🎯 Marker type: {marker_type}")
        except Exception as e:
            print(f"Marker type change error: {e}")

    def send_marker_type(self, marker_type):
        """Send marker type change to PyQt process"""
        try:
            command = f"set_marker_type:{marker_type}"
            self.send_command(command)
        except Exception as e:
            print(f"Send marker type error: {e}")

    def add_marker(self):
        """Add marker from inputs"""
        try:
            title = dpg.get_value("marker_title") or "New Marker"
            marker_type = dpg.get_value("marker_type")
            lat = dpg.get_value("lat_input")
            lng = dpg.get_value("lng_input")

            marker_data = {"lat": lat, "lng": lng, "title": title, "type": marker_type}
            self.markers_data.append(marker_data)
            self.save_markers()

            self.update_markers_table()
            self.update_marker_count()
            self.update_status(f"📍 Added: {title}")

            dpg.set_value("marker_title", "")

        except Exception as e:
            self.update_status(f"❌ Add error: {str(e)}")

    def add_preset(self):
        """Add preset location"""
        preset_coords = {
            "Mumbai Port": (19.0760, 72.8777, "Port"),
            "Shanghai Port": (31.2304, 121.4737, "Port"),
            "Singapore Port": (1.3521, 103.8198, "Port"),
            "Hong Kong Port": (22.3193, 114.1694, "Port")
        }

        selected = dpg.get_value("preset_combo")
        if selected in preset_coords:
            lat, lng, marker_type = preset_coords[selected]

            marker_data = {"lat": lat, "lng": lng, "title": selected, "type": marker_type}
            self.markers_data.append(marker_data)
            self.save_markers()

            self.update_markers_table()
            self.update_marker_count()
            self.update_status(f"📍 Added: {selected}")

    def add_indian_ports(self):
        """Add Indian ports"""
        ports = [
            (19.0760, 72.8777, "Mumbai Port", "Port"),
            (22.5726, 88.3639, "Kolkata Port", "Port"),
            (13.0827, 80.2707, "Chennai Port", "Port"),
            (9.9312, 76.2673, "Cochin Port", "Port")
        ]
        self.add_multiple_markers(ports, "Indian ports")

    def add_financial(self):
        """Add financial centers"""
        centers = [
            (19.1136, 72.8697, "Mumbai Financial District", "Bank"),
            (28.5355, 77.3910, "Delhi Financial District", "Bank"),
            (1.2797, 103.8565, "Singapore Financial Center", "Bank")
        ]
        self.add_multiple_markers(centers, "Financial centers")

    def add_multiple_markers(self, markers_list, description):
        """Add multiple markers"""
        added = 0
        for lat, lng, title, marker_type in markers_list:
            marker_data = {"lat": lat, "lng": lng, "title": title, "type": marker_type}
            self.markers_data.append(marker_data)
            added += 1

        if added > 0:
            self.save_markers()
            self.update_markers_table()
            self.update_marker_count()
            self.update_status(f"📍 Added {added} {description}")

    def toggle_routes(self):
        """Toggle trade routes"""
        self.send_command("toggle_routes")
        self.update_status("🚢 Routes toggled")

    def toggle_ships(self):
        """Toggle live ships"""
        self.send_command("toggle_ships")
        self.update_status("⚓ Ships toggled")

    def clear_all(self):
        """Clear all markers"""
        self.markers_data = []
        self.save_markers()
        self.send_command("clear_all")
        self.update_markers_table()
        self.update_marker_count()
        self.update_status("🗑️ Cleared all")

    def update_markers_table(self):
        """Update markers table"""
        try:
            if dpg.does_item_exist("markers_table"):
                children = dpg.get_item_children("markers_table", slot=1)
                for child in children:
                    dpg.delete_item(child)

                for marker in self.markers_data[-6:]:  # Show last 6
                    with dpg.table_row(parent="markers_table"):
                        title = marker['title'][:20] + "..." if len(marker['title']) > 20 else marker['title']
                        dpg.add_text(title)
                        dpg.add_text(marker['type'])
                        dpg.add_text(f"{marker['lat']:.2f}, {marker['lng']:.2f}")
        except Exception as e:
            print(f"Table update error: {e}")

    def update_marker_count(self):
        """Update marker count"""
        try:
            if dpg.does_item_exist("marker_count"):
                dpg.set_value("marker_count", str(len(self.markers_data)))
        except:
            pass

    def update_status(self, message):
        """Update status"""
        try:
            if dpg.does_item_exist("status_text"):
                dpg.set_value("status_text", message)
            print(f"Maps: {message}")
        except:
            pass

    def cleanup(self):
        """Cleanup"""
        try:
            if self.map_process:
                self.map_process.terminate()

            # Clean up files
            for f in [self.commands_file, self.status_file]:
                if os.path.exists(f):
                    os.remove(f)
        except:
            pass