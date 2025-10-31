import React, { useState, useEffect, useRef } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

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
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [markers, setMarkers] = useState<MarkerData[]>([]);
  const [selectedRoute, setSelectedRoute] = useState<TradeRoute | null>(null);
  const [markerType, setMarkerType] = useState<MarkerData['type']>('Ship');
  const [showRoutes, setShowRoutes] = useState(true);
  const [showShips, setShowShips] = useState(true);
  const [showPlanes, setShowPlanes] = useState(true);
  const [showSatellites, setShowSatellites] = useState(true);
  const [isRotating, setIsRotating] = useState(true);
  const [intelligence, setIntelligence] = useState<IntelligenceData>({
    timestamp: new Date().toISOString(),
    threat_level: 'low',
    active_vessels: 1247,
    monitored_routes: 48,
    trade_volume: '$847.3B'
  });
  const mapRef = useRef<HTMLIFrameElement>(null);

  const tradeRoutes: TradeRoute[] = [
    { name: 'Mumbai → Rotterdam', value: '$45B', status: 'active', vessels: 23, coordinates: [[18.9388, 72.8354], [51.9553, 4.1392]] },
    { name: 'Mumbai → Shanghai', value: '$156B', status: 'active', vessels: 89, coordinates: [[18.9388, 72.8354], [31.3548, 121.6431]] },
    { name: 'Mumbai → Singapore', value: '$89B', status: 'active', vessels: 45, coordinates: [[18.9388, 72.8354], [1.2644, 103.8224]] },
    { name: 'Chennai → Tokyo', value: '$67B', status: 'delayed', vessels: 34, coordinates: [[13.0827, 80.2707], [35.4437, 139.6380]] },
    { name: 'Kolkata → Hong Kong', value: '$45B', status: 'active', vessels: 28, coordinates: [[22.5726, 88.3639], [22.2855, 114.1577]] },
    { name: 'Mumbai → Dubai', value: '$78B', status: 'critical', vessels: 12, coordinates: [[18.9388, 72.8354], [24.9857, 55.0272]] },
    { name: 'Mumbai → New York', value: '$123B', status: 'active', vessels: 56, coordinates: [[18.9388, 72.8354], [40.6655, -74.0781]] },
    { name: 'Chennai → Sydney', value: '$54B', status: 'active', vessels: 31, coordinates: [[13.0827, 80.2707], [-33.9544, 151.2093]] },
    { name: 'Cochin → Klang', value: '$34B', status: 'active', vessels: 19, coordinates: [[9.9667, 76.2667], [3.0048, 101.3975]] },
    { name: 'Mumbai → Cape Town', value: '$28B', status: 'delayed', vessels: 8, coordinates: [[18.9388, 72.8354], [-33.9055, 18.4232]] }
  ];

  const addMarker = (title: string, lat: number, lng: number) => {
    const newMarker: MarkerData = { lat, lng, title, type: markerType };
    setMarkers([...markers, newMarker]);
  };

  const clearMarkers = () => {
    setMarkers([]);
  };

  const presetLocations = [
    { name: 'Mumbai Port', lat: 18.9388, lng: 72.8354, type: 'Port' as const },
    { name: 'Shanghai Port', lat: 31.3548, lng: 121.6431, type: 'Port' as const },
    { name: 'Singapore Port', lat: 1.2644, lng: 103.8224, type: 'Port' as const },
    { name: 'Hong Kong Port', lat: 22.2855, lng: 114.1577, type: 'Port' as const },
    { name: 'Rotterdam Port', lat: 51.9553, lng: 4.1392, type: 'Port' as const },
    { name: 'Dubai Port', lat: 24.9857, lng: 55.0272, type: 'Port' as const },
  ];

  const addPreset = (preset: typeof presetLocations[0]) => {
    setMarkers([...markers, { ...preset, title: preset.name }]);
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
    body { margin: 0; padding: 0; background: #000; overflow: hidden; }
    #globeViz { height: 100vh; width: 100vw; background: #000; position: absolute; top: 0; left: 0; }
    #mapViz { height: 100vh; width: 100vw; position: absolute; top: 0; left: 0; display: none; }
    #modeIndicator {
      position: absolute;
      top: 16px;
      right: 16px;
      background: rgba(0, 255, 255, 0.9);
      color: #000;
      padding: 8px 16px;
      border-radius: 4px;
      font-family: 'Consolas', monospace;
      font-size: 12px;
      font-weight: bold;
      z-index: 1000;
      box-shadow: 0 0 10px rgba(0, 255, 255, 0.5);
    }
  </style>
</head>
<body>
  <div id="modeIndicator">3D GLOBE MODE</div>
  <div id="globeViz"></div>
  <div id="mapViz"></div>
  <script src="https://unpkg.com/three@0.160.0/build/three.min.js"></script>
  <script src="https://unpkg.com/globe.gl@2.27.2/dist/globe.gl.min.js"></script>
  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  <script>
    console.log('Initializing Hybrid Globe/Map System...');

    // Wait for libraries to load
    setTimeout(() => {
      try {
        console.log('Creating globe...');

        // Create professional globe with natural appearance
        const world = Globe()
          (document.getElementById('globeViz'))
          .globeImageUrl('https://unpkg.com/three-globe@2.31.1/example/img/earth-blue-marble.jpg')
          .bumpImageUrl('https://unpkg.com/three-globe@2.31.1/example/img/earth-topology.png')
          .backgroundColor('#0a0a0a')
          .backgroundImageUrl('https://unpkg.com/three-globe/example/img/night-sky.png')
          .showAtmosphere(true)
          .atmosphereColor('#4a90e2')
          .atmosphereAltitude(0.12)
          .width(window.innerWidth)
          .height(window.innerHeight);

        console.log('Globe created successfully');

        // Initialize 2D Leaflet map for close zoom (lazy - will initialize when needed)
        let map2D = null;

        function initMap2D() {
          if (!map2D) {
            map2D = L.map('mapViz', {
              zoomControl: true,
              attributionControl: false,
              preferCanvas: false
            }).setView([20, 75], 12);

            // Add satellite imagery layer (Google Satellite)
            L.tileLayer('https://mt1.google.com/vt/lyrs=s&x={x}&y={y}&z={z}', {
              attribution: 'FINCEPT MARITIME INTELLIGENCE | Satellite Imagery',
              maxZoom: 20,
              minZoom: 3,
              subdomains: ['mt0', 'mt1', 'mt2', 'mt3']
            }).addTo(map2D);

            // Monitor 2D map zoom to switch back to 3D
            map2D.on('zoomend', function() {
              const zoom = map2D.getZoom();
              console.log('2D map zoom:', zoom);
              if (zoom < 8 && currentMode === '2D') {
                const center = map2D.getCenter();
                switchTo3D(center.lat, center.lng, 2.0);
              }
            });

            // Force map to resize and load tiles
            setTimeout(() => {
              map2D.invalidateSize();
              console.log('2D map resized and tiles loading');
            }, 100);

            console.log('2D map initialized');
          }
        }

        console.log('2D map setup ready');

        // Professional balanced lighting
        const scene = world.scene();
        const ambientLight = new THREE.AmbientLight(0x808080, 0.7);  // Balanced ambient light
        const directionalLight = new THREE.DirectionalLight(0xaaaaaa, 0.6);  // Natural directional light
        directionalLight.position.set(5, 3, 5);
        scene.add(ambientLight);
        scene.add(directionalLight);

        console.log('Professional lighting added');

        // Set initial point of view
        world.pointOfView({ lat: 20, lng: 75, altitude: 2.0 }, 1000);

        console.log('Camera positioned');

        // Hybrid mode state
        let currentMode = '3D'; // '3D' or '2D'
        let lastGlobeView = { lat: 20, lng: 75, altitude: 2.0 };
        let targetCoords = { lat: 20, lng: 75 }; // Track where user is actually looking
        const ZOOM_THRESHOLD = 0.3; // Switch to 2D when altitude < 0.3 (very close)

      var markers = [];
      var allArcs = [];
      var allLabels = [];
      var planeLabels = [];
      var showRoutesFlag = true;
      var showShipsFlag = true;
      var showPlanesFlag = true;

      var oceanRoutes = [
        // Mumbai Port (18.9388° N, 72.8354° E) - actual port coordinates
        { name: "Mumbai → Rotterdam", start: {lat: 18.9388, lng: 72.8354}, end: {lat: 51.9553, lng: 4.1392}, value: "$45B", vessels: 23, color: 'rgba(100, 180, 255, 0.6)', width: 0.8 },
        { name: "Mumbai → Shanghai", start: {lat: 18.9388, lng: 72.8354}, end: {lat: 31.3548, lng: 121.6431}, value: "$156B", vessels: 89, color: 'rgba(120, 200, 255, 0.7)', width: 1.2 },
        { name: "Mumbai → Singapore", start: {lat: 18.9388, lng: 72.8354}, end: {lat: 1.2644, lng: 103.8224}, value: "$89B", vessels: 45, color: 'rgba(100, 180, 255, 0.65)', width: 1.0 },
        // Chennai Port (13.0827° N, 80.2707° E)
        { name: "Chennai → Tokyo", start: {lat: 13.0827, lng: 80.2707}, end: {lat: 35.4437, lng: 139.6380}, value: "$67B", vessels: 34, color: 'rgba(150, 220, 255, 0.6)', width: 0.9 },
        // Kolkata Port (22.5726° N, 88.3639° E)
        { name: "Kolkata → Hong Kong", start: {lat: 22.5726, lng: 88.3639}, end: {lat: 22.2855, lng: 114.1577}, value: "$45B", vessels: 28, color: 'rgba(100, 180, 255, 0.55)', width: 0.8 },
        // Mumbai → Dubai (Port Jebel Ali: 24.9857° N, 55.0272° E)
        { name: "Mumbai → Dubai", start: {lat: 18.9388, lng: 72.8354}, end: {lat: 24.9857, lng: 55.0272}, value: "$78B", vessels: 12, color: 'rgba(255, 160, 100, 0.6)', width: 0.9 },
        // Mumbai → New York (Port of NY/NJ: 40.6655° N, 74.0781° W)
        { name: "Mumbai → New York", start: {lat: 18.9388, lng: 72.8354}, end: {lat: 40.6655, lng: -74.0781}, value: "$123B", vessels: 56, color: 'rgba(120, 200, 255, 0.75)', width: 1.2 },
        // Chennai → Sydney (Port Botany: 33.9544° S, 151.2093° E)
        { name: "Chennai → Sydney", start: {lat: 13.0827, lng: 80.2707}, end: {lat: -33.9544, lng: 151.2093}, value: "$54B", vessels: 31, color: 'rgba(130, 190, 255, 0.6)', width: 0.9 },
        // Cochin Port (9.9667° N, 76.2667° E) → Port Klang, Malaysia (3.0048° N, 101.3975° E)
        { name: "Cochin → Klang", start: {lat: 9.9667, lng: 76.2667}, end: {lat: 3.0048, lng: 101.3975}, value: "$34B", vessels: 19, color: 'rgba(180, 200, 255, 0.5)', width: 0.7 },
        // Mumbai → Cape Town (33.9055° S, 18.4232° E)
        { name: "Mumbai → Cape Town", start: {lat: 18.9388, lng: 72.8354}, end: {lat: -33.9055, lng: 18.4232}, value: "$28B", vessels: 8, color: 'rgba(160, 180, 255, 0.5)', width: 0.7 }
      ];

      var airRoutes = [
        // Major international air cargo routes
        { name: "Mumbai → London", start: {lat: 19.0896, lng: 72.8656}, end: {lat: 51.4700, lng: -0.4543}, value: "$12B", flights: 45, color: 'rgba(255, 200, 100, 0.7)', width: 0.6 },
        { name: "Delhi → New York", start: {lat: 28.5562, lng: 77.1000}, end: {lat: 40.6413, lng: -73.7781}, value: "$18B", flights: 67, color: 'rgba(255, 200, 100, 0.75)', width: 0.8 },
        { name: "Mumbai → Dubai", start: {lat: 19.0896, lng: 72.8656}, end: {lat: 25.2532, lng: 55.3657}, value: "$22B", flights: 123, color: 'rgba(255, 200, 100, 0.8)', width: 0.9 },
        { name: "Bangalore → Singapore", start: {lat: 13.1979, lng: 77.7063}, end: {lat: 1.3644, lng: 103.9915}, value: "$8B", flights: 89, color: 'rgba(255, 200, 100, 0.65)', width: 0.6 },
        { name: "Delhi → Frankfurt", start: {lat: 28.5562, lng: 77.1000}, end: {lat: 50.0379, lng: 8.5622}, value: "$15B", flights: 56, color: 'rgba(255, 200, 100, 0.7)', width: 0.7 },
        { name: "Mumbai → Hong Kong", start: {lat: 19.0896, lng: 72.8656}, end: {lat: 22.3080, lng: 113.9185}, value: "$11B", flights: 78, color: 'rgba(255, 200, 100, 0.65)', width: 0.6 },
        { name: "Chennai → Tokyo", start: {lat: 12.9941, lng: 80.1709}, end: {lat: 35.7720, lng: 140.3929}, value: "$9B", flights: 34, color: 'rgba(255, 200, 100, 0.6)', width: 0.6 },
        { name: "Delhi → Sydney", start: {lat: 28.5562, lng: 77.1000}, end: {lat: -33.9399, lng: 151.1753}, value: "$7B", flights: 28, color: 'rgba(255, 200, 100, 0.6)', width: 0.5 }
      ];

      function createRoutes() {
        // Combine ocean and air routes
        const oceanArcsData = oceanRoutes.map(route => ({
          startLat: route.start.lat,
          startLng: route.start.lng,
          endLat: route.end.lat,
          endLng: route.end.lng,
          color: route.color,
          name: route.name,
          value: route.value,
          width: route.width || 0.5,
          type: 'ocean'
        }));

        const airArcsData = airRoutes.map(route => ({
          startLat: route.start.lat,
          startLng: route.start.lng,
          endLat: route.end.lat,
          endLng: route.end.lng,
          color: route.color,
          name: route.name,
          value: route.value,
          width: route.width || 0.5,
          type: 'air'
        }));

        const allArcsData = [...oceanArcsData, ...airArcsData];
        allArcs = allArcsData;
        world.arcsData(allArcsData)
          .arcColor('color')
          .arcStroke((d) => d.width)         // Thin, professional lines
          .arcDashLength(1.0)                // Full solid lines
          .arcDashGap(0)                     // No gaps - complete lines
          .arcDashInitialGap(0)              // Start from beginning
          .arcDashAnimateTime(0)             // No animation to avoid cutting
          .arcAltitude((d) => d.type === 'air' ? 0.15 : 0.1)  // Higher for air, moderate for ocean
          .arcAltitudeAutoScale(0.5)         // More curve for visibility
          .arcLabel((d) => \`
            <div style="background: rgba(10,10,10,0.92); padding: 6px 10px; border-left: 2px solid \${d.type === 'air' ? 'rgba(255, 200, 100, 0.8)' : 'rgba(100, 180, 255, 0.8)'}; font-family: 'Segoe UI', sans-serif; font-size: 11px; box-shadow: 0 2px 8px rgba(0,0,0,0.4);">
              <div style="color: #e0e0e0; font-weight: 500; margin-bottom: 2px;">\${d.type === 'air' ? '✈' : '🚢'} \${d.name}</div>
              <div style="color: #999; font-size: 9px;">\${d.value}</div>
            </div>
          \`);

        // Extract unique ports and airports
        const uniquePorts = new Map();
        oceanRoutes.forEach(route => {
          const startName = route.name.split(' → ')[0];
          const endName = route.name.split(' → ')[1];

          if (!uniquePorts.has(startName)) {
            uniquePorts.set(startName, { lat: route.start.lat, lng: route.start.lng, type: 'port' });
          }
          if (!uniquePorts.has(endName)) {
            uniquePorts.set(endName, { lat: route.end.lat, lng: route.end.lng, type: 'port' });
          }
        });

        const uniqueAirports = new Map();
        airRoutes.forEach(route => {
          const startName = route.name.split(' → ')[0];
          const endName = route.name.split(' → ')[1];

          if (!uniqueAirports.has(startName)) {
            uniqueAirports.set(startName, { lat: route.start.lat, lng: route.start.lng, type: 'airport' });
          }
          if (!uniqueAirports.has(endName)) {
            uniqueAirports.set(endName, { lat: route.end.lat, lng: route.end.lng, type: 'airport' });
          }
        });

        const portPoints = Array.from(uniquePorts.entries()).map(([name, data]) => ({
          lat: data.lat,
          lng: data.lng,
          size: 0.6,
          color: 'rgba(255, 180, 100, 0.85)',  // Orange for ports
          label: name,
          type: 'port'
        }));

        const airportPoints = Array.from(uniqueAirports.entries()).map(([name, data]) => ({
          lat: data.lat,
          lng: data.lng,
          size: 0.5,
          color: 'rgba(180, 100, 255, 0.85)',  // Purple for airports
          label: name,
          type: 'airport'
        }));

        const allPoints = [...portPoints, ...airportPoints];

        // Professional markers - ports (orange) and airports (purple)
        world.pointsData(allPoints)
          .pointAltitude(0.002)
          .pointRadius('size')
          .pointColor('color')
          .pointLabel((d) => \`
            <div style="background: rgba(10,10,10,0.92); padding: 5px 9px; border-left: 2px solid \${d.color}; font-family: 'Segoe UI', sans-serif; font-size: 10px; box-shadow: 0 2px 8px rgba(0,0,0,0.4);">
              <span style="color: #e0e0e0; font-weight: 500;">\${d.type === 'port' ? '⚓' : '✈'} \${d.label}</span>
            </div>
          \`);
      }

      function createShips() {
        // Professional ship visualization using proper globe coordinates
        const shipData = oceanRoutes.map((route, routeIdx) => ({
          routeIdx: routeIdx,
          progress: Math.random(),
          startLat: route.start.lat,
          startLng: route.start.lng,
          endLat: route.end.lat,
          endLng: route.end.lng,
          label: route.name,
          value: route.value,
          vessels: route.vessels
        }));

        // Calculate interpolated positions along great circle arc
        function interpolateArc(startLat, startLng, endLat, endLng, progress) {
          // Simple linear interpolation for latitude and longitude
          const lat = startLat + (endLat - startLat) * progress;
          const lng = startLng + (endLng - startLng) * progress;
          return { lat, lng };
        }

        // Generate ship positions
        function generateShipPoints(shipsData) {
          return shipsData.map(ship => {
            const pos = interpolateArc(
              ship.startLat,
              ship.startLng,
              ship.endLat,
              ship.endLng,
              ship.progress
            );
            return {
              lat: pos.lat,
              lng: pos.lng,
              size: 0.15,
              color: 'rgba(100, 180, 255, 0.95)',
              altitude: 0.003,
              label: ship.label,
              value: ship.value,
              vessels: ship.vessels
            };
          });
        }

        allLabels = shipData;

        // Use htmlElementsData for ships with ship icons (similar to planes)
        const initialShipPoints = generateShipPoints(shipData);
        world.customLayerData(initialShipPoints)
          .customThreeObject(d => {
            // Create HTML element for ship icon
            const el = document.createElement('div');
            el.innerHTML = '🚢';
            el.style.color = 'rgba(100, 180, 255, 0.95)';
            el.style.fontSize = '16px';
            el.style.cursor = 'pointer';
            el.style.filter = 'drop-shadow(0 0 4px rgba(100, 180, 255, 0.8))';
            el.title = \`\${d.label} - \${d.value} (\${d.vessels} vessels)\`;

            // Convert HTML to texture for THREE.js
            const canvas = document.createElement('canvas');
            canvas.width = 64;
            canvas.height = 64;
            const ctx = canvas.getContext('2d');
            ctx.font = '48px Arial';
            ctx.fillText('🚢', 8, 48);

            const texture = new THREE.CanvasTexture(canvas);
            const material = new THREE.SpriteMaterial({
              map: texture,
              transparent: true,
              opacity: 0.95
            });
            const sprite = new THREE.Sprite(material);
            sprite.scale.set(2, 2, 1);

            return sprite;
          })
          .customThreeObjectUpdate((obj, d) => {
            Object.assign(obj.position, world.getCoords(d.lat, d.lng, d.altitude));
          });

        // Animate ships along routes
        setInterval(() => {
          // Update progress for each ship
          shipData.forEach(ship => {
            ship.progress = (ship.progress + 0.0002) % 1.0;
          });

          // Regenerate ship positions
          const updatedShipPoints = generateShipPoints(shipData);
          world.customLayerData(updatedShipPoints);
        }, 200);
      }

      function createPlanes() {
        // Professional plane visualization using proper globe coordinates
        const planeData = airRoutes.map((route, routeIdx) => ({
          routeIdx: routeIdx,
          progress: Math.random(),
          startLat: route.start.lat,
          startLng: route.start.lng,
          endLat: route.end.lat,
          endLng: route.end.lng,
          label: route.name,
          value: route.value,
          flights: route.flights
        }));

        // Calculate interpolated positions along arc
        function interpolateArc(startLat, startLng, endLat, endLng, progress) {
          const lat = startLat + (endLat - startLat) * progress;
          const lng = startLng + (endLng - startLng) * progress;
          return { lat, lng };
        }

        // Generate plane positions
        function generatePlanePoints(planesData) {
          return planesData.map(plane => {
            const pos = interpolateArc(
              plane.startLat,
              plane.startLng,
              plane.endLat,
              plane.endLng,
              plane.progress
            );
            return {
              lat: pos.lat,
              lng: pos.lng,
              size: 0.15,
              color: 'rgba(255, 200, 100, 0.95)',
              altitude: 0.01,  // Higher altitude for planes
              label: plane.label,
              value: plane.value,
              flights: plane.flights
            };
          });
        }

        planeLabels = planeData;

        // Use separate layer for planes
        const initialPlanePoints = generatePlanePoints(planeData);
        world.htmlElementsData(initialPlanePoints)
          .htmlElement(d => {
            const el = document.createElement('div');
            el.innerHTML = '✈';
            el.style.color = 'rgba(255, 200, 100, 0.95)';
            el.style.fontSize = '16px';
            el.style.cursor = 'pointer';
            el.style.filter = 'drop-shadow(0 0 4px rgba(255, 200, 100, 0.8))';
            el.style.transform = 'rotate(-45deg)';
            el.title = \`\${d.label} - \${d.value} (\${d.flights} flights/week)\`;
            return el;
          })
          .htmlAltitude(d => d.altitude);

        // Animate planes along routes (faster than ships)
        setInterval(() => {
          // Update progress for each plane
          planeData.forEach(plane => {
            plane.progress = (plane.progress + 0.0004) % 1.0;  // Faster than ships
          });

          // Regenerate plane positions
          const updatedPlanePoints = generatePlanePoints(planeData);
          world.htmlElementsData(updatedPlanePoints);
        }, 200);
      }

      function createSatellites() {
        // Create satellite data in different orbits with unique orbital paths
        // Speeds reduced significantly for more realistic and pleasant viewing
        const satelliteOrbits = [
          // Low Earth Orbit satellites (LEO) - closer to Earth, faster orbits
          { name: 'GPS-1', inclination: 55, altitude: 0.35, speed: 0.0008, color: '#00ff00', orbitId: 'gps' },
          { name: 'GPS-2', inclination: 55, altitude: 0.35, speed: 0.0008, color: '#00ff00', phaseOffset: Math.PI, orbitId: 'gps' },
          { name: 'IRNSS-1', inclination: 29, altitude: 0.37, speed: 0.0007, color: '#ffd700', orbitId: 'irnss' },
          { name: 'IRNSS-2', inclination: 29, altitude: 0.37, speed: 0.0007, color: '#ffd700', phaseOffset: Math.PI / 2, orbitId: 'irnss' },
          { name: 'Starlink-1', inclination: 53, altitude: 0.32, speed: 0.0009, color: '#00ccff', orbitId: 'starlink' },
          { name: 'Starlink-2', inclination: 53, altitude: 0.32, speed: 0.0009, color: '#00ccff', phaseOffset: 2 * Math.PI / 3, orbitId: 'starlink' },
          { name: 'Starlink-3', inclination: 53, altitude: 0.32, speed: 0.0009, color: '#00ccff', phaseOffset: 4 * Math.PI / 3, orbitId: 'starlink' },
          // Medium Earth Orbit (MEO) - higher altitude, medium speed
          { name: 'GLONASS-1', inclination: 64.8, altitude: 0.42, speed: 0.0006, color: '#ff6b6b', orbitId: 'glonass' },
          { name: 'GLONASS-2', inclination: 64.8, altitude: 0.42, speed: 0.0006, color: '#ff6b6b', phaseOffset: Math.PI / 3, orbitId: 'glonass' },
          { name: 'Galileo-1', inclination: 56, altitude: 0.40, speed: 0.0007, color: '#9d4edd', orbitId: 'galileo' },
          { name: 'Galileo-2', inclination: 56, altitude: 0.40, speed: 0.0007, color: '#9d4edd', phaseOffset: Math.PI / 2, orbitId: 'galileo' },
          // Geostationary orbit representation (higher altitude, slowest)
          { name: 'INSAT-1', inclination: 0, altitude: 0.50, speed: 0.0004, color: '#ff9500', orbitId: 'insat' },
          { name: 'INSAT-2', inclination: 0, altitude: 0.50, speed: 0.0004, color: '#ff9500', phaseOffset: Math.PI, orbitId: 'insat' },
        ];

        const satellites = satelliteOrbits.map((orbit, idx) => ({
          id: idx,
          name: orbit.name,
          inclination: orbit.inclination,
          altitude: orbit.altitude,
          speed: orbit.speed,
          color: orbit.color,
          orbitId: orbit.orbitId,
          phase: orbit.phaseOffset || 0
        }));

        // Create orbital ring paths - draw complete circles for each unique orbit
        function createOrbitalPaths() {
          const uniqueOrbits = new Map();

          satelliteOrbits.forEach(orbit => {
            if (!uniqueOrbits.has(orbit.orbitId)) {
              uniqueOrbits.set(orbit.orbitId, {
                inclination: orbit.inclination,
                altitude: orbit.altitude,
                color: orbit.color,
                name: orbit.orbitId
              });
            }
          });

          const orbitalPaths = [];
          uniqueOrbits.forEach((orbit, orbitId) => {
            const numPoints = 100; // Number of points to draw the orbital ring
            const pathPoints = [];

            for (let i = 0; i <= numPoints; i++) {
              const angle = (i / numPoints) * 2 * Math.PI;
              const inclinationRad = (orbit.inclination * Math.PI) / 180;

              // Calculate position on orbit
              const x = Math.cos(angle);
              const y = Math.sin(angle) * Math.cos(inclinationRad);
              const z = Math.sin(angle) * Math.sin(inclinationRad);

              // Convert to lat/lng
              const lat = Math.asin(y) * (180 / Math.PI);
              const lng = Math.atan2(z, x) * (180 / Math.PI);

              pathPoints.push([lat, lng]);
            }

            orbitalPaths.push({
              id: orbitId,
              color: orbit.color,
              altitude: orbit.altitude,
              points: pathPoints
            });
          });

          return orbitalPaths;
        }

        const orbitalPaths = createOrbitalPaths();

        // Store orbital paths for toggling
        orbitalPathsData = orbitalPaths;

        // Draw orbital rings using pathsData
        world.pathsData(orbitalPaths)
          .pathPoints('points')
          .pathPointLat(p => p[0])
          .pathPointLng(p => p[1])
          .pathColor('color')
          .pathStroke(0.8)
          .pathDashLength(0.5)
          .pathDashGap(0.1)
          .pathDashAnimateTime(20000)
          .pathTransitionDuration(0)
          .pathPointAlt('altitude');

        // Function to calculate satellite position in orbit
        function calculateSatellitePosition(sat, time) {
          const angle = sat.phase + time * sat.speed;

          // Convert orbital elements to 3D position
          const inclinationRad = (sat.inclination * Math.PI) / 180;

          // Orbital position
          const x = Math.cos(angle);
          const y = Math.sin(angle) * Math.cos(inclinationRad);
          const z = Math.sin(angle) * Math.sin(inclinationRad);

          // Convert to lat/lng (approximate)
          const lat = Math.asin(y) * (180 / Math.PI);
          const lng = Math.atan2(z, x) * (180 / Math.PI);

          return { lat, lng, altitude: sat.altitude };
        }

        // Create satellite objects with animation
        const satelliteData = satellites.map((sat, idx) => ({
          ...sat,
          progress: (idx / satellites.length) // Distribute satellites evenly around orbit initially
        }));

        // Function to generate satellite HTML elements
        function generateSatellitePoints(satsData) {
          return satsData.map(sat => {
            const angle = sat.progress * 2 * Math.PI;
            const inclinationRad = (sat.inclination * Math.PI) / 180;

            // Calculate position on orbit
            const x = Math.cos(angle);
            const y = Math.sin(angle) * Math.cos(inclinationRad);
            const z = Math.sin(angle) * Math.sin(inclinationRad);

            // Convert to lat/lng
            const lat = Math.asin(y) * (180 / Math.PI);
            const lng = Math.atan2(z, x) * (180 / Math.PI);

            return {
              lat: lat,
              lng: lng,
              altitude: sat.altitude,
              name: sat.name,
              color: sat.color
            };
          });
        }

        // Initial satellite positions - use objectsData for satellites (separate from planes)
        const initialSatellitePoints = generateSatellitePoints(satelliteData);

        world.objectsData(initialSatellitePoints)
          .objectLat('lat')
          .objectLng('lng')
          .objectAltitude('altitude')
          .objectLabel(d => \`
            <div style="background: rgba(10,10,10,0.92); padding: 5px 9px; border-left: 2px solid \${d.color}; font-family: 'Segoe UI', sans-serif; font-size: 10px; box-shadow: 0 2px 8px rgba(0,0,0,0.4);">
              <span style="color: #e0e0e0; font-weight: 500;">🛰 \${d.name}</span>
            </div>
          \`)
          .objectThreeObject(d => {
            // Create satellite icon using canvas texture with better visibility
            const canvas = document.createElement('canvas');
            canvas.width = 128;
            canvas.height = 128;
            const ctx = canvas.getContext('2d');

            // Add colored background circle for visibility
            ctx.beginPath();
            ctx.arc(64, 64, 50, 0, 2 * Math.PI);
            ctx.fillStyle = d.color;
            ctx.globalAlpha = 0.4;
            ctx.fill();
            ctx.globalAlpha = 1.0;

            // Add bright border
            ctx.strokeStyle = d.color;
            ctx.lineWidth = 3;
            ctx.stroke();

            // Draw satellite emoji larger and centered
            ctx.font = 'bold 70px Arial';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillStyle = '#ffffff';
            ctx.fillText('🛰', 64, 64);

            const texture = new THREE.CanvasTexture(canvas);
            const material = new THREE.SpriteMaterial({
              map: texture,
              transparent: true,
              opacity: 1.0,
              sizeAttenuation: true
            });
            const sprite = new THREE.Sprite(material);
            sprite.scale.set(4, 4, 1);

            // Add pulsing glow ring around satellite
            const ringGeometry = new THREE.RingGeometry(1.5, 2.0, 16);
            const ringMaterial = new THREE.MeshBasicMaterial({
              color: d.color,
              transparent: true,
              opacity: 0.6,
              side: THREE.DoubleSide
            });
            const ring = new THREE.Mesh(ringGeometry, ringMaterial);

            const group = new THREE.Group();
            group.add(sprite);
            group.add(ring);

            return group;
          });

        // Animate satellites along their orbits at a gentle, realistic pace
        setInterval(() => {
          if (showSatellitesFlag) {
            // Update progress for each satellite with reduced multiplier
            satelliteData.forEach(sat => {
              sat.progress = (sat.progress + sat.speed * 0.15) % 1.0;
            });

            // Regenerate satellite positions
            const updatedSatellitePoints = generateSatellitePoints(satelliteData);
            world.objectsData(updatedSatellitePoints);
          }
        }, 100);
      }

      window.addMarker = function(lat, lng, title, type) {
        const icons = {
          'Ship': '🚢',
          'Port': '⚓',
          'Industry': '🏭',
          'Bank': '🏦',
          'Exchange': '💱'
        };

        markers.push({ lat, lng, title, type, icon: icons[type] || icons['Ship'] });

        world.htmlElementsData([...allLabels, ...markers])
          .htmlElement(d => {
            const el = document.createElement('div');
            el.innerHTML = d.icon || d.label;
            el.style.color = d.color || '#ff8c00';
            el.style.fontSize = '20px';
            el.style.cursor = 'pointer';
            el.style.textShadow = '0 0 5px ' + (d.color || '#ff8c00');
            el.title = d.title || d.label || '';
            return el;
          });
      };

      window.clearMarkers = function() {
        markers = [];
        world.htmlElementsData(allLabels);
      };

      window.toggleRoutes = function() {
        if (showRoutesFlag) {
          world.arcsData([]);
        } else {
          createRoutes();
        }
        showRoutesFlag = !showRoutesFlag;
      };

      window.toggleShips = function() {
        if (showShipsFlag) {
          world.customLayerData([]);
        } else {
          // Regenerate ship positions
          function interpolateArc(startLat, startLng, endLat, endLng, progress) {
            const lat = startLat + (endLat - startLat) * progress;
            const lng = startLng + (endLng - startLng) * progress;
            return { lat, lng };
          }

          const shipPoints = allLabels.map(ship => {
            const pos = interpolateArc(ship.startLat, ship.startLng, ship.endLat, ship.endLng, ship.progress);
            return {
              lat: pos.lat,
              lng: pos.lng,
              size: 0.15,
              color: 'rgba(255, 255, 255, 0.95)',
              altitude: 0.003,
              label: ship.label,
              value: ship.value,
              vessels: ship.vessels
            };
          });
          world.customLayerData(shipPoints);
        }
        showShipsFlag = !showShipsFlag;
      };

      window.togglePlanes = function() {
        if (showPlanesFlag) {
          world.htmlElementsData([]);
        } else {
          // Regenerate plane positions
          function interpolateArc(startLat, startLng, endLat, endLng, progress) {
            const lat = startLat + (endLat - startLat) * progress;
            const lng = startLng + (endLng - startLng) * progress;
            return { lat, lng };
          }

          const planePoints = planeLabels.map(plane => {
            const pos = interpolateArc(plane.startLat, plane.startLng, plane.endLat, plane.endLng, plane.progress);
            return {
              lat: pos.lat,
              lng: pos.lng,
              size: 0.15,
              color: 'rgba(255, 200, 100, 0.95)',
              altitude: 0.01,
              label: plane.label,
              value: plane.value,
              flights: plane.flights
            };
          });
          world.htmlElementsData(planePoints);
        }
        showPlanesFlag = !showPlanesFlag;
      };

      window.toggleRotation = function(shouldRotate) {
        userPausedRotation = !shouldRotate;
        world.controls().autoRotate = shouldRotate;
        console.log('Globe rotation:', shouldRotate ? 'enabled' : 'disabled');
      };

      var showSatellitesFlag = true;
      var orbitalPathsData = [];

      window.toggleSatellites = function() {
        showSatellitesFlag = !showSatellitesFlag;

        if (!showSatellitesFlag) {
          // Hide satellites and orbital paths
          world.objectsData([]);
          world.pathsData([]);
        } else {
          // Show orbital paths again
          world.pathsData(orbitalPathsData);
          // Satellites will be updated by the interval
        }
      };

        // Enable auto-rotation with enhanced zoom
        world.controls().autoRotate = true;
        world.controls().autoRotateSpeed = 0.2;  // Slower, more professional rotation
        world.controls().enableDamping = true;
        world.controls().dampingFactor = 0.05;

        // Allow deep zoom but not too extreme on 3D globe
        world.controls().minDistance = 50;     // Don't go too close on 3D (will switch to 2D)
        world.controls().maxDistance = 800;    // Very far zoom (space view)
        world.controls().zoomSpeed = 1.5;      // Faster zoom for better control
        world.controls().enableZoom = true;

        console.log('Controls configured');

      // Keep rotation state controlled by user toggle
      let userPausedRotation = false;
      let interactionTimeout;
      const resumeRotation = () => {
        clearTimeout(interactionTimeout);
        world.controls().autoRotate = false;
        interactionTimeout = setTimeout(() => {
          // Only resume if user hasn't manually paused
          if (!userPausedRotation) {
            world.controls().autoRotate = true;
          }
        }, 3000); // Resume after 3 seconds of inactivity (only if not manually paused)
      };

        // Function to switch between 3D and 2D modes
        function switchTo2D(lat, lng) {
          if (currentMode === '2D') return; // Prevent multiple switches

          // Use the most recent target coordinates (where user was actually looking)
          const finalLat = targetCoords.lat;
          const finalLng = targetCoords.lng;

          initMap2D(); // Initialize if needed

          document.getElementById('globeViz').style.display = 'none';
          document.getElementById('mapViz').style.display = 'block';
          document.getElementById('modeIndicator').textContent = '2D MAP MODE - Street Level';
          document.getElementById('modeIndicator').style.background = 'rgba(255, 215, 0, 0.9)';

          // Calculate zoom based on altitude (lower altitude = higher zoom)
          // More aggressive zoom calculation for better street detail
          const altitudeFactor = Math.max(0, 0.3 - lastGlobeView.altitude);
          const zoom2D = Math.max(14, Math.min(18, Math.floor(16 + altitudeFactor * 20)));

          console.log('Switching to 2D - Final coords:', finalLat.toFixed(6), finalLng.toFixed(6), 'zoom:', zoom2D);

          // Set view and force map refresh
          setTimeout(() => {
            map2D.setView([finalLat, finalLng], zoom2D);
            map2D.invalidateSize();

            // Force tile refresh
            setTimeout(() => {
              map2D.eachLayer(function(layer) {
                if (layer instanceof L.TileLayer) {
                  layer.redraw();
                }
              });
              console.log('2D map centered at:', finalLat.toFixed(6), finalLng.toFixed(6));
            }, 100);
          }, 50);

          currentMode = '2D';
        }

        function switchTo3D(lat, lng, altitude) {
          document.getElementById('globeViz').style.display = 'block';
          document.getElementById('mapViz').style.display = 'none';
          document.getElementById('modeIndicator').textContent = '3D GLOBE MODE';
          document.getElementById('modeIndicator').style.background = 'rgba(0, 255, 255, 0.9)';

          world.pointOfView({ lat, lng, altitude }, 500);
          currentMode = '3D';
          console.log('Switched to 3D mode at altitude:', altitude);
        }

        // Monitor zoom level and switch modes
        world.onZoom((view) => {
          lastGlobeView = view;
          targetCoords = { lat: view.lat, lng: view.lng };
          console.log('Globe zoom - altitude:', view.altitude, 'lat:', view.lat.toFixed(4), 'lng:', view.lng.toFixed(4));

          if (view.altitude < ZOOM_THRESHOLD && currentMode === '3D') {
            console.log('Triggering switch to 2D at exact position:', view.lat, view.lng);
            switchTo2D(view.lat, view.lng);
          }
        });

        // Listen for user interactions
        const container = document.getElementById('globeViz');

        // Track mouse position for accurate zoom targeting
        let mousePos = null;
        container.addEventListener('mousemove', (e) => {
          mousePos = { x: e.clientX, y: e.clientY };
        });

        // Get lat/lng from screen coordinates
        function getLatLngFromScreen(x, y) {
          try {
            const scene = world.scene();
            const camera = world.camera();
            const raycaster = new THREE.Raycaster();
            const mouse = new THREE.Vector2(
              (x / window.innerWidth) * 2 - 1,
              -(y / window.innerHeight) * 2 + 1
            );

            raycaster.setFromCamera(mouse, camera);
            const intersects = raycaster.intersectObject(scene.children.find(c => c.type === 'Mesh'), true);

            if (intersects.length > 0) {
              const point = intersects[0].point;
              const lat = Math.asin(point.y / Math.sqrt(point.x * point.x + point.y * point.y + point.z * point.z)) * 180 / Math.PI;
              const lng = Math.atan2(point.x, point.z) * 180 / Math.PI;
              return { lat, lng };
            }
          } catch (e) {
            console.log('Could not get exact coordinates, using view center');
          }
          return null;
        }
        container.addEventListener('mousedown', resumeRotation);
        container.addEventListener('wheel', (e) => {
          resumeRotation();

          // Update target coords based on mouse position
          if (mousePos) {
            const coords = getLatLngFromScreen(mousePos.x, mousePos.y);
            if (coords) {
              targetCoords = coords;
              console.log('Updated target from mouse:', targetCoords.lat.toFixed(4), targetCoords.lng.toFixed(4));
            }
          }
        });
        container.addEventListener('touchstart', resumeRotation);

        // Start with auto-rotation enabled
        world.controls().autoRotate = true;

        console.log('Event listeners added');

        createRoutes();
        createShips();
        createPlanes();
        createSatellites();

        // Handle window resize
        window.addEventListener('resize', () => {
          world.width(window.innerWidth).height(window.innerHeight);
        });

        console.log('3D Globe fully initialized');
      } catch (error) {
        console.error('Error initializing Globe:', error);
        document.body.innerHTML = '<div style="color: red; padding: 20px; font-family: monospace;">ERROR: Failed to load 3D Globe<br>Check console for details<br><br>' + error.message + '</div>';
      }
    }, 500); // Wait 500ms for libraries to load
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
          <button
            onClick={() => {
              const newRotating = !isRotating;
              setIsRotating(newRotating);
              if (mapRef.current?.contentWindow) {
                (mapRef.current.contentWindow as any).toggleRotation?.(newRotating);
              }
            }}
            style={{
              background: isRotating ? '#0a0a0a' : '#1a1a1a',
              color: isRotating ? '#00ff00' : '#888',
              border: `1px solid ${isRotating ? '#00ff00' : '#444'}`,
              padding: '4px 10px',
              fontSize: '9px',
              cursor: 'pointer',
              fontWeight: 'bold',
              fontFamily: 'Consolas, monospace',
              transition: 'all 0.2s'
            }}
          >
            {isRotating ? '● ROTATING' : '○ PAUSED'}
          </button>
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
              <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '8px' }}>
                <div style={{ color: '#888', fontSize: '8px' }}>AIRCRAFT</div>
                <div style={{ color: 'rgba(255, 200, 100, 0.95)', fontSize: '16px', fontWeight: 'bold' }}>342</div>
              </div>
              <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '8px' }}>
                <div style={{ color: '#888', fontSize: '8px' }}>SATELLITES</div>
                <div style={{ color: 'rgba(138, 43, 226, 0.9)', fontSize: '16px', fontWeight: 'bold' }}>13</div>
              </div>
              <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '8px', gridColumn: 'span 2' }}>
                <div style={{ color: '#888', fontSize: '8px' }}>TRADE VOLUME (24H)</div>
                <div style={{ color: '#00ff00', fontSize: '16px', fontWeight: 'bold' }}>{intelligence.trade_volume}</div>
              </div>
              <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '8px' }}>
                <div style={{ color: '#888', fontSize: '8px' }}>MAJOR PORTS</div>
                <div style={{ color: '#ffa500', fontSize: '14px', fontWeight: 'bold' }}>6</div>
              </div>
              <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '8px' }}>
                <div style={{ color: '#888', fontSize: '8px' }}>AIRPORTS</div>
                <div style={{ color: '#9d4edd', fontSize: '14px', fontWeight: 'bold' }}>8</div>
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
            <div style={{ display: 'flex', gap: '6px', marginBottom: '6px' }}>
              <button
                onClick={() => {
                  setShowPlanes(!showPlanes);
                  if (mapRef.current?.contentWindow) {
                    (mapRef.current.contentWindow as any).togglePlanes?.();
                  }
                }}
                style={{
                  flex: 1,
                  background: showPlanes ? 'rgba(255, 200, 100, 0.9)' : '#0a0a0a',
                  color: showPlanes ? '#000' : 'rgba(255, 200, 100, 0.9)',
                  border: '1px solid rgba(255, 200, 100, 0.9)',
                  padding: '6px',
                  fontSize: '9px',
                  cursor: 'pointer',
                  fontWeight: 'bold'
                }}
              >
                AIRCRAFT
              </button>
              <button
                onClick={() => {
                  setShowSatellites(!showSatellites);
                  if (mapRef.current?.contentWindow) {
                    (mapRef.current.contentWindow as any).toggleSatellites?.();
                  }
                }}
                style={{
                  flex: 1,
                  background: showSatellites ? 'rgba(138, 43, 226, 0.9)' : '#0a0a0a',
                  color: showSatellites ? '#fff' : 'rgba(138, 43, 226, 0.9)',
                  border: '1px solid rgba(138, 43, 226, 0.9)',
                  padding: '6px',
                  fontSize: '9px',
                  cursor: 'pointer',
                  fontWeight: 'bold'
                }}
              >
                SATELLITES
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
            <div style={{ fontWeight: 'bold', marginBottom: '4px' }}>HYBRID 3D/2D SYSTEM ACTIVE</div>
            <div>CENTER: 20.0000° N, 75.0000° E</div>
            <div>MODE: Globe.GL 3D → Auto-switches → Leaflet 2D</div>
            <div style={{ marginTop: '4px', color: '#ffd700' }}>🔍 ZOOM: Deep zoom auto-switches to street-level map!</div>
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
                <span style={{ color: '#0ff' }}>●</span> 3D Globe: ACTIVE
              </div>
              <div style={{ marginBottom: '4px' }}>
                <span style={{ color: '#0ff' }}>●</span> Satellite imagery: ONLINE
              </div>
              <div style={{ marginBottom: '4px' }}>
                <span style={{ color: '#0ff' }}>●</span> AIS transponders: STREAMING
              </div>
              <div style={{ marginBottom: '4px' }}>
                <span style={{ color: 'rgba(138, 43, 226, 0.9)' }}>●</span> Orbital tracking: 13 SATs
              </div>
              <div style={{ marginBottom: '4px' }}>
                <span style={{ color: 'rgba(255, 200, 100, 0.95)' }}>●</span> Air traffic: MONITORED
              </div>
              <div style={{ marginBottom: '4px' }}>
                <span style={{ color: '#0ff' }}>●</span> Maritime routes: {tradeRoutes.length} corridors
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
          <span style={{ color: '#888', whiteSpace: 'nowrap' }}>Powered by Globe.GL 3D Visualization</span>
          <span style={{ color: '#888' }}>|</span>
          <span style={{ color: '#ffd700', whiteSpace: 'nowrap' }}>CLEARANCE: TOP SECRET // SCI</span>
        </div>
      </div>
    </div>
  );
}