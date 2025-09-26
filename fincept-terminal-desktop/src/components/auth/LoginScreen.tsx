// File: src/components/auth/LoginScreen.tsx
// Login screen with username/password authentication and guest access

import React, { useState } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Monitor, Lock, Mail } from "lucide-react";
import { Screen } from '../../App';
import { AuthApiService } from '@/services/authApi';
import { useAuth } from '@/contexts/AuthContext';

interface LoginScreenProps {
  onNavigate: (screen: Screen) => void;
}

const LoginScreen: React.FC<LoginScreenProps> = ({ onNavigate }) => {
  const [formData, setFormData] = useState({
    email: "",
    password: ""
  });
  const [isLoading, setIsLoading] = useState(false);
  const [isGuestLoading, setIsGuestLoading] = useState(false);
  const [error, setError] = useState("");

  const { login, setupGuestAccess } = useAuth();

  const handleChange = (field: string, value: string) => {
    setFormData(prev => ({ ...prev, [field]: value }));
    if (error) setError("");
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!formData.email || !formData.password) {
      setError("Please fill in all fields");
      return;
    }

    // Basic email validation
    const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    if (!emailRegex.test(formData.email)) {
      setError("Please enter a valid email address");
      return;
    }

    setIsLoading(true);
    setError("");

    try {
      console.log('Login attempt for:', formData.email);

      const result = await login(formData.email, formData.password);

      if (result.success) {
        console.log('Login successful');
        // The useAuth hook will handle navigation/state updates
        // You can add any post-login logic here if needed
      } else {
        console.log('Login failed:', result.error);
        setError(result.error || 'Login failed. Please check your credentials.');
      }
    } catch (err) {
      console.error('Login error:', err);
      setError("An unexpected error occurred. Please try again.");
    } finally {
      setIsLoading(false);
    }
  };

  const handleGuestAccess = async () => {
    setIsGuestLoading(true);
    setError("");

    try {
      console.log('Setting up guest access...');

      const result = await setupGuestAccess();

      if (result.success) {
        console.log('Guest access setup successful');
        // The useAuth hook will handle navigation/state updates
      } else {
        console.log('Guest access setup failed:', result.error);
        setError(result.error || 'Failed to setup guest access. Please try again.');
      }
    } catch (err) {
      console.error('Guest access error:', err);
      setError("An unexpected error occurred. Please try again.");
    } finally {
      setIsGuestLoading(false);
    }
  };

  return (
    <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
      <div className="mb-6">
        <h2 className="text-white text-2xl font-light mb-3">Login</h2>
        <p className="text-zinc-400 text-xs leading-5">
          This is a secure Fincept authentication service that allows you access to Fincept services from wherever you are.
        </p>
      </div>

      <form onSubmit={handleSubmit} className="space-y-4">
        <div className="space-y-1">
          <Label htmlFor="email" className="text-white text-xs">
            Email Address
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
              disabled={isLoading || isGuestLoading}
              required
            />
          </div>
        </div>

        <div className="space-y-1">
          <div className="flex justify-between items-center">
            <Label htmlFor="password" className="text-white text-xs">
              Password
            </Label>
            <button
              type="button"
              onClick={() => onNavigate('forgotPassword')}
              className="text-blue-400 hover:text-blue-300 text-xs transition-colors"
              disabled={isLoading || isGuestLoading}
            >
              Forgot Password?
            </button>
          </div>
          <div className="relative">
            <Lock className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
            <Input
              id="password"
              type="password"
              placeholder="Enter your password"
              value={formData.password}
              onChange={(e) => handleChange("password", e.target.value)}
              className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
              disabled={isLoading || isGuestLoading}
              required
            />
          </div>
        </div>

        <div className="pt-1">
          <p className="text-zinc-500 text-xs">
            Your account credentials are required to access Fincept services.
          </p>
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
            disabled={isLoading || isGuestLoading}
          >
            {isLoading ? (
              <div className="flex items-center">
                <div className="w-3 h-3 border border-white/30 border-t-white rounded-full animate-spin mr-1.5"></div>
                Signing In...
              </div>
            ) : "Sign In"}
          </Button>
        </div>
      </form>

      <div className="mt-5 pt-4 border-t border-zinc-700">
        <div className="text-center mb-3">
          <p className="text-zinc-400 text-xs mb-3">Want to explore first?</p>
          <Button
            onClick={handleGuestAccess}
            disabled={isLoading || isGuestLoading}
            className="bg-green-600/20 hover:bg-green-600/30 border border-green-600/50 text-green-400 px-4 py-1.5 text-xs font-normal transition-colors disabled:opacity-50 w-full"
          >
            {isGuestLoading ? (
              <div className="flex items-center justify-center">
                <div className="w-3 h-3 border border-green-400/30 border-t-green-400 rounded-full animate-spin mr-1.5"></div>
                Setting up Guest Access...
              </div>
            ) : (
              <div className="flex items-center justify-center">
                <svg className="w-3 h-3 mr-1.5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13 10V3L4 14h7v7l9-11h-7z" />
                </svg>
                Continue as Guest
              </div>
            )}
          </Button>
          <p className="text-zinc-500 text-xs mt-1.5">
            50 API requests • 24-hour access • No signup required
          </p>
        </div>

        <div className="text-center">
          <p className="text-zinc-400 text-xs">
            Don't have an account?{" "}
            <button
              type="button"
              onClick={() => onNavigate('register')}
              className="text-blue-400 hover:text-blue-300 transition-colors"
              disabled={isLoading || isGuestLoading}
            >
              Create Account
            </button>
          </p>
        </div>
      </div>
    </div>
  );
};

export default LoginScreen;