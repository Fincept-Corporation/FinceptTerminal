// Kite Connect Portfolio Module
// Handles holdings, positions, and portfolio operations

import { AxiosInstance } from 'axios';
import { Holding, Position } from './types';

export class KitePortfolio {
  constructor(private client: AxiosInstance) {}

  /**
   * Get holdings
   */
  async getHoldings(): Promise<Holding[]> {
    const response = await this.client.get('/portfolio/holdings');
    return response.data.data;
  }

  /**
   * Get positions
   */
  async getPositions(): Promise<{ net: Position[]; day: Position[] }> {
    const response = await this.client.get('/portfolio/positions');
    return response.data.data;
  }

  /**
   * Convert position
   */
  async convertPosition(params: {
    tradingsymbol: string;
    exchange: string;
    transaction_type: 'BUY' | 'SELL';
    position_type: 'overnight' | 'day';
    quantity: number;
    old_product: string;
    new_product: string;
  }): Promise<boolean> {
    const data = new URLSearchParams();
    Object.entries(params).forEach(([key, value]) => {
      data.append(key, value.toString());
    });

    const response = await this.client.put('/portfolio/positions', data);
    return response.data.data;
  }

  /**
   * Get holdings auction list
   */
  async getHoldingsAuctions(): Promise<any[]> {
    const response = await this.client.get('/portfolio/holdings/auctions');
    return response.data.data;
  }

  /**
   * Initiate holdings authorisation
   */
  async initiateHoldingsAuth(
    holdings?: Array<{ isin: string; quantity: number }>
  ): Promise<{ request_id: string }> {
    const data = new URLSearchParams();

    if (holdings) {
      holdings.forEach(holding => {
        data.append('isin', holding.isin);
        data.append('quantity', holding.quantity.toString());
      });
    }

    const response = await this.client.post('/portfolio/holdings/authorise', data);
    return response.data.data;
  }
}
