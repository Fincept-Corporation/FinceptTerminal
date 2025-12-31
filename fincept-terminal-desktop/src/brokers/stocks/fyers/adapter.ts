// Fyers Broker Adapter
// Main adapter class that orchestrates all Fyers modules

import { fyersModel } from 'fyers-web-sdk-v3';
import { sqliteService } from '../../../services/sqliteService';
import { FyersCredentials } from './types';
import { FyersAuth } from './auth';
import { FyersMarket } from './market';
import { FyersTrading } from './trading';
import { FyersPortfolio } from './portfolio';
import { FyersWebSocket } from './websocket';

export class FyersAdapter {
  private fyers: any = null;
  private isInitialized = false;

  public auth: FyersAuth;
  public market: FyersMarket;
  public trading: FyersTrading;
  public portfolio: FyersPortfolio;
  public websocket: FyersWebSocket;

  constructor() {
    this.auth = new FyersAuth();
    this.market = new FyersMarket(null);
    this.trading = new FyersTrading(null);
    this.portfolio = new FyersPortfolio(null);
    this.websocket = new FyersWebSocket();
  }

  /**
   * Initialize Fyers client with credentials from database
   */
  async initialize(): Promise<boolean> {
    try {
      console.log('[Fyers] Starting initialization...');
      const creds = await sqliteService.getCredentialByService('fyers');

      console.log('[Fyers] Credentials loaded:', {
        exists: !!creds,
        hasApiKey: !!creds?.api_key,
        hasApiSecret: !!creds?.api_secret,
        hasPassword: !!creds?.password
      });

      if (!creds || !creds.api_key || !creds.api_secret) {
        console.error('[Fyers] Missing required credentials');
        return false;
      }

      const additionalData = creds.additional_data ? JSON.parse(creds.additional_data) : {};
      const appType = additionalData.appType || '100';
      console.log('[Fyers] App type:', appType);

      // If we have an access token, initialize the Fyers SDK
      if (creds.password) {
        console.log('[Fyers] Access token found, initializing SDK...');
        const fullAccessToken = `${creds.api_key}-${appType}:${creds.password}`;

        this.fyers = new fyersModel({
          AccessToken: fullAccessToken,
          AppID: creds.api_key,
          RedirectURL: creds.api_secret,
          enableLogging: false
        });

        this.market = new FyersMarket(this.fyers);
        this.trading = new FyersTrading(this.fyers);
        this.portfolio = new FyersPortfolio(this.fyers);
        console.log('[Fyers] SDK initialized with access token');
      } else {
        console.log('[Fyers] No access token yet, will authenticate');
      }

      // Mark as initialized even without access token (will authenticate next)
      this.isInitialized = true;
      console.log('[Fyers] Initialization complete, isInitialized:', this.isInitialized);
      return true;
    } catch (error) {
      console.error('[Fyers] Initialization error:', error);
      this.isInitialized = false;
      return false;
    }
  }

  /**
   * Save Fyers credentials to database
   */
  async saveCredentials(credentials: FyersCredentials): Promise<void> {
    await sqliteService.saveCredential({
      service_name: 'fyers',
      username: 'fyers_user',
      api_key: credentials.appId,
      api_secret: credentials.redirectUrl,
      password: credentials.accessToken,
      additional_data: JSON.stringify({
        appType: credentials.appType || '100',
        connected_at: new Date().toISOString()
      })
    });

    await this.initialize();
  }

  /**
   * Check if service is ready
   */
  isReady(): boolean {
    return this.isInitialized && this.fyers !== null;
  }

  /**
   * Get stored credentials from database
   */
  async getCredentials(): Promise<FyersCredentials | null> {
    try {
      const creds = await sqliteService.getCredentialByService('fyers');
      if (!creds || !creds.api_key) {
        return null;
      }

      const additionalData = creds.additional_data ? JSON.parse(creds.additional_data) : {};

      return {
        appId: creds.api_key,
        redirectUrl: creds.api_secret || '',
        accessToken: creds.password || '',
        appType: additionalData.appType || '100',
      };
    } catch (error) {
      console.error('[Fyers] Failed to get credentials:', error);
      return null;
    }
  }
}

export const fyersAdapter = new FyersAdapter();
