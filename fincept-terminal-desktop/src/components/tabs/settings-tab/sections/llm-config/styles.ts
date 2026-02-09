// LLM Configuration Styles
import type { CSSProperties } from 'react';
import type { SettingsColors } from '../../types';

// Base font family for the component
const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

// Create style factory functions that accept colors from props
export const createStyles = (colors: SettingsColors) => ({
  // Container styles
  container: {
    fontFamily: FONT_FAMILY,
    display: 'flex',
    flexDirection: 'column',
    gap: '20px',
  } as CSSProperties,

  // Header styles
  header: {
    borderBottom: `2px solid ${colors.primary}`,
    padding: '16px 0',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'space-between',
  } as CSSProperties,

  headerTitle: {
    display: 'flex',
    alignItems: 'center',
    gap: '12px',
  } as CSSProperties,

  headerTitleText: {
    color: colors.primary,
    fontWeight: 700,
    fontSize: '16px',
    letterSpacing: '0.5px',
  } as CSSProperties,

  headerSubtext: {
    color: '#888',
    fontSize: '12px',
    marginTop: '2px',
  } as CSSProperties,

  // Panel styles
  panel: {
    backgroundColor: '#0F0F0F',
    border: '1px solid #2A2A2A',
    padding: '20px',
  } as CSSProperties,

  panelHeader: {
    color: colors.primary,
    fontSize: '13px',
    fontWeight: 700,
    marginBottom: '16px',
    letterSpacing: '0.5px',
    display: 'flex',
    alignItems: 'center',
    gap: '8px',
  } as CSSProperties,

  // Button styles
  primaryButton: (disabled: boolean) => ({
    background: disabled ? '#2A2A2A' : colors.primary,
    color: disabled ? '#666' : '#000',
    border: 'none',
    padding: '10px 20px',
    fontSize: '12px',
    fontWeight: 700,
    letterSpacing: '0.5px',
    cursor: disabled ? 'not-allowed' : 'pointer',
    display: 'flex',
    alignItems: 'center',
    gap: '8px',
  } as CSSProperties),

  secondaryButton: (active: boolean) => ({
    background: active ? colors.primary : '#1A1A1A',
    border: `1px solid ${active ? colors.primary : '#2A2A2A'}`,
    color: active ? '#000' : '#FFF',
    padding: '6px 12px',
    fontSize: '11px',
    cursor: 'pointer',
    display: 'flex',
    alignItems: 'center',
    gap: '6px',
    fontWeight: 600,
  } as CSSProperties),

  providerButton: (active: boolean) => ({
    padding: '12px 20px',
    backgroundColor: active ? colors.primary : '#1A1A1A',
    border: `2px solid ${active ? colors.primary : '#2A2A2A'}`,
    color: active ? '#000' : '#FFF',
    cursor: 'pointer',
    fontSize: '13px',
    fontWeight: 700,
    letterSpacing: '0.5px',
    transition: 'all 0.2s',
    display: 'flex',
    alignItems: 'center',
    gap: '8px',
  } as CSSProperties),

  customModelButton: (selected: boolean, secondaryColor: string) => ({
    padding: '10px 16px',
    backgroundColor: selected ? secondaryColor : '#1A1A1A',
    border: `2px solid ${selected ? secondaryColor : '#2A2A2A'}`,
    color: selected ? '#000' : '#FFF',
    cursor: 'pointer',
    fontSize: '12px',
    fontWeight: 600,
    letterSpacing: '0.5px',
    transition: 'all 0.2s',
    display: 'flex',
    alignItems: 'center',
    gap: '8px',
  } as CSSProperties),

  // Input styles
  input: {
    width: '100%',
    background: '#000',
    border: '1px solid #2A2A2A',
    color: '#FFF',
    padding: '12px 14px',
    fontSize: '13px',
    fontFamily: FONT_FAMILY,
    outline: 'none',
  } as CSSProperties,

  select: {
    width: '100%',
    background: '#000',
    border: '1px solid #2A2A2A',
    color: '#FFF',
    padding: '12px 14px',
    fontSize: '13px',
    cursor: 'pointer',
  } as CSSProperties,

  textarea: {
    width: '100%',
    background: '#000',
    border: '1px solid #2A2A2A',
    color: '#FFF',
    padding: '12px 14px',
    fontSize: '12px',
    resize: 'vertical',
    fontFamily: FONT_FAMILY,
    lineHeight: '1.5',
    outline: 'none',
  } as CSSProperties,

  // Label styles
  label: {
    color: '#FFF',
    fontSize: '11px',
    display: 'block',
    marginBottom: '8px',
    fontWeight: 600,
    letterSpacing: '0.5px',
  } as CSSProperties,

  labelRequired: {
    color: colors.alert,
  } as CSSProperties,

  // Info box styles
  successBox: {
    backgroundColor: '#0a2a0a',
    border: `2px solid ${colors.success}`,
    padding: '20px',
  } as CSSProperties,

  infoBox: {
    backgroundColor: '#0a1a2a',
    border: `2px solid ${colors.secondary}`,
    padding: '20px',
  } as CSSProperties,

  warningBox: {
    marginTop: '12px',
    padding: '10px',
    background: '#3a0a0a',
    border: `1px solid ${colors.alert}`,
    color: colors.alert,
    fontSize: '11px',
  } as CSSProperties,

  errorMessage: {
    marginTop: '8px',
    padding: '10px 12px',
    background: '#3a0a0a',
    border: `1px solid ${colors.alert}`,
    fontSize: '12px',
    color: colors.alert,
  } as CSSProperties,

  successMessage: {
    marginTop: '8px',
    fontSize: '12px',
    color: colors.success,
    display: 'flex',
    alignItems: 'center',
    gap: '6px',
  } as CSSProperties,

  // Data display styles
  dataRow: {
    display: 'flex',
    alignItems: 'center',
    gap: '16px',
    padding: '12px',
    backgroundColor: '#000',
    border: '1px solid #2A2A2A',
  } as CSSProperties,

  dataLabel: {
    color: '#666',
    fontSize: '10px',
    marginBottom: '4px',
  } as CSSProperties,

  dataValue: {
    color: '#FFF',
    fontSize: '12px',
  } as CSSProperties,

  // Provider badge
  providerBadge: {
    padding: '2px 6px',
    background: colors.primary + '30',
    border: `1px solid ${colors.primary}`,
    fontSize: '9px',
    fontWeight: 700,
    color: colors.primary,
  } as CSSProperties,

  // Grid layouts
  twoColumnGrid: {
    display: 'grid',
    gridTemplateColumns: '1fr 1fr',
    gap: '20px',
  } as CSSProperties,

  formGrid: {
    display: 'grid',
    gridTemplateColumns: '1fr 1fr',
    gap: '12px',
    marginBottom: '12px',
  } as CSSProperties,

  // Flex containers
  flexRow: {
    display: 'flex',
    gap: '10px',
    flexWrap: 'wrap',
  } as CSSProperties,

  flexColumn: {
    display: 'flex',
    flexDirection: 'column',
    gap: '16px',
  } as CSSProperties,

  // Model list item
  modelListItem: (enabled: boolean) => ({
    display: 'flex',
    alignItems: 'center',
    gap: '12px',
    padding: '12px 14px',
    background: enabled ? '#1A1A1A' : '#0A0A0A',
    border: `1px solid ${enabled ? '#2A2A2A' : '#1A1A1A'}`,
    opacity: enabled ? 1 : 0.6,
  } as CSSProperties),

  // Empty state
  emptyState: {
    padding: '40px 20px',
    textAlign: 'center',
    color: '#666',
    border: '1px dashed #2A2A2A',
    background: '#000',
  } as CSSProperties,

  // Slider container
  sliderContainer: {
    display: 'flex',
    alignItems: 'center',
    gap: '12px',
  } as CSSProperties,

  sliderValue: {
    minWidth: '50px',
    padding: '8px 12px',
    background: colors.primary + '20',
    border: `1px solid ${colors.primary}`,
    color: colors.primary,
    fontSize: '13px',
    fontWeight: 700,
    textAlign: 'center',
  } as CSSProperties,

  // Section divider
  sectionDivider: {
    paddingBottom: '12px',
    borderBottom: '1px solid #2A2A2A',
  } as CSSProperties,

  // Summary bar
  summaryBar: {
    marginTop: '16px',
    padding: '12px 14px',
    background: '#0a2a0a',
    border: `1px solid ${colors.success}`,
    display: 'flex',
    alignItems: 'center',
    gap: '10px',
  } as CSSProperties,
});

export type LLMConfigStyles = ReturnType<typeof createStyles>;
