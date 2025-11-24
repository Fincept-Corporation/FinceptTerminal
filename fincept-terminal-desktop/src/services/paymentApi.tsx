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
  plan_id: string;
  return_url?: string;
}

// Response Types
interface SubscriptionPlan {
  plan_id: string;
  name: string;
  description: string;
  price_usd: number;
  billing_interval: string;
  api_calls_limit: number | string;
  databases_access: string[];
  priority_support: boolean;
  features: {
    api_calls: string;
    databases: string;
    support: string;
  };
}

interface PlansResponse {
  plans: SubscriptionPlan[];
  total_plans: number;
}

interface CheckoutResponse {
  checkout_url: string;
  session_id: string;
  payment_uuid: string;
  plan: {
    name: string;
    price: number;
  };
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

// Payment Window Manager Interface
interface PaymentWindowManager {
  openPaymentWindow: (url: string, planName: string, planPrice: number) => Promise<boolean>;
}

// Global payment window manager - will be set by the main app
let paymentWindowManager: PaymentWindowManager | null = null;

export const setPaymentWindowManager = (manager: PaymentWindowManager) => {
  paymentWindowManager = manager;
};

// Generic Payment API request function with enhanced debugging
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

    // Enhanced debug logging
    console.log('💳 Making payment request:', {
      method,
      url,
      endpoint,
      data,
      timestamp: new Date().toISOString()
    });

    // Debug headers (with API key redacted)
    const debugHeaders: any = { ...defaultHeaders };
    if (debugHeaders['X-API-Key']) {
      debugHeaders['X-API-Key'] = '[REDACTED - Length: ' + debugHeaders['X-API-Key'].length + ']';
    }
    console.log('💳 Request headers:', debugHeaders);

    // Check if API key is actually present
    const hasApiKey = !!headers?.['X-API-Key'];
    const apiKeyLength = headers?.['X-API-Key']?.length || 0;
    console.log('💳 API Key debug:', {
      hasApiKey,
      apiKeyLength,
      headerKeys: Object.keys(defaultHeaders),
      originalHeaderKeys: headers ? Object.keys(headers) : 'none'
    });

    const config: RequestInit = {
      method,
      headers: defaultHeaders,
      mode: 'cors',
    };

    if (data && (method === 'POST' || method === 'PUT')) {
      config.body = JSON.stringify(data);
      console.log('💳 Request body:', JSON.stringify(data, null, 2));
    }

    console.log('💳 Final fetch config:', {
      url,
      method: config.method,
      mode: config.mode,
      hasBody: !!config.body,
      bodyLength: config.body ? (config.body as string).length : 0
    });

    const response = await fetch(url, config);

    console.log('💰 Payment response received:', {
      status: response.status,
      statusText: response.statusText,
      url: response.url,
      ok: response.ok,
      headers: Object.fromEntries(response.headers.entries())
    });

    const responseText = await response.text();
    console.log('📄 Payment response text (first 500 chars):', responseText.substring(0, 500));
    console.log('📄 Payment response full length:', responseText.length);

    let responseData;
    try {
      responseData = JSON.parse(responseText);
      console.log('✅ Payment JSON parsed successfully:', {
        success: responseData.success,
        hasData: !!responseData.data,
        message: responseData.message,
        error: responseData.error,
        errorCode: responseData.error_code
      });
    } catch (parseError) {
      console.error('❌ Payment JSON parse failed:', parseError);
      console.log('📄 Raw response that failed to parse:', responseText);
      return {
        success: false,
        error: `Invalid JSON response: ${responseText.substring(0, 100)}...`,
        status_code: response.status
      };
    }

    // Enhanced response logging for failures
    if (!response.ok) {
      console.error('❌ Payment request failed:', {
        status: response.status,
        statusText: response.statusText,
        responseData,
        requestHeaders: debugHeaders,
        requestUrl: url,
        requestMethod: method
      });
    }

