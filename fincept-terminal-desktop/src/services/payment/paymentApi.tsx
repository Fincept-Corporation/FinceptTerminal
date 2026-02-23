// File: src/services/paymentApi.ts
// Payment API service for handling all payment-related API calls with in-app payment window support

import { fetch } from '@tauri-apps/plugin-http';

// API Configuration
const PAYMENT_API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://api.fincept.in',
  CONNECTION_TIMEOUT: 10000,
  REQUEST_TIMEOUT: 30000
};

const getPaymentApiEndpoint = (path: string) => `${PAYMENT_API_CONFIG.BASE_URL}${path}`;

// API Response Types
interface PaymentApiResponse<T = any> {
  success: boolean;
  data?: T;
  message?: string;
  error?: string;
  status_code?: number;
}

// Request Types
interface CheckoutRequest {
  plan_id: string; // Valid values: free, basic, standard, pro, enterprise
  currency: string; // USD, EUR, INR, etc.
}

// Response Types - Matches backend exactly
interface SubscriptionPlan {
  plan_id: string;
  name: string;
  description: string;
  price_usd: number;
  currency: string;
  credits: number;
  support_type: string; // "community" | "fincept"
  validity_days: number;
  features: string[];
  is_free: boolean;
  display_order: number;
  // Computed fields for UI display (added by frontend)
  price_display?: string;
  credits_amount?: number;
  support_display?: string;
  validity_display?: string;
  is_popular?: boolean;
}

interface PlansResponse {
  success: boolean;
  message: string;
  data: SubscriptionPlan[];
}

interface CheckoutResponse {
  order_id: string;
  cf_order_id: string;
  payment_session_id: string;
  order_status: string;
  order_amount: number;
  order_currency: string;
  payment_uuid: string;
  plan_name: string;
  environment: string;
  checkout_url?: string;
}

interface UserSubscriptionResponse {
  has_subscription: boolean;
  subscription?: {
    subscription_uuid: string;
    plan: {
      name: string;
      plan_id: string;
      price: number;
    };
    status: string;
    current_period_start: string;
    current_period_end: string;
    days_remaining: number;
    cancel_at_period_end: boolean;
    usage: {
      api_calls_used: number;
      api_calls_limit: number | string;
      usage_percentage: number;
    };
  };
}

interface PaymentSuccessResponse {
  message: string;
  payment_uuid: string;
  status: string;
  amount: number;
}

interface PaymentHistoryItem {
  payment_uuid: string;
  amount: number;
  currency: string;
  status: string;
  payment_method: string;
  created_at: string;
  completed_at: string | null;
  subscription_plan: string;
}

interface PaymentHistoryResponse {
  payments: PaymentHistoryItem[];
  pagination: {
    current_page: number;
    per_page: number;
    total_payments: number;
    total_pages: number;
  };
}

// Generic Payment API request function
const makePaymentApiRequest = async <T = any>(
  method: string,
  endpoint: string,
  data?: any,
  headers?: Record<string, string>
): Promise<PaymentApiResponse<T>> => {
  try {
    const defaultHeaders = {
      'Content-Type': 'application/json',
      ...headers
    };

    const url = getPaymentApiEndpoint(endpoint);

    const config: RequestInit = {
      method,
      headers: defaultHeaders,
      mode: 'cors',
    };

    if (data && (method === 'POST' || method === 'PUT')) {
      config.body = JSON.stringify(data);
    }

    const response = await fetch(url, config);

    const responseText = await response.text();

    let responseData;
    try {
      responseData = JSON.parse(responseText);
    } catch (parseError) {
      console.error('Payment JSON parse failed:', parseError);
      return {
        success: false,
        error: `Invalid JSON response: ${responseText.substring(0, 100)}...`,
        status_code: response.status
      };
    }

    // Response handling

    return {
      success: response.ok,
      data: responseData,
      status_code: response.status,
      error: response.ok ? undefined : responseData.message || `HTTP ${response.status}`
    };

  } catch (error) {
    console.error('Payment request error:', error);

    if (error instanceof TypeError && error.message.includes('Failed to fetch')) {
      return {
        success: false,
        error: 'Network error: Unable to connect to payment server. Please check your connection.'
      };
    }

    return {
      success: false,
      error: error instanceof Error ? error.message : 'Payment network error'
    };
  }
};

// Payment API Service Class with enhanced debugging
export class PaymentApiService {
  // Get all subscription plans from API
  static async getSubscriptionPlans(): Promise<PaymentApiResponse<SubscriptionPlan[]>> {
    const response = await makePaymentApiRequest<PlansResponse>('GET', '/cashfree/plans');

    // Backend returns: { success: true, message: "Success", data: [plans array] }
    if (response.success && response.data) {
      const plansResponse = response.data;

      // Extract plans array from the data field
      const plans = plansResponse.data || [];

      // Add computed display fields to each plan
      const enrichedPlans = plans.map((plan) => ({
        ...plan,
        // Add computed display fields for UI
        price_display: plan.price_usd === 0 ? 'Free' : `$${plan.price_usd}/mo`,
        credits_amount: plan.credits,
        support_display: plan.support_type === 'community' ? 'Community Support' : 'Fincept Support',
        validity_display: plan.validity_days === 0 ? 'Forever' : `${plan.validity_days} days`,
        is_popular: plan.plan_id === 'pro', // Mark Pro ($50) as popular
      }));

      return {
        success: true,
        data: enrichedPlans,
        status_code: response.status_code
      };
    }

    return {
      success: false,
      error: response.error || 'Failed to load plans',
      status_code: response.status_code
    };
  }

