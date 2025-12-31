// Kite Connect Adapter
// Main adapter class that orchestrates all Kite Connect modules

import axios, { AxiosInstance, AxiosResponse } from 'axios';
import { KiteConfig } from './types';
import { KiteAuth } from './auth';
import { KiteMarket } from './market';
import { KiteTrading } from './trading';
import { KitePortfolio } from './portfolio';

export class KiteConnectAdapter {
  private config: KiteConfig;
  private client: AxiosInstance;
  private readonly baseUrl: string;

  public auth: KiteAuth;
  public market: KiteMarket;
  public trading: KiteTrading;
  public portfolio: KitePortfolio;

  constructor(config: KiteConfig) {
    this.config = config;
    this.baseUrl = config.baseUrl || 'https://api.kite.trade';

    this.client = axios.create({
      baseURL: this.baseUrl,
      timeout: config.timeout || 10000,
      headers: {
        'X-Kite-Version': '3',
        'Content-Type': 'application/x-www-form-urlencoded',
      },
    });

    this.setupInterceptors();

    this.auth = new KiteAuth(this.client, config.apiKey, config.apiSecret);
    this.market = new KiteMarket(this.client);
    this.trading = new KiteTrading(this.client);
    this.portfolio = new KitePortfolio(this.client);
  }

  private setupInterceptors(): void {
    this.client.interceptors.request.use(
      (config) => {
        if (this.config.accessToken) {
          config.headers.Authorization = `token ${this.config.apiKey}:${this.config.accessToken}`;
        }
        return config;
      },
      (error) => Promise.reject(error)
    );

    this.client.interceptors.response.use(
      (response: AxiosResponse) => response,
      (error) => {
        if (error.response?.status === 403) {
          throw new Error('Token expired or invalid. Please re-authenticate.');
        }
        throw error;
      }
    );
  }

  /**
   * Set access token (for session persistence)
   */
  setAccessToken(accessToken: string): void {
    this.config.accessToken = accessToken;
  }

  /**
   * Get current access token
   */
  getAccessToken(): string | undefined {
    return this.config.accessToken;
  }

  /**
   * Check if authenticated
   */
  isAuthenticated(): boolean {
    return !!this.config.accessToken;
  }
}

export function createKiteAdapter(config: KiteConfig): KiteConnectAdapter {
  return new KiteConnectAdapter(config);
}

export default KiteConnectAdapter;
