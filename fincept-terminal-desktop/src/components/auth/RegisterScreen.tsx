// File: src/components/auth/RegisterScreen.tsx
// User registration screen with country code selector and complete form validation

import React, { useState, useEffect } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { ArrowLeft, User, Mail, Phone, Building, Lock, Key, ChevronDown, Check, X } from "lucide-react";
import { Screen } from '../../App';
import { useAuth } from '@/contexts/AuthContext';
import { useTranslation } from 'react-i18next';

interface RegisterScreenProps {
  onNavigate: (screen: Screen) => void;
}

type RegisterStep = 'form' | 'otp-verification';

interface Country {
  name: string;
  code: string;
  dialCode: string;
  flag: string;
}

const RegisterScreen: React.FC<RegisterScreenProps> = ({ onNavigate }) => {
  const { signup, verifyOtp, session } = useAuth();
  const { t } = useTranslation('auth');
  const [step, setStep] = useState<RegisterStep>('form');
  const [formData, setFormData] = useState({
    firstName: "",
    lastName: "",
    email: "",
    phone: "",
    countryCode: "+1", // Default to US
    company: "",
    password: "",
    confirmPassword: ""
  });
  const [otp, setOtp] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState("");
  const [isVerificationComplete, setIsVerificationComplete] = useState(false);
  const [countries, setCountries] = useState<Country[]>([]);
  const [isCountryDropdownOpen, setIsCountryDropdownOpen] = useState(false);
  const [countrySearchTerm, setCountrySearchTerm] = useState("");

  // Password validation state
  const [passwordValidation, setPasswordValidation] = useState({
    minLength: false,
    hasUpperCase: false,
    hasLowerCase: false,
    hasNumber: false,
    hasSpecialChar: false
  });

  // Fetch countries data from REST Countries API
  useEffect(() => {
    const fetchCountries = async () => {
      try {
        const response = await fetch('https://restcountries.com/v3.1/all?fields=name,cca2,idd,flag');
        const data = await response.json();

        const formattedCountries: Country[] = data
          .filter((country: any) => country.idd?.root && country.idd?.suffixes?.length > 0)
          .map((country: any) => ({
            name: country.name.common,
            code: country.cca2,
            dialCode: country.idd.root + (country.idd.suffixes[0] || ''),
            flag: country.flag
          }))
          .sort((a: Country, b: Country) => a.name.localeCompare(b.name));

        setCountries(formattedCountries);
      } catch (error) {
        console.error('Failed to fetch countries:', error);
        // Fallback to common countries if API fails
        setCountries([
          { name: "United States", code: "US", dialCode: "+1", flag: "🇺🇸" },
          { name: "United Kingdom", code: "GB", dialCode: "+44", flag: "🇬🇧" },
          { name: "Canada", code: "CA", dialCode: "+1", flag: "🇨🇦" },
          { name: "India", code: "IN", dialCode: "+91", flag: "🇮🇳" },
          { name: "Australia", code: "AU", dialCode: "+61", flag: "🇦🇺" },
          { name: "Germany", code: "DE", dialCode: "+49", flag: "🇩🇪" },
          { name: "France", code: "FR", dialCode: "+33", flag: "🇫🇷" },
          { name: "Japan", code: "JP", dialCode: "+81", flag: "🇯🇵" },
        ]);
      }
    };

    fetchCountries();
  }, []);

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
  };

  const handleCountrySelect = (country: Country) => {
    setFormData(prev => ({ ...prev, countryCode: country.dialCode }));
    setIsCountryDropdownOpen(false);
    setCountrySearchTerm("");
  };

  const filteredCountries = countries.filter(country =>
    country.name.toLowerCase().includes(countrySearchTerm.toLowerCase()) ||
    country.dialCode.includes(countrySearchTerm)
  );

  const selectedCountry = countries.find(country => country.dialCode === formData.countryCode);

  const handleRegister = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!formData.firstName || !formData.lastName || !formData.email || !formData.password || !formData.confirmPassword) {
      setError(t('register.errors.requiredFields'));
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

      const result = await signup(username, formData.email, formData.password);

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
      const result = await signup(username, formData.email, formData.password);

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
                  className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                  disabled={isLoading}
                  required
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
                className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                disabled={isLoading}
                required
              />
            </div>
          </div>

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
                className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                disabled={isLoading}
                required
              />
            </div>
          </div>

          <div className="space-y-1">
            <Label htmlFor="phone" className="text-white text-xs">
              {t('register.phoneLabel')}
            </Label>
            <div className="flex gap-2">
              {/* Country Code Dropdown */}
              <div className="relative">
                <button
                  type="button"
                  onClick={() => setIsCountryDropdownOpen(!isCountryDropdownOpen)}
                  className="bg-zinc-800 border border-zinc-600 text-white h-9 px-2.5 text-sm rounded flex items-center gap-1 hover:bg-zinc-700 focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500 min-w-[75px]"
                  disabled={isLoading}
                >
                  <span className="text-base">{selectedCountry?.flag || "🌍"}</span>
                  <span className="text-xs">{formData.countryCode}</span>
                  <ChevronDown className="h-3 w-3" />
                </button>

                {isCountryDropdownOpen && (
                  <div className="absolute top-full left-0 mt-1 w-80 bg-zinc-800 border border-zinc-600 rounded-lg shadow-lg z-50 max-h-60 overflow-hidden">
                    <div className="p-2">
                      <Input
                        type="text"
                        placeholder={t('register.searchCountries')}
                        value={countrySearchTerm}
                        onChange={(e) => setCountrySearchTerm(e.target.value)}
                        className="bg-zinc-700 border-zinc-600 text-white placeholder-zinc-500 h-8 text-sm"
                      />
                    </div>
                    <div className="overflow-y-auto max-h-48">
                      {filteredCountries.slice(0, 50).map((country) => (
                        <button
                          key={country.code}
                          type="button"
                          onClick={() => handleCountrySelect(country)}
                          className="w-full px-3 py-2 text-left hover:bg-zinc-700 flex items-center gap-2 text-sm text-white"
                        >
                          <span className="text-base">{country.flag}</span>
                          <span className="flex-1">{country.name}</span>
                          <span className="text-zinc-400">{country.dialCode}</span>
                        </button>
                      ))}
                    </div>
                  </div>
                )}
              </div>

              {/* Phone Number Input */}
              <div className="flex-1 relative">
                <Phone className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
                <Input
                  id="phone"
                  type="tel"
                  placeholder={t('register.phonePlaceholder')}
                  value={formData.phone}
                  onChange={(e) => handleChange("phone", e.target.value)}
                  className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                  disabled={isLoading}
                />
              </div>
            </div>
          </div>

          <div className="space-y-1">
            <Label htmlFor="company" className="text-white text-xs">
              {t('register.companyLabel')}
            </Label>
            <div className="relative">
              <Building className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
              <Input
                id="company"
                type="text"
                placeholder={t('register.companyPlaceholder')}
                value={formData.company}
                onChange={(e) => handleChange("company", e.target.value)}
                className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                disabled={isLoading}
              />
            </div>
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

        {/* Close dropdown when clicking outside */}
        {isCountryDropdownOpen && (
          <div
            className="fixed inset-0 z-40"
            onClick={() => setIsCountryDropdownOpen(false)}
          />
        )}
      </div>
    );
  }

  // Step 2: OTP Verification
  if (step === 'otp-verification') {
    return (
      <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
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