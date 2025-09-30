// File: src/contexts/AuthContext.tsx
// Authentication context for managing user state and backend API integration with payment support

import React, { createContext, useContext, useState, useEffect, ReactNode } from 'react';
import { AuthApiService } from '@/services/authApi';
import { PaymentApiService } from '@/services/paymentApi';

// Response types to match your API
export interface LoginResponse {
  success: boolean;
  data?: {
    api_key: string;
    username: string;
    email: string;
    credit_balance: number;
  };
  message?: string;
  error?: string;
}

export interface UserProfileResponse {
  success: boolean;
  data?: {
    id: number;
    username: string;
    email: string;
    account_type: string;
    credit_balance: number;
    is_verified: boolean;
    mfa_enabled: boolean;
    created_at: string;
    last_login_at: string;
  };
  message?: string;
  error?: string;
}

export interface DeviceRegisterResponse {
  success: boolean;
  data?: {
    api_key?: string;
    temp_api_key?: string;
    expires_at?: string;
    daily_limit?: number;
    requests_today?: number;
  };
  message?: string;
  error?: string;
  status_code?: number;
}

export interface ApiResponse {
  success: boolean;
  data?: any;
  message?: string;
  error?: string;
  status_code?: number;
}

// Payment related types
export interface SubscriptionPlan {
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

export interface PaymentSession {
  checkout_url: string;
  session_id: string;
  payment_uuid: string;
  plan: {
    name: string;
    price: number;
  };
}

export interface UserSubscription {
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

// Types based on your Python API client
export interface SessionData {
  authenticated: boolean;
  user_type: 'guest' | 'registered' | null;
  api_key: string | null;
  device_id: string;
  user_info?: {
    username?: string;
    email?: string;
    account_type?: string;
    credit_balance?: number;
    is_verified?: boolean;
    mfa_enabled?: boolean;
  };
  expires_at?: string;
  daily_limit?: number;
  requests_today?: number;
  subscription?: UserSubscription;
}

interface AuthContextType {
  session: SessionData | null;
  isLoading: boolean;
  isFirstTimeUser: boolean;
  availablePlans: SubscriptionPlan[];
  isLoadingPlans: boolean;
  login: (email: string, password: string) => Promise<{ success: boolean; error?: string }>;
  signup: (username: string, email: string, password: string) => Promise<{ success: boolean; error?: string }>;
  verifyOtp: (email: string, otp: string) => Promise<{ success: boolean; error?: string }>;
  setupGuestAccess: () => Promise<{ success: boolean; error?: string }>;
  logout: () => Promise<void>;
  checkApiConnectivity: () => Promise<boolean>;
  forgotPassword: (email: string) => Promise<{ success: boolean; error?: string }>;
  resetPassword: (email: string, otp: string, newPassword: string) => Promise<{ success: boolean; error?: string }>;
  // Payment methods
  fetchPlans: () => Promise<{ success: boolean; error?: string }>;
  createPaymentSession: (planId: string) => Promise<{ success: boolean; data?: PaymentSession; error?: string }>;
  getUserSubscription: () => Promise<{ success: boolean; data?: UserSubscription; error?: string }>;
  refreshUserData: () => Promise<void>;
}

const AuthContext = createContext<AuthContextType | undefined>(undefined);

// Device ID generation
const generateDeviceId = (): string => {
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

// Session storage utilities
const STORAGE_KEY = 'fincept_session';

const saveSession = (session: SessionData) => {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({
      ...session,
      saved_at: new Date().toISOString()
    }));
  } catch (error) {
    console.error('Failed to save session:', error);
  }
};

const loadSession = (): SessionData | null => {
  try {
    const stored = localStorage.getItem(STORAGE_KEY);
    if (!stored) return null;

    const session = JSON.parse(stored);
    if (session.user_type === 'guest' && session.expires_at) {
      if (new Date() > new Date(session.expires_at)) {
        clearSession();
        return null;
      }
    }

    return session;
  } catch (error) {
    console.error('Failed to load session:', error);
    clearSession();
    return null;
  }
};

const clearSession = () => {
  try {
    localStorage.removeItem(STORAGE_KEY);
  } catch (error) {
    console.error('Failed to clear session:', error);
  }
};

const checkIsFirstTimeUser = (): boolean => {
  return !localStorage.getItem(STORAGE_KEY);
};

interface AuthProviderProps {
  children: ReactNode;
}

export const AuthProvider: React.FC<AuthProviderProps> = ({ children }) => {
  const [session, setSession] = useState<SessionData | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [isFirstTimeUser, setIsFirstTimeUser] = useState(false);
  const [availablePlans, setAvailablePlans] = useState<SubscriptionPlan[]>([]);
  const [isLoadingPlans, setIsLoadingPlans] = useState(false);

  // Fetch user profile after login
  // Find this function in AuthContext.tsx and update it:
const fetchUserProfile = async (apiKey: string): Promise<UserProfileResponse['data'] | null> => {
  try {
    const result = await AuthApiService.getUserProfile(apiKey);
    console.log('AuthContext: Raw profile result:', result); // Add this debug line

    if (result.success && result.data) {
      // Fix: Extract the nested data properly
      const profileData = (result.data as any).data || result.data;
      console.log('AuthContext: Extracted profile data:', profileData); // Add this debug line
      return profileData;
    }
    return null;
  } catch (error) {
    console.error('Failed to fetch user profile:', error);
    return null;
  }
};

  // Fetch user subscription data
  const fetchUserSubscription = async (apiKey: string): Promise<UserSubscription | null> => {
    try {
      const result = await PaymentApiService.getUserSubscription(apiKey);
      if (result.success && result.data) {
        return result.data;
      }
      return null;
    } catch (error) {
      console.error('Failed to fetch user subscription:', error);
      return null;
    }
  };

  // Refresh user data including subscription
  const refreshUserData = async (): Promise<void> => {
    if (!session?.api_key || session.user_type !== 'registered') return;

    try {
      const [userProfile, userSubscription] = await Promise.all([
        fetchUserProfile(session.api_key),
        fetchUserSubscription(session.api_key)
      ]);

      if (userProfile) {
        const updatedSession: SessionData = {
          ...session,
          user_info: {
            username: userProfile.username,
            email: userProfile.email,
            account_type: userProfile.account_type,
            credit_balance: userProfile.credit_balance,
            is_verified: userProfile.is_verified,
            mfa_enabled: userProfile.mfa_enabled
          },
          subscription: userSubscription || undefined
        };

        setSession(updatedSession);
        saveSession(updatedSession);
      }
    } catch (error) {
      console.error('Failed to refresh user data:', error);
    }
  };

  // Validate session with API
  const validateSession = async (sessionData: SessionData): Promise<SessionData | null> => {
    if (!sessionData.api_key) {
      console.log('validateSession: No API key found');
      return null;
    }

    console.log('validateSession: Validating session with API...');
    const result = await AuthApiService.getAuthStatus(sessionData.api_key, sessionData.device_id);
    console.log('validateSession: Auth status result:', result);
    console.log('validateSession: result.success =', result.success);
    console.log('validateSession: result.data =', result.data);

    // Backend returns nested structure: result.data.data contains the actual auth data
    const authData = (result.data as any)?.data || result.data;
    console.log('validateSession: authData =', authData);
    console.log('validateSession: authData?.authenticated =', authData?.authenticated);

    if (result.success && authData?.authenticated) {
      const apiData = authData;
      console.log('validateSession: User authenticated, type:', apiData.user_type);

      // For registered users, fetch detailed profile and subscription
      let userProfile = null;
      let userSubscription = null;

      if (apiData.user_type === 'registered') {
        console.log('validateSession: Fetching user profile...');
        userProfile = await fetchUserProfile(sessionData.api_key);
        console.log('validateSession: User profile result:', userProfile);

        console.log('validateSession: Fetching user subscription...');
        userSubscription = await fetchUserSubscription(sessionData.api_key);
        console.log('validateSession: User subscription result:', userSubscription);
      }

      const validatedSession = {
        authenticated: true,
        user_type: apiData.user_type,
        api_key: sessionData.api_key,
        device_id: sessionData.device_id,
        user_info: apiData.user_type === 'registered' && userProfile ? {
          username: userProfile.username,
          email: userProfile.email,
          account_type: userProfile.account_type,
          credit_balance: userProfile.credit_balance,
          is_verified: userProfile.is_verified,
          mfa_enabled: userProfile.mfa_enabled
        } : apiData.user,
        expires_at: apiData.user_type === 'guest' ? apiData.guest?.expires_at : undefined,
        daily_limit: apiData.user_type === 'guest' ? apiData.guest?.daily_limit : undefined,
        requests_today: apiData.user_type === 'guest' ? apiData.guest?.requests_today : undefined,
        subscription: userSubscription || undefined
      };

      console.log('validateSession: Returning validated session:', validatedSession);
      return validatedSession;
    }

    console.log('validateSession: Auth status check failed or not authenticated');
    return null;
  };

  // Initialize session on app load
  useEffect(() => {
    const initializeSession = async () => {
      console.log('AuthContext: Initializing session...');

      try {
        setIsFirstTimeUser(checkIsFirstTimeUser());

        const savedSession = loadSession();
        console.log('AuthContext: Saved session:', savedSession ? 'Found' : 'Not found');

        // In your AuthContext useEffect:
        if (savedSession) {
          if (savedSession.user_type === 'registered') {
            // Always validate registered users, even in dev mode
            const validatedSession = await validateSession(savedSession);
            if (validatedSession) {
              setSession(validatedSession);
              saveSession(validatedSession);
            } else {
              console.log('Session validation failed, clearing...');
              clearSession();
            }
          } else {
            // Only skip validation for guest users in dev mode
            setSession(savedSession);
          }
        }

      } catch (error) {
        console.error('AuthContext: Initialization error:', error);
      } finally {
        console.log('AuthContext: Initialization complete');
        setIsLoading(false);
      }
    };

    initializeSession();
  }, []);

  // Check API connectivity
  const checkApiConnectivity = async (): Promise<boolean> => {
    try {
      const isConnected = await AuthApiService.checkHealth();
      return isConnected;
    } catch (error) {
      console.error('API connectivity check failed:', error);
      return false;
    }
  };

  // Fetch subscription plans
  // Fetch subscription plans
  const fetchPlans = async (): Promise<{ success: boolean; error?: string }> => {
    setIsLoadingPlans(true);
    try {
      console.log('AuthContext: Fetching plans from API...');
      const result = await PaymentApiService.getSubscriptionPlans();

      console.log('AuthContext: Plans API result:', result);

      if (result.success && result.data) {
        // Handle the nested response structure: result.data.data.plans
        const plansData = (result.data as any).data || result.data;
        const plans = plansData.plans || [];

        console.log('AuthContext: Extracted plans:', plans);
        console.log('AuthContext: Number of plans:', plans.length);

        if (plans.length > 0) {
          setAvailablePlans(plans);
          console.log('AuthContext: Plans set successfully');
          return { success: true };
        } else {
          console.log('AuthContext: No plans found in response');
          return { success: false, error: 'No plans available' };
        }
      } else {
        console.log('AuthContext: API request failed:', result.error);
        return { success: false, error: result.error || 'Failed to fetch plans' };
      }
    } catch (error) {
      console.error('AuthContext: Failed to fetch plans:', error);
      return { success: false, error: 'Failed to fetch plans' };
    } finally {
      setIsLoadingPlans(false);
    }
  };

  // Create payment session
  const createPaymentSession = async (planId: string): Promise<{ success: boolean; data?: PaymentSession; error?: string }> => {
    if (!session?.api_key) {
      return { success: false, error: 'No API key available' };
    }

    try {
      const result = await PaymentApiService.createCheckoutSession(session.api_key, {
        plan_id: planId,
        return_url: `${window.location.origin}/payment-success`
      });

      if (result.success && result.data) {
        return { success: true, data: result.data };
      } else {
        return { success: false, error: result.error || 'Failed to create payment session' };
      }
    } catch (error) {
      console.error('Failed to create payment session:', error);
      return { success: false, error: 'Failed to create payment session' };
    }
  };

  // Get user subscription
  const getUserSubscription = async (): Promise<{ success: boolean; data?: UserSubscription; error?: string }> => {
    if (!session?.api_key) {
      return { success: false, error: 'No API key available' };
    }

    try {
      const result = await PaymentApiService.getUserSubscription(session.api_key);
      if (result.success) {
        return { success: true, data: result.data };
      } else {
        return { success: false, error: result.error || 'Failed to get subscription' };
      }
    } catch (error) {
      console.error('Failed to get subscription:', error);
      return { success: false, error: 'Failed to get subscription' };
    }
  };

  // Login function
  const login = async (email: string, password: string): Promise<{ success: boolean; error?: string }> => {
    try {
      console.log('AuthContext: Attempting login for:', email);

      const result = await AuthApiService.login({ email, password }) as ApiResponse;

      console.log('AuthContext: Full login result:', result);
      console.log('AuthContext: result.success:', result.success);
      console.log('AuthContext: result.data:', result.data);

      // In AuthContext.tsx, update the login function around line 480-515:

if (result.success && result.data && result.data.data && result.data.data.api_key) {
  const apiKey = result.data.data.api_key;
  console.log('AuthContext: Login successful, API key received:', apiKey.substring(0, 20) + '...');

  // Fetch user profile and subscription to get account_type and other details
  const [userProfile, userSubscription] = await Promise.all([
    fetchUserProfile(apiKey),
    fetchUserSubscription(apiKey)
  ]);

  const newSession: SessionData = {
    authenticated: true,
    user_type: 'registered',
    api_key: apiKey,
    device_id: generateDeviceId(),
    user_info: userProfile ? {
      username: userProfile.username,
      email: userProfile.email,
      account_type: userProfile.account_type,  // ← This should be working
      credit_balance: userProfile.credit_balance,
      is_verified: userProfile.is_verified,
      mfa_enabled: userProfile.mfa_enabled
    } : {
      // Fallback if profile fetch fails - assume free account
      email: email,
      account_type: 'free',  // ← Make sure this fallback is 'free', not undefined
      credit_balance: 0
    },
    subscription: userSubscription || undefined
  };

  setSession(newSession);
  saveSession(newSession);
  setIsFirstTimeUser(checkIsFirstTimeUser());

  console.log('AuthContext: Login successful, account_type:', newSession.user_info?.account_type);
  console.log('AuthContext: Has subscription:', !!newSession.subscription?.has_subscription);
  return { success: true };
} else {
        console.log('AuthContext: Login failed - success:', result.success, 'data:', result.data, 'api_key:', result.data?.api_key);
        return {
          success: false,
          error: result.message || result.error || 'Login failed'
        };
      }
    } catch (error) {
      console.error('AuthContext: Login error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Login failed'
      };
    }
  };

  // Signup function
  const signup = async (username: string, email: string, password: string): Promise<{ success: boolean; error?: string }> => {
    try {
      const result = await AuthApiService.register({
        username,
        email,
        password
      }) as ApiResponse;

      if (result.success) {
        return { success: true };
      } else {
        return {
          success: false,
          error: result.message || result.error || 'Registration failed'
        };
      }
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Registration failed'
      };
    }
  };

