import React, { useState, useEffect, useRef } from 'react';

interface MarkerData {
  lat: number;
  lng: number;
  title: string;
  type: 'Ship' | 'Port' | 'Industry' | 'Bank' | 'Exchange';
}

interface TradeRoute {
  name: string;
  value: string;
  status: 'active' | 'delayed' | 'critical';
  vessels: number;
  coordinates: [number, number][];
}

interface IntelligenceData {
  timestamp: string;
  threat_level: 'low' | 'medium' | 'high' | 'critical';
  active_vessels: number;
  monitored_routes: number;
  trade_volume: string;
}

export default function MaritimeTab() {
  const [markers, setMarkers] = useState<MarkerData[]>([]);
  const [selectedRoute, setSelectedRoute] = useState<TradeRoute | null>(null);
  const [markerType, setMarkerType] = useState<MarkerData['type']>('Ship');
  const [showRoutes, setShowRoutes] = useState(true);
  const [showShips, setShowShips] = useState(true);
  const [intelligence, setIntelligence] = useState<IntelligenceData>({
    timestamp: new Date().toISOString(),
    threat_level: 'low',
    active_vessels: 1247,
    monitored_routes: 48,
    trade_volume: '$847.3B'
  });
  const mapRef = useRef<HTMLIFrameElement>(null);

  const tradeRoutes: TradeRoute[] = [
    { name: 'Mumbai → Rotterdam', value: '$45B', status: 'active', vessels: 23, coordinates: [[19.0760, 72.8777], [51.9244, 4.4777]] },
    { name: 'Mumbai → Shanghai', value: '$156B', status: 'active', vessels: 89, coordinates: [[19.0760, 72.8777], [31.2304, 121.4737]] },
    { name: 'Mumbai → Singapore', value: '$89B', status: 'active', vessels: 45, coordinates: [[19.0760, 72.8777], [1.3521, 103.8198]] },
    { name: 'Chennai → Tokyo', value: '$67B', status: 'delayed', vessels: 34, coordinates: [[13.0827, 80.2707], [35.6762, 139.6503]] },
    { name: 'Kolkata → Hong Kong', value: '$45B', status: 'active', vessels: 28, coordinates: [[22.5726, 88.3639], [22.3193, 114.1694]] },
    { name: 'Mumbai → Dubai', value: '$78B', status: 'critical', vessels: 12, coordinates: [[19.0760, 72.8777], [25.2048, 55.2708]] },
    { name: 'Mumbai → New York', value: '$123B', status: 'active', vessels: 56, coordinates: [[19.0760, 72.8777], [40.7128, -74.0060]] },
    { name: 'Chennai → Sydney', value: '$54B', status: 'active', vessels: 31, coordinates: [[13.0827, 80.2707], [-33.8688, 151.2093]] },
    { name: 'Cochin → Malacca', value: '$34B', status: 'active', vessels: 19, coordinates: [[9.9312, 76.2673], [1.3521, 103.8198]] },
    { name: 'Mumbai → Cape Town', value: '$28B', status: 'delayed', vessels: 8, coordinates: [[19.0760, 72.8777], [-33.9249, 18.4241]] }
  ];

  const addMarker = (title: string, lat: number, lng: number) => {
    const newMarker: MarkerData = { lat, lng, title, type: markerType };
    setMarkers([...markers, newMarker]);
  };

  const clearMarkers = () => {
    setMarkers([]);
  };

  const presetLocations = [
    { name: 'Mumbai Port', lat: 19.0760, lng: 72.8777, type: 'Port' as const },
    { name: 'Shanghai Port', lat: 31.2304, lng: 121.4737, type: 'Port' as const },
    { name: 'Singapore Port', lat: 1.3521, lng: 103.8198, type: 'Port' as const },
    { name: 'Hong Kong Port', lat: 22.3193, lng: 114.1694, type: 'Port' as const },
  ];

  const addPreset = (preset: typeof presetLocations[0]) => {
    setMarkers([...markers, preset]);
  };

  useEffect(() => {
    const interval = setInterval(() => {
      setIntelligence(prev => ({
        ...prev,
        timestamp: new Date().toISOString(),
        active_vessels: prev.active_vessels + Math.floor(Math.random() * 10 - 5)
      }));
    }, 5000);
    return () => clearInterval(interval);
  }, []);

  const mapHTML = `
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8" />
  <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
  <style>
    body { margin: 0; padding: 0; background: #000; }
    #map { height: 100vh; width: 100vw; }
    .ocean-route { stroke: #00ffff; stroke-width: 2; stroke-opacity: 0.8; stroke-dasharray: 8,4; }
    .route-animation { animation: routeFlow 3s linear infinite; }
    @keyframes routeFlow { 0% { stroke-dashoffset: 0; } 100% { stroke-dashoffset: -24; } }
    .ship-marker { animation: shipBob 2s ease-in-out infinite; z-index: 1000 !important; }
    @keyframes shipBob { 0%, 100% { transform: translateY(0px); } 50% { transform: translateY(-3px); } }
  </style>
</head>
<body>
  <div id="map"></div>
  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  <script>
    var map = L.map('map', { zoomControl: true }).setView([20.0, 75.0], 4);

    L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {
      attribution: 'FINCEPT MARITIME INTELLIGENCE',
      maxZoom: 18
    }).addTo(map);

    var markers = [];
    var routesLayer = null;
    var shipsLayer = null;

    function createIcon(type, size = 24) {
      var icons = {
        'Ship': { symbol: '🚢', color: '#1e90ff' },
        'Port': { symbol: '⚓', color: '#ff8c00' },
        'Industry': { symbol: '🏭', color: '#ff4500' },
        'Bank': { symbol: '🏦', color: '#ffd700' },
        'Exchange': { symbol: '💱', color: '#ff1493' }
      };
      var config = icons[type] || icons['Ship'];
      return L.divIcon({
        html: '<div style="width:' + size + 'px;height:' + size + 'px;border-radius:50%;background:#0a0a0a;border:3px solid ' + config.color + ';display:flex;align-items:center;justify-content:center;font-size:' + (size-8) + 'px;box-shadow:0 0 20px ' + config.color + ';">' + config.symbol + '</div>',
        iconSize: [size, size],
        iconAnchor: [size/2, size/2],
        className: type === 'Ship' ? 'ship-marker' : ''
      });
    }

    var oceanRoutes = [
      { name: "Mumbai-Rotterdam", coords: [[19.0760, 72.8777], [15.0, 60.0], [12.0, 45.0], [15.0, 35.0], [30.0, 32.0], [35.0, 25.0], [38.0, 15.0], [40.0, 5.0], [42.0, -5.0], [45.0, -15.0], [51.9244, 4.4777]], value: "45B USD" },
      { name: "Mumbai-Shanghai", coords: [[19.0760, 72.8777], [15.0, 68.0], [10.0, 75.0], [5.0, 85.0], [10.0, 95.0], [20.0, 110.0], [31.2304, 121.4737]], value: "156B USD" },
      { name: "Mumbai-Singapore", coords: [[19.0760, 72.8777], [15.0, 75.0], [10.0, 80.0], [5.0, 90.0], [1.3521, 103.8198]], value: "89B USD" },
      { name: "Chennai-Tokyo", coords: [[13.0827, 80.2707], [10.0, 90.0], [15.0, 105.0], [25.0, 125.0], [35.6762, 139.6503]], value: "67B USD" },
      { name: "Kolkata-Hong Kong", coords: [[22.5726, 88.3639], [18.0, 95.0], [15.0, 105.0], [22.3193, 114.1694]], value: "45B USD" },
      { name: "Mumbai-Dubai", coords: [[19.0760, 72.8777], [20.0, 65.0], [23.0, 58.0], [25.2048, 55.2708]], value: "78B USD" },
      { name: "Mumbai-New York", coords: [[19.0760, 72.8777], [15.0, 60.0], [10.0, 40.0], [5.0, 20.0], [0.0, 0.0], [10.0, -20.0], [25.0, -40.0], [35.0, -60.0], [40.7128, -74.0060]], value: "123B USD" },
      { name: "Chennai-Sydney", coords: [[13.0827, 80.2707], [5.0, 90.0], [-10.0, 105.0], [-25.0, 130.0], [-33.8688, 151.2093]], value: "54B USD" },
      { name: "Cochin-Malacca", coords: [[9.9312, 76.2673], [7.0, 79.0], [6.9271, 79.8612], [3.0, 95.0], [1.3521, 103.8198]], value: "34B USD" },
      { name: "Mumbai-Cape Town", coords: [[19.0760, 72.8777], [10.0, 60.0], [-5.0, 50.0], [-20.0, 35.0], [-33.9249, 18.4241]], value: "28B USD" }
    ];

    function createRoutes() {
      if (routesLayer) map.removeLayer(routesLayer);
      routesLayer = L.layerGroup();
      oceanRoutes.forEach(route => {
        var polyline = L.polyline(route.coords, {
          color: '#00ffff',
          weight: 2,
          opacity: 0.8,
          className: 'route-animation ocean-route'
        });
        polyline.bindPopup('<div style="background:#000;color:#0ff;padding:8px;border:1px solid #0ff;font-family:Consolas;font-size:10px;"><b>' + route.name + '</b><br/>Value: ' + route.value + '</div>');
        routesLayer.addLayer(polyline);
      });
      map.addLayer(routesLayer);
    }

    function createShips() {
      if (shipsLayer) map.removeLayer(shipsLayer);
      shipsLayer = L.layerGroup();
      oceanRoutes.forEach(route => {
        for (var i = 1; i < route.coords.length - 1; i += 2) {
          var ship = L.marker(route.coords[i], { icon: createIcon('Ship', 28) });
          ship.bindPopup('<div style="background:#000;color:#1e90ff;padding:8px;border:1px solid #1e90ff;font-family:Consolas;font-size:10px;"><b>🚢 VESSEL</b><br/>Route: ' + route.name.split('-')[0] + '<br/>Speed: ' + (Math.floor(Math.random()*10+15)) + ' knots</div>');
          shipsLayer.addLayer(ship);
        }
      });
      map.addLayer(shipsLayer);
    }

    window.addMarker = function(lat, lng, title, type) {
      var marker = L.marker([lat, lng], { icon: createIcon(type, 28) });
      marker.bindPopup('<div style="background:#000;color:#ff8c00;padding:8px;border:1px solid #ff8c00;font-family:Consolas;font-size:10px;"><b>' + title + '</b><br/>Type: ' + type + '<br/>Coords: ' + lat.toFixed(4) + ', ' + lng.toFixed(4) + '</div>');
      marker.addTo(map);
      markers.push(marker);
    };

    window.clearMarkers = function() {
      markers.forEach(m => map.removeLayer(m));
      markers = [];
    };

    window.toggleRoutes = function() {
      if (map.hasLayer(routesLayer)) map.removeLayer(routesLayer);
      else createRoutes();
    };

    window.toggleShips = function() {
      if (map.hasLayer(shipsLayer)) map.removeLayer(shipsLayer);
      else createShips();
    };

    createRoutes();
    createShips();
  </script>
</body>
</html>
  `;

  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#000' }}>
      <style>{`
        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: #0a0a0a; }
        *::-webkit-scrollbar-thumb { background: #1a1a1a; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: #2a2a2a; }
      `}</style>

      {/* CIA-Style Header */}
      <div style={{
        borderBottom: '2px solid #00ff00',
        padding: '8px 16px',
        background: 'linear-gradient(180deg, #0a0a0a 0%, #000 100%)',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span style={{ color: '#00ff00', fontSize: '14px', fontWeight: 'bold', letterSpacing: '2px' }}>
            ⚓ FINCEPT MARITIME INTELLIGENCE
          </span>
          <div style={{ width: '1px', height: '16px', background: '#00ff00' }} />
          <span style={{ color: '#0ff', fontSize: '10px' }}>CLASSIFIED // TRADE ROUTE ANALYSIS</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span style={{ color: intelligence.threat_level === 'critical' ? '#ff0000' : intelligence.threat_level === 'high' ? '#ff8c00' : intelligence.threat_level === 'medium' ? '#ffd700' : '#00ff00', fontSize: '10px', fontWeight: 'bold' }}>
            THREAT: {intelligence.threat_level.toUpperCase()}
          </span>
          <span style={{ color: '#888', fontSize: '9px' }}>{new Date(intelligence.timestamp).toLocaleString()}</span>
        </div>
      </div>

      {/* Main Layout */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden' }}>

        {/* Left Sidebar - Intelligence Panel */}
        <div style={{ width: '280px', borderRight: '1px solid #1a1a1a', display: 'flex', flexDirection: 'column', background: '#000', flexShrink: 0 }}>

          {/* Intelligence Stats */}
          <div style={{ padding: '12px', borderBottom: '1px solid #1a1a1a' }}>
            <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>GLOBAL INTEL</div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '8px' }}>
                <div style={{ color: '#888', fontSize: '8px' }}>ACTIVE VESSELS</div>
                <div style={{ color: '#0ff', fontSize: '16px', fontWeight: 'bold' }}>{intelligence.active_vessels}</div>
              </div>
              <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '8px' }}>
                <div style={{ color: '#888', fontSize: '8px' }}>ROUTES</div>
                <div style={{ color: '#0ff', fontSize: '16px', fontWeight: 'bold' }}>{intelligence.monitored_routes}</div>
              </div>
              <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '8px', gridColumn: 'span 2' }}>
                <div style={{ color: '#888', fontSize: '8px' }}>TRADE VOLUME (24H)</div>
                <div style={{ color: '#00ff00', fontSize: '16px', fontWeight: 'bold' }}>{intelligence.trade_volume}</div>
              </div>
            </div>
          </div>

          {/* Trade Routes List */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '12px', minHeight: 0 }}>
            <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>TRADE CORRIDORS</div>
            {tradeRoutes.map((route, idx) => (
              <div
                key={idx}
                onClick={() => setSelectedRoute(route)}
                style={{
                  background: selectedRoute?.name === route.name ? '#1a1a1a' : '#0a0a0a',
                  border: `1px solid ${route.status === 'critical' ? '#ff0000' : route.status === 'delayed' ? '#ffd700' : '#1a1a1a'}`,
                  padding: '8px',
                  marginBottom: '6px',
                  cursor: 'pointer',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => e.currentTarget.style.background = '#1a1a1a'}
                onMouseLeave={(e) => e.currentTarget.style.background = selectedRoute?.name === route.name ? '#1a1a1a' : '#0a0a0a'}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
                  <span style={{ color: '#0ff', fontSize: '9px', fontWeight: 'bold' }}>{route.name}</span>
                  <span style={{
                    color: route.status === 'critical' ? '#ff0000' : route.status === 'delayed' ? '#ffd700' : '#00ff00',
                    fontSize: '7px',
                    padding: '2px 4px',
                    background: '#000',
                    border: `1px solid ${route.status === 'critical' ? '#ff0000' : route.status === 'delayed' ? '#ffd700' : '#00ff00'}`
                  }}>
                    {route.status.toUpperCase()}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: '#888', fontSize: '8px' }}>Value: {route.value}</span>
                  <span style={{ color: '#888', fontSize: '8px' }}>Ships: {route.vessels}</span>
                </div>
              </div>
            ))}
          </div>

          {/* Map Controls */}
          <div style={{ padding: '12px', borderTop: '1px solid #1a1a1a' }}>
            <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>MAP CONTROLS</div>
            <div style={{ display: 'flex', gap: '6px', marginBottom: '6px' }}>
              <button
                onClick={() => {
                  setShowRoutes(!showRoutes);
                  if (mapRef.current?.contentWindow) {
                    (mapRef.current.contentWindow as any).toggleRoutes?.();
                  }
                }}
                style={{
                  flex: 1,
                  background: showRoutes ? '#0ff' : '#0a0a0a',
                  color: showRoutes ? '#000' : '#0ff',
                  border: '1px solid #0ff',
                  padding: '6px',
                  fontSize: '9px',
                  cursor: 'pointer',
                  fontWeight: 'bold'
                }}
              >
                ROUTES
              </button>
              <button
                onClick={() => {
                  setShowShips(!showShips);
                  if (mapRef.current?.contentWindow) {
                    (mapRef.current.contentWindow as any).toggleShips?.();
                  }
                }}
                style={{
                  flex: 1,
                  background: showShips ? '#0ff' : '#0a0a0a',
                  color: showShips ? '#000' : '#0ff',
                  border: '1px solid #0ff',
                  padding: '6px',
                  fontSize: '9px',
                  cursor: 'pointer',
                  fontWeight: 'bold'
                }}
              >
                VESSELS
              </button>
            </div>
            <button
              onClick={() => {
                clearMarkers();
                if (mapRef.current?.contentWindow) {
                  (mapRef.current.contentWindow as any).clearMarkers?.();
                }
              }}
              style={{
                width: '100%',
                background: '#0a0a0a',
                color: '#ff0000',
                border: '1px solid #ff0000',
                padding: '6px',
                fontSize: '9px',
                cursor: 'pointer',
                fontWeight: 'bold'
              }}
            >
              CLEAR ALL MARKERS
            </button>
          </div>
        </div>

        {/* Center - Map */}
        <div style={{ flex: 1, position: 'relative', minWidth: 0, overflow: 'hidden' }}>
          <iframe
            ref={mapRef}
            srcDoc={mapHTML}
            style={{
              width: '100%',
              height: '100%',
              border: 'none',
              background: '#000'
            }}
          />
          {/* Map Overlay - Coordinates Display */}
          <div style={{
            position: 'absolute',
            bottom: '16px',
            left: '16px',
            background: 'rgba(0, 0, 0, 0.9)',
            border: '1px solid #0ff',
            padding: '8px 12px',
            color: '#0ff',
            fontSize: '9px',
            fontFamily: 'Consolas, monospace'
          }}>
            <div style={{ fontWeight: 'bold', marginBottom: '4px' }}>SATELLITE UPLINK ACTIVE</div>
            <div>CENTER: 20.0000° N, 75.0000° E</div>
            <div>ZOOM: 4 | TILES: ESRI World Imagery</div>
          </div>
        </div>

        {/* Right Sidebar - Marker Tools */}
        <div style={{ width: '280px', borderLeft: '1px solid #1a1a1a', display: 'flex', flexDirection: 'column', background: '#000', flexShrink: 0 }}>

          {/* Selected Route Details */}
          {selectedRoute && (
            <div style={{ padding: '12px', borderBottom: '1px solid #1a1a1a' }}>
              <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>ROUTE DETAILS</div>
              <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '10px' }}>
                <div style={{ color: '#0ff', fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>{selectedRoute.name}</div>
                <div style={{ display: 'grid', gap: '6px', fontSize: '9px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: '#888' }}>Value:</span>
                    <span style={{ color: '#fff' }}>{selectedRoute.value}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: '#888' }}>Status:</span>
                    <span style={{ color: selectedRoute.status === 'critical' ? '#ff0000' : selectedRoute.status === 'delayed' ? '#ffd700' : '#00ff00' }}>
                      {selectedRoute.status.toUpperCase()}
                    </span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: '#888' }}>Active Vessels:</span>
                    <span style={{ color: '#fff' }}>{selectedRoute.vessels}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: '#888' }}>Waypoints:</span>
                    <span style={{ color: '#fff' }}>{selectedRoute.coordinates.length}</span>
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* Add Markers */}
          <div style={{ padding: '12px', borderBottom: '1px solid #1a1a1a' }}>
            <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>ADD TARGET</div>

            <div style={{ marginBottom: '8px' }}>
              <div style={{ color: '#888', fontSize: '8px', marginBottom: '4px' }}>MARKER TYPE</div>
              <select
                value={markerType}
                onChange={(e) => setMarkerType(e.target.value as MarkerData['type'])}
                style={{
                  width: '100%',
                  background: '#0a0a0a',
                  color: '#0ff',
                  border: '1px solid #1a1a1a',
                  padding: '6px',
                  fontSize: '9px'
                }}
              >
                <option value="Ship">🚢 Ship</option>
                <option value="Port">⚓ Port</option>
                <option value="Industry">🏭 Industry</option>
                <option value="Bank">🏦 Bank</option>
                <option value="Exchange">💱 Exchange</option>
              </select>
            </div>

            <div style={{ color: '#888', fontSize: '8px', marginBottom: '8px' }}>QUICK ADD PRESETS</div>
            <div style={{ display: 'grid', gap: '4px' }}>
              {presetLocations.map((preset, idx) => (
                <button
                  key={idx}
                  onClick={() => {
                    addPreset(preset);
                    if (mapRef.current?.contentWindow) {
                      (mapRef.current.contentWindow as any).addMarker?.(preset.lat, preset.lng, preset.name, preset.type);
                    }
                  }}
                  style={{
                    background: '#0a0a0a',
                    color: '#fff',
                    border: '1px solid #1a1a1a',
                    padding: '6px',
                    fontSize: '9px',
                    cursor: 'pointer',
                    textAlign: 'left'
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.borderColor = '#0ff'}
                  onMouseLeave={(e) => e.currentTarget.style.borderColor = '#1a1a1a'}
                >
                  {preset.name}
                </button>
              ))}
            </div>
          </div>

          {/* Markers List */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '12px', minHeight: 0 }}>
            <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>
              TRACKED TARGETS ({markers.length})
            </div>
            {markers.length === 0 ? (
              <div style={{ color: '#444', fontSize: '9px', fontStyle: 'italic', padding: '8px' }}>
                No targets marked
              </div>
            ) : (
              markers.map((marker, idx) => (
                <div
                  key={idx}
                  style={{
                    background: '#0a0a0a',
                    border: '1px solid #1a1a1a',
                    padding: '8px',
                    marginBottom: '6px',
                    fontSize: '9px'
                  }}
                >
                  <div style={{ color: '#0ff', fontWeight: 'bold', marginBottom: '4px' }}>{marker.title}</div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: '#888' }}>Type: {marker.type}</span>
                    <span style={{ color: '#666', fontSize: '8px' }}>{marker.lat.toFixed(2)}°, {marker.lng.toFixed(2)}°</span>
                  </div>
                </div>
              ))
            )}
          </div>

          {/* Info Panel */}
          <div style={{ padding: '12px', borderTop: '1px solid #1a1a1a', background: '#0a0a0a' }}>
            <div style={{ color: '#00ff00', fontSize: '9px', fontWeight: 'bold', marginBottom: '6px' }}>SYSTEM STATUS</div>
            <div style={{ fontSize: '8px', color: '#888', lineHeight: '1.4' }}>
              <div style={{ marginBottom: '4px' }}>
                <span style={{ color: '#0ff' }}>●</span> Satellite feed: ACTIVE
              </div>
              <div style={{ marginBottom: '4px' }}>
                <span style={{ color: '#0ff' }}>●</span> AIS transponders: ONLINE
              </div>
              <div style={{ marginBottom: '4px' }}>
                <span style={{ color: '#0ff' }}>●</span> Trade data: STREAMING
              </div>
              <div style={{ marginTop: '8px', padding: '6px', background: '#000', border: '1px solid #1a1a1a', color: '#ffd700' }}>
                ⚠ CLASSIFIED: For authorized personnel only
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Footer */}
      <div style={{
        borderTop: '2px solid #00ff00',
        padding: '8px 16px',
        background: 'linear-gradient(180deg, #000 0%, #0a0a0a 100%)',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0,
        flexWrap: 'wrap',
        gap: '12px',
        minHeight: '40px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '9px', flexWrap: 'wrap' }}>
          <span style={{ color: '#00ff00', fontWeight: 'bold', whiteSpace: 'nowrap' }}>MARITIME INTEL v2.4.1</span>
          <span style={{ color: '#888' }}>|</span>
          <span style={{ color: '#0ff', whiteSpace: 'nowrap' }}>AIS Feed: ACTIVE</span>
          <span style={{ color: '#888' }}>|</span>
          <span style={{ color: '#0ff', whiteSpace: 'nowrap' }}>Satellite Link: NOMINAL</span>
          <span style={{ color: '#888' }}>|</span>
          <span style={{ color: '#0ff', whiteSpace: 'nowrap' }}>Data Latency: 2.3ms</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '9px', flexWrap: 'wrap' }}>
          <span style={{ color: '#888', whiteSpace: 'nowrap' }}>Powered by Leaflet.js + ESRI Imagery</span>
          <span style={{ color: '#888' }}>|</span>
          <span style={{ color: '#ffd700', whiteSpace: 'nowrap' }}>CLEARANCE: TOP SECRET // SCI</span>
        </div>
      </div>
    </div>
  );
}