import React from 'react';
import { InvestmentVerdict } from '@/services/alternativeInvestments/api/types';

interface VerdictPanelProps {
  verdict: InvestmentVerdict;
  className?: string;
}

/**
 * VerdictPanel - Displays investment analysis verdict
 * Shows category (GOOD/BAD/FLAWED/UGLY), rating, and recommendations
 */
export const VerdictPanel: React.FC<VerdictPanelProps> = ({ verdict, className = '' }) => {
  const categoryConfig = getCategoryConfig(verdict.category);

  return (
    <div className={`verdict-panel bg-gray-800 rounded-lg border-2 ${categoryConfig.borderColor} ${className}`}>
      {/* Header with Category Badge */}
      <div className={`p-6 ${categoryConfig.bgColor} border-b-2 ${categoryConfig.borderColor}`}>
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-3">
            <span className="text-4xl">{categoryConfig.icon}</span>
            <div>
              <h3 className="text-2xl font-bold">{verdict.category}</h3>
              <p className="text-sm text-gray-300 mt-1">{verdict.analysis_summary}</p>
            </div>
          </div>
          <div className={`text-3xl font-bold ${categoryConfig.textColor}`}>
            {verdict.rating}
          </div>
        </div>
      </div>

      {/* Key Findings */}
      <div className="p-6 space-y-6">
        <div>
          <h4 className="text-lg font-semibold mb-3 flex items-center gap-2">
            <span>📋</span> Key Findings
          </h4>
          <ul className="space-y-2">
            {verdict.key_findings.map((finding, index) => (
              <li key={index} className="flex items-start gap-2 text-gray-300">
                <span className="text-gray-500 mt-1">•</span>
                <span>{finding}</span>
              </li>
            ))}
          </ul>
        </div>

        {/* Recommendation */}
        <div className={`p-4 rounded-lg ${categoryConfig.recommendationBg} border ${categoryConfig.borderColor}`}>
          <h4 className="font-semibold mb-2 flex items-center gap-2">
            <span>💡</span> Recommendation
          </h4>
          <p className="text-gray-200">{verdict.recommendation}</p>
        </div>

        {/* When Acceptable (if provided) */}
        {verdict.when_acceptable && (
          <div className="p-4 bg-gray-900/50 rounded-lg border border-gray-700">
            <h4 className="font-semibold mb-2 text-yellow-400">⚠️ When It Might Be Acceptable</h4>
            <p className="text-gray-300 text-sm">{verdict.when_acceptable}</p>
          </div>
        )}

        {/* Better Alternatives (if provided) */}
        {verdict.better_alternatives && (
          <div className="p-4 bg-gray-900/50 rounded-lg border border-gray-700">
            <h4 className="font-semibold mb-3 text-green-400">✅ Better Alternatives</h4>
            <p className="text-gray-300 mb-3 font-medium">{verdict.better_alternatives.portfolio}</p>
            <ul className="space-y-1">
              {verdict.better_alternatives.benefits.map((benefit, index) => (
                <li key={index} className="flex items-start gap-2 text-gray-400 text-sm">
                  <span className="text-green-500">→</span>
                  <span>{benefit}</span>
                </li>
              ))}
            </ul>
          </div>
        )}
      </div>
    </div>
  );
};

/**
 * Get configuration for each verdict category
 */
function getCategoryConfig(category: string) {
  const configs = {
    'THE GOOD': {
      icon: '✅',
      borderColor: 'border-green-500',
      bgColor: 'bg-green-900/20',
      textColor: 'text-green-400',
      recommendationBg: 'bg-green-900/20',
    },
    'THE BAD': {
      icon: '⚠️',
      borderColor: 'border-orange-500',
      bgColor: 'bg-orange-900/20',
      textColor: 'text-orange-400',
      recommendationBg: 'bg-orange-900/20',
    },
    'THE FLAWED': {
      icon: '❌',
      borderColor: 'border-yellow-500',
      bgColor: 'bg-yellow-900/20',
      textColor: 'text-yellow-400',
      recommendationBg: 'bg-yellow-900/20',
    },
    'THE UGLY': {
      icon: '🚫',
      borderColor: 'border-red-500',
      bgColor: 'bg-red-900/20',
      textColor: 'text-red-400',
      recommendationBg: 'bg-red-900/20',
    },
    MIXED: {
      icon: '⚖️',
      borderColor: 'border-blue-500',
      bgColor: 'bg-blue-900/20',
      textColor: 'text-blue-400',
      recommendationBg: 'bg-blue-900/20',
    },
  };

  return configs[category as keyof typeof configs] || configs.MIXED;
}

export default VerdictPanel;
