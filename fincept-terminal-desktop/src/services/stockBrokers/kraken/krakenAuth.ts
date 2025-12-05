/**
 * Kraken Authentication Management
 */

import crypto from 'crypto';
import { KRAKEN_CONFIG } from './krakenConfig';

export interface KrakenCredentials {
  apiKey: string;
  secret: string;
  passphrase?: string; // Not used by Kraken but kept for compatibility
  sandbox?: boolean;
}

export interface AuthTokens {
  apiToken?: string;
  wsToken?: string;
  expiresAt?: number;
}

export class KrakenAuth {
  private credentials: KrakenCredentials | null = null;
  private tokens: AuthTokens = {};

  constructor(credentials?: KrakenCredentials) {
    if (credentials) {
      this.setCredentials(credentials);
    }
  }

  /**
   * Set API credentials
   */
  setCredentials(credentials: KrakenCredentials): void {
    this.validateCredentials(credentials);
    this.credentials = { ...credentials };
    this.tokens = {}; // Clear existing tokens
  }

  /**
   * Get current credentials
   */
  getCredentials(): KrakenCredentials | null {
    return this.credentials ? { ...this.credentials } : null;
  }

  /**
   * Clear all credentials and tokens
   */
  clearCredentials(): void {
    this.credentials = null;
    this.tokens = {};
  }

  /**
   * Check if authenticated
   */
  isAuthenticated(): boolean {
    return this.credentials !== null;
  }

  /**
   * Validate credentials format
   */
  private validateCredentials(credentials: KrakenCredentials): void {
    if (!credentials.apiKey || typeof credentials.apiKey !== 'string') {
      throw new Error('Invalid API key format');
    }

    if (!credentials.secret || typeof credentials.secret !== 'string') {
      throw new Error('Invalid secret format');
    }

    if (credentials.apiKey.length < 20) {
      throw new Error('API key too short');
    }

    if (credentials.secret.length < 32) {
      throw new Error('Secret too short');
    }
  }

  /**
   * Generate API signature for REST requests
   */
  generateSignature(
    endpoint: string,
    params: Record<string, any>,
    nonce: string | number
  ): string {
    if (!this.credentials) {
      throw new Error('No credentials set');
    }

    // Create the message to sign
    const message = nonce + JSON.stringify(params);

    // Decode the secret from base64
    const secretBuffer = Buffer.from(this.credentials.secret, 'base64');

    // Create HMAC-SHA256 signature
    const hmac = crypto.createHmac('sha512', secretBuffer);
    hmac.update(message);
    const signature = hmac.digest('base64');

    return signature;
  }

  /**
   * Generate WebSocket authentication token
   */
  async generateWebSocketToken(): Promise<string> {
    if (!this.credentials) {
      throw new Error('No credentials set');
    }

    // Check if existing token is still valid
    if (this.tokens.wsToken && this.tokens.expiresAt && Date.now() < this.tokens.expiresAt) {
      return this.tokens.wsToken;
    }

    // Generate new token using API-Sign method
    const nonce = Date.now().toString();
    const endpoint = '/ws/auth';

    const params = {
      event: 'subscribe',
      subscription: {
        name: 'authenticate',
        token: this.credentials.apiKey
      }
    };

    const signature = this.generateSignature(endpoint, params, nonce);

    // For Kraken WebSocket, we use the API key directly in the subscription message
    // The signature is used for REST API authentication
    this.tokens.wsToken = this.credentials.apiKey;
    this.tokens.expiresAt = Date.now() + (24 * 60 * 60 * 1000); // 24 hours

    return this.tokens.wsToken;
  }

  /**
   * Get authentication headers for REST requests
   */
  getAuthHeaders(
    endpoint: string,
    params: Record<string, any> = {}
  ): Record<string, string> {
    if (!this.credentials) {
      throw new Error('No credentials set');
    }

    const nonce = Date.now().toString();
    const signature = this.generateSignature(endpoint, params, nonce);

    return {
      'API-Key': this.credentials.apiKey,
      'API-Sign': signature,
      'API-Nonce': nonce,
      'Content-Type': 'application/x-www-form-urlencoded',
    };
  }

