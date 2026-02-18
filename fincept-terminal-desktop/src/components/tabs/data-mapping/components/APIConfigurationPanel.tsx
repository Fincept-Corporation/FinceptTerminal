// API Configuration Panel - Terminal UI/UX

import React, { useState, useEffect } from 'react';
import { Play, Eye, EyeOff, AlertCircle, Network, Key, Globe, Code, CheckCircle2, RefreshCw, Copy } from 'lucide-react';
import { APIConfig, APIAuthConfig, APIResponse } from '../types';
import { KeyValueEditor } from './KeyValueEditor';
import { ErrorPanel } from './ErrorPanel';
import { apiClient } from '../services/APIClient';
import { extractPlaceholders, getURLPreview } from '../utils/urlBuilder';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface APIConfigurationPanelProps {
  config: APIConfig;
  onChange: (config: APIConfig) => void;
  onTestSuccess: (response: any) => void;
}

const SECTION_HEADER_STYLE = (colors: any) => ({
  color: colors.primary,
  borderBottom: `1px solid var(--ft-border-color, #2A2A2A)`,
  paddingBottom: '6px',
  marginBottom: '12px',
});

const INPUT_STYLE = (colors: any, focused?: boolean) => ({
  backgroundColor: colors.background,
  border: `1px solid ${focused ? colors.primary : 'var(--ft-border-color, #2A2A2A)'}`,
  color: colors.text,
  outline: 'none',
});

