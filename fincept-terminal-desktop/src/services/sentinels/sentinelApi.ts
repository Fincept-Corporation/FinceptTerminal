// Sentinel API Service - AI-powered monitoring sentinels
// Endpoints: /sentinels/*

import { fetch as tauriFetch } from '@tauri-apps/plugin-http';

const API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://api.fincept.in',
};

const getApiEndpoint = (path: string) => `${API_CONFIG.BASE_URL}${path}`;
const safeFetch = import.meta.env.DEV ? window.fetch.bind(window) : tauriFetch;

interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
  status_code: number;
}

async function makeApiRequest<T = any>(
  endpoint: string,
  method: 'GET' | 'POST' | 'PUT' | 'DELETE' = 'GET',
  apiKey: string,
  body?: any
): Promise<ApiResponse<T>> {
  try {
    const options: RequestInit = {
      method,
      headers: {
        'Content-Type': 'application/json',
        'X-API-Key': apiKey,
      },
    };

    if (body && (method === 'POST' || method === 'PUT')) {
      options.body = JSON.stringify(body);
    }

    const response = await safeFetch(getApiEndpoint(endpoint), options);
    const data = await response.json();

    return {
      success: response.ok,
      data,
      error: response.ok ? undefined : data.detail || data.error || 'Request failed',
      status_code: response.status,
    };
  } catch (error) {
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error',
      status_code: 500,
    };
  }
}

export interface Sentinel {
  id: string;
  domain: string;
  status: string;
  created_at: string;
  updated_at: string;
}

export interface SentinelFinding {
  id: string;
  sentinel_id: string;
  content: string;
  created_at: string;
}

export interface DeployRequest {
  domain: string;
  config?: Record<string, any>;
}

export class SentinelApiService {
  // Deploy a new sentinel
  static async deploy(apiKey: string, request: DeployRequest): Promise<ApiResponse> {
    return makeApiRequest('/sentinels/deploy', 'POST', apiKey, request);
  }

  // List all sentinels
  static async listSentinels(apiKey: string, domain?: string, status?: string): Promise<ApiResponse<Sentinel[]>> {
    const params = new URLSearchParams();
    if (domain) params.append('domain', domain);
    if (status) params.append('status', status);
    const query = params.toString();
    return makeApiRequest(`/sentinels/${query ? `?${query}` : ''}`, 'GET', apiKey);
  }

  // Get sentinel details
  static async getSentinel(apiKey: string, sentinelId: string): Promise<ApiResponse<Sentinel>> {
    return makeApiRequest(`/sentinels/${sentinelId}`, 'GET', apiKey);
  }

  // Stop/delete a sentinel
  static async stopSentinel(apiKey: string, sentinelId: string): Promise<ApiResponse> {
    return makeApiRequest(`/sentinels/${sentinelId}`, 'DELETE', apiKey);
  }

  // Get sentinel findings
  static async getFindings(
    apiKey: string,
    sentinelId: string,
    limit: number = 20,
    offset: number = 0
  ): Promise<ApiResponse<SentinelFinding[]>> {
    return makeApiRequest(`/sentinels/${sentinelId}/findings?limit=${limit}&offset=${offset}`, 'GET', apiKey);
  }

  // Pause a sentinel
  static async pauseSentinel(apiKey: string, sentinelId: string): Promise<ApiResponse> {
    return makeApiRequest(`/sentinels/${sentinelId}/pause`, 'PUT', apiKey);
  }

  // Resume a sentinel
  static async resumeSentinel(apiKey: string, sentinelId: string): Promise<ApiResponse> {
    return makeApiRequest(`/sentinels/${sentinelId}/resume`, 'PUT', apiKey);
  }

  // Ask a sentinel a question
  static async askSentinel(apiKey: string, query: string): Promise<ApiResponse> {
    return makeApiRequest('/sentinels/ask', 'POST', apiKey, { query });
  }
}
