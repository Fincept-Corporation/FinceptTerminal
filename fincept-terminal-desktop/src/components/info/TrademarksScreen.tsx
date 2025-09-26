// File: src/components/info/TrademarksScreen.tsx
// Trademarks screen with intellectual property information

import React from 'react';
import { ArrowLeft } from "lucide-react";
import { Screen } from '../../App';

interface TrademarksScreenProps {
  onNavigate: (screen: Screen) => void;
}

const TrademarksScreen: React.FC<TrademarksScreenProps> = ({ onNavigate }) => {
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
          <h2 className="text-white text-2xl font-light">Trademarks</h2>
        </div>
        <p className="text-zinc-400 text-xs leading-5">
          Information about trademarks and intellectual property rights.
        </p>
      </div>

      <div className="space-y-6 text-sm">
        <div>
          <p className="text-zinc-500 text-xs mb-4">Last updated: January 1, 2025</p>
        </div>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">Fincept Trademarks</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-3">
            <p>
              The following marks are trademarks or registered trademarks of Fincept LP in the United States and other countries:
            </p>

            <div className="bg-zinc-800/50 p-4 rounded space-y-2">
              <div className="flex justify-between">
                <span className="font-medium text-white">FINCEPT®</span>
                <span className="text-zinc-400">Registered Trademark</span>
              </div>
              <div className="flex justify-between">
                <span className="font-medium text-white">FINCEPT TERMINAL™</span>
                <span className="text-zinc-400">Trademark</span>
              </div>
              <div className="flex justify-between">
                <span className="font-medium text-white">B-UNIT™</span>
                <span className="text-zinc-400">Trademark</span>
              </div>
              <div className="flex justify-between">
                <span className="font-medium text-white">FINCEPT ANALYTICS™</span>
                <span className="text-zinc-400">Trademark</span>
              </div>
            </div>

            <p>
              All rights in these trademarks are reserved. Unauthorized use is strictly prohibited.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">Third-Party Trademarks</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-3">
            <p>
              The following are trademarks or registered trademarks of their respective owners:
            </p>

            <div className="space-y-2 text-zinc-400">
              <div className="flex justify-between">
                <span>Bloomberg Terminal®</span>
                <span>Bloomberg Finance L.P.</span>
              </div>
              <div className="flex justify-between">
                <span>Reuters®</span>
                <span>Thomson Reuters Corporation</span>
              </div>
              <div className="flex justify-between">
                <span>S&P 500®</span>
                <span>S&P Global Inc.</span>
              </div>
              <div className="flex justify-between">
                <span>NASDAQ®</span>
                <span>NASDAQ, Inc.</span>
              </div>
              <div className="flex justify-between">
                <span>NYSE®</span>
                <span>NYSE Group, Inc.</span>
              </div>
              <div className="flex justify-between">
                <span>FTSE®</span>
                <span>FTSE International Limited</span>
              </div>
            </div>

            <p>
              All third-party trademarks mentioned are the property of their respective owners. Use of these marks does not imply endorsement by or affiliation with Fincept.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">Trademark Guidelines</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-3">
            <p>
              If you wish to use any Fincept trademark, please follow these guidelines:
            </p>

            <div className="space-y-2">
              <h4 className="text-white text-sm font-medium">Permitted Uses:</h4>
              <ul className="list-disc list-inside ml-4 space-y-1 text-zinc-400">
                <li>Referential use to identify Fincept products or services</li>
                <li>Academic or journalistic references with proper attribution</li>
                <li>Use in comparative advertising when truthful and not misleading</li>
              </ul>

              <h4 className="text-white text-sm font-medium mt-4">Prohibited Uses:</h4>
              <ul className="list-disc list-inside ml-4 space-y-1 text-zinc-400">
                <li>Use as part of your own trademark or business name</li>
                <li>Use that implies sponsorship, endorsement, or affiliation</li>
                <li>Modification, alteration, or combination with other marks</li>
                <li>Use in a way that disparages or damages Fincept's reputation</li>
              </ul>
            </div>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">Copyright Notice</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              © 2025 Fincept LP. All rights reserved.
            </p>
            <p>
              The content, features, and functionality of Fincept Terminal are protected by copyright, trademark, patent, and other intellectual property laws.
            </p>
            <p>
              Reproduction, modification, or distribution of any content without written permission is strictly prohibited.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">Data Provider Acknowledgments</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-3">
            <p>
              Market data and financial information are provided by various third-party sources. Each data provider retains all rights to their content:
            </p>

            <div className="bg-zinc-800/50 p-3 rounded text-zinc-400 text-xs space-y-1">
              <p>• Market data © respective exchanges and data providers</p>
              <p>• Financial data © FactSet Research Systems Inc.</p>
              <p>• News content © respective news organizations</p>
              <p>• Economic indicators © government statistical agencies</p>
              <p>• Corporate information © respective companies and filing authorities</p>
            </div>

            <p>
              Users must comply with all third-party data provider terms and conditions.
            </p>
          </div>
        </section>

        <section>
          <h3 className="text-white text-lg font-medium mb-3">Reporting Trademark Infringement</h3>
          <div className="text-zinc-300 text-xs leading-relaxed space-y-2">
            <p>
              If you believe your trademark rights have been infringed, or if you discover unauthorized use of Fincept trademarks, please contact us:
            </p>
            <div className="bg-zinc-800/50 p-3 rounded mt-2">
              <p><strong>Legal Department</strong></p>
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
            onClick={() => onNavigate('termsOfService')}
            className="text-blue-400 hover:text-blue-300 text-xs transition-colors"
          >
            Terms of Service
          </button>
          <button
            onClick={() => onNavigate('privacyPolicy')}
            className="text-blue-400 hover:text-blue-300 text-xs transition-colors"
          >
            Privacy Policy
          </button>
        </div>
      </div>
    </div>
  );
};

export default TrademarksScreen;