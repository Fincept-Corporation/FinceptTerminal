// Kite Connect Market Data Module
// Handles quotes, OHLCV, historical data, and instruments

import { AxiosInstance } from 'axios';
import { Quote, HistoricalParams, CandleData } from './types';

export class KiteMarket {
  constructor(private client: AxiosInstance) {}

  /**
   * Get full market quotes for instruments
   * Max 500 instruments per request
   */
  async getQuotes(instruments: string[]): Promise<Record<string, Quote>> {
    if (instruments.length > 500) {
      throw new Error('Maximum 500 instruments allowed per request');
    }

    const params = new URLSearchParams();
    instruments.forEach(instrument => params.append('i', instrument));

    const response = await this.client.get(`/quote?${params.toString()}`);
    return response.data.data;
  }

  /**
   * Get OHLC quotes for instruments
   * Max 1000 instruments per request
   */
  async getOHLC(instruments: string[]): Promise<Record<string, any>> {
    if (instruments.length > 1000) {
      throw new Error('Maximum 1000 instruments allowed per request');
    }

    const params = new URLSearchParams();
    instruments.forEach(instrument => params.append('i', instrument));

    const response = await this.client.get(`/quote/ohlc?${params.toString()}`);
    return response.data.data;
  }

  /**
   * Get LTP for instruments
   * Max 1000 instruments per request
   */
  async getLTP(instruments: string[]): Promise<Record<string, any>> {
    if (instruments.length > 1000) {
      throw new Error('Maximum 1000 instruments allowed per request');
    }

    const params = new URLSearchParams();
    instruments.forEach(instrument => params.append('i', instrument));

    const response = await this.client.get(`/quote/ltp?${params.toString()}`);
    return response.data.data;
  }

  /**
   * Get historical data
   */
  async getHistoricalData(params: HistoricalParams): Promise<CandleData[]> {
    const queryParams = new URLSearchParams({
      from: params.from,
      to: params.to,
    });

    if (params.continuous) queryParams.append('continuous', '1');
    if (params.oi) queryParams.append('oi', '1');

    const response = await this.client.get(
      `/instruments/historical/${params.instrument_token}/${params.interval}?${queryParams.toString()}`
    );

    return response.data.data.candles.map((candle: any[]) => ({
      timestamp: candle[0],
      open: candle[1],
      high: candle[2],
      low: candle[3],
      close: candle[4],
      volume: candle[5],
      oi: candle[6] || undefined,
    }));
  }

  /**
   * Get instruments list for exchange
   */
  async getInstruments(exchange?: string): Promise<string> {
    const url = exchange ? `/instruments/${exchange}` : '/instruments';
    const response = await this.client.get(url);
    return response.data;
  }
}
