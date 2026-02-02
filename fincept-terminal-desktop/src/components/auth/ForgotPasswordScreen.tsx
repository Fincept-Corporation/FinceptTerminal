// File: src/components/auth/ForgotPasswordScreen.tsx
// Complete password reset flow with email submission, OTP verification, and password reset
// Production-ready: State machine, cleanup, timeout, validation

import React, { useReducer, useEffect, useRef, useCallback } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { ArrowLeft, Mail, Key, Eye, EyeOff } from "lucide-react";
import { Screen } from '../../App';
import { AuthApiService } from '@/services/auth/authApi';
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

type ResetStep = 'email' | 'otp-sent' | 'reset-form' | 'success';
type ResetStatus = 'idle' | 'loading';

interface State {
  step: ResetStep;
  status: ResetStatus;
  email: string;
  otp: string;
  newPassword: string;
  confirmPassword: string;
  showPassword: boolean;
  error: string | null;
}

type Action =
  | { type: 'SET_EMAIL'; value: string }
  | { type: 'SET_OTP'; value: string }
  | { type: 'SET_NEW_PASSWORD'; value: string }
  | { type: 'SET_CONFIRM_PASSWORD'; value: string }
  | { type: 'TOGGLE_PASSWORD' }
  | { type: 'SET_STEP'; step: ResetStep }
  | { type: 'START_LOADING' }
  | { type: 'STOP_LOADING' }
  | { type: 'SET_ERROR'; error: string }
  | { type: 'CLEAR_ERROR' };

const initialState: State = {
  step: 'email',
  status: 'idle',
  email: '',
  otp: '',
  newPassword: '',
  confirmPassword: '',
  showPassword: false,
  error: null,
};

function reducer(state: State, action: Action): State {
  switch (action.type) {
    case 'SET_EMAIL':
      return { ...state, email: action.value, error: null };
    case 'SET_OTP':
      return { ...state, otp: action.value, error: null };
    case 'SET_NEW_PASSWORD':
      return { ...state, newPassword: action.value, error: null };
    case 'SET_CONFIRM_PASSWORD':
      return { ...state, confirmPassword: action.value, error: null };
    case 'TOGGLE_PASSWORD':
      return { ...state, showPassword: !state.showPassword };
    case 'SET_STEP':
      return { ...state, step: action.step, error: null };
    case 'START_LOADING':
      return { ...state, status: 'loading', error: null };
    case 'STOP_LOADING':
      return { ...state, status: 'idle' };
    case 'SET_ERROR':
      return { ...state, status: 'idle', error: action.error };
    case 'CLEAR_ERROR':
      return { ...state, error: null };
    default:
      return state;
  }
}

// ============================================================================
// Component
// ============================================================================

interface ForgotPasswordScreenProps {
  onNavigate: (screen: Screen) => void;
}

