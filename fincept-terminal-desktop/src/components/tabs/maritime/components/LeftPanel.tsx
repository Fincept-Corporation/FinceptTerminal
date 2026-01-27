// Maritime Tab Left Panel - Intelligence Stats, Trade Routes, Map Controls

import React from 'react';
import type { IntelligenceData, TradeRoute } from '../types';

interface LeftPanelProps {
  intelligence: IntelligenceData;
  realVesselsCount: number;
  isLoadingVessels: boolean;
  vesselLoadError: string | null;
  tradeRoutes: TradeRoute[];
  selectedRoute: TradeRoute | null;
  onSelectRoute: (route: TradeRoute | null) => void;
  showRoutes: boolean;
  showShips: boolean;
  showPlanes: boolean;
  showSatellites: boolean;
  onToggleRoutes: () => void;
  onToggleShips: () => void;
  onTogglePlanes: () => void;
  onToggleSatellites: () => void;
  onClearMarkers: () => void;
  onLoadVessels: () => void;
}

export const LeftPanel: React.FC<LeftPanelProps> = ({
  intelligence,
  realVesselsCount,
  isLoadingVessels,
  vesselLoadError,
  tradeRoutes,
  selectedRoute,
  onSelectRoute,
  showRoutes,
  showShips,
  showPlanes,
  showSatellites,
  onToggleRoutes,
  onToggleShips,
  onTogglePlanes,
  onToggleSatellites,
  onClearMarkers,
  onLoadVessels
}) => {
  return (
    <div style={{
      width: '280px',
      borderRight: '1px solid #1a1a1a',
      display: 'flex',
      flexDirection: 'column',
      background: '#000',
      flexShrink: 0
    }}>
      {/* Intelligence Stats */}
      <div style={{ padding: '12px', borderBottom: '1px solid #1a1a1a' }}>
        <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>
          GLOBAL INTEL
        </div>

        {isLoadingVessels && (
          <div style={{
            background: 'rgba(0, 255, 255, 0.1)',
            border: '1px solid #0ff',
            padding: '6px',
            fontSize: '8px',
            color: '#0ff',
            marginBottom: '8px',
            textAlign: 'center'
          }}>
            ⏳ LOADING VESSELS...
          </div>
        )}

        {vesselLoadError && (
          <div style={{
            background: 'rgba(255, 0, 0, 0.1)',
            border: '1px solid #ff0000',
            padding: '6px',
            fontSize: '8px',
            color: '#ff0000',
            marginBottom: '8px'
          }}>
            ⚠ {vesselLoadError}
          </div>
        )}

        {/* Load Vessels Button */}
        <button
          onClick={onLoadVessels}
          disabled={isLoadingVessels}
          style={{
            width: '100%',
            background: isLoadingVessels ? '#1a1a1a' : '#0a0a0a',
            color: isLoadingVessels ? '#888' : '#0ff',
            border: '1px solid #0ff',
            padding: '8px',
            fontSize: '10px',
            cursor: isLoadingVessels ? 'not-allowed' : 'pointer',
            fontWeight: 'bold',
            marginBottom: '8px'
          }}
        >
          {isLoadingVessels ? '⏳ LOADING...' : realVesselsCount > 0 ? '🔄 REFRESH VESSELS' : '📡 LOAD VESSEL DATA'}
        </button>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <StatBox label="TOTAL VESSELS" value={intelligence.active_vessels} color="#0ff" />
          <StatBox label="DISPLAYED" value={realVesselsCount} color="#00ff00" />
          <StatBox label="ROUTES" value={intelligence.monitored_routes} color="#0ff" />
          <StatBox label="AIRCRAFT" value={342} color="rgba(255, 200, 100, 0.95)" />
          <StatBox label="SATELLITES" value={13} color="rgba(138, 43, 226, 0.9)" />
          <div style={{
            background: '#0a0a0a',
            border: '1px solid #1a1a1a',
            padding: '8px',
            gridColumn: 'span 2'
          }}>
            <div style={{ color: '#888', fontSize: '8px' }}>TRADE VOLUME (24H)</div>
            <div style={{ color: '#00ff00', fontSize: '16px', fontWeight: 'bold' }}>
              {intelligence.trade_volume}
            </div>
          </div>
          <StatBox label="MAJOR PORTS" value={6} color="#ffa500" fontSize="14px" />
          <StatBox label="AIRPORTS" value={8} color="#9d4edd" fontSize="14px" />
        </div>
      </div>

      {/* Trade Routes List */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '12px', minHeight: 0 }}>
        <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>
          TRADE CORRIDORS
        </div>
        {tradeRoutes.map((route, idx) => (
          <RouteItem
            key={idx}
            route={route}
            isSelected={selectedRoute?.name === route.name}
            onClick={() => onSelectRoute(route)}
          />
        ))}
      </div>

      {/* Map Controls */}
      <div style={{ padding: '12px', borderTop: '1px solid #1a1a1a' }}>
        <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>
          MAP CONTROLS
        </div>
        <div style={{ display: 'flex', gap: '6px', marginBottom: '6px' }}>
          <ToggleButton label="ROUTES" active={showRoutes} onClick={onToggleRoutes} color="#0ff" />
          <ToggleButton label="VESSELS" active={showShips} onClick={onToggleShips} color="#0ff" />
        </div>
        <div style={{ display: 'flex', gap: '6px', marginBottom: '6px' }}>
          <ToggleButton label="AIRCRAFT" active={showPlanes} onClick={onTogglePlanes} color="rgba(255, 200, 100, 0.9)" />
          <ToggleButton label="SATELLITES" active={showSatellites} onClick={onToggleSatellites} color="rgba(138, 43, 226, 0.9)" textColor={showSatellites ? '#fff' : undefined} />
        </div>
        <button
          onClick={onClearMarkers}
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
  );
};

