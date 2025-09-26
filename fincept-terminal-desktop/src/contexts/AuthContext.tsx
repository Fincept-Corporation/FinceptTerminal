// File: src/contexts/AuthContext.tsx
// Authentication context for managing user state and backend API integration

import React, { createContext, useContext, useState, useEffect, ReactNode } from 'react';
import { AuthApiService } from '@/services/authApi';

// Types based on your Python API client
interface SessionData {
  authenticated: boolean;
  user_type: 'guest' | 'registered' | null;
  api_key: string | null;
  device_id: string;
  user_info?: {
    username?: string;
    email?: string;
    credit_balance?: number;
  };
  expires_at?: string;
  daily_limit?: number;
  requests_today?: number;
}

interface AuthContextType {
  session: SessionData | null;
  isLoading: boolean;
  isFirstTimeUser: boolean;
  login: (email: string, password: string) => Promise<{ success: boolean; error?: string }>;
  signup: (username: string, email: string, password: string) => Promise<{ success: boolean; error?: string }>;
  verifyOtp: (email: string, otp: string) => Promise<{ success: boolean; error?: string }>;
  setupGuestAccess: () => Promise<{ success: boolean; error?: string }>;
  logout: () => Promise<void>;
  checkApiConnectivity: () => Promise<boolean>;
  forgotPassword: (email: string) => Promise<{ success: boolean; error?: string }>;
  resetPassword: (email: string, otp: string, newPassword: string) => Promise<{ success: boolean; error?: string }>;
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

