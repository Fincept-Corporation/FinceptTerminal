// File: src/components/auth/ForgotPasswordScreen.tsx
// Complete password reset flow with email submission, OTP verification, and password reset

import React, { useState } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { ArrowLeft, Mail, Key, Eye, EyeOff } from "lucide-react";
import { Screen } from '../../App';
import { AuthApiService } from '@/services/authApi';

interface ForgotPasswordScreenProps {
  onNavigate: (screen: Screen) => void;
}

type ResetStep = 'email' | 'otp-sent' | 'reset-form' | 'success';

const ForgotPasswordScreen: React.FC<ForgotPasswordScreenProps> = ({ onNavigate }) => {
  const [step, setStep] = useState<ResetStep>('email');
  const [email, setEmail] = useState("");
  const [otp, setOtp] = useState("");
  const [newPassword, setNewPassword] = useState("");
  const [confirmPassword, setConfirmPassword] = useState("");
  const [showPassword, setShowPassword] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState("");

  const handleEmailSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!email) {
      setError("Please enter your email address");
      return;
    }

    setIsLoading(true);
    setError("");

    try {
      console.log('Requesting password reset for:', email);

      const result = await AuthApiService.forgotPassword({ email });

      if (result.success) {
        console.log('Password reset OTP sent successfully');
        setStep('otp-sent');
      } else {
        console.log('Password reset request failed:', result.error);
        setError(result.error || 'Failed to send reset code. Please try again.');
      }
    } catch (err) {
      console.error('Password reset request error:', err);
      setError("An unexpected error occurred. Please try again.");
    } finally {
      setIsLoading(false);
    }
  };

  const handleContinueToReset = () => {
    setStep('reset-form');
    setError("");
  };

  const handlePasswordReset = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!otp) {
      setError("Please enter the verification code");
      return;
    }

    if (!newPassword) {
      setError("Please enter a new password");
      return;
    }

    if (newPassword.length < 8) {
      setError("Password must be at least 8 characters long");
      return;
    }

    if (newPassword !== confirmPassword) {
      setError("Passwords do not match");
      return;
    }

    setIsLoading(true);
    setError("");

    try {
      console.log('Resetting password for:', email);

      const result = await AuthApiService.resetPassword({
        email,
        otp,
        new_password: newPassword
      });

      if (result.success) {
        console.log('Password reset successful');
        setStep('success');
      } else {
        console.log('Password reset failed:', result.error);
        setError(result.error || 'Failed to reset password. Please check your code and try again.');
      }
    } catch (err) {
      console.error('Password reset error:', err);
      setError("An unexpected error occurred. Please try again.");
    } finally {
      setIsLoading(false);
    }
  };

  const handleResendOtp = async () => {
    setIsLoading(true);
    setError("");

    try {
      const result = await AuthApiService.forgotPassword({ email });

      if (result.success) {
        setError(""); // Clear any previous errors
        // You could show a success message here if needed
      } else {
        setError(result.error || 'Failed to resend code. Please try again.');
      }
    } catch (err) {
      setError("Failed to resend code. Please try again.");
    } finally {
      setIsLoading(false);
    }
  };

  // Step 1: Email Input
  if (step === 'email') {
    return (
      <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
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
                onChange={(e) => {
                  setEmail(e.target.value);
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
              onClick={() => setStep('email')}
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
              onClick={() => setStep('otp-sent')}
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
                onChange={(e) => {
                  setNewPassword(e.target.value);
                  if (error) setError("");
                }}
                className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pr-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
                disabled={isLoading}
                required
                minLength={8}
              />
              <button
                type="button"
                onClick={() => setShowPassword(!showPassword)}
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
              onChange={(e) => {
                setConfirmPassword(e.target.value);
                if (error) setError("");
              }}
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