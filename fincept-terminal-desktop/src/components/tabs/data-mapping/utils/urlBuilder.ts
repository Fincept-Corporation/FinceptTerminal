// URL Builder Utility - Build complete URLs with parameter substitution

import { APIConfig } from '../types';

/**
 * Extract placeholder names from a string
 * Example: "/ticker/{symbol}/range/{from}/{to}" → ["symbol", "from", "to"]
 */
export function extractPlaceholders(text: string): string[] {
  const regex = /\{([^}]+)\}/g;
  const matches: string[] = [];
  let match;

  while ((match = regex.exec(text)) !== null) {
    matches.push(match[1]);
  }

  return matches;
}

/**
 * Replace placeholders in a string with actual values
 * Example: "/ticker/{symbol}" + {symbol: "AAPL"} → "/ticker/AAPL"
 */
export function replacePlaceholders(
  text: string,
  params: Record<string, any>
): string {
  let result = text;

  for (const [key, value] of Object.entries(params)) {
    const placeholder = `{${key}}`;
    result = result.replace(new RegExp(placeholder, 'g'), encodeURIComponent(String(value)));
  }

  return result;
}

/**
 * Build complete URL from API config and runtime parameters
 */
export function buildURL(
  config: APIConfig,
  runtimeParams: Record<string, any> = {}
): string {
  // 1. Start with base URL
  let url = config.baseUrl;

  // Remove trailing slash from base URL
  if (url.endsWith('/')) {
    url = url.slice(0, -1);
  }

  // 2. Add endpoint (with parameter substitution)
  let endpoint = config.endpoint;
  if (!endpoint.startsWith('/')) {
    endpoint = '/' + endpoint;
  }
  endpoint = replacePlaceholders(endpoint, runtimeParams);
  url += endpoint;

  // 3. Add query parameters
  const queryParams = new URLSearchParams();

  // Add config query params (with parameter substitution)
  for (const [key, value] of Object.entries(config.queryParams || {})) {
    const processedValue = replacePlaceholders(value, runtimeParams);
    queryParams.append(key, processedValue);
  }

  // Add API key to query params if needed
  if (
    config.authentication.type === 'apikey' &&
    config.authentication.config?.apiKey?.location === 'query'
  ) {
    queryParams.append(
      config.authentication.config.apiKey.keyName,
      config.authentication.config.apiKey.value
    );
  }

  // Append query string if there are params
  const queryString = queryParams.toString();
  if (queryString) {
    url += '?' + queryString;
  }

  return url;
}

/**
 * Build headers from API config
 */
export function buildHeaders(config: APIConfig): Record<string, string> {
  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
    'Accept': 'application/json',
    ...config.headers,
  };

  // Add authentication headers
  const { authentication } = config;

  if (authentication.type === 'apikey' && authentication.config?.apiKey?.location === 'header') {
    headers[authentication.config.apiKey.keyName] = authentication.config.apiKey.value;
  } else if (authentication.type === 'bearer' && authentication.config?.bearer) {
    headers['Authorization'] = `Bearer ${authentication.config.bearer.token}`;
  } else if (authentication.type === 'basic' && authentication.config?.basic) {
    const credentials = btoa(`${authentication.config.basic.username}:${authentication.config.basic.password}`);
    headers['Authorization'] = `Basic ${credentials}`;
  } else if (authentication.type === 'oauth2' && authentication.config?.oauth2) {
    headers['Authorization'] = `Bearer ${authentication.config.oauth2.accessToken}`;
  }

  return headers;
}

/**
 * Validate URL format
 */
export function validateURL(url: string): { valid: boolean; error?: string } {
  try {
    new URL(url);
    return { valid: true };
  } catch (error) {
    return {
      valid: false,
      error: 'Invalid URL format. Make sure it starts with http:// or https://',
    };
  }
}

/**
 * Check if all required parameters are provided
 */
export function validateParameters(
  config: APIConfig,
  runtimeParams: Record<string, any>
): { valid: boolean; missing?: string[] } {
  const requiredInEndpoint = extractPlaceholders(config.endpoint);
  const requiredInQueryParams = Object.values(config.queryParams || {})
    .flatMap(value => extractPlaceholders(value));

  const allRequired = [...new Set([...requiredInEndpoint, ...requiredInQueryParams])];
  const provided = Object.keys(runtimeParams);

  const missing = allRequired.filter(param => !provided.includes(param));

  if (missing.length > 0) {
    return { valid: false, missing };
  }

  return { valid: true };
}

/**
 * Get preview URL for display purposes
 */
export function getURLPreview(
  config: APIConfig,
  runtimeParams: Record<string, any> = {}
): string {
  try {
    const url = buildURL(config, runtimeParams);

    // Hide sensitive data in preview
    if (
      config.authentication.type === 'apikey' &&
      config.authentication.config?.apiKey?.location === 'query'
    ) {
      const keyName = config.authentication.config.apiKey.keyName;
      return url.replace(new RegExp(`${keyName}=[^&]+`), `${keyName}=***`);
    }

    return url;
  } catch (error) {
    return 'Invalid URL configuration';
  }
}
