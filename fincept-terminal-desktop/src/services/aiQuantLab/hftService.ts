/**
 * HFT Service - High Frequency Trading operations
 * Order book, market making, microstructure analysis
 */

import { invoke } from '@tauri-apps/api/core';

export interface CreateOrderBookRequest {
  symbol: string;
  depth?: number;
}

export interface UpdateOrderBookRequest {
  symbol: string;
  bids: { [price: string]: number };
  asks: { [price: string]: number };
  timestamp?: string;
}

export interface MarketMakingQuotesRequest {
  symbol: string;
  inventory?: number;
  risk_aversion?: number;
  spread_multiplier?: number;
}

export interface OrderBookSnapshotRequest {
  symbol: string;
  n_levels?: number;
}

class HFTService {
  async initialize(providerUri: string = '~/.qlib/qlib_data/cn_data', region: string = 'cn'): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_high_frequency',
        args: ['initialize', providerUri, region]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to initialize' };
    }
  }

  async createOrderBook(request: CreateOrderBookRequest): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_high_frequency',
        args: ['create_orderbook', request.symbol, (request.depth || 10).toString()]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to create order book' };
    }
  }

  async updateOrderBook(request: UpdateOrderBookRequest): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_high_frequency',
        args: ['update_orderbook', JSON.stringify(request)]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to update order book' };
    }
  }

  async calculateMarketMakingQuotes(request: MarketMakingQuotesRequest): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_high_frequency',
        args: ['market_making_quotes', JSON.stringify(request)]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to calculate quotes' };
    }
  }

  async detectToxicFlow(symbol: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_high_frequency',
        args: ['detect_toxic', symbol]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to detect toxic flow' };
    }
  }

  async getOrderBookSnapshot(request: OrderBookSnapshotRequest): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_high_frequency',
        args: ['snapshot', request.symbol, (request.n_levels || 5).toString()]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to get snapshot' };
    }
  }

  async getLatencyStats(): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_high_frequency',
        args: ['latency_stats']
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to get latency stats' };
    }
  }
}

export const hftService = new HFTService();
