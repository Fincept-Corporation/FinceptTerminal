/**
 * NodeConfigPanel - Props interface and style constants
 */

import React from 'react';
import { Node } from 'reactflow';
import type { NodeParameterValue } from '@/services/nodeSystem';
import {
  FINCEPT,
  SPACING,
  BORDER_RADIUS,
  FONT_FAMILY,
  FONT_SIZE,
} from '../nodes/shared';

export interface NodeConfigPanelProps {
  selectedNode: Node;
  onLabelChange: (nodeId: string, newLabel: string) => void;
  onParameterChange: (paramName: string, value: NodeParameterValue) => void;
  onTimeoutChange: (nodeId: string, timeoutMs: number | undefined) => void;
  onClose: () => void;
  onDelete: () => void;
  onDuplicate: () => void;
}

// Section header style helper
export const sectionHeaderStyle = (color: string): React.CSSProperties => ({
  display: 'flex',
  alignItems: 'center',
  gap: SPACING.MD,
  marginBottom: SPACING.LG,
  paddingBottom: SPACING.MD,
  borderBottom: `1px solid ${FINCEPT.BORDER}`,
});

export const sectionHeaderTextStyle = (color: string): React.CSSProperties => ({
  color,
  fontSize: FONT_SIZE.LG,
  fontWeight: 700,
  textTransform: 'uppercase',
  letterSpacing: '0.5px',
});

export const labelBlockStyle: React.CSSProperties = {
  display: 'block',
  color: FINCEPT.GRAY,
  fontSize: FONT_SIZE.SM,
  fontWeight: 700,
  marginBottom: SPACING.SM,
  textTransform: 'uppercase',
};

export const selectInputStyle: React.CSSProperties = {
  width: '100%',
  backgroundColor: FINCEPT.DARK_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  color: FINCEPT.WHITE,
  padding: SPACING.MD,
  fontSize: FONT_SIZE.MD,
  fontFamily: FONT_FAMILY,
};

export const textInputStyle: React.CSSProperties = {
  width: '100%',
  backgroundColor: FINCEPT.DARK_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  color: FINCEPT.WHITE,
  padding: SPACING.MD,
  fontSize: FONT_SIZE.MD,
  fontFamily: FONT_FAMILY,
  boxSizing: 'border-box',
};

export const textareaInputStyle: React.CSSProperties = {
  ...textInputStyle,
  height: '100px',
  resize: 'vertical',
};

export const executeButtonStyle = (color: string, disabled: boolean): React.CSSProperties => ({
  width: '100%',
  backgroundColor: disabled ? FINCEPT.BORDER : color,
  color: FINCEPT.DARK_BG,
  border: 'none',
  padding: SPACING.MD,
  fontSize: FONT_SIZE.SM,
  fontWeight: 700,
  cursor: disabled ? 'not-allowed' : 'pointer',
  opacity: disabled ? 0.5 : 1,
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  gap: SPACING.SM,
  fontFamily: FONT_FAMILY,
});

export const infoPanelStyle: React.CSSProperties = {
  padding: SPACING.LG,
  backgroundColor: FINCEPT.DARK_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  color: FINCEPT.GRAY,
  fontSize: FONT_SIZE.MD,
  textAlign: 'center',
};
