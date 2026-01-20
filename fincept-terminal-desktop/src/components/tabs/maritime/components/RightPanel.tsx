// Maritime Tab Right Panel - Route Details, Vessel Search, Markers

import React from 'react';
import type { VesselData } from '@/services/maritime/marineApi';
import type { TradeRoute, MarkerData, PresetLocation } from '../types';

interface RightPanelProps {
  selectedRoute: TradeRoute | null;
  // Vessel Search
  searchImo: string;
  onSearchImoChange: (imo: string) => void;
  onSearchVessel: () => void;
  searchingVessel: boolean;
  searchResult: VesselData | null;
  searchError: string | null;
  // Markers
  markerType: MarkerData['type'];
  onMarkerTypeChange: (type: MarkerData['type']) => void;
  presetLocations: PresetLocation[];
  onAddPreset: (preset: PresetLocation) => void;
  markers: MarkerData[];
  tradeRoutesCount: number;
}

export const RightPanel: React.FC<RightPanelProps> = ({
  selectedRoute,
  searchImo,
  onSearchImoChange,
  onSearchVessel,
  searchingVessel,
  searchResult,
  searchError,
  markerType,
  onMarkerTypeChange,
  presetLocations,
  onAddPreset,
  markers,
  tradeRoutesCount
}) => {
  return (
    <div style={{
      width: '280px',
      borderLeft: '1px solid #1a1a1a',
      display: 'flex',
      flexDirection: 'column',
      background: '#000',
      flexShrink: 0
    }}>
      {/* Selected Route Details */}
      {selectedRoute && (
        <RouteDetails route={selectedRoute} />
      )}

      {/* Vessel Search by IMO */}
      <VesselSearch
        searchImo={searchImo}
        onSearchImoChange={onSearchImoChange}
        onSearch={onSearchVessel}
        searching={searchingVessel}
        result={searchResult}
        error={searchError}
      />

      {/* Add Markers */}
      <MarkerControls
        markerType={markerType}
        onMarkerTypeChange={onMarkerTypeChange}
        presetLocations={presetLocations}
        onAddPreset={onAddPreset}
      />

      {/* Markers List */}
      <MarkersList markers={markers} />

      {/* System Status */}
      <SystemStatus tradeRoutesCount={tradeRoutesCount} />
    </div>
  );
};

// Sub-components
interface RouteDetailsProps {
  route: TradeRoute;
}

const RouteDetails: React.FC<RouteDetailsProps> = ({ route }) => {
  const getStatusColor = (status: TradeRoute['status']) => {
    switch (status) {
      case 'critical': return '#ff0000';
      case 'delayed': return '#ffd700';
      default: return '#00ff00';
    }
  };

  return (
    <div style={{ padding: '12px', borderBottom: '1px solid #1a1a1a' }}>
      <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>
        ROUTE DETAILS
      </div>
      <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '10px' }}>
        <div style={{ color: '#0ff', fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
          {route.name}
        </div>
        <div style={{ display: 'grid', gap: '6px', fontSize: '9px' }}>
          <DetailRow label="Value:" value={route.value} />
          <DetailRow
            label="Status:"
            value={route.status.toUpperCase()}
            valueColor={getStatusColor(route.status)}
          />
          <DetailRow label="Active Vessels:" value={route.vessels.toString()} />
          <DetailRow label="Waypoints:" value={route.coordinates.length.toString()} />
        </div>
      </div>
    </div>
  );
};

interface DetailRowProps {
  label: string;
  value: string;
  valueColor?: string;
}

const DetailRow: React.FC<DetailRowProps> = ({ label, value, valueColor = '#fff' }) => (
  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
    <span style={{ color: '#888' }}>{label}</span>
    <span style={{ color: valueColor }}>{value}</span>
  </div>
);

interface VesselSearchProps {
  searchImo: string;
  onSearchImoChange: (imo: string) => void;
  onSearch: () => void;
  searching: boolean;
  result: VesselData | null;
  error: string | null;
}

