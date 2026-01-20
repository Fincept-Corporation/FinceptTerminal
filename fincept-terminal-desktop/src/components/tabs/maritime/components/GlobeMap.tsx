// Maritime Tab Globe/Map Component

import React, { forwardRef } from 'react';
import { generateMapHTML } from '../utils';

interface GlobeMapProps {
  // The iframe ref is passed from parent to allow control communication
}

export const GlobeMap = forwardRef<HTMLIFrameElement | null, GlobeMapProps>((_, ref) => {
  const mapHTML = generateMapHTML();

  return (
    <div style={{ flex: 1, position: 'relative', minWidth: 0, overflow: 'hidden' }}>
      <iframe
        ref={ref}
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
        <div style={{ marginTop: '4px', color: '#ffd700' }}>
          🔍 ZOOM: Deep zoom auto-switches to street-level map!
        </div>
      </div>
    </div>
  );
});

GlobeMap.displayName = 'GlobeMap';
