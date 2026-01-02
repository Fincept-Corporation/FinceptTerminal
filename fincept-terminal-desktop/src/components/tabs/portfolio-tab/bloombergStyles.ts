/**
 * Bloomberg Terminal Style Constants for Portfolio Tab
 *
 * This file contains all styling constants to ensure consistent
 * Bloomberg-style terminal aesthetics across all portfolio components.
 */

// ========== COLOR PALETTE ==========
export const BLOOMBERG = {
  // Primary Colors
  ORANGE: '#FF8800',          // Primary brand color, headers, highlights
  WHITE: '#FFFFFF',            // Primary text

  // Status Colors
  RED: '#FF3B3B',              // Alerts, losses, sell signals
  GREEN: '#00D66F',            // Success, gains, buy signals
  YELLOW: '#FFD700',           // Warnings, price highlights

  // Accent Colors
  CYAN: '#00E5FF',             // Accent, info elements
  BLUE: '#0088FF',             // Action buttons, secondary accent
  PURPLE: '#9D4EDD',           // Alternative accent color

  // Grayscale
  GRAY: '#787878',             // Secondary text, labels, muted content
  MUTED: '#4A4A4A',            // Disabled/secondary elements

  // Backgrounds
  DARK_BG: '#000000',          // Main background (pure black)
  PANEL_BG: '#0F0F0F',         // Panel/card backgrounds (slightly lighter black)
  HEADER_BG: '#1A1A1A',        // Header sections
  HOVER: '#1F1F1F',            // Hover states

  // Borders
  BORDER: '#2A2A2A',           // Border lines, dividers
} as const;

// ========== TYPOGRAPHY ==========
export const TYPOGRAPHY = {
  // Font Families
  MONO: '"IBM Plex Mono", "Consolas", "Monaco", "Courier New", monospace',

  // Font Sizes (consistent scale)
  TINY: '8px',
  SMALL: '9px',
  BODY: '10px',
  DEFAULT: '11px',
  SUBHEADING: '12px',
  HEADING: '14px',
  LARGE: '16px',

  // Font Weights
  REGULAR: 400,
  SEMIBOLD: 600,
  BOLD: 700,

  // Letter Spacing
  TIGHT: '0px',
  NORMAL: '0.3px',
  WIDE: '0.5px',
} as const;

// ========== SPACING ==========
export const SPACING = {
  // Padding/Margin scale
  NONE: '0px',
  TINY: '2px',
  SMALL: '4px',
  MEDIUM: '8px',
  DEFAULT: '12px',
  LARGE: '16px',
  XLARGE: '24px',

  // Gaps
  GAP_TINY: '4px',
  GAP_SMALL: '6px',
  GAP_MEDIUM: '8px',
  GAP_DEFAULT: '12px',
  GAP_LARGE: '16px',
} as const;

// ========== BORDERS ==========
export const BORDERS = {
  // Border widths
  THIN: '1px',
  MEDIUM: '2px',

  // Border styles
  STANDARD: `1px solid ${BLOOMBERG.BORDER}`,
  ORANGE: `1px solid ${BLOOMBERG.ORANGE}`,
  ORANGE_THICK: `2px solid ${BLOOMBERG.ORANGE}`,
  RED: `1px solid ${BLOOMBERG.RED}`,
  GREEN: `1px solid ${BLOOMBERG.GREEN}`,
  CYAN: `1px solid ${BLOOMBERG.CYAN}`,

  // Border radius (minimal for terminal aesthetic)
  RADIUS_NONE: '0px',
  RADIUS_SMALL: '2px',
  RADIUS_MEDIUM: '4px',
} as const;

// ========== LAYOUT ==========
export const LAYOUT = {
  // Sidebar widths
  SIDEBAR_LEFT: '320px',
  SIDEBAR_RIGHT: '300px',

  // Modal widths
  MODAL_SMALL: '400px',
  MODAL_MEDIUM: '600px',
  MODAL_LARGE: '800px',

  // Header/Footer heights
  HEADER_HEIGHT: '40px',
  FOOTER_HEIGHT: '32px',
  TICKER_BAR_HEIGHT: '36px',

  // Panel heights
  PANEL_MIN_HEIGHT: '200px',
  PANEL_CHART_HEIGHT: '400px',
} as const;

// ========== EFFECTS ==========
export const EFFECTS = {
  // Text shadows (glow effects)
  ORANGE_GLOW: `0 0 10px ${BLOOMBERG.ORANGE}40`,
  ORANGE_GLOW_STRONG: `0 0 15px ${BLOOMBERG.ORANGE}60`,

  // Box shadows
  PANEL_SHADOW: `0 2px 8px ${BLOOMBERG.ORANGE}20`,
  PANEL_SHADOW_STRONG: `0 4px 16px ${BLOOMBERG.DARK_BG}80`,

  // Drop shadows (for icons)
  ICON_GLOW_ORANGE: `drop-shadow(0 0 4px ${BLOOMBERG.ORANGE})`,
  ICON_GLOW_GREEN: `drop-shadow(0 0 4px ${BLOOMBERG.GREEN})`,
  ICON_GLOW_RED: `drop-shadow(0 0 4px ${BLOOMBERG.RED})`,

  // Transitions
  TRANSITION_FAST: 'all 0.15s ease',
  TRANSITION_STANDARD: 'all 0.2s ease',
  TRANSITION_SLOW: 'all 0.3s ease',
} as const;