const VesselSearch: React.FC<VesselSearchProps> = ({
  searchImo,
  onSearchImoChange,
  onSearch,
  searching,
  result,
  error
}) => (
  <div style={{ padding: '12px', borderBottom: '1px solid #1a1a1a' }}>
    <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>
      VESSEL SEARCH
    </div>

    <div style={{ marginBottom: '8px' }}>
      <div style={{ color: '#888', fontSize: '8px', marginBottom: '4px' }}>IMO NUMBER</div>
      <div style={{ display: 'flex', gap: '4px' }}>
        <input
          type="text"
          value={searchImo}
          onChange={(e) => onSearchImoChange(e.target.value)}
          onKeyPress={(e) => e.key === 'Enter' && onSearch()}
          placeholder="e.g., 8020604"
          style={{
            flex: 1,
            background: '#0a0a0a',
            color: '#0ff',
            border: '1px solid #1a1a1a',
            padding: '6px',
            fontSize: '9px',
            outline: 'none'
          }}
        />
        <button
          onClick={onSearch}
          disabled={searching}
          style={{
            background: searching ? '#1a1a1a' : '#0ff',
            color: searching ? '#888' : '#000',
            border: '1px solid #0ff',
            padding: '6px 12px',
            fontSize: '9px',
            cursor: searching ? 'not-allowed' : 'pointer',
            fontWeight: 'bold'
          }}
        >
          {searching ? 'SEARCHING...' : 'TRACK'}
        </button>
      </div>
    </div>

    {error && (
      <div style={{
        background: 'rgba(255, 0, 0, 0.1)',
        border: '1px solid #ff0000',
        padding: '6px',
        fontSize: '8px',
        color: '#ff0000',
        marginBottom: '8px'
      }}>
        ⚠ {error}
      </div>
    )}

    {result && (
      <div style={{
        background: '#0a0a0a',
        border: '1px solid #0ff',
        padding: '8px',
        fontSize: '9px',
        marginBottom: '8px'
      }}>
        <div style={{ color: '#0ff', fontWeight: 'bold', marginBottom: '6px' }}>{result.name}</div>
        <div style={{ display: 'grid', gap: '4px', color: '#fff' }}>
          <DetailRow label="IMO:" value={result.imo} />
          <DetailRow
            label="Position:"
            value={`${parseFloat(result.last_pos_latitude).toFixed(4)}°, ${parseFloat(result.last_pos_longitude).toFixed(4)}°`}
          />
          <DetailRow label="Speed:" value={`${result.last_pos_speed} kts`} />
          {result.route_from_port_name && (
            <DetailRow label="From:" value={result.route_from_port_name} />
          )}
          {result.route_to_port_name && (
            <DetailRow label="To:" value={result.route_to_port_name} />
          )}
        </div>
      </div>
    )}
  </div>
);

interface MarkerControlsProps {
  markerType: MarkerData['type'];
  onMarkerTypeChange: (type: MarkerData['type']) => void;
  presetLocations: PresetLocation[];
  onAddPreset: (preset: PresetLocation) => void;
}

const MarkerControls: React.FC<MarkerControlsProps> = ({
  markerType,
  onMarkerTypeChange,
  presetLocations,
  onAddPreset
}) => (
  <div style={{ padding: '12px', borderBottom: '1px solid #1a1a1a' }}>
    <div style={{ color: '#00ff00', fontSize: '10px', fontWeight: 'bold', marginBottom: '8px', letterSpacing: '1px' }}>
      ADD TARGET
    </div>

    <div style={{ marginBottom: '8px' }}>
      <div style={{ color: '#888', fontSize: '8px', marginBottom: '4px' }}>MARKER TYPE</div>
      <select
        value={markerType}
        onChange={(e) => onMarkerTypeChange(e.target.value as MarkerData['type'])}
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
          onClick={() => onAddPreset(preset)}
          style={{
            background: '#0a0a0a',
            color: '#fff',
            border: '1px solid #1a1a1a',
            padding: '6px',
            fontSize: '9px',
            cursor: 'pointer',
            textAlign: 'left'
          }}
          onMouseEnter={(e) => (e.currentTarget.style.borderColor = '#0ff')}
          onMouseLeave={(e) => (e.currentTarget.style.borderColor = '#1a1a1a')}
        >
          {preset.name}
        </button>
      ))}
    </div>
  </div>
);

interface MarkersListProps {
  markers: MarkerData[];
}

const MarkersList: React.FC<MarkersListProps> = ({ markers }) => (
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
            <span style={{ color: '#666', fontSize: '8px' }}>
              {marker.lat.toFixed(2)}°, {marker.lng.toFixed(2)}°
            </span>
          </div>
        </div>
      ))
    )}
  </div>
);

interface SystemStatusProps {
  tradeRoutesCount: number;
}

const SystemStatus: React.FC<SystemStatusProps> = ({ tradeRoutesCount }) => (
  <div style={{ padding: '12px', borderTop: '1px solid #1a1a1a', background: '#0a0a0a' }}>
    <div style={{ color: '#00ff00', fontSize: '9px', fontWeight: 'bold', marginBottom: '6px' }}>
      SYSTEM STATUS
    </div>
    <div style={{ fontSize: '8px', color: '#888', lineHeight: '1.4' }}>
      <StatusLine color="#0ff" text="3D Globe: ACTIVE" />
      <StatusLine color="#0ff" text="Satellite imagery: ONLINE" />
      <StatusLine color="#0ff" text="AIS transponders: STREAMING" />
      <StatusLine color="rgba(138, 43, 226, 0.9)" text="Orbital tracking: 13 SATs" />
      <StatusLine color="rgba(255, 200, 100, 0.95)" text="Air traffic: MONITORED" />
      <StatusLine color="#0ff" text={`Maritime routes: ${tradeRoutesCount} corridors`} />
      <div style={{
        marginTop: '8px',
        padding: '6px',
        background: '#000',
        border: '1px solid #1a1a1a',
        color: '#ffd700'
      }}>
        [WARN] CLASSIFIED: For authorized personnel only
      </div>
    </div>
  </div>
);

interface StatusLineProps {
  color: string;
  text: string;
}

const StatusLine: React.FC<StatusLineProps> = ({ color, text }) => (
  <div style={{ marginBottom: '4px' }}>
    <span style={{ color }}>●</span> {text}
  </div>
);
