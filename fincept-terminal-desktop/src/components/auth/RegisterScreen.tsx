// File: src/components/auth/RegisterScreen.tsx
// User registration screen with country code selector and complete form validation
// Production-ready: State machine, cleanup, timeout, validation

import React, { useReducer, useEffect, useRef, useCallback } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { ArrowLeft, User, Mail, Lock, Key, Check, X } from "lucide-react";
import { Screen } from '../../App';
import { useAuth } from '@/contexts/AuthContext';
import { useTranslation } from 'react-i18next';
import PhoneInput from 'react-phone-number-input';
import 'react-phone-number-input/style.css';
import './phone-input.css';
import { CompactLanguageSelector } from './CompactLanguageSelector';
import { withTimeout } from '@/services/core/apiUtils';
import { validateEmail, sanitizeInput } from '@/services/core/validators';

// ============================================================================
// Constants
// ============================================================================

const AUTH_TIMEOUT_MS = 30000;

// ============================================================================
// Types & State Machine
// ============================================================================

type RegisterStep = 'form' | 'otp-verification';
type RegisterStatus = 'idle' | 'loading' | 'success';

interface FormData {
  firstName: string;
  lastName: string;
  email: string;
  password: string;
  confirmPassword: string;
}

interface PasswordValidation {
  minLength: boolean;
  hasUpperCase: boolean;
  hasLowerCase: boolean;
  hasNumber: boolean;
  hasSpecialChar: boolean;
}

interface State {
  step: RegisterStep;
  status: RegisterStatus;
  formData: FormData;
  phoneNumber: string | undefined;
  otp: string;
  error: string | null;
  isVerificationComplete: boolean;
  passwordValidation: PasswordValidation;
  usernameValidation: { isValid: boolean; length: number; message: string };
  emailValidation: { isValid: boolean; message: string };
  phoneValidation: { isValid: boolean; message: string };
}

type Action =
  | { type: 'SET_FORM_FIELD'; field: keyof FormData; value: string }
  | { type: 'SET_PHONE'; value: string | undefined }
  | { type: 'SET_OTP'; value: string }
  | { type: 'SET_STEP'; step: RegisterStep }
  | { type: 'START_LOADING' }
  | { type: 'STOP_LOADING' }
  | { type: 'SET_ERROR'; error: string }
  | { type: 'CLEAR_ERROR' }
  | { type: 'VERIFICATION_COMPLETE' }
  | { type: 'SET_PASSWORD_VALIDATION'; validation: PasswordValidation }
  | { type: 'SET_USERNAME_VALIDATION'; validation: { isValid: boolean; length: number; message: string } }
  | { type: 'SET_EMAIL_VALIDATION'; validation: { isValid: boolean; message: string } }
  | { type: 'SET_PHONE_VALIDATION'; validation: { isValid: boolean; message: string } };

const initialState: State = {
  step: 'form',
  status: 'idle',
  formData: { firstName: '', lastName: '', email: '', password: '', confirmPassword: '' },
  phoneNumber: undefined,
  otp: '',
  error: null,
  isVerificationComplete: false,
  passwordValidation: { minLength: false, hasUpperCase: false, hasLowerCase: false, hasNumber: false, hasSpecialChar: false },
  usernameValidation: { isValid: false, length: 0, message: '' },
  emailValidation: { isValid: false, message: '' },
  phoneValidation: { isValid: false, message: '' },
};

function reducer(state: State, action: Action): State {
  switch (action.type) {
    case 'SET_FORM_FIELD':
      return { ...state, formData: { ...state.formData, [action.field]: action.value }, error: null };
    case 'SET_PHONE':
      return { ...state, phoneNumber: action.value, error: null };
    case 'SET_OTP':
      return { ...state, otp: action.value, error: null };
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
    case 'VERIFICATION_COMPLETE':
      return { ...state, status: 'success', isVerificationComplete: true };
    case 'SET_PASSWORD_VALIDATION':
      return { ...state, passwordValidation: action.validation };
    case 'SET_USERNAME_VALIDATION':
      return { ...state, usernameValidation: action.validation };
    case 'SET_EMAIL_VALIDATION':
      return { ...state, emailValidation: action.validation };
    case 'SET_PHONE_VALIDATION':
      return { ...state, phoneValidation: action.validation };
    default:
      return state;
  }
}

// ============================================================================
// Component
// ============================================================================

