/**
 * Fyers Portfolio Operations
 *
 * Module-level portfolio functions used by FyersAdapter
 */

import { invoke } from '@tauri-apps/api/core';
import type {
  Position,
  Holding,
  Funds,
  StockExchange,
} from '../../types';
import { fromFyersPosition, fromFyersHolding, fromFyersFunds } from './mapper';

/**
 * Get all positions
 */
export async function fyersGetPositions(
  apiKey: string,
  accessToken: string
): Promise<Position[]> {
  try {
    const response = await invoke<{
      success: boolean;
      data?: unknown[];
      error?: string;
    }>('fyers_get_positions', {
      apiKey,
      accessToken,
    });

    if (response.success && response.data) {
      return response.data.map((position: unknown) =>
        fromFyersPosition(position as Record<string, unknown>)
      );
    }

    return [];
  } catch (error) {
    console.error('Failed to fetch positions:', error);
    return [];
  }
}

/**
 * Get open position quantity for a specific symbol
 */
export async function fyersGetOpenPositionQuantity(
  apiKey: string,
  accessToken: string,
  symbol: string,
  exchange: StockExchange,
  productType: string
): Promise<number> {
  try {
    const positions = await fyersGetPositions(apiKey, accessToken);
    const position = positions.find(
      p =>
        p.symbol === symbol &&
        p.exchange === exchange &&
        p.productType === productType
    );

    return position ? position.quantity : 0;
  } catch (error) {
    console.error('Failed to get open position:', error);
    return 0;
  }
}

/**
 * Get all holdings
 */
export async function fyersGetHoldings(
  apiKey: string,
  accessToken: string
): Promise<Holding[]> {
  try {
    const response = await invoke<{
      success: boolean;
      data?: unknown[];
      error?: string;
    }>('fyers_get_holdings', {
      apiKey,
      accessToken,
    });

    if (response.success && response.data) {
      return response.data.map((holding: unknown) =>
        fromFyersHolding(holding as Record<string, unknown>)
      );
    }

    return [];
  } catch (error) {
    console.error('Failed to fetch holdings:', error);
    return [];
  }
}

/**
 * Get account funds/margins
 * Combines funds data with unrealized P&L from positions (like OpenAlgo)
 */
export async function fyersGetFunds(
  apiKey: string,
  accessToken: string
): Promise<Funds> {
  try {
    const response = await invoke<{
      success: boolean;
      data?: unknown;
      error?: string;
    }>('fyers_get_funds', {
      apiKey,
      accessToken,
    });

    if (response.success && response.data) {
      const funds = fromFyersFunds(response.data as Record<string, unknown>);

      // Get positions to calculate total unrealized P&L (OpenAlgo pattern)
      try {
        const positions = await fyersGetPositions(apiKey, accessToken);
        const totalUnrealizedPnl = positions.reduce((sum, pos) => sum + pos.pnl, 0);

        if (positions.length > 0) {
          funds.unrealizedPnl = totalUnrealizedPnl;
        }
      } catch (err) {
        console.warn('[Fyers] Failed to fetch position P&L for funds:', err);
      }

      return funds;
    }

    return {
      availableCash: 0,
      usedMargin: 0,
      availableMargin: 0,
      totalBalance: 0,
      currency: 'INR',
    };
  } catch (error) {
    console.error('Failed to fetch funds:', error);
    return {
      availableCash: 0,
      usedMargin: 0,
      availableMargin: 0,
      totalBalance: 0,
      currency: 'INR',
    };
  }
}
