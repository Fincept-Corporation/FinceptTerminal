// File: src/components/info/ContactUsScreen.tsx
// Contact us screen with contact information and support options

import React from 'react';
import { Button } from "@/components/ui/button";
import { ArrowLeft, Mail, Phone, MapPin, Clock } from "lucide-react";
import { Screen } from '../../App';

interface ContactUsScreenProps {
  onNavigate: (screen: Screen) => void;
}

const ContactUsScreen: React.FC<ContactUsScreenProps> = ({ onNavigate }) => {
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
          <h2 className="text-white text-2xl font-light">Contact Us</h2>
        </div>
        <p className="text-zinc-400 text-xs leading-5">
          Get in touch with our support team for assistance with your Fincept account.
        </p>
      </div>

      <div className="space-y-6">
        <div className="space-y-4">
          <h3 className="text-white text-sm font-medium">Contact Information</h3>

          <div className="space-y-3">
            <div className="flex items-start p-3 bg-zinc-800/50 rounded">
              <Mail className="w-4 h-4 text-zinc-400 mr-3 mt-0.5" />
              <div>
                <div className="text-white text-sm font-medium">Email Support</div>
                <div className="text-zinc-400 text-xs mt-1">support@fincept.com</div>
                <div className="text-zinc-500 text-xs mt-1">Average response time: 4-6 hours</div>
              </div>
            </div>

            <div className="flex items-start p-3 bg-zinc-800/50 rounded">
              <Phone className="w-4 h-4 text-zinc-400 mr-3 mt-0.5" />
              <div>
                <div className="text-white text-sm font-medium">Phone Support</div>
                <div className="text-zinc-400 text-xs mt-1">1-800-FINCEPT (346-2378)</div>
                <div className="text-zinc-500 text-xs mt-1">For urgent technical issues</div>
              </div>
            </div>

            <div className="flex items-start p-3 bg-zinc-800/50 rounded">
              <Clock className="w-4 h-4 text-zinc-400 mr-3 mt-0.5" />
              <div>
                <div className="text-white text-sm font-medium">Support Hours</div>
                <div className="text-zinc-400 text-xs mt-1">Monday - Friday: 9:00 AM - 6:00 PM EST</div>
                <div className="text-zinc-400 text-xs">Saturday: 10:00 AM - 4:00 PM EST</div>
                <div className="text-zinc-500 text-xs mt-1">Emergency support available 24/7</div>
              </div>
            </div>

            <div className="flex items-start p-3 bg-zinc-800/50 rounded">
              <MapPin className="w-4 h-4 text-zinc-400 mr-3 mt-0.5" />
              <div>
                <div className="text-white text-sm font-medium">Office Address</div>
                <div className="text-zinc-400 text-xs mt-1">
                  Fincept LP<br />
                  123 Financial District<br />
                  New York, NY 10004<br />
                  United States
                </div>
              </div>
            </div>
          </div>
        </div>

        <div className="space-y-4 pt-4 border-t border-zinc-700">
          <h3 className="text-white text-sm font-medium">Quick Actions</h3>

          <div className="grid grid-cols-1 gap-2">
            <Button
              onClick={() => alert('Opening live chat...')}
              className="bg-green-600/20 hover:bg-green-600/30 border border-green-600/50 text-green-400 p-3 font-normal transition-colors justify-start"
            >
              <svg className="w-4 h-4 mr-2" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 12h.01M12 12h.01M16 12h.01M21 12c0 4.418-3.582 8-8 8a8.014 8.014 0 01-2.215-.31l-5.297 2.67c-.346.174-.735-.002-.735-.394V17.7a8 8 0 113.467-13.876" />
              </svg>
              <div className="text-left">
                <div className="text-sm">Start Live Chat</div>
                <div className="text-xs text-green-300">Usually responds in minutes</div>
              </div>
            </Button>

            <Button
              onClick={() => window.open('mailto:support@fincept.com', '_blank')}
              className="bg-blue-600/20 hover:bg-blue-600/30 border border-blue-600/50 text-blue-400 p-3 font-normal transition-colors justify-start"
            >
              <Mail className="w-4 h-4 mr-2" />
              <div className="text-left">
                <div className="text-sm">Send Email</div>
                <div className="text-xs text-blue-300">For detailed inquiries</div>
              </div>
            </Button>

            <Button
              onClick={() => window.open('tel:1-800-346-2378', '_blank')}
              className="bg-orange-600/20 hover:bg-orange-600/30 border border-orange-600/50 text-orange-400 p-3 font-normal transition-colors justify-start"
            >
              <Phone className="w-4 h-4 mr-2" />
              <div className="text-left">
                <div className="text-sm">Call Support</div>
                <div className="text-xs text-orange-300">For urgent issues</div>
              </div>
            </Button>
          </div>
        </div>

        <div className="space-y-4 pt-4 border-t border-zinc-700">
          <h3 className="text-white text-sm font-medium">Common Issues</h3>

          <div className="space-y-2">
            <button
              onClick={() => onNavigate('help')}
              className="w-full text-left p-2 bg-zinc-800/30 hover:bg-zinc-800/50 rounded text-xs transition-colors"
            >
              <div className="text-zinc-300">Account login problems</div>
              <div className="text-zinc-500 mt-1">Password reset, account lockouts, 2FA issues</div>
            </button>

            <button
              onClick={() => onNavigate('help')}
              className="w-full text-left p-2 bg-zinc-800/30 hover:bg-zinc-800/50 rounded text-xs transition-colors"
            >
              <div className="text-zinc-300">Technical difficulties</div>
              <div className="text-zinc-500 mt-1">Platform errors, connectivity, performance</div>
            </button>

            <button
              onClick={() => onNavigate('help')}
              className="w-full text-left p-2 bg-zinc-800/30 hover:bg-zinc-800/50 rounded text-xs transition-colors"
            >
              <div className="text-zinc-300">Billing inquiries</div>
              <div className="text-zinc-500 mt-1">Subscription, payments, invoices</div>
            </button>
          </div>
        </div>
      </div>

      <div className="mt-6 pt-4 border-t border-zinc-700">
        <button
          onClick={() => onNavigate('login')}
          className="text-zinc-400 hover:text-white text-xs transition-colors flex items-center"
        >
          <ArrowLeft className="w-3 h-3 mr-1" />
          Back to Login
        </button>
      </div>
    </div>
  );
};

export default ContactUsScreen;