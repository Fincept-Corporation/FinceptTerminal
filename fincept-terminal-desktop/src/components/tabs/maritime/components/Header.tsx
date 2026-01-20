// Maritime Tab Header Component

import React from 'react';
import type { IntelligenceData, MapMode } from '../types';

interface HeaderProps {
  intelligence: IntelligenceData;
  mapMode: MapMode;
  isRotating: boolean;
  onToggleMapMode: () => void;
  onToggleRotation: () => void;
}

export const Header: React.FC<HeaderProps> = ({
  intelligence,
  mapMode,
  isRotating,
  onToggleMapMode,
  onToggleRotation
}) => {
  const getThreatColor = (level: IntelligenceData['threat_level']) => {
    switch (level) {
      case 'critical': return '#ff0000';
      case 'high': return '#ff8c00';
      case 'medium': return '#ffd700';
      default: return '#00ff00';
    }
  };

  return (
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
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        <button
          onClick={onToggleMapMode}
          style={{
            background: mapMode === '2D' ? '#ffd700' : '#0a0a0a',
            color: mapMode === '2D' ? '#000' : '#ffd700',
            border: '1px solid #ffd700',
            padding: '4px 12px',
            fontSize: '9px',
            cursor: 'pointer',
            fontWeight: 'bold',
            fontFamily: 'Consolas, monospace',
            transition: 'all 0.2s'
          }}
        >
          {mapMode === '3D' ? '🌍 3D GLOBE' : '🗺️ 2D MAP'}
        </button>
        {mapMode === '3D' && (
          <button
            onClick={onToggleRotation}
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
        )}
        <span style={{
          color: getThreatColor(intelligence.threat_level),
          fontSize: '10px',
          fontWeight: 'bold'
        }}>
          THREAT: {intelligence.threat_level.toUpperCase()}
        </span>
        <span style={{ color: '#888', fontSize: '9px' }}>
          {new Date(intelligence.timestamp).toLocaleString()}
        </span>
      </div>
    </div>
  );
};
