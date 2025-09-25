"""
Streamlined Headless Maritime Map for Fincept Terminal
Optimized with India-focused trade routes and proper window controls
"""

import sys
import json
import os
import time
from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QHBoxLayout, QWidget, QPushButton, QLabel, QSizeGrip
from PyQt5.QtWebEngineWidgets import QWebEngineView
from PyQt5.QtCore import Qt, QUrl, QTimer, QPoint
from PyQt5.QtGui import QMouseEvent
from fincept_terminal.utils.Logging.logger import logger, operation, monitor_performance


class DragBar(QWidget):
    """Custom drag bar for frameless window"""

    def __init__(self, parent):
        super().__init__(parent)
        self.parent = parent
        self.dragging = False
        self.drag_position = QPoint()

        layout = QHBoxLayout(self)
        layout.setContentsMargins(5, 2, 5, 2)

        self.title_label = QLabel("⚓ FINCEPT MARITIME MAP")
        self.title_label.setStyleSheet("color: #ff8c00; font-weight: bold; font-size: 11px;")
        layout.addWidget(self.title_label)

        layout.addStretch()

        # Minimize button
        min_btn = QPushButton("_")
        min_btn.setFixedSize(25, 20)
        min_btn.clicked.connect(parent.showMinimized)
        min_btn.setStyleSheet("""
            QPushButton { background: #333; color: #fff; border: none; font-weight: bold; }
            QPushButton:hover { background: #555; }
        """)
        layout.addWidget(min_btn)

        # Maximize button
        max_btn = QPushButton("□")
        max_btn.setFixedSize(25, 20)
        max_btn.clicked.connect(self.toggle_maximize)
        max_btn.setStyleSheet("""
            QPushButton { background: #333; color: #fff; border: none; font-weight: bold; }
            QPushButton:hover { background: #555; }
        """)
        layout.addWidget(max_btn)

        # Close button
        close_btn = QPushButton("✕")
        close_btn.setFixedSize(25, 20)
        close_btn.clicked.connect(parent.close_application)
        close_btn.setStyleSheet("""
            QPushButton { background: #d32f2f; color: #fff; border: none; font-weight: bold; }
            QPushButton:hover { background: #f44336; }
        """)
        layout.addWidget(close_btn)

        self.setStyleSheet("background: #1a1a1a; border-bottom: 1px solid #ff8c00;")
        self.setFixedHeight(24)

    def toggle_maximize(self):
        if self.parent.isMaximized():
            self.parent.showNormal()
        else:
            self.parent.showMaximized()

    def mousePressEvent(self, event: QMouseEvent):
        if event.button() == Qt.LeftButton:
            self.dragging = True
            self.drag_position = event.globalPos() - self.parent.frameGeometry().topLeft()
            event.accept()

    def mouseMoveEvent(self, event: QMouseEvent):
        if self.dragging and event.buttons() == Qt.LeftButton:
            self.parent.move(event.globalPos() - self.drag_position)
            event.accept()

    def mouseReleaseEvent(self, event: QMouseEvent):
        self.dragging = False
        event.accept()


