// Error Panel Component - User-friendly error display with suggestions

import React from 'react';
import { AlertCircle, AlertTriangle, Info, RefreshCw } from 'lucide-react';
import { APIResponse } from '../types';

interface ErrorPanelProps {
  error: APIResponse['error'];
  onRetry?: () => void;
}

export function ErrorPanel({ error, onRetry }: ErrorPanelProps) {
  if (!error) return null;

  const getErrorIcon = () => {
    switch (error.type) {
      case 'network':
      case 'timeout':
        return <AlertCircle size={24} className="text-red-500" />;
      case 'auth':
        return <AlertTriangle size={24} className="text-yellow-500" />;
      case 'client':
      case 'server':
      case 'parse':
        return <Info size={24} className="text-orange-500" />;
      default:
        return <AlertCircle size={24} className="text-red-500" />;
    }
  };

  const getErrorColor = () => {
    switch (error.type) {
      case 'auth':
        return 'border-yellow-700 bg-yellow-900/20';
      case 'client':
        return 'border-orange-700 bg-orange-900/20';
      case 'server':
        return 'border-red-700 bg-red-900/20';
      case 'network':
      case 'timeout':
        return 'border-red-700 bg-red-900/20';
      case 'parse':
        return 'border-purple-700 bg-purple-900/20';
      default:
        return 'border-red-700 bg-red-900/20';
    }
  };

  const getSuggestions = () => {
    const suggestions: string[] = [];

    switch (error.type) {
      case 'network':
        suggestions.push('Check your internet connection');
        suggestions.push('Verify the API URL is correct');
        suggestions.push('Check if CORS is blocking the request (see browser console)');
        break;
      case 'timeout':
        suggestions.push('The API might be slow or unresponsive');
        suggestions.push('Try increasing the timeout setting');
        suggestions.push('Check if the API endpoint is correct');
        break;
      case 'auth':
        suggestions.push('Verify your API key or credentials are correct');
        suggestions.push('Check if the authentication type is correct (API Key, Bearer, Basic)');
        suggestions.push('Ensure the API key has the necessary permissions');
        break;
      case 'client':
        if (error.statusCode === 400) {
          suggestions.push('Check if all required parameters are provided');
          suggestions.push('Verify parameter formats and values');
        } else if (error.statusCode === 404) {
          suggestions.push('Check if the endpoint URL is correct');
          suggestions.push('Verify the resource exists');
        } else if (error.statusCode === 429) {
          suggestions.push('You have exceeded the API rate limit');
          suggestions.push('Wait a moment and try again');
        }
        break;
      case 'server':
        suggestions.push('The API server is experiencing issues');
        suggestions.push('Try again in a few moments');
        suggestions.push('Contact the API provider if the issue persists');
        break;
      case 'parse':
        suggestions.push('The API returned data in an unexpected format');
        suggestions.push('Check if the endpoint is correct');
        suggestions.push('Verify the API documentation');
        break;
    }

    return suggestions;
  };

  const getErrorTitle = () => {
    switch (error.type) {
      case 'network':
        return 'Network Error';
      case 'timeout':
        return 'Request Timeout';
      case 'auth':
        return 'Authentication Failed';
      case 'client':
        return `Client Error ${error.statusCode ? `(${error.statusCode})` : ''}`;
      case 'server':
        return `Server Error ${error.statusCode ? `(${error.statusCode})` : ''}`;
      case 'parse':
        return 'Response Parse Error';
      default:
        return 'Error';
    }
  };

  const suggestions = getSuggestions();

  return (
    <div className={`rounded-lg border ${getErrorColor()} p-4`}>
      {/* Header */}
      <div className="flex items-start gap-3 mb-3">
        {getErrorIcon()}
        <div className="flex-1">
          <h3 className="text-sm font-bold text-white mb-1">{getErrorTitle()}</h3>
          <p className="text-xs text-gray-300">{error.message}</p>
        </div>
      </div>

      {/* Status Code */}
      {error.statusCode && (
        <div className="mb-3 text-xs">
          <span className="text-gray-400">Status Code:</span>{' '}
          <span className="font-mono text-white">{error.statusCode}</span>
        </div>
      )}

      {/* Suggestions */}
      {suggestions.length > 0 && (
        <div className="mb-3">
          <h4 className="text-xs font-bold text-orange-500 uppercase mb-2">Suggestions:</h4>
          <ul className="space-y-1">
            {suggestions.map((suggestion, index) => (
              <li key={index} className="text-xs text-gray-300 flex items-start gap-2">
                <span className="text-orange-500 mt-0.5">→</span>
                <span>{suggestion}</span>
              </li>
            ))}
          </ul>
        </div>
      )}

      {/* Details (if available) */}
      {error.details && (
        <details className="mb-3">
          <summary className="text-xs font-bold text-gray-400 cursor-pointer hover:text-white">
            Technical Details
          </summary>
          <pre className="mt-2 text-xs text-gray-500 font-mono whitespace-pre-wrap overflow-auto max-h-32 bg-black/30 p-2 rounded">
            {error.details}
          </pre>
        </details>
      )}

      {/* Retry Button */}
      {onRetry && (
        <button
          onClick={onRetry}
          className="w-full bg-orange-600 hover:bg-orange-700 text-white py-2 rounded text-xs font-bold flex items-center justify-center gap-2 transition-colors"
        >
          <RefreshCw size={14} />
          RETRY REQUEST
        </button>
      )}
    </div>
  );
}
