/**
 * Fyers Authentication Helpers
 *
 * Module-level auth functions used by FyersAdapter
 */

import { invoke } from '@tauri-apps/api/core';
import type {
  BrokerCredentials,
  AuthResponse,
  AuthCallbackParams,
} from '../../types';

/**
 * Exchange auth code for access token via Rust backend
 */
export async function fyersExchangeToken(
  apiKey: string,
  apiSecret: string,
  authCode: string
): Promise<AuthResponse> {
  try {
    const response = await invoke<{
      success: boolean;
      access_token?: string;
      user_id?: string;
      error?: string;
    }>('fyers_exchange_token', {
      apiKey,
      apiSecret,
      authCode,
    });

    if (response.success && response.access_token) {
      return {
        success: true,
        accessToken: response.access_token,
        userId: response.user_id || undefined,
        message: 'Authentication successful',
      };
    }

    return {
      success: false,
      message: response.error || 'Token exchange failed',
      errorCode: 'TOKEN_EXCHANGE_FAILED',
    };
  } catch (error) {
    return {
      success: false,
      message: error instanceof Error ? error.message : 'Authentication callback failed',
      errorCode: 'AUTH_CALLBACK_FAILED',
    };
  }
}

/**
 * Store credentials securely via Rust backend
 */
export async function fyersStoreCredentials(
  brokerId: string,
  credentials: BrokerCredentials
): Promise<void> {
  try {
    await invoke('store_indian_broker_credentials', {
      brokerId,
      credentials: {
        api_key: credentials.apiKey,
        api_secret: credentials.apiSecret || '',
        access_token: credentials.accessToken || '',
        user_id: credentials.userId || '',
      },
    });
  } catch (error) {
    console.error('Failed to store credentials:', error);
  }
}

/**
 * Clear stored credentials
 */
export async function fyersClearCredentials(brokerId: string): Promise<void> {
  try {
    await invoke('delete_indian_broker_credentials', {
      brokerId,
    });
  } catch (error) {
    console.error('Failed to clear credentials:', error);
  }
}

/**
 * Validate token freshness (IST date check)
 * Fyers access tokens expire at end of each trading day (IST)
 */
export function fyersIsTokenFromToday(tokenCreatedAt?: Date): boolean {
  if (!tokenCreatedAt) return false;

  const now = new Date();
  const istFormatter = new Intl.DateTimeFormat('en-US', {
    timeZone: 'Asia/Kolkata',
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
  });

  const todayIST = istFormatter.format(now);
  const tokenDateIST = istFormatter.format(tokenCreatedAt);

  return todayIST === tokenDateIST;
}

/**
 * Build OAuth login URL
 */
export function fyersBuildAuthUrl(apiKey: string, loginUrl: string): string {
  const redirectUri = encodeURIComponent('https://127.0.0.1/');
  const responseType = 'code';
  const state = 'fyers_auth';
  return `${loginUrl}?client_id=${apiKey}&redirect_uri=${redirectUri}&response_type=${responseType}&state=${state}`;
}
