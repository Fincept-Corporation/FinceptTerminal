// File: src/services/userApi.tsx
// User and Profile API Service

import { fetch as tauriFetch } from '@tauri-apps/plugin-http';

// API Configuration - always use full URL for Tauri fetch
const API_CONFIG = {
  BASE_URL: 'https://api.fincept.in',
};

const getApiEndpoint = (path: string) => `${API_CONFIG.BASE_URL}${path}`;

// Use native fetch in dev mode (for Vite proxy), Tauri fetch in production
const safeFetch = import.meta.env.DEV ? window.fetch.bind(window) : tauriFetch;

interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
  status_code: number;
}

const REQUEST_TIMEOUT_MS = 30000;

// Generic API request handler
async function makeApiRequest<T = any>(
  endpoint: string,
  method: 'GET' | 'POST' | 'PUT' | 'DELETE' = 'GET',
  apiKey: string,
  body?: any
): Promise<ApiResponse<T>> {
  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);

    const options: RequestInit = {
      method,
      headers: {
        'Content-Type': 'application/json',
        'X-API-Key': apiKey,
      },
      signal: controller.signal,
    };

    if (body && (method === 'POST' || method === 'PUT')) {
      options.body = JSON.stringify(body);
    }

    let response: Response;
    try {
      response = await safeFetch(getApiEndpoint(endpoint), options);
    } finally {
      clearTimeout(timeoutId);
    }
    const data = await response.json();

    return {
      success: response.ok,
      data: data,
      error: response.ok ? undefined : data.detail || data.error || 'Request failed',
      status_code: response.status,
    };
  } catch (error) {
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error',
      status_code: 500,
    };
  }
}

export class UserApiService {
  // ========================
  // PROFILE MANAGEMENT
  // ========================

  static async getUserProfile(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/profile', 'GET', apiKey);
  }

  static async updateUserProfile(apiKey: string, profileData: any): Promise<ApiResponse> {
    return makeApiRequest('/user/profile', 'PUT', apiKey, profileData);
  }

  static async regenerateApiKey(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/regenerate-api-key', 'POST', apiKey);
  }

  static async getUserUsage(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/usage', 'GET', apiKey);
  }

  static async getUserCredits(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/credits', 'GET', apiKey);
  }

  static async deleteUserAccount(apiKey: string, confirmationData: any): Promise<ApiResponse> {
    return makeApiRequest('/user/account', 'DELETE', apiKey, confirmationData);
  }

  // ========================
  // SUBSCRIPTIONS & DATABASES
  // ========================

  static async subscribeToDatabase(apiKey: string, databaseName: string): Promise<ApiResponse> {
    return makeApiRequest('/user/subscribe', 'POST', apiKey, { database_name: databaseName });
  }

  static async getUserSubscriptions(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/subscriptions', 'GET', apiKey);
  }

  static async getUserTransactions(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/transactions', 'GET', apiKey);
  }

  // ========================
  // PAYMENT MANAGEMENT
  // ========================

  static async getUserSubscription(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/cashfree/subscription', 'GET', apiKey);
  }

  static async getUsageDetails(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/cashfree/usage', 'GET', apiKey);
  }



  static async getPaymentHistory(apiKey: string, page: number = 1, limit: number = 10): Promise<ApiResponse> {
    return makeApiRequest(`/payment/payments?page=${page}&limit=${limit}`, 'GET', apiKey);
  }

  static async getPaymentDetails(apiKey: string, paymentUuid: string): Promise<ApiResponse> {
    return makeApiRequest(`/payment/payments/${paymentUuid}`, 'GET', apiKey);
  }

  // ========================
  // SUPPORT & TICKETS
  // ========================

  static async getUserTickets(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/support/tickets', 'GET', apiKey);
  }

  static async createTicket(apiKey: string, ticketData: {
    subject: string;
    description: string;
    category: string;
    priority?: string;
  }): Promise<ApiResponse> {
    return makeApiRequest('/support/tickets', 'POST', apiKey, ticketData);
  }

  static async getTicketDetails(apiKey: string, ticketId: number): Promise<ApiResponse> {
    return makeApiRequest(`/support/tickets/${ticketId}`, 'GET', apiKey);
  }

  static async updateTicket(apiKey: string, ticketId: number, status: string): Promise<ApiResponse> {
    return makeApiRequest(`/support/tickets/${ticketId}`, 'PUT', apiKey, { status });
  }

  static async addTicketMessage(apiKey: string, ticketId: number, message: string): Promise<ApiResponse> {
    return makeApiRequest(`/support/tickets/${ticketId}/messages`, 'POST', apiKey, { message });
  }

  static async getSupportStats(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/support/stats', 'GET', apiKey);
  }

  // ========================
  // MFA (Multi-Factor Authentication)
  // ========================

  static async enableMFA(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/mfa/enable', 'POST', apiKey);
  }

  static async disableMFA(apiKey: string, password: string): Promise<ApiResponse> {
    return makeApiRequest('/user/mfa/disable', 'POST', apiKey, { password });
  }

  static async verifyMFA(email: string, otp: string): Promise<ApiResponse> {
    // No API key required - called after initial login
    try {
      const response = await fetch(getApiEndpoint('/user/verify-mfa'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ email, otp }),
      });
      const data = await response.json();
      return {
        success: response.ok,
        data,
        error: response.ok ? undefined : data.detail || 'MFA verification failed',
        status_code: response.status,
      };
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Network error',
        status_code: 500,
      };
    }
  }

  // ========================
  // NEWSLETTER
  // ========================

  static async subscribeToNewsletter(email: string, name?: string): Promise<ApiResponse> {
    // No API key required - public endpoint
    try {
      const response = await fetch(getApiEndpoint('/support/newsletter/subscribe'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ email, name, source: 'terminal' }),
      });
      const data = await response.json();
      return {
        success: response.ok,
        data,
        error: response.ok ? undefined : data.detail || 'Subscription failed',
        status_code: response.status,
      };
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Network error',
        status_code: 500,
      };
    }
  }

  static async checkNewsletterStatus(email: string): Promise<ApiResponse> {
    try {
      const response = await fetch(getApiEndpoint(`/support/newsletter/status?email=${encodeURIComponent(email)}`));
      const data = await response.json();
      return {
        success: response.ok,
        data,
        error: response.ok ? undefined : data.detail,
        status_code: response.status,
      };
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Network error',
        status_code: 500,
      };
    }
  }
}
