// Fyers Portfolio Module
// Handles holdings, positions, funds, and profile

import { FyersProfile, FyersFund, FyersHoldingsResponse, FyersPosition } from './types';

export class FyersPortfolio {
  private fyers: any;

  constructor(fyersClient: any) {
    this.fyers = fyersClient;
  }

  /**
   * Get user profile
   */
  async getProfile(): Promise<FyersProfile> {
    const response = await this.fyers.get_profile();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch profile');
    }

    return response.data;
  }

  /**
   * Get funds/limits
   */
  async getFunds(): Promise<FyersFund[]> {
    const response = await this.fyers.get_funds();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch funds');
    }

    return response.fund_limit;
  }

  /**
   * Get holdings
   */
  async getHoldings(): Promise<FyersHoldingsResponse> {
    const response = await this.fyers.get_holdings();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch holdings');
    }

    return {
      overall: response.overall,
      holdings: response.holdings
    };
  }

  /**
   * Get positions
   */
  async getPositions(): Promise<FyersPosition[]> {
    const response = await this.fyers.get_positions();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch positions');
    }

    return response.netPositions || [];
  }
}