// ========== COMMON STYLES ==========
export const COMMON_STYLES = {
  // Container
  container: {
    height: '100%',
    backgroundColor: BLOOMBERG.DARK_BG,
    color: BLOOMBERG.WHITE,
    fontFamily: TYPOGRAPHY.MONO,
    overflow: 'hidden',
    display: 'flex',
    flexDirection: 'column' as const,
  },

  // Panel
  panel: {
    backgroundColor: BLOOMBERG.PANEL_BG,
    border: BORDERS.STANDARD,
    padding: SPACING.DEFAULT,
  },

  // Header
  header: {
    backgroundColor: BLOOMBERG.HEADER_BG,
    borderBottom: BORDERS.ORANGE_THICK,
    padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'space-between',
    flexShrink: 0,
    boxShadow: EFFECTS.PANEL_SHADOW,
  },

  // Section Header
  sectionHeader: {
    color: BLOOMBERG.ORANGE,
    fontSize: TYPOGRAPHY.SUBHEADING,
    fontWeight: TYPOGRAPHY.BOLD,
    letterSpacing: TYPOGRAPHY.WIDE,
    textTransform: 'uppercase' as const,
    marginBottom: SPACING.MEDIUM,
  },

  // Data Label
  dataLabel: {
    color: BLOOMBERG.GRAY,
    fontSize: TYPOGRAPHY.SMALL,
    fontWeight: TYPOGRAPHY.SEMIBOLD,
    textTransform: 'uppercase' as const,
  },

  // Data Value
  dataValue: {
    color: BLOOMBERG.WHITE,
    fontSize: TYPOGRAPHY.DEFAULT,
    fontWeight: TYPOGRAPHY.REGULAR,
    fontFamily: TYPOGRAPHY.MONO,
  },

  // Metric Card
  metricCard: {
    backgroundColor: BLOOMBERG.PANEL_BG,
    border: BORDERS.STANDARD,
    padding: SPACING.DEFAULT,
    display: 'flex',
    flexDirection: 'column' as const,
    gap: SPACING.SMALL,
  },

  // Button Primary
  buttonPrimary: {
    padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
    backgroundColor: BLOOMBERG.ORANGE,
    border: BORDERS.ORANGE,
    color: BLOOMBERG.DARK_BG,
    fontSize: TYPOGRAPHY.BODY,
    fontWeight: TYPOGRAPHY.BOLD,
    cursor: 'pointer',
    transition: EFFECTS.TRANSITION_STANDARD,
    textTransform: 'uppercase' as const,
  },

  // Button Secondary
  buttonSecondary: {
    padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
    backgroundColor: 'transparent',
    border: BORDERS.STANDARD,
    color: BLOOMBERG.WHITE,
    fontSize: TYPOGRAPHY.BODY,
    fontWeight: TYPOGRAPHY.SEMIBOLD,
    cursor: 'pointer',
    transition: EFFECTS.TRANSITION_STANDARD,
    textTransform: 'uppercase' as const,
  },

  // Input Field
  inputField: {
    width: '100%',
    padding: SPACING.MEDIUM,
    backgroundColor: BLOOMBERG.DARK_BG,
    border: BORDERS.STANDARD,
    color: BLOOMBERG.WHITE,
    fontSize: TYPOGRAPHY.BODY,
    fontFamily: TYPOGRAPHY.MONO,
    outline: 'none',
  },

  // Table Header
  tableHeader: {
    backgroundColor: BLOOMBERG.HEADER_BG,
    color: BLOOMBERG.ORANGE,
    fontSize: TYPOGRAPHY.BODY,
    fontWeight: TYPOGRAPHY.BOLD,
    padding: SPACING.SMALL,
    textAlign: 'left' as const,
    borderBottom: BORDERS.ORANGE,
    textTransform: 'uppercase' as const,
  },

  // Table Row
  tableRow: {
    padding: SPACING.TINY,
    fontSize: TYPOGRAPHY.BODY,
    borderBottom: BORDERS.STANDARD,
    minHeight: '24px',
    display: 'flex',
    alignItems: 'center',
  },

  // Table Row Alternate
  tableRowAlt: {
    backgroundColor: 'rgba(255,255,255,0.03)',
  },

  // Divider
  divider: {
    height: '1px',
    backgroundColor: BLOOMBERG.BORDER,
    margin: `${SPACING.MEDIUM} 0`,
  },

  // Vertical Divider
  verticalDivider: {
    width: '1px',
    backgroundColor: BLOOMBERG.BORDER,
    alignSelf: 'stretch',
  },

  // Modal Overlay
  modalOverlay: {
    position: 'fixed' as const,
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: 'rgba(0,0,0,0.85)',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    zIndex: 9999,
  },

  // Modal Panel
  modalPanel: {
    backgroundColor: BLOOMBERG.PANEL_BG,
    border: BORDERS.ORANGE_THICK,
    padding: SPACING.XLARGE,
    minWidth: '400px',
    maxWidth: '600px',
    boxShadow: EFFECTS.PANEL_SHADOW_STRONG,
  },

  // Scrollbar styling (inject into <style> tag)
  scrollbarCSS: `
    *::-webkit-scrollbar {
      width: 6px;
      height: 6px;
    }
    *::-webkit-scrollbar-track {
      background: ${BLOOMBERG.DARK_BG};
    }
    *::-webkit-scrollbar-thumb {
      background: ${BLOOMBERG.BORDER};
      border-radius: 3px;
    }
    *::-webkit-scrollbar-thumb:hover {
      background: ${BLOOMBERG.MUTED};
    }
  `,
} as const;

