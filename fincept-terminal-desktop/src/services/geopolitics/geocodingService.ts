// Geocoding Service - Location search using Python geopy
import { invoke } from '@tauri-apps/api/core';

export interface GeocodeLocation {
  name: string;
  latitude: number;
  longitude: number;
  bbox: {
    min_lat: number;
    max_lat: number;
    min_lng: number;
    max_lng: number;
  };
  type: string;
  importance?: number;
}

export interface GeocodeSearchResponse {
  success: boolean;
  query?: string;
  count?: number;
  suggestions?: GeocodeLocation[];
  error?: string;
}

export interface ReverseGeocodeResponse {
  success: boolean;
  name?: string;
  latitude?: number;
  longitude?: number;
  bbox?: {
    min_lat: number;
    max_lat: number;
    min_lng: number;
    max_lng: number;
  };
  type?: string;
  error?: string;
}

export class GeocodingService {
  /**
   * Search for locations with autocomplete suggestions
   * @param query Search query (e.g., "Suez Canal", "Singapore")
   * @param limit Maximum number of results (default: 5)
   * @returns Geocoding results with coordinates and bounding boxes
   */
  static async searchLocation(
    query: string,
    limit: number = 5
  ): Promise<GeocodeSearchResponse> {
    try {
      const result = await invoke<string>('search_geocode', {
        query,
        limit,
      });

      const parsed: GeocodeSearchResponse = JSON.parse(result);
      return parsed;
    } catch (error) {
      console.error('Geocoding search failed:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error',
        suggestions: [],
      };
    }
  }

  /**
   * Reverse geocode coordinates to location name
   * @param lat Latitude
   * @param lng Longitude
   * @returns Location information
   */
  static async reverseGeocode(
    lat: number,
    lng: number
  ): Promise<ReverseGeocodeResponse> {
    try {
      const result = await invoke<string>('reverse_geocode', {
        lat,
        lng,
      });

      const parsed: ReverseGeocodeResponse = JSON.parse(result);
      return parsed;
    } catch (error) {
      console.error('Reverse geocoding failed:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error',
      };
    }
  }
}