  // Create checkout session - Updated to use /cashfree/create-order endpoint
  static async createCheckoutSession(
    apiKey: string,
    request: CheckoutRequest
  ): Promise<PaymentApiResponse<CheckoutResponse>> {
    if (!apiKey) {
      return {
        success: false,
        error: 'No API key provided'
      };
    }

    if (typeof apiKey !== 'string') {
      return {
        success: false,
        error: 'Invalid API key format'
      };
    }

    const headers = {
      'X-API-Key': apiKey
    };

    const requestBody = {
      plan_id: request.plan_id,
      currency: request.currency || 'USD'
    };

    const response = await makePaymentApiRequest<{ data: CheckoutResponse }>('POST', '/cashfree/create-order', requestBody, headers);

    // Construct checkout URL from payment_session_id
    if (response.success && response.data?.data?.payment_session_id) {
      const checkoutData = response.data.data;
      checkoutData.checkout_url = `https://fincept.in/checkout.html?session=${checkoutData.payment_session_id}`;
      return {
        success: true,
        data: checkoutData,
        status_code: response.status_code
      };
    }

    return {
      success: false,
      error: response.error || 'Failed to create checkout session',
      status_code: response.status_code
    };
  }

  // Handle payment success
  static async handlePaymentSuccess(
    apiKey: string,
    sessionId?: string,
    paymentId?: string
  ): Promise<PaymentApiResponse<PaymentSuccessResponse>> {
    const params = new URLSearchParams();
    if (sessionId) params.append('session_id', sessionId);
    if (paymentId) params.append('payment_id', paymentId);

    const queryString = params.toString();
    const endpoint = `/payment/success${queryString ? `?${queryString}` : ''}`;

    return makePaymentApiRequest<PaymentSuccessResponse>('GET', endpoint, undefined, {
      'X-API-Key': apiKey
    });
  }

  // Get user's current subscription - Updated to use new endpoint
  static async getUserSubscription(apiKey: string): Promise<PaymentApiResponse<UserSubscriptionResponse>> {
    return makePaymentApiRequest<UserSubscriptionResponse>('GET', '/cashfree/subscription', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Get user's usage details
  static async getUserUsage(apiKey: string): Promise<PaymentApiResponse<any>> {
    return makePaymentApiRequest('GET', '/cashfree/usage', undefined, {
      'X-API-Key': apiKey
    });
  }


  // Get payment history (transactions) - Updated to use /cashfree/payments endpoint
  static async getPaymentHistory(
    apiKey: string,
    page: number = 1,
    limit: number = 20
  ): Promise<PaymentApiResponse<PaymentHistoryResponse>> {
    return makePaymentApiRequest<PaymentHistoryResponse>(
      'GET',
      `/cashfree/payments?limit=${limit}`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  // Get transaction status - New endpoint
  static async getTransactionStatus(
    apiKey: string,
    transactionId: string
  ): Promise<PaymentApiResponse<any>> {
    return makePaymentApiRequest(
      'GET',
      `/cashfree/order/${transactionId}`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

}

// Updated PaymentUtils class with in-app window support
export class PaymentUtils {
  // Format currency
  static formatCurrency(amount: number, currency: string = 'USD'): string {
    return new Intl.NumberFormat('en-US', {
      style: 'currency',
      currency: currency,
      minimumFractionDigits: 2,
      maximumFractionDigits: 2
    }).format(amount);
  }

  // Validate payment URL before opening
static validatePaymentUrl(url: string): boolean {
  if (!url || url === 'undefined' || url === 'null') {
    return false;
  }

  try {
    const urlObj = new URL(url);

    // Check for valid protocols
    if (!['http:', 'https:'].includes(urlObj.protocol)) {
      return false;
    }

    // Updated: Remove overly restrictive validation
    // Only block obviously dangerous patterns
    const dangerousPatterns = [
      'javascript:',
      'data:',
      'file:',
    ];

    for (const pattern of dangerousPatterns) {
      if (url.toLowerCase().includes(pattern)) {
        console.warn('PaymentUtils: Dangerous URL pattern detected:', pattern);
        return false;
      }
    }

    // Allow all HTTPS URLs from legitimate payment providers
    if (urlObj.protocol === 'https:') {
      return true;
    }

    return true;
  } catch (error) {
    console.error('PaymentUtils: Invalid URL format:', error);
    return false;
  }
}

}

export default PaymentApiService;