// ========== UTILITY FUNCTIONS ==========

/**
 * Get color based on value (positive/negative)
 */
export function getValueColor(value: number): string {
  if (value > 0) return BLOOMBERG.GREEN;
  if (value < 0) return BLOOMBERG.RED;
  return BLOOMBERG.WHITE;
}

/**
 * Get formatted percentage with color
 */
export function formatPercentage(value: number, decimals: number = 2): string {
  const sign = value >= 0 ? '+' : '';
  return `${sign}${value.toFixed(decimals)}%`;
}

/**
 * Get formatted currency
 */
export function formatCurrency(value: number, currency: string = '$', decimals: number = 2): string {
  return `${currency}${value.toLocaleString('en-US', { minimumFractionDigits: decimals, maximumFractionDigits: decimals })}`;
}

/**
 * Create hover style
 */
export function createHoverStyle(baseStyle: React.CSSProperties): {
  style: React.CSSProperties;
  onMouseEnter: (e: React.MouseEvent<HTMLElement>) => void;
  onMouseLeave: (e: React.MouseEvent<HTMLElement>) => void;
} {
  return {
    style: baseStyle,
    onMouseEnter: (e) => {
      e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
    },
    onMouseLeave: (e) => {
      e.currentTarget.style.backgroundColor = baseStyle.backgroundColor || 'transparent';
    },
  };
}

/**
 * Create focus style handlers for inputs
 */
export function createFocusHandlers() {
  return {
    onFocus: (e: React.FocusEvent<HTMLInputElement | HTMLTextAreaElement>) => {
      e.currentTarget.style.borderColor = BLOOMBERG.ORANGE;
    },
    onBlur: (e: React.FocusEvent<HTMLInputElement | HTMLTextAreaElement>) => {
      e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
    },
  };
}

// ========== GRID TEMPLATES ==========
export const GRID_TEMPLATES = {
  // 2-column grid
  twoColumn: {
    display: 'grid',
    gridTemplateColumns: '1fr 1fr',
    gap: SPACING.GAP_DEFAULT,
  },

  // 3-column grid
  threeColumn: {
    display: 'grid',
    gridTemplateColumns: '1fr 1fr 1fr',
    gap: SPACING.GAP_DEFAULT,
  },

  // 4-column grid
  fourColumn: {
    display: 'grid',
    gridTemplateColumns: '1fr 1fr 1fr 1fr',
    gap: SPACING.GAP_DEFAULT,
  },

  // Auto-fit grid (responsive)
  autoFit: (minWidth: string = '250px') => ({
    display: 'grid',
    gridTemplateColumns: `repeat(auto-fit, minmax(${minWidth}, 1fr))`,
    gap: SPACING.GAP_DEFAULT,
  }),
} as const;

// ========== ANIMATION KEYFRAMES ==========
export const ANIMATIONS = {
  // Price flash animation
  priceFlash: `
    @keyframes priceFlash {
      0% { background-color: ${BLOOMBERG.YELLOW}40; }
      100% { background-color: transparent; }
    }
  `,

  // Pulse animation
  pulse: `
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
  `,

  // Fade in animation
  fadeIn: `
    @keyframes fadeIn {
      from { opacity: 0; }
      to { opacity: 1; }
    }
  `,
} as const;

export default {
  BLOOMBERG,
  TYPOGRAPHY,
  SPACING,
  BORDERS,
  LAYOUT,
  EFFECTS,
  COMMON_STYLES,
  GRID_TEMPLATES,
  ANIMATIONS,
  getValueColor,
  formatPercentage,
  formatCurrency,
  createHoverStyle,
  createFocusHandlers,
};
