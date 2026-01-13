// File: src/services/marineApi.ts
// Marine Vessel Tracking API service for handling all marine-related API calls

import { fetch } from '@tauri-apps/plugin-http';

// API Configuration
const API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://finceptbackend.share.zrok.io',
  API_VERSION: 'v1',
  CONNECTION_TIMEOUT: 10000,
  REQUEST_TIMEOUT: 30000
};

const getApiEndpoint = (path: string) => `${API_CONFIG.BASE_URL}${path}`;

// API Response Types
interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  message?: string;
  error?: string;
  error_code?: string;
  status_code?: number;
}

// Request Types
interface VesselPositionRequest {
  imo: string;
}

interface MultiVesselRequest {
  imos: string[];
}

interface AreaSearchRequest {
  min_lat: number;
  max_lat: number;
  min_lng: number;
  max_lng: number;
  days_ago?: number; // 0-365, for historical data
}

// Response Types
interface VesselData {
  id: number;
  imo: string;
  name: string;
  last_pos_updated_at: string | null;
  last_pos_latitude: string;
  last_pos_longitude: string;
  last_pos_speed: string;
  last_pos_angle: string;
  last_pos_id: string | null;
  route_last_source_updated: string | null;
  route_from_port_id: string | null;
  route_from_port_name: string | null;
  route_from_date: string | null;
  route_from_id: string | null;
  route_to_port_id: string | null;
  route_to_port_name: string | null;
  route_to_date: string | null;
  route_to_id: string | null;
  route_progress: string | null;
  route_id: string | null;
  current_draught: string | null;
  fetched_at: string;
}

interface VesselPositionResponse {
  vessel: VesselData;
  credits_used: number;
  remaining_credits: number;
}

interface MultiVesselResponse {
  vessels: VesselData[];
  found_count: number;
  not_found: string[];
  not_found_count: number;
  credits_used: number;
  remaining_credits: number;
}

interface AreaSearchResponse {
  search_area: {
    min_latitude: number;
    max_latitude: number;
    min_longitude: number;
    max_longitude: number;
  };
  vessels: VesselData[];
  vessel_count: number;
  days_ago?: number;
  credits_used: number;
  remaining_credits: number;
}

interface VesselHistoryData {
  positions: Array<{
    timestamp: string;
    latitude: string;
    longitude: string;
    speed: string;
    angle: string;
  }>;
  vessel_info: {
    imo: string;
    name: string;
  };
}

interface VesselHistoryResponse {
  history: VesselHistoryData;
  credits_used: number;
  remaining_credits: number;
}

interface MarineHealthResponse {
  module: string;
  status: string;
  database: {
    status: string;
    total_records: number;
  };
  mode: string;
  endpoints: {
    [key: string]: {
      path: string;
      credit_cost: number;
      description: string;
    };
  };
}

// Generic API Request Function
async function makeApiRequest<T>(
  endpoint: string,
  options: RequestInit = {},
  apiKey?: string
): Promise<ApiResponse<T>> {
  try {
    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
      ...((options.headers as Record<string, string>) || {})
    };

    // Add API key if provided
    if (apiKey) {
      headers['X-API-Key'] = apiKey;
    }

    const response = await fetch(endpoint, {
      ...options,
      headers
    });

    const data = await response.json();

    return {
      success: data.success ?? response.ok,
      data: data.data,
      message: data.message,
      error: data.error || data.message,
      error_code: data.error_code,
      status_code: response.status
    };
  } catch (error) {
    console.error('Marine API request failed:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error occurred',
      status_code: 500
    };
  }
}

// Marine API Service
export const MarineApiService = {
  /**
   * Get last known position of a vessel by IMO number
   * Cost: 1 credit per request
   */
  async getVesselPosition(
    imo: string,
    apiKey: string
  ): Promise<ApiResponse<VesselPositionResponse>> {
    const endpoint = getApiEndpoint('/marine/vessel/position');
    const body: VesselPositionRequest = { imo };

    return makeApiRequest<VesselPositionResponse>(
      endpoint,
      {
        method: 'POST',
        body: JSON.stringify(body)
      },
      apiKey
    );
  },

  /**
   * Get complete historical location data for a vessel
   * Cost: 5 credits per request
   */
  async getVesselHistory(
    imo: string,
    apiKey: string
  ): Promise<ApiResponse<VesselHistoryResponse>> {
    const endpoint = getApiEndpoint('/marine/vessel/history');
    const body: VesselPositionRequest = { imo };

    return makeApiRequest<VesselHistoryResponse>(
      endpoint,
      {
        method: 'POST',
        body: JSON.stringify(body)
      },
      apiKey
    );
  },

  /**
   * Get last known positions for multiple vessels (max 50)
   * Cost: 3 credits per request
   */
  async getMultiVesselPositions(
    imos: string[],
    apiKey: string
  ): Promise<ApiResponse<MultiVesselResponse>> {
    if (imos.length > 50) {
      return {
        success: false,
        error: 'Maximum 50 vessels allowed per request',
        status_code: 400
      };
    }

    const endpoint = getApiEndpoint('/marine/vessel/multi');
    const body: MultiVesselRequest = { imos };

    return makeApiRequest<MultiVesselResponse>(
      endpoint,
      {
        method: 'POST',
        body: JSON.stringify(body)
      },
      apiKey
    );
  },

  /**
   * Search for all vessels in a defined geographic area
   * Cost: 8 credits per request
   * @param params - Area bounds and optional days_ago (0-365) for historical data
   */
  async searchVesselsByArea(
    params: AreaSearchRequest,
    apiKey: string
  ): Promise<ApiResponse<AreaSearchResponse>> {
    const endpoint = getApiEndpoint('/marine/vessel/area-search');

    return makeApiRequest<AreaSearchResponse>(
      endpoint,
      {
        method: 'POST',
        body: JSON.stringify(params)
      },
      apiKey
    );
  },

  /**
   * Health check for marine module
   * Free - no credits required
   */
  async getMarineHealth(apiKey: string): Promise<ApiResponse<MarineHealthResponse>> {
    const endpoint = getApiEndpoint('/marine/health');

    return makeApiRequest<MarineHealthResponse>(
      endpoint,
      {
        method: 'GET'
      },
      apiKey
    );
  }
};

// Export types for use in components
export type {
  VesselData,
  VesselPositionResponse,
  MultiVesselResponse,
  AreaSearchResponse,
  VesselHistoryResponse,
  MarineHealthResponse,
  VesselPositionRequest,
  MultiVesselRequest,
  AreaSearchRequest
};
