// File: src/services/tradeService.ts
// Trade Data Service - Integrates with Fincept Backend Trade API

// import { loggerService } from './loggerService';

const API_BASE = 'https://finceptbackend.share.zrok.io';

export interface TradePartner {
  partner_id: string;
  partner_name: string;
  export_value: number;
  import_value: number;
  total_trade: number;
  flow_type?: string;
}

export interface ChordData {
  country_id: string;
  country_name: string;
  year: number;
  hs_code: string;
  total_export: number;
  total_import: number;
  total_trade: number;
  partners: TradePartner[];
}

export interface Country {
  country_id: string;
  country_name: string;
  continent_id: string;
  continent_name: string;
}

export interface Product {
  hs_code: string;
  description: string;
  level: number;
  parent_code?: string;
}

export interface TimeSeriesPoint {
  year: number;
  export_value: number;
  import_value: number;
  total_trade: number;
}

export interface BilateralTrade {
  exporter_id: string;
  exporter_name: string;
  importer_id: string;
  importer_name: string;
  hs_code: string;
  year: number;
  trade_value: number;
}

export interface TradeBalance {
  country_id: string;
  country_name: string;
  year: number;
  total_exports: number;
  total_imports: number;
  trade_balance: number;
  balance_type: 'surplus' | 'deficit';
}

class TradeService {
  // private logger = loggerService.createLogger('TradeService');

  /**
   * Get API key from session storage (AuthContext)
   */
  private getApiKey(): string {
    try {
      const sessionData = localStorage.getItem('fincept_session');
      if (!sessionData) {
        console.warn('[TradeService] No session found in localStorage');
        return '';
      }

      const session = JSON.parse(sessionData);
      const apiKey = session.api_key;

      if (!apiKey) {
        console.warn('[TradeService] No API key found in session');
      }

      return apiKey || '';
    } catch (error) {
      console.error('[TradeService] Failed to retrieve API key from session:', error);
      return '';
    }
  }

