import { invoke } from '@tauri-apps/api/core';

// Type definitions for BIS API responses
export interface BISDataResponse {
  success: boolean;
  data?: any;
  error?: string;
  endpoint?: string;
  timestamp?: string;
}

export interface BISEconomicOverview {
  economic_overview: {
    effective_exchange_rates?: BISDataResponse;
    central_bank_policy_rates?: BISDataResponse;
    long_term_interest_rates?: BISDataResponse;
    short_term_interest_rates?: BISDataResponse;
    exchange_rates?: BISDataResponse;
    credit_to_non_financial_sector?: BISDataResponse;
  };
  countries: string[];
  period: {
    start: string;
    end: string;
  };
  indicators_requested: number;
  indicators_retrieved: number;
}

/**
 * BIS Data Service
 * Provides access to Bank for International Settlements data via Tauri commands
 */
class BISDataService {
  /**
   * Get comprehensive economic overview for countries
   */
  async getEconomicOverview(
    countries?: string[],
    startPeriod?: string,
    endPeriod?: string
  ): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('get_bis_economic_overview', {
        countries: countries || null,
        startPeriod: startPeriod || null,
        endPeriod: endPeriod || null,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error fetching BIS economic overview:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Get central bank policy rates
   */
  async getCentralBankPolicyRates(
    countries?: string[],
    startPeriod?: string,
    endPeriod?: string
  ): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('get_bis_central_bank_policy_rates', {
        countries: countries || null,
        startPeriod: startPeriod || null,
        endPeriod: endPeriod || null,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error fetching central bank policy rates:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Get effective exchange rates
   */
  async getEffectiveExchangeRates(
    countries?: string[],
    startPeriod?: string,
    endPeriod?: string
  ): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('get_bis_effective_exchange_rates', {
        countries: countries || null,
        startPeriod: startPeriod || null,
        endPeriod: endPeriod || null,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error fetching effective exchange rates:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Get interest rates (long-term or short-term)
   */
  async getInterestRates(
    type: 'long_term' | 'short_term',
    countries?: string[],
    startPeriod?: string,
    endPeriod?: string
  ): Promise<BISDataResponse> {
    try {
      const command = type === 'long_term'
        ? 'get_bis_long_term_interest_rates'
        : 'get_bis_short_term_interest_rates';

      const response = await invoke<string>(command, {
        countries: countries || null,
        startPeriod: startPeriod || null,
        endPeriod: endPeriod || null,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error(`Error fetching ${type} interest rates:`, error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Get exchange rates
   */
  async getExchangeRates(
    currencies?: string[],
    startPeriod?: string,
    endPeriod?: string
  ): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('get_bis_exchange_rates', {
        currencyPairs: currencies || null,
        startPeriod: startPeriod || null,
        endPeriod: endPeriod || null,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error fetching exchange rates:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Get credit to non-financial sector
   */
  async getCreditToNonFinancialSector(
    countries?: string[],
    sectors?: string[],
    startPeriod?: string,
    endPeriod?: string
  ): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('get_bis_credit_to_non_financial_sector', {
        countries: countries || null,
        sectors: sectors || null,
        startPeriod: startPeriod || null,
        endPeriod: endPeriod || null,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error fetching credit data:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Get house prices
   */
  async getHousePrices(
    countries?: string[],
    startPeriod?: string,
    endPeriod?: string
  ): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('get_bis_house_prices', {
        countries: countries || null,
        startPeriod: startPeriod || null,
        endPeriod: endPeriod || null,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error fetching house prices:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Get available datasets
   */
  async getAvailableDatasets(): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('get_bis_available_datasets');
      return JSON.parse(response);
    } catch (error) {
      console.error('Error fetching available datasets:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Search datasets by query
   */
  async searchDatasets(query: string): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('search_bis_datasets', {
        query,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error searching datasets:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Get raw data for a specific flow
   */
  async getData(
    flow: string,
    key?: string,
    startPeriod?: string,
    endPeriod?: string
  ): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('get_bis_data', {
        flow,
        key: key || 'all',
        startPeriod: startPeriod || null,
        endPeriod: endPeriod || null,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error fetching BIS data:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Get dataset metadata
   */
  async getDatasetMetadata(flow: string): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('get_bis_dataset_metadata', {
        flow,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error fetching dataset metadata:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Get monetary policy overview
   */
  async getMonetaryPolicyOverview(
    countries?: string[],
    startPeriod?: string,
    endPeriod?: string
  ): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('get_bis_monetary_policy_overview', {
        countries: countries || null,
        startPeriod: startPeriod || null,
        endPeriod: endPeriod || null,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error fetching monetary policy overview:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Execute custom BIS command
   */
  async executeCommand(command: string, args: string[]): Promise<BISDataResponse> {
    try {
      const response = await invoke<string>('execute_bis_command', {
        command,
        args,
      });
      return JSON.parse(response);
    } catch (error) {
      console.error('Error executing BIS command:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }
}

// Export singleton instance
export const bisDataService = new BISDataService();
export default bisDataService;
