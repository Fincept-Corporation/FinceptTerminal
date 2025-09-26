// File: src/components/info/PrivacyPolicyScreen.tsx
// Privacy policy screen with data protection and usage information

import React from 'react';
import { ArrowLeft, Shield, Eye, Lock, Share } from "lucide-react";
import { Screen } from '../../App';

interface PrivacyPolicyScreenProps {
  onNavigate: (screen: Screen) => void;
}

const PrivacyPolicyScreen: React.FC<PrivacyPolicyScreenProps> = ({ onNavigate }) => {
  return (
    <div className="bg-zinc-900/90 backdrop-blur-sm border border-zinc-700 rounded-lg p-6 w-full max-w-2xl mx-4 shadow-2xl max-h-[80vh] overflow-y-auto">
      <div className="mb-6">
        <div className="flex items-center mb-3">
          <button
            onClick={() => onNavigate('login')}
            className="text-zinc-400 hover:text-white transition-colors mr-3"
          >
            <ArrowLeft className="h-4 w-4" />
          </button>
          <h2 className="text-white text-2xl font-light">Privacy Policy</h2>
        </div>
        <p className="text-zinc-400 text-xs leading-5">
          Learn how we collect, use, and protect your personal information.
        </p>
      </div>

      <div className="space-y-6 text-sm">
        <div>
          <p className="text-zinc-500 text-xs mb-4">Last updated: January 1, 2025</p>
        </div>

        <section>
          <h3 className="text-white text-lg font-medium mb-3 flex items-center">
            <Shield className="w-4 h-4 mr-2" />
            Our Commitment to Privacy
          </h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              At Fincept, we are committed to protecting your privacy and ensuring the security of your personal information. This Privacy Policy explains how we collect, use, and safeguard your data when you use our services.
            </p>
            <p>
              We believe in transparency and will never sell, rent, or trade your personal information to third parties for marketing purposes.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3 flex items-center">
            <Eye className="w-4 h-4 mr-2" />
            Information We Collect
          </h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-3">
            <div>
              <h4 className="text-white text-sm font-medium mb-2">Personal Information</h4>
              <ul className="list-disc list-inside ml-4 space-y-1 text-zinc-400">
                <li>Name, email address, and phone number</li>
                <li>Company information and job title</li>
                <li>Account credentials and authentication data</li>
                <li>Billing and payment information</li>
                <li>Communication preferences</li>
              </ul>
            </div>

            <div>
              <h4 className="text-white text-sm font-medium mb-2">Usage Information</h4>
              <ul className="list-disc list-inside ml-4 space-y-1 text-zinc-400">
                <li>Terminal usage patterns and feature interactions</li>
                <li>Search queries and data requests</li>
                <li>Session duration and frequency of use</li>
                <li>Device information and browser type</li>
                <li>IP address and location data</li>
              </ul>
            </div>

            <div>
              <h4 className="text-white text-sm font-medium mb-2">Financial Data</h4>
              <ul className="list-disc list-inside ml-4 space-y-1 text-zinc-400">
                <li>Portfolio compositions and holdings</li>
                <li>Trading preferences and strategies</li>
                <li>Watchlists and saved analyses</li>
                <li>Custom dashboard configurations</li>
              </ul>
            </div>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3 flex items-center">
            <Lock className="w-4 h-4 mr-2" />
            How We Use Your Information
          </h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-3">
            <p>We use your information to:</p>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div className="bg-zinc-800/50 p-3 rounded">
                <h4 className="text-white text-sm font-medium mb-2">Service Delivery</h4>
                <ul className="text-zinc-400 text-xs space-y-1">
                  <li>• Provide terminal access and functionality</li>
                  <li>• Personalize your user experience</li>
                  <li>• Process transactions and billing</li>
                  <li>• Deliver customer support</li>
                </ul>
              </div>

              <div className="bg-zinc-800/50 p-3 rounded">
                <h4 className="text-white text-sm font-medium mb-2">Security & Compliance</h4>
                <ul className="text-zinc-400 text-xs space-y-1">
                  <li>• Verify identity and prevent fraud</li>
                  <li>• Comply with regulatory requirements</li>
                  <li>• Monitor for suspicious activity</li>
                  <li>• Maintain audit trails</li>
                </ul>
              </div>

              <div className="bg-zinc-800/50 p-3 rounded">
                <h4 className="text-white text-sm font-medium mb-2">Communication</h4>
                <ul className="text-zinc-400 text-xs space-y-1">
                  <li>• Send service notifications</li>
                  <li>• Provide technical updates</li>
                  <li>• Share relevant market insights</li>
                  <li>• Respond to inquiries</li>
                </ul>
              </div>

              <div className="bg-zinc-800/50 p-3 rounded">
                <h4 className="text-white text-sm font-medium mb-2">Improvement</h4>
                <ul className="text-zinc-400 text-xs space-y-1">
                  <li>• Analyze usage patterns</li>
                  <li>• Improve platform performance</li>
                  <li>• Develop new features</li>
                  <li>• Enhance security measures</li>
                </ul>
              </div>
            </div>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3 flex items-center">
            <Share className="w-4 h-4 mr-2" />
            Information Sharing
          </h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>We may share your information in the following circumstances:</p>

            <div className="space-y-2">
              <div className="bg-zinc-800/30 p-2 rounded">
                <span className="text-white font-medium">Service Providers:</span>
                <span className="text-zinc-400 ml-2">With trusted third parties who help deliver our services</span>
              </div>

              <div className="bg-zinc-800/30 p-2 rounded">
                <span className="text-white font-medium">Legal Requirements:</span>
                <span className="text-zinc-400 ml-2">When required by law, regulation, or legal process</span>
              </div>

              <div className="bg-zinc-800/30 p-2 rounded">
                <span className="text-white font-medium">Business Transfer:</span>
                <span className="text-zinc-400 ml-2">In connection with a merger, acquisition, or sale of assets</span>
              </div>

              <div className="bg-zinc-800/30 p-2 rounded">
                <span className="text-white font-medium">Consent:</span>
                <span className="text-zinc-400 ml-2">When you explicitly consent to sharing</span>
              </div>
            </div>

            <p className="mt-3 text-zinc-400">
              We never sell personal information to data brokers or advertising companies.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">Data Security</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>We implement industry-standard security measures to protect your data:</p>

            <div className="bg-zinc-800/50 p-3 rounded space-y-1 text-zinc-400 text-xs">
              <p>• End-to-end encryption for data transmission</p>
              <p>• AES-256 encryption for data storage</p>
              <p>• Multi-factor authentication requirements</p>
              <p>• Regular security audits and penetration testing</p>
              <p>• SOC 2 Type II compliance</p>
              <p>• 24/7 security monitoring</p>
            </div>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">Your Rights</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>You have the following rights regarding your personal data:</p>

            <div className="space-y-2">
              <div className="flex items-start">
                <span className="text-green-400 mr-2">•</span>
                <span><strong>Access:</strong> Request a copy of your personal data</span>
              </div>
              <div className="flex items-start">
                <span className="text-green-400 mr-2">•</span>
                <span><strong>Correction:</strong> Update inaccurate or incomplete information</span>
              </div>
              <div className="flex items-start">
                <span className="text-green-400 mr-2">•</span>
                <span><strong>Deletion:</strong> Request deletion of your personal data</span>
              </div>
              <div className="flex items-start">
                <span className="text-green-400 mr-2">•</span>
                <span><strong>Portability:</strong> Receive your data in a machine-readable format</span>
              </div>
              <div className="flex items-start">
                <span className="text-green-400 mr-2">•</span>
                <span><strong>Opt-out:</strong> Withdraw consent for data processing</span>
              </div>
            </div>

            <p className="mt-3">
              To exercise these rights, contact us at privacy@fincept.com or through your account settings.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">Contact Us</h3>
          <div className="text-zinc-300 text-xs leading-relaxed">
            <p className="mb-3">
              If you have questions about this Privacy Policy or our data practices:
            </p>
            <div className="bg-zinc-800/50 p-3 rounded">
              <p><strong>Privacy Officer</strong></p>
              <p>Email: privacy@fincept.com</p>
              <p>Phone: 1-800-FINCEPT (346-2378)</p>
              <p>Address: Fincept LP, 123 Financial District, New York, NY 10004</p>
            </div>
          </div>
        </section>
      </div>

      <div className="mt-8 pt-4 border-t border-zinc-700 flex justify-between items-center">
        <button
          onClick={() => onNavigate('login')}
          className="text-zinc-400 hover:text-white text-xs transition-colors flex items-center"
        >
          <ArrowLeft className="w-3 h-3 mr-1" />
          Back to Login
        </button>

        <div className="flex space-x-4">
          <button
            onClick={() => onNavigate('termsOfService')}
            className="text-blue-400 hover:text-blue-300 text-xs transition-colors"
          >
            Terms of Service
          </button>
          <button
            onClick={() => onNavigate('contactUs')}
            className="text-blue-400 hover:text-blue-300 text-xs transition-colors"
          >
            Contact Us
          </button>
        </div>
      </div>
    </div>
  );
};

export default PrivacyPolicyScreen;