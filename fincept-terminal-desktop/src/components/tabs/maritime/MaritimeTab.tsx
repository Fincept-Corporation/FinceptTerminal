// Maritime Tab - Main Component
// A comprehensive maritime intelligence dashboard with 3D globe visualization

import React, { useRef, useCallback } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { TabFooter } from '@/components/common/TabFooter';

// Local imports
import { Header, LeftPanel, RightPanel, GlobeMap } from './components';
import { useMaritimeData, useMapControls } from './hooks';
import { TRADE_ROUTES, PRESET_LOCATIONS } from './constants';

// Styles
const scrollbarStyles = `
  *::-webkit-scrollbar { width: 6px; height: 6px; }
  *::-webkit-scrollbar-track { background: #0a0a0a; }
  *::-webkit-scrollbar-thumb { background: #1a1a1a; border-radius: 3px; }
  *::-webkit-scrollbar-thumb:hover { background: #2a2a2a; }
`;

export default function MaritimeTab() {
  const { session } = useAuth();
  const apiKey = session?.api_key || null;
  const mapRef = useRef<HTMLIFrameElement>(null);

  // Custom hooks for data and controls
  const mapControls = useMapControls({ mapRef });

  const handleVesselsUpdate = useCallback((vessels: any[]) => {
    mapControls.updateRealVessels(vessels);
  }, [mapControls]);

  const maritimeData = useMaritimeData({
    apiKey,
    onVesselsUpdate: handleVesselsUpdate
  });

  // Handle vessel search result - zoom to location
  const handleSearchVessel = useCallback(async () => {
    await maritimeData.searchVesselByImo();
    if (maritimeData.searchResult) {
      const lat = parseFloat(maritimeData.searchResult.last_pos_latitude);
      const lng = parseFloat(maritimeData.searchResult.last_pos_longitude);
      mapControls.zoomToVessel(lat, lng);
    }
  }, [maritimeData, mapControls]);

  // Handle map mode toggle
  const handleToggleMapMode = useCallback(() => {
    mapControls.toggleMapMode(maritimeData.realVessels);
  }, [mapControls, maritimeData.realVessels]);

  return (
    <div style={{
      width: '100%',
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: '#000'
    }}>
      <style>{scrollbarStyles}</style>

      {/* Header */}
      <Header
        intelligence={maritimeData.intelligence}
        mapMode={mapControls.mapMode}
        isRotating={mapControls.isRotating}
        onToggleMapMode={handleToggleMapMode}
        onToggleRotation={mapControls.toggleRotation}
      />

      {/* Main Layout */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden' }}>
        {/* Left Panel - Intelligence & Controls */}
        <LeftPanel
          intelligence={maritimeData.intelligence}
          realVesselsCount={maritimeData.realVessels.length}
          isLoadingVessels={maritimeData.isLoadingVessels}
          vesselLoadError={maritimeData.vesselLoadError}
          tradeRoutes={TRADE_ROUTES}
          selectedRoute={mapControls.selectedRoute}
          onSelectRoute={mapControls.setSelectedRoute}
          showRoutes={mapControls.showRoutes}
          showShips={mapControls.showShips}
          showPlanes={mapControls.showPlanes}
          showSatellites={mapControls.showSatellites}
          onToggleRoutes={mapControls.toggleRoutes}
          onToggleShips={mapControls.toggleShips}
          onTogglePlanes={mapControls.togglePlanes}
          onToggleSatellites={mapControls.toggleSatellites}
          onClearMarkers={mapControls.clearMarkers}
          onLoadVessels={maritimeData.loadRealVessels}
        />

        {/* Center - Globe/Map */}
        <GlobeMap ref={mapRef} />

        {/* Right Panel - Search & Markers */}
        <RightPanel
          selectedRoute={mapControls.selectedRoute}
          searchImo={maritimeData.searchImo}
          onSearchImoChange={maritimeData.setSearchImo}
          onSearchVessel={handleSearchVessel}
          searchingVessel={maritimeData.searchingVessel}
          searchResult={maritimeData.searchResult}
          searchError={maritimeData.searchError}
          markerType={mapControls.markerType}
          onMarkerTypeChange={mapControls.setMarkerType}
          presetLocations={PRESET_LOCATIONS}
          onAddPreset={mapControls.addPreset}
          markers={mapControls.markers}
          tradeRoutesCount={TRADE_ROUTES.length}
        />
      </div>

      {/* Footer */}
      <TabFooter
        tabName="MARITIME INTELLIGENCE"
        leftInfo={[
          { label: 'AIS Feed: ACTIVE', color: '#0ff' },
          { label: 'Satellite Link: NOMINAL', color: '#0ff' },
          { label: 'Data Latency: 2.3ms', color: '#0ff' },
        ]}
        statusInfo={
          <>
            <span style={{ color: '#888' }}>Powered by Globe.GL 3D Visualization</span>
            <span style={{ color: '#888', marginLeft: '8px' }}>|</span>
            <span style={{ color: '#ffd700', marginLeft: '8px' }}>CLEARANCE: TOP SECRET // SCI</span>
          </>
        }
        backgroundColor="#0a0a0a"
        borderColor="#00ff00"
      />
    </div>
  );
}
