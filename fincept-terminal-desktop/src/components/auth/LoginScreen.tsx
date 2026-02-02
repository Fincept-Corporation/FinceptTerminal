// File: src/components/auth/LoginScreen.tsx
// User login screen with email/password authentication, MFA support, and guest access
// Production-ready: State machine, cleanup, timeout, validation

import React, { useReducer, useEffect, useRef, useCallback } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Mail, Lock, Eye, EyeOff, Shield } from "lucide-react";
import { Screen } from '../../App';
import { useAuth } from '@/contexts/AuthContext';
import { useTranslation } from 'react-i18next';
import { CompactLanguageSelector } from './CompactLanguageSelector';
import { withTimeout } from '@/services/core/apiUtils';
import { validateEmail, sanitizeInput } from '@/services/core/validators';

// ============================================================================
// Constants
// ============================================================================

const AUTH_TIMEOUT_MS = 30000;

// ============================================================================
// State Machine
// ============================================================================

type AuthStatus = 'idle' | 'loading' | 'guest_loading' | 'mfa_required' | 'success' | 'error';

interface State {
  status: AuthStatus;
  email: string;
  password: string;
  mfaCode: string;
  showPassword: boolean;
  error: string | null;
}

type Action =
  | { type: 'SET_EMAIL'; payload: string }
  | { type: 'SET_PASSWORD'; payload: string }
  | { type: 'SET_MFA_CODE'; payload: string }
  | { type: 'TOGGLE_PASSWORD' }
  | { type: 'START_LOGIN' }
  | { type: 'START_GUEST' }
  | { type: 'MFA_REQUIRED' }
  | { type: 'SUCCESS' }
  | { type: 'ERROR'; payload: string }
  | { type: 'CLEAR_ERROR' }
  | { type: 'BACK_TO_LOGIN' };

const initialState: State = {
  status: 'idle',
  email: '',
  password: '',
  mfaCode: '',
  showPassword: false,
  error: null,
};

function reducer(state: State, action: Action): State {
  switch (action.type) {
    case 'SET_EMAIL':
      return { ...state, email: action.payload, error: null };
    case 'SET_PASSWORD':
      return { ...state, password: action.payload, error: null };
    case 'SET_MFA_CODE':
      return { ...state, mfaCode: action.payload, error: null };
    case 'TOGGLE_PASSWORD':
      return { ...state, showPassword: !state.showPassword };
    case 'START_LOGIN':
      return { ...state, status: 'loading', error: null };
    case 'START_GUEST':
      return { ...state, status: 'guest_loading', error: null };
    case 'MFA_REQUIRED':
      return { ...state, status: 'mfa_required', error: null };
    case 'SUCCESS':
      return { ...state, status: 'success', error: null };
    case 'ERROR':
      return { ...state, status: state.status === 'mfa_required' ? 'mfa_required' : 'idle', error: action.payload };
    case 'CLEAR_ERROR':
      return { ...state, error: null };
    case 'BACK_TO_LOGIN':
      return { ...state, status: 'idle', mfaCode: '', error: null };
    default:
      return state;
  }
}

// ============================================================================
// Component
// ============================================================================

interface LoginScreenProps {
  onNavigate: (screen: Screen) => void;
}

