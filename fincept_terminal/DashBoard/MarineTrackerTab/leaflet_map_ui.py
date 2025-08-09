"""
Leaflet Map Ui module for Fincept Terminal
Updated to use centralized logging system with automatic class detection
"""

# leaflet_map_ui.py - Standalone PyQt Maritime Map Process
import sys
import json
import os
import time
from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QHBoxLayout, QWidget, QPushButton, QLabel, QLineEdit, QComboBox
from fincept_terminal.Utils.Logging.logger import logger, operation, monitor_performance
from PyQt5.QtWebEngineWidgets import QWebEngineView
from PyQt5.QtCore import Qt, QUrl, QTimer


class MaritimeMapWindow(QMainWindow):
    """Standalone Maritime Map Window with Full Features"""

    def __init__(self):
        super().__init__()
        self.markers_file = "maritime_markers.json"
        self.commands_file = "map_commands.json"
        self.status_file = "map_status.json"

        self.markers_data = self.load_markers()
        self.existing_markers = set()
        self.selected_marker_type = "Ship"
        self.last_modified = 0

        # Initialize existing markers set
        for marker in self.markers_data:
            key = f"{marker['lat']:.6f}_{marker['lng']:.6f}"
            self.existing_markers.add(key)

        logger.info("Maritime map window initialized")

        self.init_ui()
        self.create_map()
        self.start_file_watcher()

    @monitor_performance
    def init_ui(self):
        """Initialize UI with enhanced controls"""
        with operation("init_ui"):
            self.setWindowTitle(" FINCEPT MARITIME MAP")
            self.setGeometry(100, 100, 600, 600)
            self.setWindowFlags(Qt.WindowStaysOnTopHint)

            central_widget = QWidget()
            self.setCentralWidget(central_widget)
            main_layout = QVBoxLayout(central_widget)
            main_layout.setContentsMargins(2, 2, 2, 2)
            main_layout.setSpacing(2)

            # Enhanced control panel
            control_widget = QWidget()
            control_widget.setMaximumHeight(50)
            control_layout = QHBoxLayout(control_widget)

            control_layout.addWidget(QLabel(" MARITIME:"))

            self.marker_title = QLineEdit()
            self.marker_title.setPlaceholderText("Marker Title")
            self.marker_title.setMaximumWidth(120)
            control_layout.addWidget(self.marker_title)

            self.marker_type = QComboBox()
            self.marker_type.addItems(["Ship", "Port", "Industry", "House", "Bank", "Exchange", "Warehouse", "Airport"])
            self.marker_type.currentTextChanged.connect(self.on_marker_type_changed)
            self.marker_type.setMaximumWidth(80)
            control_layout.addWidget(self.marker_type)

            # Action buttons
            clear_btn = QPushButton("CLEAR")
            clear_btn.clicked.connect(self.clear_all_markers)
            clear_btn.setMaximumWidth(60)
            control_layout.addWidget(clear_btn)

            close_btn = QPushButton("CLOSE")
            close_btn.clicked.connect(self.close_application)
            close_btn.setMaximumWidth(60)
            close_btn.setStyleSheet(
                "QPushButton { background-color: #ff4444; } QPushButton:hover { background-color: #ff6666; }")
            control_layout.addWidget(close_btn)

            delete_mode_btn = QPushButton("DELETE")
            delete_mode_btn.clicked.connect(self.toggle_delete_mode)
            delete_mode_btn.setObjectName("delete_mode_btn")
            delete_mode_btn.setMaximumWidth(60)
            control_layout.addWidget(delete_mode_btn)

            heatmap_btn = QPushButton("HEAT")
            heatmap_btn.clicked.connect(self.toggle_heatmap)
            heatmap_btn.setMaximumWidth(50)
            control_layout.addWidget(heatmap_btn)

            routes_btn = QPushButton("ROUTES")
            routes_btn.clicked.connect(self.toggle_trade_routes)
            routes_btn.setMaximumWidth(60)
            control_layout.addWidget(routes_btn)

            ships_btn = QPushButton("SHIPS")
            ships_btn.clicked.connect(self.toggle_live_ships)
            ships_btn.setMaximumWidth(50)
            control_layout.addWidget(ships_btn)

            control_layout.addStretch()
            main_layout.addWidget(control_widget)

            # Web view
            self.web_view = QWebEngineView()
            main_layout.addWidget(self.web_view)

            # Status bar
            status_widget = QWidget()
            status_widget.setMaximumHeight(30)
            status_layout = QHBoxLayout(status_widget)

            self.status_label = QLabel(" LOADING MARITIME MAP...")
            self.status_label.setStyleSheet("color: #00ff00; font-weight: bold; font-size: 10px;")
            status_layout.addWidget(self.status_label)

            status_layout.addStretch()

            self.marker_count_label = QLabel("Markers: 0")
            self.marker_count_label.setStyleSheet("color: #ff8c00; font-weight: bold; font-size: 10px;")
            status_layout.addWidget(self.marker_count_label)

            main_layout.addWidget(status_widget)

            # Enhanced dark theme
            self.setStyleSheet("""
                QMainWindow { background-color: #0a0a0a; color: #ffffff; }
                QWidget { background-color: #0a0a0a; color: #ffffff; font-family: 'Arial', sans-serif; font-size: 10px; }
                QPushButton { 
                    background-color: #ff8c00; color: #000000; border: none; padding: 6px 12px; 
                    border-radius: 4px; font-weight: bold; font-size: 9px;
                }
                QPushButton:hover { background-color: #ffa500; }
                QPushButton:pressed { background-color: #ff7700; }
                QPushButton#delete_mode_btn { background-color: #ff4444; }
                QPushButton#delete_mode_btn:hover { background-color: #ff6666; }
                QLineEdit, QComboBox { 
                    background-color: #2d2d2d; color: #ffffff; border: 2px solid #ff8c00; 
                    padding: 6px; border-radius: 4px; font-size: 9px;
                }
                QLabel { color: #ffffff; font-weight: bold; }
            """)

            self.delete_mode = False
            logger.info("UI initialization completed")

    @monitor_performance
    def create_map(self):
        """Create enhanced Leaflet map based on original working code"""
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
        .ocean-route {{ stroke: #00bfff; stroke-width: 4; stroke-opacity: 0.7; stroke-dasharray: 10,5; }}
        .route-animation {{ animation: routeFlow 3s linear infinite; }}
        @keyframes routeFlow {{ 
            0% {{ stroke-dashoffset: 0; }} 
            100% {{ stroke-dashoffset: -30; }} 
        }}
        .ship-marker {{ 
            animation: shipBob 2s ease-in-out infinite; 
            z-index: 1000 !important;
        }}
        @keyframes shipBob {{ 0%, 100% {{ transform: translateY(0px); }} 50% {{ transform: translateY(-3px); }} }}
        .custom-marker {{
            z-index: 1000 !important;
        }}
        .delete-cursor {{ cursor: crosshair !important; }}
        .leaflet-marker-icon {{ z-index: 1000 !important; }}
        .leaflet-marker-pane {{ z-index: 600 !important; }}
    </style>
</head>
<body>
    <div id="map"></div>

    <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
    <script src="https://unpkg.com/leaflet.markercluster@1.4.1/dist/leaflet.markercluster.js"></script>
    <script src="https://unpkg.com/leaflet.heat@0.2.0/dist/leaflet-heat.js"></script>

    <script>
        console.log('Initializing maritime map...');

        // Initialize map
        var map = L.map('map').setView([15.0, 75.0], 4);

        // SATELLITE LAYER - FIXED
        var satelliteLayer = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{{z}}/{{y}}/{{x}}', {{
            attribution: 'FINCEPT MARITIME TERMINAL'
        }});
        satelliteLayer.addTo(map);
        console.log(' Satellite layer added');

        // Variables
        var markerCluster = L.markerClusterGroup();
        map.addLayer(markerCluster);
        var existingMarkers = new Set();
        var markers = [];
        var markerObjects = [];
        var heatmapData = [];
        var heatLayer = null;
        var routesLayer = null;
        var liveShipsLayer = null;
        var deleteMode = false;

        // Load existing markers
        var markersData = {markers_json};
        console.log('Loading', markersData.length, 'existing markers');

        // Custom marker icons using CSS and Unicode symbols
        function createCustomIcon(type, size = 24) {{
            var iconConfig = {{
                'Ship': {{ symbol: '', color: '#1e90ff', bg: '#ffffff' }},
                'Port': {{ symbol: '', color: '#8b4513', bg: '#ffffff' }},
                'Industry': {{ symbol: '', color: '#ff4500', bg: '#ffffff' }},
                'House': {{ symbol: '', color: '#32cd32', bg: '#ffffff' }},
                'Bank': {{ symbol: '', color: '#ffd700', bg: '#ffffff' }},
                'Exchange': {{ symbol: '', color: '#ff1493', bg: '#ffffff' }},
                'Warehouse': {{ symbol: '', color: '#8a2be2', bg: '#ffffff' }},
                'Airport': {{ symbol: '', color: '#00ced1', bg: '#ffffff' }}
            }};

            var config = iconConfig[type] || iconConfig['Ship'];

            return L.divIcon({{
                html: `
                    <div style="
                        width: ${{size}}px; 
                        height: ${{size}}px; 
                        border-radius: 50%; 
                        background: ${{config.bg}}; 
                        border: 3px solid ${{config.color}};
                        display: flex; 
                        align-items: center; 
                        justify-content: center; 
                        font-size: ${{size-8}}px;
                        box-shadow: 0 2px 5px rgba(0,0,0,0.3);
                        cursor: pointer;
                    ">
                        ${{config.symbol}}
                    </div>
                `,
                iconSize: [size, size],
                iconAnchor: [size/2, size/2],
                popupAnchor: [0, -size/2],
                className: type === 'Ship' ? 'ship-marker' : 'custom-marker'
            }});
        }}

        // Ocean trade routes
        var oceanRoutes = [
            {{
                name: "Mumbai-Shanghai via Arabian Sea",
                coords: [
                    [19.0760, 72.8777], [18.5, 68.0], [16.0, 60.0], [12.0, 55.0], [8.0, 50.0],
                    [5.0, 75.0], [10.0, 85.0], [15.0, 95.0], [20.0, 105.0], [25.0, 115.0], [31.2304, 121.4737]
                ],
                value: "156.3B USD"
            }},
            {{
                name: "Kolkata-Hong Kong via Bay of Bengal",
                coords: [
                    [22.5726, 88.3639], [20.0, 90.0], [18.0, 95.0], [15.0, 100.0], [18.0, 110.0], [22.3193, 114.1694]
                ],
                value: "45.2B USD"
            }},
            {{
                name: "Chennai-Guangzhou via Indian Ocean",
                coords: [
                    [13.0827, 80.2707], [10.0, 85.0], [8.0, 90.0], [5.0, 95.0], [3.0, 100.0],
                    [8.0, 105.0], [15.0, 110.0], [23.1291, 113.2644]
                ],
                value: "32.8B USD"
            }}
        ];

        // Add marker function with DUPLICATE PREVENTION
        function addMarker(lat, lng, title, type) {{
            // Create unique key for location checking (precision to 4 decimal places for ~11m accuracy)
            var key = `${{lat.toFixed(4)}}_${{lng.toFixed(4)}}`;

            // Check for duplicates with tolerance
            var isDuplicate = false;
            for (let marker of markers) {{
                var existingKey = `${{marker.lat.toFixed(4)}}_${{marker.lng.toFixed(4)}}`;
                if (existingKey === key) {{
                    console.log(' DUPLICATE: Marker already exists near this location');
                    alert(' A marker already exists at this location!\\nExisting: ' + marker.title);
                    return false;
                }}
            }}

            console.log(' Creating new marker:', type, 'at', lat, lng, 'Key:', key);
            existingMarkers.add(key);

            var marker = L.marker([lat, lng], {{ 
                icon: createCustomIcon(type, 28),
                zIndexOffset: 1000
            }});

            var popupContent = `
                <div style="background:#1e1e1e;color:#fff;padding:12px;border-radius:8px;min-width:220px;border:2px solid #ff8c00;">
                    <h3 style="color:#ff8c00;margin:0 0 10px 0;font-size:16px;">${{title}}</h3>
                    <p style="margin:5px 0;"><strong>Type:</strong> ${{type}}</p>
                    <p style="margin:5px 0;"><strong>Location:</strong> ${{lat.toFixed(4)}}, ${{lng.toFixed(4)}}</p>
                    <button onclick="deleteMarker('${{key}}')" 
                            style="background:#ff4444;color:#fff;border:none;padding:8px 12px;border-radius:4px;cursor:pointer;margin-top:10px;font-weight:bold;">
                         Delete Marker
                    </button>
                </div>
            `;

            marker.bindPopup(popupContent);

            // Add click handler for delete mode
            marker.on('click', function(e) {{
                if (deleteMode) {{
                    deleteMarker(key);
                    e.originalEvent.stopPropagation();
                }}
            }});

            // Add to cluster
            markerCluster.addLayer(marker);

            // Store marker data with precise coordinates
            markers.push({{lat: lat, lng: lng, title: title, type: type, key: key}});
            markerObjects.push(marker);
            heatmapData.push([lat, lng, 1]);

            console.log(' Marker added successfully:', title, 'Total markers:', markers.length);
            updateMarkerCount();
            return true;
        }}

        // Delete marker function
        function deleteMarker(key) {{
            var markerIndex = markers.findIndex(m => m.key === key);
            if (markerIndex >= 0) {{
                markerCluster.removeLayer(markerObjects[markerIndex]);
                markers.splice(markerIndex, 1);
                markerObjects.splice(markerIndex, 1);
                heatmapData.splice(markerIndex, 1);
                existingMarkers.delete(key);
                updateHeatmap();
                updateMarkerCount();
                console.log(' Marker deleted:', key);
            }}
        }}

        // Load existing markers
        markersData.forEach(m => {{
            addMarker(m.lat, m.lng, m.title, m.type);
        }});

        // Create ocean trade routes
        function createOceanRoutes() {{
            if (routesLayer) map.removeLayer(routesLayer);

            routesLayer = L.layerGroup();

            oceanRoutes.forEach(route => {{
                var polyline = L.polyline(route.coords, {{
                    color: '#00bfff',
                    weight: 5,
                    opacity: 0.8,
                    className: 'route-animation ocean-route'
                }});

                polyline.bindPopup(`
                    <div style="background:#1e1e1e;color:#fff;padding:10px;border-radius:8px;">
                        <h3 style="color:#00bfff;margin:0 0 10px 0;">${{route.name}}</h3>
                        <p><strong>Trade Value:</strong> ${{route.value}}</p>
                        <p><strong>Route Type:</strong> Ocean Maritime</p>
                    </div>
                `);

                routesLayer.addLayer(polyline);
            }});

            map.addLayer(routesLayer);
        }}

        // Live ships simulation
        function createLiveShips() {{
            if (liveShipsLayer) map.removeLayer(liveShipsLayer);

            liveShipsLayer = L.layerGroup();

            // Simulate ships along routes
            oceanRoutes.forEach((route, routeIndex) => {{
                for (var i = 1; i < route.coords.length - 1; i += 2) {{
                    var shipIcon = createCustomIcon('Ship', 32);
                    var ship = L.marker(route.coords[i], {{
                        icon: shipIcon,
                        zIndexOffset: 1500
                    }});

                    ship.bindPopup(`
                        <div style="background:#1e1e1e;color:#fff;padding:10px;border-radius:8px;">
                            <h3 style="color:#ff8c00;margin:0 0 10px 0;"> Live Cargo Ship</h3>
                            <p><strong>Route:</strong> ${{route.name.split(' via ')[0]}}</p>
                            <p><strong>Status:</strong> In Transit</p>
                            <p><strong>Speed:</strong> ${{Math.floor(Math.random() * 10 + 15)}} knots</p>
                        </div>
                    `);

                    liveShipsLayer.addLayer(ship);
                }}
            }});

            map.addLayer(liveShipsLayer);
        }}

        // ENHANCED MAP CLICK HANDLER with duplicate prevention
        map.on('click', function(e) {{
            if (deleteMode) return;

            console.log(' Map clicked at:', e.latlng.lat, e.latlng.lng);

            // Check for nearby markers BEFORE showing prompt
            var clickLat = e.latlng.lat;
            var clickLng = e.latlng.lng;
            var nearbyMarker = null;

            // Check if there's already a marker within ~50 meters (0.0005 degrees)
            for (let marker of markers) {{
                var distance = Math.sqrt(
                    Math.pow(marker.lat - clickLat, 2) + 
                    Math.pow(marker.lng - clickLng, 2)
                );
                if (distance < 0.0005) {{ // ~50 meter tolerance
                    nearbyMarker = marker;
                    break;
                }}
            }}

            if (nearbyMarker) {{
                alert(' Duplicate Location!\\n\\nA marker already exists nearby:\\n"' + nearbyMarker.title + '" (' + nearbyMarker.type + ')\\n\\nPlease click elsewhere to add a new marker.');
                console.log(' Blocked duplicate marker near:', nearbyMarker.title);
                return;
            }}

            var title = prompt(" Enter marker name (or Cancel to skip):");

            // Only add marker if user provides a valid name
            if (title && title.trim() !== '' && title.trim().toLowerCase() !== 'null') {{
                title = title.trim();
                var type = window.currentMarkerType || "Ship";

                console.log(' Adding marker:', title, 'Type:', type);

                if (addMarker(clickLat, clickLng, title, type)) {{
                    updateHeatmap();

                    // Save to localStorage for Qt to pick up
                    try {{
                        var clickMarkers = JSON.parse(localStorage.getItem('clickMarkers') || '[]');
                        clickMarkers.push({{
                            lat: clickLat,
                            lng: clickLng,
                            title: title,
                            type: type,
                            timestamp: Date.now()
                        }});
                        localStorage.setItem('clickMarkers', JSON.stringify(clickMarkers));
                        console.log(' Saved to localStorage for Qt');
                    }} catch(e) {{
                        console.error(' LocalStorage error:', e);
                    }}
                }} else {{
                    console.log(' Failed to add marker');
                }}
            }} else {{
                console.log(' Marker creation cancelled - no valid name provided');
            }}
        }});

        // Update heatmap
        function updateHeatmap() {{
            if (heatLayer) map.removeLayer(heatLayer);
            if (heatmapData.length > 0) {{
                heatLayer = L.heatLayer(heatmapData, {{
                    radius: 30,
                    blur: 20,
                    gradient: {{0.0:'#000080', 0.3:'#0080ff', 0.6:'#00ff80', 0.8:'#ff8000', 1.0:'#ff0000'}}
                }});
            }}
        }}

        // Update marker count
        function updateMarkerCount() {{
            console.log(' Current marker count:', markers.length);
        }}

        // Global functions for Qt integration
        window.addMarkerFromQt = function(lat, lng, title, type) {{
            console.log(' Adding marker from Qt:', lat, lng, title, type);
            var result = addMarker(lat, lng, title, type);
            if (result) {{
                updateHeatmap();
                updateMarkerCount();
            }}
            return result;
        }};

        window.setCurrentMarkerType = function(type) {{
            window.currentMarkerType = type;
            console.log(' Marker type set to:', type);
        }};

        window.clearAllMarkers = function() {{
            console.log(' Clearing all markers');
            markerCluster.clearLayers();
            markers = [];
            markerObjects = [];
            heatmapData = [];
            existingMarkers.clear();
            updateHeatmap();
            updateMarkerCount();
        }};

        window.toggleHeatmap = function() {{
            if (map.hasLayer(heatLayer)) {{
                map.removeLayer(heatLayer);
                console.log(' Heatmap hidden');
            }} else if (heatLayer) {{
                map.addLayer(heatLayer);
                console.log(' Heatmap shown');
            }}
        }};

        window.toggleTradeRoutes = function() {{
            if (map.hasLayer(routesLayer)) {{
                map.removeLayer(routesLayer);
                console.log(' Routes hidden');
            }} else {{
                createOceanRoutes();
                console.log(' Routes shown');
            }}
        }};

        window.toggleLiveShips = function() {{
            if (map.hasLayer(liveShipsLayer)) {{
                map.removeLayer(liveShipsLayer);
                console.log(' Ships hidden');
            }} else {{
                createLiveShips();
                console.log(' Ships shown');
            }}
        }};

        window.setDeleteMode = function(mode) {{
            deleteMode = mode;
            if (mode) {{
                map.getContainer().classList.add('delete-cursor');
                console.log(' Delete mode ON');
            }} else {{
                map.getContainer().classList.remove('delete-cursor');
                console.log(' Delete mode OFF');
            }}
        }};

        // Function to get click markers for Qt
        window.getClickMarkers = function() {{
            try {{
                var clickMarkers = JSON.parse(localStorage.getItem('clickMarkers') || '[]');
                localStorage.removeItem('clickMarkers');
                console.log(' Retrieved', clickMarkers.length, 'click markers for Qt');
                return clickMarkers;
            }} catch(e) {{
                console.error(' Error getting click markers:', e);
                return [];
            }}
        }};

        // Initialize
        window.currentMarkerType = "Ship";
        createOceanRoutes();
        createLiveShips();
        updateHeatmap();
        updateMarkerCount();

        console.log(' Maritime map loaded with', markers.length, 'markers');
        console.log(' Satellite view active, click map to add markers');
    </script>