  /**
   * Make authenticated API request
   */
  private async apiRequest<T>(endpoint: string, params?: Record<string, any>): Promise<T> {
    const apiKey = this.getApiKey();
    const url = new URL(`${API_BASE}${endpoint}`);

    // Add query parameters
    if (params) {
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          url.searchParams.append(key, String(value));
        }
      });
    }

    console.log(`[TradeService] API Request: ${url.toString()}`);

    try {
      const response = await fetch(url.toString(), {
        method: 'GET',
        headers: {
          'X-API-Key': apiKey,
          'Content-Type': 'application/json',
        },
      });

      if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`API error (${response.status}): ${errorText}`);
      }

      const data = await response.json();
      console.log('[TradeService] API Response:', data);
      return data;
    } catch (error) {
      console.error('[TradeService] API Request failed:', error);
      throw error;
    }
  }

  /**
   * Get chord diagram data for a country
   * This is the PRIMARY endpoint for the visualization
   */
  async getChordData(
    countryId: string,
    year?: number,
    hsCode?: string,
    topN?: number
  ): Promise<ChordData> {
    const response: any = await this.apiRequest(`/trade/chord-data/${countryId}`, {
      year: year || 2019,
      hs_code: hsCode || '1',
      top_n: topN || 15,
    });

    // Transform API response to match our interface
    const data = response.data;
    return {
      country_id: data.country.id,
      country_name: data.country.name,
      year: data.year,
      hs_code: data.hs_code,
      total_export: data.flows.filter((f: any) => f.source === data.country.id).reduce((sum: number, f: any) => sum + f.value, 0),
      total_import: data.flows.filter((f: any) => f.target === data.country.id).reduce((sum: number, f: any) => sum + f.value, 0),
      total_trade: data.flows.reduce((sum: number, f: any) => sum + f.value, 0),
      partners: data.nodes.filter((n: any) => n.type === 'partner').map((partner: any) => {
        const exportFlow = data.flows.find((f: any) => f.source === data.country.id && f.target === partner.id);
        const importFlow = data.flows.find((f: any) => f.source === partner.id && f.target === data.country.id);
        return {
          partner_id: partner.id,
          partner_name: partner.name,
          export_value: exportFlow?.value || 0,
          import_value: importFlow?.value || 0,
          total_trade: (exportFlow?.value || 0) + (importFlow?.value || 0)
        };
      })
    };
  }

  /**
   * Get top trading partners for a country
   */
  async getCountryPartners(
    countryId: string,
    year: number,
    hsCode?: string,
    flowType?: 'export' | 'import' | 'total',
    limit?: number
  ): Promise<TradePartner[]> {
    return this.apiRequest<TradePartner[]>(`/trade/country-partners/${countryId}`, {
      year,
      hs_code: hsCode || '1',
      flow_type: flowType || 'total',
      limit: limit || 50,
    });
  }

  /**
   * Get bilateral trade data between exporter and importer
   */
  async getBilateralTrade(
    hsCode: string,
    year: number,
    exporter?: string,
    importer?: string,
    limit?: number
  ): Promise<BilateralTrade[]> {
    return this.apiRequest<BilateralTrade[]>('/trade/bilateral', {
      hs_code: hsCode,
      year,
      exporter,
      importer,
      limit: limit || 100,
    });
  }

  /**
   * Get time series data for a country
   */
  async getTimeSeries(
    countryId: string,
    hsCode?: string,
    startYear?: number,
    endYear?: number,
    flow?: 'export' | 'import' | 'both'
  ): Promise<TimeSeriesPoint[]> {
    return this.apiRequest<TimeSeriesPoint[]>(`/trade/time-series/${countryId}`, {
      hs_code: hsCode || '1',
      start_year: startYear || 1995,
      end_year: endYear || 2023,
      flow: flow || 'both',
    });
  }

  /**
   * Get trade balance for a country
   */
  async getTradeBalance(
    countryId: string,
    hsCode?: string,
    year?: number
  ): Promise<TradeBalance> {
    return this.apiRequest<TradeBalance>(`/trade/balance/${countryId}`, {
      hs_code: hsCode || '1',
      year: year || 2023,
    });
  }

  /**
   * Get trade growth rates
   */
  async getTradeGrowth(
    countryId: string,
    hsCode?: string,
    year?: number,
    flow?: 'export' | 'import'
  ): Promise<any> {
    return this.apiRequest(`/trade/growth/${countryId}`, {
      hs_code: hsCode || '1',
      year: year || 2023,
      flow: flow || 'export',
    });
  }

  /**
   * Get top exporters globally
   */
  async getTopExporters(hsCode?: string, year?: number, limit?: number): Promise<any[]> {
    return this.apiRequest('/trade/top-exporters', {
      hs_code: hsCode || '1',
      year: year || 2023,
      limit: limit || 20,
    });
  }

  /**
   * Get top importers globally
   */
  async getTopImporters(hsCode?: string, year?: number, limit?: number): Promise<any[]> {
    return this.apiRequest('/trade/top-importers', {
      hs_code: hsCode || '1',
      year: year || 2023,
      limit: limit || 20,
    });
  }

  /**
   * Get continental trade aggregates
   */
  async getContinentTrade(hsCode?: string, year?: number): Promise<any> {
    return this.apiRequest('/trade/continent-trade', {
      hs_code: hsCode || '1',
      year: year || 2023,
    });
  }

  /**
   * Get list of all countries
   */
  async getCountries(continent?: string, search?: string): Promise<Country[]> {
    const response: any = await this.apiRequest('/trade/countries', {
      continent,
      search,
    });
    return response.data?.countries || [];
  }

  /**
   * Get list of products (HS codes)
   */
  async getProducts(level?: number, parent?: string): Promise<Product[]> {
    return this.apiRequest<Product[]>('/trade/products', {
      level,
      parent,
    });
  }

  /**
   * Get list of continents
   */
  async getContinents(): Promise<any[]> {
    return this.apiRequest('/trade/continents');
  }

  /**
   * Health check for trade API
   */
  async healthCheck(): Promise<any> {
    return this.apiRequest('/trade/health');
  }
}

export const tradeService = new TradeService();
