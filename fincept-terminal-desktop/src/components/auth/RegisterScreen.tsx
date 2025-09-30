// File: src/components/auth/RegisterScreen.tsx
// User registration screen with country code selector and complete form validation

import React, { useState, useEffect } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { ArrowLeft, User, Mail, Phone, Building, Lock, Key, ChevronDown } from "lucide-react";
import { Screen } from '../../App';
import { useAuth } from '@/contexts/AuthContext';

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
      setError("Please fill in all required fields");
      return;
    }

    if (formData.password !== formData.confirmPassword) {
      setError("Passwords do not match");
      return;
    }

    if (formData.password.length < 8) {
      setError("Password must be at least 8 characters long");
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
        setStep('otp-verification');
      } else {
        console.log('Registration failed:', result.error);
        setError(result.error || 'Registration failed. Please try again.');
      }
    } catch (err) {
      console.error('Registration error:', err);
      setError("An unexpected error occurred. Please try again.");
    } finally {
      setIsLoading(false);
    }
  };

  const handleVerifyOtp = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!otp) {
      setError("Please enter the verification code");
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
        setError(result.error || 'Verification failed. Please check your code and try again.');
      }
    } catch (err) {
      console.error('OTP verification error:', err);
      setError("An unexpected error occurred. Please try again.");
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
        setError(result.error || 'Failed to resend code. Please try again.');
      }
    } catch (err) {
      setError("Failed to resend code. Please try again.");
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
          <h2 className="text-white text-xl font-semibold mb-2">Verification Complete</h2>
          <p className="text-zinc-400 text-sm">Setting up your account...</p>
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
            <h2 className="text-white text-2xl font-light">Create Account</h2>
          </div>
          <p className="text-zinc-400 text-sm leading-5">
            Join Fincept to access professional financial terminal and analytics platform.
          </p>
        </div>

        <form onSubmit={handleRegister} className="space-y-3">
          <div className="grid grid-cols-2 gap-3">
            <div className="space-y-1">
              <Label htmlFor="firstName" className="text-white text-xs">
                First Name *
              </Label>
              <div className="relative">
                <User className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
                <Input
                  id="firstName"
                  type="text"
                  placeholder="First name"
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
                Last Name *
              </Label>
              <Input
                id="lastName"
                type="text"
                placeholder="Last name"
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
              Email Address *
            </Label>
            <div className="relative">
              <Mail className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
              <Input
                id="email"
                type="email"
                placeholder="Enter your email"
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
              Phone Number
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
                        placeholder="Search countries..."
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
                  placeholder="Phone number (optional)"
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
              Company
            </Label>
            <div className="relative">
              <Building className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
              <Input
                id="company"
                type="text"
                placeholder="Company name (optional)"
                value={formData.company}
                onChange={(e) => handleChange("company", e.target.value)}
                className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                disabled={isLoading}
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div className="space-y-1">
              <Label htmlFor="password" className="text-white text-xs">
                Password *
              </Label>
              <div className="relative">
                <Lock className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
                <Input
                  id="password"
                  type="password"
                  placeholder="Create password"
                  value={formData.password}
                  onChange={(e) => handleChange("password", e.target.value)}
                  className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                  disabled={isLoading}
                  required
                  minLength={8}
                />
              </div>
            </div>

            <div className="space-y-1">
              <Label htmlFor="confirmPassword" className="text-white text-xs">
                Confirm Password *
              </Label>
              <div className="relative">
                <Lock className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
                <Input
                  id="confirmPassword"
                  type="password"
                  placeholder="Confirm password"
                  value={formData.confirmPassword}
                  onChange={(e) => handleChange("confirmPassword", e.target.value)}
                  className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                  disabled={isLoading}
                  required
                />
              </div>
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
              className="bg-zinc-700 hover:bg-zinc-600 text-white px-6 py-2 text-sm font-normal transition-colors disabled:opacity-50"
              disabled={isLoading}
            >
              {isLoading ? (
                <div className="flex items-center">
                  <div className="w-3 h-3 border border-white/30 border-t-white rounded-full animate-spin mr-1.5"></div>
                  Creating...
                </div>
              ) : "Create Account"}
            </Button>
          </div>
        </form>

        <div className="mt-4 pt-4 border-t border-zinc-700 text-center">
          <p className="text-zinc-400 text-xs">
            Already have an account?{" "}
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

  // Step 2: OTP Verification (unchanged)
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
            <h2 className="text-white text-2xl font-light">Verify Email</h2>
          </div>
          <p className="text-zinc-400 text-xs leading-5">
            We've sent a verification code to <span className="text-white">{formData.email}</span>.
            Please enter the code below to complete your registration.
          </p>
        </div>

        <form onSubmit={handleVerifyOtp} className="space-y-4">
          <div className="space-y-1">
            <Label htmlFor="otp" className="text-white text-xs">
              Verification Code
            </Label>
            <div className="relative">
              <Key className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
              <Input
                id="otp"
                type="text"
                placeholder="Enter verification code"
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
                  Verifying...
                </div>
              ) : "Verify & Complete"}
            </Button>
          </div>
        </form>

        <div className="mt-5 pt-4 border-t border-zinc-700 text-center">
          <button
            onClick={handleResendOtp}
            className="text-blue-400 hover:text-blue-300 text-xs transition-colors"
            disabled={isLoading}
          >
            {isLoading ? "Sending..." : "Didn't receive the code? Resend"}
          </button>
        </div>
      </div>
    );
  }

  return null;
};

export default RegisterScreen;