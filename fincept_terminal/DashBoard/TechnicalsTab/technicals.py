# import dearpygui.dearpygui as dpg
# import sys
# import threading
# import json
# import random
# from datetime import datetime
# from PyQt5.QtWidgets import QApplication, QMainWindow
# from PyQt5.QtWebEngineWidgets import QWebEngineView
# from PyQt5.QtCore import QUrl, pyqtSignal, QObject
# from PyQt5.QtWebChannel import QWebChannel
# from flask import Flask, render_template_string, jsonify, request
#
# # Flask app setup
# app = Flask(__name__)
#
# # Global markers storage
# markers = [
#     {
#         'id': 1,
#         'lat': 21.1702,
#         'lng': 72.8311,
#         'title': 'Surat',
#         'description': 'Diamond City of India',
#         'type': 'city'
#     }
# ]
#
# # HTML template with Leaflet map
# HTML_TEMPLATE = """
# <!DOCTYPE html>
# <html>
# <head>
#     <title>Interactive World Map</title>
#     <meta charset="utf-8" />
#     <meta name="viewport" content="width=device-width, initial-scale=1.0">
#     <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
#     <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
#     <script src="https://cdnjs.cloudflare.com/ajax/libs/socket.io/4.0.0/socket.io.js"></script>
#     <style>
#         body {
#             margin: 0;
#             padding: 0;
#             font-family: Arial, sans-serif;
#             background: #1a1a1a;
#             color: white;
#         }
#         #map {
#             height: 100vh;
#             width: 100vw;
#         }
#         .map-controls {
#             position: absolute;
#             top: 10px;
#             right: 10px;
#             z-index: 1000;
#             background: rgba(26, 26, 26, 0.9);
#             padding: 15px;
#             border-radius: 8px;
#             box-shadow: 0 4px 15px rgba(0,0,0,0.3);
#             min-width: 200px;
#         }
#         .control-section {
#             margin-bottom: 15px;
#         }
#         .control-title {
#             font-size: 14px;
#             font-weight: bold;
#             margin-bottom: 8px;
#             color: #4CAF50;
#         }
#         .marker-info {
#             background: rgba(42, 42, 42, 0.95);
#             padding: 10px;
#             border-radius: 5px;
#             margin: 5px 0;
#             font-size: 12px;
#         }
#         .marker-count {
#             color: #ffc107;
#             font-weight: bold;
#         }
#         .coordinates {
#             color: #2196f3;
#             font-size: 11px;
#         }
#         .leaflet-popup-content {
#             color: #333 !important;
#         }
#         .custom-popup {
#             background: white;
#             color: #333;
#             padding: 10px;
#             border-radius: 5px;
#             box-shadow: 0 2px 10px rgba(0,0,0,0.3);
#         }
#         .popup-title {
#             font-weight: bold;
#             font-size: 16px;
#             margin-bottom: 5px;
#             color: #2196f3;
#         }
#         .popup-desc {
#             margin-bottom: 5px;
#         }
#         .popup-coords {
#             font-size: 11px;
#             color: #666;
#         }
#     </style>
# </head>
# <body>
#     <div class="map-controls">
#         <div class="control-section">
#             <div class="control-title">🗺️ Map Info</div>
#             <div class="marker-info">
#                 <div>Active Markers: <span class="marker-count" id="marker-count">1</span></div>
#                 <div class="coordinates">Current View: Surat, India</div>
#             </div>
#         </div>
#
#         <div class="control-section">
#             <div class="control-title">📍 Latest Marker</div>
#             <div class="marker-info" id="latest-marker">
#                 <div>📍 Surat</div>
#                 <div class="coordinates">21.1702°N, 72.8311°E</div>
#             </div>
#         </div>
#
#         <div class="control-section">
#             <div class="control-title">🎯 Map Actions</div>
#             <div class="marker-info">
#                 <div>🖱️ Click to add markers</div>
#                 <div>🔍 Zoom: Mouse wheel</div>
#                 <div>📍 Pan: Click and drag</div>
#             </div>
#         </div>
#     </div>
#
#     <div id="map"></div>
#
#     <script>
#         // Initialize map centered on Surat
#         const map = L.map('map').setView([21.1702, 72.8311], 10);
#
#         // Add OpenStreetMap tiles
#         L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
#             attribution: '© OpenStreetMap contributors',
#             maxZoom: 18
#         }).addTo(map);
#
#         // Dark theme tile layer alternative
#         // L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
#         //     attribution: '© OpenStreetMap contributors © CARTO',
#         //     maxZoom: 18
#         // }).addTo(map);
#
#         let markers = [];
#         let markerCount = 0;
#
#         // Custom icon for markers
#         const customIcon = L.divIcon({
#             className: 'custom-marker',
#             html: '<div style="background: #ff4444; width: 20px; height: 20px; border-radius: 50%; border: 3px solid white; box-shadow: 0 2px 5px rgba(0,0,0,0.3);"></div>',
#             iconSize: [20, 20],
#             iconAnchor: [10, 10]
#         });
#
#         const cityIcon = L.divIcon({
#             className: 'city-marker',
#             html: '<div style="background: #4CAF50; width: 25px; height: 25px; border-radius: 50%; border: 3px solid white; box-shadow: 0 2px 5px rgba(0,0,0,0.3);"></div>',
#             iconSize: [25, 25],
#             iconAnchor: [12, 12]
#         });
#
#         // Function to add marker
#         function addMarker(lat, lng, title, description, type = 'custom') {
#             const icon = type === 'city' ? cityIcon : customIcon;
#
#             const marker = L.marker([lat, lng], { icon: icon }).addTo(map);
#
#             const popupContent = `
#                 <div class="custom-popup">
#                     <div class="popup-title">${title}</div>
#                     <div class="popup-desc">${description}</div>
#                     <div class="popup-coords">${lat.toFixed(4)}°N, ${lng.toFixed(4)}°E</div>
#                 </div>
#             `;
#
#             marker.bindPopup(popupContent);
#             markers.push({ marker, lat, lng, title, description, type });
#
#             updateMarkerCount();
#             updateLatestMarker(title, lat, lng);
#
#             return marker;
#         }
#
#         // Load initial markers
#         fetch('/api/markers')
#             .then(response => response.json())
#             .then(data => {
#                 data.forEach(m => {
#                     addMarker(m.lat, m.lng, m.title, m.description, m.type);
#                 });
#             });
#
#         // Map click event to add new markers
#         map.on('click', function(e) {
#             const lat = e.latlng.lat;
#             const lng = e.latlng.lng;
#             const title = `Marker ${markers.length + 1}`;
#             const description = `Custom marker at ${lat.toFixed(4)}, ${lng.toFixed(4)}`;
#
#             // Add marker to map
#             addMarker(lat, lng, title, description);
#
#             // Send to server
#             fetch('/api/add_marker', {
#                 method: 'POST',
#                 headers: {
#                     'Content-Type': 'application/json',
#                 },
#                 body: JSON.stringify({
#                     lat: lat,
#                     lng: lng,
#                     title: title,
#                     description: description,
#                     type: 'custom'
#                 })
#             });
#         });
#
#         function updateMarkerCount() {
#             document.getElementById('marker-count').textContent = markers.length;
#         }
#
#         function updateLatestMarker(title, lat, lng) {
#             document.getElementById('latest-marker').innerHTML = `
#                 <div>📍 ${title}</div>
#                 <div class="coordinates">${lat.toFixed(4)}°N, ${lng.toFixed(4)}°E</div>
#             `;
#         }
#
#         // Fetch new markers periodically
#         setInterval(() => {
#             fetch('/api/markers')
#                 .then(response => response.json())
#                 .then(data => {
#                     if (data.length > markers.length) {
#                         // New markers added
#                         const newMarkers = data.slice(markers.length);
#                         newMarkers.forEach(m => {
#                             addMarker(m.lat, m.lng, m.title, m.description, m.type);
#                         });
#                     }
#                 });
#         }, 2000);
#     </script>
# </body>
# </html>
# """
#
#
# @app.route('/')
# def map_view():
#     return render_template_string(HTML_TEMPLATE)
#
#
# @app.route('/api/markers')
# def get_markers():
#     return jsonify(markers)
#
#
# @app.route('/api/add_marker', methods=['POST'])
# def add_marker():
#     data = request.json
#     new_marker = {
#         'id': len(markers) + 1,
#         'lat': data['lat'],
#         'lng': data['lng'],
#         'title': data['title'],
#         'description': data['description'],
#         'type': data.get('type', 'custom')
#     }
#     markers.append(new_marker)
#
#     # Log to DearPyGui
#     log_marker_addition(new_marker)
#
#     return jsonify({'success': True, 'marker': new_marker})
#
#
# def log_marker_addition(marker):
#     timestamp = datetime.now().strftime('%H:%M:%S')
#     log_msg = f"[{timestamp}] Added: {marker['title']} at ({marker['lat']:.4f}, {marker['lng']:.4f})"
#     dpg.add_text(log_msg, parent="marker_log")
#
#
# class WebBrowser(QMainWindow):
#     def __init__(self):
#         super().__init__()
#         self.setWindowTitle("Interactive World Map - Leaflet")
#         self.setGeometry(100, 100, 1400, 900)
#
#         self.browser = QWebEngineView()
#         self.browser.load(QUrl("http://localhost:5000"))
#         self.setCentralWidget(self.browser)
#
#
# def start_flask_app():
#     app.run(host='localhost', port=5000, debug=False)
#
#
# def open_map():
#     global qt_app
#     if qt_app is None:
#         qt_app = QApplication(sys.argv)
#
#     browser = WebBrowser()
#     browser.show()
#
#     if not qt_app.exec_():
#         qt_app = None
#
#
# def add_marker_from_gui():
#     try:
#         lat = float(dpg.get_value("lat_input"))
#         lng = float(dpg.get_value("lng_input"))
#         title = dpg.get_value("title_input") or f"Marker {len(markers) + 1}"
#         description = dpg.get_value("desc_input") or f"Added from DearPyGui at {lat:.4f}, {lng:.4f}"
#
#         new_marker = {
#             'id': len(markers) + 1,
#             'lat': lat,
#             'lng': lng,
#             'title': title,
#             'description': description,
#             'type': 'dearpygui'
#         }
#
#         markers.append(new_marker)
#         log_marker_addition(new_marker)
#
#         # Clear inputs
#         dpg.set_value("lat_input", "")
#         dpg.set_value("lng_input", "")
#         dpg.set_value("title_input", "")
#         dpg.set_value("desc_input", "")
#
#     except ValueError:
#         dpg.add_text("Error: Please enter valid coordinates!", parent="marker_log", color=(255, 100, 100))
#
#
# def add_preset_locations():
#     presets = [
#         {'lat': 19.0760, 'lng': 72.8777, 'title': 'Mumbai', 'description': 'Financial Capital of India'},
#         {'lat': 28.6139, 'lng': 77.2090, 'title': 'New Delhi', 'description': 'Capital of India'},
#         {'lat': 12.9716, 'lng': 77.5946, 'title': 'Bangalore', 'description': 'Silicon Valley of India'},
#         {'lat': 22.5726, 'lng': 88.3639, 'title': 'Kolkata', 'description': 'Cultural Capital of India'},
#         {'lat': 13.0827, 'lng': 80.2707, 'title': 'Chennai', 'description': 'Detroit of India'}
#     ]
#
#     for preset in presets:
#         new_marker = {
#             'id': len(markers) + 1,
#             'lat': preset['lat'],
#             'lng': preset['lng'],
#             'title': preset['title'],
#             'description': preset['description'],
#             'type': 'preset'
#         }
#         markers.append(new_marker)
#         log_marker_addition(new_marker)
#
#
# def clear_all_markers():
#     global markers
#     # Keep only Surat (first marker)
#     markers = markers[:1]
#     dpg.add_text(f"[{datetime.now().strftime('%H:%M:%S')}] Cleared all markers except Surat",
#                  parent="marker_log", color=(255, 200, 100))
#
#
# # Global Qt app
# qt_app = None
#
# # Create DearPyGui interface
# dpg.create_context()
#
# with dpg.window(label="🌍 World Map Controller", width=700, height=650):
#     dpg.add_text("🗺️ Interactive World Map with Leaflet.js", color=(100, 255, 100))
#     dpg.add_separator()
#
#     dpg.add_text("Map Features:")
#     dpg.add_text("• Interactive world map with Surat as default location", color=(150, 150, 150))
#     dpg.add_text("• Click on map to add markers", color=(150, 150, 150))
#     dpg.add_text("• Add markers from DearPyGui controls", color=(150, 150, 150))
#     dpg.add_text("• Real-time marker synchronization", color=(150, 150, 150))
#     dpg.add_separator()
#
#     with dpg.group(horizontal=True):
#         dpg.add_button(label="🗺️ Open World Map",
#                        callback=lambda: threading.Thread(target=open_map, daemon=True).start())
#         dpg.add_button(label="🏙️ Add Indian Cities", callback=add_preset_locations)
#         dpg.add_button(label="🗑️ Clear Markers", callback=clear_all_markers)
#
#     dpg.add_separator()
#
#     dpg.add_text("📍 Add Custom Marker:")
#
#     with dpg.group(horizontal=True):
#         dpg.add_text("Latitude:")
#         dpg.add_input_text(tag="lat_input", hint="21.1702", width=100)
#         dpg.add_text("Longitude:")
#         dpg.add_input_text(tag="lng_input", hint="72.8311", width=100)
#
#     with dpg.group(horizontal=True):
#         dpg.add_text("Title:")
#         dpg.add_input_text(tag="title_input", hint="Marker Name", width=200)
#
#     dpg.add_text("Description:")
#     dpg.add_input_text(tag="desc_input", hint="Marker description", width=400)
#
#     dpg.add_button(label="📌 Add Marker", callback=add_marker_from_gui)
#
#     dpg.add_separator()
#
#     with dpg.group(horizontal=True):
#         with dpg.group():
#             dpg.add_text("🎯 Quick Locations:")
#             dpg.add_text("• Surat: 21.1702, 72.8311", color=(100, 200, 255))
#             dpg.add_text("• Mumbai: 19.0760, 72.8777", color=(100, 200, 255))
#             dpg.add_text("• Delhi: 28.6139, 77.2090", color=(100, 200, 255))
#             dpg.add_text("• Bangalore: 12.9716, 77.5946", color=(100, 200, 255))
#
#         dpg.add_spacer(width=20)
#
#         with dpg.group():
#             dpg.add_text("🎮 Map Controls:")
#             dpg.add_text("• Left Click: Add marker", color=(200, 200, 100))
#             dpg.add_text("• Mouse Wheel: Zoom in/out", color=(200, 200, 100))
#             dpg.add_text("• Drag: Pan the map", color=(200, 200, 100))
#             dpg.add_text("• Marker Click: Show info", color=(200, 200, 100))
#
#     dpg.add_separator()
#
#     dpg.add_text("📋 Marker Activity Log:")
#     with dpg.child_window(tag="marker_log", height=200, border=True):
#         dpg.add_text("Map initialized with Surat as default location...", color=(100, 255, 100))
#         dpg.add_text(f"[{datetime.now().strftime('%H:%M:%S')}] 📍 Surat loaded at (21.1702, 72.8311)",
#                      color=(100, 200, 255))
#
# # Start Flask app in background
# flask_thread = threading.Thread(target=start_flask_app, daemon=True)
# flask_thread.start()
#
# dpg.create_viewport(title="🌍 Interactive World Map Controller", width=720, height=670)
# dpg.setup_dearpygui()
# dpg.show_viewport()
# dpg.start_dearpygui()
# dpg.destroy_context()