export function APIConfigurationPanel({ config, onChange, onTestSuccess }: APIConfigurationPanelProps) {
  const { colors } = useTerminalTheme();
  const [testParams, setTestParams] = useState<Record<string, string>>({});
  const [isTesting, setIsTesting] = useState(false);
  const [testError, setTestError] = useState<APIResponse['error'] | null>(null);
  const [testSuccess, setTestSuccess] = useState<any>(null);
  const [showPassword, setShowPassword] = useState(false);
  const [focusedField, setFocusedField] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);

  const requiredParams = [
    ...extractPlaceholders(config.endpoint),
    ...Object.values(config.queryParams).flatMap(v => extractPlaceholders(v)),
  ];
  const uniqueParams = [...new Set(requiredParams)];

  useEffect(() => {
    const params: Record<string, string> = {};
    uniqueParams.forEach(param => { params[param] = testParams[param] || ''; });
    setTestParams(params);
  }, [config.endpoint, JSON.stringify(config.queryParams)]);

  const handleAuthTypeChange = (type: APIAuthConfig['type']) => {
    onChange({ ...config, authentication: { type, config: type === 'none' ? undefined : {} } });
  };

  const handleAuthConfigChange = (updates: Partial<APIAuthConfig['config']>) => {
    onChange({ ...config, authentication: { ...config.authentication, config: { ...config.authentication.config, ...updates } } });
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
      setTestError({ type: 'network', message: error instanceof Error ? error.message : String(error) });
      setTestSuccess(null);
    } finally {
      setIsTesting(false);
    }
  };

  const urlPreview = getURLPreview(config, testParams);

  const copyUrl = () => {
    navigator.clipboard.writeText(urlPreview);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const methodColors: Record<string, string> = {
    GET: colors.success,
    POST: colors.primary,
    PUT: colors.warning || '#F59E0B',
    DELETE: colors.alert,
    PATCH: colors.info || '#0088FF',
  };

  const inputClass = "w-full px-3 py-2 text-xs font-mono transition-all";
  const sectionClass = "space-y-3";

  return (
    <div className="space-y-5 max-w-3xl">

      {/* ── Section: Mapping Details ── */}
      <div>
        <div className="flex items-center gap-2 text-xs font-bold tracking-wider pb-1.5 mb-3" style={SECTION_HEADER_STYLE(colors)}>
          <Code size={13} color={colors.primary} />
          MAPPING DETAILS
        </div>
        <div className={sectionClass}>
          <div>
            <label className="text-[11px] font-bold tracking-wider block mb-1.5" style={{ color: colors.text }}>
              MAPPING NAME <span style={{ color: colors.alert }}>*</span>
            </label>
            <input
              type="text"
              value={config.name}
              onChange={(e) => onChange({ ...config, name: e.target.value })}
              placeholder="My API Mapping"
              className={inputClass}
              style={INPUT_STYLE(colors, focusedField === 'name')}
              onFocus={() => setFocusedField('name')}
              onBlur={() => setFocusedField(null)}
            />
          </div>
          <div>
            <label className="text-[11px] font-bold tracking-wider block mb-1.5" style={{ color: colors.text }}>DESCRIPTION</label>
            <textarea
              value={config.description}
              onChange={(e) => onChange({ ...config, description: e.target.value })}
              placeholder="Brief description of this mapping"
              rows={2}
              className={`${inputClass} resize-none`}
              style={INPUT_STYLE(colors, focusedField === 'desc')}
              onFocus={() => setFocusedField('desc')}
              onBlur={() => setFocusedField(null)}
            />
          </div>
        </div>
      </div>

      {/* ── Section: API Endpoint ── */}
      <div>
        <div className="flex items-center gap-2 text-xs font-bold tracking-wider pb-1.5 mb-3" style={SECTION_HEADER_STYLE(colors)}>
          <Globe size={13} color={colors.primary} />
          API ENDPOINT
        </div>
        <div className={sectionClass}>
          {/* Method + Base URL row */}
          <div className="flex gap-2">
            <div className="flex-shrink-0">
              <label className="text-[11px] font-bold tracking-wider block mb-1.5" style={{ color: colors.text }}>METHOD</label>
              <select
                value={config.method}
                onChange={(e) => onChange({ ...config, method: e.target.value as any })}
                className="px-3 py-2 text-xs font-bold font-mono transition-all w-24"
                style={{
                  backgroundColor: colors.background,
                  border: `1px solid var(--ft-border-color, #2A2A2A)`,
                  color: methodColors[config.method] || colors.text,
                  outline: 'none',
                }}
              >
                {['GET', 'POST', 'PUT', 'DELETE', 'PATCH'].map(m => (
                  <option key={m} value={m} style={{ color: methodColors[m] }}>{m}</option>
                ))}
              </select>
            </div>
            <div className="flex-1">
              <label className="text-[11px] font-bold tracking-wider block mb-1.5" style={{ color: colors.text }}>
                BASE URL <span style={{ color: colors.alert }}>*</span>
              </label>
              <input
                type="text"
                value={config.baseUrl}
                onChange={(e) => onChange({ ...config, baseUrl: e.target.value })}
                placeholder="https://api.example.com"
                className={inputClass}
                style={INPUT_STYLE(colors, focusedField === 'baseUrl')}
                onFocus={() => setFocusedField('baseUrl')}
                onBlur={() => setFocusedField(null)}
              />
            </div>
          </div>

          <div>
            <label className="text-[11px] font-bold tracking-wider block mb-1.5" style={{ color: colors.text }}>
              ENDPOINT PATH <span style={{ color: colors.alert }}>*</span>
            </label>
            <input
              type="text"
              value={config.endpoint}
              onChange={(e) => onChange({ ...config, endpoint: e.target.value })}
              placeholder="/v1/data/{symbol}"
              className={inputClass}
              style={INPUT_STYLE(colors, focusedField === 'endpoint')}
              onFocus={() => setFocusedField('endpoint')}
              onBlur={() => setFocusedField(null)}
            />
            <div className="text-[10px] mt-1" style={{ color: colors.textMuted }}>
              Use <span style={{ color: colors.primary }} className="font-mono">{'{placeholder}'}</span> for dynamic values
            </div>
          </div>

          {/* URL Preview */}
          <div className="p-3" style={{ backgroundColor: colors.background, border: `1px solid ${colors.info || '#0088FF'}40` }}>
            <div className="flex items-center justify-between mb-1.5">
              <span className="text-[10px] font-bold tracking-wider" style={{ color: colors.info || '#0088FF' }}>URL PREVIEW</span>
              <button
                onClick={copyUrl}
                className="flex items-center gap-1 text-[10px] px-2 py-0.5 transition-all"
                style={{ color: copied ? colors.success : colors.textMuted, backgroundColor: 'transparent' }}
                title="Copy URL"
              >
                {copied ? <CheckCircle2 size={11} /> : <Copy size={11} />}
                {copied ? 'COPIED' : 'COPY'}
              </button>
            </div>
            <div className="text-[11px] font-mono break-all" style={{ color: colors.info || '#0088FF' }}>
              <span style={{ color: methodColors[config.method] || colors.primary }} className="font-bold mr-2">
                {config.method}
              </span>
              {urlPreview}
            </div>
          </div>
        </div>
      </div>

      {/* ── Section: Authentication ── */}
      <div>
        <div className="flex items-center gap-2 text-xs font-bold tracking-wider pb-1.5 mb-3" style={SECTION_HEADER_STYLE(colors)}>
          <Key size={13} color={colors.primary} />
          AUTHENTICATION
        </div>
        <div className={sectionClass}>
          <div>
            <label className="text-[11px] font-bold tracking-wider block mb-1.5" style={{ color: colors.text }}>AUTH TYPE</label>
            <select
              value={config.authentication.type}
              onChange={(e) => handleAuthTypeChange(e.target.value as any)}
              className={`${inputClass} w-full`}
              style={INPUT_STYLE(colors)}
            >
              <option value="none">No Authentication</option>
              <option value="apikey">API Key</option>
              <option value="bearer">Bearer Token</option>
              <option value="basic">Basic Auth</option>
              <option value="oauth2">OAuth 2.0</option>
            </select>
          </div>

          {/* API Key */}
          {config.authentication.type === 'apikey' && (
            <div className="space-y-2 p-3" style={{ backgroundColor: `${colors.primary}08`, border: `1px solid ${colors.primary}30` }}>
              <div className="text-[10px] font-bold tracking-wider mb-2" style={{ color: colors.primary }}>API KEY CONFIG</div>
              <select
                value={config.authentication.config?.apiKey?.location || 'header'}
                onChange={(e) => handleAuthConfigChange({ apiKey: { ...config.authentication.config?.apiKey, location: e.target.value as 'header' | 'query', keyName: config.authentication.config?.apiKey?.keyName || '', value: config.authentication.config?.apiKey?.value || '' } })}
                className={`${inputClass} w-full`}
                style={INPUT_STYLE(colors)}
              >
                <option value="header">Header</option>
                <option value="query">Query Parameter</option>
              </select>
              <input
                type="text"
                value={config.authentication.config?.apiKey?.keyName || ''}
                onChange={(e) => handleAuthConfigChange({ apiKey: { ...config.authentication.config?.apiKey, location: config.authentication.config?.apiKey?.location || 'header', keyName: e.target.value, value: config.authentication.config?.apiKey?.value || '' } })}
                placeholder="Key name (e.g., X-API-Key)"
                className={inputClass}
                style={INPUT_STYLE(colors, focusedField === 'keyName')}
                onFocus={() => setFocusedField('keyName')}
                onBlur={() => setFocusedField(null)}
              />
              <div className="relative">
                <input
                  type={showPassword ? 'text' : 'password'}
                  value={config.authentication.config?.apiKey?.value || ''}
                  onChange={(e) => handleAuthConfigChange({ apiKey: { ...config.authentication.config?.apiKey, location: config.authentication.config?.apiKey?.location || 'header', keyName: config.authentication.config?.apiKey?.keyName || '', value: e.target.value } })}
                  placeholder="API Key value"
                  className={`${inputClass} pr-10`}
                  style={INPUT_STYLE(colors, focusedField === 'apiKeyVal')}
                  onFocus={() => setFocusedField('apiKeyVal')}
                  onBlur={() => setFocusedField(null)}
                />
                <button type="button" onClick={() => setShowPassword(!showPassword)} className="absolute right-2 top-1/2 -translate-y-1/2" style={{ color: colors.textMuted }}>
                  {showPassword ? <EyeOff size={13} /> : <Eye size={13} />}
                </button>
              </div>
            </div>
          )}

          {/* Bearer Token */}
          {config.authentication.type === 'bearer' && (
            <div className="p-3" style={{ backgroundColor: `${colors.primary}08`, border: `1px solid ${colors.primary}30` }}>
              <div className="text-[10px] font-bold tracking-wider mb-2" style={{ color: colors.primary }}>BEARER TOKEN</div>
              <div className="relative">
                <input
                  type={showPassword ? 'text' : 'password'}
                  value={config.authentication.config?.bearer?.token || ''}
                  onChange={(e) => handleAuthConfigChange({ bearer: { token: e.target.value } })}
                  placeholder="Bearer token"
                  className={`${inputClass} pr-10`}
                  style={INPUT_STYLE(colors, focusedField === 'bearer')}
                  onFocus={() => setFocusedField('bearer')}
                  onBlur={() => setFocusedField(null)}
                />
                <button type="button" onClick={() => setShowPassword(!showPassword)} className="absolute right-2 top-1/2 -translate-y-1/2" style={{ color: colors.textMuted }}>
                  {showPassword ? <EyeOff size={13} /> : <Eye size={13} />}
                </button>
              </div>
            </div>
          )}

          {/* Basic Auth */}
          {config.authentication.type === 'basic' && (
            <div className="space-y-2 p-3" style={{ backgroundColor: `${colors.primary}08`, border: `1px solid ${colors.primary}30` }}>
              <div className="text-[10px] font-bold tracking-wider mb-2" style={{ color: colors.primary }}>BASIC AUTH</div>
              <input
                type="text"
                value={config.authentication.config?.basic?.username || ''}
                onChange={(e) => handleAuthConfigChange({ basic: { username: e.target.value, password: config.authentication.config?.basic?.password || '' } })}
                placeholder="Username"
                className={inputClass}
                style={INPUT_STYLE(colors, focusedField === 'username')}
                onFocus={() => setFocusedField('username')}
                onBlur={() => setFocusedField(null)}
              />
              <div className="relative">
                <input
                  type={showPassword ? 'text' : 'password'}
                  value={config.authentication.config?.basic?.password || ''}
                  onChange={(e) => handleAuthConfigChange({ basic: { username: config.authentication.config?.basic?.username || '', password: e.target.value } })}
                  placeholder="Password"
                  className={`${inputClass} pr-10`}
                  style={INPUT_STYLE(colors, focusedField === 'password')}
                  onFocus={() => setFocusedField('password')}
                  onBlur={() => setFocusedField(null)}
                />
                <button type="button" onClick={() => setShowPassword(!showPassword)} className="absolute right-2 top-1/2 -translate-y-1/2" style={{ color: colors.textMuted }}>
                  {showPassword ? <EyeOff size={13} /> : <Eye size={13} />}
                </button>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* ── Section: Headers ── */}
      <div>
        <div className="flex items-center gap-2 text-xs font-bold tracking-wider pb-1.5 mb-3" style={SECTION_HEADER_STYLE(colors)}>
          <Network size={13} color={colors.primary} />
          HEADERS
          <span className="text-[10px] font-normal" style={{ color: colors.textMuted }}>(Optional)</span>
        </div>
        <KeyValueEditor
          items={config.headers}
          onChange={(headers) => onChange({ ...config, headers })}
          placeholder={{ key: 'Header Name', value: 'Header Value' }}
        />
      </div>

      {/* ── Section: Query Params ── */}
      <div>
        <div className="flex items-center gap-2 text-xs font-bold tracking-wider pb-1.5 mb-3" style={SECTION_HEADER_STYLE(colors)}>
          <Code size={13} color={colors.primary} />
          QUERY PARAMETERS
          <span className="text-[10px] font-normal" style={{ color: colors.textMuted }}>(Optional)</span>
        </div>
        <KeyValueEditor
          items={config.queryParams}
          onChange={(queryParams) => onChange({ ...config, queryParams })}
          placeholder={{ key: 'Parameter Name', value: 'Parameter Value' }}
          allowPlaceholders
        />
      </div>

      {/* ── Section: Request Body ── */}
      {['POST', 'PUT', 'PATCH'].includes(config.method) && (
        <div>
          <div className="flex items-center gap-2 text-xs font-bold tracking-wider pb-1.5 mb-3" style={SECTION_HEADER_STYLE(colors)}>
            <Code size={13} color={colors.primary} />
            REQUEST BODY (JSON)
          </div>
          <textarea
            value={config.body || ''}
            onChange={(e) => onChange({ ...config, body: e.target.value })}
            placeholder='{"key": "value"}'
            rows={6}
            className="w-full p-3 text-xs font-mono resize-none transition-all"
            style={INPUT_STYLE(colors, focusedField === 'body')}
            onFocus={() => setFocusedField('body')}
            onBlur={() => setFocusedField(null)}
          />
        </div>
      )}

      {/* ── Section: Test Parameters ── */}
      {uniqueParams.length > 0 && (
        <div>
          <div className="flex items-center gap-2 text-xs font-bold tracking-wider pb-1.5 mb-3" style={SECTION_HEADER_STYLE(colors)}>
            <Play size={13} color={colors.primary} />
            TEST PARAMETERS
          </div>
          <div className="space-y-2">
            {uniqueParams.map(param => (
              <div key={param}>
                <label className="text-[11px] font-bold tracking-wider block mb-1.5" style={{ color: colors.text }}>
                  {param.toUpperCase()} <span style={{ color: colors.alert }}>*</span>
                </label>
                <input
                  type="text"
                  value={testParams[param] || ''}
                  onChange={(e) => setTestParams({ ...testParams, [param]: e.target.value })}
                  placeholder={`Enter ${param}`}
                  className={inputClass}
                  style={INPUT_STYLE(colors, focusedField === `param_${param}`)}
                  onFocus={() => setFocusedField(`param_${param}`)}
                  onBlur={() => setFocusedField(null)}
                />
              </div>
            ))}
          </div>
        </div>
      )}

      {/* ── Test Button ── */}
      <button
        onClick={handleTestRequest}
        disabled={isTesting || !config.baseUrl || !config.endpoint}
        className="w-full flex items-center justify-center gap-2 py-3 text-xs font-bold tracking-wide transition-all"
        style={{
          backgroundColor: isTesting || !config.baseUrl || !config.endpoint ? colors.panel : colors.success,
          color: isTesting || !config.baseUrl || !config.endpoint ? colors.textMuted : '#000',
          border: `1px solid ${isTesting || !config.baseUrl || !config.endpoint ? 'var(--ft-border-color, #2A2A2A)' : colors.success}`,
          cursor: isTesting || !config.baseUrl || !config.endpoint ? 'not-allowed' : 'pointer',
        }}
        onMouseEnter={(e) => { if (!isTesting && config.baseUrl && config.endpoint) e.currentTarget.style.opacity = '0.85'; }}
        onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
      >
        {isTesting ? (
          <><RefreshCw size={14} className="animate-spin" /> FETCHING DATA...</>
        ) : (
          <><Play size={14} /> FETCH SAMPLE DATA</>
        )}
      </button>

      {/* ── Success Display ── */}
      {testSuccess && !testError && (
        <div style={{ backgroundColor: `${colors.success}10`, border: `1px solid ${colors.success}` }}>
          <div
            className="flex items-center gap-2 px-3 py-2 border-b"
            style={{ borderColor: `${colors.success}40`, backgroundColor: `${colors.success}15` }}
          >
            <CheckCircle2 size={13} color={colors.success} />
            <span className="text-[11px] font-bold tracking-wider" style={{ color: colors.success }}>
              SAMPLE DATA FETCHED SUCCESSFULLY
            </span>
            <div className="ml-auto text-[10px] font-mono" style={{ color: colors.success }}>
              {Array.isArray(testSuccess)
                ? `${testSuccess.length} records`
                : typeof testSuccess === 'object' && testSuccess?.data && Array.isArray(testSuccess.data)
                ? `${testSuccess.data.length} records in data[]`
                : 'Response received'
              }
            </div>
          </div>
          <div className="p-3 max-h-64 overflow-auto">
            <pre className="text-[11px] font-mono" style={{ color: colors.success }}>
              {JSON.stringify(testSuccess, null, 2)}
            </pre>
          </div>
        </div>
      )}

      {/* ── Error Display ── */}
      {testError && <ErrorPanel error={testError} onRetry={handleTestRequest} />}
    </div>
  );
}
