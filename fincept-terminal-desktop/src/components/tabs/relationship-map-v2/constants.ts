// Relationship Map V2 - Configuration Constants

// ============================================================================
// TIMEOUT CONFIGURATION
// ============================================================================

export const TIMEOUTS = {
  EDGAR_COMPANY: 15000,
  EDGAR_FILINGS: 15000,
  EDGAR_PROXY: 20000,
  EDGAR_EVENTS: 15000,
  EDGAR_INSTITUTIONAL: 20000,
  EDGAR_INSIDER: 15000,
  YFINANCE_INFO: 10000,
  YFINANCE_PEERS: 12000,
  TOTAL_MAX: 30000,
} as const;

// ============================================================================
// CACHE CONFIGURATION
// ============================================================================

export const CACHE_TTL = {
  REALTIME: 5 * 60 * 1000,        // 5 minutes - price, quick metrics
  DAILY: 24 * 60 * 60 * 1000,     // 24 hours - holdings changes
  WEEKLY: 7 * 24 * 60 * 60 * 1000, // 7 days - filings, events
  QUARTERLY: 30 * 24 * 60 * 60 * 1000, // 30 days - 13F data
} as const;

// ============================================================================
// NODE DIMENSIONS
// ============================================================================

export const NODE_SIZES = {
  company: { width: 420, height: 260 },
  peer: { width: 260, height: 160 },
  institutional: { width: 220, height: 110 },
  fund_family: { width: 280, height: 180 },
  insider: { width: 240, height: 120 },
  event: { width: 240, height: 100 },
  supply_chain: { width: 260, height: 130 },
  metrics: { width: 260, height: 160 },
} as const;

// ============================================================================
// LAYOUT CONFIGURATION
// ============================================================================

export const LAYOUT_CONFIG = {
  // Layered layout ring radii (px from center)
  RINGS: {
    INSIDERS: 320,
    INSTITUTIONAL: 520,
    FUND_FAMILIES: 700,
    PEERS: 880,
    EVENTS: 1050,
    METRICS: 400,
  },
  // Force-directed layout params
  FORCE: {
    CHARGE_STRENGTH: -800,
    LINK_DISTANCE: 250,
    CENTER_GRAVITY: 0.05,
    ITERATIONS: 300,
    COLLISION_RADIUS: 100,
  },
  // Transition animation
  TRANSITION_DURATION: 800,
} as const;

// ============================================================================
// NODE COLORS (using CSS variables for theme compatibility)
// ============================================================================

export const NODE_COLORS = {
  company: {
    bg: 'rgba(255, 165, 0, 0.15)',
    border: 'var(--ft-color-primary)',
    text: 'var(--ft-color-text)',
  },
  peer: {
    bg: 'rgba(100, 150, 250, 0.12)',
    border: 'var(--ft-color-info)',
    text: 'var(--ft-color-text)',
  },
  institutional: {
    bg: 'rgba(0, 200, 0, 0.12)',
    border: 'var(--ft-color-success)',
    text: 'var(--ft-color-text)',
  },
  fund_family: {
    bg: 'rgba(200, 100, 255, 0.12)',
    border: 'var(--ft-color-purple)',
    text: 'var(--ft-color-text)',
  },
  insider: {
    bg: 'rgba(0, 255, 255, 0.12)',
    border: 'var(--ft-color-accent)',
    text: 'var(--ft-color-text)',
  },
  event: {
    bg: 'rgba(255, 0, 0, 0.10)',
    border: 'var(--ft-color-alert)',
    text: 'var(--ft-color-text)',
  },
  supply_chain: {
    bg: 'rgba(255, 255, 0, 0.10)',
    border: 'var(--ft-color-warning)',
    text: 'var(--ft-color-text)',
  },
  metrics: {
    bg: 'rgba(120, 120, 120, 0.12)',
    border: 'var(--ft-color-text-muted)',
    text: 'var(--ft-color-text)',
  },
} as const;

// ============================================================================
// VALUATION SIGNAL COLORS
// ============================================================================

export const SIGNAL_COLORS = {
  BUY: { bg: 'rgba(0, 200, 0, 0.2)', border: '#00C800', text: '#00FF41' },
  HOLD: { bg: 'rgba(255, 255, 0, 0.15)', border: '#FFFF00', text: '#FFFF00' },
  SELL: { bg: 'rgba(255, 0, 0, 0.2)', border: '#FF0000', text: '#FF4444' },
} as const;

// ============================================================================
// EDGE STYLES
// ============================================================================

export const EDGE_STYLES = {
  ownership: {
    stroke: 'var(--ft-color-success)',
    strokeWidth: 2,
    animated: false,
  },
  peer: {
    stroke: 'var(--ft-color-info)',
    strokeWidth: 1.5,
    strokeDasharray: '5 5',
    animated: false,
  },
  supply_chain: {
    stroke: 'var(--ft-color-warning)',
    strokeWidth: 2,
    animated: true,
  },
  event: {
    stroke: 'var(--ft-color-alert)',
    strokeWidth: 1,
    strokeDasharray: '3 3',
    animated: false,
  },
  internal: {
    stroke: 'var(--ft-color-text-muted)',
    strokeWidth: 1,
    animated: false,
  },
} as const;

// ============================================================================
// DATA LIMITS
// ============================================================================

export const DATA_LIMITS = {
  MAX_INSTITUTIONAL_HOLDERS: 15,
  MAX_INSIDER_HOLDERS: 10,
  MAX_INSIDER_TRANSACTIONS: 20,
  MAX_PEERS: 8,
  MAX_EVENTS: 10,
  MAX_FILINGS: 15,
  MAX_FUND_FAMILIES: 5,
} as const;

// ============================================================================
// FILING FORM DESCRIPTIONS
// ============================================================================

export const FORM_DESCRIPTIONS: Record<string, string> = {
  '10-K': 'Annual Report',
  '10-Q': 'Quarterly Report',
  '8-K': 'Current Event',
  '4': 'Insider Trading',
  'DEF 14A': 'Proxy Statement',
  '13F-HR': 'Institutional Holdings',
  'S-8': 'Employee Stock Plan',
  'SC 13G': '5%+ Ownership',
  'SC 13G/A': '5%+ Ownership Amendment',
  'DEFA14A': 'Proxy Materials',
  '144': 'Insider Sale Notice',
  'S-1': 'IPO Registration',
  '424B4': 'Prospectus',
};

// ============================================================================
// EVENT CATEGORIES
// ============================================================================

export const EVENT_CATEGORIES: Record<string, string> = {
  '1.01': 'merger',
  '1.02': 'restructuring',
  '2.01': 'restructuring',
  '2.02': 'earnings',
  '5.02': 'management',
  '5.03': 'governance',
  '5.07': 'governance',
  '8.01': 'other',
};
