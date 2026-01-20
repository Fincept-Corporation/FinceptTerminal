// MaritimeTabKepler.tsx - Professional Maritime Intelligence with Kepler.gl
// NO DUMMY DATA - Only real vessel data from Marine API

import React, { useState, useEffect, useMemo } from 'react';
// @ts-ignore - Kepler.gl types are not fully compatible
import KeplerGl from 'kepler.gl/components';
// @ts-ignore
import { addDataToMap, wrapTo } from 'kepler.gl/actions';
import { useDispatch } from 'react-redux';
import { useAuth } from '@/contexts/AuthContext';
import { MarineApiService, VesselData } from '@/services/maritime/marineApi';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';

interface VesselGeoJSON {
  type: 'FeatureCollection';
  features: Array<{
    type: 'Feature';
    geometry: {
      type: 'Point';
      coordinates: [number, number]; // [lng, lat]
    };
    properties: {
      imo: string;
      name: string;
      speed: string;
      angle: string;
      from_port?: string;
      to_port?: string;
      last_updated: string;
    };
  }>;
}

export default function MaritimeTabKepler() {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const { t } = useTranslation('maritime');
  const { session } = useAuth();
  const dispatch = useDispatch();
  const apiKey = session?.api_key || null;

  const [vessels, setVessels] = useState<VesselData[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [searchImo, setSearchImo] = useState('');
  const [searchingVessel, setSearchingVessel] = useState(false);
  const [vesselCount, setVesselCount] = useState(0);

  // Load vessels from API - Mumbai area
  const loadVessels = async () => {
    if (!apiKey) {
      setError('No API key available');
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const response = await MarineApiService.searchVesselsByArea(
        {
          min_lat: 18.5,
          max_lat: 19.5,
          min_lng: 72.0,
          max_lng: 73.5
        },
        apiKey
      );

      if (response.success && response.data) {
        const validVessels = response.data.vessels.filter(
          v => v.last_pos_latitude && v.last_pos_longitude
        );
        setVessels(validVessels);
        setVesselCount(validVessels.length);
      } else {
        setError(response.error || 'Failed to load vessels');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setIsLoading(false);
    }
  };

  // Search specific vessel by IMO
  const searchVessel = async () => {
    if (!apiKey || !searchImo.trim()) return;

    setSearchingVessel(true);
    setError(null);

    try {
      const response = await MarineApiService.getVesselPosition(searchImo.trim(), apiKey);

      if (response.success && response.data) {
        // Add searched vessel to the map
        setVessels(prev => {
          const exists = prev.find(v => v.imo === response.data!.vessel.imo);
          if (exists) return prev;
          return [...prev, response.data!.vessel];
        });
        setSearchImo('');
      } else {
        setError(response.error || 'Vessel not found');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Search failed');
    } finally {
      setSearchingVessel(false);
    }
  };

  // Convert vessel data to GeoJSON for Kepler.gl
  const vesselGeoJSON: VesselGeoJSON = useMemo(() => ({
    type: 'FeatureCollection',
    features: vessels.map(v => ({
      type: 'Feature',
      geometry: {
        type: 'Point',
        coordinates: [parseFloat(v.last_pos_longitude), parseFloat(v.last_pos_latitude)]
      },
      properties: {
        imo: v.imo,
        name: v.name,
        speed: v.last_pos_speed,
        angle: v.last_pos_angle,
        from_port: v.route_from_port_name || undefined,
        to_port: v.route_to_port_name || undefined,
        last_updated: v.last_pos_updated_at || 'Unknown'
      }
    }))
  }), [vessels]);

  // Load vessel data into Kepler.gl
  useEffect(() => {
    if (vessels.length > 0) {
      const dataset = {
        info: {
          label: 'Marine Vessels',
          id: 'marine_vessels'
        },
        data: {
          fields: [
            { name: 'imo', type: 'string' },
            { name: 'name', type: 'string' },
            { name: 'speed', type: 'real' },
            { name: 'angle', type: 'real' },
            { name: 'from_port', type: 'string' },
            { name: 'to_port', type: 'string' },
            { name: 'lat', type: 'real' },
            { name: 'lng', type: 'real' },
            { name: 'last_updated', type: 'timestamp' }
          ],
          rows: vessels.map(v => [
            v.imo,
            v.name,
            parseFloat(v.last_pos_speed),
            parseFloat(v.last_pos_angle),
            v.route_from_port_name || '',
            v.route_to_port_name || '',
            parseFloat(v.last_pos_latitude),
            parseFloat(v.last_pos_longitude),
            v.last_pos_updated_at || ''
          ])
        }
      };

      const config = {
        version: 'v1',
        config: {
          visState: {
            filters: [],
            layers: [
              {
                id: 'vessels',
                type: 'point',
                config: {
                  dataId: 'marine_vessels',
                  label: 'Vessels',
                  color: [0, 255, 255],
                  columns: {
                    lat: 'lat',
                    lng: 'lng'
                  },
                  isVisible: true,
                  visConfig: {
                    radius: 8,
                    fixedRadius: false,
                    opacity: 0.8,
                    outline: false,
                    thickness: 2,
                    colorRange: {
                      name: 'Global Warming',
                      type: 'sequential',
                      category: 'Uber',
                      colors: ['#5A1846', '#900C3F', '#C70039', '#E3611C', '#F1920E', '#FFC300']
                    },
                    radiusRange: [0, 50]
                  }
                },
                visualChannels: {
                  colorField: {
                    name: 'speed',
                    type: 'real'
                  },
                  colorScale: 'quantile',
                  sizeField: null,
                  sizeScale: 'linear'
                }
              }
            ]
          },
          mapState: {
            latitude: 19.0,
            longitude: 72.8,
            zoom: 10
          },
          mapStyle: {
            styleType: 'dark'
          }
        }
      };

      dispatch(
        wrapTo(
          'maritime',
          addDataToMap({
            datasets: dataset,
            options: {
              centerMap: true,
              readOnly: false
            },
            config
          })
        )
      );
    }
  }, [vessels, dispatch]);

  // Auto-load vessels on mount
  useEffect(() => {
    loadVessels();
    const interval = setInterval(loadVessels, 300000); // Refresh every 5 minutes
    return () => clearInterval(interval);
  }, [apiKey]);

  return (
    <div
      style={{
        width: '100%',
        height: '100vh',
        display: 'flex',
        flexDirection: 'column',
        background: colors.background,
        color: colors.text,
        fontFamily: fontFamily,
        fontSize: `${fontSize}px`
      }}
    >
      {/* Header Controls */}
      <div
        style={{
          padding: '12px 20px',
          background: 'rgba(0, 0, 0, 0.8)',
          borderBottom: '1px solid rgba(255, 255, 255, 0.1)',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          gap: '20px',
          flexWrap: 'wrap'
        }}
      >
        {/* Title */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ fontSize: '18px', fontWeight: 'bold', color: '#00ffff' }}>
            🚢 MARITIME INTELLIGENCE
          </div>
          <div
            style={{
              padding: '4px 12px',
              background: vesselCount > 0 ? 'rgba(0, 255, 0, 0.2)' : 'rgba(255, 0, 0, 0.2)',
              border: `1px solid ${vesselCount > 0 ? '#00ff00' : '#ff0000'}`,
              borderRadius: '4px',
              fontSize: '12px',
              fontWeight: 'bold'
            }}
          >
            {vesselCount} VESSELS
          </div>
        </div>

        {/* IMO Search */}
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <input
            type="text"
            placeholder="Search by IMO..."
            value={searchImo}
            onChange={(e) => setSearchImo(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && searchVessel()}
            disabled={searchingVessel}
            style={{
              padding: '8px 12px',
              background: 'rgba(0, 0, 0, 0.6)',
              border: '1px solid rgba(255, 255, 255, 0.2)',
              borderRadius: '4px',
              color: colors.text,
              fontFamily: 'Consolas, monospace',
              fontSize: '13px',
              width: '200px'
            }}
          />
          <button
            onClick={searchVessel}
            disabled={searchingVessel || !searchImo.trim()}
            style={{
              padding: '8px 16px',
              background: searchingVessel ? '#333' : '#0a0a0a',
              border: '1px solid #00ffff',
              borderRadius: '4px',
              color: '#00ffff',
              fontFamily: 'Consolas, monospace',
              fontSize: '12px',
              fontWeight: 'bold',
              cursor: searchingVessel ? 'not-allowed' : 'pointer',
              opacity: searchingVessel ? 0.5 : 1
            }}
          >
            {searchingVessel ? 'SEARCHING...' : 'SEARCH'}
          </button>
          <button
            onClick={loadVessels}
            disabled={isLoading}
            style={{
              padding: '8px 16px',
              background: '#0a0a0a',
              border: '1px solid #ffd700',
              borderRadius: '4px',
              color: '#ffd700',
              fontFamily: 'Consolas, monospace',
              fontSize: '12px',
              fontWeight: 'bold',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              opacity: isLoading ? 0.5 : 1
            }}
          >
            {isLoading ? 'LOADING...' : '🔄 REFRESH'}
          </button>
        </div>
      </div>

      {/* Error Display */}
      {error && (
        <div
          style={{
            padding: '12px 20px',
            background: 'rgba(255, 0, 0, 0.2)',
            border: '1px solid #ff0000',
            color: '#ff6b6b',
            fontFamily: 'Consolas, monospace',
            fontSize: '13px'
          }}
        >
          ⚠️ {error}
        </div>
      )}

      {/* Kepler.gl Map */}
      <div style={{ flex: 1, position: 'relative' }}>
        <KeplerGl
          id="maritime"
          mapboxApiAccessToken={import.meta.env.VITE_MAPBOX_TOKEN || ''}
          width={window.innerWidth}
          height={window.innerHeight - 120}
          theme="dark"
        />
      </div>

      <TabFooter tabName="maritime" />
    </div>
  );
}