const ForgotPasswordScreen: React.FC<ForgotPasswordScreenProps> = ({ onNavigate }) => {
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

  const handleEmailSubmit = useCallback(async (e: React.FormEvent) => {
    e.preventDefault();

    // Prevent duplicate submissions
    if (state.status === 'loading') return;

    if (!state.email) {
      dispatch({ type: 'SET_ERROR', error: 'Please enter your email address' });
      return;
    }

    // Validate email
    const emailResult = validateEmail(state.email);
    if (!emailResult.valid) {
      dispatch({ type: 'SET_ERROR', error: emailResult.error || 'Invalid email format' });
      return;
    }

    // Abort previous request
    abortControllerRef.current?.abort();
    abortControllerRef.current = new AbortController();

    dispatch({ type: 'START_LOADING' });

    try {
      const sanitizedEmail = sanitizeInput(state.email.trim().toLowerCase());

      const result = await withTimeout(
        AuthApiService.forgotPassword({ email: sanitizedEmail }),
        AUTH_TIMEOUT_MS,
        'Request timed out'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'SET_STEP', step: 'otp-sent' });
        dispatch({ type: 'STOP_LOADING' });
      } else {
        dispatch({ type: 'SET_ERROR', error: result.error || 'Failed to send reset code. Please try again.' });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatch({ type: 'SET_ERROR', error: 'An unexpected error occurred. Please try again.' });
    }
  }, [state.status, state.email]);

  const handleContinueToReset = useCallback(() => {
    dispatch({ type: 'SET_STEP', step: 'reset-form' });
  }, []);

  const handlePasswordReset = useCallback(async (e: React.FormEvent) => {
    e.preventDefault();

    // Prevent duplicate submissions
    if (state.status === 'loading') return;

    if (!state.otp) {
      dispatch({ type: 'SET_ERROR', error: 'Please enter the verification code' });
      return;
    }

    if (!state.newPassword) {
      dispatch({ type: 'SET_ERROR', error: 'Please enter a new password' });
      return;
    }

    if (state.newPassword.length < 8) {
      dispatch({ type: 'SET_ERROR', error: 'Password must be at least 8 characters long' });
      return;
    }

    if (state.newPassword !== state.confirmPassword) {
      dispatch({ type: 'SET_ERROR', error: 'Passwords do not match' });
      return;
    }

    // Abort previous request
    abortControllerRef.current?.abort();
    abortControllerRef.current = new AbortController();

    dispatch({ type: 'START_LOADING' });

    try {
      const sanitizedEmail = sanitizeInput(state.email.trim().toLowerCase());
      const sanitizedOtp = sanitizeInput(state.otp.trim());

      const result = await withTimeout(
        AuthApiService.resetPassword({
          email: sanitizedEmail,
          otp: sanitizedOtp,
          new_password: state.newPassword,
        }),
        AUTH_TIMEOUT_MS,
        'Request timed out'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'SET_STEP', step: 'success' });
        dispatch({ type: 'STOP_LOADING' });
      } else {
        dispatch({ type: 'SET_ERROR', error: result.error || 'Failed to reset password. Please check your code and try again.' });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatch({ type: 'SET_ERROR', error: 'An unexpected error occurred. Please try again.' });
    }
  }, [state.status, state.email, state.otp, state.newPassword, state.confirmPassword]);

  const handleResendOtp = useCallback(async () => {
    if (state.status === 'loading') return;

    dispatch({ type: 'START_LOADING' });

    try {
      const sanitizedEmail = sanitizeInput(state.email.trim().toLowerCase());

      const result = await withTimeout(
        AuthApiService.forgotPassword({ email: sanitizedEmail }),
        AUTH_TIMEOUT_MS,
        'Request timed out'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'CLEAR_ERROR' });
        dispatch({ type: 'STOP_LOADING' });
      } else {
        dispatch({ type: 'SET_ERROR', error: result.error || 'Failed to resend code. Please try again.' });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatch({ type: 'SET_ERROR', error: 'Failed to resend code. Please try again.' });
    }
  }, [state.status, state.email]);

  // Destructure for easier access
  const { step, email, otp, newPassword, confirmPassword, showPassword, error } = state;

  // Step 1: Email Input
  if (step === 'email') {
    return (
      <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
        {/* Language Selector at Top */}
        <div className="mb-4">
          <CompactLanguageSelector />
        </div>

        <div className="mb-6">
          <div className="flex items-center mb-3">
            <button
              onClick={() => onNavigate('login')}
              className="text-zinc-400 hover:text-white transition-colors mr-3"
            >
              <ArrowLeft className="h-4 w-4" />
            </button>
            <h2 className="text-white text-2xl font-light">Reset Password</h2>
          </div>
          <p className="text-zinc-400 text-xs leading-5">
            Enter your email address and we'll send you a verification code to reset your password.
          </p>
        </div>

        <form onSubmit={handleEmailSubmit} className="space-y-4">
          <div className="space-y-1">
            <Label htmlFor="resetEmail" className="text-white text-xs">
              Email Address
            </Label>
            <div className="relative">
              <Mail className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
              <Input
                id="resetEmail"
                type="email"
                placeholder="Enter your email"
                value={email}
                onChange={(e) => dispatch({ type: 'SET_EMAIL', value: e.target.value })}
                className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                disabled={isLoading}
                required
              />
            </div>
          </div>

          {error && (
            <div className="text-red-400 text-xs bg-red-900/20 border border-red-900/50 rounded p-2">
              {error}
            </div>
          )}

          <div className="flex justify-end pt-2">
            <Button
              type="submit"
              className="bg-zinc-700 hover:bg-zinc-600 text-white px-6 py-1.5 text-sm font-normal transition-colors disabled:opacity-50"
              disabled={isLoading}
            >
              {isLoading ? (
                <div className="flex items-center">
                  <div className="w-3 h-3 border border-white/30 border-t-white rounded-full animate-spin mr-1.5"></div>
                  Sending...
                </div>
              ) : "Send Code"}
            </Button>
          </div>
        </form>

        <div className="mt-5 pt-4 border-t border-zinc-700 text-center">
          <p className="text-zinc-400 text-xs">
            Remember your password?{" "}
            <button
              type="button"
              onClick={() => onNavigate('login')}
              className="text-blue-400 hover:text-blue-300 transition-colors"
              disabled={isLoading}
            >
              Sign In
            </button>
          </p>
        </div>
      </div>
    );
  }

  // Step 2: OTP Sent Confirmation
  if (step === 'otp-sent') {
    return (
      <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
        <div className="text-center mb-6">
          <div className="mb-4">
            <div className="w-12 h-12 bg-blue-600/20 rounded-full flex items-center justify-center mx-auto mb-4">
              <Mail className="w-6 h-6 text-blue-400" />
            </div>
            <h2 className="text-white text-2xl font-light mb-3">Check Your Email</h2>
            <p className="text-zinc-400 text-xs leading-5">
              We've sent a verification code to <span className="text-white">{email}</span>.
              Enter the code below to reset your password.
            </p>
          </div>
        </div>

        <div className="space-y-4">
          <Button
            onClick={handleContinueToReset}
            className="bg-zinc-700 hover:bg-zinc-600 text-white px-6 py-1.5 text-sm font-normal transition-colors w-full"
          >
            I Have the Code
          </Button>

          <div className="text-center space-y-2">
            <button
              onClick={handleResendOtp}
              className="text-blue-400 hover:text-blue-300 text-xs transition-colors"
              disabled={isLoading}
            >
              {isLoading ? "Sending..." : "Didn't receive the code? Resend"}
            </button>
            <br />
            <button
              onClick={() => dispatch({ type: 'SET_STEP', step: 'email' })}
              className="text-zinc-400 hover:text-zinc-300 text-xs transition-colors"
            >
              Use a different email
            </button>
          </div>
        </div>

        {error && (
          <div className="text-red-400 text-xs bg-red-900/20 border border-red-900/50 rounded p-2 mt-4">
            {error}
          </div>
        )}
      </div>
    );
  }

  // Step 3: Reset Form (OTP + New Password)
  if (step === 'reset-form') {
    return (
      <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
        <div className="mb-6">
          <div className="flex items-center mb-3">
            <button
              onClick={() => dispatch({ type: 'SET_STEP', step: 'otp-sent' })}
              className="text-zinc-400 hover:text-white transition-colors mr-3"
            >
              <ArrowLeft className="h-4 w-4" />
            </button>
            <h2 className="text-white text-2xl font-light">Reset Password</h2>
          </div>
          <p className="text-zinc-400 text-xs leading-5">
            Enter the verification code and your new password.
          </p>
        </div>

        <form onSubmit={handlePasswordReset} className="space-y-4">
          <div className="space-y-1">
            <Label htmlFor="otp" className="text-white text-xs">
              Verification Code
            </Label>
            <div className="relative">
              <Key className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
              <Input
                id="otp"
                type="text"
                placeholder="Enter code from email"
                value={otp}
                onChange={(e) => dispatch({ type: 'SET_OTP', value: e.target.value })}
                className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                disabled={isLoading}
                required
              />
            </div>
          </div>

          <div className="space-y-1">
            <Label htmlFor="newPassword" className="text-white text-xs">
              New Password
            </Label>
            <div className="relative">
              <Input
                id="newPassword"
                type={showPassword ? "text" : "password"}
                placeholder="Enter new password"
                value={newPassword}
                onChange={(e) => dispatch({ type: 'SET_NEW_PASSWORD', value: e.target.value })}
                className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pr-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                disabled={isLoading}
                required
                minLength={8}
              />
              <button
                type="button"
                onClick={() => dispatch({ type: 'TOGGLE_PASSWORD' })}
                className="absolute right-2.5 top-1/2 transform -translate-y-1/2 text-zinc-400 hover:text-zinc-300"
              >
                {showPassword ? <EyeOff className="h-3.5 w-3.5" /> : <Eye className="h-3.5 w-3.5" />}
              </button>
            </div>
          </div>

          <div className="space-y-1">
            <Label htmlFor="confirmPassword" className="text-white text-xs">
              Confirm Password
            </Label>
            <Input
              id="confirmPassword"
              type={showPassword ? "text" : "password"}
              placeholder="Confirm new password"
              value={confirmPassword}
              onChange={(e) => dispatch({ type: 'SET_CONFIRM_PASSWORD', value: e.target.value })}
              className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
              disabled={isLoading}
              required
            />
          </div>

          {error && (
            <div className="text-red-400 text-xs bg-red-900/20 border border-red-900/50 rounded p-2">
              {error}
            </div>
          )}

          <div className="flex justify-end pt-2">
            <Button
              type="submit"
              className="bg-zinc-700 hover:bg-zinc-600 text-white px-6 py-1.5 text-sm font-normal transition-colors disabled:opacity-50"
              disabled={isLoading}
            >
              {isLoading ? (
                <div className="flex items-center">
                  <div className="w-3 h-3 border border-white/30 border-t-white rounded-full animate-spin mr-1.5"></div>
                  Resetting...
                </div>
              ) : "Reset Password"}
            </Button>
          </div>
        </form>
      </div>
    );
  }

  // Step 4: Success
  if (step === 'success') {
    return (
      <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
        <div className="text-center mb-6">
          <div className="mb-4">
            <div className="w-12 h-12 bg-green-600/20 rounded-full flex items-center justify-center mx-auto mb-4">
              <svg className="w-6 h-6 text-green-400" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
              </svg>
            </div>
            <h2 className="text-white text-2xl font-light mb-3">Password Reset</h2>
            <p className="text-zinc-400 text-xs leading-5">
              Your password has been successfully reset. You can now sign in with your new password.
            </p>
          </div>
        </div>

        <div className="space-y-4">
          <Button
            onClick={() => onNavigate('login')}
            className="bg-zinc-700 hover:bg-zinc-600 text-white px-6 py-1.5 text-sm font-normal transition-colors w-full"
          >
            Continue to Login
          </Button>
        </div>
      </div>
    );
  }

  return null;
};

export default ForgotPasswordScreen;