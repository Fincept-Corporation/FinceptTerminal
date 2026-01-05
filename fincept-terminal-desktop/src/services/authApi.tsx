// File: src/services/authApi.ts
// Authentication API service for handling all auth-related API calls

import { fetch } from '@tauri-apps/plugin-http';

// API Configuration
const API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://finceptbackend.share.zrok.io',
  API_VERSION: 'v1',
  CONNECTION_TIMEOUT: 10000,
  REQUEST_TIMEOUT: 30000
};

const getApiEndpoint = (path: string) => `${API_CONFIG.BASE_URL}${path}`;

// API Response Types
interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  message?: string;
  error?: string;
  status_code?: number;
}

// Request Types
interface LoginRequest {
  email: string;
  password: string;
}

interface RegisterRequest {
  username: string;
  email: string;
  password: string;
  phone?: string;
  country_code?: string;
  country?: string;
}

interface VerifyOtpRequest {
  email: string;
  otp: string;
}

interface ForgotPasswordRequest {
  email: string;
}

interface ResetPasswordRequest {
  email: string;
  otp: string;
  new_password: string;
}

interface DeviceRegisterRequest {
  device_id: string;
  device_name: string;
  platform: string;
  hardware_info: {
    platform: string;
    user_agent: string;
    language: string;
    screen_resolution?: string;
    cpu_cores?: number;
    memory_gb?: number;
  };
}

// Response Types
interface LoginResponse {
  api_key: string;
  username?: string;
  email?: string;
  credit_balance?: number;
  user?: {
    id: number;
    username: string;
    email: string;
    account_type: string;
    credit_balance: number;
  };
}

interface UserProfileResponse {
  id: number;
  username: string;
  email: string;
  account_type: string;
  credit_balance: number;
  is_verified: boolean;
  is_admin: boolean;
  mfa_enabled: boolean;
  created_at: string;
  last_login_at: string;
}

interface AuthStatusResponse {
  authenticated: boolean;
  user_type: 'guest' | 'registered';
  user?: {
    id: number;
    username: string;
    email: string;
    account_type: string;
    credit_balance: number;
    created_at: string;
  };
  guest?: {
    id: number;
    device_id: string;
    platform: string;
    requests_today: number;
    expires_at: string;
    daily_limit?: number;
  };
}

interface DeviceRegisterResponse {
  api_key?: string;
  temp_api_key?: string;
  guest_id?: number;
  device_id?: string;
  expires_at?: string;
  daily_limit?: number;
  requests_today?: number;
  credit_balance?: number;
}

// Generic API request function
const makeApiRequest = async <T = any>(
  method: string,
  endpoint: string,
  data?: any,
  headers?: Record<string, string>
): Promise<ApiResponse<T>> => {
  try {
    const defaultHeaders = {
      'Content-Type': 'application/json',
      ...headers
    };

    const url = getApiEndpoint(endpoint);

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
      console.log('📄 Full auth response text:', responseText);

      // Provide user-friendly error messages based on status code
      let userFriendlyError = 'Unable to connect to server. Please try again later.';

      if (response.status === 401 || response.status === 403) {
        userFriendlyError = 'Invalid email or password. Please check your credentials and try again.';
      } else if (response.status === 404) {
        userFriendlyError = 'Account not found. Please check your email or sign up for a new account.';
      } else if (response.status === 400) {
        userFriendlyError = 'Invalid request. Please check your information and try again.';
      } else if (response.status >= 500) {
        userFriendlyError = 'Server error. Please try again later.';
      } else if (!response.ok) {
        userFriendlyError = 'An error occurred. Please try again.';
      }

      return {
        success: false,
        error: userFriendlyError,
        status_code: response.status
      };
    }

    // Generate user-friendly error message
    let errorMessage: string | undefined;
    if (!response.ok) {
      const backendMessage = responseData.message || responseData.error || '';

      // Map common backend error messages to user-friendly ones
      if (response.status === 401 || response.status === 403) {
        // Check if it's specifically about password or general authentication
        if (backendMessage.toLowerCase().includes('password')) {
          errorMessage = 'Incorrect password. Please try again.';
        } else if (backendMessage.toLowerCase().includes('credential')) {
          errorMessage = 'Invalid email or password. Please check your credentials and try again.';
        } else {
          errorMessage = 'Invalid email or password. Please check your credentials and try again.';
        }
      } else if (response.status === 404) {
        errorMessage = 'Account not found. Please check your email or sign up for a new account.';
      } else if (response.status === 400) {
        errorMessage = backendMessage || 'Invalid request. Please check your information and try again.';
      } else if (response.status >= 500) {
        errorMessage = 'Server error. Please try again later.';
      } else {
        errorMessage = backendMessage || `HTTP ${response.status}`;
      }
    }

    return {
      success: response.ok,
      data: responseData,
      status_code: response.status,
      error: errorMessage
    };

  } catch (error) {

    if (error instanceof TypeError && error.message.includes('Failed to fetch')) {
      return {
        success: false,
        error: 'Network error: Unable to connect to server. Please check your internet connection or try again later.'
      };
    }

    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error'
    };
  }
};

