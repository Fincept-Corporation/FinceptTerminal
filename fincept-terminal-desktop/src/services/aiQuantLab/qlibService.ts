/**
 * Qlib Service - Frontend API for Microsoft Qlib integration
 * Handles all Qlib operations including data fetching, model training, and backtesting
 */

import { invoke } from '@tauri-apps/api/core';

export interface QlibModel {
  id: string;
  name: string;
  description: string;
  type: 'tree_based' | 'neural_network';
  features: string[];
  use_cases: string[];
}

export interface QlibStatusResponse {
  success: boolean;
  qlib_available: boolean;
  initialized: boolean;
  version?: string;
  error?: string;
}

export interface QlibInitializeResponse {
  success: boolean;
  message?: string;
  provider_uri?: string;
  region?: string;
  error?: string;
}

export interface QlibModelsResponse {
  success: boolean;
  models?: QlibModel[];
  count?: number;
  error?: string;
}

export interface FactorLibrary {
  [key: string]: {
    description: string;
    factor_count: number;
    category: string;
  };
}

export interface FactorLibraryResponse {
  success: boolean;
  factors?: FactorLibrary;
  error?: string;
}

export interface MarketDataRequest {
  instruments: string[];
  start_date: string;
  end_date: string;
  fields?: string[];
}

export interface MarketDataResponse {
  success: boolean;
  data?: any;
  instruments?: string[];
  date_range?: {
    start: string;
    end: string;
  };
  error?: string;
}

export interface BacktestRequest {
  strategy_config: any;
  portfolio_config: any;
}

export interface BacktestResponse {
  success: boolean;
  metrics?: any;
  message?: string;
  error?: string;
}

class QlibService {
  /**
   * Check Qlib installation status and availability
   */
  async checkStatus(): Promise<QlibStatusResponse> {
    console.log('[QlibService] checkStatus() called');
    try {
      console.log('[QlibService] Invoking qlib_check_status command...');
      const result = await invoke<string>('qlib_check_status');
      console.log('[QlibService] Raw result:', result);
      const parsed = JSON.parse(result);
      console.log('[QlibService] Parsed result:', parsed);
      return parsed;
    } catch (error) {
      console.error('[QlibService] checkStatus error:', error);
      console.error('[QlibService] Full error object:', JSON.stringify(error, null, 2));
      return {
        success: false,
        qlib_available: false,
        initialized: false,
        error: String(error)
      };
    }
  }

  /**
   * Initialize Qlib with data provider
   * @param providerUri - Path to Qlib data directory
   * @param region - Market region ('cn', 'us', etc.)
   */
  async initialize(
    providerUri?: string,
    region?: string
  ): Promise<QlibInitializeResponse> {
    console.log('[QlibService] initialize() called with:', { providerUri, region });
    try {
      console.log('[QlibService] Invoking qlib_initialize command...');
      const result = await invoke<string>('qlib_initialize', {
        provider_uri: providerUri,
        region
      });
      console.log('[QlibService] Initialize raw result:', result);
      const parsed = JSON.parse(result);
      console.log('[QlibService] Initialize parsed result:', parsed);
      return parsed;
    } catch (error) {
      console.error('[QlibService] initialize error:', error);
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * List all available pre-trained models
   */
  async listModels(): Promise<QlibModelsResponse> {
    try {
      const result = await invoke<string>('qlib_list_models');
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Get factor library information
   */
  async getFactorLibrary(): Promise<FactorLibraryResponse> {
    try {
      const result = await invoke<string>('qlib_get_factor_library');
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Fetch market data
   */
  async getData(request: MarketDataRequest): Promise<MarketDataResponse> {
    try {
      const result = await invoke<string>('qlib_get_data', {
        instruments: request.instruments,
        startDate: request.start_date,
        endDate: request.end_date,
        fields: request.fields
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Train a model
   */
  async trainModel(
    modelType: string,
    datasetConfig: any,
    modelConfig?: any
  ): Promise<any> {
    try {
      const result = await invoke<string>('qlib_train_model', {
        modelType,
        datasetConfig: JSON.stringify(datasetConfig),
        modelConfig: modelConfig ? JSON.stringify(modelConfig) : null
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Run backtest
   */
  async runBacktest(request: BacktestRequest): Promise<BacktestResponse> {
    try {
      const result = await invoke<string>('qlib_run_backtest', {
        strategyConfig: JSON.stringify(request.strategy_config),
        portfolioConfig: JSON.stringify(request.portfolio_config)
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Optimize portfolio
   */
  async optimizePortfolio(predictions: any, method?: string): Promise<any> {
    try {
      const result = await invoke<string>('qlib_optimize_portfolio', {
        predictions: JSON.stringify(predictions),
        method
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }
}

// Export singleton instance
export const qlibService = new QlibService();
