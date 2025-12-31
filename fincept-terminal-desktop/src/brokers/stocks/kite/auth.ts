// Kite Connect Authentication Module
// Handles login, session management, and access token operations

import { AxiosInstance } from 'axios';
import { SessionData } from './types';

// Browser-compatible SHA256 helper
async function sha256(text: string): Promise<string> {
  const encoder = new TextEncoder();
  const data = encoder.encode(text);
  const hashBuffer = await crypto.subtle.digest('SHA-256', data);
  const hashArray = Array.from(new Uint8Array(hashBuffer));
  return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
}

export class KiteAuth {
  constructor(
    private client: AxiosInstance,
    private apiKey: string,
    private apiSecret: string
  ) {}

  /**
   * Generate login URL for manual authentication
   * User must complete this step manually
   */
  generateLoginUrl(redirectUrl?: string): string {
    const url = new URL('https://kite.zerodha.com/connect/login');
    url.searchParams.set('api_key', this.apiKey);
    if (redirectUrl) {
      url.searchParams.set('redirect_url', redirectUrl);
    }
    return url.toString();
  }

  /**
   * Generate session after manual login
   * Call this with the request_token received from redirect
   */
  async generateSession(requestToken: string): Promise<SessionData> {
    const checksum = await sha256(this.apiKey + requestToken + this.apiSecret);

    const data = new URLSearchParams({
      api_key: this.apiKey,
      request_token: requestToken,
      checksum: checksum,
    });

    const response = await this.client.post('/session/token', data);
    return response.data.data;
  }

  /**
   * Invalidate session
   */
  async invalidateSession(): Promise<void> {
    await this.client.delete('/session/token');
  }

  /**
   * Get user profile
   */
  async getProfile(): Promise<any> {
    const response = await this.client.get('/user/profile');
    return response.data.data;
  }

  /**
   * Get user margins
   */
  async getMargins(segment?: string): Promise<any> {
    const url = segment ? `/user/margins/${segment}` : '/user/margins';
    const response = await this.client.get(url);
    return response.data.data;
  }

  /**
   * Validate postback checksum
   */
  async validatePostbackChecksum(
    orderId: string,
    orderTimestamp: string,
    receivedChecksum: string
  ): Promise<boolean> {
    const expectedChecksum = await sha256(orderId + orderTimestamp + this.apiSecret);
    return expectedChecksum === receivedChecksum;
  }
}
