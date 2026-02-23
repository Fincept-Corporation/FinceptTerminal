// Economics MCP Bridge - Connects Fincept Macro API endpoints to MCP Internal Tools
// Provides AI access to: CEIC economic series, economic calendar, WGB sovereign data

import { terminalMCPProvider } from './TerminalMCPProvider';

const BASE_URL = 'https://api.fincept.in';
const FINCEPT_API_KEY = 'fk_user_vU20qwUxKtPmg0fWpriNBhcAnBVGgOtJxsKiiwfD9Qo';

const CONTEXT_KEYS = [
  'getFinceptCeicCountries',
  'getFinceptCeicIndicators',
  'getFinceptCeicSeries',
  'getFinceptEconomicCalendar',
  'getFinceptUpcomingEvents',
  'getFinceptCentralBankRates',
  'getFinceptCreditRatings',
  'getFinceptSovereignCds',
  'getFinceptBondSpreads',
  'getFinceptInvertedYields',
] as const;

async function finceptFetch(path: string): Promise<any> {
  const res = await fetch(`${BASE_URL}${path}`, {
    headers: { 'X-API-Key': FINCEPT_API_KEY, 'Accept': 'application/json' },
  });
  if (!res.ok) {
    const body = await res.text().catch(() => '');
    let msg = `${res.status} ${res.statusText}`;
    try { msg = JSON.parse(body).message || msg; } catch {}
    throw new Error(msg);
  }
  const json = await res.json();
  if (json.success === false) throw new Error(json.message || 'Fincept API error');
  return json;
}

/**
 * Extracts the data array from a known Fincept response shape.
 * Tries each known key in the data block and returns the first non-empty array.
 */
function extractRows(json: any): Record<string, any>[] {
  const block = json?.data;
  if (!block) return [];
  const knownKeys = [
    'central_bank_rates', 'credit_ratings', 'sovereign_cds',
    'bond_spreads', 'inverted_yields', 'events',
    'countries', 'indicators', 'series',
  ];
  for (const key of knownKeys) {
    if (Array.isArray(block[key]) && block[key].length > 0) return block[key];
  }
  // Fallback: first array value in the data block
  const firstArr = Object.values(block).find(v => Array.isArray(v) && (v as any[]).length > 0);
  return firstArr ? (firstArr as Record<string, any>[]) : [];
}

export class EconomicsMCPBridge {
  private connected = false;

  connect(): void {
    if (this.connected) return;

    terminalMCPProvider.setContexts({

      // ── CEIC ─────────────────────────────────────────────────────────────────

      getFinceptCeicCountries: async () => {
        const json = await finceptFetch('/macro/ceic/series/countries');
        return extractRows(json);
      },

      getFinceptCeicIndicators: async (countrySlug: string) => {
        const json = await finceptFetch(
          `/macro/ceic/series/indicators?country=${encodeURIComponent(countrySlug)}`
        );
        return json?.data?.indicators || extractRows(json);
      },

      getFinceptCeicSeries: async (params: {
        country: string;
        indicator?: string;
        yearFrom?: number;
        yearTo?: number;
        limit?: number;
      }) => {
        const { country, indicator, yearFrom, yearTo, limit = 500 } = params;
        const now = new Date().getFullYear();
        const from = yearFrom ?? now - 5;
        const to = yearTo ?? now;
        let path = `/macro/ceic/series?country=${encodeURIComponent(country)}&year_from=${from}&year_to=${to}&limit=${limit}`;
        if (indicator) path += `&indicator=${encodeURIComponent(indicator)}`;
        const json = await finceptFetch(path);
        const rows = extractRows(json);
        // Replace fractional period encoding with the plain integer year field
        return rows.map(({ period: _period, ...rest }) => rest);
      },

      // ── Economic Calendar ─────────────────────────────────────────────────────

      getFinceptEconomicCalendar: async (params: {
        startDate?: string;
        endDate?: string;
        limit?: number;
      }) => {
        const { startDate, endDate, limit = 200 } = params;
        const now = new Date();
        const from = startDate ?? now.toISOString().split('T')[0];
        const to = endDate ?? new Date(now.getTime() + 30 * 24 * 60 * 60 * 1000).toISOString().split('T')[0];
        const json = await finceptFetch(
          `/macro/economic-calendar?start_date=${from}&end_date=${to}&limit=${limit}`
        );
        return extractRows(json);
      },

      getFinceptUpcomingEvents: async (limit?: number) => {
        const json = await finceptFetch(`/macro/upcoming-events?limit=${limit ?? 200}`);
        return extractRows(json);
      },

      // ── World Government Bonds ────────────────────────────────────────────────

      getFinceptCentralBankRates: async () => {
        const json = await finceptFetch('/macro/wgb/central-bank-rates');
        return extractRows(json);
      },

      getFinceptCreditRatings: async () => {
        const json = await finceptFetch('/macro/wgb/credit-ratings');
        return extractRows(json);
      },

      getFinceptSovereignCds: async () => {
        const json = await finceptFetch('/macro/wgb/sovereign-cds');
        return extractRows(json);
      },

      getFinceptBondSpreads: async () => {
        const json = await finceptFetch('/macro/wgb/bond-spreads');
        return extractRows(json);
      },

      getFinceptInvertedYields: async () => {
        const json = await finceptFetch('/macro/wgb/inverted-yields');
        return extractRows(json);
      },
    });

    this.connected = true;
    console.log('[EconomicsMCPBridge] Connected');
  }

  disconnect(): void {
    if (!this.connected) return;
    const cleared = Object.fromEntries(CONTEXT_KEYS.map(k => [k, undefined]));
    terminalMCPProvider.setContexts(cleared);
    this.connected = false;
    console.log('[EconomicsMCPBridge] Disconnected');
  }

  isConnected(): boolean {
    return this.connected;
  }
}

export const economicsMCPBridge = new EconomicsMCPBridge();
