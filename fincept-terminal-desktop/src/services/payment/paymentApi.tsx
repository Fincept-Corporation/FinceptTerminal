// File: src/services/paymentApi.ts
// Payment API service for handling all payment-related API calls with in-app payment window support

import { fetch } from '@tauri-apps/plugin-http';

// API Configuration
const PAYMENT_API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://finceptbackend.share.zrok.io',
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
    const response = await makePaymentApiRequest<PlansResponse>('GET', '/payment/plans');

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

  // Get specific plan details
  static async getPlanDetails(planId: string): Promise<PaymentApiResponse<SubscriptionPlan>> {
    return makePaymentApiRequest<SubscriptionPlan>('GET', `/payment/plans/${planId}`);
  }

  // Create checkout session - Updated to use /payment/create-order endpoint
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

    const response = await makePaymentApiRequest<{ data: CheckoutResponse }>('POST', '/payment/create-order', requestBody, headers);

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
    return makePaymentApiRequest<UserSubscriptionResponse>('GET', '/payment/subscription', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Get user's usage details
  static async getUserUsage(apiKey: string): Promise<PaymentApiResponse<any>> {
    return makePaymentApiRequest('GET', '/user/usage', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Cancel subscription - Updated to use new endpoint
  // Cancels subscription in Polar, user keeps access until billing period ends
  // Auto-downgrades to free plan when subscription expires via webhook
  static async cancelSubscription(apiKey: string): Promise<PaymentApiResponse<any>> {
    return makePaymentApiRequest('POST', '/payment/subscription/cancel', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Get payment history (transactions) - Updated to use /payment/history endpoint
  static async getPaymentHistory(
    apiKey: string,
    page: number = 1,
    limit: number = 20
  ): Promise<PaymentApiResponse<PaymentHistoryResponse>> {
    return makePaymentApiRequest<PaymentHistoryResponse>(
      'GET',
      `/payment/history?limit=${limit}`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  // Get specific payment details
  static async getPaymentDetails(
    apiKey: string,
    paymentUuid: string
  ): Promise<PaymentApiResponse<any>> {
    return makePaymentApiRequest(
      'GET',
      `/payment/payments/${paymentUuid}`,
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
      `/payment/order/${transactionId}`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  // Webhook validation (for future use)
  static async validateWebhook(webhookData: any): Promise<PaymentApiResponse<any>> {
    return makePaymentApiRequest('POST', '/payment/webhook', webhookData);
  }

  // Debug function to test API key validity
  static async testApiKey(apiKey: string): Promise<PaymentApiResponse<any>> {
    return await makePaymentApiRequest('GET', '/payment/subscription', undefined, {
      'X-API-Key': apiKey
    });
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

  // Calculate annual savings
  static calculateAnnualSavings(monthlyPrice: number): number {
    const annualPrice = monthlyPrice * 10; // 2 months free
    const fullYearPrice = monthlyPrice * 12;
    return fullYearPrice - annualPrice;
  }

  // Get plan recommendation based on usage
  static getRecommendedPlan(apiCallsPerMonth: number): string {
    if (apiCallsPerMonth <= 10000) return 'starter_20';
    if (apiCallsPerMonth <= 50000) return 'professional_49';
    if (apiCallsPerMonth <= 200000) return 'enterprise_99';
    return 'unlimited_199';
  }

  // Validate plan selection
  static validatePlanSelection(planId: string, availablePlans: SubscriptionPlan[]): boolean {
    return availablePlans.some(plan => plan.plan_id === planId);
  }

  // Parse URL parameters for payment success page
  static parsePaymentParams(url: string): { sessionId?: string; paymentId?: string } {
    const urlParams = new URLSearchParams(url.split('?')[1]);
    return {
      sessionId: urlParams.get('session_id') || undefined,
      paymentId: urlParams.get('payment_id') || undefined
    };
  }

  // Check if subscription is expiring soon (within 7 days)
  static isSubscriptionExpiringSoon(endDate: string): boolean {
    const end = new Date(endDate);
    const now = new Date();
    const daysUntilExpiry = Math.ceil((end.getTime() - now.getTime()) / (1000 * 60 * 60 * 24));
    return daysUntilExpiry <= 7 && daysUntilExpiry > 0;
  }

  // Get usage percentage with color coding
  static getUsageStatus(used: number, limit: number | string): {
    percentage: number;
    status: 'low' | 'medium' | 'high' | 'critical';
    color: string;
  } {
    if (limit === 'unlimited' || limit === -1) {
      return { percentage: 0, status: 'low', color: 'green' };
    }

    const limitNum = typeof limit === 'string' ? parseInt(limit) : limit;
    const percentage = Math.min((used / limitNum) * 100, 100);

    let status: 'low' | 'medium' | 'high' | 'critical';
    let color: string;

    if (percentage >= 95) {
      status = 'critical';
      color = 'red';
    } else if (percentage >= 80) {
      status = 'high';
      color = 'orange';
    } else if (percentage >= 50) {
      status = 'medium';
      color = 'yellow';
    } else {
      status = 'low';
      color = 'green';
    }

    return { percentage: Math.round(percentage), status, color };
  }

  // Validate payment URL before opening
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

  // Open payment URL in external browser (Tauri shell)
  static async openPaymentUrl(
    checkoutUrl: string,
    planName: string = 'Subscription',
    planPrice: number = 0
  ): Promise<boolean> {
    if (!this.validatePaymentUrl(checkoutUrl)) {
      console.error('Invalid checkout URL provided');
      alert('Payment URL is invalid. Please try again or contact support.');
      return false;
    }

    try {
      // Always use external browser for Tauri
      return this.openPaymentUrlExternal(checkoutUrl);
    } catch (error) {
      console.error('Failed to open payment window:', error);
      return false;
    }
  }

  // Enhanced payment URL opener with validation
  static async openValidatedPaymentUrl(
    checkoutUrl: string,
    planName: string = 'Subscription',
    planPrice: number = 0
  ): Promise<boolean> {
    if (!this.validatePaymentUrl(checkoutUrl)) {
      console.error('Invalid or suspicious payment URL');
      alert('Invalid payment URL. Please contact support.');
      return false;
    }

    return this.openPaymentUrl(checkoutUrl, planName, planPrice);
  }

  // Fallback method: Open external payment URL
  private static openPaymentUrlExternal(checkoutUrl: string): boolean {
    try {
      const newWindow = window.open(checkoutUrl, '_blank', 'noopener,noreferrer');

      if (!newWindow) {

        // Try to copy URL to clipboard as fallback
        if (navigator.clipboard && checkoutUrl !== 'undefined') {
          navigator.clipboard.writeText(checkoutUrl).then(() => {
            alert(`Popup blocked. Payment URL copied to clipboard: ${checkoutUrl}`);
          }).catch(() => {
            alert(`Please visit this URL to complete payment: ${checkoutUrl}`);
          });
        } else {
          alert(`Please visit this URL to complete payment: ${checkoutUrl}`);
        }

        return false;
      }

      return true;

    } catch (error) {
      console.error('PaymentUtils: Failed to open external payment URL:', error);
      alert(`Please visit this URL to complete payment: ${checkoutUrl}`);
      return false;
    }
  }

  // Generate receipt data for successful payment
  static generateReceiptData(paymentData: any): {
    receiptId: string;
    date: string;
    description: string;
    amount: string;
    status: string;
  } {
    return {
      receiptId: paymentData.payment_uuid || 'N/A',
      date: new Date().toLocaleDateString(),
      description: `Fincept API Subscription - ${paymentData.plan?.name || 'Plan'}`,
      amount: this.formatCurrency(paymentData.plan?.price || 0),
      status: paymentData.status || 'Completed'
    };
  }
}

export default PaymentApiService;