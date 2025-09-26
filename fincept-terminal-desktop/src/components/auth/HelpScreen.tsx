// File: src/components/auth/HelpScreen.tsx
// Help center screen with FAQ, quick actions, and contact information

import React from 'react';
import { Button } from "@/components/ui/button";
import { ArrowLeft, Mail, Phone } from "lucide-react";
import { Screen } from '../../App';

interface HelpScreenProps {
  onNavigate: (screen: Screen) => void;
}

const HelpScreen: React.FC<HelpScreenProps> = ({ onNavigate }) => {
  return (
    <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-lg mx-4 shadow-2xl">
      <div className="mb-6">
        <div className="flex items-center mb-3">
          <button
            onClick={() => onNavigate('login')}
            className="text-zinc-400 hover:text-white transition-colors mr-3"
          >
            <ArrowLeft className="h-4 w-4" />
          </button>
          <h2 className="text-white text-2xl font-light">Help Center</h2>
        </div>
        <p className="text-zinc-400 text-xs leading-5">
          Find answers to common questions and get support for your Fincept account.
        </p>
      </div>

      <div className="space-y-4">
        <div className="space-y-3">
          <h3 className="text-white text-sm font-medium">Frequently Asked Questions</h3>

          <div className="space-y-2">
            <details className="group">
              <summary className="text-zinc-300 text-xs cursor-pointer hover:text-white transition-colors list-none flex items-center justify-between p-2 bg-zinc-800/50 rounded">
                <span>How do I reset my password?</span>
                <svg className="w-3 h-3 group-open:rotate-180 transition-transform" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
                </svg>
              </summary>
              <div className="mt-2 p-2 text-zinc-400 text-xs leading-relaxed">
                Click "Forgot Password" on the login screen, enter your email, and follow the reset link we send you.
              </div>
            </details>

            <details className="group">
              <summary className="text-zinc-300 text-xs cursor-pointer hover:text-white transition-colors list-none flex items-center justify-between p-2 bg-zinc-800/50 rounded">
                <span>What is Guest Access?</span>
                <svg className="w-3 h-3 group-open:rotate-180 transition-transform" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
                </svg>
              </summary>
              <div className="mt-2 p-2 text-zinc-400 text-xs leading-relaxed">
                Guest Access allows you to explore Fincept with 50 API requests and 24-hour access without creating an account.
              </div>
            </details>

            <details className="group">
              <summary className="text-zinc-300 text-xs cursor-pointer hover:text-white transition-colors list-none flex items-center justify-between p-2 bg-zinc-800/50 rounded">
                <span>How do I contact support?</span>
                <svg className="w-3 h-3 group-open:rotate-180 transition-transform" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
                </svg>
              </summary>
              <div className="mt-2 p-2 text-zinc-400 text-xs leading-relaxed">
                You can reach our support team at support@fincept.com or through the Contact Us link in the footer.
              </div>
            </details>

            <details className="group">
              <summary className="text-zinc-300 text-xs cursor-pointer hover:text-white transition-colors list-none flex items-center justify-between p-2 bg-zinc-800/50 rounded">
                <span>What is a B-Unit?</span>
                <svg className="w-3 h-3 group-open:rotate-180 transition-transform" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
                </svg>
              </summary>
              <div className="mt-2 p-2 text-zinc-400 text-xs leading-relaxed">
                B-Unit is our biometric authentication device that provides an additional layer of security for your terminal access.
              </div>
            </details>

            <details className="group">
              <summary className="text-zinc-300 text-xs cursor-pointer hover:text-white transition-colors list-none flex items-center justify-between p-2 bg-zinc-800/50 rounded">
                <span>System Requirements</span>
                <svg className="w-3 h-3 group-open:rotate-180 transition-transform" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
                </svg>
              </summary>
              <div className="mt-2 p-2 text-zinc-400 text-xs leading-relaxed">
                Fincept Terminal works on modern web browsers. We recommend Chrome, Firefox, Safari, or Edge for the best experience.
              </div>
            </details>
          </div>
        </div>

        <div className="space-y-3 pt-4 border-t border-zinc-700">
          <h3 className="text-white text-sm font-medium">Quick Actions</h3>
          <div className="grid grid-cols-2 gap-2">
            <button
              onClick={() => onNavigate('register')}
              className="bg-zinc-800 hover:bg-zinc-700 text-white p-3 rounded text-xs text-left transition-colors"
            >
              <div className="font-medium mb-1">Create Account</div>
              <div className="text-zinc-400">Get started with Fincept</div>
            </button>

            <button
              onClick={() => onNavigate('forgotPassword')}
              className="bg-zinc-800 hover:bg-zinc-700 text-white p-3 rounded text-xs text-left transition-colors"
            >
              <div className="font-medium mb-1">Reset Password</div>
              <div className="text-zinc-400">Recover your account</div>
            </button>
          </div>
        </div>

        <div className="space-y-3 pt-4 border-t border-zinc-700">
          <h3 className="text-white text-sm font-medium">Contact Information</h3>
          <div className="space-y-2 text-xs">
            <div className="flex items-center text-zinc-400">
              <Mail className="w-3 h-3 mr-2" />
              <span>support@fincept.com</span>
            </div>
            <div className="flex items-center text-zinc-400">
              <Phone className="w-3 h-3 mr-2" />
              <span>1-800-FINCEPT (346-2378)</span>
            </div>
            <div className="flex items-start text-zinc-400">
              <svg className="w-3 h-3 mr-2 mt-0.5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z" />
              </svg>
              <span>Support Hours: Mon-Fri 9AM-6PM EST</span>
            </div>
          </div>
        </div>
      </div>

      <div className="mt-6 pt-4 border-t border-zinc-700">
        <div className="flex justify-between">
          <button
            onClick={() => onNavigate('login')}
            className="text-zinc-400 hover:text-white text-xs transition-colors flex items-center"
          >
            <ArrowLeft className="w-3 h-3 mr-1" />
            Back to Login
          </button>

          <Button
            onClick={() => alert('Opening live chat...')}
            className="bg-blue-600/20 hover:bg-blue-600/30 border border-blue-600/50 text-blue-400 px-3 py-1 text-xs font-normal transition-colors"
          >
            Live Chat
          </Button>
        </div>
      </div>
    </div>
  );
};

export default HelpScreen;