// MaritimeWidget - Maritime Trade Intelligence Visualization
// NOTE: This widget displays a 3D globe visualization with sample trade routes.
// Real maritime data requires specialized APIs (MarineTraffic, VesselFinder, etc.)
// which are not currently integrated. The data shown is illustrative.

import React, { useRef, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';

interface TradeRoute {
  name: string;
  value: string;
  status: 'active' | 'delayed' | 'critical';
  vessels: number;
}

interface MaritimeWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

// Sample trade routes - illustrative data
// Real data would come from maritime APIs like MarineTraffic, VesselFinder, etc.
const TRADE_ROUTES: TradeRoute[] = [
  { name: 'Mumbai → Shanghai', value: 'Major Route', status: 'active', vessels: 89 },
  { name: 'Mumbai → Singapore', value: 'Major Route', status: 'active', vessels: 45 },
  { name: 'Mumbai → New York', value: 'Major Route', status: 'active', vessels: 56 },
  { name: 'Chennai → Tokyo', value: 'Active', status: 'active', vessels: 34 },
  { name: 'Mumbai → Dubai', value: 'Active', status: 'active', vessels: 12 }
];

export const MaritimeWidget: React.FC<MaritimeWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const { t } = useTranslation('dashboard');
  const mapRef = useRef<HTMLIFrameElement>(null);

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'critical': return 'var(--ft-color-alert)';
      case 'delayed': return 'var(--ft-color-warning)';
      case 'active': return 'var(--ft-color-success)';
      default: return 'var(--ft-color-text-muted)';
    }
  };

  // Compact globe HTML with animations
  const globeHTML = `
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8" />
  <style>
    body { margin: 0; padding: 0; background: #000; overflow: hidden; }
    #globeViz { height: 100%; width: 100%; background: #000; cursor: pointer; }
  </style>
</head>
<body>
  <div id="globeViz"></div>
  <script src="https://unpkg.com/three@0.160.0/build/three.min.js"></script>
  <script src="https://unpkg.com/globe.gl@2.27.2/dist/globe.gl.min.js"></script>
  <script>
    setTimeout(() => {
      try {
        const world = Globe()
          (document.getElementById('globeViz'))
          .globeImageUrl('https://unpkg.com/three-globe@2.31.1/example/img/earth-blue-marble.jpg')
          .bumpImageUrl('https://unpkg.com/three-globe@2.31.1/example/img/earth-topology.png')
          .backgroundColor('#000000')
          .backgroundImageUrl('https://unpkg.com/three-globe/example/img/night-sky.png')
          .showAtmosphere(true)
          .atmosphereColor('#00ffff')
          .atmosphereAltitude(0.12)
          .width(window.innerWidth)
          .height(window.innerHeight);

        const scene = world.scene();
        const ambientLight = new THREE.AmbientLight(0x808080, 0.7);
        const directionalLight = new THREE.DirectionalLight(0xaaaaaa, 0.6);
        directionalLight.position.set(5, 3, 5);
        scene.add(ambientLight);
        scene.add(directionalLight);

        world.pointOfView({ lat: 20, lng: 75, altitude: 2.0 }, 1000);

        // Trade routes
        const oceanRoutes = [
          { start: {lat: 18.9388, lng: 72.8354}, end: {lat: 51.9553, lng: 4.1392}, color: 'rgba(100, 180, 255, 0.6)', width: 0.8 },
          { start: {lat: 18.9388, lng: 72.8354}, end: {lat: 31.3548, lng: 121.6431}, color: 'rgba(120, 200, 255, 0.7)', width: 1.2 },
          { start: {lat: 18.9388, lng: 72.8354}, end: {lat: 1.2644, lng: 103.8224}, color: 'rgba(100, 180, 255, 0.65)', width: 1.0 },
          { start: {lat: 13.0827, lng: 80.2707}, end: {lat: 35.4437, lng: 139.6380}, color: 'rgba(150, 220, 255, 0.6)', width: 0.9 },
          { start: {lat: 22.5726, lng: 88.3639}, end: {lat: 22.2855, lng: 114.1577}, color: 'rgba(100, 180, 255, 0.55)', width: 0.8 },
          { start: {lat: 18.9388, lng: 72.8354}, end: {lat: 24.9857, lng: 55.0272}, color: 'rgba(255, 160, 100, 0.6)', width: 0.9 },
          { start: {lat: 18.9388, lng: 72.8354}, end: {lat: 40.6655, lng: -74.0781}, color: 'rgba(120, 200, 255, 0.75)', width: 1.2 },
          { start: {lat: 13.0827, lng: 80.2707}, end: {lat: -33.9544, lng: 151.2093}, color: 'rgba(130, 190, 255, 0.6)', width: 0.9 }
        ];

        const arcsData = oceanRoutes.map(route => ({
          startLat: route.start.lat,
          startLng: route.start.lng,
          endLat: route.end.lat,
          endLng: route.end.lng,
          color: route.color,
          width: route.width
        }));

        world.arcsData(arcsData)
          .arcColor('color')
          .arcStroke('width')
          .arcDashLength(1.0)
          .arcDashGap(0)
          .arcAltitude(0.1)
          .arcAltitudeAutoScale(0.5);

        // Ports
        const ports = [
          {lat: 18.9388, lng: 72.8354}, {lat: 31.3548, lng: 121.6431}, {lat: 1.2644, lng: 103.8224},
          {lat: 51.9553, lng: 4.1392}, {lat: 24.9857, lng: 55.0272}, {lat: 40.6655, lng: -74.0781}
        ];

        world.pointsData(ports)
          .pointAltitude(0.002)
          .pointRadius(0.6)
          .pointColor(() => 'rgba(255, 180, 100, 0.85)');

        // Ships
        const shipData = oceanRoutes.map((route, idx) => ({
          routeIdx: idx,
          progress: Math.random(),
          startLat: route.start.lat,
          startLng: route.start.lng,
          endLat: route.end.lat,
          endLng: route.end.lng
        }));

        function updateShips() {
          const shipPoints = shipData.map(ship => {
            const lat = ship.startLat + (ship.endLat - ship.startLat) * ship.progress;
            const lng = ship.startLng + (ship.endLng - ship.startLng) * ship.progress;
            return { lat, lng, size: 0.15, color: 'rgba(255, 255, 255, 0.95)', altitude: 0.003 };
          });
          world.customLayerData(shipPoints)
            .customThreeObject(() => {
              const geometry = new THREE.TetrahedronGeometry(0.7);
              const material = new THREE.MeshBasicMaterial({ color: 0x00ccff, transparent: true, opacity: 0.9 });
              return new THREE.Mesh(geometry, material);
            })
            .customThreeObjectUpdate((obj, d) => {
              Object.assign(obj.position, world.getCoords(d.lat, d.lng, d.altitude));
            });
        }

        updateShips();

        setInterval(() => {
          shipData.forEach(ship => {
            ship.progress = (ship.progress + 0.0002) % 1.0;
          });
          updateShips();
        }, 200);

        // Auto-rotate
        world.controls().autoRotate = true;
        world.controls().autoRotateSpeed = 0.3;
        world.controls().enableDamping = true;
        world.controls().dampingFactor = 0.05;

        // Click to notify parent
        document.getElementById('globeViz').addEventListener('click', () => {
          window.parent.postMessage('navigate-to-maritime', '*');
        });

        window.addEventListener('resize', () => {
          world.width(window.innerWidth).height(window.innerHeight);
        });
      } catch (error) {
        console.error('Globe error:', error);
      }
    }, 500);
  </script>
</body>
</html>
  `;

  // Listen for navigation message from iframe
  useEffect(() => {
    const handleMessage = (event: MessageEvent) => {
      if (event.data === 'navigate-to-maritime' && onNavigate) {
        onNavigate();
      }
    };
    window.addEventListener('message', handleMessage);
    return () => window.removeEventListener('message', handleMessage);
  }, [onNavigate]);

  return (
    <BaseWidget
      id={id}
      title={t('widgets.maritimeIntelligence')}
      onRemove={onRemove}
      headerColor="var(--ft-color-accent)"
    >
      <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
        {/* 3D Globe Section - Top 60% */}
        <div
          style={{
            flex: '0 0 60%',
            position: 'relative',
            overflow: 'hidden',
            cursor: 'pointer',
            borderBottom: '2px solid var(--ft-color-accent)'
          }}
          title="Click to open full Maritime Intelligence view"
        >
          <iframe
            ref={mapRef}
            srcDoc={globeHTML}
            style={{
              width: '100%',
              height: '100%',
              border: 'none',
              background: 'var(--ft-color-background)'
            }}
          />
          <div style={{
            position: 'absolute',
            top: '4px',
            right: '4px',
            background: 'rgba(0, 255, 255, 0.9)',
            color: '#000',
            padding: '2px 6px',
            fontSize: '7px',
            fontWeight: 'bold',
            borderRadius: '2px'
          }}>
            {t('widgets.globe3D')}
          </div>
        </div>

        {/* Trade Data Section - Bottom 40% */}
        <div style={{ flex: '0 0 40%', padding: '6px', fontSize: 'var(--ft-font-size-tiny)', overflowY: 'auto' }}>
          {/* Visualization notice */}
          <div style={{
            backgroundColor: 'rgba(0, 229, 255, 0.1)',
            border: '1px solid rgba(0, 229, 255, 0.3)',
            borderRadius: '2px',
            padding: '4px 6px',
            marginBottom: '6px',
            fontSize: '6px',
            color: 'var(--ft-color-accent)'
          }}>
            ℹ️ 3D Globe Visualization - Click for full Maritime tab
          </div>

          {/* Top Trade Routes */}
          <div style={{ color: 'var(--ft-color-accent)', fontSize: '7px', fontWeight: 'bold', marginBottom: '3px' }}>
            MAJOR TRADE CORRIDORS
          </div>
          {TRADE_ROUTES.map((route, idx) => (
            <div
              key={idx}
              style={{
                background: idx % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                padding: '3px',
                marginBottom: '2px',
                borderLeft: `2px solid ${getStatusColor(route.status)}`,
                paddingLeft: '4px'
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: '1px'
              }}>
                <span style={{ color: 'var(--ft-color-text)', fontSize: '7px', fontWeight: 'bold' }}>
                  {route.name}
                </span>
                <span style={{
                  color: getStatusColor(route.status),
                  fontSize: '5px',
                  padding: '1px 2px',
                  border: `1px solid ${getStatusColor(route.status)}`
                }}>
                  {route.status.toUpperCase()}
                </span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '6px' }}>
                <span style={{ color: 'var(--ft-color-text-muted)' }}>{route.value}</span>
                <span style={{ color: 'var(--ft-color-accent)' }}>{route.vessels} ships</span>
              </div>
            </div>
          ))}
        </div>
      </div>
    </BaseWidget>
  );
};