// Auth API Service Class
export class AuthApiService {
  // Health check
  static async checkHealth(): Promise<boolean> {
    try {
      const result = await makeApiRequest('GET', '/health');
      return result.success;
    } catch (error) {
      console.error('Health check failed:', error);
      return false;
    }
  }

  // Login user
  static async login(request: LoginRequest): Promise<ApiResponse<LoginResponse>> {
    console.log('=== LOGIN REQUEST DEBUG ===');
    console.log('Endpoint: /user/login');
    console.log('Request body:', JSON.stringify(request, null, 2));
    console.log('========================');
    return makeApiRequest<LoginResponse>('POST', '/user/login', request);
  }

  // Register user
  static async register(request: RegisterRequest): Promise<ApiResponse> {
    return makeApiRequest('POST', '/user/register', request);
  }

  // Verify OTP
  static async verifyOtp(request: VerifyOtpRequest): Promise<ApiResponse<LoginResponse>> {
    return makeApiRequest<LoginResponse>('POST', '/user/verify-otp', request);
  }

  // Forgot password
  static async forgotPassword(request: ForgotPasswordRequest): Promise<ApiResponse> {
    return makeApiRequest('POST', '/user/forgot-password', request);
  }

  // Reset password
  static async resetPassword(request: ResetPasswordRequest): Promise<ApiResponse> {
    return makeApiRequest('POST', '/user/reset-password', request);
  }

