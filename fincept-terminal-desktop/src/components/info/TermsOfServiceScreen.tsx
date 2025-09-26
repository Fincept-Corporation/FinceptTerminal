// File: src/components/info/TermsOfServiceScreen.tsx
// Terms of service screen with legal information and navigation

import React from 'react';
import { ArrowLeft } from "lucide-react";
import { Screen } from '../../App';

interface TermsOfServiceScreenProps {
  onNavigate: (screen: Screen) => void;
}

const TermsOfServiceScreen: React.FC<TermsOfServiceScreenProps> = ({ onNavigate }) => {
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
          <h2 className="text-white text-2xl font-light">Terms of Service</h2>
        </div>
        <p className="text-zinc-400 text-xs leading-5">
          Please read these terms carefully before using Fincept services.
        </p>
      </div>

      <div className="space-y-6 text-sm">
        <div>
          <p className="text-zinc-500 text-xs mb-4">Last updated: January 1, 2025</p>
        </div>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">1. Acceptance of Terms</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              By accessing and using Fincept Terminal ("Service"), you accept and agree to be bound by the terms and provision of this agreement.
            </p>
            <p>
              If you do not agree to abide by the above, please do not use this service.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">2. Description of Service</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              Fincept Terminal is a professional financial analytics and data platform that provides real-time market data, financial analysis tools, and portfolio management capabilities.
            </p>
            <p>
              The Service includes both free guest access with limited features and paid subscription tiers with enhanced functionality.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">3. User Accounts and Registration</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              To access certain features of the Service, you must register for an account. You agree to provide accurate, current, and complete information during the registration process.
            </p>
            <p>
              You are responsible for safeguarding the password and all activities that occur under your account.
            </p>
            <p>
              You must notify us immediately of any unauthorized use of your account.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">4. Acceptable Use Policy</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>You agree not to use the Service to:</p>
            <ul className="list-disc list-inside ml-4 space-y-1 text-zinc-400">
              <li>Violate any applicable laws or regulations</li>
              <li>Transmit any harmful, offensive, or disruptive content</li>
              <li>Attempt to gain unauthorized access to our systems</li>
              <li>Use automated scripts or bots without permission</li>
              <li>Redistribute or resell our data without authorization</li>
            </ul>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">5. Data and Privacy</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              Your privacy is important to us. Our data collection and usage practices are detailed in our Privacy Policy, which forms part of these Terms.
            </p>
            <p>
              Market data provided through the Service is sourced from third-party providers and is subject to their respective terms and conditions.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">6. Subscription and Billing</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              Paid subscriptions are billed in advance on a monthly or annual basis. All fees are non-refundable except as required by law.
            </p>
            <p>
              We reserve the right to change our pricing with 30 days' notice to existing subscribers.
            </p>
            <p>
              Your subscription will automatically renew unless cancelled before the renewal date.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">7. Disclaimers and Limitations</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              The Service is provided "as is" without any warranties, express or implied. We do not guarantee the accuracy, completeness, or timeliness of market data.
            </p>
            <p>
              Investment decisions should not be based solely on information provided by our Service. Always consult with qualified financial advisors.
            </p>
            <p>
              Our liability is limited to the amount paid for the Service in the preceding 12 months.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">8. Termination</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              We may terminate or suspend your account immediately, without prior notice, for conduct that we believe violates these Terms or is harmful to other users, us, or third parties.
            </p>
            <p>
              You may terminate your account at any time by contacting our support team.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">9. Changes to Terms</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              We reserve the right to modify these Terms at any time. We will notify users of significant changes via email or through the Service.
            </p>
            <p>
              Continued use of the Service after changes constitutes acceptance of the new Terms.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">10. Contact Information</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              If you have questions about these Terms, please contact us at:
            </p>
            <div className="bg-zinc-800/50 p-3 rounded mt-2">
              <p>Email: legal@fincept.com</p>
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
            onClick={() => onNavigate('privacyPolicy')}
            className="text-blue-400 hover:text-blue-300 text-xs transition-colors"
          >
            Privacy Policy
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

export default TermsOfServiceScreen;