  // Validate session with API
  const validateSession = async (sessionData: SessionData): Promise<SessionData | null> => {
    if (!sessionData.api_key) return null;

    const headers: Record<string, string> = {};
    if (sessionData.api_key) {
      headers['X-API-Key'] = sessionData.api_key;
    }
    if (sessionData.device_id) {
      headers['X-Device-ID'] = sessionData.device_id;
    }

    const result = await AuthApiService.getAuthStatus(sessionData.api_key, sessionData.device_id);

    if (result.success && result.data?.authenticated) {
      const apiData = result.data;
      return {
        authenticated: true,
        user_type: apiData.user_type,
        api_key: sessionData.api_key,
        device_id: sessionData.device_id,
        user_info: apiData.user_type === 'registered' ? apiData.user : undefined,
        expires_at: apiData.user_type === 'guest' ? apiData.guest?.expires_at : undefined,
        daily_limit: apiData.user_type === 'guest' ? apiData.guest?.daily_limit : undefined,
        requests_today: apiData.user_type === 'guest' ? apiData.guest?.requests_today : undefined
      };
    }

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

        if (savedSession) {
          console.log('AuthContext: Validating session with API...');
          // For development, skip API validation to avoid CORS issues
          if (import.meta.env.DEV) {
            console.log('AuthContext: Development mode - using saved session without validation');
            setSession(savedSession);
          } else {
            const validatedSession = await validateSession(savedSession);
            if (validatedSession) {
              console.log('AuthContext: Session validated successfully');
              setSession(validatedSession);
              saveSession(validatedSession);
            } else {
              console.log('AuthContext: Session validation failed, clearing...');
              clearSession();
            }
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
      console.log('Checking API connectivity...');

      const isConnected = await AuthApiService.checkHealth();

      console.log('API connectivity result:', isConnected);
      return isConnected;
    } catch (error) {
      console.error('API connectivity check failed:', error);
      return false;
    }
  };

  // Login function
  const login = async (email: string, password: string): Promise<{ success: boolean; error?: string }> => {
    try {
      console.log('AuthContext: Attempting login for:', email);

      const result = await AuthApiService.login({ email, password });

      if (result.success && result.data?.success) {
        const apiData = result.data.data;
        const newSession: SessionData = {
          authenticated: true,
          user_type: 'registered',
          api_key: apiData.api_key,
          device_id: generateDeviceId(),
          user_info: {
            username: apiData.username,
            email: apiData.email,
            credit_balance: apiData.credit_balance
          }
        };

        setSession(newSession);
        saveSession(newSession);
        setIsFirstTimeUser(checkIsFirstTimeUser());

        console.log('AuthContext: Login successful');
        return { success: true };
      } else {
        console.log('AuthContext: Login failed:', result.data?.message || result.error);
        return {
          success: false,
          error: result.data?.message || result.error || 'Login failed'
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
      console.log('AuthContext: Attempting signup for:', username, email);

      const result = await AuthApiService.register({
        username,
        email,
        password
      });

      if (result.success && result.data?.success) {
        console.log('AuthContext: Signup successful, OTP sent');
        return { success: true };
      } else {
        console.log('AuthContext: Signup failed:', result.data?.message || result.error);
        return {
          success: false,
          error: result.data?.message || result.error || 'Registration failed'
        };
      }
    } catch (error) {
      console.error('AuthContext: Signup error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Registration failed'
      };
    }
  };

  // OTP verification
  const verifyOtp = async (email: string, otp: string): Promise<{ success: boolean; error?: string }> => {
    try {
      console.log('AuthContext: Verifying OTP for:', email);

      const result = await AuthApiService.verifyOtp({ email, otp });

      if (result.success && result.data?.success) {
        const apiData = result.data.data;
        const newSession: SessionData = {
          authenticated: true,
          user_type: 'registered',
          api_key: apiData.api_key,
          device_id: generateDeviceId(),
          user_info: {
            username: apiData.username,
            email: apiData.email,
            credit_balance: apiData.credit_balance
          }
        };

        setSession(newSession);
        saveSession(newSession);
        setIsFirstTimeUser(checkIsFirstTimeUser());

        console.log('AuthContext: OTP verification successful');
        return { success: true };
      } else {
        console.log('AuthContext: OTP verification failed:', result.data?.message || result.error);
        return {
          success: false,
          error: result.data?.message || result.error || 'Verification failed'
        };
      }
    } catch (error) {
      console.error('AuthContext: OTP verification error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Verification failed'
      };
    }
  };

  // Forgot password function
  const forgotPassword = async (email: string): Promise<{ success: boolean; error?: string }> => {
    try {
      console.log('AuthContext: Requesting password reset for:', email);

      const result = await AuthApiService.forgotPassword({ email });

      if (result.success && result.data?.success) {
        console.log('AuthContext: Password reset OTP sent successfully');
        return { success: true };
      } else {
        console.log('AuthContext: Password reset request failed:', result.data?.message || result.error);
        return {
          success: false,
          error: result.data?.message || result.error || 'Failed to send reset email'
        };
      }
    } catch (error) {
      console.error('AuthContext: Forgot password error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to send reset email'
      };
    }
  };

  // Reset password function
  const resetPassword = async (email: string, otp: string, newPassword: string): Promise<{ success: boolean; error?: string }> => {
    try {
      console.log('AuthContext: Attempting password reset for:', email);

      const result = await AuthApiService.resetPassword({
        email,
        otp,
        new_password: newPassword
      });

      if (result.success && result.data?.success) {
        console.log('AuthContext: Password reset successful');
        return { success: true };
      } else {
        console.log('AuthContext: Password reset failed:', result.data?.message || result.error);
        return {
          success: false,
          error: result.data?.message || result.error || 'Password reset failed'
        };
      }
    } catch (error) {
      console.error('AuthContext: Reset password error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Password reset failed'
      };
    }
  };

  // Guest access setup
  const setupGuestAccess = async (): Promise<{ success: boolean; error?: string }> => {
    try {
      console.log('AuthContext: Setting up guest access...');

      // For development, create a mock guest session
      if (import.meta.env.DEV) {
        console.log('AuthContext: Development mode - creating mock guest session');

        const mockGuestSession: SessionData = {
          authenticated: true,
          user_type: 'guest',
          api_key: 'mock_guest_api_key_' + Date.now(),
          device_id: generateDeviceId(),
          expires_at: new Date(Date.now() + 24 * 60 * 60 * 1000).toISOString(), // 24 hours
          daily_limit: 50,
          requests_today: 0
        };

        setSession(mockGuestSession);
        saveSession(mockGuestSession);
        setIsFirstTimeUser(checkIsFirstTimeUser());

        console.log('AuthContext: Mock guest session created');
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
      });

      if (result.success && result.data?.success) {
        const apiData = result.data.data;
        const guestSession: SessionData = {
          authenticated: true,
          user_type: 'guest',
          api_key: apiData.api_key || apiData.temp_api_key,
          device_id: deviceId,
          expires_at: apiData.expires_at,
          daily_limit: apiData.daily_limit || 50,
          requests_today: apiData.requests_today || 0
        };

        setSession(guestSession);
        saveSession(guestSession);
        setIsFirstTimeUser(checkIsFirstTimeUser());

        console.log('AuthContext: Guest access setup successful');
        return { success: true };
      } else if (result.status_code === 409) {
        console.log('AuthContext: Device exists, getting existing session...');

        const statusResult = await AuthApiService.getGuestStatus(deviceId);

        if (statusResult.success && statusResult.data?.success) {
          const statusData = statusResult.data.data;
          const existingSession: SessionData = {
            authenticated: true,
            user_type: 'guest',
            api_key: statusData.api_key,
            device_id: deviceId,
            expires_at: statusData.expires_at,
            daily_limit: statusData.daily_limit || 50,
            requests_today: statusData.requests_today || 0
          };

          setSession(existingSession);
          saveSession(existingSession);
          setIsFirstTimeUser(checkIsFirstTimeUser());

          console.log('AuthContext: Existing guest session retrieved');
          return { success: true };
        }
      }

      console.log('AuthContext: Guest setup failed:', result.data?.message || result.error);
      return {
        success: false,
        error: result.data?.message || result.error || 'Failed to setup guest access'
      };
    } catch (error) {
      console.error('AuthContext: Guest setup error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to setup guest access'
      };
    }
  };

  // Logout function
  const logout = async (): Promise<void> => {
    console.log('AuthContext: Logging out...');

    if (session?.api_key) {
      try {
        await AuthApiService.logout(session.api_key);
      } catch (error) {
        console.error('Logout API call failed:', error);
      }
    }

    setSession(null);
    clearSession();
    setIsFirstTimeUser(checkIsFirstTimeUser());

    console.log('AuthContext: Logout complete');
  };

  const value: AuthContextType = {
    session,
    isLoading,
    isFirstTimeUser,
    login,
    signup,
    verifyOtp,
    setupGuestAccess,
    logout,
    checkApiConnectivity,
    forgotPassword,
    resetPassword
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