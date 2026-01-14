// File: src/components/auth/RegisterScreen.tsx
// User registration screen with country code selector and complete form validation

import React, { useState, useEffect } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { ArrowLeft, User, Mail, Lock, Key, Check, X } from "lucide-react";
import { Screen } from '../../App';
import { useAuth } from '@/contexts/AuthContext';
import { useTranslation } from 'react-i18next';
import PhoneInput, { getCountryCallingCode as getCallingCode } from 'react-phone-number-input';
import 'react-phone-number-input/style.css';
import './phone-input.css';
import { CompactLanguageSelector } from './CompactLanguageSelector';

interface RegisterScreenProps {
  onNavigate: (screen: Screen) => void;
}

type RegisterStep = 'form' | 'otp-verification';

const RegisterScreen: React.FC<RegisterScreenProps> = ({ onNavigate }) => {
  const { signup, verifyOtp, session } = useAuth();
  const { t } = useTranslation('auth');
  const [step, setStep] = useState<RegisterStep>('form');
  const [formData, setFormData] = useState({
    firstName: "",
    lastName: "",
    email: "",
    password: "",
    confirmPassword: ""
  });
  const [phoneNumber, setPhoneNumber] = useState<string | undefined>();
  const [otp, setOtp] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState("");
  const [isVerificationComplete, setIsVerificationComplete] = useState(false);

  // Password validation state
  const [passwordValidation, setPasswordValidation] = useState({
    minLength: false,
    hasUpperCase: false,
    hasLowerCase: false,
    hasNumber: false,
    hasSpecialChar: false
  });

  // Username validation state
  const [usernameValidation, setUsernameValidation] = useState({
    isValid: false,
    length: 0,
    message: ''
  });

  // Email validation state
  const [emailValidation, setEmailValidation] = useState({
    isValid: false,
    message: ''
  });

  // Phone validation state
  const [phoneValidation, setPhoneValidation] = useState({
    isValid: false,
    message: ''
  });

  // Handle navigation after successful authentication
  useEffect(() => {
    if (session?.authenticated && session?.user_type === 'registered' && isVerificationComplete) {
      console.log('Registration complete, navigating based on account type:', session.user_info?.account_type);

      // Check if user has subscription
      if (session.subscription?.has_subscription) {
        console.log('User has subscription, redirecting to dashboard');
        onNavigate('dashboard' as Screen);
      } else {
        console.log('User is free tier, showing pricing screen');
        onNavigate('pricing' as Screen);
      }
    }
  }, [session, isVerificationComplete, onNavigate]);

  const handleChange = (field: string, value: string) => {
    setFormData(prev => ({ ...prev, [field]: value }));
    if (error) setError("");

    // Real-time password validation
    if (field === 'password') {
      setPasswordValidation({
        minLength: value.length >= 8,
        hasUpperCase: /[A-Z]/.test(value),
        hasLowerCase: /[a-z]/.test(value),
        hasNumber: /[0-9]/.test(value),
        hasSpecialChar: /[!@#$%^&*(),.?":{}|<>]/.test(value)
      });
    }

    // Real-time email validation
    if (field === 'email') {
      const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
      const isValid = emailRegex.test(value);
      setEmailValidation({
        isValid: isValid || value === '',
        message: value && !isValid ? 'Invalid email format' : ''
      });
    }

    // Real-time username validation (firstName or lastName changes)
    if (field === 'firstName' || field === 'lastName') {
      const updatedFormData = { ...formData, [field]: value };
      const username = `${updatedFormData.firstName.trim()}${updatedFormData.lastName.trim()}`.toLowerCase();
      const length = username.length;

      if (updatedFormData.firstName || updatedFormData.lastName) {
        if (length < 3) {
          setUsernameValidation({
            isValid: false,
            length,
            message: `Username too short (${length}/3 min)`
          });
        } else if (length > 50) {
          setUsernameValidation({
            isValid: false,
            length,
            message: `Username too long (${length}/50 max)`
          });
        } else {
          setUsernameValidation({
            isValid: true,
            length,
            message: `Username valid (${length} characters)`
          });
        }
      } else {
        setUsernameValidation({
          isValid: false,
          length: 0,
          message: ''
        });
      }
    }
  };

  const handlePhoneChange = (value: string | undefined) => {
    setPhoneNumber(value);
    if (error) setError("");

    // Real-time phone validation
    if (value) {
      // Basic validation - check if it has enough digits (minimum 7)
      const digitsOnly = value.replace(/\D/g, '');
      if (digitsOnly.length < 7) {
        setPhoneValidation({
          isValid: false,
          message: 'Phone number too short'
        });
      } else if (digitsOnly.length > 15) {
        setPhoneValidation({
          isValid: false,
          message: 'Phone number too long'
        });
      } else {
        setPhoneValidation({
          isValid: true,
          message: 'Valid phone number'
        });
      }
    } else {
      setPhoneValidation({
        isValid: false,
        message: 'Phone number required'
      });
    }
  };

  const handleRegister = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!formData.firstName || !formData.lastName || !formData.email || !formData.password || !formData.confirmPassword) {
      setError(t('register.errors.requiredFields'));
      return;
    }

    if (!phoneNumber) {
      setError('Phone number is required');
      return;
    }

    if (formData.password !== formData.confirmPassword) {
      setError(t('register.errors.passwordMismatch'));
      return;
    }

    if (formData.password.length < 8) {
      setError(t('register.errors.passwordLength'));
      return;
    }

    setIsLoading(true);
    setError("");

    try {
      console.log('Registration attempt for:', formData.email);

      // Create username from first and last name, ensuring no spaces
      const username = `${formData.firstName.trim()}${formData.lastName.trim()}`.toLowerCase();
      console.log('Generated username:', username);

      // Validate username length (3-50 characters as per API requirement)
      if (username.length < 3 || username.length > 50) {
        setError('Username must be between 3 and 50 characters. Please use shorter first/last names.');
        setIsLoading(false);
        return;
      }

      // Parse phone number if provided
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

      const result = await signup(username, formData.email, formData.password, phone, country, countryCode);

      if (result.success) {
        console.log('Registration successful, OTP sent to:', formData.email);
        // Note: Backend handles re-registration for unverified accounts automatically
        // A fresh OTP is sent even if the user previously registered but didn't verify
        setStep('otp-verification');
      } else {
        console.log('Registration failed:', result.error);
        // Enhanced error handling for common scenarios
        const errorMessage = result.error || 'Registration failed. Please try again.';

        // If the error indicates an already verified account, show helpful message
        if (errorMessage.toLowerCase().includes('already exists') ||
            errorMessage.toLowerCase().includes('already registered')) {
          setError(t('register.errors.alreadyExists'));
        } else {
          setError(errorMessage);
        }
      }
    } catch (err) {
      console.error('Registration error:', err);
      setError(t('register.errors.unexpectedError'));
    } finally {
      setIsLoading(false);
    }
  };

  const handleVerifyOtp = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!otp) {
      setError(t('register.errors.verificationCode'));
      return;
    }

    if (isLoading || isVerificationComplete) {
      console.log('Verification already in progress or complete, skipping');
      return;
    }

    setIsLoading(true);
    setError("");

    try {
      console.log('OTP verification for:', formData.email);

      const result = await verifyOtp(formData.email, otp);

      if (result.success) {
        console.log('OTP verification successful');
        setIsVerificationComplete(true);
        // Navigation will be handled by useEffect
      } else {
        console.log('OTP verification failed:', result.error);
        setError(result.error || t('register.errors.verificationFailed'));
      }
    } catch (err) {
      console.error('OTP verification error:', err);
      setError(t('register.errors.unexpectedError'));
    } finally {
      setIsLoading(false);
    }
  };

  const handleResendOtp = async () => {
    setIsLoading(true);
    setError("");

    try {
      const username = `${formData.firstName.trim()}${formData.lastName.trim()}`.toLowerCase();

      // Parse phone number if provided
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

      const result = await signup(username, formData.email, formData.password, phone, country, countryCode);

      if (result.success) {
        setError(""); // Clear any previous errors
        console.log('OTP resent successfully');
      } else {
        setError(result.error || t('register.errors.resendFailed'));
      }
    } catch (err) {
      setError(t('register.errors.resendFailed'));
    } finally {
      setIsLoading(false);
    }
  };

  // Prevent double submission
  if (isVerificationComplete) {
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
                  onChange={(e) => handleChange("firstName", e.target.value)}
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
              onClick={() => setStep('form')}
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
                onChange={(e) => {
                  setOtp(e.target.value);
                  if (error) setError("");
                }}
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
              disabled={isLoading || isVerificationComplete}
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