// Sub-components
interface StatBoxProps {
  label: string;
  value: number | string;
  color: string;
  fontSize?: string;
}

const StatBox: React.FC<StatBoxProps> = ({ label, value, color, fontSize = '16px' }) => (
  <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '8px' }}>
    <div style={{ color: '#888', fontSize: '8px' }}>{label}</div>
    <div style={{ color, fontSize, fontWeight: 'bold' }}>{value}</div>
  </div>
);

interface RouteItemProps {
  route: TradeRoute;
  isSelected: boolean;
  onClick: () => void;
}

const RouteItem: React.FC<RouteItemProps> = ({ route, isSelected, onClick }) => {
  const getStatusColor = (status: TradeRoute['status']) => {
    switch (status) {
      case 'critical': return '#ff0000';
      case 'delayed': return '#ffd700';
      default: return '#00ff00';
    }
  };

  const statusColor = getStatusColor(route.status);

  return (
    <div
      onClick={onClick}
      style={{
        background: isSelected ? '#1a1a1a' : '#0a0a0a',
        border: `1px solid ${route.status === 'critical' ? '#ff0000' : route.status === 'delayed' ? '#ffd700' : '#1a1a1a'}`,
        padding: '8px',
        marginBottom: '6px',
        cursor: 'pointer',
        transition: 'all 0.2s'
      }}
      onMouseEnter={(e) => (e.currentTarget.style.background = '#1a1a1a')}
      onMouseLeave={(e) => (e.currentTarget.style.background = isSelected ? '#1a1a1a' : '#0a0a0a')}
    >
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
        <span style={{ color: '#0ff', fontSize: '9px', fontWeight: 'bold' }}>{route.name}</span>
        <span style={{
          color: statusColor,
          fontSize: '7px',
          padding: '2px 4px',
          background: '#000',
          border: `1px solid ${statusColor}`
        }}>
          {route.status.toUpperCase()}
        </span>
      </div>
      <div style={{ display: 'flex', justifyContent: 'space-between' }}>
        <span style={{ color: '#888', fontSize: '8px' }}>Value: {route.value}</span>
        <span style={{ color: '#888', fontSize: '8px' }}>Ships: {route.vessels}</span>
      </div>
    </div>
  );
};

interface ToggleButtonProps {
  label: string;
  active: boolean;
  onClick: () => void;
  color: string;
  textColor?: string;
}

const ToggleButton: React.FC<ToggleButtonProps> = ({ label, active, onClick, color, textColor }) => (
  <button
    onClick={onClick}
    style={{
      flex: 1,
      background: active ? color : '#0a0a0a',
      color: active ? (textColor || '#000') : color,
      border: `1px solid ${color}`,
      padding: '6px',
      fontSize: '9px',
      cursor: 'pointer',
      fontWeight: 'bold'
    }}
  >
    {label}
  </button>
);
