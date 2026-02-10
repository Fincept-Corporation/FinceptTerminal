// Fincept Node Design System
// Shared design constants and utilities for all node components

export const FINCEPT = {
  // Primary Colors
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  YELLOW: '#FFFF00',
  CYAN: '#00E5FF',
  BLUE: '#0088FF',

  // Background Colors
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
} as const;

// Typography
export const FONT_FAMILY = '"IBM Plex Mono", Consolas, monospace';

// Spacing
export const SPACING = {
  XS: '4px',
  SM: '6px',
  MD: '8px',
  LG: '12px',
  XL: '16px',
  XXL: '20px',
} as const;

// Border Radius (sharp design)
export const BORDER_RADIUS = {
  NONE: '0px',
  SM: '2px',
  MD: '3px',
  LG: '4px',
} as const;

// Font Sizes
export const FONT_SIZE = {
  XXS: '7px',
  XS: '8px',
  SM: '9px',
  MD: '10px',
  LG: '11px',
  XL: '12px',
  XXL: '14px',
} as const;

// Common Styles
export const getContainerStyle = (selected: boolean, borderColor?: string) => ({
  background: selected ? FINCEPT.HEADER_BG : FINCEPT.PANEL_BG,
  border: `2px solid ${borderColor || (selected ? FINCEPT.ORANGE : FINCEPT.BORDER)}`,
  borderRadius: BORDER_RADIUS.SM,
  fontFamily: FONT_FAMILY,
  boxShadow: selected
    ? `0 0 12px ${FINCEPT.ORANGE}40`
    : '0 2px 8px rgba(0,0,0,0.3)',
  position: 'relative' as const,
});

export const getHeaderStyle = () => ({
  backgroundColor: FINCEPT.DARK_BG,
  padding: `${SPACING.MD} ${SPACING.LG}`,
  borderBottom: `1px solid ${FINCEPT.BORDER}`,
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'space-between',
});

export const getHeaderTitleStyle = () => ({
  color: FINCEPT.WHITE,
  fontSize: FONT_SIZE.LG,
  fontWeight: 700,
});

export const getHeaderSubtitleStyle = () => ({
  color: FINCEPT.GRAY,
  fontSize: FONT_SIZE.XS,
  textTransform: 'uppercase' as const,
});

export const getButtonStyle = (
  variant: 'primary' | 'secondary' | 'danger' = 'primary',
  disabled: boolean = false
) => {
  const baseStyle = {
    border: 'none',
    padding: `${SPACING.MD} ${SPACING.LG}`,
    fontSize: FONT_SIZE.MD,
    fontWeight: 700,
    cursor: disabled ? 'not-allowed' : 'pointer',
    borderRadius: BORDER_RADIUS.SM,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    gap: SPACING.SM,
    transition: 'all 0.2s',
    opacity: disabled ? 0.6 : 1,
  };

  const variantStyles = {
    primary: {
      backgroundColor: disabled ? FINCEPT.GRAY : FINCEPT.ORANGE,
      color: FINCEPT.DARK_BG,
    },
    secondary: {
      backgroundColor: 'transparent',
      border: `1px solid ${FINCEPT.BORDER}`,
      color: FINCEPT.GRAY,
    },
    danger: {
      backgroundColor: disabled ? FINCEPT.GRAY : FINCEPT.RED,
      color: FINCEPT.WHITE,
    },
  };

  return { ...baseStyle, ...variantStyles[variant] };
};

export const getIconButtonStyle = (active: boolean = false) => ({
  backgroundColor: active ? FINCEPT.ORANGE : 'transparent',
  border: `1px solid ${FINCEPT.BORDER}`,
  color: active ? FINCEPT.DARK_BG : FINCEPT.GRAY,
  padding: SPACING.XS,
  cursor: 'pointer',
  borderRadius: BORDER_RADIUS.SM,
  display: 'flex',
  alignItems: 'center',
  transition: 'all 0.2s',
});

export const getInputStyle = (disabled: boolean = false) => ({
  width: '100%',
  backgroundColor: FINCEPT.DARK_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  color: FINCEPT.WHITE,
  padding: SPACING.XS,
  fontSize: FONT_SIZE.SM,
  fontFamily: FONT_FAMILY,
  borderRadius: BORDER_RADIUS.SM,
  opacity: disabled ? 0.5 : 1,
  cursor: disabled ? 'not-allowed' : 'text',
});

export const getTextareaStyle = (disabled: boolean = false) => ({
  ...getInputStyle(disabled),
  resize: 'vertical' as const,
  minHeight: '40px',
});

export const getSelectStyle = () => ({
  width: '100%',
  backgroundColor: FINCEPT.PANEL_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  color: FINCEPT.WHITE,
  padding: SPACING.XS,
  fontSize: FONT_SIZE.SM,
  borderRadius: BORDER_RADIUS.MD,
});

export const getLabelStyle = (required: boolean = false) => ({
  color: required ? FINCEPT.YELLOW : FINCEPT.GRAY,
  fontSize: FONT_SIZE.SM,
  display: 'block',
  marginBottom: SPACING.XS,
  fontWeight: required ? 700 : 400,
});

export const getPanelStyle = () => ({
  backgroundColor: FINCEPT.DARK_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  borderRadius: BORDER_RADIUS.SM,
  padding: SPACING.MD,
});

export const getStatusPanelStyle = (type: 'success' | 'error' | 'warning' | 'info') => {
  const colors = {
    success: FINCEPT.GREEN,
    error: FINCEPT.RED,
    warning: FINCEPT.ORANGE,
    info: FINCEPT.CYAN,
  };

  const color = colors[type];

  return {
    backgroundColor: `${color}20`,
    border: `1px solid ${color}`,
    borderRadius: BORDER_RADIUS.LG,
    padding: SPACING.SM,
    marginBottom: SPACING.MD,
    display: 'flex',
    alignItems: 'center',
    gap: SPACING.SM,
  };
};

export const getHandleStyle = (color: string = FINCEPT.GREEN) => ({
  background: color,
  width: '10px',
  height: '10px',
  border: `2px solid ${FINCEPT.DARK_BG}`,
});

export const getResizeHandleStyle = () => ({
  position: 'absolute' as const,
  bottom: '2px',
  right: '2px',
  width: '20px',
  height: '20px',
  cursor: 'nwse-resize',
  backgroundColor: FINCEPT.DARK_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  borderBottomRightRadius: BORDER_RADIUS.SM,
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  fontSize: FONT_SIZE.XL,
  color: FINCEPT.GRAY,
  transition: 'all 0.2s',
  zIndex: 10,
  opacity: 0.6,
});

export const getEmptyStateStyle = () => ({
  backgroundColor: FINCEPT.DARK_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  borderRadius: BORDER_RADIUS.SM,
  padding: SPACING.XXL,
  textAlign: 'center' as const,
});

// Status colors helper
export const getStatusColor = (status: 'idle' | 'running' | 'completed' | 'error') => {
  const statusColors = {
    idle: FINCEPT.GRAY,
    running: FINCEPT.ORANGE,
    completed: FINCEPT.GREEN,
    error: FINCEPT.RED,
  };
  return statusColors[status];
};
