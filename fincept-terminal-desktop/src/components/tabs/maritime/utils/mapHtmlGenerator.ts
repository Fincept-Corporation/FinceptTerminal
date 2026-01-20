// Generates the HTML content for the 3D Globe / 2D Map visualization

import { OCEAN_ROUTES, AIR_ROUTES } from '../constants';

export function generateMapHTML(): string {
  // Serialize routes for injection into the HTML
  const oceanRoutesJson = JSON.stringify(OCEAN_ROUTES);
  const airRoutesJson = JSON.stringify(AIR_ROUTES);

  return `
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

    // Route data injected from React
    var oceanRoutes = ${oceanRoutesJson};
    var airRoutes = ${airRoutesJson};

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

        // Initialize 2D Leaflet map (lazy)
        let map2D = null;

        function initMap2D() {
          if (!map2D) {
            map2D = L.map('mapViz', {
              zoomControl: true,
              attributionControl: false,
              preferCanvas: true,
              scrollWheelZoom: true,
              doubleClickZoom: true,
              touchZoom: true,
              boxZoom: true,
              keyboard: true,
              dragging: true,
              maxZoom: 18,
              minZoom: 3
            }).setView([19.0, 72.8], 11);

            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
              attribution: 'FINCEPT MARITIME INTELLIGENCE',
              maxZoom: 18,
              minZoom: 3
            }).addTo(map2D);

            setTimeout(() => {
              map2D.invalidateSize();
              console.log('2D map resized and tiles loading');
            }, 100);

            console.log('2D map initialized');
          }
        }

        // Professional balanced lighting
        const scene = world.scene();
        const ambientLight = new THREE.AmbientLight(0x808080, 0.7);
        const directionalLight = new THREE.DirectionalLight(0xaaaaaa, 0.6);
        directionalLight.position.set(5, 3, 5);
        scene.add(ambientLight);
        scene.add(directionalLight);

        // Set initial point of view
        world.pointOfView({ lat: 20, lng: 75, altitude: 2.0 }, 1000);

        // State variables
        let currentMode = '3D';
        let lastGlobeView = { lat: 20, lng: 75, altitude: 2.0 };
        let targetCoords = { lat: 20, lng: 75 };

        var markers = [];
        var allArcs = [];
        var allLabels = [];
        var planeLabels = [];
        var showRoutesFlag = true;
        var showShipsFlag = true;
        var showPlanesFlag = true;
        var showSatellitesFlag = true;
        var orbitalPathsData = [];

        function createRoutes() {
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
            .arcStroke((d) => d.width)
            .arcDashLength(1.0)
            .arcDashGap(0)
            .arcDashInitialGap(0)
            .arcDashAnimateTime(0)
            .arcAltitude((d) => d.type === 'air' ? 0.15 : 0.1)
            .arcAltitudeAutoScale(0.5)
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
            color: 'rgba(255, 180, 100, 0.85)',
            label: name,
            type: 'port'
          }));

          const airportPoints = Array.from(uniqueAirports.entries()).map(([name, data]) => ({
            lat: data.lat,
            lng: data.lng,
            size: 0.5,
            color: 'rgba(180, 100, 255, 0.85)',
            label: name,
            type: 'airport'
          }));

          const allPoints = [...portPoints, ...airportPoints];

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

          function interpolateArc(startLat, startLng, endLat, endLng, progress) {
            const lat = startLat + (endLat - startLat) * progress;
            const lng = startLng + (endLng - startLng) * progress;
            return { lat, lng };
          }

          function generateShipPoints(shipsData) {
            return shipsData.map(ship => {
              const pos = interpolateArc(ship.startLat, ship.startLng, ship.endLat, ship.endLng, ship.progress);
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

          const initialShipPoints = generateShipPoints(shipData);
          world.customLayerData(initialShipPoints)
            .customThreeObject(d => {
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

          setInterval(() => {
            shipData.forEach(ship => {
              ship.progress = (ship.progress + 0.0002) % 1.0;
            });
            const updatedShipPoints = generateShipPoints(shipData);
            world.customLayerData(updatedShipPoints);
          }, 200);
        }

        function createPlanes() {
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

          function interpolateArc(startLat, startLng, endLat, endLng, progress) {
            const lat = startLat + (endLat - startLat) * progress;
            const lng = startLng + (endLng - startLng) * progress;
            return { lat, lng };
          }

          function generatePlanePoints(planesData) {
            return planesData.map(plane => {
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
          }

          planeLabels = planeData;

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

          setInterval(() => {
            planeData.forEach(plane => {
              plane.progress = (plane.progress + 0.0004) % 1.0;
            });
            const updatedPlanePoints = generatePlanePoints(planeData);
            world.htmlElementsData(updatedPlanePoints);
          }, 200);
        }

        function createSatellites() {
          const satelliteOrbits = [
            { name: 'GPS-1', inclination: 55, altitude: 0.35, speed: 0.0008, color: '#00ff00', orbitId: 'gps' },
            { name: 'GPS-2', inclination: 55, altitude: 0.35, speed: 0.0008, color: '#00ff00', phaseOffset: Math.PI, orbitId: 'gps' },
            { name: 'IRNSS-1', inclination: 29, altitude: 0.37, speed: 0.0007, color: '#ffd700', orbitId: 'irnss' },
            { name: 'IRNSS-2', inclination: 29, altitude: 0.37, speed: 0.0007, color: '#ffd700', phaseOffset: Math.PI / 2, orbitId: 'irnss' },
            { name: 'Starlink-1', inclination: 53, altitude: 0.32, speed: 0.0009, color: '#00ccff', orbitId: 'starlink' },
            { name: 'Starlink-2', inclination: 53, altitude: 0.32, speed: 0.0009, color: '#00ccff', phaseOffset: 2 * Math.PI / 3, orbitId: 'starlink' },
            { name: 'Starlink-3', inclination: 53, altitude: 0.32, speed: 0.0009, color: '#00ccff', phaseOffset: 4 * Math.PI / 3, orbitId: 'starlink' },
            { name: 'GLONASS-1', inclination: 64.8, altitude: 0.42, speed: 0.0006, color: '#ff6b6b', orbitId: 'glonass' },
            { name: 'GLONASS-2', inclination: 64.8, altitude: 0.42, speed: 0.0006, color: '#ff6b6b', phaseOffset: Math.PI / 3, orbitId: 'glonass' },
            { name: 'Galileo-1', inclination: 56, altitude: 0.40, speed: 0.0007, color: '#9d4edd', orbitId: 'galileo' },
            { name: 'Galileo-2', inclination: 56, altitude: 0.40, speed: 0.0007, color: '#9d4edd', phaseOffset: Math.PI / 2, orbitId: 'galileo' },
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
              const numPoints = 100;
              const pathPoints = [];

              for (let i = 0; i <= numPoints; i++) {
                const angle = (i / numPoints) * 2 * Math.PI;
                const inclinationRad = (orbit.inclination * Math.PI) / 180;

                const x = Math.cos(angle);
                const y = Math.sin(angle) * Math.cos(inclinationRad);
                const z = Math.sin(angle) * Math.sin(inclinationRad);

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
          orbitalPathsData = orbitalPaths;

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

          const satelliteData = satellites.map((sat, idx) => ({
            ...sat,
            progress: (idx / satellites.length)
          }));

          function generateSatellitePoints(satsData) {
            return satsData.map(sat => {
              const angle = sat.progress * 2 * Math.PI;
              const inclinationRad = (sat.inclination * Math.PI) / 180;

              const x = Math.cos(angle);
              const y = Math.sin(angle) * Math.cos(inclinationRad);
              const z = Math.sin(angle) * Math.sin(inclinationRad);

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
              const canvas = document.createElement('canvas');
              canvas.width = 128;
              canvas.height = 128;
              const ctx = canvas.getContext('2d');

              ctx.beginPath();
              ctx.arc(64, 64, 50, 0, 2 * Math.PI);
              ctx.fillStyle = d.color;
              ctx.globalAlpha = 0.4;
              ctx.fill();
              ctx.globalAlpha = 1.0;

              ctx.strokeStyle = d.color;
              ctx.lineWidth = 3;
              ctx.stroke();

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

          setInterval(() => {
            if (showSatellitesFlag) {
              satelliteData.forEach(sat => {
                sat.progress = (sat.progress + sat.speed * 0.15) % 1.0;
              });
              const updatedSatellitePoints = generateSatellitePoints(satelliteData);
              world.objectsData(updatedSatellitePoints);
            }
          }, 100);
        }

        window.addMarker = function(lat, lng, title, type) {
          const icons = { 'Ship': '🚢', 'Port': '⚓', 'Industry': '🏭', 'Bank': '🏦', 'Exchange': '💱' };
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

        // Real vessel data handling
        var realVesselPoints = [];
        var vesselTexture = null;

        function createVesselTexture() {
          if (!vesselTexture) {
            const canvas = document.createElement('canvas');
            canvas.width = 64;
            canvas.height = 64;
            const ctx = canvas.getContext('2d');
            ctx.font = '48px Arial';
            ctx.fillText('🚢', 8, 48);
            vesselTexture = new THREE.CanvasTexture(canvas);
          }
          return vesselTexture;
        }

        window.updateRealVessels = function(vessels) {
          console.log('Updating real vessels:', vessels.length);

          if (!vessels || vessels.length === 0) {
            world.customLayerData([]);
            realVesselPoints = [];
            return;
          }

          realVesselPoints = vessels.map(vessel => ({
            lat: vessel.lat,
            lng: vessel.lng,
            size: 0.15,
            color: 'rgba(0, 255, 255, 0.9)',
            altitude: 0.003,
            name: vessel.name,
            imo: vessel.imo,
            speed: vessel.speed,
            angle: vessel.angle,
            from_port: vessel.from_port,
            to_port: vessel.to_port,
            route_progress: vessel.route_progress
          }));

          if (showShipsFlag) {
            const texture = createVesselTexture();

            world.customLayerData(realVesselPoints)
              .customThreeObject(d => {
                const material = new THREE.SpriteMaterial({
                  map: texture,
                  transparent: true,
                  opacity: 0.9,
                  sizeAttenuation: true
                });
                const sprite = new THREE.Sprite(material);
                sprite.scale.set(1.5, 1.5, 1);
                return sprite;
              })
              .customThreeObjectUpdate((obj, d) => {
                const coords = world.getCoords(d.lat, d.lng, d.altitude);
                if (coords && coords.x !== undefined && coords.y !== undefined && coords.z !== undefined) {
                  Object.assign(obj.position, coords);
                }
              });
          }
        };

        // Mode switching
        var vesselMarkers = [];
        window.switchMapMode = function(mode, vesselsFromReact) {
          console.log('Switching to mode:', mode);

          if (mode === '2D') {
            initMap2D();

            document.getElementById('globeViz').style.display = 'none';
            document.getElementById('mapViz').style.display = 'block';
            document.getElementById('modeIndicator').textContent = '2D MAP MODE - Real Vessels Only';
            document.getElementById('modeIndicator').style.background = 'rgba(255, 215, 0, 0.9)';

            setTimeout(() => {
              if (map2D) {
                map2D.setView([19.0, 72.8], 11);
                map2D.invalidateSize();

                const vesselsToRender = vesselsFromReact || realVesselPoints;
                if (vesselsToRender && vesselsToRender.length > 0) {
                  updateVesselsOn2DMap(vesselsToRender);
                }
              }
            }, 200);

          } else {
            document.getElementById('globeViz').style.display = 'block';
            document.getElementById('mapViz').style.display = 'none';
            document.getElementById('modeIndicator').textContent = '3D GLOBE MODE';
            document.getElementById('modeIndicator').style.background = 'rgba(0, 255, 255, 0.9)';

            vesselMarkers.forEach(marker => marker.remove());
            vesselMarkers = [];
          }
        };

        function updateVesselsOn2DMap(vessels) {
          if (!map2D || !vessels || vessels.length === 0) return;

          vesselMarkers.forEach(marker => marker.remove());
          vesselMarkers = [];

          const shipIcon = L.divIcon({
            className: 'vessel-marker',
            html: '<div style="font-size: 16px;">🚢</div>',
            iconSize: [20, 20],
            iconAnchor: [10, 10]
          });

          const vesselsToDisplay = vessels.slice(0, 500);

          vesselsToDisplay.forEach(vessel => {
            const marker = L.marker([vessel.lat, vessel.lng], { icon: shipIcon })
              .bindPopup(\`
                <div style="font-family: 'Consolas', monospace; font-size: 11px;">
                  <div style="font-weight: bold; color: #0ff; margin-bottom: 4px;">\${vessel.name}</div>
                  <div style="font-size: 9px;">
                    <div><strong>IMO:</strong> \${vessel.imo}</div>
                    <div><strong>Position:</strong> \${vessel.lat.toFixed(4)}°, \${vessel.lng.toFixed(4)}°</div>
                    <div><strong>Speed:</strong> \${vessel.speed} kts</div>
                    \${vessel.from_port ? \`<div><strong>From:</strong> \${vessel.from_port}</div>\` : ''}
                    \${vessel.to_port ? \`<div><strong>To:</strong> \${vessel.to_port}</div>\` : ''}
                  </div>
                </div>
              \`)
              .addTo(map2D);

            vesselMarkers.push(marker);
          });
        }

        // Toggle functions
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
            if (realVesselPoints.length > 0) {
              window.updateRealVessels(realVesselPoints.map(p => ({
                lat: p.lat, lng: p.lng, name: p.name, imo: p.imo,
                speed: p.speed, angle: p.angle, from_port: p.from_port, to_port: p.to_port
              })));
            }
          }
          showShipsFlag = !showShipsFlag;
        };

        window.togglePlanes = function() {
          if (showPlanesFlag) {
            world.htmlElementsData([]);
          } else {
            function interpolateArc(startLat, startLng, endLat, endLng, progress) {
              const lat = startLat + (endLat - startLat) * progress;
              const lng = startLng + (endLng - startLng) * progress;
              return { lat, lng };
            }

            const planePoints = planeLabels.map(plane => {
              const pos = interpolateArc(plane.startLat, plane.startLng, plane.endLat, plane.endLng, plane.progress);
              return {
                lat: pos.lat, lng: pos.lng, size: 0.15,
                color: 'rgba(255, 200, 100, 0.95)', altitude: 0.01,
                label: plane.label, value: plane.value, flights: plane.flights
              };
            });
            world.htmlElementsData(planePoints);
          }
          showPlanesFlag = !showPlanesFlag;
        };

        let userPausedRotation = false;

        window.toggleRotation = function(shouldRotate) {
          userPausedRotation = !shouldRotate;
          world.controls().autoRotate = shouldRotate;
          console.log('Globe rotation:', shouldRotate ? 'enabled' : 'disabled');
        };

        window.toggleSatellites = function() {
          showSatellitesFlag = !showSatellitesFlag;
          if (!showSatellitesFlag) {
            world.objectsData([]);
            world.pathsData([]);
          } else {
            world.pathsData(orbitalPathsData);
          }
        };

        // Configure controls
        world.controls().autoRotate = true;
        world.controls().autoRotateSpeed = 0.2;
        world.controls().enableDamping = true;
        world.controls().dampingFactor = 0.05;
        world.controls().minDistance = 150;
        world.controls().maxDistance = 800;
        world.controls().zoomSpeed = 1.2;
        world.controls().enableZoom = true;

        let interactionTimeout;
        const resumeRotation = () => {
          clearTimeout(interactionTimeout);
          world.controls().autoRotate = false;
          interactionTimeout = setTimeout(() => {
            if (!userPausedRotation) {
              world.controls().autoRotate = true;
            }
          }, 3000);
        };

        world.onZoom((view) => {
          lastGlobeView = view;
          targetCoords = { lat: view.lat, lng: view.lng };
        });

        const container = document.getElementById('globeViz');
        container.addEventListener('mousedown', resumeRotation);
        container.addEventListener('wheel', resumeRotation);
        container.addEventListener('touchstart', resumeRotation);

        world.controls().autoRotate = true;

        // Initialize visualizations
        createRoutes();
        createShips();
        createPlanes();
        createSatellites();

        window.addEventListener('resize', () => {
          world.width(window.innerWidth).height(window.innerHeight);
        });

        console.log('3D Globe fully initialized');
      } catch (error) {
        console.error('Error initializing Globe:', error);
        document.body.innerHTML = '<div style="color: red; padding: 20px; font-family: monospace;">ERROR: Failed to load 3D Globe<br>Check console for details<br><br>' + error.message + '</div>';
      }
    }, 500);
  </script>
</body>
</html>
  `;
}
