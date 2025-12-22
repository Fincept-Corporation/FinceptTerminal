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
    try {
      const result = await invoke<string>('qlib_check_status');
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        qlib_available: false,
        initialized: false,
        error: error as string
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
    try {
      const result = await invoke<string>('qlib_initialize', {
        providerUri,
        region
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
