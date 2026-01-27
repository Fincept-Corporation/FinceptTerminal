// Hook for managing maritime vessel data

import { useState, useEffect, useCallback } from 'react';
import { MarineApiService, VesselData } from '@/services/maritime/marineApi';
import type { RealVesselData, IntelligenceData } from '../types';
import {
  DEFAULT_INTELLIGENCE,
  MUMBAI_SEARCH_AREA,
  VESSEL_REFRESH_INTERVAL,
  TIMESTAMP_UPDATE_INTERVAL,
  MAX_VESSELS_DISPLAY
} from '../constants';

interface UseMaritimeDataProps {
  apiKey: string | null;
  onVesselsUpdate?: (vessels: RealVesselData[]) => void;
}

interface UseMaritimeDataReturn {
  realVessels: RealVesselData[];
  intelligence: IntelligenceData;
  isLoadingVessels: boolean;
  vesselLoadError: string | null;
  searchImo: string;
  setSearchImo: (imo: string) => void;
  searchingVessel: boolean;
  searchResult: VesselData | null;
  searchError: string | null;
  loadRealVessels: (limitCount?: number) => Promise<void>;
  searchVesselByImo: () => Promise<void>;
}

export function useMaritimeData({ apiKey, onVesselsUpdate }: UseMaritimeDataProps): UseMaritimeDataReturn {
  const [realVessels, setRealVessels] = useState<RealVesselData[]>([]);
  const [isLoadingVessels, setIsLoadingVessels] = useState(false);
  const [vesselLoadError, setVesselLoadError] = useState<string | null>(null);
  const [searchImo, setSearchImo] = useState('');
  const [searchingVessel, setSearchingVessel] = useState(false);
  const [searchResult, setSearchResult] = useState<VesselData | null>(null);
  const [searchError, setSearchError] = useState<string | null>(null);
  const [intelligence, setIntelligence] = useState<IntelligenceData>(DEFAULT_INTELLIGENCE);

  // Load real vessel data from API with performance optimization
  const loadRealVessels = useCallback(async (limitCount: number = MAX_VESSELS_DISPLAY) => {
    if (!apiKey) {
      setVesselLoadError('No API key available');
      return;
    }

    setIsLoadingVessels(true);
    setVesselLoadError(null);

    try {
      const response = await MarineApiService.searchVesselsByArea(MUMBAI_SEARCH_AREA, apiKey);

      if (response.success && response.data) {
        const allVessels = response.data.vessels
          .filter(v => v.last_pos_latitude && v.last_pos_longitude)
          .map(v => ({
            imo: v.imo,
            name: v.name,
            lat: parseFloat(v.last_pos_latitude),
            lng: parseFloat(v.last_pos_longitude),
            speed: v.last_pos_speed,
            angle: v.last_pos_angle,
            from_port: v.route_from_port_name || undefined,
            to_port: v.route_to_port_name || undefined,
            route_progress: v.route_progress ? parseFloat(v.route_progress) : undefined
          }));

        // Prioritize vessels with routes for better visualization
        const vesselsWithRoutes = allVessels.filter(v => v.from_port || v.to_port);
        const vesselsWithoutRoutes = allVessels.filter(v => !v.from_port && !v.to_port);

        const limitedVessels = [
          ...vesselsWithRoutes.slice(0, Math.floor(limitCount * 0.7)),
          ...vesselsWithoutRoutes.slice(0, Math.floor(limitCount * 0.3))
        ].slice(0, limitCount);

        console.log(`Loaded ${limitedVessels.length} vessels out of ${allVessels.length} total`);

        setRealVessels(limitedVessels);
        setIntelligence(prev => ({
          ...prev,
          active_vessels: allVessels.length,
          timestamp: new Date().toISOString()
        }));

        onVesselsUpdate?.(limitedVessels);
      } else {
        setVesselLoadError(response.error || 'Failed to load vessel data');
      }
    } catch (error) {
      console.error('Error loading vessels:', error);
      setVesselLoadError(error instanceof Error ? error.message : 'Unknown error');
    } finally {
      setIsLoadingVessels(false);
    }
  }, [apiKey, onVesselsUpdate]);

  // Search for a specific vessel by IMO
  const searchVesselByImo = useCallback(async () => {
    if (!apiKey) {
      setSearchError('No API key available');
      return;
    }

    if (!searchImo.trim()) {
      setSearchError('Please enter an IMO number');
      return;
    }

    setSearchingVessel(true);
    setSearchError(null);
    setSearchResult(null);

    try {
      const response = await MarineApiService.getVesselPosition(searchImo.trim(), apiKey);

      if (response.success && response.data) {
        setSearchResult(response.data.vessel);
      } else {
        setSearchError(response.error || 'Vessel not found');
      }
    } catch (error) {
      console.error('Error searching vessel:', error);
      setSearchError(error instanceof Error ? error.message : 'Search failed');
    } finally {
      setSearchingVessel(false);
    }
  }, [apiKey, searchImo]);

  // NOTE: Auto-fetch removed - vessels are only loaded when explicitly requested
  // Call loadRealVessels() manually from the component when the tab is opened

  // Update timestamp periodically
  useEffect(() => {
    const interval = setInterval(() => {
      setIntelligence(prev => ({
        ...prev,
        timestamp: new Date().toISOString()
      }));
    }, TIMESTAMP_UPDATE_INTERVAL);
    return () => clearInterval(interval);
  }, []);

  return {
    realVessels,
    intelligence,
    isLoadingVessels,
    vesselLoadError,
    searchImo,
    setSearchImo,
    searchingVessel,
    searchResult,
    searchError,
    loadRealVessels,
    searchVesselByImo
  };
}
