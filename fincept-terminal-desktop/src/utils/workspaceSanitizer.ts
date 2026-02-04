/**
 * Workspace tab state sanitizer.
 * Allowlist-based: only explicitly permitted keys for each tab pass through.
 * Blocks any value that looks like a credential or secret.
 */

/** Allowed keys per tab ID */
const TAB_ALLOWLISTS: Record<string, readonly string[]> = {
  screener: ['seriesIds', 'startDate', 'endDate', 'chartType', 'normalizeData'],
  watchlist: ['activeWatchlistName', 'sortBy'],
  markets: ['autoUpdate', 'updateInterval'],
  nodes: ['workflowName', 'activeView'],
  'ai-quant-lab': ['activeView', 'isLeftPanelMinimized', 'isRightPanelMinimized'],
  'equity-trading': [
    'selectedSymbol', 'selectedExchange', 'chartInterval',
    'leftSidebarView', 'centerView', 'rightPanelView',
    'activeBottomTab', 'isBottomPanelMinimized',
  ],
  chat: ['activeProvider', 'activeModel', 'temperature', 'maxTokens'],
  datasources: ['view', 'selectedCategory'],
  portfolio: ['activeSubTab'],
  dashboard: ['widgetLayoutKey'],
  economics: ['dataSource', 'view', 'selectedIndicator', 'selectedCountry', 'selectedTransform', 'bisDataType', 'profileCountry'],
  news: ['activeFilter', 'feedCount', 'refreshInterval', 'searchQuery'],
  geopolitics: ['countryFilter', 'categoryFilter', 'isPanelCollapsed'],
  'equity-research': ['currentSymbol', 'chartPeriod', 'activeTab'],
  'code-editor': ['activeFileId', 'showExplorer', 'showOutput'],
  backtesting: ['selectedProvider', 'activeCommand', 'selectedCategory', 'selectedStrategy', 'symbols', 'startDate', 'endDate', 'initialCapital', 'commission'],
  'crypto-trading': [
    'selectedSymbol', 'selectedView', 'rightPanelView', 'leftSidebarView',
    'activeBottomTab', 'isBottomPanelMinimized',
  ],
  notes: ['isRightPanelMinimized'],
  'report-builder': ['pageTheme', 'fontFamily', 'defaultFontSize', 'rightPanelView'],
  derivatives: ['activeInstrument'],
  'ma-analytics': ['activeTab'],
  'alternative-investments': ['activeCategory', 'isLeftPanelMinimized'],
  'asia-markets': ['activeRegion', 'viewMode', 'isBottomPanelMinimized', 'activeBottomTab'],
  'surface-analytics': ['activeChart'],
};

/** Patterns that indicate credential/secret values */
const BLOCKED_KEY_PATTERNS = /api[_-]?key|secret|password|token|credential|auth|private[_-]?key|session[_-]?id/i;

/** Check if a value is a safe primitive or simple array of primitives */
function isSafeValue(value: unknown): boolean {
  if (value === null) return true;
  const t = typeof value;
  if (t === 'string' || t === 'number' || t === 'boolean') return true;
  if (Array.isArray(value)) {
    return value.every(item => {
      const it = typeof item;
      return it === 'string' || it === 'number' || it === 'boolean';
    });
  }
  return false;
}

/** Check if a string value looks like a credential */
function looksLikeCredential(value: string): boolean {
  if (value.length > 200) return true; // unusually long strings are suspicious
  // JWT-like pattern
  if (/^eyJ[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+/.test(value)) return true;
  return false;
}

/**
 * Sanitize a single tab's state using the allowlist.
 * Returns only permitted keys with safe primitive values.
 */
function sanitizeTabState(tabId: string, state: unknown): Record<string, unknown> | null {
  if (!state || typeof state !== 'object' || Array.isArray(state)) return null;

  const allowlist = TAB_ALLOWLISTS[tabId];
  if (!allowlist) return null;

  const input = state as Record<string, unknown>;
  const result: Record<string, unknown> = {};
  let hasKeys = false;

  for (const key of allowlist) {
    if (!(key in input)) continue;
    const value = input[key];

    // Block credential-looking keys (defense in depth)
    if (BLOCKED_KEY_PATTERNS.test(key)) continue;

    // Only allow safe primitive values
    if (!isSafeValue(value)) continue;

    // Check string values for credential patterns
    if (typeof value === 'string' && looksLikeCredential(value)) continue;

    result[key] = value;
    hasKeys = true;
  }

  return hasKeys ? result : null;
}

/**
 * Sanitize all tab states in a workspace.
 * Run this on save AND on import.
 */
export function sanitizeAllTabStates(
  tabStates: Record<string, unknown>
): Record<string, unknown> {
  if (!tabStates || typeof tabStates !== 'object') return {};

  const sanitized: Record<string, unknown> = {};

  for (const [tabId, state] of Object.entries(tabStates)) {
    const clean = sanitizeTabState(tabId, state);
    if (clean) {
      sanitized[tabId] = clean;
    }
  }

  return sanitized;
}
