// Hook for managing map controls and state

import { useState, useCallback, RefObject } from 'react';
import type { MarkerData, MapMode, RealVesselData } from '../types';
import type { PresetLocation } from '../types';

interface UseMapControlsProps {
  mapRef: RefObject<HTMLIFrameElement | null>;
}

interface UseMapControlsReturn {
  markers: MarkerData[];
  selectedRoute: any | null;
  setSelectedRoute: (route: any | null) => void;
  markerType: MarkerData['type'];
  setMarkerType: (type: MarkerData['type']) => void;
  showRoutes: boolean;
  showShips: boolean;
  showPlanes: boolean;
  showSatellites: boolean;
  isRotating: boolean;
  mapMode: MapMode;
  addMarker: (title: string, lat: number, lng: number) => void;
  addPreset: (preset: PresetLocation) => void;
  clearMarkers: () => void;
  toggleRoutes: () => void;
  toggleShips: () => void;
  togglePlanes: () => void;
  toggleSatellites: () => void;
  toggleRotation: () => void;
  toggleMapMode: (vessels?: RealVesselData[]) => void;
  zoomToVessel: (lat: number, lng: number) => void;
  updateRealVessels: (vessels: RealVesselData[]) => void;
}

export function useMapControls({ mapRef }: UseMapControlsProps): UseMapControlsReturn {
  const [markers, setMarkers] = useState<MarkerData[]>([]);
  const [selectedRoute, setSelectedRoute] = useState<any | null>(null);
  const [markerType, setMarkerType] = useState<MarkerData['type']>('Ship');
  const [showRoutes, setShowRoutes] = useState(true);
  const [showShips, setShowShips] = useState(true);
  const [showPlanes, setShowPlanes] = useState(true);
  const [showSatellites, setShowSatellites] = useState(true);
  const [isRotating, setIsRotating] = useState(true);
  const [mapMode, setMapMode] = useState<MapMode>('3D');

  const getIframeWindow = useCallback(() => {
    return mapRef.current?.contentWindow as any;
  }, [mapRef]);

  const addMarker = useCallback((title: string, lat: number, lng: number) => {
    const newMarker: MarkerData = { lat, lng, title, type: markerType };
    setMarkers(prev => [...prev, newMarker]);
    getIframeWindow()?.addMarker?.(lat, lng, title, markerType);
  }, [markerType, getIframeWindow]);

  const addPreset = useCallback((preset: PresetLocation) => {
    const newMarker: MarkerData = {
      lat: preset.lat,
      lng: preset.lng,
      title: preset.name,
      type: preset.type
    };
    setMarkers(prev => [...prev, newMarker]);
    getIframeWindow()?.addMarker?.(preset.lat, preset.lng, preset.name, preset.type);
  }, [getIframeWindow]);

  const clearMarkers = useCallback(() => {
    setMarkers([]);
    getIframeWindow()?.clearMarkers?.();
  }, [getIframeWindow]);

  const toggleRoutes = useCallback(() => {
    setShowRoutes(prev => !prev);
    getIframeWindow()?.toggleRoutes?.();
  }, [getIframeWindow]);

  const toggleShips = useCallback(() => {
    setShowShips(prev => !prev);
    getIframeWindow()?.toggleShips?.();
  }, [getIframeWindow]);

  const togglePlanes = useCallback(() => {
    setShowPlanes(prev => !prev);
    getIframeWindow()?.togglePlanes?.();
  }, [getIframeWindow]);

  const toggleSatellites = useCallback(() => {
    setShowSatellites(prev => !prev);
    getIframeWindow()?.toggleSatellites?.();
  }, [getIframeWindow]);

  const toggleRotation = useCallback(() => {
    const newRotating = !isRotating;
    setIsRotating(newRotating);
    getIframeWindow()?.toggleRotation?.(newRotating);
  }, [isRotating, getIframeWindow]);

  const toggleMapMode = useCallback((vessels?: RealVesselData[]) => {
    const newMode = mapMode === '3D' ? '2D' : '3D';
    setMapMode(newMode);

    if (newMode === '2D' && vessels && vessels.length > 0) {
      console.log('Toggling to 2D with', vessels.length, 'vessels');
      getIframeWindow()?.switchMapMode?.(newMode, vessels);
    } else {
      getIframeWindow()?.switchMapMode?.(newMode);
    }
  }, [mapMode, getIframeWindow]);

  const zoomToVessel = useCallback((lat: number, lng: number) => {
    getIframeWindow()?.world?.pointOfView?.({ lat, lng, altitude: 0.5 }, 1000);
  }, [getIframeWindow]);

  const updateRealVessels = useCallback((vessels: RealVesselData[]) => {
    getIframeWindow()?.updateRealVessels?.(vessels);
  }, [getIframeWindow]);

  return {
    markers,
    selectedRoute,
    setSelectedRoute,
    markerType,
    setMarkerType,
    showRoutes,
    showShips,
    showPlanes,
    showSatellites,
    isRotating,
    mapMode,
    addMarker,
    addPreset,
    clearMarkers,
    toggleRoutes,
    toggleShips,
    togglePlanes,
    toggleSatellites,
    toggleRotation,
    toggleMapMode,
    zoomToVessel,
    updateRealVessels
  };
}
