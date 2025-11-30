// File: src/services/userApi.tsx
// User and Profile API Service

import { fetch } from '@tauri-apps/plugin-http';

// API Configuration
const API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://finceptbackend.share.zrok.io',
};

const getApiEndpoint = (path: string) => `${API_CONFIG.BASE_URL}${path}`;

interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
  status_code: number;
}

// Generic API request handler
async function makeApiRequest<T = any>(
  endpoint: string,
  method: 'GET' | 'POST' | 'PUT' | 'DELETE' = 'GET',
  apiKey: string,
  body?: any
): Promise<ApiResponse<T>> {
  try {
    const options: RequestInit = {
      method,
      headers: {
        'Content-Type': 'application/json',
        'X-API-Key': apiKey,
      },
    };

    if (body && (method === 'POST' || method === 'PUT')) {
      options.body = JSON.stringify(body);
    }

    console.log(`[UserAPI] ${method} ${endpoint}`, body || '');

    const response = await fetch(getApiEndpoint(endpoint), options);
    const data = await response.json();

    console.log(`[UserAPI] Response:`, data);

    return {
      success: response.ok,
      data: data,
      error: response.ok ? undefined : data.detail || data.error || 'Request failed',
      status_code: response.status,
    };
  } catch (error) {
    console.error(`[UserAPI] Error in ${method} ${endpoint}:`, error);
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
    return makeApiRequest('/payment/subscription', 'GET', apiKey);
  }

  static async getUsageDetails(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/payment/usage', 'GET', apiKey);
  }

  static async cancelSubscription(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/payment/cancel', 'POST', apiKey);
  }

  static async reactivateSubscription(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/payment/reactivate', 'POST', apiKey);
  }

  static async getPaymentHistory(apiKey: string, page: number = 1, limit: number = 10): Promise<ApiResponse> {
    return makeApiRequest(`/payment/payments?page=${page}&limit=${limit}`, 'GET', apiKey);
  }

  static async getPaymentDetails(apiKey: string, paymentUuid: string): Promise<ApiResponse> {
    return makeApiRequest(`/payment/payments/${paymentUuid}`, 'GET', apiKey);
  }

  // ========================
  // GUEST USER ENDPOINTS
  // ========================

  static async getGuestStatus(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/guest/status', 'GET', apiKey);
  }

  static async extendGuestSession(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/guest/extend', 'POST', apiKey);
  }

  static async getGuestUsage(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/guest/usage', 'GET', apiKey);
  }

  static async deleteGuestSession(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/guest/session', 'DELETE', apiKey);
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