interface RegisterScreenProps {
  onNavigate: (screen: Screen) => void;
}

const RegisterScreen: React.FC<RegisterScreenProps> = ({ onNavigate }) => {
  const { signup, verifyOtp, session } = useAuth();
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

  // Handle navigation after successful authentication
  useEffect(() => {
    if (session?.authenticated && session?.user_type === 'registered' && state.isVerificationComplete) {
      console.log('Registration complete, navigating based on account type:', session.user_info?.account_type);

      if (session.subscription?.has_subscription) {
        console.log('User has subscription, redirecting to dashboard');
        onNavigate('dashboard' as Screen);
      } else {
        console.log('User is free tier, showing pricing screen');
        onNavigate('pricing' as Screen);
      }
    }
  }, [session, state.isVerificationComplete, onNavigate]);

  const handleChange = useCallback((field: keyof FormData, value: string) => {
    dispatch({ type: 'SET_FORM_FIELD', field, value });

    // Real-time password validation
    if (field === 'password') {
      dispatch({
        type: 'SET_PASSWORD_VALIDATION',
        validation: {
          minLength: value.length >= 8,
          hasUpperCase: /[A-Z]/.test(value),
          hasLowerCase: /[a-z]/.test(value),
          hasNumber: /[0-9]/.test(value),
          hasSpecialChar: /[!@#$%^&*(),.?":{}|<>]/.test(value),
        },
      });
    }

    // Real-time email validation using shared validator
    if (field === 'email') {
      const result = validateEmail(value);
      dispatch({
        type: 'SET_EMAIL_VALIDATION',
        validation: {
          isValid: result.valid || value === '',
          message: value && !result.valid ? (result.error || 'Invalid email format') : '',
        },
      });
    }
  }, []);

  // Update username validation when names change
  useEffect(() => {
    const { firstName, lastName } = state.formData;
    const username = `${firstName.trim()}${lastName.trim()}`.toLowerCase();
    const length = username.length;

    if (firstName || lastName) {
      if (length < 3) {
        dispatch({ type: 'SET_USERNAME_VALIDATION', validation: { isValid: false, length, message: `Username too short (${length}/3 min)` } });
      } else if (length > 50) {
        dispatch({ type: 'SET_USERNAME_VALIDATION', validation: { isValid: false, length, message: `Username too long (${length}/50 max)` } });
      } else {
        dispatch({ type: 'SET_USERNAME_VALIDATION', validation: { isValid: true, length, message: `Username valid (${length} characters)` } });
      }
    } else {
      dispatch({ type: 'SET_USERNAME_VALIDATION', validation: { isValid: false, length: 0, message: '' } });
    }
  }, [state.formData.firstName, state.formData.lastName]);

  const handlePhoneChange = useCallback((value: string | undefined) => {
    dispatch({ type: 'SET_PHONE', value });

    if (value) {
      const digitsOnly = value.replace(/\D/g, '');
      if (digitsOnly.length < 7) {
        dispatch({ type: 'SET_PHONE_VALIDATION', validation: { isValid: false, message: 'Phone number too short' } });
      } else if (digitsOnly.length > 15) {
        dispatch({ type: 'SET_PHONE_VALIDATION', validation: { isValid: false, message: 'Phone number too long' } });
      } else {
        dispatch({ type: 'SET_PHONE_VALIDATION', validation: { isValid: true, message: 'Valid phone number' } });
      }
    } else {
      dispatch({ type: 'SET_PHONE_VALIDATION', validation: { isValid: false, message: 'Phone number required' } });
    }
  }, []);

  const handleRegister = useCallback(async (e: React.FormEvent) => {
    e.preventDefault();

    // Prevent duplicate submissions
    if (state.status === 'loading') return;

    const { formData, phoneNumber } = state;

    if (!formData.firstName || !formData.lastName || !formData.email || !formData.password || !formData.confirmPassword) {
      dispatch({ type: 'SET_ERROR', error: t('register.errors.requiredFields') });
      return;
    }

    // Validate email using shared validator
    const emailResult = validateEmail(formData.email);
    if (!emailResult.valid) {
      dispatch({ type: 'SET_ERROR', error: emailResult.error || 'Invalid email format' });
      return;
    }

    if (!phoneNumber) {
      dispatch({ type: 'SET_ERROR', error: 'Phone number is required' });
      return;
    }

    if (formData.password !== formData.confirmPassword) {
      dispatch({ type: 'SET_ERROR', error: t('register.errors.passwordMismatch') });
      return;
    }

    if (formData.password.length < 8) {
      dispatch({ type: 'SET_ERROR', error: t('register.errors.passwordLength') });
      return;
    }

    // Abort previous request
    abortControllerRef.current?.abort();
    abortControllerRef.current = new AbortController();

    dispatch({ type: 'START_LOADING' });

    try {
      // Sanitize inputs
      const sanitizedEmail = sanitizeInput(formData.email.trim().toLowerCase());
      const username = sanitizeInput(`${formData.firstName.trim()}${formData.lastName.trim()}`).toLowerCase();

      if (username.length < 3 || username.length > 50) {
        dispatch({ type: 'SET_ERROR', error: 'Username must be between 3 and 50 characters.' });
        return;
      }

      // Parse phone number
      let phone = '';
      let country = '';
      let countryCode = '';

      if (phoneNumber) {
        try {
          const { parsePhoneNumber } = await import('libphonenumber-js');
          const parsed = parsePhoneNumber(phoneNumber);
          phone = parsed.nationalNumber;
          country = parsed.country || '';
          countryCode = `+${parsed.countryCallingCode}`;
        } catch (err) {
          console.error('Failed to parse phone number:', err);
        }
      }

      const result = await withTimeout(
        signup(username, sanitizedEmail, formData.password, phone, country, countryCode),
        AUTH_TIMEOUT_MS,
        'Registration request timed out'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'SET_STEP', step: 'otp-verification' });
        dispatch({ type: 'STOP_LOADING' });
      } else {
        const errorMessage = result.error || 'Registration failed. Please try again.';
        if (errorMessage.toLowerCase().includes('already exists') || errorMessage.toLowerCase().includes('already registered')) {
          dispatch({ type: 'SET_ERROR', error: t('register.errors.alreadyExists') });
        } else {
          dispatch({ type: 'SET_ERROR', error: errorMessage });
        }
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatch({ type: 'SET_ERROR', error: t('register.errors.unexpectedError') });
    }
  }, [state.status, state.formData, state.phoneNumber, signup, t]);

  const handleVerifyOtp = useCallback(async (e: React.FormEvent) => {
    e.preventDefault();

    // Prevent duplicate submissions
    if (state.status === 'loading' || state.isVerificationComplete) return;

    if (!state.otp) {
      dispatch({ type: 'SET_ERROR', error: t('register.errors.verificationCode') });
      return;
    }

    // Abort previous request
    abortControllerRef.current?.abort();
    abortControllerRef.current = new AbortController();

    dispatch({ type: 'START_LOADING' });

    try {
      const sanitizedEmail = sanitizeInput(state.formData.email.trim().toLowerCase());
      const sanitizedOtp = sanitizeInput(state.otp.trim());

      const result = await withTimeout(
        verifyOtp(sanitizedEmail, sanitizedOtp),
        AUTH_TIMEOUT_MS,
        'Verification request timed out'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'VERIFICATION_COMPLETE' });
      } else {
        dispatch({ type: 'SET_ERROR', error: result.error || t('register.errors.verificationFailed') });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatch({ type: 'SET_ERROR', error: t('register.errors.unexpectedError') });
    }
  }, [state.status, state.isVerificationComplete, state.otp, state.formData.email, verifyOtp, t]);

  const handleResendOtp = useCallback(async () => {
    if (state.status === 'loading') return;

    dispatch({ type: 'START_LOADING' });

    try {
      const { formData, phoneNumber } = state;
      const username = sanitizeInput(`${formData.firstName.trim()}${formData.lastName.trim()}`).toLowerCase();
      const sanitizedEmail = sanitizeInput(formData.email.trim().toLowerCase());

      let phone = '';
      let country = '';
      let countryCode = '';

      if (phoneNumber) {
        try {
          const { parsePhoneNumber } = await import('libphonenumber-js');
          const parsed = parsePhoneNumber(phoneNumber);
          phone = parsed.nationalNumber;
          country = parsed.country || '';
          countryCode = `+${parsed.countryCallingCode}`;
        } catch (err) {
          console.error('Failed to parse phone number:', err);
        }
      }

      const result = await withTimeout(
        signup(username, sanitizedEmail, formData.password, phone, country, countryCode),
        AUTH_TIMEOUT_MS,
        'Resend request timed out'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'CLEAR_ERROR' });
        dispatch({ type: 'STOP_LOADING' });
      } else {
        dispatch({ type: 'SET_ERROR', error: result.error || t('register.errors.resendFailed') });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatch({ type: 'SET_ERROR', error: t('register.errors.resendFailed') });
    }
  }, [state.status, state.formData, state.phoneNumber, signup, t]);

  // Destructure for easier access
  const { formData, phoneNumber, otp, error, step, passwordValidation, usernameValidation, emailValidation, phoneValidation } = state;

  // Prevent double submission
  if (state.isVerificationComplete) {
    return (
      <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
        <div className="text-center">
          <div className="w-8 h-8 border border-white/30 border-t-white rounded-full animate-spin mx-auto mb-4"></div>
          <h2 className="text-white text-xl font-semibold mb-2">{t('register.verification.complete')}</h2>
          <p className="text-zinc-400 text-sm">{t('register.verification.settingUp')}</p>
        </div>
      </div>
    );
  }

  // Step 1: Registration Form
  if (step === 'form') {
    return (
      <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-lg mx-4 shadow-2xl">
        {/* Language Selector at Top */}
        <div className="mb-4">
          <CompactLanguageSelector />
        </div>

        <div className="mb-5">
          <div className="flex items-center mb-2.5">
            <button
              onClick={() => onNavigate('login')}
              className="text-zinc-400 hover:text-white transition-colors mr-3"
            >
              <ArrowLeft className="h-4 w-4" />
            </button>
            <h2 className="text-white text-2xl font-light">{t('register.title')}</h2>
          </div>
          <p className="text-zinc-400 text-sm leading-5">
            {t('register.subtitle')}
          </p>
        </div>

        <form onSubmit={handleRegister} className="space-y-3">
          <div className="grid grid-cols-2 gap-3">
            <div className="space-y-1">
              <Label htmlFor="firstName" className="text-white text-xs">
                {t('register.firstNameLabel')}
              </Label>
              <div className="relative">
                <User className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
                <Input
                  id="firstName"
                  type="text"
                  placeholder={t('register.firstNamePlaceholder')}
                  value={formData.firstName}
                  onChange={(e) => handleChange('firstName', e.target.value)}
                  className={`bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500 ${
                    formData.firstName && formData.firstName.length < 1 ? 'border-red-500' : ''
                  }`}
                  disabled={isLoading}
                  required
                  minLength={1}
                />
              </div>
            </div>

            <div className="space-y-1">
              <Label htmlFor="lastName" className="text-white text-xs">
                {t('register.lastNameLabel')}
              </Label>
              <Input
                id="lastName"
                type="text"
                placeholder={t('register.lastNamePlaceholder')}
                value={formData.lastName}
                onChange={(e) => handleChange("lastName", e.target.value)}
                className={`bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500 ${
                  formData.lastName && formData.lastName.length < 1 ? 'border-red-500' : ''
                }`}
                disabled={isLoading}
                required
                minLength={1}
              />
            </div>
          </div>

          {/* Username validation feedback */}
          {(formData.firstName || formData.lastName) && usernameValidation.message && (
            <div className={`flex items-center gap-1.5 text-xs ${
              usernameValidation.isValid ? 'text-green-400' : 'text-amber-400'
            }`}>
              {usernameValidation.isValid ? <Check className="h-3 w-3" /> : <X className="h-3 w-3" />}
              <span>{usernameValidation.message}</span>
            </div>
          )}

          <div className="space-y-1">
            <Label htmlFor="email" className="text-white text-xs">
              {t('register.emailLabel')}
            </Label>
            <div className="relative">
              <Mail className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
              <Input
                id="email"
                type="email"
                placeholder={t('register.emailPlaceholder')}
                value={formData.email}
                onChange={(e) => handleChange("email", e.target.value)}
                className={`bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500 ${
                  emailValidation.message ? 'border-red-500' : ''
                }`}
                disabled={isLoading}
                required
              />
            </div>
            {emailValidation.message && (
              <div className="flex items-center gap-1.5 text-red-400 text-xs">
                <X className="h-3 w-3" />
                <span>{emailValidation.message}</span>
              </div>
            )}
          </div>

          <div className="space-y-1">
            <Label htmlFor="phone" className="text-white text-xs">
              {t('register.phoneLabel')}
            </Label>
            <PhoneInput
              international
              defaultCountry="US"
              value={phoneNumber}
              onChange={handlePhoneChange}
              disabled={isLoading}
              placeholder={t('register.phonePlaceholder')}
              className={`phone-input-custom ${!phoneValidation.isValid && phoneNumber ? 'phone-input-error' : ''}`}
              required
              style={{
                '--PhoneInput-color--focus': '#71717a',
                '--PhoneInputInternationalIconPhone-opacity': '0.8',
                '--PhoneInputInternationalIconGlobe-opacity': '0.65',
                '--PhoneInputCountrySelect-marginRight': '0.35em',
                '--PhoneInputCountrySelectArrow-width': '0.3em',
                '--PhoneInputCountrySelectArrow-marginLeft': '0.35em',
                '--PhoneInputCountrySelectArrow-borderWidth': '1px',
                '--PhoneInputCountrySelectArrow-opacity': '0.45',
                '--PhoneInputCountrySelectArrow-color': 'currentColor',
                '--PhoneInputCountrySelectArrow-color--focus': 'currentColor',
                '--PhoneInputCountrySelectArrow-transform': 'rotate(45deg)',
                '--PhoneInputCountryFlag-aspectRatio': '1.5',
                '--PhoneInputCountryFlag-height': '1em',
                '--PhoneInputCountryFlag-borderWidth': '1px',
                '--PhoneInputCountryFlag-borderColor': 'rgba(0,0,0,0.5)',
                '--PhoneInputCountryFlag-borderColor--focus': 'currentColor',
                '--PhoneInputCountryFlag-backgroundColor--loading': 'rgba(0,0,0,0.1)'
              } as any}
            />
            {phoneValidation.message && (
              <div className={`flex items-center gap-1.5 text-xs ${
                phoneValidation.isValid ? 'text-green-400' : 'text-amber-400'
              }`}>
                {phoneValidation.isValid ? <Check className="h-3 w-3" /> : <X className="h-3 w-3" />}
                <span>{phoneValidation.message}</span>
              </div>
            )}
          </div>

          <div className="space-y-1">
            <Label htmlFor="password" className="text-white text-xs">
              {t('register.passwordLabel')}
            </Label>
            <div className="relative">
              <Lock className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
              <Input
                id="password"
                type="password"
                placeholder={t('register.passwordPlaceholder')}
                value={formData.password}
                onChange={(e) => handleChange("password", e.target.value)}
                className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                disabled={isLoading}
                required
                minLength={8}
              />
            </div>

            {/* Password Strength Indicators */}
            {formData.password && (
              <div className="mt-2 space-y-1 text-xs">
                <div className={`flex items-center gap-1.5 ${passwordValidation.minLength ? 'text-green-400' : 'text-zinc-500'}`}>
                  {passwordValidation.minLength ? <Check className="h-3 w-3" /> : <X className="h-3 w-3" />}
                  <span>{t('register.passwordRequirements.minLength')}</span>
                </div>
                <div className={`flex items-center gap-1.5 ${passwordValidation.hasUpperCase ? 'text-green-400' : 'text-zinc-500'}`}>
                  {passwordValidation.hasUpperCase ? <Check className="h-3 w-3" /> : <X className="h-3 w-3" />}
                  <span>{t('register.passwordRequirements.uppercase')}</span>
                </div>
                <div className={`flex items-center gap-1.5 ${passwordValidation.hasLowerCase ? 'text-green-400' : 'text-zinc-500'}`}>
                  {passwordValidation.hasLowerCase ? <Check className="h-3 w-3" /> : <X className="h-3 w-3" />}
                  <span>{t('register.passwordRequirements.lowercase')}</span>
                </div>
                <div className={`flex items-center gap-1.5 ${passwordValidation.hasNumber ? 'text-green-400' : 'text-zinc-500'}`}>
                  {passwordValidation.hasNumber ? <Check className="h-3 w-3" /> : <X className="h-3 w-3" />}
                  <span>{t('register.passwordRequirements.number')}</span>
                </div>
                <div className={`flex items-center gap-1.5 ${passwordValidation.hasSpecialChar ? 'text-green-400' : 'text-zinc-500'}`}>
                  {passwordValidation.hasSpecialChar ? <Check className="h-3 w-3" /> : <X className="h-3 w-3" />}
                  <span>{t('register.passwordRequirements.special')}</span>
                </div>
              </div>
            )}
          </div>

          <div className="space-y-1">
            <Label htmlFor="confirmPassword" className="text-white text-xs">
              {t('register.confirmPasswordLabel')}
            </Label>
            <div className="relative">
              <Lock className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
              <Input
                id="confirmPassword"
                type="password"
                placeholder={t('register.confirmPasswordPlaceholder')}
                value={formData.confirmPassword}
                onChange={(e) => handleChange("confirmPassword", e.target.value)}
                className={`bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500 ${
                  formData.confirmPassword && formData.password !== formData.confirmPassword ? 'border-red-500' : ''
                }`}
                disabled={isLoading}
                required
              />
            </div>
            {formData.confirmPassword && formData.password !== formData.confirmPassword && (
              <div className="flex items-center gap-1.5 text-red-400 text-xs mt-1">
                <X className="h-3 w-3" />
                <span>{t('register.errors.passwordMismatch')}</span>
              </div>
            )}
          </div>

          {error && (
            <div className="text-red-400 text-xs bg-red-900/20 border border-red-900/50 rounded p-2">
              {error}
            </div>
          )}

          <div className="flex justify-end pt-2">
            <Button
              type="submit"
              className="bg-zinc-700 hover:bg-zinc-600 text-white px-6 py-2 text-sm font-normal transition-colors disabled:opacity-50"
              disabled={isLoading}
            >
              {isLoading ? (
                <div className="flex items-center">
                  <div className="w-3 h-3 border border-white/30 border-t-white rounded-full animate-spin mr-1.5"></div>
                  {t('register.creating')}
                </div>
              ) : t('register.registerButton')}
            </Button>
          </div>
        </form>

        <div className="mt-4 pt-4 border-t border-zinc-700 text-center">
          <p className="text-zinc-400 text-xs">
            {t('register.haveAccount')}{" "}
            <button
              type="button"
              onClick={() => onNavigate('login')}
              className="text-blue-400 hover:text-blue-300 transition-colors"
              disabled={isLoading}
            >
              {t('register.signIn')}
            </button>
          </p>
        </div>
      </div>
    );
  }

  // Step 2: OTP Verification
  if (step === 'otp-verification') {
    return (
      <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
        {/* Language Selector at Top */}
        <div className="mb-4">
          <CompactLanguageSelector />
        </div>

        <div className="mb-6">
          <div className="flex items-center mb-3">
            <button
              onClick={() => dispatch({ type: 'SET_STEP', step: 'form' })}
              className="text-zinc-400 hover:text-white transition-colors mr-3"
              disabled={isLoading}
            >
              <ArrowLeft className="h-4 w-4" />
            </button>
            <h2 className="text-white text-2xl font-light">{t('register.verification.title')}</h2>
          </div>
          <p className="text-zinc-400 text-xs leading-5">
            {t('register.verification.subtitle')} <span className="text-white">{formData.email}</span>.
            {t('register.verification.description')}
          </p>
          <p className="text-zinc-500 text-xs leading-5 mt-2">
            {t('register.verification.note')}
          </p>
        </div>

        <form onSubmit={handleVerifyOtp} className="space-y-4">
          <div className="space-y-1">
            <Label htmlFor="otp" className="text-white text-xs">
              {t('register.verification.codeLabel')}
            </Label>
            <div className="relative">
              <Key className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
              <Input
                id="otp"
                type="text"
                placeholder={t('register.verification.codePlaceholder')}
                value={otp}
                onChange={(e) => dispatch({ type: 'SET_OTP', value: e.target.value })}
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
              disabled={isLoading || state.isVerificationComplete}
            >
              {isLoading ? (
                <div className="flex items-center">
                  <div className="w-3 h-3 border border-white/30 border-t-white rounded-full animate-spin mr-1.5"></div>
                  {t('register.verification.verifying')}
                </div>
              ) : t('register.verification.verifyButton')}
            </Button>
          </div>
        </form>

        <div className="mt-5 pt-4 border-t border-zinc-700 text-center">
          <button
            onClick={handleResendOtp}
            className="text-blue-400 hover:text-blue-300 text-xs transition-colors"
            disabled={isLoading}
          >
            {isLoading ? t('register.verification.resending') : t('register.verification.resend')}
          </button>
        </div>
      </div>
    );
  }

  return null;
};

export default RegisterScreen;