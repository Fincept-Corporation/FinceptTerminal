// File: src/components/auth/LoginScreen.tsx
// User login screen with email/password authentication and guest access

import React, { useState } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Mail, Lock, Eye, EyeOff } from "lucide-react";
import { Screen } from '../../App';
import { useAuth } from '@/contexts/AuthContext';

interface LoginScreenProps {
  onNavigate: (screen: Screen) => void;
}

const LoginScreen: React.FC<LoginScreenProps> = ({ onNavigate }) => {
  const { login, setupGuestAccess } = useAuth();
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [showPassword, setShowPassword] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [isGuestLoading, setIsGuestLoading] = useState(false);
  const [error, setError] = useState("");

  const handleLogin = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!email || !password) {
      setError("Please enter both email and password");
      return;
    }

    setIsLoading(true);
    setError("");

    try {
      console.log('Login attempt for:', email);

      const result = await login(email, password);

      if (result.success) {
        console.log('Login successful - App.tsx will handle routing based on account type');
        // App.tsx useEffect will handle navigation based on account_type
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
        console.log('Guest access setup successful - App.tsx will handle routing');
        // App.tsx useEffect will handle navigation to dashboard
      } else {
        console.log('Guest access setup failed:', result.error);
        setError(result.error || 'Failed to setup guest access. Please try again.');
      }
    } catch (err) {
      console.error('Guest access error:', err);
      setError("Failed to setup guest access. Please try again.");
    } finally {
      setIsGuestLoading(false);
    }
  };

  return (
    <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-sm mx-4 shadow-2xl">
      <div className="mb-6">
        <h2 className="text-white text-2xl font-light mb-3">Welcome to Fincept</h2>
        <p className="text-zinc-400 text-xs leading-5">
          Sign in to access your financial terminal and analytics platform.
        </p>
      </div>

      <form onSubmit={handleLogin} className="space-y-4">
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
              value={email}
              onChange={(e) => {
                setEmail(e.target.value);
                if (error) setError("");
              }}
              className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
              disabled={isLoading || isGuestLoading}
              required
            />
          </div>
        </div>

        <div className="space-y-1">
          <Label htmlFor="password" className="text-white text-xs">
            Password
          </Label>
          <div className="relative">
            <Lock className="absolute left-2.5 top-1/2 transform -translate-y-1/2 h-3.5 w-3.5 text-zinc-400" />
            <Input
              id="password"
              type={showPassword ? "text" : "password"}
              placeholder="Enter your password"
              value={password}
              onChange={(e) => {
                setPassword(e.target.value);
                if (error) setError("");
              }}
              className="bg-zinc-800 border-zinc-600 text-white placeholder-zinc-500 pl-9 pr-9 py-2 h-9 text-sm focus:border-zinc-500 focus:ring-1 focus:ring-zinc-500"
              disabled={isLoading || isGuestLoading}
              required
            />
            <button
              type="button"
              onClick={() => setShowPassword(!showPassword)}
              className="absolute right-2.5 top-1/2 transform -translate-y-1/2 text-zinc-400 hover:text-zinc-300"
              disabled={isLoading || isGuestLoading}
            >
              {showPassword ? <EyeOff className="h-3.5 w-3.5" /> : <Eye className="h-3.5 w-3.5" />}
            </button>
          </div>
        </div>

        {error && (
          <div className="text-red-400 text-xs bg-red-900/20 border border-red-900/50 rounded p-2">
            {error}
          </div>
        )}

        <div className="flex justify-between items-center text-xs pt-1">
          <button
            type="button"
            onClick={() => onNavigate('forgotPassword')}
            className="text-blue-400 hover:text-blue-300 transition-colors"
            disabled={isLoading || isGuestLoading}
          >
            Forgot Password?
          </button>
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

      <div className="mt-5 pt-4 border-t border-zinc-700 space-y-3">
        <Button
          onClick={handleGuestAccess}
          className="w-full bg-blue-600/20 hover:bg-blue-600/30 border border-blue-600/50 text-blue-400 py-2 text-sm font-normal transition-colors disabled:opacity-50"
          disabled={isLoading || isGuestLoading}
        >
          {isGuestLoading ? (
            <div className="flex items-center justify-center">
              <div className="w-3 h-3 border border-blue-400/30 border-t-blue-400 rounded-full animate-spin mr-1.5"></div>
              Setting up...
            </div>
          ) : (
            <>
              <svg className="w-4 h-4 mr-2" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 15v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2zm10-10V7a4 4 0 00-8 0v4h8z" />
              </svg>
              Continue as Guest
            </>
          )}
        </Button>

        <div className="text-center">
          <p className="text-zinc-400 text-xs">
            Don't have an account?{" "}
            <button
              type="button"
              onClick={() => onNavigate('register')}
              className="text-blue-400 hover:text-blue-300 transition-colors"
              disabled={isLoading || isGuestLoading}
            >
              Sign Up
            </button>
          </p>
        </div>

        <div className="text-center text-zinc-500 text-xs">
          <p>Guest Access: 50 API requests • 24-hour session</p>
        </div>
      </div>
    </div>
  );
};

export default LoginScreen