  // Get user profile
  static async getUserProfile(apiKey: string): Promise<ApiResponse<UserProfileResponse>> {
    return makeApiRequest<UserProfileResponse>('GET', '/user/profile', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Get auth status
  static async getAuthStatus(apiKey: string, deviceId: string): Promise<ApiResponse<AuthStatusResponse>> {
    const headers: Record<string, string> = {};
    if (apiKey) {
      headers['X-API-Key'] = apiKey;
    }
    if (deviceId) {
      headers['X-Device-ID'] = deviceId;
    }

    return makeApiRequest<AuthStatusResponse>('GET', '/auth/status', undefined, headers);
  }

  // Register device for guest access
  static async registerDevice(request: DeviceRegisterRequest): Promise<ApiResponse<DeviceRegisterResponse>> {
    return makeApiRequest<DeviceRegisterResponse>('POST', '/guest/register', request);
  }

  // Get guest status
  static async getGuestStatus(apiKey: string): Promise<ApiResponse<any>> {
    return makeApiRequest('GET', '/guest/status', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Update user profile
  static async updateUserProfile(apiKey: string, profileData: Partial<UserProfileResponse>): Promise<ApiResponse> {
    return makeApiRequest('PUT', '/user/profile', profileData, {
      'X-API-Key': apiKey
    });
  }

  // Regenerate API key
  static async regenerateApiKey(apiKey: string): Promise<ApiResponse<{ api_key: string }>> {
    return makeApiRequest('POST', '/user/regenerate-api-key', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Get user usage statistics
  static async getUserUsage(apiKey: string): Promise<ApiResponse<any>> {
    return makeApiRequest('GET', '/user/usage', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Get user subscriptions (database subscriptions, not payment subscriptions)
  static async getUserSubscriptions(apiKey: string): Promise<ApiResponse<any>> {
    return makeApiRequest('GET', '/user/subscriptions', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Subscribe to database
  static async subscribeToDatabase(apiKey: string, databaseName: string): Promise<ApiResponse> {
    return makeApiRequest('POST', '/user/subscribe', { database_name: databaseName }, {
      'X-API-Key': apiKey
    });
  }

  // Get user transactions
  static async getUserTransactions(apiKey: string): Promise<ApiResponse<any>> {
    return makeApiRequest('GET', '/user/transactions', undefined, {
      'X-API-Key': apiKey
    });
  }

  // Delete user account
  static async deleteAccount(apiKey: string, password: string): Promise<ApiResponse> {
    return makeApiRequest('DELETE', '/user/account', { password }, {
      'X-API-Key': apiKey
    });
  }

  // Test API connectivity with authentication
  static async testApiConnectivity(apiKey?: string): Promise<{
    success: boolean;
    response?: any;
    error?: string;
    authenticated?: boolean;
  }> {
    try {
      // Test basic connectivity
      const healthResult = await this.checkHealth();
      if (!healthResult) {
        return { success: false, error: 'API server is not responding' };
      }

      // Test authentication if API key provided
      if (apiKey) {
        const authResult = await this.getAuthStatus(apiKey, 'test_device');
        return {
          success: authResult.success,
          response: authResult.data,
          authenticated: authResult.data?.authenticated || false,
          error: authResult.error
        };
      }

      // Test unauthenticated endpoint
      const testResult = await makeApiRequest('GET', '/test');
      return {
        success: testResult.success,
        response: testResult.data,
        error: testResult.error
      };

    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Connectivity test failed'
      };
    }
  }

  // Validate session
  static async validateSession(apiKey: string, deviceId: string): Promise<{
    valid: boolean;
    userType?: 'guest' | 'registered';
    userData?: any;
    error?: string;
  }> {
    try {
      const result = await this.getAuthStatus(apiKey, deviceId);

      if (result.success && result.data?.authenticated) {
        return {
          valid: true,
          userType: result.data.user_type,
          userData: result.data.user || result.data.guest
        };
      } else {
        return {
          valid: false,
          error: result.error || 'Session invalid'
        };
      }
    } catch (error) {
      return {
        valid: false,
        error: error instanceof Error ? error.message : 'Session validation failed'
      };
    }
  }

  // Refresh user session data
  static async refreshSession(apiKey: string): Promise<ApiResponse<{
    profile: UserProfileResponse;
    usage: any;
    subscriptions: any;
  }>> {
    try {
      const [profileResult, usageResult, subscriptionsResult] = await Promise.all([
        this.getUserProfile(apiKey),
        this.getUserUsage(apiKey),
        this.getUserSubscriptions(apiKey)
      ]);

      if (profileResult.success) {
        return {
          success: true,
          data: {
            profile: profileResult.data!,
            usage: usageResult.data,
            subscriptions: subscriptionsResult.data
          }
        };
      } else {
        return {
          success: false,
          error: profileResult.error || 'Failed to refresh session'
        };
      }
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Session refresh failed'
      };
    }
  }
}

// Utility function to generate device ID (must be 10-255 characters)
export const generateDeviceId = (): string => {
  const getNavigatorInfo = () => {
    return {
      platform: navigator.platform,
      userAgent: navigator.userAgent,
      language: navigator.language,
      cookieEnabled: navigator.cookieEnabled,
      onLine: navigator.onLine,
      screenRes: `${window.screen.width}x${window.screen.height}`,
      timezone: Intl.DateTimeFormat().resolvedOptions().timeZone
    };
  };

  const info = getNavigatorInfo();
  const infoString = JSON.stringify(info);

  let hash = 0;
  for (let i = 0; i < infoString.length; i++) {
    const char = infoString.charCodeAt(i);
    hash = ((hash << 5) - hash) + char;
    hash = hash & hash;
  }

  // Generate a device ID that's between 10-255 characters
  const hashStr = Math.abs(hash).toString(16);
  const timestamp = Date.now().toString(36);
  const random = Math.random().toString(36).substring(2);

  // Format: fincept_desktop_{hash}_{timestamp}_{random}
  const deviceId = `fincept_desktop_${hashStr}_${timestamp}_${random}`;

  // Ensure it's within the valid range (10-255 characters)
  return deviceId.length > 255 ? deviceId.substring(0, 255) : deviceId;
};

// Auth utility functions
export class AuthUtils {
  // Check if user is authenticated
  static isAuthenticated(session: any): boolean {
    return !!(session?.authenticated && session?.api_key);
  }

  // Check if user is admin
  static isAdmin(session: any): boolean {
    return this.isAuthenticated(session) && session?.user_info?.is_admin === true;
  }

  // Check if user has verified email
  static isVerified(session: any): boolean {
    return this.isAuthenticated(session) && session?.user_info?.is_verified === true;
  }

  // Check if user is on free plan
  static isFreePlan(session: any): boolean {
    return session?.user_info?.account_type === 'free';
  }

  // Check if user has active subscription
  static hasActiveSubscription(session: any): boolean {
    return session?.subscription?.has_subscription === true;
  }

  // Get user display name
  static getDisplayName(session: any): string {
    if (session?.user_type === 'guest') {
      return 'Guest User';
    }
    return session?.user_info?.username || session?.user_info?.email || 'User';
  }

  // Get account type display name
  static getAccountTypeDisplay(accountType: string): string {
    const typeMap: Record<string, string> = {
      'free': 'Free',
      'basic': 'Basic',
      'standard': 'Standard',
      'pro': 'Pro',
      'enterprise': 'Enterprise'
    };
    return typeMap[accountType] || accountType;
  }

  // Check if session is expiring soon (for guest users)
  static isSessionExpiringSoon(session: any, minutesThreshold: number = 30): boolean {
    if (session?.user_type !== 'guest' || !session?.expires_at) {
      return false;
    }

    const expiryTime = new Date(session.expires_at).getTime();
    const currentTime = new Date().getTime();
    const timeDiff = expiryTime - currentTime;
    const minutesRemaining = timeDiff / (1000 * 60);

    return minutesRemaining <= minutesThreshold && minutesRemaining > 0;
  }

  // Format time remaining for guest sessions
  static formatTimeRemaining(expiresAt: string): string {
    const expiryTime = new Date(expiresAt).getTime();
    const currentTime = new Date().getTime();
    const timeDiff = expiryTime - currentTime;

    if (timeDiff <= 0) {
      return 'Expired';
    }

    const hours = Math.floor(timeDiff / (1000 * 60 * 60));
    const minutes = Math.floor((timeDiff % (1000 * 60 * 60)) / (1000 * 60));

    if (hours > 0) {
      return `${hours}h ${minutes}m`;
    } else {
      return `${minutes}m`;
    }
  }

  // Validate email format
  static isValidEmail(email: string): boolean {
    const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    return emailRegex.test(email);
  }

  // Validate password strength
  static validatePassword(password: string): {
    isValid: boolean;
    errors: string[];
    score: number;
  } {
    const errors: string[] = [];
    let score = 0;

    if (password.length < 8) {
      errors.push('Password must be at least 8 characters long');
    } else {
      score += 1;
    }

    if (!/[A-Z]/.test(password)) {
      errors.push('Password must contain at least one uppercase letter');
    } else {
      score += 1;
    }

    if (!/[a-z]/.test(password)) {
      errors.push('Password must contain at least one lowercase letter');
    } else {
      score += 1;
    }

    if (!/\d/.test(password)) {
      errors.push('Password must contain at least one number');
    } else {
      score += 1;
    }

    if (!/[!@#$%^&*(),.?":{}|<>]/.test(password)) {
      errors.push('Password should contain at least one special character');
    } else {
      score += 1;
    }

    return {
      isValid: errors.length === 0,
      errors,
      score: Math.min(score, 5)
    };
  }
}

export default AuthApiService;