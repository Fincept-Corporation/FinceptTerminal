// File: src/services/userApi.tsx
// User and Profile API Service

import { fetch as tauriFetch } from '@tauri-apps/plugin-http';
import { getSessionToken } from '@/services/auth/authApi';

// API Configuration
const API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://api.fincept.in',
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

    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
      'X-API-Key': apiKey,
    };
    const sessionToken = getSessionToken();
    if (sessionToken) {
      headers['X-Session-Token'] = sessionToken;
    }

    const options: RequestInit = {
      method,
      headers,
      signal: controller.signal,
    };

    if (body && (method === 'POST' || method === 'PUT')) {
      options.body = JSON.stringify(body);
    }

    const url = getApiEndpoint(endpoint);
    console.log(`[UserAPI] ${method} ${url}`);

    let response: Response;
    try {
      response = await safeFetch(url, options);
    } finally {
      clearTimeout(timeoutId);
    }
    const data = await response.json();

    console.log(`[UserAPI] ${method} ${endpoint} → ${response.status}`, data?.success);

    return {
      success: response.ok,
      data: data,
      error: response.ok ? undefined : data.detail || data.message || data.error || 'Request failed',
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

  static async updateUserProfile(apiKey: string, profileData: {
    username?: string;
    phone?: string | null;
    country?: string | null;
    country_code?: string | null;
  }): Promise<ApiResponse> {
    return makeApiRequest('/user/profile', 'PUT', apiKey, profileData);
  }

  static async regenerateApiKey(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/regenerate-api-key', 'POST', apiKey);
  }

  static async getUserUsage(apiKey: string, options?: {
    days?: number;
    page?: number;
    page_size?: number;
    endpoint_filter?: string;
    method_filter?: string;
  }): Promise<ApiResponse> {
    const params = new URLSearchParams();
    if (options?.days) params.append('days', String(options.days));
    if (options?.page) params.append('page', String(options.page));
    if (options?.page_size) params.append('page_size', String(options.page_size));
    if (options?.endpoint_filter) params.append('endpoint_filter', options.endpoint_filter);
    if (options?.method_filter) params.append('method_filter', options.method_filter);
    const query = params.toString();
    return makeApiRequest(`/user/usage${query ? `?${query}` : ''}`, 'GET', apiKey);
  }

  static async getUserCredits(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/credits', 'GET', apiKey);
  }

  static async deleteUserAccount(apiKey: string, confirmationData: any): Promise<ApiResponse> {
    return makeApiRequest('/user/account', 'DELETE', apiKey, confirmationData);
  }

  // ========================
  // SESSION MANAGEMENT
  // ========================

  static async logout(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/logout', 'POST', apiKey);
  }

  static async validateSession(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/validate-session', 'POST', apiKey);
  }

  // ========================
  // LOGIN HISTORY
  // ========================

  static async getLoginHistory(apiKey: string, limit: number = 20, offset: number = 0): Promise<ApiResponse> {
    return makeApiRequest(`/user/login-history?limit=${limit}&offset=${offset}`, 'GET', apiKey);
  }

  // ========================
  // NOTIFICATIONS
  // ========================

  static async getNotifications(apiKey: string, limit: number = 20, offset: number = 0, unreadOnly: boolean = false): Promise<ApiResponse> {
    return makeApiRequest(`/user/notifications?limit=${limit}&offset=${offset}&unread_only=${unreadOnly}`, 'GET', apiKey);
  }

  static async markNotificationRead(apiKey: string, notificationId: number): Promise<ApiResponse> {
    return makeApiRequest(`/user/notifications/${notificationId}/read`, 'PUT', apiKey);
  }

  static async markAllNotificationsRead(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/notifications/read-all', 'PUT', apiKey);
  }

  static async deleteNotification(apiKey: string, notificationId: number): Promise<ApiResponse> {
    return makeApiRequest(`/user/notifications/${notificationId}`, 'DELETE', apiKey);
  }

  static async getNotificationPreferences(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/notifications/preferences', 'GET', apiKey);
  }

  static async updateNotificationPreferences(apiKey: string, preferences: Record<string, any>): Promise<ApiResponse> {
    return makeApiRequest('/user/notifications/preferences', 'PUT', apiKey, preferences);
  }

  static async testTelegramNotification(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/notifications/telegram/test', 'POST', apiKey);
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

  // ========================
  // ADMIN USER MANAGEMENT
  // ========================

  static async adminListUsers(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/admin/users', 'GET', apiKey);
  }

  static async adminGetUserDetails(apiKey: string, userId: number): Promise<ApiResponse> {
    return makeApiRequest(`/user/admin/user/${userId}`, 'GET', apiKey);
  }

  static async adminManageUserCredits(apiKey: string, userId: number, creditData: {
    amount: number;
    reason?: string;
  }): Promise<ApiResponse> {
    return makeApiRequest(`/user/admin/user/${userId}/credits`, 'POST', apiKey, creditData);
  }

  static async adminGetStats(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('/user/admin/stats', 'GET', apiKey);
  }

  static async adminBroadcastNotification(apiKey: string, notification: {
    title: string;
    message: string;
    type?: string;
  }): Promise<ApiResponse> {
    return makeApiRequest('/user/admin/notifications/broadcast', 'POST', apiKey, notification);
  }
}
