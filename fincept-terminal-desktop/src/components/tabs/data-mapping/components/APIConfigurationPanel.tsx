// API Configuration Panel - Main interface for configuring API endpoints

import React, { useState, useEffect } from 'react';
import { Play, Eye, EyeOff, AlertCircle } from 'lucide-react';
import { APIConfig, APIAuthConfig, APIResponse } from '../types';
import { KeyValueEditor } from './KeyValueEditor';
import { ErrorPanel } from './ErrorPanel';
import { apiClient } from '../services/APIClient';
import { extractPlaceholders, getURLPreview } from '../utils/urlBuilder';

interface APIConfigurationPanelProps {
  config: APIConfig;
  onChange: (config: APIConfig) => void;
  onTestSuccess: (response: any) => void;
}

export function APIConfigurationPanel({
  config,
  onChange,
  onTestSuccess,
}: APIConfigurationPanelProps) {
  const [testParams, setTestParams] = useState<Record<string, string>>({});
  const [isTesting, setIsTesting] = useState(false);
  const [testError, setTestError] = useState<APIResponse['error'] | null>(null);
  const [testSuccess, setTestSuccess] = useState<any>(null);
  const [showPassword, setShowPassword] = useState(false);

  // Extract required parameters from endpoint and query params
  const requiredParams = [
    ...extractPlaceholders(config.endpoint),
    ...Object.values(config.queryParams).flatMap(v => extractPlaceholders(v)),
  ];
  const uniqueParams = [...new Set(requiredParams)];

  useEffect(() => {
    // Initialize test params
    const params: Record<string, string> = {};
    uniqueParams.forEach(param => {
      params[param] = testParams[param] || '';
    });
    setTestParams(params);
  }, [config.endpoint, JSON.stringify(config.queryParams)]);

  const handleAuthTypeChange = (type: APIAuthConfig['type']) => {
    onChange({
      ...config,
      authentication: {
        type,
        config: type === 'none' ? undefined : {},
      },
    });
  };

  const handleAuthConfigChange = (updates: Partial<APIAuthConfig['config']>) => {
    onChange({
      ...config,
      authentication: {
        ...config.authentication,
        config: {
          ...config.authentication.config,
          ...updates,
        },
      },
    });
  };

  const handleTestRequest = async () => {
    setIsTesting(true);
    setTestError(null);
    setTestSuccess(null);

    try {
      const response = await apiClient.executeRequest(config, testParams);

      if (response.success) {
        setTestSuccess(response.data);
        onTestSuccess(response.data);
        setTestError(null);
      } else {
        setTestError(response.error);
        setTestSuccess(null);
      }
    } catch (error) {
      setTestError({
        type: 'network',
        message: error instanceof Error ? error.message : String(error),
      });
      setTestSuccess(null);
    } finally {
      setIsTesting(false);
    }
  };

  const urlPreview = getURLPreview(config, testParams);

  return (
    <div className="space-y-6">
      {/* Basic Info */}
      <div>
        <h3 className="text-xs font-bold text-orange-500 uppercase mb-3">
          Mapping Details
        </h3>
        <div className="space-y-3">
          <div>
            <label className="text-xs text-gray-400 block mb-1">
              Mapping Name *
            </label>
            <input
              type="text"
              value={config.name}
              onChange={(e) => onChange({ ...config, name: e.target.value })}
              placeholder="My API Mapping"
              className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-sm rounded focus:outline-none focus:border-orange-500"
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 block mb-1">
              Description
            </label>
            <textarea
              value={config.description}
              onChange={(e) => onChange({ ...config, description: e.target.value })}
              placeholder="Brief description of this mapping"
              rows={2}
              className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-sm rounded focus:outline-none focus:border-orange-500 resize-none"
            />
          </div>
        </div>
      </div>

      {/* API Endpoint */}
      <div>
        <h3 className="text-xs font-bold text-orange-500 uppercase mb-3">
          API Endpoint
        </h3>
        <div className="space-y-3">
          <div>
            <label className="text-xs text-gray-400 block mb-1">
              Base URL *
            </label>
            <input
              type="text"
              value={config.baseUrl}
              onChange={(e) => onChange({ ...config, baseUrl: e.target.value })}
              placeholder="https://api.example.com"
              className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-sm rounded focus:outline-none focus:border-orange-500 font-mono"
            />
          </div>

          <div>
            <label className="text-xs text-gray-400 block mb-1">
              Endpoint Path *
            </label>
            <input
              type="text"
              value={config.endpoint}
              onChange={(e) => onChange({ ...config, endpoint: e.target.value })}
              placeholder="/v1/data/{symbol}"
              className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-sm rounded focus:outline-none focus:border-orange-500 font-mono"
            />
            <div className="text-xs text-gray-500 mt-1">
              Use {'{'}placeholder{'}'} for dynamic values
            </div>
          </div>

          <div>
            <label className="text-xs text-gray-400 block mb-1">
              Method *
            </label>
            <select
              value={config.method}
              onChange={(e) =>
                onChange({ ...config, method: e.target.value as any })
              }
              className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-sm rounded focus:outline-none focus:border-orange-500"
            >
              <option value="GET">GET</option>
              <option value="POST">POST</option>
              <option value="PUT">PUT</option>
              <option value="DELETE">DELETE</option>
              <option value="PATCH">PATCH</option>
            </select>
          </div>
        </div>
      </div>

      {/* Authentication */}
      <div>
        <h3 className="text-xs font-bold text-orange-500 uppercase mb-3">
          Authentication
        </h3>
        <div className="space-y-3">
          <select
            value={config.authentication.type}
            onChange={(e) => handleAuthTypeChange(e.target.value as any)}
            className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-sm rounded focus:outline-none focus:border-orange-500"
          >
            <option value="none">No Authentication</option>
            <option value="apikey">API Key</option>
            <option value="bearer">Bearer Token</option>
            <option value="basic">Basic Auth</option>
            <option value="oauth2">OAuth 2.0</option>
          </select>

          {/* API Key Auth */}
          {config.authentication.type === 'apikey' && (
            <div className="space-y-3 p-3 bg-zinc-900 rounded border border-zinc-700">
              <select
                value={config.authentication.config?.apiKey?.location || 'header'}
                onChange={(e) =>
                  handleAuthConfigChange({
                    apiKey: {
                      ...config.authentication.config?.apiKey,
                      location: e.target.value as 'header' | 'query',
                      keyName: config.authentication.config?.apiKey?.keyName || '',
                      value: config.authentication.config?.apiKey?.value || '',
                    },
                  })
                }
                className="w-full bg-zinc-800 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500"
              >
                <option value="header">Header</option>
                <option value="query">Query Parameter</option>
              </select>

              <input
                type="text"
                value={config.authentication.config?.apiKey?.keyName || ''}
                onChange={(e) =>
                  handleAuthConfigChange({
                    apiKey: {
                      ...config.authentication.config?.apiKey,
                      location: config.authentication.config?.apiKey?.location || 'header',
                      keyName: e.target.value,
                      value: config.authentication.config?.apiKey?.value || '',
                    },
                  })
                }
                placeholder="Key name (e.g., X-API-Key, apiKey)"
                className="w-full bg-zinc-800 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500"
              />

              <div className="relative">
                <input
                  type={showPassword ? 'text' : 'password'}
                  value={config.authentication.config?.apiKey?.value || ''}
                  onChange={(e) =>
                    handleAuthConfigChange({
                      apiKey: {
                        ...config.authentication.config?.apiKey,
                        location: config.authentication.config?.apiKey?.location || 'header',
                        keyName: config.authentication.config?.apiKey?.keyName || '',
                        value: e.target.value,
                      },
                    })
                  }
                  placeholder="API Key value"
                  className="w-full bg-zinc-800 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500 pr-10 font-mono"
                />
                <button
                  type="button"
                  onClick={() => setShowPassword(!showPassword)}
                  className="absolute right-2 top-1/2 -translate-y-1/2 text-gray-400 hover:text-white"
                >
                  {showPassword ? <EyeOff size={14} /> : <Eye size={14} />}
                </button>
              </div>
            </div>
          )}

          {/* Bearer Token */}
          {config.authentication.type === 'bearer' && (
            <div className="relative p-3 bg-zinc-900 rounded border border-zinc-700">
              <input
                type={showPassword ? 'text' : 'password'}
                value={config.authentication.config?.bearer?.token || ''}
                onChange={(e) =>
                  handleAuthConfigChange({
                    bearer: { token: e.target.value },
                  })
                }
                placeholder="Bearer token"
                className="w-full bg-zinc-800 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500 pr-10 font-mono"
              />
              <button
                type="button"
                onClick={() => setShowPassword(!showPassword)}
                className="absolute right-5 top-1/2 -translate-y-1/2 text-gray-400 hover:text-white"
              >
                {showPassword ? <EyeOff size={14} /> : <Eye size={14} />}
              </button>
            </div>
          )}

          {/* Basic Auth */}
          {config.authentication.type === 'basic' && (
            <div className="space-y-3 p-3 bg-zinc-900 rounded border border-zinc-700">
              <input
                type="text"
                value={config.authentication.config?.basic?.username || ''}
                onChange={(e) =>
                  handleAuthConfigChange({
                    basic: {
                      username: e.target.value,
                      password: config.authentication.config?.basic?.password || '',
                    },
                  })
                }
                placeholder="Username"
                className="w-full bg-zinc-800 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500"
              />

              <div className="relative">
                <input
                  type={showPassword ? 'text' : 'password'}
                  value={config.authentication.config?.basic?.password || ''}
                  onChange={(e) =>
                    handleAuthConfigChange({
                      basic: {
                        username: config.authentication.config?.basic?.username || '',
                        password: e.target.value,
                      },
                    })
                  }
                  placeholder="Password"
                  className="w-full bg-zinc-800 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500 pr-10"
                />
                <button
                  type="button"
                  onClick={() => setShowPassword(!showPassword)}
                  className="absolute right-2 top-1/2 -translate-y-1/2 text-gray-400 hover:text-white"
                >
                  {showPassword ? <EyeOff size={14} /> : <Eye size={14} />}
                </button>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Headers */}
      <div>
        <h3 className="text-xs font-bold text-orange-500 uppercase mb-3">
          Headers (Optional)
        </h3>
        <KeyValueEditor
          items={config.headers}
          onChange={(headers) => onChange({ ...config, headers })}
          placeholder={{ key: 'Header Name', value: 'Header Value' }}
        />
      </div>

      {/* Query Parameters */}
      <div>
        <h3 className="text-xs font-bold text-orange-500 uppercase mb-3">
          Query Parameters (Optional)
        </h3>
        <KeyValueEditor
          items={config.queryParams}
          onChange={(queryParams) => onChange({ ...config, queryParams })}
          placeholder={{ key: 'Parameter Name', value: 'Parameter Value' }}
          allowPlaceholders
        />
      </div>

      {/* Request Body */}
      {['POST', 'PUT', 'PATCH'].includes(config.method) && (
        <div>
          <h3 className="text-xs font-bold text-orange-500 uppercase mb-3">
            Request Body (JSON)
          </h3>
          <textarea
            value={config.body || ''}
            onChange={(e) => onChange({ ...config, body: e.target.value })}
            placeholder='{"key": "value"}'
            rows={6}
            className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 p-3 text-xs font-mono rounded focus:outline-none focus:border-orange-500 resize-none"
          />
        </div>
      )}

      {/* URL Preview */}
      <div className="p-3 bg-blue-900/20 border border-blue-700 rounded">
        <div className="text-xs font-bold text-blue-400 mb-1">URL PREVIEW</div>
        <div className="text-xs text-blue-300 font-mono break-all">
          {urlPreview}
        </div>
      </div>

      {/* Test Request */}
      {uniqueParams.length > 0 && (
        <div>
          <h3 className="text-xs font-bold text-orange-500 uppercase mb-3">
            Test Parameters
          </h3>
          <div className="space-y-2">
            {uniqueParams.map(param => (
              <div key={param}>
                <label className="text-xs text-gray-400 block mb-1">
                  {param} *
                </label>
                <input
                  type="text"
                  value={testParams[param] || ''}
                  onChange={(e) =>
                    setTestParams({ ...testParams, [param]: e.target.value })
                  }
                  placeholder={`Enter ${param}`}
                  className="w-full bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500 font-mono"
                />
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Test Button */}
      <button
        onClick={handleTestRequest}
        disabled={isTesting || !config.baseUrl || !config.endpoint}
        className={`w-full py-3 rounded font-bold text-sm flex items-center justify-center gap-2 transition-all ${
          isTesting || !config.baseUrl || !config.endpoint
            ? 'bg-zinc-800 text-gray-600 cursor-not-allowed'
            : 'bg-green-600 hover:bg-green-700 text-white shadow-lg'
        }`}
      >
        <Play size={16} />
        {isTesting ? 'TESTING...' : 'FETCH SAMPLE DATA'}
      </button>

      {/* Success Display */}
      {testSuccess && !testError && (
        <div className="bg-green-900/20 border border-green-700 rounded p-4">
          <div className="flex items-center gap-2 mb-3">
            <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse"></div>
            <h4 className="text-xs font-bold text-green-500 uppercase">
              ✓ Sample Data Fetched Successfully
            </h4>
          </div>
          <div className="bg-black/30 rounded p-3 max-h-96 overflow-auto">
            <pre className="text-xs text-green-300 font-mono">
              {JSON.stringify(testSuccess, null, 2)}
            </pre>
          </div>
          <div className="mt-3 text-xs text-green-400">
            {Array.isArray(testSuccess)
              ? `${testSuccess.length} records received`
              : typeof testSuccess === 'object' && testSuccess.data && Array.isArray(testSuccess.data)
              ? `${testSuccess.data.length} records in data array`
              : 'Response received'}
          </div>
        </div>
      )}

      {/* Error Display */}
      {testError && (
        <ErrorPanel error={testError} onRetry={handleTestRequest} />
      )}
    </div>
  );
}