  /**
   * Add authentication to request parameters
   */
  addAuthToParams(params: Record<string, any> = {}): Record<string, any> {
    if (!this.credentials) {
      return params;
    }

    return {
      ...params,
      nonce: Date.now().toString(),
    };
  }

  /**
   * Validate API key permissions
   */
  async validatePermissions(): Promise<{
    canTrade: boolean;
    canDeposit: boolean;
    canWithdraw: boolean;
    queryTime: number;
  }> {
    if (!this.credentials) {
      throw new Error('No credentials set');
    }

    try {
      // This would typically make a test API call to validate permissions
      // For now, return a default response
      return {
        canTrade: true,
        canDeposit: true,
        canWithdraw: true,
        queryTime: Date.now(),
      };
    } catch (error) {
      throw new Error(`Failed to validate permissions: ${error}`);
    }
  }

  /**
   * Test connection with current credentials
   */
  async testConnection(): Promise<boolean> {
    if (!this.credentials) {
      throw new Error('No credentials set');
    }

    try {
      // This would typically make a test API call
      // For now, just validate the credentials format
      this.validateCredentials(this.credentials);
      return true;
    } catch (error) {
      return false;
    }
  }

  /**
   * Encrypt credentials for storage
   */
  encryptCredentials(credentials: KrakenCredentials): string {
    const secret = process.env.ENCRYPTION_SECRET || 'default-secret';
    const cipher = crypto.createCipher('aes-256-cbc', secret);

    let encrypted = cipher.update(JSON.stringify(credentials), 'utf8', 'hex');
    encrypted += cipher.final('hex');

    return encrypted;
  }

  /**
   * Decrypt credentials from storage
   */
  decryptCredentials(encrypted: string): KrakenCredentials {
    const secret = process.env.ENCRYPTION_SECRET || 'default-secret';
    const decipher = crypto.createDecipher('aes-256-cbc', secret);

    let decrypted = decipher.update(encrypted, 'hex', 'utf8');
    decrypted += decipher.final('utf8');

    return JSON.parse(decrypted);
  }

  /**
   * Get rate limit information
   */
  getRateLimitInfo(): {
    publicApi: { callsPerSecond: number; lastCall: number };
    privateApi: { callsPerSecond: number; lastCall: number };
    tradingApi: { callsPerSecond: number; lastCall: number };
  } {
    const now = Date.now();
    return {
      publicApi: {
        callsPerSecond: 1000 / KRAKEN_CONFIG.REST.PUBLIC,
        lastCall: now,
      },
      privateApi: {
        callsPerSecond: 1000 / KRAKEN_CONFIG.REST.PRIVATE,
        lastCall: now,
      },
      tradingApi: {
        callsPerSecond: 1000 / KRAKEN_CONFIG.REST.TRADING,
        lastCall: now,
      },
    };
  }

  /**
   * Check if rate limit allows making a request
   */
  canMakeRequest(apiType: 'public' | 'private' | 'trading'): boolean {
    const rateLimitInfo = this.getRateLimitInfo();
    const timeSinceLastCall = Date.now() - rateLimitInfo[`${apiType}Api`].lastCall;
    const minInterval = rateLimitInfo[`${apiType}Api`].callsPerSecond;

    return timeSinceLastCall >= minInterval;
  }

  /**
   * Wait for rate limit if necessary
   */
  async waitForRateLimit(apiType: 'public' | 'private' | 'trading'): Promise<void> {
    if (this.canMakeRequest(apiType)) {
      return;
    }

    const rateLimitInfo = this.getRateLimitInfo();
    const timeSinceLastCall = Date.now() - rateLimitInfo[`${apiType}Api`].lastCall;
    const minInterval = rateLimitInfo[`${apiType}Api`].callsPerSecond;
    const waitTime = minInterval - timeSinceLastCall;

    if (waitTime > 0) {
      await new Promise(resolve => setTimeout(resolve, waitTime));
    }
  }
}