</body>
</html>
                """

                # Save and load HTML
                html_path = "maritime_map.html"
                with open(html_path, 'w', encoding='utf-8') as f:
                    f.write(html_content)

                self.web_view.loadFinished.connect(self.on_loaded)
                self.web_view.load(QUrl.fromLocalFile(os.path.abspath(html_path)))
                logger.info(f"Map HTML created and loading from: {html_path}")

            except Exception as e:
                logger.error(f"Failed to create map: {e}", exc_info=True)
                raise

    def on_marker_type_changed(self, marker_type):
        """Handle marker type change"""
        with operation("marker_type_change", marker_type=marker_type):
            self.selected_marker_type = marker_type
            js_code = f"if(window.setCurrentMarkerType) window.setCurrentMarkerType('{marker_type}');"
            self.web_view.page().runJavaScript(js_code)
            self.status_label.setText(f" Selected: {marker_type}")
            logger.debug(f"Marker type changed to: {marker_type}")

    def toggle_delete_mode(self):
        """Toggle delete mode"""
        with operation("toggle_delete_mode"):
            self.delete_mode = not self.delete_mode
            if self.delete_mode:
                self.status_label.setText(" DELETE MODE | Click markers to remove")
                self.web_view.page().runJavaScript("if(window.setDeleteMode) window.setDeleteMode(true);")
                logger.info("Delete mode enabled")
            else:
                self.status_label.setText(" ADD MODE | Click map to add markers")
                self.web_view.page().runJavaScript("if(window.setDeleteMode) window.setDeleteMode(false);")
                logger.info("Delete mode disabled")

    def on_loaded(self, success):
        """Map loaded callback"""
        with operation("map_load", success=success):
            if success:
                logger.info("Map loaded successfully")
                self.status_label.setText(" MARITIME MAP READY")

                # Set initial marker type
                js_code = f"if(window.setCurrentMarkerType) window.setCurrentMarkerType('{self.selected_marker_type}');"
                self.web_view.page().runJavaScript(js_code)

                self.update_marker_count()
                self.write_status("ready")

                QTimer.singleShot(2000, lambda: self.status_label.setText(" Ready | Click map to add markers"))
            else:
                logger.error("Map failed to load")
                self.status_label.setText(" MAP LOAD FAILED")
                self.write_status("error")

    def start_file_watcher(self):
        """Watch for file changes"""
        logger.debug("Starting file watcher timer")
        self.timer = QTimer()
        self.timer.timeout.connect(self.check_files)
        self.timer.start(500)

    def check_files(self):
        """Check for file updates"""
        try:
            # Check markers file
            if os.path.exists(self.markers_file):
                mtime = os.path.getmtime(self.markers_file)
                if mtime > self.last_modified:
                    self.last_modified = mtime
                    logger.debug("Markers file was modified, reloading")
                    self.load_markers()

            # Check for click-added markers
            self.check_click_markers()

            # Check commands file
            if os.path.exists(self.commands_file):
                with open(self.commands_file, 'r') as f:
                    commands = json.load(f)

                for cmd in commands.get('commands', []):
                    self.execute_command(cmd)

                # Clear commands
                with open(self.commands_file, 'w') as f:
                    json.dump({'commands': []}, f)

        except Exception as e:
            logger.error(f"Error checking files: {e}", exc_info=True)

    def check_click_markers(self):
        """Check for markers added by clicking"""
        try:
            js_code = "if(window.getClickMarkers) window.getClickMarkers(); else [];"
            self.web_view.page().runJavaScript(js_code, self.process_click_markers)
        except Exception as e:
            logger.error(f"Failed to check click markers: {e}", exc_info=True)

    def process_click_markers(self, click_markers):
        """Process click markers from JavaScript with duplicate prevention"""
        with operation("process_click_markers", count=len(click_markers) if click_markers else 0):
            try:
                if click_markers and len(click_markers) > 0:
                    # Load existing markers
                    existing_markers = self.load_markers_data()

                    added_count = 0

                    # Add new click markers with duplicate checking
                    for marker in click_markers:
                        # Check for duplicates in existing markers
                        is_duplicate = False
                        for existing in existing_markers:
                            # Check if coordinates are too close (within ~50 meters)
                            lat_diff = abs(existing['lat'] - marker['lat'])
                            lng_diff = abs(existing['lng'] - marker['lng'])
                            if lat_diff < 0.0005 and lng_diff < 0.0005:
                                is_duplicate = True
                                logger.debug(f"Skipping duplicate marker: {marker['title']} (near {existing['title']})")
                                break

                        if not is_duplicate:
                            marker_data = {
                                'lat': marker['lat'],
                                'lng': marker['lng'],
                                'title': marker['title'],
                                'type': marker['type']
                            }
                            existing_markers.append(marker_data)
                            added_count += 1
                            logger.info(f"Added click marker: {marker['title']}")

                    if added_count > 0:
                        # Save updated markers
                        with open(self.markers_file, 'w') as f:
                            json.dump(existing_markers, f, indent=2)

                        logger.info(f"Saved {added_count} new markers to file (skipped {len(click_markers) - added_count} duplicates)")
                    else:
                        logger.debug("No new markers added - all were duplicates")

            except Exception as e:
                logger.error(f"Failed to process click markers: {e}", exc_info=True)

    def closeEvent(self, event):
        """Handle window close event"""
        try:
            logger.info("Window close event triggered")
            self.close_application()
        except Exception as e:
            logger.error(f"Error in close event: {e}", exc_info=True)
            # Force close if there's an error
            event.accept()
            sys.exit(0)

    def load_markers_data(self):
        """Load markers data from file"""
        try:
            if os.path.exists(self.markers_file):
                with open(self.markers_file, 'r') as f:
                    data = json.load(f)
                    logger.debug(f"Loaded {len(data)} markers from data file")
                    return data
        except Exception as e:
            logger.error(f"Failed to load markers data: {e}", exc_info=True)
        return []

    def load_markers(self):
        """Load markers from file"""
        with operation("load_markers"):
            try:
                if os.path.exists(self.markers_file):
                    with open(self.markers_file, 'r') as f:
                        data = json.load(f)

                    # Clear existing markers on map
                    self.web_view.page().runJavaScript("if(window.clearAllMarkers) window.clearAllMarkers();")

                    # Add all markers to map
                    for marker in data:
                        js = f"if(window.addMarkerFromQt) window.addMarkerFromQt({marker['lat']}, {marker['lng']}, '{marker['title']}', '{marker['type']}');"
                        self.web_view.page().runJavaScript(js)

                    self.markers_data = data
                    self.update_marker_count()
                    logger.info(f"Loaded {len(data)} markers from file")
                    return data
                else:
                    logger.debug("No markers file found, starting with empty list")
                    return []
            except Exception as e:
                logger.error(f"Failed to load markers: {e}", exc_info=True)
                return []

    def execute_command(self, cmd):
        """Execute map command"""
        with operation("execute_command", command=cmd):
            try:
                if cmd == 'toggle_routes':
                    self.web_view.page().runJavaScript("if(window.toggleTradeRoutes) window.toggleTradeRoutes();")
                    logger.debug("Toggled trade routes")
                elif cmd == 'toggle_ships':
                    self.web_view.page().runJavaScript("if(window.toggleLiveShips) window.toggleLiveShips();")
                    logger.debug("Toggled live ships")
                elif cmd == 'clear_all':
                    self.web_view.page().runJavaScript("if(window.clearAllMarkers) window.clearAllMarkers();")
                    # Also clear the file
                    with open(self.markers_file, 'w') as f:
                        json.dump([], f)
                    self.markers_data = []
                    self.existing_markers.clear()
                    logger.info("Cleared all markers")
                elif cmd.startswith('set_marker_type:'):
                    marker_type = cmd.split(':', 1)[1]
                    js_code = f"if(window.setCurrentMarkerType) window.setCurrentMarkerType('{marker_type}');"
                    self.web_view.page().runJavaScript(js_code)
                    self.selected_marker_type = marker_type
                    logger.info(f"Set marker type to: {marker_type}")
                else:
                    logger.warning(f"Unknown command: {cmd}")
            except Exception as e:
                logger.error(f"Failed to execute command '{cmd}': {e}", exc_info=True)

    def clear_all_markers(self):
        """Clear all markers"""
        with operation("clear_all_markers"):
            try:
                self.markers_data = []
                self.existing_markers.clear()

                # Save empty file
                with open(self.markers_file, 'w') as f:
                    json.dump([], f)

                # Clear on map
                self.web_view.page().runJavaScript("if(window.clearAllMarkers) window.clearAllMarkers();")
                self.update_marker_count()
                self.status_label.setText(" CLEARED | All markers removed")
                logger.info("All markers cleared successfully")
            except Exception as e:
                logger.error(f"Failed to clear markers: {e}", exc_info=True)

    def toggle_heatmap(self):
        """Toggle heatmap"""
        try:
            self.web_view.page().runJavaScript("if(window.toggleHeatmap) window.toggleHeatmap();")
            self.status_label.setText(" HEATMAP | Toggled")
            logger.debug("Toggled heatmap")
        except Exception as e:
            logger.error(f"Failed to toggle heatmap: {e}", exc_info=True)

    def toggle_trade_routes(self):
        """Toggle ocean trade routes"""
        try:
            self.web_view.page().runJavaScript("if(window.toggleTradeRoutes) window.toggleTradeRoutes();")
            self.status_label.setText(" ROUTES | Toggled")
            logger.debug("Toggled trade routes")
        except Exception as e:
            logger.error(f"Failed to toggle routes: {e}", exc_info=True)

    def toggle_live_ships(self):
        """Toggle live ships"""
        try:
            self.web_view.page().runJavaScript("if(window.toggleLiveShips) window.toggleLiveShips();")
            self.status_label.setText(" SHIPS | Toggled")
            logger.debug("Toggled live ships")
        except Exception as e:
            logger.error(f"Failed to toggle ships: {e}", exc_info=True)

    def update_marker_count(self):
        """Update marker count display"""
        try:
            count = len(self.markers_data)
            self.marker_count_label.setText(f"Markers: {count}")
            logger.debug(f"Updated marker count to {count}")
        except Exception as e:
            logger.error(f"Failed to update marker count: {e}", exc_info=True)

    def write_status(self, status):
        """Write status to file"""
        try:
            with open(self.status_file, 'w') as f:
                json.dump({'status': status, 'timestamp': time.time()}, f)
            logger.debug(f"Status written: {status}")
        except Exception as e:
            logger.error(f"Failed to write status: {e}", exc_info=True)

    def close_application(self):
        """Properly close the application"""
        with operation("close_application"):
            try:
                logger.info("Closing maritime map application...")
                self.status_label.setText(" CLOSING...")

                # Write closed status
                self.write_status("closed")

                # Stop file watcher
                if hasattr(self, 'timer'):
                    self.timer.stop()
                    logger.debug("File watcher timer stopped")

                # Clean up temp files
                temp_files = ["maritime_map.html", self.commands_file, self.status_file]
                for file_path in temp_files:
                    try:
                        if os.path.exists(file_path):
                            os.remove(file_path)
                            logger.debug(f"Removed temp file: {file_path}")
                    except Exception as e:
                        logger.warning(f"Could not remove temp file {file_path}: {e}")

                logger.info("Maritime map application closed successfully")

                # Close the window and quit application
                self.close()
                QApplication.quit()
                sys.exit(0)

            except Exception as e:
                logger.error(f"Error during application close: {e}", exc_info=True)
                # Force exit if normal close fails
                sys.exit(1)


def main():
    """Main function"""
    with operation("main"):
        logger.info("Starting maritime map application")
        app = QApplication(sys.argv)
        window = MaritimeMapWindow()
        window.show()
        logger.info("Maritime map window displayed, entering event loop")
        sys.exit(app.exec_())


if __name__ == "__main__":
    main()