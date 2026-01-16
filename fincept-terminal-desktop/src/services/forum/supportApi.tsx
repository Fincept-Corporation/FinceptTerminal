// File: src/services/supportApi.tsx
// Support Ticket API Service

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
  apiKey?: string,
  body?: any
): Promise<ApiResponse<T>> {
  try {
    const options: RequestInit = {
      method,
      headers: {
        'Content-Type': 'application/json',
        ...(apiKey && { 'X-API-Key': apiKey }),
      },
    };

    if (body && (method === 'POST' || method === 'PUT')) {
      options.body = JSON.stringify(body);
    }

    console.log(`[SupportAPI] ${method} ${endpoint}`, body || '');

    const response = await fetch(getApiEndpoint(endpoint), options);
    const data = await response.json();

    console.log(`[SupportAPI] Response:`, data);

    return {
      success: response.ok,
      data: data,
      error: response.ok ? undefined : data.detail || data.error || 'Request failed',
      status_code: response.status,
    };
  } catch (error) {
    console.error(`[SupportAPI] Error in ${method} ${endpoint}:`, error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error',
      status_code: 500,
    };
  }
}

export class SupportApiService {
  // ========================
  // SUPPORT TICKETS
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
  // CONTACT & FEEDBACK (Public)
  // ========================

  static async submitContactForm(formData: {
    name: string;
    email: string;
    subject: string;
    message: string;
  }): Promise<ApiResponse> {
    return makeApiRequest('/support/contact', 'POST', undefined, formData);
  }

  static async submitFeedback(feedbackData: {
    name?: string;
    email?: string;
    rating: number;
    feedback_text: string;
    category?: string;
  }): Promise<ApiResponse> {
    return makeApiRequest('/support/feedback', 'POST', undefined, feedbackData);
  }

  static async getSupportCategories(): Promise<ApiResponse> {
    return makeApiRequest('/support/categories', 'GET');
  }

  // ========================
  // NEWSLETTER (Public)
  // ========================

  static async subscribeToNewsletter(email: string, name?: string): Promise<ApiResponse> {
    return makeApiRequest('/support/newsletter/subscribe', 'POST', undefined, {
      email,
      name,
      source: 'terminal'
    });
  }

  static async unsubscribeFromNewsletter(email: string, token?: string): Promise<ApiResponse> {
    const params = new URLSearchParams({ email });
    if (token) params.append('token', token);
    return makeApiRequest(`/support/newsletter/unsubscribe?${params.toString()}`, 'POST');
  }

  static async checkNewsletterStatus(email: string): Promise<ApiResponse> {
    return makeApiRequest(`/support/newsletter/status?email=${encodeURIComponent(email)}`, 'GET');
  }
}