class MaritimeMapWindow(QMainWindow):
    """Maritime Map Window with drag and resize"""

    def __init__(self):
        super().__init__()
        self.markers_file = "maritime_markers.json"
        self.commands_file = "map_commands.json"
        self.status_file = "map_status.json"
        self.markers_data = self.load_markers()
        self.existing_markers = set()
        self.selected_marker_type = "Ship"
        self.last_modified = 0

        for marker in self.markers_data:
            key = f"{marker['lat']:.6f}_{marker['lng']:.6f}"
            self.existing_markers.add(key)

        logger.info("Maritime map initialized")
        self.init_ui()
        self.create_map()
        self.start_file_watcher()

    def init_ui(self):
        """Initialize UI with drag bar and resize"""
        self.setWindowTitle("FINCEPT MARITIME MAP")
        self.setGeometry(100, 100, 1200, 800)

        # Frameless but resizable - MODIFIED for proper fullscreen behavior
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
        self.setAttribute(Qt.WA_TranslucentBackground, False)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        # Add drag bar
        self.drag_bar = DragBar(self)
        main_layout.addWidget(self.drag_bar)

        # Add web view
        self.web_view = QWebEngineView()
        main_layout.addWidget(self.web_view)

        # Add resize grip in bottom-right corner
        self.size_grip = QSizeGrip(self)
        self.size_grip.setStyleSheet("background: #ff8c00; width: 16px; height: 16px;")

        grip_layout = QHBoxLayout()
        grip_layout.addStretch()
        grip_layout.addWidget(self.size_grip)
        grip_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.addLayout(grip_layout)

        self.setStyleSheet("""
            QMainWindow { 
                background-color: #000000; 
                border: 2px solid #ff8c00; 
            }
        """)

        logger.info("UI initialized with drag and resize")

    @monitor_performance
    def create_map(self):
        """Create enhanced Leaflet map with India trade routes"""
        with operation("create_map"):
            try:
                markers_json = json.dumps(self.markers_data)

                html_content = f"""
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8" />
    <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
    <link rel="stylesheet" href="https://unpkg.com/leaflet.markercluster@1.4.1/dist/MarkerCluster.css" />
    <style>
        body {{ margin: 0; padding: 0; background: #000; }}
        #map {{ height: 100vh; width: 100vw; }}
        .ocean-route {{ stroke: #00bfff; stroke-width: 3; stroke-opacity: 0.8; stroke-dasharray: 8,4; }}
        .route-animation {{ animation: routeFlow 3s linear infinite; }}
        @keyframes routeFlow {{ 0% {{ stroke-dashoffset: 0; }} 100% {{ stroke-dashoffset: -24; }} }}
        .ship-marker {{ animation: shipBob 2s ease-in-out infinite; z-index: 1000 !important; }}
        @keyframes shipBob {{ 0%, 100% {{ transform: translateY(0px); }} 50% {{ transform: translateY(-3px); }} }}
        .custom-marker {{ z-index: 1000 !important; }}
        .leaflet-marker-icon {{ z-index: 1000 !important; }}
    </style>
</head>
<body>
    <div id="map"></div>
    <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
    <script src="https://unpkg.com/leaflet.markercluster@1.4.1/dist/leaflet.markercluster.js"></script>
    <script src="https://unpkg.com/leaflet.heat@0.2.0/dist/leaflet-heat.js"></script>
    <script>
        var map = L.map('map').setView([20.0, 75.0], 4);
        
        L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{{z}}/{{y}}/{{x}}', {{
            attribution: 'FINCEPT MARITIME'
        }}).addTo(map);

        var markerCluster = L.markerClusterGroup();
        map.addLayer(markerCluster);
        var markers = [];
        var markerObjects = [];
        var heatmapData = [];
        var heatLayer = null;
        var routesLayer = null;
        var liveShipsLayer = null;
        var deleteMode = false;

        var markersData = {markers_json};

        function createCustomIcon(type, size = 24) {{
            var icons = {{
                'Ship': {{ symbol: '🚢', color: '#1e90ff' }},
                'Port': {{ symbol: '⚓', color: '#8b4513' }},
                'Industry': {{ symbol: '🏭', color: '#ff4500' }},
                'Bank': {{ symbol: '🏦', color: '#ffd700' }},
                'Exchange': {{ symbol: '💱', color: '#ff1493' }}
            }};
            var config = icons[type] || icons['Ship'];
            return L.divIcon({{
                html: `<div style="width:${{size}}px;height:${{size}}px;border-radius:50%;background:#fff;border:3px solid ${{config.color}};display:flex;align-items:center;justify-content:center;font-size:${{size-8}}px;box-shadow:0 2px 5px rgba(0,0,0,0.3);">${{config.symbol}}</div>`,
                iconSize: [size, size],
                iconAnchor: [size/2, size/2],
                popupAnchor: [0, -size/2],
                className: type === 'Ship' ? 'ship-marker' : 'custom-marker'
            }});
        }}

        // Enhanced India-focused trade routes
        var oceanRoutes = [
            {{
                name: "Mumbai-Rotterdam (Europe)",
                coords: [[19.0760, 72.8777], [15.0, 60.0], [12.0, 45.0], [15.0, 35.0], [30.0, 32.0], [35.0, 25.0], [38.0, 15.0], [40.0, 5.0], [42.0, -5.0], [45.0, -15.0], [51.9244, 4.4777]],
                value: "45B USD"
            }},
            {{
                name: "Mumbai-Shanghai (China)",
                coords: [[19.0760, 72.8777], [15.0, 68.0], [10.0, 75.0], [5.0, 85.0], [10.0, 95.0], [20.0, 110.0], [31.2304, 121.4737]],
                value: "156B USD"
            }},
            {{
                name: "Mumbai-Singapore",
                coords: [[19.0760, 72.8777], [15.0, 75.0], [10.0, 80.0], [5.0, 90.0], [1.3521, 103.8198]],
                value: "89B USD"
            }},
            {{
                name: "Chennai-Tokyo (Japan)",
                coords: [[13.0827, 80.2707], [10.0, 90.0], [15.0, 105.0], [25.0, 125.0], [35.6762, 139.6503]],
                value: "67B USD"
            }},
            {{
                name: "Kolkata-Hong Kong",
                coords: [[22.5726, 88.3639], [18.0, 95.0], [15.0, 105.0], [22.3193, 114.1694]],
                value: "45B USD"
            }},
            {{
                name: "Mumbai-Dubai (UAE)",
                coords: [[19.0760, 72.8777], [20.0, 65.0], [23.0, 58.0], [25.2048, 55.2708]],
                value: "78B USD"
            }},
            {{
                name: "Mumbai-New York (USA)",
                coords: [[19.0760, 72.8777], [15.0, 60.0], [10.0, 40.0], [5.0, 20.0], [0.0, 0.0], [10.0, -20.0], [25.0, -40.0], [35.0, -60.0], [40.7128, -74.0060]],
                value: "123B USD"
            }},
            {{
                name: "Chennai-Sydney (Australia)",
                coords: [[13.0827, 80.2707], [5.0, 90.0], [-10.0, 105.0], [-25.0, 130.0], [-33.8688, 151.2093]],
                value: "54B USD"
            }},
            {{
                name: "Cochin-Colombo-Malacca",
                coords: [[9.9312, 76.2673], [7.0, 79.0], [6.9271, 79.8612], [3.0, 95.0], [1.3521, 103.8198]],
                value: "34B USD"
            }},
            {{
                name: "Mumbai-Cape Town (Africa)",
                coords: [[19.0760, 72.8777], [10.0, 60.0], [-5.0, 50.0], [-20.0, 35.0], [-33.9249, 18.4241]],
                value: "28B USD"
            }}
        ];

        function addMarker(lat, lng, title, type) {{
            var key = `${{lat.toFixed(4)}}_${{lng.toFixed(4)}}`;
            for (let m of markers) {{
                if (`${{m.lat.toFixed(4)}}_${{m.lng.toFixed(4)}}` === key) {{
                    return false;
                }}
            }}
            
            var marker = L.marker([lat, lng], {{ icon: createCustomIcon(type, 28), zIndexOffset: 1000 }});
            marker.bindPopup(`<div style="background:#1e1e1e;color:#fff;padding:10px;border-radius:6px;border:2px solid #ff8c00;"><h3 style="color:#ff8c00;margin:0 0 8px 0;">${{title}}</h3><p><strong>Type:</strong> ${{type}}</p><p><strong>Coords:</strong> ${{lat.toFixed(4)}}, ${{lng.toFixed(4)}}</p></div>`);
            markerCluster.addLayer(marker);
            markers.push({{lat, lng, title, type, key}});
            markerObjects.push(marker);
            heatmapData.push([lat, lng, 1]);
            return true;
        }}

        markersData.forEach(m => addMarker(m.lat, m.lng, m.title, m.type));

        function createOceanRoutes() {{
            if (routesLayer) map.removeLayer(routesLayer);
            routesLayer = L.layerGroup();
            oceanRoutes.forEach(route => {{
                var polyline = L.polyline(route.coords, {{
                    color: '#00bfff',
                    weight: 4,
                    opacity: 0.8,
                    className: 'route-animation ocean-route'
                }});
                polyline.bindPopup(`<div style="background:#1e1e1e;color:#fff;padding:10px;border-radius:6px;"><h3 style="color:#00bfff;margin:0 0 8px 0;">${{route.name}}</h3><p><strong>Trade Value:</strong> ${{route.value}}</p></div>`);
                routesLayer.addLayer(polyline);
            }});
            map.addLayer(routesLayer);
        }}

        function createLiveShips() {{
            if (liveShipsLayer) map.removeLayer(liveShipsLayer);
            liveShipsLayer = L.layerGroup();
            oceanRoutes.forEach(route => {{
                for (var i = 1; i < route.coords.length - 1; i += 2) {{
                    var ship = L.marker(route.coords[i], {{ icon: createCustomIcon('Ship', 32), zIndexOffset: 1500 }});
                    ship.bindPopup(`<div style="background:#1e1e1e;color:#fff;padding:10px;border-radius:6px;"><h3 style="color:#ff8c00;">🚢 Cargo Ship</h3><p><strong>Route:</strong> ${{route.name.split(' ')[0]}}</p><p><strong>Speed:</strong> ${{Math.floor(Math.random()*10+15)}} knots</p></div>`);
                    liveShipsLayer.addLayer(ship);
                }}
            }});
            map.addLayer(liveShipsLayer);
        }}

        map.on('click', function(e) {{
            if (deleteMode) return;
            var title = prompt("Enter marker name:");
            if (title && title.trim()) {{
                var type = window.currentMarkerType || "Ship";
                if (addMarker(e.latlng.lat, e.latlng.lng, title.trim(), type)) {{
                    updateHeatmap();
                    try {{
                        var clickMarkers = JSON.parse(localStorage.getItem('clickMarkers') || '[]');
                        clickMarkers.push({{lat: e.latlng.lat, lng: e.latlng.lng, title: title.trim(), type: type, timestamp: Date.now()}});
                        localStorage.setItem('clickMarkers', JSON.stringify(clickMarkers));
                    }} catch(err) {{}}
                }}
            }}
        }});

        function updateHeatmap() {{
            if (heatLayer) map.removeLayer(heatLayer);
            if (heatmapData.length > 0) {{
                heatLayer = L.heatLayer(heatmapData, {{radius: 25, blur: 15, gradient: {{0.0:'#000080', 0.5:'#00ff80', 1.0:'#ff0000'}}}});
            }}
        }}

        window.addMarkerFromQt = function(lat, lng, title, type) {{
            var result = addMarker(lat, lng, title, type);
            if (result) updateHeatmap();
            return result;
        }};

        window.setCurrentMarkerType = function(type) {{ window.currentMarkerType = type; }};

        window.clearAllMarkers = function() {{
            markerCluster.clearLayers();
            markers = [];
            markerObjects = [];
            heatmapData = [];
            updateHeatmap();
        }};

        window.toggleHeatmap = function() {{
            if (map.hasLayer(heatLayer)) map.removeLayer(heatLayer);
            else if (heatLayer) map.addLayer(heatLayer);
        }};

        window.toggleTradeRoutes = function() {{
            if (map.hasLayer(routesLayer)) map.removeLayer(routesLayer);
            else createOceanRoutes();
        }};

        window.toggleLiveShips = function() {{
            if (map.hasLayer(liveShipsLayer)) map.removeLayer(liveShipsLayer);
            else createLiveShips();
        }};

        window.setDeleteMode = function(mode) {{ deleteMode = mode; }};

        window.getClickMarkers = function() {{
            try {{
                var clickMarkers = JSON.parse(localStorage.getItem('clickMarkers') || '[]');
                localStorage.removeItem('clickMarkers');
                return clickMarkers;
            }} catch(e) {{ return []; }}
        }};

        window.currentMarkerType = "Ship";
        createOceanRoutes();
        createLiveShips();
        updateHeatmap();
    </script>
</body>
</html>"""

                html_path = "maritime_map.html"
                with open(html_path, 'w', encoding='utf-8') as f:
                    f.write(html_content)

                self.web_view.loadFinished.connect(self.on_loaded)
                self.web_view.load(QUrl.fromLocalFile(os.path.abspath(html_path)))
                logger.info("Map created successfully")

            except Exception as e:
                logger.error(f"Map creation failed: {e}", exc_info=True)

    def on_loaded(self, success):
        """Map loaded callback"""
        if success:
            logger.info("Map loaded successfully")
            js_code = f"if(window.setCurrentMarkerType) window.setCurrentMarkerType('{self.selected_marker_type}');"
            self.web_view.page().runJavaScript(js_code)
            self.write_status("ready")
        else:
            logger.error("Map load failed")
            self.write_status("error")

    def start_file_watcher(self):
        """Watch for file changes"""
        self.timer = QTimer()
        self.timer.timeout.connect(self.check_files)
        self.timer.start(500)

    def check_files(self):
        """Check for file updates"""
        try:
            if os.path.exists(self.markers_file):
                mtime = os.path.getmtime(self.markers_file)
                if mtime > self.last_modified:
                    self.last_modified = mtime
                    self.load_markers()

            self.check_click_markers()

            if os.path.exists(self.commands_file):
                with open(self.commands_file, 'r') as f:
                    commands = json.load(f)
                for cmd in commands.get('commands', []):
                    self.execute_command(cmd)
                with open(self.commands_file, 'w') as f:
                    json.dump({'commands': []}, f)
        except Exception as e:
            logger.error(f"File check error: {e}", exc_info=True)

    def check_click_markers(self):
        """Check for click-added markers"""
        try:
            self.web_view.page().runJavaScript("if(window.getClickMarkers) window.getClickMarkers(); else [];", self.process_click_markers)
        except Exception as e:
            logger.error(f"Click marker check failed: {e}", exc_info=True)

    def process_click_markers(self, click_markers):
        """Process click markers"""
        if click_markers and len(click_markers) > 0:
            existing = self.load_markers_data()
            added = 0
            for marker in click_markers:
                is_dup = False
                for ex in existing:
                    if abs(ex['lat'] - marker['lat']) < 0.0005 and abs(ex['lng'] - marker['lng']) < 0.0005:
                        is_dup = True
                        break
                if not is_dup:
                    existing.append({'lat': marker['lat'], 'lng': marker['lng'], 'title': marker['title'], 'type': marker['type']})
                    added += 1
            if added > 0:
                with open(self.markers_file, 'w') as f:
                    json.dump(existing, f, indent=2)
                logger.info(f"Added {added} new markers")

    def load_markers(self):
        """Load markers from file"""
        try:
            if os.path.exists(self.markers_file):
                with open(self.markers_file, 'r') as f:
                    data = json.load(f)
                self.web_view.page().runJavaScript("if(window.clearAllMarkers) window.clearAllMarkers();")
                for marker in data:
                    js = f"if(window.addMarkerFromQt) window.addMarkerFromQt({marker['lat']}, {marker['lng']}, '{marker['title']}', '{marker['type']}');"
                    self.web_view.page().runJavaScript(js)
                self.markers_data = data
                logger.info(f"Loaded {len(data)} markers")
                return data
        except Exception as e:
            logger.error(f"Load markers failed: {e}", exc_info=True)
        return []

    def load_markers_data(self):
        """Load markers data"""
        try:
            if os.path.exists(self.markers_file):
                with open(self.markers_file, 'r') as f:
                    return json.load(f)
        except:
            pass
        return []

    def execute_command(self, cmd):
        """Execute command"""
        try:
            if cmd == 'toggle_routes':
                self.web_view.page().runJavaScript("if(window.toggleTradeRoutes) window.toggleTradeRoutes();")
            elif cmd == 'toggle_ships':
                self.web_view.page().runJavaScript("if(window.toggleLiveShips) window.toggleLiveShips();")
            elif cmd == 'clear_all':
                self.web_view.page().runJavaScript("if(window.clearAllMarkers) window.clearAllMarkers();")
                with open(self.markers_file, 'w') as f:
                    json.dump([], f)
                self.markers_data = []
                self.existing_markers.clear()
            elif cmd.startswith('set_marker_type:'):
                marker_type = cmd.split(':', 1)[1]
                self.web_view.page().runJavaScript(f"if(window.setCurrentMarkerType) window.setCurrentMarkerType('{marker_type}');")
                self.selected_marker_type = marker_type
        except Exception as e:
            logger.error(f"Command execution failed: {e}", exc_info=True)

    def write_status(self, status):
        """Write status"""
        try:
            with open(self.status_file, 'w') as f:
                json.dump({'status': status, 'timestamp': time.time()}, f)
        except Exception as e:
            logger.error(f"Status write failed: {e}", exc_info=True)

    def closeEvent(self, event):
        """Handle close event"""
        self.close_application()
        event.accept()

    def close_application(self):
        """Close application"""
        try:
            logger.info("Closing maritime map")
            self.write_status("closed")
            if hasattr(self, 'timer'):
                self.timer.stop()
            for f in ["maritime_map.html", self.commands_file, self.status_file]:
                if os.path.exists(f):
                    os.remove(f)
            self.close()
            QApplication.quit()
            sys.exit(0)
        except Exception as e:
            logger.error(f"Close error: {e}", exc_info=True)
            sys.exit(1)


def main():
    """Main function"""
    logger.info("Starting maritime map")
    app = QApplication(sys.argv)
    window = MaritimeMapWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()