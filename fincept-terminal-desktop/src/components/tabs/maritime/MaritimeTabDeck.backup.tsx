// MaritimeTabDeck.tsx - High-Performance Maritime Intelligence with Deck.gl
// NO DUMMY DATA - Only real vessel data from Marine API

import React, { useState, useEffect, useMemo } from 'react';
import DeckGL from '@deck.gl/react';
import { ScatterplotLayer, PathLayer } from '@deck.gl/layers';
import { TileLayer } from '@deck.gl/geo-layers';
import { BitmapLayer } from '@deck.gl/layers';
import { useAuth } from '@/contexts/AuthContext';
import { MarineApiService, VesselData } from '@/services/maritime/marineApi';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';
import { GeocodingService, GeocodeLocation } from '@/services/geopolitics/geocodingService';

const INITIAL_VIEW_STATE = {
  longitude: 72.8,
  latitude: 19.0,
  zoom: 10,
  pitch: 0,
  bearing: 0
};

export default function MaritimeTabDeck() {
  const { colors, fontFamily, fontSize } = useTerminalTheme();
  const { session } = useAuth();
  const apiKey = session?.api_key || null;

  const [vessels, setVessels] = useState<VesselData[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [searchImo, setSearchImo] = useState('');
  const [searchingVessel, setSearchingVessel] = useState(false);
  const [viewState, setViewState] = useState(INITIAL_VIEW_STATE);
  const [selectedVessel, setSelectedVessel] = useState<VesselData | null>(null);
  const [vesselHistory, setVesselHistory] = useState<any[]>([]);
  const [showHistory, setShowHistory] = useState(false);
  const [healthStatus, setHealthStatus] = useState<any>(null);
  const [multiImoInput, setMultiImoInput] = useState('');
  const [searchMode, setSearchMode] = useState<'single' | 'multi'>('single');
  const [locationSearch, setLocationSearch] = useState('');
  const [searchingLocation, setSearchingLocation] = useState(false);
  const [suggestions, setSuggestions] = useState<GeocodeLocation[]>([]);
  const [showSuggestions, setShowSuggestions] = useState(false);

  // Load vessels from API
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
        console.log(`Loaded ${validVessels.length} vessels`);
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
        const vessel = response.data.vessel;

        // Add to vessel list if not exists
        setVessels(prev => {
          const exists = prev.find(v => v.imo === vessel.imo);
          if (exists) return prev;
          return [...prev, vessel];
        });

        // Zoom to vessel
        setViewState({
          longitude: parseFloat(vessel.last_pos_longitude),
          latitude: parseFloat(vessel.last_pos_latitude),
          zoom: 14,
          pitch: 0,
          bearing: 0
        });

        setSearchImo('');
        console.log(`Found vessel: ${vessel.name}`);
      } else {
        setError(response.error || 'Vessel not found');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Search failed');
    } finally {
      setSearchingVessel(false);
    }
  };

  // Multi-vessel search
  const searchMultiVessels = async () => {
    if (!apiKey || !multiImoInput.trim()) return;

    setSearchingVessel(true);
    setError(null);

    try {
      const imos = multiImoInput.split(/[,\s]+/).map(s => s.trim()).filter(s => s.length > 0);

      if (imos.length === 0) {
        setError('Please enter at least one IMO number');
        setSearchingVessel(false);
        return;
      }

      if (imos.length > 50) {
        setError('Maximum 50 IMO numbers allowed');
        setSearchingVessel(false);
        return;
      }

      const response = await MarineApiService.getMultiVesselPositions(imos, apiKey);

      if (response.success && response.data && response.data.vessels) {
        const newVessels = response.data.vessels;

        setVessels(prev => {
          const combined = [...prev];
          newVessels.forEach(vessel => {
            const exists = combined.find(v => v.imo === vessel.imo);
            if (!exists) combined.push(vessel);
          });
          return combined;
        });

        setMultiImoInput('');
        console.log(`Found ${response.data.found_count || 0} vessels, ${response.data.not_found_count || 0} not found`);

        if (response.data.not_found_count > 0 && response.data.not_found) {
          setError(`${response.data.not_found_count} vessels not found: ${response.data.not_found.join(', ')}`);
        }
      } else {
        setError(response.error || 'Multi-vessel search failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Search failed');
    } finally {
      setSearchingVessel(false);
    }
  };

  // Load vessel history
  const loadVesselHistory = async (vessel: VesselData) => {
    if (!apiKey) return;

    setSelectedVessel(vessel);
    setShowHistory(true);
    setError(null);

    try {
      const response = await MarineApiService.getVesselHistory(vessel.imo, apiKey);

      if (response.success && response.data && response.data.history && response.data.history.positions) {
        setVesselHistory(response.data.history.positions);
        console.log(`Loaded history: ${response.data.history.positions.length} positions`);
      } else {
        setError(response.error || 'Failed to load vessel history');
        setVesselHistory([]);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'History load failed');
      setVesselHistory([]);
    }
  };

  // Load health status
  const loadHealthStatus = async () => {
    if (!apiKey) return;

    try {
      const response = await MarineApiService.getMarineHealth(apiKey);

      if (response.success && response.data) {
        setHealthStatus(response.data);
        console.log('Marine API Health:', response.data.status);
      }
    } catch (err) {
      console.error('Health check failed:', err);
    }
  };

  // Fetch autocomplete suggestions using Python geopy
  const fetchSuggestions = async (query: string) => {
    if (query.length < 3) {
      setSuggestions([]);
      setShowSuggestions(false);
      return;
    }

    try {
      const results = await GeocodingService.searchLocation(query, 5);
      if (results.success && results.suggestions) {
        setSuggestions(results.suggestions);
        setShowSuggestions(results.suggestions.length > 0);
      }
    } catch (err) {
      console.error('Autocomplete failed:', err);
      setSuggestions([]);
      setShowSuggestions(false);
    }
  };

  // Geocoding search
  const searchByLocation = async (selectedLocation?: GeocodeLocation) => {
    if (!apiKey) return;

    const locationData = selectedLocation || (suggestions.length > 0 ? suggestions[0] : null);
    if (!locationData && !locationSearch.trim()) return;

    setSearchingLocation(true);
    setError(null);
    setShowSuggestions(false);

    try {
      let bounds;
      if (locationData) {
        // Use location from suggestions
        bounds = locationData.bbox;
      } else {
        // Search new location
        const results = await GeocodingService.searchLocation(locationSearch, 1);
        if (!results.success || !results.suggestions || results.suggestions.length === 0) {
          setError('Location not found');
          setSearchingLocation(false);
          return;
        }
        bounds = results.suggestions[0].bbox;
      }

      // Search vessels in this area
      const vesselResponse = await MarineApiService.searchVesselsByArea(bounds, apiKey);

      if (vesselResponse.success && vesselResponse.data) {
        const validVessels = vesselResponse.data.vessels.filter(
          v => v.last_pos_latitude && v.last_pos_longitude
        );
        setVessels(validVessels);

        // Pan to center
        const centerLat = (bounds.min_lat + bounds.max_lat) / 2;
        const centerLng = (bounds.min_lng + bounds.max_lng) / 2;
        setViewState({
          longitude: centerLng,
          latitude: centerLat,
          zoom: 10,
          pitch: 0,
          bearing: 0
        });

        console.log(`Found ${validVessels.length} vessels in area`);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Geocoding failed');
    } finally {
      setSearchingLocation(false);
    }
  };

  // Search current viewport
  const searchCurrentView = async () => {
    if (!apiKey) return;

    setIsLoading(true);
    setError(null);

    try {
      // Calculate bounds from viewport
      const zoom = viewState.zoom;
      const lat = viewState.latitude;
      const lng = viewState.longitude;

      // Approximate bounds based on zoom level
      const latDelta = 180 / Math.pow(2, zoom);
      const lngDelta = 360 / Math.pow(2, zoom);

      const bounds = {
        min_lat: lat - latDelta,
        max_lat: lat + latDelta,
        min_lng: lng - lngDelta,
        max_lng: lng + lngDelta
      };

      const response = await MarineApiService.searchVesselsByArea(bounds, apiKey);

      if (response.success && response.data) {
        const validVessels = response.data.vessels.filter(
          v => v.last_pos_latitude && v.last_pos_longitude
        );
        setVessels(validVessels);
        console.log(`Loaded ${validVessels.length} vessels in current view`);
      } else {
        setError(response.error || 'Failed to load vessels');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Search failed');
    } finally {
      setIsLoading(false);
    }
  };

  // Auto-load vessels on mount
  useEffect(() => {
    loadVessels();
    loadHealthStatus();
    const interval = setInterval(loadVessels, 300000); // Refresh every 5 minutes
    return () => clearInterval(interval);
  }, [apiKey]);

  // Memoized vessel data - precompute positions and colors
  const vesselPoints = useMemo(() => {
    return vessels.map(v => {
      const speed = parseFloat(v.last_pos_speed) || 0;
      let color: [number, number, number, number];
      if (speed < 5) color = [0, 255, 255, 180];
      else if (speed < 15) color = [255, 215, 0, 180];
      else color = [255, 69, 0, 180];

      return {
        position: [parseFloat(v.last_pos_longitude), parseFloat(v.last_pos_latitude), 0] as [number, number, number],
        color,
        imo: v.imo,
        name: v.name,
        speed
      };
    });
  }, [vessels]);

  // Memoized layers with static accessors for performance
  const layers = useMemo(() => {
    const allLayers: any[] = [
      new TileLayer({
        id: 'basemap',
        data: 'https://basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png',
        minZoom: 0,
        maxZoom: 19,
        tileSize: 256,
        renderSubLayers: (props: any) => {
          const { boundingBox } = props.tile;
          return new BitmapLayer(props, {
            data: undefined,
            image: props.data,
            bounds: [boundingBox[0][0], boundingBox[0][1], boundingBox[1][0], boundingBox[1][1]]
          });
        }
      })
    ];

    // Add vessel history track
    if (showHistory && vesselHistory.length > 0) {
      const pathData = vesselHistory.map(pos => [
        parseFloat(pos.longitude),
        parseFloat(pos.latitude)
      ]);

      allLayers.push(
        new PathLayer({
          id: 'vessel-history',
          data: [{ path: pathData }],
          getPath: (d: any) => d.path,
          getColor: [255, 215, 0, 200],
          getWidth: 3,
          widthMinPixels: 2,
          widthMaxPixels: 8,
          jointRounded: true,
          capRounded: true
        })
      );
    }

    if (vesselPoints.length > 0) {
      allLayers.push(
        new ScatterplotLayer({
          id: 'vessels',
          data: vesselPoints,
          getPosition: (d: any) => d.position,
          getFillColor: (d: any) => d.color,
          getRadius: 80,
          radiusMinPixels: 4,
          radiusMaxPixels: 20,
          pickable: true,
          autoHighlight: true,
          highlightColor: [255, 255, 255, 100],
          onClick: (info: any) => {
            if (info.object) {
              const vessel = vessels.find(v => v.imo === info.object.imo);
              if (vessel) {
                loadVesselHistory(vessel);
              }
            }
          }
        })
      );
    }

    return allLayers;
  }, [vesselPoints, showHistory, vesselHistory, vessels]);

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        background: colors.background,
        color: colors.text,
        fontFamily: fontFamily,
        fontSize: `${fontSize}px`,
        position: 'relative',
        overflow: 'hidden'
      }}
    >
      {/* Header - Fincept Style - Split into Two Rows */}
      <div
        style={{
          position: 'absolute',
          top: 0,
          left: 0,
          right: 0,
          background: '#000000',
          borderBottom: '2px solid #ff6600',
          zIndex: 1000,
          fontFamily: 'Consolas, monospace',
          fontSize: '12px'
        }}
      >
        {/* Row 1: Title + Stats */}
        <div style={{
          padding: '4px 12px',
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          borderBottom: '1px solid #333'
        }}>
          <div style={{ color: '#ff6600', fontWeight: 'bold', fontSize: '13px' }}>
            MAR&gt;
          </div>
          <div style={{ color: '#ffffff', fontWeight: 'bold' }}>
            MARITIME TRACKING
          </div>
          <div style={{ color: '#888888' }}>|</div>
          <div style={{ color: vessels.length > 0 ? '#00ff00' : '#ff0000', fontWeight: 'bold' }}>
            {vesselPoints.length}/{vessels.length} VESSELS
          </div>
        </div>

        {/* Row 2: Search Controls */}
        <div style={{
          padding: '4px 12px',
          display: 'flex',
          gap: '8px',
          alignItems: 'center'
        }}>
          {/* Location Search with Autocomplete */}
          <div style={{ position: 'relative' }}>
            <input
              type="text"
              placeholder="Search: Suez Canal..."
              value={locationSearch}
              onChange={(e) => {
                setLocationSearch(e.target.value);
                fetchSuggestions(e.target.value);
              }}
              onKeyDown={(e) => {
                if (e.key === 'Enter') {
                  searchByLocation();
                } else if (e.key === 'Escape') {
                  setShowSuggestions(false);
                }
              }}
              onFocus={() => suggestions.length > 0 && setShowSuggestions(true)}
              disabled={searchingLocation}
              style={{
                padding: '4px 8px',
                background: '#1a1a1a',
                border: '1px solid #333',
                color: '#fff',
                fontFamily: 'Consolas, monospace',
                fontSize: '11px',
                width: '160px',
                outline: 'none'
              }}
            />
            {showSuggestions && suggestions.length > 0 && (
              <div
                style={{
                  position: 'absolute',
                  top: '100%',
                  left: 0,
                  width: '160px',
                  background: '#1a1a1a',
                  border: '1px solid #333',
                  borderTop: 'none',
                  maxHeight: '200px',
                  overflowY: 'auto',
                  zIndex: 2000,
                  marginTop: '1px'
                }}
              >
                {suggestions.map((suggestion, idx) => (
                  <div
                    key={idx}
                    onClick={() => {
                      setLocationSearch(suggestion.name);
                      searchByLocation(suggestion);
                      setShowSuggestions(false);
                    }}
                    style={{
                      padding: '6px 8px',
                      cursor: 'pointer',
                      fontSize: '10px',
                      borderBottom: idx < suggestions.length - 1 ? '1px solid #333' : 'none',
                      color: '#ccc',
                      background: '#1a1a1a',
                      transition: 'background 0.1s'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.background = '#2a2a2a';
                      e.currentTarget.style.color = '#fff';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.background = '#1a1a1a';
                      e.currentTarget.style.color = '#ccc';
                    }}
                  >
                    {suggestion.name}
                  </div>
                ))}
              </div>
            )}
          </div>

          <button
            onClick={() => searchByLocation()}
            disabled={searchingLocation || !locationSearch.trim()}
            style={{
              padding: '4px 10px',
              background: '#10b981',
              border: 'none',
              color: '#000',
              fontFamily: 'Consolas, monospace',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: searchingLocation ? 'not-allowed' : 'pointer',
              opacity: searchingLocation ? 0.5 : 1
            }}
          >
            LOCATION
          </button>

          <button
            onClick={searchCurrentView}
            disabled={isLoading}
            style={{
              padding: '4px 10px',
              background: '#06b6d4',
              border: 'none',
              color: '#000',
              fontFamily: 'Consolas, monospace',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              opacity: isLoading ? 0.5 : 1
            }}
          >
            CURRENT VIEW
          </button>

          <div style={{ width: '1px', height: '18px', background: '#333' }} />

          <select
            value={searchMode}
            onChange={(e) => setSearchMode(e.target.value as 'single' | 'multi')}
            style={{
              padding: '4px 6px',
              background: '#1a1a1a',
              border: '1px solid #333',
              color: '#fff',
              fontFamily: 'Consolas, monospace',
              fontSize: '10px',
              outline: 'none',
              cursor: 'pointer'
            }}
          >
            <option value="single">SINGLE</option>
            <option value="multi">MULTI</option>
          </select>

          {searchMode === 'single' ? (
            <input
              type="text"
              placeholder="IMO"
              value={searchImo}
              onChange={(e) => setSearchImo(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && searchVessel()}
              disabled={searchingVessel}
              style={{
                padding: '4px 8px',
                background: '#1a1a1a',
                border: '1px solid #333',
                color: '#fff',
                fontFamily: 'Consolas, monospace',
                fontSize: '11px',
                width: '100px',
                outline: 'none'
              }}
            />
          ) : (
            <input
              type="text"
              placeholder="IMO1, IMO2..."
              value={multiImoInput}
              onChange={(e) => setMultiImoInput(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && searchMultiVessels()}
              disabled={searchingVessel}
              style={{
                padding: '4px 8px',
                background: '#1a1a1a',
                border: '1px solid #333',
                color: '#fff',
                fontFamily: 'Consolas, monospace',
                fontSize: '11px',
                width: '120px',
                outline: 'none'
              }}
            />
          )}

          <button
            onClick={searchMode === 'single' ? searchVessel : searchMultiVessels}
            disabled={searchingVessel || (searchMode === 'single' ? !searchImo.trim() : !multiImoInput.trim())}
            style={{
              padding: '4px 12px',
              background: '#ff6600',
              border: 'none',
              color: '#000',
              fontFamily: 'Consolas, monospace',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: searchingVessel ? 'not-allowed' : 'pointer',
              opacity: searchingVessel ? 0.5 : 1
            }}
          >
            GO
          </button>

          {showHistory && (
            <button
              onClick={() => {
                setShowHistory(false);
                setVesselHistory([]);
                setSelectedVessel(null);
              }}
              style={{
                padding: '4px 10px',
                background: '#000',
                border: '1px solid #ffd700',
                color: '#ffd700',
                fontFamily: 'Consolas, monospace',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer'
              }}
            >
              CLEAR
            </button>
          )}
        </div>
      </div>

      {/* Error Display */}
      {error && (
        <div
          style={{
            position: 'absolute',
            top: '80px',
            left: '20px',
            right: '20px',
            padding: '12px 20px',
            background: 'rgba(255, 0, 0, 0.2)',
            border: '1px solid #ff0000',
            borderRadius: '4px',
            color: '#ff6b6b',
            fontFamily: 'Consolas, monospace',
            fontSize: '13px',
            zIndex: 10
          }}
        >
          ERROR: {error}
        </div>
      )}

      {/* Deck.gl Map */}
      <div style={{ flex: 1, position: 'relative', background: '#000', minHeight: 0 }}>
        <DeckGL
          viewState={viewState}
          onViewStateChange={({ viewState: vs }: any) => setViewState(vs)}
          controller={true}
          layers={layers}
          width="100%"
          height="100%"
          style={{ position: 'absolute', inset: '0' }}
          getTooltip={({ object }: any) => object && {
            html: `<strong>${object.name}</strong><br/>IMO: ${object.imo}<br/>Speed: ${object.speed.toFixed(1)} kts`,
            style: {
              backgroundColor: '#000',
              color: '#fff',
              padding: '8px',
              borderRadius: '4px',
              border: '1px solid #ff6600',
              fontFamily: 'Consolas, monospace',
              fontSize: '11px'
            }
          }}
        />
      </div>

      {/* Footer */}
      <div style={{ flexShrink: 0 }}>
        <TabFooter
          tabName="MARITIME TRACKING"
          leftInfo={[
            { label: 'VESSELS', value: `${vesselPoints.length}/${vessels.length}`, color: vesselPoints.length > 0 ? '#10b981' : '#ef4444' },
            { label: 'ZONE', value: 'MUMBAI', color: '#fbbf24' },
            { label: 'ENGINE', value: 'DECK.GL v9.2', color: '#06b6d4' },
            ...(healthStatus ? [{ label: 'API', value: healthStatus.status.toUpperCase(), color: healthStatus.status === 'healthy' ? '#10b981' : '#ef4444' }] : []),
            ...(selectedVessel ? [{ label: 'TRACKING', value: selectedVessel.name, color: '#ffd700' }] : []),
            ...(showHistory && vesselHistory.length > 0 ? [{ label: 'HISTORY', value: `${vesselHistory.length} PTS`, color: '#ffd700' }] : [])
          ]}
          statusInfo={`REAL-TIME AIS DATA | AUTO-REFRESH: 5MIN | ${new Date().toLocaleTimeString()}`}
          backgroundColor="#000000"
          borderColor="#ff6600"
        />
      </div>
    </div>
  );
}