    return {
      success: response.ok,
      data: responseData,
      status_code: response.status,
      error: response.ok ? undefined : responseData.message || `HTTP ${response.status}`
    };

  } catch (error) {
    console.error('💥 Payment request failed completely:', error);
    console.error('💥 Error details:', {
      name: error instanceof Error ? error.name : 'Unknown',
      message: error instanceof Error ? error.message : 'Unknown error',
      stack: error instanceof Error ? error.stack : 'No stack trace'
    });

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
  // Get all subscription plans
  static async getSubscriptionPlans(): Promise<PaymentApiResponse<PlansResponse>> {
    console.log('📋 PaymentApiService: Starting getSubscriptionPlans request');

    const result = await makePaymentApiRequest<PlansResponse>('GET', '/payment/plans');

    console.log('📋 PaymentApiService: getSubscriptionPlans result:', {
      success: result.success,
      hasData: !!result.data,
      statusCode: result.status_code,
      error: result.error
    });

    if (result.success && result.data) {
      console.log('📋 PaymentApiService: Raw API response structure:', {
        dataKeys: Object.keys(result.data),
        hasPlans: !!(result.data as any).plans,
        hasDataProperty: !!(result.data as any).data,
        plansCount: (result.data as any).plans?.length || 'no plans array'
      });

      // Check if the data has the expected structure
      if ((result.data as any).plans) {
        console.log('📋 PaymentApiService: Found plans directly in result.data');
        console.log('📋 PaymentApiService: Number of plans:', (result.data as any).plans.length);
      } else if ((result.data as any).data && (result.data as any).data.plans) {
        console.log('📋 PaymentApiService: Found plans in result.data.data');
        console.log('📋 PaymentApiService: Number of plans:', (result.data as any).data.plans.length);
      } else {
        console.log('📋 PaymentApiService: Plans not found in expected location');
        console.log('📋 PaymentApiService: Available properties:', Object.keys(result.data));
      }
    }

    return result;
  }

  // Get specific plan details
  static async getPlanDetails(planId: string): Promise<PaymentApiResponse<SubscriptionPlan>> {
    console.log('📋 PaymentApiService: Getting plan details for:', planId);
    return makePaymentApiRequest<SubscriptionPlan>('GET', `/payment/plans/${planId}`);
  }

  // Create checkout session with enhanced debugging
  static async createCheckoutSession(
    apiKey: string,
    request: CheckoutRequest
  ): Promise<PaymentApiResponse<CheckoutResponse>> {
    console.log('🔑 PaymentApiService: createCheckoutSession called');
    console.log('🔑 PaymentApiService: API Key debug:', {
      hasApiKey: !!apiKey,
      apiKeyType: typeof apiKey,
      apiKeyLength: apiKey?.length || 0,
      apiKeyPrefix: apiKey ? apiKey.substring(0, 10) + '...' : 'null',
      apiKeyValid: apiKey && apiKey.length > 10
    });
    console.log('🔑 PaymentApiService: Request data:', request);

    // Verify API key format
    if (!apiKey) {
      console.error('🔑 PaymentApiService: No API key provided!');
      return {
        success: false,
        error: 'No API key provided'
      };
    }

    if (typeof apiKey !== 'string') {
      console.error('🔑 PaymentApiService: API key is not a string:', typeof apiKey);
      return {
        success: false,
        error: 'Invalid API key format'
      };
    }

    // Test different header variations to debug the issue
    const headers = {
      'X-API-Key': apiKey
    };

    console.log('🔑 PaymentApiService: Headers being passed:', {
      'X-API-Key': '[REDACTED - Length: ' + apiKey.length + ']',
      headerCount: Object.keys(headers).length
    });

    const result = await makePaymentApiRequest<CheckoutResponse>('POST', '/payment/checkout', request, headers);

    console.log('🔑 PaymentApiService: createCheckoutSession result:', {
      success: result.success,
      statusCode: result.status_code,
      error: result.error,
      hasData: !!result.data
    });

    return result;
  }

  // Handle payment success
  static async handlePaymentSuccess(
    apiKey: string,
    sessionId?: string,
    paymentId?: string
  ): Promise<PaymentApiResponse<PaymentSuccessResponse>> {
    console.log('✅ PaymentApiService: handlePaymentSuccess called:', {
      hasApiKey: !!apiKey,
      sessionId,
      paymentId
    });

    const params = new URLSearchParams();
    if (sessionId) params.append('session_id', sessionId);
    if (paymentId) params.append('payment_id', paymentId);

    const queryString = params.toString();
    const endpoint = `/payment/success${queryString ? `?${queryString}` : ''}`;

    return makePaymentApiRequest<PaymentSuccessResponse>('GET', endpoint, undefined, {
      'X-API-Key': apiKey
    });
  }

  // Get user's current subscription
  static async getUserSubscription(apiKey: string): Promise<PaymentApiResponse<UserSubscriptionResponse>> {
    console.log('👤 PaymentApiService: getUserSubscription called:', {
      hasApiKey: !!apiKey,
      apiKeyLength: apiKey?.length || 0
    });

    return makePaymentApiRequest<UserSubscriptionResponse>('GET', '/payment/subscription', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Get user's usage details
  static async getUserUsage(apiKey: string): Promise<PaymentApiResponse<any>> {
    console.log('📊 PaymentApiService: getUserUsage called');
    return makePaymentApiRequest('GET', '/payment/usage', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Cancel subscription
  static async cancelSubscription(apiKey: string): Promise<PaymentApiResponse<any>> {
    console.log('❌ PaymentApiService: cancelSubscription called');
    return makePaymentApiRequest('POST', '/payment/cancel', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Reactivate subscription
  static async reactivateSubscription(apiKey: string): Promise<PaymentApiResponse<any>> {
    console.log('🔄 PaymentApiService: reactivateSubscription called');
    return makePaymentApiRequest('POST', '/payment/reactivate', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Get payment history
  static async getPaymentHistory(
    apiKey: string,
    page: number = 1,
    limit: number = 10
  ): Promise<PaymentApiResponse<PaymentHistoryResponse>> {
    console.log('📜 PaymentApiService: getPaymentHistory called:', { page, limit });
    return makePaymentApiRequest<PaymentHistoryResponse>(
      'GET',
      `/payment/payments?page=${page}&limit=${limit}`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  // Get specific payment details
  static async getPaymentDetails(
    apiKey: string,
    paymentUuid: string
  ): Promise<PaymentApiResponse<any>> {
    console.log('💳 PaymentApiService: getPaymentDetails called:', paymentUuid);
    return makePaymentApiRequest(
      'GET',
      `/payment/payments/${paymentUuid}`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  // Webhook validation (for future use)
  static async validateWebhook(webhookData: any): Promise<PaymentApiResponse<any>> {
    console.log('🔗 PaymentApiService: validateWebhook called');
    return makePaymentApiRequest('POST', '/payment/webhook', webhookData);
  }

  // Debug function to test API key validity
  static async testApiKey(apiKey: string): Promise<PaymentApiResponse<any>> {
    console.log('🧪 PaymentApiService: Testing API key validity');
    console.log('🧪 API Key test details:', {
      hasApiKey: !!apiKey,
      apiKeyLength: apiKey?.length || 0,
      apiKeyPrefix: apiKey ? apiKey.substring(0, 15) + '...' : 'null'
    });

    // Try a simple GET request first
    const testResult = await makePaymentApiRequest('GET', '/payment/subscription', undefined, {
      'X-API-Key': apiKey
    });

    console.log('🧪 API Key test result:', {
      success: testResult.success,
      statusCode: testResult.status_code,
      error: testResult.error
    });

    return testResult;
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

  // Updated: Open payment URL using in-app window
  static async openPaymentUrl(
    checkoutUrl: string,
    planName: string = 'Subscription',
    planPrice: number = 0
  ): Promise<boolean> {
    console.log('PaymentUtils: Opening payment URL:', checkoutUrl);

    // Validate the checkout URL
    if (!this.validatePaymentUrl(checkoutUrl)) {
      console.error('PaymentUtils: Invalid checkout URL provided');
      alert('Payment URL is invalid. Please try again or contact support.');
      return false;
    }

    try {
      // Try to use in-app payment window first
      if (paymentWindowManager) {
        console.log('PaymentUtils: Using in-app payment window');
        return await paymentWindowManager.openPaymentWindow(checkoutUrl, planName, planPrice);
      }

      // Fallback to external browser if in-app window is not available
      console.log('PaymentUtils: Falling back to external browser');
      return this.openPaymentUrlExternal(checkoutUrl);

    } catch (error) {
      console.error('PaymentUtils: Failed to open payment window:', error);

      // Final fallback to external browser
      return this.openPaymentUrlExternal(checkoutUrl);
    }
  }

  // Enhanced payment URL opener with validation
  static async openValidatedPaymentUrl(
    checkoutUrl: string,
    planName: string = 'Subscription',
    planPrice: number = 0
  ): Promise<boolean> {
    // Validate URL first
    if (!this.validatePaymentUrl(checkoutUrl)) {
      console.error('PaymentUtils: Invalid or suspicious payment URL');
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
        console.warn('PaymentUtils: Popup blocked, trying clipboard fallback');

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