  // OTP verification
  const verifyOtp = async (email: string, otp: string): Promise<{ success: boolean; error?: string }> => {
    try {
      const result = await AuthApiService.verifyOtp({ email, otp }) as ApiResponse;

      if (result.success && result.data && result.data.data && result.data.data.api_key) {
        const apiKey = result.data.data.api_key;
        console.log('AuthContext: OTP verification successful, API key received');

        // Fetch user profile for new registered user (no subscription yet)
        const userProfile = await fetchUserProfile(apiKey);

        const newSession: SessionData = {
          authenticated: true,
          user_type: 'registered',
          api_key: apiKey,
          device_id: generateDeviceId(),
          user_info: userProfile ? {
            username: userProfile.username,
            email: userProfile.email,
            account_type: userProfile.account_type,
            credit_balance: userProfile.credit_balance,
            is_verified: userProfile.is_verified,
            mfa_enabled: userProfile.mfa_enabled
          } : {
            // Fallback if profile fetch fails - assume free account
            email: email,
            account_type: 'free',
            credit_balance: 0
          },
          subscription: undefined // New user won't have subscription yet
        };

        setSession(newSession);
        saveSession(newSession);
        setIsFirstTimeUser(checkIsFirstTimeUser());

        return { success: true };
      } else {
        return {
          success: false,
          error: result.message || result.error || 'Verification failed'
        };
      }
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Verification failed'
      };
    }
  };

  // Forgot password function
  const forgotPassword = async (email: string): Promise<{ success: boolean; error?: string }> => {
    try {
      const result = await AuthApiService.forgotPassword({ email }) as ApiResponse;
      return result.success
        ? { success: true }
        : { success: false, error: result.message || result.error || 'Failed to send reset email' };
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to send reset email'
      };
    }
  };

  // Reset password function
  const resetPassword = async (email: string, otp: string, newPassword: string): Promise<{ success: boolean; error?: string }> => {
    try {
      const result = await AuthApiService.resetPassword({
        email,
        otp,
        new_password: newPassword
      }) as ApiResponse;

      return result.success
        ? { success: true }
        : { success: false, error: result.message || result.error || 'Password reset failed' };
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Password reset failed'
      };
    }
  };

  // Guest access setup
  const setupGuestAccess = async (): Promise<{ success: boolean; error?: string }> => {
    try {
      if (import.meta.env.DEV) {
        const mockGuestSession: SessionData = {
          authenticated: true,
          user_type: 'guest',
          api_key: 'mock_guest_api_key_' + Date.now(),
          device_id: generateDeviceId(),
          expires_at: new Date(Date.now() + 24 * 60 * 60 * 1000).toISOString(),
          daily_limit: 50,
          requests_today: 0
        };

        setSession(mockGuestSession);
        saveSession(mockGuestSession);
        setIsFirstTimeUser(checkIsFirstTimeUser());

        return { success: true };
      }

      const deviceId = generateDeviceId();

      const result = await AuthApiService.registerDevice({
        device_id: deviceId,
        device_name: `Fincept Terminal - ${navigator.platform}`,
        platform: 'desktop',
        hardware_info: {
          platform: navigator.platform,
          user_agent: navigator.userAgent,
          language: navigator.language
        }
      }) as DeviceRegisterResponse;

      if (result.success && result.data) {
        const apiData = result.data;
        const guestSession: SessionData = {
          authenticated: true,
          user_type: 'guest',
          api_key: apiData.api_key || apiData.temp_api_key || null,
          device_id: deviceId,
          expires_at: apiData.expires_at,
          daily_limit: apiData.daily_limit || 50,
          requests_today: apiData.requests_today || 0
        };

        setSession(guestSession);
        saveSession(guestSession);
        setIsFirstTimeUser(checkIsFirstTimeUser());

        return { success: true };
      }

      return {
        success: false,
        error: result.message || result.error || 'Failed to setup guest access'
      };
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to setup guest access'
      };
    }
  };

  // Logout function
  const logout = async (): Promise<void> => {
    console.log('AuthContext: Logging out...');

    setSession(null);
    clearSession();
    setIsFirstTimeUser(checkIsFirstTimeUser());

    console.log('AuthContext: Logout complete');
  };

  const value: AuthContextType = {
    session,
    isLoading,
    isFirstTimeUser,
    availablePlans,
    isLoadingPlans,
    login,
    signup,
    verifyOtp,
    setupGuestAccess,
    logout,
    checkApiConnectivity,
    forgotPassword,
    resetPassword,
    fetchPlans,
    createPaymentSession,
    getUserSubscription,
    refreshUserData
  };

  return (
    <AuthContext.Provider value={value}>
      {children}
    </AuthContext.Provider>
  );
};

export const useAuth = (): AuthContextType => {
  const context = useContext(AuthContext);
  if (context === undefined) {
    throw new Error('useAuth must be used within an AuthProvider');
  }
  return context;
};