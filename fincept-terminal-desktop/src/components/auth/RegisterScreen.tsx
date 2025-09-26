// File: src/components/auth/RegisterScreen.tsx
// User registration screen with complete form validation

import React, { useState } from 'react';
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { ArrowLeft, User, Mail, Phone, Building, Lock } from "lucide-react";
import { Screen } from '../../App';

interface RegisterScreenProps {
  onNavigate: (screen: Screen) => void;
}

const RegisterScreen: React.FC<RegisterScreenProps> = ({ onNavigate }) => {
  const [formData, setFormData] = useState({
    firstName: "",
    lastName: "",
    email: "",
    phone: "",
    company: "",
    password: "",
    confirmPassword: ""
  });
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState("");

  const handleChange = (field: string, value: string) => {
    setFormData(prev => ({ ...prev, [field]: value }));
    if (error) setError("");
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!formData.firstName || !formData.lastName || !formData.email || !formData.password || !formData.confirmPassword) {
      setError("Please fill in all required fields");
      return;
    }

    if (formData.password !== formData.confirmPassword) {
      setError("Passwords do not match");
      return;
    }

    setIsLoading(true);
    setError("");

    try {
      // TODO: Replace with actual API call
      setTimeout(() => {
        console.log('Registration attempt:', formData);
        // Handle successful registration here
        setIsLoading(false);
      }, 2000);
    } catch (err) {
      setError("An unexpected error occurred");
      setIsLoading(false);
    }
  };

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
          <h2 className="text-white text-2xl font-light">Create Account</h2>
        </div>
        <p className="text-zinc-400 text-xs leading-5">
          Join Fincept to access professional financial terminal and analytics platform.
        </p>
      </div>

      <form onSubmit={handleSubmit} className="space-y-4">
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
            />
          </div>
        </div>

        <div className="space-y-1">
          <Label htmlFor="phone" className="text-white text-xs">
            Phone Number
          </Label>
          <div className="relative">
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
                Creating...
              </div>
            ) : "Create Account"}
          </Button>
        </div>
      </form>

      <div className="mt-5 pt-4 border-t border-zinc-700 text-center">
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
    </div>
  );
};

export default RegisterScreen;