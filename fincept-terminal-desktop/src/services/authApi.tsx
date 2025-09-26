// File: src/services/authApi.ts
// Authentication API service for handling all auth-related API calls

import { SessionData } from '@/contexts/AuthContext';

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
  };
}

// Response Types
interface LoginResponse {
  api_key: string;
  username: string;
  email: string;
  credit_balance: number;
}

interface AuthStatusResponse {
  authenticated: boolean;
  user_type: 'guest' | 'registered';
  user?: {
    username: string;
    email: string;
    credit_balance: number;
  };
  guest?: {
    expires_at: string;
    daily_limit: number;
    requests_today: number;
  };
}

interface DeviceRegisterResponse {
  api_key?: string;
  temp_api_key?: string;
  expires_at?: string;
  daily_limit?: number;
  requests_today?: number;
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
      'skip_zrok_interstitial': '1',
      ...headers
    };

    const url = getApiEndpoint(endpoint);
    console.log('🚀 Making request:', {
      method,
      url,
      endpoint,
      data,
      headers: defaultHeaders
    });

    const config: RequestInit = {
      method,
      headers: defaultHeaders,
      mode: 'cors',
    };

    if (data && (method === 'POST' || method === 'PUT')) {
      config.body = JSON.stringify(data);
    }

    const response = await fetch(url, config);

    console.log('📡 Response received:', {
      status: response.status,
      statusText: response.statusText,
      headers: Object.fromEntries(response.headers.entries()),
      url: response.url
    });

    const responseText = await response.text();
    console.log('📝 Raw response text:', responseText.substring(0, 200));

    let responseData;
    try {
      responseData = JSON.parse(responseText);
      console.log('✅ Parsed JSON:', responseData);
    } catch (parseError) {
      console.error('❌ JSON parse failed:', parseError);
      console.log('📄 Full response text:', responseText);

      return {
        success: false,
        error: `Invalid JSON response: ${responseText.substring(0, 100)}...`,
        status_code: response.status
      };
    }

    return {
      success: response.ok,
      data: responseData,
      status_code: response.status,
      error: response.ok ? undefined : responseData.message || `HTTP ${response.status}`
    };

  } catch (error) {
    console.error('💥 Request failed completely:', error);

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
    return makeApiRequest<LoginResponse>('POST', '/auth/login', request);
  }

  // Register user
  static async register(request: RegisterRequest): Promise<ApiResponse> {
    return makeApiRequest('POST', '/auth/register', request);
  }

  // Verify OTP
  static async verifyOtp(request: VerifyOtpRequest): Promise<ApiResponse<LoginResponse>> {
    return makeApiRequest<LoginResponse>('POST', '/auth/verify-otp', request);
  }

  // Forgot password
  static async forgotPassword(request: ForgotPasswordRequest): Promise<ApiResponse> {
    return makeApiRequest('POST', '/auth/forgot-password', request);
  }

  // Reset password
  static async resetPassword(request: ResetPasswordRequest): Promise<ApiResponse> {
    return makeApiRequest('POST', '/auth/reset-password', request);
  }

  // Logout user
  static async logout(apiKey: string): Promise<ApiResponse> {
    return makeApiRequest('POST', '/auth/logout', undefined, {
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
    return makeApiRequest<DeviceRegisterResponse>('POST', '/device/register', request);
  }

  // Get guest status
  static async getGuestStatus(deviceId: string): Promise<ApiResponse<DeviceRegisterResponse>> {
    return makeApiRequest<DeviceRegisterResponse>('GET', '/guest/status', undefined, {
      'X-Device-ID': deviceId
    });
  }
}

// Utility function to generate device ID
export const generateDeviceId = (): string => {
  const getNavigatorInfo = () => {
    return {
      platform: navigator.platform,
      userAgent: navigator.userAgent,
      language: navigator.language,
      cookieEnabled: navigator.cookieEnabled,
      onLine: navigator.onLine
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

  return `desktop_${Math.abs(hash).toString(16)}`;
};

export default AuthApiService;