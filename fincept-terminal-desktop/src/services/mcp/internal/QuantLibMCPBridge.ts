// QuantLib MCP Bridge - Manages API key and provides shared utilities for QuantLib tools

import { terminalMCPProvider } from './TerminalMCPProvider';

const QUANTLIB_BASE_URL = 'https://api.fincept.in';

/**
 * Shared API call function for all QuantLib MCP tools
 */
export async function quantlibApiCall<T = any>(
  path: string,
  body: any,
  apiKey: string | null,
  method: 'GET' | 'POST' = 'POST'
): Promise<T> {
  let url = `${QUANTLIB_BASE_URL}${path}`;

  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (apiKey) headers['X-API-Key'] = apiKey;

  const opts: RequestInit = { method, headers };

  if (method === 'POST' && body) {
    opts.body = JSON.stringify(body);
  } else if (method === 'GET' && body) {
    // For GET requests, append query params
    const params = new URLSearchParams();
    for (const [k, v] of Object.entries(body)) {
      if (v !== undefined && v !== null) params.append(k, String(v));
    }
    const qs = params.toString();
    if (qs) url += `?${qs}`;
  }

  const res = await fetch(url, opts);

  if (!res.ok) {
    const text = await res.text();
    throw new Error(`QuantLib API ${res.status}: ${text}`);
  }

  return res.json();
}

/**
 * Format numeric results for human-readable messages
 */
export function formatNumber(value: number | undefined | null, decimals = 4): string {
  if (value === undefined || value === null) return 'N/A';
  return value.toFixed(decimals);
}

/**
 * Format percentage values
 */
export function formatPercent(value: number | undefined | null, decimals = 2): string {
  if (value === undefined || value === null) return 'N/A';
  return `${(value * 100).toFixed(decimals)}%`;
}

/**
 * QuantLib MCP Bridge Class
 * Manages API key context and provides connection to terminal MCP provider
 */
class QuantLibMCPBridge {
  private apiKey: string | null = null;
  private isConnected = false;

  /**
   * Connect the bridge and register context functions
   */
  connect(apiKey?: string): void {
    if (apiKey) {
      this.apiKey = apiKey;
    }

    // Register context functions with the terminal MCP provider
    terminalMCPProvider.setContexts({
      getQuantLibApiKey: () => this.apiKey,
      setQuantLibApiKey: (key: string | null) => {
        this.apiKey = key;
      },
    });

    this.isConnected = true;
  }

  /**
   * Disconnect the bridge
   */
  disconnect(): void {
    this.isConnected = false;
  }

  /**
   * Set the API key
   */
  setApiKey(key: string | null): void {
    this.apiKey = key;
  }

  /**
   * Get the current API key
   */
  getApiKey(): string | null {
    return this.apiKey;
  }

  /**
   * Check if the bridge is connected
   */
  isActive(): boolean {
    return this.isConnected;
  }

  /**
   * Check if API key is configured
   */
  hasApiKey(): boolean {
    return this.apiKey !== null && this.apiKey.length > 0;
  }
}

export const quantLibMCPBridge = new QuantLibMCPBridge();