const LoginScreen: React.FC<LoginScreenProps> = ({ onNavigate }) => {
  const { login, setupGuestAccess, verifyMfaAndLogin } = useAuth();
  const { t } = useTranslation('auth');

  const [state, dispatch] = useReducer(reducer, initialState);
  const mountedRef = useRef(true);
  const abortControllerRef = useRef<AbortController | null>(null);

  // Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
      abortControllerRef.current?.abort();
    };
  }, []);

  const isLoading = state.status === 'loading';
  const isGuestLoading = state.status === 'guest_loading';
  const isAnyLoading = isLoading || isGuestLoading;

  // Map API errors to user-friendly messages
  const mapErrorMessage = useCallback((errorMessage: string): string => {
    const msg = errorMessage.toLowerCase();
    if (msg.includes('invalid email or password') || msg.includes('invalid credentials') || msg.includes('check your credentials')) {
      return t('login.errors.invalidCredentials');
    }
    if (msg.includes('account not found') || msg.includes('user not found') || msg.includes('user does not exist')) {
      return t('login.errors.accountNotFound');
    }
    if (msg.includes('password')) {
      return t('login.errors.incorrectPassword');
    }
    if (msg.includes('server error')) {
      return t('login.errors.serverError');
    }
    if (msg.includes('unable to connect') || msg.includes('timeout')) {
      return t('login.errors.connectionError');
    }
    return errorMessage;
  }, [t]);

  const handleLogin = useCallback(async (e: React.FormEvent) => {
    e.preventDefault();

    // Prevent duplicate submissions
    if (state.status === 'loading') return;

    // Validate email format
    const emailValidation = validateEmail(state.email);
    if (!emailValidation.valid) {
      dispatch({ type: 'ERROR', payload: emailValidation.error || t('login.errors.emailPassword') });
      return;
    }

    if (!state.password) {
      dispatch({ type: 'ERROR', payload: t('login.errors.emailPassword') });
      return;
    }

    // Abort previous request
    abortControllerRef.current?.abort();
    abortControllerRef.current = new AbortController();

    dispatch({ type: 'START_LOGIN' });

    try {
      // Sanitize inputs
      const sanitizedEmail = sanitizeInput(state.email.trim().toLowerCase());

      const result = await withTimeout(
        login(sanitizedEmail, state.password),
        AUTH_TIMEOUT_MS,
        'Login request timed out'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'SUCCESS' });
      } else if (result.mfa_required) {
        dispatch({ type: 'MFA_REQUIRED' });
      } else {
        const errorMessage = mapErrorMessage(result.error || 'Login failed. Please check your credentials.');
        dispatch({ type: 'ERROR', payload: errorMessage });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      const message = err instanceof Error ? err.message : t('login.errors.unexpectedError');
      dispatch({ type: 'ERROR', payload: mapErrorMessage(message) });
    }
  }, [state.status, state.email, state.password, login, t, mapErrorMessage]);

  const handleMfaVerify = useCallback(async (e: React.FormEvent) => {
    e.preventDefault();

    // Prevent duplicate submissions
    if (state.status === 'loading') return;

    if (!state.mfaCode) {
      dispatch({ type: 'ERROR', payload: t('login.errors.mfaCodeRequired') });
      return;
    }

    // Abort previous request
    abortControllerRef.current?.abort();
    abortControllerRef.current = new AbortController();

    dispatch({ type: 'START_LOGIN' });

    try {
      const sanitizedEmail = sanitizeInput(state.email.trim().toLowerCase());
      const sanitizedCode = sanitizeInput(state.mfaCode.trim());

      const result = await withTimeout(
        verifyMfaAndLogin(sanitizedEmail, sanitizedCode),
        AUTH_TIMEOUT_MS,
        'MFA verification timed out'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'SUCCESS' });
      } else {
        dispatch({ type: 'ERROR', payload: result.error || t('login.errors.mfaInvalid') });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatch({ type: 'ERROR', payload: t('login.errors.mfaFailed') });
    }
  }, [state.status, state.email, state.mfaCode, verifyMfaAndLogin, t]);

  const handleGuestAccess = useCallback(async () => {
    // Prevent duplicate submissions
    if (state.status === 'guest_loading') return;

    // Abort previous request
    abortControllerRef.current?.abort();
    abortControllerRef.current = new AbortController();

    dispatch({ type: 'START_GUEST' });

    try {
      const result = await withTimeout(
        setupGuestAccess(),
        AUTH_TIMEOUT_MS,
        'Guest setup timed out'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'SUCCESS' });
      } else {
        dispatch({ type: 'ERROR', payload: result.error || t('login.errors.guestSetupFailed') });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatch({ type: 'ERROR', payload: t('login.errors.guestSetupFailed') });
    }
  }, [state.status, setupGuestAccess, t]);

  // MFA Verification Screen
  if (state.status === 'mfa_required' || (state.status === 'loading' && state.mfaCode)) {
    return (
      <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
        {/* Language Selector at Top */}
        <div className="mb-4">
          <CompactLanguageSelector />
        </div>

        <div className="mb-6">
          <div className="flex items-center gap-2 mb-3">
            <Shield className="h-6 w-6 text-blue-400" />
            <h2 className="text-white text-2xl font-light">{t('login.mfa.title')}</h2>
          </div>
          <p className="text-zinc-400 text-xs leading-5">
            {t('login.mfa.subtitle')}
          </p>
        </div>

        <form onSubmit={handleMfaVerify} className="space-y-4">
          <div className="space-y-1">
            <Label htmlFor="mfaCode" className="text-white text-xs">
              {t('login.mfa.codeLabel')}
            </Label>
            <Input
              id="mfaCode"
              type="text"
              placeholder={t('login.mfa.codePlaceholder')}
              value={state.mfaCode}
              onChange={(e) => dispatch({ type: 'SET_MFA_CODE', payload: e.target.value })}
              className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500 text-center tracking-widest text-lg"
              disabled={isLoading}
              maxLength={6}
              autoComplete="one-time-code"
              required
            />
          </div>

          {state.error && (
            <div className="text-red-400 text-xs bg-red-900/20 border border-red-900/50 rounded p-2">
              {state.error}
            </div>
          )}

          <div className="flex flex-col gap-2 pt-1">
            <Button
              type="submit"
              className="w-full bg-blue-600 hover:bg-blue-700 text-white py-2 text-sm font-normal transition-colors disabled:opacity-50"
              disabled={isLoading}
            >
              {isLoading ? (
                <div className="flex items-center justify-center">
                  <div className="w-3 h-3 border border-white/30 border-t-white rounded-full animate-spin mr-1.5"></div>
                  {t('login.mfa.verifying')}
                </div>
              ) : t('login.mfa.verifyButton')}
            </Button>

            <button
              type="button"
              onClick={() => dispatch({ type: 'BACK_TO_LOGIN' })}
              className="text-zinc-400 hover:text-zinc-300 text-xs transition-colors"
              disabled={isLoading}
            >
              {t('login.mfa.backToLogin')}
            </button>
          </div>
        </form>
      </div>
    );
  }

  // Regular Login Screen
  return (
    <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
      {/* Language Selector at Top */}
      <div className="mb-4">
        <CompactLanguageSelector />
      </div>

      <div className="mb-6">
        <h2 className="text-white text-2xl font-light mb-3">{t('login.title')}</h2>
        <p className="text-zinc-400 text-xs leading-5">
          {t('login.subtitle')}
        </p>
      </div>

      <form onSubmit={handleLogin} className="space-y-4">
        <div className="space-y-1">
          <Label htmlFor="email" className="text-white text-xs">
            {t('login.emailLabel')}
          </Label>
          <div className="relative">
            <Mail className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
            <Input
              id="email"
              type="email"
              placeholder={t('login.emailPlaceholder')}
              value={state.email}
              onChange={(e) => dispatch({ type: 'SET_EMAIL', payload: e.target.value })}
              className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
              disabled={isAnyLoading}
              required
            />
          </div>
        </div>

        <div className="space-y-1">
          <Label htmlFor="password" className="text-white text-xs">
            {t('login.passwordLabel')}
          </Label>
          <div className="relative">
            <Lock className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
            <Input
              id="password"
              type={state.showPassword ? "text" : "password"}
              placeholder={t('login.passwordPlaceholder')}
              value={state.password}
              onChange={(e) => dispatch({ type: 'SET_PASSWORD', payload: e.target.value })}
              className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 pr-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500 [&::-ms-reveal]:hidden [&::-ms-clear]:hidden"
              style={{ WebkitTextSecurity: state.showPassword ? 'none' : undefined } as any}
              autoComplete="current-password"
              disabled={isAnyLoading}
              required
            />
            <button
              type="button"
              onClick={() => dispatch({ type: 'TOGGLE_PASSWORD' })}
              className="absolute right-2.5 top-1/2 transform -translate-y-1/2 text-zinc-400 hover:text-zinc-300"
              disabled={isAnyLoading}
            >
              {state.showPassword ? <EyeOff className="h-3.5 w-3.5" /> : <Eye className="h-3.5 w-3.5" />}
            </button>
          </div>
        </div>

        {state.error && (
          <div className="text-red-400 text-xs bg-red-900/20 border border-red-900/50 rounded p-2">
            {state.error}
          </div>
        )}

        <div className="flex justify-between items-center text-xs pt-1">
          <button
            type="button"
            onClick={() => onNavigate('forgotPassword')}
            className="text-blue-400 hover:text-blue-300 transition-colors"
            disabled={isAnyLoading}
          >
            {t('login.forgotPassword')}
          </button>
          <Button
            type="submit"
            className="bg-zinc-700 hover:bg-zinc-600 text-white px-6 py-1.5 text-sm font-normal transition-colors disabled:opacity-50"
            disabled={isAnyLoading}
          >
            {isLoading ? (
              <div className="flex items-center">
                <div className="w-3 h-3 border border-white/30 border-t-white rounded-full animate-spin mr-1.5"></div>
                {t('login.signingIn')}
              </div>
            ) : t('login.loginButton')}
          </Button>
        </div>
      </form>

      <div className="mt-5 pt-4 border-t border-zinc-700 space-y-3">
        <Button
          onClick={handleGuestAccess}
          className="w-full bg-blue-600/20 hover:bg-blue-600/30 border border-blue-600/50 text-blue-400 py-2 text-sm font-normal transition-colors disabled:opacity-50"
          disabled={isAnyLoading}
        >
          {isGuestLoading ? (
            <div className="flex items-center justify-center">
              <div className="w-3 h-3 border border-blue-400/30 border-t-blue-400 rounded-full animate-spin mr-1.5"></div>
              {t('login.guestSettingUp')}
            </div>
          ) : (
            <>
              <svg className="w-4 h-4 mr-2" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 15v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2zm10-10V7a4 4 0 00-8 0v4h8z" />
              </svg>
              {t('login.guestButton')}
            </>
          )}
        </Button>

        <div className="text-center">
          <p className="text-zinc-400 text-xs">
            {t('login.noAccount')}{" "}
            <button
              type="button"
              onClick={() => onNavigate('register')}
              className="text-blue-400 hover:text-blue-300 transition-colors"
              disabled={isAnyLoading}
            >
              {t('login.signUp')}
            </button>
          </p>
        </div>

        <div className="text-center text-zinc-500 text-xs">
          <p>{t('login.guestInfo')}</p>
        </div>
      </div>
    </div>
  );
};

export default LoginScreen;
