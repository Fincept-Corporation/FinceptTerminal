// Relationship Map - Configuration Constants
// Following FINCEPT UI Design System

// ============================================================================
// FINCEPT DESIGN SYSTEM COLORS
// ============================================================================

export const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
} as const;

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
  REALTIME: 5 * 60 * 1000,
  DAILY: 24 * 60 * 60 * 1000,
  WEEKLY: 7 * 24 * 60 * 60 * 1000,
  QUARTERLY: 30 * 24 * 60 * 60 * 1000,
} as const;

// ============================================================================
// NODE DIMENSIONS
// ============================================================================

export const NODE_SIZES = {
  company: { width: 380, height: 220 },
  peer: { width: 240, height: 140 },
  institutional: { width: 200, height: 90 },
  fund_family: { width: 240, height: 150 },
  insider: { width: 220, height: 100 },
  event: { width: 220, height: 90 },
  supply_chain: { width: 240, height: 110 },
  segment: { width: 220, height: 100 },
  debt_holder: { width: 220, height: 90 },
  metrics: { width: 240, height: 140 },
} as const;

// ============================================================================
// LAYOUT CONFIGURATION
// ============================================================================

export const LAYOUT_CONFIG = {
  RINGS: {
    INSIDERS: 320,
    INSTITUTIONAL: 520,
    FUND_FAMILIES: 700,
    PEERS: 880,
    EVENTS: 1050,
    SUPPLY_CHAIN: 750,
    SEGMENTS: 400,
    DEBT_HOLDERS: 900,
    METRICS: 400,
  },
  FORCE: {
    CHARGE_STRENGTH: -800,
    LINK_DISTANCE: 250,
    CENTER_GRAVITY: 0.05,
    ITERATIONS: 300,
    COLLISION_RADIUS: 100,
  },
  TRANSITION_DURATION: 800,
} as const;

// ============================================================================
// NODE COLORS - FINCEPT DESIGN SYSTEM
// ============================================================================

export const NODE_COLORS = {
  company: {
    bg: `${FINCEPT.ORANGE}18`,
    border: FINCEPT.ORANGE,
    text: FINCEPT.WHITE,
    accent: FINCEPT.ORANGE,
  },
  peer: {
    bg: `${FINCEPT.BLUE}15`,
    border: FINCEPT.BLUE,
    text: FINCEPT.WHITE,
    accent: FINCEPT.BLUE,
  },
  institutional: {
    bg: `${FINCEPT.GREEN}15`,
    border: FINCEPT.GREEN,
    text: FINCEPT.WHITE,
    accent: FINCEPT.GREEN,
  },
  fund_family: {
    bg: `${FINCEPT.PURPLE}15`,
    border: FINCEPT.PURPLE,
    text: FINCEPT.WHITE,
    accent: FINCEPT.PURPLE,
  },
  insider: {
    bg: `${FINCEPT.CYAN}12`,
    border: FINCEPT.CYAN,
    text: FINCEPT.WHITE,
    accent: FINCEPT.CYAN,
  },
  event: {
    bg: `${FINCEPT.RED}12`,
    border: FINCEPT.RED,
    text: FINCEPT.WHITE,
    accent: FINCEPT.RED,
  },
  supply_chain: {
    bg: `${FINCEPT.YELLOW}12`,
    border: FINCEPT.YELLOW,
    text: FINCEPT.WHITE,
    accent: FINCEPT.YELLOW,
  },
  segment: {
    bg: `${FINCEPT.ORANGE}12`,
    border: FINCEPT.ORANGE,
    text: FINCEPT.WHITE,
    accent: FINCEPT.ORANGE,
  },
  debt_holder: {
    bg: `${FINCEPT.RED}10`,
    border: '#FF6B6B',
    text: FINCEPT.WHITE,
    accent: '#FF6B6B',
  },
  metrics: {
    bg: `${FINCEPT.GRAY}15`,
    border: FINCEPT.GRAY,
    text: FINCEPT.WHITE,
    accent: FINCEPT.CYAN,
  },
} as const;

// ============================================================================
// VALUATION SIGNAL COLORS
// ============================================================================

export const SIGNAL_COLORS = {
  BUY: { bg: `${FINCEPT.GREEN}25`, border: FINCEPT.GREEN, text: FINCEPT.GREEN },
  HOLD: { bg: `${FINCEPT.YELLOW}20`, border: FINCEPT.YELLOW, text: FINCEPT.YELLOW },
  SELL: { bg: `${FINCEPT.RED}25`, border: FINCEPT.RED, text: FINCEPT.RED },
} as const;

// ============================================================================
// EDGE STYLES
// ============================================================================

export const EDGE_STYLES = {
  ownership: { stroke: FINCEPT.GREEN, strokeWidth: 2 },
  peer: { stroke: FINCEPT.BLUE, strokeWidth: 1.5, strokeDasharray: '5 5' },
  supply_chain: { stroke: FINCEPT.YELLOW, strokeWidth: 2 },
  event: { stroke: FINCEPT.RED, strokeWidth: 1, strokeDasharray: '3 3' },
  segment: { stroke: FINCEPT.ORANGE, strokeWidth: 1.5, strokeDasharray: '4 4' },
  debt: { stroke: '#FF6B6B', strokeWidth: 1.5 },
  internal: { stroke: FINCEPT.MUTED, strokeWidth: 1 },
} as const;

// ============================================================================
// DATA LIMITS - TIERED CONFIGURATION
// ============================================================================

export const DATA_LIMITS = {
  MAX_INSTITUTIONAL_HOLDERS: 40,
  MAX_INSIDER_HOLDERS: 25,
  MAX_INSIDER_TRANSACTIONS: 30,
  MAX_PEERS: 15,
  MAX_EVENTS: 30,
  MAX_FILINGS: 20,
  MAX_FUND_FAMILIES: 15,
  MAX_SUPPLY_CHAIN: 20,
  MAX_SEGMENTS: 10,
  MAX_DEBT_HOLDERS: 10,
} as const;

// Preset configurations for user-selectable density levels
export const DENSITY_PRESETS = {
  minimal: {
    INSTITUTIONAL_HOLDERS: 10,
    INSIDER_HOLDERS: 5,
    PEERS: 6,
    EVENTS: 8,
    FUND_FAMILIES: 3,
    SUPPLY_CHAIN: 5,
    SEGMENTS: 3,
    DEBT_HOLDERS: 3,
  },
  standard: {
    INSTITUTIONAL_HOLDERS: 25,
    INSIDER_HOLDERS: 15,
    PEERS: 12,
    EVENTS: 20,
    FUND_FAMILIES: 8,
    SUPPLY_CHAIN: 12,
    SEGMENTS: 6,
    DEBT_HOLDERS: 6,
  },
  professional: {
    INSTITUTIONAL_HOLDERS: 40,
    INSIDER_HOLDERS: 25,
    PEERS: 15,
    EVENTS: 30,
    FUND_FAMILIES: 15,
    SUPPLY_CHAIN: 20,
    SEGMENTS: 10,
    DEBT_HOLDERS: 10,
  },
} as const;
